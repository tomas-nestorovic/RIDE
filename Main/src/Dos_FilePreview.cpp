#include "stdafx.h"

	#define INI_POSITION	_T("pos")

	#define DOS		pFileManager->tab.dos
	#define IMAGE	DOS->image

	CDos::CFilePreview::CFilePreview(HWND hPreviewWnd,LPCTSTR iniSection,const CFileManagerView *pFileManager,WORD initialWindowWidth,WORD initialWindowHeight)
		// ctor
		// - initialization
		: hPreviewWnd(hPreviewWnd) , iniSection(iniSection) , pFileManager(pFileManager)
		, pdt( DOS->BeginDirectoryTraversal() ) {
		// - restoring previous position of Preview on the screen
		const float scaleFactor=TUtils::LogicalUnitScaleFactor;
		const CString s=app.GetProfileString(iniSection,INI_POSITION,_T(""));
		if (!s.IsEmpty()){
			RECT r;
			_stscanf(s,_T("%d,%d,%d,%d"),&r.left,&r.top,&r.right,&r.bottom);
			::SetWindowPos(	hPreviewWnd, 0,
							r.left*scaleFactor, r.top*scaleFactor,
							(r.right-r.left)*scaleFactor, (r.bottom-r.top)*scaleFactor,
							SWP_NOZORDER
						);
		}else{
			RECT r={ 0, 0, initialWindowWidth*scaleFactor, initialWindowHeight*scaleFactor };
			::AdjustWindowRect( &r, ::GetWindowLong(hPreviewWnd,GWL_STYLE), FALSE );
			r.bottom+=::GetSystemMetrics(SM_CYCAPTION);
			::SetWindowPos( hPreviewWnd, 0, 0,0, r.right,r.bottom, SWP_NOZORDER|SWP_NOMOVE );
		}
	}

	CDos::CFilePreview::~CFilePreview(){
		// dtor
		// - freeing resources
		DOS->EndDirectoryTraversal(pdt);
		// - saving current position on the Screen for next time
		RECT r;
		::GetWindowRect(hPreviewWnd,&r);
		const float scaleFactor=TUtils::LogicalUnitScaleFactor;
		for( BYTE b=4; b; ((PINT)&r)[--b]/=scaleFactor );
		TCHAR buf[80];
		::wsprintf(buf,_T("%d,%d,%d,%d"),r);
		app.WriteProfileString(iniSection,INI_POSITION,buf);
	}



	void CDos::CFilePreview::__showNextFile__(){
		// shows next File
		if (POSITION pos=pFileManager->GetFirstSelectedFilePosition()){
			// next File selected in the FileManager
			while (pos)
				if (pFileManager->GetNextSelectedFile(pos)==pdt->entry)
					break;
			if (!pos)
				pos=pFileManager->GetFirstSelectedFilePosition();
			pdt->entry=pFileManager->GetNextSelectedFile(pos);
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
		RefreshPreview();
	}

	void CDos::CFilePreview::__showPreviousFile__(){
		// shows previous File
		if (POSITION pos=pFileManager->GetLastSelectedFilePosition()){
			// previous File selected in the FileManager
			while (pos)
				if (pFileManager->GetPreviousSelectedFile(pos)==pdt->entry)
					break;
			if (!pos)
				pos=pFileManager->GetLastSelectedFilePosition();
			pdt->entry=pFileManager->GetPreviousSelectedFile(pos);
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
		RefreshPreview();
	}
