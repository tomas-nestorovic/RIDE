#include "stdafx.h"
#include "MDOS2.h"


	#define SDOS_TEXT	0x534F4453 /* DWORD containing the "SDOS" string */

	const TPhysicalAddress CMDOS2::TBootSector::CHS={ 0, 0, {0,0,1,MDOS2_SECTOR_LENGTH_STD_CODE} };

	TStdWinError CMDOS2::__recognizeDisk__(PImage image,PFormat pFormatBoot){
		// returns the result of attempting to recognize Image by this DOS as follows: ERROR_SUCCESS = recognized, ERROR_CANCELLED = user cancelled the recognition sequence, any other error = not recognized
		static const TFormat Fmt={ TMedium::FLOPPY_DD, 1,1,10, MDOS2_SECTOR_LENGTH_STD_CODE,MDOS2_SECTOR_LENGTH_STD, 1 };
		if (image->SetMediumTypeAndGeometry(&Fmt,StdSidesMap,1)==ERROR_SUCCESS)
			if (const PCBootSector boot=(PCBootSector)image->GetSectorData(TBootSector::CHS))
				if (boot->sdos==SDOS_TEXT){
					*pFormatBoot=Fmt;
					if (( pFormatBoot->nCylinders=boot->currDrive.disk.nCylinders )
						*
						( pFormatBoot->nHeads=1+(boot->currDrive.disk.diskFlags>>4) )
						*
						( pFormatBoot->nSectors=boot->currDrive.disk.nSectors )
						>= // testing minimal number of Sectors
						MDOS2_DATA_LOGSECTOR_FIRST
					)
						return ERROR_SUCCESS;
				}
		return ERROR_UNRECOGNIZED_VOLUME;
	}

	#define CYLINDER_COUNT_MIN	2
	#define CYLINDER_COUNT_MAX	FDD_CYLINDERS_MAX

	static PDos __instantiate__(PImage image,PCFormat pFormatBoot){
		return new CMDOS2(image,pFormatBoot);
	}
	static const CFormatDialog::TStdFormat StdFormats[]={
		{ _T("DS 80x9"), 0, {TMedium::FLOPPY_DD,79,2,9,MDOS2_SECTOR_LENGTH_STD_CODE,MDOS2_SECTOR_LENGTH_STD,1}, 1, 0, FDD_SECTOR_GAP3_STD, 1, 128 },
		{ _T("DS 40x9 (beware under MDOS1!)"), 0, {TMedium::FLOPPY_DD,39,2,9,MDOS2_SECTOR_LENGTH_STD_CODE,MDOS2_SECTOR_LENGTH_STD,1}, 1, 0, FDD_SECTOR_GAP3_STD, 1, 128 }
	};
	const CDos::TProperties CMDOS2::Properties={
		_T("MDOS 2.0"), // name
		MAKE_DOS_ID('M','D','O','S','2','0','_','_'), // unique identifier
		80, // recognition priority (the bigger the number the earlier the DOS gets crack on the image)
		__recognizeDisk__, // recognition function
		__instantiate__, // instantiation function
		TMedium::FLOPPY_DD,
		2,	// number of std Formats
		StdFormats, // std Formats
		1,10, // range of supported number of Sectors
		MDOS2_DATA_LOGSECTOR_FIRST, // minimal total number of Sectors required
		1, // maximum number of Sector in one Cluster (must be power of 2)
		-1, // maximum size of a Cluster (in Bytes)
		1,1, // range of supported number of allocation tables (FATs)
		128,128, // range of supported number of root Directory entries
		1,	// lowest Sector number on each Track
		0xe5,TDirectoryEntry::EMPTY_ENTRY,	// regular Sector and Directory Sector filler Byte
		0,0 // number of reserved Bytes at the beginning and end of each Sector
	};









	struct TDriveInfo sealed{
		int connected, error, letter, d40, stepping, lastTrack, nCylinders, nSectors;
		int twosideFloppy, dsk40cyl;

		TDriveInfo(PSectorData m){
			// ctor
			connected=*m&1;
			error=(*m&2)>>1;
			lastTrack=*(m+=4);
			letter=(*++m&4)>>2;
			d40=(*m&8)>>3;
			twosideFloppy=(*m&16)>>4;
			dsk40cyl=(*m&32)>>5;
			stepping=(*m&192)>>6;
			nCylinders=*++m;
			nSectors=*++m;
		}
	};

	void WINAPI CMDOS2::TBootSector::TDiskAndDriveInfo::__pg_drawProperty__(PVOID,LPCVOID diskAndDriveInfo,short,PDRAWITEMSTRUCT pdis){
		// drawing the summary on MDOS drive
		const TDiskAndDriveInfo *const pddi=(PDiskAndDriveInfo)diskAndDriveInfo; // drive information
		TCHAR buf[30];
		if (pddi->disk.driveFlags&1){
			const TDriveInfo di((PSectorData)pddi);
			::wsprintf( buf, _T(" %c: D%d, %cS %dx%d"), 'A'+di.letter, 80-di.d40*40, di.twosideFloppy?'D':'S', di.nCylinders, di.nSectors );
		}else
			::lstrcpy(buf,_T(" Disconnected"));
		::DrawText(	pdis->hDC, buf,-1, &pdis->rcItem, DT_SINGLELINE|DT_LEFT|DT_VCENTER );
	}
	bool WINAPI CMDOS2::TBootSector::TDiskAndDriveInfo::__pg_editProperty__(PVOID,PVOID diskAndDriveInfo,short){
		// True <=> editing of MDOS drive confirmed, otherwise False
		// - defining the Dialog
		class CEditDialog sealed:public CDialog{
		public:
			TDriveInfo di;
		private:
			void DoDataExchange(CDataExchange *pDX) override{
				// exchange of data from and to controls
				DDX_Check(	pDX, ID_CONNECTED	,di.connected);
				DDX_Check(	pDX, ID_ERROR		,di.error);
				DDX_Radio(	pDX, ID_DRIVEA		,di.letter);
				DDX_Radio(	pDX, ID_D80			,di.d40);
				DDX_Check(	pDX, ID_DSFLOPPY	,di.twosideFloppy);
				DDX_Check(	pDX, ID_40D80		,di.dsk40cyl);
				DDX_CBIndex(pDX, ID_STEPPING	,di.stepping);
				DDX_Text(	pDX, ID_TRACK		,di.lastTrack);
					DDV_MinMaxInt(	pDX, di.lastTrack, 0, 255 );
				DDX_Text(	pDX, ID_CYLINDER	,di.nCylinders);
					DDV_MinMaxInt(	pDX, di.nCylinders, CYLINDER_COUNT_MIN, CYLINDER_COUNT_MAX );
				DDX_Text(	pDX, ID_SECTOR		,di.nSectors);
					DDV_MinMaxInt(	pDX, di.nSectors, MDOS2_TRACK_SECTORS_MIN, MDOS2_TRACK_SECTORS_MAX );
			}
		public:
			CEditDialog(PDiskAndDriveInfo pddi)
				// ctor
				: CDialog(IDR_MDOS_DRIVE_EDITOR)
				, di((PSectorData)pddi) {
			}
		} d((PDiskAndDriveInfo)diskAndDriveInfo);
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			PSectorData p=(PSectorData)diskAndDriveInfo; // drive information
			*p=d.di.connected|(d.di.error<<1);
			*(p+=4)=d.di.lastTrack;
			*++p=( d.di.letter|(d.di.d40<<1)|(d.di.twosideFloppy<<2)|(d.di.dsk40cyl<<3)|(d.di.stepping<<4) )<<2;
			*++p=d.di.nCylinders;
			*++p=d.di.nSectors;
			return CBootView::__bootSectorModified__(NULL,0);
		}else
			return false;
	}








	CMDOS2::CMdos2BootView::CMdos2BootView(PMDOS2 mdos)
		// ctor
		// - base
		: CBootView(mdos,TBootSector::CHS) {
	}










	void CMDOS2::CMdos2BootView::GetCommonBootParameters(RCommonBootParameters rParam,PSectorData boot){
		// gets basic parameters from the Boot Sector
		rParam.geometryCategory=true;
			rParam.chs=true;
		rParam.volumeCategory=true;
			rParam.label.length=MDOS2_VOLUME_LABEL_LENGTH_MAX;
				rParam.label.bufferA=( (PBootSector)boot )->label;
				rParam.label.fillerByte='\0';
			rParam.id.buffer=&( (PBootSector)boot )->diskID;
				rParam.id.bufferCapacity=sizeof(WORD);
	}

	#define UNIRUN_NAME			_T("UniRUN 2.0")
	#define UNIRUN_IMPORT_NAME	_T("run.P ZXP98000aL1000T8")
	#define UNIRUN_ONLINE_NAME	_T("MDOS2/UniRUN/") UNIRUN_IMPORT_NAME

	static bool WINAPI __unirun_updateOnline__(CPropGridCtrl::PCustomParam,int hyperlinkId,LPCTSTR hyperlinkName){
		// True <=> PropertyGrid's Editor can be destroyed after this function has terminated, otherwise False
		BYTE unirunDataBuffer[8192]; // sufficiently big buffer
		DWORD unirunDataLength;
		TCHAR unirunUrl[200];
		TStdWinError err =	Utils::DownloadSingleFile( // also displays the error message in case of problems
								Utils::GetApplicationOnlineFileUrl( UNIRUN_ONLINE_NAME, unirunUrl ),
								unirunDataBuffer, sizeof(unirunDataBuffer), &unirunDataLength,
								MDOS2_RUNP_NOT_MODIFIED
							);
		if (err==ERROR_SUCCESS){
			CDos::PFile tmp;
			CFileManagerView::TConflictResolution conflictResolution=CFileManagerView::TConflictResolution::UNDETERMINED;
			if ( err=CDos::__getFocused__()->pFileManager->ImportFileAndResolveConflicts( &CMemFile(unirunDataBuffer,sizeof(unirunDataBuffer)), unirunDataLength, UNIRUN_IMPORT_NAME, 0, tmp, conflictResolution ) )
				Utils::FatalError( _T("Cannot import ") UNIRUN_NAME, err, MDOS2_RUNP_NOT_MODIFIED );
		}
		return true; // True = destroy PropertyGrid's Editor
	}

	void CMDOS2::CMdos2BootView::AddCustomBootParameters(HWND hPropGrid,HANDLE hGeometry,HANDLE hVolume,const TCommonBootParameters &rParam,PSectorData _boot){
		// gets DOS-specific parameters from the Boot
		const PBootSector boot=(PBootSector)_boot;
		// . drives
		const HANDLE hDrives=CPropGridCtrl::AddCategory(hPropGrid,NULL,_T("Drives"));
			const CPropGridCtrl::PCEditor driveEditor=CPropGridCtrl::TCustom::DefineEditor( 0, TBootSector::TDiskAndDriveInfo::__pg_drawProperty__, NULL, TBootSector::TDiskAndDriveInfo::__pg_editProperty__ );
			CPropGridCtrl::AddProperty(	hPropGrid, hDrives, _T("Used"),
										&boot->currDrive, sizeof(TBootSector::TDiskAndDriveInfo),
										driveEditor
									);
			TBootSector::PDiskAndDriveInfo pddi=boot->drives; // drive information
			for( TCHAR buf[]=_T("Drive @"); ++buf[6]<='D'; pddi++ )
				CPropGridCtrl::AddProperty(	hPropGrid, hDrives, buf,
											pddi, sizeof(TBootSector::TDiskAndDriveInfo),
											driveEditor
										);
		// . GK's File Manager
		TBootSector::UReserved1::TGKFileManager::__addToPropertyGrid__(hPropGrid,boot);
		// . UniRUN by Proxima
		const HANDLE hUniRun=CPropGridCtrl::AddCategory(hPropGrid,NULL,UNIRUN_NAME);
			CPropGridCtrl::AddProperty(	hPropGrid, hUniRun, MDOS2_RUNP,
										BOOT_SECTOR_UPDATE_ONLINE_HYPERLINK, -1,
										CPropGridCtrl::THyperlink::DefineEditorA(__unirun_updateOnline__)
									);
	}









	void CMDOS2::FlushToBootSector() const{
		// flushes internal Format information to the actual Boot Sector's data
		if (const PBootSector boot=(PBootSector)image->GetSectorData(TBootSector::CHS)){
			boot->currDrive.disk.nCylinders=formatBoot.nCylinders;
			if (formatBoot.nHeads==2)
				boot->currDrive.disk.diskFlags|=16;
			else
				boot->currDrive.disk.diskFlags&=~16;
			boot->currDrive.disk.nSectors=formatBoot.nSectors;
			image->MarkSectorAsDirty(TBootSector::CHS);
		}
	}

	void CMDOS2::InitializeEmptyMedium(CFormatDialog::PCParameters params){
		// initializes a fresh formatted Medium (Boot, FAT, root dir, etc.)
		// - initializing the Boot Sector
		WORD w;
		if (const PBootSector boot=(PBootSector)image->GetSectorData(TBootSector::CHS,&w)){ // Boot Sector may not be found
			::ZeroMemory(boot,w);
			// . the floppy was formatted in two-head D80-compatible drive
			static const TBootSector::TDiskAndDriveInfo DefaultInfo={ 1, 16, 80, 9, 0, 16, 80, 9 };
			boot->drives[0]=DefaultInfo;
			// . floppy parameters
			boot->currDrive=DefaultInfo;
			FlushToBootSector(); // already carried out in CDos::__formatStdCylinders__ but overwritten by ZeroMemory above
			boot->currDrive.drive.diskFlags=boot->currDrive.disk.diskFlags; // flag on two-headed drive
			// . label and identification
			::lstrcpyA( boot->label, VOLUME_LABEL_DEFAULT_ANSI_8CHARS );
			boot->diskID=::GetTickCount();
			boot->sdos=SDOS_TEXT;
		}
		// - initializing the FAT (first 14 Sectors are System, the rest is Empty)
		for( TLogSector logSector=MDOS2_DATA_LOGSECTOR_FIRST; logSector; __setLogicalSectorFatItem__(--logSector,MDOS2_FAT_SECTOR_SYSTEM) );
		TLogSector logSectorZ=formatBoot.GetCountOfAllSectors();
		for( TLogSector logSector=MDOS2_DATA_LOGSECTOR_FIRST; logSector<logSectorZ; __setLogicalSectorFatItem__(logSector++,MDOS2_FAT_SECTOR_EMPTY) );
		while (logSectorZ<1705) __setLogicalSectorFatItem__(logSectorZ++,MDOS2_FAT_SECTOR_UNAVAILABLE); // 1705 = max number of items in FAT12
		// - empty Directory
		//nop (DirectoryEntry set as Empty during formatting - FillerByte happens to have the same value)
	}









	namespace D80{
		static PImage __instantiate__(){
			return new CImageRaw( &Properties, true );
		}
		const CImage::TProperties Properties={	_T("Didaktik D40/D80"),// name
												__instantiate__,// instantiation function
												_T("*.d80") IMAGE_FORMAT_SEPARATOR _T("*.d40"),	// filter
												TMedium::FLOPPY_DD,
												MDOS2_SECTOR_LENGTH_STD, MDOS2_SECTOR_LENGTH_STD	// min and max length of storable Sectors
											};
	}
