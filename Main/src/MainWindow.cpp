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











	void CMainWindow::__resetStatusBar__(){
		// resets the MainWindow's StatusBar
		static const UINT Indicator=ID_SEPARATOR;
		( (CMainWindow *)app.m_pMainWnd )->statusBar.SetIndicators(&Indicator,1);
	}
	void CMainWindow::__setStatusBarText__(LPCTSTR text){
		// sets the MainWindow's StatusBar main part to the specified Text
		( (CMainWindow *)app.m_pMainWnd )->statusBar.SetPaneText(0,text);
	}

	








	BEGIN_MESSAGE_MAP(CMainWindow,CFrameWnd)
		ON_WM_CREATE()
		ON_WM_INITMENU()
		ON_WM_DROPFILES()
		ON_UPDATE_COMMAND_UI_RANGE(ID_FILE_SAVE,ID_FILE_SAVE_AS,__imageOperation_updateUI__)
		ON_UPDATE_COMMAND_UI_RANGE(ID_IMAGE_DUMP,ID_IMAGE_PROTECT,__imageOperation_updateUI__)
		ON_COMMAND(ID_TDI_SWITCH,__switchToNextTab__)
		ON_COMMAND(ID_TDI_SWITCH_BACK,__switchToPrevTab__)
		ON_COMMAND(ID_FILE_CLOSE,__closeCurrentTab__)
			ON_UPDATE_COMMAND_UI(ID_FILE_CLOSE,__closeCurrentTab_updateUI__)
		ON_COMMAND(ID_RECOGNIZE,__changeAutomaticDiskRecognitionOrder__)
		ON_COMMAND(ID_HELP_WHATSNEW,__openUrl_whatsNew__)
		ON_COMMAND(ID_APP_UPDATE,__openUrl_checkForUpdates__)
		ON_COMMAND(ID_HELP_FAQ,__openUrl_faq__)
		ON_COMMAND(ID_CREDITS,__openUrl_credits__)
	END_MESSAGE_MAP()











	BOOL CMainWindow::PreCreateWindow(CREATESTRUCT &cs){
		// adjusting the instantiation
		if (!CFrameWnd::PreCreateWindow(cs)) return FALSE;
		cs.dwExStyle&=~WS_EX_CLIENTEDGE;
		return TRUE;
	}

	BOOL CMainWindow::PreTranslateMessage(PMSG pMsg){
		// pre-processing the Message
		if (const CTdiView::TTab *const t=pTdi->pCurrentTab){
			// . FileManager's Editor receives all messages and none must be pre-translated
			if (CFileManagerView::CEditorBase::pSingleShown)
				return FALSE;
			// . PropertyGrid's Editor receives all messages and none must be pre-translated
			if (CBootView::pCurrentlyShown && CBootView::pCurrentlyShown->__isValueBeingEditedInPropertyGrid__())
				return FALSE;
			// . File HexaPreview receives all messages and none must be pre-translated (but only if focused)
			if (CDos::CHexaPreview::pSingleInstance)
				if (::GetActiveWindow()==CDos::CHexaPreview::pSingleInstance->m_hWnd) // must be focused
					return FALSE;
			// . pretranslating the Message by accelerators of current TDI Tab
			if (::TranslateAccelerator(m_hWnd,t->menu.hAccel,pMsg))
				return TRUE;
			// . pretranslating the Message by accelerators of currently focused DOS (if any; e.g. WebPageView usually isn't associated with any DOS)
			if (t->dos)
				if (::TranslateAccelerator(m_hWnd,t->dos->menu.hAccel,pMsg)) return TRUE;
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
		pTdi->Create( NULL, NULL, AFX_WS_DEFAULT_VIEW&~WS_BORDER, CRect(), this, AFX_IDW_PANE_FIRST );
		CTdiCtrl::SubclassWnd(
			app.m_hInstance,
			pTdi->m_hWnd,
			&CTdiCtrl::TParams( pTdi, CTdiView::__fnShowContent__, CTdiView::__fnHideContent__, CTdiView::__fnRepaintContent__, CTdiView::__fnGetHwnd__ )
		);
		pTdi->SendMessage( WM_SETFONT, (WPARAM)CRideFont(FONT_MS_SANS_SERIF,90,false,true).Detach(), TRUE );
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

	afx_msg void CMainWindow::OnInitMenu(CMenu *menu){
		// clicked somewhere in the main menu
		SetFocus(); // to immediately carry out actions that depend on focus
		CFrameWnd::OnInitMenu(menu);
	}

	afx_msg void CMainWindow::__imageOperation_updateUI__(CCmdUI *pCmdUI){
		// projecting Image operability into UI
		pCmdUI->Enable( GetActiveDocument()!=NULL );
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
		if (((CMainWindow *)app.m_pMainWnd)->pTdi->__getCurrentTab__()->dos) // if current Tab relates to a DOS ...
			CTdiTemplate::pSingleInstance->__closeDocument__(); // ... closing the corresponding Image (and thus the DOS and all its Tabs and Views)
		else
			CTdiCtrl::RemoveCurrentTab( pTdi->m_hWnd ); // ... otherwise closing just the Tab (e.g. a WebPageView that usually isn't associated with any DOS)
	}

	afx_msg void CMainWindow::__closeCurrentTab_updateUI__(CCmdUI *pCmdUI) const{
		// projecting possibility to close current Tab into UI
		pCmdUI->Enable( ((CMainWindow *)app.m_pMainWnd)->pTdi->__getCurrentTab__()!=NULL );
	}



	static bool WINAPI __onWebPageClosing__(LPCVOID tab){
		delete ((CMainWindow::CTdiView::PTab)tab)->view;
		return true;
	}
	void CMainWindow::OpenWebPage(LPCTSTR tabCaption,LPCTSTR url){
		// opens specified URL in a new Tab
		CWebPageView *const webView=new CWebPageView(url);
		CTdiCtrl::AddTabLast( TDI_HWND, tabCaption, &webView->tab, true, true, __onWebPageClosing__ );
	}
	void CMainWindow::OpenApplicationPresentationWebPage(LPCTSTR tabCaption,LPCTSTR documentName){
		// in new Tab opens the specified Document from application's on-line presentation
		TCHAR url[MAX_PATH];
		OpenWebPage( tabCaption, TUtils::GetApplicationOnlineDocumentUrl(documentName,url) );
	}

	afx_msg void CMainWindow::__openUrl_whatsNew__(){
		// opens the "What's New" page in a new Tab
		OpenApplicationPresentationWebPage(_T("Change log"),_T("changelog.html"));
	}

	#define VERSION_LATEST_WEB	_T("usingLatest.html")

	class CHtmlStatus sealed:public CHtmlView{
		void OnDownloadBegin() override{
			CHtmlView::OnDownloadBegin();
			connected=true;
		}
		void OnTitleChange(LPCTSTR lpszText) override{
			CHtmlView::OnTitleChange(lpszText);
			::lstrcpy(latestVersion,lpszText);
		}
		void OnDownloadComplete() override{
			CHtmlView::OnDownloadComplete();
			downloaded=true;
		}
		void PostNcDestroy() override{
		}
	public:
		bool connected,downloaded;
		TCHAR latestVersion[MAX_PATH];
		CHtmlStatus() // ctor
			: connected(false) , downloaded(false) {
			*latestVersion='\0';
			Create( NULL, NULL, WS_CHILD, RECT(), app.m_pMainWnd, 0 );
			//OnInitialUpdate();
		}
	};
	static UINT AFX_CDECL __checkApplicationRecency_thread__(PVOID _pCancelableAction){
		// checks if this instance of application is the latest by comparing it against the on-line information; returns ERROR_SUCCESS (on-line information was downloaded and this instance is up-to-date), ERROR_EVT_VERSION_TOO_OLD (on-line information was downloaded but this instance is out-of-date), or other error
		const TBackgroundActionCancelable *const pAction=(TBackgroundActionCancelable *)_pCancelableAction;
		CHtmlStatus *const phs=(CHtmlStatus *)pAction->fnParams;
		// - step 1) connecting to the server
		TCHAR buf[MAX_PATH];
		phs->Navigate( TUtils::GetApplicationOnlineDocumentUrl(VERSION_LATEST_WEB,buf) );
		while (!phs->connected){
			if (!pAction->bContinue) return ERROR_CANCELLED;
			::Sleep(100);
		}
		pAction->UpdateProgress(1);
		// - step 2) downloading the information on latest version
		while (!phs->downloaded){
			if (!pAction->bContinue) return ERROR_CANCELLED;
			::Sleep(100);
		}
		pAction->UpdateProgress(2);
		// - analysing the obtained information (comparing it against this instance version)
		return	::lstrcmp(phs->latestVersion,APP_VERSION)
				? ERROR_EVT_VERSION_TOO_OLD
				: ERROR_SUCCESS;
	}
	afx_msg void CMainWindow::__openUrl_checkForUpdates__(){
		// checks if this instance of application is the latest by comparing it against the on-line information; opens either "You are using the latest version" web page, or the "You are using out-of-date version" web page
		CHtmlStatus hs;
		TBackgroundActionCancelable bac( __checkApplicationRecency_thread__, &hs );
		switch (const TStdWinError err=bac.CarryOut(2)){ // 2 = see number of steps in CheckApplicationRecency
			case ERROR_SUCCESS:
				return OpenApplicationPresentationWebPage(_T("Version"),VERSION_LATEST_WEB);
			case ERROR_EVT_VERSION_TOO_OLD:
				return OpenApplicationPresentationWebPage(_T("Version"),_T("usingOld.html"));
			default:
				return TUtils::Information(_T("Cannot retrieve current version"),err);
		}
	}

	afx_msg void CMainWindow::__openUrl_faq__(){
		// opens the "Frequently Asked Questions" page in a new Tab
		OpenApplicationPresentationWebPage(_T("FAQ"),_T("faq.html"));
	}

	afx_msg void CMainWindow::__openUrl_credits__(){
		// opens the "Credits" page in a new Tab
		OpenApplicationPresentationWebPage(_T("Credits"),_T("credits.html"));
	}

