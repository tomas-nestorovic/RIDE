#include "stdafx.h"
#include "Charting.h"

namespace Charting
{
	const CChartView::TMargin CChartView::TMargin::None;
	const CChartView::TMargin CChartView::TMargin::Default={ 20, 20, 20, 20 };





	void CChartView::CHistogram::AddValue(TLogValue value){
		// increases by one the number of occurences of Value
		auto it=find(value);
		if (it!=cend())
			it->second++;
		else
			insert( std::make_pair(value,1) );
	}

	TIndex CChartView::CHistogram::GetCount(TLogValue value) const{
		// returns the number of occurences of Value (or zero)
		const auto it=find(value);
		if (it!=cend())
			return it->second;
		else
			return 0;
	}






	CChartView::CGraphics::CGraphics()
		// ctor
		: visible(true)
		, name(nullptr) { // caller eventually overrides
	}

	PCItem CChartView::CGraphics::GetItem(TIndex i,const POINT &ptClient) const{
		// returns pointer to an item identified by the Index and input client position
		return nullptr;
	}

	TIndex CChartView::CGraphics::GetNearestItemIndex(const CDisplayInfo &,const POINT &ptClient,TLogValue &rOutDistance) const{
		// returns the Index of the item closest to the input position
		ASSERT( !SupportsCursorSnapping() );
		return rOutDistance=LogValueMax, -1; // no known item to snap cursor to
	}






	void CChartView::CXyGraphics::GetDrawingLimits(WORD percentile,TLogValue &rOutMaxX,TLogValue &rOutMaxY) const{
		// returns the XY position of the last item still to be drawn with specified Percentile
		//nop (not applicable)
	}






	CChartView::CXyPointSeries::CXyPointSeries(TIndex nPoints,PCLogPoint points,HPEN hVertexPen)
		// ctor
		: nPoints(nPoints) , points(points)
		, hPen(hVertexPen) {
	}

	void CChartView::CXyPointSeries::GetDrawingLimits(WORD percentile,TLogValue &rOutMaxX,TLogValue &rOutMaxY) const{
		// sets corresponding outputs to the last item still to be drawn with specified Percentile
		const CHistogram h=CreateYxHistogram();
		int sum=0,const sumMax=::MulDiv( nPoints, percentile, 10000 );
		rOutMaxX=std::max( rOutMaxX, points[nPoints-1].x );
		rOutMaxY=1;
		for( auto it=h.cbegin(); it!=h.cend()&&sum<sumMax; sum+=it++->second )
			rOutMaxY=it->first;
	}

	PCItem CChartView::CXyPointSeries::GetItem(TIndex i,const POINT &ptClient) const{
		// returns pointer to an item identified by the Index and input client position
		ASSERT( 0<=i && i<nPoints );
		return points+i;
	}

	TIndex CChartView::CXyPointSeries::GetItemCount() const{
		// returns the number of available items
		return nPoints;
	}

	void CChartView::CXyPointSeries::DrawAsync(const CPainter &p,HDC dc,const CActionProgress &ap) const{
		// asynchronous drawing; always compare actual drawing ID with the one on start
		const CXyDisplayInfo &di=*(const CXyDisplayInfo *)&p.di;
		const HGDIOBJ hPen0=::SelectObject( dc, hPen );
			constexpr TIndex Stride=64;
			for( TIndex i=0; i<Stride; i++ )
				for( TIndex j=i+0; j<nPoints; j+=Stride,ap.IncrementProgress() ){
					EXCLUSIVELY_LOCK(p);
					if (ap.Cancelled)
						break;
					if (points[j].y>di.GetAxisY().GetLength())
						continue;
					const POINT &&pt=di.GetClientUnits( points[j] );
					::MoveToEx( dc, pt.x, pt.y, nullptr );
					::LineTo( dc, pt.x+1, pt.y );
				}
		::SelectObject( dc, hPen0 );
	}

	CChartView::CHistogram CChartView::CXyPointSeries::CreateYxHistogram(TLogValue mergeFilter) const{
		// creates histogram of Y values
		CHistogram tmp;
		for( TIndex n=nPoints; n>0; tmp.AddValue(points[--n].y) );
		if (mergeFilter>0)
			for( auto it=tmp.begin(); it!=tmp.end(); ){
				auto itNext=it;
				if (++itNext==tmp.end())
					break;
				if (itNext->first-it->first>mergeFilter) // values far away enough?
					it=itNext;
				else // add lower count to bigger count
					if (it->second>=itNext->second){
						it->second+=itNext->second;
						tmp.erase(itNext);
					}else{
						itNext->second+=it->second;
						tmp.erase(it);
						it=itNext;
					}
			}
		return tmp;
	}






