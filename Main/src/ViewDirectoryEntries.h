#ifndef DIRHEXAVIEW_H
#define DIRHEXAVIEW_H

	class CDirEntriesView sealed:public CHexaEditor{
		DECLARE_MESSAGE_MAP()
	private:
		bool navigatedToFirstSelectedFile;
		std::unique_ptr<CDos::CFileReaderWriter> f;

		afx_msg int OnCreate(LPCREATESTRUCT lpcs);
		afx_msg void OnDestroy();
		afx_msg void ToggleWriteProtection();
		afx_msg void __closeView__();
		void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override;
		BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override;
	public:
		const CMainWindow::CTdiView::TTab tab;
		const CDos::PFile directory;

		CDirEntriesView(PDos dos,CDos::PFile directory);
		~CDirEntriesView();
	};

#endif // DIRHEXAVIEW_H