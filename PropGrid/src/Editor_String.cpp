#include "stdafx.h"

	static bool WINAPI __alwaysAccept__(PropGrid::PCustomParam,PropGrid::PValue,PropGrid::TSize){
		return true; // new Value is by default always accepted
	}

	TStringEditor::TStringEditor(	PropGrid::TOnEllipsisButtonClicked onEllipsisBtnClicked,
									bool wideChar,
									PropGrid::TSize nCharsMax,
									PropGrid::String::TOnValueConfirmed onValueConfirmed,
									PropGrid::TOnValueChanged onValueChanged
								)
		// ctor
		: TEditor( EDITOR_DEFAULT_HEIGHT, true, std::min(STRING_LENGTH_MAX,nCharsMax), onEllipsisBtnClicked, onValueChanged )
		, wideChar(wideChar)
		, onValueConfirmed( onValueConfirmed ? onValueConfirmed : __alwaysAccept__ ) {
	}

	void TStringEditor::__drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const{
		// draws the Value into the specified rectangle
		if (wideChar)
			__drawString__( (LPCWSTR)value.buffer, valueSize, pdis );
		else
			__drawString__( (LPCSTR)value.buffer, valueSize, pdis );
	}

	HWND TStringEditor::__createEditBox__(HWND hParent,UINT extraStyle){
		// creates and returns an Edit box with given ExtraStyle
		const HWND hEdit=::CreateWindowW(WC_EDITW,
										nullptr, // descendant sets the edit-box content
										extraStyle | ES_AUTOHSCROLL | ES_WANTRETURN | EDITOR_STYLE,
										0,0, 1,1,
										hParent, 0, GET_PROPGRID_HINSTANCE(hParent), nullptr
									);
		//Edit_SetSel(hEdit,0,-1);	// selecting all text
		return hEdit;
	}

	HWND TStringEditor::__createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const{
		// creates, initializes with current Value, and returns Editor's MainControl
		// - creating the Edit-box
		const HWND hEdit=__createEditBox__(hParent,0);
		// - constraining the length of text in the Edit-box
		::SendMessageW( hEdit, EM_LIMITTEXT, valueSize, 0 );
		// - returning the Edit-box
		return hEdit;
	}

	bool TStringEditor::__tryToAcceptMainCtrlValue__() const{
		// True <=> Editor's current Value is acceptable, otherwise False
		bool accepted;
		ignoreRequestToDestroy=true;
			const HWND hEdit=TEditor::pSingleShown->hMainCtrl;
			const TPropGridInfo::TItem::TValue &value=TEditor::pSingleShown->value;
			if (wideChar){
				WCHAR buffer[STRING_LENGTH_MAX+1];
				accepted=onValueConfirmed(	value.param, buffer,
											::GetWindowTextW( hEdit, buffer, STRING_LENGTH_MAX+1 )
										);
			}else{
				char buffer[STRING_LENGTH_MAX+1];
				accepted=onValueConfirmed(	value.param, buffer,
											::GetWindowTextA( hEdit, buffer, STRING_LENGTH_MAX+1 )
										);
			}
		ignoreRequestToDestroy=false;
		return accepted;
	}

	LRESULT TStringEditor::__mainCtrl_wndProc__(HWND hEdit,UINT msg,WPARAM wParam,LPARAM lParam) const{
		// window procedure
		switch (msg){
			case WM_KEYDOWN:
				// a key has been pressed
				// . selecting all text upon the Ctrl+A shortcut
				if (wParam=='A' && ::GetKeyState(VK_CONTROL)<0)
					::SendMessageW( hEdit, EM_SETSEL, 0, -1 );
				// . base
				break;
		}
		return __super::__mainCtrl_wndProc__(hEdit,msg,wParam,lParam);
	}









	TFixedPaddedStringEditor::TFixedPaddedStringEditor(	PropGrid::TOnEllipsisButtonClicked onEllipsisBtnClicked,
														bool wideChar,
														PropGrid::TSize nCharsMax,
														PropGrid::String::TOnValueConfirmed onValueConfirmed,
														WCHAR paddingChar,
														PropGrid::TOnValueChanged onValueChanged
													)
		// ctor
		: TStringEditor( nullptr, wideChar, nCharsMax, onValueConfirmed, onValueChanged )
		, paddingChar(paddingChar) {
	}

	HWND TFixedPaddedStringEditor::__createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const{
		// creates, initializes with current Value, and returns Editor's MainControl
		// - base
		const HWND hEdit=__super::__createMainControl__(value,hParent);
		// - initializing the edit-box
		if (wideChar){
			WCHAR tmp[STRING_LENGTH_MAX+2]; // "+2" = terminator and null character
			*tmp=~paddingChar; // terminator
			::lstrcpynW( tmp+1, (LPCWSTR)value.buffer, valueSize+1 );
				for( PWCHAR p=tmp+valueSize; *p==paddingChar; *p--='\0' );
			::SetWindowTextW( hEdit, tmp+1 ); // "+1" = terminator
		}else{
			char tmp[STRING_LENGTH_MAX+2]; // "+2" = terminator and null character
			*tmp=~paddingChar; // terminator
			::lstrcpynA( tmp+1, (LPCSTR)value.buffer, valueSize+1 );
				for( PCHAR p=tmp+valueSize; *p==paddingChar; *p--='\0' );
			::SetWindowTextA( hEdit, tmp+1 ); // "+1" = terminator
		}
		return hEdit;
	}

	bool TFixedPaddedStringEditor::__tryToAcceptMainCtrlValue__() const{
		// True <=> Editor's current Value is acceptable, otherwise False
		// - base
		if (!__super::__tryToAcceptMainCtrlValue__())
			return false;
		// - yes, the current Value is acceptable - replacing the old Value with it
		const HWND hEdit=TEditor::pSingleShown->hMainCtrl;
		const TPropGridInfo::TItem::TValue &value=TEditor::pSingleShown->value;
		const int n=valueSize;
		if (wideChar){
			WCHAR tmp[STRING_LENGTH_MAX+1];
			for( int i=::GetWindowTextW(hEdit,tmp,STRING_LENGTH_MAX+1); i<n; tmp[i++]=paddingChar );
			::memcpy( value.buffer, tmp, n*sizeof(WCHAR) );
		}else{
			char tmp[STRING_LENGTH_MAX+1];
			for( int i=::GetWindowTextA(hEdit,tmp,STRING_LENGTH_MAX+1); i<n; tmp[i++]=(char)paddingChar );
			::memcpy( value.buffer, tmp, n*sizeof(char) );
		}
		return true;
	}









	TDynamicStringEditor::TDynamicStringEditor(	bool wideChar,
												PropGrid::String::TOnValueConfirmed onValueConfirmed,
												PropGrid::TOnValueChanged onValueChanged
											)
		// ctor
		: TStringEditor( nullptr, wideChar, STRING_LENGTH_MAX, onValueConfirmed, onValueChanged ) {
	}

	HWND TDynamicStringEditor::__createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const{
		// creates, initializes with current Value, and returns Editor's MainControl
		// - base
		const HWND hEdit=__super::__createMainControl__(value,hParent);
		// - initializing the text in the Edit-box
		if (wideChar)
			::SetWindowTextW(hEdit,(LPCWSTR)value.buffer);
		else
			::SetWindowTextA(hEdit,(LPCSTR)value.buffer);
		return hEdit;
	}








	static bool WINAPI __ellipsis_selectFileInDialog__(PropGrid::PCustomParam param,PropGrid::PValue newFileName,PropGrid::TSize valueSize){
		// True <=> file in shown dialog selected and confirmed (via the OK button), otherwise False
		const TPropGridInfo::TItem::TValue &value=TEditor::pSingleShown->value;
		const HWND hEdit=TEditor::pSingleShown->hMainCtrl;
		if (((const TFileNameEditor *)value.editor)->wideChar){
			WCHAR buf[MAX_PATH];
			OPENFILENAMEW ofn;
				::ZeroMemory(&ofn,sizeof(ofn));
				ofn.lStructSize=sizeof(ofn);
				ofn.hInstance=GET_PROPGRID_HINSTANCE( ofn.hwndOwner=hEdit );
				ofn.lpstrFile=::lstrcpynW( buf, (LPCWSTR)value.buffer, std::min<>(valueSize+1,MAX_PATH) );
				ofn.nMaxFile=MAX_PATH;
			if (::GetOpenFileNameW(&ofn))
				return ::SetWindowTextW( hEdit, buf )!=FALSE;
			else
				return false;
		}else{
			char buf[MAX_PATH];
			OPENFILENAMEA ofn;
				::ZeroMemory(&ofn,sizeof(ofn));
				ofn.lStructSize=sizeof(ofn);
				ofn.hInstance=GET_PROPGRID_HINSTANCE( ofn.hwndOwner=hEdit );
				ofn.lpstrFile=::lstrcpynA( buf, (LPCSTR)value.buffer, std::min<>(valueSize+1,MAX_PATH) );
				ofn.nMaxFile=MAX_PATH;
			if (::GetOpenFileNameA(&ofn))
				return ::SetWindowTextA( hEdit, buf )!=FALSE;
			else
				return false;
		}
	}

	TFileNameEditor::TFileNameEditor(	bool wideChar,
										PropGrid::TSize nCharsMax,
										PropGrid::String::TOnValueConfirmed onValueConfirmed,
										PropGrid::TOnValueChanged onValueChanged
									)
		// ctor
		: TFixedPaddedStringEditor(	__ellipsis_selectFileInDialog__, wideChar, nCharsMax, onValueConfirmed, '\0', onValueChanged ) {
	}










	PropGrid::PCEditor PropGrid::String::DefineFixedLengthEditorA(TSize nCharsMax,TOnValueConfirmedA onValueConfirmed,char paddingChar,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TFixedPaddedStringEditor( nullptr, false, nCharsMax, (TOnValueConfirmed)onValueConfirmed, paddingChar, onValueChanged ),
					sizeof(TFixedPaddedStringEditor)
				);
	}

	PropGrid::PCEditor PropGrid::String::DefineFixedLengthEditorW(TSize nCharsMax,TOnValueConfirmedW onValueConfirmed,WCHAR paddingChar,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TFixedPaddedStringEditor( nullptr, true, nCharsMax, (TOnValueConfirmed)onValueConfirmed, paddingChar, onValueChanged ),
					sizeof(TFixedPaddedStringEditor)
				);
	}



	PropGrid::PCEditor PropGrid::String::DefineDynamicLengthEditorA(TOnValueConfirmedA onValueConfirmed,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TDynamicStringEditor( false, (TOnValueConfirmed)onValueConfirmed, onValueChanged ),
					sizeof(TDynamicStringEditor)
				);
	}

	PropGrid::PCEditor PropGrid::String::DefineDynamicLengthEditorW(TOnValueConfirmedW onValueConfirmed,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TDynamicStringEditor( true, (TOnValueConfirmed)onValueConfirmed, onValueChanged ),
					sizeof(TDynamicStringEditor)
				);
	}



	PropGrid::PCEditor PropGrid::String::DefineFileNameEditorA(TSize nCharsMax,TOnValueConfirmedA onValueConfirmed,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TFileNameEditor( false, nCharsMax, (TOnValueConfirmed)onValueConfirmed, onValueChanged ),
					sizeof(TFileNameEditor)
				);
	}

	PropGrid::PCEditor PropGrid::String::DefineFileNameEditorW(TSize nCharsMax,TOnValueConfirmedW onValueConfirmed,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TFileNameEditor( true, nCharsMax, (TOnValueConfirmed)onValueConfirmed, onValueChanged ),
					sizeof(TFileNameEditor)
				);
	}
