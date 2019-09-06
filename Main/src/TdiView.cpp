#include "stdafx.h"

	CMainWindow::CTdiView::TTab::TTab(UINT nMenuResId,UINT nToolbarResId,UINT nToolBarId,PDos _dos,PView _view)
		// ctor
		// - initialization
		: dos(_dos) , view(_view)
		// - creating the Menu
		, menu(nMenuResId)
		// - creating the ToolBar (its displaying in CTdiView::ShowContent)
		, toolbar(nToolbarResId,nToolBarId) {
	}










	class CIntroductoryGuidePost sealed:public CDialog{
		const CRideFont sectionTitleFont;
		const CRideFont buttonCaptionFont;
		BYTE nCategories;
		struct{
			short posY;
			WCHAR webdingsGlyph;
		} categories[5];
		CRect rcCurrContent;

		CIntroductoryGuidePost()
			// ctor
			: CDialog(IDR_GUIDEPOST)
			, sectionTitleFont( FONT_MS_SANS_SERIF, 90, true, true )
			, buttonCaptionFont( FONT_MS_SANS_SERIF, 80, false, true )
			, nCategories(0) {
		}

		void __addStaticText__(LPCTSTR text,const CRideFont &rFont){
			// adds a new static text under currently open category
			const int height=rFont.charHeight+2*(1+::GetSystemMetrics(SM_CYBORDER));
			::SendMessage(
				::CreateWindow(
					WC_STATIC, text, WS_VISIBLE|WS_CHILD|SS_CENTERIMAGE,
					rcCurrContent.left,rcCurrContent.top, rcCurrContent.Width(),height,
					m_hWnd, 0, app.m_hInstance, nullptr
				),
				WM_SETFONT, (WPARAM)rFont.m_hObject, 0
			);
			rcCurrContent.top+=height;
		}

		void __addCategory__(LPCTSTR title,WCHAR webdingsGlyph){
			// opens a new category in the GuidePost
			categories[nCategories].posY = rcCurrContent.top+=10;
			categories[nCategories].webdingsGlyph=webdingsGlyph;
			nCategories++;
			__addStaticText__( title, sectionTitleFont );
		}

		void __addButton__(LPCTSTR caption,UINT id,WCHAR wingdingsGlyphBeforeText='\0'){
			// adds a new button under currently open category
			ASSERT(nCategories>0); // a category must currently be open
			const int height=buttonCaptionFont.charHeight+2*(1+::GetSystemMetrics(SM_CYBORDER));
			Utils::ConvertToCommandLikeButton(
				::CreateWindow(
					WC_BUTTON, caption, WS_VISIBLE|WS_CHILD,
					rcCurrContent.left,rcCurrContent.top, rcCurrContent.Width(),height,
					m_hWnd, (HMENU)id, app.m_hInstance, nullptr
				),
				wingdingsGlyphBeforeText,
				0xa00000,
				-20
			);
			SendDlgItemMessage( id, WM_SETFONT, (WPARAM)buttonCaptionFont.m_hObject );
			rcCurrContent.top+=height;
		}

		void PreInitDialog() override{
			// dialog initialization
			// - base
			__super::PreInitDialog();
			// - composing the "Recently accessed locations" section
			GetClientRect(&rcCurrContent);
			rcCurrContent.top=55*Utils::LogicalUnitScaleFactor, rcCurrContent.left=70*Utils::LogicalUnitScaleFactor, rcCurrContent.right-=16*Utils::LogicalUnitScaleFactor;
			__addCategory__( _T("Recently accessed locations"), 0xf0cd );
				BYTE i=0;
				for( CRecentFileList *const pMru=app.GetRecentFileList(); i<4 && i<pMru->GetSize(); i++ ){ // 4 = max # of MRU files displayed in the GuidePost
					const CString &fileName=pMru->operator[](i);
					if (fileName.IsEmpty())
						break;
					TCHAR buf[MAX_PATH];
					::PathCompactPath( CClientDC(this), ::lstrcpy(buf,fileName), rcCurrContent.Width() );
					__addButton__( buf, ID_FILE_MRU_FILE1+i, 0xf030 );
				}
				if (!i)
					__addStaticText__( _T("Currently none. Files you open or drives you access will be shown here."), buttonCaptionFont );
			// - composing the "FAQ" section
			__addCategory__( _T("Frequent questions (network connection needed)"), 0xf0a8 );
				__addButton__( _T("How do I format a real floppy?"), ID_FORMAT, 0xf026 );
				__addButton__( _T("How do I dump a real floppy to an image?"), ID_IMAGE, 0xf026 );
				__addButton__( _T("How do I dump an image back to a real floppy?"), ID_MEDIUM, 0xf026 );
				__addButton__( _T("How do I make an exact copy of a real floppy?"), ID_CREATOR, 0xf026 );
				__addButton__( _T("How do I convert one image format to another?"), ID_DATA, 0xf026 );
				__addButton__( _T("Are tape images supported?"), ID_TAPE_OPEN, 0xf026 );
			// - composing the "Miscellaneous" section
			//TODO
			// - adjusting the window height so that all the content is visible
			CRect rc;
			GetWindowRect(&rc);
			SetWindowPos( nullptr, 0,0, rc.Width(), rcCurrContent.top+20, SWP_NOZORDER|SWP_NOMOVE|SWP_FRAMECHANGED );
		}

		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
			// window procedure
			switch (msg){
				case WM_PAINT:{
					// drawing
					// . base
					__super::WindowProc(msg,wParam,lParam);
					// . header background
					const CClientDC dc(this);
					RECT rc;
					GetDlgItem(ID_HEAD)->GetClientRect(&rc);
					::FillRect( dc, &rc, CRideBrush::White );
					// . application title
					::SetBkMode(dc,TRANSPARENT);
					rc.left=55*Utils::LogicalUnitScaleFactor;
					const CRideFont fontTitle( FONT_MS_SANS_SERIF, 195, false, true );
					const HGDIOBJ hFont0=::SelectObject( dc, fontTitle );
						::SetTextColor( dc, 0xffecd9 );
						::DrawText( dc, APP_FULLNAME,-1, &rc, DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX );
					//::SelectObject(dc,hFont0); // commented out as further changes to DC's font below
					// . category Glyphs etc
					const CRideFont fontGlyph( FONT_WEBDINGS, 300, false, true );
					::SelectObject( dc, fontGlyph );
						::SetTextColor( dc, Utils::GetBlendedColor(::GetSysColor(COLOR_BTNFACE),::GetSysColor(COLOR_BTNTEXT),.3f) );
						for( BYTE c=0; c<nCategories; c++ )
							::TextOutW( dc, 15,categories[c].posY, &categories[c].webdingsGlyph,1 );
					::SelectObject(dc,hFont0);
					return 0;
				}
				case WM_COMMAND:
					// processing a command
					#define TAB_TITLE	_T("Answer")
					switch (wParam){
						case ID_FORMAT:
							((CMainWindow *)app.m_pMainWnd)->OpenApplicationPresentationWebPage(TAB_TITLE,_T("faq_formatFloppy.html"));
							return 0;
						case ID_IMAGE:
							((CMainWindow *)app.m_pMainWnd)->OpenApplicationPresentationWebPage(TAB_TITLE,_T("faq_floppy2image.html"));
							return 0;
						case ID_MEDIUM:
							((CMainWindow *)app.m_pMainWnd)->OpenApplicationPresentationWebPage(TAB_TITLE,_T("faq_image2floppy.html"));
							return 0;
						case ID_CREATOR:
							((CMainWindow *)app.m_pMainWnd)->OpenApplicationPresentationWebPage(TAB_TITLE,_T("faq_copyFloppy.html"));
							return 0;
						case ID_DATA:
							((CMainWindow *)app.m_pMainWnd)->OpenApplicationPresentationWebPage(TAB_TITLE,_T("faq_convertImage.html"));
							return 0;
						case ID_TAPE_OPEN:
							((CMainWindow *)app.m_pMainWnd)->OpenApplicationPresentationWebPage(TAB_TITLE,_T("faq_supportedTapes.html"));
							return 0;
					}
					break;
			}
			return __super::WindowProc(msg,wParam,lParam);
		}
	public:
		static const CIntroductoryGuidePost *pSingleInstance;

		static void Show(){
			// creates and positions the GuidePost
			if (!pSingleInstance){
				CIntroductoryGuidePost *const pIgp=new CIntroductoryGuidePost;
				pSingleInstance=pIgp;
				pIgp->Create( IDR_GUIDEPOST, TDI_INSTANCE );
				pIgp->ShowWindow(SW_SHOW);
				app.m_pMainWnd->SetFocus();
			}
		}

		static void CentreInParent(){
			// centres the GuidePost in parent's client area
			if (pSingleInstance){ // may not exist if some Tab already exists and the MainWindow has been resized
				CRect rcGuidePost, rcTdi;
				pSingleInstance->GetClientRect(&rcGuidePost);
				TDI_INSTANCE->GetClientRect(&rcTdi);
				::SetWindowPos(	pSingleInstance->m_hWnd,
								nullptr,
								(rcTdi.Width()-rcGuidePost.Width())/2, (rcTdi.Height()-rcGuidePost.Height())/2,
								0,0,
								SWP_NOZORDER|SWP_NOSIZE
							);
			}
		}

		static void Hide(){
			// destroys the GuidePost
			if (pSingleInstance){
				::DestroyWindow( pSingleInstance->m_hWnd );
				pSingleInstance=nullptr;
			}
		}
	};

	const CIntroductoryGuidePost *CIntroductoryGuidePost::pSingleInstance;

	afx_msg BOOL CRideApp::OnOpenRecentFile(UINT nID){
		// opens document from the MRU files list under the given index
		// - base
		if (!__super::OnOpenRecentFile(nID))
			return FALSE;
		// - if no Image opened, it wasn't found in which case it was removed from the MRU files list - projecting the updated MRU files list to the just shown introductory GuidePost
		if (!CImage::__getActive__()){
			CIntroductoryGuidePost::Hide();
			CIntroductoryGuidePost::Show();
		}
		// - successfully handled opening of a MRU file
		return TRUE;
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
			CImage::GetActive()->dos->menu.__show__(MENU_POSITION_DOS);
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
			cc.m_pCurrentDoc=CImage::GetActive();
		view->Create(	nullptr, nullptr,
						(AFX_WS_DEFAULT_VIEW & ~WS_BORDER) | WS_CLIPSIBLINGS,
						r, (CTdiView *)pTdi, 0,
						&cc
					);
		// - initializing the associated View
		pMainWindow->SetActiveView(view);
		pMainWindow->OnUpdateFrameTitle(TRUE); // just to be sure, forcing the document's name to the MainWindow's title
		view->SetFocus();
		// - hiding the introductory GuidePost if this is the first Tab in the TDI
		if (CTdiCtrl::GetCurrentTabContentRect(TDI_HWND,nullptr)) // some Tabs exist
			CIntroductoryGuidePost::Hide();
	}

	void WINAPI CMainWindow::CTdiView::__fnHideContent__(PVOID pTdi,LPCVOID pTab){
		// hides the content of the Tab
		const PTab tab=(PTab)pTab;
		// - hiding the associated View
		const PView view=tab->view;
		( (CMainWindow *)app.m_pMainWnd )->SetActiveView((CTdiView *)pTdi); // neccessary to call manually as otherwise no view will be active after closing the CurrentTab (and with no active documents, no command will be propagated to the document, etc.)
		((CTdiView *)pTdi)->pCurrentTab=nullptr;
		if (IS_TAB_PART_OF_DOS(tab)) // the Tab is part of a DOS (e.g. a WebPage is usually not part of any DOS)
			if (CImage *const image=CImage::GetActive())
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
			CImage::GetActive()->dos->menu.__hide__();
		// - resetting the StatusBar
		__resetStatusBar__();
		// - displaying the introductory GuidePost if this was the last Tab in the TDI
		if (!CTdiCtrl::GetCurrentTabContentRect(TDI_HWND,nullptr)) // no Tabs exist
			CIntroductoryGuidePost::Show();
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
			case WM_CREATE:
				// window created
				if (const LRESULT err=__super::WindowProc(msg,wParam,lParam))
					return err;
				SetFocus();
				return 0;
			case WM_SETFOCUS:
				// window has received focus
				if (!CIntroductoryGuidePost::pSingleInstance && !CTdiCtrl::GetCurrentTabContentRect(m_hWnd,nullptr))
					CIntroductoryGuidePost::Show(); // displaying the introductory GuidePost
				//fallthrough
			case WM_SIZE:
				// window size changed
				CIntroductoryGuidePost::CentreInParent();
				break;
			case WM_KILLFOCUS:
				// window has lost focus
				if (CIntroductoryGuidePost::pSingleInstance){
					HWND hFocusedWnd=(HWND)wParam;
					do{
						if (hFocusedWnd==CIntroductoryGuidePost::pSingleInstance->m_hWnd)
							return __super::WindowProc(msg,wParam,lParam);
					}while ( hFocusedWnd=::GetParent(hFocusedWnd) );
					( (CMainWindow *)app.m_pMainWnd )->SetActiveView(	pCurrentTab
																		? pCurrentTab->view
																		: this
																	);
					CIntroductoryGuidePost::Hide(); // hiding the introductory GuidePost
				}
				break;
		}
		return __super::WindowProc(msg,wParam,lParam);
	}
