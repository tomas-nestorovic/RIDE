#include "stdafx.h"

	CFormatDialog::CFormatDialog(PDos _dos,PCStdFormat _additionalFormats,BYTE _nAdditionalFormats)
		// ctor
		: Utils::CRideDialog(IDR_DOS_FORMAT) , dos(_dos)
		, updateBoot(BST_CHECKED)
		, addTracksToFat(BST_CHECKED)
		, showReportOnFormatting(_dos->image->properties->IsRealDevice()?BST_CHECKED:BST_UNCHECKED)
		, additionalFormats(_additionalFormats) , nAdditionalFormats(_nAdditionalFormats) {
		params.format.mediumType=dos->formatBoot.mediumType; // to initialize Parameters using the first suitable Format; it holds: MediumType==Unknown <=> this is initial formatting of an Image, MediumType!=Unknown <=> any subsequent formatting of the same Image
	}

	BEGIN_MESSAGE_MAP(CFormatDialog,CDialog)
		ON_WM_PAINT()
		ON_CBN_SELCHANGE(ID_MEDIUM,__onMediumOrEncodingChanged__)
		ON_CBN_SELCHANGE(ID_CODEC,__onMediumOrEncodingChanged__)
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
		cb.Attach(GetDlgItemHwnd(ID_CLUSTER));
			for( WORD nMax=propDos->nSectorsInClusterMax,n=1; n<=nMax; n<<=1 ){
				TCHAR buf[32];
				::wsprintf(buf,_T("%d sectors"),n);
				cb.SetItemDataPtr( cb.AddString(buf), (PVOID)n );
			}
			cb.SetCurSel(0);
		cb.Detach();
		// - populating dedicated ComboBox with Codecs supported by both DOS and Image
		CImage::PopulateComboBoxWithCompatibleCodecs(
			GetDlgItemHwnd(ID_CODEC),
			params.format.mediumType==Medium::UNKNOWN
				? propDos->supportedCodecs
				: params.format.supportedCodecs,
			dos->image->properties
		);
		// - populating dedicated ComboBox with Media supported by both DOS and Image
		if (params.format.mediumType==Medium::UNKNOWN)
			CImage::PopulateComboBoxWithCompatibleMedia( GetDlgItemHwnd(ID_MEDIUM), propDos->supportedMedia, dos->image->properties );
		else
			CImage::PopulateComboBoxWithCompatibleMedia( GetDlgItemHwnd(ID_MEDIUM), params.format.mediumType, dos->image->properties );
		params=propDos->stdFormats->params, params.format.mediumType=Medium::UNKNOWN, params.format.codecType=Codec::ANY; // initialization using the first StandardFormat (eventually a Custom one)
		__onMediumOrEncodingChanged__();
		// - adjusting interactivity
		const bool bootSectorAlreadyExists=((CMainWindow *)app.m_pMainWnd)->pTdi->__getCurrentTab__()!=nullptr;
		EnableDlgItem( ID_CYLINDER, bootSectorAlreadyExists );
		static const WORD Controls[]={ ID_MEDIUM, ID_CLUSTER, ID_FAT, ID_DIRECTORY, 0 };
		EnableDlgItems( Controls, !bootSectorAlreadyExists );
		ShowDlgItem( ID_DRIVE, bootSectorAlreadyExists );
		if (!bootSectorAlreadyExists){
			const CRect rc=GetDlgItemClientRect(ID_FORMAT);
			SetDlgItemSize( ID_MEDIUM, rc.Width(), rc.Height() );
		}
	}

	void CFormatDialog::DoDataExchange(CDataExchange *pDX){
		// exchange of data from and to controls
		const CDos::PCProperties propDos=dos->properties;
		const CImage::PCProperties propImage=dos->image->properties;
		const HWND hMedium=GetDlgItemHwnd(ID_MEDIUM);
		const Medium::PCProperties propMedium=Medium::GetProperties((Medium::TType)ComboBox_GetItemData( hMedium, ComboBox_GetCurSel(hMedium) ));
		DDX_Text( pDX,	ID_CYLINDER_N,(RCylinder)params.format.nCylinders );
			DDV_MinMaxUInt( pDX, params.format.nCylinders, propMedium->cylinderRange.iMin, propMedium->cylinderRange.iMax );
		DDX_Text( pDX,	ID_CYLINDER	,(RCylinder)params.cylinder0 );
			DDV_MinMaxUInt( pDX, params.cylinder0, 0, params.format.nCylinders );
		DDX_Text( pDX,	ID_HEAD	,params.format.nHeads );
			const THead nHeadsMax =	params.cylinder0 ? std::min<int>(dos->formatBoot.nHeads,propMedium->headRange.iMax) : propMedium->headRange.iMax;
			DDV_MinMaxUInt( pDX, params.format.nHeads, propMedium->headRange.iMin, nHeadsMax );
		DDX_Text( pDX,	ID_SECTOR	,params.format.nSectors );
			DDV_MinMaxUInt( pDX, params.format.nSectors, std::max<int>(propMedium->sectorRange.iMin,propDos->nSectorsOnTrackMin), std::min<int>(propMedium->sectorRange.iMax,propDos->nSectorsOnTrackMax) );
		DDX_Text( pDX,	ID_SIZE	,(short &)params.format.sectorLength );
			DDV_MinMaxUInt( pDX, params.format.sectorLength, propImage->sectorLengthMin, propImage->sectorLengthMax );
			if (pDX->m_bSaveAndValidate)
				params.format.sectorLengthCode=CImage::GetSectorLengthCode(params.format.sectorLength);
			else
				params.format.sectorLengthCode=dos->formatBoot.sectorLengthCode;
		DDX_Text( pDX,	ID_INTERLEAVE,params.interleaving);
			DDV_MinMaxUInt( pDX, params.interleaving, 1, params.format.nSectors );
		DDX_Text( pDX,	ID_SKEW	,params.skew);
			DDV_MinMaxUInt( pDX, params.skew, 0, params.format.nSectors-1 );
		DDX_Text( pDX,	ID_GAP		,params.gap3 );
		if (pDX->m_bSaveAndValidate){
			CComboBox cb;
			cb.Attach(GetDlgItemHwnd(ID_CLUSTER));
				const int sel=cb.GetCurSel();
				if (sel<0){
					pDX->PrepareEditCtrl(ID_CLUSTER);
					pDX->Fail();
				}else
					params.format.clusterSize=cb.GetItemData(sel);
			cb.Detach();
		}else
			SelectDlgComboBoxValue( ID_CLUSTER, params.format.clusterSize );
		DDX_Text( pDX,	ID_FAT		,params.nAllocationTables );
			DDV_MinMaxUInt( pDX, params.nAllocationTables, propDos->nAllocationTablesMin, propDos->nAllocationTablesMax );
		DDX_Text( pDX,	ID_DIRECTORY	,(short &)params.nRootDirectoryEntries );
			DDV_MinMaxUInt( pDX, params.nRootDirectoryEntries, propDos->nRootDirectoryEntriesMin, propDos->nRootDirectoryEntriesMax );
		DDX_Check( pDX, ID_BOOT		, updateBoot );
		DDX_Check( pDX, ID_VERIFY_TRACK, addTracksToFat );
		DDX_Check( pDX, ID_REPORT	, showReportOnFormatting );
		if (pDX->m_bSaveAndValidate){
			// . checking that all Cylinders to format are Empty
			if (params.cylinder0>0 && ERROR_EMPTY!=dos->AreStdCylindersEmpty( params.cylinder0, params.format.nCylinders )){
				Utils::Information( DOS_ERR_CANNOT_FORMAT, DOS_ERR_CYLINDERS_NOT_EMPTY, DOS_MSG_CYLINDERS_UNCHANGED );
				pDX->PrepareEditCtrl(ID_CYLINDER_N);
				pDX->Fail();
			// . checking that new format is acceptable
			}else{
				params.format.nCylinders++;
					if (!dos->ValidateFormatChangeAndReportProblem( updateBoot&&params.cylinder0>0, addTracksToFat&&params.cylinder0>0, params.format )){
						pDX->PrepareEditCtrl(ID_CYLINDER_N);
						pDX->Fail();
					}
				params.format.nCylinders--;
			}
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
				case NM_RETURN:
					if (Utils::QuestionYesNo(_T("Media may introduce themselves wrongly (e.g. copy-protection). Instead of here, the introduction (if any) can be changed in the \"") BOOT_SECTOR_TAB_LABEL _T("\" tab.\n\nUnlock this setting anyway?"),MB_DEFBUTTON2)){
						ShowDlgItem( ID_DRIVE, false );
						const CRect rc=GetDlgItemClientRect(ID_FORMAT);
						const HWND hMedium=GetDlgItemHwnd(ID_MEDIUM);
						const LPCTSTR currMediumDesc=Medium::GetDescription((Medium::TType)ComboBox_GetItemData( hMedium, ComboBox_GetCurSel(hMedium) ));
						CImage::PopulateComboBoxWithCompatibleMedia( hMedium, dos->properties->supportedMedia, dos->image->properties );
						ComboBox_SelectString( hMedium, 0, currMediumDesc );
						::SetWindowPos( hMedium, nullptr, 0,0, rc.Width(),rc.Height(), SWP_NOZORDER|SWP_NOMOVE );
						::EnableWindow( hMedium, TRUE );
					}
					*pResult=0;
					return TRUE;
			}
		return __super::OnNotify(wParam,lParam,pResult);
	}

	afx_msg void CFormatDialog::OnPaint(){
		// drawing
		// - base
		__super::OnPaint();
		// - drawing curly brackets with the number of Cylinders
		TCHAR buf[80];
		::wsprintf( buf, _T("%d cylinder(s)"), GetDlgItemInt(ID_CYLINDER_N)+1-GetDlgItemInt(ID_CYLINDER) );
		WrapDlgItemsByClosingCurlyBracketWithText( ID_CYLINDER, ID_CYLINDER_N, buf, ::GetSysColor(COLOR_3DSHADOW) );
		// - drawing curly brackets with Track length
		switch (params.format.mediumType){
			case Medium::FLOPPY_DD_525:
			case Medium::FLOPPY_DD:
			case Medium::FLOPPY_HD_525:
			case Medium::FLOPPY_HD_350:{
				WrapDlgItemsByClosingCurlyBracketWithText( ID_SECTOR, ID_GAP, _T(""), ::GetSysColor(COLOR_3DSHADOW) );
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
				ShowDlgItem(ID_TRACK);
				break;
			}
			case Medium::HDD_RAW:
			default:
				WrapDlgItemsByClosingCurlyBracketWithText( ID_SECTOR, ID_GAP, _T("N/A Bytes per track"), ::GetSysColor(COLOR_3DSHADOW) );
				ShowDlgItem( ID_TRACK, false );
				break;
		}
		// - drawing curly brackets with warning on risking disk inconsistency
		if (!(IsDlgButtonChecked(ID_BOOT) & IsDlgButtonChecked(ID_VERIFY_TRACK)))
			WrapDlgItemsByClosingCurlyBracketWithText(
				ID_BOOT, ID_VERIFY_TRACK,
				WARNING_MSG_CONSISTENCY_AT_STAKE, COLOR_BLACK
			);
	}

	afx_msg void CFormatDialog::__onMediumOrEncodingChanged__(){
		// Medium changed in corresponding ComboBox
		// - getting the currently SelectedMediumType
		const Medium::TType selectedMediumType=(Medium::TType)GetDlgComboBoxSelectedValue(ID_MEDIUM);
		const Codec::TType selectedCodecType=(Codec::TType)GetDlgComboBoxSelectedValue(ID_CODEC);
		// - populating dedicated ComboBox with StandardFormats available for currently SelectedMediumType
		CComboBox cb;
		cb.Attach(GetDlgItemHwnd(ID_FORMAT));
			cb.ResetContent();
			// . StandardFormats
			const CDos::PCProperties dosProps=dos->properties;
			PCStdFormat psf=dosProps->stdFormats;
			for( BYTE n=dosProps->nStdFormats; n--; psf++ )
				if (psf->params.format.supportedMedia & selectedMediumType
					&&
					psf->params.format.supportedCodecs & selectedCodecType
				){
					cb.SetItemDataPtr( cb.AddString(psf->name), (PVOID)psf );
					if (params.format.mediumType==Medium::UNKNOWN) params=psf->params; // initializing Parameters using the first Format that's suitable for selected {Medium,Codec} combination
				}
			// . AdditionalFormats
			PCStdFormat paf=additionalFormats;
			for( BYTE n=nAdditionalFormats; n--; paf++ )
				if (paf->params.format.supportedMedia & selectedMediumType
					&&
					psf->params.format.supportedCodecs & selectedCodecType
				){
					cb.SetItemDataPtr( cb.AddString(paf->name), (PVOID)paf );
					if (params.format.mediumType==Medium::UNKNOWN) params=paf->params; // initializing Parameters using the first Format that's suitable for selected {Medium,Codec} combination
				}
			// . custom
			cb.SetItemDataPtr( cb.AddString(_T("Custom")), FORMAT_CUSTOM );
			//cb.SetCurSel(0);
		cb.Detach();
		params.format.mediumType=selectedMediumType;
		params.format.codecType=selectedCodecType;
		__recognizeStandardFormat__(); // recognizing one of StandardFormats and selecting it in dedicated ComboBox
		Invalidate(); // to repaint curly brackets
	}

	afx_msg void CFormatDialog::__onFormatChanged__(){
		// Format changed in dedicated ComboBox
		const HWND hComboBox=GetDlgItemHwnd(ID_FORMAT);
		if (const PCStdFormat f=(PCStdFormat)ComboBox_GetItemData(hComboBox, ComboBox_GetCurSel(hComboBox) )){
			// selected a StandardFormat
			SetDlgItemInt( ID_HEAD		,f->params.format.nHeads );
			SetDlgItemInt( ID_CYLINDER	,f->params.cylinder0 );
			SetDlgItemInt( ID_CYLINDER_N,f->params.format.nCylinders );
			SetDlgItemInt( ID_SECTOR	,f->params.format.nSectors );
			SetDlgItemInt( ID_SIZE		,f->params.format.sectorLength );
			SetDlgItemInt( ID_INTERLEAVE,f->params.interleaving );
			SetDlgItemInt( ID_SKEW		,f->params.skew );
			SetDlgItemInt( ID_GAP		,f->params.gap3 );
			SetDlgItemInt( ID_FAT		,f->params.nAllocationTables );
			SelectDlgComboBoxValue( ID_CLUSTER, f->params.format.clusterSize );
			SetDlgItemInt( ID_DIRECTORY	,f->params.nRootDirectoryEntries );
		}//else
			// selected custom format
			//nop
	}

	afx_msg void CFormatDialog::__recognizeStandardFormat__(){
		// determines if current settings represent one of DOS StandardFormats (settings include # of Sides, Cylinders, Sectors, RootDirectoryItems, etc.); if StandardFormat detected, it's selected in dedicated ComboBox
		// - enabling/disabling Boot and FAT modification
		static const WORD Controls[]={ ID_BOOT, ID_VERIFY_TRACK, 0 }; // Boot and FAT modification allowed only if NOT formatting from zeroth Track (e.g. when NOT creating a new Image)
		if (!EnableDlgItems( Controls, GetDlgItemInt(ID_CYLINDER)>0 )){
			// if formatting from zeroth Track, Boot and FAT modification always necessary (e.g. when creating a new Image)
			CheckDlgButton(ID_BOOT,BST_CHECKED);
			CheckDlgButton(ID_VERIFY_TRACK,BST_CHECKED);
		}
		Invalidate(); // eventually warning on driving disk into an inconsistent state
		// - Recognizing StandardFormat
		const HWND hFormat=GetDlgItemHwnd(ID_FORMAT);
		const BYTE nFormatsInTotal=ComboBox_GetCount(hFormat);
		params.cylinder0=GetDlgItemInt(ID_CYLINDER);
		const HWND hCluster=GetDlgItemHwnd(ID_CLUSTER);
		const BYTE interleaving=GetDlgItemInt(ID_INTERLEAVE), skew=GetDlgItemInt(ID_SKEW), gap3=GetDlgItemInt(ID_GAP), nAllocationTables=GetDlgItemInt(ID_FAT);
		const WORD nRootDirectoryEntries=GetDlgItemInt(ID_DIRECTORY);
		const TFormat f={ Medium::UNKNOWN, Codec::ANY, GetDlgItemInt(ID_CYLINDER_N), GetDlgItemInt(ID_HEAD), GetDlgItemInt(ID_SECTOR), dos->formatBoot.sectorLengthCode, GetDlgItemInt(ID_SIZE), ComboBox_GetItemData(hCluster,ComboBox_GetCurSel(hCluster)) };
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
		EnableDlgItem( ID_REPORT, verifyTracks );
		CheckDlgButton( ID_REPORT, verifyTracks&&showReportOnFormatting );
		Invalidate(); // eventually warning on driving disk into an inconsistent state
	}
