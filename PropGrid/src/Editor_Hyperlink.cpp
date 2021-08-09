#include "stdafx.h"

	static bool WINAPI __doNothingAfterClick__(PropGrid::PCustomParam,int,LPCTSTR){
		return true; // True = destroy the SysLink after clicking on a hyperlink, otherwise False
	}

	THyperlinkEditor::THyperlinkEditor(	bool wideChar,
										PropGrid::Hyperlink::TOnHyperlinkClicked onHyperlinkClicked,
										PropGrid::TOnValueChanged onValueChanged
									)
		// ctor
		// - base
		: TEditor( EDITOR_DEFAULT_HEIGHT, true, 0, nullptr, onValueChanged )
		// - initialization
		, wideChar(wideChar)
		, onHyperlinkClicked( onHyperlinkClicked ? onHyperlinkClicked : __doNothingAfterClick__ ) {
		// - initializing OLE and Common Controls
		::OleInitialize(nullptr);
		static constexpr INITCOMMONCONTROLSEX Icc={ sizeof(Icc), ICC_LINK_CLASS };
		::InitCommonControlsEx(&Icc); // silently assuming that running on Windows 2000 and newer (see handling failures to create a SysLink window below)
	}

	void THyperlinkEditor::__drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const{
		// draws the Value into the specified rectangle
		const LONG w=pdis->rcItem.right-pdis->rcItem.left, h=pdis->rcItem.bottom-pdis->rcItem.top;
		const HDC dcTmpBmp=::CreateCompatibleDC(pdis->hDC);
			const HBITMAP hTmpBmp=::CreateCompatibleBitmap( pdis->hDC, w, h ); // TemporaryBitmap
			const HGDIOBJ hBmp0=::SelectObject(dcTmpBmp,hTmpBmp);
				const HWND hSysLink=__createMainControl__( value, pdis->hwndItem ); // creating the temporary SysLink control to adopt the appearance of
					::SendMessage( hSysLink, WM_SETFONT, (WPARAM)TPropGridInfo::FONT_DEFAULT, 0 ); // explicitly setting DPI-scaled font
					::SetWindowPos(	hSysLink, nullptr, 0,0, w,h, SWP_NOZORDER|SWP_SHOWWINDOW );
					// . capturing the SysLink visuals to the TemporaryBitmap
					::SendMessage( hSysLink, WM_PRINTCLIENT, (WPARAM)dcTmpBmp, PRF_CHILDREN|PRF_CLIENT );
					// . drawing the captured visuals at place of the SysLink control
					::TransparentBlt( pdis->hDC, 0,0, w,h, dcTmpBmp, 0,0, w,h, ::GetSysColor(COLOR_BTNFACE) );
				::DestroyWindow(hSysLink);
			::DeleteObject( ::SelectObject(dcTmpBmp,hBmp0) );
		::DeleteDC(dcTmpBmp);
	}

	HWND THyperlinkEditor::__createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const{
		// creates, initializes with current Value, and returns Editor's MainControl
		WCHAR buf[STRING_LENGTH_MAX+1];
		*buf=' '; // imitating the value cell padding by prepending an extra space to the text
		if (wideChar)
			::lstrcpyW(buf+1,(LPCWSTR)value.buffer);
		else
			::MultiByteToWideChar( CP_ACP, 0, (LPCSTR)value.buffer,-1, buf+1,sizeof(buf)/sizeof(WCHAR)-1 );
		return ::CreateWindowW(	WC_LINK,
								buf,
								EDITOR_STYLE,
								0,0, 1,1,
								hParent, 0, GET_PROPGRID_HINSTANCE(hParent), nullptr
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
							::WideCharToMultiByte( CP_ACP, 0, info.item.szID,-1, buf,sizeof(buf), nullptr,nullptr );
							destroy=((THyperlinkEditor *)value.editor)->onHyperlinkClicked( value.param, info.item.iLink, (LPCTSTR)buf );
						}
						::SetFocus(hWnd); // renewing the focus, should it be lost during the Action
					ignoreRequestToDestroy=false;
					if (destroy)
						PropGrid::TryToAcceptCurrentValueAndCloseEditor(); // on success also destroys the Editor
					return 0;
				}else
					break;
			}
		}
		return __super::__mainCtrl_wndProc__(hWnd,msg,wParam,lParam);
	}









	PropGrid::PCEditor PropGrid::Hyperlink::DefineEditorA(TOnHyperlinkClickedA onHyperlinkClicked,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new THyperlinkEditor( false, (TOnHyperlinkClicked)onHyperlinkClicked, onValueChanged ),
					sizeof(THyperlinkEditor)
				);
	}

	PropGrid::PCEditor PropGrid::Hyperlink::DefineEditorW(TOnHyperlinkClickedW onHyperlinkClicked,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new THyperlinkEditor( true, (TOnHyperlinkClicked)onHyperlinkClicked, onValueChanged ),
					sizeof(THyperlinkEditor)
				);
	}
