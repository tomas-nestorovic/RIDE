#include "stdafx.h"

namespace Utils{

	CCommandDialog::CCommandDialog(LPCTSTR _information)
		// ctor
		: CDialog(IDR_ACTION_DIALOG) , information(_information) {
	}

	CCommandDialog::CCommandDialog(WORD dialogId,LPCTSTR _information)
		// ctor
		: CDialog(dialogId) , information(_information) {
	}

	void CCommandDialog::PreInitDialog(){
		// dialog initialization
		// - base
		CDialog::PreInitDialog();
		// - initializing the main message
		SetDlgItemText( ID_INFORMATION, information );
	}

	typedef struct TCommandLikeButtonInfo sealed{
		const WNDPROC wndProc0;
		bool cursorHovering, pressed;
		TCommandLikeButtonInfo(WNDPROC _wndProc0)
			// ctor
			: wndProc0(_wndProc0) , cursorHovering(false) , pressed(false) {
		}
	} *PCommandLikeButtonInfo;

	static LRESULT WINAPI __commandLikeButton_wndProc__(HWND hCmdBtn,UINT msg,WPARAM wParam,LPARAM lParam){
		const PCommandLikeButtonInfo cmdInfo=(PCommandLikeButtonInfo)::GetWindowLong(hCmdBtn,GWL_USERDATA);
		const WNDPROC wndProc0=cmdInfo->wndProc0;
		switch (msg){
			case WM_MOUSEMOVE:{
				// mouse moved - registering consumption of mouse leaving the Button's client area
				cmdInfo->cursorHovering=true;
				TRACKMOUSEEVENT tme={ sizeof(tme), TME_LEAVE, hCmdBtn };
				::TrackMouseEvent(&tme);
				::InvalidateRect(hCmdBtn,nullptr,TRUE);
				break;
			}
			case WM_MOUSELEAVE:
				// mouse left Button's client area
				cmdInfo->cursorHovering=false;
				::InvalidateRect(hCmdBtn,nullptr,TRUE);
				break;
			case WM_LBUTTONDOWN:
				// left mouse button pressed
				cmdInfo->pressed=true;
				::InvalidateRect(hCmdBtn,nullptr,TRUE);
				break;
			case WM_LBUTTONUP:
				// left mouse button released
				cmdInfo->pressed=false;
				::InvalidateRect(hCmdBtn,nullptr,TRUE);
				break;
			case WM_PAINT:{
				// drawing
				RECT r;
				::GetClientRect(hCmdBtn,&r);
				PAINTSTRUCT ps;
				const HDC dc=::BeginPaint(hCmdBtn,&ps);
					if (cmdInfo->cursorHovering){
						bool buttonBackgroundPainted=false; // initialization
						// . drawing under Windows Vista and higher
						if (const HMODULE hUxTheme=::LoadLibrary(DLL_UXTHEME)){
							typedef HANDLE (WINAPI *TOpenThemeData)(HWND hWnd,LPCWSTR className);
							const HANDLE hTheme=((TOpenThemeData)::GetProcAddress(hUxTheme,_T("OpenThemeData")))(hCmdBtn,WC_BUTTONW);
								typedef HRESULT (WINAPI *TDrawThemeBackground)(HTHEME hTheme,HDC dc,int iPartId,int iStateId,LPRECT lpRect,LPRECT lpClipRect);
								buttonBackgroundPainted=((TDrawThemeBackground)::GetProcAddress(hUxTheme,_T("DrawThemeBackground")))( hTheme, dc, BP_PUSHBUTTON, cmdInfo->pressed?PBS_PRESSED:PBS_HOT, &r, nullptr )==S_OK;
							typedef BOOL (WINAPI *TCloseThemeData)(HANDLE);
							((TCloseThemeData)::GetProcAddress(hUxTheme,_T("CloseThemeData")))(hTheme);
							::FreeLibrary(hUxTheme);
						}
						// . drawing under Windows XP and lower (or if the above drawing failed)
						if (!buttonBackgroundPainted)
							if (cmdInfo->pressed)
								::DrawFrameControl( dc, &r, DFC_BUTTON, DFCS_BUTTONPUSH|DFCS_PUSHED );
							else
								::DrawFrameControl( dc, &r, DFC_BUTTON, DFCS_BUTTONPUSH );
					}
					::SetBkMode(dc,TRANSPARENT);
					const CRideFont font( FONT_WINGDINGS, 130, false, true );
					const HFONT hFont0=(HFONT)::SelectObject( dc, font );
						r.left+=10;
						static const WCHAR Arrow=0xf0e8;
						::DrawTextW( dc, &Arrow,1, &r, DT_SINGLELINE|DT_LEFT|DT_VCENTER );
					::SelectObject( dc, (HGDIOBJ)::SendMessage(::GetParent(hCmdBtn),WM_GETFONT,0,0) );
						r.left+=35;
						TCHAR buf[200];
						::GetWindowText(hCmdBtn,buf,sizeof(buf)/sizeof(TCHAR));
						::DrawText( dc, buf,-1, &r, DT_SINGLELINE|DT_LEFT|DT_VCENTER );
					::SelectObject(dc,hFont0);
				::EndPaint(hCmdBtn,&ps);
				break;
			}
			case WM_NCDESTROY:
				// about to be destroyed
				delete cmdInfo;
				break;
		}
		return ::CallWindowProc( wndProc0, hCmdBtn, msg, wParam, lParam );
	}

