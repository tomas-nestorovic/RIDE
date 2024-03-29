#include "stdafx.h"

	CTdiCtrl::TParams::TParams(PCustomParam customParam,TTab::TShowContent fnShowContent,TTab::THideContent fnHideContent,TTab::TRepaintContent fnRepaintContent,TTab::TGetContentHwnd fnGetHwnd)
		// ctor
		: customParam(customParam)
		, fnShowContent(fnShowContent) , fnHideContent(fnHideContent) , fnRepaintContent(fnRepaintContent) , fnGetHwnd(fnGetHwnd) {
	}








	HWND WINAPI CTdiCtrl::Create(HINSTANCE hInstance,LPCTSTR windowName,UINT style,int width,int height,HWND hParent,UINT id,PCParams params){
		// creates and returns a new instance of PropertyGrid
		::InitCommonControls();
		const HWND hTab32=::CreateWindow(	WC_TABCONTROL,windowName,
											style,
											0,0, width,height,
											hParent, (HMENU)id,hInstance,nullptr
										);
		SubclassWnd(hInstance,hTab32,params);
		return hTab32;
	}

	void WINAPI CTdiCtrl::SubclassWnd(HINSTANCE hInstance,HWND hTabCtrl,PCParams params){
		// subclasses the existing Window to a TDI
		::SetWindowLong(hTabCtrl, GWL_USERDATA,
						(LONG)new TTdiInfo( hInstance, hTabCtrl, params )
					);
		::InvalidateRect(hTabCtrl,nullptr,FALSE);
	}

	CTdiCtrl::TTab::PContent CTdiCtrl::GetTabContent(HWND hTdi,int iIndex){
		// returns the Content of the Tab identified by its Index; returns Null if Tab with given Index doesn't exist
		if (0<=iIndex && iIndex<TabCtrl_GetItemCount(hTdi))
			return GetTabInfo(hTdi,iIndex)->content;
		else
			return nullptr;
	}

	bool WINAPI CTdiCtrl::GetCurrentTabContentRect(HWND hTdi,LPRECT pOutRect){
		// True <=> the rectangle of TDI canvas below Tab captions (where Tab Contents are shown) has been retrieved, otherwise False
		RECT r,s;
		if (!TabCtrl_GetItemRect(hTdi,0,&s)) return false;
		if (pOutRect){
			::GetClientRect(hTdi,&r);
			const int padding=r.top;
			pOutRect->left=padding;
			pOutRect->top=s.bottom+padding;
			pOutRect->right=r.right-padding;
			pOutRect->bottom=r.bottom;
		}
		return true;
	}

	void WINAPI CTdiCtrl::InsertTabA(HWND hTdi,int iIndex,LPCSTR tabName,TTab::PContent tabContent,bool makeCurrent,TTab::TCanBeClosed fnCanBeClosed,TTab::TOnClosing fnOnTabClosing){
		// adds to specified position (Index) a new Tab with given Name and Content
		WCHAR bufW[1024];
		::MultiByteToWideChar( CP_ACP,0, tabName,-1, bufW,ARRAYSIZE(bufW) );
		return InsertTabW( hTdi, iIndex, bufW, tabContent, makeCurrent, fnCanBeClosed, fnOnTabClosing );
	}

	void WINAPI CTdiCtrl::InsertTabW(HWND hTdi,int iIndex,LPCWSTR tabName,TTab::PContent tabContent,bool makeCurrent,TTab::TCanBeClosed fnCanBeClosed,TTab::TOnClosing fnOnTabClosing){
		// adds to specified position (Index) a new Tab with given Name and Content
		for( int i=TabCtrl_GetItemCount(hTdi); i--; ){
			if (GetTabContent(hTdi,i)==tabContent){
				// Tab already created - switching to it (if not already switched to)
				if (TabCtrl_GetCurSel(hTdi)!=i) // not yet switched to
					if (makeCurrent) // wanted to be switched to
						SwitchToTab( hTdi, i );
				return;
			}
		}
		TCITEMW ti;
			ti.mask=TCIF_TEXT|TCIF_PARAM;
			ti.pszText=(PWCHAR)tabName;
			ti.lParam=(LPARAM)new TTabInfo( fnCanBeClosed, fnOnTabClosing, tabContent );
		::SendMessageW( hTdi, TCM_INSERTITEMW, iIndex, (LPARAM)&ti );
		if (makeCurrent){
			SwitchToTab( hTdi, iIndex );
			::InvalidateRect( hTdi, nullptr, TRUE );
		}
	}

	void WINAPI CTdiCtrl::AddTabLastA(HWND hTdi,LPCSTR tabName,TTab::PContent tabContent,bool makeCurrent,TTab::TCanBeClosed fnCanBeClosed,TTab::TOnClosing fnOnTabClosing){
		// adds a new Tab with given Name and Content as the last Tab in the TDI
		InsertTabA( hTdi, TabCtrl_GetItemCount(hTdi), tabName, tabContent, makeCurrent, fnCanBeClosed, fnOnTabClosing );
	}

	void WINAPI CTdiCtrl::AddTabLastW(HWND hTdi,LPCWSTR tabName,TTab::PContent tabContent,bool makeCurrent,TTab::TCanBeClosed fnCanBeClosed,TTab::TOnClosing fnOnTabClosing){
		// adds a new Tab with given Name and Content as the last Tab in the TDI
		InsertTabW( hTdi, TabCtrl_GetItemCount(hTdi), tabName, tabContent, makeCurrent, fnCanBeClosed, fnOnTabClosing );
	}

	void WINAPI CTdiCtrl::UpdateTabCaptionA(HWND hTdi,TTab::PContent tabContent,LPCSTR tabNewName){
		// updates caption of a Tab identified by its Content (but doesn't switch to the Tab)
		WCHAR bufW[1024];
		::MultiByteToWideChar( CP_ACP,0, tabNewName,-1, bufW,ARRAYSIZE(bufW) );
		return UpdateTabCaptionW( hTdi, tabContent, bufW );
	}

	void WINAPI CTdiCtrl::UpdateTabCaptionW(HWND hTdi,TTab::PContent tabContent,LPCWSTR tabNewName){
		// updates caption of a Tab identified by its Content (but doesn't switch to the Tab)
		for( int i=TabCtrl_GetItemCount(hTdi); i--; )
			if (GetTabContent(hTdi,i)==tabContent){
				// Tab exists
				// . updating the caption
				TCITEMW ti;
					ti.mask=TCIF_TEXT|TCIF_PARAM;
					ti.pszText=(PWCHAR)tabNewName;
				::SendMessageW( hTdi, TCM_SETITEMW, i, (LPARAM)&ti );
				// . redrawing the CurrentTabContent (might have been obscured by the whole TDI after the above change of caption)
				const PTdiInfo pTdiInfo=GET_TDI_INFO(hTdi);
				pTdiInfo->params.fnRepaintContent( pTdiInfo->currentTabContent );
				break;
			}
	}

	void WINAPI CTdiCtrl::RemoveTab(HWND hTdi,int tabId){
		// removes I-th Tab
		TabCtrl_DeleteItem(hTdi,tabId);
	}

	void WINAPI CTdiCtrl::RemoveTab(HWND hTdi,TTab::PContent tabContent){
		// removes Tab identified by its Content
		for( int i=TabCtrl_GetItemCount(hTdi); i--; )
			if (GetTabContent(hTdi,i)==tabContent){
				RemoveTab(hTdi,i);
				break;
			}
	}

	void WINAPI CTdiCtrl::RemoveCurrentTab(HWND hTdi){
		// removes the Tab that is currently switched to
		const int i=TabCtrl_GetCurSel(hTdi);
		const PCTabInfo pti=GetTabInfo(hTdi,i);
		if (pti->fnCanBeClosed!=TDI_TAB_CANCLOSE_NEVER)
			if (pti->fnCanBeClosed(pti->content)) // confirming closing of the Tab
				RemoveTab(hTdi,i);
	}

	void WINAPI CTdiCtrl::SwitchToTab(HWND hTdi,int tabId){
		// switches to the Tab identified by the Content
		GET_TDI_INFO(hTdi)->__switchToTab__( tabId );
	}

	void WINAPI CTdiCtrl::SwitchToTab(HWND hTdi,TTab::PContent tabContent){
		// switches to the Tab identified by the Content
		for( int i=TabCtrl_GetItemCount(hTdi); i--; )
			if (GetTabContent(hTdi,i)==tabContent)
				return SwitchToTab( hTdi, i );
	}

	void WINAPI CTdiCtrl::SwitchToNextTab(HWND hTdi){
		// switches to the Tab to the right of currently switched Tab (modulo)
		int i=1+TabCtrl_GetCurSel(hTdi);
		if (i==TabCtrl_GetItemCount(hTdi))
			i=0;
		SwitchToTab( hTdi, i );
	}

	void WINAPI CTdiCtrl::SwitchToPrevTab(HWND hTdi){
		// switches to the Tab to the left of currently switched Tab (modulo)
		int i=TabCtrl_GetCurSel(hTdi)-1;
		if (i<0)
			i=TabCtrl_GetItemCount(hTdi)-1;
		SwitchToTab( hTdi, i );
	}
