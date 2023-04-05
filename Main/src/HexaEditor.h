#ifndef HEXAEDITOR_H
#define HEXAEDITOR_H

	class CHexaEditor:public CCtrlView,protected Yahel::IOwner{
	public:
		class CYahelStreamFile:public CFile,public IStream{
			volatile ULONG nReferences;
		public:
			CYahelStreamFile();
			CYahelStreamFile(const CYahelStreamFile &r);
			virtual ~CYahelStreamFile();
			// IUnknown methods
			ULONG STDMETHODCALLTYPE AddRef() override;
			ULONG STDMETHODCALLTYPE Release() override;
			HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,PVOID *ppvObject) override;
			// IStream methods - all except for Clone are SEALED, override methods of CFile!
			HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER dlibMove,DWORD dwOrigin,ULARGE_INTEGER *plibNewPosition) override sealed;
			HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize) override sealed;
			HRESULT STDMETHODCALLTYPE CopyTo(IStream *pstm,ULARGE_INTEGER cb,ULARGE_INTEGER *pcbRead,ULARGE_INTEGER *pcbWritten) override sealed;
			HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags) override sealed;
			HRESULT STDMETHODCALLTYPE Revert() override sealed;
			HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER libOffset,ULARGE_INTEGER cb,DWORD dwLockType) override sealed;
			HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER libOffset,ULARGE_INTEGER cb,DWORD dwLockType) override sealed;
			HRESULT STDMETHODCALLTYPE Stat(STATSTG *pstatstg,DWORD grfStatFlag) override sealed;
			HRESULT STDMETHODCALLTYPE Read(PVOID target,ULONG nCount,PULONG pcbRead) override sealed;
			HRESULT STDMETHODCALLTYPE Write(LPCVOID data,ULONG dataLength,PULONG pcbWritten) override sealed;
		};
	protected:
		const CComPtr<Yahel::IInstance> instance;
		const HACCEL hDefaultAccelerators;
		Utils::CRideContextMenu contextMenu;

		void PostNcDestroy() override;
		BOOL PreTranslateMessage(PMSG pMsg) override;
		BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override sealed; // use ProcessCommand and Yahel's GetCustomCommandMenuFlags instead!
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;

		// --- Yahel::IOwner ---
		// searching
		bool QueryNewSearchParams(Yahel::TSearchParams &outSp) const override;
		Yahel::TPosition ContinueSearching(const Yahel::TSearchParams &sp) const override;
		// navigation
		bool QueryAddressToGoTo(Yahel::TGoToParams &outGtp) const override;
		// resetting
		bool QueryByteToResetSelectionWith(Yahel::TResetSelectionParams &outRsp) const override;
		// GUI
		int GetCustomCommandMenuFlags(WORD cmd) const override;
		bool ShowOpenFileDialog(LPCWSTR singleFilter,DWORD ofnFlags,PWCHAR lpszFileNameBuffer,WORD bufferCapacity) const override sealed;
		bool ShowSaveFileDialog(LPCWSTR singleFilter,DWORD ofnFlags,PWCHAR lpszFileNameBuffer,WORD bufferCapacity) const override sealed;
		void ShowInformation(Yahel::TMsg id,UINT errorCode=ERROR_SUCCESS) const override sealed;
		bool ShowQuestionYesNo(Yahel::TMsg id,UINT defaultButton=MB_DEFBUTTON2) const override sealed;

	public:
		CHexaEditor(PVOID param);

		virtual bool ProcessCustomCommand(UINT cmd);

		// general
		inline void SetEditable(bool editable) const{ return instance->SetEditable(editable); }
		inline bool IsEditable() const{ return instance->IsEditable(); }
		inline void ShowColumns(BYTE columns) const{ return instance->ShowColumns(columns); }
		inline CComPtr<IStream> GetCurrentStream() const{ CComPtr<IStream> s; s.Attach(instance->GetCurrentStream()); return s; }
		inline void Update(IStream *s,Yahel::Stream::IAdvisor *sa) const{ instance->Update(s,sa); }
		inline void Update(IStream *s,Yahel::Stream::IAdvisor *sa,const Yahel::TPosInterval &fileLogicalSizeLimits) const{ instance->Update(s,sa,fileLogicalSizeLimits); }
		inline void Reset(IStream *s,Yahel::Stream::IAdvisor *sa,const Yahel::TPosInterval &fileLogicalSizeLimits) const{ instance->Reset( s, sa, fileLogicalSizeLimits ); }
		inline bool SetLogicalSizeLimits(const Yahel::TPosInterval &limits) const{ return instance->SetStreamLogicalSizeLimits(limits); }
		inline void SetLogicalSize(Yahel::TPosition logicalSize) const{ return instance->SetStreamLogicalSize(logicalSize); }
		inline Yahel::TPosition GetCaretPosition() const{ return instance->GetCaretPosition(); }
		inline Yahel::TPosInterval GetSelectionAsc() const{ return instance->GetSelectionAsc(); }
		inline void SetSelection(Yahel::TPosition selStart,Yahel::TPosition selEnd) const{ return instance->SetSelection(selStart,selEnd); }
		inline Yahel::TPosInterval GetVisiblePart() const{ return instance->GetVisiblePart(); }
		inline void ScrollTo(Yahel::TPosition pos,bool moveAlsoCaret=false) const{ return instance->ScrollTo(pos,moveAlsoCaret); }
		inline void ScrollToRow(Yahel::TRow iRow,bool moveAlsoCaret=false) const{ return instance->ScrollToRow(iRow,moveAlsoCaret); }
		inline void RepaintData() const{ return instance->RepaintData(); }
		inline Yahel::TRow GetVertScrollPos() const{ return GetScrollPos(SB_VERT); }

		// "Address" column
		inline int GetAddressColumnWidth() const{ return instance->GetAddressColumnWidth(); }

		// "Stream" column
		inline WORD GetBytesPerRow() const{ return instance->GetStreamBytesCountPerRow(); }

		// highlights
		inline bool AddHighlight(const Yahel::TPosInterval &range) const{ return instance->AddHighlight(range,COLOR_YELLOW); }
		inline bool AddHighlight(Yahel::TPosition a,Yahel::TPosition z) const{ return instance->AddHighlight( Yahel::TPosInterval(a,z), COLOR_YELLOW ); }
		inline void RemoveAllHighlights() const{ return instance->RemoveAllHighlights(); }
	};

#endif // HEXAEDITOR_H
