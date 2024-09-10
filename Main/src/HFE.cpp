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
		MAKE_IMAGE_ID('H','x','C','-','2','0','0','1'), // a unique identifier
		Recognize,	// list of recognized device names
		Instantiate,// instantiation function
		_T("*.hfe"),	// filter
		Medium::FLOPPY_ANY, // supported Media
		Codec::MFM, // supported Codecs
		1,16384	// Sector supported min and max length
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
				0<nCylinders // mustn't be zero for 'capsImageInfo.maxcylinder' is inclusive! (and "-1" isn't valid)
				&&
				0<nHeads && nHeads<=2 // mustn't be zero for 'capsImageInfo.maxhead' is inclusive! (and "-1" isn't valid)
				&&
				dataBitRate>0
				//&&
				//driveRpm>0 // commented out as this is often not set correctly
				&&
				cylInfosBegin>0;
	}








	#define TRACK_BYTES_MAX	USHRT_MAX

	CHFE::CTrackBytes::CTrackBytes(WORD count)
		// ctor
		: Utils::CCallocPtr<BYTE>( Utils::RoundUpToMuls<int>(count,sizeof(TTrackData)), 0 )
		, count(count) {
		ASSERT( count>0 ); // call Invalidate() to indicate "no Bytes"
	}

	CHFE::CTrackBytes::CTrackBytes(CTrackBytes &&r)
		// move ctor
		: count(r.count) {
		reset( r.release() );
	}

	void CHFE::CTrackBytes::Invalidate(){
		// disposes all Bytes, rendering this object unusable
		reset(nullptr), count=0;
	}

	void CHFE::CTrackBytes::ReverseBitsInEachByte() const{
		// reverses the order of bits in each Byte
		for( PBYTE p=*this,pLast=GetEnd(); p<pLast; p++ )
			*p=Utils::GetReversedByte(*p);
	}








	#define INI_SECTION		_T("HxC2k1")

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
		canBeModified=true; // assumption
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
			canBeModified&=header.writeable!=0;
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
		// - if Track already read before, returning the result from before
		if (const auto tr=ReadExistingTrack(cyl,head))
			return tr;
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return CTrackReaderWriter::Invalid;
		// - construction of InternalTracks for both Heads
		PInternalTrack &rit=internalTracks[cyl][head];
		if (!cylInfos[cyl].IsValid()) // maybe an error during Image creation?
			return CTrackReaderWriter::Invalid;
		internalTracks[cyl][0]=BytesToTrack( ReadTrackBytes(cyl,0) );
		internalTracks[cyl][1]=BytesToTrack( ReadTrackBytes(cyl,1) );
		return	rit ? *rit : CTrackReaderWriter::Invalid;
	}

	inline int GetTotalBitRate(int dataBitRate){
		return dataBitRate*2; // "*2" = data and clock bits
	}

	inline static TLogTime GetCellTime(int dataBitRate){
		return TIME_SECOND(1)/GetTotalBitRate(dataBitRate);
	}

	TStdWinError CHFE::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - must be setting Medium compatible with the FloppyInterface specified in the Header
		if (header.floppyInterface<TFloppyInterface::LAST_KNOWN)
			switch (pFormat->mediumType){
				case Medium::FLOPPY_DD:{
					static constexpr TFloppyInterface Compatibles[]={ TFloppyInterface::IBM_PC_DD, TFloppyInterface::ATARI_ST_DD, TFloppyInterface::AMIGA_DD, TFloppyInterface::CPC_DD, TFloppyInterface::GENERIC_SHUGART, TFloppyInterface::MSX2_DD, TFloppyInterface::C64_DD, TFloppyInterface::EMU_SHUGART, TFloppyInterface::S950_DD };
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
					static constexpr TFloppyInterface Compatibles[]={ TFloppyInterface::IBM_PC_HD, TFloppyInterface::ATARI_ST_HD, TFloppyInterface::AMIGA_HD, TFloppyInterface::S950_HD, TFloppyInterface::GENERIC_SHUGART };
					if (::memchr( Compatibles, header.floppyInterface, sizeof(Compatibles) ))
						break;
					return ERROR_UNRECOGNIZED_MEDIA;
				}
				default:
					return ERROR_UNRECOGNIZED_MEDIA;
			}
		// - must be setting Medium compatible with the nominal # of Cells
		if (pFormat->mediumType!=Medium::UNKNOWN){
			const auto mp=Medium::GetProperties(pFormat->mediumType);
			if (!mp->IsAcceptableCountOfCells( mp->revolutionTime/GetCellTime(header.dataBitRate*1000) ))
				return ERROR_UNRECOGNIZED_MEDIA;
		}
		// - base
		if (floppyType!=pFormat->mediumType) // must reconstruct all Tracks with parameters corresponding to new Medium Type?
			if (m_strPathName.GetLength()>0)
				DestroyAllTracks(); 
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

	TStdWinError CHFE::SaveTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const{
		// saves the specified Track to the inserted Medium; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED; // individual Track saving is not supported for this kind of Image (OnSaveDocument must be called instead)
	}

	TStdWinError CHFE::FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte,const volatile bool &cancelled){
		// formats given Track {Cylinder,Head} to the requested NumberOfSectors, each with corresponding Length and FillerByte as initial content; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - grow before calling base (for current Cyl/Head dimensions mustn't be the cause of failure)
		const UDWORD nNewCylsMax=std::max<UDWORD>( capsImageInfo.maxcylinder, cyl ); // inclusive!
		const UDWORD nNewHeadsMax=std::max<UDWORD>( capsImageInfo.maxhead, head ); // inclusive!
{		const auto mc0=Utils::CVarTempReset<UDWORD>( capsImageInfo.maxcylinder, nNewCylsMax );
		const auto mh0=Utils::CVarTempReset<UDWORD>( capsImageInfo.maxhead, nNewHeadsMax );
		if (const TStdWinError err=__super::FormatTrack( cyl, head, codec, nSectors, bufferId, bufferLength, bufferFdcStatus, gap3, fillerByte, cancelled ))
			return err;
}		// - adopt grown Cyl/Head dimensions
		capsImageInfo.maxcylinder=nNewCylsMax;
		capsImageInfo.maxhead=nNewHeadsMax;
		return ERROR_SUCCESS;
	}

	CHFE::CTrackBytes CHFE::ReadTrackBytes(TCylinder cyl,THead head) const{
		// reads from File and returns raw data of specified Track
		f.Seek(  cylInfos[cyl].nBlocksOffset*sizeof(TCylinderBlock) + head*sizeof(TTrackData),  CFile::begin  );
		CTrackBytes result( cylInfos[cyl].nBytesLength/2 );
		PBYTE pLast=result;
		for( auto nCylBlocks=Utils::RoundDivUp(cylInfos[cyl].nBytesLength,(WORD)sizeof(TCylinderBlock)); nCylBlocks-->0; ){
			const auto nBytesRead=f.Read( pLast, sizeof(TTrackData) );
			if (nBytesRead!=sizeof(TTrackData)){
				result.Invalidate();
				break;
			}
			f.Seek( sizeof(TTrackData), CFile::current ); // skip unwanted Head
			pLast+=nBytesRead;
		}
		return result;
	}

	CHFE::CTrackBytes CHFE::TrackToBytes(CInternalTrack &rit) const{
		// converts specified InternalTrack to HFE-encoded Bytes
		CTrackBytes result(TRACK_BYTES_MAX);
		rit.FlushSectorBuffers();
		PBYTE p=result;
		CTrackReader tr=rit;
		for( tr.RewindToIndexAndResetProfile(0); tr && p-result<TRACK_BYTES_MAX; ){
			const char nBitsRead=tr.ReadBits8(*p);
			*p++<<=(8-nBitsRead);
		}
		result.TrimTo( p-result );
		result.ReverseBitsInEachByte();
		return result;
	}

	CCapsBase::PInternalTrack CHFE::BytesToTrack(const CTrackBytes &bytes) const{
		// converts specified HFE-encoded Bytes to InternalTrack
		if (bytes){
		bytes.ReverseBitsInEachByte();
			CapsTrackInfoT2 cti={};
				cti.trackbuf=bytes;
				cti.tracklen=bytes.GetCount();
			return CInternalTrack::CreateFrom( *this, &cti, 1, 0 );
		}else
			return nullptr;
		}

	TStdWinError CHFE::SaveAllModifiedTracks(LPCTSTR lpszPathName,CActionProgress &ap){
		// saves all Modified Tracks; returns Windows standard i/o error
		const DWORD nRequiredBytesHeaderAndCylInfos=sizeof(UHeader)+Utils::RoundUpToMuls( (int)sizeof(TCylinderInfo)*GetCylinderCount(), (int)sizeof(TBlock) );
		if (nRequiredBytesHeaderAndCylInfos!=sizeof(UHeader)+sizeof(TBlock)) // only 1 Block allowed for CylInfo table
			return ERROR_NOT_SUPPORTED;
{		CFile fTmp;
		const bool savingToCurrentFile= lpszPathName==f.GetFilePath() && f.m_hFile!=CFile::hFileNull && ::GetFileAttributes(lpszPathName)!=INVALID_FILE_ATTRIBUTES; // saving to the same file and that file exists (handle doesn't exist when creating new Image)
		if (!savingToCurrentFile)
			if (const TStdWinError err=CreateImageForReadingAndWriting(lpszPathName,fTmp))
				return err;
		ap.SetProgressTarget( 2*ARRAYSIZE(cylInfos) + 1 + 1 );
		const Medium::PCProperties mp=Medium::GetProperties( floppyType );
		// - creating ContentLayout map of the file in which UNMODIFIED occupied space is represented by positive numbers, whereas gaps with negative numbers
		CFile &fTarget= savingToCurrentFile ? f : fTmp;
		typedef std::map<DWORD,LONG> CContentLayout;
		CContentLayout contentLayout; // key = position in file, value>0 = Track length, value<0 = unused gap size
		if (savingToCurrentFile){
			// . adding unmodified Tracks to ContentLayout as "occupied" (value>0)
			for( TCylinder cyl=ARRAYSIZE(cylInfos); cyl-->0; )
				if (cylInfos[cyl].IsValid()) // Cylinder actually existed in the file before?
					if (!AnyTrackModified(cyl)) // not Modified or not even read Cylinder?
						contentLayout.insert(
							std::make_pair( cylInfos[cyl].nBlocksOffset*sizeof(TBlock), Utils::RoundUpToMuls<LONG>(cylInfos[cyl].nBytesLength,sizeof(TBlock)) )
						);
			// . adding gaps (value<0)
			if (contentLayout.size()>0){ // some Cylinders left untouched in the Image?
				const auto itLastCyl=contentLayout.crbegin();
				const auto lastCylEnd=itLastCyl->first+itLastCyl->second;
				contentLayout.insert(
					std::make_pair( lastCylEnd, lastCylEnd-fTarget.GetLength() )
				);
			}
			CContentLayout gaps;
			DWORD prevTrackEnd=sizeof(UHeader)+Utils::RoundUpToMuls(header.nCylinders*sizeof(TCylinderInfo), sizeof(TBlock) );
			for each( const auto &kvp in contentLayout ){
				if (prevTrackEnd<kvp.first) // gap in the file?
					gaps.insert(  std::make_pair( prevTrackEnd, prevTrackEnd-kvp.first )  );
				prevTrackEnd=kvp.first+kvp.second;
			}
			contentLayout.insert( gaps.cbegin(), gaps.cend() );
		}else
			fTarget.SetLength( nRequiredBytesHeaderAndCylInfos );
		// - saving
		auto sub=ap.CreateSubactionProgress( ARRAYSIZE(cylInfos), ARRAYSIZE(cylInfos) );
		CTrackBytes invalidTrackBytes(1);
			invalidTrackBytes.Invalidate();
		for( TCylinder cyl=0; cyl<ARRAYSIZE(cylInfos); sub.UpdateProgress(++cyl) ){
			if (!AnyTrackModified(cyl)) // not Modified or not even read Cylinder?
				continue;
			const PInternalTrack pitHead0=GetInternalTrackSafe(cyl,0), pitHead1=GetInternalTrackSafe(cyl,1);
			const CTrackBytes head0=pitHead0!=nullptr // Track 0 exists?
									? TrackToBytes( *pitHead0 )
									: std::move(invalidTrackBytes);
			const CTrackBytes head1=pitHead1!=nullptr // Track 1 exists?
									? TrackToBytes( *pitHead1 )
									: std::move(invalidTrackBytes);
			const auto nBytesLongerTrack=std::max( head0.GetCount(), head1.GetCount() );
			auto nBytesCylinder=Utils::RoundUpToMuls( nBytesLongerTrack*2, (int)sizeof(TCylinderBlock) );
			DWORD fPosition=Utils::RoundUpToMuls<DWORD>( fTarget.GetLength(), sizeof(TBlock) ); // assumption (Cylinder doesn't fit in anywhere between existing Track and must be appended to the Image)
			for( auto it=contentLayout.begin(); it!=contentLayout.end(); it++ )
				if (it->second<=-nBytesCylinder){ // a gap that can contain the Cylinder
					if (it->second<-nBytesCylinder) // the gap not yet entirely filled?
						contentLayout.insert( // shrunk new gap
							std::make_pair( it->first+nBytesCylinder, it->second+nBytesCylinder )
						);
					fPosition=it->first; // save Cylinder in this gap
					it->second=nBytesCylinder; // this gap is now the Cylinder
					break;
				}
			cylInfos[cyl].nBlocksOffset=Utils::RoundDivUp<DWORD>( fPosition, sizeof(TBlock) );
			cylInfos[cyl].nBytesLength=nBytesLongerTrack*2;
			if (fTarget.Seek( fPosition, CFile::begin )<fPosition)
				fTarget.SetLength(fPosition), fTarget.SeekToEnd();
			TCylinderBlock cylBlock;
			for( PCBYTE p0=head0, p1=head1; nBytesCylinder>0; nBytesCylinder-=sizeof(cylBlock) ){
				::ZeroMemory( &cylBlock, sizeof(cylBlock) );
				if (p0<head0.GetEnd())
					::memcpy( cylBlock.head[0].bytes, p0, sizeof(TTrackData) ),  p0+=sizeof(TTrackData);
				if (p1<head1.GetEnd())
					::memcpy( cylBlock.head[1].bytes, p1, sizeof(TTrackData) ),  p1+=sizeof(TTrackData);
				fTarget.Write( &cylBlock, sizeof(cylBlock) );
			}
		}
		// - consolidating/defragmenting the file
		if (savingToCurrentFile){ //TODO
			auto sub=ap.CreateSubactionProgress( ARRAYSIZE(cylInfos), f.GetLength() );
			//TODO
			//f.SetLength(f.GetPosition()); // "trimming" eventual unnecessary data (e.g. when unformatting Cylinders)
		}
		// - save CylInfos
		fTarget.Seek( sizeof(header), CFile::begin );
		fTarget.Write( cylInfos, sizeof(cylInfos) );
		ap.IncrementProgress();
		// - update and save Header
		header.nCylinders=GetCylinderCount();
		header.nHeads=GetHeadCount();
		header.dataBitRate=Medium::GetProperties(floppyType)->nCells/1000/2; // "/2" = only data bits, not clock bits
		ASSERT( header.IsValid() );
		fTarget.SeekToBegin();
		fTarget.Write( &header, sizeof(header) );
		ap.IncrementProgress();
		// - reopening Image's underlying file
}		//m_bModified=FALSE; // commented out as done by caller
		return OpenImageForReadingAndWriting(lpszPathName,f);
	}
