#include "stdafx.h"

	#define INI_POSITION	_T("pos")

	#define DOS		rFileManager.tab.dos
	#define IMAGE	DOS->image

	static const RECT defaultRect={ 0, 0, 0, 0 };

	CDos::CFilePreview::CFilePreview(const CWnd *pView,LPCTSTR iniSection,const CFileManagerView &rFileManager,WORD initialWindowWidth,WORD initialWindowHeight,DWORD resourceId)
		// ctor
		// - initialization
		: pView(pView)
		, iniSection(iniSection) , rFileManager(rFileManager)
		, pdt( DOS->BeginDirectoryTraversal() ) {
		// - creating the Preview FrameWindow
		Create(	NULL, NULL,
				WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_VISIBLE,
				CRect(defaultRect), NULL, (LPCTSTR)resourceId, WS_EX_TOPMOST
			);
		// - restoring previous position of Preview on the screen
		const float scaleFactor=TUtils::LogicalUnitScaleFactor;
		const CString s=app.GetProfileString(iniSection,INI_POSITION,_T(""));
		if (!s.IsEmpty()){
			RECT r;
			_stscanf(s,_T("%d,%d,%d,%d"),&r.left,&r.top,&r.right,&r.bottom);
			SetWindowPos(	NULL,
							r.left*scaleFactor, r.top*scaleFactor,
							(r.right-r.left)*scaleFactor, (r.bottom-r.top)*scaleFactor,
							SWP_NOZORDER
						);
		}else{
			RECT r={ 0, 0, initialWindowWidth*scaleFactor, initialWindowHeight*scaleFactor };
			::AdjustWindowRect( &r, ::GetWindowLong(m_hWnd,GWL_STYLE), FALSE );
			r.bottom+=::GetSystemMetrics(SM_CYCAPTION);
			SetWindowPos( NULL, 0,0, r.right-r.left,r.bottom-r.top, SWP_NOZORDER );
		}
	}

	CDos::CFilePreview::~CFilePreview(){
		// dtor
		// - freeing resources
		DOS->EndDirectoryTraversal(pdt);
	}




	void CDos::CFilePreview::__showNextFile__(){
		// shows next File
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
			PFile next=NULL;
				while (pdt->AdvanceToNextEntry())
					if (pdt->entryType==TDirectoryTraversal::FILE){
						next=pdt->entry;
						break;
					}
				if (!next)
					for( DOS->EndDirectoryTraversal(pdt),pdt=DOS->BeginDirectoryTraversal(); pdt->AdvanceToNextEntry(); )
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
			PFile prev=NULL;
				const PFile curr=pdt->entry;
				for( DOS->EndDirectoryTraversal(pdt),pdt=DOS->BeginDirectoryTraversal(); pdt->AdvanceToNextEntry(); )
					if (pdt->entryType==TDirectoryTraversal::FILE){
						if (pdt->entry==curr){
							if (prev) // doesn't exist if showing the first File
								// setting the "cursor" to Previous File (currently points to Current File)
								for( DOS->EndDirectoryTraversal(pdt),pdt=DOS->BeginDirectoryTraversal(); pdt->AdvanceToNextEntry() && pdt->entry!=prev; );
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
				RECT r;
				GetWindowRect(&r);
				for( BYTE b=4; b; ((PINT)&r)[--b]/=TUtils::LogicalUnitScaleFactor );
				TCHAR buf[80];
				::wsprintf(buf,_T("%d,%d,%d,%d"),r);
				app.WriteProfileString(iniSection,INI_POSITION,buf);
				break;
			}
		}
		return __super::WindowProc(msg,wParam,lParam);
	}
