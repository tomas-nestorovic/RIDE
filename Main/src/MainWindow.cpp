#include "stdafx.h"
#include "MSDOS7.h"

	CMainWindow::CDynMenu::CDynMenu(UINT nResId)
		// ctor
		: hAccel( ::LoadAccelerators(AfxGetInstanceHandle(),(LPCTSTR)nResId) ) {
		if (nResId!=0)
			LoadMenu(nResId);
	}
	CMainWindow::CDynMenu::~CDynMenu(){
		// dtor
		::DestroyAcceleratorTable(hAccel);
	}

	void CMainWindow::CDynMenu::Show(UINT position) const{
		// shows this Menu in MainWindow's menu
		if (m_hMenu){ // may not if the MainWindow is being closed
			TCHAR buf[30];
			GetMenuString( 0, buf, ARRAYSIZE(buf), MF_BYPOSITION );
			app.m_pMainWnd->GetMenu()->InsertMenu(	position,
													MF_BYPOSITION | MF_POPUP,
													(UINT)::GetSubMenu(m_hMenu,0),
													buf
												);
			app.m_pMainWnd->DrawMenuBar();
		}
	}
	void CMainWindow::CDynMenu::Hide() const{
		// removes this Menu from MainWindow's menu
		if (m_hMenu && app.m_pMainWnd){ // may not if the MainWindow is being closed
			app.m_pMainWnd->GetMenu()->RemoveMenu( (UINT)::GetSubMenu(m_hMenu,0), MF_BYCOMMAND|MF_POPUP );
			app.m_pMainWnd->DrawMenuBar();
		}
	}








	CMainWindow::CDockableToolBar::CDockableToolBar(UINT nResId,UINT id)
		// ctor
		: nResId(nResId) , id(id) {
		ASSERT( !nResId || id );
	}

	void CMainWindow::CDockableToolBar::Show(const CToolBar &rDockNextTo){
		// shows this ToolBar docked immediately next to the specified existing one
		if (!nResId)
			return;
		ASSERT( CWnd::FromHandle(*app.m_pMainWnd)==app.m_pMainWnd ); // must be launched from the main thread
		if (!m_hWnd){
			Create( app.m_pMainWnd, WS_CHILD|CBRS_TOP|CBRS_TOOLTIPS, id );
			LoadToolBar(nResId);
			EnableDocking(CBRS_ALIGN_TOP);
		}
		if (m_hWnd && rDockNextTo.m_hWnd){ // only if both ToolBars exist (may not if the MainWin			w is being closed)
			RECT r;
			rDockNextTo.GetWindowRect(&r);
			::OffsetRect(&r,1,0);
			CMainWindow *const pMainWindow=app.GetMainWindow();
			pMainWindow->DockControlBar(this,AFX_IDW_DOCKBAR_TOP,&r);
			pMainWindow->ShowControlBar(this,TRUE,FALSE);
		}
	}

	void CMainWindow::CDockableToolBar::Hide(){
		// hides this Toolbar
		if (m_hWnd) // this ToolBar is destroyed automatically when the MainWindow is being closed
			app.GetMainWindow()->ShowControlBar(this,FALSE,FALSE);
	}

	void CMainWindow::CDockableToolBar::OnUpdateCmdUI(CFrameWnd* pTarget,BOOL bDisableIfNoHndler){
		return __super::OnUpdateCmdUI( pTarget, FALSE ); // False = don't search the message map for corresponding command handlers
	}









	constexpr int MessageBarMargin=20;

	int CMainWindow::CMessageBar::D;

	CMainWindow::CMessageBar::CMessageBar(LPCTSTR msgHyperlink,WCHAR webdingsGlyph)
		// ctor
		: glyph(webdingsGlyph) , msgHyperlink(msgHyperlink) {
		m_cxLeftBorder = m_cxRightBorder = m_cyTopBorder = m_cyBottomBorder = 0; // assure there is no non-client area that the user might use for resizing
	}

	CMainWindow::CMessageBar::~CMessageBar(){
		// dtor
		if (m_hWnd){
			DestroyWindow();
			app.GetMainWindow()->RecalcLayout();
		}
	}

	void CMainWindow::CMessageBar::Show(){
		// shows the MessageBar with formatted text at the top of MainWindow
		// - "base"
		if (!Create( app.m_pMainWnd, WS_CHILD|WS_VISIBLE|CBRS_TOP ))
			return;
		D = Utils::CRideFont::StdDpi.charHeight*(100+2*MessageBarMargin)/100;
		GetStatusBarCtrl().SetMinHeight(D);
		// - glyph (adjust vertical centering by empirical constant)
		hGlyph=::CreateWindowW( WC_STATICW, nullptr, WS_VISIBLE|WS_CHILD|SS_CENTER|SS_CENTERIMAGE, 0,0, D,D, m_hWnd, (HMENU)ID_HEAD, nullptr, nullptr );
		Utils::CRideDialog::SetDlgItemSingleCharUsingFont( m_hWnd, ID_HEAD, glyph, Utils::CRideFont::Webdings120 );
		// - message (fake vertical centering by prepending '\n' and positioning then control accordingly)
		const int yCenterFake=Utils::CRideFont::StdDpi.charHeight*(MessageBarMargin-100)/100-Utils::CRideFont::StdDpi.charDescent;
		hSysLink=::CreateWindowW( WC_LINK, CDos::CPathString(L'\n').Append((LPCTSTR)msgHyperlink).GetUnicode(), WS_VISIBLE|WS_CHILD, D,yCenterFake, 1,1, m_hWnd, (HMENU)ID_INFORMATION, nullptr, nullptr );
		SetWindowFont( hSysLink, Utils::CRideFont::StdDpi, TRUE );
		// - "Close" button
		hCloseBtn=::CreateWindowW( WC_BUTTONW, nullptr, WS_VISIBLE|WS_CHILD|BS_CENTER|BS_FLAT, 0,0, D,D, m_hWnd, (HMENU)ID_CREDITS, nullptr, nullptr );
		Utils::CRideDialog::SetDlgItemSingleCharUsingFont( m_hWnd, ID_CREDITS, L'\xf072', Utils::CRideFont::Webdings80 );
		// - position at top
		app.GetMainWindow()->RecalcLayout();
	}

	LRESULT CMainWindow::CMessageBar::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_SIZE:{
				if (const LRESULT err=__super::WindowProc( msg, wParam, lParam ))
					return err;
				const Utils::TClientRect client(m_hWnd);
				::SetWindowPos( hSysLink, nullptr, 0,0, client.Width()-2*D, 2*client.Height(), SWP_NOZORDER|SWP_NOMOVE );
				::SetWindowPos( hCloseBtn, nullptr, client.Width()-D,0, 0,0, SWP_NOZORDER|SWP_NOSIZE|SWP_SHOWWINDOW );
				return 0;
			}
			case WM_COMMAND:
				switch (wParam){
					case MAKELONG(ID_CREDITS,BN_CLICKED):
						::DestroyWindow(m_hWnd);
						app.GetMainWindow()->RecalcLayout();
						return 0;
				}
				break;
			case WM_CTLCOLORSTATIC:{
				static const Utils::CRideBrush Yellowish((COLORREF)0xabfbff);
				::SetBkMode( (HDC)wParam, TRANSPARENT );
				return Yellowish;
			}
			case WM_NOTIFY:
				if (Utils::CRideDialog::GetClickedHyperlinkId(lParam)==ID_INFORMATION){
					HyperlinkClicked( ((PNMLINK)lParam)->item.szID );
					return 0;
				}
				break;
			case WM_DESTROY:
				::DestroyWindow(hGlyph);
				::DestroyWindow(hSysLink);
				::DestroyWindow(hCloseBtn);
				break;
		}
		return __super::WindowProc( msg, wParam, lParam );
	}

	void CMainWindow::CMessageBar::HyperlinkClicked(LPCWSTR id) const{
	}










	void CMainWindow::SetStatusBarTextReady(){
		// sets the MainWindow's StatusBar text
		CStatusBar &rsb=app.GetMainWindow()->statusBar;
		if (rsb.m_hWnd)
			rsb.SetPaneText(0,_T("Ready"));
	}
	void CMainWindow::__resetStatusBar__(){
		// resets the MainWindow's StatusBar
		CStatusBar &rsb=app.GetMainWindow()->statusBar;
		if (rsb.m_hWnd){
			static constexpr UINT Indicator=ID_SEPARATOR;
			rsb.SetIndicators(&Indicator,1);
			SetStatusBarTextReady();
		}
	}
	void CMainWindow::__setStatusBarText__(LPCTSTR text){
		// sets the MainWindow's StatusBar main part to the specified Text
		CStatusBar &rsb=app.GetMainWindow()->statusBar;
		if (rsb.m_hWnd)
			rsb.SetPaneText(0,text);
	}

	








	BEGIN_MESSAGE_MAP(CMainWindow,CFrameWnd)
		ON_WM_CREATE()
		ON_WM_CLOSE()
		ON_WM_INITMENU()
		ON_WM_DROPFILES()
		ON_WM_LBUTTONDBLCLK()
		ON_UPDATE_COMMAND_UI_RANGE(ID_FILE_SAVE,ID_FILE_SAVE_AS,__imageOperation_updateUI__)
		ON_UPDATE_COMMAND_UI_RANGE(ID_IMAGE_DUMP,ID_IMAGE_SETTINGS,__imageOperation_updateUI__)
		ON_COMMAND(ID_TDI_SWITCH,__switchToNextTab__)
		ON_COMMAND(ID_TDI_SWITCH_BACK,__switchToPrevTab__)
		ON_COMMAND(ID_FILE_CLOSE,__closeCurrentTab__)
			ON_UPDATE_COMMAND_UI(ID_FILE_CLOSE,__closeCurrentTab_updateUI__)
		ON_COMMAND(ID_RECOGNIZE,EditAutomaticRecognitionSequence)
		ON_COMMAND(ID_APP_UPDATE,__openUrl_checkForUpdates__)
			ON_UPDATE_COMMAND_UI(ID_APP_UPDATE,__openUrl_checkForUpdates_updateUI__)
		ON_COMMAND(ID_HELP_FAQ,__openUrl_faq__)
		ON_COMMAND(ID_HELP_DONATE,OpenUrl_Donate)
		ON_COMMAND(ID_HELP_REPORT_BUG,__openUrl_reportBug__)
		ON_COMMAND(ID_HELP_REPOSITORY,__openUrl_repository__)
		ON_COMMAND(ID_HELP_TUTORIALS,__openUrl_tutorials__)
		ON_COMMAND(ID_YAHEL_REPOSITORY,OpenYahelRepositoryUrl)
		ON_COMMAND(ID_YAHEL_ABOUT,ShowYahelAbout)
		ON_COMMAND(ID_CREDITS,__openUrl_credits__)
	END_MESSAGE_MAP()











	BOOL CMainWindow::PreCreateWindow(CREATESTRUCT &cs){
		// adjusting the instantiation
		// - base
		if (!__super::PreCreateWindow(cs))
			return FALSE;
		// - adjusting the style
		cs.dwExStyle&=~WS_EX_CLIENTEDGE;
		// - registering a custom named class, so that the count of running instances can be computed (to determine whether the app has crashed last time)
		WNDCLASS wc;
		::GetClassInfo( app.m_hInstance, cs.lpszClass, &wc );
		wc.lpszClassName = cs.lpszClass = APP_CLASSNAME;
		wc.hIcon=app.LoadIcon(IDR_MAINFRAME);
		return AfxRegisterClass(&wc);
	}

	BOOL CMainWindow::PreTranslateMessage(PMSG pMsg){
		// pre-processing the Message
		if (const CTdiView::TTab *const t=pTdi->pCurrentTab){
			// . FileManager's Editor receives all messages and none must be pre-translated
			if (CFileManagerView::CEditorBase::pSingleShown)
				return FALSE;
			// . PropertyGrid's Editor receives all messages and none must be pre-translated
			if (CCriticalSectorView::__isValueBeingEditedInPropertyGrid__())
				return FALSE;
			// . File HexaPreview receives all messages and none must be pre-translated (but only if focused)
			if (CDos::CHexaPreview::pSingleInstance)
				if (::GetActiveWindow()==CDos::CHexaPreview::pSingleInstance->m_hWnd) // must be focused
					return FALSE;
			// . pretranslating the Message by accelerators of current TDI Tab
			if (::TranslateAccelerator(m_hWnd,t->menu.hAccel,pMsg))
				return TRUE;
			// . pretranslating the Message by accelerators of currently focused DOS (if any; e.g. WebPageView usually isn't associated with any DOS)
			if (const PCImage image=t->image){
				// focused DOS (e.g. a ZX Tape)
				if (::TranslateAccelerator(m_hWnd,image->dos->menu.hAccel,pMsg)) return TRUE;
			}else if (const PCImage image=CImage::GetActive())
				// active DOS (e.g. MDOS when a WebPage is focused)
				if (image->dos)
					if (::TranslateAccelerator(m_hWnd,image->dos->menu.hAccel,pMsg)) return TRUE;
		}else
			if (app.GetRecentFileList()->PreTranslateMessage(m_hWnd,pMsg)) return TRUE;
		return __super::PreTranslateMessage(pMsg); // base
	}

	afx_msg int CMainWindow::OnCreate(LPCREATESTRUCT lpcs){
		// window created
		// - base
		if (__super::OnCreate(lpcs)==-1) return -1;
		EnableDocking(CBRS_ALIGN_ANY);
		m_bAutoMenuEnable=false;
		// - creating the TDI
		pTdi=new CTdiView;
		pTdi->Create( nullptr, nullptr, AFX_WS_DEFAULT_VIEW&~WS_BORDER, rectDefault, this, AFX_IDW_PANE_FIRST );
		CTdiCtrl::SubclassWnd(
			app.m_hInstance,
			pTdi->m_hWnd,
			&CTdiCtrl::TParams( pTdi, CTdiView::__fnShowContent__, CTdiView::__fnHideContent__, CTdiView::__fnRepaintContent__, CTdiView::__fnGetHwnd__ )
		);
		pTdi->SendMessage( WM_SETFONT, (WPARAM)Utils::CRideFont(FONT_MS_SANS_SERIF,90,false,true).Detach(), TRUE );
		// - creating the StatusBar
		statusBar.Create(this);
		__resetStatusBar__();
		// - creating a visible main ToolBar (WS_VISIBLE)
		toolbar.Create(this,WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS);
		toolbar.LoadToolBar(IDR_MAINFRAME);
		toolbar.EnableDocking(CBRS_ALIGN_TOP);
		DockControlBar(&toolbar,AFX_IDW_DOCKBAR_TOP);
		// - registering the MainWindow as the target of drag&drop (e.g. when dropping an Image dragged from Explorer)
		DragAcceptFiles();
		// - 
		/*
		CCommandLineInfo cmdInfo;
		app.ParseCommandLine(cmdInfo);
		if (!app.ProcessShellCommand(cmdInfo))
			return FALSE;
			//*/
		return 0;
	}

	afx_msg void CMainWindow::OnClose(){
		// a fixed (and simplified) version of CFrameWnd::OnClose
		CDocument *const pDocument=GetActiveDocument();
		if (pDocument && !pDocument->CanCloseFrame(this)) // document can't close right now
			return; // don't close us
		if (!pDocument && !app.SaveAllModified()) // attempt to save all documents
			return; // don't close us
		app.CloseAllDocuments(FALSE); // close all documents first
		if (!AfxOleCanExitApp()) // don't exit if there are outstanding component objects
			return AfxOleSetUserCtrl(FALSE); // take user out of control of the app
		DestroyWindow(); // then destroy the window
	}

	afx_msg void CMainWindow::OnDropFiles(HDROP hDrop){
		// a File from Windows Explorer dropped on the client area
		extern CDos::PCProperties manuallyForceDos;
		manuallyForceDos=nullptr; // Null = use automatic recognition
		__super::OnDropFiles(hDrop);
	}

	afx_msg void CMainWindow::OnLButtonDblClk(UINT,CPoint){
		// mouse double-clicked on the client area
		if (!pTdi->GetCurrentTab()) // double-click on the client interpreted as opening an Image
			if (::GetKeyState(VK_SHIFT)<0) // without DOS ?
				app.OpenImageWithoutDos();
			else
				app.OpenImage();
	}

	afx_msg void CMainWindow::OnInitMenu(CMenu *menu){
		// clicked somewhere in the main menu
		SetFocus(); // to immediately carry out actions that depend on focus
		__super::OnInitMenu(menu);
	}

	afx_msg void CMainWindow::__imageOperation_updateUI__(CCmdUI *pCmdUI){
		// projecting Image operability into UI
		pCmdUI->Enable( GetActiveDocument()!=nullptr );
	}

	afx_msg void CMainWindow::__switchToNextTab__(){
		// switches to the next Tab (Ctrl+Tab); this handler must be defined here as the TDI is not always the current view (and thus not always receives this command, Cmd)
		CTdiCtrl::SwitchToNextTab( pTdi->m_hWnd );
	}

	afx_msg void CMainWindow::__switchToPrevTab__(){
		// switches to the previous Tab (Ctrl+Shit+Tab); this handler must be defined here as the TDI is not always the current view (and thus not always receives this command, Cmd)
		CTdiCtrl::SwitchToPrevTab( pTdi->m_hWnd );
	}

	afx_msg void CMainWindow::__closeCurrentTab__(){
		// closes currently visible TDI Tab
		if (pTdi->GetCurrentTab()->image) // if current Tab relates to an Image ...
			CTdiTemplate::pSingleInstance->__closeDocument__(); // ... closing it (and thus the DOS and all its Tabs and Views)
		else
			CTdiCtrl::RemoveCurrentTab( pTdi->m_hWnd ); // ... otherwise closing just the Tab (e.g. a WebPageView that usually isn't associated with any DOS)
	}

	afx_msg void CMainWindow::__closeCurrentTab_updateUI__(CCmdUI *pCmdUI){
		// projecting possibility to close current Tab into UI
		pCmdUI->Enable( pTdi->GetCurrentTab()!=nullptr );
	}



	void CMainWindow::OpenWebPage(LPCTSTR tabCaption,LPCTSTR url){
		// opens specified URL in a new Tab
		CWebPageView *const webView=new CWebPageView(url);
		CTdiCtrl::AddTabLast( TDI_HWND, tabCaption, &webView->tab, true, TDI_TAB_CANCLOSE_ALWAYS, CTdiView::TTab::OnOptionalTabClosing );
	}
	void CMainWindow::OpenRepositoryWebPage(LPCTSTR tabCaption,LPCTSTR documentName){
		// in new Tab opens the specified Document from application's on-line presentation
		TCHAR url[MAX_PATH];
		OpenWebPage(
			tabCaption ? tabCaption : _T("GitHub"),
			::lstrcat(  ::lstrcpy( url, GITHUB_REPOSITORY ),  documentName  )
		);
	}
	void CMainWindow::OpenApplicationPresentationWebPage(LPCTSTR tabCaption,LPCTSTR documentName){
		// in new Tab opens the specified Document from application's on-line presentation
		TCHAR url[MAX_PATH];
		OpenWebPage( tabCaption, Utils::GetApplicationOnlineHtmlDocumentUrl(documentName,url) );
	}

	void CMainWindow::OpenApplicationFaqWebPage(LPCTSTR documentName){
		OpenApplicationPresentationWebPage( _T("Answer"), documentName );
	}

	afx_msg void CMainWindow::EditAutomaticRecognitionSequence(){
		CDos::CRecognition::EditSequence();
	}

	#define VERSION_LATEST_WEB	_T("usingLatest.html")

	UINT AFX_CDECL CMainWindow::CTdiView::RecencyDetermination_thread(PVOID pCancelableAction){
		// checks if this instance of application is the latest by comparing it against the on-line information; returns ERROR_SUCCESS (on-line information was downloaded and this instance is up-to-date), ERROR_EVT_VERSION_TOO_OLD (on-line information was downloaded but this instance is out-of-date), or other error
		CBackgroundAction *const pAction=(CBackgroundAction *)pCancelableAction;
		const PBackgroundActionCancelable pBac=dynamic_cast<PBackgroundActionCancelable>(pAction);
		if (pBac)
			pBac->SetProgressTargetInfinity();
		// - download the JSON description
		const Utils::TInternetSession session;
		if (!session)
			return pBac ? pBac->TerminateWithLastError() : ::GetLastError();
		if (pBac)
			if (pBac->Cancelled) return ERROR_CANCELLED;
		struct{
			char buffer[16384];
			TStdWinError EvaluateRecency(){
				if (const PCHAR githubTagName=::StrStrIA(buffer,GITHUB_VERSION_TAG_NAME))
					if (PCHAR r=::StrChrA(githubTagName+sizeof(GITHUB_VERSION_TAG_NAME),'\"')) // "R"emote tag
						if (const PCHAR q=::StrChrA( ++r, '\"' )){ // "+1" = skip the opening quote
							*q='\0'; // replace the closing quote with the Null character
							app.WriteProfileString( INI_GENERAL, INI_LATEST_KNOWN_VERSION, Utils::ToStringT(r) );
							LPCSTR t=APP_VERSION; // "T"his tag
							do{
								if (::IsCharSpaceA(*t))
									t++; // ignoring any whitespaces in "T"his tag
								else if (*r++!=*t++)
									return ERROR_EVT_VERSION_TOO_OLD; // the app is outdated
							} while (*r/*&&*t*/); // commented out as redundant (any differences already caught above)
							return ERROR_SUCCESS; // the app is up-to-date
						}
				return ERROR_INVALID_FIELD;
			}
		} status;
		DWORD nBytesRead;
		if (const TStdWinError err=session.DownloadOneHttp( GITHUB_API_SERVER_NAME, _T("/repos/tomas-nestorovic/RIDE/releases/latest"), status.buffer, sizeof(status.buffer), nBytesRead ))
			return pBac ? pBac->TerminateWithError(err) : err;
		// - analysing the obtained information (comparing it against this instance version)
		status.buffer[nBytesRead]='\0';
		if (const DWORD now=Utils::CRideTime().GetDosDateTime()){ // recording that recency last checked Now
			app.dateRecencyLastChecked=now;
			app.WriteProfileInt( INI_GENERAL, INI_IS_UP_TO_DATE, app.dateRecencyLastChecked );
		}
		if (const TStdWinError err=status.EvaluateRecency()){
			if (err==ERROR_EVT_VERSION_TOO_OLD)
				TDI_INSTANCE->RepopulateGuidePost();
			return pBac ? pBac->TerminateWithError(err) : err;
		}else
			return pBac ? pBac->TerminateWithSuccess() : ERROR_SUCCESS;
	}

	afx_msg void CMainWindow::__openUrl_checkForUpdates__(){
		// checks if this instance of application is the latest by comparing it against the on-line information; opens either "You are using the latest version" web page, or the "You are using out-of-date version" web page
		switch (const TStdWinError err=	CBackgroundActionCancelable(
											CMainWindow::CTdiView::RecencyDetermination_thread,
											nullptr,
											THREAD_PRIORITY_LOWEST
										).Perform()
		){
			case ERROR_SUCCESS:
				return OpenApplicationPresentationWebPage(_T("Version"),VERSION_LATEST_WEB);
			case ERROR_EVT_VERSION_TOO_OLD:
				return OpenApplicationPresentationWebPage(_T("Version"),_T("usingOld.html"));
			default:
				return Utils::Information(_T("Cannot retrieve the information"),err);
		}
	}
	afx_msg void CMainWindow::__openUrl_checkForUpdates_updateUI__(CCmdUI *pCmdUI){
		#ifdef APP_SPECIAL_VER
			pCmdUI->Enable(FALSE);
		#else
			pCmdUI->Enable(TRUE);
		#endif
	}

	afx_msg void CMainWindow::__openUrl_faq__(){
		// opens the "Frequently Asked Questions" page in a new Tab
		OpenApplicationPresentationWebPage(_T("FAQ"),_T("faq.html"));
	}

	afx_msg void CMainWindow::OpenUrl_Donate(){
		// opens the "Report a bug" page in a new Tab
		OpenApplicationPresentationWebPage( _T("Donate"), _T("donate.html") );
	}

	afx_msg void CMainWindow::__openUrl_reportBug__(){
		// opens the "Report a bug" page in a new Tab
		OpenApplicationPresentationWebPage(_T("Report a bug"),_T("faq_reportBug.html"));
	}

	afx_msg void CMainWindow::__openUrl_repository__(){
		// opens the repository webpage in a new Tab
		OpenRepositoryWebPage( _T("Repository"), _T("") );
	}

	afx_msg void CMainWindow::__openUrl_tutorials__(){
		// opens the webpage with development tutorials in a new Tab
		OpenApplicationPresentationWebPage(_T("Tutorials"),_T("tutorials.html"));
	}

	afx_msg void CMainWindow::OpenYahelRepositoryUrl(){
		// opens YAHEL repository webpage in a new Tab
		OpenWebPage(
			_T("GitHub"),
			GITHUB_HTTPS_NAME _T("/tomas-nestorovic/YAHEL")
		);
	}

	afx_msg void CMainWindow::ShowYahelAbout(){
		Yahel::IInstance::ShowModalAboutDialog( app.m_pMainWnd->m_hWnd );
	}

	afx_msg void CMainWindow::__openUrl_credits__(){
		// opens the "Credits" page in a new Tab
		OpenApplicationPresentationWebPage(_T("Credits"),_T("credits.html"));
	}