	LRESULT CCommandDialog::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_COMMAND:
				if (::GetWindowLong((HWND)lParam,GWL_WNDPROC)==(LONG)__commandLikeButton_wndProc__){
					UpdateData(TRUE);
					EndDialog(wParam);
				}
				break;
		}
		return CDialog::WindowProc(msg,wParam,lParam);
	}

	void CCommandDialog::__convertToCommandLikeButton__(HWND hButton,LPCTSTR text) const{
		// supplies given Button the "command-like" style from Windows Vista
		::SetWindowText(hButton,text);
		::SetWindowLong( hButton, GWL_STYLE, ::GetWindowLong(hButton,GWL_STYLE)|BS_OWNERDRAW );
		::SetWindowLong(hButton,
						GWL_USERDATA,
						(long)new TCommandLikeButtonInfo(
							(WNDPROC)::SetWindowLong( hButton, GWL_WNDPROC, (long)__commandLikeButton_wndProc__ )
						)
					);
		::InvalidateRect(hButton,nullptr,FALSE);
	}

	#define CMDBUTTON_HEIGHT	32
	#define CMDBUTTON_MARGIN	1

	void CCommandDialog::__addCommandButton__(WORD id,LPCTSTR caption){
		// adds a new "command-like" Button with given Id and Caption
		// - increasing the parent window size for the new Button to fit in
		RECT r;
		GetWindowRect(&r);
		SetWindowPos(	nullptr,
						0,0, r.right-r.left, r.bottom-r.top+CMDBUTTON_MARGIN+CMDBUTTON_HEIGHT,
						SWP_NOZORDER|SWP_NOMOVE
					);
		GetClientRect(&r);
		// - creating a new "command-like" Button
		RECT t;
		const CWnd *const pInformation=GetDlgItem(ID_INFORMATION);
		pInformation->GetClientRect(&t);
		pInformation->MapWindowPoints(this,&t);
		__convertToCommandLikeButton__(
			::CreateWindow( WC_BUTTON,nullptr,
							WS_CHILD|WS_VISIBLE,
							t.left, r.bottom-t.top-CMDBUTTON_HEIGHT, t.right-t.left, CMDBUTTON_HEIGHT,
							m_hWnd, (HMENU)id, app.m_hInstance, nullptr
						),
			caption
		);
	}








	CByteIdentity::CByteIdentity(){
		// ctor
		for( BYTE i=0; (values[i]=i)<(BYTE)-1; i++ );
	}

	CByteIdentity::operator PCBYTE() const{
		return values;
	}








	CLocalTime::CLocalTime(){
		// ctor
		SYSTEMTIME st;
		::GetLocalTime(&st);
		(CTimeSpan &)*this=CTimeSpan(st.wDay,st.wHour,st.wMinute,st.wSecond);
		nMilliseconds=st.wMilliseconds;
	}

	CLocalTime::CLocalTime(const CTimeSpan &ts,short nMilliseconds)
		// ctor for internal purposes only
		: CTimeSpan(ts)
		, nMilliseconds(nMilliseconds) {
	}

	CLocalTime CLocalTime::operator+(const CLocalTime &rTime2) const{
		const short tmpMilliseconds=nMilliseconds+rTime2.nMilliseconds;
		return	tmpMilliseconds<0
				? CLocalTime( __super::operator+(CTimeSpan(rTime2.GetDays(),rTime2.GetHours(),rTime2.GetMinutes(),rTime2.GetSeconds()+1)), tmpMilliseconds-1000 )
				: CLocalTime( __super::operator+(rTime2), tmpMilliseconds );
	}

	CLocalTime CLocalTime::operator-(const CLocalTime &rTime2) const{
		const short tmpMilliseconds=nMilliseconds-rTime2.nMilliseconds;
		return	tmpMilliseconds<0
				? CLocalTime( __super::operator-(CTimeSpan(rTime2.GetDays(),rTime2.GetHours(),rTime2.GetMinutes(),rTime2.GetSeconds()+1)), tmpMilliseconds+1000 )
				: CLocalTime( __super::operator-(rTime2), tmpMilliseconds );
	}

	WORD CLocalTime::GetMilliseconds() const{
		return nMilliseconds;
	}

	DWORD CLocalTime::ToMilliseconds() const{
		return GetTotalSeconds()*1000+nMilliseconds;
	}









	#define ERROR_BUFFER_SIZE	220

	PTCHAR __formatErrorCode__(PTCHAR buf,TStdWinError errCode){
		// generates into Buffer a message corresponding to the ErrorCode; assumed that the Buffer is at least ERROR_BUFFER_SIZE characters big
		PTCHAR p;
		if (errCode<=12000)
			// "standard" error
			p=buf+::FormatMessage(	FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errCode, 0,
									buf, ERROR_BUFFER_SIZE-20,
									nullptr
								);
		else
			// WinInet error
			if (errCode!=ERROR_INTERNET_EXTENDED_ERROR)
				// "standard" WinInet error message
				p=buf+::FormatMessage(	FORMAT_MESSAGE_FROM_HMODULE, ::GetModuleHandle(DLL_WININET), errCode, 0,
										buf, ERROR_BUFFER_SIZE-20,
										nullptr
									);
			else{
				// detailed error message from the server
				DWORD tmp, bufLength=ERROR_BUFFER_SIZE-20;
				::InternetGetLastResponseInfo( &tmp, buf, &bufLength );
				p=buf+bufLength;
			}
		::wsprintf( p, _T("(Error %d)"), errCode );
		return buf;
	}



	void FatalError(LPCTSTR text){
		// shows fatal error
		//if (!hParent) hParent=::GetActiveWindow();
		::MessageBox(0,text,nullptr,MB_ICONERROR|MB_TASKMODAL);
	}

	#define ERROR_BECAUSE		_T("%s because:\n\n%s")
	#define ERROR_CONSEQUENCE	_T("\n\n\n%s")

	void FatalError(LPCTSTR text,LPCTSTR causeOfError,LPCTSTR consequence){
		// shows fatal error along with its Cause and immediate Consequence
		TCHAR buf[2000];
		const int n=::wsprintf( buf, ERROR_BECAUSE, text, causeOfError );
		if (consequence)
			::wsprintf( buf+n, ERROR_CONSEQUENCE, consequence );
		FatalError(buf);
	}
	void FatalError(LPCTSTR text,TStdWinError causeOfError,LPCTSTR consequence){
		// shows fatal error along with its Cause and immediate Consequence
		TCHAR buf[ERROR_BUFFER_SIZE];
		FatalError( text, __formatErrorCode__(buf,causeOfError), consequence );
	}




	void Information(LPCTSTR text){
		// shows Textual information
		//if (!hParent) hParent=::GetActiveWindow();
		::MessageBox(0,text,_T("Information"),MB_ICONINFORMATION|MB_TASKMODAL);
	}
	void Information(LPCTSTR text,LPCTSTR causeOfError,LPCTSTR consequence){
		// shows Textual information along with its Cause and immediate Consequence
		TCHAR buf[2000];
		const int n=::wsprintf( buf, ERROR_BECAUSE, text, causeOfError );
		if (consequence)
			::wsprintf( buf+n, ERROR_CONSEQUENCE, consequence );
		Information(buf);
	}
	void Information(LPCTSTR text,TStdWinError causeOfError,LPCTSTR consequence){
		// shows Textual information along with its Cause and immediate Consequence
		TCHAR buf[ERROR_BUFFER_SIZE];
		Information( text, __formatErrorCode__(buf,causeOfError), consequence );
	}




	#define CHECKBOX_MARGIN	10

	static HHOOK hMsgBoxHook;
	static HWND hMsgBox;
	static DWORD checkBoxChecked;
	static LPCTSTR checkBoxMessage;

	static LRESULT CALLBACK __addCheckBox_hook__(int msg,WPARAM wParam,LPARAM lParam){
		// hooking the MessageBox
		static HWND hCheckBox;
		static int checkBoxY;
		static SIZE checkBoxSize;
		switch (msg){
			case HCBT_CREATEWND:{
				// a new window is just being created (but don't necessarily have to be right the MessageBox)
				const LPCREATESTRUCT lpcs=((LPCBT_CREATEWND)lParam)->lpcs;
				if ( lpcs->lpszClass==MAKEINTRESOURCE(WC_DIALOG) ){
					// the window that is being created is a MessageBox (but at the moment it doesn't yet have the window procedure set and we therefore cannot add the CheckBox)
					hMsgBox=(HWND)wParam;
					const HDC dc=::GetDC(hMsgBox);
						::GetTextExtentPoint32(dc,checkBoxMessage,::lstrlen(checkBoxMessage),&checkBoxSize);
					::ReleaseDC(hMsgBox,dc);
					checkBoxSize.cx+=16+CHECKBOX_MARGIN; // 16 = the size of the "checked" icon
					checkBoxY=lpcs->cy - ::GetSystemMetrics(SM_CYCAPTION) - ::GetSystemMetrics(SM_CYBORDER);
					lpcs->cy+=checkBoxSize.cy+CHECKBOX_MARGIN;
					if (lpcs->cx<checkBoxSize.cx+2*CHECKBOX_MARGIN)
						lpcs->cx=checkBoxSize.cx+2*CHECKBOX_MARGIN;
				}else if ( !hCheckBox && hMsgBox ){
					// the window that is being created is MessageBox's child window (e.g. a button)
					hCheckBox++; // for this branch to be not entered when creating the CheckBox ...
					hCheckBox=::CreateWindow(	WC_BUTTON, checkBoxMessage, // ... that is, here!
												WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | checkBoxChecked,
												CHECKBOX_MARGIN,checkBoxY,checkBoxSize.cx,checkBoxSize.cy,hMsgBox,0,AfxGetInstanceHandle(),nullptr
											);
					//Button_SetCheck(hCheckBox,checkBoxChecked);
				}
				return 0;
			}
			case HCBT_DESTROYWND:
				// a window is just being destroyed (but don't necessarily have to be right the MessageBox)
				if ((HWND)wParam==hMsgBox){
					checkBoxChecked=Button_GetCheck(hCheckBox);
					::DestroyWindow(hCheckBox);
					hCheckBox=0;
				}
				break;
		}
		return ::CallNextHookEx(hMsgBoxHook,msg,wParam,lParam);
	}
	bool InformationWithCheckBox(LPCTSTR textInformation,LPCTSTR checkBoxCaption){
		// shows Textual information with CheckBox in the lower bottom corner
		// - hooking the MessageBox
		hMsgBoxHook=::SetWindowsHookEx( WH_CBT, __addCheckBox_hook__, 0, ::GetCurrentThreadId() );
		// - showing the MessageBox, now with CheckBox
		checkBoxChecked=BST_UNCHECKED, checkBoxMessage=checkBoxCaption;
		Information(textInformation);
		// - unhooking the MessageBox
		::UnhookWindowsHookEx(hMsgBoxHook);
		return checkBoxChecked!=BST_UNCHECKED;
	}
	void InformationWithCheckableShowNoMore(LPCTSTR text,LPCTSTR sectionId,LPCTSTR messageId){
		// shows Textual information with a "Show no more" CheckBox
		// - suppressing this message if the user has decided in the past to not show it anymore
		if (app.GetProfileInt(sectionId,messageId,0)) return;
		// - storing user's decision of showing or not this message the next time
		app.WriteProfileInt(	sectionId, messageId,
								InformationWithCheckBox(text,_T("Don't show this message again"))
							);
	}





	bool InformationOkCancel(LPCTSTR text){
		// True <=> user confirmed the shown Textual information, otherwise False
		LOG_DIALOG_DISPLAY(text);
		return LOG_DIALOG_RESULT( ::MessageBox(0,text,_T("Information"),MB_ICONINFORMATION|MB_OKCANCEL|MB_TASKMODAL)==IDOK );
	}





	bool QuestionYesNo(LPCTSTR text,UINT defaultButton){
		// shows a yes-no question
		//if (!hParent) hParent=::GetActiveWindow();
		LOG_DIALOG_DISPLAY(text);
		return LOG_DIALOG_RESULT( ::MessageBox(0,text,_T("Question"),MB_ICONQUESTION|MB_TASKMODAL|MB_YESNO|defaultButton)==IDYES );
	}




	BYTE QuestionYesNoCancel(LPCTSTR text,UINT defaultButton){
		// shows a yes-no question
		//if (!hParent) hParent=::GetActiveWindow();
		LOG_DIALOG_DISPLAY(text);
		return LOG_DIALOG_RESULT( ::MessageBox(0,text,_T("Question"),MB_ICONQUESTION|MB_TASKMODAL|MB_YESNOCANCEL|defaultButton) );
	}
	BYTE QuestionYesNoCancel(LPCTSTR text,UINT defaultButton,LPCTSTR causeOfError,LPCTSTR consequence){
		// shows a yes-no question along with its Cause and immediate Consequence
		TCHAR buf[2000];
		const int n=::wsprintf( buf, ERROR_BECAUSE, text, causeOfError );
		if (consequence)
			::wsprintf( buf+n, ERROR_CONSEQUENCE, consequence );
		return QuestionYesNoCancel(buf,defaultButton);
	}
	BYTE QuestionYesNoCancel(LPCTSTR text,UINT defaultButton,TStdWinError causeOfError,LPCTSTR consequence){
		// shows a yes-no question along with its Cause and immediate Consequence
		TCHAR buf[ERROR_BUFFER_SIZE];
		return QuestionYesNoCancel( text, defaultButton, __formatErrorCode__(buf,causeOfError), consequence );
	}




	BYTE AbortRetryIgnore(LPCTSTR text,UINT defaultButton){
		// shows an abort-retry-ignore question
		//if (!hParent) hParent=::GetActiveWindow();
		LOG_DIALOG_DISPLAY(text);
		return LOG_DIALOG_RESULT( ::MessageBox(0,text,_T("Question"),MB_ICONQUESTION|MB_TASKMODAL|MB_ABORTRETRYIGNORE|defaultButton) );
	}

	BYTE AbortRetryIgnore(LPCTSTR text,TStdWinError causeOfError,UINT defaultButton,LPCTSTR consequence){
		// shows an abort-retry-ignore question along with its Cause
		TCHAR bufCause[ERROR_BUFFER_SIZE], buf[2000];
		const int n=::wsprintf( buf, ERROR_BECAUSE, text, __formatErrorCode__(bufCause,causeOfError) );
		if (consequence)
			::wsprintf( buf+n, ERROR_CONSEQUENCE, consequence );
		return AbortRetryIgnore(buf,defaultButton);
	}

	BYTE AbortRetryIgnore(TStdWinError causeOfError,UINT defaultButton){
		// shows an abort-retry-ignore question
		TCHAR bufCause[ERROR_BUFFER_SIZE];
		return AbortRetryIgnore( __formatErrorCode__(bufCause,causeOfError), defaultButton );
	}

	bool RetryCancel(LPCTSTR text){
		// shows an retry-cancel question
		//if (!hParent) hParent=::GetActiveWindow();
		LOG_DIALOG_DISPLAY(text);
		return LOG_DIALOG_RESULT( ::MessageBox(0,text,_T("Question"),MB_ICONEXCLAMATION|MB_TASKMODAL|MB_RETRYCANCEL|MB_DEFBUTTON1)==IDRETRY );
	}
	bool RetryCancel(TStdWinError causeOfError){
		// shows an retry-cancel question
		TCHAR bufCause[ERROR_BUFFER_SIZE];
		return RetryCancel( __formatErrorCode__(bufCause,causeOfError) );
	}

	void Warning(LPCTSTR text){
		// shows Textual warning
		//if (!hParent) hParent=::GetActiveWindow();
		::MessageBox(0,text,_T("Warning"),MB_ICONINFORMATION|MB_TASKMODAL);
	}

	bool EnableDlgControls(HWND hDlg,PCWORD controlIds,bool enabled){
		// enables/disables all specified Dialog controls and returns this new state
		while (const WORD id=*controlIds++)
			::EnableWindow( ::GetDlgItem(hDlg,id), enabled );
		return enabled;
	}

	void BytesToHigherUnits(DWORD bytes,float &rHigherUnit,LPCTSTR &rHigherUnitName){
		// converts Bytes to suitable HigherUnits (e.g. "12345 Bytes" to "12.345 kiB")
		if (bytes>=0x40000000)
			rHigherUnit=(float)bytes/0x40000000, rHigherUnitName=_T("GiB");
		else if (bytes>=0x100000)
			rHigherUnit=(float)bytes/0x100000, rHigherUnitName=_T("MiB");
		else if (bytes>=0x400)
			rHigherUnit=(float)bytes/0x400, rHigherUnitName=_T("kiB");
		else
			rHigherUnit=bytes, rHigherUnitName=_T("Bytes");
	}

	void NavigateToUrlInDefaultBrowser(LPCTSTR url){
		// opens specified URL in user's default browser
		::CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );
		if ((int)::ShellExecute( 0, nullptr, url, nullptr, nullptr, SW_SHOWDEFAULT )<=32){
			TCHAR buf[300];
			::wsprintf(buf,_T("Cannot navigate to\n%s\n\nDo you want to copy the link to clipboard?"),url);
			if (QuestionYesNo(buf,MB_DEFBUTTON1))
				if (::OpenClipboard(0)){
					::EmptyClipboard();
					const HGLOBAL h=::GlobalAlloc(GMEM_MOVEABLE,1+::lstrlen(url));
						::lstrcpy((PTCHAR)::GlobalLock(h),url);
					::GlobalUnlock(h);
					::SetClipboardData(CF_TEXT,h);
					::CloseClipboard();
				}else
					FatalError(_T("Couldn't copy to clipboard"),::GetLastError());
		}
	}

	#define BRACKET_CURLY_FONT_SIZE	12

	void DrawClosingCurlyBracket(HDC dc,int x,int yMin,int yMax){
		// draws a closing curly bracket at position X and wrapping all points in {yMin,...,yMax}
		const CRideFont font( FONT_SYMBOL, BRACKET_CURLY_FONT_SIZE*10, false, true );
		::SetBkMode(dc,TRANSPARENT);
		const HFONT hFont0=(HFONT)::SelectObject(dc,font);
			RECT r={ x, yMin, x+100, yMax };
			static const WCHAR CurveUpper=0xf0fc;
			::DrawTextW( dc, &CurveUpper,1, &r, DT_LEFT|DT_TOP|DT_SINGLELINE );
			static const WCHAR CurveLower=0xf0fe;
			::DrawTextW( dc, &CurveLower,1, &r, DT_LEFT|DT_BOTTOM|DT_SINGLELINE );
			static const WCHAR CurveMiddle=0xf0fd;
			::DrawTextW( dc, &CurveMiddle,1, &r, DT_LEFT|DT_VCENTER|DT_SINGLELINE );
			SIZE fontSize;
			::GetTextExtentPoint32W(dc,&CurveMiddle,1,&fontSize);
			r.top+=fontSize.cy/5, r.bottom-=fontSize.cy/5;
			while (r.bottom-r.top>2.2*fontSize.cy){
				static const WCHAR CurveStraight=0xf0ef;
				::DrawTextW( dc, &CurveStraight,1, &r, DT_LEFT|DT_TOP|DT_SINGLELINE );
				::DrawTextW( dc, &CurveStraight,1, &r, DT_LEFT|DT_BOTTOM|DT_SINGLELINE );
				r.top++, r.bottom--;
			}
		::SelectObject(dc,hFont0);
	}

	void WrapControlsByClosingCurlyBracketWithText(CWnd *wnd,const CWnd *pCtrlA,const CWnd *pCtrlZ,LPCTSTR text,DWORD textColor){
		// wraps ControlsA-Z from right using closing curly brackets and draws given Text in given Color
		// - drawing curly brackets
		RECT rCtrlA,rCtrlZ;
		pCtrlA->GetClientRect(&rCtrlA), pCtrlA->MapWindowPoints(wnd,&rCtrlA);
		pCtrlZ->GetClientRect(&rCtrlZ), pCtrlZ->MapWindowPoints(wnd,&rCtrlZ);
		RECT r={ max(rCtrlA.right,rCtrlZ.right)+5, rCtrlA.top-6, 1000, rCtrlZ.bottom+6 };
		CClientDC dc(wnd);
		dc.SetTextColor( textColor );
		DrawClosingCurlyBracket( dc, r.left, r.top, r.bottom );
		// . text
		r.left+=14*LogicalUnitScaleFactor;
		const HFONT hFont0=(HFONT)::SelectObject( dc, pCtrlA->GetFont()->m_hObject );
			dc.DrawText( text,-1, &r, DT_VCENTER|DT_SINGLELINE );
		::SelectObject(dc,hFont0);
	}








	typedef const struct TSplitButtonInfo sealed{
		const PCSplitButtonAction pAction;
		const BYTE nActions;
		const WNDPROC wndProc0;
		RECT rcClientArea;

		TSplitButtonInfo(HWND hBtn,PCSplitButtonAction _pAction,BYTE _nActions,WNDPROC _wndProc0)
			// ctor
			: pAction(_pAction) , nActions(_nActions) , wndProc0(_wndProc0) {
			::GetClientRect(hBtn,&rcClientArea);
		}
	} *PCSplitButtonInfo;

	#define SPLITBUTTON_ARROW_WIDTH	18*LogicalUnitScaleFactor

	static LRESULT WINAPI __splitButton_wndProc__(HWND hSplitBtn,UINT msg,WPARAM wParam,LPARAM lParam){
		const PCSplitButtonInfo psbi=(PCSplitButtonInfo)::GetWindowLong(hSplitBtn,GWL_USERDATA);
		const WNDPROC wndProc0=psbi->wndProc0;
		switch (msg){
			case WM_GETDLGCODE:
				// the SplitButton must receive all keyboard input (it may not receive a Tab keystroke if part of a dialog or CControlBar)
				return DLGC_WANTALLKEYS;
			case WM_LBUTTONDBLCLK:
			case WM_LBUTTONDOWN:
				// left mouse button pressed
				if (GET_X_LPARAM(lParam)<psbi->rcClientArea.right-SPLITBUTTON_ARROW_WIDTH)
					// in default Action area
					break; // base
				else{
					// in area of selecting additional Actions
					CMenu mnu;
					mnu.CreatePopupMenu();
					for( BYTE id=0; id<psbi->nActions; id++ )
						mnu.AppendMenu( MF_STRING, psbi->pAction[id].commandId, psbi->pAction[id].commandCaption );
					POINT pt={ psbi->rcClientArea.right-SPLITBUTTON_ARROW_WIDTH, psbi->rcClientArea.bottom };
					::ClientToScreen( hSplitBtn, &pt );
					::TrackPopupMenu( mnu.m_hMenu, TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON, pt.x, pt.y, 0, ::GetParent(hSplitBtn), nullptr );
					//fallthrough
				}
			case WM_LBUTTONUP:
				// left mouse button released
				if (GET_X_LPARAM(lParam)<psbi->rcClientArea.right-SPLITBUTTON_ARROW_WIDTH)
					// in default Action area
					break; // base
				else{
					// in area of selecting additional Actions
					::ReleaseCapture();
					return 0;
				}
			case WM_PAINT:{
				// drawing
				// . base
				::CallWindowProc( wndProc0, hSplitBtn, msg, wParam, lParam );
				// . drawing
				const HDC dc=::GetDC(hSplitBtn);
					::SetBkMode(dc,TRANSPARENT);
					// : caption of 0.Action (the default)
					RECT r=psbi->rcClientArea;
					r.right-=SPLITBUTTON_ARROW_WIDTH;
					if (!::IsWindowEnabled(hSplitBtn))
						::SetTextColor( dc, ::GetSysColor(COLOR_GRAYTEXT) );
					const HGDIOBJ hFont0=::SelectObject( dc, (HGDIOBJ)::SendMessage(::GetParent(hSplitBtn),WM_GETFONT,0,0) );
						::DrawText( dc, psbi->pAction->commandCaption,-1, &r, DT_SINGLELINE|DT_CENTER|DT_VCENTER );
					//::SelectObject(dc,hFont0);
					// : arrow
					r=psbi->rcClientArea;
					r.left=r.right-SPLITBUTTON_ARROW_WIDTH;
					const CRideFont font( FONT_WEBDINGS, 110, false, true );
					::SelectObject( dc, font );
						static const WCHAR Arrow=0xf036;
						::DrawTextW( dc, &Arrow,1, &r, DT_SINGLELINE|DT_CENTER|DT_VCENTER );
					::SelectObject(dc,hFont0);
					// : splitting using certical line
					LOGPEN logPen={ PS_SOLID, {1,1}, ::GetSysColor(COLOR_BTNSHADOW) };
					const HGDIOBJ hPen0=::SelectObject( dc, ::CreatePenIndirect(&logPen) );
						::MoveToEx( dc, r.left, 1, nullptr );
						::LineTo( dc, r.left, --r.bottom );
					logPen.lopnColor=::GetSysColor(COLOR_BTNHIGHLIGHT);
					::DeleteObject( ::SelectObject(dc,::CreatePenIndirect(&logPen)) );
						::MoveToEx( dc, ++r.left, 1, nullptr );
						::LineTo( dc, r.left, r.bottom );
					::DeleteObject( ::SelectObject(dc,hPen0) );
				::ReleaseDC(hSplitBtn,dc);
				return 0;
			}
			case WM_NCDESTROY:
				// about to be destroyed
				delete psbi;
				break;
		}
		return ::CallWindowProc( wndProc0, hSplitBtn, msg, wParam, lParam );
	}

	void ConvertToSplitButton(HWND hStdBtn,PCSplitButtonAction pAction,BYTE nActions){
		// converts an existing standard button to a SplitButton featuring specified additional Actions
		::SetWindowText(hStdBtn,nullptr);
		::SetWindowLong(hStdBtn,GWL_ID,pAction->commandId); // 0.Action is the default
		::SetWindowLong(hStdBtn, GWL_USERDATA,
						(long)new TSplitButtonInfo(
							hStdBtn,
							pAction,
							nActions,
							(WNDPROC)::SetWindowLong( hStdBtn, GWL_WNDPROC, (long)__splitButton_wndProc__ )
						)
					);
		::InvalidateRect(hStdBtn,nullptr,TRUE);
	}

	void SetSingleCharTextUsingFont(HWND hWnd,WCHAR singleChar,LPCTSTR fontFace,int fontPointSize){
		// sets given window's text to the SingleCharacter displayed in specified Font
		const WCHAR buf[]={ singleChar, '\0' };
		::SetWindowTextW( hWnd, buf );
		::SendMessage( hWnd, WM_SETFONT, (WPARAM)CRideFont(fontFace,fontPointSize,false,true).Detach(), 0 );
	}

	void PopulateComboBoxWithSequenceOfNumbers(HWND hComboBox,BYTE iStartValue,LPCTSTR strStartValueDesc,BYTE iEndValue,LPCTSTR strEndValueDesc){
		// fills ComboBox with integral numbers from the {Start,End} range (in ascending order)
		TCHAR buf[80];
		::wsprintf( buf, _T("%d %s"), iStartValue, strStartValueDesc );
		ComboBox_AddString( hComboBox, buf );
		while (++iStartValue<iEndValue)
			ComboBox_AddString( hComboBox, _itot(iStartValue,buf,10) );
		::wsprintf( buf, _T("%d %s"), iEndValue, strEndValueDesc );
		ComboBox_AddString( hComboBox, buf );
	}

	#define SCREEN_DPI_DEFAULT	96

	static float __getLogicalUnitScaleFactor__(){
		// computes and returns the factor (from (0;oo)) to multiply the size of one logical unit with; returns 1 if the logical unit size doesn't have to be changed
		const CClientDC screen(nullptr);
		return	min(::GetDeviceCaps(screen,LOGPIXELSX)/(float)SCREEN_DPI_DEFAULT,
					::GetDeviceCaps(screen,LOGPIXELSY)/(float)SCREEN_DPI_DEFAULT
				);
	}

	const float LogicalUnitScaleFactor=__getLogicalUnitScaleFactor__();

	float ScaleLogicalUnit(HDC dc){
		// changes given DeviceContext's size of one logical unit; returns the Factor using which the logical unit size has been multiplied with
		const float factor=LogicalUnitScaleFactor;
		if (factor!=1){
			::SetMapMode(dc,MM_ISOTROPIC);
			::SetWindowExtEx( dc, SCREEN_DPI_DEFAULT, SCREEN_DPI_DEFAULT, nullptr );
			::SetViewportExtEx( dc, ::GetDeviceCaps(dc,LOGPIXELSX), ::GetDeviceCaps(dc,LOGPIXELSY), nullptr );
		}
		return factor;
	}

	void UnscaleLogicalUnit(PINT values,BYTE nValues){
		// removes from specified Values the logical unit scale factor
		for( const float dpiScaleFactor=LogicalUnitScaleFactor; nValues--; *values++/=dpiScaleFactor );
	}

	COLORREF GetSaturatedColor(COLORREF currentColor,float saturationFactor){
		// saturates input Color by specified SaturationFactor and returns the result
		ASSERT(saturationFactor>=0);
		COLORREF result=0;
		for( BYTE i=sizeof(COLORREF),*pbIn=(PBYTE)&currentColor,*pbOut=(PBYTE)&result; i-->0; ){
			const WORD w=*pbIn++*saturationFactor;
			*pbOut++=min(w,255);
		}
		return result;
	}

	COLORREF GetBlendedColor(COLORREF color1,COLORREF color2,float blendFactor){
		// computes and returns the Color that is the mixture of the two input Colors in specified ratio (BlendFactor=0 <=> only Color1, BlendFactor=1 <=> only Color2
		ASSERT(0.f<=blendFactor && blendFactor<=1.f);
		COLORREF result=0;
		for( BYTE i=sizeof(COLORREF),*pbIn1=(PBYTE)&color1,*pbIn2=(PBYTE)&color2,*pbOut=(PBYTE)&result; i-->0; ){
			const WORD w = blendFactor**pbIn1++ + (1.f-blendFactor)**pbIn2++;
			*pbOut++=min(w,255);
		}
		return result;
	}

	CFile &WriteToFile(CFile &f,LPCTSTR text){
		// writes specified Text into the File
		f.Write( text, ::lstrlen(text) );
		return f;
	}
	CFile &WriteToFile(CFile &f,TCHAR chr){
		// writes specified Character into the File
		f.Write( &chr, sizeof(TCHAR) );
		return f;
	}
	CFile &WriteToFile(CFile &f,int number,LPCTSTR formatting){
		// writes specified Number into the File using the given Formatting
		TCHAR buf[16];
		::wsprintf( buf, formatting, number );
		return WriteToFile(f,buf);
	}
	CFile &WriteToFile(CFile &f,int number){
		// writes specified Number into the File
		return WriteToFile(f,number,_T("%d"));
	}
	CFile &WriteToFile(CFile &f,double number,LPCTSTR formatting){
		// writes specified Number into the File
		if (!formatting) // if no explicit Formatting specified ...
			if ((int)number==number) // ... and the Number doesn't have decimal digits (only integral part) ...
				return WriteToFile(f,(int)number); // ... then simply writing the number as an integer without a decimal point ...
			else
				formatting=_T("%f"); // ... otherwise using the "default" Formatting
		TCHAR buf[512]; // just in case the number had really many digits (not a problem for a Double)
		_stprintf( buf, formatting, number );
		return WriteToFile(f,buf);
	}
	CFile &WriteToFile(CFile &f,double number){
		// writes specified Number into the File
		return WriteToFile(f,number,nullptr);
	}

	PTCHAR GetApplicationOnlineFileUrl(LPCTSTR documentName,PTCHAR buffer){
		// fills the Buffer with URL of a file that is part of this application's on-line presentation, and returns the Buffer
		#ifdef _DEBUG
			return ::lstrcat( ::lstrcpy(buffer,_T("file://c:/Documents and Settings/Tom/Plocha/ride/www/")), documentName );
		#else
			return ::lstrcat( ::lstrcpy(buffer,_T("http://nestorovic.hyperlink.cz/ride/")), documentName );
		#endif
	}

	PTCHAR GetApplicationOnlineHtmlDocumentUrl(LPCTSTR documentName,PTCHAR buffer){
		// fills the Buffer with URL of an HTML document that is part of this application's on-line presentation, and returns the Buffer
		return ::lstrcat( GetApplicationOnlineFileUrl(_T("html/"),buffer), documentName );
	}











	#pragma pack(1)
	struct TDownloadSingleFileParams sealed{
		const LPCTSTR onlineFileUrl;
		const PBYTE buffer;
		const DWORD bufferSize;
		const LPCTSTR fatalErrConsequence;
		DWORD outOnlineFileSize;

		TDownloadSingleFileParams(LPCTSTR onlineFileUrl,PBYTE buffer,DWORD bufferSize,LPCTSTR fatalErrConsequence)
			// ctor
			: onlineFileUrl(onlineFileUrl)
			, buffer(buffer) , bufferSize(bufferSize)
			, fatalErrConsequence(fatalErrConsequence)
			, outOnlineFileSize(-1) {
		}
	};

	static UINT AFX_CDECL __downloadSingleFile_thread__(PVOID _pCancelableAction){
		// thread to download an on-line file with given URL to a local Buffer; caller is to dimension the Buffer so that it can contain the whole on-line file
		TBackgroundActionCancelable *const pAction=(TBackgroundActionCancelable *)_pCancelableAction;
		TDownloadSingleFileParams &rdsfp=*(TDownloadSingleFileParams *)pAction->fnParams;
		HINTERNET hSession=nullptr, hOnlineFile=nullptr;
		// - opening a new Session
		hSession=::InternetOpen(APP_IDENTIFIER,
								INTERNET_OPEN_TYPE_PRECONFIG,
								nullptr, nullptr,
								0
							);
		if (hSession==nullptr)
			goto quitWithErr;
		// - opening the on-line file with given URL
		hOnlineFile=::InternetOpenUrl(	hSession,
										rdsfp.onlineFileUrl,
										nullptr, 0,
										INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
										INTERNET_NO_CALLBACK
									);
		if (hOnlineFile==nullptr)
			goto quitWithErr;
		// - reading the on-line file to the Buffer, allocated and initialized by the caller; caller is to dimension the Buffer so that it can contain the whole on-line file
		if (!::InternetReadFile( hOnlineFile, rdsfp.buffer, rdsfp.bufferSize, &rdsfp.outOnlineFileSize )){
quitWithErr:const DWORD err=::GetLastError();
			FatalError( _T("File download failed"), err, rdsfp.fatalErrConsequence );
			if (hOnlineFile!=nullptr)
				::InternetCloseHandle(hOnlineFile);
			if (hSession!=nullptr)
				::InternetCloseHandle(hSession);
			return pAction->TerminateWithError(err);
		}
		// - downloaded successfully
		return pAction->TerminateWithError(ERROR_SUCCESS);
	}

	TStdWinError DownloadSingleFile(LPCTSTR onlineFileUrl,PBYTE fileDataBuffer,DWORD fileDataBufferLength,PDWORD pDownloadedFileSize,LPCTSTR fatalErrorConsequence){
		// returns the result of downloading the file with given Url
		TDownloadSingleFileParams params( onlineFileUrl, fileDataBuffer, fileDataBufferLength, fatalErrorConsequence );
		const TStdWinError err=TBackgroundActionCancelable(__downloadSingleFile_thread__,&params,THREAD_PRIORITY_ABOVE_NORMAL).CarryOut(-1);
		if (pDownloadedFileSize!=nullptr)
			*pDownloadedFileSize=params.outOnlineFileSize;
		return err;
	}

}
