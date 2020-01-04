#include "stdafx.h"

	CVerifyVolumeDialog::CVerifyVolumeDialog(const CDos *dos)
		// ctor
		// - base
		: CDialog(IDR_DOS_VERIFY)
		// - initialization
		, dos(dos)
		, nOptionsChecked(0)
		, verifyBootSector(BST_UNCHECKED) , verifyFat(BST_UNCHECKED) , verifyFilesystem(BST_UNCHECKED)
		, verifyDiskSurface(BST_UNCHECKED)
		, repairStyle(0) {
	}









	void CVerifyVolumeDialog::DoDataExchange(CDataExchange *pDX){
		// exchange of data from and to controls
		DDX_Text( pDX, ID_DOS, CString(dos->properties->name) );
		DDX_Check( pDX, ID_BOOT,	verifyBootSector );
		DDX_Check( pDX, ID_FAT,		verifyFat );
		DDX_Check( pDX, ID_FILE1,	verifyFilesystem );
		DDX_Check( pDX, ID_IMAGE,	verifyDiskSurface );
		DDX_CBIndex( pDX, ID_REPAIR, repairStyle );
	}

	LRESULT CVerifyVolumeDialog::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
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
			case WM_NOTIFY:
				// processing notification from child control
				if (((LPNMHDR)lParam)->code==NM_CLICK){
					CheckDlgButton( ID_BOOT, BST_CHECKED );
					CheckDlgButton( ID_FAT, BST_CHECKED );
					CheckDlgButton( ID_FILE1, BST_CHECKED );
				}
				break;
		}
		return __super::WindowProc(msg,wParam,lParam);
	}
