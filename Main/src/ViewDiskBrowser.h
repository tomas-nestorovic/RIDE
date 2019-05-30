#ifndef DISKHEXAVIEW_H
#define DISKHEXAVIEW_H

	class CDiskBrowserView sealed:public CHexaEditor{
		DECLARE_MESSAGE_MAP()
	private:
		int iScrollY; // ScrollBar position
		CImage::CSectorDataSerializer *f;

		afx_msg int OnCreate(LPCREATESTRUCT lpcs);
		afx_msg void OnDestroy();
		afx_msg void __toggleWriteProtection__();
		afx_msg void __closeView__();
		void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override;
		BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override;
	public:
		const CMainWindow::CTdiView::TTab tab;

		CDiskBrowserView(PDos dos);
		~CDiskBrowserView();
	};

#endif // DISKHEXAVIEW_H