#ifndef TRDOS_H
#define TRDOS_H

	#define TRDOS_NAME_BASE	_T("TR-DOS")

	#define TRDOS503_FILE_NAME_LENGTH_MAX	8

	#define TRDOS503_TRACK_SECTORS_COUNT	16

	#define TRDOS503_SECTOR_FIRST_NUMBER	1
	#define TRDOS503_SECTOR_LENGTH_STD		256
	#define TRDOS503_SECTOR_LENGTH_STD_CODE	TFormat::LENGTHCODE_256
	#define TRDOS503_SECTOR_RESERVED_COUNT	TRDOS503_TRACK_SECTORS_COUNT

	#define TRDOS503_BOOT_SECTOR_NUMBER		(TRDOS503_SECTOR_FIRST_NUMBER+8)
	#define TRDOS503_BOOT_LABEL_LENGTH_MAX	9
	#define TRDOS503_BOOT_PASSWORD_LENGTH_MAX	9

	#define TRDOS503_DIR_SECTOR_ENTRIES_COUNT	(TRDOS503_SECTOR_LENGTH_STD/sizeof(CTRDOS503::TDirectoryEntry))

	#define TRDOS503_FILE_COUNT_MAX			((TRDOS503_BOOT_SECTOR_NUMBER-TRDOS503_SECTOR_FIRST_NUMBER)*TRDOS503_DIR_SECTOR_ENTRIES_COUNT)

	#define TRDOS503_BOOTB_NOT_MODIFIED		_T("No changes to the \"boot.B\" file made (if previously any).")

	class CTRDOS503:public CSpectrumDos{
		friend class CSCL;
	public:
		enum TDiskFormat:BYTE{
			DS80	=22,
			DS40	=23,
			SS80	=24,
			SS40	=25
		};

		#pragma pack(1)
		struct TSectorTrackPair sealed{
			BYTE sector;
			BYTE track;

			TSectorTrackPair operator+(BYTE nSectors) const;
		};

		#pragma pack(1)
		typedef struct TBootSector sealed{
			static const TPhysicalAddress CHS;

			BYTE zero1; // end of Directory
			BYTE reserved1[224];
			TSectorTrackPair firstFree;
			TDiskFormat format;
			BYTE nFiles;
			WORD nFreeSectors;
			BYTE id;
			WORD reserved2;
			char password[TRDOS503_BOOT_PASSWORD_LENGTH_MAX];
			BYTE zero2;
			BYTE nFilesDeleted;
			char label[TRDOS503_BOOT_LABEL_LENGTH_MAX];
			WORD zero3;

			BYTE __getLabelLengthEstimation__() const; // for TR-DOS version recognition
			void __init__(PCFormat pFormatBoot,BYTE nCharsInLabel,bool userDataInSysTrackAllowed);
			void __setDiskType__(PCFormat pFormatBoot);
		} *PBootSector;
		typedef const TBootSector *PCBootSector;
	protected:
		#pragma pack(1)
		typedef struct TDirectoryEntry sealed{
			enum TExtension:BYTE{
				BASIC_PRG	='B', // Basic
				DATA_FIELD	='D',
				BLOCK		='C', // Code
				PRINT		='#',
				DELETED		=1,
				END_OF_DIR	=0
			};

			char name[TRDOS503_FILE_NAME_LENGTH_MAX];
			BYTE extension;
			WORD parameterA;
			WORD parameterB;
			BYTE nSectors;
			TSectorTrackPair first;

			WORD __getOfficialFileSize__(PBYTE pnBytesReservedAfterData) const;
			WORD __getFileSizeOnDisk__() const;
			void __markTemporary__();
			bool __isTemporary__() const;
		} *PDirectoryEntry;
		typedef const TDirectoryEntry *PCDirectoryEntry;

		typedef CTRDOS503 *PTRDOS503;

		struct TTrdosDirectoryTraversal sealed:public TDirectoryTraversal{
		private:
			const CTRDOS503 *const trdos;
			bool foundEndOfDirectory;
			BYTE nRemainingEntriesInSector;
		public:
			TTrdosDirectoryTraversal(const CTRDOS503 *_trdos); // ctor
			bool AdvanceToNextEntry() override;
			void ResetCurrentEntry(BYTE directoryFillerByte) const override;
			bool __existsNextEntry__();
		};

		class CTrdosBootView sealed:public CBootView{
			static PropGrid::Enum::PCValueList WINAPI __getListOfKnownFormats__(PVOID,WORD &rnFormats);
			static LPCTSTR WINAPI __getFormatDescription__(PVOID,PropGrid::Enum::UValue format,PTCHAR buf,short bufCapacity);
			static bool WINAPI __onFormatChanged__(PVOID,PropGrid::Enum::UValue newValue);

			void GetCommonBootParameters(RCommonBootParameters rParam,PSectorData _boot) override;
			void AddCustomBootParameters(HWND hPropGrid,HANDLE hGeometry,HANDLE hVolume,const TCommonBootParameters &rParam,PSectorData _boot) override;
		public:
			const BYTE nCharsInLabel;

			CTrdosBootView(PTRDOS503 trdos,BYTE nCharsInLabel);
		} boot;

		class CTrdosFileManagerView sealed:public CSpectrumFileManagerView{
			static const TFileInfo InformationList[];

			static bool WINAPI __onStdParam1Changed__(PVOID file,int newWord);
			static bool WINAPI __onStdParam2Changed__(PVOID file,int newWord);

			mutable WORD stdParameter; // buffer to edit StandardParameter

			void DrawReportModeCell(PCFileInfo pFileInfo,LPDRAWITEMSTRUCT pdis) const override;
			int CompareFiles(PCFile file1,PCFile file2,BYTE information) const override;
			PEditorBase CreateFileInformationEditor(PFile file,BYTE infoId) const override;
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		public:
			CTrdosFileManagerView(PTRDOS503 trdos);
		} fileManager;

		static const CFormatDialog::TStdFormat StdFormats[];

		static TStdWinError __recognizeDisk__(PImage image,PFormat pFormatBoot);
		static PBootSector __getBootSector__(PImage image);
		static UINT AFX_CDECL __defragmentation_thread__(PVOID _pCancelableAction);
		static void __informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId);

		bool zeroLengthFilesEnabled,importToSysTrack;

		PBootSector __getBootSector__() const;
		void InitializeEmptyMedium(CFormatDialog::PCParameters) override;
		BYTE __getDirectory__(PDirectoryEntry *directory) const;
		bool __parameterAfterData__(PCDirectoryEntry de,bool modify,PWORD pw) const;
		bool __getStdParameter1__(PCDirectoryEntry de,WORD &rParam1) const;
		bool __setStdParameter1__(PDirectoryEntry de,WORD newParam1);
		bool __getStdParameter2__(PCDirectoryEntry de,WORD &rParam2) const;
		bool __setStdParameter2__(PDirectoryEntry de,WORD newParam2);
	public:
		static const TProperties Properties;

		CTRDOS503(PImage image,PCFormat pFormatBoot,PCProperties pTrdosProps);

		// boot
		void FlushToBootSector() const override; // projects information stored in internal FormatBoot back to the Boot Sector (e.g. called automatically by BootView)
		// FAT
		bool GetSectorStatuses(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PSectorStatus buffer) const override;
		bool ModifyStdSectorStatus(RCPhysicalAddress,TSectorStatus) override;
		bool GetFileFatPath(PCFile file,CFatPath &rFatPath) const override;
		DWORD GetFreeSpaceInBytes(TStdWinError &rError) const override;
		// file system
		void GetFileNameOrExt(PCFile file,PTCHAR bufName,PTCHAR bufExt) const override;
		TStdWinError ChangeFileNameAndExt(PFile file,LPCTSTR newName,LPCTSTR newExt,PFile &rRenamedFile) override;
		DWORD GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData,TGetFileSizeOptions option) const override;
		TStdWinError DeleteFile(PFile file) override;
		std::unique_ptr<TDirectoryTraversal> BeginDirectoryTraversal(PCFile directory) const override;
		PTCHAR GetFileExportNameAndExt(PCFile file,bool shellCompliant,PTCHAR buf) const override;
		TStdWinError ImportFile(CFile *fIn,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile) override;
		// other
		TStdWinError CreateUserInterface(HWND hTdi) override;
		TCmdResult ProcessCommand(WORD cmd) override;
		bool UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const override;
	};




	#define TRDOS504_BOOT_LABEL_LENGTH_MAX	8

	class CTRDOS504:public CTRDOS503{
		static TStdWinError __recognizeDisk__(PImage image,PFormat pFormatBoot);
	public:
		static const TProperties Properties;

		CTRDOS504(PImage image,PCFormat pFormatBoot,PCProperties pTrdosProps);
	};




	#define TRDOS505_BOOT_LABEL_LENGTH_MAX	10

	class CTRDOS505 sealed:public CTRDOS504{
		static TStdWinError __recognizeDisk__(PImage image,PFormat pFormatBoot);
	public:
		static const TProperties Properties;

		CTRDOS505(PImage image,PCFormat pFormatBoot);
	};



	



	namespace TRD{
		extern const CImage::TProperties Properties;
	}

#endif // TRDOS_H
