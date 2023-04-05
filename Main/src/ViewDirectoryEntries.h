#ifndef DIRHEXAVIEW_H
#define DIRHEXAVIEW_H

	class CDirEntriesView sealed:public CHexaEditor{
		DECLARE_MESSAGE_MAP()
	private:
		CDos::PCFile fileToSeekTo;
		CComPtr<CDos::CFileReaderWriter> f;

		afx_msg int OnCreate(LPCREATESTRUCT lpcs);
		afx_msg void ToggleWriteProtection();
		afx_msg void __closeView__();
		void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override;
		int GetCustomCommandMenuFlags(WORD cmd) const override;
		bool ProcessCustomCommand(UINT cmd) override;
	public:
		const CMainWindow::CTdiView::TTab tab;
		const CDos::PFile directory;
		const WORD sectorLength; // e.g. for Spectrum Tape, the SectorLength may temporarily be faked to correctly segment a display Headers, and then reset to normal to correctly display Tape data; this is the backup of the eventually faked value

		CDirEntriesView(PDos dos,CDos::PFile directory,CDos::PCFile fileToSeekTo=nullptr);
	};

#endif // DIRHEXAVIEW_H