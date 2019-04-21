#include "stdafx.h"

	CMainWindow::CTdiTemplate *CMainWindow::CTdiTemplate::pSingleInstance;

	CMainWindow::CTdiTemplate::CTdiTemplate()
		// ctor
		// - base
		: CSingleDocTemplate( IDR_MAINFRAME, nullptr, nullptr, nullptr ){
		empty.m_bAutoDelete=FALSE; // Empty document destroyed along with this Template
		m_bAutoDelete=FALSE; // for the Template to be not destroyed when it contains no open documents (no open Images)
		// - creating the MainWindow
		app.m_pMainWnd=new CMainWindow;
		((CMainWindow *)app.m_pMainWnd)->LoadFrame( m_nIDResource, WS_OVERLAPPEDWINDOW|FWS_ADDTOTITLE, nullptr, nullptr );
		pSingleInstance=this;
	}

	CMainWindow::CTdiTemplate::~CTdiTemplate(){
		// dtor
		if (m_pOnlyDoc)
			delete m_pOnlyDoc;
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
			delete m_pOnlyDoc, m_pOnlyDoc=nullptr;
		}
		return true;
	}

	typedef CDocument *PDocument;

	PDocument CMainWindow::CTdiTemplate::OpenDocumentFile(LPCTSTR lpszPathName,BOOL bMakeVisible){
		// opens the document stored in specified file
		// - closing current document
		if (!__closeDocument__())
			return nullptr;
		// - opening the requested document
		//nop (caller is to open and load the document, and then call AddDocument)
		return &empty;
	}
