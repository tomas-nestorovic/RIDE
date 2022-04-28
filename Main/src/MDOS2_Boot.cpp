#include "stdafx.h"
#include "MDOS2.h"


	#define SDOS_TEXT	0x534F4453 /* DWORD containing the "SDOS" string */

	const TPhysicalAddress CMDOS2::TBootSector::CHS={ 0, 0, {0,0,1,MDOS2_SECTOR_LENGTH_STD_CODE} };

	TStdWinError CMDOS2::__recognizeDisk__(PImage image,PFormat pFormatBoot){
		// returns the result of attempting to recognize Image by this DOS as follows: ERROR_SUCCESS = recognized, ERROR_CANCELLED = user cancelled the recognition sequence, any other error = not recognized
		TFormat fmt={ Medium::FLOPPY_DD_525, Codec::MFM, 1,1,10, MDOS2_SECTOR_LENGTH_STD_CODE,MDOS2_SECTOR_LENGTH_STD, 1 };
		if (image->SetMediumTypeAndGeometry(&fmt,StdSidesMap,1)!=ERROR_SUCCESS || !image->GetNumberOfFormattedSides(0)){
			fmt.mediumType=Medium::FLOPPY_DD;
			if (image->SetMediumTypeAndGeometry(&fmt,StdSidesMap,1)!=ERROR_SUCCESS || !image->GetNumberOfFormattedSides(0))
				return ERROR_UNRECOGNIZED_VOLUME; // unknown Medium Type
		}
		if (const PCBootSector boot=(PCBootSector)image->GetHealthySectorData(TBootSector::CHS))
			if (boot->sdos==SDOS_TEXT){
				*pFormatBoot=fmt;
				if (MDOS2_TRACK_SECTORS_MIN<=boot->current.nSectors && boot->current.nSectors<=MDOS2_TRACK_SECTORS_MAX
					&&
					( pFormatBoot->nCylinders=boot->current.nCylinders )
					*
					( pFormatBoot->nHeads=1+(boot->current.diskFlags.doubleSided) )
					*
					( pFormatBoot->nSectors=boot->current.nSectors )
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
	static constexpr CFormatDialog::TStdFormat StdFormats[]={
		{ _T("3.5\" DS 80x9"), 0, {Medium::FLOPPY_DD,Codec::MFM,79,2,9,MDOS2_SECTOR_LENGTH_STD_CODE,MDOS2_SECTOR_LENGTH_STD,1}, 1, 0, FDD_350_SECTOR_GAP3, 1, 128 },
		{ _T("3.5\" DS 40x9 (beware under MDOS1!)"), 0, {Medium::FLOPPY_DD,Codec::MFM,39,2,9,MDOS2_SECTOR_LENGTH_STD_CODE,MDOS2_SECTOR_LENGTH_STD,1}, 1, 0, FDD_350_SECTOR_GAP3, 1, 128 },
		{ _T("5.25\" DS 40x9, 360 RPM"), 0, {Medium::FLOPPY_DD_525,Codec::MFM,39,2,9,MDOS2_SECTOR_LENGTH_STD_CODE,MDOS2_SECTOR_LENGTH_STD,1}, 1, 0, FDD_350_SECTOR_GAP3, 1, 128 } // Gap3 is fine to be the same as with 3.5" media (plus the Format easier human-recognizable in the "Format cyls" dialog)
	};
	const CDos::TProperties CMDOS2::Properties={
		_T("MDOS 2.0"), // name
		MAKE_DOS_ID('M','D','O','S','2','0','_','_'), // unique identifier
		80, // recognition priority (the bigger the number the earlier the DOS gets crack on the image)
		__recognizeDisk__, // recognition function
		__instantiate__, // instantiation function
		Medium::FLOPPY_DD_ANY,
		&D80::Properties, // the most common Image to contain data for this DOS (e.g. *.D80 Image for MDOS)
		3,	// number of std Formats
		StdFormats, // std Formats
		Codec::MFM, // a set of Codecs this DOS supports
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









	void WINAPI CMDOS2::TBootSector::TDiskAndDriveInfo::DrawPropGridItem(PropGrid::PCustomParam,PropGrid::PCValue diskAndDriveInfo,short,PDRAWITEMSTRUCT pdis){
		// drawing the summary on MDOS drive
		const PCDiskAndDriveInfo pddi=(PCDiskAndDriveInfo)diskAndDriveInfo; // drive information
		TCHAR buf[30];
		if (pddi->driveConnected){
			::wsprintf( buf, _T(" %c: D%d, %cS %dx%d"), 'A'+pddi->driveFlags.driveB, 80-pddi->driveFlags.driveD40*40, pddi->driveFlags.doubleSided?'D':'S', pddi->driveCylinders, pddi->driveSectorsPerTrack );
		}else
			::lstrcpy(buf,_T(" Disconnected"));
		::DrawText(	pdis->hDC, buf,-1, &pdis->rcItem, DT_SINGLELINE|DT_LEFT|DT_VCENTER );
	}
	bool WINAPI CMDOS2::TBootSector::TDiskAndDriveInfo::EditPropGridItem(PropGrid::PCustomParam,PropGrid::PValue diskAndDriveInfo,short){
		// True <=> editing of MDOS drive confirmed, otherwise False
		// - defining the Dialog
		class CEditDialog sealed:public CDialog{
		public:
			int connected, error, letter, d40, stepping, lastCylinder, nCylinders, nSectors;
			int doubleSided, dsk40cyl;
		private:
			void DoDataExchange(CDataExchange *pDX) override{
				// exchange of data from and to controls
				DDX_Check(	pDX, ID_CONNECTED	,connected);
				DDX_Check(	pDX, ID_ERROR		,error);
				DDX_Radio(	pDX, ID_DRIVEA		,letter);
				DDX_Radio(	pDX, ID_D80			,d40);
				DDX_Check(	pDX, ID_DSFLOPPY	,doubleSided);
				DDX_Check(	pDX, ID_40D80		,dsk40cyl);
				DDX_CBIndex(pDX, ID_STEPPING	,stepping);
				DDX_Text(	pDX, ID_TRACK		,lastCylinder);
					DDV_MinMaxInt(	pDX, lastCylinder, 0, CYLINDER_COUNT_MAX );
				DDX_Text(	pDX, ID_CYLINDER	,nCylinders);
					DDV_MinMaxInt(	pDX, nCylinders, CYLINDER_COUNT_MIN, CYLINDER_COUNT_MAX );
				DDX_Text(	pDX, ID_SECTOR		,nSectors);
					DDV_MinMaxInt(	pDX, nSectors, MDOS2_TRACK_SECTORS_MIN, MDOS2_TRACK_SECTORS_MAX );
			}
		public:
			CEditDialog(PCDiskAndDriveInfo pddi)
				// ctor
				: CDialog(IDR_MDOS_DRIVE_EDITOR)
				, connected(pddi->driveConnected)
				, error(pddi->driveError)
				, lastCylinder(pddi->driveLastSeekedCylinder)
				, letter(pddi->driveFlags.driveB)
				, d40(pddi->driveFlags.driveD40)
				, doubleSided(pddi->driveFlags.doubleSided)
				, dsk40cyl(pddi->driveFlags.fortyCylDiskInD80)
				, stepping(pddi->driveFlags.stepSpeed)
				, nCylinders(pddi->driveCylinders)
				, nSectors(pddi->driveSectorsPerTrack) {
			}
		} d((PCDiskAndDriveInfo)diskAndDriveInfo);
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			TDiskAndDriveInfo *const p=(TDiskAndDriveInfo *)diskAndDriveInfo;
			p->driveConnected=d.connected==BST_CHECKED;
			p->driveError=d.error==BST_CHECKED;
			p->driveLastSeekedCylinder=d.lastCylinder;
			p->driveFlags.driveB=d.letter!=0;
			CDos::GetFocused()->formatBoot.mediumType =	(p->driveFlags.driveD40=d.d40!=0)
														? Medium::FLOPPY_DD_525 // likely 360 rpm in PC
														: Medium::FLOPPY_DD;
			p->driveFlags.doubleSided=d.doubleSided==BST_CHECKED;
			p->driveFlags.fortyCylDiskInD80=d.dsk40cyl==BST_CHECKED;
			p->driveFlags.stepSpeed=d.stepping;
			p->driveCylinders=d.nCylinders;
			p->driveSectorsPerTrack=d.nSectors;
			p->diskFlags=p->driveFlags;
			return CBootView::__bootSectorModified__(nullptr,0);
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
			/*rParam.label.length=MDOS2_VOLUME_LABEL_LENGTH_MAX; // commented out as the default text editor isn't suitable to input Speccy keywords in disk label (e.g. "RETURN TO Zork" with keyword capitalized)
				rParam.label.bufferA=( (PBootSector)boot )->label;
				rParam.label.fillerByte='\0';*/
			rParam.id.buffer=&( (PBootSector)boot )->diskID;
				rParam.id.bufferCapacity=sizeof(WORD);
	}

	#define UNIRUN_NAME			_T("UniRUN 2.0")
	#define UNIRUN_IMPORT_NAME	_T("run.P ZXP98000aL1000T8")
	#define UNIRUN_ONLINE_NAME	_T("MDOS2/UniRUN/") UNIRUN_IMPORT_NAME

	static bool WINAPI __unirun_updateOnline__(PropGrid::PCustomParam,int hyperlinkId,LPCTSTR hyperlinkName){
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
			DWORD conflictResolution=CFileManagerView::TConflictResolution::UNDETERMINED;
			if ( err=CDos::GetFocused()->pFileManager->ImportFileAndResolveConflicts( &CMemFile(unirunDataBuffer,sizeof(unirunDataBuffer)), unirunDataLength, UNIRUN_IMPORT_NAME, 0, FILETIME(), FILETIME(), FILETIME(), tmp, conflictResolution ) )
				Utils::FatalError( _T("Cannot import ") UNIRUN_NAME, err, MDOS2_RUNP_NOT_MODIFIED );
		}
		return true; // True = destroy PropertyGrid's Editor
	}

	void CMDOS2::CMdos2BootView::AddCustomBootParameters(HWND hPropGrid,HANDLE hGeometry,HANDLE hVolume,const TCommonBootParameters &rParam,PSectorData _boot){
		// gets DOS-specific parameters from the Boot
		const PBootSector boot=(PBootSector)_boot;
		// . Volume
		PropGrid::AddProperty(	hPropGrid, hVolume, _T("Label"), boot->label,
								TZxRom::CLineComposerPropGridEditor::Define(MDOS2_VOLUME_LABEL_LENGTH_MAX,'\0',nullptr,__bootSectorModified__)
							);
		// . drives
		const HANDLE hDrives=PropGrid::AddCategory(hPropGrid,nullptr,_T("Drives"));
			const PropGrid::PCEditor driveEditor=PropGrid::Custom::DefineEditor( 0, sizeof(TBootSector::TDiskAndDriveInfo), TBootSector::TDiskAndDriveInfo::DrawPropGridItem, nullptr, TBootSector::TDiskAndDriveInfo::EditPropGridItem );
			PropGrid::AddProperty(	hPropGrid, hDrives, _T("Used"),
									&boot->current,
									driveEditor
								);
			TBootSector::TDiskAndDriveInfo *pddi=boot->drives; // drive information
			for( TCHAR buf[]=_T("Drive @"); ++buf[6]<='D'; pddi++ )
				PropGrid::AddProperty(	hPropGrid, hDrives, buf,
										pddi,
										driveEditor
									);
		// . GK's File Manager
		boot->gkfm.AddToPropertyGrid(hPropGrid);
		// . UniRUN by Proxima
		const HANDLE hUniRun=PropGrid::AddCategory(hPropGrid,nullptr,UNIRUN_NAME);
			PropGrid::AddProperty(	hPropGrid, hUniRun, MDOS2_RUNP,
									BOOT_SECTOR_UPDATE_ONLINE_HYPERLINK,
									PropGrid::Hyperlink::DefineEditorA(__unirun_updateOnline__)
								);
	}









	void CMDOS2::FlushToBootSector() const{
		// flushes internal Format information to the actual Boot Sector's data
		if (const PBootSector boot=(PBootSector)image->GetHealthySectorData(TBootSector::CHS)){
			boot->current.nCylinders=formatBoot.nCylinders;
			boot->current.diskFlags.doubleSided=formatBoot.nHeads==2;
			boot->current.nSectors=formatBoot.nSectors;
			image->MarkSectorAsDirty(TBootSector::CHS);
		}
	}

	void CMDOS2::InitializeEmptyMedium(CFormatDialog::PCParameters params){
		// initializes a fresh formatted Medium (Boot, FAT, root dir, etc.)
		// - initializing the Boot Sector
		WORD w;
		if (const PBootSector boot=(PBootSector)image->GetHealthySectorData(TBootSector::CHS,&w)){ // Boot Sector may not be found
			::ZeroMemory(boot,w);
			// . Current drive
			boot->current.driveConnected=true;
			boot->current.driveError=false;
			boot->current.driveFlags.driveB=formatBoot.mediumType==Medium::FLOPPY_DD_525; // 5.25" drives are typically mapped as B's; this flag is not important
			boot->current.driveFlags.driveD40=formatBoot.mediumType==Medium::FLOPPY_DD_525;
			boot->current.driveFlags.doubleSided=true; // all modern PC drives are
			boot->current.driveFlags.fortyCylDiskInD80=formatBoot.nCylinders==40; // 40-track disks were best avoided!
			boot->current.driveCylinders= formatBoot.mediumType==Medium::FLOPPY_DD_525 ? 40 : 80;
			boot->current.driveSectorsPerTrack=9;
			// . Current disk
			boot->current.diskFlags=boot->current.driveFlags;
			FlushToBootSector(); // already carried out in CDos::__formatStdCylinders__ but overwritten by ZeroMemory above
			// . label and identification
			boot->drives[0]=boot->current;
			::lstrcpyA( boot->label, VOLUME_LABEL_DEFAULT_ANSI_8CHARS );
			Utils::RandomizeData( &boot->diskID, sizeof(boot->diskID) );
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








	#define D40_CYLINDERS_MAX	(FDD_CYLINDERS_MAX-40)

	#define MDOS2	static_cast<CMDOS2 *>(vp.dos)
	#define IMAGE	MDOS2->image

	UINT AFX_CDECL CMDOS2::TBootSector::Verification_thread(PVOID pCancelableAction){
		// thread to verify the Boot Sector
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const TSpectrumVerificationParams &vp=*(TSpectrumVerificationParams *)pAction->GetParams();
		vp.fReport.OpenSection(BOOT_SECTOR_TAB_LABEL);
		const PBootSector boot=(PBootSector)IMAGE->GetHealthySectorData(TBootSector::CHS);
		if (!boot)
			return vp.TerminateAll( Utils::ErrorByOs(ERROR_VOLMGR_DISK_INVALID,ERROR_UNRECOGNIZED_VOLUME) );
		// - verifying this is actually an MDOS disk
		if (const TStdWinError err=vp.VerifyUnsignedValue( CHS, BOOT_SECTOR_LOCATION_STRING, nullptr, boot->sdos, (DWORD)SDOS_TEXT ))
			return vp.TerminateAll(err);
		// - verifying Current drive information
		if (!boot->current.driveConnected)
			switch (vp.ConfirmFix( _T("Current drive is reported \"Disconnected\""), _T("The drive should be connected.") )){
				case IDCANCEL:
					return vp.CancelAll();
				case IDNO:
					break;
				case IDYES:
					vp.fReport.CloseProblem( boot->current.driveConnected=true );
					IMAGE->MarkSectorAsDirty(CHS);
					break;
			}
		if (boot->current.driveError)
			vp.fReport.LogWarning( VERIF_MSG_DRIVE_ERROR );
		// - verifying Current disk information
		if (const TStdWinError err=vp.VerifyUnsignedValue( CHS, BOOT_SECTOR_LOCATION_STRING, VERIF_CYLINDER_COUNT, boot->current.nCylinders, (BYTE)1, (BYTE)FDD_CYLINDERS_MAX ))
			return vp.TerminateAll(err);
		if (const TStdWinError err=vp.VerifyUnsignedValue( CHS, BOOT_SECTOR_LOCATION_STRING, VERIF_SECTOR_COUNT, boot->current.nSectors, (BYTE)MDOS2_TRACK_SECTORS_MIN, (BYTE)MDOS2_TRACK_SECTORS_MAX ))
			return vp.TerminateAll(err);
		if (boot->current.diskFlags.driveD40 && boot->current.nCylinders>D40_CYLINDERS_MAX){
			TCHAR msg[80];
			::wsprintf( msg, _T("The disk has %d cylinders but claims to be formatted on D40"), boot->current.nCylinders );
			switch (vp.ConfirmFix( msg, _T("It should claim D80.") )){
				case IDCANCEL:
					return vp.CancelAll();
				case IDNO:
					break;
				case IDYES:
					boot->current.diskFlags.driveD40=false;
					MDOS2->formatBoot.mediumType=Medium::FLOPPY_DD;
					IMAGE->MarkSectorAsDirty(CHS);
					vp.fReport.CloseProblem(true);
					break;
			}
		}
		if (boot->current.diskFlags.driveD40 && !(boot->current.diskFlags.fortyCylDiskInD80&&boot->current.driveFlags.fortyCylDiskInD80)){
			TCHAR txt[80];
			vp.fReport.LogWarning(
				_T("D40's often contained 80-track drives, please revise the \"%s\" setting"),
				Utils::CRideDialog::GetDialogTemplateItemText( IDR_MDOS_DRIVE_EDITOR, ID_40D80, txt, sizeof(txt)/sizeof(TCHAR) )
			);
		}
		// - verifying Current drive information (continued)
		if (const TStdWinError err=vp.WarnIfUnsignedValueOutOfRange( CHS, BOOT_SECTOR_LOCATION_STRING, _T("Last seeked cylinder"), boot->current.driveLastSeekedCylinder, (BYTE)0, (BYTE)(boot->current.nCylinders-1) ))
			if (err!=ERROR_INVALID_PARAMETER)
				return vp.TerminateAll(err);
		if (boot->current.driveFlags.driveD40 ^ MDOS2->formatBoot.mediumType==Medium::FLOPPY_DD_525){
			TCHAR msg[80], sug[80];
			_stprintf( msg, _T("The actual %.2f\" disk claims to be inserted in a D%c0 drive"), MDOS2->formatBoot.mediumType==Medium::FLOPPY_DD_525?5.25f:3.5, '8'-4*boot->current.driveFlags.driveD40 );
			::wsprintf( sug, _T("It should be a D%c0 drive."), '4'+4*boot->current.driveFlags.driveD40 );
			switch (vp.ConfirmFix( msg, sug )){
				case IDCANCEL:
					return vp.CancelAll();
				case IDNO:
					break;
				case IDYES:
					MDOS2->formatBoot.mediumType =	(boot->current.driveFlags.driveD40=!boot->current.driveFlags.driveD40)
													? Medium::FLOPPY_DD_525 // likely 360 rpm in PC
													: Medium::FLOPPY_DD;
					IMAGE->MarkSectorAsDirty(CHS);
					vp.fReport.CloseProblem(true);
					break;
			}
		}
		if (boot->current.driveFlags.driveD40 && boot->current.driveCylinders>D40_CYLINDERS_MAX
			||
			!boot->current.driveFlags.driveD40 && boot->current.driveCylinders>FDD_CYLINDERS_MAX
		){
			TCHAR msg[80], sug[80];
			::wsprintf( msg, _T("The drive claims to be D%c0 with %d cylinders"), '8'-4*boot->current.driveFlags.driveD40, boot->current.driveCylinders );
			const BYTE nCylindersGuaranteed= boot->current.driveFlags.driveD40 ? D40_CYLINDERS_MAX : FDD_CYLINDERS_MAX;
			::wsprintf( sug, _T("Guaranteed are just %d cylinders."), nCylindersGuaranteed );
			switch (vp.ConfirmFix( msg, sug )){
				case IDCANCEL:
					return vp.CancelAll();
				case IDNO:
					break;
				case IDYES:
					boot->current.driveCylinders=nCylindersGuaranteed;
					IMAGE->MarkSectorAsDirty(CHS);
					vp.fReport.CloseProblem(true);
					break;
			}
		}
		if (const TStdWinError err=vp.VerifyUnsignedValue( CHS, BOOT_SECTOR_LOCATION_STRING, _T("Drive sectors"), boot->current.driveSectorsPerTrack, (BYTE)MDOS2_TRACK_SECTORS_MIN, (BYTE)MDOS2_TRACK_SECTORS_MAX ))
			return vp.TerminateAll(err);
		// - verifying DiskName
		if (const TStdWinError err=vp.VerifyAllCharactersPrintable( CHS, BOOT_SECTOR_LOCATION_STRING, VERIF_VOLUME_NAME, boot->label, sizeof(boot->label), '\0' ))
			return vp.TerminateAll(err);
		// - Boot Sector verified
		return pAction->TerminateWithSuccess();
	}









	namespace D80{
		LPCTSTR Recognize(PTCHAR){
			static constexpr TCHAR SingleDeviceName[]=_T("Didaktik D40/D80\0");
			return SingleDeviceName;
		}
		PImage Instantiate(LPCTSTR){
			return new CImageRaw( &Properties, true );
		}
	}
