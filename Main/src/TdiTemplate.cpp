#include "stdafx.h"

	CMainWindow::CTdiTemplate *CMainWindow::CTdiTemplate::pSingleInstance;

	CMainWindow::CTdiTemplate::CTdiTemplate()
		// ctor
		// - base
		: CSingleDocTemplate( IDR_MAINFRAME, nullptr, nullptr, nullptr ){
		m_bAutoDelete=FALSE; // for the Template to be not destroyed when it contains no open documents (no open Images)
		// - creating the MainWindow
		app.m_pMainWnd=new CMainWindow;
		((CMainWindow *)app.m_pMainWnd)->LoadFrame( m_nIDResource, WS_OVERLAPPEDWINDOW|FWS_ADDTOTITLE, nullptr, nullptr );
		pSingleInstance=this;
	}

	CMainWindow::CTdiTemplate::~CTdiTemplate(){
		// dtor
		__closeDocument__();
	}










	CDocument *CMainWindow::CTdiTemplate::__getDocument__() const{
		// returns the main Image (usually a disk)
		return m_pOnlyDoc;
	}

	bool CMainWindow::CTdiTemplate::__closeDocument__(){
		// closes the main Image (usually a disk)
		if (m_pOnlyDoc){
			if (!m_pOnlyDoc->SaveModified()) // if refused to close the document ...
				return false; // ... keeping it open
			if (app.m_pMainWnd){ // may not exist if the application is starting or closing
				TDI_INSTANCE->__closeAllTabsOfFocusedDos__();
				( (CFrameWnd *)app.m_pMainWnd )->OnUpdateFrameTitle(FALSE); // updating the MainWindow's title (now without document)
			}
			if (const PImage image=CImage::GetActive())
				if (image->dos)
					delete image->dos, image->dos=nullptr;
			delete m_pOnlyDoc, m_pOnlyDoc=nullptr;
		}
		return true;
	}

	typedef CDocument *PDocument;

	#if _MFC_VER>=0x0A00
	PDocument CMainWindow::CTdiTemplate::OpenDocumentFile(LPCTSTR lpszPathName,BOOL bAddToMRU,BOOL bMakeVisible){
	#else
	PDocument CMainWindow::CTdiTemplate::OpenDocumentFile(LPCTSTR lpszPathName,BOOL bMakeVisible){
	#endif
		// opens the document stored in specified file
		// - closing current document
		if (!__closeDocument__())
			return nullptr;
		// - creating a new document doesn't require opening any existing document
		if (!lpszPathName)
			return nullptr;
		// - opening the requested document (caller is now to load the document, and then call AddDocument)
		if (const CImage::PCProperties p=CImage::DetermineType(lpszPathName))
			return p->fnInstantiate(nullptr); // instantiating recognized file Image; Null as buffer = one Image represents only one "device" whose name is known at compile-time
		Utils::FatalError(_T("Unknown container to load."));
		return nullptr;
	}
