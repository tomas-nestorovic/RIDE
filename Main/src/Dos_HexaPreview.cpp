#include "stdafx.h"

	#define INI_PREVIEW	_T("HexaPreview")

	#define LABEL		_T("Binary")

	#define HEXA_WIDTH	650
	#define HEXA_HEIGHT	250

	#define IMAGE	rFileManager.tab.image
	#define DOS		IMAGE->dos

	CDos::CHexaPreview *CDos::CHexaPreview::pSingleInstance;

	CDos::CHexaPreview::CHexaPreview(const CFileManagerView &rFileManager)
		// ctor
		// - base
		: CFilePreview( &hexaEditor, LABEL, INI_PREVIEW, rFileManager, HEXA_WIDTH, HEXA_HEIGHT, false, 0 )
		// - initialization
		, hexaEditor(DOS,this) {
		pSingleInstance=this;
		// - creating the HexaEditor view
		hexaEditor.Create( nullptr, nullptr, AFX_WS_DEFAULT_VIEW&~WS_BORDER|WS_CLIPSIBLINGS, rectDefault, this, AFX_IDW_PANE_FIRST );
		hexaEditor.SetEditable(!IMAGE->IsWriteProtected());
		// - showing the first File
		__showNextFile__();
	}

	CDos::CHexaPreview::~CHexaPreview(){
		// dtor
		// - destroying the HexaEditor window
		hexaEditor.DestroyWindow();
		pSingleInstance=nullptr;
	}



	void CDos::CHexaPreview::RefreshPreview(){
		// refreshes the Preview (e.g. when switched to another File)
		if (const PCFile file=pdt->entry){
			// . resetting the content of the HexaPreview
			CFileReaderWriter *const frw=new CFileReaderWriter(DOS,file);
				hexaEditor.Reset( frw, frw, DOS->GetFileOccupiedSize(file) );
			frw->Release();
		}
		//SetWindowPos( nullptr, 0,0, 0,0, SWP_NOZORDER|SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED|SWP_NOSENDCHANGING );
		hexaEditor.SetFocus();
	}







	CDos::CHexaPreview::CHexaEditorView::CHexaEditorView(PCDos dos,CHexaPreview *pHexaPreview)
		// ctor
		: CFileReaderWriter::CHexaEditor(pHexaPreview) {
	}

	LRESULT CDos::CHexaPreview::CHexaEditorView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_KEYDOWN:
				// char
				switch (wParam){
					case 'W':
						// toggling the WriteProtection of Image
						if (::GetAsyncKeyState(VK_CONTROL)<0){ // if Ctrl+W pressed
							app.m_pMainWnd->SendMessage( WM_COMMAND, ID_IMAGE_PROTECT ); // toggling the WriteProtection
							SetEditable(!pSingleInstance->IMAGE->IsWriteProtected()); // setting the possibility edit in HexaEditor
							SetFocus(); // focusing the HexaEditor
						}
						break;
					case VK_ESCAPE:
						// closing the Preview's window
						pSingleInstance->DestroyWindow();
						return 0;
				}
				break;
		}
		return __super::WindowProc(msg,wParam,lParam);
	}
