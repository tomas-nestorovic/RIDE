#include "stdafx.h"

	static bool WINAPI __alwaysAccept__(CPropGridCtrl::PCustomParam,bool){
		return true; // new Value is by default always accepted
	}

	TBooleanEditor::TBooleanEditor(	DWORD reservedValue,
									bool reservedForTrue,
									CPropGridCtrl::TBoolean::TOnValueConfirmed onValueConfirmed,
									CPropGridCtrl::TOnValueChanged onValueChanged
								)
		// ctor
		: TEditor( EDITOR_DEFAULT_HEIGHT, true, nullptr, onValueChanged )
		, reservedValue(reservedValue) , reservedForTrue(reservedForTrue)
		, onValueConfirmed( onValueConfirmed ? onValueConfirmed : __alwaysAccept__ ) {
	}

	void TBooleanEditor::__drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const{
		// draws the Value into the specified rectangle
		RECT r={ PROPGRID_CELL_MARGIN_LEFT+1, PROPGRID_CELL_MARGIN_TOP+1, PROPGRID_CELL_MARGIN_LEFT+16, PROPGRID_CELL_MARGIN_TOP+16 };
		::FrameRect(pdis->hDC, &r,
					pdis->CtlType==ODT_LISTBOX // drawing invoked by the ListBox?
						? TPropGridInfo::BRUSH_GRAY
						: TPropGridInfo::BRUSH_BLACK
				);
		DWORD tmp=0;
		::memcpy( &tmp, value.buffer, value.bufferCapacity );
		if (reservedForTrue && tmp==reservedValue  ||  !reservedForTrue && tmp!=reservedValue){
			const HDC dcmem=::CreateCompatibleDC(pdis->hDC);
				const HGDIOBJ hBitmap0=::SelectObject( dcmem, TPropGridInfo::CHECKBOX_CHECKED );
					::BitBlt(	pdis->hDC,
								PROPGRID_CELL_MARGIN_LEFT+2, PROPGRID_CELL_MARGIN_TOP+2, 13, 13,
								dcmem, 0, 0,
								SRCCOPY
							);
				::SelectObject(dcmem,hBitmap0);
			::DeleteDC(dcmem);
		}
	}

	static bool checked;

	HWND TBooleanEditor::__createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const{
		// creates, initializes with current Value, and returns Editor's MainControl
		DWORD tmp=0;
		::memcpy( &tmp, value.buffer, value.bufferCapacity );
		checked=reservedForTrue && tmp==reservedValue  ||  !reservedForTrue && tmp!=reservedValue;
		return ::CreateWindow(	WC_BUTTON,
								nullptr, // no caption next to the check-box
								EDITOR_STYLE | BS_AUTOCHECKBOX | BS_OWNERDRAW,
								0,0, 1,1,
								hParent, 0, GET_PROPGRID_HINSTANCE(hParent), nullptr
							);
	}

	bool TBooleanEditor::__tryToAcceptMainCtrlValue__() const{
		// True <=> Editor's current Value is acceptable, otherwise False
		ignoreRequestToDestroy=true;
			const TPropGridInfo::TItem::TValue &value=TEditor::pSingleShown->value;
			const bool accepted=onValueConfirmed( value.param, checked );
			if (accepted){
				const DWORD tmp=reservedForTrue && checked  ||  !reservedForTrue && !checked
								? reservedValue
								: ~reservedValue;
				::memcpy( value.buffer, &tmp, value.bufferCapacity );
			}
		ignoreRequestToDestroy=false;
		return accepted;
	}

	LRESULT TBooleanEditor::__mainCtrl_wndProc__(HWND hCheckBox,UINT msg,WPARAM wParam,LPARAM lParam) const{
		// window procedure
		switch (msg){
			case WM_CAPTURECHANGED:
				// CheckBox clicked (either by left mouse button or using space-bar)
				checked=!checked;
				::InvalidateRect(hCheckBox,nullptr,TRUE);
				break;
			case WM_ERASEBKGND:{
				RECT rc;
				GetClientRect(hCheckBox,&rc);
				::FillRect( (HDC)wParam, &rc, (HBRUSH)::GetStockObject(WHITE_BRUSH) );
				return TRUE;
			}
			case WM_PAINT:{
				// painting
				DRAWITEMSTRUCT dis;
				::ZeroMemory(&dis,sizeof(dis));
				dis.rcItem.right=1000;
				dis.rcItem.bottom=height;
				PAINTSTRUCT ps;
				dis.hDC=::BeginPaint(hCheckBox,&ps);
					DWORD tmp=	reservedForTrue && checked  ||  !reservedForTrue && !checked
								? reservedValue
								: ~reservedValue;
					__drawValue__(	TPropGridInfo::TItem::TValue( pSingleShown->value.editor, &tmp, sizeof(tmp), pSingleShown->value.param ),
									&dis
								);
				::EndPaint(hCheckBox,&ps);
				return 1;
			}
		}
		return __super::__mainCtrl_wndProc__(hCheckBox,msg,wParam,lParam);
	}









	CPropGridCtrl::PCEditor CPropGridCtrl::TBoolean::DefineEditor(TOnValueConfirmed onValueConfirmed,TOnValueChanged onValueChanged,DWORD reservedValue,bool reservedForTrue){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TBooleanEditor( reservedValue, reservedForTrue, onValueConfirmed, onValueChanged ),
					sizeof(TBooleanEditor)
				);
	}
