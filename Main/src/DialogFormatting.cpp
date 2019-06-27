#include "stdafx.h"

	CFormatDialog::CFormatDialog(PDos _dos,PCStdFormat _additionalFormats,BYTE _nAdditionalFormats)
		// ctor
		: CDialog(IDR_DOS_FORMAT) , dos(_dos)
		, updateBoot(BST_CHECKED)
		, addTracksToFat(BST_CHECKED)
		, showReportOnFormatting(dynamic_cast<CFDD *>(_dos->image)?BST_CHECKED:BST_UNCHECKED)
		, additionalFormats(_additionalFormats) , nAdditionalFormats(_nAdditionalFormats) {
		params.format.mediumType=dos->formatBoot.mediumType; // to initialize Parameters using the first suitable Format; it holds: MediumType==Unknown <=> this is initial formatting of an Image, MediumType!=Unknown <=> any subsequent formatting of the same Image
	}

	BEGIN_MESSAGE_MAP(CFormatDialog,CDialog)
		ON_WM_PAINT()
		ON_CBN_SELCHANGE(ID_MEDIUM,__onMediumChanged__)
		ON_CBN_SELCHANGE(ID_FORMAT,__onFormatChanged__)
		ON_EN_CHANGE(ID_HEAD,__recognizeStandardFormat__)
		ON_EN_CHANGE(ID_CYLINDER,__recognizeStandardFormatAndRepaint__)
		ON_EN_CHANGE(ID_CYLINDER_N,__recognizeStandardFormatAndRepaint__)
		ON_EN_CHANGE(ID_SECTOR,__recognizeStandardFormatAndRepaint__)
		ON_EN_CHANGE(ID_SIZE,__recognizeStandardFormatAndRepaint__)
		ON_EN_CHANGE(ID_GAP,__recognizeStandardFormatAndRepaint__)
		ON_EN_CHANGE(ID_INTERLEAVE,__recognizeStandardFormat__)
		ON_EN_CHANGE(ID_SKEW,__recognizeStandardFormat__)
		ON_CBN_SELCHANGE(ID_CLUSTER,__recognizeStandardFormat__)
		ON_EN_CHANGE(ID_FAT,__recognizeStandardFormat__)
		ON_EN_CHANGE(ID_DIRECTORY,__recognizeStandardFormat__)
		ON_BN_CLICKED(ID_BOOT,__toggleReportingOnFormatting__) // some function that causes repainting of the canvas
		ON_BN_CLICKED(ID_VERIFY_TRACK,__toggleReportingOnFormatting__)
	END_MESSAGE_MAP()
	









	#define FORMAT_CUSTOM	nullptr

	void CFormatDialog::PreInitDialog(){
		// dialog initialization
		// - showing the name of the DOS
		const CDos::PCProperties propDos=dos->properties;
		SetDlgItemText( ID_SYSTEM, propDos->name );
		// - populating dedicated ComboBox with possible Cluster sizes
		CComboBox cb;
		cb.Attach(GetDlgItem(ID_CLUSTER)->m_hWnd);
			for( WORD nMax=propDos->nSectorsInClusterMax,n=1; n<=nMax; n<<=1 ){
				TCHAR buf[32];
				::wsprintf(buf,_T("%d sectors"),n);
				cb.SetItemDataPtr( cb.AddString(buf), (PVOID)n );
			}
			cb.SetCurSel(0);
		cb.Detach();
		// - populating dedicated ComboBox with Media supported by both DOS and Image
		if (params.format.mediumType==TMedium::UNKNOWN)
			CImage::__populateComboBoxWithCompatibleMedia__( GetDlgItem(ID_MEDIUM)->m_hWnd, propDos->supportedMedia, dos->image->properties );
		else
			CImage::__populateComboBoxWithCompatibleMedia__( GetDlgItem(ID_MEDIUM)->m_hWnd, params.format.mediumType, dos->image->properties );
		params.format.mediumType=TMedium::UNKNOWN; // to initialize Parameters using the first suitable Format; it holds: MediumType==Unknown <=> initial formatting of an Image, MediumType!=Unknown <=> any subsequent formatting of the same Image
		__onMediumChanged__();
		// - adjusting interactivity
		const bool bootSectorAlreadyExists=((CMainWindow *)app.m_pMainWnd)->pTdi->__getCurrentTab__()!=nullptr;
		GetDlgItem(ID_CYLINDER)->EnableWindow(bootSectorAlreadyExists);
		static const WORD Controls[]={ ID_MEDIUM, ID_CLUSTER, ID_FAT, ID_DIRECTORY, 0 };
		Utils::EnableDlgControls( m_hWnd, Controls, !bootSectorAlreadyExists );
		GetDlgItem(ID_DRIVE)->ShowWindow( bootSectorAlreadyExists ? SW_SHOW : SW_HIDE );
	}

	void CFormatDialog::__selectClusterSize__(CComboBox &rcb,TSector clusterSize) const{
		// selects an item that corresponds with the chosen ClusterSize in the specified ComboBox
		rcb.SetCurSel(-1); // cancelling previous selection
		for( BYTE n=rcb.GetCount(); n--; )
			if (rcb.GetItemData(n)==clusterSize){
				rcb.SetCurSel(n);
				break;
			}
	}

	void CFormatDialog::DoDataExchange(CDataExchange *pDX){
		// exchange of data from and to controls
		const CDos::PCProperties propDos=dos->properties;
		const CImage::PCProperties propImage=dos->image->properties;
		const HWND hMedium=GetDlgItem(ID_MEDIUM)->m_hWnd;
		const TMedium::PCProperties propMedium=TMedium::GetProperties((TMedium::TType)ComboBox_GetItemData( hMedium, ComboBox_GetCurSel(hMedium) ));
		DDX_Text( pDX,	ID_CYLINDER_N,(RCylinder)params.format.nCylinders );
			DDV_MinMaxUInt( pDX, params.format.nCylinders, propMedium->cylinderRange.iMin, propMedium->cylinderRange.iMax );
		DDX_Text( pDX,	ID_CYLINDER	,(RCylinder)params.cylinder0 );
			DDV_MinMaxUInt( pDX, params.cylinder0, 0, params.format.nCylinders );
		DDX_Text( pDX,	ID_HEAD	,params.format.nHeads );
			const THead nHeadsMax =	params.cylinder0 ? min(dos->formatBoot.nHeads,propMedium->headRange.iMax) : propMedium->headRange.iMax;
			DDV_MinMaxUInt( pDX, params.format.nHeads, propMedium->headRange.iMin, nHeadsMax );
		DDX_Text( pDX,	ID_SECTOR	,params.format.nSectors );
			DDV_MinMaxUInt( pDX, params.format.nSectors, max(propMedium->sectorRange.iMin,propDos->nSectorsOnTrackMin), min(propMedium->sectorRange.iMax,propDos->nSectorsOnTrackMax) );
		DDX_Text( pDX,	ID_SIZE	,(short &)params.format.sectorLength );
			DDV_MinMaxUInt( pDX, params.format.sectorLength, propImage->sectorLengthMin, propImage->sectorLengthMax );
		DDX_Text( pDX,	ID_INTERLEAVE,params.interleaving);
			DDV_MinMaxUInt( pDX, params.interleaving, 1, params.format.nSectors );
		DDX_Text( pDX,	ID_SKEW	,params.skew);
			DDV_MinMaxUInt( pDX, params.skew, 0, params.format.nSectors-1 );
		DDX_Text( pDX,	ID_GAP		,params.gap3 );
		CComboBox cb;
		cb.Attach(GetDlgItem(ID_CLUSTER)->m_hWnd);
			if (pDX->m_bSaveAndValidate){
				const int sel=cb.GetCurSel();
				if (sel<0){
					pDX->PrepareEditCtrl(ID_CLUSTER);
					pDX->Fail();
				}else
					params.format.clusterSize=cb.GetItemData(sel);
			}else
				__selectClusterSize__(cb,params.format.clusterSize);
		cb.Detach();
		DDX_Text( pDX,	ID_FAT		,params.nAllocationTables );
			DDV_MinMaxUInt( pDX, params.nAllocationTables, propDos->nAllocationTablesMin, propDos->nAllocationTablesMax );
		DDX_Text( pDX,	ID_DIRECTORY	,(short &)params.nRootDirectoryEntries );
			DDV_MinMaxUInt( pDX, params.nRootDirectoryEntries, propDos->nRootDirectoryEntriesMin, propDos->nRootDirectoryEntriesMax );
		DDX_Check( pDX, ID_BOOT		, updateBoot );
		DDX_Check( pDX, ID_VERIFY_TRACK, addTracksToFat );
		DDX_Check( pDX, ID_REPORT	, showReportOnFormatting );
		if (pDX->m_bSaveAndValidate){
			params.format.nCylinders++;
				if (!dos->ValidateFormatChangeAndReportProblem(params.cylinder0>0,&params.format)){
					pDX->PrepareEditCtrl(ID_CYLINDER_N);
					pDX->Fail();
				}
			params.format.nCylinders--;
		}else{
			__recognizeStandardFormat__(); // selecting StandardFormat in ComboBox
			__toggleReportingOnFormatting__();
		}
	}

	BOOL CFormatDialog::OnNotify(WPARAM wParam,LPARAM lParam,LRESULT *pResult){
		// processes notification
		const LPCWPSTRUCT pcws=(LPCWPSTRUCT)lParam;
		if (pcws->wParam==ID_TRACK) // notification regarding Drive A:
			switch (pcws->message){
				case NM_CLICK:
				case NM_RETURN:{
					Utils::NavigateToUrlInDefaultBrowser(_T("http://www.hermannseib.com/documents/floppy.pdf"));
					*pResult=0;
					return TRUE;
				}
			}
		if (pcws->wParam==ID_DRIVE) // notification regarding Drive A:
			switch (pcws->message){
				case NM_CLICK:
				case NM_RETURN:{
					Utils::Information(_T("This may happen if the media descriptor is set inconsistently from what the volume is actually stored on. To recover, change the media descriptor in the \"") BOOT_SECTOR_TAB_LABEL _T("\" tab and try again."));
					*pResult=0;
					return TRUE;
				}
			}
		return CDialog::OnNotify(wParam,lParam,pResult);
	}

	afx_msg void CFormatDialog::OnPaint(){
		// drawing
		// - base
		CDialog::OnPaint();
		// - drawing curly brackets with the number of Cylinders
		TCHAR buf[80];
		::wsprintf( buf, _T("%d cylinder(s)"), GetDlgItemInt(ID_CYLINDER_N)+1-GetDlgItemInt(ID_CYLINDER) );
		Utils::WrapControlsByClosingCurlyBracketWithText( this, GetDlgItem(ID_CYLINDER), GetDlgItem(ID_CYLINDER_N), buf, ::GetSysColor(COLOR_3DSHADOW) );
		// - drawing curly brackets with Track length
		switch (params.format.mediumType){
			case TMedium::FLOPPY_HD:
			case TMedium::FLOPPY_DD:{
				Utils::WrapControlsByClosingCurlyBracketWithText( this, GetDlgItem(ID_SECTOR), GetDlgItem(ID_GAP), _T(""), ::GetSysColor(COLOR_3DSHADOW) );
				const WORD nBytesOnTrack=65 // Gap 1
										+
										GetDlgItemInt(ID_SECTOR)
										*(
											7	// ID record
											+
											37	// Gap 2
											+
											1+GetDlgItemInt(ID_SIZE)+2 // data
											+
											GetDlgItemInt(ID_GAP)+12+3	// Gap 3
										);
				::wsprintf( buf, _T("%d Bytes per track\n(see <a id=\"ID_TRACK\">IBM norm</a>)"), nBytesOnTrack );
				SetDlgItemText(ID_TRACK,buf);
				GetDlgItem(ID_TRACK)->ShowWindow(SW_SHOW);
				break;
			}
			case TMedium::HDD_RAW:
			default:
				Utils::WrapControlsByClosingCurlyBracketWithText( this, GetDlgItem(ID_SECTOR), GetDlgItem(ID_GAP), _T("N/A Bytes per track"), ::GetSysColor(COLOR_3DSHADOW) );
				GetDlgItem(ID_TRACK)->ShowWindow(SW_HIDE);
				break;
		}
		// - drawing curly brackets with warning on risking disk inconsistency
		if (!(IsDlgButtonChecked(ID_BOOT) & IsDlgButtonChecked(ID_VERIFY_TRACK)))
			Utils::WrapControlsByClosingCurlyBracketWithText(
				this,
				GetDlgItem(ID_BOOT), GetDlgItem(ID_VERIFY_TRACK),
				WARNING_MSG_CONSISTENCY_AT_STAKE, COLOR_BLACK
			);
	}

	afx_msg void CFormatDialog::__onMediumChanged__(){
		// Medium changed in corresponding ComboBox
		// - getting the currently SelectedMediumType
		CComboBox cb;
		cb.Attach(GetDlgItem(ID_MEDIUM)->m_hWnd);
			const TMedium::TType selectedMediumType=(TMedium::TType)cb.GetItemData( cb.GetCurSel() );
		cb.Detach();
		// - populating dedicated ComboBox with StandardFormats available for currently SelectedMediumType
		cb.Attach(GetDlgItem(ID_FORMAT)->m_hWnd);
			cb.ResetContent();
			// . StandardFormats
			const CDos::PCProperties dosProps=dos->properties;
			PCStdFormat psf=dosProps->stdFormats;
			for( BYTE n=dosProps->nStdFormats; n--; psf++ )
				if (psf->params.format.supportedMedia & selectedMediumType){
					cb.SetItemDataPtr( cb.AddString(psf->name), (PVOID)psf );
					if (params.format.mediumType==TMedium::UNKNOWN) params=psf->params; // initializing Parameters using the first Format that's suitable for given SelectedMediumType
				}
			// . AdditionalFormats
			PCStdFormat paf=additionalFormats;
			for( BYTE n=nAdditionalFormats; n--; paf++ )
				if (paf->params.format.supportedMedia & selectedMediumType){
					cb.SetItemDataPtr( cb.AddString(paf->name), (PVOID)paf );
					if (params.format.mediumType==TMedium::UNKNOWN) params=paf->params; // initializing Parameters using the first Format that's suitable for given SelectedMediumType
				}
			// . custom
			cb.SetItemDataPtr( cb.AddString(_T("Custom")), FORMAT_CUSTOM );
			//cb.SetCurSel(0);
		cb.Detach();
		params.format.mediumType=selectedMediumType;
		__recognizeStandardFormat__(); // recognizing one of StandardFormats and selecting it in dedicated ComboBox
		Invalidate(); // to repaint curly brackets
	}

	afx_msg void CFormatDialog::__onFormatChanged__(){
		// Format changed in dedicated ComboBox
		const HWND hComboBox=GetDlgItem(ID_FORMAT)->m_hWnd;
		if (const PCStdFormat f=(PCStdFormat)ComboBox_GetItemData(hComboBox, ComboBox_GetCurSel(hComboBox) )){
			// selected a StandardFormat
			SetDlgItemInt( ID_HEAD		,f->params.format.nHeads );
			SetDlgItemInt( ID_CYLINDER	,f->params.cylinder0 );
			SetDlgItemInt( ID_CYLINDER_N	,f->params.format.nCylinders );
			SetDlgItemInt( ID_SECTOR	,f->params.format.nSectors );
			SetDlgItemInt( ID_SIZE	,f->params.format.sectorLength );
			SetDlgItemInt( ID_INTERLEAVE,f->params.interleaving );
			SetDlgItemInt( ID_SKEW	,f->params.skew );
			SetDlgItemInt( ID_GAP		,f->params.gap3 );
			SetDlgItemInt( ID_FAT		,f->params.nAllocationTables );
			CComboBox cb;
			cb.Attach(GetDlgItem(ID_CLUSTER)->m_hWnd);
				__selectClusterSize__(cb,f->params.format.clusterSize);
			cb.Detach();
			SetDlgItemInt( ID_DIRECTORY	,f->params.nRootDirectoryEntries );
		}//else
			// selected custom format
			//nop
	}

	afx_msg void CFormatDialog::__recognizeStandardFormat__(){
		// determines if current settings represent one of DOS StandardFormats (settings include # of Sides, Cylinders, Sectors, RootDirectoryItems, etc.); if StandardFormat detected, it's selected in dedicated ComboBox
		// - enabling/disabling Boot and FAT modification
		static const WORD Controls[]={ ID_BOOT, ID_VERIFY_TRACK, 0 }; // Boot and FAT modification allowed only if NOT formatting from zeroth Track (e.g. when NOT creating a new Image)
		if (!Utils::EnableDlgControls( m_hWnd, Controls, GetDlgItemInt(ID_CYLINDER)>0 )){
			// if formatting from zeroth Track, Boot and FAT modification always necessary (e.g. when creating a new Image)
			CheckDlgButton(ID_BOOT,BST_CHECKED);
			CheckDlgButton(ID_VERIFY_TRACK,BST_CHECKED);
		}
		Invalidate(); // eventually warning on driving disk into an inconsistent state
		// - Recognizing StandardFormat
		const HWND hFormat=GetDlgItem(ID_FORMAT)->m_hWnd;
		const BYTE nFormatsInTotal=ComboBox_GetCount(hFormat);
		params.cylinder0=GetDlgItemInt(ID_CYLINDER);
		const HWND hCluster=GetDlgItem(ID_CLUSTER)->m_hWnd;
		const BYTE interleaving=GetDlgItemInt(ID_INTERLEAVE), skew=GetDlgItemInt(ID_SKEW), gap3=GetDlgItemInt(ID_GAP), nAllocationTables=GetDlgItemInt(ID_FAT);
		const WORD nRootDirectoryEntries=GetDlgItemInt(ID_DIRECTORY);
		const TFormat f={ TMedium::UNKNOWN, GetDlgItemInt(ID_CYLINDER_N), GetDlgItemInt(ID_HEAD), GetDlgItemInt(ID_SECTOR), dos->formatBoot.sectorLengthCode, GetDlgItemInt(ID_SIZE), ComboBox_GetItemData(hCluster,ComboBox_GetCurSel(hCluster)) };
		for( BYTE n=nFormatsInTotal-1; n--; ){ // "-1" = custom format
			const PCStdFormat psf=(PCStdFormat)ComboBox_GetItemData(hFormat,n);
			if (psf->params.cylinder0==params.cylinder0 && psf->params.format==f && psf->params.interleaving==interleaving && psf->params.skew==skew && psf->params.gap3==gap3 && psf->params.nAllocationTables==nAllocationTables && psf->params.nRootDirectoryEntries==nRootDirectoryEntries){
				ComboBox_SetCurSel(hFormat,n);
				return;
			}
		}
		ComboBox_SetCurSel( hFormat, nFormatsInTotal-1 ); // custom format
	}

	afx_msg void CFormatDialog::__recognizeStandardFormatAndRepaint__(){
		// determines if current settings represent one of DOS StandardFormats (settings include # of Sides, Cylinders, Sectors, RootDirectoryItems, etc.); if StandardFormat detected, it's selected in dedicated ComboBox
		__recognizeStandardFormat__();
		Invalidate(); // to repaint curly brackets
	}

	afx_msg void CFormatDialog::__toggleReportingOnFormatting__(){
		// if SectorVerification allowed, enables ReportingOnFormatting, otherwise disables ReportingOnFormatting
		const bool verifyTracks=IsDlgButtonChecked(ID_VERIFY_TRACK)==BST_CHECKED;
		GetDlgItem(ID_REPORT)->EnableWindow(verifyTracks);
		CheckDlgButton( ID_REPORT, verifyTracks&&showReportOnFormatting );
		Invalidate(); // eventually warning on driving disk into an inconsistent state
	}
