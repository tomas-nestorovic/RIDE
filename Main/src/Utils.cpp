#include "stdafx.h"

namespace Utils{

	#define SCREEN_DPI_DEFAULT	96

	TLogicalUnitScaleFactor::TLogicalUnitScaleFactor()
		: TRationalNumber(0,1) {
		// ctor; computes the factor (from (0;oo)) to multiply the size of one logical unit with; returns 1 if the logical unit size doesn't have to be changed
		const CClientDC screen(nullptr);
		quot=std::min( ::GetDeviceCaps(screen,LOGPIXELSX), ::GetDeviceCaps(screen,LOGPIXELSY) );
		rem=SCREEN_DPI_DEFAULT;
	}

	const TLogicalUnitScaleFactor LogicalUnitScaleFactor;




	CExclusivelyLocked::CExclusivelyLocked(CSyncObject &syncObj)
		// ctor
		: syncObj(syncObj) {
		syncObj.Lock();
	}

	CExclusivelyLocked::~CExclusivelyLocked(){
		// ctor
		syncObj.Unlock();
	}




	CRidePen::CRidePen(BYTE thickness,COLORREF color)
		// ctor
		: CPen(PS_SOLID,thickness,color) {
	}

	CRidePen::CRidePen(BYTE thickness,COLORREF color,UINT style)
		// ctor
		: CPen(style,thickness,color) {
	}

	const CRidePen CRidePen::BlackHairline(0,0);
	const CRidePen CRidePen::WhiteHairline(0,0xffffff);
	const CRidePen CRidePen::RedHairline(0,0xff);

	


	CRideBrush::CRideBrush(int stockObjectId){
		// ctor
		CreateStockObject(stockObjectId);
	}

	CRideBrush::CRideBrush(COLORREF solidColor){
		// ctor
		CreateSolidBrush(solidColor);
	}

	CRideBrush::CRideBrush(bool sysColor,int sysColorId){
		// ctor
		CreateSysColorBrush(sysColorId);
	}

	CRideBrush::CRideBrush(CRideBrush &&r){
		// move ctor
		Attach( r.Detach() );
	}

	const CRideBrush CRideBrush::None=NULL_BRUSH;
	const CRideBrush CRideBrush::Black=BLACK_BRUSH;
	const CRideBrush CRideBrush::White=WHITE_BRUSH;
	const CRideBrush CRideBrush::BtnFace(true,COLOR_BTNFACE);
	const CRideBrush CRideBrush::Selection(true,COLOR_ACTIVECAPTION);

	CRideBrush::operator COLORREF() const{
		// 
		LOGBRUSH lb;
		::GetObject( m_hObject, sizeof(lb), &lb );
		return lb.lbColor;
	}



	CRideFont::CRideFont(LPCTSTR face,int pointHeight,bool bold,bool dpiScaled,int pointWidth){
		// ctor
		// - creating the Font
		//CreatePointFont(pointHeight,face);
		Utils::TRationalNumber scaleFactor=Utils::LogicalUnitScaleFactor;
		if (!dpiScaled)
			scaleFactor.quot = scaleFactor.rem = 1;
		const int fontHeight=scaleFactor*(10*-pointHeight)/72, fontWidth=scaleFactor*(10*-pointWidth)/72;
		CreateFont( fontHeight, fontWidth, 0, 0,
					bold*FW_BOLD,
					FALSE, FALSE, FALSE,
					DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
					ANTIALIASED_QUALITY,
					FF_DONTCARE,
					face
				);
/*		if (bold){
			LOGFONT logFont;
			GetObject(sizeof(logFont),&logFont);
				logFont.lfWeight=FW_BOLD;
			DeleteObject();
			CreateFontIndirect(&logFont);
		}*/
		// - determining the AvgWidth and Height of Font characters
		CClientDC dc(app.m_pMainWnd);
		const HGDIOBJ hFont0=::SelectObject( dc, m_hObject );
			TEXTMETRIC tm;
			dc.GetTextMetrics(&tm);
			charAvgWidth=tm.tmAveCharWidth;
			charHeight=tm.tmHeight;
		::SelectObject(dc,hFont0);
	}

	CRideFont::CRideFont(HWND hWnd,bool bold){
		// ctor
		// - creating the Font
		LOGFONT logFont;
			::GetObject( (HGDIOBJ)::SendMessage(hWnd,WM_GETFONT,0,0), sizeof(logFont), &logFont );
			if (bold)
				logFont.lfWeight=FW_BOLD;
		CreateFontIndirect(&logFont);
		// - determining the AvgWidth and Height of Font characters
		CClientDC dc(app.m_pMainWnd);
		const HGDIOBJ hFont0=::SelectObject( dc, m_hObject );
			TEXTMETRIC tm;
			dc.GetTextMetrics(&tm);
			charAvgWidth=tm.tmAveCharWidth;
			charHeight=tm.tmHeight;
		::SelectObject(dc,hFont0);
	}

	SIZE CRideFont::GetTextSize(LPCTSTR text,int textLength) const{
		// determines and returns the Size of the specified Text using using this font face
		SIZE result={ 0, 0 };
		const CClientDC screen(nullptr);
		const HGDIOBJ hFont0=::SelectObject( screen, m_hObject );
			for( LPCTSTR subA=text,subZ=subA; *subA; subZ++ )
				switch (*subZ){
					case '\0':
					case '\n':{
						const SIZE tmp=screen.GetTextExtent( subA, subZ-subA );
						if (tmp.cx>result.cx)
							result.cx=tmp.cx;
						result.cy+=charHeight;
						subA=subZ+(*subZ!='\0');
						break;
					}
				}
		::SelectObject( screen, hFont0 );
		result.cx*=LogicalUnitScaleFactor, result.cy*=LogicalUnitScaleFactor;
		return result;
	}

	SIZE CRideFont::GetTextSize(LPCTSTR text) const{
		// determines and returns the Size of the specified Text using using this font face
		return GetTextSize( text, ::lstrlen(text) );
	}

	const CRideFont CRideFont::Small(FONT_MS_SANS_SERIF,70,false,false);
	const CRideFont CRideFont::Std(FONT_MS_SANS_SERIF,90,false,false);
	const CRideFont CRideFont::StdBold(FONT_MS_SANS_SERIF,90,true,false);




	void CRideContextMenu::UpdateUI(CWnd *pUiUpdater,CMenu *pMenu){
		// leveraging OnCmdMsg processing to adjust UI of the ContextMenu
		CCmdUI state;
			state.m_pMenu=pMenu;
			state.m_pParentMenu=pMenu;
			state.m_nIndexMax=pMenu->GetMenuItemCount();
		for( state.m_nIndex=0; state.m_nIndex<state.m_nIndexMax; state.m_nIndex++ )
			switch ( state.m_nID=pMenu->GetMenuItemID(state.m_nIndex) ){
				case 0:
					// menu separators and invalid commands are ignored
					continue;
				case UINT_MAX:
					// recurrently updating Submenus
					if(CMenu *const pSubMenu=pMenu->GetSubMenu(state.m_nIndex))
						UpdateUI( pUiUpdater, pSubMenu );
					break;
				default:
					// normal menu item
					state.m_pSubMenu=nullptr;
					if (pUiUpdater)
						state.DoUpdate( pUiUpdater, FALSE ); // False = don't auto-disable unroutable items
					break;
			}
	}

	CRideContextMenu::CRideContextMenu(HMENU hMenuOwnedByCaller){
		// ctor (for internal purposes only)
		Attach( hMenuOwnedByCaller ); // caller in charge to destroy the menu, see dtor
	}

	CRideContextMenu::CRideContextMenu(UINT idMenuRes,CWnd *pUiUpdater){
		// ctor
		parent.LoadMenu(idMenuRes);
		Attach( parent.GetSubMenu(0)->m_hMenu );
		if (pUiUpdater->GetSafeHwnd())
			UpdateUI( pUiUpdater, this );
	}

	CRideContextMenu::~CRideContextMenu(){
		// dtor
		Detach(); // whole menu will be disposed by disposing the Parent
	}

	CString CRideContextMenu::GetMenuString(UINT uIDItem,UINT flags) const{
		//
		TCHAR buf[80];
		if (::GetMenuString( m_hMenu, uIDItem, buf, ARRAYSIZE(buf), flags )<=0)
			*buf='\0';
		return buf;
	}

	bool CRideContextMenu::InsertAfter(WORD existingId,UINT nFlags,UINT_PTR nIDNewItem,LPCTSTR lpszNewItem){
		//
		for( int i=0; i<GetMenuItemCount(); i++ )
			switch (const UINT id=GetMenuItemID(i)){
				case 0:
					// menu separators and invalid commands are ignored
					continue;
				case UINT_MAX:
					// recurrently processing Submenus
					if (const CMenu *const pSubMenu=GetSubMenu(i))
						if (CRideContextMenu(*pSubMenu).InsertAfter( existingId, nFlags, nIDNewItem, lpszNewItem ))
							return true;
					break;
				default:
					// normal menu item
					if (id==existingId)
						if (::GetMenuString( *this, nIDNewItem, nullptr, 0, MF_BYCOMMAND )>0) // does requested new command already exist?
							return true;
						else
							return InsertMenu( ++i, MF_BYPOSITION|MF_STRING, nIDNewItem, lpszNewItem )!=FALSE;
					break;
			}
		return false;
	}

	bool CRideContextMenu::ModifySubmenu(UINT uPosition,HMENU hNewSubmenu){
		//
		return	ModifyMenu( uPosition, MF_BYPOSITION|MF_POPUP, (UINT_PTR)hNewSubmenu, GetMenuStringByPos(uPosition) )!=FALSE;
	}

	int CRideContextMenu::GetPosByContainedSubcommand(WORD cmd) const{
		//
		for( int i=0; i<GetMenuItemCount(); i++ )
			if (const HMENU hSubmenu=::GetSubMenu( *this, i )) // interested only in submenus if they contain specified Command
				if (::GetMenuPosFromID( hSubmenu, cmd )>=0)
					return i;
		return -1; // none of the immediate Submenus contains the specified Command
	}




#ifdef UNICODE
	static_assert( false, "Unicode support not implemented" );
#else
	CString ToStringT(LPCWSTR lpsz){
		// converts Unicode to UTF-8
		CString result;
		::WideCharToMultiByte(
			CP_UTF8, 0,
			lpsz,-1,
			result.GetBufferSetLength(
				::WideCharToMultiByte( CP_UTF8, 0, lpsz,-1, nullptr,0, nullptr,nullptr )
			), SHRT_MAX,
			nullptr,nullptr
		);
		return result;
	}

	LPCWSTR ToStringW(LPCSTR lpszUtf8){
		// converts input UTF-8 string to Unicode, making use of static object - BEWARE CONCURRENCY!
		static CString unicode;
		::MultiByteToWideChar(
			CP_UTF8, 0,
			lpszUtf8,-1,
			(PWCHAR)unicode.GetBufferSetLength( (::lstrlen(lpszUtf8)+1)*sizeof(WCHAR) ), // pessimistic estimation
			SHRT_MAX
		);
		return (LPCWSTR)(LPCTSTR)unicode;
	}
#endif




	CCommandDialog::CCommandDialog(LPCTSTR _information)
		// ctor
		: CRideDialog( IDR_ACTION_DIALOG, CWnd::FromHandle(app.GetEnabledActiveWindow()) )
		, information(_information) , defaultCommandId(0) , checkBoxStatus(BST_UNCHECKED) {
	}

	CCommandDialog::CCommandDialog(WORD dialogId,LPCTSTR _information)
		// ctor
		: CRideDialog( dialogId, CWnd::FromHandle(app.GetEnabledActiveWindow()) )
		, information(_information) , defaultCommandId(0) , checkBoxStatus(BST_UNCHECKED) {
	}

