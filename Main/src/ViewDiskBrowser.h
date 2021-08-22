#ifndef DISKHEXAVIEW_H
#define DISKHEXAVIEW_H

	class CDiskBrowserView sealed:public CHexaEditor{
		DECLARE_MESSAGE_MAP()
	private:
		struct{
			TPhysicalAddress chs;
			BYTE nSectorsToSkip;
		} seekTo;
		int iScrollY; // ScrollBar position
		std::unique_ptr<CImage::CSectorDataSerializer> f;
		Revolution::TType revolution;

		afx_msg int OnCreate(LPCREATESTRUCT lpcs);
		afx_msg void OnDestroy();
		afx_msg void ToggleWriteProtection();
		afx_msg void __closeView__();
		void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override;
		BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override;
	public:
		static void WINAPI OnDiskBrowserViewClosing(LPCVOID tab);

		const CMainWindow::CTdiView::TTab tab;

		CDiskBrowserView(PDos dos,RCPhysicalAddress chsToSeekTo,BYTE nSectorsToSkip);
		~CDiskBrowserView();

		void SetLogicalSize(int newLogicalSize) override;
	};

#endif // DISKHEXAVIEW_H