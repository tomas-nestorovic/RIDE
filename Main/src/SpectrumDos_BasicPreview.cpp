#include "stdafx.h"

	const CSpectrumDos::CBasicPreview *CSpectrumDos::CBasicPreview::pSingleInstance;

	#define INI_PREVIEW	_T("ZxBasic")

	#define PREVIEW_WIDTH_DEFAULT	400
	#define PREVIEW_HEIGHT_DEFAULT	300

	CSpectrumDos::CBasicPreview::CBasicPreview(const CFileManagerView *pFileManager)
		// ctor
		// - base
		: CFilePreview( __createWindow__(), INI_PREVIEW, pFileManager, PREVIEW_WIDTH_DEFAULT, PREVIEW_HEIGHT_DEFAULT ) {
/*
		CreateEx(	0, _T("RICHEDIT50W"), NULL,
					WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_BORDER|WS_VISIBLE|ES_MULTILINE,
					0,0, PREVIEW_WIDTH_DEFAULT,PREVIEW_HEIGHT_DEFAULT,
					NULL, 0, NULL
				);
		//*/
/*
		const HWND hRichEdit=::CreateWindowEx(	0, _T("RICHEDIT50W"), //_T("RichEdit20A"),
												NULL,
												WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_BORDER|WS_VISIBLE|ES_MULTILINE,
												100,100,500,500,
												NULL, 0, app.m_hInstance,
												NULL
											);
		this->Attach(hRichEdit);
		//*/
		// - initialization
		pSingleInstance=this;
		// - showing the first File
		__showNextFile__();
	}

	CSpectrumDos::CBasicPreview::~CBasicPreview(){
		// dtor
		//delete pListingView;
		pSingleInstance=NULL;
	}







	#define DOS		pFileManager->tab.dos
	#define IMAGE	DOS->image

	#define PREVIEW_LABEL	"BASIC listing"

	HWND CSpectrumDos::CBasicPreview::__createWindow__(){
		// creates and returns the Preview's window
/*		
		::InitCommonControls();
		//::CoInitialize(NULL);
		::LoadLibrary("riched20.dll");		// _T("RichEdit20A")
		::LoadLibrary(_T("MSFTEDIT.DLL"));	// _T("RICHEDIT50W"), but must also load "riched20.dll" !!
		//*/
		( pListingView=new CListingView )->CreateEx(
			WS_EX_TOPMOST, NULL, NULL,
			WS_CHILD|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_BORDER|WS_VISIBLE|ES_MULTILINE,
			0,0, PREVIEW_WIDTH_DEFAULT,PREVIEW_HEIGHT_DEFAULT,
			TDI_HWND, 0, NULL
		);
		pListingView->ModifyStyle( WS_CHILD, WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_BORDER|WS_VISIBLE|ES_MULTILINE );
		pListingView->SetParent(NULL);
		return pListingView->m_hWnd;
	}

	void CSpectrumDos::CBasicPreview::RefreshPreview(){
		// refreshes the Preview (e.g. when switched to another File)
		if (const PCFile file=pdt->entry){
			// . resetting the content of the Preview
			pListingView->__loadBasicAndVariablesFromFile__(file);
			// . updating the window caption
			TCHAR bufZx[MAX_PATH],bufCaption[20+MAX_PATH];
			::wsprintf(	bufCaption,
						PREVIEW_LABEL " (%s)",
						TZxRom::ZxToAscii( DOS->GetFileNameWithAppendedExt(file,bufZx),-1, bufCaption+20 )
					);
		}else
			::SetWindowText( pListingView->m_hWnd, PREVIEW_LABEL );
		pListingView->SetWindowPos( NULL, 0,0, 0,0, SWP_NOZORDER|SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED );
	}

	LRESULT CSpectrumDos::CBasicPreview::CListingView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_KEYDOWN:
				// char
				switch (wParam){
					case 'W':
						// toggling the WriteProtection of Image
						if (::GetAsyncKeyState(VK_CONTROL)<0){ // if Ctrl+W pressed
							app.m_pMainWnd->SendMessage( WM_COMMAND, ID_IMAGE_PROTECT ); // toggling the WriteProtection
							SetFocus(); // focusing the Preview
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
		return __super::WindowProc(msg,wParam,lParam);
	}

	void CSpectrumDos::CBasicPreview::CListingView::__loadBasicAndVariablesFromFile__(PCFile file){
		// loads Basic program and/or Basic variables from specified File
		SetRedraw(FALSE);
			// - resetting the content
			SetWindowText(_T(""));			
			// - resetting the paragraph style
			PARAFORMAT pf=GetParaFormatSelection();
				pf.dxOffset=500;
				pf.dxStartIndent=500;
				pf.dxRightIndent=500;
			SetParaFormat(pf);
			// - loading the Basic program (if any)
			SetWindowText(_T("ajhd askjdhk jdkdjahdkd hka hdkadh k hdkjhd akjdh akjd hakjd hakjdh kdh kdh kadh k dhkjdh akjdh kjdh ak"));
		SetRedraw(TRUE);
		Invalidate();
	}
