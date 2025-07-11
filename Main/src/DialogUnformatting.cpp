#include "stdafx.h"

	#define DOS_ERR_CANNOT_UNFORMAT		_T("Cannot unformat")

	CUnformatDialog::TParams::TParams(PDos dos,PCHead specificHeadOnly)
		// ctor
		: dos(dos) , specificHeadOnly(specificHeadOnly)
		, cylA(0) , cylZInclusive(0) {
	}

	#define DOS		params.dos
	#define IMAGE	DOS->image

	UINT AFX_CDECL CUnformatDialog::UnformatTracks_thread(PVOID pCancelableAction){
		// thread to unformat specified Tracks
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const TParams &ufp=*(TParams *)pAction->GetParams();
		const TTrack nTracks=(ufp.cylZInclusive+1-ufp.cylA)*(ufp.specificHeadOnly!=nullptr?1:ufp.dos->formatBoot.nHeads);
		pAction->SetProgressTarget( nTracks );
		TCylinder cyl=ufp.cylZInclusive; // unformatting "backwards"
		THead head= ufp.specificHeadOnly!=nullptr ? *ufp.specificHeadOnly : 0; // one particular or all Heads?
		for( TTrack t=0; t<nTracks; pAction->UpdateProgress(++t) ){
			if (pAction->Cancelled) return ERROR_CANCELLED;
			// . unformatting
			if (const TStdWinError err=ufp.dos->image->UnformatTrack(cyl,head))
				return pAction->TerminateWithError(err);
			// . next Track
			if (ufp.specificHeadOnly!=nullptr) // one particular Head
				cyl--;
			else // all Heads
				if (++head==ufp.dos->formatBoot.nHeads){
					cyl--;
					head=0;
				}
		}
		return ERROR_SUCCESS;
	}

	UINT AFX_CDECL CUnformatDialog::UnregisterStdCylinders_thread(PVOID pCancelableAction){
		// thread to remove std cylinders optionally from boot and FAT
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const CUnformatDialog &d=*(CUnformatDialog *)pAction->GetParams();
		TFormat fmt=d.DOS->formatBoot;
		if (fmt.nCylinders<=1+d.params.cylZInclusive)
			fmt.nCylinders=std::min( fmt.nCylinders, d.params.cylA );
		pAction->SetProgressTarget(200);
		if (d.removeTracksFromFat){
			// requested to include newly formatted Tracks into FAT
			d.DOS->RemoveStdCylindersFromFat( d.params.cylA, d.params.cylZInclusive, pAction->CreateSubactionProgress(100) ); // no error checking as its assumed that some Cylinders couldn't be marked in (eventually shrunk) FAT as Unavailable
			d.DOS->ChangeFormat( d.updateBoot&&d.params.cylA>0, d.removeTracksFromFat&&d.params.cylA>0, fmt );
		}
		pAction->UpdateProgress(100);
		if (d.updateBoot){
			// requested to update Format in Boot Sector
			d.DOS->formatBoot=fmt;
			d.DOS->FlushToBootSector();
		}
		pAction->IncrementProgress(100);
		return pAction->TerminateWithSuccess();
	}








	CUnformatDialog::CUnformatDialog(PDos dos,PCStdUnformat stdUnformats,BYTE nStdUnformats)
		// ctor
		// - base
		: Utils::CRideDialog(IDR_DOS_UNFORMAT)
		// - initialization
		, params(dos,nullptr)
		, stdUnformats(stdUnformats) , nStdUnformats(nStdUnformats)
		, updateBoot(true) , removeTracksFromFat(true) {
		params.cylA = params.cylZInclusive = dos->image->GetCylinderCount()-1;
	}

	BEGIN_MESSAGE_MAP(CUnformatDialog,CDialog)
		ON_WM_PAINT()
		ON_CBN_SELCHANGE(ID_FORMAT,__onUnformatChanged__)
		ON_EN_CHANGE(ID_CYLINDER,__recognizeStandardUnformatAndRepaint__)
		ON_EN_CHANGE(ID_CYLINDER_N,__recognizeStandardUnformatAndRepaint__)
		ON_BN_CLICKED(ID_BOOT,__warnOnPossibleInconsistency__)
		ON_BN_CLICKED(ID_FAT,__warnOnPossibleInconsistency__)
	END_MESSAGE_MAP()








	#define UNFORMAT_CUSTOM		nullptr

	void CUnformatDialog::PreInitDialog(){
		// dialog initialization
		// - base
		__super::PreInitDialog();
		// - displaying DOS name
		SetDlgItemText( ID_SYSTEM, DOS->properties->name );
		// - populating dedicated ComboBox with available StandardUnformattings
		CComboBox cb;
		cb.Attach(GetDlgItemHwnd(ID_FORMAT));
			PCStdUnformat psuf=stdUnformats;
			for( BYTE n=nStdUnformats; n--; psuf++ )
				cb.SetItemDataPtr( cb.AddString(psuf->name), (PVOID)psuf );
			cb.SetItemDataPtr( cb.AddString(_T("Custom")), UNFORMAT_CUSTOM );
			cb.SetCurSel(nStdUnformats); // custom unformat
		cb.Detach();
	}

	void CUnformatDialog::DoDataExchange(CDataExchange *pDX){
		// exchange of data from and to controls
		const Medium::PCProperties p=Medium::GetProperties(DOS->formatBoot.mediumType);
		DDX_Text( pDX,	ID_CYLINDER_N,(RCylinder)params.cylZInclusive );
			DDV_MinMaxUInt( pDX, params.cylZInclusive, p->cylinderRange.iMin, IMAGE->GetCylinderCount()-1 );
		DDX_Text( pDX,	ID_CYLINDER	,(RCylinder)params.cylA );
			DDV_MinMaxUInt( pDX, params.cylA, p->cylinderRange.iMin, params.cylZInclusive );
		DDX_Check( pDX, ID_BOOT		, updateBoot );
		DDX_Check( pDX, ID_FAT		, removeTracksFromFat );
		if (pDX->m_bSaveAndValidate){
			// . checking that new format is acceptable
			{
				TFormat f=DOS->formatBoot;
				if (f.nCylinders<=params.cylZInclusive+1)
					f.nCylinders=std::min( f.nCylinders, params.cylA );
				if (!DOS->ValidateFormatAndReportProblem( updateBoot&&params.cylA>0, removeTracksFromFat&&params.cylA>0, f )){
					pDX->PrepareEditCtrl(ID_CYLINDER);
					pDX->Fail();
				}
			}
		}
	}

	afx_msg void CUnformatDialog::OnPaint(){
		// drawing
		// - base
		__super::OnPaint();
		// - drawing curly brackets and number of Cylinders
		TCHAR buf[20];
		::wsprintf( buf, _T("%d cylinder(s)"), GetDlgItemInt(ID_CYLINDER_N)+1-GetDlgItemInt(ID_CYLINDER) );
		WrapDlgItemsByClosingCurlyBracketWithText( ID_CYLINDER, ID_CYLINDER_N, buf, ::GetSysColor(COLOR_3DSHADOW) );
		// - drawing curly brackets with warning on risking disk inconsistency
		if (!(IsDlgButtonChecked(ID_BOOT) & IsDlgButtonChecked(ID_FAT)))
			WrapDlgItemsByClosingCurlyBracketWithText(
				ID_BOOT, ID_FAT,
				WARNING_MSG_CONSISTENCY_AT_STAKE, COLOR_BLACK
			);
	}

	afx_msg void CUnformatDialog::__onUnformatChanged__(){
		// selected another Unformat in ComboBox
		if (const PCStdUnformat psuf=(PCStdUnformat)GetDlgComboBoxSelectedValue(ID_FORMAT)){
			// StandardUnformatting
			SetDlgItemInt( ID_CYLINDER	,psuf->cylA );
			SetDlgItemInt( ID_CYLINDER_N	,psuf->cylZ );
		}//else
			// custom unformatting
			//nop
	}

	afx_msg void CUnformatDialog::__recognizeStandardUnformat__(){
		// determines if current settings represent one of DOS StandardUnformats (settings include # of Sides, Cylinders, Sectors, RootDirectoryItems, etc.); if StandardUnformat detected, it's selected in dedicated ComboBox
		const HWND hComboBox=GetDlgItemHwnd(ID_FORMAT);
		params.cylA=GetDlgItemInt(ID_CYLINDER), params.cylZInclusive=GetDlgItemInt(ID_CYLINDER_N);
		PCStdUnformat psuf=stdUnformats;
		for( BYTE n=0; n<nStdUnformats; psuf++,n++ )
			if (psuf->cylA==params.cylA && psuf->cylZ==params.cylZInclusive){
				ComboBox_SetCurSel(hComboBox,n);
				return;
			}
		ComboBox_SetCurSel(hComboBox,nStdUnformats); // custom format
	}

	afx_msg void CUnformatDialog::__recognizeStandardUnformatAndRepaint__(){
		// determines if current settings represent one of DOS StandardUnformats (settings include # of Sides, Cylinders, Sectors, RootDirectoryItems, etc.); if StandardUnformat detected, it's selected in dedicated ComboBox
		__recognizeStandardUnformat__();
		Invalidate(); // to repaint curly brackets
	}

	afx_msg void CUnformatDialog::__warnOnPossibleInconsistency__(){
		// draws curly brackets with warning on risking disk inconsistency
		Invalidate(); // eventually warning on driving disk into an inconsistent state
	}

	TStdWinError CUnformatDialog::ShowModalAndUnformatStdCylinders(){
		// unformats Cylinders using Parameters obtained from confirmed UnformatDialog (CDos-derivate and UnformatDialog guarantee that all parameters are valid); returns Windows standard i/o error
		if (IMAGE->ReportWriteProtection()) return ERROR_WRITE_PROTECT;
		do{
			LOG_DIALOG_DISPLAY(_T("CUnformatDialog"));
			if (LOG_DIALOG_RESULT(DoModal())!=IDOK)
				return ERROR_CANCELLED;
			CBackgroundMultiActionCancelable bmac(THREAD_PRIORITY_BELOW_NORMAL);
				const CDos::TEmptyCylinderParams ecp( params.dos, params.cylA, params.cylZInclusive );
				ecp.AddAction(bmac);
				bmac.AddAction( UnformatTracks_thread, &params, _T("Unformatting") );
				if (updateBoot|removeTracksFromFat)
					bmac.AddAction( UnregisterStdCylinders_thread, this, _T("Updating disk") );
			if (const TStdWinError err=bmac.Perform(true))
				if (bmac.GetCurrentFunction()==CDos::TEmptyCylinderParams::Thread){ // error in checking if disk region empty?
					Utils::Information( DOS_ERR_CANNOT_UNFORMAT, DOS_ERR_CYLINDERS_NOT_EMPTY, DOS_MSG_CYLINDERS_UNCHANGED );
					continue; // show this Dialog once again so the user can amend
				}else{
					Utils::Information( DOS_ERR_CANNOT_UNFORMAT, err );
					return LOG_ERROR(err);
				}
			break;
		}while (true);
		IMAGE->UpdateAllViews(nullptr); // although updated already in UnformatTracks, here calling too as FormatBoot might have changed since then
		return ERROR_SUCCESS;
	}
