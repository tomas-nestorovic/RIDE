#ifndef HEXAEDITOR_H
#define HEXAEDITOR_H

	#define HEXAEDITOR_RECORD_SIZE_INFINITE	0x7fffff00

	class CRideFont; // forward

	class CHexaEditor:public CEditView{
	public:
		typedef interface IContentAdviser{
			virtual void GetRecordInfo(int logPos,PINT pOutRecordStartLogPos,PINT pOutRecordLength,bool *pOutDataReady)=0;
			virtual int LogicalPositionToRow(int logPos,BYTE nBytesInRow)=0;
			virtual int RowToLogicalPosition(int row,BYTE nBytesInRow)=0;
			virtual LPCTSTR GetRecordLabel(int logPos,PTCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const=0;
		} *PContentAdviser;
	private:
		class COleBinaryDataSource sealed:public COleDataSource{
			CFile *const f;
			const DWORD dataBegin,dataLength;
		public:
			COleBinaryDataSource(CFile *_f,DWORD _dataBegin,DWORD dataEnd);

			//BOOL OnRenderFileData(LPFORMATETC lpFormatEtc,CFile *pFile) override;
		};

		typedef struct TEmphasis sealed{ // aka, "highlightings" in listing of underlying File (e.g. when comparing two Files using FileManager)
			static const TEmphasis Terminator;

			int a,z;
			TEmphasis *pNext;
		} *PEmphasis;

		static void __informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId);

		const PVOID param;
		const CRideFont font;
		const HACCEL hDefaultAccelerators;
		BYTE nBytesInRow;
		int nLogicalRows;
		int nRowsDisplayed;
		int nRowsOnPage;
		HWND hPreviouslyFocusedWnd;
		struct TCursor sealed{
			bool ascii; // True <=> Cursor is in the Ascii listing section
			bool hexaLow; // True <=> ready to modify the lower half-byte in hexa mode
			int position; // current logical position in underlying File
			int selectionA,selectionZ; // beginning (including) and end (excluding)
			TCursor(int position); // ctor
			void __detectNewSelection__();
			inline void __cancelSelection__();
		} cursor;
		class CBookmarks sealed:CDWordArray{
		public:
			void __addBookmark__(int logPos);
			void __removeBookmark__(int logPos);
			void __removeAllBookmarks__();
			int __getNearestNextBookmarkPosition__(int logPos) const;
		} bookmarks;
		PEmphasis emphases; // must be ordered ascending by A (and thus automatically also by Z)

		CFile *f;
		PContentAdviser pContentAdviser;
		int minFileSize,maxFileSize;
		int logicalSize; // zero by default
		BYTE addrLength; // Address format length (see ADDRESS_FORMAT); modified in ShowAddresses
		bool editable;

		int __firstByteInRowToLogicalPosition__(int row) const;
		int __logicalPositionToRow__(int logPos) const;
		int __logicalPositionFromPoint__(const POINT &rPoint,bool *pOutAsciiArea) const;
		int __scrollToRow__(int row);
		void __refreshVertically__();
		void __refreshCursorDisplay__() const;
		void __showMessage__(LPCTSTR msg) const;
	protected:
		const HMENU customSelectSubmenu, customResetSubmenu, customGotoSubmenu;

		int __getCursorPos__() const;
		void PostNcDestroy() override sealed;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override; // enabling/disabling ToolBar buttons
	public:
		CHexaEditor(PVOID param,HMENU customSelectSubmenu=nullptr,HMENU customResetSubmenu=nullptr,HMENU customGotoSubmenu=nullptr);
		~CHexaEditor();

		void SetEditable(bool _editable);
		int ShowAddressBand(bool _show);
		void Reset(CFile *_f,int _minFileSize,int _maxFileSize);
		void SetLogicalBounds(int _minFileSize,int _maxFileSize);
		void SetLogicalSize(int _logicalSize);
		void GetVisiblePart(int &rLogicalBegin,int &rLogicalEnd) const;
		void AddEmphasis(int a,int z);
		void CancelAllEmphases();
		void RepaintData(bool immediately=false) const;
		BOOL PreTranslateMessage(PMSG pMsg) override;
	};

#endif // HEXAEDITOR_H
