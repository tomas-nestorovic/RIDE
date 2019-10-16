#ifndef MSDOS_H
#define MSDOS_H

	#define MSDOS7_SECTOR_LENGTH_STD	512
	#define MSDOS7_SECTOR_LENGTH_STD_CODE	TFormat::LENGTHCODE_512
	#define MSDOS7_SECTOR_FSINFO		1
	#define MSDOS7_SECTOR_BKBOOT		6

	#define MSDOS7_LABEL_LENGTH_MAX		11

	#define MSDOS7_FAT_ERROR			1
	#define MSDOS7_FAT_CLUSTER_BAD		0x0ffffff7
	#define MSDOS7_FAT_CLUSTER_EMPTY	0x0
	#define MSDOS7_FAT_CLUSTER_EOF		0x0ffffff8

	#define MSDOS7_DIR_ROOT			nullptr
	#define MSDOS7_DIR_DOT			0x2020202e
	#define MSDOS7_DIR_DOTDOT		0x20202e2e

	#define MSDOS7_DATA_CLUSTER_FIRST	2 /* logical number of the first data Cluster */

	#define MSDOS7_FILE_NAME_LENGTH_MAX	8
	#define MSDOS7_FILE_EXT_LENGTH_MAX	3
	#define MSDOS7_FILE_ICONS_COUNT		11

	#define FILE_ATTRIBUTE_VOLUME		8


	class CMSDOS7 sealed:public CDos{
		typedef WORD TLogSector16;
		typedef DWORD TLogSector32;
		typedef DWORD TCluster32;

		class CFat sealed{
			const CMSDOS7 &msdos;
		public:
			enum TType:BYTE{
				UNDETERMINED=0,
				FAT12=3, // number of half-Bytes
				FAT16=4,
				FAT32=8
			} type;

			static const TType Types[];

			static TType GetFatType(TCluster32 nClusters);

			mutable TCluster32 nFreeClustersTemp;
			mutable TCluster32 firstFreeClusterTemp;

			CFat(const CMSDOS7 &msdos);

			TCluster32 GetMinCountOfClusters() const;
			TCluster32 GetMaxCountOfClusters() const;
			DWORD GetClusterValue(TCluster32 cluster) const;
			bool SetClusterValue(TCluster32 cluster,DWORD newValue) const;
			bool FreeChainOfClusters(TCluster32 cluster) const;
		} fat;

		#pragma pack(1)
		struct TVolumeInfo{
			BYTE infoValid;
			DWORD id;
			char label[MSDOS7_LABEL_LENGTH_MAX];
			char fatId[8];

			void __init__(const CFat &rFat);
		};

		#pragma pack(1)
		typedef struct TBootSector sealed{
			enum TMsdosMediumType:BYTE{
					FLOPPY	=0,
					HDD		=0x80
			};

			static TPhysicalAddress __getRecognizedChs__(PImage image,bool recognizeBoot,bool *pSuccess);

			#pragma pack(1)
			struct{
				BYTE opCode;
				WORD param;
			} jmpInstruction;
			char oemName[8];
			WORD sectorSize;
			BYTE nSectorsInCluster;
			TLogSector16 nReservedSectors;
			BYTE nFatCopies;
			WORD nRootDirectoryEntries;
			TLogSector16 nSectorsInTotal16;
			enum TMsdosMedium:BYTE{
				DISK_35_1440_DS_18	=0xf0,
				DISK_35_720_DS_9	=0xf9,
				DISK_525_180_SS_9	=0xfc,
				DISK_525_360_DS_9	=0xfd,
				DISK_525_160_SS_8	=0xfe,
				DISK_525_320_DS_8	=0xff,
				DISK_HARD			=0xf8
			} medium;
			TLogSector16 nSectorsFat16;
			TLogSector16 nSectorsOnTrack;
			WORD nHeads;
			TLogSector32 nSectorsHidden;
			TLogSector32 nSectorsInTotal32;
			union{
				#pragma pack(1)
				struct{
					TMsdosMediumType mediumType;
					BYTE reserved1;
					TVolumeInfo volume;
					BYTE reserved2[448];
				} fat1216;
				#pragma pack(1)
				struct{
					TLogSector32 nSectorsFat32;
					WORD flags; // TODO: missing in "Boot Sector" tab
					WORD version; // TODO: missing in "Boot Sector" tab
					TCluster32 rootDirectoryFirstCluster; // TODO: missing in "Boot Sector" tab
					TLogSector16 fsInfo; // TODO: missing in "Boot Sector" tab
					TLogSector16 bootCopy; // TODO: missing in "Boot Sector" tab
					BYTE reserved1[12];
					TMsdosMediumType mediumType;
					BYTE reserved2;
					TVolumeInfo volume;
					BYTE reserved3[420];
				} fat32;
			};
			WORD AA55mark;

			bool __recognize__(WORD sectorLength) const;
			bool __isUsable__() const;
			TLogSector32 __getCountOfAllSectors__() const;
			void __getGeometry__(PFormat pFormat) const;
			void __init__(PCFormat pFormatBoot,CFormatDialog::PCParameters params,CFat &rOutFat);
			DWORD __getCountOfSectorsInOneFatCopy__() const;
			TLogSector32 __getRootDirectoryFirstSector__() const;
			TLogSector16 __getCountOfPermanentRootDirectorySectors__() const;
			DWORD __getCountOfNondataSectors__() const;
			DWORD __getClusterSizeInBytes__() const;
		} *PBootSector;
		typedef const TBootSector *PCBootSector;

		#pragma pack(1)
		typedef struct TFsInfoSector sealed{
			DWORD mark41615252;
			BYTE reserved1[480];
			DWORD mark61417272;
			TCluster32 nFreeClusters;
			TCluster32 firstFreeCluster;
			BYTE reserved2[12];
			DWORD markAA550000;

			bool __recognize__(WORD sectorLength) const;
			void __init__();
		} *PFsInfoSector;
		typedef const TFsInfoSector *PCFsInfoSector;

		#pragma pack(1)
		typedef union UDirectoryEntry{
			enum TSpecial:char{
				EMPTY_ENTRY		=(char)0xe5,
				LONG_NAME_END	=0x40,
				DIRECTORY_END	=0
			};
			#pragma pack(1)
			struct TShortNameEntry sealed{
				static bool __isCharacterValid__(char c);
				//static PTCHAR __convertLongToShortTerm__(PTCHAR bufShort,BYTE bufShortChars,LPCTSTR bufLong);

				char name[MSDOS7_FILE_NAME_LENGTH_MAX];
				char extension[MSDOS7_FILE_EXT_LENGTH_MAX];
				BYTE attributes;
				WORD reserved;
				DWORD timeAndDateCreated; // LOW = time, HIGH = date
				WORD dateLastAccessed;
				WORD firstClusterHigh;
				DWORD timeAndDateLastModified; // LOW = time, HIGH = date
				WORD firstClusterLow;
				DWORD size;

				TCluster32 __getFirstCluster__() const;
				void __setFirstCluster__(TCluster32 c);
				BYTE __getChecksum__() const;
				bool __isDotOrDotdot__() const;
			} shortNameEntry;
			#pragma pack(1)
			struct TLongNameEntry sealed{
				BYTE sequenceNumber;
				WCHAR name1[5];
				BYTE attributes;
				BYTE zero1;
				BYTE checksum;
				WCHAR name2[6];
				WORD zero2;
				WCHAR name3[2];

				//bool __isCharacterValid__(WCHAR c) const; //TODO
			} longNameEntry;
		} *PDirectoryEntry;
		typedef const UDirectoryEntry *PCDirectoryEntry;

		typedef CMSDOS7 *PMSDOS7;

		struct TMsdos7DirectoryTraversal sealed:public TDirectoryTraversal{
		private:
			const CMSDOS7 *const msdos7;
			bool foundEndOfDirectory,fatError;
			TCluster32 cluster,next;
			TLogSector16 nRemainingSectorsInCluster; // TLogSector16 as there can be up to 4095 Root Directory Sectors in FAT16
			TLogSector32 dirSector;
			BYTE nRemainingEntriesInSector;
		public:
			TMsdos7DirectoryTraversal(const CMSDOS7 *_msdos7,PCFile directory); // ctor
			PFile AllocateNewEntry() override;
			PDirectoryEntry __allocateNewEntry__();
			bool AdvanceToNextEntry() override;
			void ResetCurrentEntry(BYTE directoryFillerByte) const override;
			bool __existsNextEntry__();
		};

		CTrackMapView trackMap;

		class CMsdos7BootView sealed:public CBootView{
			static bool WINAPI __pg_createLabel__(CPropGridCtrl::PCustomParam,int hyperlinkId,LPCTSTR hyperlinkName);
			static bool WINAPI __labelModified__(CPropGridCtrl::PCustomParam,LPCSTR,short);
			static bool WINAPI __onMediumChanged__(PVOID,CPropGridCtrl::TEnum::UValue newValue);
			static CPropGridCtrl::TEnum::PCValueList WINAPI __getListOfMedia__(PVOID,WORD &rnMedia);
			static LPCTSTR WINAPI __getMediumDescription__(PVOID,CPropGridCtrl::TEnum::UValue medium,PTCHAR,short);
			static CPropGridCtrl::TEnum::PCValueList WINAPI __getListOfMediaTypes__(PVOID,WORD &rnMediumTypes);
			static LPCTSTR WINAPI __getMediumTypeDescription__(PVOID,CPropGridCtrl::TEnum::UValue mediumType,PTCHAR,short);

			void GetCommonBootParameters(RCommonBootParameters rParam,PSectorData _boot) override;
			void AddCustomBootParameters(HWND hPropGrid,HANDLE hGeometry,HANDLE hVolume,const TCommonBootParameters &rParam,PSectorData _boot) override;
		public:
			CMsdos7BootView(PMSDOS7 msdos);

			PBootSector GetSectorData() const;
		} boot;

		class CFsInfoView sealed:public CCriticalSectorView{
			static bool WINAPI __sectorModified__(CPropGridCtrl::PCustomParam,int);

			void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override;
		public:
			CFsInfoView(CMSDOS7 *msdos);

			//TPhysicalAddress GetPhysicalAddress() const;
			PFsInfoSector GetSectorData() const;
			void MarkSectorAsDirty() const;
		} fsInfo;

		class CMsdos7FileManagerView sealed:public CFileManagerView{
			static bool WINAPI __onNameAndExtConfirmed__(PVOID file,LPCTSTR newNameAndExt,short nCharsOfNewNameAndExt);
			static bool WINAPI __editFileAttributes__(PVOID file,PVOID,short);
			static bool WINAPI __editFileDateTime__(PVOID file,PVOID value,short valueSize);

			static const TFileInfo InformationList[];
			static const TDirectoryStructureManagement dirManagement;

			const HMODULE hShell32;
			const Utils::CRideFont font;
			HICON icons[MSDOS7_FILE_ICONS_COUNT];

			HICON __getIcon__(PCDirectoryEntry de) const;
			void DrawFileInfo(LPDRAWITEMSTRUCT pdis,const int *tabs) const override;
			int CompareFiles(PCFile file1,PCFile file2,BYTE information) const override;
			PEditorBase CreateFileInformationEditor(PFile file,BYTE infoId) const override;
			PTCHAR GenerateExportNameAndExtOfNextFileCopy(CDos::PCFile file,bool shellCompliant,PTCHAR pOutBuffer) const override;
			void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override; // supplying File Icons
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		public:
			CMsdos7FileManagerView(PMSDOS7 msdos);
			~CMsdos7FileManagerView();
		} fileManager;

		static TStdWinError __recognizeDisk__(PImage image,PFormat pFormatBoot);
		static PDos __instantiate__(PImage image,PCFormat pFormatBoot);
		static TLogSector32 __cluster2logSector__(TCluster32 c,PCBootSector boot);
		static UINT AFX_CDECL __removeLongNames_thread__(PVOID _pCancelableAction);
		static void __informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId);
		static PTCHAR __getFileExportNameAndExt__(LPCTSTR bufName,LPCTSTR bufExt,bool shellCompliant,PTCHAR pOutBuffer);

		bool dontShowLongFileNames, dontShowDotEntries, dontShowDotdotEntries;

		TLogSector32 __fyzlog__(RCPhysicalAddress chs) const;
		TPhysicalAddress __logfyz__(TLogSector32 ls) const;
		TLogSector32 __cluster2logSector__(TCluster32 cluster,BYTE &rnSectorsInCluster) const;
		TCluster32 __logSector2cluster__(TLogSector32 ls) const;
		TCluster32 __getCountOfClusters__() const;
		void __adoptMediumFromBootSector__();
		PSectorData __getHealthyLogicalSectorData__(TLogSector32 logSector) const;
		void __markLogicalSectorAsDirty__(TLogSector32 logSector) const;
		TCluster32 __getFirstFreeHealthyCluster__() const;
		TCluster32 __allocateAndResetDirectoryCluster__() const;
		BYTE __getLongFileNameEntries__(PCDirectoryEntry de,PDirectoryEntry *bufEntries) const;
		void __deleteLongFileNameEntries__(PCDirectoryEntry de) const;
		void __getShortFileNameAndExt__(PCDirectoryEntry de,PTCHAR bufName,PTCHAR bufExt) const;
		TStdWinError __changeShortFileNameAndExt__(PDirectoryEntry de,LPCTSTR newName,LPCTSTR newExt,PDirectoryEntry &rRenamedFile) const;
		void __generateShortFileNameAndExt__(PDirectoryEntry de,LPCTSTR longName,LPCTSTR longExt) const;
		bool __getLongFileNameAndExt__(PCDirectoryEntry de,PTCHAR bufName,PTCHAR bufExt) const;
		TStdWinError __changeLongFileNameAndExt__(PDirectoryEntry de,LPCTSTR newName,LPCTSTR newExt,PDirectoryEntry &rRenamedFile) const;
		TStdWinError __createSubdirectory__(LPCTSTR name,DWORD winAttr,PDirectoryEntry &rCreatedSubdir);
		TStdWinError __switchToDirectory__(PDirectoryEntry directory);
		TStdWinError __moveFileToCurrDir__(PDirectoryEntry de,LPCTSTR fileNameAndExt,PDirectoryEntry &rMovedFile);
	public:
		struct TDateTime sealed:public TFileDateTime{
			static const SYSTEMTIME Epoch[];

			TDateTime(WORD msdosDate);
			TDateTime(DWORD msdosTimeAndDate);
			TDateTime(const FILETIME &r);

			LPCTSTR ToString(PTCHAR buf) const;
			PTCHAR DateToString(PTCHAR buf) const;
			bool ToDWord(PDWORD pOutResult) const;
			void DrawInPropGrid(HDC dc,RECT rc,bool onlyDate=false,BYTE horizonalAlignment=DT_RIGHT) const;
		};

		static const TProperties Properties;

		CMSDOS7(PImage image,PCFormat pFormatBoot);

		// boot
		void FlushToBootSector() const override; // projects information stored in internal FormatBoot back to the Boot Sector (e.g. called automatically by BootView)
		// FAT
		bool GetSectorStatuses(TCylinder cyl,THead head,TSector nSectors,PCSectorId sectors,PSectorStatus buffer) const override;
		bool ModifyTrackInFat(TCylinder cyl,THead head,PSectorStatus statuses) override;
		bool GetFileFatPath(PCFile file,CFatPath &rFatPath) const override;
		DWORD GetFreeSpaceInBytes(TStdWinError &rError) const override;
		TCylinder GetFirstCylinderWithEmptySector() const override;
		// file system
		void GetFileNameAndExt(PCFile file,PTCHAR bufName,PTCHAR bufExt) const override;
		TStdWinError ChangeFileNameAndExt(PFile file,LPCTSTR newName,LPCTSTR newExt,PFile &rRenamedFile) override;
		DWORD GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData,TGetFileSizeOptions option) const override;
		void GetFileTimeStamps(PCFile file,LPFILETIME pCreated,LPFILETIME pLastRead,LPFILETIME pLastWritten) const override;
		void SetFileTimeStamps(PFile file,const FILETIME *pCreated,const FILETIME *pLastRead,const FILETIME *pLastWritten) override;
		DWORD GetAttributes(PCFile file) const override;
		TStdWinError DeleteFile(PFile file) override;
		std::unique_ptr<TDirectoryTraversal> BeginDirectoryTraversal(PCFile directory) const override;
		PTCHAR GetFileExportNameAndExt(PCFile file,bool shellCompliant,PTCHAR buf) const override;
		TStdWinError ImportFile(CFile *fIn,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile) override;
		// other
		TStdWinError CreateUserInterface(HWND hTdi) override;
		TCmdResult ProcessCommand(WORD cmd) override;
		bool UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const override;
		void InitializeEmptyMedium(CFormatDialog::PCParameters params) override;
		bool ValidateFormatChangeAndReportProblem(bool reformatting,PCFormat f) const override;
	};

#endif // MSDOS_H