	CChartView::CXyBrokenLineSeries::CXyBrokenLineSeries(TIndex nPoints,PCLogPoint points,HPEN hLinePen)
		// ctor
		: CXyPointSeries( nPoints, points, hLinePen ) {
	}

	void CChartView::CXyBrokenLineSeries::DrawAsync(const CPainter &p,HDC dc,const CActionProgress &ap) const{
		// asynchronous drawing; always compare actual drawing ID with the one on start
		if (nPoints<2)
			return;
		const CXyDisplayInfo &di=*(const CXyDisplayInfo *)&p.di;
		const HGDIOBJ hPen0=::SelectObject( dc, hPen );
			POINT pt=di.GetClientUnits( *points );
			::MoveToEx( dc, pt.x, pt.y, nullptr );
			for( TIndex j=1; j<nPoints; j++,ap.IncrementProgress() ){
				EXCLUSIVELY_LOCK(p);
				if (ap.Cancelled)
					break;
				pt=di.GetClientUnits( points[j] );
				::LineTo( dc, pt.x+1, pt.y );
			}
		::SelectObject( dc, hPen0 );
	}






	CChartView::CXyOrderedBarSeries::CXyOrderedBarSeries(TIndex nPoints,PCLogPoint points,HPEN hLinePen,LPCTSTR name)
		// ctor
		: CXyPointSeries( nPoints, points, hLinePen ) {
		this->name=name;
	}

	void CChartView::CXyOrderedBarSeries::GetDrawingLimits(WORD percentile,TLogValue &rOutMaxX,TLogValue &rOutMaxY) const{
		// sets corresponding outputs to the last item still to be drawn with specified Percentile
		LONGLONG ySum=0;
		for( TIndex i=nPoints; i>0; ySum+=points[--i].y );
		ySum=ySum*percentile/10000; // estimation of percentile
		rOutMaxX = rOutMaxY = 1;
		for( TIndex i=0; i<nPoints&&ySum>0; i++ ){
			const TLogPoint &pt=points[i];
			ySum-=pt.y;
			if (pt.x>rOutMaxX)
				rOutMaxX=pt.x;
			if (pt.y>rOutMaxY)
				rOutMaxY=pt.y;
		}
	}

	TIndex CChartView::CXyOrderedBarSeries::GetNearestItemIndex(const CDisplayInfo &di,const POINT &ptClient,TLogValue &rOutDistance) const{
		// returns the Index of the item closest to the input position
		if (!nPoints)
			return	__super::GetNearestItemIndex( di, ptClient, rOutDistance );
		const CXyDisplayInfo &xydi=*(const CXyDisplayInfo *)&di;
		const TLogPoint ptClientValue={ xydi.GetAxisX().GetValue(ptClient), xydi.GetAxisY().GetValue(ptClient) };
		if (nPoints==1)
			return	rOutDistance=ptClientValue.ManhattanDistance(*points), 0;
		TIndex L=0, R=nPoints-1;
		for( TIndex M; L+1<R; )
			if (ptClientValue.x<points[ M=(L+R)/2 ].x)
				R=M;
			else
				L=M;
		const TLogPoint &ptL=points[L],&ptR=points[R];
		if (ptClientValue.x-ptL.x<ptR.x-ptClientValue.x) // cursor closer to the left Point?
			return rOutDistance=ptClientValue.ManhattanDistance(ptL), L;
		else
			return rOutDistance=ptClientValue.ManhattanDistance(ptR), R;
	}

	void CChartView::CXyOrderedBarSeries::DrawAsync(const CPainter &p,HDC dc,const CActionProgress &ap) const{
		// asynchronous drawing; always compare actual drawing ID with the one on start
		const CXyDisplayInfo &di=*(const CXyDisplayInfo *)&p.di;
		const HGDIOBJ hPen0=::SelectObject( dc, hPen );
			for( TIndex i=0; i<nPoints; i++,ap.IncrementProgress() ){
				const TLogPoint &pt=points[i];
				if (pt.x>di.GetAxisX().GetLength())
					break;
				EXCLUSIVELY_LOCK(p);
				if (ap.Cancelled)
					break;
				const POINT &&ptT=di.GetClientUnits( pt );
				::MoveToEx( dc, ptT.x, ptT.y, nullptr );
				const POINT &&ptB=di.GetClientUnits( pt.x, 0 );
				::LineTo( dc, ptB.x, ptB.y );
			}
		::SelectObject( dc, hPen0 );
	}






