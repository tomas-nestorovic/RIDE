#include "stdafx.h"

	static bool WINAPI __alwaysAccept__(CPropGridCtrl::PCustomParam,bool){
		return true; // new Value is by default always accepted
	}

	TBooleanEditor::TBooleanEditor(	CPropGridCtrl::TBoolean::TOnValueConfirmed onValueConfirmed
								)
		// ctor
		: TEditor( EDITOR_DEFAULT_HEIGHT, true, NULL )
		, onValueConfirmed( onValueConfirmed ? onValueConfirmed : __alwaysAccept__ ) {
	}

	void TBooleanEditor::__drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const{
		// draws the Value into the specified rectangle
		RECT r={ PADDING_LEFT+1, PADDING_TOP+1, PADDING_LEFT+16, PADDING_TOP+16 };
		::FrameRect(pdis->hDC, &r,
					pdis->CtlType==ODT_LISTBOX // drawing invoked by the ListBox?
						? TPropGridInfo::BRUSH_GRAY
						: TPropGridInfo::BRUSH_BLACK
				);
		if (*(PBYTE)value.buffer){
			const HDC dcmem=::CreateCompatibleDC(pdis->hDC);
				const HGDIOBJ hBitmap0=::SelectObject( dcmem, TPropGridInfo::CHECKBOX_CHECKED );
					::BitBlt(	pdis->hDC,
								PADDING_LEFT+2,PADDING_TOP+2, 13, 13,
								dcmem, 0, 0,
								SRCCOPY
							);
				::SelectObject(dcmem,hBitmap0);
			::DeleteDC(dcmem);
		}
	}

	//rozepsat value na buffer+size
	HWND TBooleanEditor::__createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const{
		// creates, initializes with current Value, and returns Editor's MainControl
		return ::CreateWindow(	WC_BUTTON,
								NULL, // no caption next to the check-box
								EDITOR_STYLE | BS_CHECKBOX | BS_OWNERDRAW
									| (*(PBYTE)value.buffer?BST_CHECKED:BST_UNCHECKED),
								0,0, 1,1,
								hParent, 0, GET_PROPGRID_HINSTANCE(hParent), NULL
							);
	}

	//rozepsat value na buffer+size
	bool TBooleanEditor::__tryToAcceptMainCtrlValue__() const{
		// True <=> Editor's current Value is acceptable, otherwise False
		ignoreRequestToDestroy=true;
			const TPropGridInfo::TItem::TValue &value=TEditor::pSingleShown->value;
			const bool accepted=onValueConfirmed( value.buffer, Button_GetCheck(TEditor::pSingleShown->hMainCtrl)&BST_CHECKED );
			if (accepted)
				*(PBYTE)value.buffer=!*(PBYTE)value.buffer; // toggling the boolean Value
		ignoreRequestToDestroy=false;
		return accepted;
	}

	LRESULT TBooleanEditor::__mainCtrl_wndProc__(HWND hCheckBox,UINT msg,WPARAM wParam,LPARAM lParam) const{
		// window procedure
		switch (msg){
			case WM_PAINT:{
				// painting
				DRAWITEMSTRUCT dis;
				::ZeroMemory(&dis,sizeof(dis));
				dis.rcItem.right=1000;
				dis.rcItem.bottom=EDITOR_DEFAULT_HEIGHT;
				PAINTSTRUCT ps;
				dis.hDC==::BeginPaint(hCheckBox,&ps);
					const BYTE currValue=Button_GetCheck(hCheckBox)&BST_CHECKED;
					__drawValue__( pSingleShown->value, &dis );
				::EndPaint(hCheckBox,&ps);
				return 1;
			}
		}
		return __super::__mainCtrl_wndProc__(hCheckBox,msg,wParam,lParam);
	}









	CPropGridCtrl::PCEditor CPropGridCtrl::TBoolean::DefineEditor(TOnValueConfirmed onValueConfirmed){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TBooleanEditor(onValueConfirmed),
					sizeof(TBooleanEditor)
				);
	}
