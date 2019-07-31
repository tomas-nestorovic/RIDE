#ifndef WEBPAGEVIEW_H
#define WEBPAGEVIEW_H

	class CWebPageView sealed:public CHtmlView{
		DECLARE_MESSAGE_MAP()
	private:
		struct THistory sealed{
			#pragma pack(1)
			struct TPage sealed{
				const CString url;
				long iScrollY; // ScrollBar position
				TPage *older,*newer;
				TPage(LPCTSTR _url); // ctor
			} *currentPage;

			THistory(LPCTSTR defaultUrl);
			~THistory();

			void __destroyNewerPages__() const;
		} history;
		bool navigationToLabel;

		void OnBeforeNavigate2(LPCTSTR lpszURL,DWORD nFlags,LPCTSTR lpszTargetFrameName,CByteArray &baPostedData,LPCTSTR lpszHeaders,BOOL *pbCancel) override;
		void OnDocumentComplete(LPCTSTR strURL) override;
		void PostNcDestroy() override;
		afx_msg void OnDestroy();
		void __saveCurrentPageScrollPosition__() const;
		afx_msg void __navigateBack__();
			afx_msg void __navigateBack_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __navigateForward__();
			afx_msg void __navigateForward_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __openCurrentPageInDefaultBrowser__();
	public:
		const CMainWindow::CTdiView::TTab tab;

		CWebPageView(LPCTSTR url);

		BOOL Create(LPCTSTR,LPCTSTR,DWORD dwStyle,const RECT &rect,CWnd *pParentWnd,UINT nID,CCreateContext *) override;
	};

#endif // WEBPAGEVIEW_H