	CChartView::CDisplayInfo::CDisplayInfo(UINT menuResourceId,RCMargin margin,const PCGraphics graphics[],BYTE nGraphics)
		// ctor
		: CActionProgressBar(drawingCancelled)
		, menuResourceId(menuResourceId)
		, margin(margin)
		, graphics(graphics) , nGraphics(nGraphics)
		, snapToNearestItem(true) , drawingCancelled(false) {
		::ZeroMemory( &snapped, sizeof(snapped) );
	}

	CChartView::CDisplayInfo::PCSnappedItem CChartView::CDisplayInfo::DrawCursor(HDC,const POINT &ptClient){
		// draws cursor eventually Snapped to an Item (or Null)
		snapped.graphics=nullptr;
		if (snapToNearestItem){
			TLogValue nearest=LogValueMax;
			for( BYTE i=nGraphics; i>0; ){
				const PCGraphics g=graphics[--i];
				if (!g->SupportsCursorSnapping())
					continue;
				TLogValue distance=LogValueMax;
				const TIndex iItem=g->GetNearestItemIndex( *this, ptClient, distance );
				if (distance<nearest){
					nearest=distance;
					snapped.graphics=g, snapped.itemIndex=iItem;
				}
			};
		}
		return snapped.graphics ? &snapped : nullptr;
	}

namespace Screenshot
{
	struct TParams sealed{
		const CString fileName;
		CChartView &cv;

		TParams(const CString &fileName,CChartView &cv)
			: fileName(fileName) , cv(cv) {
		}
	};

