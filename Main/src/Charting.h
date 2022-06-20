#ifndef CHARTING_H
#define CHARTING_H

	class CChartView:public CView{
	public:
		typedef const struct TMargin sealed{
			static const TMargin None;
			static const TMargin Default;

			short L,R,T,B;
		} &RCMargin;

		class CHistogram:public std::map<int,int>{
		public:
			operator bool() const;
			void AddValue(int value);
			int GetCount(int value) const;
			CHistogram &Append(const CHistogram &r);
		};

		class CPainter; // forward

		typedef const class CGraphics abstract{
		protected:
			CGraphics();
		public:
			bool visible;

			virtual void DrawAsync(const CPainter &p) const=0;
		} *PCGraphics;

		typedef const class CXyGraphics abstract:public CGraphics{
		public:
			virtual void GetDrawingLimits(WORD percentile,TLogValue &rOutMaxX,TLogValue &rOutMaxY) const; // in hundredths (e.g. "2345" means 23.45)
		} *PCXyGraphics;

		class CXyPointSeries:public CXyGraphics{
		protected:
			const DWORD nPoints;
			const POINT *const points;
			const HPEN hPen;
		public:
			CXyPointSeries(DWORD nPoints,const POINT *points,HPEN hVertexPen);

			void GetDrawingLimits(WORD percentile,TLogValue &rOutMaxX,TLogValue &rOutMaxY) const override; // in hundredths (e.g. "2345" means 23.45)
			void DrawAsync(const CPainter &p) const override;
			CHistogram CreateXyHistogram() const;
			CHistogram CreateYxHistogram() const;
		};

		class CXyBrokenLineSeries:public CXyPointSeries{
		public:
			CXyBrokenLineSeries(DWORD nPoints,const POINT *points,HPEN hLinePen);

			void DrawAsync(const CPainter &p) const override;
		};

		class CXyOrderedBarSeries:public CXyPointSeries{
		public:
			CXyOrderedBarSeries(DWORD nPoints,const POINT *points,HPEN hLinePen);

			void GetDrawingLimits(WORD percentile,TLogValue &rOutMaxX,TLogValue &rOutMaxY) const override; // in hundredths (e.g. "2345" means 23.45)
			void DrawAsync(const CPainter &p) const override;
		};

		class CDisplayInfo abstract{
		public:
			const UINT menuResourceId;
			const TMargin margin;
			const PCGraphics *const graphics;
			const BYTE nGraphics;

			CDisplayInfo(UINT menuResourceId,RCMargin margin,const PCGraphics graphics[],BYTE nGraphics);

			virtual void DrawBackground(HDC dc,const CRect &rcClient)=0;
			virtual void DrawCursorAt(HDC dc,const POINT &ptClient,const CRect &rcClient)=0;
			virtual bool OnCmdMsg(CChartView &cv,UINT nID,int nCode,PVOID pExtra);
		};

		class CXyDisplayInfo:public CDisplayInfo{
			const Utils::CRidePen gridPen;
			const Utils::CRideFont &fontAxes;
			const TCHAR xAxisUnit,yAxisUnit;
			const LPCTSTR xAxisUnitPrefixes,yAxisUnitPrefixes;
			const TLogValue xMaxOrg, yMaxOrg;
			Utils::CAxis xAxis, yAxis;
			WORD percentile; // in hundredths (e.g. "2345" means 23.45)
		public:
			XFORM M; // matrix to transform data coordinates (TLogValue) to display coordinates (pixels)

			CXyDisplayInfo(
				RCMargin margin,
				const PCGraphics graphics[], BYTE nGraphics,
				const Utils::CRideFont &fontAxes,
				TCHAR xAxisUnit, TLogValue xMax, LPCTSTR xAxisUnitPrefixes,
				TCHAR yAxisUnit, TLogValue yMax, LPCTSTR yAxisUnitPrefixes
			);

			inline const Utils::CAxis &GetAxisX() const{ return xAxis; }
			inline const Utils::CAxis &GetAxisY() const{ return yAxis; }
			void DrawBackground(HDC dc,const CRect &rcClient) override;
			void DrawCursorAt(HDC dc,const POINT &ptClient,const CRect &rcClient) override;
			POINT Transform(long x,long y) const;
			inline POINT Transform(const POINT &pt) const{ return Transform( pt.x, pt.y ); }
			RECT Transform(const RECT &rc) const;
			POINT InverselyTransform(const POINT &pt) const;
			inline WORD GetPercentile() const{ return percentile; }
			void SetPercentile(WORD newPercentile);
			bool OnCmdMsg(CChartView &cv,UINT nID,int nCode,PVOID pExtra) override;
		};

		class CPainter sealed:public CBackgroundAction{
			static UINT AFX_CDECL Thread(PVOID _pBackgroundAction);
		public:
			CDisplayInfo &di;
			mutable CCriticalSection locker;
			WORD drawingId;
			HDC dc;
			CEvent redrawEvent;

			CPainter(const CChartView &cv,CDisplayInfo &di);

			WORD GetCurrentDrawingIdSync() const;
		} painter;
	protected:
		void OnDraw(CDC *pDC) override;
		void PostNcDestroy() override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		CChartView(CDisplayInfo &di);

		inline const CDisplayInfo &GetDisplayInfo() const{ return painter.di; }
	};






	class CChartFrame:public CFrameWnd{
	protected:
		CChartView chartView;

		BOOL PreCreateWindow(CREATESTRUCT &cs) override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override;
	public:
		CChartFrame(CChartView::CDisplayInfo &di);
	};






	class CChartDialog:public CChartFrame{
	protected:
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		void PostNcDestroy() override;
	public:
		CChartDialog(CChartView::CDisplayInfo &di);

		void ShowModal(
            LPCTSTR caption,
            CWnd *pParentWnd = nullptr,
            WORD width=800, WORD height=600,
            DWORD dwStyle = WS_MAXIMIZEBOX|WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_VISIBLE
		);
	};

#endif // CHARTING_H