	BOOL CCommandDialog::OnInitDialog(){
		// dialog initialization
		// - base
		__super::OnInitDialog();
		// - initializing the main message
		SetFocus();
		::SetDlgItemTextW( m_hWnd, ID_INFORMATION, ToStringW(information) );
		const HWND hInfo=GetDlgItemHwnd(ID_INFORMATION);
		int infoHeight=Edit_GetLineCount(hInfo)*CRideFont(*this).charHeight;
		RECT r;
		::GetClientRect( hInfo, &r );
		SetDlgItemSize( ID_INFORMATION, r.right, infoHeight );
		infoHeight-=r.bottom; // now the difference
		// - increasing the window size for the Information to fit in
		GetWindowRect(&r);
		SetWindowPos(
			nullptr,
			0,0, r.right-r.left, r.bottom-r.top+infoHeight,
			SWP_NOZORDER|SWP_NOMOVE
		);
		return FALSE; // False = focus already set manually
	}

	void CCommandDialog::DoDataExchange(CDataExchange *pDX){
		// exchange of data from and to controls
		DDX_Check( pDX, ID_APPLY, checkBoxStatus );
	}

	typedef struct TCommandLikeButtonInfo sealed{
		const WNDPROC wndProc0;
		const COLORREF textColor, glyphColor;
		const WCHAR wingdingsGlyphBeforeText;
		const int glyphPointSizeIncrement;
		bool cursorHovering, pressed;
		TCommandLikeButtonInfo(WNDPROC _wndProc0,WCHAR wingdingsGlyphBeforeText,COLORREF glyphColor,int glyphPointSizeIncrement,COLORREF textColor)
			// ctor
			: wndProc0(_wndProc0) , textColor(textColor)
			, wingdingsGlyphBeforeText(wingdingsGlyphBeforeText) , glyphColor(glyphColor) , glyphPointSizeIncrement(glyphPointSizeIncrement)
			, cursorHovering(false) , pressed(false) {
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
						if (const HTHEME hTheme=UxTheme::OpenThemeData(hCmdBtn,WC_BUTTONW)){
							buttonBackgroundPainted=UxTheme::DrawThemeBackground( hTheme, dc, BP_PUSHBUTTON, cmdInfo->pressed?PBS_PRESSED:PBS_HOT, &r, nullptr )==S_OK;
							UxTheme::CloseThemeData(hTheme);
						}
						// . drawing under Windows XP and lower (or if the above drawing failed)
						if (!buttonBackgroundPainted)
							if (cmdInfo->pressed)
								::DrawFrameControl( dc, &r, DFC_BUTTON, DFCS_BUTTONPUSH|DFCS_PUSHED );
							else
								::DrawFrameControl( dc, &r, DFC_BUTTON, DFCS_BUTTONPUSH );
					}
					::SetBkMode(dc,TRANSPARENT);
					TCHAR text[200];
					::GetWindowText(hCmdBtn,text,ARRAYSIZE(text));
					if (const WCHAR glyph=cmdInfo->wingdingsGlyphBeforeText){
						// prefixing the Text with specified Glyph
						const CRideFont font( FONT_WINGDINGS, 130+cmdInfo->glyphPointSizeIncrement, false, true );
						const HGDIOBJ hFont0=::SelectObject( dc, font );
							r.left+=10;
							::SetTextColor( dc, cmdInfo->glyphColor );
							::DrawTextW( dc, &glyph,1, &r, DT_SINGLELINE|DT_LEFT|DT_VCENTER );
						::SelectObject( dc, (HGDIOBJ)::SendMessage(hCmdBtn,WM_GETFONT,0,0) );
							r.left+=35;
							::SetTextColor( dc, cmdInfo->textColor );
							::DrawText( dc, text,-1, &r, DT_SINGLELINE|DT_LEFT|DT_VCENTER );
						::SelectObject(dc,hFont0);
					}else{
						// keeping the text without prefix
						const HGDIOBJ hFont0=::SelectObject( dc, (HGDIOBJ)::SendMessage(::GetParent(hCmdBtn),WM_GETFONT,0,0) );
							r.left+=10;
							::SetTextColor( dc, cmdInfo->textColor );
							::DrawText( dc, text,-1, &r, DT_SINGLELINE|DT_LEFT|DT_VCENTER );
						::SelectObject(dc,hFont0);
					}
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
				if (wParam==IDOK)
					if (defaultCommandId)
						wParam=defaultCommandId;
					else
						return 0;
				if (::GetWindowLong((HWND)lParam,GWL_WNDPROC)==(LONG)__commandLikeButton_wndProc__){
					UpdateData(TRUE);
					EndDialog(wParam);
					return 0;
				}
				break;
		}
		return __super::WindowProc(msg,wParam,lParam);
	}

	#define CMDBUTTON_HEIGHT	32
	#define CMDBUTTON_MARGIN	1

	void CCommandDialog::AddButton(WORD id,LPCTSTR caption,WCHAR wingdingsGlyphBeforeText){
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
		const RECT t=MapDlgItemClientRect(ID_INFORMATION);
		ConvertToCommandLikeButton(
			::CreateWindow(
				WC_BUTTON,caption,
				WS_CHILD|WS_VISIBLE,
				t.left, r.bottom-t.top-CMDBUTTON_HEIGHT, t.right-t.left, CMDBUTTON_HEIGHT,
				m_hWnd,
				(HMENU)id,
				app.m_hInstance, nullptr
			),
			wingdingsGlyphBeforeText
		);
	}

	void CCommandDialog::AddCommandButton(WORD id,LPCTSTR caption,bool defaultCommand){
		// adds a new "command-like" Button with given Id and Caption
		if (defaultCommand){
			ASSERT( defaultCommandId==0 ); // the Dialog can contain at most one DefaultCommand
			defaultCommandId=id;
		}
		AddButton(
			defaultCommand ? IDOK : id,
			caption,
			defaultCommand ? 0xf0e8 : 0xf0e0 // a thick or thin arrow right
		);
	}

	void CCommandDialog::AddHelpButton(WORD id,LPCTSTR caption){
		// adds a new "command-like" Button with given Id and Caption
		AddButton( id, caption, 0xf026 ); // a symbol with open book
	}

	#define CANCEL_COMMAND_GLYPH	0xf0e5
										// = arrow left-down

	void CCommandDialog::AddCancelButton(LPCTSTR caption){
		// adds a new "command-like" Button with given Id and Caption
		AddButton( IDCANCEL, caption, CANCEL_COMMAND_GLYPH );
	}

	void CCommandDialog::AddCheckBox(LPCTSTR caption){
		// adds a check-box with given Caption
		// - increasing the parent window size for the new check-box to fit in
		CWnd *const pCheckBox=GetDlgItem(ID_APPLY);
		CRect r,ch;
		pCheckBox->GetClientRect(&ch);
		GetWindowRect(&r);
		SetWindowPos(	nullptr,
						0,0, r.Width(), r.Height()+CMDBUTTON_MARGIN+3*ch.Height()/2,
						SWP_NOZORDER|SWP_NOMOVE
					);
		GetClientRect(&r);
		// - displaying the check-box
		const RECT t=MapDlgItemClientRect(ID_INFORMATION);
		pCheckBox->MoveWindow( t.left, r.bottom-t.top-ch.Height(), ch.Width(), ch.Height() );
		pCheckBox->SetWindowText(caption);
		pCheckBox->ShowWindow(SW_SHOW);
	}







	CByteIdentity::CByteIdentity(){
		// ctor
		for( BYTE i=0; (values[i]=i)<(BYTE)-1; i++ );
	}

	CByteIdentity::operator PCBYTE() const{
		return values;
	}








	static constexpr FILETIME None={};
	
	const CRideTime CRideTime::None(Utils::None);
	
	CRideTime::CRideTime(){
		// ctor (current local time)
		::GetLocalTime(this);
	}

	CRideTime::CRideTime(const time_t &t){
		::FileTimeToSystemTime( (const FILETIME *)&t, this );
	}

	CRideTime::CRideTime(const FILETIME &t){
		// ctor
		::FileTimeToSystemTime( &t, this );
	}

	CRideTime::operator time_t() const{
		static_assert( sizeof(FILETIME)==sizeof(time_t), "" );
		return	*(time_t *)&operator FILETIME();
	}

	CRideTime::operator FILETIME() const{
		FILETIME tmp;
		::SystemTimeToFileTime( this, &tmp );
		return tmp;
	}

	bool CRideTime::operator==(const FILETIME &t2) const{
		return	operator-(t2)==0;
	}

	bool CRideTime::operator!=(const FILETIME &t2) const{
		return	!operator==(t2);
	}

	CRideTime CRideTime::operator-(const time_t &t2) const{
		return	(time_t)*this-t2;
	}

	CRideTime CRideTime::operator-(const FILETIME &t2) const{
		static_assert( sizeof(FILETIME)==sizeof(time_t), "" );
		return	operator-( *(const time_t *)&t2 );
	}

	CRideTime CRideTime::operator-(const CRideTime &t2) const{
		return	operator-( (FILETIME)t2 );
	}

	int CRideTime::ToMilliseconds() const{
		return	(time_t)*this/10000;
	}

	WORD CRideTime::GetDosDate() const{
		WORD dosDate=0, dosTime;
		::FileTimeToDosDateTime( &(FILETIME)*this, &dosDate, &dosTime );
		return dosDate;
	}

	DWORD CRideTime::GetDosDateTime() const{
		WORD dosDate=0, dosTime=0;
		::FileTimeToDosDateTime( &(FILETIME)*this, &dosDate, &dosTime );
		return MAKELONG(dosTime,dosDate);
	}

	CString CRideTime::DateToStdString() const{
		//return CTime(*this).Format(_T("%x")); // standard date string; commented out as e.g. "June 26, 2089" fires an exception
		TCHAR buf[16];
		static constexpr LPCTSTR MonthAbbreviations[]={ _T("Jan"), _T("Feb"), _T("Mar"), _T("Apr"), _T("May"), _T("Jun"), _T("Jul"), _T("Aug"), _T("Sep"), _T("Oct"), _T("Nov"), _T("Dec") };
		::wsprintf( buf, _T("%d/%s/%d"), wDay, MonthAbbreviations[wMonth-1], wYear );
		return buf;
	}

	CString CRideTime::TimeToStdString() const{
		//return CTime(*this).Format(_T("%X")); // standard time string; commented out as e.g. "June 26, 2089" fires an exception
		TCHAR buf[16];
		::wsprintf( buf, _T("%d:%02d:%02d"), wHour, wMinute, wSecond );
		return buf;
	}

	CRideTime CRideTime::ToTzSpecificLocalTime() const{
		// factors into the date/time the local timezone and returns the result
		CRideTime result;
		if (::SystemTimeToTzSpecificLocalTime( nullptr, this, &result ))
			return result;
		else
			return *this;
	}

	bool CRideTime::Edit(bool dateEditingEnabled,bool timeEditingEnabled,const SYSTEMTIME *epoch){
		// True <=> user confirmed the shown editation dialog and accepted the new value, otherwise False
		// - defining the Dialog
		class CDateTimeDialog sealed:public CRideDialog{
			void DoDataExchange(CDataExchange *pDX) override{
				// exchange of data from and to controls
				if (pDX->m_bSaveAndValidate){
					// saving the date and time combined from values of both controls together, impossible to do using DDX_* functions
					SYSTEMTIME tmp;
					SendDlgItemMessage( ID_DATE, MCM_GETCURSEL, 0, (LPARAM)&st );
					SendDlgItemMessage( ID_TIME, DTM_GETSYSTEMTIME, 0, (LPARAM)&tmp );
					st.wHour=tmp.wHour, st.wMinute=tmp.wMinute, st.wSecond=tmp.wSecond, st.wMilliseconds=tmp.wMilliseconds;
				}else{
					// loading the date and time values
					// . adjusting interactivity
					EnableDlgItem( ID_DATE, dateEditingEnabled );
					EnableDlgItem( ID_TIME, timeEditingEnabled );
					// . restricting the Date control to specified Epoch only
					SendDlgItemMessage( ID_DATE, MCM_SETRANGE, GDTR_MIN|GDTR_MAX, (LPARAM)epoch );
					// . loading
					SendDlgItemMessage( ID_DATE, MCM_SETCURSEL, 0, (LPARAM)&st );
					SendDlgItemMessage( ID_TIME, DTM_SETSYSTEMTIME, 0, (LPARAM)&st );
				}
			}
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				if (msg==WM_NOTIFY){
					const PNMLINK pnml=(PNMLINK)lParam;
					if (pnml->hdr.code==NM_CLICK || pnml->hdr.code==NM_RETURN)
						switch (pnml->hdr.idFrom){
							case ID_AUTO:{
								// notification regarding the "Select current {date,time}" option
								::GetLocalTime(&st);
								pnml->item.iLink++;
								if (pnml->item.iLink&1)
									SendDlgItemMessage( ID_DATE, MCM_SETCURSEL, 0, (LPARAM)&st );
								if (pnml->item.iLink&2)
									SendDlgItemMessage( ID_TIME, DTM_SETSYSTEMTIME, 0, (LPARAM)&st );
								return 0;
							}
							case ID_REMOVE:
								// notification regarding the "Remove from FAT" option
								EndDialog(ID_REMOVE);
								return 0;
						}
				}
				return __super::WindowProc(msg,wParam,lParam);
			}
		public:
			const bool dateEditingEnabled, timeEditingEnabled;
			const SYSTEMTIME *const epoch;
			SYSTEMTIME st;

			CDateTimeDialog(const SYSTEMTIME &st,bool dateEditingEnabled,bool timeEditingEnabled,const SYSTEMTIME *epoch)
				// ctor
				: Utils::CRideDialog(IDR_DOS_DATETIME_EDIT)
				, dateEditingEnabled(dateEditingEnabled) , timeEditingEnabled(timeEditingEnabled) , epoch(epoch)
				, st(st) {
			}
		} d( *this, dateEditingEnabled, timeEditingEnabled, epoch );
		// - showing the Dialog and processing its result
		switch (d.DoModal()){
			case ID_REMOVE:
				d.st=None;
				//fallthrough
			case IDOK:
				*static_cast<SYSTEMTIME *>(this)=d.st;
				return true;
			default:
				return false;
		}
	}










	CAxis::CDcState::CDcState(HDC dc,int nUnitsStart)
		// ctor
		: mappingMode( ::GetMapMode(dc) )
		, graphicsMode( ::GetGraphicsMode(dc) )
		, nUnitsStart(nUnitsStart) {
		::GetWorldTransform( dc, &advanced );
		::GetViewportOrgEx( dc, &ptViewportOrg );
	}

	int CAxis::CDcState::ApplyTo(HDC dc) const{
		// changes the state of the specified DeviceContext, returning the current back-up identifier for further restoration
		const int iSavedDc=::SaveDC(dc);
		::SetMapMode( dc, mappingMode );
		::SetGraphicsMode( dc, graphicsMode );
		::SetWorldTransform( dc, &advanced );
		::SetViewportOrgEx( dc, LogicalUnitScaleFactor*nUnitsStart+ptViewportOrg.x, ptViewportOrg.y, nullptr );
		return iSavedDc;
	}

	void CAxis::CDcState::RevertFrom(HDC dc,int iSavedDc) const{
		// reverts changes previously applied to the specified DeviceContext
		::RestoreDC( dc, iSavedDc );
	}




	const TCHAR CAxis::NoPrefixes[12]={};
	const TCHAR CAxis::CountPrefixes[]=_T("   kkkMMMBBB"); // no-prefix, thousand, million, billion
	const CRideFont CAxis::FontWingdings( FONT_WINGDINGS, 120 );

	CAxis::CAxis(TLogValue logLength,TLogTime logValuePerUnit,TCHAR unit,LPCTSTR unitPrefixes,BYTE initZoomFactor,TVerticalAlign ticksAndLabelsAlign)
		// ctor
		: logLength(logLength+1) , logValuePerUnit(logValuePerUnit)
		, logCursorPos(-1) // cursor indicator hidden
		, ticksAndLabelsAlign(ticksAndLabelsAlign)
		, unit(unit) , unitPrefixes(unitPrefixes)
		, zoomFactor(initZoomFactor) {
		ASSERT( unitPrefixes!=nullptr ); // use NoPrefixes instead of Nullptr
	}

	BYTE CAxis::Draw(HDC dc,long nVisiblePixels,const CRideFont &font,int primaryGridLength,HPEN hPrimaryGridPen,PLogTime pOutVisibleStart,PLogTime pOutVisibleEnd){
		// draws an Axis starting at current origin; returns index into the UnitPrefixes indicating which prefix was used to draw the Axis
		// - determinining the primary granuality of the Axis
		TCHAR label[32];
		TLogValue intervalBig=1, iUnitPrefix=0;
		for( TLogValue v=logLength; intervalBig<logLength; intervalBig*=10 ){
			::wsprintf( label, _T("%d %c%c"), v, unitPrefixes[iUnitPrefix], unit );
			if (font.GetTextSize(label).cx<LogicalUnitScaleFactor*GetUnitCount(intervalBig))
				// the consecutive Labels won't overlap - adopting it
				break;
			else if (++iUnitPrefix%3==0)
				v/=1000;
		}
		// - determining the visible range to draw
		POINT org;
		::GetViewportOrgEx( dc, &org );
		const TLogValue valueA=std::max( PixelToValue(-org.x), 0 )/intervalBig*intervalBig;
		if (pOutVisibleStart!=nullptr)
			*pOutVisibleStart=valueA;
		const TLogValue valueZ=std::min<LONGLONG>( logLength, ((LONGLONG)PixelToValue(std::max(-org.x,0L)+nVisiblePixels)+intervalBig-1)/intervalBig*intervalBig ); // rounding to whole multiples of IntervalBig
		if (pOutVisibleEnd!=nullptr)
			*pOutVisibleEnd=valueZ;
		// - saving the current state of DC for any subsequent drawing to match the Axis (e.g. cursor indicator)
		const int nUnitsA=GetUnitCount(valueA);
		dcState=CDcState( dc, nUnitsA );
		logCursorPos=-1; // cursor indicator hidden
		// - drawing using a workaround to overcome the coordinate space limits
		const auto dcSettings0=::SaveDC(dc);
			::SetViewportOrgEx( dc, LogicalUnitScaleFactor*nUnitsA+org.x, org.y, nullptr );
			::SelectObject( dc, font );
			short smallMarkLength=0, bigMarkLength=0, labelY=0;
			switch (ticksAndLabelsAlign){
				case TVerticalAlign::TOP:
					smallMarkLength=-4, bigMarkLength=-7, labelY=bigMarkLength-font.charHeight;
					break;
				case TVerticalAlign::BOTTOM:
					smallMarkLength=4, bigMarkLength = labelY = 7;
					break;
			}
			// . horizontal line representing the timeline
			::MoveToEx( dc, 0,0, nullptr );
			::LineTo( dc, GetUnitCount(valueZ)-nUnitsA, 0 );
			// . drawing secondary time marks on the timeline
			if (smallMarkLength)
				if (const TLogValue intervalSmall=intervalBig/10)
					for( TLogValue v=valueA; v<valueZ; v+=intervalSmall ){
						const auto x=GetUnitCount(v)-nUnitsA;
						::MoveToEx( dc, x,0, nullptr );
						::LineTo( dc, x,smallMarkLength );
					}
			// . drawing primary time marks on the timeline along with respective times
			if (bigMarkLength){
				int k=1;
				for( int i=iUnitPrefix/3; i--; k*=1000 );
				for( TLogValue v=valueA; v<=valueZ; v+=intervalBig ){
					const auto x=GetUnitCount(v)-nUnitsA;
					if (primaryGridLength && v>valueA){ // it's undesired to draw a grid at ValueA, e.g. when drawing two orthogonal Axes to divide a plane (one overdraws the other)
						const HGDIOBJ hPen0=::SelectObject( dc, hPrimaryGridPen );
							::MoveToEx( dc, x,primaryGridLength, nullptr );
							::LineTo( dc, x,0 );
						::SelectObject(dc,hPen0);
					}else
						::MoveToEx( dc, x,0, nullptr );
					::LineTo( dc, x,bigMarkLength );
					::wsprintf( label, _T("%d %c%c"), v/k, unitPrefixes[iUnitPrefix], unit );
					::TextOut(
						dc,
						x, labelY,
						label, ::lstrlen(label)
					);
				}
			}
		::RestoreDC(dc,dcSettings0);
		return iUnitPrefix;
	}

	TLogValue CAxis::PixelToValue(int pixel) const{
		return	GetValue( pixel/LogicalUnitScaleFactor );
	}

	int CAxis::GetUnitCount(TLogValue logValue,BYTE zoomFactor) const{
		return	logValue/logValuePerUnit>>zoomFactor;
	}

	int CAxis::GetUnitCount(TLogValue logValue) const{
		return	GetUnitCount( logValue, zoomFactor );
	}

	int CAxis::GetUnitCount() const{
		return	GetUnitCount( logLength );
	}

	TLogValue CAxis::GetValue(int nUnits) const{
		const auto tmp=((LONGLONG)nUnits<<zoomFactor)*logValuePerUnit;
		return	tmp<INT_MAX ? tmp : INT_MAX;
	}

	void CAxis::SetLength(TLogValue newLogLength){
		logCursorPos=std::min( logCursorPos, logLength=newLogLength+1 );
	}

	BYTE CAxis::GetZoomFactorToFitWidth(int nUnits,BYTE zoomFactorMax) const{
		return	GetZoomFactorToFitWidth( logLength, nUnits, zoomFactorMax );
	}

	BYTE CAxis::GetZoomFactorToFitWidth(TLogValue logValue,int nUnits,BYTE zoomFactorMax) const{
		BYTE zf=0;
		while (GetUnitCount(logValue,zf)>nUnits && zf<zoomFactorMax)
			zf++;
		return zf;
	}

	void CAxis::SetZoomFactor(BYTE newZoomFactor){
		zoomFactor=newZoomFactor;
	}

	static void drawCursorAt(HDC dc,int nUnitsCenter,bool vaTop){
		const WCHAR glyph=L'\xf0f1'+vaTop;
		RECT rc={ nUnitsCenter-80, -80*vaTop, nUnitsCenter+80, 80*!vaTop };
		::BeginPath(dc);
			::DrawTextW( dc, &glyph,1, &rc, DT_SINGLELINE|DT_CENTER|DT_NOPREFIX|DT_NOCLIP|(DT_BOTTOM*vaTop) );
		::EndPath(dc);
		::StrokePath(dc);
	}

	void CAxis::SetCursorPos(HDC dc,TLogValue newLogPos){
		// sets logical position of the cursor indicator
		if (ticksAndLabelsAlign==TVerticalAlign::NONE)
			return;
		const int iSavedDc=dcState.ApplyTo( dc );
			const HGDIOBJ hFont0=::SelectObject( dc, FontWingdings );
				::SetROP2( dc, R2_NOT );
				::SetBkMode( dc, TRANSPARENT );
				// . erasing previously drawn cursor, if any
				if (logCursorPos>=0)
					drawCursorAt( dc, GetUnitCount(logCursorPos), ticksAndLabelsAlign==TVerticalAlign::TOP );
				// . drawing cursor at new position
				if (0<=newLogPos && newLogPos<logLength)
					drawCursorAt( dc, GetUnitCount(  logCursorPos=newLogPos  ), ticksAndLabelsAlign==TVerticalAlign::TOP );
				else
					logCursorPos=-1;
			::SelectObject(dc,hFont0);
		dcState.RevertFrom( dc, iSavedDc );
	}

	int CAxis::ValueToReadableString(TLogValue logValue,PTCHAR buffer) const{
		// converts specified Value to an easy-to-read string, returning its length
		BYTE unitPrefix=0;
		div_t d={ logValue, 0 };
		while (d.quot>=1000)
			d=div(d.quot,1000), unitPrefix+=3;
		int nChars=::wsprintf( buffer, _T("%d.%03d"), d.quot, d.rem );
		while (buffer[nChars-1]=='0') // removing trail zeroes
			nChars--;
		nChars-=buffer[nChars-1]=='.'; // removing trail floating point
		return	nChars+::wsprintf( buffer+nChars, _T(" %c%c"), unitPrefixes[unitPrefix], unit );
	}

	CString CAxis::ValueToReadableString(TLogValue logValue) const{
		// converts specified Value to an easy-to-read string
		TCHAR buffer[80];
		ValueToReadableString( logValue, buffer );
		return buffer;
	}










	CTimeline::CTimeline(TLogTime logTimeLength,TLogTime logTimePerUnit,BYTE initZoomFactor)
		// ctor
		: CAxis( logTimeLength, logTimePerUnit, 's', TimePrefixes, initZoomFactor ) {
	}

	const TCHAR CTimeline::TimePrefixes[]=_T("nnnµµµmmm   "); // nano, micro, milli, no-prefix

	void CTimeline::Draw(HDC dc,const CRideFont &font,PLogTime pOutVisibleStart,PLogTime pOutVisibleEnd){
		// draws a HORIZONTAL Timeline starting at current origin
		CRect rcClient;
		::GetClientRect( ::WindowFromDC(dc), &rcClient );
		__super::Draw( dc, rcClient.Width(), font, 0, nullptr, pOutVisibleStart, pOutVisibleEnd );
	}










	CBigEndianWord::CBigEndianWord(WORD initLittleEndianValue)
		// ctor
		: highByte(HIBYTE(initLittleEndianValue)) , lowByte(LOBYTE(initLittleEndianValue)) {
	}

	WORD CBigEndianWord::operator=(WORD newValue){
		// "setter"
		highByte=HIBYTE(newValue), lowByte=LOBYTE(newValue);
		return newValue;
	}

	CBigEndianWord::operator WORD() const{
		// "getter"
		return MAKEWORD(lowByte,highByte);
	}








	DWORD CBigEndianDWord::operator=(DWORD newValue){
		// "setter"
		highWord=HIWORD(newValue), lowWord=LOWORD(newValue);
		return newValue;
	}

	CBigEndianDWord::operator DWORD() const{
		// "getter"
		return MAKELONG(lowWord,highWord);
	}










	TStdWinError ErrorByOs(TStdWinError vistaOrNewer,TStdWinError xpOrOlder){
		// returns the error code by observing the current operating system version; it's up to the caller to know whether specified error is supported by the OS
		return	(::GetVersion()&0xff)<=5 // Windows XP or older
				? xpOrOlder
				: vistaOrNewer;
	}

	#define ERROR_BUFFER_SIZE	220

	PTCHAR FormatErrorCode(PTCHAR buf,TStdWinError errCode){
		// generates into Buffer a message corresponding to the ErrorCode; assumed that the Buffer is at least ERROR_BUFFER_SIZE characters big
		WCHAR msg[ERROR_BUFFER_SIZE];
		if (errCode<=12000 || errCode>USHRT_MAX)
			// "standard" or COM (HRESULT) error
			::FormatMessageW(
				FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errCode, 0,
				msg, ERROR_BUFFER_SIZE-20,
				nullptr
			);
		else
			// WinInet error
			if (errCode!=ERROR_INTERNET_EXTENDED_ERROR)
				// "standard" WinInet error message
				::FormatMessageW(
					FORMAT_MESSAGE_FROM_HMODULE, ::GetModuleHandle(DLL_WININET), errCode, 0,
					msg, ERROR_BUFFER_SIZE-20,
					nullptr
				);
			else{
				// detailed error message from the server
				DWORD tmp, bufLength=ERROR_BUFFER_SIZE-20;
				::InternetGetLastResponseInfoW( &tmp, msg, &bufLength );
			}
		#ifdef UNICODE
			static_assert( false, "Unicode support not implemented" );
		#else
			::wsprintf(
				buf+::WideCharToMultiByte( CP_UTF8, 0, msg,-1, buf,ERROR_BUFFER_SIZE-20, nullptr, nullptr ),
				_T("(Error 0x%X)"), errCode
			);
		#endif
		return buf;
	}



	void FatalError(LPCTSTR text){
		// shows fatal error
		const HWND hParent=app.GetEnabledActiveWindow();
		CBackgroundActionCancelable::SignalPausedProgress( hParent );
		::MessageBoxW( hParent, ToStringW(text), nullptr, MB_ICONERROR|MB_TASKMODAL );
	}

	CString SimpleFormat(LPCTSTR format,va_list v){
		CString result;
		result.FormatV( format, v );
		return result;
	}

	CString SimpleFormat(LPCTSTR format,LPCTSTR param){
		CString result;
		result.Format( format, param );
		return result;
	}

	CString SimpleFormat(LPCTSTR format,LPCTSTR param1,LPCTSTR param2){
		CString result;
		result.Format( format, param1, param2 );
		return result;
	}

	CString SimpleFormat(LPCTSTR format,int param1,LPCTSTR param2){
		CString result;
		result.Format( format, param1, param2 );
		return result;
	}

	CString SimpleFormat(LPCTSTR format,int param1,int param2,LPCTSTR param3){
		CString result;
		result.Format( format, param1, param2, param3 );
		return result;
	}

	#define ERROR_BECAUSE		_T("%s because:\n\n%s")
	#define ERROR_CONSEQUENCE	_T("\n\n\n%s")

	CString ComposeErrorMessage(LPCTSTR text,LPCTSTR causeOfError,LPCTSTR consequence){
		// compiles a message explaining the situation caused by the Error, and appends immediate Consequence it implies
		TCHAR buf[2000];
		const int n=::wsprintf( buf, ERROR_BECAUSE, text, causeOfError );
		if (consequence)
			::wsprintf( buf+n, ERROR_CONSEQUENCE, consequence );
		return buf;
	}

	CString ComposeErrorMessage(LPCTSTR text,TStdWinError causeOfError,LPCTSTR consequence){
		// compiles a message explaining the situation caused by the Error, and appends immediate Consequence it implies
		TCHAR buf[ERROR_BUFFER_SIZE];
		return ComposeErrorMessage( text, FormatErrorCode(buf,causeOfError), consequence );
	}

	void FatalError(LPCTSTR text,LPCTSTR causeOfError,LPCTSTR consequence){
		// shows fatal error along with its Cause and immediate Consequence
		FatalError( ComposeErrorMessage(text,causeOfError,consequence) );
	}
	void FatalError(LPCTSTR text,TStdWinError causeOfError,LPCTSTR consequence){
		// shows fatal error along with its Cause and immediate Consequence
		FatalError( ComposeErrorMessage(text,causeOfError,consequence) );
	}




	void Information(LPCTSTR text){
		// shows Textual information
		const HWND hParent=app.GetEnabledActiveWindow();
		CBackgroundActionCancelable::SignalPausedProgress( hParent );
		::MessageBoxW( hParent, ToStringW(text), L"Information", MB_ICONINFORMATION|MB_TASKMODAL );
	}
	void Information(LPCTSTR text,LPCTSTR causeOfError,LPCTSTR consequence){
		// shows Textual information along with its Cause and immediate Consequence
		Information( ComposeErrorMessage(text,causeOfError,consequence) );
	}
	void Information(LPCTSTR text,TStdWinError causeOfError,LPCTSTR consequence){
		// shows Textual information along with its Cause and immediate Consequence
		Information( ComposeErrorMessage(text,causeOfError,consequence) );
	}




	#define CHECKBOX_MARGIN	10

	static HHOOK hMsgBoxHook;
	static HWND hMsgBox;
	static DWORD checkBoxChecked;
	static LPCTSTR checkBoxMessage;

	static LRESULT CALLBACK __addCheckBox_hook__(int msg,WPARAM wParam,LPARAM lParam){
		// hooking the MessageBox
		static HWND hCheckBox;
		switch (msg){
			case HCBT_ACTIVATE:{
				// the window is about to be activated
				if (!hMsgBox){
					// the window to be activated is a fresh created MessageBox
					hMsgBox=(HWND)wParam;
					// . determining the size of the CheckBox with specified text
					SIZE checkBoxSize;
					const HDC dc=::GetDC(hMsgBox);
						::GetTextExtentPoint32( dc, checkBoxMessage, ::lstrlen(checkBoxMessage), &checkBoxSize );
					::ReleaseDC(hMsgBox,dc);
					checkBoxSize.cx+=16+CHECKBOX_MARGIN; // 16 = the size of the "checked" icon
					// . adjusting the size of the MessageBox dialog to accommodate the CheckBox
					CRect r;
					::GetWindowRect( hMsgBox, &r );
					::SetWindowPos(	hMsgBox, 0,
									0,0,
									std::max<int>( r.Width(), checkBoxSize.cx+2*CHECKBOX_MARGIN ),  r.Height()+CHECKBOX_MARGIN+checkBoxSize.cy,
									SWP_NOZORDER | SWP_NOMOVE
								);
					// . creating the CheckBox
					::GetClientRect( hMsgBox, &r );
					hCheckBox=::CreateWindow(	WC_BUTTON, checkBoxMessage,
												WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | checkBoxChecked,
												CHECKBOX_MARGIN,  r.Height() - LogicalUnitScaleFactor*( ::GetSystemMetrics(SM_CYCAPTION) - ::GetSystemMetrics(SM_CYBORDER) ),
												checkBoxSize.cx, checkBoxSize.cy,
												hMsgBox, 0, AfxGetInstanceHandle(), nullptr
											);
				}
				break;
			}
			case HCBT_DESTROYWND:
				// a window is about to be destroyed
				if ((HWND)wParam==hMsgBox){
					// the window to be destroyed is the MessageBox
					checkBoxChecked=Button_GetCheck(hCheckBox);
					::DestroyWindow(hCheckBox);
					hCheckBox = hMsgBox = 0;
				}
				break;
		}
		return ::CallNextHookEx(0,msg,wParam,lParam);
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
		const HWND hParent=app.GetEnabledActiveWindow();
		CBackgroundActionCancelable::SignalPausedProgress( hParent );
		LOG_DIALOG_DISPLAY(text);
		return LOG_DIALOG_RESULT(  ::MessageBoxW( hParent, ToStringW(text), L"Information", MB_ICONINFORMATION|MB_OKCANCEL|MB_TASKMODAL )==IDOK  );
	}





	bool QuestionYesNo(LPCTSTR text,UINT defaultButton){
		// shows a yes-no question
		const HWND hParent=app.GetEnabledActiveWindow();
		CBackgroundActionCancelable::SignalPausedProgress( hParent );
		LOG_DIALOG_DISPLAY(text);
		return LOG_DIALOG_RESULT(  ::MessageBoxW( hParent, ToStringW(text), L"Question", MB_ICONQUESTION|MB_TASKMODAL|MB_YESNO|defaultButton )==IDYES  );
	}




	BYTE QuestionYesNoCancel(LPCTSTR text,UINT defaultButton){
		// shows a yes-no question
		const HWND hParent=app.GetEnabledActiveWindow();
		CBackgroundActionCancelable::SignalPausedProgress( hParent );
		LOG_DIALOG_DISPLAY(text);
		return LOG_DIALOG_RESULT(  ::MessageBoxW( hParent, ToStringW(text), L"Question", MB_ICONQUESTION|MB_TASKMODAL|MB_YESNOCANCEL|defaultButton )  );
	}
	BYTE QuestionYesNoCancel(LPCTSTR text,UINT defaultButton,LPCTSTR causeOfError,LPCTSTR consequence){
		// shows a yes-no question along with its Cause and immediate Consequence
		return QuestionYesNoCancel( ComposeErrorMessage(text,causeOfError,consequence), defaultButton );
	}
	BYTE QuestionYesNoCancel(LPCTSTR text,UINT defaultButton,TStdWinError causeOfError,LPCTSTR consequence){
		// shows a yes-no question along with its Cause and immediate Consequence
		return QuestionYesNoCancel( ComposeErrorMessage(text,causeOfError,consequence), defaultButton );
	}




	BYTE AbortRetryIgnore(LPCTSTR text,UINT defaultButton){
		// shows an abort-retry-ignore question
		const HWND hParent=app.GetEnabledActiveWindow();
		CBackgroundActionCancelable::SignalPausedProgress( hParent );
		LOG_DIALOG_DISPLAY(text);
		return LOG_DIALOG_RESULT(  ::MessageBoxW( hParent, ToStringW(text), L"Question", MB_ICONQUESTION|MB_TASKMODAL|MB_ABORTRETRYIGNORE|defaultButton )  );
	}

	BYTE AbortRetryIgnore(LPCTSTR text,TStdWinError causeOfError,UINT defaultButton,LPCTSTR consequence){
		// shows an abort-retry-ignore question along with its Cause
		return AbortRetryIgnore( ComposeErrorMessage(text,causeOfError,consequence), defaultButton );
	}

	BYTE AbortRetryIgnore(TStdWinError causeOfError,UINT defaultButton){
		// shows an abort-retry-ignore question
		TCHAR bufCause[ERROR_BUFFER_SIZE];
		return AbortRetryIgnore( FormatErrorCode(bufCause,causeOfError), defaultButton );
	}

	bool RetryCancel(LPCTSTR text){
		// shows an retry-cancel question
		const HWND hParent=app.GetEnabledActiveWindow();
		CBackgroundActionCancelable::SignalPausedProgress( hParent );
		LOG_DIALOG_DISPLAY(text);
		return LOG_DIALOG_RESULT(  ::MessageBoxW( hParent, ToStringW(text), L"Question", MB_ICONEXCLAMATION|MB_TASKMODAL|MB_RETRYCANCEL|MB_DEFBUTTON1 )==IDRETRY  );
	}
	bool RetryCancel(TStdWinError causeOfError){
		// shows an retry-cancel question
		TCHAR bufCause[ERROR_BUFFER_SIZE];
		return RetryCancel( FormatErrorCode(bufCause,causeOfError) );
	}

	BYTE CancelRetryContinue(LPCTSTR text,UINT defaultButton){
		// shows an cancel-retry-continue question
		const HWND hParent=app.GetEnabledActiveWindow();
		CBackgroundActionCancelable::SignalPausedProgress( hParent );
		LOG_DIALOG_DISPLAY(text);
		return LOG_DIALOG_RESULT(  ::MessageBoxW( hParent, ToStringW(text), L"Question", MB_ICONEXCLAMATION|MB_TASKMODAL|MB_CANCELTRYCONTINUE|defaultButton )  );
	}
	BYTE CancelRetryContinue(LPCTSTR text,TStdWinError causeOfError,UINT defaultButton,LPCTSTR consequence){
		// shows an cancel-retry-continue question along with its Cause
		return CancelRetryContinue( ComposeErrorMessage(text,causeOfError,consequence), defaultButton );
	}



	void Warning(LPCTSTR text){
		// shows Textual warning
		const HWND hParent=app.GetEnabledActiveWindow();
		CBackgroundActionCancelable::SignalPausedProgress( hParent );
		::MessageBoxW( hParent, ToStringW(text), L"Warning", MB_ICONINFORMATION|MB_TASKMODAL );
	}











	CRideDialog::CRideDialog(){
		// ctor
	}

	CRideDialog::CRideDialog(LPCTSTR lpszTemplateName,const CWnd *pParentWnd)
		// ctor
		: CDialog( lpszTemplateName, pParentWnd?CWnd::FromHandle(pParentWnd->m_hWnd):nullptr ) {
	}

	CRideDialog::CRideDialog(UINT nIDTemplate,const CWnd *pParentWnd)
		// ctor
		: CDialog( nIDTemplate, pParentWnd?CWnd::FromHandle(pParentWnd->m_hWnd):nullptr ) {
	}

	INT_PTR CRideDialog::DoModal(){
		// modal processing
		INT_PTR result;
		app.m_pMainWnd->BeginModalState(); // block any interaction with the MainWindow
			if (app.m_pActiveWnd) app.m_pActiveWnd->BeginModalState(); // block any interaction with previously active window
				CBackgroundActionCancelable::SignalPausedProgress( *this );
		{		const CVarBackup<CWnd *> pActiveWindowOrg( app.m_pActiveWnd );
				result=__super::DoModal(); // sets pActiveWindowOrg via PreInitDialog
		}	if (app.m_pActiveWnd) app.m_pActiveWnd->EndModalState();
		app.m_pMainWnd->EndModalState();
		return result;
	}

	HWND CRideDialog::GetDlgItemHwnd(WORD id) const{
		// determines and returns the handle of the item specified by its Id
		return ::GetDlgItem( m_hWnd, id );
	}

	int CRideDialog::GetDlgItemTextLength(WORD id) const{
		// returns the length of text in a text-box with specified Id
		return ::GetWindowTextLength( GetDlgItemHwnd(id) );
	}

	bool CRideDialog::IsDlgItemShown(WORD id) const{
		// True <=> control with specified Id is shown, otherwise False
		return ::IsWindowVisible( GetDlgItemHwnd(id) )!=FALSE;
	}

	bool CRideDialog::CheckDlgItem(WORD id,bool checked) const{
		// checks/unchecks the specified two-state Dialog control and returns this new state
		::CheckDlgButton( m_hWnd, id, checked );
		return checked;
	}

	bool CRideDialog::IsDlgItemChecked(WORD id) const{
		// True <=> the specified Dialog control is checked, otherwise False
		return ::IsDlgButtonChecked( m_hWnd, id )==BST_CHECKED;
	}

	bool CRideDialog::EnableDlgItem(WORD id,bool enabled) const{
		// enables/disables the specified Dialog control and returns this new state
		::EnableWindow( ::GetDlgItem(m_hWnd,id), enabled );
		return enabled;
	}

	bool CRideDialog::EnableDlgItems(PCWORD pIds,bool enabled) const{
		// enables/disables all specified Dialog controls and returns this new state
		while (const WORD id=*pIds++)
			::EnableWindow( ::GetDlgItem(m_hWnd,id), enabled );
		return enabled;
	}

	void CRideDialog::CheckAndEnableDlgItem(WORD id,bool check,bool enable) const{
		// checking and enabling of a control at once
		CheckDlgItem( id, check );
		EnableDlgItem( id, enable );
	}

	void CRideDialog::CheckAndEnableDlgItem(WORD id,bool checkAndEnable) const{
		// checking and enabling of a control at once
		CheckAndEnableDlgItem( id, checkAndEnable, checkAndEnable );
	}

	bool CRideDialog::ShowDlgItem(WORD id,bool show) const{
		// shows/hides the specified Dialog control and returns this new state
		::ShowWindow( ::GetDlgItem(m_hWnd,id), show?SW_SHOW:SW_HIDE );
		return show;
	}

	bool CRideDialog::ShowDlgItems(PCWORD pIds,bool show) const{
		// shows/hides all specified Dialog controls and returns this new state
		while (const WORD id=*pIds++)
			::ShowWindow( ::GetDlgItem(m_hWnd,id), show?SW_SHOW:SW_HIDE );
		return show;
	}

	void CRideDialog::FocusDlgItem(WORD id) const{
		// sets keyboard focus on item specified by its Id
		::SetFocus( ::GetDlgItem(m_hWnd,id) );
	}

	bool CRideDialog::IsDlgItemEnabled(WORD id) const{
		// True <=> the specified Dialog control is enabled, otherwise False
		return ::IsWindowEnabled( ::GetDlgItem(m_hWnd,id) )!=FALSE;
	}

	RECT CRideDialog::GetDlgItemClientRect(WORD id) const{
		// determines the client area of specified Dialog control
		RECT tmp;
		::GetClientRect( ::GetDlgItem(m_hWnd,id), &tmp );
		return tmp;
	}

	RECT CRideDialog::MapDlgItemClientRect(WORD id) const{
		// returns coordinates of the client area of the specified item
		return MapDlgItemClientRect( ::GetDlgItem(m_hWnd,id) );
	}

	RECT CRideDialog::MapDlgItemClientRect(HWND hItem) const{
		// returns coordinates of the client area of the specified item
		RECT tmp;
		::GetClientRect( hItem, &tmp );
		::MapWindowPoints( hItem, m_hWnd, (LPPOINT)&tmp, 2 );
		return tmp;
	}

	POINT CRideDialog::MapDlgItemClientOrigin(WORD id) const{
		//
		POINT tmp={};
		::MapWindowPoints( ::GetDlgItem(m_hWnd,id), m_hWnd, &tmp, 1 );
		return tmp;
	}

	void CRideDialog::OffsetDlgItem(WORD id,int dx,int dy) const{
		// changes Dialog control position by [dx,dy]
		const HWND hCtrl=::GetDlgItem(m_hWnd,id);
		POINT pt={};
		::MapWindowPoints( hCtrl, m_hWnd, &pt, 1 );
		::SetWindowPos( hCtrl, 0, pt.x+dx, pt.y+dy, 0, 0, SWP_NOZORDER|SWP_NOSIZE );
	}

	void CRideDialog::SetDlgItemPos(HWND itemHwnd,int x,int y,int cx,int cy) const{
		// changes Dialog control position and size
		::SetWindowPos(
			itemHwnd,
			0,
			x, y,
			cx, cy,
			SWP_NOZORDER | (x|y?0:SWP_NOMOVE) | (cx|cy?0:SWP_NOSIZE)
		);
	}

	void CRideDialog::SetDlgItemPos(HWND itemHwnd,const RECT &rc) const{
		// changes Dialog control position and size
		SetDlgItemPos( itemHwnd, rc.left,rc.top, rc.right-rc.left,rc.bottom-rc.top );
	}

	void CRideDialog::SetDlgItemPos(WORD id,int x,int y,int cx,int cy) const{
		// changes Dialog control position and size
		SetDlgItemPos( ::GetDlgItem(m_hWnd,id), x, y, cx, cy );
	}

	void CRideDialog::SetDlgItemPos(WORD id,const RECT &rc) const{
		// changes Dialog control position and size
		SetDlgItemPos( id, rc.left,rc.top, rc.right-rc.left,rc.bottom-rc.top );
	}

	void CRideDialog::SetDlgItemSize(WORD id,int cx,int cy) const{
		// changes Dialog control size
		SetDlgItemPos( id, 0,0, cx,cy );
	}

	void CRideDialog::InvalidateDlgItem(WORD id) const{
		// invalidates the Dialog control
		InvalidateDlgItem( ::GetDlgItem(m_hWnd,id) );
	}

	void CRideDialog::InvalidateDlgItem(HWND hItem) const{
		// invalidates the Dialog control
		// - invalidating the Item
		::InvalidateRect( hItem, nullptr, TRUE );
		// - invalidating also the Dialog under the Item (e.g. static controls; see https://stackoverflow.com/questions/1823883 )
		::RedrawWindow( m_hWnd, &MapDlgItemClientRect(hItem), nullptr, RDW_ERASE|RDW_INVALIDATE );
	}

	LONG_PTR CRideDialog::GetDlgComboBoxSelectedValue(WORD id) const{
		// returns the Value selected in specified ComboBox
		CComboBox cb;
		cb.Attach( GetDlgItemHwnd(id) );
			const auto value=cb.GetItemData( cb.GetCurSel() );
		cb.Detach();
		return value;
	}

	bool CRideDialog::SelectDlgComboBoxValue(WORD id,LONG_PTR value,bool cancelPrevSelection) const{
		// True <=> specified Value found in ComboBox's value list, otherwise False
		CComboBox cb;
		cb.Attach( GetDlgItemHwnd(id) );
			if (cancelPrevSelection)
				cb.SetCurSel(-1); // cancelling previous selection
			bool valueFound=false; // assumption
			for( BYTE n=cb.GetCount(); n--; )
				if ( valueFound=cb.GetItemData(n)==value ){
					cb.SetCurSel(n);
					break;
				}
		cb.Detach();
		return valueFound;
	}

	int CRideDialog::GetDlgComboBoxSelectedIndex(WORD id) const{
		// returns the index of the item selected in specified ComboBox
		return ::SendDlgItemMessage( *this, id, CB_GETCURSEL, 0, 0 );
	}

	int CRideDialog::GetDlgListBoxSelectedIndex(WORD id) const{
		// returns the index of the item selected in specified ListBox
		return ::SendDlgItemMessage( *this, id, LB_GETCURSEL, 0, 0 );
	}

	class CTempDlg sealed:public CDialog{
		const HRSRC hRes;
		const HGLOBAL gRes;
		const LPCDLGTEMPLATE lpRes;
	public:
		CTempDlg(UINT idDlgRes)
			// ctor
			: hRes(
				::FindResource( app.m_hInstance, MAKEINTRESOURCE(idDlgRes), RT_DIALOG )		)
			, gRes(
				hRes!=nullptr
				? ::LoadResource( app.m_hInstance, hRes )
				: nullptr	)
			, lpRes(
				(LPCDLGTEMPLATE)::LockResource( gRes )	) {
			if (lpRes)
				CreateDlgIndirect( lpRes, nullptr, app.m_hInstance );
		}

		~CTempDlg(){
			// dtor
			if (gRes){
				::UnlockResource(gRes);
				::FreeResource(gRes);
			}
		}

		operator bool() const{
			return m_hWnd!=0;
		}
	};

	LPCTSTR CRideDialog::GetDialogTemplateCaptionText(UINT idDlgRes,PTCHAR chars,WORD nCharsMax){
		// in given Dialog resource retrieves the caption associated with that Dialog
		if (const CTempDlg d=idDlgRes){
			d.GetWindowText( chars, nCharsMax );
			return chars;
		}else
			return nullptr;
	}

	LPCTSTR CRideDialog::GetDialogTemplateItemText(UINT idDlgRes,WORD idItem,PTCHAR chars,WORD nCharsMax){
		// in given Dialog resource finds the item with specified ID and retrieves the text associated with that item
		if (const CTempDlg d=idDlgRes){
			d.GetDlgItemText( idItem, chars, nCharsMax );
			return chars;
		}else
			return nullptr;
	}

	#define BRACKET_CURLY_FONT_SIZE	12

	struct TCurlyBracket sealed{
		WCHAR CurveUpper, CurveLower, CurveMiddle;
	};

	static void DrawCurlyBracket(HDC dc,int x,int yMin,int yMax,const TCurlyBracket &cb){
		// draws a CurlyBracket at position X and wrapping all points in {yMin,...,yMax}
		const CRideFont font( FONT_SYMBOL, BRACKET_CURLY_FONT_SIZE*10, false, true );
		::SetBkMode(dc,TRANSPARENT);
		const HGDIOBJ hFont0=::SelectObject(dc,font);
			RECT r={ x, yMin, x+100, yMax };
			::DrawTextW( dc, &cb.CurveUpper,1, &r, DT_LEFT|DT_TOP|DT_SINGLELINE );
			::DrawTextW( dc, &cb.CurveLower,1, &r, DT_LEFT|DT_BOTTOM|DT_SINGLELINE );
			::DrawTextW( dc, &cb.CurveMiddle,1, &r, DT_LEFT|DT_VCENTER|DT_SINGLELINE );
			SIZE fontSize;
			::GetTextExtentPoint32W(dc,&cb.CurveMiddle,1,&fontSize);
			r.top+=fontSize.cy/5, r.bottom-=fontSize.cy/5;
			while (r.bottom-r.top>2.2*fontSize.cy){
				static constexpr WCHAR CurveStraight=0xf0ef;
				::DrawTextW( dc, &CurveStraight,1, &r, DT_LEFT|DT_TOP|DT_SINGLELINE );
				::DrawTextW( dc, &CurveStraight,1, &r, DT_LEFT|DT_BOTTOM|DT_SINGLELINE );
				r.top++, r.bottom--;
			}
		::SelectObject(dc,hFont0);
	}

	void CRideDialog::DrawOpeningCurlyBracket(HDC dc,int x,int yMin,int yMax){
		// draws a opening curly bracket at position X and wrapping all points in {yMin,...,yMax}
		static constexpr TCurlyBracket CurlyBracket={ 0xf0ec, 0xf0ee, 0xf0ed };
		DrawCurlyBracket( dc, x, yMin, yMax, CurlyBracket );
	}

	void CRideDialog::WrapDlgItemsByOpeningCurlyBracket(WORD idA,WORD idZ) const{
		// wraps ControlsA-Z from left using opening curly bracket
		const RECT rcA=MapDlgItemClientRect(idA), rcZ=MapDlgItemClientRect(idZ);
		RECT r={ std::min(rcA.left,rcZ.left)-13, rcA.top-6, 1000, rcZ.bottom+6 };
		const CClientDC dc( const_cast<CRideDialog *>(this) );
		DrawOpeningCurlyBracket( dc, r.left, r.top, r.bottom );
	}

	void CRideDialog::DrawClosingCurlyBracket(HDC dc,int x,int yMin,int yMax){
		// draws a closing curly bracket at position X and wrapping all points in {yMin,...,yMax}
		static constexpr TCurlyBracket CurlyBracket={ 0xf0fc, 0xf0fe, 0xf0fd };
		DrawCurlyBracket( dc, x, yMin, yMax, CurlyBracket );
	}

	void CRideDialog::WrapDlgItemsByClosingCurlyBracketWithText(WORD idA,WORD idZ,LPCTSTR text,DWORD textColor) const{
		// wraps ControlsA-Z from right using closing curly brackets and draws given Text in given Color
		// - drawing curly brackets
		const RECT rcA=MapDlgItemClientRect(idA), rcZ=MapDlgItemClientRect(idZ);
		RECT r={ std::max(rcA.right,rcZ.right)+5, rcA.top-6, 1000, rcZ.bottom+6 };
		CClientDC dc( const_cast<CRideDialog *>(this) );
		dc.SetTextColor( textColor );
		DrawClosingCurlyBracket( dc, r.left, r.top, r.bottom );
		// . text
		r.left+=LogicalUnitScaleFactor*14;
		const HGDIOBJ hFont0=::SelectObject( dc, GetFont()->m_hObject );
			dc.DrawText( text,-1, &r, DT_VCENTER|DT_SINGLELINE );
		::SelectObject(dc,hFont0);
	}

	CString CRideDialog::CompactPathToFitInDlgItem(WORD id,LPCTSTR fullpath) const{
		// compacts FullPath to fit in the dimensions of the given item
		CWnd *const pWnd=GetDlgItem(id);
		CRect rc;
		pWnd->GetClientRect(&rc);
		const CString result=fullpath;
		::PathCompactPath( CClientDC(pWnd), const_cast<PTCHAR>((LPCTSTR)result), rc.Width() );
		return result;
	}

	void CRideDialog::SetDlgItemCompactPath(WORD id,LPCTSTR fullpath) const{
		// sets given window's text to compacted FullPath that fits in its dimensions
		::SetDlgItemText(
			m_hWnd, id,
			CompactPathToFitInDlgItem( id, fullpath )
		);
	}

	void CRideDialog::SetDlgItemFormattedText(WORD id,LPCTSTR format,...) const{
		// sets given window's text to the text Formatted using given string and parameters; returns the number of characters set
		va_list argList;
		va_start( argList, format );
			::SetDlgItemText( m_hWnd, id, SimpleFormat(format,argList) );
		va_end(argList);
	}

	void CRideDialog::SetDlgItemSingleCharUsingFont(HWND hDlg,WORD controlId,WCHAR singleChar,HFONT hFont){
		// sets given window's text to the SingleCharacter displayed in specified Font
		const HWND hCtrl=::GetDlgItem(hDlg,controlId);
		const WCHAR buf[]={ singleChar, '\0' };
		::SetWindowTextW( hCtrl, buf );
		::SendMessage( hCtrl, WM_SETFONT, (WPARAM)hFont, 0 );
	}

	void CRideDialog::SetDlgItemSingleCharUsingFont(WORD controlId,WCHAR singleChar,HFONT hFont) const{
		// sets given window's text to the SingleCharacter displayed in specified Font
		SetDlgItemSingleCharUsingFont( m_hWnd, controlId, singleChar, hFont );
	}

	void CRideDialog::SetDlgItemSingleCharUsingFont(WORD controlId,WCHAR singleChar,LPCTSTR fontFace,int fontPointSize) const{
		// sets given window's text to the SingleCharacter displayed in specified Font
		SetDlgItemSingleCharUsingFont( controlId, singleChar, (HFONT)CRideFont(fontFace,fontPointSize,false,true).Detach() );
	}

	void CRideDialog::PopulateDlgComboBoxWithSequenceOfNumbers(WORD controlId,BYTE iStartValue,LPCTSTR strStartValueDesc,BYTE iEndValue,LPCTSTR strEndValueDesc) const{
		// fills ComboBox with integral numbers from the {Start,End} range (in ascending order)
		const HWND hComboBox=::GetDlgItem(m_hWnd,controlId);
		TCHAR buf[80];
		::wsprintf( buf, _T("%d %s"), iStartValue, strStartValueDesc );
		ComboBox_AddString( hComboBox, buf );
		while (++iStartValue<iEndValue)
			ComboBox_AddString( hComboBox, _itot(iStartValue,buf,10) );
		::wsprintf( buf, _T("%d %s"), iEndValue, strEndValueDesc );
		ComboBox_AddString( hComboBox, buf );
	}

	static constexpr TCHAR RangeSign='-'; // "minus"
	static constexpr TCHAR Delimiters[]={ ',', ';', RangeSign, '\0' }; // valid integer delimiters, INCLUDING RangeSign

	bool CRideDialog::GetDlgItemIntList(WORD id,CIntList &rOutList,const PropGrid::Integer::TUpDownLimits &limits,int nIntsMin,int nIntsMax) const{
		// True <=> item with the specified ID contains list of integer values (grammar bellow), otherwise False
		// - elimination of white spaces from the content
		TCHAR buf[16384], *pEnd=buf;
		for( int i=0,n=GetDlgItemText(id,buf,ARRAYSIZE(buf)); i<n; i++ )
			if (!::isspace(buf[i]))
				*pEnd++=buf[i];
		// - empty content is incorrect
		if (pEnd==buf)
			return false;
		// - not beginning with a digit is incorrect
		if (!::isdigit(*buf))
			return false;
		// - parsing the content and populating the List with recognized integers
		*pEnd=*Delimiters;
		struct{
			bool open;
			int begin;
		} range={};
		for( const TCHAR *p=buf; p<pEnd; p++ ){
			int i,n;
			if (!_stscanf( p, _T("%d%n"), &i, &n ))
				return false; // invalid or no number
			if (!limits.Contains(i))
				return false; // out of Limits
			p+=n;
			if (!::StrChr(Delimiters,*p))
				return false; // each integer must be terminated with one of Delimiters
			if (range.open){
				if (*p==RangeSign)
					return false; // a "range within a range" is invalid
				if (range.begin<=i) // range specified in ascending order (e.g. "2-5")
					while (++range.begin<=i)
						rOutList.AddTail(range.begin);
				else // range specified in descending order (e.g. "5-2")
					while (i<=--range.begin)
						rOutList.AddTail(range.begin);
			}else
				rOutList.AddTail(i);
			if ( range.open=*p==RangeSign )
				range.begin=i;
		}
		// - must stay within count limits
		if (rOutList.GetCount()<nIntsMin || nIntsMax<rOutList.GetCount())
			return false;
		// - the content is valid and the output List has been populated
		return true;
	}

	void CRideDialog::SetDlgItemIntList(WORD id,const CIntList &list) const{
		// populates item with the specified ID with textual representation of the integer values
		TCHAR buf[16384], *p=buf;
		for( POSITION pos=list.GetHeadPosition(); pos; ){
			int rangeBegin=list.GetNext(pos), rangeEnd=rangeBegin;
			if (pos)
				if (rangeBegin<list.GetAt(pos)) // ascending range?
					while (pos && list.GetAt(pos)==rangeEnd+1)
						rangeEnd=list.GetNext(pos);
				else if (list.GetAt(pos)<rangeBegin) // descending range?
					while (pos && list.GetAt(pos)==rangeEnd-1)
						rangeEnd=list.GetNext(pos);
			if (rangeEnd==rangeBegin) // only one isolated number
				p+=::wsprintf( p, _T("%d%c "), rangeBegin, *Delimiters );
			else if (std::abs(rangeEnd-rangeBegin)==1) // range of two numbers is better to be listed as two isolated numbers
				p+=::wsprintf( p, _T("%d%c %d%c "), rangeBegin, *Delimiters, rangeEnd, *Delimiters );
			else // a range of at least three consecutive numbers
				p+=::wsprintf( p, _T("%d%c%d%c "), rangeBegin, RangeSign, rangeEnd, *Delimiters );
		}
		p[-2]='\0';
		::SetDlgItemText( *this, id, buf );
	}












	BOOL CRideDialog::CSplitterWnd::OnCommand(WPARAM wParam,LPARAM lParam){
		// command processing
		return	CWnd::OnCommand( wParam, lParam ); // <<<<< DON'T USE "__super" as this would call code that assumes CFrameWnd is the parent of this splitter!
	}
	
	BOOL CRideDialog::CSplitterWnd::OnNotify(WPARAM wParam,LPARAM lParam,LRESULT *pResult){
		// notification processing
		return	CWnd::OnNotify( wParam, lParam, pResult ); // <<<<< DON'T USE "__super" as this would call code that assumes CFrameWnd is the parent of this splitter!
	}
	
	CWnd *CRideDialog::CSplitterWnd::GetActivePane(int *pRow,int *pCol){
		// returns the active view of the parent CFrameWnd, that exists as one of the SplitterWnd cells
		if (CWnd *const pFocusedWnd=GetFocus())
			if (IsChildPane( pFocusedWnd, pRow, pCol ))
				return pFocusedWnd;
		return nullptr;
	}

	void CRideDialog::CSplitterWnd::SetActivePane(int row,int col,CWnd *pWnd){
		// sets the focus to the specified window
		if (pWnd)
			pWnd->SetFocus();
		else
			GetPane(row,col)->SetFocus();
	}












	CSingleNumberDialog::CSingleNumberDialog(LPCTSTR caption,LPCTSTR label,const PropGrid::Integer::TUpDownLimits &range,int initValue,bool hexa,CWnd *pParent)
		// ctor
		: CRideDialog( IDR_SINGLE_NUMBER, pParent )
		, caption(caption) , label(label) , range(range) , hexa(hexa*BST_CHECKED)
		, Value(initValue) {
	}

	void CSingleNumberDialog::PreInitDialog(){
		// dialog initialization
		__super::PreInitDialog();
		SetWindowText(caption);
		if (!EnableDlgItem( ID_FORMAT, (range.iMin|range.iMax)>=0 ))
			CheckDlgButton( ID_FORMAT, BST_UNCHECKED );
		TCHAR buf[200], strMin[16], strMax[16];
		if (hexa!=BST_UNCHECKED){
			TCHAR format[16];
			::wsprintf( format, _T("0x%%0%dX"), ::lstrlen(_itot(range.iMin|range.iMax,strMax,16)) );
			::wsprintf( strMin, format, range.iMin );
			::wsprintf( strMax, format, range.iMax );
		}else{
			_itot( range.iMin, strMin, 10 );
			_itot( range.iMax, strMax, 10 );
		}
		const int nLabelChars=::lstrlen(label);
		if (label[nLabelChars-1]==')'){ // Label finishes with text enclosed in brackets
			::wsprintf( ::lstrcpy(buf,label)+nLabelChars-1, _T("; %s - %s):"), strMin, strMax );
		}else
			::wsprintf( buf, _T("%s (%s - %s):"), label, strMin, strMax );
		SetDlgItemText( ID_INFORMATION, buf );
	}

	bool CSingleNumberDialog::GetCurrentValue(int &outValue) const{
		// True <=> input value successfully parsed, otherwise False
		TCHAR buf[16], *p=buf;
		auto nChars=GetDlgItemText( ID_NUMBER, buf, ARRAYSIZE(buf) );
		if (hexa!=BST_UNCHECKED){
			if (nChars>2 && *buf=='0' && buf[1]=='x')
				p+=2, nChars-=2;
			else if (nChars>1 && (*buf=='$'||*buf=='#'||*buf=='%'))
				p++, nChars--;
			if (nChars>sizeof(Value)*2)
				return false;
			return	_stscanf( ::CharLower(p), _T("%x"), &outValue )>0;
		}else{
			if (nChars>11) // e.g. "-1234567890"
				return false;
			return	_stscanf( p, _T("%d"), &outValue )>0;
		}
	}

	void CSingleNumberDialog::DoDataExchange(CDataExchange *pDX){
		// exchange of data from and to controls
		__super::DoDataExchange(pDX);
		const HWND hValue=GetDlgItemHwnd(ID_NUMBER);
		if (pDX->m_bSaveAndValidate){
			#if _MFC_VER>=0x0A00
				pDX->m_idLastControl=ID_NUMBER;
			#else
				pDX->m_hWndLastControl=hValue;
			#endif
			int v;
			if (!GetCurrentValue(v))
				pDX->Fail();
			DDV_MinMaxInt( pDX, v, range.iMin, range.iMax );
			Value=v;
		}else
			if (hexa!=BST_UNCHECKED){
				::SetWindowLong( hValue, GWL_STYLE, ::GetWindowLong(hValue,GWL_STYLE)&~ES_NUMBER );
				SetDlgItemFormattedText( ID_NUMBER, _T("0x%X"), Value );
			}else{
				::SetWindowLong( hValue, GWL_STYLE, ::GetWindowLong(hValue,GWL_STYLE)|ES_NUMBER );
				DDX_Text( pDX, ID_NUMBER, Value );
			}
		DDX_Check( pDX, ID_FORMAT, hexa );
	}

	LRESULT CSingleNumberDialog::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		if (msg==WM_COMMAND && wParam==MAKELONG(ID_FORMAT,BN_CLICKED)){
			int v;
			if (GetCurrentValue(v) && range.Contains(v))
				Value=v;
			hexa=IsDlgButtonChecked(ID_FORMAT);
			PreInitDialog();
			DoDataExchange( &CDataExchange(this,FALSE) );
			FocusDlgItem( ID_NUMBER );
			Edit_SetSel( GetDlgItemHwnd(ID_NUMBER), 0, -1 ); // selecting full content
		}
		return __super::WindowProc(msg,wParam,lParam);
	}

	CSingleNumberDialog::operator bool() const{
		// True <=> showed dialog confirmed, otherwise False
		const auto result=const_cast<CSingleNumberDialog *>(this)->DoModal();
		if (m_pParentWnd)
			::SetFocus( *m_pParentWnd );
		return result==IDOK;
	}













	void BytesToHigherUnits(DWORD bytes,float &rHigherUnit,LPCTSTR &rHigherUnitName){
		// converts Bytes to suitable HigherUnits (e.g. "12345 Bytes" to "12.345 kiB")
		if (bytes>=0x40000000)
			rHigherUnit=(double)bytes/0x40000000, rHigherUnitName=_T("GiB");
		else if (bytes>=0x100000)
			rHigherUnit=(float)bytes/0x100000, rHigherUnitName=_T("MiB");
		else if (bytes>=0x400)
			rHigherUnit=(float)bytes/0x400, rHigherUnitName=_T("KiB");
		else
			rHigherUnit=bytes, rHigherUnitName=_T("Bytes");
	}

	CString BytesToHexaText(PCBYTE bytes,BYTE nBytes,bool lastDelimitedWithAnd){
		// composes a string containing hexa-decimal notation of specified Bytes, the last Byte optionally separated with "and"; for example, given Bytes="HELLO", the returned string is "0x48, 0x45, 0x4C, 0x4C, and 0x4F"
		lastDelimitedWithAnd&=nBytes>=2;
		CString result;
		for( TCHAR tmp[8]; nBytes--; result+=tmp ){
			::wsprintf( tmp, _T("0x%02X, "), *bytes++ );
			if (!nBytes){ // this is the last Byte
				if (lastDelimitedWithAnd)
					result+=_T("and ");
				tmp[4]='\0';
			}
		}
		return result;
	}

	CString BytesToHexaText(const char *chars,BYTE nChars,bool lastDelimitedWithAnd){
		// composes a string containing hexa-decimal notation of specified Bytes, the last Byte optionally separated with "and"; for example, given Bytes="HELLO", the returned string is "0x48, 0x45, 0x4C, 0x4C, and 0x4F"
		return BytesToHexaText( (PCBYTE)chars, nChars, lastDelimitedWithAnd );
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








	const TSplitButtonAction TSplitButtonAction::HorizontalLine={ 0, 0, MF_SEPARATOR };

	typedef struct TSplitButtonInfo sealed{
		const PCSplitButtonAction pAction;
		const BYTE nActions;
		const WNDPROC wndProc0;
		RECT rcClientArea;
		TCHAR text[512];

		TSplitButtonInfo(HWND hBtn,PCSplitButtonAction _pAction,BYTE _nActions,WNDPROC _wndProc0)
			// ctor
			: pAction(_pAction) , nActions(_nActions) , wndProc0(_wndProc0) {
			::GetClientRect(hBtn,&rcClientArea);
			*text='\0';
		}

		inline bool ExistsDefaultAction() const{
			return pAction->commandId!=0;
		}
	} *PSplitButtonInfo;

	#define SPLITBUTTON_ARROW_WIDTH	(LogicalUnitScaleFactor*18)

	static LRESULT WINAPI __splitButton_wndProc__(HWND hSplitBtn,UINT msg,WPARAM wParam,LPARAM lParam){
		const PSplitButtonInfo psbi=(PSplitButtonInfo)::GetWindowLong(hSplitBtn,GWL_USERDATA);
		const WNDPROC wndProc0=psbi->wndProc0;
		switch (msg){
			case WM_CAPTURECHANGED:
				// clicked on (either by left mouse button or using space-bar)
				lParam=0; // treated as if clicked onto the default Action area
				//fallthrough
			case WM_LBUTTONDBLCLK:
			case WM_LBUTTONDOWN:
				// left mouse button pressed
				if (psbi->ExistsDefaultAction() && GET_X_LPARAM(lParam)<psbi->rcClientArea.right-SPLITBUTTON_ARROW_WIDTH)
					// A&B, A = default Action exists, B = in default Action area
					break; // base
				else{
					// in area of selecting additional Actions
					CMenu mnu;
					mnu.CreatePopupMenu();
					for( BYTE id=!psbi->ExistsDefaultAction(); id<psbi->nActions; id++ ){
						const auto &a=psbi->pAction[id];
						mnu.AppendMenu( a.menuItemFlags, a.commandId, a.commandCaption );
					}
					POINT pt={ psbi->rcClientArea.right-SPLITBUTTON_ARROW_WIDTH, psbi->rcClientArea.bottom };
					::ClientToScreen( hSplitBtn, &pt );
					::TrackPopupMenu( mnu.m_hMenu, TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON, pt.x, pt.y, 0, ::GetParent(hSplitBtn), nullptr );
					//fallthrough
				}
			case WM_LBUTTONUP:
				// left mouse button released
				if (psbi->ExistsDefaultAction() && GET_X_LPARAM(lParam)<psbi->rcClientArea.right-SPLITBUTTON_ARROW_WIDTH)
					// in default Action area
					break; // base
				else{
					// in area of selecting additional Actions
					::ReleaseCapture();
					return 0;
				}
			case WM_SETTEXT:
				// text about to change
				::lstrcpyn( psbi->text, (LPCTSTR)lParam, ARRAYSIZE(psbi->text) );
				::InvalidateRect( hSplitBtn, nullptr, TRUE );
				return 0;
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
					const CRideFont fontParent( ::GetParent(hSplitBtn) );
					const HGDIOBJ hFont0=::SelectObject( dc, fontParent );
						const SIZE textSize=fontParent.GetTextSize( psbi->text );
						r.left=std::max( (r.right-textSize.cx)/2, 0L ), r.top=std::max( (r.bottom-textSize.cy)/2, 0L );
						::DrawText( dc, psbi->text,-1, &r, DT_LEFT|DT_TOP );
					//::SelectObject(dc,hFont0);
					// : arrow
					r=psbi->rcClientArea;
					r.left=r.right-SPLITBUTTON_ARROW_WIDTH;
					const CRideFont font( FONT_WEBDINGS, 110, false, true );
					::SelectObject( dc, font );
						static constexpr WCHAR Arrow=0xf036;
						::DrawTextW( dc, &Arrow,1, &r, DT_SINGLELINE|DT_CENTER|DT_VCENTER );
					::SelectObject(dc,hFont0);
					// : splitting using vertical line
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

	void CRideDialog::ConvertDlgButtonToSplitButton(WORD id,PCSplitButtonAction pAction,BYTE nActions) const{
		// converts an existing standard button to a SplitButton featuring specified additional Actions
		const HWND hStdBtn=GetDlgItemHwnd(id);
		::SetWindowText(hStdBtn,nullptr); // before window procedure changed
		::SetWindowLong(hStdBtn,GWL_ID,pAction->commandId); // 0.Action is the default
		::SetWindowLong(hStdBtn, GWL_USERDATA,
						(long)new TSplitButtonInfo(
							hStdBtn,
							pAction,
							nActions,
							(WNDPROC)::SetWindowLong( hStdBtn, GWL_WNDPROC, (long)__splitButton_wndProc__ )
						)
					);
		::SetWindowText(hStdBtn,pAction->commandCaption); // after window procedure changed
		::InvalidateRect(hStdBtn,nullptr,TRUE);
	}

	void CRideDialog::ConvertToCommandLikeButton(HWND hStdBtn,WCHAR wingdingsGlyphBeforeText,COLORREF textColor,int glyphPointSizeIncrement,COLORREF glyphColor){
		// converts an existing standard button to a "command-like" one known from Windows Vista, featuring specified GlypfBeforeText ('\0' = no Glyph)
		::SetWindowLong( hStdBtn, GWL_STYLE, ::GetWindowLong(hStdBtn,GWL_STYLE)|BS_OWNERDRAW );
		::SetWindowLong(hStdBtn,
						GWL_USERDATA,
						(long)new TCommandLikeButtonInfo(
							(WNDPROC)::SetWindowLong( hStdBtn, GWL_WNDPROC, (long)__commandLikeButton_wndProc__ ),
							wingdingsGlyphBeforeText, glyphColor, glyphPointSizeIncrement,
							textColor
						)
					);
		::SendMessage( hStdBtn, WM_SETFONT, ::SendMessage(::GetParent(hStdBtn),WM_GETFONT,0,0), 0 );
		::InvalidateRect(hStdBtn,nullptr,FALSE);
	}

	void CRideDialog::ConvertToCancelLikeButton(HWND hStdBtn,COLORREF textColor,int glyphPointSizeIncrement,COLORREF glyphColor){
		// converts an existing standard button to a "command-like" one known from Windows Vista, featuring specified GlypfBeforeText ('\0' = no Glyph)
		ConvertToCommandLikeButton( hStdBtn, CANCEL_COMMAND_GLYPH, textColor, glyphPointSizeIncrement, glyphColor );
	}

	void ScaleLogicalUnit(HDC dc){
		// changes given DeviceContext's size of one logical unit
		if (LogicalUnitScaleFactor!=1){
			::SetMapMode(dc,MM_ISOTROPIC);
			::SetWindowExtEx( dc, SCREEN_DPI_DEFAULT, SCREEN_DPI_DEFAULT, nullptr );
			::SetViewportExtEx( dc, ::GetDeviceCaps(dc,LOGPIXELSX), ::GetDeviceCaps(dc,LOGPIXELSY), nullptr );
		}
	}

	void ScaleLogicalUnit(PINT values,BYTE nValues){
		// adds to specified Values the logical unit scale factor
		while (nValues--)
			*values++*=LogicalUnitScaleFactor;
	}

	void UnscaleLogicalUnit(PINT values,BYTE nValues){
		// removes from specified Values the logical unit scale factor
		while (nValues--)
			*values++/=LogicalUnitScaleFactor;
	}

	COLORREF GetSaturatedColor(COLORREF currentColor,float saturationFactor){
		// saturates input Color by specified SaturationFactor and returns the result
		ASSERT(saturationFactor>=0);
		COLORREF result=0;
		for( BYTE i=sizeof(COLORREF),*pbIn=(PBYTE)&currentColor,*pbOut=(PBYTE)&result; i-->0; ){
			const float w=*pbIn++*saturationFactor;
			*pbOut++=std::min( w, 255.f );
		}
		return result;
	}

	COLORREF GetBlendedColor(COLORREF color1,COLORREF color2,float blendFactor){
		// computes and returns the Color that is the mixture of the two input Colors in specified ratio (BlendFactor=0 <=> only Color1, BlendFactor=1 <=> only Color2
		ASSERT(0.f<=blendFactor && blendFactor<=1.f);
		COLORREF result=0;
		for( BYTE i=sizeof(COLORREF),*pbIn1=(PBYTE)&color1,*pbIn2=(PBYTE)&color2,*pbOut=(PBYTE)&result; i-->0; ){
			const float w = blendFactor**pbIn1++ + (1.f-blendFactor)**pbIn2++;
			*pbOut++=std::min( w, 255.f );
		}
		return result;
	}

	CString GenerateTemporaryFileName(){
		// generates and returns a new temporary file name
		TCHAR buf[MAX_PATH];
		::GetTempPath( ARRAYSIZE(buf), buf );
		::GetTempFileName( buf, nullptr, FALSE, buf );
		return buf;
	}

	CFile &WriteToFile(CFile &f,LPCTSTR text){
		// writes specified Text into the File
		f.Write( text, ::lstrlen(text) );
		return f;
	}
	CFile &WriteToFileFormatted(CFile &f,LPCTSTR format,...){
		// writes Formatted string into the File
		va_list argList;
		va_start( argList, format );
			TCHAR buf[16384];
			f.Write(  buf,  ::wvsprintf( buf, format, argList )  );
		va_end(argList);
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
			if (std::floor(number)==number) // ... and the Number doesn't have decimal digits (only integral part) ...
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

	void RandomizeData(PVOID buffer,WORD nBytes){
		// populates Buffer with given CountOfBytes of random data
		::srand( ::GetTickCount() );
		for( PBYTE p=(PBYTE)buffer; nBytes--; *p++=::rand() );
	}

	CString DoPromptSingleTypeFileName(LPCTSTR defaultSaveName,LPCTSTR singleFilter,DWORD flags){
		// shows corresponding Open/Save dialog and returns full path selected in it (or empty string, if cancelled)
		app.m_pMainWnd->BeginModalState(); // must not operate with the MainWindow
			CString title;
				title.LoadString( defaultSaveName==nullptr ? AFX_IDS_OPENFILE : AFX_IDS_SAVEFILE );
			TCHAR fileName[MAX_PATH];
			const HWND hParent=app.GetEnabledActiveWindow();
			CFileDialog d(
				defaultSaveName==nullptr,
				singleFilter ? _tcsrchr(singleFilter,'.') : nullptr,
				nullptr,
				OFN_EXPLORER | OFN_OVERWRITEPROMPT | flags,
				singleFilter,
				hParent ? CWnd::FromHandle(hParent) : nullptr
			);
				d.m_ofn.lStructSize=sizeof(OPENFILENAME); // to show the "Places bar"
				d.m_ofn.nFilterIndex=1;
				d.m_ofn.lpstrTitle=title;
				d.m_ofn.lpstrFile=fileName;
				if (defaultSaveName) // saving?
					::lstrcpy( fileName, defaultSaveName );
				else
					*fileName='\0';
			const bool dialogConfirmed=d.DoModal()==IDOK;
		app.m_pMainWnd->EndModalState();
		if (dialogConfirmed)
			return fileName;
		else
			return (LPCTSTR)nullptr;
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
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		TDownloadSingleFileParams &rdsfp=*(TDownloadSingleFileParams *)pAction->GetParams();
		pAction->SetProgressTargetInfinity();
		HINTERNET hSession=nullptr, hOnlineFile=nullptr;
		// - opening a new Session
		hSession=::InternetOpenA(APP_IDENTIFIER,
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
		return pAction->TerminateWithSuccess();
	}

	TStdWinError DownloadSingleFile(LPCTSTR onlineFileUrl,PBYTE fileDataBuffer,DWORD fileDataBufferLength,PDWORD pDownloadedFileSize,LPCTSTR fatalErrorConsequence){
		// returns the result of downloading the file with given Url
		TDownloadSingleFileParams params( onlineFileUrl, fileDataBuffer, fileDataBufferLength, fatalErrorConsequence );
		const TStdWinError err=	CBackgroundActionCancelable(
									__downloadSingleFile_thread__,
									&params,
									THREAD_PRIORITY_ABOVE_NORMAL
								).Perform();
		if (pDownloadedFileSize!=nullptr)
			*pDownloadedFileSize=params.outOnlineFileSize;
		return err;
	}

}
