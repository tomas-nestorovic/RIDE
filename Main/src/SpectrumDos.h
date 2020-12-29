#ifndef ZXSPECTRUMDOS_H
#define ZXSPECTRUMDOS_H

	#define INI_SPECTRUM	_T("ZXSpectrum")

	#define ZX_PARAMETER_1	_T("Std param 1")
	#define ZX_PARAMETER_2	_T("Std param 2")

	#define ZX_DIR_ROOT		DOS_DIR_ROOT

	#define ZX_BASIC_START_ADDR			0x5ccb

	#define ZX_TAPE_FILE_NAME_LENGTH_MAX	10

	#define ZX_TAPE_FILE_COUNT_MAX			2048
	#define ZX_TAPE_EXTENSION_STD_COUNT		4

	#define ZX_TAPE_EXTENSION_PROGRAM	TUniFileType::PROGRAM
	#define ZX_TAPE_EXTENSION_NUMBERS	TUniFileType::NUMBER_ARRAY
	#define ZX_TAPE_EXTENSION_CHARS		TUniFileType::CHAR_ARRAY
	#define ZX_TAPE_EXTENSION_BYTES		TUniFileType::BLOCK

	#define ZX_TAPE_HEADERLESS_STR		_T("Headerless")

	class CSpectrumBase:public CDos{
	protected:
		class CScreenPreview sealed:CFilePreview{
			friend class CSpectrumBase;

			static void CALLBACK __flash__(HWND hPreview,UINT nMsg,UINT nTimerID,DWORD dwTime);

			HANDLE hFlashTimer;
			bool paperFlash;
			struct{
				BITMAPINFO bmi;
				RGBQUAD colors[16];
				RGBQUAD flashCombinations[128];
				HBITMAP handle;
				PBYTE data;
			} dib;

			void RefreshPreview() override;
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		public:
			static CScreenPreview *pSingleInstance;

			CScreenPreview(const CFileManagerView &rFileManager);
			~CScreenPreview();			
		};

		class CAssemblerPreview:public CFilePreview{
			union{
				BYTE info;
				struct{
					BYTE address:1;
					BYTE machineCode:1;
					BYTE machineCodeChars:1;
					BYTE instruction:1;
					BYTE colorSyntax:1;
					BYTE capitalSyntax:1;
				};
			} features;
			enum TNumberFormat{
				HexaHashtag,// e.g. #3039
				Hexa0x,		// e.g. 0x3039
				HexaH,		// e.g. 3039h
				Decadic		// e.g. 12345
			} numberFormat;
			struct{
				CFile *pfIn;
				WORD orgAddress;
			} constantInput;
		protected:
			TCHAR tmpFileName[MAX_PATH];
			CWebPageView contentView;

			CAssemblerPreview(const CFileManagerView &rFileManager,DWORD resourceId=IDR_SPECTRUM_PREVIEW_ASSEMBLER,LPCTSTR iniSection=_T("ZxZ80"));

			void ParseZ80BinaryFileAndGenerateHtmlFormattedContent(CFile &fIn,WORD orgAddress,CFile &f,bool smallerHeading=false) const;
			void RefreshPreview() override;
			BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override;
		public:
			static CAssemblerPreview *pSingleInstance; // only single file can be previewed at a time

			static CAssemblerPreview *CreateInstance(const CFileManagerView &rFileManager);

			~CAssemblerPreview();

			void ParseZ80BinaryFileAndShowContent(CFile &fIn,WORD orgAddress);
		};

		class CBasicPreview sealed:public CAssemblerPreview{
			const CMainWindow::CDynMenu machineCodeMenu;
			union{
				BYTE info;
				struct{
					BYTE applyColors:1;
					BYTE showNonprintableChars:1;
					BYTE showRemAsMachineCode:1;
					BYTE wrapLines:1;
				};
			} features;
			enum TBinaryAfter0x14{
				DONT_SHOW,
				SHOW_AS_RAW_BYTES,
				SHOW_AS_NUMBER
			} binaryAfter0x14;
			enum TDataAfterBasic{
				DONT_INTERPRET		=ID_NONE,
				SHOW_AS_VARIABLES	=ID_VARIABLE,
				SHOW_AS_MACHINE_CODE=ID_INSTRUCTION
			} dataAfterBasic;

			void __parseBasicFileAndGenerateHtmlFormattedContent__(PCFile file) const;
			void RefreshPreview() override;
			BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override;
		public:
			static CBasicPreview *pSingleInstance; // only single file can be previewed at a time

			CBasicPreview(const CFileManagerView &rFileManager);
			~CBasicPreview();
		};
	public:
		const struct TZxRom sealed{
			enum TStdBlockFlag:BYTE{
				HEADER	=0,
				DATA	=255
			};

			enum TFileType:BYTE{
				PROGRAM		=0,
				NUMBER_ARRAY=1,
				CHAR_ARRAY	=2,
				CODE		=3,
				HEADERLESS	=(BYTE)-1,	// defined foremost due to formal reasons (e.g. Headerless File cannot have a Header with this value)
				FRAGMENT	=(BYTE)-2	// defined foremost due to formal reasons (e.g. Headerless File cannot have a Header with this value)
			};

			#pragma pack(1)
			typedef const struct TNumberInternalForm sealed{
				union{
					struct{
						BYTE exponent;
						TBigEndianDWord mantissa;
					};
					BYTE bytes[5];
				};

				double ToDouble() const;
			} *PCNumberInternalForm;

			static const LPCSTR Keywords[];

			static bool IsKnownFileType(TFileType type);
			static LPCTSTR GetFileTypeName(TFileType type);
			static PTCHAR ZxToAscii(LPCSTR zx,short zxLength,PTCHAR buf,char zxBefore=' ');
			static PTCHAR AsciiToZx(LPCTSTR pc,PCHAR zx,PBYTE pOutZxLength);
			//inline
			static bool IsStdUdgSymbol(BYTE s);
			//inline
			static bool IsPrintable(BYTE s);
			//inline
			static LPCSTR GetKeywordTranscript(BYTE k);

			TZxRom();

			const Utils::CRideFont font;
			mutable class CLineComposerPropGridEditor sealed{
				static HWND WINAPI __create__(PropGrid::PValue value,PropGrid::TSize combinedValue,HWND hParent);
				static void WINAPI __drawValue__(PropGrid::PCustomParam,PropGrid::PCValue value,PropGrid::TSize combinedValue,PDRAWITEMSTRUCT pdis);
				static bool WINAPI __onChanged__(PropGrid::PCustomParam,HWND,PropGrid::PValue);
				static LRESULT CALLBACK __wndProc__(HWND hEditor,UINT msg,WPARAM wParam,LPARAM lParam);

				HWND handle;
				int scrollX;
				struct TCaret sealed{
					enum TMode:char{
						K='K',
						LC='L', // "L" mode alternated with "C" mode
						E='E',
						G='G',
						X='X'
					} mode;
					BYTE position; // logical Position in Buffer
				} caret;
				BYTE lengthMax; // mustn't exceed Buffer's capacity
				BYTE length;
				bool hexaLow; // True <=> ready to modify the lower half-byte in hexa mode
				char paddingChar;
				char buf[255]; // "big enough" to contain any ZX Spectrum line
				bool __addChar__(char c);
			public:
				static PropGrid::PCEditor Define(BYTE nCharsMax,char paddingChar,PropGrid::Custom::TOnValueConfirmed onValueConfirmed,PropGrid::TOnValueChanged onValueChanged);

				LPCSTR GetCurrentZxText() const;
				BYTE GetCurrentZxTextLength() const;
			} lineComposerPropGridEditor;

			WORD PrintAt(HDC dc,LPCSTR zx,short zxLength,RECT r,UINT drawTextFormat,char zxBefore=' ') const;
		} zxRom;
	protected:
		enum TUniFileType:char{ // ZX platform-independent File types ("universal" types) - used during exporting/importing of Files across ZX platforms
			UNKNOWN			='X',
			SUBDIRECTORY	='D',
			PROGRAM			='P',
			CHAR_ARRAY		='C',
			NUMBER_ARRAY	='N',
			BLOCK			='B',
			SCREEN			='$',
			SNAPSHOT_48k	='S',
			SNAPSHOT_128k	='A',
			SEQUENTIAL		='Q',
			PRINT			='#',
			HEADERLESS		='H',
			FRAGMENT		='F'
		};

		#pragma pack(1)
		struct TStdParameters sealed{
			static const TStdParameters Default;

			WORD param1,param2;
		};

		#pragma pack(1)
		struct TSpectrumVerificationParams:public CVerifyVolumeDialog::TParams{
			TStdWinError VerifyAllCharactersPrintable(RCPhysicalAddress chs,LPCTSTR chsName,LPCTSTR valueName,PCHAR zx,BYTE zxLength,char paddingChar) const;
			bool WarnSomeCharactersNonPrintable(LPCTSTR locationName,LPCTSTR valueName,PCHAR zx,BYTE zxLength,char paddingChar) const;

			TSpectrumVerificationParams(CSpectrumBase *dos,const TVerificationFunctions &rvf);
		};

		class CSpectrumBaseFileManagerView:public CFileManagerView{
			const BYTE nameCharsMax;
		protected:
			class CSingleCharExtensionEditor sealed:public CValueEditorBase{
				static bool WINAPI __onChanged__(PVOID file,PropGrid::Enum::UValue newExt);
				static LPCTSTR WINAPI __getDescription__(PVOID file,PropGrid::Enum::UValue extension,PTCHAR buf,short bufCapacity);

				const CSpectrumBaseFileManagerView &rZxFileManager;
				mutable BYTE data;
			public:
				CSingleCharExtensionEditor(const CSpectrumBaseFileManagerView &rZxFileManager);

				PEditorBase Create(PFile file) const;
				void DrawReportModeCell(BYTE extension,LPDRAWITEMSTRUCT pdis,LPCSTR knownExtensions=nullptr) const;
			} singleCharExtEditor;

			class CVarLengthCommandLineEditor sealed:public CValueEditorBase{
				static bool WINAPI __onCmdLineConfirmed__(PVOID file,HWND,PVOID value);
				static bool WINAPI __onFileNameConfirmed__(PVOID file,HWND,PVOID);

				const CSpectrumBaseFileManagerView &rZxFileManager;
				mutable TCHAR bufOldCmd[256];
			public:
				CVarLengthCommandLineEditor(const CSpectrumBaseFileManagerView &rZxFileManager);

				PEditorBase Create(PFile file,PCHAR cmd,BYTE cmdLengthMax,char paddingChar,PropGrid::TOnValueChanged onChanged=__markDirectorySectorAsDirty__) const;
				PEditorBase CreateForFileName(PFile file,BYTE fileNameLengthMax,char paddingChar,PropGrid::TOnValueChanged onChanged=__markDirectorySectorAsDirty__) const;
				void DrawReportModeCell(LPCSTR cmd,BYTE cmdLength,char paddingChar,LPDRAWITEMSTRUCT pdis) const;
			} varLengthCommandLineEditor;

			class CStdTapeHeaderBlockTypeEditor sealed:public CValueEditorBase{
			public:
				enum TDisplayTypes{
					STD_ONLY,
					STD_AND_HEADERLESS,
					STD_AND_HEADERLESS_AND_FRAGMENT
				};
			private:
				static PropGrid::Enum::PCValueList WINAPI __createValues__(PVOID file,WORD &rnValues);
				static LPCTSTR WINAPI __getDescription__(PVOID file,PropGrid::Enum::UValue stdType,PTCHAR,short);

				mutable BYTE data;
				mutable TDisplayTypes types;
			public:				
				static void DrawReportModeCell(BYTE type,LPDRAWITEMSTRUCT pdis);

				PEditorBase Create(PFile file,TZxRom::TFileType type,TDisplayTypes _types,PropGrid::Enum::TOnValueConfirmed onChanged) const;
			} stdTapeHeaderTypeEditor;

			PTCHAR GenerateExportNameAndExtOfNextFileCopy(CDos::PCFile file,bool shellCompliant,PTCHAR pOutBuffer) const override sealed;
		protected:
			CSpectrumBaseFileManagerView(PDos dos,const TZxRom &rZxRom,BYTE supportedDisplayModes,BYTE initialDisplayMode,BYTE nInformation,PCFileInfo informationList,BYTE nameCharsMax,PCDirectoryStructureManagement pDirManagement=nullptr);
		public:
			const TZxRom &zxRom;
		};

		static const RGBQUAD Colors[16];
		static DWORD idHeaderless;

		static void __parseFat32LongName__(PTCHAR buf,RPathString rOutName,RPathString rOutExt,LPCTSTR &rOutZxInfo);
		static int __exportFileInformation__(PTCHAR buf,TUniFileType uniFileType);
		static int __exportFileInformation__(PTCHAR buf,TUniFileType uniFileType,TStdParameters params,DWORD fileLength);
		static int __exportFileInformation__(PTCHAR buf,TUniFileType uniFileType,TStdParameters params,DWORD fileLength,BYTE dataFlag);
		static int __importFileInformation__(LPCTSTR buf,TUniFileType &rUniFileType);
		static int __importFileInformation__(LPCTSTR buf,TUniFileType &rUniFileType,TStdParameters &rParams,DWORD &rFileLength);
		static int __importFileInformation__(LPCTSTR buf,TUniFileType &rUniFileType,TStdParameters &rParams,DWORD &rFileLength,BYTE &rDataFlag);
		static void __informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId);

		TSide sideMap[2]; // 2 = only one- or two-sided floppies are considered to be used with any ZX Spectrum derivate

		CSpectrumBase(PImage image,PCFormat pFormatBoot,TTrackScheme trackAccessScheme,PCProperties properties,UINT nResId,CSpectrumBaseFileManagerView *pFileManager,TGetFileSizeOptions getFileSizeDefaultOption,TSectorStatus unformatFatStatus);
		~CSpectrumBase();
	public:
		CString GetFilePresentationNameAndExt(PCFile file) const override;
		CString GetFileExportNameAndExt(PCFile file,bool shellCompliant) const override;
		DWORD GetAttributes(PCFile file) const override;
		TCmdResult ProcessCommand(WORD cmd) override;
	};




	class CSpectrumDos:public CSpectrumBase{
		bool __isTapeFileManagerShown__() const;
	protected:
		class CTape sealed:private CImageRaw,public CSpectrumBase{ // CImageRaw = the type of Image doesn't matter (not used by Tape)
			friend class CSpectrumDos;
		public:
			#pragma pack(1)
			typedef struct THeader sealed{
				TZxRom::TFileType type; // any type but Headerless
				char name[ZX_TAPE_FILE_NAME_LENGTH_MAX];
				WORD length;
				TStdParameters params;

				void GetNameOrExt(PPathString pOutName,PPathString pOutExt) const;
				TStdWinError SetName(RCPathString newName);
				TUniFileType GetUniFileType() const;
				bool SetFileType(TUniFileType uts);
			} *PHeader;
			typedef const THeader *PCHeader;
		private:
			#pragma pack(1)
			typedef struct TTapeFile sealed{
				enum TType{
					STD_HEADER,	// both Header and Data* fields are valid
					HEADERLESS,	// only Data* fields are valid
					FRAGMENT	// only DataLength and Data (without asterisk) fields are valid
				} type;
				THeader stdHeader;
				BYTE dataBlockFlag; // 255 = block saved using the standard ROM routine, otherwise any other value
				BYTE dataChecksum;
				enum TDataChecksumStatus:BYTE{
					UNDETERMINED,
					CORRECT,
					INCORRECT
				} dataChecksumStatus;
				WORD dataLength;
				BYTE data[6]; // to make the structure 32 Bytes long (so that FILE_LENGTH_MAX is a round multiple of it - HexaEditor's requirement)

				PHeader GetHeader();
				PCHeader GetHeader() const;
			} *PTapeFile,**PPTapeFile;
			typedef const TTapeFile *PCTapeFile;

			class CTapeFileManagerView sealed:public CSpectrumBaseFileManagerView{
				friend class CTape;

				static const TFileInfo InformationList[];

				static bool WINAPI __checksumModified__(PVOID file,int);
				static bool WINAPI __tapeBlockTypeModified__(PVOID file,PropGrid::Enum::UValue _newType);

				CMainWindow::CDockableToolBar toolbar;

				void DrawReportModeCell(PCFileInfo pFileInfo,LPDRAWITEMSTRUCT pdis) const override;
				int CompareFiles(PCFile file1,PCFile file2,BYTE information) const override;
				PEditorBase CreateFileInformationEditor(PFile,BYTE infoId) const override;
				LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
			public:
				CFile f; // physical Tape file (e.g. "C:\myTape.tap")
				PTapeFile files[ZX_TAPE_FILE_COUNT_MAX]; // Files stored on this Tape
				short nFiles; // number of Files stored on this Tape

				CTapeFileManagerView(CTape *tape,const TZxRom &rZxRom,LPCTSTR fileName,bool makeCurrentTab);
				~CTapeFileManagerView();
			} fileManager;

			struct TTapeTraversal sealed:public TDirectoryTraversal{
				const CTapeFileManagerView &rFileManager;
				short fileId;
				TTapeTraversal(const CTapeFileManagerView &rFileManager); // ctor
				bool AdvanceToNextEntry() override;
				void ResetCurrentEntry(BYTE directoryFillerByte) const override;
			};
		
			BOOL DoSave(LPCTSTR,BOOL) override;
		public:
			static CTape *pSingleInstance;
			static const TCHAR Extensions[ZX_TAPE_EXTENSION_STD_COUNT];

			CTape(LPCTSTR fileName,const CSpectrumDos *diskDos,bool makeCurrentTab);
			~CTape();

			void GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override;
			TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus) override;

			// boot
			void FlushToBootSector() const override; // projects information stored in internal FormatBoot back to the Boot Sector (e.g. called automatically by BootView)
			// FAT
			bool GetSectorStatuses(TCylinder,THead,TSector,PCSectorId,PSectorStatus) const override;
			bool ModifyStdSectorStatus(RCPhysicalAddress,TSectorStatus) const override;
			bool GetFileFatPath(PCFile file,CFatPath &rFatPath) const override;
			bool ModifyFileFatPath(PFile file,const CFatPath &rFatPath) const override;
			DWORD GetFreeSpaceInBytes(TStdWinError &rError) const override;
			// file system
			bool GetFileNameOrExt(PCFile file,PPathString pOutName,PPathString pOutExt) const override;
			TStdWinError ChangeFileNameAndExt(PFile file,RCPathString newName,RCPathString newExt,PFile &rRenamedFile) override;
			DWORD GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData,TGetFileSizeOptions option) const override;
			DWORD GetAttributes(PCFile file) const override;
			TStdWinError DeleteFile(PFile file) override;
			std::unique_ptr<TDirectoryTraversal> BeginDirectoryTraversal(PCFile directory) const override;
			CString GetFileExportNameAndExt(PCFile file,bool shellCompliant) const override;
			TStdWinError ImportFile(CFile *fIn,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile) override;
			// other
			TCmdResult ProcessCommand(WORD cmd) override;
			bool UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const override;
			void InitializeEmptyMedium(CFormatDialog::PCParameters) override;
		};

		class CSpectrumFileManagerView:public CSpectrumBaseFileManagerView{
		protected:
			TStdWinError ImportPhysicalFile(LPCTSTR pathAndName,CDos::PFile &rImportedFile,DWORD &rConflictedSiblingResolution) override;

			CSpectrumFileManagerView(PDos dos,const TZxRom &rZxRom,BYTE supportedDisplayModes,BYTE initialDisplayMode,BYTE nInformation,PCFileInfo informationList,BYTE nameCharsMax,PCDirectoryStructureManagement pDirManagement=nullptr);
		};

		CTrackMapView trackMap;

		CSpectrumDos(PImage image,PCFormat pFormatBoot,TTrackScheme trackAccessScheme,PCProperties properties,UINT nResId,CSpectrumBaseFileManagerView *pFileManager,TGetFileSizeOptions getFileSizeDefaultOption,TSectorStatus unformatFatStatus);
		~CSpectrumDos();
	public:
		mutable CRecentFileList mruTapes;

		TCmdResult ProcessCommand(WORD cmd) override;
		bool UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const override;
		bool CanBeShutDown(CFrameWnd* pFrame) const override sealed; // sealed as no need to ever change the functionality
	};

#endif // ZXSPECTRUMDOS_H