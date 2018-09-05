#include "stdafx.h"
#include "TRDOS.h"

	#define BOOT_ID	16

	const TPhysicalAddress CTRDOS503::TBootSector::CHS={ 0, 0, {0,0,TRDOS503_BOOT_SECTOR_NUMBER,TRDOS503_SECTOR_LENGTH_STD_CODE} };

	BYTE CTRDOS503::TBootSector::__getLabelLengthEstimation__() const{
		// estimates and returns the Label length (useful for more precise TR-DOS version recognition)
		if (const LPCSTR nullChar=(LPCSTR)::memchr(label,'\0',TRDOS503_BOOT_LABEL_LENGTH_MAX+sizeof(WORD)))
			return nullChar-label;
		else
			return TRDOS503_BOOT_LABEL_LENGTH_MAX+sizeof(WORD);
	}

	#define PASSWORD_FILLER_BYTE	' '

	void CTRDOS503::TBootSector::__init__(PCFormat pFormatBoot,BYTE nCharsInLabel){
		// initializes the Boot Sector to the specified Format
		::ZeroMemory(this,sizeof(*this));
		firstFreeTrack=1;
		nFreeSectors=pFormatBoot->GetCountOfAllSectors()-TRDOS503_SECTOR_RESERVED_COUNT;
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

	CTRDOS503::PBootSector CTRDOS503::__getBootSector__(PImage image){
		// returns the data of Boot Sector (or Null if Boot unreadable)
		return (CTRDOS503::PBootSector)image->GetSectorData(TBootSector::CHS);
	}
	CTRDOS503::PBootSector CTRDOS503::__getBootSector__() const{
		// returns the data of Boot Sector (or Null if Boot unreadable)
		return __getBootSector__(image);
	}

	TStdWinError CTRDOS503::__recognizeDisk__(PImage image,PFormat pFormatBoot){
		// returns the result of attempting to recognize Image by this DOS as follows: ERROR_SUCCESS = recognized, ERROR_CANCELLED = user cancelled the recognition sequence, any other error = not recognized
		static const TFormat Fmt={ TMedium::FLOPPY_DD, 1,2,TRDOS503_TRACK_SECTORS_COUNT, TRDOS503_SECTOR_LENGTH_STD_CODE,TRDOS503_SECTOR_LENGTH_STD, 1 };
		if (const TStdWinError err=image->SetMediumTypeAndGeometry( &Fmt, StdSidesMap, TRDOS503_SECTOR_FIRST_NUMBER ))
			return err;
		const PCBootSector boot=(PCBootSector)image->GetSectorData(TBootSector::CHS);
		if (boot && boot->id==BOOT_ID){
			*pFormatBoot=Fmt;
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



	#define CYLINDER_COUNT_MIN	1
	#define CYLINDER_COUNT_MAX	FDD_CYLINDERS_MAX

	#define DS80_CAPTION	_T("DS 80 cylinders")
	#define DS40_CAPTION	_T("DS 40 cylinders")
	#define SS80_CAPTION	_T("SS 80 cylinders")
	#define SS40_CAPTION	_T("SS 40 cylinders")

	static PDos __instantiate__(PImage image,PCFormat pFormatBoot){
		return new CTRDOS503(image,pFormatBoot,&CTRDOS503::Properties);
	}
	const CFormatDialog::TStdFormat CTRDOS503::StdFormats[]={ // zeroth position must always be occupied by the biggest capacity
		{ DS80_CAPTION, 0, {TMedium::FLOPPY_DD,79,2,TRDOS503_TRACK_SECTORS_COUNT,TRDOS503_SECTOR_LENGTH_STD_CODE,TRDOS503_SECTOR_LENGTH_STD,1}, 1, 0, FDD_SECTOR_GAP3_STD, 0, 128 },
		{ DS40_CAPTION, 0, {TMedium::FLOPPY_DD,39,2,TRDOS503_TRACK_SECTORS_COUNT,TRDOS503_SECTOR_LENGTH_STD_CODE,TRDOS503_SECTOR_LENGTH_STD,1}, 1, 0, FDD_SECTOR_GAP3_STD, 0, 128 },
		{ SS80_CAPTION, 0, {TMedium::FLOPPY_DD,79,1,TRDOS503_TRACK_SECTORS_COUNT,TRDOS503_SECTOR_LENGTH_STD_CODE,TRDOS503_SECTOR_LENGTH_STD,1}, 1, 0, FDD_SECTOR_GAP3_STD, 0, 128 },
		{ SS40_CAPTION, 0, {TMedium::FLOPPY_DD,39,1,TRDOS503_TRACK_SECTORS_COUNT,TRDOS503_SECTOR_LENGTH_STD_CODE,TRDOS503_SECTOR_LENGTH_STD,1}, 1, 0, FDD_SECTOR_GAP3_STD, 0, 128 }
	};
	const CDos::TProperties CTRDOS503::Properties={
		TRDOS_NAME_BASE _T(" 5.03"), // name
		MAKE_DOS_ID('T','R','D','O','S','5','0','3'), // unique identifier
		40, // recognition priority (the bigger the number the earlier the DOS gets crack on the image)
		__recognizeDisk__, // recognition function
		__instantiate__, // instantiation function
		TMedium::FLOPPY_DD,
		4,	// number of std Formats
		StdFormats, // std Formats
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
		: CBootView(trdos,TBootSector::CHS)
		, nCharsInLabel(nCharsInLabel) {
	}










	void CTRDOS503::CTrdosBootView::GetCommonBootParameters(RCommonBootParameters rParam,PSectorData boot){
		// gets basic parameters from the Boot Sector
		rParam.geometryCategory=true;
			rParam.chs=false;
		rParam.volumeCategory=true;
			rParam.label.length=nCharsInLabel;
			rParam.label.bufferA=((PBootSector)boot)->label;
			rParam.label.fillerByte=' ';
	}

	CPropGridCtrl::TEnum::PCValueList WINAPI CTRDOS503::CTrdosBootView::__getListOfKnownFormats__(PVOID,WORD &rnFormats){
		// returns the list of known Formats
		static const TDiskFormat List[]={ DS80, DS40, SS80, SS40 };
		rnFormats=4;
		return (CPropGridCtrl::TEnum::PCValueList)List;
	}
	LPCTSTR WINAPI CTRDOS503::CTrdosBootView::__getFormatDescription__(PVOID,CPropGridCtrl::TEnum::UValue format,PTCHAR buf,short bufCapacity){
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
	bool WINAPI CTRDOS503::CTrdosBootView::__onFormatChanged__(PVOID,CPropGridCtrl::TEnum::UValue newValue){
		// disk Format changed through PropertyGrid
		CTRDOS503 *const trdos=(CTRDOS503 *)CDos::__getFocused__();
		const PBootSector boot=trdos->__getBootSector__();
		if (!boot) return false;
		// - validating new Format
		const PFormat pf=&trdos->formatBoot;
		TFormat fmt=*pf;
		switch ((TDiskFormat)newValue.charValue){
			case DS80:
				fmt.nCylinders=80,fmt.nHeads=2; break;
			case DS40:
				fmt.nCylinders=40,fmt.nHeads=2; break;
			case SS80:
				fmt.nCylinders=80,fmt.nHeads=1; break;
			case SS40:
				fmt.nCylinders=40,fmt.nHeads=1; break;
		}
		if (!trdos->ValidateFormatChangeAndReportProblem(false,&fmt))
			return false;
		if (boot->firstFreeTrack/fmt.nHeads>=fmt.nCylinders){
			TUtils::Information(_T("Cannot modify the format as there are occupied sectors exceeding it."));
			return false;
		}
		// - accepting new Format
		*pf=fmt;
		// - adjusting relevant information in the Boot Sector
		boot->nFreeSectors-=pf->nCylinders*pf->nHeads*pf->nSectors;
		boot->nFreeSectors+=fmt.nCylinders*fmt.nHeads*fmt.nSectors;
		return __bootSectorModified__(NULL,0);
	}

	void CTRDOS503::CTrdosBootView::AddCustomBootParameters(HWND hPropGrid,HANDLE hGeometry,HANDLE hVolume,PSectorData _boot){
		// gets DOS-specific parameters from the Boot
		const PBootSector boot=(PBootSector)_boot;
		// - Geometry category
		CPropGridCtrl::AddProperty(	hPropGrid, hGeometry, _T("Format"),
									&boot->format, sizeof(TDiskFormat),
									CPropGridCtrl::TEnum::DefineConstStringListEditorA( __getListOfKnownFormats__, __getFormatDescription__, NULL, __onFormatChanged__ )
								);
		// - Volume category
		CPropGridCtrl::AddProperty( hPropGrid, hVolume, _T("Password"),
									boot->password, TRDOS503_BOOT_PASSWORD_LENGTH_MAX,
									CPropGridCtrl::TString::DefineFixedLengthEditorA( __bootSectorModified__, PASSWORD_FILLER_BYTE )
								);
		// - Advanced category
		const HANDLE hAdvanced=CPropGridCtrl::AddCategory(hPropGrid,NULL,BOOT_SECTOR_ADVANCED);
			const CPropGridCtrl::PCEditor advByteEditor=CPropGridCtrl::TInteger::DefineByteEditor(__bootSectorModified__);
			CPropGridCtrl::AddProperty( hPropGrid, hAdvanced, _T("Files"),
										&boot->nFiles, sizeof(BYTE), advByteEditor
									);
			CPropGridCtrl::AddProperty( hPropGrid, hAdvanced, _T("Deleted files"),
										&boot->nFilesDeleted, sizeof(BYTE), advByteEditor
									);
			const HANDLE hFirstFreeSector=CPropGridCtrl::AddCategory(hPropGrid,hAdvanced,_T("First empty sector"));
				CPropGridCtrl::AddProperty( hPropGrid, hFirstFreeSector, _T("Track number"),
											&boot->firstFreeTrack, sizeof(BYTE), advByteEditor
										);
				CPropGridCtrl::AddProperty( hPropGrid, hFirstFreeSector, _T("Sector ID"),
											&boot->firstFreeSector, sizeof(BYTE), advByteEditor
										);
			CPropGridCtrl::AddProperty( hPropGrid, hAdvanced, _T("Free sectors"),
										&boot->nFreeSectors, sizeof(WORD),
										CPropGridCtrl::TInteger::DefineWordEditor(__bootSectorModified__)
									);
	}












	void CTRDOS503::FlushToBootSector() const{
		// flushes internal Format information to the actual Boot Sector's data
		if (const PBootSector boot=__getBootSector__()){
			boot->__setDiskType__(&formatBoot);
			image->MarkSectorAsDirty(TBootSector::CHS);
		}
	}

	void CTRDOS503::InitializeEmptyMedium(CFormatDialog::PCParameters){
		// initializes a fresh formatted Medium (Boot, FAT, root dir, etc.)
		// . initializing the Boot Sector
		if (const PBootSector bootSector=__getBootSector__()){
			bootSector->__init__( &formatBoot, boot.nCharsInLabel );
			FlushToBootSector(); // already carried out in CDos::__formatStdCylinders__ but overwritten by ZeroMemory above
		}
		// . empty Directory
		//nop (set automatically by formatting to default FillerByte)
	}










	namespace TRD{
		static PImage __instantiate__(){
			return new CImageRaw( &Properties, true );
		}
		const CImage::TProperties Properties={	_T("TR-DOS image"),// name
												__instantiate__,// instantiation function
												_T("*.trd"),	// filter
												TMedium::FLOPPY_DD,
												TRDOS503_SECTOR_LENGTH_STD,TRDOS503_SECTOR_LENGTH_STD	// min and max length of storable Sectors
											};
	}
