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

	#define FDD_CYLINDERS_HD	80
	#define FDD_CYLINDERS_EXTRA	2
	#define FDD_CYLINDERS_MAX	(FDD_CYLINDERS_HD+FDD_CYLINDERS_EXTRA)
	#define FDD_SECTORS_MAX		64
	#define FDD_350_SECTOR_GAP3	54
	#define FDD_525_SECTOR_GAP3	80
	#define FDD_NANOSECONDS_PER_DD_BYTE	TIME_MICRO(32)

	#define HDD_CYLINDERS_MAX	(TCylinder)-1
	#define HDD_HEADS_MAX		63

	#define DEVICE_NAME_CHARS_MAX 48


	#define MAKE_IMAGE_ID(char1,char2,char3,char4,char5,char6,char7,char8)\
		( (((((((Track::TTypeId)char1<<4^char2)<<4^char3)<<4^char4)<<4^char5)<<4^char6)<<4^char7)<<4^char8 )

	class CImage:public CDocument{
		friend class CRideApp;
		friend class CTrackMapView;
		friend class CFileManagerView;

		void Dump() const;
	protected:
		static DWORD GetCurrentDiskFreeSpace();
		static TStdWinError OpenImageForReading(LPCTSTR fileName,CFile &f);
		static TStdWinError OpenImageForReadingAndWriting(LPCTSTR fileName,CFile &f);
		static TStdWinError CreateImageForReadingAndWriting(LPCTSTR fileName,CFile &f);
		static TSector CountSectorsBelongingToCylinder(TCylinder cylRef,PCSectorId ids,TSector nIds);

		bool writeProtected;
		bool canBeModified; // can remove the WriteProtection? (e.g. can't for CAPS *.IPF images that is always read-only)
		PCSide sideMap; // explicit mapping of Heads to Side numbers (index = Head id, [index] = Side number); may be Null if the container doesn't have such feature (e.g. DSK images)

		BOOL DoSave(LPCTSTR lpszPathName,BOOL bReplace) override;
		BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override; // enabling/disabling ToolBar buttons
		virtual TStdWinError SaveAllModifiedTracks(LPCTSTR lpszPathName,CActionProgress &ap);
	public:
		typedef LPCTSTR (*TFnRecognize)(PTCHAR deviceNameList);
		typedef CImage *(*TFnInstantiate)(LPCTSTR deviceName);

		typedef const struct TSaveThreadParams sealed{
			const PImage image;
			const LPCTSTR lpszPathName;

			inline TSaveThreadParams(PImage image,LPCTSTR lpszPathName)
				: image(image) , lpszPathName(lpszPathName) {
			}
		} &RCSaveThreadParams;

		#pragma pack(1)
		typedef const struct TProperties sealed{
			Track::TTypeId id; // container's unique identifier (see other containers to be REALLY unique!)
			TFnRecognize fnRecognize;
			TFnInstantiate fnInstantiate;
			LPCTSTR filter; // filter for the "Open/Save file" dialogs (e.g. "*.d80;*.d40"); ATTENTION - must be all in lowercase (normalization) and the extension must always have right three characters (otherwise changes in DoSave needed)
			Medium::TType supportedMedia;
			Codec::TType supportedCodecs;
			Sector::L sectorLengthMin,sectorLengthMax;
			bool isReadOnly;

			inline bool IsRealDevice() const{ return filter==nullptr; }
		} *PCProperties;

		class CSettings sealed:public CMapStringToString{
		public:
			void Add(LPCTSTR name,bool value);
			void Add(LPCTSTR name,bool value,bool userForcedValue);
			void Add(LPCTSTR name,int value);
			void Add(LPCTSTR name,LPCSTR value);
			void AddLibrary(LPCTSTR name,int major,int minor=-1);
			void AddRevision(int major,int minor=-1);
			void AddMediumIsForced(bool isForced);
			void AddMediumIsFlippy(bool isFlippy,bool userForced);
			void AddDecaHexa(LPCTSTR name,int value);
			void AddId(int value);
			void AddAuto(LPCTSTR name);
			void AddCylinderCount(TCylinder n);
			void AddHeadCount(THead n);
			void AddRevolutionCount(TRev n);
			void AddSectorCount(TSector n);
			void AddSides(PCSide list,THead n);
			void AddSectorSize(Sector::L nBytes);
			void Add40TrackDrive(bool value);
			void AddDoubleTrackStep(bool isDouble,bool userForced);
			void AddBaudRate(int baudRate);
		};

		CMainWindow::CMessageBar draftVersionMessageBar;

		class CReadOnlyMessageBar sealed:public CMainWindow::CMessageBar{
		public:
			CReadOnlyMessageBar(LPCTSTR readOnlyReason);
			void SetReadOnlyReason(LPCTSTR readOnlyReason);
		} readOnlyMessageBar; // the reason why WriteProtection can't be removed

		class CUnsupportedFeaturesMessageBar sealed:public CMainWindow::CMessageBar{
			CString report;
			void HyperlinkClicked(LPCWSTR id) const override;
		public:
			static CString CreateListItemIfUnsupported(TCylinder nCyls,TCylinder nCylsMax=FDD_CYLINDERS_MAX);
			CUnsupportedFeaturesMessageBar();
			void Show(const CString &report);
		} unsupportedFeaturesMessageBar;

		static Utils::CPtrList<PCProperties> Known; // list of known Images (registered in CRideApp::InitInstance)
		static Utils::CPtrList<PCProperties> Devices; // list of known Devices (registered in CRideApp::InitInstance)

		static CImage *GetActive();
		static PCProperties DetermineType(LPCTSTR fileName);
		static BYTE PopulateComboBoxWithCompatibleMedia(HWND hComboBox,WORD dosSupportedMedia,PCProperties imageProperties);
		static BYTE PopulateComboBoxWithCompatibleCodecs(HWND hComboBox,WORD dosSupportedCodecs,PCProperties imageProperties);
		static void PopulateComboBoxWithSectorLengths(HWND hComboBox);
		static UINT AFX_CDECL SaveAllModifiedTracks_thread(PVOID _pCancelableAction);

		const PCProperties properties;
		const bool hasEditableSettings;
		const CTrackMapView trackMap;
		CMutex destructionLocker;
		mutable CCriticalSection locker;
		CMainWindow::CDockableToolBar toolbar;
		PDos dos;
		CComPtr<IDataObject> dataInClipboard;

		CImage(PCProperties _properties,bool hasEditableSettings);
		~CImage();

		BOOL OnSaveDocument(LPCTSTR lpszPathName) override sealed; // sealed = override CImage::SaveAllModifiedTracks instead
		void OnCloseDocument() override sealed;
		bool IsWriteProtected() const;
		bool CanBeModified() const;
		inline PCSide GetSideMap() const{ return sideMap; }
		virtual TCylinder GetCylinderCount() const=0;
		virtual THead GetHeadCount() const=0;
		THead GetNumberOfFormattedSides(TCylinder cyl) const;
		TTrack GetTrackCount() const;
		virtual TRev GetAvailableRevolutionCount(TCylinder cyl,THead head) const;
		virtual TStdWinError SeekHeadsHome() const;
		virtual TSector ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec=nullptr,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const=0;
		virtual bool IsTrackScanned(TCylinder cyl,THead head) const=0;
		virtual TStdWinError UnscanTrack(TCylinder cyl,THead head);
		CString ListSectors(TCylinder cyl,THead head,TSector iHighlight=-1,char highlightBullet='\0') const;
		bool IsTrackDirty(TCylinder cyl,THead head) const;
		virtual TLogTime EstimateNanosecondsPerOneByte() const;
		TSector GetCountOfHealthySectors(TCylinder cyl,THead head) const;
		bool IsTrackHealthy(TCylinder cyl,THead head) const;
		virtual void GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,PSectorData *outBufferData,PByteInfo *outByteInfos,PWORD outBufferLengths,TFdcStatus *outFdcStatuses,TLogTime *outDataStarts)=0;
		void BufferTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors);
		PSectorData GetSectorData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId pid,BYTE nSectorsToSkip,PWORD pSectorLength=nullptr,TFdcStatus *pFdcStatus=nullptr,TLogTime *outDataStart=nullptr);
		PSectorData GetSectorData(RCPhysicalAddress chs,BYTE nSectorsToSkip,Revolution::TType rev,PWORD pSectorLength=nullptr,TFdcStatus *pFdcStatus=nullptr,TLogTime *outDataStart=nullptr,PByteInfo *outByteInfos=nullptr);
		PSectorData GetHealthySectorData(TCylinder cyl,THead head,PCSectorId pid,PWORD sectorLength=nullptr,BYTE nSectorsToSkip=0);
		PSectorData GetHealthySectorData(RCPhysicalAddress chs,PWORD sectorLength,BYTE nSectorsToSkip=0);
		PSectorData GetHealthySectorData(RCPhysicalAddress chs);
		PSectorData GetHealthySectorDataOfUnknownLength(TPhysicalAddress &rChs,PWORD sectorLength);
		virtual TDataStatus IsSectorDataReady(TCylinder cyl,THead head,RCSectorId id,BYTE nSectorsToSkip,Revolution::TType rev) const=0;
		virtual TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus,bool flush)=0;
		void MarkSectorAsDirty(RCPhysicalAddress chs);
		virtual Revolution::TType GetDirtyRevolution(RCPhysicalAddress chs,BYTE nSectorsToSkip) const;
		virtual TStdWinError GetInsertedMediumType(TCylinder cyl,Medium::TType &rOutMediumType) const;
		virtual TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber);
		virtual bool EditSettings(bool initialEditing)=0;
		virtual void EnumSettings(CSettings &rOut) const;
		virtual TStdWinError Reset()=0;
		virtual TStdWinError SaveTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const;
		virtual CTrackReader ReadTrack(TCylinder cyl,THead head) const;
		virtual TStdWinError WriteTrack(TCylinder cyl,THead head,CTrackReader tr);
		virtual TStdWinError FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte,const volatile bool &cancelled)=0;
		virtual bool RequiresFormattedTracksVerification() const;
		virtual TStdWinError PresumeHealthyTrackStructure(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,BYTE gap3,BYTE fillerByte);
		virtual TStdWinError UnformatTrack(TCylinder cyl,THead head)=0;
		virtual TStdWinError MineTrack(TCylinder cyl,THead head,bool autoStartLastConfig=false);
		virtual Sector::CReaderWriter::CComPtr CreateDiskSerializer(CHexaEditor *pParentHexaEditor)=0;
		virtual TStdWinError CreateUserInterface(HWND hTdi);
		virtual CString ListUnsupportedFeatures() const;
		void SetRedrawToAllViews(bool redraw) const;
		bool ReportWriteProtection() const;
		void ToggleWriteProtection();
		BYTE ShowModalTrackTimingAt(RCPhysicalAddress chs,BYTE nSectorsToSkip,Sector::L positionInSector,Revolution::TType rev);
		void SetPathName(LPCTSTR lpszPathName,BOOL bAddToMRU=TRUE) override;
		BOOL CanCloseFrame(CFrameWnd* pFrame) override;
	};

	#define EXCLUSIVELY_LOCK(rObj)			const Utils::CExclusivelyLocked locker( (rObj).locker )

	#define EXCLUSIVELY_LOCK_IMAGE(rImg)	EXCLUSIVELY_LOCK(rImg)
	#define EXCLUSIVELY_LOCK_THIS_IMAGE()	EXCLUSIVELY_LOCK_IMAGE(*this)

	#define PREVENT_FROM_DESTRUCTION(rObj)	const Utils::CExclusivelyLocked dlocker( (rObj).destructionLocker )

#endif // IMAGE_H
