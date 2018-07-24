#ifndef ZXSPECTRUMDOS_H
#define ZXSPECTRUMDOS_H

	#define ZX_PARAMETER_1	_T("Std param 1")
	#define ZX_PARAMETER_2	_T("Std param 2")

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

			static PTCHAR ZxToAscii(LPCSTR zx,BYTE zxLength,PTCHAR buf);
			static PTCHAR AsciiToZx(LPCTSTR pc,PCHAR zx,PBYTE pOutZxLength);
			static bool IsStdUdgSymbol(BYTE c);

			TZxRom();

			const CRideFont font;

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
			const TZxRom &zxRom;

			mutable class CSingleCharExtensionEditor sealed{
				static bool WINAPI __onChanged__(PVOID file,CPropGridCtrl::TEnum::UValue newExt);
				static LPCTSTR WINAPI __getDescription__(PVOID file,CPropGridCtrl::TEnum::UValue extension,PTCHAR buf,short bufCapacity);

				BYTE data;
			public:
				PEditorBase Create(PFile file);
			} singleCharExtEditor;

			mutable class CVarLengthFileNameEditor sealed{
				static bool WINAPI __help__(PVOID,PVOID,short);
				static HWND WINAPI __create__(PVOID,short,HWND hParent);
				static bool WINAPI __onChanged__(PVOID file,HWND,PVOID,short);
				static LRESULT CALLBACK __wndProc__(HWND hEditor,UINT msg,WPARAM wParam,LPARAM lParam);

				WNDPROC wndProc0; // Editor's original window procedure (from PropertyGrid)
				struct TCursor sealed{
					enum TMode:char{
						K='K',
						L='L',
						E='E',
						G='G'
					} mode;
					BYTE position; // logical Position in Buffer
				} cursor;
				BYTE lengthMax; // mustn't exceed Buffer's capacity
				BYTE length;
				char buf[32]; // "big enough" buffer to contain the name (and extension!!) of any Spectrum-derivate's file
				void __addChar__(char c);
			public:
				PEditorBase Create(PFile file,BYTE _lengthMax);
			} varLengthFileNameEditor;

			mutable class CStdParamEditor sealed{
			public:
				PEditorBase Create(PFile file,PWORD pwParam,CPropGridCtrl::TInteger::TOnValueConfirmed fnOnConfirmed);
			} stdParamEditor;

			PTCHAR GenerateExportNameAndExtOfNextFileCopy(CDos::PCFile file,bool shellCompliant,PTCHAR pOutBuffer) const override sealed;
		public:
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
				WORD dataLength;
				BYTE data;

				PHeader GetHeader();
				PCHeader GetHeader() const;
			} *PTapeFile,**PPTapeFile;
			typedef const TTapeFile *PCTapeFile;

			typedef WORD TTapeFileId;

			static const TCHAR Extensions[ZX_TAPE_EXTENSION_STD_COUNT];

			class CTapeFileManagerView sealed:public CSpectrumFileManagerView{
				friend class CTape;

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
				TTapeFileId nFiles; // number of Files stored on this Tape

				CTapeFileManagerView(CTape *tape,const TZxRom &rZxRom,LPCTSTR fileName);
				~CTapeFileManagerView();
			} fileManager;

			struct TTapeTraversal sealed:public TDirectoryTraversal{
				const CTapeFileManagerView &rFileManager;
				TTapeFileId fileId;
				TTapeTraversal(const CTapeFileManagerView &rFileManager); // ctor
				bool AdvanceToNextEntry() override;
				void ResetCurrentEntry(BYTE directoryFillerByte) const override;
			};
		
			BOOL DoSave(LPCTSTR,BOOL) override;
		public:
			static bool WINAPI __markAsDirty__(PVOID,int);

			CTape(LPCTSTR fileName,const CSpectrumDos *diskDos);
			~CTape();

			PSectorData GetSectorData(RCPhysicalAddress chs,BYTE nSectorsToSkip,bool recoverFromError,PWORD sectorLength,TFdcStatus *pFdcStatus) override;

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
			DWORD GetFileDataSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData) const override;
			DWORD GetAttributes(PCFile file) const override;
			TStdWinError DeleteFile(PFile file) override;
			PDirectoryTraversal BeginDirectoryTraversal() const override;
			PTCHAR GetFileExportNameAndExt(PCFile file,bool shellCompliant,PTCHAR buf) const override;
			TStdWinError ImportFile(CFile *fIn,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile) override;
			// other
			TCmdResult ProcessCommand(WORD cmd) override;
			bool UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const override;
			void InitializeEmptyMedium(CFormatDialog::PCParameters) override;
		} *pSingleTape;

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

			void RefreshPreview() override;
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

		CSpectrumDos(PImage image,PCFormat pFormatBoot,TTrackScheme trackAccessScheme,PCProperties properties,UINT nResId,CSpectrumFileManagerView *pFileManager);
		~CSpectrumDos();

		PTCHAR GetFileExportNameAndExt(PCFile file,bool shellCompliant,PTCHAR buf) const;
		DWORD GetAttributes(PCFile file) const override;
		TCmdResult ProcessCommand(WORD cmd) override;
		bool UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const override;
		bool CanBeShutDown(CFrameWnd* pFrame) const override sealed; // sealed as no need to ever change the functionality
	};

#endif // ZXSPECTRUMDOS_H