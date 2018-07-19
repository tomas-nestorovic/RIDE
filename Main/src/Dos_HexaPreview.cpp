#include "stdafx.h"

	#define INI_PREVIEW	_T("HexaPreview")

	#define LABEL		_T("Binary")

	#define HEXA_WIDTH	650
	#define HEXA_HEIGHT	250

	#define DOS		pFileManager->tab.dos
	#define IMAGE	DOS->image

	CDos::CHexaPreview *CDos::CHexaPreview::pSingleInstance;

	CDos::CHexaPreview::CHexaPreview(const CFileManagerView *pFileManager)
		// ctor
		// - base
		: CHexaEditor(NULL)
		, CFilePreview( __createWindow__(), INI_PREVIEW, pFileManager, HEXA_WIDTH, HEXA_HEIGHT )
		// - initialization
		, fEmpty((PBYTE)&fEmpty,0) , pFileRW(NULL) {
		pSingleInstance=this;
		// - showing the window
		ShowWindow(SW_SHOW);
		SetEditable(!IMAGE->IsWriteProtected());
		// - showing the first File
		__showNextFile__();
	}

	CDos::CHexaPreview::~CHexaPreview(){
		// dtor
		// - releasing resources
		if (pFileRW)
			delete pFileRW;
		pSingleInstance=NULL;
	}



	HWND CDos::CHexaPreview::__createWindow__(){
		// creates and returns the HexaPreview's window
		Reset(&fEmpty,0,0);
		CreateEx( WS_EX_TOPMOST, HEXAEDITOR_BASE_CLASS, LABEL, WS_CAPTION|WS_SYSMENU|WS_THICKFRAME, 0,0,HEXA_WIDTH,HEXA_HEIGHT, NULL, 0, NULL );
		return m_hWnd;
	}

	void CDos::CHexaPreview::RefreshPreview(){
		// refreshes the Preview (e.g. when switched to another File)
		if (const PCFile file=pdt->entry){
			// . resetting the content of the HexaPreview
			if (pFileRW) delete pFileRW;
			const DWORD size=DOS->__getFileSize__(file);
			Reset( pFileRW=new CFileReaderWriter(DOS,file) ,size,size );
			// . updating the window caption
			TCHAR bufCaption[20+MAX_PATH];
			::wsprintf( bufCaption, LABEL _T(" (%s)"), DOS->GetFileNameWithAppendedExt(file,bufCaption+20) );
			SetWindowText(bufCaption);
		}else
			SetWindowText(LABEL);
		SetWindowPos( NULL, 0,0, 0,0, SWP_NOZORDER|SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED );
	}

	LRESULT CDos::CHexaPreview::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_KEYDOWN:
				// char
				switch (wParam){
					case 'W':
						// toggling the WriteProtection of Image
						if (::GetAsyncKeyState(VK_CONTROL)<0){ // if Ctrl+W pressed
							app.m_pMainWnd->SendMessage( WM_COMMAND, ID_IMAGE_PROTECT ); // toggling the WriteProtection
							SetEditable(!IMAGE->IsWriteProtected()); // setting the possibility edit in HexaEditor
							SetFocus(); // focusing the HexaEditor
						}
						break;
					case VK_ESCAPE:
						// closing the Preview's window
						::DestroyWindow(m_hWnd);
						return 0;
				}
				break;
			case WM_NCDESTROY:
				// closing the Preview's window
				delete pSingleInstance;
				return 0;
		}
		return CHexaEditor::WindowProc(msg,wParam,lParam);
	}
