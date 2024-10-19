#ifndef TRACKMAPVIEW_H
#define TRACKMAPVIEW_H

	typedef enum TSectorStatus:BYTE{
		SYSTEM,		// e.g. reserved for root Directory
		UNAVAILABLE,// Sectors that are not included in FAT (e.g. beyond the FAT, or FAT Sector error)
		SKIPPED,	// e.g. deleted Files in TR-DOS
		BAD,
		OCCUPIED,
		RESERVED,	// e.g. zero-length File in MDOS, or File with error during importing
		EMPTY,		// reported as unallocated
		UNKNOWN		// any Sector whose ID doesn't match any ID from the standard format, e.g. ID={2,1,0,3} for an MDOS Sector
	} *PSectorStatus;

	#define TRACK_MAP_TAB_LABEL	_T("Track map")

	#define TRACK_MAP_COLORS_COUNT	256

	class CTrackMapView sealed:public CScrollView{
		DECLARE_MESSAGE_MAP()
	public:
		const CMainWindow::CTdiView::TTab tab;
	private:
		bool informOnCapabilities; // True <=> user will be informed on what the TrackMap tab can do, otherwise False
		int iScrollX, iScrollY; // ScrollBar position
		struct TTrackLength sealed{
			static TTrackLength FromTime(TLogTime nNanosecondsTotal,TLogTime nNanosecondsPerByte);

			TSector nSectors;
			int nBytes;

			TTrackLength(TSector nSectors,int nBytes); // ctor

			int GetUnitCount(BYTE zoomFactor) const;
			BYTE GetZoomFactorToFitWidth(int pixelWidth) const;
			bool operator<(const TTrackLength &r) const;
		} longestTrack; // to infer maximum horizontal scroll position
		TLogTime longestTrackNanoseconds;
		enum TDisplayType:WORD{
			STATUS		=ID_TRACKMAP_STATUS,
			DATA_OK_ONLY=ID_TRACKMAP_DATA,
			DATA_ALL	=ID_TRACKMAP_BAD_DATA
		} displayType;
		HBRUSH statusBrushes[TSectorStatus::UNKNOWN+1];
		HBRUSH rainbowBrushes[TRACK_MAP_COLORS_COUNT];
		struct TTrackScanner sealed{
			static UINT AFX_CDECL Thread(PVOID _pBackgroundAction);
			const CBackgroundAction action;
			struct TParams sealed{
				CCriticalSection locker;
				THead nHeads; // 0 = terminate the Scanner
				TTrack a,z,x; // first, last, and currect Track to scan; it holds: A <= X < Z
				bool skipUnscannedTracks; // True <=> display only Tracks scanned thus far, otherwise scan all Tracks
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
		void TimesToPixels(TSector nSectors,PLogTime pInOutBuffer,PCWORD pInSectorLengths) const;
		enum TCursorPos{ NONE, TRACK, SECTOR } GetPhysicalAddressAndNanosecondsFromPoint(POINT point,TPhysicalAddress &rOutChs,BYTE &rnOutSectorsToSkip,int &rOutNanoseconds);
		void ResetStatusBarMessage() const;
		void UpdateLogicalDimensions();
		afx_msg int OnCreate(LPCREATESTRUCT lpcs);
		afx_msg void OnSize(UINT nType,int cx,int cy);
		afx_msg void OnMouseMove(UINT nFlags,CPoint point);
		afx_msg void OnLButtonUp(UINT nFlags,CPoint point);
		afx_msg void OnRButtonUp(UINT nFlags,CPoint point);
		afx_msg BOOL OnMouseWheel(UINT nFlags,short delta,CPoint point);
		afx_msg void OnDestroy();
		afx_msg LRESULT DrawTrack(WPARAM wParam,LPARAM lParam);
		afx_msg void ChangeDisplayType(UINT id);
			afx_msg void __changeDisplayType_updateUI__(CCmdUI *pCmdUI);
		afx_msg void ToggleSectorNumbering();
			afx_msg void __toggleSectorNumbering_updateUI__(CCmdUI *pCmdUI);
		afx_msg void ToggleTiming();
			afx_msg void __toggleTiming_updateUI__(CCmdUI *pCmdUI);
		afx_msg void ZoomOut();
			afx_msg void __zoomOut_updateUI__(CCmdUI *pCmdUI);
		afx_msg void ZoomIn();
			afx_msg void __zoomIn_updateUI__(CCmdUI *pCmdUI);
		afx_msg void ZoomFitWidth();
			afx_msg void __zoomFitWidth_updateUI__(CCmdUI *pCmdUI);
		afx_msg void ShowSelectedFiles();
			afx_msg void __showSelectedFiles_updateUI__(CCmdUI *pCmdUI);
		afx_msg void TogglePaused();
			afx_msg void TogglePaused_updateUI(CCmdUI *pCmdUI);
		afx_msg void ChangeFileSelectionColor();
		afx_msg void ShowDiskStatistics();
	public:
		CTrackMapView(PImage image);
		~CTrackMapView();

		BOOL Create(LPCTSTR lpszClassName,LPCTSTR lpszWindowName,DWORD dwStyle,const RECT &rect,CWnd *pParentWnd,UINT nID,CCreateContext *pContext=nullptr) override;
		afx_msg void RefreshDisplay();
	};

#endif // TRACKMAPVIEW_H
