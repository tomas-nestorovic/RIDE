#include "stdafx.h"
#include "CapsBase.h"
#include "HFE.h"

	static LPCTSTR Recognize(PTCHAR){
		static constexpr TCHAR SingleDeviceName[]=_T("HxC Floppy Emulator (PIC18F-based)\0");
		return SingleDeviceName;
	}
	static PImage Instantiate(LPCTSTR){
		return new CHFE;
	}

	const CImage::TProperties CHFE::Properties={
		MAKE_IMAGE_ID('H','x','C','F','M','T','1','1'), // a unique identifier
		Recognize,	// list of recognized device names
		Instantiate,// instantiation function
		_T("*.hfe"),	// filter
		Medium::FLOPPY_ANY, // supported Media
		Codec::MFM, // supported Codecs
		1,16384	// Sector supported min and max length
		,true // read-only
	};






	

	#define HEADER_SIGNATURE	"HXCPICFE"

	CHFE::UHeader::UHeader(){
		// ctor
		::ZeroMemory( this, sizeof(*this) );
		::lstrcpyA( signature, HEADER_SIGNATURE );
		trackEncoding=TTrackEncoding::UNKNOWN;
		floppyInterface=TFloppyInterface::DISABLED;
		cylInfosBegin=1;
		writeable=true;
		alternative[0].disabled = alternative[1].disabled = true;
	}

	bool CHFE::UHeader::IsValid() const{
		// True <=> this Header follows HFE specification, otherwise False
		return	!::memcmp( signature, HEADER_SIGNATURE, sizeof(signature) )
				&&
				formatRevision==0
				&&
				0<nHeads && nHeads<=2
				&&
				bitrate>0
				//&&
				//driveRpm>0 // commented out as this is often not set correctly
				&&
				cylInfosBegin>0;
	}








	#define INI_SECTION		_T("HxCFlopEm")

	CHFE::CHFE()
		// ctor
		: CCapsBase( &Properties, '\0', true, INI_SECTION ) {
		Reset();
	}








	BOOL CHFE::OnOpenDocument(LPCTSTR lpszPathName){
		// True <=> Image opened successfully, otherwise False
		// - base
		if (!__super::OnOpenDocument(nullptr) // don't involve CAPS in Image opening
			&&
			::GetLastError()!=ERROR_NOT_SUPPORTED // the CAPS library currently doesn't support reading Stream files
		)
			return FALSE;
		// - opening
		canBeModified=false; // assumption; TODO: assume True
		if (TStdWinError err=OpenImageForReadingAndWriting(lpszPathName,f)) // if cannot open for both reading and writing ...
			if ( err=OpenImageForReading(lpszPathName,f) ){ // ... trying to open at least for reading, and if neither this works ...
				::SetLastError(err);
				return FALSE; // ... the Image cannot be open in any way
			}else
				canBeModified=false;
		Reset();
		// - if data shorter than an empty Image, keeping reset to empty Image
		const WORD nHeaderBytesRead=f.Read(&header,sizeof(header));
		if (!app.IsInGodMode()) // must follow the rules?
			canBeModified&=header.writeable;
		if (!nHeaderBytesRead)
			return TRUE;
		else if (nHeaderBytesRead<sizeof(header)){
formatError: ::SetLastError(ERROR_BAD_FORMAT);
			return FALSE;
		}
		// - reading content of the Image and continuously validating its structure
		if (!header.IsValid()){
			::SetLastError(ERROR_INVALID_DATA);
			return FALSE;
		}
		const UINT nCylInfoBytes=std::min( header.nCylinders*sizeof(*cylInfos), sizeof(cylInfos) );
		if (f.Read( cylInfos, nCylInfoBytes )!=nCylInfoBytes)
			goto formatError;
		// - adopting geometry from the Header
		if (header.nCylinders)
			capsImageInfo.maxcylinder=header.nCylinders-1; // inclusive!
		if (header.nHeads)
			capsImageInfo.maxhead=header.nHeads-1; // inclusive!
		// - confirming initial settings
		if (!EditSettings(true)){ // dialog cancelled?
			::SetLastError( ERROR_CANCELLED );
			return FALSE;
		}
		// - warning on unsupported features
		if (header.formatRevision!=0
			//||
			//header.step!=TStep::SINGLE // according to official HxC source code, this parameter is completely ignored during Image loading
			||
			header.alternative[0].disabled==0
			||
			header.alternative[1].disabled==0
		)
			Utils::Warning( _T("The image contains features currently not supported by ") APP_ABBREVIATION _T(". Possible unexpected behavior!") );
		// - warning on unsupported Cylinders
		WarnOnAndCorrectExceedingCylinders();
		return TRUE;
	}

	CImage::CTrackReader CHFE::ReadTrack(TCylinder cyl,THead head) const{
		// creates and returns a general description of the specified Track, represented using neutral LogicalTimes
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return CTrackReaderWriter::Invalid;
		// - if Track already read before, returning the result from before
		PInternalTrack &rit=internalTracks[cyl][head];
		if (rit!=nullptr)
			return *rit;
		// - construction of InternalTrack
		//const BYTE cylPhysical=cyl<<(BYTE)header.WantDoubleTrackStep();
		if (!cylInfos[cyl].nBlocksOffset) // maybe an error during Image creation?
			return CTrackReaderWriter::Invalid;
		f.Seek( cylInfos[cyl].nBlocksOffset*sizeof(TCylinderBlock)+head*sizeof(TTrackBlock), CFile::begin );
		BYTE trackBytes[USHRT_MAX+1], *pTrackBytes=trackBytes;
		for( WORD nCylBlocks=Utils::RoundDivUp(cylInfos[cyl].nBytesLength,(WORD)sizeof(TCylinderBlock)); nCylBlocks-->0; ){
			const auto nBytesRead=f.Read( pTrackBytes, sizeof(TTrackBlock) );
			f.Seek( sizeof(TTrackBlock), CFile::current ); // skip unwanted Head
			pTrackBytes+=nBytesRead;
		}
		if (const auto trackLength=std::min<UDWORD>( cylInfos[cyl].nBytesLength, pTrackBytes-trackBytes )){
			CapsTrackInfoT2 cti={};
				cti.trackbuf=trackBytes;
				cti.tracklen=trackLength;
				while (pTrackBytes-->trackBytes)
					*pTrackBytes=Utils::GetReversedByte(*pTrackBytes);
			rit = CInternalTrack::CreateFrom( *this, &cti, 1, 0 );
			return *rit;
		}
		return CTrackReaderWriter::Invalid;
	}

	TStdWinError CHFE::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		// - must be setting Medium compatible with the FloppyInterface specified in the Header
		if (header.floppyInterface!=TFloppyInterface::DISABLED)
			switch (pFormat->mediumType){
				case Medium::FLOPPY_DD:{
					static constexpr TFloppyInterface Compatibles[]={ TFloppyInterface::IBM_PC_DD, TFloppyInterface::ATARI_ST_DD, TFloppyInterface::AMIGA_DD, TFloppyInterface::CPC_DD, TFloppyInterface::GENERIC_SHUGART_DD, TFloppyInterface::MSX2_DD, TFloppyInterface::C64_DD, TFloppyInterface::EMU_SHUGART, TFloppyInterface::S950_DD };
					if (::memchr( Compatibles, header.floppyInterface, sizeof(Compatibles) ))
						break;
					return ERROR_UNRECOGNIZED_MEDIA;
				}
				case Medium::FLOPPY_DD_525:{
					static constexpr TFloppyInterface Compatibles[]={ TFloppyInterface::IBM_PC_DD, TFloppyInterface::ATARI_ST_DD, TFloppyInterface::AMIGA_DD, TFloppyInterface::C64_DD };
					if (::memchr( Compatibles, header.floppyInterface, sizeof(Compatibles) ))
						break;
					return ERROR_UNRECOGNIZED_MEDIA;
				}
				case Medium::FLOPPY_HD_525:
				case Medium::FLOPPY_HD_350:{
					static constexpr TFloppyInterface Compatibles[]={ TFloppyInterface::IBM_PC_HD, TFloppyInterface::ATARI_ST_HD, TFloppyInterface::AMIGA_HD, TFloppyInterface::S950_HD };
					if (::memchr( Compatibles, header.floppyInterface, sizeof(Compatibles) ))
						break;
					return ERROR_UNRECOGNIZED_MEDIA;
				}
				default:
					return ERROR_UNRECOGNIZED_MEDIA;
			}
		// - base
		return __super::SetMediumTypeAndGeometry( pFormat, sideMap, firstSectorNumber );
	}

	bool CHFE::EditSettings(bool initialEditing){
		// True <=> new settings have been accepted (and adopted by this Image), otherwise False
		return true;
	}

	TStdWinError CHFE::Reset(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - base
		if (const TStdWinError err=__super::Reset())
			return err;
		// - reinitializing to an empty Image
		header=UHeader();
		::ZeroMemory( cylInfos, sizeof(cylInfos) );
		::ZeroMemory( &capsImageInfo, sizeof(capsImageInfo) );
		return ERROR_SUCCESS;
	}
