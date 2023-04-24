#include "stdafx.h"

#ifdef RELEASE_MFC42
	void __cdecl operator delete(PVOID ptr, UINT sz) noexcept{
		operator delete(ptr);
	}
#endif

	static bool WINAPI __canCloseTabAlways__(CTdiCtrl::TTab::PContent){
		return true;
	}

	TTabInfo::TTabInfo(CTdiCtrl::TTab::TCanBeClosed fnCanBeClosed,CTdiCtrl::TTab::TOnClosing fnOnTabClosing,CTdiCtrl::TTab::PContent content)
		// ctor
		: fnCanBeClosed( fnCanBeClosed!=TDI_TAB_CANCLOSE_ALWAYS ? fnCanBeClosed : __canCloseTabAlways__ )
		, fnOnTabClosing(fnOnTabClosing) , content(content) {
	}

	PCTabInfo GetTabInfo(HWND hTdi,int tabId){
		TCITEM ti;
			ti.mask=TCIF_PARAM;
		TabCtrl_GetItem(hTdi,tabId,&ti);
		return (PCTabInfo)ti.lParam;
	}




	TDraggedTabInfo::TDraggedTabInfo(HWND hTdi,int _tabId)
		// ctor
		: tabId(_tabId) {
		data.mask = TCIF_PARAM | TCIF_TEXT;
		data.pszText=caption;
		data.cchTextMax=ARRAYSIZE(caption);
		TabCtrl_GetItem(hTdi,_tabId,&data);
		TabCtrl_GetItemRect(hTdi,_tabId,&targetArea);
	}




	#define BUTTON_CLOSE_SIZE	20

	unsigned int TTdiInfo::nInstances;
	HFONT TTdiInfo::fontWebdings;

	TTdiInfo::TTdiInfo(HINSTANCE hInstance,HWND hTdi,CTdiCtrl::PCParams params)
		// ctor
		// - initializing
		: handle(hTdi) , params(*params)
		, wndProc0((WNDPROC)SubclassWindow(hTdi,__wndProc__))
		, currentTabContent(nullptr)
		, hBtnCloseCurrentTab(	::CreateWindow(
									WC_BUTTON, _T("r"),
									WS_CHILD|WS_CLIPSIBLINGS|BS_FLAT|BS_CENTER,
									0,0, BUTTON_CLOSE_SIZE,BUTTON_CLOSE_SIZE,
									hTdi,(HMENU)IDCLOSE,hInstance,nullptr
								)
		){
		// - allocating shared resources if creating the the first instance of TDI
		if (::InterlockedIncrement(&nInstances)==1){
			LOGFONT font;
				::GetObject(::GetStockObject(DEFAULT_GUI_FONT),sizeof(font),&font);
				::lstrcpy(font.lfFaceName,_T("Webdings"));
				font.lfCharSet=SYMBOL_CHARSET;
				font.lfHeight=-13;
				font.lfWeight=FW_BOLD;
			fontWebdings=::CreateFontIndirect(&font);
		}
		::SendMessage( hBtnCloseCurrentTab, WM_SETFONT, (WPARAM)fontWebdings, TRUE );
	}

	TTdiInfo::~TTdiInfo(){
		// dtor
		// - freeing shared resources if destructing the last instance of TDI
		if (!::InterlockedDecrement(&nInstances)){
			::DeleteObject(fontWebdings);
		}
	}



	LRESULT CALLBACK TTdiInfo::__wndProc__(HWND hTdi,UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		static PDraggedTabInfo pDraggedTabInfo;
		const PTdiInfo pTdiInfo=GET_TDI_INFO(hTdi);
		const WNDPROC wndProc0=pTdiInfo->wndProc0;
		switch (msg){
			case WM_MOUSEACTIVATE:
				// preventing the focus from being stolen by the parent
				return MA_ACTIVATE;
			case WM_SETFOCUS:
				// window has received focus
				if (pTdiInfo->currentTabContent!=nullptr){
					// some Tab shown
					const HWND hContent=pTdiInfo->params.fnGetHwnd(pTdiInfo->currentTabContent);
					if (hContent==(HWND)wParam) // if the window that loses the focus is the Content ...
						break; // ... preventing from potential infinite loop by better accepting the focus
					::SetFocus(hContent); // otherwise forwarding focus to the Content instead of keeping it
					return 0;
				}else
					break;
			case WM_PAINT:
				// drawing
				if (TabCtrl_GetItemCount(hTdi))
					// at least one Tab exists - painting normally
					break;
				else{
					// no Tab exists - painting as empty MDI container (using "workspace" color)
					PAINTSTRUCT ps;
					const HDC dc=::BeginPaint(hTdi,&ps);
						RECT r;
						::GetClientRect(hTdi,&r);
						::FillRect( dc, &r, ::GetSysColorBrush(COLOR_APPWORKSPACE) );
					::EndPaint(hTdi,&ps);
					return 0;
				}
			case WM_SIZE:{
				// window size changed
				::SetWindowPos(	pTdiInfo->hBtnCloseCurrentTab,
								nullptr,
								GET_X_LPARAM(lParam)-5-BUTTON_CLOSE_SIZE,1, 0,0,
								SWP_NOZORDER|SWP_NOSIZE
							);
				pTdiInfo->__fitContentToTdiCanvas__();
				break;
			}
			case WM_RBUTTONDOWN:
				::SendMessage( hTdi, WM_LBUTTONUP, wParam, lParam ); // the left and right buttons are in exclusive relationship!
				//fallthrough
			case WM_LBUTTONDBLCLK:
			case WM_LBUTTONDOWN:{
				// left mouse button pressed
				const TCHITTESTINFO hti={ { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) } };
				const int tabId=TabCtrl_HitTest(hTdi,&hti);
				if (tabId>=0){
					// Tab caption pressed
					pTdiInfo->__switchToTab__(tabId);
					if (::DragDetect(hTdi,hti.pt)){
						// dragging initiated
						pDraggedTabInfo=new TDraggedTabInfo(hTdi,tabId);
						::SetCapture(hTdi); // want to always know when the dragging ends
					}
					return 0;
				}
				break;
			}
			case WM_RBUTTONUP:{
				// right mouse button released
				const TCHITTESTINFO hti={ { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) } };
				const int tabId=TabCtrl_HitTest(hTdi,&hti);
				if (tabId>=0){
					// released over a Tab - displaying context menu with corresponding actions
					const PCTabInfo pti=GetTabInfo(hTdi,tabId);
					const HMENU hMenu=::CreatePopupMenu();
						::AppendMenu( hMenu, MF_STRING|MF_GRAYED*(pti->fnCanBeClosed==TDI_TAB_CANCLOSE_NEVER), IDCLOSE, _T("Close tab") );
						::AppendMenu( hMenu, MF_STRING, IDIGNORE, _T("Move tab") );
						POINT cursorPos;
						::GetCursorPos(&cursorPos);
						switch (::TrackPopupMenu( hMenu, TPM_RETURNCMD, cursorPos.x, cursorPos.y, 0, hTdi, nullptr )){
							case IDIGNORE:
								// moving current Tab
								::SetCursorPos(cursorPos.x,cursorPos.y);
								pDraggedTabInfo=new TDraggedTabInfo(hTdi,tabId);
								break;
							case IDCLOSE:
								// closing current Tab
								CTdiCtrl::RemoveCurrentTab(hTdi);
								break;
						}
					::DestroyMenu(hMenu);
					return 0;
				}
				break;
			}
			case WM_MOUSEMOVE:{
				// mouse moved
				if (pDraggedTabInfo){
					const TCHITTESTINFO hti={ { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) } };
					if (!::PtInRect(&pDraggedTabInfo->targetArea,hti.pt)){
						// cursor out of "insensitive area" (preventing from cyclic fast switching between neighboring tabs during dragging)
						const int iTargetTab=TabCtrl_HitTest(hTdi,&hti);
						if (iTargetTab>=0 && iTargetTab!=pDraggedTabInfo->tabId){
							// A&B; A = cursor over TargetTab, B = TargetTab is different from DraggedTab
							// . new "insensitive area" (preventing from cyclic fast switching between neighboring tabs during dragging)
							TabCtrl_GetItemRect( hTdi, iTargetTab, &pDraggedTabInfo->targetArea );
							// . switching Tabs
							TCITEM tmp;
								TCHAR tmpCaption[80];
								tmp.mask = TCIF_PARAM | TCIF_TEXT;
								tmp.pszText=tmpCaption;
								tmp.cchTextMax=ARRAYSIZE(tmpCaption);
							TabCtrl_GetItem( hTdi, iTargetTab, &tmp );
							TabCtrl_SetItem( hTdi, iTargetTab, &pDraggedTabInfo->data );
							TabCtrl_SetItem( hTdi, pDraggedTabInfo->tabId, &tmp );
							// . redrawing CurrentTabContent (as it might get hidden after switching)
							pTdiInfo->__switchToTab__(iTargetTab);
							pTdiInfo->params.fnRepaintContent(pTdiInfo->currentTabContent);
							// . updating the DraggedTabInformation
							pDraggedTabInfo->tabId=iTargetTab;
						}
					}
				}
				break;
			}
			case WM_LBUTTONUP:
				// left mouse button released
				if (pDraggedTabInfo){
					::ReleaseCapture(); // no need to further block the capture once we know that the dragging has ended
					delete pDraggedTabInfo, pDraggedTabInfo=nullptr;
				}
				break;
			case WM_COMMAND:
				// processing a command
				if (wParam==MAKELONG(IDCLOSE,BN_CLICKED))
					// tab close button pressed
					CTdiCtrl::RemoveCurrentTab(hTdi);
				break;
			case TCM_DELETEITEM:{
				// closing and disposing the Tab
				// . getting information on the current situation
				const PCTabInfo pti=GetTabInfo(hTdi,wParam);
				UINT iCurrentTab=TabCtrl_GetCurSel(hTdi);
				const bool closingCurrentTab=iCurrentTab==wParam;
				// . aborting movement of Tab that is about to be closed
				if (pDraggedTabInfo!=nullptr && pDraggedTabInfo->tabId==wParam)
					::SendMessage( hTdi, WM_LBUTTONUP, 0, 0 );
				// . base (closing the Tab)
				::CallWindowProc(wndProc0,hTdi,msg,wParam,lParam);
				// . switching to some of remaining Tabs
				if (!closingCurrentTab){
					// closing another but current Tab - just repainting it
					iCurrentTab -= wParam<iCurrentTab; // indices of Tabs right from the just closed Tab must be decreased (e.g. if just closed Tab3 while currently switched to Tab4, then Tab4 shifts to place of Tab3)
					pTdiInfo->params.fnRepaintContent( CTdiCtrl::GetTabContent(hTdi,iCurrentTab) );
				}else if (const UINT n=TabCtrl_GetItemCount(hTdi))
					// some Tabs have remained - switching to one of them
					pTdiInfo->__switchToTab__( wParam<n ? wParam : n-1 );
				else
					// no Tabs have remained
					pTdiInfo->__hideCurrentContent__();
				// . letting the caller know that the Tab is being closed
				if (pti->fnOnTabClosing)
					pti->fnOnTabClosing(pti->content);
				delete pti;
				return 0;
			}
			case TCM_DELETEALLITEMS:
				// closing and disposing all Tabs
				pTdiInfo->__hideCurrentContent__();
				// . aborting movement of any Tab
				if (pDraggedTabInfo!=nullptr)
					::SendMessage( hTdi, WM_LBUTTONUP, 0, 0 );
				// . letting the caller know that each Tab is being closed
				for( int i=TabCtrl_GetItemCount(hTdi); i; ){
					const PCTabInfo pti=GetTabInfo(hTdi,--i);
					if (pti->fnOnTabClosing)
						pti->fnOnTabClosing(pti->content);
					delete pti;
				}
				// . base, closing all Tabs
				break;
			case WM_DESTROY:
				// destroying the TDI
				TabCtrl_DeleteAllItems(hTdi);
				SubclassWindow(hTdi,wndProc0);
				::DestroyWindow( pTdiInfo->hBtnCloseCurrentTab );
				delete pTdiInfo;
				break;
		}
		return ::CallWindowProc(wndProc0,hTdi,msg,wParam,lParam);
	}

	void TTdiInfo::__hideCurrentContent__(){
		// hides CurrentTabContent
		if (!currentTabContent) return; // already hidden
		::ShowWindow( hBtnCloseCurrentTab, SW_HIDE );
		const CTdiCtrl::TTab::PContent tabContent=currentTabContent;
		currentTabContent=nullptr;
		params.fnHideContent( params.customParam, tabContent );
	}

	void TTdiInfo::__switchToTab__(int i){
		// switches to I-th Tab
		// - hiding CurrentTabContent
		__hideCurrentContent__();
		// - switching to I-th Tab
		TabCtrl_SetCurSel(handle,i);
		if (const PCTabInfo pti=GetTabInfo(handle,i)){
			::ShowWindow( hBtnCloseCurrentTab, (pti->fnCanBeClosed!=TDI_TAB_CANCLOSE_NEVER)*SW_SHOW );
			params.fnShowContent( params.customParam, currentTabContent=pti->content );
			__fitContentToTdiCanvas__();
		}
	}

	void TTdiInfo::__fitContentToTdiCanvas__() const{
		// stretches CurrentTabContent so that it covers the whole TDI canvas below Tab captions
		RECT r;
		if (currentTabContent && CTdiCtrl::GetCurrentTabContentRect(handle,&r))
			::SetWindowPos(	params.fnGetHwnd(currentTabContent), nullptr,
							r.left, r.top,
							r.right-r.left, r.bottom-r.top,
							SWP_NOZORDER | SWP_SHOWWINDOW
						);
	}
