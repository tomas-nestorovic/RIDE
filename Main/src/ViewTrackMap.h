#ifndef TRACKMAPVIEW_H
#define TRACKMAPVIEW_H

	#define TRACK_MAP_TAB_LABEL	_T("Track map")

	#define TRACK_MAP_COLORS_COUNT	256

	class CTrackMapView sealed:public CScrollView{
		DECLARE_MESSAGE_MAP()
	private:
		int iScrollX, iScrollY; // ScrollBar position
		struct TTrackLength sealed{
			static TTrackLength FromTime(int nNanosecondsTotal,int nNanosecondsPerByte);

			TSector nSectors;
			int nBytes;

			TTrackLength(TSector nSectors,int nBytes); // ctor

			int GetPixelCount(BYTE zoomFactor) const;
			BYTE GetZoomFactorToFitWidth(int windowWidth) const;
			bool operator<(const TTrackLength &r) const;
		} longestTrack; // to infer maximum horizontal scroll position
		int longestTrackNanoseconds;
		enum TDisplayType:WORD{
			STATUS		=ID_TRACKMAP_STATUS,
			DATA_OK_ONLY=ID_TRACKMAP_DATA,
			DATA_ALL	=ID_TRACKMAP_BAD_DATA
		} displayType;
		HBRUSH rainbowBrushes[TRACK_MAP_COLORS_COUNT];
		struct TTrackScanner sealed{
			static UINT AFX_CDECL __thread__(PVOID _pBackgroundAction);
			const CBackgroundAction action;
			struct TParams sealed{
				CCriticalSection criticalSection;
				TTrack a,z,x; // first, last, and currect Track to scan; it holds: A <= X < Z
			} params;
			CEvent scanNextTrack;
			TTrackScanner(const CTrackMapView *pvtm); // ctor
		} scanner;
		bool showSectorNumbers,showTimed,fitLongestTrackInWindow,showSelectedFiles;
		BYTE zoomLengthFactor;
		COLORREF fileSelectionColor;

		void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override;
		BOOL OnScroll(UINT nScrollCode,UINT nPos,BOOL bDoScroll=TRUE) override;
		void OnPrepareDC(CDC *pDC,CPrintInfo *pInfo=nullptr) override;
		void OnDraw(CDC *pDC) override;
		void PostNcDestroy() override;
		void TimesToPixels(TSector nSectors,PINT pInOutBuffer,PCWORD pInSectorLengths) const;
		bool __getPhysicalAddressFromPoint__(POINT point,TPhysicalAddress &rOutChs,BYTE &rnOutSectorsToSkip);
		void __updateStatusBarIfCursorOutsideAnySector__() const;
		void __updateLogicalDimensions__();
		afx_msg int OnCreate(LPCREATESTRUCT lpcs);
		afx_msg void OnSize(UINT nType,int cx,int cy);
		afx_msg void OnMouseMove(UINT nFlags,CPoint point);
		afx_msg void OnLButtonUp(UINT nFlags,CPoint point);
		afx_msg BOOL OnMouseWheel(UINT nFlags,short delta,CPoint point);
		afx_msg void OnDestroy();
		afx_msg LRESULT __drawTrack__(WPARAM wParam,LPARAM lParam);
		afx_msg void __changeDisplayType__(UINT id);
			afx_msg void __changeDisplayType_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __toggleSectorNumbering__();
			afx_msg void __toggleSectorNumbering_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __toggleTiming__();
			afx_msg void __toggleTiming_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __zoomOut__();
			afx_msg void __zoomOut_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __zoomIn__();
			afx_msg void __zoomIn_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __zoomFitWidth__();
			afx_msg void __zoomFitWidth_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __showSelectedFiles__();
			afx_msg void __showSelectedFiles_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __changeFileSelectionColor__();
		afx_msg void __showDiskStatistics__();
	public:
		const CMainWindow::CTdiView::TTab tab;

		CTrackMapView(PDos _dos);
		~CTrackMapView();

		afx_msg void RefreshDisplay();
	};

#endif // TRACKMAPVIEW_H
