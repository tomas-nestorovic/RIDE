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
		if (it!=std::map<int,int>::cend())
			it->second++;
		else
			insert( std::make_pair(value,1) );
	}

	int CChartView::CHistogram::GetCount(int value) const{
		// returns the number of occurences of Value (or zero)
		const auto it=find(value);
		if (it!=std::map<int,int>::cend())
			return it->second;
		else
			return 0;
	}





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

	CChartView::CHistogram CChartView::CSeries::CreateYxHistogram() const{
		// creates histogram of Y values
		CHistogram tmp;
		for( DWORD n=nValues; n>0; tmp.AddValue(xy.pValues[--n].y) );
		return tmp;
	}






	CChartView::CDisplayInfo::CDisplayInfo(TType chartType,RCMargin margin,PCSeries series,BYTE nSeries)
		// ctor
		: chartType(chartType) , margin(margin)
		, percentile(101) // invalid, must call SetPercentile !
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
			tmp.SetPercentile(100); // all data shown by default
		return tmp;
	}

	void CChartView::CDisplayInfo::SetPercentile(BYTE newPercentile){
		//
		if (newPercentile==percentile)
			return;
		percentile=newPercentile;
		switch (chartType){
			case TType::XY_LINE_BROKEN:
				// planar broken line chart
				xy.yMax=1;
				for( BYTE i=0; i<nSeries; )
					if (const CSeries &s=series[i++]){
						const CHistogram h=s.CreateYxHistogram();
						int counts[4096], n=0;
						for( auto it=h.cbegin(); it!=h.cend(); it++ )
							counts[n++]=it->second;
						std::sort( counts, counts+n ); // ordering ascending
						for( DWORD sum=0,const sumMax=(ULONGLONG)s.nValues*percentile/100; sum<sumMax; sum+=counts[--n] );
						const int countThreshold= n>0 ? counts[n] : 0;
						for( auto it=h.cbegin(); it!=h.cend(); it++ )
							if (it->second>countThreshold)
								if (it->first>xy.yMax)
									xy.yMax=it->first;
					}
				break;
			case TType::XY_BARS:
				// planar bar chart
				xy.xMax = xy.yMax = 1;
				for( BYTE i=0; i<nSeries; )
					if (const CSeries &s=series[i++]){
						const CHistogram h=s.CreateYxHistogram();
						auto it=h.cbegin();
						for( DWORD sum=0,const sumMax=(ULONGLONG)s.nValues*percentile/100; sum<sumMax; sum+=it++->second ){
							if (it->first>xy.xMax)
								xy.xMax=it->first;
							if (it->second>xy.yMax)
								xy.yMax=it->second;
						}
					}
				break;
			default:
				ASSERT(FALSE); break;
		}
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
						const CSeries &s=cv.di.series[i++];
						const auto &data=s.xy;
						if (!s.nValues)
							continue;
						const HGDIOBJ hPen0=::SelectObject( dc, data.hLinePen );
							DWORD j=0;
							while (data.pValues[j].y>cv.di.xy.yMax)
								j++;
							const POINT firstPt=Transform( p.params.valuesTransf, data.pValues[j] );
							p.params.locker.Lock();
								dc.MoveTo(firstPt);
								if (data.hVertexPen)
									if (continuePainting=p.params.id==id){
										::SelectObject( dc, data.hVertexPen );
										::LineTo( dc, firstPt.x+1, firstPt.y );
										dc.MoveTo(firstPt);
									}
							p.params.locker.Unlock();
							while (continuePainting && j<s.nValues){
								const POINT &ptData=data.pValues[j++];
								if (ptData.y>cv.di.xy.yMax)
									continue;
								const POINT pt=Transform( p.params.valuesTransf, ptData );
								p.params.locker.Lock();
									if (continuePainting=p.params.id==id){
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
									}
								p.params.locker.Unlock();
							}
						::SelectObject( dc, hPen0 );
						if (!continuePainting) // new paint request?
							break;
					}
					break;
				case TType::XY_BARS:
					// planar bar chart
					for( BYTE i=0; i<cv.di.nSeries; )
						if (const CSeries &s=cv.di.series[i++]){
							const auto &data=s.xy;
							const HGDIOBJ hPen0=::SelectObject( dc, data.hLinePen );
								const CHistogram h=s.CreateYxHistogram();
								for( auto it=h.cbegin(); continuePainting&&it!=h.cend(); it++ ){
									if (it->first>=cv.di.xy.xMax)
										break;
									p.params.locker.Lock();
										if (continuePainting=p.params.id==id){
											dc.MoveTo(  Transform( p.params.valuesTransf, it->first, 0 )  );
											dc.LineTo(  Transform( p.params.valuesTransf, it->first, it->second )  );
										}
									p.params.locker.Unlock();
								}
							::SelectObject( dc, hPen0 );
							if (!continuePainting) // new paint request?
								break;
						}
					break;
				default:
					ASSERT(FALSE); break;
			}
		}while (true);
		return ERROR_SUCCESS;
	}

	LRESULT CChartView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_PAINT:
				// window's size is being changed
				painter.params.locker.Lock();
					painter.params.id++;
				painter.params.locker.Unlock();
				break;
		}
		return __super::WindowProc( msg, wParam, lParam );
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

	void CChartView::SetPercentile(BYTE newPercentile){
		di.SetPercentile(newPercentile);
		Invalidate();
	}









	CChartFrame::CChartFrame(const CChartView::CDisplayInfo &di)
		// ctor
		: chartView(di)
		, menu(IDR_CHARTFRAME) {
	}

	BOOL CChartFrame::PreCreateWindow(CREATESTRUCT &cs){
		// adjusting the instantiation
		if (!__super::PreCreateWindow(cs)) return FALSE;
		cs.dwExStyle&=~WS_EX_CLIENTEDGE;
		return TRUE;
	}

	BOOL CChartFrame::PreTranslateMessage(PMSG pMsg){
		// pre-processing the Message
		return	::TranslateAccelerator( m_hWnd, menu.hAccel, pMsg );
	}

	LRESULT CChartFrame::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_CREATE:{
				const LRESULT result=__super::WindowProc(msg,wParam,lParam);
				SetMenu(&menu);
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
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:{
				// update
				CCmdUI *const pCmdUi=(CCmdUI *)pExtra;
				switch (nID){
					case ID_DATA:
					case ID_ACCURACY:
					case ID_STANDARD:
					case ID_NUMBER:
					case IDCANCEL:
						pCmdUi->Enable();
						return TRUE;
				}
				break;
			}
			case CN_COMMAND:
				// command
				switch (nID){
					case ID_DATA:
						chartView.SetPercentile(100);
						return TRUE;
					case ID_ACCURACY:
						chartView.SetPercentile(99);
						return TRUE;
					case ID_STANDARD:
						chartView.SetPercentile(97);
						return TRUE;
					case ID_NUMBER:
						if (const Utils::CSingleNumberDialog d=Utils::CSingleNumberDialog( _T("Set"), _T("Percentile"), PropGrid::Integer::TUpDownLimits::Percent, chartView.GetPercentile(), this ))
							chartView.SetPercentile(d.Value);
						return TRUE;
					case IDCANCEL:
						::PostMessage( m_hWnd, WM_DESTROY, 0, 0 );
						return TRUE;
				}
				break;
		}
		return __super::OnCmdMsg( nID, nCode, pExtra, pHandlerInfo ); // base
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
