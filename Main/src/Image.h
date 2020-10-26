#ifndef IMAGE_H
#define IMAGE_H

/*	#define DECLARE_KNOWN_IMAGE(Class) \
		public: \
			static PImage known##Class();

	#define IMPLEMENT_KNOWN_IMAGE(Class) \
		PImage KNOWN_IMAGE(Class) (){ \
			return new Class; \
		}

	#define KNOWN_IMAGE(Class) \
		Class::known##Class

	#define INSTANTIATE_KNOWN_IMAGE(image,fn) \
		image=( (PImage (*)())fn )(); /* retyping Fn to a function and calling this funtion */


	#define IMAGE_FORMAT_SEPARATOR	_T(";")	/* e.g. "*.d40;*.d80" */


	typedef BYTE *PSectorData;
	typedef const BYTE *PCSectorData;
	typedef WORD TCylinder,*PCylinder; typedef short &RCylinder;
	typedef const TCylinder *PCCylinder;
	typedef BYTE THead,*PHead,TSide,*PSide,TSector,*PSector;
	typedef const THead *PCHead;
	typedef const TSide *PCSide;
	typedef const TSector *PCSector;
	typedef int TTrack,*PTrack;

	#define FDD_CYLINDERS_MAX	82
	#define FDD_SECTORS_MAX		64
	#define FDD_350_SECTOR_GAP3	54
	#define FDD_525_SECTOR_GAP3	80
	#define FDD_NANOSECONDS_PER_DD_BYTE	TIME_MICRO(32)

	#define HDD_CYLINDERS_MAX	(TCylinder)-1
	#define HDD_HEADS_MAX		63

	#define DEVICE_NAME_CHARS_MAX 48

	#pragma pack(1)
	typedef const struct TMedium sealed{
		enum TType:BYTE{
			UNKNOWN			=(BYTE)-1,
			FLOPPY_HD_350	=1,
			FLOPPY_DD_350	=2,
			FLOPPY_DD_525	=4,
			FLOPPY_ANY_DD	=FLOPPY_DD_350|FLOPPY_DD_525,
			FLOPPY_ANY		=FLOPPY_HD_350|FLOPPY_ANY_DD,
			HDD_RAW			=8
		};
		#pragma pack(1)
		typedef const struct TProperties sealed{
			PropGrid::Integer::TUpDownLimits cylinderRange, headRange, sectorRange;
		} *PCProperties;

		static LPCTSTR GetDescription(TType mediumType);
		static PCProperties GetProperties(TType mediumType);
	} *PCMedium;

	typedef struct TFormat sealed{
		static const TFormat Unknown;

		union{
			TMedium::TType supportedMedia;
			TMedium::TType mediumType;
		};
		TCylinder nCylinders;
		THead nHeads;
		TSector nSectors;
		enum TLengthCode:BYTE{
			LENGTHCODE_128	=0,
			LENGTHCODE_256	=1,
			LENGTHCODE_512	=2,
			LENGTHCODE_1024	=3,
			LENGTHCODE_2048	=4,
			LENGTHCODE_4096	=5,
			LENGTHCODE_8192	=6,
			LENGTHCODE_16384=7
		} sectorLengthCode;
		WORD sectorLength;
		WORD clusterSize; // in Sectors

		bool operator==(const TFormat &fmt2) const;
		DWORD GetCountOfAllSectors() const;
		WORD GetCountOfSectorsPerCylinder() const;
		TTrack GetCountOfAllTracks() const;
	} *PFormat;
	typedef const TFormat *PCFormat,&RCFormat;

	#pragma pack(1)
	typedef struct TSectorId sealed{
		TCylinder cylinder;
		TSide side;
		TSector sector;
		BYTE lengthCode;

		bool operator==(const TSectorId &id2) const;
		bool operator!=(const TSectorId &id2) const;
		TSectorId &operator=(const FD_ID_HEADER &rih);
		TSectorId &operator=(const FD_TIMED_ID_HEADER &rtih);

		CString ToString() const;
	} *PSectorId;
	typedef const TSectorId *PCSectorId;

	#pragma pack(1)
	typedef const struct TPhysicalAddress sealed{
		static const TPhysicalAddress Invalid;

		TCylinder cylinder;
		THead head;
		TSectorId sectorId;

		bool operator==(const TPhysicalAddress &chs2) const;
		bool operator!=(const TPhysicalAddress &chs2) const;
		TTrack GetTrackNumber() const;
		TTrack GetTrackNumber(THead nHeads) const;
	} &RCPhysicalAddress;


	#define FDC_ST1_END_OF_CYLINDER		128
			// ^ FDC tried to access a sector beyond the final sector of the track (255D*).  Will be set if TC is not issued after Read or Write Data command
	#define FDC_ST1_DATA_ERROR			32
			// ^ FDC detected a CRC error in either the ID field or the data field of a sector
	#define FDC_ST1_NO_DATA				4
			// ^ (1) "Read Data" or "Read Deleted Data" commands - FDC did not find the specified sector; may also occur in "Read ID" and "Read Track" commands, but that's not important in scope of this application
			//	 (2) "Read ID" command - FDC cannot read the ID field without an error
			//	 (3) "Read Track" command - FDC cannot find the proper sector sequence
	#define FDC_ST1_NO_ADDRESS_MARK		1
			// ^ (1) The FDC did not detect an ID address mark at the specified track after encountering the index pulse from the IDX pin twice
			//	 (2) The FDC cannot detect a data address mark or a deleted data address mark on the specified track


	#define FDC_ST2_DELETED_DAM			64
			// ^ (1) "Read Data" command - FDC encountered a deleted data address mark
			//	 (2) "Read Deleted Data" command - the FDC encountered a data address mark
	#define FDC_ST2_CRC_ERROR_IN_DATA	32
			// ^ FDC detected a CRC error in the data field
	#define FDC_ST2_NOT_DAM				1
			// ^ The FDC cannot detect a data address mark or a deleted data address mark


	#pragma pack(1)
	typedef const struct TFdcStatus sealed{
		static const TFdcStatus WithoutError;
		static const TFdcStatus SectorNotFound;
		static const TFdcStatus DeletedDam;

		BYTE reg1,reg2;

		TFdcStatus();
		TFdcStatus(BYTE _reg1,BYTE _reg2);

		WORD ToWord() const;
		void GetDescriptionsOfSetBits(LPCTSTR *pDescriptions) const;
		bool IsWithoutError() const;
		bool DescribesIdFieldCrcError() const;
		void CancelIdFieldCrcError();
		bool DescribesDataFieldCrcError() const;
		void CancelDataFieldCrcError();
		bool DescribesDeletedDam() const;
		bool DescribesMissingDam() const;
	} *PCFdcStatus;

	enum TTrackScheme:BYTE{
		BY_CYLINDERS	=1,
		BY_SIDES		=2
	};





	#define MAKE_IMAGE_ID(char1,char2,char3,char4,char5,char6,char7,char8)\
		( (((((((CImage::TId)char1<<4^char2)<<4^char3)<<4^char4)<<4^char5)<<4^char6)<<4^char7)<<4^char8 )

	class CImage:public CDocument{
		friend class CRideApp;
		friend class CTrackMapView;
		friend class CFileManagerView;

		const bool hasEditableSettings;
		bool writeProtected;

		void Dump() const;
		void Patch();
		void OnCloseDocument() override sealed;
	protected:
		static bool OpenImageForReading(LPCTSTR fileName,CFile *f);
		static bool OpenImageForWriting(LPCTSTR fileName,CFile *f);

		struct TExclusiveLocker sealed{
			const PCImage image;
			TExclusiveLocker(PCImage image);
			~TExclusiveLocker();
		};

		mutable CCriticalSection locker;
		bool canBeModified;

		BOOL DoSave(LPCTSTR lpszPathName,BOOL bReplace) override;
		BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override; // enabling/disabling ToolBar buttons
	public:
		typedef DWORD TId;
		typedef LPCTSTR (*TFnRecognize)(PTCHAR deviceNameList);
		typedef CImage *(*TFnInstantiate)(LPCTSTR deviceName);

		#pragma pack(1)
		typedef const struct TProperties sealed{
			TId id; // container's unique identifier (see other containers to be REALLY unique!)
			TFnRecognize fnRecognize;
			TFnInstantiate fnInstantiate;
			LPCTSTR filter; // filter for the "Open/Save file" dialogs (e.g. "*.d80;*.d40"); ATTENTION - must be all in lowercase (normalization) and the extension must always have right three characters (otherwise changes in DoSave needed)
			TMedium::TType supportedMedia;
			WORD sectorLengthMin,sectorLengthMax;

			bool IsRealDevice() const;
		} *PCProperties;

		class CSectorDataSerializer abstract:public CFile,public CHexaEditor::IContentAdviser{
		protected:
			CHexaEditor *const pParentHexaEditor;
			const PImage image;
			#if _MFC_VER>=0x0A00
			LONGLONG dataTotalLength;
			LONGLONG position;
			#else
			LONG dataTotalLength;
			LONG position;
			#endif
			TTrack currTrack; // Track (inferred from Position) to currently read from or write to
			struct{ // Sector (inferred from Position) to currently read from or write to
				BYTE indexOnTrack; // zero-based index of the Sector on the Track (to distinguish among duplicate-ID Sectors)
				WORD offset;
			} sector;

			CSectorDataSerializer(CHexaEditor *pParentHexaEditor,PImage image,LONG dataTotalLength);
		public:
			#if _MFC_VER>=0x0A00
			ULONGLONG GetLength() const override sealed;
			void SetLength(ULONGLONG dwNewLen) override sealed;
			ULONGLONG GetPosition() const override sealed;
			ULONGLONG Seek(LONGLONG lOff,UINT nFrom) override;
			#else
			DWORD GetLength() const override sealed;
			void SetLength(DWORD dwNewLen) override sealed;
			DWORD GetPosition() const override sealed;
			LONG Seek(LONG lOff,UINT nFrom) override;
			#endif
			UINT Read(LPVOID lpBuf,UINT nCount) override sealed;
			void Write(LPCVOID lpBuf,UINT nCount) override sealed;
			BYTE GetCurrentSectorIndexOnTrack() const;
			virtual TPhysicalAddress GetCurrentPhysicalAddress() const=0;
			virtual DWORD GetSectorStartPosition(RCPhysicalAddress chs,BYTE nSectorsToSkip) const=0;
		};

		static CPtrList known; // list of known Images (registered in CRideApp::InitInstance)
		static CPtrList devices; // list of known Devices (registered in CRideApp::InitInstance)

		static CImage *GetActive();
		static PCProperties DetermineType(LPCTSTR fileName);
		static BYTE PopulateComboBoxWithCompatibleMedia(HWND hComboBox,WORD dosSupportedMedia,PCProperties imageProperties);
		static TFormat::TLengthCode GetSectorLengthCode(WORD sectorLength);
		static WORD GetOfficialSectorLength(BYTE sectorLengthCode);

		const PCProperties properties;
		CMainWindow::CDockableToolBar toolbar;
		PDos dos;

		CImage(PCProperties _properties,bool hasEditableSettings);
		~CImage();

		bool IsWriteProtected() const;
		bool CanBeModified() const;
		virtual TCylinder GetCylinderCount() const=0;
		virtual THead GetNumberOfFormattedSides(TCylinder cyl) const=0;
		TTrack GetTrackCount() const;
		virtual TSector ScanTrack(TCylinder cyl,THead head,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PINT startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const=0;
		virtual int EstimateNanosecondsPerOneByte() const;
		bool IsTrackHealthy(TCylinder cyl,THead head);
		virtual void GetTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses)=0;
		void BufferTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors);
		PSectorData GetSectorData(TCylinder cyl,THead head,PCSectorId pid,BYTE nSectorsToSkip,bool silentlyRecoverFromError,PWORD sectorLength,TFdcStatus *pFdcStatus);
		PSectorData GetSectorData(RCPhysicalAddress chs,BYTE nSectorsToSkip,bool silentlyRecoverFromError,PWORD sectorLength,TFdcStatus *pFdcStatus);
		PSectorData GetHealthySectorData(TCylinder cyl,THead head,PCSectorId pid,PWORD sectorLength);
		PSectorData GetHealthySectorData(RCPhysicalAddress chs,PWORD sectorLength);
		PSectorData GetHealthySectorData(RCPhysicalAddress chs);
		PSectorData GetHealthySectorDataOfUnknownLength(TPhysicalAddress &rChs,PWORD sectorLength);
		virtual TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus)=0;
		void MarkSectorAsDirty(RCPhysicalAddress chs);
		virtual TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber);
		virtual bool EditSettings(bool initialEditing)=0;
		virtual TStdWinError Reset()=0;
		virtual TStdWinError SaveTrack(TCylinder cyl,THead head);
		virtual TStdWinError FormatTrack(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte)=0;
		virtual bool RequiresFormattedTracksVerification() const;
		virtual TStdWinError PresumeHealthyTrackStructure(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,BYTE gap3,BYTE fillerByte);
		virtual TStdWinError UnformatTrack(TCylinder cyl,THead head)=0;
		virtual std::unique_ptr<CSectorDataSerializer> CreateSectorDataSerializer(CHexaEditor *pParentHexaEditor)=0;
		bool ReportWriteProtection() const;
		void ToggleWriteProtection();
		void SetPathName(LPCTSTR lpszPathName,BOOL bAddToMRU=TRUE) override;
		BOOL CanCloseFrame(CFrameWnd* pFrame) override;
	};

	#define EXCLUSIVELY_LOCK_THIS_IMAGE()	const TExclusiveLocker locker(this)

#endif // IMAGE_H
