#ifndef DOS_H
#define DOS_H

	#define DOS_ERR_CANNOT_FORMAT		_T("Cannot format")
	#define DOS_ERR_CANNOT_UNFORMAT		_T("Cannot unformat")
	#define DOS_ERR_CANNOT_ACCEPT_VALUE	_T("Cannot accept the new value")
	#define DOS_ERR_CYLINDERS_NOT_EMPTY	_T("Not all cylinders are reported empty.")

	#define DOS_MSG_CYLINDERS_UNCHANGED	_T("No cylinders have been modified.")
	#define DOS_MSG_HIT_ESC				_T("Hit Escape to cancel editing.")

	#define DOS_DIR_ROOT	nullptr
	#define DOS_DIR_ROOT_ID	0

	#define VOLUME_LABEL_DEFAULT_ANSI_8CHARS	"Untitled"

	#define FAT_SECTOR_UNMODIFIABLE		_T("Some FAT sectors could not be modified")

	#define MAKE_DOS_ID(char1,char2,char3,char4,char5,char6,char7,char8)\
		( (((((((CDos::TId)char1<<4^char2)<<4^char3)<<4^char4)<<4^char5)<<4^char6)<<4^char7)<<4^char8 )

	#define INI_MISRECOGNITION	_T("misrec")

	class CDos{
		friend class CRideApp;
		friend class CBootView;
		friend class CTrackMapView;
		friend class CFileManagerView;
	public:
		typedef int (WINAPI *TFnCompareNames)(LPCTSTR name1,LPCTSTR name2);
		typedef DWORD TId;
		typedef PVOID PFile;
		typedef LPCVOID PCFile;

		typedef class CPathString sealed{
			short nCharsInBuf;
			TCHAR buf[MAX_PATH];
		public:
			static bool IsValidFat32LongNameChar(WCHAR c);
			static CPathString Unescape(LPCTSTR term);
			static short Unescape(PTCHAR buf,LPCTSTR term);

			CPathString();
			CPathString(TCHAR c,short nRepeats=1);
			CPathString(LPCSTR str);
			CPathString(LPCSTR str,short strLength);

			operator LPCTSTR() const;
			TCHAR operator[](int i) const;
			//TCHAR &operator[](int i);
			//bool operator==(const CPathString &r) const;
			CPathString &operator+=(TCHAR c);
			CPathString &operator+=(const CPathString &r);

			short GetLength() const;
			bool Equals(const CPathString &r,TFnCompareNames comparer) const;
			CString EscapeToString() const;
			CPathString &LowerCase();
			CPathString &TrimRight(TCHAR c);
			CPathString &TrimToLength(short nCharsMax);
			CPathString &ExcludeFat32LongNameInvalidChars();
			CPathString & AFX_CDECL Format(LPCTSTR format,...);
		} *PPathString,&RPathString;
		typedef const CPathString *PCPathString,&RCPathString;

		typedef TStdWinError (*TFnRecognize)(PImage image,PFormat pFormatBoot);
		typedef PDos (*TFnInstantiate)(PImage image,PCFormat pFormatBoot);
		typedef TStdWinError (CDos::*TFnCreateSubdirectory)(LPCTSTR name,DWORD winAttr,PFile &rCreatedSubdir);
		typedef TStdWinError (CDos::*TFnChangeCurrentDirectory)(PFile directory);
		typedef TStdWinError (CDos::*TFnMoveFileToCurrDir)(PFile file,LPCTSTR exportFileNameAndExt,PFile &rMovedFile);

		#pragma pack(1)
		typedef const struct TProperties sealed{
			LPCTSTR name;
			TId id; // DOS unique identifier (see other DOSes to be REALLY unique!)
			BYTE recognitionPriority; // the order in which the DOS gets crack at the disk content (the bigger the number the earlier it sniffes the disk)
			TFnRecognize fnRecognize;
			TFnInstantiate fnInstantiate;
			TMedium::TType supportedMedia; // a set of MediumTypes this DOS supports
			CImage::PCProperties typicalImage; // the most common Image to contain data for this DOS (e.g. *.D80 Image for MDOS)
			// formatting
			BYTE nStdFormats;
			CFormatDialog::PCStdFormat stdFormats;
			TSector nSectorsOnTrackMin,nSectorsOnTrackMax; // range of supported number of Sectors
			TSector nSectorsInTotalMin; // minimal total number of Sectors required
			BYTE nSectorsInClusterMax; // maximum number of Sector in one Cluster (must be power of 2)
			WORD clusterSizeMax; // maximum size of a Cluster (in Bytes)
			BYTE nAllocationTablesMin,nAllocationTablesMax; // range of supported number of allocation tables (FATs)
			WORD nRootDirectoryEntriesMin,nRootDirectoryEntriesMax; // range of supported number of root Directory entries
			TSector firstSectorNumber; // lowest Sector number on each Track
			BYTE sectorFillerByte,directoryFillerByte; // regular Sector and Directory Sector filler Byte
			BYTE dataBeginOffsetInSector,dataEndOffsetInSector; // number of reserved Bytes at the beginning and end of each Sector

			BYTE GetValidGap3ForMedium(TMedium::TType medium) const;
		} *PCProperties;

		struct TFileDateTime:public FILETIME{
			static const TFileDateTime None;

			static TFileDateTime GetCurrent();

			TFileDateTime(const FILETIME &r);

			bool operator==(const FILETIME &r) const;
			bool operator!=(const FILETIME &r) const;
			PTCHAR DateToString(PTCHAR buf) const;
			PTCHAR TimeToString(PTCHAR buf) const;
			bool Edit(bool dateEditingEnabled,bool timeEditingEnabled,const SYSTEMTIME *epoch);
		};

		class CHexaValuePropGridEditor sealed:public CDialog{
			BYTE newValueBuffer[2048];
			CHexaEditor hexaEditor;
			CMemFile f;

			void PreInitDialog() override;

			CHexaValuePropGridEditor(PropGrid::PValue value,PropGrid::TSize valueSize);
		public:
			static void WINAPI DrawValue(PropGrid::PCustomParam,PropGrid::PCValue value,PropGrid::TSize valueSize,PDRAWITEMSTRUCT pdis);
			static bool WINAPI EditValue(PropGrid::PCustomParam,PropGrid::PValue value,PropGrid::TSize valueSize);
			static PropGrid::PCEditor Define(PropGrid::PCustomParam,PropGrid::TSize valueSize,PropGrid::TOnValueChanged onValueChanged);
		};

		typedef struct TDirectoryTraversal{
			const PCFile directory;
			const WORD entrySize;
			TPhysicalAddress chs;
			enum TDirEntryType:char{
				EMPTY	=0,
				FILE	=1, // current Entry is a File
				SUBDIR	=2, // current Entry is a Subdirectory
				CUSTOM	=3,	// current Entry is occupied and only a CDos-derivate knows how to process it (e.g., see long file name entries in MS-DOS); such entries are skipped in all basic CDos routines
				WARNING	=4,	// Directory Sector not found, but may be also another error/warning; continuing to traverse the Directory usually suffices (as virtually in all CDos routines)
				END		=-1 // end of Directory reached, no more DirectoryEntries can be retrieved
			} entryType;
			union{
				PFile entry;
				TStdWinError warning; // it's up to the caller to consider further traversal of the Directory (i.e. interpret this warning as a serious error)
			};

			TDirectoryTraversal(PCFile directory,WORD entrySize); // ctor

			virtual PFile AllocateNewEntry();
			virtual bool AdvanceToNextEntry()=0;
			virtual void ResetCurrentEntry(BYTE directoryFillerByte) const=0;
			PFile GetNextFileOrSubdir();
		} *PDirectoryTraversal;

		class CFatPath sealed{
		public:
			#pragma pack(1)
			typedef const struct TItem sealed{
				DWORD value;
				TPhysicalAddress chs;
			} *PCItem;
			typedef TItem *PItem;

			enum TError:BYTE{
				OK				=0,
				SECTOR			=1,	// FAT Sector not found or read with error, e.g. Data Field CRC error
				VALUE_CYCLE		=2, // cyclic path in FAT
				VALUE_INVALID	=3, // nonsense value in FAT, e.g. out of certain range, e.g. beyond last Cluster number
				VALUE_BAD_SECTOR=5, // value in FAT indicates a bad File data Sector; this value usually prelimiary terminates the path
				FILE			=16	// invalid File entry to find path of, e.g. an empty Directory entry
			} error;
		private:
			const DWORD nItemsMax;
			TItem *const buffer;
			DWORD nItems;
			TItem *pLastItem;
		public:
			static LPCTSTR GetErrorDesc(TError error);

			CFatPath(DWORD nItemsMax); // for Dummy object which has no Buffer and just counts the Items (allocation units)
			CFatPath(const CDos *dos,PCFile file); // ctor for exporting a File on Image
			CFatPath(const CDos *dos,DWORD fileSize); // ctor for importing a File to Image
			CFatPath(const CDos *dos,RCPhysicalAddress chs); // ctor for editing a Sector (e.g. Boot Sector)
			~CFatPath();

			bool AddItem(PCItem pItem);
			LPCTSTR GetItems(PCItem &rBuffer,DWORD &rnItems) const;
			LPCTSTR GetItems(PItem &rBuffer,DWORD &rnItems) const;
			DWORD GetNumberOfItems() const;
			LPCTSTR GetErrorDesc() const;
		};

		class CFileReaderWriter:public CFile,public CHexaEditor::IContentAdviser{
			const CDos *const dos;
			const BYTE dataBeginOffsetInSector,dataEndOffsetInSector;
			#if _MFC_VER>=0x0A00
			LONGLONG fileSize;
			LONGLONG position;
			#else
			LONG fileSize;
			LONG position;
			#endif
		protected:
			int recordLength;
		public:
			const CFatPath fatPath;

			CFileReaderWriter(const CDos *dos,PCFile file,bool wholeSectors=false); // ctor to read/edit an existing File on the Image
			CFileReaderWriter(const CDos *dos,RCPhysicalAddress chs); // ctor to read/write particular Sector in the Image (e.g. Boot Sector)
			~CFileReaderWriter();

			// CFile methods
			#if _MFC_VER>=0x0A00
			ULONGLONG GetLength() const override;
			void SetLength(ULONGLONG dwNewLen) override;
			ULONGLONG GetPosition() const override;
			ULONGLONG Seek(LONGLONG lOff,UINT nFrom) override;
			#else
			DWORD GetLength() const override;
			void SetLength(DWORD dwNewLen) override;
			DWORD GetPosition() const override;
			LONG Seek(LONG lOff,UINT nFrom) override;
			#endif
			UINT Read(LPVOID lpBuf,UINT nCount) override;
			void Write(LPCVOID lpBuf,UINT nCount) override;

			// CHexaEditor::IContentAdviser methods
			void GetRecordInfo(int logPos,PINT pOutRecordStartLogPos,PINT pOutRecordLength,bool *pOutDataReady) override;
			int LogicalPositionToRow(int logPos,BYTE nBytesInRow) override;
			int RowToLogicalPosition(int row,BYTE nBytesInRow) override;
			LPCTSTR GetRecordLabel(int logPos,PTCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const override;
		};

		struct TBigEndianWord sealed{
		private:
			BYTE highByte,lowByte;
		public:
			WORD operator=(WORD newValue);
			operator WORD() const;
		};

		struct TBigEndianDWord sealed{
		private:
			TBigEndianWord highWord,lowWord;
		public:
			DWORD operator=(DWORD newValue);
			operator DWORD() const;
		};

		enum TGetFileSizeOptions:BYTE{
			OfficialDataLength,
			SizeOnDisk
		};

		const PImage image;
		const PCProperties properties;
	private:
		static UINT AFX_CDECL __checkCylindersAreEmpty_thread__(PVOID _pCancelableAction);
		static UINT AFX_CDECL __fillEmptySectors_thread__(PVOID _pCancelableAction);
		static UINT AFX_CDECL __fillEmptyLastSectors_thread__(PVOID _pCancelableAction);
		static UINT AFX_CDECL __fillEmptyDirEntries_thread__(PVOID _pCancelableAction);
		static UINT AFX_CDECL __formatTracks_thread__(PVOID _pCancelableAction);
		static UINT AFX_CDECL __unformatTracks_thread__(PVOID _pCancelableAction);

		TStdWinError __isTrackEmpty__(TCylinder cyl,THead head,TSector nSectors,PCSectorId sectors) const;
		TStdWinError __areStdCylindersEmpty__(TTrack nCylinders,PCylinder bufCylinders) const;
	protected:
		class CFilePreview:public CFrameWnd{
			const CWnd *const pView;
			const PFile directory;
		protected:
			const LPCTSTR iniSection;
			std::unique_ptr<CDos::TDirectoryTraversal> pdt;

			CFilePreview(const CWnd *pView,LPCTSTR iniSection,const CFileManagerView &rFileManager,WORD initialWindowWidth,WORD initialWindowHeight,DWORD resourceId);

			void __showNextFile__();
			void __showPreviousFile__();
			virtual void RefreshPreview()=0;
			BOOL PreCreateWindow(CREATESTRUCT &cs) override;
			BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override;
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		public:
			const CFileManagerView &rFileManager;
		};

		static const TSide StdSidesMap[];

		static void __warnOnEnteringCriticalConfiguration__(bool b);
		static BYTE __xorChecksum__(PCBYTE buffer,WORD nBytes);
		static BYTE __xorChecksum__(LPCSTR buffer,WORD nChars);

		const TFnCompareNames fnCompareNames;
		const TTrackScheme trackAccessScheme; // single Scheme to access Tracks in Image
		PFile currentDir;
		bool generateShellCompliantExportNames;
		TGetFileSizeOptions getFileSizeDefaultOption;
		
		CDos(PImage _image,PCFormat _pFormatBoot,TTrackScheme trackAccessScheme,PCProperties _properties,TFnCompareNames _fnCompareNames,PCSide _sideMap,UINT nResId,CFileManagerView * _pFileManager,TGetFileSizeOptions _getFileSizeDefaultOption);

		int __getProfileInt__(LPCTSTR entryName,int defaultValue) const;
		void __writeProfileInt__(LPCTSTR entryName,int value) const;
		bool __getProfileBool__(LPCTSTR entryName,bool defaultValue) const;
		void __writeProfileBool__(LPCTSTR entryName,bool value) const;
		TCylinder __getLastOccupiedStdCylinder__() const;
		TStdWinError __showDialogAndFormatStdCylinders__(CFormatDialog &rd,PCylinder bufCylinders,PHead bufHeads);
		TStdWinError __formatStdCylinders__(const CFormatDialog &rd,PCylinder bufCylinders,PHead bufHeads);
		TStdWinError __formatTracks__(TTrack nTracks,PCCylinder cylinders,PCHead heads,TSector nSectors,PSectorId bufferId,PCWORD bufferLength,const CFormatDialog::TParameters &rParams,bool showReport);
		TStdWinError __unformatStdCylinders__(CUnformatDialog &rd,PCylinder bufCylinders,PHead bufHeads);
		TStdWinError __unformatTracks__(TTrack nTracks,PCCylinder cylinders,PCHead heads);
		bool __addStdTracksToFatAsEmpty__(TTrack nTracks,PCCylinder cylinders,PCHead heads);
		bool __removeStdTracksFromFat__(TTrack nTracks,PCCylinder cylinders,PCHead heads);
		bool __fillEmptySpace__(CFillEmptySpaceDialog &rd);
		bool __verifyVolume__(CVerifyVolumeDialog &rd);
		LPCTSTR __exportFileData__(PCFile file,CFile *fOut,DWORD nMaxDataBytesToExport) const;
		TStdWinError __importData__(CFile *fIn,DWORD fileSize,bool skipBadSectors,CFatPath &rFatPath) const;
		TStdWinError __importFileData__(CFile *fIn,PFile fDesc,RCPathString fileName,RCPathString fileExt,DWORD fileSize,bool skipBadSectors,PFile &rFile,CFatPath &rFatPath);
		PFile __findFile__(PCFile directory,RCPathString fileName,RCPathString fileExt,PCFile ignoreThisFile) const;
		TStdWinError __shiftFileContent__(const CFatPath &rFatPath,char nBytesShift) const;
	public:
		typedef enum TSectorStatus:COLORREF{ // each value must be bigger than the biggest possible Sector length (typically 16384)
			SYSTEM		=0xff40ff, // e.g. reserved for root Directory
			UNAVAILABLE	=0x707070, // Sectors that are not included in FAT (e.g. beyond the FAT, or FAT Sector error)
			SKIPPED		=0xb8b8b8, // e.g. deleted Files in TR-DOS
			BAD			=0x0000ff,
			OCCUPIED	=0xffcc99,
			RESERVED	=0xffff99, // e.g. zero-length File in MDOS, or File with error during importing
			EMPTY		=0xffffff, // reported as unallocated
			UNKNOWN		=0x00ffff  // any Sector whose ID doesn't match any ID from the standard format, e.g. ID={2,1,0,3} for an MDOS Sector
		} *PSectorStatus;

		class CHexaPreview sealed:public CFilePreview{
			CMemFile fEmpty;
			std::unique_ptr<CFileReaderWriter> pFileRW;

			void RefreshPreview() override;
		public:
			static CHexaPreview *pSingleInstance; // only single file can be previewed at a time

			class CHexaEditorView sealed:public CHexaEditor{
				LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
			public:
				CHexaEditorView(PCDos dos,CHexaPreview *pHexaPreview);
			} hexaEditor;

			CHexaPreview(const CFileManagerView &rFileManager);
			~CHexaPreview();
		};

		class CRecognition sealed{
			friend class CMainWindow; // as that's where the CAutomaticRecognitionOrderDialog is defined
			BYTE nDoses;
			PCProperties order[(BYTE)-1];
		public:
			CRecognition();

			void __saveToProfile__() const;
			BYTE __addDosByPriorityDescending__(PCProperties props);
			BYTE __getOrderIndex__(PCProperties props) const;
			POSITION __getFirstRecognizedDosPosition__() const;
			PCProperties __getNextRecognizedDos__(POSITION &pos) const;
			PCProperties __perform__(PImage image,PFormat pOutFormatBoot) const;
		};

		static CPtrList known; // list of known DOSes (registered in CRideApp::InitInstance)

		static PDos GetFocused();
		static void __errorCannotDoCommand__(TStdWinError cause);

		CFileManagerView *const pFileManager;
		const PCSide sideMap; // how Heads map to Side numbers (Head->Side)
		const CMainWindow::TDynMenu menu;
		TFormat formatBoot; // information on Medium Format retrieved from Boot; this information has ALWAYS priority if handling the disk; changes in this structure must be projected back to Boot Sector using FlushToBootSector (e.g. called automatically by BootView)

		virtual ~CDos();

		// boot
		virtual void FlushToBootSector() const=0; // projects information stored in internal FormatBoot back to the Boot Sector (e.g. called automatically by BootView)
		// FAT
		virtual bool GetSectorStatuses(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PSectorStatus buffer) const=0;
		virtual bool ModifyStdSectorStatus(RCPhysicalAddress chs,TSectorStatus status)=0;
		virtual bool GetFileFatPath(PCFile file,CFatPath &rFatPath) const=0;
		virtual bool ModifyFileFatPath(PFile file,const CFatPath &rFatPath)=0;
		virtual DWORD GetFreeSpaceInBytes(TStdWinError &rError) const;
		virtual TCylinder GetFirstCylinderWithEmptySector() const;
		TStdWinError GetFirstEmptyHealthySector(bool skipBadSectors,TPhysicalAddress &rOutChs) const;
		// file system
		virtual bool GetFileNameOrExt(PCFile file,PPathString pOutName,PPathString pOutExt) const=0;
		virtual CString GetFilePresentationNameAndExt(PCFile file) const;
		bool HasFileNameAndExt(PCFile file,RCPathString fileName,RCPathString fileExt) const;
		virtual TStdWinError ChangeFileNameAndExt(PFile file,RCPathString newName,RCPathString newExt,PFile &rRenamedFile)=0;
		virtual DWORD GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData,TGetFileSizeOptions option) const=0;
		DWORD GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData) const;
		DWORD GetFileSize(PCFile file) const;
		DWORD GetFileOfficialSize(PCFile file) const;
		DWORD GetFileOccupiedSize(PCFile file) const;
		DWORD GetFileSizeOnDisk(PCFile file) const;
		virtual void GetFileTimeStamps(PCFile file,LPFILETIME pCreated,LPFILETIME pLastRead,LPFILETIME pLastWritten) const;
		bool GetFileCreatedTimeStamp(PCFile file,FILETIME &rCreated) const;
		bool GetFileLastReadTimeStamp(PCFile file,FILETIME &rCreated) const;
		bool GetFileLastWrittenTimeStamp(PCFile file,FILETIME &rCreated) const;
		virtual void SetFileTimeStamps(PFile file,const FILETIME *pCreated,const FILETIME *pLastRead,const FILETIME *pLastWritten);
		virtual DWORD GetAttributes(PCFile file) const=0;
		bool IsDirectory(PCFile file) const;
		virtual TStdWinError DeleteFile(PFile file)=0;
		virtual std::unique_ptr<TDirectoryTraversal> BeginDirectoryTraversal(PCFile directory) const=0;
		std::unique_ptr<TDirectoryTraversal> BeginDirectoryTraversal() const;
		virtual DWORD GetDirectoryUid(PCFile dir) const;
		void MarkDirectorySectorAsDirty(PCFile file) const;
		DWORD GetCountOfItemsInCurrentDir(TStdWinError &rError) const;
		virtual CString GetFileExportNameAndExt(PCFile file,bool shellCompliant) const;
		virtual DWORD ExportFile(PCFile file,CFile *fOut,DWORD nBytesToExportMax,LPCTSTR *pOutError) const;
		virtual TStdWinError ImportFile(CFile *fIn,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile)=0;
		PFile FindFileInCurrentDir(RCPathString fileName,RCPathString fileExt,PCFile ignoreThisFile) const;
		// other
		TSector GetListOfStdSectors(TCylinder cyl,THead head,PSectorId bufferId) const;
		virtual TStdWinError CreateUserInterface(HWND hTdi);
		virtual enum TCmdResult:BYTE{
					REFUSED	=0,	// not carried out (e.g. because cannot)
					DONE_REDRAW =1,	// carried out OK and caller is asked to redraw the currently active View
					DONE		=2	// carried out OK and called doesn't have to do anything
				} ProcessCommand(WORD cmd);
		virtual bool UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const;
		virtual void InitializeEmptyMedium(CFormatDialog::PCParameters params)=0;
		virtual bool ValidateFormatChangeAndReportProblem(bool reformatting,PCFormat f) const;
		void ShowFileProcessingError(PCFile file,LPCTSTR cause) const;
		void ShowFileProcessingError(PCFile file,TStdWinError cause) const;
		virtual bool CanBeShutDown(CFrameWnd* pFrame) const;
	};

#endif // DOS_H
