#include "stdafx.h"

	CWebPageView::THistory::TPage::TPage(LPCTSTR _url)
		// ctor
		: url(_url) , iScrollY(0) , newer(nullptr) , older(nullptr) {
	}



	CWebPageView::THistory::THistory(LPCTSTR defaultUrl)
		// ctor
		: initialPage(defaultUrl)
		, currentPage(&initialPage) {
	}

	CWebPageView::THistory::~THistory(){
		// dtor
		currentPage=&initialPage;
		DestroyNewerPages();
	}

	void CWebPageView::THistory::DestroyNewerPages() const{
		// removes from History Pages that are newer than CurrentPage
		for( TPage *&rNewer=currentPage->newer; TPage *const tmp=rNewer; delete tmp )
			rNewer=rNewer->newer;
	}









	CWebPageView::CWebPageView(LPCTSTR url)
		// ctor
		: tab( IDR_WEBPAGE, IDR_WEBPAGE, ID_CYLINDER, nullptr, this )
		, history(url)
		, navigationToLabel(false) {
	}

	BEGIN_MESSAGE_MAP(CWebPageView,CHtmlView)
		ON_WM_DESTROY()
		ON_COMMAND(ID_WEB_NAVIGATE_BACK,__navigateBack__)
			ON_UPDATE_COMMAND_UI(ID_WEB_NAVIGATE_BACK,__navigateBack_updateUI__)
		ON_COMMAND(ID_WEB_NAVIGATE_FORTH,__navigateForward__)
			ON_UPDATE_COMMAND_UI(ID_WEB_NAVIGATE_FORTH,__navigateForward_updateUI__)
		ON_COMMAND(ID_FILE_PRINT,OnFilePrint)
		ON_COMMAND(ID_WEB_DEFAULT_BROWSER,__openCurrentPageInDefaultBrowser__)
	END_MESSAGE_MAP()









	BOOL CWebPageView::Create(LPCTSTR,LPCTSTR,DWORD dwStyle,const RECT &rect,CWnd *pParentWnd,UINT nID,CCreateContext *){
		// True <=> window created successfully, otherwise False
		// - base
		if (!__super::Create(nullptr,nullptr,dwStyle,rect,pParentWnd,nID,nullptr)) // (CCreateContext *)==nullptr => doesn't allow current Image to get crack on MFC commands
			return FALSE;
		// - update the "Default browser" Toolbar button
		if (tab.toolbar){
			// . extract icon of local application associated with opening web pages
			if (!defaultBrowserIcon.m_hObject){ // not yet extracted?
				WCHAR defBrowserPath[MAX_PATH];
				DWORD n=ARRAYSIZE(defBrowserPath);
				if (SUCCEEDED(::AssocQueryStringW( ASSOCF_INIT_IGNOREUNKNOWN, ASSOCSTR_EXECUTABLE, L".html", L"open", defBrowserPath, &n ))){
					HICON hIcon;
					if (::ExtractIconExW( defBrowserPath, 0, nullptr, &hIcon, 1 )){
						// draw the Icon into a usable bitmap
						CClientDC dcScreen(nullptr);
						CDC dcMem;
							dcMem.CreateCompatibleDC( &dcScreen );
						int w,h;
						ImageList_GetIconSize(
							(HIMAGELIST)tab.toolbar.GetToolBarCtrl().SendMessage(TB_GETIMAGELIST),
							&w, &h
						);
						defaultBrowserIcon.CreateCompatibleBitmap( &dcScreen, w, h );
						const HGDIOBJ hBmp0=::SelectObject( dcMem, defaultBrowserIcon );
							::DrawIconEx( dcMem, 0,0, hIcon, w, h, 0, nullptr, DI_NORMAL );
						::SelectObject( dcMem, hBmp0 );
						::DestroyIcon(hIcon);
					}
				}
			}
			// . update the Toolbar
			if (defaultBrowserIcon.m_hObject) // is there a default browser?
				tab.toolbar.GetToolBarCtrl().SendMessage(
					TB_CHANGEBITMAP,
					ID_WEB_DEFAULT_BROWSER,
					tab.toolbar.GetToolBarCtrl().AddBitmap( 1, &defaultBrowserIcon )
				);
		}
		// - performing initial update as the method isn't called automatically during OnCreate
		OnInitialUpdate(); // calls SetScrollSizes via OnUpdate
		// - navigating to and displaying CurrentPage
		Navigate(history.currentPage->url);
		return TRUE;
	}

	void CWebPageView::OnBeforeNavigate2(LPCTSTR lpszURL,DWORD nFlags,LPCTSTR lpszTargetFrameName,CByteArray &baPostedData,LPCTSTR lpszHeaders,BOOL *pbCancel){
		// request to navigate to new Page with given URL
		// - base
		__super::OnBeforeNavigate2(lpszURL,nFlags,lpszTargetFrameName,baPostedData,lpszHeaders,pbCancel);
		// - ignoring refreshing of CurrentPage
		if (history.currentPage->url==lpszURL)
			return;
		// - ignoring navigation to a label in CurrentPage
		const LPCTSTR pCurrLabel=_tcsrchr(history.currentPage->url,'#');
		const int nCurrPageChars= pCurrLabel!=nullptr ? pCurrLabel-history.currentPage->url : ::lstrlen(history.currentPage->url);
		const LPCTSTR pNewLabel=_tcsrchr(lpszURL,'#');
		const int nNewPageChars= pNewLabel!=nullptr ? pNewLabel-lpszURL : ::lstrlen(lpszURL);
		if (nCurrPageChars==nNewPageChars)
			if ( navigationToLabel=!::strncmp(history.currentPage->url,lpszURL,nNewPageChars) )
				return;
		// - adding new Page and making it Current
		__saveCurrentPageScrollPosition__();
		history.DestroyNewerPages(); // destroying newer History (if any)
		( history.currentPage->newer=new THistory::TPage(lpszURL) )->older=history.currentPage;
		history.currentPage=history.currentPage->newer;
	}

	void CWebPageView::OnDocumentComplete(LPCTSTR strURL){
		// Page with given URL loaded
		// - base
		__super::OnDocumentComplete(strURL);
		// - recovering scroll position
		if (!navigationToLabel)
			if (const CComPtr<IDispatch> disp=GetHtmlDocument()){
				CComPtr<IHTMLDocument2> doc;
				if (SUCCEEDED(disp->QueryInterface(&doc))){
					CComPtr<IHTMLElement> pBody;
					if (SUCCEEDED(doc->get_body(&pBody)) && pBody){
						CComPtr<IHTMLElement2> pBody2;
						if (SUCCEEDED(pBody->QueryInterface(&pBody2)))
							pBody2->put_scrollTop(history.currentPage->iScrollY);
					}
				}
			}
		navigationToLabel=false;
	}

	void CWebPageView::PostNcDestroy(){
		// self-destruction
		//nop (View destroyed by its owner)
	}

	void CWebPageView::__saveCurrentPageScrollPosition__() const{
		// saves CurrentPage scroll position
		if (const CComPtr<IDispatch> disp=GetHtmlDocument()){
			CComPtr<IHTMLDocument2> doc;
			if (SUCCEEDED(disp->QueryInterface(&doc))){
				CComPtr<IHTMLElement> pBody;
				if (SUCCEEDED(doc->get_body(&pBody)) && pBody){
					CComPtr<IHTMLElement2> pBody2;
					if (SUCCEEDED(pBody->QueryInterface(&pBody2)))
						pBody2->get_scrollTop(&history.currentPage->iScrollY);
				}
			}
		}
	}

	afx_msg void CWebPageView::OnDestroy(){
		// window destroyed
		// - saving CurrentPage scroll position for later
		__saveCurrentPageScrollPosition__();
		// - base (continuing with destruction)
		__super::OnDestroy();
		// - bugfix to missing release of COM object in MFC
		if (m_pBrowserApp){ // if bug not yet fixed
			#if _MFC_VER>=0x0A00
				m_pBrowserApp.Detach()->Release();
			#else
				m_pBrowserApp->Release();
				m_pBrowserApp=nullptr;
			#endif
		}
	}

	afx_msg void CWebPageView::__navigateBack__(){
		// navigates to Historically older Page
		__saveCurrentPageScrollPosition__();
		if (THistory::TPage *pOlder=history.currentPage->older)
			Navigate(( history.currentPage=pOlder )->url);
	}
	afx_msg void CWebPageView::__navigateBack_updateUI__(CCmdUI *pCmdUI){
		// projecting existence of Historically older Page into UI
		pCmdUI->Enable( history.currentPage->older!=&history.initialPage );
	}

	afx_msg void CWebPageView::__navigateForward__(){
		// navigates to Historically newer Page
		__saveCurrentPageScrollPosition__();
		if (THistory::TPage *pNewer=history.currentPage->newer)
			Navigate(( history.currentPage=pNewer )->url);
	}
	afx_msg void CWebPageView::__navigateForward_updateUI__(CCmdUI *pCmdUI){
		// projecting existence of Historically newer Page into UI
		pCmdUI->Enable(history.currentPage->newer!=nullptr);
	}

	afx_msg void CWebPageView::__openCurrentPageInDefaultBrowser__(){
		// opens CurrentPage in user's default browser
		Utils::NavigateToUrlInDefaultBrowser(history.currentPage->url);
	}
