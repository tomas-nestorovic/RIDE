#include "stdafx.h"

	CMainWindow::TDynMenu::TDynMenu(UINT nResId)
		// ctor
		: hMenu( ::LoadMenu(AfxGetInstanceHandle(),(LPCTSTR)nResId) )
		, hAccel( ::LoadAccelerators(AfxGetInstanceHandle(),(LPCTSTR)nResId) ) {
	}
	CMainWindow::TDynMenu::~TDynMenu(){
		// dtor
		::DestroyAcceleratorTable(hAccel);
		::DestroyMenu(hMenu);
	}

	void CMainWindow::TDynMenu::__show__(UINT position) const{
		// shows this Menu in MainWindow's menu
		if (hMenu){ // may not if the MainWindow is being closed
			TCHAR buf[30];
			::GetMenuString(hMenu,0,buf,sizeof(buf)/sizeof(TCHAR),MF_BYPOSITION);
			app.m_pMainWnd->GetMenu()->InsertMenu(	position,
													MF_BYPOSITION | MF_POPUP,
													(UINT)::GetSubMenu(hMenu,0),
													buf
												);
			app.m_pMainWnd->DrawMenuBar();
		}
	}
	void CMainWindow::TDynMenu::__hide__() const{
		// removes this Menu from MainWindow's menu
		if (hMenu && app.m_pMainWnd){ // may not if the MainWindow is being closed
			app.m_pMainWnd->GetMenu()->RemoveMenu( (UINT)::GetSubMenu(hMenu,0), MF_BYCOMMAND|MF_POPUP );
			app.m_pMainWnd->DrawMenuBar();
		}
	}








	CMainWindow::CDockableToolBar::CDockableToolBar(UINT nResId,UINT id){
		// ctor
		if (nResId){
			Create( app.m_pMainWnd, WS_CHILD|CBRS_TOP|CBRS_TOOLTIPS, id );
			LoadToolBar(nResId);
			EnableDocking(CBRS_ALIGN_TOP);
		}
	}

	void CMainWindow::CDockableToolBar::__show__(const CToolBar &rDockNextTo){
		// shows this ToolBar docked immediately next to the specified existing one
		if (m_hWnd && rDockNextTo.m_hWnd){ // only if both ToolBars exist (may not if the MainWindow is being closed)
			RECT r;
			rDockNextTo.GetWindowRect(&r);
			::OffsetRect(&r,1,0);
			CMainWindow *const pMainWindow=(CMainWindow *)app.m_pMainWnd;
			pMainWindow->DockControlBar(this,AFX_IDW_DOCKBAR_TOP,&r);
			pMainWindow->ShowControlBar(this,TRUE,FALSE);
		}
	}

	void CMainWindow::CDockableToolBar::__hide__(){
		// hides this Toolbar
		if (m_hWnd) // this ToolBar is destroyed automatically when the MainWindow is being closed
			( (CMainWindow *)app.m_pMainWnd )->ShowControlBar(this,FALSE,FALSE);
	}

	void CMainWindow::CDockableToolBar::OnUpdateCmdUI(CFrameWnd* pTarget,BOOL bDisableIfNoHndler){
		return __super::OnUpdateCmdUI( pTarget, FALSE ); // False = don't search the message map for corresponding command handlers
	}










	void CMainWindow::__resetStatusBar__(){
		// resets the MainWindow's StatusBar
		CStatusBar &rsb=( (CMainWindow *)app.m_pMainWnd )->statusBar;
		if (rsb.m_hWnd){
			static const UINT Indicator=ID_SEPARATOR;
			rsb.SetIndicators(&Indicator,1);
		}
	}
	void CMainWindow::__setStatusBarText__(LPCTSTR text){
		// sets the MainWindow's StatusBar main part to the specified Text
		CStatusBar &rsb=( (CMainWindow *)app.m_pMainWnd )->statusBar;
		if (rsb.m_hWnd)
			rsb.SetPaneText(0,text);
	}

	








	BEGIN_MESSAGE_MAP(CMainWindow,CFrameWnd)
		ON_WM_CREATE()
		ON_WM_INITMENU()
		ON_WM_DROPFILES()
		ON_WM_LBUTTONDBLCLK()
		ON_UPDATE_COMMAND_UI_RANGE(ID_FILE_SAVE,ID_FILE_SAVE_AS,__imageOperation_updateUI__)
		ON_UPDATE_COMMAND_UI_RANGE(ID_IMAGE_DUMP,ID_IMAGE_SETTINGS,__imageOperation_updateUI__)
		ON_COMMAND(ID_TDI_SWITCH,__switchToNextTab__)
		ON_COMMAND(ID_TDI_SWITCH_BACK,__switchToPrevTab__)
		ON_COMMAND(ID_FILE_CLOSE,__closeCurrentTab__)
			ON_UPDATE_COMMAND_UI(ID_FILE_CLOSE,__closeCurrentTab_updateUI__)
		ON_COMMAND(ID_RECOGNIZE,__changeAutomaticDiskRecognitionOrder__)
		ON_COMMAND(ID_HELP_WHATSNEW,__openUrl_whatsNew__)
		ON_COMMAND(ID_APP_UPDATE,__openUrl_checkForUpdates__)
		ON_COMMAND(ID_HELP_FAQ,__openUrl_faq__)
		ON_COMMAND(ID_HELP_REPORT_BUG,__openUrl_reportBug__)
		ON_COMMAND(ID_HELP_REPOSITORY,__openUrl_repository__)
		ON_COMMAND(ID_HELP_TUTORIALS,__openUrl_tutorials__)
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
			if (const PCDos dos=t->dos){
				// focused DOS (e.g. a ZX Tape)
				if (::TranslateAccelerator(m_hWnd,dos->menu.hAccel,pMsg)) return TRUE;
			}else if (const PCImage image=CImage::GetActive())
				// active DOS (e.g. MDOS when a WebPage is focused)
				if (::TranslateAccelerator(m_hWnd,image->dos->menu.hAccel,pMsg)) return TRUE;
		}
		return CFrameWnd::PreTranslateMessage(pMsg); // base
	}

	afx_msg int CMainWindow::OnCreate(LPCREATESTRUCT lpcs){
		// window created
		// - base
		if (CFrameWnd::OnCreate(lpcs)==-1) return -1;
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

	afx_msg void CMainWindow::OnDropFiles(HDROP hDrop){
		// a File from Windows Explorer dropped on the client area
		extern CDos::PCProperties manuallyForceDos;
		manuallyForceDos=nullptr; // Null = use automatic recognition
		__super::OnDropFiles(hDrop);
	}

	afx_msg void CMainWindow::OnLButtonDblClk(UINT,CPoint){
		// mouse double-clicked on the client area
		if (!pTdi->__getCurrentTab__())
			app.__openImage__(); // double-clicking on the client area is interpreted as wanting to open an Image
	}

	afx_msg void CMainWindow::OnInitMenu(CMenu *menu){
		// clicked somewhere in the main menu
		SetFocus(); // to immediately carry out actions that depend on focus
		CFrameWnd::OnInitMenu(menu);
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
		if (pTdi->__getCurrentTab__()->dos) // if current Tab relates to a DOS ...
			CTdiTemplate::pSingleInstance->__closeDocument__(); // ... closing the corresponding Image (and thus the DOS and all its Tabs and Views)
		else
			CTdiCtrl::RemoveCurrentTab( pTdi->m_hWnd ); // ... otherwise closing just the Tab (e.g. a WebPageView that usually isn't associated with any DOS)
	}

	afx_msg void CMainWindow::__closeCurrentTab_updateUI__(CCmdUI *pCmdUI){
		// projecting possibility to close current Tab into UI
		pCmdUI->Enable( pTdi->__getCurrentTab__()!=nullptr );
	}



	static void WINAPI __onWebPageClosing__(CTdiCtrl::TTab::PContent tab){
		delete ((CMainWindow::CTdiView::PTab)tab)->view;
	}
	void CMainWindow::OpenWebPage(LPCTSTR tabCaption,LPCTSTR url){
		// opens specified URL in a new Tab
		CWebPageView *const webView=new CWebPageView(url);
		CTdiCtrl::AddTabLast( TDI_HWND, tabCaption, &webView->tab, true, TDI_TAB_CANCLOSE_ALWAYS, __onWebPageClosing__ );
	}
	void CMainWindow::OpenApplicationPresentationWebPage(LPCTSTR tabCaption,LPCTSTR documentName){
		// in new Tab opens the specified Document from application's on-line presentation
		TCHAR url[MAX_PATH];
		OpenWebPage( tabCaption, Utils::GetApplicationOnlineHtmlDocumentUrl(documentName,url) );
	}

	afx_msg void CMainWindow::__openUrl_whatsNew__(){
		// opens the "What's New" page in a new Tab
		OpenApplicationPresentationWebPage(_T("Change log"),_T("changelog.html"));
	}

	#define VERSION_LATEST_WEB	_T("usingLatest.html")

	static UINT AFX_CDECL __checkApplicationRecency_thread__(PVOID _pCancelableAction){
		// checks if this instance of application is the latest by comparing it against the on-line information; returns ERROR_SUCCESS (on-line information was downloaded and this instance is up-to-date), ERROR_EVT_VERSION_TOO_OLD (on-line information was downloaded but this instance is out-of-date), or other error
		const PBackgroundActionCancelableBase pAction=(PBackgroundActionCancelableBase)_pCancelableAction;
		pAction->SetProgressTarget(5); // 5 = see number of steps below
		HINTERNET hSession=nullptr, hConnection=nullptr, hRequest=nullptr;
		// - Step 1: opening a new Session
		hSession=::InternetOpen( APP_IDENTIFIER, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0 );
		if (hSession==nullptr){
quitWithErr:const DWORD err=::GetLastError();
			if (hRequest!=nullptr)
				::InternetCloseHandle(hRequest);
			if (hConnection!=nullptr)
				::InternetCloseHandle(hConnection);
			if (hSession!=nullptr)
				::InternetCloseHandle(hSession);
			return pAction->TerminateWithError(err);
		}
		if (!pAction->CanContinue()) return ERROR_CANCELLED;
		pAction->UpdateProgress(1);
		// - Step 2: connecting to the repository server
		hConnection=::InternetConnect( hSession, GITHUB_API_SERVER_NAME, INTERNET_DEFAULT_HTTPS_PORT, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0 );
		if (hConnection==nullptr)
			goto quitWithErr;
		if (!pAction->CanContinue()) return ERROR_CANCELLED;
		pAction->UpdateProgress(2);
		// - Step 3: creating a new Request to the server
		hRequest=::HttpOpenRequest(	hConnection, _T("GET"), _T("/repos/tomas-nestorovic/RIDE/releases/latest"),
									nullptr, nullptr, nullptr,
									INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_NO_CALLBACK,
									0
								);
		if (hRequest==nullptr)
			goto quitWithErr;
		if (!pAction->CanContinue()) return ERROR_CANCELLED;
		pAction->UpdateProgress(3);
		// - Step 4: sending the Request
		if (!::HttpSendRequest( hRequest, "User-Agent:RIDE", -1, nullptr, 0 ))
			goto quitWithErr;
		if (!pAction->CanContinue()) return ERROR_CANCELLED;
		pAction->UpdateProgress(4);
		// - Step 5: receiving the response
		char buffer[16384];
		DWORD nBytesRead;
		if (!::InternetReadFile( hRequest, buffer, sizeof(buffer), &nBytesRead ))
			goto quitWithErr;
		buffer[nBytesRead]='\0';
		if (!pAction->CanContinue()) return ERROR_CANCELLED;
		pAction->UpdateProgress(5);
		// - analysing the obtained information (comparing it against this instance version)
		if (const PCHAR githubTagName=::strstr(buffer,GITHUB_VERSION_TAG_NAME))
			if (PCHAR r=::strchr(githubTagName+sizeof(GITHUB_VERSION_TAG_NAME),'\"')){ // "R"emote tag
				buffer[nBytesRead]='\"'; // guaranteeing that closing quote is always found
				*::strchr( ++r, '\"' )='\0'; // "+1" = skipping the opening quote; replacing the closing quote with the Null character
				const TCHAR *t=APP_VERSION; // "T"his tag
				do{
					if (::isspace(*t))
						t++; // ignoring any whitespaces in "T"his tag
					else if (*r++!=*t++)
						return ERROR_EVT_VERSION_TOO_OLD; // the app is outdated
				} while (*r/*&&*t*/); // commented out as redundant (any differences already caught above)
				return ERROR_SUCCESS; // the app is up-to-date
			}
		return ERROR_DS_SERVER_DOWN;
	}
	afx_msg void CMainWindow::__openUrl_checkForUpdates__(){
		// checks if this instance of application is the latest by comparing it against the on-line information; opens either "You are using the latest version" web page, or the "You are using out-of-date version" web page
		switch (const TStdWinError err=	CBackgroundActionCancelable(
											__checkApplicationRecency_thread__,
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

	afx_msg void CMainWindow::__openUrl_faq__(){
		// opens the "Frequently Asked Questions" page in a new Tab
		OpenApplicationPresentationWebPage(_T("FAQ"),_T("faq.html"));
	}

	afx_msg void CMainWindow::__openUrl_reportBug__(){
		// opens the "Report a bug" page in a new Tab
		OpenApplicationPresentationWebPage(_T("Report a bug"),_T("faq_reportBug.html"));
	}

	afx_msg void CMainWindow::__openUrl_repository__(){
		// opens the repository webpage in a new Tab
		OpenWebPage( _T("Repository"), GITHUB_HTTPS_NAME _T("/tomas-nestorovic/RIDE") );
	}

	afx_msg void CMainWindow::__openUrl_tutorials__(){
		// opens the webpage with development tutorials in a new Tab
		OpenApplicationPresentationWebPage(_T("Tutorials"),_T("tutorials.html"));
	}

	afx_msg void CMainWindow::__openUrl_credits__(){
		// opens the "Credits" page in a new Tab
		OpenApplicationPresentationWebPage(_T("Credits"),_T("credits.html"));
	}

