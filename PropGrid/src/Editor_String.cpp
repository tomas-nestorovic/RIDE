#include "stdafx.h"

	static bool WINAPI __alwaysAccept__(CPropGridCtrl::PCustomParam,CPropGridCtrl::PValue,CPropGridCtrl::TValueSize){
		return true; // new Value is by default always accepted
	}

	TStringEditor::TStringEditor(	CPropGridCtrl::TOnEllipsisButtonClicked onEllipsisBtnClicked,
									bool wideChar,
									CPropGridCtrl::TString::TOnValueConfirmed onValueConfirmed
								)
		// ctor
		: TEditor( EDITOR_DEFAULT_HEIGHT, true, onEllipsisBtnClicked )
		, wideChar(wideChar)
		, onValueConfirmed( onValueConfirmed ? onValueConfirmed : __alwaysAccept__ ) {
	}

	void TStringEditor::__drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const{
		// draws the Value into the specified rectangle
		if (wideChar)
			__drawString__( (LPCWSTR)value.buffer, value.bufferCapacity, pdis );
		else
			__drawString__( (LPCSTR)value.buffer, value.bufferCapacity, pdis );
	}

	HWND TStringEditor::__createEditBox__(HWND hParent,UINT extraStyle){
		// creates and returns an Edit box with given ExtraStyle
		const HWND hEdit=::CreateWindow(WC_EDIT,
										NULL, // descendant sets the edit-box content
										extraStyle | ES_AUTOHSCROLL | ES_WANTRETURN | EDITOR_STYLE,
										0,0, 1,1,
										hParent, 0, GET_PROPGRID_HINSTANCE(hParent), NULL
									);
		//Edit_SetSel(hEdit,0,-1);	// selecting all text
		return hEdit;
	}

	HWND TStringEditor::__createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const{
		// creates, initializes with current Value, and returns Editor's MainControl
		// - creating the Edit-box
		const HWND hEdit=__createEditBox__(hParent,0);
		// - constraining the length of text in the Edit-box
		Edit_LimitText(hEdit,value.bufferCapacity);
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
				WCHAR buffer[STRING_LENGTH_MAX];
				const CPropGridCtrl::TBufferCapacity nChars=::GetWindowTextW( hEdit, buffer, STRING_LENGTH_MAX );
				accepted=onValueConfirmed(	value.param, buffer,
											min( nChars, value.bufferCapacity ) // we should never need to trim the text, but do so just to be sure :-)
										);
			}else{
				char buffer[STRING_LENGTH_MAX];
				const CPropGridCtrl::TBufferCapacity nChars=::GetWindowTextA( hEdit, buffer, STRING_LENGTH_MAX );
				accepted=onValueConfirmed(	value.param, buffer,
											min( nChars, value.bufferCapacity ) // we should never need to trim the text, but do so just to be sure :-)
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
					Edit_SetSel(hEdit,0,-1);
				// . base
				break;
		}
		return __super::__mainCtrl_wndProc__(hEdit,msg,wParam,lParam);
	}









	TFixedPaddedStringEditor::TFixedPaddedStringEditor(	CPropGridCtrl::TOnEllipsisButtonClicked onEllipsisBtnClicked,
														bool wideChar,
														CPropGridCtrl::TString::TOnValueConfirmed onValueConfirmed,
														WCHAR paddingChar
													)
		// ctor
		: TStringEditor( NULL, wideChar, onValueConfirmed )
		, paddingChar(paddingChar) {
	}

	HWND TFixedPaddedStringEditor::__createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const{
		// creates, initializes with current Value, and returns Editor's MainControl
		// - base
		const HWND hEdit=__super::__createMainControl__(value,hParent);
		// - initializing the edit-box
		if (wideChar){
			WCHAR tmp[STRING_LENGTH_MAX];
			*tmp=~paddingChar; // terminator
			::lstrcpynW( tmp+1, (LPCWSTR)value.buffer, min(STRING_LENGTH_MAX-1,value.bufferCapacity+1) ); // "+1", "-1" = terminator, "+1" = null character in the TmpBuffer
				for( PWCHAR p=tmp+value.bufferCapacity; *p==paddingChar; *p--='\0' );
			::SetWindowTextW( hEdit, tmp+1 ); // "+1" = terminator
		}else{
			char tmp[STRING_LENGTH_MAX];
			*tmp=~paddingChar; // terminator
			::lstrcpynA( tmp+1, (LPCSTR)value.buffer, min(STRING_LENGTH_MAX-1,value.bufferCapacity+1) ); // "+1", "-1" = terminator, "+1" = null character in the TmpBuffer
				for( PCHAR p=tmp+value.bufferCapacity; *p==paddingChar; *p--='\0' );
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
		const int n=value.bufferCapacity;
		if (wideChar){
			WCHAR tmp[STRING_LENGTH_MAX];
			for( int i=::GetWindowTextW(hEdit,tmp,STRING_LENGTH_MAX); i<n; tmp[i++]=paddingChar );
			::memcpy( value.buffer, tmp, n*sizeof(WCHAR) );
		}else{
			char tmp[STRING_LENGTH_MAX];
			for( int i=::GetWindowTextA(hEdit,tmp,STRING_LENGTH_MAX); i<n; tmp[i++]=paddingChar );
			::memcpy( value.buffer, tmp, n*sizeof(char) );
		}
		return true;
	}









	TDynamicStringEditor::TDynamicStringEditor(	bool wideChar,
												CPropGridCtrl::TString::TOnValueConfirmed onValueConfirmed
											)
		// ctor
		: TStringEditor( NULL, wideChar, onValueConfirmed ) {
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








	static bool WINAPI __ellipsis_selectFileInDialog__(CPropGridCtrl::PCustomParam param,CPropGridCtrl::PValue newFileName,CPropGridCtrl::TValueSize newFileNameLength){
		// True <=> file in shown dialog selected and confirmed (via the OK button), otherwise False
		const TPropGridInfo::TItem::TValue &value=TEditor::pSingleShown->value;
		const HWND hEdit=TEditor::pSingleShown->hMainCtrl;
		if (((const TFileNameEditor *)value.editor)->wideChar){
			WCHAR buf[MAX_PATH];
			OPENFILENAMEW ofn;
				::ZeroMemory(&ofn,sizeof(ofn));
				ofn.lStructSize=sizeof(ofn);
				ofn.hInstance=GET_PROPGRID_HINSTANCE( ofn.hwndOwner=hEdit );
				ofn.lpstrFile=::lstrcpynW( buf, (LPCWSTR)value.buffer, value.bufferCapacity+1 );
				ofn.nMaxFile=MAX_PATH;
			if (::GetOpenFileNameW(&ofn))
				return ::SetWindowTextW( hEdit, buf );
			else
				return false;
		}else{
			char buf[MAX_PATH];
			OPENFILENAMEA ofn;
				::ZeroMemory(&ofn,sizeof(ofn));
				ofn.lStructSize=sizeof(ofn);
				ofn.hInstance=GET_PROPGRID_HINSTANCE( ofn.hwndOwner=hEdit );
				ofn.lpstrFile=::lstrcpynA( buf, (LPCSTR)value.buffer, value.bufferCapacity+1 );
				ofn.nMaxFile=MAX_PATH;
			if (::GetOpenFileNameA(&ofn))
				return ::SetWindowTextA( hEdit, buf );
			else
				return false;
		}
	}

	TFileNameEditor::TFileNameEditor(	bool wideChar,
										CPropGridCtrl::TString::TOnValueConfirmed onValueConfirmed
									)
		// ctor
		: TFixedPaddedStringEditor(	__ellipsis_selectFileInDialog__, wideChar, onValueConfirmed, '\0' ) {
	}










	CPropGridCtrl::PCEditor CPropGridCtrl::TString::DefineFixedLengthEditorA(TOnValueConfirmedA onValueConfirmed,char paddingChar){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TFixedPaddedStringEditor( NULL, false, (TOnValueConfirmed)onValueConfirmed, paddingChar ),
					sizeof(TFixedPaddedStringEditor)
				);
	}

	CPropGridCtrl::PCEditor CPropGridCtrl::TString::DefineFixedLengthEditorW(TOnValueConfirmedW onValueConfirmed,WCHAR paddingChar){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TFixedPaddedStringEditor( NULL, true, (TOnValueConfirmed)onValueConfirmed, paddingChar ),
					sizeof(TFixedPaddedStringEditor)
				);
	}



	CPropGridCtrl::PCEditor CPropGridCtrl::TString::DefineDynamicLengthEditorA(TOnValueConfirmedA onValueConfirmed){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TDynamicStringEditor( false, (TOnValueConfirmed)onValueConfirmed ),
					sizeof(TDynamicStringEditor)
				);
	}

	CPropGridCtrl::PCEditor CPropGridCtrl::TString::DefineDynamicLengthEditorW(TOnValueConfirmedW onValueConfirmed){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TDynamicStringEditor( true, (TOnValueConfirmed)onValueConfirmed ),
					sizeof(TDynamicStringEditor)
				);
	}



	CPropGridCtrl::PCEditor CPropGridCtrl::TString::DefineFileNameEditorA(TOnValueConfirmedA onValueConfirmed){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TFileNameEditor( false, (TOnValueConfirmed)onValueConfirmed ),
					sizeof(TFileNameEditor)
				);
	}

	CPropGridCtrl::PCEditor CPropGridCtrl::TString::DefineFileNameEditorW(TOnValueConfirmedW onValueConfirmed){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TFileNameEditor( true, (TOnValueConfirmed)onValueConfirmed ),
					sizeof(TFileNameEditor)
				);
	}