	static UINT AFX_CDECL Thread(PVOID pCancelableAction){
		// thread to take a screenshot of current state of Chart
		CBackgroundActionCancelable &bac=*(PBackgroundActionCancelable)pCancelableAction;
		TParams &p=*(TParams *)bac.GetParams();
		CChartView &cv=p.cv;
		const CClientDC dc(&cv);
		const Utils::TClientRect rc(cv);
		const CRect rcEmf( 0, 0, // see MSDN "Creating an Enhanced Metafile" article
			rc.Width()*100*::GetDeviceCaps(dc,HORZSIZE)/::GetDeviceCaps(dc,HORZRES),
			rc.Height()*100*::GetDeviceCaps(dc,VERTSIZE)/::GetDeviceCaps(dc,VERTRES)
		);
		if (const HDC dcMem=::CreateEnhMetaFile( dc, p.fileName, rcEmf, nullptr )){
			cv.painter.Draw( dcMem, rc, bac );
			if (const HENHMETAFILE h=::CloseEnhMetaFile(dcMem))
				if (::DeleteEnhMetaFile(h))
					if (bac.Cancelled){
						::DeleteFile(p.fileName);
						return ERROR_CANCELLED;
					}else
						return bac.TerminateWithSuccess();
		}
		return bac.TerminateWithError( ::GetLastError() );
	}
}
	bool CChartView::CDisplayInfo::OnCmdMsg(CChartView &cv,UINT nID,int nCode,PVOID pExtra){
		// command processing
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:{
				// update
				CCmdUI *const pCmdUi=(CCmdUI *)pExtra;
				switch (nID){
					case ID_ALIGN:{
						bool containsSnappableGraphics=false; // assumption
						for( BYTE i=nGraphics; i; containsSnappableGraphics|=graphics[--i]->SupportsCursorSnapping() );
						pCmdUi->Enable( containsSnappableGraphics );
						pCmdUi->SetCheck( containsSnappableGraphics && snapToNearestItem );
						return true;
					}
					case ID_FILE_SAVE_AS:
						return true;
				}
				break;
			}
			case CN_COMMAND:
				// command
				switch (nID){
					case ID_ALIGN:
						snapToNearestItem=!snapToNearestItem;
						snapped.graphics=nullptr;
						cv.Invalidate();
						return true;
					case ID_FILE_SAVE_AS:{
						// save screenshot as
						const Screenshot::TParams sp(
							Utils::DoPromptSingleTypeFileName( _T(""), _T("Windows Metafile (*.wmf)|*.wmf|"), 0 ),
							cv
						);
						if (sp.fileName.IsEmpty())
							return true;
						if (const TStdWinError err=CBackgroundActionCancelable( Screenshot::Thread, &sp, THREAD_PRIORITY_ABOVE_NORMAL ).Perform())
							if (err!=ERROR_CANCELLED)
								Utils::Information( _T("Screenshot creation failed"), err );
						return true;
					}
				}
				break;
		}
		return false; // unrecognized command
	}






	CChartView::CXyDisplayInfo::CXyDisplayInfo(
		RCMargin margin,
		const PCGraphics graphics[], BYTE nGraphics,
		const Utils::CRideFont &fontAxes,
		TCHAR xAxisUnit, TLogValue xMax, LPCTSTR xAxisUnitPrefixes,
		TCHAR yAxisUnit, TLogValue yMax, LPCTSTR yAxisUnitPrefixes
	)
		// ctor
		// - base
		: CDisplayInfo( IDR_CHARTFRAME_XY, margin, graphics, nGraphics )
		// - initialization
		, gridPen( 0, 0xcacaca, PS_DOT ) // light gray
		, fontAxes(fontAxes)
		, xMaxOrg(xMax), yMaxOrg(yMax)
		, xAxis( xMaxOrg, 1, xAxisUnit, xAxisUnitPrefixes, 0, Utils::CAxis::TVerticalAlign::BOTTOM, fontAxes )
		, yAxis( yMaxOrg, 1, yAxisUnit, yAxisUnitPrefixes, 0, Utils::CAxis::TVerticalAlign::TOP, fontAxes )
		// - all data shown by default
		, xAxisFocus( 0, LogValueMax )
		, percentile(10100) { // invalid, must call SetPercentile !
		SetPercentile(10000);
	}

	void CChartView::CXyDisplayInfo::DrawBackground(HDC dc,const CRect &rcClient){
		// draws background
		// - drawing both X- and Y-Axis
		CRect rcChartBody=rcClient;
		rcChartBody.InflateRect( Utils::LogicalUnitScaleFactor*-margin.L, Utils::LogicalUnitScaleFactor*-margin.T, Utils::LogicalUnitScaleFactor*-margin.R, Utils::LogicalUnitScaleFactor*-margin.B );
		const SIZE szChartBody={ rcChartBody.Width(), rcChartBody.Height() };
		const SIZE szChartBodyUnits={ szChartBody.cx/Utils::LogicalUnitScaleFactor, szChartBody.cy/Utils::LogicalUnitScaleFactor };
		const TLogValue xAxisVisibleLength=std::min( xAxis.GetLength(), xAxisFocus.GetLength() );
		const Utils::TGdiMatrix shiftOrigin(
			margin.L, margin.T+szChartBodyUnits.cy // in units
		);
		xAxis.SetZoomFactor( xAxis.GetZoomFactorToFitWidth(xAxisVisibleLength,szChartBody.cx,30) );
			const float xAxisScale=(float)szChartBodyUnits.cx/xAxis.GetUnitCount(xAxisVisibleLength);
			::SetWorldTransform( dc,
				&Utils::TGdiMatrix().Scale( xAxisScale, 1 ).Combine( shiftOrigin )
			);
			xAxis.Draw( dc, xAxisFocus, -szChartBodyUnits.cy, gridPen );
		yAxis.SetZoomFactor( yAxis.GetZoomFactorToFitWidth(szChartBody.cy,30) );
			const float yAxisScale=(float)szChartBodyUnits.cy/yAxis.GetUnitCount();
			::SetWorldTransform( dc,
				&Utils::TGdiMatrix().RotateCcv90().Scale( 1, yAxisScale ).Combine( shiftOrigin )
			);
			yAxis.DrawWhole( dc, szChartBodyUnits.cx, gridPen );
		// - setting transformation to correctly draw all Series (looks better than when relying on the Axes)
		mValues=Utils::TGdiMatrix(
			-xAxisFocus.a , 0
		).Scale(
			xAxisScale/xAxis.GetValue(1), -yAxisScale/yAxis.GetValue(1)
		).Combine(
			shiftOrigin
		);
	}

	CChartView::CDisplayInfo::PCSnappedItem CChartView::CXyDisplayInfo::DrawCursor(HDC dc,const POINT &ptClient){
		// draws cursor eventually Snapped to an Item (or Null)
		if (const PCSnappedItem psi=__super::DrawCursor( dc, ptClient )){ // snapped?
			const TLogPoint &pt=*(PCLogPoint)psi->graphics->GetItem( psi->itemIndex, ptClient );
			xAxis.DrawCursorPos( dc, pt.x );
			yAxis.DrawCursorPos( dc, pt.y );
			return psi;
		}else{
			xAxis.DrawCursorPos( dc, ptClient );
			yAxis.DrawCursorPos( dc, ptClient );
			return nullptr;
		}
	}

	void CChartView::CXyDisplayInfo::SetPercentile(WORD newPercentile){
		//
		if (newPercentile==percentile)
			return;
		percentile=newPercentile;
		RefreshDrawingLimits();
	}

	void CChartView::CXyDisplayInfo::RefreshDrawingLimits(TLogValue xMaxNew,TLogValue yMaxNew){
		TLogValue xMax= xMaxNew<0 ? xMaxOrg : xMaxNew;
		TLogValue yMax= yMaxNew<0 ? yMaxOrg : yMaxNew;
		for( BYTE i=0; i<nGraphics; i++ )
			if (const PCXyGraphics g=dynamic_cast<PCXyGraphics>(graphics[i]))
				if (g->visible)
					g->GetDrawingLimits( percentile, xMax, yMax );
		xAxis.SetLength(xMax), yAxis.SetLength(yMax);
	}

	POINT CChartView::CXyDisplayInfo::GetClientUnits(TLogValue x,TLogValue y) const{
		const POINTF &&ptUnitsF=mValues.Transform( x, y );
		const POINT ptUnits={ ptUnitsF.x, ptUnitsF.y };
		return ptUnits;
	}

	CString CChartView::CXyDisplayInfo::GetStatus() const{
		// composes a string representing actual display status
		const CString x=xAxis.CursorValueToReadableString(), y=yAxis.CursorValueToReadableString();
		CString status;
		#define XY_STATUS	_T("x = %s,  y = %s")
		if (snapped.graphics!=nullptr)
			status.Format( _T("(%s),  ") XY_STATUS, snapped.graphics->name, (LPCTSTR)x, (LPCTSTR)y );
		else
			status.Format( XY_STATUS, (LPCTSTR)x, (LPCTSTR)y );
		return status;
	}

	static WORD MenuItemToStdPercentile(UINT menuItemId){
		switch (menuItemId){
			case ID_DATA:
				return 10000;
			case ID_ACCURACY:
				return 9995;
			case ID_STANDARD:
				return 9985;
			default:
				return -1;
		}
	}

	bool CChartView::CXyDisplayInfo::OnCmdMsg(CChartView &cv,UINT nID,int nCode,PVOID pExtra){
		// command processing
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:{
				// update
				CCmdUI *const pCmdUi=(CCmdUI *)pExtra;
				switch (nID){
					case ID_DATA:
					case ID_ACCURACY:
					case ID_STANDARD:
						pCmdUi->SetCheck( percentile==MenuItemToStdPercentile(nID) );
						//fallthrough
					case ID_NUMBER:
						pCmdUi->Enable();
						return true;
				}
				break;
			}
			case CN_COMMAND:
				// command
				switch (nID){
					case ID_DATA:
					case ID_ACCURACY:
					case ID_STANDARD:
						SetPercentile( MenuItemToStdPercentile(nID) );
						cv.Invalidate();
						return true;
					case ID_NUMBER:
						if (const Utils::CSingleNumberDialog &&d=Utils::CSingleNumberDialog( _T("Set"), _T("Percentile"), PropGrid::Integer::TUpDownLimits::PositivePercent, GetPercentile()/100, false, CWnd::FromHandle(app.GetEnabledActiveWindow()) ))
							SetPercentile(d.Value*100), cv.Invalidate();
						return true;
				}
				break;
		}
		return __super::OnCmdMsg( cv, nID, nCode, pExtra ); // base
	}






	CChartView::CPainter::CPainter(const CChartView &cv,CDisplayInfo &di)
		// ctor
		: CBackgroundAction( Thread, &cv, THREAD_PRIORITY_IDLE )
		, di(di) {
	}

	void CChartView::CPainter::CancelDrawingSync(bool cancel){
		EXCLUSIVELY_LOCK(*this);
		di.drawingCancelled=cancel;
	}

	bool CChartView::CPainter::Draw(HDC dc,const CRect &rcClient,CActionProgress &ap) const{
		// True <=> all Chart Graphics painted, otherwise False
		// - estimate drawing complexity
		TIndex nItemsTotal=0;
		for( BYTE i=0; i<di.nGraphics; i++ )
			nItemsTotal+=di.graphics[i]->GetItemCount();
		ap.SetProgressTarget( nItemsTotal );
		// - registering and preparing the canvas
		Utils::ScaleLogicalUnit(dc);
		::SetGraphicsMode( dc, GM_ADVANCED );
		::SetBkMode( dc, TRANSPARENT );
		// - drawing the background
		di.DrawBackground( dc, rcClient );
		if (ap.Cancelled) // new paint request?
			return false;
		// - preventing from drawing inside the Margin
		::SetWorldTransform( dc, &Utils::TGdiMatrix::Identity );
		::IntersectClipRect( dc, di.margin.L, di.margin.T/2, rcClient.right/Utils::LogicalUnitScaleFactor-di.margin.R/2, rcClient.bottom/Utils::LogicalUnitScaleFactor-di.margin.B );
		// - drawing all Graphic assets as they appear in the list
		for( BYTE i=0; i<di.nGraphics; i++ ){
			const PCGraphics g=di.graphics[i];
			if (g->visible)
				g->DrawAsync( *this, dc, ap );
			if (ap.Cancelled) // new paint request?
				return false;
		}
		// - all Graphics successfully painted
		return true;
	}

	UINT AFX_CDECL CChartView::CPainter::Thread(PVOID _pBackgroundAction){
		// thread to paint the Chart
		const PCBackgroundAction pAction=(PCBackgroundAction)_pBackgroundAction;
		CChartView &cv=*(CChartView *)pAction->GetParams();
		CPainter &p=cv.painter;
		do{
			// . waiting for next paint request
			p.redrawEvent.Lock();
			if (!::IsWindow(cv.m_hWnd)) // window closed?
				break;
			// . drawing
			cv.SetStatus(DRAWING);
				p.CancelDrawingSync(false); // begin new drawing
				if (!p.Draw( CClientDC(&cv), Utils::TClientRect(cv), cv.painter.di )) // new paint request during drawing?
					continue;
			cv.SetStatus(READY);
		}while (true);
		return ERROR_SUCCESS;
	}

	LRESULT CChartView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_PAINT:
				// window's size is being changed
				painter.CancelDrawingSync();
				break;
			case WM_MOUSEMOVE:
				// mouse moved
				if (painter.di.nGraphics){
					painter.di.DrawCursor( CClientDC(this), CPoint(lParam) );
					GetParent()->SendMessage( WM_CHART_STATUS_CHANGED, (WPARAM)m_hWnd );
				}
				break;
		}
		return __super::WindowProc( msg, wParam, lParam );
	}











	const UINT CChartView::WM_CHART_STATUS_CHANGED=::RegisterWindowMessage(_T("WM_CHART_STATUS_CHANGED"));

	CChartView::CChartView(CDisplayInfo &di)
		// ctor
		: painter( *this, di )
		, status(READY) {
		painter.Resume();
	}

	void CChartView::SetStatus(TStatus newStatus){
		status=newStatus;
		if (::IsWindow(m_hWnd)) // window not yet closed?
			GetParent()->SendMessage( WM_CHART_STATUS_CHANGED, (WPARAM)m_hWnd );
	}

	void CChartView::PostNcDestroy(){
		// self-destruction
		// - letting the Painter finish normally
		painter.CancelDrawingSync();
		painter.redrawEvent.SetEvent();
		::WaitForSingleObject( painter, INFINITE );
		// - base
		//__super::PostNcDestroy(); // commented out (View destroyed by its owner)
	}

	void CChartView::OnDraw(CDC *pDC){
		// drawing
		// - base
		__super::OnDraw(pDC);
		// - drawing the chart (in parallel thread due to computational complexity if painting the whole Track)
		painter.CancelDrawingSync();
		painter.redrawEvent.SetEvent();
	}









	CChartFrame::CChartFrame(const CString &caption,CChartView::CDisplayInfo &di)
		// ctor
		: chartView(di)
		, captionBase(caption) {
	}

	BOOL CChartFrame::PreCreateWindow(CREATESTRUCT &cs){
		// adjusting the instantiation
		if (!__super::PreCreateWindow(cs)) return FALSE;
		cs.dwExStyle&=~WS_EX_CLIENTEDGE;
		return TRUE;
	}

	LRESULT CChartFrame::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_CREATE:{
				const LRESULT result=__super::WindowProc(msg,wParam,lParam);
				chartView.Create( nullptr, captionBase, AFX_WS_DEFAULT_VIEW&~WS_BORDER|WS_CLIPSIBLINGS, rectDefault, this, AFX_IDW_PANE_FIRST );
				static constexpr UINT Indicators[]={ ID_SEPARATOR, ID_SEPARATOR };
				statusBar.Create(this);
				statusBar.SetIndicators( Indicators, ARRAYSIZE(Indicators) );
				statusBar.SetPaneInfo( 0, ID_SEPARATOR, SBPS_NOBORDERS, 64 );
				statusBar.SetPaneInfo( 1, ID_SEPARATOR, SBPS_NORMAL|SBPS_STRETCH, 0 );
				drawingProgressBar.Create( WS_CHILD, rectDefault, &statusBar, 0 );
				chartView.painter.di.hProgressBar=drawingProgressBar.m_hWnd;
				return result;
			}
			case WM_ACTIVATE:
			case WM_SETFOCUS:
				// window has received focus
				// - passing the focus over to the View
				::SetFocus( chartView.m_hWnd );
				return 0;
			case WM_SETCURSOR:
				// cursor must be updated
				if (LOWORD(lParam)==HTCLIENT){
					::SetCursor( app.LoadStandardCursor(IDC_CROSS) );
					return TRUE;
				}
				break;
			default:
				if (msg==CChartView::WM_CHART_STATUS_CHANGED){
					switch (chartView.GetStatus()){
						case CChartView::TStatus::READY:
							statusBar.SetPaneText( 0, _T("Ready") );
							drawingProgressBar.ShowWindow(SW_HIDE);
							break;
						case CChartView::TStatus::DRAWING:{
							CRect rc;
							statusBar.GetItemRect(0,&rc);
							drawingProgressBar.SetWindowPos( nullptr, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOZORDER );
							drawingProgressBar.ShowWindow(SW_SHOW);
							break;
						}
					}
					statusBar.SetPaneText( 1, chartView.painter.di.GetStatus() );
					return 0;
				}
		}
		return __super::WindowProc(msg,wParam,lParam);
	}

	BOOL CChartFrame::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		if (chartView.painter.di.OnCmdMsg( chartView, nID, nCode, pExtra ))
			return true;
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:{
				// update
				CCmdUI *const pCmdUi=(CCmdUI *)pExtra;
				switch (nID){
					case IDCANCEL:
						pCmdUi->Enable();
						return true;
				}
				break;
			}
			case CN_COMMAND:
				// command
				switch (nID){
					case IDCANCEL:
						::PostMessage( m_hWnd, WM_DESTROY, 0, 0 );
						return true;
				}
				break;
		}
		return __super::OnCmdMsg( nID, nCode, pExtra, pHandlerInfo ); // base
	}









	CChartDialog::CChartDialog(CChartView::CDisplayInfo &di)
		// ctor
		: CChartFrame(_T(""),di) {
	}

	LRESULT CChartDialog::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		if (msg==WM_DESTROY)
			EndModalLoop(IDCLOSE);
		return __super::WindowProc(msg,wParam,lParam);
	}

	void CChartDialog::PostNcDestroy(){
		// self-destruction
		//nop (assumed Dialog allocated on stack)
	}
	
	void CChartDialog::ShowModal(const CString &caption,CWnd *pParentWnd,WORD width,WORD height,DWORD dwStyle){
		// modal display of the Dialog
		CWnd *const pBlockedWnd= pParentWnd ? pParentWnd : app.m_pMainWnd;
		pBlockedWnd->BeginModalState();
			LoadFrame( chartView.GetDisplayInfo().menuResourceId, dwStyle, pParentWnd );
			SetWindowText( captionBase=caption );
			SetWindowPos( nullptr, 0,0, width*Utils::LogicalUnitScaleFactor, height*Utils::LogicalUnitScaleFactor, SWP_NOZORDER|SWP_NOMOVE );
			RunModalLoop( MLF_SHOWONIDLE|MLF_NOIDLEMSG );
		pBlockedWnd->EndModalState();
		pBlockedWnd->BringWindowToTop();
	}

}








	TLogValue TLogPoint::ManhattanDistance(const TLogPoint &other) const{
		return std::abs(x-other.x)+std::abs(y-other.y);
	}
