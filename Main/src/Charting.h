#ifndef CHARTING_H
#define CHARTING_H

	class CChartView:public CView{
		const Utils::CRidePen gridPen;
		const Utils::CRideFont &fontAxes;

		struct TPainter sealed{
			const CBackgroundAction action;
			struct{
				CCriticalSection locker;
				WORD id;
				XFORM valuesTransf;
			} params;
			CEvent repaintEvent;

			static UINT AFX_CDECL Thread(PVOID _pBackgroundAction);

			TPainter(CChartView &cv);
		} painter;
	public:
		enum TType:BYTE{
			XY_LINE_BROKEN,
			XY_BARS
		};

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

		typedef const class CSeries sealed{
			CSeries(DWORD nValues);
		public:
			const DWORD nValues;
			union{
				struct{
					const POINT *pValues;
					HPEN hLinePen, hVertexPen;
				} xy;
			};

			static CSeries CreateXy(DWORD nValues,const POINT *pXyValues,HPEN hLinePen,HPEN hVertexPen);

			inline operator bool() const{
				return nValues>0;
			}

			CHistogram CreateYxHistogram() const;
		} *PCSeries;

		class CDisplayInfo sealed{
			WORD percentile; // in hundredths (e.g. "2345" means 23.45)

			CDisplayInfo(TType chartType,RCMargin margin,PCSeries series,BYTE nSeries);
		public:
			const TType chartType;
			const TMargin margin;
			const PCSeries series;
			const BYTE nSeries;
			union{
				struct{
					TLogValue xMax,yMax;
					TCHAR xAxisUnit,yAxisUnit;
					LPCTSTR xAxisUnitPrefixes,yAxisUnitPrefixes;
				} xy;
			};

			static CDisplayInfo CreateXy(
				TType chartType, RCMargin margin, PCSeries series, BYTE nSeries,
				TCHAR xAxisUnit, TLogValue xMax, LPCTSTR xAxisUnitPrefixes,
				TCHAR yAxisUnit, TLogValue yMax, LPCTSTR yAxisUnitPrefixes
			);

			inline WORD GetPercentile() const{
				return percentile;
			}
			void SetPercentile(WORD newPercentile);
		} di;
	protected:
		XFORM DrawXyAxes(HDC dc) const;
		void OnDraw(CDC *pDC) override;
		void PostNcDestroy() override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		CChartView(const CDisplayInfo &di);

		inline WORD GetPercentile() const{
			return di.GetPercentile();
		}
		void SetPercentile(WORD newPercentile);
	};






	class CChartFrame:public CFrameWnd{
		CChartView chartView;
		CMainWindow::CDynMenu menu;
	protected:
		BOOL PreCreateWindow(CREATESTRUCT &cs) override;
		BOOL PreTranslateMessage(PMSG pMsg) override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override;
	public:
		CChartFrame(const CChartView::CDisplayInfo &di);
	};






	class CChartDialog:public CChartFrame{
	protected:
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		void PostNcDestroy() override;
	public:
		CChartDialog(const CChartView::CDisplayInfo &di);

		void ShowModal(
            LPCTSTR caption,
            CWnd *pParentWnd = nullptr,
            const RECT &rect = rectDefault,
            DWORD dwStyle = WS_MAXIMIZEBOX|WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_VISIBLE
		);
	};

#endif // CHARTING_H
