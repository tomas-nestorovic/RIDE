#include "stdafx.h"
#include "Charting.h"

	const CChartView::TMargin CChartView::TMargin::None;
	const CChartView::TMargin CChartView::TMargin::Default={ 20, 20, 20, 20 };





	CChartView::CHistogram::operator bool() const{
		// True <=> the Histogram isn't empty, otherwise False
		return size()>0;
	}

	void CChartView::CHistogram::AddValue(int value){
		// increases by one the number of occurences of Value
		auto it=find(value);
		if (it!=cend())
			it->second++;
		else
			insert( std::make_pair(value,1) );
	}

	int CChartView::CHistogram::GetCount(int value) const{
		// returns the number of occurences of Value (or zero)
		const auto it=find(value);
		if (it!=cend())
			return it->second;
		else
			return 0;
	}






	CChartView::CGraphics::CGraphics()
		// ctor
		: visible(true) {
	}






	void CChartView::CXyGraphics::GetDrawingLimits(WORD percentile,TLogValue &rOutMaxX,TLogValue &rOutMaxY) const{
		// returns the XY position of the last item still to be drawn with specified Percentile
		//nop (not applicable)
	}






	CChartView::CXyPointSeries::CXyPointSeries(DWORD nPoints,const POINT *points,HPEN hVertexPen)
		// ctor
		: nPoints(nPoints) , points(points)
		, hPen(hVertexPen) {
	}

	void CChartView::CXyPointSeries::GetDrawingLimits(WORD percentile,TLogValue &rOutMaxX,TLogValue &rOutMaxY) const{
		// sets corresponding outputs to the last item still to be drawn with specified Percentile
		const CHistogram h=CreateYxHistogram();
		DWORD sum=0,const sumMax=(ULONGLONG)nPoints*percentile/10000;
		//rOutMaxX=...; // working out percentile along Y-axis only
		rOutMaxY=1;
		for( auto it=h.cbegin(); it!=h.cend()&&sum<sumMax; sum+=it++->second )
			rOutMaxY=it->first;
	}

	void CChartView::CXyPointSeries::DrawAsync(const CPainter &p) const{
		// asynchronous drawing; always compare actual drawing ID with the one on start
		const WORD id=p.GetCurrentDrawingIdSync();
		const CXyDisplayInfo &di=*(const CXyDisplayInfo *)&p.di;
		const HGDIOBJ hPen0=::SelectObject( p.dc, hPen );
			for( DWORD j=0; j<nPoints; j++ ){
				EXCLUSIVELY_LOCK(p);
				if (p.drawingId!=id)
					break;
				if (points[j].y>di.GetAxisY().GetLength())
					continue;
				const POINT pt=di.Transform( points[j] );
				::MoveToEx( p.dc, pt.x, pt.y, nullptr );
				::LineTo( p.dc, pt.x+1, pt.y );
			}
		::SelectObject( p.dc, hPen0 );
	}

	CChartView::CHistogram CChartView::CXyPointSeries::CreateYxHistogram() const{
		// creates histogram of Y values
		CHistogram tmp;
		for( DWORD n=nPoints; n>0; tmp.AddValue(points[--n].y) );
		return tmp;
	}






	CChartView::CXyBrokenLineSeries::CXyBrokenLineSeries(DWORD nPoints,const POINT *points,HPEN hLinePen)
		// ctor
		: CXyPointSeries( nPoints, points, hLinePen ) {
	}

	void CChartView::CXyBrokenLineSeries::DrawAsync(const CPainter &p) const{
		// asynchronous drawing; always compare actual drawing ID with the one on start
		if (nPoints<2)
			return;
		const WORD id=p.GetCurrentDrawingIdSync();
		const CXyDisplayInfo &di=*(const CXyDisplayInfo *)&p.di;
		const HGDIOBJ hPen0=::SelectObject( p.dc, hPen );
			POINT pt=di.Transform( *points );
			::MoveToEx( p.dc, pt.x, pt.y, nullptr );
			for( DWORD j=1; j<nPoints; j++ ){
				EXCLUSIVELY_LOCK(p);
				if (p.drawingId!=id)
					break;
				pt=di.Transform( points[j] );
				::LineTo( p.dc, pt.x+1, pt.y );
			}
		::SelectObject( p.dc, hPen0 );
	}






	CChartView::CXyOrderedBarSeries::CXyOrderedBarSeries(DWORD nPoints,const POINT *points,HPEN hLinePen)
		// ctor
		: CXyPointSeries( nPoints, points, hLinePen ) {
	}

	void CChartView::CXyOrderedBarSeries::GetDrawingLimits(WORD percentile,TLogValue &rOutMaxX,TLogValue &rOutMaxY) const{
		// sets corresponding outputs to the last item still to be drawn with specified Percentile
		LONGLONG ySum=0;
		for( DWORD i=0; i<nPoints; ySum+=points[i++].y );
		ySum=ySum*percentile/10000; // estimation of percentile
		rOutMaxX = rOutMaxY = 1;
		for( DWORD i=0; i<nPoints&&ySum>0; i++ ){
			const POINT &pt=points[i];
			ySum-=pt.y;
			if (pt.x>rOutMaxX)
				rOutMaxX=pt.x;
			if (pt.y>rOutMaxY)
				rOutMaxY=pt.y;
		}
	}

	void CChartView::CXyOrderedBarSeries::DrawAsync(const CPainter &p) const{
		// asynchronous drawing; always compare actual drawing ID with the one on start
		const WORD id=p.GetCurrentDrawingIdSync();
		const CXyDisplayInfo &di=*(const CXyDisplayInfo *)&p.di;
		const HGDIOBJ hPen0=::SelectObject( p.dc, hPen );
			for( DWORD i=0; i<nPoints; i++ ){
				const POINT &pt=points[i];
				if (pt.x>di.GetAxisX().GetLength())
					break;
				EXCLUSIVELY_LOCK(p);
				if (p.drawingId!=id)
					break;
				const POINT ptT=di.Transform( pt );
				::MoveToEx( p.dc, ptT.x, ptT.y, nullptr );
				const POINT ptB=di.Transform( pt.x, 0 );
				::LineTo( p.dc, ptB.x, ptB.y );
			}
		::SelectObject( p.dc, hPen0 );
	}






	CChartView::CDisplayInfo::CDisplayInfo(UINT menuResourceId,RCMargin margin,const PCGraphics *graphics,BYTE nGraphics)
		// ctor
		: menuResourceId(menuResourceId) , margin(margin) , graphics(graphics) , nGraphics(nGraphics) {
	}

	bool CChartView::CDisplayInfo::OnCmdMsg(CChartView &cv,UINT nID,int nCode,PVOID pExtra){
		// command processing
		return false; // no common commands
	}






	static constexpr XFORM IdentityTransf={ 1, 0, 0, 1, 0, 0 };

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
		, xAxisUnit(xAxisUnit) , xAxisUnitPrefixes(xAxisUnitPrefixes)
		, yAxisUnit(yAxisUnit) , yAxisUnitPrefixes(yAxisUnitPrefixes)
		, xAxis( xMaxOrg, 1, 0, Utils::CAxis::TVerticalAlign::BOTTOM )
		, yAxis( yMaxOrg, 1, 0, Utils::CAxis::TVerticalAlign::TOP )
		, M(IdentityTransf)
		// - all data shown by default
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
		xAxis.SetZoomFactor( xAxis.GetZoomFactorToFitWidth(szChartBodyUnits.cx,30) );
			const XFORM xAxisTransf={ (float)szChartBodyUnits.cx/xAxis.GetUnitCount(), 0, 0, 1, margin.L, rcClient.Height()/Utils::LogicalUnitScaleFactor-margin.B };
			::SetWorldTransform( dc, &xAxisTransf );
			xAxis.Draw( dc, szChartBody.cx, xAxisUnit, xAxisUnitPrefixes, fontAxes, -szChartBodyUnits.cy, gridPen );
		yAxis.SetZoomFactor( yAxis.GetZoomFactorToFitWidth(szChartBodyUnits.cy,30) );
			const XFORM yAxisTransf={ 0, -(float)szChartBodyUnits.cy/yAxis.GetUnitCount(), 1, 0, xAxisTransf.eDx, xAxisTransf.eDy };
			::SetWorldTransform( dc, &yAxisTransf );
			yAxis.Draw( dc, szChartBody.cy, yAxisUnit, yAxisUnitPrefixes, fontAxes, szChartBodyUnits.cx, gridPen );
		// - setting transformation to correctly draw all Series
		::SetWorldTransform( dc, &IdentityTransf );
		const XFORM valuesTransf={ xAxisTransf.eM11/(1<<xAxis.GetZoomFactor()), 0, 0, yAxisTransf.eM12/(1<<yAxis.GetZoomFactor()), xAxisTransf.eDx, xAxisTransf.eDy };
		M=valuesTransf;
	}

	void CChartView::CXyDisplayInfo::DrawCursorAt(HDC dc,const POINT &ptClient,const CRect &rcClient){
		// indicades the positions of the cursor, given its position in the Display's client area
		// - determining the XY-values at which the cursor points
		const POINT value=InverselyTransform( ptClient/Utils::LogicalUnitScaleFactor );
		// - drawing cursor indicators
		Utils::ScaleLogicalUnit(dc);
		xAxis.DrawCursorAt( dc, value.x );
		yAxis.DrawCursorAt( dc, value.y );
	}

	POINT CChartView::CXyDisplayInfo::Transform(long x,long y) const{
		const POINT tmp={
			M.eDx + M.eM11*x + M.eM21*y,
			M.eDy + M.eM12*x + M.eM22*y
		};
		return tmp;
	}

	RECT CChartView::CXyDisplayInfo::Transform(const RECT &rc) const{
		return CRect( Transform(*(const POINT *)&rc), Transform(((const POINT *)&rc)[1]) );
	}

	POINT CChartView::CXyDisplayInfo::InverselyTransform(const POINT &pt) const{
		// Solving of the system of equations:
		// | m n d | x |
		// | r s e | y |
		// | 0 0 1 | 1 |
		const double m=M.eM11, n=M.eM21, d=M.eDx;
		const double r=M.eM12, s=M.eM22, e=M.eDy;
		const double dx=pt.x-d, dy=pt.y-e;
		const double resultY= (m*dy-r*dx) / (s*m-r*n);
		return CPoint( (dx-n*resultY)/m, resultY );
	}

	void CChartView::CXyDisplayInfo::SetPercentile(WORD newPercentile){
		//
		if (newPercentile==percentile)
			return;
		percentile=newPercentile;
		TLogValue xMax=xMaxOrg, yMax=yMaxOrg;
		for( BYTE i=0; i<nGraphics; i++ )
			if (const PCXyGraphics g=dynamic_cast<PCXyGraphics>(graphics[i]))
				if (g->visible)
					g->GetDrawingLimits( newPercentile, xMax, yMax );
		xAxis.SetLength(xMax), yAxis.SetLength(yMax);
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
						SetPercentile(10000);
						cv.Invalidate();
						return true;
					case ID_ACCURACY:
						SetPercentile(9995);
						cv.Invalidate();
						return true;
					case ID_STANDARD:
						SetPercentile(9985);
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

	WORD CChartView::CPainter::GetCurrentDrawingIdSync() const{
		locker.Lock();
			const WORD id=drawingId;
		locker.Unlock();
		return id;
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
			const WORD id=p.GetCurrentDrawingIdSync();
			// . creating and preparing the canvas
			const CClientDC dc( &cv );
			::SetBkMode( dc, TRANSPARENT );
			Utils::ScaleLogicalUnit(dc);
			p.dc=dc;
			// . preventing from drawing inside the Margin
			RECT rcClient;
			cv.GetClientRect(&rcClient);
			::IntersectClipRect( dc, p.di.margin.L, p.di.margin.T/2, rcClient.right/Utils::LogicalUnitScaleFactor-p.di.margin.R/2, rcClient.bottom/Utils::LogicalUnitScaleFactor-p.di.margin.B );
			// . drawing all Graphic assets as they appear in the list
			for( BYTE i=0; i<p.di.nGraphics; i++ ){
				const PCGraphics g=p.di.graphics[i];
				if (g->visible)
					g->DrawAsync( p );
				if (id!=p.GetCurrentDrawingIdSync()) // new paint request?
					break;
			}
		}while (true);
		return ERROR_SUCCESS;
	}

	LRESULT CChartView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_PAINT:{
				// window's size is being changed
				EXCLUSIVELY_LOCK(painter);
				painter.drawingId++;
				break;
			}
			case WM_MOUSEMOVE:{
				// mouse moved
				RECT rcClient;
				GetClientRect(&rcClient);
				painter.di.DrawCursorAt( CClientDC(this), CPoint(lParam), rcClient );
				break;
			}
		}
		return __super::WindowProc( msg, wParam, lParam );
	}











	CChartView::CChartView(CDisplayInfo &di)
		// ctor
		: painter( *this, di ) {
		painter.Resume();
	}

	void CChartView::PostNcDestroy(){
		// self-destruction
		// - letting the Painter finish normally
		painter.locker.Lock();
			painter.drawingId++;
		painter.locker.Unlock();
		painter.redrawEvent.SetEvent();
		::WaitForSingleObject( painter, INFINITE );
		// - base
		//__super::PostNcDestroy(); // commented out (View destroyed by its owner)
	}

	void CChartView::OnDraw(CDC *pDC){
		// drawing
		// - base
		__super::OnDraw(pDC);
		// - scaling the canvas
		Utils::ScaleLogicalUnit(*pDC);
		// - preparing the canvas for drawing a Chart of specified Type
		const HDC dc=*pDC;
		pDC->SetBkMode(TRANSPARENT);
		pDC->SetMapMode(MM_ANISOTROPIC);
		::SetGraphicsMode( dc, GM_ADVANCED );
		// - drawing the chart
		RECT rcClient;
		GetClientRect(&rcClient);
		EXCLUSIVELY_LOCK(painter);
		painter.drawingId++;
		painter.di.DrawBackground( dc, rcClient );
		// . drawing the rest in parallel thread due to computational complexity if painting the whole Track
		painter.redrawEvent.SetEvent();
	}









	CChartFrame::CChartFrame(CChartView::CDisplayInfo &di)
		// ctor
		: chartView(di) {
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
				chartView.Create( nullptr, nullptr, AFX_WS_DEFAULT_VIEW&~WS_BORDER|WS_CLIPSIBLINGS, rectDefault, this, AFX_IDW_PANE_FIRST );
				return result;
			}
			case WM_ACTIVATE:
			case WM_SETFOCUS:
				// window has received focus
				// - passing the focus over to the View
				::SetFocus( chartView.m_hWnd );
				return 0;
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
		: CChartFrame(di) {
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
	
	void CChartDialog::ShowModal(LPCTSTR caption,CWnd *pParentWnd,WORD width,WORD height,DWORD dwStyle){
		// modal display of the Dialog
		CWnd *const pBlockedWnd= pParentWnd ? pParentWnd : app.m_pMainWnd;
		pBlockedWnd->BeginModalState();
			LoadFrame( chartView.GetDisplayInfo().menuResourceId, dwStyle, pParentWnd );
			SetWindowText(caption);
			SetWindowPos( nullptr, 0,0, width*Utils::LogicalUnitScaleFactor, height*Utils::LogicalUnitScaleFactor, SWP_NOZORDER|SWP_NOMOVE );
			RunModalLoop( MLF_SHOWONIDLE|MLF_NOIDLEMSG );
		pBlockedWnd->EndModalState();
		pBlockedWnd->BringWindowToTop();
	}
