#include "stdafx.h"

	#define INI_DISKBROWSER	_T("DiskBrowser")

	#define DOS		tab.dos
	#define IMAGE	DOS->image


	CDiskBrowserView::CDiskBrowserView(PDos dos)
		// ctor
		// - base
		: CHexaEditor(this)
		// - initialization
		, tab( 0, IDR_HEXAEDITOR, ID_CYLINDER, dos, this )
		, iScrollY(0) , f(nullptr) {
	}

	BEGIN_MESSAGE_MAP(CDiskBrowserView,CHexaEditor)
		ON_WM_CREATE()
		ON_COMMAND(ID_IMAGE_PROTECT,__toggleWriteProtection__)
		ON_COMMAND(ID_FILE_CLOSE,__closeView__)
		ON_WM_DESTROY()
	END_MESSAGE_MAP()







	afx_msg int CDiskBrowserView::OnCreate(LPCREATESTRUCT lpcs){
		// window created
		// - base
		if (__super::OnCreate(lpcs)==-1)
			return -1;
		// - displaying the content
		OnUpdate(nullptr,0,nullptr);
		// - recovering the Scroll position and repainting the view (by setting its editability)
		SetScrollPos( SB_VERT, iScrollY );
		SetEditable( !IMAGE->IsWriteProtected() );
		return 0;
	}

	void CDiskBrowserView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		if (f)
			delete f;
		f=IMAGE->CreateSectorDataSerializer(this);
		Reset( f, f->GetLength(), f->GetLength() );
	}

	afx_msg void CDiskBrowserView::OnDestroy(){
		// window destroyed
		// - saving Scroll position for later
		iScrollY=GetScrollPos(SB_VERT);
		// - disposing the underlying File
		delete f, f=nullptr;
		// - base
		CView::OnDestroy();
	}

	afx_msg void CDiskBrowserView::__toggleWriteProtection__(){
		// toggles Image's WriteProtection flag
		IMAGE->__toggleWriteProtection__(); // "base"
		SetEditable( !IMAGE->IsWriteProtected() );
	}

	afx_msg void CDiskBrowserView::__closeView__(){
		CTdiCtrl::RemoveCurrentTab( TDI_HWND );
	}
