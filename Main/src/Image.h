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

	namespace Revolution{
		enum TType:BYTE{
			// constants to address one particular Revolution
			R0			=0,
			R1, R2, R3, R4, R5, R6, R7,
			MAX,
			// constants to select Revolution
			CURRENT		=MAX,	// don't calibrate the head, settle with any (already buffered) Data, even erroneous
			NEXT,				// don't calibrate the head, settle with any (newly buffered) Data, even erroneous
			ANY_GOOD,			// do anything that doesn't involve the user to get flawless data
			NONE,
			// constants to describe quantities
			QUANTITY_FIRST,
			INFINITY,
			// the following constants should be ignored by all containers
			ALL_INTERSECTED
		};
	}

	namespace Medium{
		enum TType:BYTE{
			UNKNOWN			=(BYTE)-1,
			FLOPPY_HD_350	=1, // 3.5" HD
			FLOPPY_HD_525	=2, // 5.25" HD in 360 RPM drive
			FLOPPY_HD_ANY	=FLOPPY_HD_350|FLOPPY_HD_525,
			FLOPPY_DD		=4, // 3" DD or 3.5" DD or 5.25" DD in 300 RPM drive
			FLOPPY_DD_525	=8, // 5.25" DD in 360 RPM drive
			FLOPPY_DD_ANY	=FLOPPY_DD|FLOPPY_DD_525,
			FLOPPY_ANY		=FLOPPY_HD_ANY|FLOPPY_DD_ANY,
			HDD_RAW			=16,
			ANY				=HDD_RAW|FLOPPY_ANY
		};

		#pragma pack(1)
		struct TIwProfile{
			TLogTime iwTimeDefault; // inspection window default size
			TLogTime iwTime; // inspection window size; a "1" is expected in its centre
			TLogTime iwTimeMin,iwTimeMax; // inspection window possible time range

			TIwProfile(TLogTime iwTimeDefault,BYTE iwTimeTolerancePercent=0);

			void ClampIwTime();
			inline TLogTime PeekNextIwTime(TLogTime tIwCurr) const{ return tIwCurr+iwTime; }
		};

		#pragma pack(1)
		typedef const struct TProperties sealed{
			static const TProperties FLOPPY_HD_350;
			static const TProperties FLOPPY_HD_525;
			static const TProperties FLOPPY_DD;
			static const TProperties FLOPPY_DD_525;

			LPCTSTR description;
			PropGrid::Integer::TUpDownLimits cylinderRange, headRange, sectorRange; // supported range of Cylinders/Heads/Sectors (min and max)
			BYTE rps; // Revolutions per second
			TLogTime revolutionTime; // single revolution time [nanoseconds]
			TLogTime cellTime; // single recorded data cell time [nanoseconds]
			DWORD nCells; // RevolutionTime/CellTime

			bool IsAcceptableRevolutionTime(TLogTime tRevolutionQueried) const;
			bool IsAcceptableCountOfCells(DWORD nCellsQueried) const;
			inline TIwProfile CreateIwProfile(BYTE iwTimeTolerancePercent=4) const{ return TIwProfile(cellTime,iwTimeTolerancePercent); }
		} *PCProperties;

		LPCTSTR GetDescription(TType mediumType);
		PCProperties GetProperties(TType mediumType);
	}

	namespace Codec{
		typedef BYTE TTypeSet;

		typedef const struct TProperties sealed{
			LPCTSTR description;
			struct{
				BYTE d,k;
			} RLL; // Run-length Limited, https://en.wikipedia.org/wiki/Run-length_limited
		} *PCProperties;

		typedef enum TType:TTypeSet{
			UNKNOWN		=0,
			MFM			=1,
			FM			=2,
			//AMIGA		=4,
			//GCR		=8,
			FLOPPY_IBM	=/*FM|*/MFM,
			FLOPPY_ANY	=FLOPPY_IBM,//|AMIGA|GCR
			ANY			=FLOPPY_ANY
		} *PType;

		PCProperties GetProperties(TType codec);
		LPCTSTR GetDescription(TType codec);
		TType FirstFromMany(TTypeSet set);
	}

	typedef struct TFormat sealed{
		static const TFormat Unknown;

		union{
			Medium::TType supportedMedia;
			Medium::TType mediumType;
		};
		union{
			Codec::TType supportedCodecs;
			Codec::TType codecType;
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
			LENGTHCODE_16384=7,
			LAST
		} sectorLengthCode;
		WORD sectorLength;
		WORD clusterSize; // in Sectors

		inline operator bool() const{ return !operator==(Unknown); }
		bool operator==(const TFormat &fmt2) const;
		DWORD GetCountOfAllSectors() const;
		WORD GetCountOfSectorsPerCylinder() const;
		TTrack GetCountOfAllTracks() const;
	} *PFormat;
	typedef const TFormat *PCFormat,&RCFormat;

	#pragma pack(1)
	typedef struct TSectorId sealed{
		static const TSectorId Invalid;

		static TSector CountAppearances(const TSectorId *ids,TSector nIds,const TSectorId &id);
		static CString List(const TSectorId *ids,TSector nIds,TSector iHighlight=-1,char highlightBullet='\0');

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
	typedef const TSectorId *PCSectorId,&RCSectorId;

	#pragma pack(1)
	typedef const struct TPhysicalAddress{
		static const TPhysicalAddress Invalid;

		inline static TTrack GetTrackNumber(TCylinder cyl,THead head,THead nHeads){ return cyl*nHeads+head; }

		TCylinder cylinder;
		THead head;
		TSectorId sectorId;

		inline operator bool() const{ return *this!=Invalid; }
		bool operator==(const TPhysicalAddress &chs2) const;
		bool operator!=(const TPhysicalAddress &chs2) const;
		TTrack GetTrackNumber() const;
		TTrack GetTrackNumber(THead nHeads) const;
		CString GetTrackIdDesc(THead nHeads=0) const;
		inline void Invalidate(){ *this=Invalid; }
	} &RCPhysicalAddress;

	enum TDataStatus{
		NOT_READY	=0,		// querying data via CImage::GetTrackData may lead to delay (e.g. application freezes if called from main thread)
		READY		=1,		// erroneous data are buffered, there will be no delay in calling CImage::GetTrackData
		READY_HEALTHY=READY|2 // healthy data are buffered, there will be no delay in calling CImage::GetTrackData
	};


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
		static const TFdcStatus Unknown; // e.g. when Sector not yet attempted for reading
		static const TFdcStatus WithoutError;
		static const TFdcStatus SectorNotFound;
		static const TFdcStatus IdFieldCrcError;
		static const TFdcStatus DataFieldCrcError;
		static const TFdcStatus NoDataField;
		static const TFdcStatus DeletedDam;

		union{
			struct{
				BYTE reg1,reg2;
			};
			WORD w;
		};

		inline TFdcStatus() : w(0) {}
		inline TFdcStatus(WORD w) : w(w) {}
		TFdcStatus(BYTE _reg1,BYTE _reg2);

		inline operator WORD() const{ return w; }
		inline bool operator==(const WORD st) const{ return w==st; }

		WORD GetSeverity(WORD mask=-1) const;
		inline void ExtendWith(const WORD st){ w|=st; }
		void GetDescriptionsOfSetBits(LPCTSTR *pDescriptions) const;
		bool IsWithoutError() const;
		bool DescribesIdFieldCrcError() const;
		void CancelIdFieldCrcError();
		bool DescribesDataFieldCrcError() const;
		void CancelDataFieldCrcError();
		bool DescribesDeletedDam() const;
		bool DescribesMissingId() const; // aka. Sector not found
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

		void Dump() const;
		void Patch();
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
		typedef DWORD TId;
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
			TId id; // container's unique identifier (see other containers to be REALLY unique!)
			TFnRecognize fnRecognize;
			TFnInstantiate fnInstantiate;
			LPCTSTR filter; // filter for the "Open/Save file" dialogs (e.g. "*.d80;*.d40"); ATTENTION - must be all in lowercase (normalization) and the extension must always have right three characters (otherwise changes in DoSave needed)
			Medium::TType supportedMedia;
			Codec::TType supportedCodecs;
			WORD sectorLengthMin,sectorLengthMax;
			bool isReadOnly;

			bool IsRealDevice() const;
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
			void AddRevolutionCount(BYTE n);
			void AddSectorCount(TSector n);
			void AddSides(PCSide list,THead n);
			void AddSectorSize(WORD nBytes);
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

		class CTrackReaderBuffers{
		public:
			enum TDecoderMethod:BYTE{
				NONE			=1,
				KEIR_FRASER		=2,
				MARK_OGDEN		=8,
				METADATA		=16, // a hidden decoder to help extract bits from Times tagged with MetaData
				FDD_METHODS		=NONE|KEIR_FRASER|MARK_OGDEN
			};

			typedef const struct TMetaDataItem sealed:public TLogTimeInterval{
				bool isFuzzy;
				int nBits; // 0 = no explicit # of bits, use DPLL algorithm to adjust next IW size

				TMetaDataItem(const TLogTimeInterval &ti); // for clearing MetaData in specified Interval
				TMetaDataItem(const TLogTimeInterval &ti,bool isFuzzy,int nBits);

				inline bool operator<(const TMetaDataItem &r) const{ return tStart<r.tStart; }
				inline bool IsDefault() const{ return nBits<=0; }
				inline TLogTime GetStartTimeSafe() const{ return this?tStart:0; }
				TLogTime GetBitTimeAvg() const;
				TLogTime GetBitTime(int iBit) const;
				int GetBitIndex(TLogTime t) const;
				TLogTimeInterval GetIw(int iBit) const;
				TMetaDataItem Split(TLogTime tAt);
				bool Equals(const TMetaDataItem &r) const;
			} *PCMetaDataItem;

			struct CMetaData:public std::set<TMetaDataItem>{
				inline operator bool() const{ return size()>0; }
				const_iterator GetMetaDataIterator(TLogTime t) const;
				PCMetaDataItem GetMetaDataItem(TLogTime t) const;
				PCMetaDataItem GetFirst() const;
				PCMetaDataItem GetLast() const;
			};
		protected:
			struct TLogTimesInfoData abstract{
				Medium::PCProperties mediumProps;
				bool resetDecoderOnIndex;
				Codec::TType codec;
				TLogTime indexPulses[Revolution::MAX+2]; // "+2" = "+1+1" = "+A+B", A = tail IndexPulse of last possible Revolution, B = terminator
				CMetaData metaData;
				TDecoderMethod defaultDecoder; // when no MetaData available
				struct:public std::shared_ptr<Utils::CCallocPtr<BYTE,DWORD>>{ // this cumbersome construct because of compilation against MFC 4.2
					TId id;
				} rawDeviceData; // valid until Track modified, then disposed

				TLogTimesInfoData(TDecoderMethod defaultDecoder,bool resetDecoderOnIndex);
			};

			typedef class CLogTimesInfo sealed:public TLogTimesInfoData{
				UINT nRefs;
			public:
				const Utils::CCallocPtr<TLogTime,DWORD> logTimes;

				CLogTimesInfo(DWORD nLogTimesMax,TDecoderMethod defaultDecoder,bool resetDecoderOnIndex);

				inline UINT GetRefCount() const{ return nRefs; }
				inline void AddRef(){ ::InterlockedIncrement(&nRefs); }
				bool Release();
			} *PLogTimesInfo;

			PLogTime logTimes; // absolute logical times since the start of recording
			PLogTimesInfo pLogTimesInfo;
			PLogTime indexPulses; // buffer to contain 'Max' full Revolutions
			CMetaData::const_iterator itCurrMetaData;

			CTrackReaderBuffers(PLogTimesInfo pLti);
		public:
			inline const CMetaData &GetMetaData() const{ return pLogTimesInfo->metaData; }
		};

		class CTrackReaderState:public CTrackReaderBuffers{
		public:
			struct TProfile sealed:public Medium::TIwProfile{
				TDecoderMethod method;
				union{
					struct{
						DWORD nConsecutiveZeros;
					} fraserObsolete;
					struct{
						DWORD nConsecutiveZeros;
					} fraser;
					struct{
						bool up;
						BYTE fCnt, aifCnt, adfCnt, pcCnt;
					} ogden;
					struct{
						int iCurrBit;
					} metaData;
				} methodState;

				TProfile(TDecoderMethod method=TDecoderMethod::NONE);
				TProfile(const TMetaDataItem &mdi);

				void Reset();
			};
		protected:
			DWORD iNextTime,nLogTimes;
			BYTE iNextIndexPulse,nIndexPulses;
			TProfile profile;
			TLogTime currentTime;
			BYTE nConsecutiveZerosMax; // # of consecutive zeroes to lose synchronization; e.g. 3 for MFM code
			WORD lastReadBits; // validity flag and bit, e.g. 10b = valid bit '0', 11b = valid bit '1', 0Xb = invalid bit 'X'

			CTrackReaderState(PLogTimesInfo pLti);

			PCMetaDataItem GetCurrentTimeMetaData() const;
			PCMetaDataItem ApplyCurrentTimeMetaData();
			PCMetaDataItem IncrMetaDataIteratorAndApply();
			PCMetaDataItem DecrMetaDataIteratorAndApply();
			PCMetaDataItem FindMetaDataIteratorAndApply();
		public:
			inline const CMetaData::const_iterator &GetCurrentTimeMetaDataIterator() const{ return itCurrMetaData; }
			void SetCodec(Codec::TType codec);
			void SetMediumType(Medium::TType mediumType);
		};

		class CTrackReader:public CTrackReaderState{
		public:
			static LPCTSTR GetDescription(TDecoderMethod dm);

			typedef const struct TParseEvent:public TLogTimeInterval{
				enum TType:BYTE{
					NONE,			// invalid ParseEvent
					SYNC_3BYTES,	// dw
					MARK_1BYTE,		// b
					PREAMBLE,		// dw (length)
					DATA_OK,		// dw (length)
					DATA_BAD,		// dw (length)
					DATA_IN_GAP,	// dw (length)
					CRC_OK,			// dw
					CRC_BAD,		// dw
					NONFORMATTED,	// - (no params)
					FUZZY_OK,		// - (no params); at least one Revolution yields OK data
					FUZZY_BAD,		// - (no params); all Revolutions yield only bad data
					META_STRING,	// lpsz; textual description of a ParseEvent not covered above
					LAST
				} type;
				DWORD size; // length of this ParseEvent in Bytes
				union{
					BYTE b;
					DWORD dw;
					int i;
					char lpszMetaString[sizeof(DWORD)]; // textual description of a ParseEvent event
				};

				static const COLORREF TypeColors[LAST];

				inline TParseEvent(){}
				TParseEvent(TType type,TLogTime tStart,TLogTime tEnd,DWORD data);

				inline bool IsType(TParseEvent::TType typeFrom,TParseEvent::TType typeTo) const{ return typeFrom<=type && type<=typeTo; }
				inline bool IsDataStd() const{ return IsType( DATA_OK, DATA_BAD ); }
				inline bool IsDataAny() const{ return IsType( DATA_OK, DATA_IN_GAP ); }
				inline bool IsCrc() const{ return IsType( CRC_OK, CRC_BAD ); }
				inline bool IsFuzzy() const{ return IsType( FUZZY_OK, FUZZY_BAD ); }
				CString GetDescription() const;
			} *PCParseEvent;

			typedef const struct TMetaStringParseEvent:public TParseEvent{
				static void Create(TParseEvent &buffer,TLogTime tStart,TLogTime tEnd,LPCSTR lpszMetaString);
			} *PCMetaStringParseEvent;

			typedef const struct TDataParseEvent:public TParseEvent{
				typedef const struct TByteInfo sealed{
					BYTE value;
					TLogTime dtStart; // offset against ParseEvent's start
				} *PCByteInfo;

				TSectorId sectorId; // or TSectorId::Invalid
				TByteInfo byteInfos[(WORD)-1]; // derive from this ParseEvent if buffer not sufficient

				TDataParseEvent(const TSectorId &sectorId,TLogTime tStart);

				void Finalize(TLogTime tEnd,DWORD nBytes,TType type=DATA_BAD);
				inline TLogTime GetByteTime(DWORD iByte) const{ return tStart+byteInfos[iByte].dtStart; }
				inline DWORD GetByteCount() const{ return dw; }
				inline bool HasByteInfo() const{ return size>sizeof(TParseEvent); }
			} *PCDataParseEvent;

			struct TParseEventPtr sealed{
				union{
					PCDataParseEvent data;
					PCParseEvent gen; // any other non-customized ParseEvent
				};

				inline TParseEventPtr(PCParseEvent pe){ gen=pe; }
				inline PCParseEvent operator->() const{ return gen; }
			};

			class CParseEventList:private Utils::CCopyList<TParseEvent>{
				DWORD peTypeCounts[TParseEvent::LAST];

				CParseEventList(const CParseEventList &r); //delete
			public:
				typedef std::multimap<TLogTime,PCParseEvent> CLogTiming; // multimap to allow ParseEvents starting concurrently at the same time

				typedef class CIterator:public CLogTiming::const_iterator{
					friend class CParseEventList;
					const CLogTiming &logTimes;
				public:
					CIterator(const CLogTiming &logTimes,const CLogTiming::const_iterator &it);

					inline operator const TParseEvent &() const{ return *(*this)->second; }
					inline operator bool() const{ return *this!=logTimes.cend(); }
					inline CIterator &operator=(const CLogTiming::const_iterator &r){ return __super::operator=(r),*this; }
				} CIteratorByStart, CIteratorByEnd;
			private:
				struct TBinarySearch sealed:public CLogTiming{
					inline TBinarySearch(){}
					inline TBinarySearch(TBinarySearch &&r)
						: CLogTiming( std::move(static_cast<CLogTiming &>(r)) ) {
					}
					CIterator Find(TLogTime tMin,TParseEvent::TType typeFrom,TParseEvent::TType typeTo) const;
				} logStarts, logEnds; // values may not correspond to real ParseEvent timing (hence the prefix "logical") - but the value is always AT LEAST the real Start/End
			public:
				CParseEventList();
				CParseEventList(CParseEventList &&r); // move-ctor

				void Add(const TParseEvent &pe);
				void Add(const CParseEventList &list);
				CIterator GetIterator() const;
				inline DWORD GetCount() const{ return logStarts.size(); }
				inline CIteratorByStart GetFirstByStart() const{ return GetIterator(); }
				CIteratorByStart GetLastByStart() const;
				CIteratorByStart FindByStart(TLogTime tStartMin,TParseEvent::TType typeFrom=TParseEvent::NONE,TParseEvent::TType typeTo=TParseEvent::LAST) const;
				CIteratorByStart FindByStart(TLogTime tStartMin,TParseEvent::TType type) const;
				CIteratorByEnd FindByEnd(TLogTime tEndMin,TParseEvent::TType typeFrom=TParseEvent::NONE,TParseEvent::TType typeTo=TParseEvent::LAST) const;
				CIteratorByEnd FindByEnd(TLogTime tEndMin,TParseEvent::TType type) const;
				inline bool Contains(TParseEvent::TType type) const{ return peTypeCounts[type]>0; }
				bool IntersectsWith(const TLogTimeInterval &ti) const;
				void RemoveConsecutiveBeforeEnd(TLogTime tEndMax);
				TParseEvent::TType GetTypeOfFuzziness(CIterator &itContinue,const TLogTimeInterval &tiFuzzy,TLogTime tTrackEnd) const;
				// 'for each' support
				inline CLogTiming::const_iterator begin() const{ return logStarts.cbegin(); }
				inline CLogTiming::const_iterator end() const{ return logStarts.cend(); }
			};
		protected:
			CTrackReader(PLogTimesInfo pLti,Codec::TType codec);

			WORD ScanFm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,CParseEventList *pOutParseEvents);
			WORD ScanMfm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,CParseEventList *pOutParseEvents);
			TFdcStatus ReadDataFm(const TSectorId &sectorId,WORD nBytesToRead,CParseEventList *pOutParseEvents);
			TFdcStatus ReadDataMfm(const TSectorId &sectorId,WORD nBytesToRead,CParseEventList *pOutParseEvents);
		public:
			typedef const struct TRegion:public TLogTimeInterval{
				COLORREF color;
			} *PCRegion;

			class CBitSequence sealed{
			public:
				typedef const struct TBit sealed{
					union{
						struct{
							BYTE value:1; // recognized 0 or 1 from the underlying low-level timing
							BYTE fuzzy:1; // the Value is likely different in each Revolution
							BYTE cosmeticFuzzy:1; // the Value is not wrong but should be displayed so for cosmetic reasons
							BYTE bad:1; // the Value is potentially wrong due to underlying low-level timing
						};
						BYTE flags;
					};
					TLogTime time;
					int uid; // unique identifier (unused by default, set by caller)

					inline bool operator==(const TBit &r) const{ return value==r.value; }
					inline TLogTime GetLength() const{ return this[1].time-time; }
				} *PCBit;
			private:
				Utils::CCallocPtr<TBit> bitBuffer;
				TBit *pBits;
				int nBits;
			public:
				CBitSequence(CTrackReader tr,TLogTime tFrom,const CTrackReader::TProfile &profileFrom, TLogTime tTo,BYTE oneOkPercent=0);

				inline TBit &operator[](int i) const{ ASSERT(0<=i&&i<nBits); return pBits[i]; }
				inline int GetBitCount() const{ return nBits; }
				PCBit Find(TLogTime t) const;
				Utils::CCallocPtr<CDiffBase::TScriptItem> GetShortestEditScript(const CBitSequence &theirs,CActionProgress &ap) const;
				void ScriptToLocalDiffs(const CDiffBase::TScriptItem *pScript,int nScriptItems,TRegion *pOutDiffs) const;
				DWORD ScriptToLocalRegions(const CDiffBase::TScriptItem *pScript,int nScriptItems,TRegion *pOutRegions,COLORREF regionColor) const;
				void InheritFlagsFrom(const CBitSequence &theirs,const CDiffBase::TScriptItem *pScript,DWORD nScriptItems) const;
				void OffsetAll(TLogTime dt) const;
			#ifdef _DEBUG
				void SaveCsv(LPCTSTR filename) const;
			#endif
				// 'for each' support
				inline TBit *begin() const{ return pBits; }
				inline TBit *end() const{ return pBits+nBits; }
			};

			template<char N,typename T>
			bool ReadBits(T &rOut){
				// True <=> all N bits successfully read, otherwise False
				static_assert( N>1, "" );
				for( char n=N; n>0; n-- ){
					if (!*this)
						return false;
					rOut=(rOut<<1)|(T)ReadBit();
				}
				return true;
			}

			CTrackReader(const CTrackReader &tr);
			CTrackReader(CTrackReader &&rTrackReader);
			~CTrackReader();

			inline
			operator bool() const{
				// True <=> not all LogicalTimes yet read, otherwise False
				return iNextTime<nLogTimes;
			}

			inline
			BYTE GetIndexCount() const{
				// returns the number of IndexPulses recorded
				return nIndexPulses;
			}

			inline
			DWORD GetTimesCount() const{
				// returns the number of recorded LogicalTimes
				return nLogTimes;
			}

			inline
			TLogTime GetCurrentTime() const{
				// returns the LogicalTime ellapsed from the beginning of recording
				return currentTime;
			}

			inline
			const TProfile &GetCurrentProfile() const{
				// returns current read Profile
				return profile;
			}

			inline
			TLogTime RewindToIndex(BYTE index){
				// navigates back to the first Flux found just after the index pulse
				SetCurrentTime( GetIndexTime(index) );
				return GetCurrentTime();
			}

			inline
			TLogTime RewindToIndexAndResetProfile(BYTE index){
				// navigates back to the first Flux found just after the index pulse
				profile.Reset();
				return RewindToIndex( index );
			}

			inline
			Codec::TType GetCodec() const{
				// returns currently used/recognized Codec
				return pLogTimesInfo->codec;
			}


			void SetCurrentTime(TLogTime logTime);
			void SetCurrentTimeAndProfile(TLogTime logTime,const TProfile &profile);
			TProfile CreateResetProfile() const;
			TLogTime TruncateCurrentTime();
			TLogTime GetIndexTime(BYTE index) const;
			TLogTime GetLastIndexTime() const;
			TLogTime GetAvgIndexDistance() const;
			TLogTime GetLastTime() const;
			TLogTime GetTotalTime() const;
			PCBYTE GetRawDeviceData(TId dataId,DWORD &outLength) const;
			TLogTime ReadTime();
			bool ReadBit(TLogTime &rtOutOne);
			bool ReadBit();
			bool IsLastReadBitHealthy() const;
			char ReadBits8(BYTE &rOut);
			bool ReadBits15(WORD &rOut);
			bool ReadBits16(WORD &rOut);
			bool ReadBits32(DWORD &rOut);
			char ReadByte(WORD &rOutBits,PBYTE pOutValue=nullptr);
			WORD Scan(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,CParseEventList *pOutParseEvents=nullptr);
			WORD ScanAndAnalyze(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,PLogTime pOutDataEnds,CParseEventList &rOutParseEvents,CActionProgress &ap,bool fullAnalysis=true);
			WORD ScanAndAnalyze(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,PLogTime pOutDataEnds,CParseEventList &rOutParseEvents,CActionProgress &ap,bool fullAnalysis=true);
			CParseEventList ScanAndAnalyze(CActionProgress &ap,bool fullAnalysis=true);
			TFdcStatus ReadData(const TSectorId &id,TLogTime idEndTime,const TProfile &idEndProfile,WORD nBytesToRead,CParseEventList *pOutParseEvents);
			TFdcStatus ReadData(const TSectorId &id,TLogTime idEndTime,const TProfile &idEndProfile,WORD nBytesToRead,LPBYTE buffer);
			BYTE __cdecl ShowModal(PCRegion pRegions,DWORD nRegions,UINT messageBoxButtons,bool initAllFeaturesOn,TLogTime tScrollTo,LPCTSTR format,...) const;
			void __cdecl ShowModal(LPCTSTR format,...) const;
		//#ifdef _DEBUG
			void SaveCsv(LPCTSTR filename) const;
			void SaveDeltaCsv(LPCTSTR filename) const;
		//#endif
		};

		class CTrackReaderWriter:public CTrackReader{
			bool WriteBits(const bool *bits,DWORD nBits);
			WORD WriteDataFm(WORD nBytesToWrite,PCBYTE buffer,TFdcStatus sr);
			WORD WriteDataMfm(WORD nBytesToWrite,PCBYTE buffer,TFdcStatus sr);
		public:
			static const CTrackReaderWriter Invalid;

			CTrackReaderWriter(DWORD nLogTimesMax,TDecoderMethod method,bool resetDecoderOnIndex);
			CTrackReaderWriter(DWORD nLogTimes,Medium::TType mediumType); // 'nLogTimes' uniformly distributed across a single-Revolution Track
			CTrackReaderWriter(const CTrackReaderWriter &trw,bool shareTimes=true);
			CTrackReaderWriter(CTrackReaderWriter &&rTrackReaderWriter);
			CTrackReaderWriter(const CTrackReader &tr);

			inline
			PLogTime GetBuffer() const{
				// returns the inner buffer
				return logTimes;
			}

			DWORD GetBufferCapacity() const;
			void AddTime(TLogTime logTime);
			void AddTimes(PCLogTime logTimes,DWORD nLogTimes);
			void AddIndexTime(TLogTime logTime);
			void AddMetaData(const TMetaDataItem &mdi);
			void SetRawDeviceData(TId dataId,Utils::CCallocPtr<BYTE,DWORD> &&data);
			void TrimToTimesCount(DWORD nKeptLogTimes);
			void ClearMetaData(TLogTime a,TLogTime z);
			void ClearAllMetaData();
			WORD WriteData(TLogTime idEndTime,const TProfile &idEndProfile,WORD nBytesToWrite,PCBYTE buffer,TFdcStatus sr);
			TStdWinError Normalize();
			TStdWinError NormalizeEx(TLogTime indicesOffset,bool fitTimesIntoIwMiddles,bool correctCellCountPerRevolution,bool correctRevolutionTime);
			CTrackReaderWriter &Reverse();
		};

		class CSectorDataSerializer abstract:public CHexaEditor::CYahelStreamFile,public Yahel::Stream::IAdvisor{
		protected:
			CHexaEditor *const pParentHexaEditor;
			const PImage image;
			TTrack currTrack; // Track (inferred from Position) to currently read from or write to
			Revolution::TType revolution;
			struct{ // Sector (inferred from Position) to currently read from or write to
				BYTE indexOnTrack; // zero-based index of the Sector on the Track (to distinguish among duplicate-ID Sectors)
				WORD offset;
			} sector;

			CSectorDataSerializer(CHexaEditor *pParentHexaEditor,PImage image,LONG dataTotalLength,const BYTE &nDiscoveredRevolutions);
		public:
			enum TScannerStatus:BYTE{
				RUNNING, // Track scanner exists and is running (e.g. parallel thread that scans Tracks on real FDD)
				PAUSED, // Track scanner exists but is suspended (same example as above)
				UNAVAILABLE // Track scanner doesn't exist (e.g. a CImageRaw descendant)
			};

			const BYTE &nDiscoveredRevolutions;

			// CFile methods
			UINT Read(LPVOID lpBuf,UINT nCount) override sealed;
			void Write(LPCVOID lpBuf,UINT nCount) override sealed;

			// IStream methods
			HRESULT STDMETHODCALLTYPE Clone(IStream **ppstm) override sealed;

			// other
			BYTE GetCurrentSectorIndexOnTrack() const;
			inline WORD GetPositionInCurrentSector() const{ return sector.offset; }
			BYTE GetAvailableRevolutionCount(TCylinder cyl,THead head) const;
			virtual void SetCurrentRevolution(Revolution::TType rev)=0;
			virtual TPhysicalAddress GetCurrentPhysicalAddress() const=0;
			virtual DWORD GetSectorStartPosition(RCPhysicalAddress chs,BYTE nSectorsToSkip) const=0;
			virtual TScannerStatus GetTrackScannerStatus(PCylinder pnOutScannedCyls=nullptr) const=0;
			virtual void SetTrackScannerStatus(TScannerStatus status)=0;
		};

		static Utils::CPtrList<PCProperties> Known; // list of known Images (registered in CRideApp::InitInstance)
		static Utils::CPtrList<PCProperties> Devices; // list of known Devices (registered in CRideApp::InitInstance)

		static CImage *GetActive();
		static PCProperties DetermineType(LPCTSTR fileName);
		static BYTE PopulateComboBoxWithCompatibleMedia(HWND hComboBox,WORD dosSupportedMedia,PCProperties imageProperties);
		static BYTE PopulateComboBoxWithCompatibleCodecs(HWND hComboBox,WORD dosSupportedCodecs,PCProperties imageProperties);
		static void PopulateComboBoxWithSectorLengths(HWND hComboBox);
		static TFormat::TLengthCode GetSectorLengthCode(WORD sectorLength);
		static WORD GetOfficialSectorLength(BYTE sectorLengthCode);
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
		virtual BYTE GetAvailableRevolutionCount(TCylinder cyl,THead head) const;
		virtual TStdWinError SeekHeadsHome() const;
		virtual TSector ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec=nullptr,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const=0;
		virtual bool IsTrackScanned(TCylinder cyl,THead head) const=0;
		virtual TStdWinError UnscanTrack(TCylinder cyl,THead head);
		CString ListSectors(TCylinder cyl,THead head,TSector iHighlight=-1,char highlightBullet='\0') const;
		bool IsTrackDirty(TCylinder cyl,THead head) const;
		virtual TLogTime EstimateNanosecondsPerOneByte() const;
		TSector GetCountOfHealthySectors(TCylinder cyl,THead head) const;
		bool IsTrackHealthy(TCylinder cyl,THead head) const;
		virtual void GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses,TLogTime *outDataStarts)=0;
		void BufferTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors);
		PSectorData GetSectorData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId pid,BYTE nSectorsToSkip,PWORD pSectorLength=nullptr,TFdcStatus *pFdcStatus=nullptr,TLogTime *outDataStart=nullptr);
		PSectorData GetSectorData(RCPhysicalAddress chs,BYTE nSectorsToSkip,Revolution::TType rev,PWORD pSectorLength=nullptr,TFdcStatus *pFdcStatus=nullptr,TLogTime *outDataStart=nullptr);
		PSectorData GetHealthySectorData(TCylinder cyl,THead head,PCSectorId pid,PWORD sectorLength=nullptr,BYTE nSectorsToSkip=0);
		PSectorData GetHealthySectorData(RCPhysicalAddress chs,PWORD sectorLength,BYTE nSectorsToSkip=0);
		PSectorData GetHealthySectorData(RCPhysicalAddress chs);
		PSectorData GetHealthySectorDataOfUnknownLength(TPhysicalAddress &rChs,PWORD sectorLength);
		virtual TDataStatus IsSectorDataReady(TCylinder cyl,THead head,RCSectorId id,BYTE nSectorsToSkip,Revolution::TType rev) const=0;
		virtual TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus)=0;
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
		virtual CSectorDataSerializer *CreateSectorDataSerializer(CHexaEditor *pParentHexaEditor)=0;
		virtual TStdWinError CreateUserInterface(HWND hTdi);
		virtual CString ListUnsupportedFeatures() const;
		void SetRedrawToAllViews(bool redraw) const;
		bool ReportWriteProtection() const;
		void ToggleWriteProtection();
		BYTE ShowModalTrackTimingAt(RCPhysicalAddress chs,BYTE nSectorsToSkip,WORD positionInSector,Revolution::TType rev);
		void SetPathName(LPCTSTR lpszPathName,BOOL bAddToMRU=TRUE) override;
		BOOL CanCloseFrame(CFrameWnd* pFrame) override;
	};

	#define EXCLUSIVELY_LOCK(rObj)			const Utils::CExclusivelyLocked locker( (rObj).locker )

	#define EXCLUSIVELY_LOCK_IMAGE(rImg)	EXCLUSIVELY_LOCK(rImg)
	#define EXCLUSIVELY_LOCK_THIS_IMAGE()	EXCLUSIVELY_LOCK_IMAGE(*this)

	#define PREVENT_FROM_DESTRUCTION(rObj)	const Utils::CExclusivelyLocked dlocker( (rObj).destructionLocker )

#endif // IMAGE_H
