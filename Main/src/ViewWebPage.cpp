#include "stdafx.h"

	CWebPageView::THistory::TPage::TPage(LPCTSTR _url)
		// ctor
		: url(_url) , iScrollY(0) , newer(nullptr) , older(nullptr) {
	}



	CWebPageView::THistory::THistory(LPCTSTR defaultUrl)
		// ctor
		: currentPage(new TPage(defaultUrl)) {
	}

	CWebPageView::THistory::~THistory(){
		// dtor
		__destroyNewerPages__();
		while (const TPage *const tmp=currentPage)
			currentPage=currentPage->older, delete tmp;
	}

	void CWebPageView::THistory::__destroyNewerPages__() const{
		// removes from History Pages that are newer than CurrentPage
		for( TPage *&rNewer=currentPage->newer; TPage *const tmp=rNewer; delete tmp )
			rNewer=rNewer->newer;
	}









	#define DOS tab.dos

	CWebPageView::CWebPageView(LPCTSTR url)
		// ctor
		: history(url) , tab(IDR_WEBPAGE,ID_CYLINDER,nullptr,this)
		, navigationToLabel(false) {
	}

	BEGIN_MESSAGE_MAP(CWebPageView,CHtmlView)
		ON_WM_DESTROY()
		ON_COMMAND(ID_WEB_NAVIGATE_BACK,__navigateBack__)
			ON_UPDATE_COMMAND_UI(ID_WEB_NAVIGATE_BACK,__navigateBack_updateUI__)
		ON_COMMAND(ID_WEB_NAVIGATE_FORTH,__navigateForward__)
			ON_UPDATE_COMMAND_UI(ID_WEB_NAVIGATE_FORTH,__navigateForward_updateUI__)
		ON_COMMAND(ID_WEB_DEFAULT_BROWSER,__openCurrentPageInDefaultBrowser__)
	END_MESSAGE_MAP()









	BOOL CWebPageView::Create(LPCTSTR,LPCTSTR,DWORD dwStyle,const RECT &rect,CWnd *pParentWnd,UINT nID,CCreateContext *){
		// True <=> window created successfully, otherwise False
		// - base
		if (!CHtmlView::Create(nullptr,nullptr,dwStyle,rect,pParentWnd,nID,nullptr)) // (CCreateContext *)==nullptr => doesn't allow current Image to get crack on MFC commands
			return FALSE;
		// - performing initial update as the method isn't called automatically during OnCreate
		OnInitialUpdate(); // calls SetScrollSizes via OnUpdate
		// - navigating to and displaying CurrentPage
		Navigate(history.currentPage->url);
		return TRUE;
	}

	void CWebPageView::OnBeforeNavigate2(LPCTSTR lpszURL,DWORD nFlags,LPCTSTR lpszTargetFrameName,CByteArray &baPostedData,LPCTSTR lpszHeaders,BOOL *pbCancel){
		// request to navigate to new Page with given URL
		// - base
		CHtmlView::OnBeforeNavigate2(lpszURL,nFlags,lpszTargetFrameName,baPostedData,lpszHeaders,pbCancel);
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
		history.__destroyNewerPages__(); // destroying newer History (if any)
		( history.currentPage->newer=new THistory::TPage(lpszURL) )->older=history.currentPage;
		history.currentPage=history.currentPage->newer;
	}

	void CWebPageView::OnDocumentComplete(LPCTSTR strURL){
		// Page with given URL loaded
		// - base
		CHtmlView::OnDocumentComplete(strURL);
		// - recovering scroll position
		if (!navigationToLabel)
			if (const LPDISPATCH disp=GetHtmlDocument()){
				IHTMLDocument2 *doc;
				if (SUCCEEDED(disp->QueryInterface(IID_IHTMLDocument2,(void**)&doc))){
					IHTMLElement *pBody;
					if (SUCCEEDED(doc->get_body(&pBody)) && pBody){
						IHTMLElement2 *pBody2;
						if (SUCCEEDED(pBody->QueryInterface(IID_IHTMLElement2,(void**)&pBody2))){
							pBody2->put_scrollTop(history.currentPage->iScrollY);
							pBody2->Release();
						}
						pBody->Release();
					}
					doc->Release();
				}
				disp->Release();
			}
		navigationToLabel=false;
	}

	void CWebPageView::PostNcDestroy(){
		// self-destruction
		//nop (View destroyed by its owner)
	}

	void CWebPageView::__saveCurrentPageScrollPosition__() const{
		// saves CurrentPage scroll position
		if (const LPDISPATCH disp=GetHtmlDocument()){
			IHTMLDocument2 *doc;
			if (SUCCEEDED(disp->QueryInterface(IID_IHTMLDocument2,(void**)&doc))){
				IHTMLElement *pBody;
				if (SUCCEEDED(doc->get_body(&pBody)) && pBody){
					IHTMLElement2 *pBody2;
					if (SUCCEEDED(pBody->QueryInterface(IID_IHTMLElement2,(void**)&pBody2))){
						pBody2->get_scrollTop(&history.currentPage->iScrollY);
						pBody2->Release();
					}
					pBody->Release();
				}
				doc->Release();
			}
			disp->Release();
		}
	}

	afx_msg void CWebPageView::OnDestroy(){
		// window destroyed
		// - saving CurrentPage scroll position for later
		__saveCurrentPageScrollPosition__();
		// - base (continuing with destruction)
		CHtmlView::OnDestroy();
	}

	afx_msg void CWebPageView::__navigateBack__(){
		// navigates to Historically older Page
		__saveCurrentPageScrollPosition__();
		if (THistory::TPage *pOlder=history.currentPage->older)
			Navigate(( history.currentPage=pOlder )->url);
	}
	afx_msg void CWebPageView::__navigateBack_updateUI__(CCmdUI *pCmdUI) const{
		// projecting existence of Historically older Page into UI
		pCmdUI->Enable(history.currentPage->older!=nullptr);
	}

	afx_msg void CWebPageView::__navigateForward__(){
		// navigates to Historically newer Page
		__saveCurrentPageScrollPosition__();
		if (THistory::TPage *pNewer=history.currentPage->newer)
			Navigate(( history.currentPage=pNewer )->url);
	}
	afx_msg void CWebPageView::__navigateForward_updateUI__(CCmdUI *pCmdUI) const{
		// projecting existence of Historically newer Page into UI
		pCmdUI->Enable(history.currentPage->newer!=nullptr);
	}

	afx_msg void CWebPageView::__openCurrentPageInDefaultBrowser__() const{
		// opens CurrentPage in user's default browser
		Utils::NavigateToUrlInDefaultBrowser(history.currentPage->url);
	}
