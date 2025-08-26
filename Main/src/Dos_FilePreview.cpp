#include "stdafx.h"

	#define INI_POSITION	_T("pos")

	#define IMAGE	fileManager.tab.image
	#define DOS		IMAGE->dos

	static constexpr RECT defaultRect={};

	CDos::CFilePreview::CFilePreview(const CWnd *pView,LPCTSTR caption,LPCTSTR iniSection,const CFileManagerView &fileManager,short initialClientWidth,short initialClientHeight,bool keepAspectRatio,DWORD resourceId,CFilePreview **ppSingleManagedInstance)
		// ctor
		// - initialization
		: pView(pView) , ppSingleManagedInstance(ppSingleManagedInstance)
		, caption(caption) , iniSection(iniSection) , fileManager(fileManager)
		, initialClientWidth( keepAspectRatio?initialClientWidth:-initialClientWidth )
		, initialClientHeight( keepAspectRatio?initialClientHeight:-initialClientHeight )
		, directory(DOS->currentDir)
		, pdt( DOS->BeginDirectoryTraversal(DOS->currentDir) ) {
		m_bAutoMenuEnable=FALSE; // we are not set up for that
		// - manage this instance
		if (ppSingleManagedInstance){
			CFilePreview *&psmi=*ppSingleManagedInstance;
			if (psmi)
				psmi->DestroyWindow();
			psmi=this;
		}
		const_cast<CFileManagerView &>(fileManager).SetOwnership(this);
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
		}else
			SetInitialClientSize(1);
		// - if some menu exists, extending it with default items
		if (const CMenu *const pMenu=GetMenu())
			Utils::CRideContextMenu( *pMenu->GetSubMenu(0) ).Append( IDR_DOS_PREVIEW_BASE );
	}





	void CDos::CFilePreview::UpdateCaption(){
		// displays current File in the Preview window caption
		SendMessage(WM_SETTEXT);
	}

	void CDos::CFilePreview::__showNextFile__(){
		// shows next File
		if (!pdt) return;
		if (POSITION pos=fileManager.GetFirstSelectedFilePosition()){
			// next File selected in the FileManager
			while (pos)
				if (fileManager.GetNextSelectedFile(pos)==pdt->entry)
					break;
			if (!pos)
				pos=fileManager.GetFirstSelectedFilePosition();
			pdt->entry=fileManager.GetNextSelectedFile(pos), pdt->entryType=TDirectoryTraversal::CUSTOM;
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
		UpdateCaption();
	}

	void CDos::CFilePreview::__showPreviousFile__(){
		// shows previous File
		if (!pdt) return;
		if (POSITION pos=fileManager.GetLastSelectedFilePosition()){
			// previous File selected in the FileManager
			while (pos)
				if (fileManager.GetPreviousSelectedFile(pos)==pdt->entry)
					break;
			if (!pos)
				pos=fileManager.GetLastSelectedFilePosition();
			pdt->entry=fileManager.GetPreviousSelectedFile(pos), pdt->entryType=TDirectoryTraversal::CUSTOM;
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
		UpdateCaption();
	}

	void CDos::CFilePreview::SetInitialClientSize(BYTE scale){
		// sets client size to initial width and height, applying specified Scale
		CRect r( 0, 0, std::abs(initialClientWidth)*scale*Utils::LogicalUnitScaleFactor, std::abs(initialClientHeight)*scale*Utils::LogicalUnitScaleFactor );
		::AdjustWindowRect( &r, ::GetWindowLong(m_hWnd,GWL_STYLE), ::GetMenu(m_hWnd)!=nullptr );
		SetWindowPos( nullptr, 0,0, r.Width(),r.Height(), SWP_NOZORDER|SWP_NOMOVE );
	}

	BOOL CDos::CFilePreview::PreCreateWindow(CREATESTRUCT &cs){
		// adjusting the instantiation
		if (!__super::PreCreateWindow(cs)) return FALSE;
		cs.dwExStyle&=~WS_EX_CLIENTEDGE;
		return TRUE;
	}

	void CDos::CFilePreview::DestroyWindowSafe(PCFileManagerView pFileManager){
		if (this)
			if (!pFileManager || pFileManager==&fileManager)
				DestroyWindow();
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
						//case WMSZ_TOPLEFT: // commented out as this corner is troublesome; let it be inactive and fall to 'default'
						case WMSZ_TOPRIGHT:
							rc.right=( rc.bottom=r.bottom-r.top+rc.Height()-rw.Height() )*initialClientWidth/initialClientHeight;
							break;
						case WMSZ_LEFT:
						case WMSZ_RIGHT:
						case WMSZ_BOTTOMLEFT:
						case WMSZ_BOTTOMRIGHT:
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
			case WM_NCPAINT:
				__super::WindowProc(msg,wParam,lParam);
				//fallthrough
			case WM_SETTEXT:
				// forces Unicode caption even for an ANSI window
				if (const PCFile file=pdt->entry){
					if (pdt->entryType==TDirectoryTraversal::UNKNOWN)
						return 0;
					const auto tmp=DOS->GetFilePresentationNameAndExt(file).Prepend(_T(" (")).Prepend(caption).Append(L')');
					const auto wndProc0=Utils::SubclassWindow( m_hWnd, ::DefWindowProcW ); // temporarily set Unicode wndproc that can work with Unicode chars
						if (Utils::IsVistaOrNewer())
							::SetWindowText( m_hWnd, // call the generic (eventually ANSI) function ...
								(LPCTSTR)tmp.GetUnicode() // ... but always provide it with Unicode text
							);
						else
							::SetWindowTextW( m_hWnd, // call excplicitly the Unicode function
								tmp.GetUnicode()
							);
					Utils::SubclassWindow( m_hWnd, wndProc0 );
				}else
					return __super::WindowProc( msg, 0, (LPARAM)caption );
				return 0;
			case WM_SIZE:
				// window size changed
				InvalidateRect(nullptr,TRUE);
				break;
			case WM_MOUSEWHEEL:
				// mouse wheel was rotated
				wParam = (short)HIWORD(wParam)>0 ? VK_LEFT : VK_RIGHT; // navigating to the next/previous File
				//fallthrough
			case WM_COMMAND:
				// command processing
				switch (LOWORD(wParam)){
					case ID_ZOOM_FIT:
						// resetting zoom
						SetInitialClientSize(1);
						return 0;
					case ID_NEXT:
						__showNextFile__();
						return 0;
					case ID_PREV:
						__showPreviousFile__();
						return 0;
					case IDCLOSE:
						DestroyWindow();
						return 0;
				}
				break;
			case WM_KEYDOWN:
				// character
				switch (wParam){
					case VK_ESCAPE:
						// closing the Preview
						return SendMessage( WM_COMMAND, IDCLOSE );
					case VK_SPACE:
					case VK_RIGHT:
					case VK_NEXT: // page down
						// next File
						return SendMessage( WM_COMMAND, ID_NEXT );
					case VK_LEFT:
					case VK_PRIOR: // page up
						// previous File
						return SendMessage( WM_COMMAND, ID_PREV );
				}
				break;
			case WM_NCDESTROY:{
				// window is about to be destroyed
				// - manage this instance
				if (ppSingleManagedInstance)
					*ppSingleManagedInstance=nullptr;
				const_cast<CFileManagerView &>(fileManager).RevokeOwnership(this);
				// - save current position for the next time
				WINDOWPLACEMENT wp;
				GetWindowPlacement(&wp);
				for( BYTE b=4; b; ((PLONG)&wp.rcNormalPosition)[--b]/=Utils::LogicalUnitScaleFactor );
				TCHAR buf[80];
				::wsprintf(buf,_T("%d,%d,%d,%d,%d"),wp.rcNormalPosition,wp.showCmd);
				app.WriteProfileString(iniSection,INI_POSITION,buf);
				break;
			}
		}
		return __super::WindowProc(msg,wParam,lParam);
	}
