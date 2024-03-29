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
		afx_msg void ToggleWriteProtection();
		void __updateLookOfControls__();
	protected:
		class CSectorReaderWriter:public CDos::CFileReaderWriter{
		public:
			CSectorReaderWriter(PCDos dos,RCPhysicalAddress chs);

			// CFile methods
			void Write(LPCVOID lpBuf,UINT nCount) override;
			// IStream methods
			HRESULT STDMETHODCALLTYPE Clone(IStream **ppstm) override;
		};

		CComPtr<CSectorReaderWriter> fSectorData;
		CWnd propGrid;
		CDos::CFileReaderWriter::CHexaEditor hexaEditor;

		CCriticalSectorView(PDos dos,RCPhysicalAddress rChs);

		virtual void OnSectorChanging() const;
		void OnDraw(CDC *pDC) override sealed;
		void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override;
		void PostNcDestroy() override;
	public:
		static const CCriticalSectorView *pCurrentlyShown; // Sector that is currently shown (a volume can have multiple critical Sectors)

		static void WINAPI __updateCriticalSectorView__(PropGrid::PCustomParam);
		static bool __isValueBeingEditedInPropertyGrid__();

		const CMainWindow::CTdiView::TTab tab;

		RCPhysicalAddress GetPhysicalAddress() const;
		void ChangeToSector(RCPhysicalAddress rChs);
		void MarkSectorAsDirty() const;
	};

#endif // CRITICALSECTORVIEW_H
