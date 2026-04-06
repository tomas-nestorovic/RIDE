#include "stdafx.h"

	#define INI_PREVIEW	_T("HexaPreview")

	#define LABEL		_T("Binary")

	#define HEXA_WIDTH	650
	#define HEXA_HEIGHT	250

	#define IMAGE	fileManager.tab.image
	#define DOS		IMAGE->dos

	CDos::CFilePreview *CDos::CHexaPreview::pSingleInstance;

	CDos::CHexaPreview::CHexaPreview(const CFileManagerView &fileManager)
		// ctor
		// - base
		: CFilePreview( &hexaEditor, LABEL, INI_PREVIEW, fileManager, HEXA_WIDTH, HEXA_HEIGHT, false, 0, &pSingleInstance )
		// - initialization
		, hexaEditor(this) {
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
