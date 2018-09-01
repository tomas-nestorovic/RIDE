#include "stdafx.h"

	static bool WINAPI __doNothingAfterClick__(CPropGridCtrl::PCustomParam,int,LPCTSTR){
		return true; // True = destroy the SysLink after clicking on a hyperlink, otherwise False
	}

	THyperlinkEditor::THyperlinkEditor(	bool wideChar,
										CPropGridCtrl::THyperlink::TOnHyperlinkClicked onHyperlinkClicked,
										CPropGridCtrl::TOnValueChanged onValueChanged
									)
		// ctor
		// - base
		: TEditor( EDITOR_DEFAULT_HEIGHT, true, NULL, onValueChanged )
		// - initialization
		, wideChar(wideChar)
		, onHyperlinkClicked( onHyperlinkClicked ? onHyperlinkClicked : __doNothingAfterClick__ ) {
		// - initializing OLE and Common Controls
		::OleInitialize(NULL);
		static const INITCOMMONCONTROLSEX Icc={ sizeof(Icc), ICC_LINK_CLASS };
		::InitCommonControlsEx(&Icc); // silently assuming that running on Windows 2000 and newer (see handling failures to create a SysLink window below)
	}

	void THyperlinkEditor::__drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const{
		// draws the Value into the specified rectangle
		// - creating the temporary SysLink control to adopt the appearance from
		const WORD w=pdis->rcItem.right-pdis->rcItem.left, h=pdis->rcItem.bottom-pdis->rcItem.top;
		const HWND hSysLink=__createMainControl__( value, pdis->hwndItem );
		::SendMessage( hSysLink, WM_SETFONT, (WPARAM)TPropGridInfo::FONT_DEFAULT, 0 ); // explicitly setting DPI-scaled font
		::SetWindowPos(	hSysLink, NULL,
						pdis->rcItem.left, pdis->rcItem.top,
						w, h,
						SWP_NOZORDER | SWP_SHOWWINDOW
					);
		// - adopting the SysLink visuals
		::SendMessage( hSysLink, WM_PAINT, 0, 0 ); // forcing the painting onto the screen
		const HDC dcTmpBmp=::CreateCompatibleDC(pdis->hDC);
			const HBITMAP hTmpBmp=::CreateCompatibleBitmap( pdis->hDC, w, h ); // TemporaryBitmap
			const HGDIOBJ hBmp0=::SelectObject(dcTmpBmp,hTmpBmp);
				// . capturing the SysLink visuals to the TemporaryBitmap
				const HDC dc=::GetDC(hSysLink);
					RECT r;
					::GetClientRect(hSysLink,&r);
					::BitBlt( dcTmpBmp, 0,0, w,h, dc, 0,0, SRCCOPY );
				::ReleaseDC(hSysLink,dc);
				// . destroying the temporary SysLink control
				::DestroyWindow(hSysLink);
				// . drawing the captured visuals at place of the SysLink control
				::TransparentBlt( pdis->hDC, 0,0, w,h, dcTmpBmp, 0,0, w,h, ::GetSysColor(COLOR_BTNFACE) );
			::DeleteObject( ::SelectObject(dcTmpBmp,hBmp0) );
		::DeleteDC(dcTmpBmp);
		// - destroying the temporary SysLink control
		//nop (already destroyed when adopting the visuals above)
	}

	HWND THyperlinkEditor::__createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const{
		// creates, initializes with current Value, and returns Editor's MainControl
		WCHAR buf[STRING_LENGTH_MAX];
		*buf=' '; // imitating the value cell padding by prepending an extra space to the text
		if (wideChar)
			::lstrcpyW(buf+1,(LPCWSTR)value.buffer);
		else
			::MultiByteToWideChar( CP_ACP, 0, (LPCSTR)value.buffer,-1, buf+1,sizeof(buf)/sizeof(WCHAR) );
		return ::CreateWindowW(	WC_LINK,
								buf,
								EDITOR_STYLE,
								0,0, 1,1,
								hParent, 0, GET_PROPGRID_HINSTANCE(hParent), NULL
							);
	}

	bool THyperlinkEditor::__tryToAcceptMainCtrlValue__() const{
		// True <=> Editor's current Value is acceptable, otherwise False
		return true; // Hyperlink doesn't have a Value, but returning True for the OnValueChanged event to fire
	}

	LRESULT THyperlinkEditor::__mainCtrl_wndProc__(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) const{
		// window procedure
		switch (msg){
			case WM_LBUTTONUP:{
				// left mouse button released
				LHITTESTINFO info;
					info.pt.x=GET_X_LPARAM(lParam), info.pt.y=GET_Y_LPARAM(lParam);
					info.item.mask=LIF_ITEMINDEX|LIF_ITEMID;
				if (::SendMessage( hWnd, LM_HITTEST, 0, (LPARAM)&info )){
					ignoreRequestToDestroy=true;
						const TPropGridInfo::TItem::TValue &value=TEditor::pSingleShown->value;
						bool destroy;
						if (wideChar)
							destroy=((THyperlinkEditor *)value.editor)->onHyperlinkClicked( value.param, info.item.iLink, (LPCTSTR)info.item.szID );
						else{
							char buf[1024];
							::WideCharToMultiByte( CP_ACP, 0, info.item.szID,-1, buf,sizeof(buf), NULL,NULL );
							destroy=((THyperlinkEditor *)value.editor)->onHyperlinkClicked( value.param, info.item.iLink, (LPCTSTR)buf );
						}
						::SetFocus(hWnd); // renewing the focus, should it be lost during the Action
					ignoreRequestToDestroy=false;
					if (destroy)
						::ShowWindow(hWnd,SW_HIDE); // to destroy the SysLink
					return 0;
				}else
					break;
			}
		}
		return __super::__mainCtrl_wndProc__(hWnd,msg,wParam,lParam);
	}









	CPropGridCtrl::PCEditor CPropGridCtrl::THyperlink::DefineEditorA(TOnHyperlinkClickedA onHyperlinkClicked,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new THyperlinkEditor( false, (TOnHyperlinkClicked)onHyperlinkClicked, onValueChanged ),
					sizeof(THyperlinkEditor)
				);
	}

	CPropGridCtrl::PCEditor CPropGridCtrl::THyperlink::DefineEditorW(TOnHyperlinkClickedW onHyperlinkClicked,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new THyperlinkEditor( true, (TOnHyperlinkClicked)onHyperlinkClicked, onValueChanged ),
					sizeof(THyperlinkEditor)
				);
	}
