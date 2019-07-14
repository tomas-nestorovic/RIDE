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
		afx_msg void OnInitMenu(CMenu *menu);
			afx_msg void __imageOperation_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __switchToNextTab__();
		afx_msg void __switchToPrevTab__();
		afx_msg void __closeCurrentTab__();
			afx_msg void __closeCurrentTab_updateUI__(CCmdUI *pCmdUI) const;
		afx_msg void __openUrl_whatsNew__();
		afx_msg void __openUrl_checkForUpdates__();
		afx_msg void __openUrl_faq__();
		afx_msg void __openUrl_reportBug__();
		afx_msg void __openUrl_credits__();
	public:
		struct TDynMenu sealed{
			const HMENU hMenu;
			const HACCEL hAccel;

			TDynMenu(UINT nResId);
			~TDynMenu();
		public:
			void __show__(UINT position) const;
			void __hide__() const;
		};

		class CDockableToolBar sealed:public CToolBar{
			void OnUpdateCmdUI(CFrameWnd* pTarget,BOOL bDisableIfNoHndler) override;
		public:
			CDockableToolBar(UINT nResId,UINT id);

			void __show__(const CToolBar &rDockNextTo);
			void __hide__();
		};

		class CTdiTemplate sealed:public CSingleDocTemplate{
		public:
			static CTdiTemplate *pSingleInstance;

			CTdiTemplate();
			~CTdiTemplate();

			CDocument *__getDocument__() const;
			bool __closeDocument__();
			CDocument *OpenDocumentFile(LPCTSTR lpszPathName,BOOL bMakeVisible=TRUE) override;
		};

		class CTdiView sealed:public CCtrlView{
			friend class CMainWindow;
		public:
			typedef CView *PView;

			typedef struct TTab sealed{
				const PDos dos; // DOS that gets into focus when switched to this Tab (e.g. CSpectrumDos::CTape)
				const TDynMenu menu;
				const PView view;
				CDockableToolBar toolbar;
				TTab(UINT nMenuResId,UINT nToolbarResId,UINT nToolBarId,PDos _dos,PView _view); // ctor
			} *PTab;
		private:
			static void WINAPI __fnShowContent__(PVOID pTdi,LPCVOID pTab);
			static void WINAPI __fnHideContent__(PVOID pTdi,LPCVOID pTab);
			static void WINAPI __fnRepaintContent__(LPCVOID pTab);
			static HWND WINAPI __fnGetHwnd__(LPCVOID pTab);

			PTab pCurrentTab;

			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		public:
			CTdiView();

			static PDos __getFocusedDos__();

			void __closeAllTabsOfFocusedDos__();
			PTab __getCurrentTab__() const;
		} *pTdi;

		static void __resetStatusBar__();
		static void __setStatusBarText__(LPCTSTR text);

		CToolBar toolbar;
		CStatusBar statusBar;

		void OpenWebPage(LPCTSTR tabCaption,LPCTSTR url);
		void OpenApplicationPresentationWebPage(LPCTSTR tabCaption,LPCTSTR documentName);
		afx_msg void __changeAutomaticDiskRecognitionOrder__() const;
	};


#endif // MAINWINDOW_H
