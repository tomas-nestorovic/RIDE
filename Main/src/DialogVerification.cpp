#include "stdafx.h"

	CVerifyVolumeDialog::TParams::TParams(CDos *dos,const TVerificationFunctions &rvf)
		// ctor
		: dos(dos)
		, verifyBootSector(BST_UNCHECKED) , verifyFat(BST_UNCHECKED) , verifyFilesystem(BST_UNCHECKED)
		, verifyVolumeSurface(BST_UNCHECKED)
		, repairStyle(0)
		, verificationFunctions(rvf) {
	}

	BYTE CVerifyVolumeDialog::TParams::ConfirmFix(LPCTSTR problemDesc,LPCTSTR problemSolutionSuggestion) const{
		// presents the occured problem along with suggestion how to solve it, and returns user's reaction
		switch (repairStyle){
			default:
				ASSERT(FALSE);
			case 0:
				// automatic fixing of each Problem
				return IDYES;
			case 1:{
				// fixing only manually confirmed Problems
				TCHAR buf[2000];
				::wsprintf( buf, _T("%s.\n\nFix this problem? %s"), problemDesc, problemSolutionSuggestion );
				return Utils::QuestionYesNoCancel( buf, MB_DEFBUTTON1 );
			}
		}
	}









	CVerifyVolumeDialog::CVerifyVolumeDialog(CDos *dos,const TVerificationFunctions &rvf)
		// ctor
		// - base
		: CDialog(IDR_DOS_VERIFY)
		// - initialization
		, params(dos,rvf)
		, nOptionsChecked(0) {
	}









	void CVerifyVolumeDialog::DoDataExchange(CDataExchange *pDX){
		// exchange of data from and to controls
		DDX_Text( pDX, ID_DOS, CString(params.dos->properties->name) );
		DDX_Check( pDX, ID_BOOT,	params.verifyBootSector );
			Utils::EnableDlgControl( m_hWnd, ID_BOOT, params.verificationFunctions.fnBootSector );
		DDX_Check( pDX, ID_FAT,		params.verifyFat );
			Utils::EnableDlgControl( m_hWnd, ID_FAT, params.verificationFunctions.fnFatValues||params.verificationFunctions.fnFatCrossedFiles||params.verificationFunctions.fnFatLostAllocUnits );
		DDX_Check( pDX, ID_FILE1,	params.verifyFilesystem );
			Utils::EnableDlgControl( m_hWnd, ID_FILE1, params.verificationFunctions.fnFilesystem );
		DDX_Check( pDX, ID_IMAGE,	params.verifyVolumeSurface );
			Utils::EnableDlgControl( m_hWnd, ID_IMAGE, params.verificationFunctions.fnVolumeSurface );
		DDX_CBIndex( pDX, ID_REPAIR, params.repairStyle );
	}

	LRESULT CVerifyVolumeDialog::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_NOTIFY:
				// processing notification from child control
				if (((LPNMHDR)lParam)->code==NM_CLICK){
					CheckDlgButton( ID_BOOT, BST_CHECKED&&Utils::IsDlgControlEnabled(m_hWnd,ID_BOOT) );
					CheckDlgButton( ID_FAT, BST_CHECKED&&Utils::IsDlgControlEnabled(m_hWnd,ID_FAT) );
					CheckDlgButton( ID_FILE1, BST_CHECKED&&Utils::IsDlgControlEnabled(m_hWnd,ID_FILE1) );
					wParam=ID_BOOT; // for the below fallthrough
					//fallthrough
				}else
					break;
			case WM_COMMAND:
				// processing a command
				if (ID_BOOT<=wParam && wParam<=ID_IMAGE){
					// one of check-boxes (un)ticked
					nOptionsChecked=(IsDlgButtonChecked(ID_BOOT)==BST_CHECKED)
									+
									(IsDlgButtonChecked(ID_FAT)==BST_CHECKED)
									+
									(IsDlgButtonChecked(ID_FILE1)==BST_CHECKED)
									+
									(IsDlgButtonChecked(ID_IMAGE)==BST_CHECKED);
					GetDlgItem(IDOK)->EnableWindow( nOptionsChecked>0 );
				}
				break;
			case WM_PAINT:{
				// drawing
				// . base
				const LRESULT result=__super::WindowProc(msg,wParam,lParam);
				// . wrapping full integrity check options in a curly bracket
				Utils::WrapControlsByClosingCurlyBracketWithText( this, GetDlgItem(ID_BOOT), GetDlgItem(ID_FILE1), nullptr, 0 );
				return result;
			}
		}
		return __super::WindowProc(msg,wParam,lParam);
	}
