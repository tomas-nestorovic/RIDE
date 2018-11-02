#ifndef DOS_H
#define DOS_H

	#define DOS_ERR_CANNOT_FORMAT		_T("Cannot format")
	#define DOS_ERR_CANNOT_UNFORMAT		_T("Cannot unformat")
	#define DOS_ERR_CANNOT_ACCEPT_VALUE	_T("Cannot accept the new value")
	#define DOS_ERR_CYLINDERS_NOT_EMPTY	_T("Not all cylinders are reported empty.")

	#define DOS_MSG_CYLINDERS_UNCHANGED	_T("No cylinders have been modified.")
	#define DOS_MSG_HIT_ESC				_T("Hit Escape to cancel editing.")

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
		typedef DWORD TId;
		typedef PVOID PFile;
		typedef LPCVOID PCFile;

		typedef TStdWinError (*TFnRecognize)(PImage image,PFormat pFormatBoot);
		typedef PDos (*TFnInstantiate)(PImage image,PCFormat pFormatBoot);
		typedef PFile (CDos::*TFnGetCurrentDirectory)() const;
		typedef DWORD (CDos::*TFnGetCurrentDirectoryId)() const;
		typedef TStdWinError (CDos::*TFnCreateSubdirectory)(LPCTSTR name,DWORD winAttr,PFile &rCreatedSubdir);
		typedef TStdWinError (CDos::*TFnChangeCurrentDirectory)(PFile directory);
		typedef TStdWinError (CDos::*TFnMoveFileToCurrDir)(PFile file,LPCTSTR fileNameAndExt,PFile &rMovedFile);

		#pragma pack(1)
		typedef const struct TProperties sealed{
			LPCTSTR name;
			TId id; // DOS unique identifier (see other DOSes to be REALLY unique!)
			BYTE recognitionPriority; // the order in which the DOS gets crack at the disk content (the bigger the number the earlier it sniffes the disk)
			TFnRecognize fnRecognize;
			TFnInstantiate fnInstantiate;
			TMedium::TType supportedMedia; // a set of MediumTypes this DOS supports
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

		typedef struct TDirectoryTraversal{
			const WORD entrySize;
			const WORD nameCharsMax;
			TPhysicalAddress chs;
			enum TDirEntryType:BYTE{
				EMPTY	=0,
				FILE	=1, // current Entry is a File
				SUBDIR	=2, // current Entry is a Subdirectory
				CUSTOM	=3,	// current Entry is occupied and only a CDos-derivate knows how to process it (e.g., see long file name entries in MS-DOS); such entries are skipped in all basic CDos routines
				WARNING	=4	// Directory Sector not found, but may be also another error/warning; continuing to traverse the Directory usually suffices (as virtually in all CDos routines)
			} entryType;
			union{
				PFile entry;
				TStdWinError warning; // it's up to the caller to consider further traversal of the Directory (i.e. interpret this warning as a serious error)
			};

			TDirectoryTraversal(WORD entrySize,WORD nameCharsMax); // ctor

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

			CFatPath(const CDos *dos,PCFile file); // ctor for exporting a File on Image
			CFatPath(const CDos *dos,DWORD fileSize); // ctor for importing a File to Image
			CFatPath(const CDos *dos,RCPhysicalAddress chs); // ctor for editing a Sector (e.g. Boot Sector)
			~CFatPath();

			bool AddItem(PCItem pItem);
			LPCTSTR GetItems(PCItem &rBuffer,DWORD &rnItems) const;
			DWORD GetNumberOfItems() const;
			LPCTSTR GetErrorDesc() const;
		};

		class CFileReaderWriter sealed:public CFile{
			const CDos *const dos;
			const LONG fileSize;
			LONG position;
		public:
			const CFatPath fatPath;

			CFileReaderWriter(const CDos *dos,PCFile file); // ctor to read/edit an existing File on the Image
			CFileReaderWriter(const CDos *dos,RCPhysicalAddress chs); // ctor to read/write particular Sector in the Image (e.g. Boot Sector)
			~CFileReaderWriter();

			DWORD GetLength() const override;
			DWORD GetPosition() const override;
			LONG Seek(LONG lOff,UINT nFrom) override;
			UINT Read(LPVOID lpBuf,UINT nCount) override;
			void Write(LPCVOID lpBuf,UINT nCount) override;
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

		const PImage image;
		const PCProperties properties;
	private:
		static UINT AFX_CDECL __checkCylindersAreEmpty_thread__(PVOID _pCancelableAction);
		static UINT AFX_CDECL __fillEmptySpace_thread__(PVOID _pCancelableAction);
		static UINT AFX_CDECL __formatTracks_thread__(PVOID _pCancelableAction);
		static UINT AFX_CDECL __unformatTracks_thread__(PVOID _pCancelableAction);

		TStdWinError __isTrackEmpty__(TCylinder cyl,THead head,TSector nSectors,PCSectorId sectors) const;
		TStdWinError __areStdCylindersEmpty__(TTrack nCylinders,PCylinder bufCylinders) const;
	protected:
		class CFilePreview:public CFrameWnd{
			const CWnd *const pView;

		protected:
			const LPCTSTR iniSection;
			PDirectoryTraversal pdt;

			CFilePreview(const CWnd *pView,LPCTSTR iniSection,const CFileManagerView &rFileManager,WORD initialWindowWidth,WORD initialWindowHeight,DWORD resourceId);
			~CFilePreview();

			void __showNextFile__();
			void __showPreviousFile__();
			virtual void RefreshPreview()=0;
			BOOL PreCreateWindow(CREATESTRUCT &cs) override;
			BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override;
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		public:
			const CFileManagerView &rFileManager;
		};

		typedef int (WINAPI *TFnCompareNames)(LPCTSTR name1,LPCTSTR name2);

		static const TSide StdSidesMap[];

		static bool __isValidCharInFat32LongName__(WCHAR c);
		static void WINAPI __updateView__(CPropGridCtrl::PCustomParam);

		const TFnCompareNames fnCompareNames;
		const TTrackScheme trackAccessScheme; // single Scheme to access Tracks in Image
		bool generateShellCompliantExportNames;
		
		CDos(PImage _image,PCFormat _pFormatBoot,TTrackScheme trackAccessScheme,PCProperties _properties,TFnCompareNames _fnCompareNames,PCSide _sideMap,UINT nResId,const CFileManagerView * _pFileManager);

		int __getProfileInt__(LPCTSTR entryName,int defaultValue) const;
		void __writeProfileInt__(LPCTSTR entryName,int value) const;
		bool __getProfileBool__(LPCTSTR entryName,bool defaultValue) const;
		void __writeProfileBool__(LPCTSTR entryName,bool value) const;
		TSector __getListOfStdSectors__(TCylinder cyl,THead head,PSectorId bufferId) const;
		TCylinder __getLastOccupiedStdCylinder__() const;
		TStdWinError __showDialogAndFormatStdCylinders__(CFormatDialog &rd,PCylinder bufCylinders,PHead bufHeads);
		TStdWinError __formatStdCylinders__(const CFormatDialog &rd,PCylinder bufCylinders,PHead bufHeads);
		TStdWinError __formatTracks__(TTrack nTracks,PCCylinder cylinders,PCHead heads,TSector nSectors,PSectorId bufferId,PCWORD bufferLength,const CFormatDialog::TParameters &rParams,bool showReport);
		TStdWinError __unformatStdCylinders__(CUnformatDialog &rd,PCylinder bufCylinders,PHead bufHeads);
		TStdWinError __unformatTracks__(TTrack nTracks,PCCylinder cylinders,PCHead heads);
		bool __addStdTracksToFatAsEmpty__(TTrack nTracks,PCCylinder cylinders,PCHead heads);
		bool __removeStdTracksFromFat__(TTrack nTracks,PCCylinder cylinders,PCHead heads);
		DWORD __getFileSize__(PCFile file) const;
		bool __fillEmptySpace__(CFillEmptySpaceDialog &rd);
		LPCTSTR __exportFileData__(PCFile file,CFile *fOut,DWORD nMaxDataBytesToExport) const;
		TStdWinError __importFileData__(CFile *fIn,PFile fDesc,LPCTSTR fileName,LPCTSTR fileExt,DWORD fileSize,PFile &rFile,CFatPath &rFatPath);
		void __markDirectorySectorAsDirty__(LPCVOID dirEntry) const;
		PFile __findFile__(LPCTSTR fileName,LPCTSTR fileExt,PCFile ignoreThisFile) const;
		TStdWinError __shiftFileContent__(const CFatPath &rFatPath,char nBytesShift) const;
		void __showFileProcessingError__(PCFile file,LPCTSTR cause) const;
		void __showFileProcessingError__(PCFile file,TStdWinError cause) const;
	public:
		typedef enum TSectorStatus:COLORREF{ // each value must be bigger than the biggest possible Sector length (typically 16384)
			SYSTEM		=0xff40ff, // e.g. reserved for root Directory
			UNAVAILABLE	=0x707070, // Sectors that are not included in FAT, e.g. beyond the FAT
			SKIPPED		=0xb8b8b8, // e.g. deleted Files in TR-DOS
			BAD			=0x0000ff,
			OCCUPIED	=0xffcc99,
			RESERVED	=0xffff99, // e.g. zero-length File in MDOS, or File with error during importing
			EMPTY		=0xffffff, // reported as unallocated
			UNKNOWN		=0x00ffff  // any Sector whose ID doesn't match any ID from the standard format, e.g. ID={2,1,0,3} for an MDOS Sector
		} *PSectorStatus;

		class CHexaPreview sealed:public CFilePreview{
			CMemFile fEmpty;
			CFileReaderWriter *pFileRW;

			void RefreshPreview() override;
		public:
			static CHexaPreview *pSingleInstance; // only single file can be previewed at a time

			class CHexaEditorView sealed:public CHexaEditor{
				LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
			public:
				CHexaEditorView();
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

		static PDos __getFocused__();
		static void __errorCannotDoCommand__(TStdWinError cause);

		const CFileManagerView *const pFileManager;
		const PCSide sideMap; // how Heads map to Side numbers (Head->Side)
		const CMainWindow::TDynMenu menu;
		TFormat formatBoot; // information on Medium Format retrieved from Boot; this information has ALWAYS priority if handling the disk; changes in this structure must be projected back to Boot Sector using FlushToBootSector (e.g. called automatically by BootView)

		virtual ~CDos();

		// boot
		virtual void FlushToBootSector() const=0; // projects information stored in internal FormatBoot back to the Boot Sector (e.g. called automatically by BootView)
		// FAT
		virtual bool GetSectorStatuses(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PSectorStatus buffer) const=0;
		virtual bool ModifyTrackInFat(TCylinder cyl,THead head,PSectorStatus statuses)=0;
		virtual bool GetFileFatPath(PCFile file,CFatPath &rFatPath) const=0;
		virtual DWORD GetFreeSpaceInBytes(TStdWinError &rError) const;
		virtual TCylinder GetFirstCylinderWithEmptySector() const;
		// file system
		virtual void GetFileNameAndExt(PCFile file,PTCHAR bufName,PTCHAR bufExt) const=0;
		PTCHAR GetFileNameWithAppendedExt(PCFile file,PTCHAR bufNameExt) const;
		bool HasFileNameAndExt(PCFile file,LPCTSTR fileName,LPCTSTR fileExt) const;
		virtual TStdWinError ChangeFileNameAndExt(PFile file,LPCTSTR newName,LPCTSTR newExt,PFile &rRenamedFile)=0;
		virtual DWORD GetFileDataSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData) const=0;
		DWORD GetFileDataSize(PCFile file) const;
		virtual DWORD GetFileSizeOnDisk(PCFile file) const;
		virtual DWORD GetAttributes(PCFile file) const=0;
		bool IsDirectory(PCFile file) const;
		virtual TStdWinError DeleteFile(PFile file)=0;
		virtual PDirectoryTraversal BeginDirectoryTraversal() const=0;
		void EndDirectoryTraversal(PDirectoryTraversal pdt) const;
		DWORD GetCountOfItemsInCurrentDir(TStdWinError &rError) const;
		virtual PTCHAR GetFileExportNameAndExt(PCFile file,bool shellCompliant,PTCHAR buf) const;
		virtual DWORD ExportFile(PCFile file,CFile *fOut,DWORD nBytesToExportMax,LPCTSTR *pOutError) const;
		virtual TStdWinError ImportFile(CFile *fIn,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile)=0;
		// other
		virtual TStdWinError CreateUserInterface(HWND hTdi);
		virtual enum TCmdResult:BYTE{
					REFUSED	=0,	// not carried out (e.g. because cannot)
					DONE_REDRAW =1,	// carried out OK and caller is asked to redraw the currently active View
					DONE		=2	// carried out OK and called doesn't have to do anything
				} ProcessCommand(WORD cmd);
		virtual bool UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const;
		virtual void InitializeEmptyMedium(CFormatDialog::PCParameters params)=0;
		virtual bool ValidateFormatChangeAndReportProblem(bool reformatting,PCFormat f) const;
		virtual bool CanBeShutDown(CFrameWnd* pFrame) const;
	};

#endif // DOS_H
