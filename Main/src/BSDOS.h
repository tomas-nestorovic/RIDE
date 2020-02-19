#ifndef BSDOS_H
#define BSDOS_H

#include "MSDOS7.h"

	#define BSDOS_SECTOR_LENGTH_STD		1024
	#define BSDOS_SECTOR_LENGTH_STD_CODE	TFormat::LENGTHCODE_1024
	#define BSDOS_SECTOR_NUMBER_FIRST	1
	#define BSDOS_SECTOR_NUMBER_LAST	11
	#define BSDOS_SECTOR_NUMBER_TEMP	(BSDOS_SECTOR_NUMBER_FIRST+1)

	#define BSDOS_FAT_COPIES_MAX		2
	#define BSDOS_FAT_LOGSECTOR_MIN		BSDOS_SECTOR_NUMBER_TEMP
	#define BSDOS_FAT_LOGSECTOR_MAX		((BYTE)-1+1)
	#define BSDOS_FAT_ITEMS_PER_SECTOR	(BSDOS_SECTOR_LENGTH_STD/sizeof(CBSDOS308::TFatValue))

	#define BSDOS_DIRS_SLOTS_COUNT		(BSDOS_SECTOR_LENGTH_STD/sizeof(CDirsSector::TSlot))

	#define BSDOS_DIR_CORRUPTED			_T("« Corrupted »")

	class CBSDOS308 sealed:public CSpectrumDos{
		const CTrackMapView trackMap;

		typedef WORD TLogSector;
		typedef PCWORD PCLogSector;

		#pragma pack(1)
		typedef struct TBootSector sealed{
			static const TPhysicalAddress CHS;

			static TBootSector *GetData(PImage image);
			static void WINAPI OnDiskIdChanged(PropGrid::PCustomParam bootSector);
			static UINT AFX_CDECL Verification_thread(PVOID pCancelableAction);

			struct{
				BYTE opCode;
				BYTE param;
			} jmpInstruction;
			BYTE reserved1;
			BYTE signature1;
			WORD nCylinders;
			WORD nSectorsPerTrack;
			WORD nHeads;
			WORD nSectorsPerCluster;
			TLogSector dirsLogSector;
			WORD nSectorsPerFat;
			WORD nBytesInFat;
			TLogSector fatStarts[BSDOS_FAT_COPIES_MAX]; // first LogicalSectors of individual FAT copies
			BYTE diskIdChecksum;
			BYTE reserved2[3];
			BYTE fatSectorsListing[6];
			BYTE signature2;
			DWORD formattedDateTime;
			BYTE signature3;
			char diskName[ZX_TAPE_FILE_NAME_LENGTH_MAX];
			char diskComment[16];
			BYTE diskId[32];
			BYTE reserved3[928];

			bool IsValid() const;
		} *PBootSector;
		typedef const TBootSector *PCBootSector;

		class CBsdosBootView sealed:public CBootView{
			void GetCommonBootParameters(RCommonBootParameters rParam,PSectorData boot) override;
			void AddCustomBootParameters(HWND hPropGrid,HANDLE hGeometry,HANDLE hVolume,const TCommonBootParameters &rParam,PSectorData boot) override;
		public:
			CBsdosBootView(CBSDOS308 *bsdos);

			PBootSector GetSectorData() const;
		} boot;

		#pragma pack(1)
		typedef struct TFatValue sealed{
			enum TSpecialValues:WORD{
				FatError				=0xfffb,
				SystemSector			=0xff00,
				SectorErrorInDataField	=0xfffc,
				SectorNotFound			=0xfffd,
				SectorUnavailable		=0xfffe,
				SectorUnknown			=0xffff,
				SectorEmpty				=0
			};

			union{
				struct{
					WORD info:14;
					WORD continuous:1;
					WORD occupied:1;
				};
				struct{
					BYTE lowerByte;
					BYTE upperByte;
				};
			};

			inline TFatValue();
			inline TFatValue(bool occupied,bool continuous,WORD info);
			inline TFatValue(WORD w);

			inline operator WORD() const;
		} *PFatValue;
		typedef const TFatValue *PCFatValue;

		#pragma pack(1)
		typedef struct TDirectoryEntry sealed{
			class CTraversal sealed:public TDirectoryTraversal{
				const CBSDOS308 *const bsdos;
				const CFatPath dirFatPath;
				TLogSector nDirSectorsTraversed;
				WORD nRemainingEntriesInSector;
			public:
				CTraversal(const CBSDOS308 *bsdos,PCFile slot);

				bool AdvanceToNextEntry() override;
				void ResetCurrentEntry(BYTE directoryFillerByte) const override;
				PFile AllocateNewEntry() override;
			};

			BYTE reserved1:4;
			BYTE fileHasStdHeader:1;
			BYTE fileHasData:1;
			BYTE special:1;
			BYTE occupied:1;
			DWORD dateTimeCreated;
			union{
				struct{
					CTape::THeader stdHeader;
					WORD reserved2;
					DWORD dataLength;
					BYTE dataFlag;
					BYTE reserved3;
					TLogSector firstSector;
				} file;
				struct{
					BYTE reserved4;
					char name[ZX_TAPE_FILE_NAME_LENGTH_MAX];
					char comment[16];
				} dir;
			};

			TDirectoryEntry(const CBSDOS308 *bsdos,TLogSector firstSector);

			BYTE GetDirNameChecksum() const;
		} *PDirectoryEntry;
		typedef const TDirectoryEntry *PCDirectoryEntry;

		class CDirsSector sealed{
			const CBSDOS308 *const bsdos;
		public:
			#pragma pack(1)
			typedef struct TSlot sealed{
				static const TSlot Empty;

				BYTE reserved1:7;
				BYTE subdirExists:1;
				BYTE nameChecksum;
				TLogSector firstSector:14;
				TLogSector reserved2:2;
			} *PSlot;
			typedef const TSlot *PCSlot;

			class CTraversal sealed:public TDirectoryTraversal{
				WORD nSlotsRemaining;
			public:
				CTraversal(const CBSDOS308 *bsdos);

				bool AdvanceToNextEntry() override;
				void ResetCurrentEntry(BYTE directoryFillerByte) const override;
			};

			CDirsSector(const CBSDOS308 *bsdos);

			PSlot GetSlots() const;
			void MarkAsDirty() const;
			//PDirectoryEntry GetRootDirectoryEntries() const;
			PDirectoryEntry TryGetDirectoryEntry(PCSlot slot) const;
			void MarkDirectoryEntryAsDirty(PCSlot slot) const;
		} dirsSector;

		class CBsdos308FileManagerView sealed:public CSpectrumFileManagerView{
			static const TFileInfo InformationList[];
			static const TDirectoryStructureManagement DirManagement;

			static void WINAPI __onFirstDirectoryEntryChanged__(PropGrid::PCustomParam slot);
			static void WINAPI __onSubdirectoryNameChanged__(PropGrid::PCustomParam slot);
			static bool WINAPI __fileTypeModified__(PVOID file,PropGrid::Enum::UValue newType);

			mutable CMSDOS7::TDateTime::CEditor dateTimeEditor;

			void DrawReportModeCell(PCFileInfo pFileInfo,LPDRAWITEMSTRUCT pdis) const override;
			int CompareFiles(PCFile file1,PCFile file2,BYTE information) const override;
			PEditorBase CreateFileInformationEditor(PFile file,BYTE information) const override;
			void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override;
		public:
			CBsdos308FileManagerView(CBSDOS308 *bsdos);
		} fileManager;

		static TStdWinError __recognizeDisk__(PImage image,PFormat pFormatBoot);

		TLogSector __fyzlog__(RCPhysicalAddress chs) const;
		TPhysicalAddress __logfyz__(TLogSector ls) const;
		PSectorData __getHealthyLogicalSectorData__(TLogSector logSector) const;
		void __markLogicalSectorAsDirty__(TLogSector logSector) const;
		TFatValue __getLogicalSectorFatItem__(TLogSector logSector) const;
		bool __setLogicalSectorFatItem__(TLogSector logSector,TFatValue newValue) const;
		BYTE __getFatChecksum__(BYTE fatCopy) const;
		TLogSector __getNextHealthySectorWithoutFat__(TLogSector &rStart,TLogSector end) const;
		TLogSector __getEmptyHealthyFatSector__(bool allowFileFragmentation) const;
	public:
		static const TProperties Properties;

		CBSDOS308(PImage image,PCFormat pFormatBoot);

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
		void GetFileTimeStamps(PCFile file,LPFILETIME pCreated,LPFILETIME pLastRead,LPFILETIME pLastWritten) const override;
		void SetFileTimeStamps(PFile file,const FILETIME *pCreated,const FILETIME *pLastRead,const FILETIME *pLastWritten) override;
		DWORD GetAttributes(PCFile file) const override;
		TStdWinError DeleteFile(PFile file) override;
		std::unique_ptr<TDirectoryTraversal> BeginDirectoryTraversal(PCFile directory) const override;
		DWORD GetDirectoryUid(PCFile dir) const override;
		CString GetFileExportNameAndExt(PCFile file,bool shellCompliant) const override;
		TStdWinError ImportFile(CFile *fIn,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile) override;
		TStdWinError CreateSubdirectory(LPCTSTR name,DWORD winAttr,PFile &rCreatedSubdir);
		TStdWinError SwitchToDirectory(PFile slot);
		TStdWinError MoveFileToCurrentDir(PFile file,LPCTSTR exportFileNameAndExt,PFile &rMovedFile);
		// other
		TStdWinError CreateUserInterface(HWND hTdi) override;
		TCmdResult ProcessCommand(WORD cmd) override;
		void InitializeEmptyMedium(CFormatDialog::PCParameters) override;
		bool ValidateFormatChangeAndReportProblem(bool reformatting,PCFormat f) const override;
	};


	namespace MBD{
		extern const CImage::TProperties Properties;
	}

#endif // BSDOS_H