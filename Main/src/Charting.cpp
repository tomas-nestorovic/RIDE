#include "stdafx.h"
#include "Charting.h"

	const CChartView::TMargin CChartView::TMargin::None;
	const CChartView::TMargin CChartView::TMargin::Default={ 20, 20, 20, 20 };





	CChartView::CSeries::CSeries(DWORD nValues)
		// ctor
		: nValues(nValues) {
	}

	CChartView::CSeries CChartView::CSeries::CreateXy(DWORD nValues,const POINT *pXyValues,HPEN hLinePen,HPEN hVertexPen){
		// creates and returns a new planar Series
		CSeries tmp(nValues);
			auto &r=tmp.xy;
			r.pValues=pXyValues;
			r.hLinePen=hLinePen, r.hVertexPen=hVertexPen;
		return tmp;
	}





	CChartView::CDisplayInfo::CDisplayInfo(TType chartType,RCMargin margin,PCSeries series,BYTE nSeries)
		// ctor
		: chartType(chartType) , margin(margin)
		, series(series) , nSeries(nSeries) {
	}

	CChartView::CDisplayInfo CChartView::CDisplayInfo::CreateXy(
		TType chartType, RCMargin margin, PCSeries series, BYTE nSeries,
		TCHAR xAxisUnit, TLogValue xMax, LPCTSTR xAxisUnitPrefixes,
		TCHAR yAxisUnit, TLogValue yMax, LPCTSTR yAxisUnitPrefixes
	){
		// creates and returns information on planar display
		CDisplayInfo tmp( chartType, margin, series, nSeries );
			auto &r=tmp.xy;
			r.xAxisUnit=xAxisUnit, r.xMax=xMax, r.xAxisUnitPrefixes=xAxisUnitPrefixes;
			r.yAxisUnit=yAxisUnit, r.yMax=yMax, r.yAxisUnitPrefixes=yAxisUnitPrefixes;
		return tmp;
	}





	static const XFORM IdentityTransf={ 1, 0, 0, 1, 0, 0 };

	CChartView::TPainter::TPainter(CChartView &cv)
		// ctor
		: action( Thread, &cv, THREAD_PRIORITY_IDLE ) {
		params.valuesTransf=IdentityTransf;
	}

	static POINT Transform(const XFORM &M,long x,long y){
		const POINT tmp={
			M.eDx + M.eM11*x + M.eM21*y,
			M.eDy + M.eM12*x + M.eM22*y
		};
		return tmp;
	}

	inline static POINT Transform(const XFORM &M,const POINT &P){
		return Transform( M, P.x, P.y );
	}

	UINT AFX_CDECL CChartView::TPainter::Thread(PVOID _pBackgroundAction){
		// thread to paint the Chart according to specified Parameters
		const PCBackgroundAction pAction=(PCBackgroundAction)_pBackgroundAction;
		CChartView &cv=*(CChartView *)pAction->GetParams();
		TPainter &p=cv.painter;
		do{
			// . waiting for next paint request
			p.repaintEvent.Lock();
			if (!::IsWindow(cv.m_hWnd)) // window closed?
				break;
			// . retrieving the Parameters
			CClientDC dc( &cv );
			p.params.locker.Lock();
				const WORD id=p.params.id;
			p.params.locker.Unlock();
			::SetBkMode( dc, TRANSPARENT );
			// . scaling the canvas
			Utils::ScaleLogicalUnit(dc);
			// . drawing all Series
			bool continuePainting=true;
			switch (cv.di.chartType){
				case TType::XY_LINE_BROKEN:
					// planar broken line chart
					for( BYTE i=0; i<cv.di.nSeries; ){
						const CSeries &series=cv.di.series[i++];
						const auto data=series.xy;
						if (!series.nValues)
							continue;
						const HGDIOBJ hPen0=::SelectObject( dc, data.hLinePen );
							const POINT firstPt=Transform( p.params.valuesTransf, *data.pValues );
							dc.MoveTo(firstPt);
							if (data.hVertexPen){
								::SelectObject( dc, data.hVertexPen );
								::LineTo( dc, firstPt.x+1, firstPt.y );
								dc.MoveTo(firstPt);
							}
							for( DWORD j=1; j<series.nValues; ){
								const POINT pt=Transform( p.params.valuesTransf, data.pValues[j++] );
								p.params.locker.Lock();
									if (data.hLinePen){
										::SelectObject( dc, data.hLinePen );
										dc.LineTo(pt);
									}else
										dc.MoveTo(pt);
									if (data.hVertexPen){
										::SelectObject( dc, data.hVertexPen );
										::LineTo( dc, pt.x+1, pt.y );
										dc.MoveTo(pt);
									}
									continuePainting=p.params.id==id;
								p.params.locker.Unlock();
								if (!continuePainting) // new paint request?
									break;
							}
						::SelectObject( dc, hPen0 );
						if (!continuePainting) // new paint request?
							break;
					}
					break;
				case TType::XY_BARS:{
					// planar bar chart
					CMapPtrToPtr barSizes; // key = X, value = size accumulated over all Series processed thus far
					for( BYTE i=0; i<cv.di.nSeries; ){
						const CSeries &series=cv.di.series[i++];
						const auto data=series.xy;
						if (!series.nValues)
							continue;
						const HGDIOBJ hPen0=::SelectObject( dc, data.hLinePen );
							for( DWORD j=0; j<series.nValues; ){
								const POINT pt=data.pValues[j++];
								PVOID pKey=(PVOID)pt.x, pValue=nullptr;
								barSizes.Lookup( pKey, pValue );
								p.params.locker.Lock();
									dc.MoveTo(  Transform( p.params.valuesTransf, pt.x, (long)pValue )  );
									pValue=(PBYTE)pValue+pt.y;
									dc.LineTo(  Transform( p.params.valuesTransf, pt.x, (long)pValue )  );
									continuePainting=p.params.id==id;
								p.params.locker.Unlock();
								barSizes.SetAt( pKey, pValue );
								if (!continuePainting) // new paint request?
									break;
							}
						::SelectObject( dc, hPen0 );
						if (!continuePainting) // new paint request?
							break;
					}
					break;
				}
				default:
					ASSERT(FALSE); break;
			}
		}while (true);
		return ERROR_SUCCESS;
	}





	CChartView::CChartView(const CDisplayInfo &di)
		// ctor
		: di(di)
		, gridPen( 0, 0xcacaca, PS_DOT ) // light gray
		, fontAxes( Utils::CRideFont::StdBold )
		, painter(*this) {
		painter.action.Resume();
	}

	void CChartView::PostNcDestroy(){
		// self-destruction
		// - letting the Painter finish normally
		painter.params.locker.Lock();
			painter.params.id++;
			painter.params.valuesTransf.eDy=INT_MIN;
		painter.params.locker.Unlock();
		painter.repaintEvent.SetEvent();
		::WaitForSingleObject( painter.action, INFINITE );
		// - base
		//__super::PostNcDestroy(); // commented out (View destroyed by its owner)
	}

	XFORM CChartView::DrawXyAxes(HDC dc) const{
		// draws both X- and Y-Axis, and returns the graphic transformation to draw values into the Axes
		CRect rcClient;
		GetClientRect(&rcClient);
		CRect rcChartBody=rcClient;
		rcChartBody.InflateRect( Utils::LogicalUnitScaleFactor*-di.margin.L, Utils::LogicalUnitScaleFactor*-di.margin.T, Utils::LogicalUnitScaleFactor*-di.margin.R, Utils::LogicalUnitScaleFactor*-di.margin.B );
		const SIZE szChartBody={ rcChartBody.Width(), rcChartBody.Height() };
		const SIZE szChartBodyUnits={ szChartBody.cx/Utils::LogicalUnitScaleFactor, szChartBody.cy/Utils::LogicalUnitScaleFactor };
		const auto &r=di.xy;
		const Utils::CAxis xAxis( r.xMax, 1, szChartBodyUnits.cx, 30 );
			const XFORM xAxisTransf={ (float)szChartBodyUnits.cx/xAxis.GetUnitCount(), 0, 0, 1, di.margin.L, rcClient.Height()/Utils::LogicalUnitScaleFactor-di.margin.B };
			::SetWorldTransform( dc, &xAxisTransf );
			xAxis.Draw( dc, szChartBody.cx, r.xAxisUnit, r.xAxisUnitPrefixes, fontAxes, Utils::CAxis::TVerticalAlign::BOTTOM, -szChartBodyUnits.cy, gridPen );
		const Utils::CAxis yAxis( r.yMax, 1, szChartBodyUnits.cy, 30 );
			const XFORM yAxisTransf={ 0, -(float)szChartBodyUnits.cy/yAxis.GetUnitCount(), 1, 0, xAxisTransf.eDx, xAxisTransf.eDy };
			::SetWorldTransform( dc, &yAxisTransf );
			yAxis.Draw( dc, szChartBody.cy, r.yAxisUnit, r.yAxisUnitPrefixes, fontAxes, Utils::CAxis::TVerticalAlign::TOP, szChartBodyUnits.cx, gridPen );
		::SetWorldTransform( dc, &IdentityTransf );
		const XFORM valuesTransf={ xAxisTransf.eM11/(1<<xAxis.zoomFactor), 0, 0, yAxisTransf.eM12/(1<<yAxis.zoomFactor), xAxisTransf.eDx, xAxisTransf.eDy };
		return valuesTransf;
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
		// - drawing the chart of specified Type
		painter.params.locker.Lock();
			painter.params.id++;
			switch (di.chartType){
				case TType::XY_LINE_BROKEN:
					// planar broken line chart
					painter.params.valuesTransf=DrawXyAxes( dc );
					break;
				case TType::XY_BARS:
					// planar bar chart
					painter.params.valuesTransf=DrawXyAxes( dc );
					break;
				default:
					ASSERT(FALSE); break;
			}
		painter.params.locker.Unlock();
		// . drawing the rest in parallel thread due to computational complexity if painting the whole Track
		painter.repaintEvent.SetEvent();
	}









	CChartFrame::CChartFrame(const CChartView::CDisplayInfo &di)
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









	CChartDialog::CChartDialog(const CChartView::CDisplayInfo &di)
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
	
	void CChartDialog::ShowModal(LPCTSTR caption,CWnd *pParentWnd,const RECT &rect,DWORD dwStyle){
		// modal display of the Dialog
		CWnd *const pBlockedWnd= pParentWnd ? pParentWnd : app.m_pMainWnd;
		pBlockedWnd->BeginModalState();
			Create( nullptr, caption, dwStyle, rect, pParentWnd );
			RunModalLoop( MLF_SHOWONIDLE|MLF_NOIDLEMSG );
		pBlockedWnd->EndModalState();
		pBlockedWnd->BringWindowToTop();
	}
