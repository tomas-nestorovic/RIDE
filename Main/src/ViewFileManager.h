#ifndef FILEMANAGERVIEW_H
#define FILEMANAGERVIEW_H

	#define FILE_MANAGER_COLOR_EXECUTABLE	0xff0000
	#define FILE_MANAGER_TAB_LABEL			_T("File manager")

	#define FILE_MANAGER_ERROR_RENAMING	_T("Cannot rename")

	class CFileManagerView; // forward
	typedef CFileManagerView *PFileManagerView;
	typedef const CFileManagerView &RCFileManagerView;




	class CFileManagerView:public CListView{
		friend class CMainWindow;
		DECLARE_MESSAGE_MAP()
	public:
		#pragma pack(1)
		typedef const struct TFileInfo sealed{
			LPCTSTR informationName;
			BYTE aligning;
			WORD columnWidthDefault;
		} *PCFileInfo;
		typedef CPtrList TFileList;

		enum TDisplayMode:BYTE{ // options must be in same order as LVS_{ICON,REPORT,SMALLICON,LIST}
			BIG_ICONS	=1,
			REPORT		=2,
			SMALL_ICONS	=4,
			LIST		=8
		};

		enum TConflictResolution:BYTE{
			UNDETERMINED,
			MERGE,
			SKIP
		};
	protected:
		struct TDirectoryStructureManagement sealed{
			CDos::TFnGetCurrentDirectory fnGetCurrentDir;
			CDos::TFnGetCurrentDirectoryId fnGetCurrentDirId;
			CDos::TFnCreateSubdirectory fnCreateSubdir;
			CDos::TFnChangeCurrentDirectory fnChangeCurrentDir;
			CDos::TFnMoveFileToCurrDir fnMoveFileToCurrDir;
		};
	private:
		class COleVirtualFileDataSource sealed:public COleDataSource{
			const PFileManagerView fileManager;
			const DROPEFFECT preferredDropEffect;
			TFileList listOfFiles;

			int __addFileToExport__(PTCHAR relativeDir,CDos::PFile file,LPFILEDESCRIPTOR lpfd);
			BOOL OnRenderData(LPFORMATETC lpFormatEtc,LPSTGMEDIUM lpStgMedium) override;
			BOOL OnRenderGlobalData(LPFORMATETC lpFormatEtc,HGLOBAL *phGlobal) override;
			BOOL OnRenderFileData(LPFORMATETC lpFormatEtc,CFile *pFile) override;
			BOOL OnSetData(LPFORMATETC lpFormatEtc,LPSTGMEDIUM lpStgMedium,BOOL bRelease) override;
		public:
			const CDos::PCFile sourceDir;
			bool deleteFromDiskWhenMoved;

			COleVirtualFileDataSource(PFileManagerView _fileManager,DROPEFFECT _preferredDropEffect);
			~COleVirtualFileDataSource();

			CDos::PFile __getFile__(int id) const;
			bool __isInList__(CDos::PCFile file) const;
		};

		class CNameConflictResolutionDialog sealed:public ::TUtils::CCommandDialog{
			const LPCTSTR conflictedName, conflictedNameType, captionForReplaceButton, captionForSkipButton;
			TCHAR information[MAX_PATH+100];

			void PreInitDialog() override;
			void DoDataExchange(CDataExchange *pDX) override;
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam);
		public:
			int useForAllSubsequentConflicts;

			CNameConflictResolutionDialog(LPCTSTR _conflictedName,LPCTSTR _conflictedNameType,LPCTSTR _captionForReplaceButton,LPCTSTR _captionForSkipButton);
		};

		static void __informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId);
		static LRESULT WINAPI __editLabel_wndProc__(HWND hEdit,UINT msg,WPARAM wParam,LPARAM lParam);
		static UINT AFX_CDECL __selectionPropertyStatistics_thread__(PVOID _pCancelableAction);

		const BYTE supportedDisplayModes; // see TDisplayMode enumeration
		const BYTE reportModeRowHeightAdjustment; // in 1/10 of a point
		const BYTE nameColumnId;
		const BYTE nInformation;
		const PCFileInfo informationList;
		const COleVirtualFileDataSource *ownedDataSource;
		union{
			int scrollY;
			int dropTargetFileId;
		};
		COleDropTarget dropTarget;
		WORD nativelyLastFile; // index of a file that is natively the last one in current Directory
		CMapPtrToWord nativeOrderOfFiles; // map of native order of Files as they are discovered in current Directory (i.e. without Ordering)

		void __updateSummaryInStatusBar__() const;
		WORD __getNativeOrderOfFile__(CDos::PCFile file) const;
		void __order__() const;
		void __addFileToTheEndOfList__(CDos::PCFile file);
		void __deleteFiles__(TFileList &rFileList);
		void __restoreFileSelection__();
		void __switchToDirectory__(CDos::PFile directory) const;
		TStdWinError __switchToDirectory__(PTCHAR path) const;
		TStdWinError __skipNameConflict__(DWORD newFileSize,LPCTSTR newFileName,CDos::PFile conflict,TConflictResolution &rConflictedSiblingResolution) const;
		TStdWinError __moveFile__(int &i,LPFILEDESCRIPTOR files,int nFiles,CDos::PFile &rMovedFile,TConflictResolution &rConflictedSiblingResolution);
		TStdWinError __importPhysicalFile__(LPCTSTR pathAndName,CDos::PFile &rImportedFile,TConflictResolution &rConflictedSiblingResolution);
		TStdWinError __importVirtualFile__(int &i,LPCTSTR pathAndName,LPFILEDESCRIPTOR files,int nFiles,COleDataObject *pDataObject,CDos::PFile &rImportedFile,TConflictResolution &rConflictedSiblingResolution);
		CDos::PFile __getDirectoryUnderCursor__(CPoint &rPt) const;
		afx_msg int OnCreate(LPCREATESTRUCT lpcs);
		afx_msg int OnMouseActivate(CWnd *topParent,UINT nHitTest,UINT message);
		afx_msg void OnChar(UINT nChar,UINT nRepCnt,UINT nFlags);
		afx_msg void MeasureItem(LPMEASUREITEMSTRUCT pmis);
		afx_msg void __changeDisplayMode__(UINT id);
			afx_msg void __changeDisplayMode_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __imageWritableAndFileSelected_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __editNameOfSelectedFile__();
		afx_msg void __onDblClick__(NMHDR *pNMHDR,LRESULT *pResult);
		afx_msg void __onEndLabelEdit__(NMHDR *pNMHDR,LRESULT *pResult);
		afx_msg void __compareFiles__();
		afx_msg void __selectAllFilesInCurrentDir__();
		afx_msg void __unselectAllFilesInCurrentDir__();
		afx_msg void __invertSelectionInCurrentDir__();
			afx_msg void __fileSelected_updateUI__(CCmdUI *pCmdUI) const;
		afx_msg void __deleteSelectedFilesUponConfirmation__();
		afx_msg void __onBeginDrag__(NMHDR *pNMHDR,LRESULT *pResult);
		afx_msg void __copyFilesToClipboard__();
		afx_msg void __cutFilesToClipboard__();
		afx_msg void __pasteFilesFromClipboard__();
			afx_msg void __pasteFiles_updateUI__(CCmdUI *pCmdUI);
		afx_msg void __onColumnClick__(NMHDR *pNMHDR,LRESULT *pResult);
		afx_msg void __createSubdirectory__();
			afx_msg void __createSubdirectory_updateUI__(CCmdUI *pCmdUI) const;
		afx_msg void __showSelectionProperties__() const;
		afx_msg void OnDestroy();
	public:
		typedef class CEditorBase sealed{
			static LRESULT CALLBACK __wndProc__(HWND hEditor,UINT msg,WPARAM wParam,LPARAM lParam);
			static LRESULT CALLBACK __ellipsisButton_wndProc__(HWND hEllipsisButton,UINT msg,WPARAM wParam,LPARAM lParam);

			RCFileManagerView parent;
			WNDPROC wndProc0;		// Editor's original window procedure
			WNDPROC ellipsisButtonWndProc0;
		public:
			static const CEditorBase *pSingleShown; // used also to store WNDPROC, see __editFirstInformationOfSelectedFile__; is "const" as no one has the right to change it from outside

			const CDos::PFile file;	// File that is being edited
			const HWND hEditor;		// File information Editor (WC_EDIT derivate,...)
			const HWND hEllipsisButton;

			CEditorBase(CDos::PFile file,PVOID value,short valueSize,CPropGridCtrl::PCEditor editor,RCFileManagerView parent);
			~CEditorBase();

			void Repaint() const;
		} *PEditorBase;

		PEditorBase __createStdEditor__(CDos::PFile file,PVOID value,short valueSize,CPropGridCtrl::PCEditor editor) const;
		PEditorBase __createStdEditorWithEllipsis__(CDos::PFile file,CPropGridCtrl::TOnEllipsisButtonClicked buttonAction) const;
		PEditorBase __createStdEditorForByteValue__(CDos::PFile file,PBYTE pByte,CPropGridCtrl::TInteger::TOnValueConfirmed fnOnValueConfirmed) const;
		PEditorBase __createStdEditorForWordValue__(CDos::PFile file,PWORD pWord,CPropGridCtrl::TInteger::TOnValueConfirmed fnOnValueConfirmed) const;
	protected:
		class CFileComparisonDialog sealed:public CDialog{
			CMemFile fEmpty;
			POINT padding;
			int buttonWidth,buttonHeight,addressColumnWidth;
			HWND hCompareButton;
			
			class COleComparisonDropTarget sealed:public COleDropTarget{
			public:
				HWND hLabel,hEllipsisButton;
				CFile *f;
				DWORD size;
				class CHexaComparison sealed:public CHexaEditor{
					LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
				public:
					CFileComparisonDialog &rDialog;
					CHexaComparison(CFileComparisonDialog &_rDialog); // ctor
				} hexaComparison;

				COleComparisonDropTarget(CFileComparisonDialog &rDialog); // ctor
				~COleComparisonDropTarget(); // dtor

				void __init__(CWnd *pLabel,CWnd *pButton);
				void __chooseAndOpenPhysicalFile__();
				void __openPhysicalFile__(LPCTSTR fileName);
				void __openFile__(CFile *fTmp,LPCTSTR fileName);
				DROPEFFECT OnDragEnter(CWnd *,COleDataObject *pDataObject,DWORD dwKeyState,CPoint point) override;
				DROPEFFECT OnDragOver(CWnd *pWnd,COleDataObject *pDataObject,DWORD dwKeyState,CPoint point) override;
				BOOL OnDrop(CWnd *,COleDataObject *pDataObject,DROPEFFECT dropEffect,CPoint point) override;
			} file1,file2;

			void OnOK() override;
			void OnCancel() override;
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		public:
			static CFileComparisonDialog *pSingleInstance;

			CFileComparisonDialog();
		};

		static int CALLBACK __orderFiles__(LPARAM file1,LPARAM file2,LPARAM orderingInfo);
		static bool WINAPI __markDirectorySectorAsDirty__(PVOID dirEntry,int);

		BYTE displayMode; // see the TDisplayMode enumeration
		BYTE ordering;
		TFileList selectedFiles; // used only for restoring selection when the FileManager is switched back - otherwise the content is empty!
		int focusedFile;

		CFileManagerView(PDos _dos,BYTE _supportedDisplayModes,BYTE _initialDisplayMode,const CFont &rFont,BYTE reportModeRowHeightAdjustment,BYTE _nInformation,PCFileInfo _informationList,BYTE _nameColumnId,const TDirectoryStructureManagement *_pDirectoryStructureManagement);
		~CFileManagerView();

		void __editFileInformation__(CDos::PFile file,BYTE editableInformationSearchDirection) const;
		void __editNextFileInformation__(CDos::PFile file) const;
		void __editPreviousFileInformation__(CDos::PFile file) const;
		void __addToTheEndAndSelectFile__(CDos::PFile file);
		void __replaceFileDisplay__(CDos::PCFile fileToHide,CDos::PFile fileToShow);
		//BOOL PreCreateWindow(CREATESTRUCT &cs) override;
		//BOOL OnCmdMsg(UINT nID,int nCode,PVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override;
		void OnUpdate(CView *pSender,LPARAM iconType,CObject *icons) override;
		void DrawItem(LPDRAWITEMSTRUCT lpdi) override;
		DROPEFFECT OnDragEnter(COleDataObject *pDataObject,DWORD dwKeyState,CPoint point) override;
		DROPEFFECT OnDragOver(COleDataObject *pDataObject,DWORD dwKeyState,CPoint point) override;
		BOOL OnDrop(COleDataObject *pDataObject,DROPEFFECT dropEffect,CPoint point) override;
		void PostNcDestroy() override;
		afx_msg void __refreshDisplay__();
		virtual void DrawFileInfo(LPDRAWITEMSTRUCT pdis,const int *tabs) const=0;
		virtual int CompareFiles(CDos::PCFile file1,CDos::PCFile file2,BYTE information) const=0;
		virtual PEditorBase CreateFileInformationEditor(CDos::PFile file,BYTE infoId) const=0;
		virtual PTCHAR GenerateExportNameAndExtOfNextFileCopy(CDos::PCFile file,bool shellCompliant,PTCHAR pOutBuffer) const=0;
	public:
		static void __bytesToHigherUnits__(DWORD bytes,float &rHigherUnit,LPCTSTR &rHigherUnitName);

		const CFont &rFont;
		const TDirectoryStructureManagement *const pDirectoryStructureManagement;
		const CMainWindow::CTdiView::TTab tab;

		POSITION GetFirstSelectedFilePosition() const;
		CDos::PFile GetNextSelectedFile(POSITION &pos) const;
		POSITION GetLastSelectedFilePosition() const;
		CDos::PFile GetPreviousSelectedFile(POSITION &pos) const;
		DWORD GetCountOfSelectedFiles() const;
		TStdWinError ImportFileAndResolveConflicts(CFile *f,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,CDos::PFile &rImportedFile,TConflictResolution &rConflictedSiblingResolution);
	};

#endif // FILEMANAGERVIEW_H
