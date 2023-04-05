#ifndef DISKHEXAVIEW_H
#define DISKHEXAVIEW_H

	class CDiskBrowserView sealed:public CHexaEditor{
		DECLARE_MESSAGE_MAP()
	private:
		struct{
			TPhysicalAddress chs;
			BYTE nSectorsToSkip;
		} seekTo;
		Yahel::TRow iScrollY; // ScrollBar position
		CComPtr<CImage::CSectorDataSerializer> f;
		Revolution::TType revolution;

		void UpdateStatusBar();
		afx_msg int OnCreate(LPCREATESTRUCT lpcs);
		afx_msg void OnDestroy();
		afx_msg void ToggleWriteProtection();
		afx_msg LRESULT ReportScanningProgress(WPARAM,LPARAM);
		afx_msg void __closeView__();
		void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override;
		int GetCustomCommandMenuFlags(WORD cmd) const override;
		bool ProcessCustomCommand(UINT cmd) override;
	public:
		static CDiskBrowserView &CreateAndSwitchToTab(PImage image,RCPhysicalAddress chsToSeekTo,BYTE nSectorsToSkip);

		const CMainWindow::CTdiView::TTab tab;

		CDiskBrowserView(PImage image,RCPhysicalAddress chsToSeekTo,BYTE nSectorsToSkip);
	};

#endif // DISKHEXAVIEW_H