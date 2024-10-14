#ifndef CHARTING_H
#define CHARTING_H

// any value that is 'long' is in device pixels (incl. 'POINT' and 'RECT' structs!)
// any value that is 'int' is in device units (e.g. for drawing)
// any value that is 'TLogValue' is in Axis units

namespace Charting
{
	typedef int TIndex;

	typedef LPCVOID PCItem;

	class CChartView:public CView{
	public:
		enum TStatus{
			DRAWING,
			READY
		};

		typedef const struct TMargin sealed{
			static const TMargin None;
			static const TMargin Default;

			int L,R,T,B;
		} &RCMargin;

		class CHistogram:public std::map<TLogValue,TIndex>{
		public:
			inline operator bool() const{ return size()>0; }
			void AddValue(TLogValue value);
			TIndex GetCount(TLogValue value) const;
			CHistogram &Append(const CHistogram &r);
		};

		class CPainter; // forward
		class CDisplayInfo; // forward

		// see type convention note above
		typedef const class CGraphics abstract{
		protected:
			CGraphics();
		public:
			bool visible;
			LPCTSTR name; // used for cursor snapping

			inline bool SupportsCursorSnapping() const{ return name!=nullptr; }
			virtual PCItem GetItem(TIndex i,const POINT &ptClient) const;
			virtual TIndex GetNearestItemIndex(const CDisplayInfo &di,const POINT &ptClient,TLogValue &rOutDistance) const;
			virtual void DrawAsync(const CPainter &p) const=0;
		} *PCGraphics;

		// see type convention note above
		typedef const class CXyGraphics abstract:public CGraphics{
		public:
			virtual void GetDrawingLimits(WORD percentile,TLogValue &rOutMaxX,TLogValue &rOutMaxY) const; // in hundredths (e.g. "2345" means 23.45)
		} *PCXyGraphics;

		// see type convention note above
		class CXyPointSeries:public CXyGraphics{
		protected:
			const TIndex nPoints;
			const PCLogPoint points;
			const HPEN hPen;
		public:
			CXyPointSeries(TIndex nPoints,PCLogPoint points,HPEN hVertexPen);

			void GetDrawingLimits(WORD percentile,TLogValue &rOutMaxX,TLogValue &rOutMaxY) const override; // in hundredths (e.g. "2345" means 23.45)
			PCItem GetItem(TIndex i,const POINT &ptClient) const override;
			inline TIndex GetPointCount() const{ return nPoints; }
			void DrawAsync(const CPainter &p) const override;
			CHistogram CreateYxHistogram(TLogValue mergeFilter=0) const;
		};

		// see type convention note above
		class CXyBrokenLineSeries:public CXyPointSeries{
		public:
			CXyBrokenLineSeries(TIndex nPoints,PCLogPoint points,HPEN hLinePen);

			void DrawAsync(const CPainter &p) const override;
		};

		// see type convention note above
		class CXyOrderedBarSeries:public CXyPointSeries{
		public:
			CXyOrderedBarSeries(TIndex nPoints,PCLogPoint points,HPEN hLinePen,LPCTSTR name=nullptr);

			void GetDrawingLimits(WORD percentile,TLogValue &rOutMaxX,TLogValue &rOutMaxY) const override; // in hundredths (e.g. "2345" means 23.45)
			TIndex GetNearestItemIndex(const CDisplayInfo &di,const POINT &ptClient,TLogValue &rOutDistance) const override;
			void DrawAsync(const CPainter &p) const override;
		};

		// see type convention note above
		class CDisplayInfo abstract{
		public:
			typedef const struct TSnappedItem{
				PCGraphics graphics;
				TIndex itemIndex;
			} *PCSnappedItem;
		protected:
			bool snapToNearestItem;
			TSnappedItem snapped;
		public:
			const UINT menuResourceId;
			const TMargin margin;
			const PCGraphics *const graphics;
			const BYTE nGraphics;

			CDisplayInfo(UINT menuResourceId,RCMargin margin,const PCGraphics graphics[],BYTE nGraphics);

			inline bool WantSnapToNearestItem() const{ return snapToNearestItem; }
			virtual void DrawBackground(HDC dc,const CRect &rcClient)=0;
			virtual PCSnappedItem DrawCursor(HDC dc,const POINT &ptClient);
			virtual CString GetStatus() const=0;
			virtual bool OnCmdMsg(CChartView &cv,UINT nID,int nCode,PVOID pExtra);
		};

		// see type convention note above
		class CXyDisplayInfo:public CDisplayInfo{
			const Utils::CRidePen gridPen;
			const Utils::CRideFont &fontAxes;
			const TLogValue xMaxOrg, yMaxOrg;
			Utils::CAxis xAxis, yAxis;
			WORD percentile; // in hundredths (e.g. "2345" means 23.45)
			Utils::TGdiMatrix mValues; // matrix to transform LogPoints to display coordinates (pixels)
		public:
			TLogInterval xAxisFocus;

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
			PCSnappedItem DrawCursor(HDC dc,const POINT &ptClient) override;
			inline WORD GetPercentile() const{ return percentile; }
			void SetPercentile(WORD newPercentile);
			void RefreshDrawingLimits(TLogValue xNewMax=-1,TLogValue yNewMax=-1);
			POINT GetClientUnits(TLogValue x,TLogValue y) const;
			inline POINT GetClientUnits(const TLogPoint &pt) const{ return GetClientUnits( pt.x, pt.y ); }
			CString GetStatus() const override;
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
		TStatus status;

		void SetStatus(TStatus newStatus);
		void OnDraw(CDC *pDC) override;
		void PostNcDestroy() override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		static const UINT WM_CHART_STATUS_CHANGED;

		CChartView(CDisplayInfo &di);

		inline TStatus GetStatus() const{ return status; }
		LPCTSTR GetCaptionSuffix() const;
		inline const CDisplayInfo &GetDisplayInfo() const{ return painter.di; }
	};






	class CChartFrame:public CFrameWnd{
	protected:
		CString captionBase;
		CChartView chartView;
		CStatusBar statusBar;

		BOOL PreCreateWindow(CREATESTRUCT &cs) override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override;
	public:
		CChartFrame(const CString &caption,CChartView::CDisplayInfo &di);
	};






	class CChartDialog:public CChartFrame{
	protected:
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		void PostNcDestroy() override;
	public:
		CChartDialog(CChartView::CDisplayInfo &di);

		void ShowModal(
			const CString &caption,
            CWnd *pParentWnd = nullptr,
            WORD width=800, WORD height=600,
            DWORD dwStyle = WS_MAXIMIZEBOX|WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_VISIBLE
		);
	};

}
#endif // CHARTING_H
