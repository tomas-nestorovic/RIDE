#include "stdafx.h"

	const CCriticalSectorView *CCriticalSectorView::pCurrentlyShown;

	#define IMAGE	tab.image
	#define DOS		IMAGE->dos

	void WINAPI CCriticalSectorView::__updateCriticalSectorView__(PropGrid::PCustomParam){
		// refreshes any current View
		const short iCurSel=PropGrid::GetCurrentlySelectedProperty( pCurrentlyShown->propGrid.m_hWnd );
			CDos::GetFocused()->image->UpdateAllViews(nullptr);
		PropGrid::SetCurrentlySelectedProperty( pCurrentlyShown->propGrid.m_hWnd, iCurSel );
	}

	bool CCriticalSectorView::__isValueBeingEditedInPropertyGrid__(){
		// True <=> some value is right now being edited in PropertyGrid, otherwise False
		return pCurrentlyShown!=nullptr && PropGrid::IsValueBeingEdited();
	}









	CCriticalSectorView::CSectorReaderWriter::CSectorReaderWriter(PCDos dos,RCPhysicalAddress chs)
		// ctor
		: CDos::CFileReaderWriter(dos,chs) {
	}

	void CCriticalSectorView::CSectorReaderWriter::Write(LPCVOID lpBuf,UINT nCount){
		// tries to write given NumberOfBytes from the Buffer to the current Position (increments the Position by the number of Bytes actually written)
		__super::Write(lpBuf,nCount);
		pCurrentlyShown->OnSectorChanging();
	}

	HRESULT CCriticalSectorView::CSectorReaderWriter::Clone(IStream **ppstm){
		// creates an exact copy of this object
		if (ppstm){
			*ppstm=new CSectorReaderWriter(*this);
			return S_OK;
		}else
			return E_INVALIDARG;
	}










	#define PROPGRID_WIDTH_DEFAULT	250

	CCriticalSectorView::CCriticalSectorView(PDos dos,RCPhysicalAddress rChs)
		// ctor
		// - initialization
		: tab(0,0,0,dos->image,this) , splitX(PROPGRID_WIDTH_DEFAULT)
		, hexaEditor(*this) {
		// - reset of HexaEditor's content
		fSectorData.Attach( new CSectorReaderWriter(dos,rChs) );
		const TPhysicalAddress &chs=GetPhysicalAddress();
		WORD sectorDataRealLength=CImage::GetOfficialSectorLength( chs.sectorId.lengthCode ); // initializing just in case the Sector is not found
		IMAGE->GetHealthySectorData( chs, &sectorDataRealLength );
		hexaEditor.Reset( fSectorData, fSectorData, sectorDataRealLength );
	}

	BEGIN_MESSAGE_MAP(CCriticalSectorView,CView)
		ON_WM_CREATE()
		ON_WM_SIZE()
		ON_WM_KILLFOCUS()
		ON_COMMAND(ID_IMAGE_PROTECT,ToggleWriteProtection)
		ON_WM_DESTROY()
	END_MESSAGE_MAP()








	afx_msg int CCriticalSectorView::OnCreate(LPCREATESTRUCT lpcs){
		// window created
		// - base
		if (__super::OnCreate(lpcs)==-1)
			return -1;
		// - creating the Content
		//CCreateContext cc;
		//cc.m_pCurrentDoc=dos->image;
		content.reset( new CSplitterWnd );
			content->CreateStatic(this,1,2,WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS);//WS_CLIPCHILDREN|
			//content->CreateView(0,0,RUNTIME_CLASS(CPropertyGridView),CSize(splitX,0),&cc);
			//const HWND hPropGrid=content->GetDlgItemHwnd( content->IdFromRowCol(0,0) );
				propGrid.CreateEx( 0, app.GetPropGridWndClass(), nullptr, AFX_WS_DEFAULT_VIEW&~WS_BORDER, 0,0,PROPGRID_WIDTH_DEFAULT,300, content->m_hWnd, (HMENU)content->IdFromRowCol(0,0) );
				content->SetColumnInfo(0,Utils::LogicalUnitScaleFactor*PROPGRID_WIDTH_DEFAULT,0);
			//content->CreateView(0,1,RUNTIME_CLASS(CHexaEditor),CSize(),&cc); // commented out as created manually below
				hexaEditor.Update( fSectorData, fSectorData );
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
		PropGrid::RemoveProperty( propGrid.m_hWnd, nullptr );
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
		__super::OnDestroy();
		// - no Boot is currently being displayed
		pCurrentlyShown=nullptr;
	}

	afx_msg void CCriticalSectorView::ToggleWriteProtection(){
		// toggles Image's WriteProtection flag
		IMAGE->ToggleWriteProtection(); // "base"
		__updateLookOfControls__();
	}

	void CCriticalSectorView::__updateLookOfControls__(){
		PropGrid::EnableProperty( propGrid.m_hWnd, nullptr, !IMAGE->IsWriteProtected() );
		hexaEditor.SetEditable( !IMAGE->IsWriteProtected() );
	}

	void CCriticalSectorView::OnSectorChanging() const{
		// custom action performed whenever the Sector data have been modified
		//nop
	}

	void CCriticalSectorView::OnDraw(CDC *pDC){
		// drawing
	}

	void CCriticalSectorView::PostNcDestroy(){
		// self-destruction
		//nop (View destroyed by its owner)
	}

	RCPhysicalAddress CCriticalSectorView::GetPhysicalAddress() const{
		return fSectorData->fatPath.GetHealthyItem(0)->chs;
	}

	void CCriticalSectorView::ChangeToSector(RCPhysicalAddress rChs){
		// changes to a different Sector with the PhysicalAddress specified
		fSectorData->fatPath.GetHealthyItem(0)->chs=rChs;
		fSectorData->SeekToBegin();
	}

	void CCriticalSectorView::MarkSectorAsDirty() const{
		IMAGE->MarkSectorAsDirty(GetPhysicalAddress());
	}
