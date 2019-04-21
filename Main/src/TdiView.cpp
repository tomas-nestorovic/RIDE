#include "stdafx.h"

	CMainWindow::CTdiView::TTab::TTab(UINT nResId,UINT nToolBarId,PDos _dos,PView _view)
		// ctor
		// - initialization
		: dos(_dos) , view(_view)
		// - creating the Menu
		, menu(nResId)
		// - creating the ToolBar (its displaying in CTdiView::ShowContent)
		, toolbar(nResId,nToolBarId) {
	}












	CMainWindow::CTdiView::CTdiView()
		// ctor
		: CCtrlView(WC_TABCONTROL,AFX_WS_DEFAULT_VIEW & ~WS_BORDER)
		, pCurrentTab(nullptr) {
	}










	// True <=> the Tab is part of a DOS (e.g. a WebPage is usually not part of any DOS), otherwise False
	#define IS_TAB_PART_OF_DOS(pTab)	((pTab)->dos!=nullptr)

	#define MENU_POSITION_DOS		1
	#define MENU_POSITION_VIEW		2

	void WINAPI CMainWindow::CTdiView::__fnShowContent__(PVOID pTdi,LPCVOID pTab){
		// shows the content of the Tab
		const PTab tab= ((CTdiView *)pTdi)->pCurrentTab = (PTab)pTab;
		const PView view=tab->view;
		// - showing the Menus associated with the DOS and View 
		if (IS_TAB_PART_OF_DOS(tab)){ // the Tab is part of a DOS (e.g. a WebPage is usually not part of any DOS)
			CImage::__getActive__()->dos->menu.__show__(MENU_POSITION_DOS);
			tab->menu.__show__(MENU_POSITION_VIEW);
		}else
			tab->menu.__show__(MENU_POSITION_DOS); // showing the View's Menu at the DOS's position
		// - showing Image's ToolBar (guaranteed that the Toolbar always exists)
		CMainWindow *const pMainWindow=(CMainWindow *)app.m_pMainWnd;
		if (IS_TAB_PART_OF_DOS(tab)){ // the Tab is part of a DOS (e.g. a WebPage is usually not part of any DOS)
			tab->dos->image->toolbar.__show__(pMainWindow->toolbar);
		}
		// - showing the Tab's ToolBar (e.g. the FileManager's ToolBar)
		if (IS_TAB_PART_OF_DOS(tab)) // the Tab is part of a DOS (e.g. a WebPage is usually not part of any DOS)
			tab->toolbar.__show__( tab->dos->image->toolbar );
		else
			tab->toolbar.__show__( pMainWindow->toolbar );
		// - showing the associated View
		__setStatusBarText__(nullptr); // StatusBar without text
		RECT r;
		CTdiCtrl::GetCurrentTabContentRect( ( (CTdiView *)pTdi )->m_hWnd, &r );
		::OffsetRect( &r, r.left, r.top );
		CCreateContext cc;
			cc.m_pCurrentDoc=CImage::__getActive__();
		view->Create(	nullptr, nullptr,
						(AFX_WS_DEFAULT_VIEW & ~WS_BORDER) | WS_CLIPSIBLINGS,
						r, (CTdiView *)pTdi, 0,
						&cc
					);
		// - initializing the associated View
		pMainWindow->SetActiveView(view);
		pMainWindow->OnUpdateFrameTitle(TRUE); // just to be sure, forcing the document's name to the MainWindow's title
		view->SetFocus();
	}

	void WINAPI CMainWindow::CTdiView::__fnHideContent__(PVOID pTdi,LPCVOID pTab){
		// hides the content of the Tab
		const PTab tab=(PTab)pTab;
		// - hiding the associated View
		const PView view=tab->view;
		( (CMainWindow *)app.m_pMainWnd )->SetActiveView((CTdiView *)pTdi); // neccessary to call manually as otherwise no view will be active after closing the CurrentTab (and with no active documents, no command will be propagated to the document, etc.)
		((CTdiView *)pTdi)->pCurrentTab=nullptr;
		if (IS_TAB_PART_OF_DOS(tab)) // the Tab is part of a DOS (e.g. a WebPage is usually not part of any DOS)
			if (CImage *const image=CImage::__getActive__())
				image->RemoveView(view); // View added into the list when shown, see CCreateContext
		::DestroyWindow( view->m_hWnd );
		// - hiding the Tab's ToolBar (e.g. the FileManager's ToolBar)
		tab->toolbar.__hide__();
		// - hiding the Image's ToolBar (guaranteed that the Toolbar always exists)
		if (IS_TAB_PART_OF_DOS(tab))
			tab->dos->image->toolbar.__hide__();
		// - hiding the Menus associated with the DOS and View 
		tab->menu.__hide__();
		if (IS_TAB_PART_OF_DOS(tab))
			CImage::__getActive__()->dos->menu.__hide__();
		// - resetting the StatusBar
		__resetStatusBar__();
	}
	void WINAPI CMainWindow::CTdiView::__fnRepaintContent__(LPCVOID pTab){
		// redraws the Tab's associated View
		((PTab)pTab)->view->Invalidate(TRUE);
	}
	HWND WINAPI CMainWindow::CTdiView::__fnGetHwnd__(LPCVOID pTab){
		// returns the Tab's associated View handle
		return ((PTab)pTab)->view->m_hWnd;
	}

	void CMainWindow::CTdiView::__closeAllTabsOfFocusedDos__(){
		// closes all Tabs associated with the DOS in focus
		for( int i=TabCtrl_GetItemCount(m_hWnd); i--; )
			if (IS_TAB_PART_OF_DOS( (PTab)CTdiCtrl::GetTabContent(m_hWnd,i) ))
				CTdiCtrl::RemoveTab(m_hWnd,i);
	}

	CMainWindow::CTdiView::PTab CMainWindow::CTdiView::__getCurrentTab__() const{
		// wrapper
		return pCurrentTab;
	}

	LRESULT CMainWindow::CTdiView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_SETFOCUS:
				// window has received focus
				//fallthrough
			case WM_KILLFOCUS:
				// window has lost focus
				( (CMainWindow *)app.m_pMainWnd )->SetActiveView(	pCurrentTab
																	? pCurrentTab->view
																	: this
																);
				return 0;
		}
		return __super::WindowProc(msg,wParam,lParam);
	}
