#ifndef MAINWINDOW_H
#define MAINWINDOW_H

	#define TDI_INSTANCE	((CMainWindow *)app.m_pMainWnd)->pTdi
	#define TDI_HWND		TDI_INSTANCE->m_hWnd


	class CMainWindow sealed:public CFrameWnd{
		DECLARE_MESSAGE_MAP()
	private:
		BOOL PreCreateWindow(CREATESTRUCT &cs) override;
		BOOL PreTranslateMessage(PMSG pMsg) override;
		afx_msg int OnCreate(LPCREATESTRUCT lpcs);
		afx_msg void OnDropFiles(HDROP dropInfo);
		afx_msg void OnLButtonDblClk(UINT,CPoint);
		afx_msg void OnInitMenu(CMenu *menu);
			afx_msg void __imageOperation_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __switchToNextTab__();
		afx_msg void __switchToPrevTab__();
		afx_msg void __closeCurrentTab__();
			afx_msg void __closeCurrentTab_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __openUrl_checkForUpdates__();
			afx_msg void __openUrl_checkForUpdates_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __openUrl_faq__();
		afx_msg void OpenUrl_Donate();
		afx_msg void __openUrl_reportBug__();
		afx_msg void __openUrl_repository__();
		afx_msg void __openUrl_tutorials__();
		afx_msg void __openUrl_credits__();
	public:
		struct CDynMenu:public CMenu{
			const HACCEL hAccel;

			CDynMenu(UINT nResId);
			~CDynMenu();
		public:
			void Show(UINT position) const;
			void Hide() const;
		};

		class CDockableToolBar sealed:public CToolBar{
			void OnUpdateCmdUI(CFrameWnd* pTarget,BOOL bDisableIfNoHndler) override;
		public:
			CDockableToolBar(UINT nResId,UINT id);

			void Show(const CToolBar &rDockNextTo);
			void Hide();
		};

		class CTdiTemplate sealed:public CSingleDocTemplate{
		public:
			static CTdiTemplate *pSingleInstance;

			CTdiTemplate();
			~CTdiTemplate();

			CDocument *__getDocument__() const;
			bool __closeDocument__();

			#if _MFC_VER>=0x0A00
			CDocument *OpenDocumentFile(LPCTSTR lpszPathName,BOOL bAddToMRU,BOOL bMakeVisible) override;
			#else
			CDocument *OpenDocumentFile(LPCTSTR lpszPathName,BOOL bMakeVisible=TRUE) override;
			#endif
		};

		class CTdiView sealed:public CCtrlView{
			friend class CMainWindow;
		public:
			typedef CView *PView;

			typedef struct TTab sealed{
				static void WINAPI OnOptionalTabClosing(CTdiCtrl::TTab::PContent tab);

				const PImage image; // the Image that gets into focus when switched to this Tab (e.g. CSpectrumDos::CTape)
				const CDynMenu menu;
				const PView view;
				CDockableToolBar toolbar;

				TTab(UINT nMenuResId,UINT nToolbarResId,UINT nToolBarId,PImage image,PView _view);

				inline bool IsPartOfImage() const{ return image!=nullptr; } // True <=> the Tab is part of an Image (e.g. a WebPage usually isn't), otherwise False
			} *PTab;
		private:
			static void WINAPI __fnShowContent__(PVOID pTdi,LPCVOID pTab);
			static void WINAPI __fnHideContent__(PVOID pTdi,LPCVOID pTab);
			static void WINAPI __fnRepaintContent__(LPCVOID pTab);
			static HWND WINAPI __fnGetHwnd__(LPCVOID pTab);

			const CBackgroundAction recencyStatusThread;
			PTab pCurrentTab;

			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		public:
			static UINT AFX_CDECL RecencyDetermination_thread(PVOID pCancelableAction);

			CTdiView();

			void CloseAllTabsOfFocusedImage();
			inline PTab GetCurrentTab() const{ return pCurrentTab; }
			void RepopulateGuidePost() const;
		} *pTdi;

		static void __resetStatusBar__();
		static void __setStatusBarText__(LPCTSTR text);

		CToolBar toolbar;
		CStatusBar statusBar;

		void OpenWebPage(LPCTSTR tabCaption,LPCTSTR url);
		void OpenRepositoryWebPage(LPCTSTR tabCaption,LPCTSTR documentName);
		void OpenApplicationPresentationWebPage(LPCTSTR tabCaption,LPCTSTR documentName);
		void OpenApplicationFaqWebPage(LPCTSTR documentName);
		afx_msg void __changeAutomaticDiskRecognitionOrder__();
	};


#endif // MAINWINDOW_H
