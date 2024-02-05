#include "stdafx.h"
#include "MSDOS7.h"

	void WINAPI CMainWindow::CTdiView::TTab::OnOptionalTabClosing(CTdiCtrl::TTab::PContent tab){
		delete ((PTab)tab)->view;
	}



	CMainWindow::CTdiView::TTab::TTab(UINT nMenuResId,UINT nToolbarResId,UINT nToolBarId,PImage image,PView _view)
		// ctor
		// - initialization
		: image(image) , view(_view)
		// - creating the Menu
		, menu(nMenuResId)
		// - creating the ToolBar (its displaying in CTdiView::ShowContent)
		, toolbar(nToolbarResId,nToolBarId) {
	}










	class CIntroductoryGuidePost sealed:public Utils::CRideDialog{
		const Utils::CRideFont sectionTitleFont;
		const Utils::CRideFont buttonCaptionFont;
		const int singleLineButtonHeight;
		BYTE nCategories;
		struct{
			LONG posY;
			WCHAR webdingsGlyph;
		} categories[5];
		CRect rcCurrContent;

		CIntroductoryGuidePost()
			// ctor
			: Utils::CRideDialog(IDR_GUIDEPOST)
			, sectionTitleFont( FONT_MS_SANS_SERIF, 90, true, true )
			, buttonCaptionFont( FONT_MS_SANS_SERIF, 80, false, true )
			, singleLineButtonHeight( buttonCaptionFont.charHeight+2*(1+::GetSystemMetrics(SM_CYBORDER)) )
			, nCategories(0) {
		}

		void __addStaticText__(LPCTSTR text,const Utils::CRideFont &font){
			// adds a new static text under currently open category
			CRect rc=rcCurrContent;
			::DrawText( CRideDC(*this,ID_HEAD), text, -1, &rc, DT_WORDBREAK|DT_CALCRECT );
			const int height=rc.Height()+2*(1+::GetSystemMetrics(SM_CYBORDER));
			SetDlgItemFont(
				::CreateWindow(
					WC_STATIC, text, WS_VISIBLE|WS_CHILD,
					rcCurrContent.left,rcCurrContent.top, rcCurrContent.Width(),height,
					m_hWnd, 0, app.m_hInstance, nullptr
				),
				font
			);
			rcCurrContent.top+=height;
		}

		void __addStaticText__(LPCTSTR text){
			// adds a new static text under currently open category
			__addStaticText__(text,buttonCaptionFont);
		}

		void __addCategory__(LPCTSTR title,WCHAR webdingsGlyph){
			// opens a new category in the GuidePost
			categories[nCategories].posY = rcCurrContent.top+=10;
			categories[nCategories].webdingsGlyph=webdingsGlyph;
			nCategories++;
			__addStaticText__( title, sectionTitleFont );
		}

		void __addButton__(LPCTSTR caption,UINT id,WCHAR wingdingsGlyphBeforeText='\0',COLORREF glyphColor=COLOR_BLACK){
			// adds a new button under currently open category
			ASSERT(nCategories>0); // a category must currently be open
			ConvertToCommandLikeButton(
				::CreateWindow(
					WC_BUTTON, caption, WS_VISIBLE|WS_CHILD,
					rcCurrContent.left,rcCurrContent.top, rcCurrContent.Width(),singleLineButtonHeight,
					m_hWnd, (HMENU)id, app.m_hInstance, nullptr
				),
				wingdingsGlyphBeforeText,
				0,
				-20,
				glyphColor
			);
			SetDlgItemFont( id, buttonCaptionFont );
			rcCurrContent.top+=singleLineButtonHeight;
		}

		void __addHyperlinkText__(LPCWSTR hyperlinkText){
			// adds a new static text under currently open category
			SetDlgItemFont(
				::CreateWindowW(
					WC_LINK, hyperlinkText, WS_VISIBLE|WS_CHILD|SS_CENTERIMAGE,
					rcCurrContent.left,rcCurrContent.top, rcCurrContent.Width(),singleLineButtonHeight,
					m_hWnd, 0, app.m_hInstance, nullptr
				),
				buttonCaptionFont
			);
			rcCurrContent.top+=singleLineButtonHeight;
		}

		void PreInitDialog() override{
			// dialog initialization
			// - base
			__super::PreInitDialog();
			GetClientRect(&rcCurrContent);
			rcCurrContent.left=Utils::LogicalUnitScaleFactor*70, rcCurrContent.right-=Utils::LogicalUnitScaleFactor*16;
			rcCurrContent.top= 8 + GetDlgItemClientRect(ID_HEAD).bottom;
			// - informing on outdated version
			const CString strLkv=app.GetProfileString( INI_GENERAL, INI_LATEST_KNOWN_VERSION );
			if (strLkv.GetLength()>0 && ::lstrcmp(_T(APP_VERSION),strLkv)){ // known that this app is outdated?
				__addCategory__( _T("Outdated!"), 0xf069 );
				__addHyperlinkText__( L"A newer version available - get it <a id=\"UPDATE\">here</a>!" );
			}
			if (app.dateRecencyLastChecked){ // at least once in the past the recency has been checked on-line?
				const CMSDOS7::TDateTime dateTimeRlc( app.dateRecencyLastChecked );
				SetDlgItemFormattedText( ID_LATENCY, _T("Recency last checked online: %s, %s"), (LPCTSTR)dateTimeRlc.DateToStdString(), (LPCTSTR)dateTimeRlc.TimeToStdString() );
				SetDlgItemFont( ID_LATENCY, Utils::CRideFont::Small );
				ShowDlgItem( ID_LATENCY );
			}
			// - composing the "Recently accessed locations" section
			__addCategory__( _T("Recently accessed locations"), 0xf0cd );
				BYTE i=0;
				for( CRideApp::CRecentFileListEx *const pMru=app.GetRecentFileList(); i<4 && i<pMru->GetSize(); i++ ){ // 4 = max # of MRU files displayed in the GuidePost
					const CString &fileName=pMru->operator[](i);
					if (fileName.IsEmpty())
						break;
					TCHAR dosName[80];
					const CDos::PCProperties dosProps=pMru->GetDosMruFileOpenWith(i);
					if (!dosProps)
						::lstrcpy( dosName, _T("Auto") );
					else if (const LPCTSTR space=::StrChr(dosProps->name,' '))
						::lstrcpyn( dosName, dosProps->name, space-dosProps->name+1 );
					else
						::lstrcpy( dosName, dosProps->name );
					__addButton__( _T(""), ID_FILE_MRU_FILE1+i, 0xf030, 0x47bbbb );
					SetDlgItemCompactPath(
						ID_FILE_MRU_FILE1+i,
						Utils::SimpleFormat( _T("(%s) %s"), dosName, fileName )
					);
				}
				if (!i){
					//__addStaticText__( _T("Currently none. Files you open or drives you access will be shown here."), buttonCaptionFont );
					__addHyperlinkText__( L"Currently none. Begin by <a id=\"OPENIMG\">opening an image</a> or <a id=\"ACCSDRV\">accessing a drive</a>." );
				}
			// - composing the "FAQ" section
			__addCategory__( _T("Frequent questions (network connection needed)"), 0xf0a8 );
				#define HELP_GLYPH_COLOR	0x585858
				__addButton__( _T("How do I format a real floppy?"), ID_FORMAT, 0xf026, HELP_GLYPH_COLOR );
				__addButton__( _T("How do I dump a real floppy to an image?"), ID_IMAGE, 0xf026, HELP_GLYPH_COLOR );
				__addButton__( _T("How do I dump an image back to a real floppy?"), ID_MEDIUM, 0xf026, HELP_GLYPH_COLOR );
				__addButton__( _T("How do I make an exact copy of a real floppy?"), ID_CREATOR, 0xf026, HELP_GLYPH_COLOR );
				__addButton__( _T("How do I convert one image format to another?"), ID_DATA, 0xf026, HELP_GLYPH_COLOR );
				__addButton__( _T("Are tape images supported?"), ID_TAPE_OPEN, 0xf026, HELP_GLYPH_COLOR );
			// - composing the "Miscellaneous" section
			//TODO
			// - adjusting the window height so that all the content is visible
			CRect rc;
			GetWindowRect(&rc);
			SetWindowPos( nullptr, 0,0, rc.Width(), rcCurrContent.top+20, SWP_NOZORDER|SWP_NOMOVE|SWP_FRAMECHANGED );
		}

		void OnCancel() override{
			// the Dialog is about the be closed
			//nop (user can't close the Guidepost)
		}

		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
			// window procedure
			switch (msg){
				case WM_CTLCOLORSTATIC:
					// adjustment of static labels drawing
					if ((HWND)lParam==GetDlgItemHwnd(ID_LATENCY)){
						// we're about to draw the static
						::SetBkMode( (HDC)wParam, TRANSPARENT );
						::SetTextColor( (HDC)wParam, ::GetSysColor(COLOR_3DSHADOW) );
						return (LRESULT)(HBRUSH)Utils::CRideBrush::None;
					}
					break;
				case WM_PAINT:{
					// drawing
					// . base
					__super::WindowProc(msg,wParam,lParam);
					// . header background
					CRideDC dc( *this, ID_HEAD );
					::FillRect( dc, &dc.rect, Utils::CRideBrush::White );
					// . application title
					::SetBkMode(dc,TRANSPARENT);
					dc.rect.left=Utils::LogicalUnitScaleFactor*55;
					const Utils::CRideFont fontTitle( FONT_MS_SANS_SERIF, 195, false, true );
					const HGDIOBJ hFont0=::SelectObject( dc, fontTitle );
						::SetTextColor( dc, 0xffecd9 );
						::DrawText( dc, APP_FULLNAME,-1, &dc.rect, DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX );
					//::SelectObject(dc,hFont0); // commented out as further changes to DC's font below
					// . category Glyphs etc
					const Utils::CRideFont fontGlyph( FONT_WEBDINGS, 300, false, true );
					::SelectObject( dc, fontGlyph );
						::SetTextColor( dc, Utils::GetBlendedColor(::GetSysColor(COLOR_BTNFACE),::GetSysColor(COLOR_BTNTEXT),.3f) );
						for( BYTE c=0; c<nCategories; c++ )
							::TextOutW( dc, 15,categories[c].posY, &categories[c].webdingsGlyph,1 );
					::SelectObject(dc,hFont0);
					return 0;
				}
				case WM_COMMAND:
					// processing a command
					switch (wParam){
						case ID_FORMAT:
							app.GetMainWindow()->OpenApplicationFaqWebPage(_T("faq_formatFloppy.html"));
							return 0;
						case ID_IMAGE:
							app.GetMainWindow()->OpenApplicationFaqWebPage(_T("faq_floppy2image.html"));
							return 0;
						case ID_MEDIUM:
							app.GetMainWindow()->OpenApplicationFaqWebPage(_T("faq_image2floppy.html"));
							return 0;
						case ID_CREATOR:
							app.GetMainWindow()->OpenApplicationFaqWebPage(_T("faq_copyFloppy.html"));
							return 0;
						case ID_DATA:
							app.GetMainWindow()->OpenApplicationFaqWebPage(_T("faq_convertImage.html"));
							return 0;
						case ID_TAPE_OPEN:
							app.GetMainWindow()->OpenApplicationFaqWebPage(_T("faq_supportedTapes.html"));
							return 0;
					}
					break;
				case WM_NOTIFY:
					if (((LPNMHDR)lParam)->code==NM_CLICK){ // some hyperlink clicked
						const PNMLINK pNmLink=(PNMLINK)lParam;
						if (!::lstrcmpW(pNmLink->item.szID,L"OPENIMG"))
							app.__openImage__();
						else if (!::lstrcmpW(pNmLink->item.szID,L"ACCSDRV"))
							app.__openDevice__();
						else if (!::lstrcmpW(pNmLink->item.szID,L"UPDATE"))
							app.GetMainWindow()->OpenRepositoryWebPage( nullptr, _T("/releases") );
						return 0;
					}
					break;
				case WM_NCDESTROY:
					// window is about to be destroyed
					__super::WindowProc(msg,wParam,lParam);
					delete pSingleInstance;
					pSingleInstance=nullptr;
					return 0;
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
				CentreInParent();
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
			if (pSingleInstance)
				::DestroyWindow( pSingleInstance->m_hWnd );
		}
	};

	const CIntroductoryGuidePost *CIntroductoryGuidePost::pSingleInstance;

	#if _MFC_VER>=0x0A00
	afx_msg void CRideApp::OnOpenRecentFile(UINT nID){
		// opens document from the MRU files list under the given index
		// - base
		extern CImage::PCProperties imageProps;
		imageProps=app.GetRecentFileList()->GetMruDevice(nID-ID_FILE_MRU_FIRST);
		extern CDos::PCProperties manuallyForceDos;
		manuallyForceDos=app.GetRecentFileList()->GetDosMruFileOpenWith(nID-ID_FILE_MRU_FIRST);
		__super::OnOpenRecentFile(nID);
		// - if no Image opened, it wasn't found in which case it was removed from the MRU files list - projecting the updated MRU files list to the just shown introductory GuidePost
		if (!CImage::GetActive()){
			CIntroductoryGuidePost::Hide();
			CIntroductoryGuidePost::Show();
		}
	}
	#else
	afx_msg BOOL CRideApp::OnOpenRecentFile(UINT nID){
		// opens document from the MRU files list under the given index
		// - base
		extern CImage::PCProperties imageProps;
		imageProps=app.GetRecentFileList()->GetMruDevice(nID-ID_FILE_MRU_FIRST);
		extern CDos::PCProperties manuallyForceDos;
		manuallyForceDos=app.GetRecentFileList()->GetDosMruFileOpenWith(nID-ID_FILE_MRU_FIRST);
		if (!__super::OnOpenRecentFile(nID))
			return FALSE;
		// - if no Image opened, it wasn't found in which case it was removed from the MRU files list - projecting the updated MRU files list to the just shown introductory GuidePost
		if (!CImage::GetActive()){
			CIntroductoryGuidePost::Hide();
			CIntroductoryGuidePost::Show();
		}
		// - successfully handled opening of a MRU file
		return TRUE;
	}
	#endif











	CMainWindow::CTdiView::CTdiView()
		// ctor
		// - base
		: CCtrlView(WC_TABCONTROL,AFX_WS_DEFAULT_VIEW & ~WS_BORDER)
		// - initialization
		, pCurrentTab(nullptr)
		, toolbarMisc( IDR_MISC, IDR_MISC )
		// - initiating determination of recency of this app
		, recencyStatusThread( RecencyDetermination_thread, INVALID_HANDLE_VALUE, 0 ) {
	}










	#define MENU_POSITION_DOS		1
	#define MENU_POSITION_VIEW		2

	void WINAPI CMainWindow::CTdiView::__fnShowContent__(PVOID pTdi,LPCVOID pTab){
		// shows the content of the Tab
		const PTab tab= ((CTdiView *)pTdi)->pCurrentTab = (PTab)pTab;
		const PView view=tab->view;
		// - hiding the introductory GuidePost if this is the first Tab in the TDI
		if (CTdiCtrl::GetCurrentTabContentRect(TDI_HWND,nullptr)) // some Tabs exist
			CIntroductoryGuidePost::Hide();
		// - showing the Menus associated with the DOS and View 
		if (tab->IsPartOfImage()){ // the Tab is part of an Image (e.g. a WebPage usually isn't)
			CImage::GetActive()->dos->menu.Show(MENU_POSITION_DOS);
			tab->menu.Show(MENU_POSITION_VIEW);
		}else
			tab->menu.Show(MENU_POSITION_DOS); // showing the View's Menu at the DOS's position
		// - showing Image's ToolBar (guaranteed that the Toolbar always exists)
		CMainWindow *const pMainWindow=app.GetMainWindow();
		if (tab->IsPartOfImage()){ // the Tab is part of an Image (e.g. a WebPage usually isn't)
			tab->image->toolbar.Show(pMainWindow->toolbar);
		}
		// - showing the Tab's ToolBar (e.g. the FileManager's ToolBar)
		if (tab->IsPartOfImage()) // the Tab is part of an Image (e.g. a WebPage usually isn't)
			tab->toolbar.Show( tab->image->toolbar );
		else
			tab->toolbar.Show( pMainWindow->toolbar );
		// - showing the associated View
		__setStatusBarText__(nullptr); // StatusBar without text
		RECT r;
		CTdiCtrl::GetCurrentTabContentRect( ( (CTdiView *)pTdi )->m_hWnd, &r );
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
	}

	void WINAPI CMainWindow::CTdiView::__fnHideContent__(PVOID pTdi,LPCVOID pTab){
		// hides the content of the Tab
		const PTab tab=(PTab)pTab;
		// - hiding the associated View
		const PView view=tab->view;
		app.GetMainWindow()->SetActiveView((CTdiView *)pTdi); // neccessary to call manually as otherwise no view will be active after closing the CurrentTab (and with no active documents, no command will be propagated to the document, etc.)
		((CTdiView *)pTdi)->pCurrentTab=nullptr;
		if (tab->IsPartOfImage()) // the Tab is part of an Image (e.g. a WebPage usually isn't)
			if (CImage *const image=CImage::GetActive())
				image->RemoveView(view); // View added into the list when shown, see CCreateContext
		::DestroyWindow( view->m_hWnd );
		// - hiding the Tab's ToolBar (e.g. the FileManager's ToolBar)
		tab->toolbar.Hide();
		// - hiding the Image's ToolBar (guaranteed that the Toolbar always exists)
		if (tab->IsPartOfImage())
			tab->image->toolbar.Hide();
		// - hiding the Menus associated with the DOS and View 
		tab->menu.Hide();
		if (tab->IsPartOfImage())
			CImage::GetActive()->dos->menu.Hide();
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

	void CMainWindow::CTdiView::CloseAllTabsOfFocusedImage(){
		// closes all Tabs associated with the Image in focus
		// - if there is a Tab that isn't part of the Image in focus, switching to it, thus giving none of the Image Tabs a chance to be visible again (e.g. doing things in OnCreate)
		for( int i=TabCtrl_GetItemCount(m_hWnd); i--; )
			if (!( (PTab)CTdiCtrl::GetTabContent(m_hWnd,i) )->IsPartOfImage()){ // isn't part of the DOS in focus
				CTdiCtrl::SwitchToTab( m_hWnd, i );
				for( int j=TabCtrl_GetItemCount(m_hWnd); j--; )
					if (( (PTab)CTdiCtrl::GetTabContent(m_hWnd,j) )->IsPartOfImage())
						CTdiCtrl::RemoveTab(m_hWnd,j);
				return;
			}
		// - all Tabs are part of the Image
		TabCtrl_DeleteAllItems(m_hWnd);
		// - no Tabs exist now; show the GuidePost
		CIntroductoryGuidePost::Show();
	}

	#define WM_GUIDEPOST_REPOPULATE	WM_USER+1

	void CMainWindow::CTdiView::RepopulateGuidePost() const{
		::PostMessage( m_hWnd, WM_GUIDEPOST_REPOPULATE, 0, 0 );
	}

	LRESULT CMainWindow::CTdiView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_CREATE:
				// window created
				if (const LRESULT err=__super::WindowProc(msg,wParam,lParam))
					return err;
				#ifndef _DEBUG
					#ifndef APP_SPECIAL_VER
						// this application is a standard release - can check if it's up-to-date or already outdated
						if (const WORD today=CMSDOS7::TDateTime( Utils::CRideTime() ).GetDosDate())
							if (HIWORD(app.dateRecencyLastChecked)!=today) // recency suffices to be checked on-line once a day
								recencyStatusThread.Resume();
					#endif
				#endif
				SetFocus();
				return 0;
			case WM_GUIDEPOST_REPOPULATE:
				// repopulating the GuidePost with new items by hiding and showing it
				if (CIntroductoryGuidePost::pSingleInstance)
					CIntroductoryGuidePost::Hide();
				else
					return 0;
				//fallthrough
			case WM_SETFOCUS:
				// window has received focus
				if (!CIntroductoryGuidePost::pSingleInstance && !CTdiCtrl::GetCurrentTabContentRect(m_hWnd,nullptr))
					CIntroductoryGuidePost::Show(); // displaying the introductory GuidePost
				toolbarMisc.Show( app.GetMainWindow()->toolbar );
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
							return 0;
					}while ( hFocusedWnd=::GetParent(hFocusedWnd) );
					app.GetMainWindow()->SetActiveView(	pCurrentTab
																		? pCurrentTab->view
																		: this
																	);
					CIntroductoryGuidePost::Hide(); // hiding the introductory GuidePost
				}
				break;
		}
		return __super::WindowProc(msg,wParam,lParam);
	}
