#ifndef CRITICALSECTORVIEW_H
#define CRITICALSECTORVIEW_H

	class CCriticalSectorView:public CView{
		DECLARE_MESSAGE_MAP()
	private:
		std::unique_ptr<CSplitterWnd> content; // newly created for whenever Sector is switched to in TDI
		int splitX;

		afx_msg int OnCreate(LPCREATESTRUCT lpcs);
		afx_msg void OnSize(UINT nType,int cx,int cy);
		afx_msg void OnKillFocus(CWnd *newFocus);
		afx_msg void OnDestroy();
		afx_msg void __toggleWriteProtection__();
		void __updateLookOfControls__();
	protected:
		TPhysicalAddress chs;
		CDos::CFileReaderWriter fSectorData;
		CWnd propGrid;
		CHexaEditor hexaEditor;

		CCriticalSectorView(PDos dos,RCPhysicalAddress rChs);

		void OnDraw(CDC *pDC) override sealed;
		void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override;
		void PostNcDestroy() override;
	public:
		static const CCriticalSectorView *pCurrentlyShown; // Sector that is currently shown (a volume can have multiple critical Sectors)

		static void WINAPI __updateCriticalSectorView__(CPropGridCtrl::PCustomParam);
		static bool __isValueBeingEditedInPropertyGrid__();

		const CMainWindow::CTdiView::TTab tab;

		RCPhysicalAddress GetPhysicalAddress() const;
		void ChangeToSector(RCPhysicalAddress rChs);
		void MarkSectorAsDirty() const;
	};

#endif // CRITICALSECTORVIEW_H
