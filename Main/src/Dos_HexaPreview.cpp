#include "stdafx.h"

	#define INI_PREVIEW	_T("HexaPreview")

	#define LABEL		_T("Binary")

	#define HEXA_WIDTH	650
	#define HEXA_HEIGHT	250

	#define DOS		rFileManager.tab.dos
	#define IMAGE	DOS->image

	CDos::CHexaPreview *CDos::CHexaPreview::pSingleInstance;

	CDos::CHexaPreview::CHexaPreview(const CFileManagerView &rFileManager)
		// ctor
		// - base
		: CFilePreview( &hexaEditor, INI_PREVIEW, rFileManager, HEXA_WIDTH, HEXA_HEIGHT, 0 )
		// - initialization
		, fEmpty((PBYTE)&fEmpty,0) , pFileRW(NULL)
		, hexaEditor(DOS,this) {
		pSingleInstance=this;
		// - creating the HexaEditor view
		hexaEditor.Reset(&fEmpty,0,0);
		hexaEditor.Create( NULL, NULL, AFX_WS_DEFAULT_VIEW&~WS_BORDER|WS_CLIPSIBLINGS, rectDefault, this, AFX_IDW_PANE_FIRST );
		hexaEditor.SetEditable(!IMAGE->IsWriteProtected());
		// - showing the first File
		__showNextFile__();
	}

	CDos::CHexaPreview::~CHexaPreview(){
		// dtor
		// - destroying the HexaEditor window
		hexaEditor.DestroyWindow();
		// - releasing resources
		if (pFileRW)
			delete pFileRW;
		pSingleInstance=NULL;
	}



	void CDos::CHexaPreview::RefreshPreview(){
		// refreshes the Preview (e.g. when switched to another File)
		if (const PCFile file=pdt->entry){
			// . resetting the content of the HexaPreview
			if (pFileRW)
				delete pFileRW;
			const DWORD size=DOS->GetFileOccupiedSize(file);
			hexaEditor.Reset( pFileRW=new CFileReaderWriter(DOS,file) ,size,size );
			// . updating the window caption
			TCHAR bufCaption[20+MAX_PATH];
			::wsprintf( bufCaption, LABEL _T(" (%s)"), DOS->GetFileNameWithAppendedExt(file,bufCaption+20) );
			SetWindowText(bufCaption);
		}else
			SetWindowText(LABEL);
		//SetWindowPos( NULL, 0,0, 0,0, SWP_NOZORDER|SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED|SWP_NOSENDCHANGING );
	}







	CDos::CHexaPreview::CHexaEditorView::CHexaEditorView(PCDos dos,CHexaPreview *pHexaPreview)
		// ctor
		: CHexaEditor( pHexaPreview, dos->formatBoot.sectorLength, __getRecordLabel__ ) {
	}

	LPCTSTR CDos::CHexaPreview::__getRecordLabel__(int recordIndex,PTCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param){
		// populates the Buffer with label for the specified HexaEditor's Record and returns the Buffer
		const CHexaPreview *const pHexaPreview=(CHexaPreview *)param;
		CDos::CFatPath::PCItem pItem; DWORD nItems;
		if (LPCTSTR err=pHexaPreview->pFileRW->fatPath.GetItems(pItem,nItems))
			return err;
		else
			return (pItem+recordIndex)->chs.sectorId.ToString(labelBuffer);
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
