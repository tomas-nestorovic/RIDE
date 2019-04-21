#include "stdafx.h"

	CUnformatDialog::CUnformatDialog(PDos _dos,PCStdUnformat _stdUnformats,BYTE _nStdUnformats)
		// ctor
		// - base
		: CDialog(IDR_DOS_UNFORMAT)
		// - initialization
		, dos(_dos)
		, updateBoot(BST_CHECKED) , removeTracksFromFat(BST_CHECKED)
		, stdUnformats(_stdUnformats) , nStdUnformats(_nStdUnformats) {
		cylA=cylZ=dos->image->GetCylinderCount()-1;
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
		// - displaying DOS name
		SetDlgItemText( ID_SYSTEM, dos->properties->name );
		// - populating dedicated ComboBox with available StandardUnformattings
		CComboBox cb;
		cb.Attach(GetDlgItem(ID_FORMAT)->m_hWnd);
			PCStdUnformat psuf=stdUnformats;
			for( BYTE n=nStdUnformats; n--; psuf++ )
				cb.SetItemDataPtr( cb.AddString(psuf->name), (PVOID)psuf );
			cb.SetItemDataPtr( cb.AddString(_T("Custom")), UNFORMAT_CUSTOM );
			cb.SetCurSel(nStdUnformats); // custom unformat
		cb.Detach();
	}

	void CUnformatDialog::DoDataExchange(CDataExchange *pDX){
		// exchange of data from and to controls
		const TMedium::PCProperties p=TMedium::GetProperties(dos->formatBoot.mediumType);
		DDX_Text( pDX,	ID_CYLINDER_N,(RCylinder)cylZ );
			DDV_MinMaxUInt( pDX, cylZ, p->cylinderRange.iMin, dos->image->GetCylinderCount()-1 );
		DDX_Text( pDX,	ID_CYLINDER	,(RCylinder)cylA );
			DDV_MinMaxUInt( pDX, cylA, p->cylinderRange.iMin, cylZ );
		DDX_Check( pDX, ID_BOOT		, updateBoot );
		DDX_Check( pDX, ID_FAT		, removeTracksFromFat );
		if (pDX->m_bSaveAndValidate){
			TFormat f=dos->formatBoot;
			f.nCylinders=cylA;
			if (!dos->ValidateFormatChangeAndReportProblem(false,&f)){
				pDX->PrepareEditCtrl(ID_CYLINDER);
				pDX->Fail();
			}
		}
	}

	afx_msg void CUnformatDialog::OnPaint(){
		// drawing
		// - base
		CDialog::OnPaint();
		// - drawing curly brackets and number of Cylinders
		TCHAR buf[20];
		::wsprintf( buf, _T("%d cylinder(s)"), GetDlgItemInt(ID_CYLINDER_N)+1-GetDlgItemInt(ID_CYLINDER) );
		Utils::WrapControlsByClosingCurlyBracketWithText( this, GetDlgItem(ID_CYLINDER), GetDlgItem(ID_CYLINDER_N), buf, ::GetSysColor(COLOR_3DSHADOW) );
		// - drawing curly brackets with warning on risking disk inconsistency
		if (!(IsDlgButtonChecked(ID_BOOT) & IsDlgButtonChecked(ID_FAT)))
			Utils::WrapControlsByClosingCurlyBracketWithText(
				this,
				GetDlgItem(ID_BOOT), GetDlgItem(ID_FAT),
				WARNING_MSG_CONSISTENCY_AT_STAKE, COLOR_BLACK
			);
	}

	afx_msg void CUnformatDialog::__onUnformatChanged__(){
		// selected another Unformat in ComboBox
		const HWND hComboBox=GetDlgItem(ID_FORMAT)->m_hWnd;
		const int idUnformat=ComboBox_GetCurSel(hComboBox);
		if (const PCStdUnformat psuf=(PCStdUnformat)ComboBox_GetItemData(hComboBox,idUnformat)){
			// StandardUnformatting
			SetDlgItemInt( ID_CYLINDER	,psuf->cylA );
			SetDlgItemInt( ID_CYLINDER_N	,psuf->cylZ );
		}//else
			// custom unformatting
			//nop
	}

	afx_msg void CUnformatDialog::__recognizeStandardUnformat__(){
		// determines if current settings represent one of DOS StandardUnformats (settings include # of Sides, Cylinders, Sectors, RootDirectoryItems, etc.); if StandardUnformat detected, it's selected in dedicated ComboBox
		const HWND hComboBox=GetDlgItem(ID_FORMAT)->m_hWnd;
		cylA=GetDlgItemInt(ID_CYLINDER), cylZ=GetDlgItemInt(ID_CYLINDER_N);
		PCStdUnformat psuf=stdUnformats;
		for( BYTE n=0; n<nStdUnformats; psuf++,n++ )
			if (psuf->cylA==cylA && psuf->cylZ==cylZ){
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
