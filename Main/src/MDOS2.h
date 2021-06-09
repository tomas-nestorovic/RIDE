#ifndef MDOS_H
#define MDOS_H

	#define MDOS2_VOLUME_LABEL_LENGTH_MAX	10
	#define MDOS2_FILE_NAME_LENGTH_MAX	10

	#define MDOS2_SECTOR_LENGTH_STD		512
	#define MDOS2_SECTOR_LENGTH_STD_CODE	TFormat::LENGTHCODE_512

	#define MDOS2_FAT_ERROR				WORD(-1)
	#define MDOS2_FAT_SECTOR_SYSTEM		0xddd
	#define MDOS2_FAT_SECTOR_BAD		0xdff
	#define MDOS2_FAT_SECTOR_EMPTY		0x000
	#define MDOS2_FAT_SECTOR_UNAVAILABLE MDOS2_FAT_SECTOR_SYSTEM
	#define MDOS2_FAT_SECTOR_RESERVED	0xc00
	#define MDOS2_FAT_SECTOR_EOF		0xe00 /* end-of-file mark in FAT12 */

	#define MDOS2_IS_LAST_SECTOR(fatValue)\
				(fatValue>=MDOS2_FAT_SECTOR_EOF && fatValue<=MDOS2_FAT_SECTOR_EOF+MDOS2_SECTOR_LENGTH_STD-1)

	#define MDOS2_DIR_LOGSECTOR_FIRST	6 /* first logical Sector of root Directory */

	#define MDOS2_TRACK_SECTORS_MIN		MDOS2_DIR_LOGSECTOR_FIRST /* FAT must be on a single Track */
	#define MDOS2_TRACK_SECTORS_MAX		10

	#define MDOS2_DATA_LOGSECTOR_FIRST	14 /* first logical Sector dedicated for data */

	#define MDOS2_RUNP	_T("run.P")
	#define MDOS2_RUNP_NOT_MODIFIED	_T("No changes to the \"") MDOS2_RUNP _T("\" file made (if previously any).")

	class CMDOS2 sealed:public CSpectrumDos{
		#pragma pack(1)
		typedef struct TBootSector sealed{
			static const TPhysicalAddress CHS;

			static UINT AFX_CDECL Verification_thread(PVOID pCancelableAction);

			#pragma pack(1)
			union UReserved1{
				#pragma pack(1)
				struct TGKFileManager sealed{
					static HIMAGELIST __getListOfDefaultIcons__(HDC dc);
					static BYTE __addIconToList__(HIMAGELIST icons,PCBYTE iconZxData,HDC dc);
					static void __drawIcon__(PCBYTE iconZxData,HDC dcdst,BYTE zoomFactor);
					static void __addToPropertyGrid__(HWND hPropGrid,TBootSector *boot);
					static bool WINAPI __warnOnEditingAdvancedValue__(PVOID,int);
					static PCBYTE __getIconDataFromBoot__(const TBootSector *boot);
					static void __getTextFromBoot__(const TBootSector *boot,PTCHAR bufT);

					inline
					static BYTE __pg_getPropertyHeight__();
					static void WINAPI __pg_drawProperty__(PropGrid::PCustomParam,LPCVOID bootSector,short,PDRAWITEMSTRUCT pdis);
					static bool WINAPI __pg_editProperty__(PropGrid::PCustomParam,PVOID bootSector,short);
					static bool WINAPI __pg_createNew__(PropGrid::PCustomParam param,int hyperlinkId,LPCTSTR hyperlinkName);
					static void WINAPI DrawIconBytes(PropGrid::PCustomParam,PropGrid::PCValue value,PropGrid::TSize valueSize,PDRAWITEMSTRUCT pdis);
					static bool WINAPI EditIconBytes(PropGrid::PCustomParam,PropGrid::PValue value,PropGrid::TSize valueSize);
					
					WORD id;	// "FM" identification text
					BYTE y,x;	// [Y,X] = [row,column] = upper left corner (in Pixels)
					BYTE w,h;	// [W,H] = dimensions (in Pixels)
					BYTE color;	// bits correspond to Spectrum's standard attributes (255 = transparent window)
					BYTE dy,dx;	// [DY,DX] = [row,column] = text offset from window's upper left corner
					WORD aText;	// address of text in memory
					WORD aWnd;	// address of window
					WORD zero;
					WORD aIcon;	// address of icon in memory
					WORD aVRam;	// address of window in Spectrum's VideoRAM
				} gkfm;
				BYTE undefined[128];
			} reserved1;
			#pragma pack(1)
			typedef const struct TDiskAndDriveInfo sealed{
				struct TFlags sealed{
					BYTE reserved2:2;
					BYTE driveB:1;
					BYTE driveD40:1;
					BYTE doubleSided:1;
					BYTE fortyCylDiskInD80:1;
					BYTE stepSpeed:2;
				};

				static void WINAPI __pg_drawProperty__(PVOID,LPCVOID diskAndDriveInfo,short,PDRAWITEMSTRUCT pdis);
				static bool WINAPI __pg_editProperty__(PVOID,PVOID diskAndDriveInfo,short);

				// drive info
				BYTE driveConnected:1;
				BYTE driveError:1;
				// disk info
				TFlags diskFlags;
				BYTE nCylinders;
				BYTE nSectors;
				// drive info (continued)
				BYTE driveLastSeekedCylinder;
				TFlags driveFlags;
				BYTE driveCylinders;
				BYTE driveSectorsPerTrack;
				DWORD unused;
			} *PCDiskAndDriveInfo;
			TDiskAndDriveInfo drives[4];
			TDiskAndDriveInfo current;
			DWORD reserved2;
			char label[MDOS2_VOLUME_LABEL_LENGTH_MAX];
			WORD diskID; // randomly chosen after formatting the disk and constant since
			DWORD sdos; // the "SDOS" text that identifies MDOS floppies :-)
			union UReserved3{
				BYTE undefined[304];
			} reserved3;
		} *PBootSector;
		typedef const TBootSector *PCBootSector;

		typedef WORD TLogSector;

		#pragma pack(1)
		typedef struct TDirectoryEntry sealed{
			enum TExtension:BYTE{
				PROGRAM		='P',
				CHAR_ARRAY	='C',
				NUMBER_ARRAY='N',
				BLOCK		='B',
				SNAPSHOT	='S',
				SEQUENTIAL	='Q',
				EMPTY_ENTRY	=0xe5
			};
			enum TAttribute:BYTE{
				HIDDEN		=128,
				SYSTEM		=64,
				PROTECTED	=32,
				ARCHIVE		=16,
				READABLE	=8,
				WRITEABLE	=4,
				EXECUTABLE	=2,
				DELETABLE	=1
			};

			BYTE extension;
			char name[MDOS2_FILE_NAME_LENGTH_MAX];
			WORD lengthLow; // lower Word of File size
			TStdParameters params;
			TLogSector firstLogicalSector;
			BYTE reserved1;
			BYTE attributes;
			BYTE lengthHigh; // upper Word of File size
			BYTE reserved2[10];

			static const char KnownExtensions[];

			static UINT AFX_CDECL Verification_thread(PVOID pCancelableAction);

			DWORD GetLength() const;
			void SetLength(DWORD fileLength);
			bool __editAttributesViaDialog__();
			PTCHAR __attributes2text__(PTCHAR buf,bool inclDashes) const;
		} *PDirectoryEntry;
		typedef const TDirectoryEntry *PCDirectoryEntry;

		typedef CMDOS2 *PMDOS2;

		enum TVersion:TSide{
			AUTODETECT	=0,
			VERSION_1	=2,
			VERSION_2	=1
		} version;

		struct TMdos2DirectoryTraversal sealed:public TDirectoryTraversal{
		private:
			const CMDOS2 *const mdos2;
			TLogSector dirSector;
			BYTE nRemainingEntriesInSector;
		public:
			TMdos2DirectoryTraversal(const CMDOS2 *_mdos2); // ctor
			bool AdvanceToNextEntry() override;
			void ResetCurrentEntry(BYTE directoryFillerByte) const override;
			void __reinitToFirstEntry__();
			bool __existsNextEntry__();
		};

		class CMdos2BootView sealed:public CBootView{
			void GetCommonBootParameters(RCommonBootParameters rParam,PSectorData boot) override;
			void AddCustomBootParameters(HWND hPropGrid,HANDLE hGeometry,HANDLE hVolume,const TCommonBootParameters &rParam,PSectorData boot) override;
		public:
			CMdos2BootView(PMDOS2 mdos);
		} boot;

		class CMdos2FileManagerView sealed:public CSpectrumFileManagerView{
			static const TFileInfo InformationList[];

			static bool WINAPI __editFileAttributes__(PFile file,PVOID,short);

			void DrawReportModeCell(PCFileInfo pFileInfo,LPDRAWITEMSTRUCT pdis) const override;
			int CompareFiles(PCFile file1,PCFile file2,BYTE information) const override;
			PEditorBase CreateFileInformationEditor(PFile file,BYTE infoId) const override;
			TStdWinError ImportPhysicalFile(LPCTSTR pathAndName,CDos::PFile &rImportedFile,DWORD &rConflictedSiblingResolution) override;
			void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override; // GK's File Manager icons
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		public:
			CMdos2FileManagerView(PMDOS2 mdos);
		} fileManager;

		static TStdWinError __recognizeDisk__(PImage image,PFormat pFormatBoot);
		static void __informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId);
		static UINT AFX_CDECL FatVerification_thread(PVOID pCancelableAction);

		TDirectoryEntry deDefault;

		void __recognizeVersion__();
		TLogSector __fyzlog__(RCPhysicalAddress chs) const;
		TPhysicalAddress __logfyz__(TLogSector ls) const;
		PSectorData __getHealthyLogicalSectorData__(TLogSector logSector) const;
		void __markLogicalSectorAsDirty__(TLogSector logSector) const;
		WORD __getLogicalSectorFatItem__(TLogSector logSector) const;
		bool __setLogicalSectorFatItem__(TLogSector logSector,WORD value12) const;
		void InitializeEmptyMedium(CFormatDialog::PCParameters params) override;
	public:
		static const TProperties Properties;

		CMDOS2(PImage image,PCFormat pFormatBoot);

		// boot
		void FlushToBootSector() const override; // projects information stored in internal FormatBoot back to the Boot Sector (e.g. called automatically by BootView)
		// FAT
		bool GetSectorStatuses(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PSectorStatus buffer) const override;
		bool ModifyStdSectorStatus(RCPhysicalAddress chs,TSectorStatus status) const override;
		bool GetFileFatPath(PCFile file,CFatPath &rFatPath) const override;
		bool ModifyFileFatPath(PFile file,const CFatPath &rFatPath) const override;
		// file system
		bool GetFileNameOrExt(PCFile file,PPathString pOutName,PPathString pOutExt) const override;
		TStdWinError ChangeFileNameAndExt(PFile file,RCPathString newName,RCPathString newExt,PFile &rRenamedFile) override;
		DWORD GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData,TGetFileSizeOptions option) const override;
		TStdWinError DeleteFile(PFile file) override;
		std::unique_ptr<TDirectoryTraversal> BeginDirectoryTraversal(PCFile directory) const override;
		CString GetFileExportNameAndExt(PCFile file,bool shellCompliant) const override;
		TStdWinError ImportFile(CFile *fIn,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile) override;
		// other
		TStdWinError CreateUserInterface(HWND hTdi) override;
		TCmdResult ProcessCommand(WORD cmd) override;
		bool UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const override;
	};




	namespace D80{
		extern const CImage::TProperties Properties;
	}

#endif // MDOS_H
