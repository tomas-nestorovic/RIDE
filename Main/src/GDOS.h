#ifndef GDOS_H
#define GDOS_H

	#define GDOS_CYLINDERS_COUNT		80
	#define GDOS_TRACK_SECTORS_COUNT	10
	#define GDOS_SECTOR_LENGTH_STD		512
	#define GDOS_SECTOR_LENGTH_STD_CODE	TFormat::LENGTHCODE_512

	#define GDOS_FILE_NAME_LENGTH_MAX	10
	#define GDOS_DIR_FILES_COUNT_MAX	80

	class CGDOS sealed:public CSpectrumDos{
		typedef CGDOS *PGDOS;

		#pragma pack(1)
		struct TSectorInfo sealed{
			BYTE track,sector;

			TSectorInfo();
			TSectorInfo(BYTE cyl,BYTE head,BYTE sector);

			bool operator<(const TSectorInfo &rsi2) const;
			bool __isValid__() const;
			TPhysicalAddress __getChs__() const;
			void __setChs__(RCPhysicalAddress chs);
			void __setEof__();
		};

		#pragma pack(1)
		struct TStdZxTypeData sealed{
			TZxRom::TFileType romId; // as in ZX ROM: 0 = Basic, 1 = number array, 2 = character array, 3 = code
			union{
				struct{
					WORD length;
					WORD startInMemory; // usually 23755
					WORD lengthWithoutVariables;
					WORD autorunLine;
				} basic;
				struct{
					WORD length;
					WORD startInMemory;
					WORD name;
				} numberArray;
				struct{
					WORD length;
					WORD startInMemory;
					WORD name;
				} charArray;
				struct{
					WORD length;
					WORD startInMemory; // usually 23755
					WORD reserved2;
					WORD autostartAddress; // 0 = none
				} code;
			};
		};

		#pragma pack(1)
		typedef struct TGdosSectorData sealed{
			union{
				TStdZxTypeData stdZxType;
				BYTE data[GDOS_SECTOR_LENGTH_STD-sizeof(TSectorInfo)];
			};
			TSectorInfo nextSector;
		} *PGdosSectorData;
		typedef const TGdosSectorData *PCGdosSectorData;

		#pragma pack(1)
		typedef struct TDirectoryEntry sealed{
			enum TFileType:BYTE{
				BASIC			=1,
				NUMBER_ARRAY	=2,
				CHAR_ARRAY		=3,
				CODE			=4,
				SNAPSHOT_48K	=5,
				MICRODRIVE		=6,
				SCREEN			=7,
				SPECIAL			=8,
				SNAPSHOT_128K	=9,
				OPENTYPE		=10,
				EXECUTE			=11,
				EMPTY_ENTRY		=0
			} fileType;
			char name[GDOS_FILE_NAME_LENGTH_MAX];
			TBigEndianWord nSectors; // count of Sectors occupied by this File
			TSectorInfo firstSector;
			struct TSectorAllocationBitmap sealed{
			private:
				BYTE allocated[195];
			public:
				static WORD __sectorChs2sectorId__(RCPhysicalAddress chs);

				TSectorAllocationBitmap(); // ctor

				bool IsSectorAllocated(RCPhysicalAddress chs) const;
				void SetSectorAllocation(RCPhysicalAddress chs,bool isAllocated);
				bool IsDisjunctiveWith(const TSectorAllocationBitmap &rSab2) const;
				void MergeWith(const TSectorAllocationBitmap &rSab2);
			} sectorAllocationBitmap;
			union UEtc{
				struct{
					BYTE reserved;
					TStdZxTypeData stdZxType;
				};
				BYTE snapshot48k[46];
				BYTE microdrive[46];
				BYTE screen[46];
				BYTE special[46];
				BYTE snapshot128k[46];
				BYTE opentype[46];
				BYTE execute[46];
			} etc;

			bool __isStandardRomFile__() const;
			LPCTSTR __getFileTypeDesc__(PTCHAR buffer) const;
			void __getNameAndExt__(PTCHAR bufName,PTCHAR bufExt) const;
			void __setNameAndExt__(LPCTSTR newName,LPCTSTR newExt);
			PWORD __getStdParameter1__();
			void __setStdParameter1__(WORD param1);
			PWORD __getStdParameter2__();
			void __setStdParameter2__(WORD param2);
			DWORD __getDataSize__(PBYTE pnBytesReservedBeforeData) const;
			void __setDataSizeByFileType__(DWORD size);
		} *PDirectoryEntry;
		typedef const TDirectoryEntry *PCDirectoryEntry;

		struct TGdosDirectoryTraversal sealed:public TDirectoryTraversal{
		private:
			const CGDOS *const gdos;
			BYTE nRemainingEntriesInSector;
		public:
			TGdosDirectoryTraversal(const CGDOS *gdos); // ctor
			bool AdvanceToNextEntry() override;
			void ResetCurrentEntry(BYTE directoryFillerByte) const override;
			bool __existsNextEntry__();
		};

		class CGdosFileManagerView sealed:public CSpectrumFileManagerView{
			static const TFileInfo InformationList[];

			static bool WINAPI __onStdParam1Changed__(PVOID file,int newWordValue);
			static bool WINAPI __onStdParam2Changed__(PVOID file,int newWordValue);

			mutable class CExtensionEditor sealed{
				static bool WINAPI __onChanged__(PVOID file,PropGrid::Enum::UValue newExt);
				static LPCTSTR WINAPI __getDescription__(PVOID file,PropGrid::Enum::UValue extension,PTCHAR buf,short bufCapacity);

				BYTE data;
			public:
				PEditorBase Create(PDirectoryEntry de);
			} extensionEditor;

			mutable class CEtcEditor sealed{
				static bool WINAPI __onEllipsisClick__(PVOID file,PVOID,short);
			public:
				PEditorBase Create(PDirectoryEntry de);
			} etcEditor;

			void DrawReportModeCell(PCFileInfo pFileInfo,LPDRAWITEMSTRUCT pdis) const override;
			int CompareFiles(PCFile file1,PCFile file2,BYTE information) const override;
			PEditorBase CreateFileInformationEditor(PFile file,BYTE infoId) const override;
			//LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		public:
			CGdosFileManagerView(PGDOS gdos);
		} fileManager;

		static TStdWinError __recognizeDisk__(PImage image,PFormat pFormatBoot);
		static void __informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId);

		bool zeroLengthFilesEnabled;

		void InitializeEmptyMedium(CFormatDialog::PCParameters params) override;
	public:
		static const TProperties Properties;

		CGDOS(PImage image,PCFormat pFormatBoot);

		// boot
		void FlushToBootSector() const override; // projects information stored in internal FormatBoot back to the Boot Sector (e.g. called automatically by BootView)
		// FAT
		bool GetSectorStatuses(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PSectorStatus buffer) const override;
		bool ModifyStdSectorStatus(RCPhysicalAddress chs,TSectorStatus status) override;
		bool GetFileFatPath(PCFile file,CFatPath &rFatPath) const override;
		// file system
		void GetFileNameOrExt(PCFile file,PTCHAR bufName,PTCHAR bufExt) const override;
		TStdWinError ChangeFileNameAndExt(PFile file,LPCTSTR newName,LPCTSTR newExt,PFile &rRenamedFile) override;
		DWORD GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData,TGetFileSizeOptions option) const override;
		TStdWinError DeleteFile(PFile file) override;
		std::unique_ptr<TDirectoryTraversal> BeginDirectoryTraversal(PCFile directory) const override;
		PTCHAR GetFileExportNameAndExt(PCFile file,bool shellCompliant,PTCHAR buf) const override;
		DWORD ExportFile(PCFile file,CFile *fOut,DWORD nBytesToExportMax,LPCTSTR *pOutError) const override;
		TStdWinError ImportFile(CFile *fIn,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile) override;
		// other
		TStdWinError CreateUserInterface(HWND hTdi) override;
		TCmdResult ProcessCommand(WORD cmd) override;
		bool UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const override;
	};

#endif // GDOS
