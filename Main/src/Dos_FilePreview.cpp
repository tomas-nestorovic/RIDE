#include "stdafx.h"

	#define INI_POSITION	_T("pos")

	#define DOS		rFileManager.tab.dos
	#define IMAGE	DOS->image

	static constexpr RECT defaultRect={ 0, 0, 0, 0 };

	CDos::CFilePreview::CFilePreview(const CWnd *pView,LPCTSTR iniSection,const CFileManagerView &rFileManager,short initialClientWidth,short initialClientHeight,bool keepAspectRatio,DWORD resourceId)
		// ctor
		// - initialization
		: pView(pView)
		, iniSection(iniSection) , rFileManager(rFileManager)
		, initialClientWidth( keepAspectRatio?initialClientWidth:-initialClientWidth )
		, initialClientHeight( keepAspectRatio?initialClientHeight:-initialClientHeight )
		, directory(DOS->currentDir)
		, pdt( DOS->BeginDirectoryTraversal(DOS->currentDir) ) {
		// - creating the Preview FrameWindow
		Create(	nullptr, nullptr,
				WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_VISIBLE,
				CRect(defaultRect), nullptr, (LPCTSTR)resourceId, WS_EX_TOPMOST
			);
		// - restoring previous position of Preview on the screen
		const auto &scaleFactor=Utils::LogicalUnitScaleFactor;
		const CString s=app.GetProfileString(iniSection,INI_POSITION,_T(""));
		if (!s.IsEmpty()){
			RECT r; int windowState=SW_NORMAL;
			_stscanf(s,_T("%d,%d,%d,%d,%d"),&r.left,&r.top,&r.right,&r.bottom,&windowState);
			SetWindowPos(	nullptr,
							scaleFactor*r.left, scaleFactor*r.top,
							scaleFactor*(r.right-r.left), scaleFactor*(r.bottom-r.top),
							SWP_NOZORDER
						);
			ShowWindow(windowState); // minimized/maximized/normal
		}else{
			RECT r={ 0, 0, initialClientWidth*scaleFactor, initialClientHeight*scaleFactor };
			::AdjustWindowRect( &r, ::GetWindowLong(m_hWnd,GWL_STYLE), ::GetMenu(m_hWnd)!=nullptr );
			SetWindowPos( nullptr, 0,0, r.right-r.left,r.bottom-r.top, SWP_NOZORDER );
		}
		// - if some menu exists, extending it with default items
		if (const CMenu *const pMenu=GetMenu())
			if (CMenu *const pSubmenu=pMenu->GetSubMenu(0)){
				const Utils::CRideContextMenu defaultMenu( IDR_DOS_PREVIEW_BASE, this );
				for( BYTE i=0; i<defaultMenu.GetMenuItemCount(); i++ )
					if (const auto id=defaultMenu.GetMenuItemID(i)){
						TCHAR buf[80];
						defaultMenu.GetMenuString( i, buf, sizeof(buf)/sizeof(TCHAR), MF_BYPOSITION );
						pSubmenu->AppendMenu( MF_BYCOMMAND|MF_STRING, id, buf );
					}else
						pSubmenu->AppendMenu( MF_SEPARATOR );
			}
	}





	void CDos::CFilePreview::__showNextFile__(){
		// shows next File
		if (!pdt) return;
		if (POSITION pos=rFileManager.GetFirstSelectedFilePosition()){
			// next File selected in the FileManager
			while (pos)
				if (rFileManager.GetNextSelectedFile(pos)==pdt->entry)
					break;
			if (!pos)
				pos=rFileManager.GetFirstSelectedFilePosition();
			pdt->entry=rFileManager.GetNextSelectedFile(pos);
		}else{
			// next File (any)
			PFile next=nullptr;
				while (pdt->AdvanceToNextEntry())
					if (pdt->entryType==TDirectoryTraversal::FILE){
						next=pdt->entry;
						break;
					}
				if (!next)
					for( pdt=DOS->BeginDirectoryTraversal(directory); pdt->AdvanceToNextEntry(); )
						if (pdt->entryType==TDirectoryTraversal::FILE){
							next=pdt->entry;
							break;
						}
			pdt->entry=next;
		}
		RecalcLayout();
		RefreshPreview();
	}

	void CDos::CFilePreview::__showPreviousFile__(){
		// shows previous File
		if (!pdt) return;
		if (POSITION pos=rFileManager.GetLastSelectedFilePosition()){
			// previous File selected in the FileManager
			while (pos)
				if (rFileManager.GetPreviousSelectedFile(pos)==pdt->entry)
					break;
			if (!pos)
				pos=rFileManager.GetLastSelectedFilePosition();
			pdt->entry=rFileManager.GetPreviousSelectedFile(pos);
		}else{
			// previous File (any)
			PFile prev=nullptr;
				const PFile curr=pdt->entry;
				for( pdt=DOS->BeginDirectoryTraversal(directory); pdt->AdvanceToNextEntry(); )
					if (pdt->entryType==TDirectoryTraversal::FILE){
						if (pdt->entry==curr){
							if (prev) // doesn't exist if showing the first File
								// setting the "cursor" to Previous File (currently points to Current File)
								for( pdt=DOS->BeginDirectoryTraversal(directory); pdt->AdvanceToNextEntry() && pdt->entry!=prev; );
							break;
						}
						prev=pdt->entry;
					}
				if (!prev)
					while (pdt->AdvanceToNextEntry())
						if (pdt->entryType==TDirectoryTraversal::FILE)
							prev=pdt->entry;
			pdt->entry=prev;
		}
		RecalcLayout();
		RefreshPreview();
	}

	BOOL CDos::CFilePreview::PreCreateWindow(CREATESTRUCT &cs){
		// adjusting the instantiation
		if (!__super::PreCreateWindow(cs)) return FALSE;
		cs.dwExStyle&=~WS_EX_CLIENTEDGE;
		return TRUE;
	}

	BOOL CDos::CFilePreview::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:
				// update
				switch (nID){
					case ID_NEXT:
					case ID_PREV:
					case IDCLOSE:
						((CCmdUI *)pExtra)->Enable(TRUE);
						return TRUE;
				}
				break;
			case CN_COMMAND:
				// command
				switch (nID){
					case ID_NEXT:
						__showNextFile__();
						return TRUE;
					case ID_PREV:
						__showPreviousFile__();
						return TRUE;
					case IDCLOSE:
						DestroyWindow();
						return TRUE;
				}
				break;
		}
		return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	}

	LRESULT CDos::CFilePreview::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_ACTIVATE:
			case WM_SETFOCUS:
				// window has received focus
				// - passing the focus over to the View
				if (pView){
					::SetFocus(pView->m_hWnd);
					return 0;
				}else
					break;
			case WM_SIZING:
				// window size changing
				if (initialClientWidth>0 && initialClientHeight>0){
					// wanted to keep initial client size ratio
					RECT &r=*(LPRECT)lParam;
					CRect rw, rc;
					GetWindowRect(&rw), GetClientRect(&rc);
					switch (wParam){
						case WMSZ_TOP:
						case WMSZ_BOTTOM:
							rc.right=( rc.bottom=r.bottom-r.top+rc.Height()-rw.Height() )*initialClientWidth/initialClientHeight;
							break;
						case WMSZ_LEFT:
						case WMSZ_RIGHT:
							rc.bottom=( rc.right=r.right-r.left+rc.Width()-rw.Width() )*initialClientHeight/initialClientWidth;
							break;
						default:
							r=rw;
							return TRUE;
					}
					::AdjustWindowRect( &rc, ::GetWindowLong(m_hWnd,GWL_STYLE), ::GetMenu(m_hWnd)!=nullptr );
					rc.OffsetRect( r.left-rc.left, r.top-rc.top );
					r=rc;
					return TRUE;
				}
				break;
			/*case WM_WINDOWPOSCHANGING:{
				// window size changing
				LPWINDOWPOS wp=(LPWINDOWPOS)lParam;
				wp->cx=300;
				return TRUE;
				break;
			}*/
			case WM_SIZE:
				// window size changed
				InvalidateRect(nullptr,TRUE);
				break;
			case WM_MOUSEWHEEL:
				// mouse wheel was rotated
				wParam = (short)HIWORD(wParam)>0 ? VK_LEFT : VK_RIGHT; // navigating to the next/previous File
				//fallthrough
			case WM_KEYDOWN:
				// character
				switch (wParam){
					case VK_ESCAPE:
						// closing the Preview
						DestroyWindow();
						return 0;
					case VK_SPACE:
					case VK_RIGHT:
					case VK_NEXT: // page down
						// next File
						__showNextFile__();
						break;
					case VK_LEFT:
					case VK_PRIOR: // page up
						// previous File
						__showPreviousFile__();
						break;
				}
				break;
			case WM_NCDESTROY:{
				// window is about to be destroyed
				// - saving current position on the Screen for next time
				WINDOWPLACEMENT wp={ sizeof(wp) };
				GetWindowPlacement(&wp);
				for( BYTE b=4; b; ((PINT)&wp.rcNormalPosition)[--b]/=Utils::LogicalUnitScaleFactor );
				TCHAR buf[80];
				::wsprintf(buf,_T("%d,%d,%d,%d,%d"),wp.rcNormalPosition,wp.showCmd);
				app.WriteProfileString(iniSection,INI_POSITION,buf);
				break;
			}
		}
		return __super::WindowProc(msg,wParam,lParam);
	}
