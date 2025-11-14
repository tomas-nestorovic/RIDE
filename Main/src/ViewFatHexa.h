#ifndef FATHEXAVIEW_H
#define FATHEXAVIEW_H

	class CFatHexaView:public CHexaEditor{
	public:
		class CFatPathReaderWriter:public CYahelStreamFile,public Yahel::Stream::IAdvisor{
			const PCDos dos;
			const CDos::PFile file;
			const CDos::CFatPath fatPath;
		public:
			CFatPathReaderWriter(PCDos dos,CDos::PFile file);

			// IStream methods
			HRESULT STDMETHODCALLTYPE Clone(IStream **ppstm) override;

			// CFile methods
			#if _MFC_VER>=0x0A00
				void SetLength(ULONGLONG dwNewLen) override;
			#else
				void SetLength(DWORD dwNewLen) override;
			#endif
			UINT Read(LPVOID lpBuf,UINT nCount) override;
			void Write(LPCVOID lpBuf,UINT nCount) override;

			// Yahel::Stream::IAdvisor methods
			void GetRecordInfo(Yahel::TPosition pos,Yahel::PPosition pOutRecordStartLogPos,Yahel::PPosition pOutRecordLength,bool *pOutDataReady) override;
			Yahel::TRow LogicalPositionToRow(Yahel::TPosition pos,WORD nStreamBytesInRow) override;
			Yahel::TPosition RowToLogicalPosition(Yahel::TRow row,WORD nStreamBytesInRow) override;
			LPCWSTR GetRecordLabelW(Yahel::TPosition pos,PWCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const override;
		};
	protected:
		const CDos::PFile file;
		CComPtr<CFatPathReaderWriter> pFatData;

		bool ProcessCustomCommand(UINT cmd) override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		const CMainWindow::CTdiView::TTab tab;

		CFatHexaView(PDos dos,CDos::PFile file,LPCWSTR itemDefinition);
	};

#endif // FATHEXAVIEW_H
