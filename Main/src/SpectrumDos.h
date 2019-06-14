#ifndef ZXSPECTRUMDOS_H
#define ZXSPECTRUMDOS_H

	#define ZX_PARAMETER_1	_T("Std param 1")
	#define ZX_PARAMETER_2	_T("Std param 2")

	#define ZX_DIR_ROOT		DOS_DIR_ROOT

	#define ZX_TAPE_FILE_NAME_LENGTH_MAX	10

	#define ZX_TAPE_FILE_COUNT_MAX			2048
	#define ZX_TAPE_EXTENSION_STD_COUNT		4

	class CSpectrumDos:public CDos{
		bool __isTapeFileManagerShown__() const;
	protected:
		const struct TZxRom sealed{
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

			static PTCHAR ZxToAscii(LPCSTR zx,BYTE zxLength,PTCHAR buf);
			static PTCHAR AsciiToZx(LPCTSTR pc,PCHAR zx,PBYTE pOutZxLength);
			inline
			static bool IsStdUdgSymbol(BYTE s);
			//inline
			static LPCSTR GetKeywordTranscript(BYTE k);

			TZxRom();

			const CRideFont font;
			mutable class CLineComposerPropGridEditor sealed{
				static HWND WINAPI __create__(CPropGridCtrl::PValue value,CPropGridCtrl::TValueSize combinedValue,HWND hParent);
				static void WINAPI __drawValue__(CPropGridCtrl::PCustomParam,CPropGridCtrl::PCValue value,CPropGridCtrl::TValueSize combinedValue,PDRAWITEMSTRUCT pdis);
				static bool WINAPI __onChanged__(CPropGridCtrl::PCustomParam,HWND,CPropGridCtrl::PValue,CPropGridCtrl::TValueSize);
				static LRESULT CALLBACK __wndProc__(HWND hEditor,UINT msg,WPARAM wParam,LPARAM lParam);

				HWND handle;
				struct TCursor sealed{
					enum TMode:char{
						K='K',
						LC='L', // "L" mode alternated with "C" mode
						E='E',
						G='G'
					} mode;
					BYTE position; // logical Position in Buffer
				} cursor;
				BYTE lengthMax; // mustn't exceed Buffer's capacity
				BYTE length;
				char paddingChar;
				char buf[255]; // "big enough" to contain any ZX Spectrum line
				void __addChar__(char c);
			public:
				CPropGridCtrl::PCEditor Create(CPropGridCtrl::TCustom::TOnValueConfirmed onValueConfirmed) const;
				LPCSTR GetCurrentZxText() const;
				BYTE GetCurrentZxTextLength() const;
			} lineComposerPropGridEditor;

			void PrintAt(HDC dc,LPCTSTR buf,RECT r,UINT drawTextFormat) const;
		} zxRom;

		enum TUniFileType:char{ // ZX platform-independent File types ("universal" types) - used during exporting/importing of Files across ZX platforms
			UNKNOWN			='X',
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

		union UStdParameters{
			#pragma pack(1)
			struct{ WORD param1,param2; };
			DWORD dw;

			UStdParameters();
		};

		class CSpectrumFileManagerView:public CFileManagerView{
		protected:
			mutable class CSingleCharExtensionEditor sealed{
				static bool WINAPI __onChanged__(PVOID file,CPropGridCtrl::TEnum::UValue newExt);
				static LPCTSTR WINAPI __getDescription__(PVOID file,CPropGridCtrl::TEnum::UValue extension,PTCHAR buf,short bufCapacity);

				BYTE data;
			public:
				PEditorBase Create(PFile file);
			} singleCharExtEditor;

			mutable class CVarLengthFileNameEditor sealed{
				static bool WINAPI __onChanged__(PVOID file,HWND,PVOID,short);

				TCHAR bufOldName[64]; // 64 = should accommodate filename of any ZX Spectrum-derived platform
			public:
				PEditorBase Create(PFile file,BYTE lengthMax,char paddingChar);
			} varLengthFileNameEditor;

			mutable class CStdParamEditor sealed{
			public:
				PEditorBase Create(PFile file,PWORD pwParam,CPropGridCtrl::TInteger::TOnValueConfirmed fnOnConfirmed);
			} stdParamEditor;

			PTCHAR GenerateExportNameAndExtOfNextFileCopy(CDos::PCFile file,bool shellCompliant,PTCHAR pOutBuffer) const override sealed;
			TStdWinError ImportPhysicalFile(LPCTSTR pathAndName,CDos::PFile &rImportedFile,TConflictResolution &rConflictedSiblingResolution) override;
		public:
			const TZxRom &zxRom;

			CSpectrumFileManagerView(PDos dos,const TZxRom &rZxRom,BYTE supportedDisplayModes,BYTE initialDisplayMode,BYTE nInformation,PCFileInfo informationList);
		};

		class CTape sealed:private CImageRaw,public CDos{ // CImageRaw = the type of Image doesn't matter (not used by Tape)
			friend class CSpectrumDos;

			#pragma pack(1)
			typedef struct THeader sealed{
				TZxRom::TFileType type; // any type but Headerless
				char name[ZX_TAPE_FILE_NAME_LENGTH_MAX];
				WORD length;
				UStdParameters params;
			} *PHeader;
			typedef const THeader *PCHeader;

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

			static const TCHAR Extensions[ZX_TAPE_EXTENSION_STD_COUNT];

			class CTapeFileManagerView sealed:public CSpectrumFileManagerView{
				friend class CTape;

				static bool WINAPI __checksumModified__(PVOID file,int);

				mutable class CStdHeaderTypeEditor sealed{
				public:
					enum TDisplayTypes{
						STD_ONLY,
						STD_AND_HEADERLESS,
						STD_AND_HEADERLESS_AND_FRAGMENT
					};
				private:
					static bool WINAPI __onChanged__(PVOID file,CPropGridCtrl::TEnum::UValue _newType);
					static CPropGridCtrl::TEnum::PCValueList WINAPI __createValues__(PVOID file,WORD &rnValues);

					BYTE data;
					TDisplayTypes types;
				public:
					static LPCTSTR WINAPI __getDescription__(PVOID file,CPropGridCtrl::TEnum::UValue stdType,PTCHAR,short);
					
					PEditorBase Create(PFile file,TZxRom::TFileType type,TDisplayTypes _types);
				} stdHeaderTypeEditor;

				CMainWindow::CDockableToolBar toolbar;

				void DrawFileInfo(LPDRAWITEMSTRUCT pdis,const int *tabs) const override;
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

			CTape(LPCTSTR fileName,const CSpectrumDos *diskDos,bool makeCurrentTab);
			~CTape();

			void GetTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override;
			TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus) override;

			// boot
			void FlushToBootSector() const override; // projects information stored in internal FormatBoot back to the Boot Sector (e.g. called automatically by BootView)
			// FAT
			bool GetSectorStatuses(TCylinder,THead,TSector,PCSectorId,PSectorStatus) const override;
			bool ModifyTrackInFat(TCylinder,THead,PSectorStatus) override;
			bool GetFileFatPath(PCFile file,CFatPath &rFatPath) const override;
			DWORD GetFreeSpaceInBytes(TStdWinError &rError) const override;
			// file system
			void GetFileNameAndExt(PCFile file,PTCHAR bufName,PTCHAR bufExt) const override;
			TStdWinError ChangeFileNameAndExt(PFile file,LPCTSTR newName,LPCTSTR newExt,PFile &rRenamedFile) override;
			DWORD GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData,TGetFileSizeOptions option) const override;
			DWORD GetAttributes(PCFile file) const override;
			TStdWinError DeleteFile(PFile file) override;
			PDirectoryTraversal BeginDirectoryTraversal(PCFile directory) const override;
			PTCHAR GetFileExportNameAndExt(PCFile file,bool shellCompliant,PTCHAR buf) const override;
			TStdWinError ImportFile(CFile *fIn,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile) override;
			// other
			TCmdResult ProcessCommand(WORD cmd) override;
			bool UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const override;
			void InitializeEmptyMedium(CFormatDialog::PCParameters) override;
		};

		class CScreenPreview sealed:CFilePreview{
			friend class CSpectrumDos;

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

		class CBasicPreview sealed:public CFilePreview{
			TCHAR tmpFileName[MAX_PATH];
			CWebPageView listingView;
			bool applyColors,showNonprintableChars;
			enum TBinaryAfter0x14{
				DONT_SHOW,
				SHOW_AS_RAW_BYTES,
				SHOW_AS_NUMBER
			} binaryAfter0x14;

			void __parseBasicFileAndGenerateHtmlFormattedContent__(PCFile file) const;
			void RefreshPreview() override;
			BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override;
		public:
			static CBasicPreview *pSingleInstance; // only single file can be previewed at a time

			CBasicPreview(const CFileManagerView &rFileManager);
			~CBasicPreview();
		};

		static const RGBQUAD Colors[16];

		static void __parseFat32LongName__(PTCHAR buf,LPCTSTR &rOutName,BYTE nameLengthMax,LPCTSTR &rOutExt,BYTE extLengthMax,LPCTSTR &rOutZxInfo);
		static int __exportFileInformation__(PTCHAR buf,TUniFileType uniFileType,UStdParameters params,DWORD fileLength);
		static int __importFileInformation__(LPCTSTR buf,TUniFileType &rUniFileType,UStdParameters &rParams,DWORD &rFileLength);
		static void __informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId);

		CTrackMapView trackMap;
		TSide sideMap[2]; // 2 = only one- or two-sided floppies are considered to be used with any ZX Spectrum derivate

		CSpectrumDos(PImage image,PCFormat pFormatBoot,TTrackScheme trackAccessScheme,PCProperties properties,UINT nResId,CSpectrumFileManagerView *pFileManager,TGetFileSizeOptions _getFileSizeDefaultOption);
		~CSpectrumDos();

		PTCHAR GetFileExportNameAndExt(PCFile file,bool shellCompliant,PTCHAR buf) const;
		DWORD GetAttributes(PCFile file) const override;
		TCmdResult ProcessCommand(WORD cmd) override;
		bool UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const override;
		bool CanBeShutDown(CFrameWnd* pFrame) const override sealed; // sealed as no need to ever change the functionality
	};

#endif // ZXSPECTRUMDOS_H