#ifndef HEXAEDITOR_H
#define HEXAEDITOR_H

	#define HEXAEDITOR_BASE_CLASS	WC_EDIT

	class CRideFont; // forward

	class CHexaEditor:public CEdit{
		class COleBinaryDataSource sealed:public COleDataSource{
			CFile *const f;
			const DWORD dataBegin,dataLength;
		public:
			COleBinaryDataSource(CFile *_f,DWORD _dataBegin,DWORD dataEnd);

			//BOOL OnRenderFileData(LPFORMATETC lpFormatEtc,CFile *pFile) override;
		};

		typedef struct TEmphasis sealed{ // aka, "highlightings" in listing of underlying File (e.g. when comparing two Files using FileManager)
			static const TEmphasis Terminator;

			DWORD a,z;
			TEmphasis *pNext;
		} *PEmphasis;

		static void __informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId);

		const PVOID param;
		const CRideFont font;
		BYTE nBytesInRow;
		DWORD nRowsInTotal;
		DWORD nRowsDisplayed;
		DWORD nRowsOnPage;
		HWND hPreviouslyFocusedWnd;
		struct TCursor sealed{
			bool ascii; // True <=> Cursor is in the Ascii listing section
			bool hexaLow; // True <=> ready to modify the lower half-byte in hexa mode
			int position; // current logical position in underlying File
			int selectionA,selectionZ; // beginning (including) and end (exluding)
			TCursor(int position); // ctor
			void __detectNewSelection__();
		} cursor;
		PEmphasis emphases; // must be ordered ascending by A (and thus automatically also by Z)

		CFile *f;
		DWORD minFileSize,maxFileSize;
		DWORD logicalSize; // zero by default
		BYTE addrLength; // Address format length (see ADDRESS_FORMAT); modified in ShowAddresses
		bool editable;

		int __scrollToRow__(int row);
		void __refreshVertically__();
		void __setNormalPrinting__(HDC dc);
		void __refreshCursorDisplay__() const;
		void __showMessage__(LPCTSTR msg) const;
	protected:
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		CHexaEditor(PVOID _param);

		void SetEditable(bool _editable);
		int ShowAddressBand(bool _show);
		void Reset(CFile *_f,DWORD _minFileSize,DWORD _maxFileSize);
		void SetLogicalSize(DWORD _logicalSize);
		void GetVisiblePart(DWORD &rLogicalBegin,DWORD &rLogicalEnd) const;
		void AddEmphasis(DWORD a,DWORD z);
		void CancelAllEmphases();
	};

#endif // HEXAEDITOR_H
