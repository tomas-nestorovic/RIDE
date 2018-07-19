#ifndef TRACKMAPVIEW_H
#define TRACKMAPVIEW_H

	#define TRACK_MAP_TAB_LABEL	_T("Track map")

	#define TRACK_MAP_COLORS_COUNT	256

	class CTrackMapView sealed:public CScrollView{
		DECLARE_MESSAGE_MAP()
	private:
		int iScrollY; // ScrollBar position
		enum TDisplayType:WORD{
			STATUS		=ID_TRACKMAP_STATUS,
			DATA_OK_ONLY=ID_TRACKMAP_DATA,
			DATA_ALL	=ID_TRACKMAP_BAD_DATA
		} displayType;
		HBRUSH rainbowBrushes[TRACK_MAP_COLORS_COUNT];
		struct TTrackScanner sealed{
			static UINT AFX_CDECL __thread__(PVOID _pBackgroundAction);
			const TBackgroundAction action;
			struct TParams sealed{
				CCriticalSection criticalSection;
				TTrack a,z,x; // first, last, and currect Track to scan; it holds: A <= X < Z
			} params;
			CEvent scanNextTrack;
			TTrackScanner(const CTrackMapView *pvtm); // ctor
		} scanner;
		bool showSectorNumbers,highlightBadSectors;

		void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override;
		BOOL OnScroll(UINT nScrollCode,UINT nPos,BOOL bDoScroll=TRUE) override;
		void OnPrepareDC(CDC *pDC,CPrintInfo *pInfo=NULL) override;
		void OnDraw(CDC *pDC) override;
		void PostNcDestroy() override;
		void __updateStatusBarIfCursorOutsideAnySector__() const;
		afx_msg int OnCreate(LPCREATESTRUCT lpcs);
		afx_msg void OnMouseMove(UINT nFlags,CPoint point);
		afx_msg void OnDestroy();
		afx_msg LRESULT __drawTrack__(WPARAM wParam,LPARAM lParam);
		afx_msg void __changeDisplayType__(UINT id);
			afx_msg void __changeDisplayType_updateUI__(CCmdUI *pCmdUI) const;
		afx_msg void __toggleSectorNumbering__();
			afx_msg void __toggleSectorNumbering_updateUI__(CCmdUI *pCmdUI) const;
		afx_msg void __showDiskStatistics__() const;
	public:
		const CMainWindow::CTdiView::TTab tab;

		CTrackMapView(PDos _dos);
	};

#endif // TRACKMAPVIEW_H
