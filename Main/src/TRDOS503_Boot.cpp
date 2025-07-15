#include "stdafx.h"
#include "TRDOS.h"

	constexpr BYTE BOOT_ID=16;

	TPhysicalAddress CTRDOS503::TBootSector::GetPhysicalAddress(PCImage image){
		const TSide side= image->GetSideMap() ? *image->GetSideMap() : *StdSidesMap;
		const TPhysicalAddress chs={ 0, 0, {0,side,TRDOS503_BOOT_SECTOR_NUMBER,TRDOS503_SECTOR_LENGTH_STD_CODE} };
		return chs;
	}

	UINT AFX_CDECL CTRDOS503::TBootSector::Verification_thread(PVOID pCancelableAction){
		// thread to verify the Boot Sector
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const TSpectrumVerificationParams &vp=*(TSpectrumVerificationParams *)pAction->GetParams();
		vp.fReport.OpenSection(BOOT_SECTOR_TAB_LABEL);
		const auto trdos=static_cast<CTRDOS503 *>(vp.dos);
		const PImage image=trdos->image;
		const PBootSector boot=Get(image);
		if (!boot)
			return vp.TerminateAll( Utils::ErrorByOs(ERROR_VOLMGR_DISK_INVALID,ERROR_UNRECOGNIZED_VOLUME) );
		const TPhysicalAddress CHS=GetPhysicalAddress(image);
		// - verifying this is actually a TR-DOS disk
		if (const TStdWinError err=vp.VerifyUnsignedValue( CHS, BOOT_SECTOR_LOCATION_STRING, nullptr, boot->id, (BYTE)BOOT_ID ))
			return vp.TerminateAll(err);
		// - verifying constant fields
		if (const TStdWinError err=vp.VerifyUnsignedValue( CHS, BOOT_SECTOR_LOCATION_STRING, nullptr, boot->zero1, (BYTE)0 ))
			return vp.TerminateAll(err);
		if (const TStdWinError err=vp.VerifyUnsignedValue( CHS, BOOT_SECTOR_LOCATION_STRING, nullptr, boot->zero2, (BYTE)0 ))
			return vp.TerminateAll(err);
		// - verifying geometry
		if (const TStdWinError err=vp.VerifyUnsignedValue( CHS, BOOT_SECTOR_LOCATION_STRING, nullptr, boot->format, TDiskFormat::DS80, TDiskFormat::SS40 ))
			return vp.TerminateAll(err);
		// - verifying information on Directory
		BYTE nFiles=0, nDeletedFiles=0;
		for( TTrdosDirectoryTraversal dt(trdos); dt.AdvanceToNextEntry(); ){
			nFiles+=dt.entryType==TDirectoryTraversal::FILE;
			nDeletedFiles+=dt.entryType==TDirectoryTraversal::CUSTOM;
		}
		if (const TStdWinError err=vp.VerifyUnsignedValue( CHS, BOOT_SECTOR_LOCATION_STRING, VERIF_FILE_COUNT, boot->nFiles, nFiles ))
			return vp.TerminateAll(err);
		if (const TStdWinError err=vp.VerifyUnsignedValue( CHS, BOOT_SECTOR_LOCATION_STRING, _T("Count of deleted files"), boot->nFilesDeleted, nDeletedFiles ))
			return vp.TerminateAll(err);
		// - verifying information on free space
		if (const TStdWinError err=vp.VerifyUnsignedValue( CHS, BOOT_SECTOR_LOCATION_STRING, VERIF_SECTOR_FREE_COUNT, boot->nFreeSectors, (WORD)(trdos->formatBoot.GetCountOfAllSectors()-boot->firstFree.track*TRDOS503_TRACK_SECTORS_COUNT-boot->firstFree.sector) ))
			return vp.TerminateAll(err);
		// - verifying DiskName
		if (const TStdWinError err=vp.VerifyAllCharactersPrintable( CHS, BOOT_SECTOR_LOCATION_STRING, VERIF_VOLUME_NAME, boot->label, trdos->boot.nCharsInLabel, ' ' ))
			return vp.TerminateAll(err);
		// - Boot Sector verified
		return pAction->TerminateWithSuccess();
	}

	BYTE CTRDOS503::TBootSector::__getLabelLengthEstimation__() const{
		// estimates and returns the Label length (useful for more precise TR-DOS version recognition)
		if (const LPCSTR nullChar=(LPCSTR)::memchr(label,'\0',TRDOS503_BOOT_LABEL_LENGTH_MAX+sizeof(WORD)))
			return nullChar-label;
		else
			return TRDOS503_BOOT_LABEL_LENGTH_MAX+sizeof(WORD);
	}

	#define PASSWORD_FILLER_BYTE	' '

	void CTRDOS503::TBootSector::__init__(PCFormat pFormatBoot,BYTE nCharsInLabel,bool userDataInSysTrackAllowed){
		// initializes the Boot Sector to the specified Format
		::ZeroMemory(this,sizeof(*this));
		if (userDataInSysTrackAllowed){
			firstFree.sector=TRDOS503_BOOT_SECTOR_NUMBER; // firstFree.track = see ZeroMemory above
			nFreeSectors =	pFormatBoot->GetCountOfAllSectors()-TRDOS503_SECTOR_RESERVED_COUNT
							+
							TRDOS503_TRACK_SECTORS_COUNT+TRDOS503_SECTOR_FIRST_NUMBER-TRDOS503_BOOT_SECTOR_NUMBER;
		}else{
			firstFree.track=1; // firstFree.sector = see ZeroMemory above
			nFreeSectors =	pFormatBoot->GetCountOfAllSectors()-TRDOS503_SECTOR_RESERVED_COUNT;
		}
		id=BOOT_ID;
		::memcpy(	::memset( label, ' ', nCharsInLabel ),
					VOLUME_LABEL_DEFAULT_ANSI_8CHARS,
					sizeof(VOLUME_LABEL_DEFAULT_ANSI_8CHARS)-1
				);
		::memset( password, PASSWORD_FILLER_BYTE, TRDOS503_BOOT_PASSWORD_LENGTH_MAX );
		__setDiskType__(pFormatBoot);
	}
	void CTRDOS503::TBootSector::__setDiskType__(PCFormat pFormatBoot){
		// sets information on disk Format to one of predefined values that is closest
		if (pFormatBoot->nCylinders<=40)
			format= pFormatBoot->nHeads==1 ? SS40 : DS40 ;
		else
			format= pFormatBoot->nHeads==1 ? SS80 : DS80 ;
	}

	CTRDOS503::PBootSector CTRDOS503::TBootSector::Get(PImage image){
		// returns the data of Boot Sector (or Null if Boot unreadable)
		return	(PBootSector)image->GetHealthySectorData(
					GetPhysicalAddress( image )
				);
	}
	CTRDOS503::PBootSector CTRDOS503::GetBootSector() const{
		// returns the data of Boot Sector (or Null if Boot unreadable)
		return TBootSector::Get(image);
	}

	TStdWinError CTRDOS503::__recognizeDisk__(PImage image,PFormat pFormatBoot){
		// returns the result of attempting to recognize Image by this DOS as follows: ERROR_SUCCESS = recognized, ERROR_CANCELLED = user cancelled the recognition sequence, any other error = not recognized
		TFormat fmt={ Medium::FLOPPY_DD, Codec::MFM, 1,2,TRDOS503_TRACK_SECTORS_COUNT, TRDOS503_SECTOR_LENGTH_STD_CODE,TRDOS503_SECTOR_LENGTH_STD, 1 };
		if (image->SetMediumTypeAndGeometry(&fmt,image->GetSideMap(),1)!=ERROR_SUCCESS || !image->GetNumberOfFormattedSides(0)){
			fmt.mediumType=Medium::FLOPPY_DD_525;
			if (image->SetMediumTypeAndGeometry(&fmt,image->GetSideMap(),1)!=ERROR_SUCCESS || !image->GetNumberOfFormattedSides(0))
				return ERROR_UNRECOGNIZED_VOLUME; // unknown Medium Type
		}
		const PCBootSector boot=TBootSector::Get(image);
		if (boot && boot->id==BOOT_ID){
			*pFormatBoot=fmt;
			switch ( const BYTE f=boot->format ){
				case DS80:
				case DS40:
					pFormatBoot->nCylinders= f&1 ? 40 : 80;
					pFormatBoot->nHeads=2;
					break;
				case SS80:
				case SS40:
					pFormatBoot->nCylinders= f&1 ? 40 : 80;
					pFormatBoot->nHeads=1;
					break;
				default:
					return ERROR_UNRECOGNIZED_VOLUME;
			}
			return ERROR_SUCCESS;
		}else
			return ERROR_UNRECOGNIZED_VOLUME;
	}



	// 5.25" drives are likely 360 rpm ones in PC
	#define DS80_CAPTION	_T("3.5\" DS 80 cylinders")
	#define DS40_CAPTION	_T("5.25\" DS 40 cylinders")
	#define SS80_CAPTION	_T("3.5\" SS 80 cylinders")
	#define SS40_CAPTION	_T("5.25\" SS 40 cylinders")

	static PDos __instantiate__(PImage image,PCFormat pFormatBoot){
		return new CTRDOS503(image,pFormatBoot,&CTRDOS503::Properties);
	}

	#define TRDOS_SECTOR_GAP3	32 /* smaller than regular IBM norm-compliant Gap to make sure all 16 Sectors fit in a Track */

	// 5.25" drives are likely 360 rpm ones in PC
	const CFormatDialog::TStdFormat CTRDOS503::StdFormats[]={ // zeroth position must always be occupied by the biggest capacity
		{ DS80_CAPTION, 0, {Medium::FLOPPY_DD,    Codec::MFM,79,2,TRDOS503_TRACK_SECTORS_COUNT,TRDOS503_SECTOR_LENGTH_STD_CODE,TRDOS503_SECTOR_LENGTH_STD,1}, 1, 0, TRDOS_SECTOR_GAP3, 0, 128 },
		{ DS40_CAPTION, 0, {Medium::FLOPPY_DD_525,Codec::MFM,39,2,TRDOS503_TRACK_SECTORS_COUNT,TRDOS503_SECTOR_LENGTH_STD_CODE,TRDOS503_SECTOR_LENGTH_STD,1}, 1, 0, TRDOS_SECTOR_GAP3, 0, 128 },
		{ SS80_CAPTION, 0, {Medium::FLOPPY_DD,	  Codec::MFM,79,1,TRDOS503_TRACK_SECTORS_COUNT,TRDOS503_SECTOR_LENGTH_STD_CODE,TRDOS503_SECTOR_LENGTH_STD,1}, 1, 0, TRDOS_SECTOR_GAP3, 0, 128 },
		{ DS40_CAPTION, 0, {Medium::FLOPPY_DD,    Codec::MFM,39,2,TRDOS503_TRACK_SECTORS_COUNT,TRDOS503_SECTOR_LENGTH_STD_CODE,TRDOS503_SECTOR_LENGTH_STD,1}, 1, 0, TRDOS_SECTOR_GAP3, 0, 128 },
		{ SS40_CAPTION, 0, {Medium::FLOPPY_DD,	  Codec::MFM,39,1,TRDOS503_TRACK_SECTORS_COUNT,TRDOS503_SECTOR_LENGTH_STD_CODE,TRDOS503_SECTOR_LENGTH_STD,1}, 1, 0, TRDOS_SECTOR_GAP3, 0, 128 },
		{ SS40_CAPTION, 0, {Medium::FLOPPY_DD_525,Codec::MFM,39,1,TRDOS503_TRACK_SECTORS_COUNT,TRDOS503_SECTOR_LENGTH_STD_CODE,TRDOS503_SECTOR_LENGTH_STD,1}, 1, 0, TRDOS_SECTOR_GAP3, 0, 128 }
	};
	const CDos::TProperties CTRDOS503::Properties={
		TRDOS_NAME_BASE _T(" 5.03"), // name
		MAKE_DOS_ID('T','R','D','O','S','5','0','3'), // unique identifier
		40, // recognition priority (the bigger the number the earlier the DOS gets crack on the image)
		0, // the Cylinder where usually the Boot Sector (or its backup) is found
		__recognizeDisk__, // recognition function
		__instantiate__, // instantiation function
		Medium::FLOPPY_DD_ANY,
		&TRD::Properties, // the most common Image to contain data for this DOS (e.g. *.D80 Image for MDOS)
		ARRAYSIZE(StdFormats),	// number of std Formats
		StdFormats, // std Formats
		Codec::MFM, // a set of Codecs this DOS supports
		TRDOS503_TRACK_SECTORS_COUNT,TRDOS503_TRACK_SECTORS_COUNT, // range of supported number of Sectors
		TRDOS503_SECTOR_RESERVED_COUNT, // minimal total number of Sectors required
		1, // maximum number of Sector in one Cluster (must be power of 2)
		-1, // maximum size of a Cluster (in Bytes)
		0,0, // range of supported number of allocation tables (FATs)
		128,128, // range of supported number of root Directory entries
		TRDOS503_SECTOR_FIRST_NUMBER,	// lowest Sector number on each Track
		0,TDirectoryEntry::END_OF_DIR,	// regular Sector and Directory Sector filler Byte
		0,0 // number of reserved Bytes at the beginning and end of each Sector
	};
	









	CTRDOS503::CTrdosBootView::CTrdosBootView(PTRDOS503 trdos,BYTE nCharsInLabel)
		// ctor
		: CBootView( trdos, TBootSector::GetPhysicalAddress(trdos->image) )
		, nCharsInLabel(nCharsInLabel) {
	}










	void CTRDOS503::CTrdosBootView::GetCommonBootParameters(RCommonBootParameters rParam,PSectorData boot){
		// gets basic parameters from the Boot Sector
		rParam.geometryCategory=true;
			rParam.chs=false;
		rParam.volumeCategory=true;
			/*rParam.label.length=nCharsInLabel; // commented out as the default text editor isn't suitable to input Speccy keywords in disk label (e.g. "RETURN TO Zork" with keyword capitalized)
				rParam.label.bufferA=((PBootSector)boot)->label;
				rParam.label.fillerByte=' ';*/
	}

	PropGrid::Enum::PCValueList WINAPI CTRDOS503::CTrdosBootView::ListAvailableFormats(PVOID pBoot,PropGrid::Enum::UValue format,WORD &rnFormats){
		// returns the list of known Formats
		static constexpr TDiskFormat List[]={ DS80, DS40, SS80, SS40 };
		return rnFormats=2, (PropGrid::Enum::PCValueList)(List+2*(format.charValue>=(char)SS80)); // mustn't change # of Heads!
	}
	LPCTSTR WINAPI CTRDOS503::CTrdosBootView::__getFormatDescription__(PVOID,PropGrid::Enum::UValue format,PTCHAR buf,short bufCapacity){
		// populates the Buffer with a description of specified Format and returns the Buffer
		switch ((TDiskFormat)format.charValue){
			case DS80:	return DS80_CAPTION;
			case DS40:	return DS40_CAPTION;
			case SS80:	return SS80_CAPTION;
			case SS40:	return SS40_CAPTION;
			default:
				return _T("<unknown>");
		}
	}
	bool WINAPI CTRDOS503::CTrdosBootView::__onFormatChanged__(PVOID,PropGrid::Enum::UValue newValue){
		// disk Format changed through PropertyGrid
		CTRDOS503 *const trdos=(CTRDOS503 *)CDos::GetFocused();
		// - validating new Format
		TFormat fmt=trdos->formatBoot;
		switch ((TDiskFormat)newValue.charValue){
			case DS80:
				fmt.mediumType=Medium::FLOPPY_DD;
				fmt.nCylinders=80,fmt.nHeads=2; break;
			case DS40:
				fmt.mediumType=Medium::FLOPPY_DD_525; // likely 360 rpm in PC
				fmt.nCylinders=40,fmt.nHeads=2; break;
			case SS80:
				fmt.mediumType=Medium::FLOPPY_DD;
				fmt.nCylinders=80,fmt.nHeads=1; break;
			case SS40:
				fmt.mediumType=Medium::FLOPPY_DD_525; // likely 360 rpm in PC
				fmt.nCylinders=40,fmt.nHeads=1; break;
		}
		if (!trdos->ValidateFormatAndReportProblem( true, true, fmt, DOS_MSG_HIT_ESC ))
			return false;
		// - accepting new Format
		const PBootSector boot=trdos->GetBootSector(); // guaranteed to be found, otherwise 'ValidateFormat' would have returned False
		trdos->formatBoot=fmt;
		// - adjusting relevant information in the Boot Sector
		boot->nFreeSectors-=pf->nCylinders*pf->nHeads*pf->nSectors;
		boot->nFreeSectors+=fmt.nCylinders*fmt.nHeads*fmt.nSectors;
		return __bootSectorModified__(nullptr,0);
	}

	CString CTRDOS503::ValidateFormat(bool considerBoot,bool considerFat,RCFormat f) const{
		// returns reason why specified new Format cannot be accepted, or empty string if Format acceptable
		// - base
		CString &&err=__super::ValidateFormat( considerBoot, considerFat, f );
		if (!err.IsEmpty())
			return err;
		// - must be one of default Formats
		auto tmpFmt=f;
			tmpFmt.nCylinders--; // inclusive!
		for each( const auto &stdFmt in StdFormats ){
			tmpFmt.mediumType=stdFmt.params.format.mediumType; // ignore Medium (unreliable for Images)
			if (stdFmt.params.format==tmpFmt)
				return err;
		}
		// - new Format is acceptable
		return _T("Format not allowed");
	}

	#define CYGNUSBOOT_NAME			_T("CygnusBoot 2.2.3")
	#define CYGNUSBOOT_IMPORT_NAME	_T("boot.B ZXP35000aL10e2S11")
	#define CYGNUSBOOT_ONLINE_NAME	_T("TRDOS/CygnusBoot/") CYGNUSBOOT_IMPORT_NAME

	static bool WINAPI __cygnusBoot_updateOnline__(PropGrid::PCustomParam,int hyperlinkId,LPCTSTR hyperlinkName){
		// True <=> PropertyGrid's Editor can be destroyed after this function has terminated, otherwise False
		BYTE cygnusBootDataBuffer[8192]; // sufficiently big buffer
		DWORD cygnusBootDataLength;
		TCHAR cygnusBootUrl[200];
		TStdWinError err =	Utils::DownloadSingleFile( // also displays the error message in case of problems
								Utils::GetApplicationOnlineFileUrl( CYGNUSBOOT_ONLINE_NAME, cygnusBootUrl ),
								cygnusBootDataBuffer, sizeof(cygnusBootDataBuffer), &cygnusBootDataLength,
								TRDOS503_BOOTB_NOT_MODIFIED
							);
		if (err==ERROR_SUCCESS){
			CDos::PFile tmp;
			DWORD conflictResolution=CFileManagerView::TConflictResolution::UNDETERMINED;
			if ( err=CDos::GetFocused()->pFileManager->ImportFileAndResolveConflicts( &CMemFile(cygnusBootDataBuffer,sizeof(cygnusBootDataBuffer)), cygnusBootDataLength, CYGNUSBOOT_IMPORT_NAME, 0, FILETIME(), FILETIME(), FILETIME(), tmp, conflictResolution ) )
				Utils::FatalError( _T("Cannot import ") CYGNUSBOOT_NAME, err, TRDOS503_BOOTB_NOT_MODIFIED );
		}
		return true; // True = destroy PropertyGrid's Editor
	}

	void CTRDOS503::CTrdosBootView::AddCustomBootParameters(HWND hPropGrid,HANDLE hGeometry,HANDLE hVolume,const TCommonBootParameters &rParam,PSectorData _boot){
		// gets DOS-specific parameters from the Boot
		const PBootSector boot=(PBootSector)_boot;
		// - Geometry category
		PropGrid::AddProperty(	hPropGrid, hGeometry, _T("Format"),
								&boot->format,
								PropGrid::Enum::DefineConstStringListEditor( sizeof(TDiskFormat), ListAvailableFormats, __getFormatDescription__, nullptr, __onFormatChanged__ )
							);
		// - Volume category
		PropGrid::AddProperty(	hPropGrid, hVolume, _T("Label"),
								boot->label,
								TZxRom::CLineComposerPropGridEditor::Define( nCharsInLabel, ' ', nullptr, __bootSectorModified__ )
							);
		PropGrid::AddProperty(	hPropGrid, hVolume, _T("Password"),
								boot->password,
								PropGrid::String::DefineFixedLengthEditorA( TRDOS503_BOOT_PASSWORD_LENGTH_MAX, __bootSectorModifiedA__, PASSWORD_FILLER_BYTE )
							);
		// - Advanced category
		const HANDLE hAdvanced=PropGrid::AddCategory(hPropGrid,nullptr,BOOT_SECTOR_ADVANCED);
			const PropGrid::PCEditor advByteEditor=PropGrid::Integer::DefineByteEditor(__bootSectorModified__);
			PropGrid::AddProperty(	hPropGrid, hAdvanced, _T("Files"),
									&boot->nFiles, advByteEditor
								);
			PropGrid::AddProperty(	hPropGrid, hAdvanced, _T("Deleted files"),
									&boot->nFilesDeleted, advByteEditor
								);
			const HANDLE hFirstFreeSector=PropGrid::AddCategory(hPropGrid,hAdvanced,_T("First empty sector"));
				PropGrid::AddProperty(	hPropGrid, hFirstFreeSector, _T("Track number"),
										&boot->firstFree.track, advByteEditor
									);
				PropGrid::AddProperty(	hPropGrid, hFirstFreeSector, _T("Sector ID"),
										&boot->firstFree.sector, advByteEditor
									);
			PropGrid::AddProperty(	hPropGrid, hAdvanced, _T("Free sectors"),
									&boot->nFreeSectors,
									PropGrid::Integer::DefineWordEditor(__bootSectorModified__)
								);
		// - CygnusBoot category
		const HANDLE hCygnusBoot=PropGrid::AddCategory(hPropGrid,nullptr,CYGNUSBOOT_NAME);
			PropGrid::AddProperty(	hPropGrid, hCygnusBoot, _T("boot.B"),
									BOOT_SECTOR_UPDATE_ONLINE_HYPERLINK,
									PropGrid::Hyperlink::DefineEditorT(__cygnusBoot_updateOnline__)
								);
	}












	RCPhysicalAddress CTRDOS503::GetBootSectorAddress() const{
		// returns the PhysicalAddress of the boot Sector in use, or Invalid
		return boot.GetPhysicalAddress();
	}

	void CTRDOS503::FlushToBootSector() const{
		// flushes internal Format information to the actual Boot Sector's data
		if (const PBootSector boot=GetBootSector()){
			boot->__setDiskType__(&formatBoot);
			this->boot.MarkSectorAsDirty();
		}
	}

	void CTRDOS503::InitializeEmptyMedium(CFormatDialog::PCParameters,CActionProgress &){
		// initializes a fresh formatted Medium (Boot, FAT, root dir, etc.)
		// . initializing the Boot Sector
		if (const PBootSector bootSector=GetBootSector()){
			bootSector->__init__( &formatBoot, boot.nCharsInLabel, importToSysTrack );
			FlushToBootSector(); // already carried out in CDos::__formatStdCylinders__ but overwritten by ZeroMemory above
		}
		// . empty Directory
		//nop (set automatically by formatting to default FillerByte)
		// . notify respective view about new BootSector
		boot.ChangeToSector( TBootSector::GetPhysicalAddress(image) );
	}










	namespace TRD{
		static const CImage::TProperties Properties={
			MAKE_IMAGE_ID('T','R','D','O','S','T','R','D'), // a unique identifier
			Recognize,// name
			Instantiate,// instantiation function
			_T("*.trd"),	// filter
			Medium::FLOPPY_DD_ANY,
			Codec::MFM, // supported Codecs
			TRDOS503_SECTOR_LENGTH_STD,TRDOS503_SECTOR_LENGTH_STD	// min and max length of storable Sectors
		};

		LPCTSTR Recognize(PTCHAR){
			static constexpr TCHAR SingleDeviceName[]=_T("TR-DOS image\0");
			return SingleDeviceName;
		}
		PImage Instantiate(LPCTSTR){
			return new CImageRaw( &Properties, true );
		}
	}
