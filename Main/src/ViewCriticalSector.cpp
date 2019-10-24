#include "stdafx.h"

	const CCriticalSectorView *CCriticalSectorView::pCurrentlyShown;

	#define DOS		tab.dos
	#define IMAGE	DOS->image

	void WINAPI CCriticalSectorView::__updateCriticalSectorView__(CPropGridCtrl::PCustomParam){
		// refreshes any current View
		const short iCurSel=CPropGridCtrl::GetCurrentlySelectedProperty( pCurrentlyShown->propGrid.m_hWnd );
			CDos::GetFocused()->image->UpdateAllViews(nullptr);
		CPropGridCtrl::SetCurrentlySelectedProperty( pCurrentlyShown->propGrid.m_hWnd, iCurSel );
	}

	bool CCriticalSectorView::__isValueBeingEditedInPropertyGrid__(){
		// True <=> some value is right now being edited in PropertyGrid, otherwise False
		return pCurrentlyShown!=nullptr && CPropGridCtrl::IsValueBeingEdited();
	}









	#define PROPGRID_WIDTH_DEFAULT	250

	CCriticalSectorView::CCriticalSectorView(PDos dos,RCPhysicalAddress rChs)
		// ctor
		: tab(0,0,0,dos,this) , splitX(PROPGRID_WIDTH_DEFAULT) , chs(rChs)
		, fSectorData(dos,rChs) , hexaEditor(this) {
	}

	BEGIN_MESSAGE_MAP(CCriticalSectorView,CView)
		ON_WM_CREATE()
		ON_WM_SIZE()
		ON_WM_KILLFOCUS()
		ON_COMMAND(ID_IMAGE_PROTECT,__toggleWriteProtection__)
		ON_WM_DESTROY()
	END_MESSAGE_MAP()








	afx_msg int CCriticalSectorView::OnCreate(LPCREATESTRUCT lpcs){
		// window created
		// - base
		if (__super::OnCreate(lpcs)==-1)
			return -1;
		// - getting Boot Sector data
		WORD sectorDataRealLength=0; // initializing just in case the Sector is not found
		IMAGE->GetHealthySectorData(chs,&sectorDataRealLength);
		// - creating the Content
		//CCreateContext cc;
		//cc.m_pCurrentDoc=dos->image;
		content.reset( new CSplitterWnd );
			content->CreateStatic(this,1,2,WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS);//WS_CLIPCHILDREN|
			//content->CreateView(0,0,RUNTIME_CLASS(CPropertyGridView),CSize(splitX,0),&cc);
			//const HWND hPropGrid=content->GetDlgItem( content->IdFromRowCol(0,0) )->m_hWnd;
				propGrid.CreateEx( 0, CPropGridCtrl::GetWindowClass(app.m_hInstance), nullptr, AFX_WS_DEFAULT_VIEW&~WS_BORDER, 0,0,PROPGRID_WIDTH_DEFAULT,300, content->m_hWnd, (HMENU)content->IdFromRowCol(0,0) );
				content->SetColumnInfo(0,PROPGRID_WIDTH_DEFAULT*Utils::LogicalUnitScaleFactor,0);
			//content->CreateView(0,1,RUNTIME_CLASS(CHexaEditor),CSize(),&cc); // commented out as created manually below
				hexaEditor.Reset( &fSectorData, sectorDataRealLength, sectorDataRealLength );
				hexaEditor.Create( nullptr, nullptr, AFX_WS_DEFAULT_VIEW&~WS_BORDER|WS_CLIPSIBLINGS, CFrameWnd::rectDefault, content.get(), content->IdFromRowCol(0,1) );
				//hexaEditor.CreateEx( 0, HEXAEDITOR_BASE_CLASS, nullptr, AFX_WS_DEFAULT_VIEW&~WS_BORDER, RECT(), nullptr, content->IdFromRowCol(0,1), nullptr );
		OnSize( SIZE_RESTORED, lpcs->cx, lpcs->cy );
		// - populating the PropertyGrid with values from Boot Sector
		OnUpdate(nullptr,0,nullptr);
		// - manually setting that none of the Splitter cells is the actual View
		//nop (see OnKillFocus)
		// - currently it's this Boot that's displayed
		pCurrentlyShown=this;
		return 0;
	}

	void CCriticalSectorView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		// - clearing the PropertyGrid
		CPropGridCtrl::RemoveProperty( propGrid.m_hWnd, nullptr );
		// - planning repainting of the HexaEditor
		hexaEditor.Invalidate();
		// - reflecting write-protection into the look of controls
		__updateLookOfControls__();
	}

	afx_msg void CCriticalSectorView::OnSize(UINT nType,int cx,int cy){
		// window size changed
		content->SetWindowPos( nullptr, 0,0, cx,cy, SWP_NOZORDER|SWP_NOMOVE );
	}

	afx_msg void CCriticalSectorView::OnKillFocus(CWnd *newFocus){
		// window lost focus
		// - manually setting that this Boot is still the active View, regardless of the lost focus
		((CFrameWnd *)app.m_pMainWnd)->SetActiveView(this);
	}

	afx_msg void CCriticalSectorView::OnDestroy(){
		// window destroyed
		// - saving the Splitter's X position for later
		RECT r;
		content->GetDlgItem( content->IdFromRowCol(0,0) )->GetClientRect(&r);
		splitX=r.right;
		// - disposing the Content
		content.reset();
		// - base
		CView::OnDestroy();
		// - no Boot is currently being displayed
		pCurrentlyShown=nullptr;
	}

	afx_msg void CCriticalSectorView::__toggleWriteProtection__(){
		// toggles Image's WriteProtection flag
		IMAGE->__toggleWriteProtection__(); // "base"
		__updateLookOfControls__();
	}

	void CCriticalSectorView::__updateLookOfControls__(){
		CPropGridCtrl::EnableProperty( propGrid.m_hWnd, nullptr, !IMAGE->IsWriteProtected() );
		hexaEditor.SetEditable( !IMAGE->IsWriteProtected() );
	}

	void CCriticalSectorView::OnDraw(CDC *pDC){
		// drawing
	}

	void CCriticalSectorView::PostNcDestroy(){
		// self-destruction
		//nop (View destroyed by its owner)
	}

	RCPhysicalAddress CCriticalSectorView::GetPhysicalAddress() const{
		return chs;
	}

	void CCriticalSectorView::ChangeToSector(RCPhysicalAddress rChs){
		// changes to a different Sector with the PhysicalAddress specified
		CDos::CFileReaderWriter fNewSectorData( DOS, chs=rChs );
		BYTE tmp[sizeof(CDos::CFileReaderWriter)];
		::memcpy( tmp, &fSectorData, sizeof(CDos::CFileReaderWriter) );
		::memcpy( &fSectorData, &fNewSectorData, sizeof(CDos::CFileReaderWriter) );
		::memcpy( &fNewSectorData, tmp, sizeof(CDos::CFileReaderWriter) );
		//TODO: the above is nasty but easy and fast - find a formally better solution?
	}

	void CCriticalSectorView::MarkSectorAsDirty() const{
		IMAGE->MarkSectorAsDirty(chs);
	}