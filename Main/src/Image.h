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

	namespace Revolution{
		enum TType:BYTE{
			R0			=0,
			R1, R2, R3, R4, R5, R6, R7,
			MAX,
			NONE,
			UNKNOWN,
			INFINITY,
			// the following constants should be ignored by all containers
			ANY_GOOD,
			ALL_INTERSECTED
		};
	}

	namespace Medium{
		enum TType:BYTE{
			UNKNOWN			=(BYTE)-1,
			FLOPPY_HD_350	=1, // 3.5" HD
			FLOPPY_HD_525	=2, // 5.25" HD in 360 RPM drive
			FLOPPY_HD_ANY	=FLOPPY_HD_350|FLOPPY_HD_525,
			FLOPPY_DD		=4, // 3.5" DD or 5.25" DD in 300 RPM drive
			FLOPPY_DD_525	=8, // 5.25" DD in 360 RPM drive
			FLOPPY_DD_ANY	=FLOPPY_DD|FLOPPY_DD_525,
			FLOPPY_ANY		=FLOPPY_HD_ANY|FLOPPY_DD_ANY,
			HDD_RAW			=16,
			ANY				=HDD_RAW|FLOPPY_ANY
		};

		#pragma pack(1)
		typedef const struct TProperties sealed{
			static const TProperties FLOPPY_HD_350;
			static const TProperties FLOPPY_HD_525;
			static const TProperties FLOPPY_DD;
			static const TProperties FLOPPY_DD_525;

			LPCTSTR description;
			PropGrid::Integer::TUpDownLimits cylinderRange, headRange, sectorRange; // supported range of Cylinders/Heads/Sectors (min and max)
			TLogTime revolutionTime; // single revolution time [nanoseconds]
			TLogTime cellTime; // single recorded data cell time [nanoseconds]
			DWORD nCells; // RevolutionTime/CellTime
		} *PCProperties;

		LPCTSTR GetDescription(TType mediumType);
		PCProperties GetProperties(TType mediumType);
	}

	namespace Codec{
		typedef BYTE TTypeSet;

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

		LPCTSTR GetDescription(TType codec);
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
		static const TFdcStatus IdFieldCrcError;
		static const TFdcStatus DataFieldCrcError;
		static const TFdcStatus NoDataField;
		static const TFdcStatus DeletedDam;

		BYTE reg1,reg2;

		TFdcStatus();
		TFdcStatus(BYTE _reg1,BYTE _reg2);

		void ExtendWith(TFdcStatus st);
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

		typedef const struct TSaveThreadParams sealed{
			PImage image;
			LPCTSTR lpszPathName;
		} &RCSaveThreadParams;

		mutable CCriticalSection locker;
		bool canBeModified;

		BOOL DoSave(LPCTSTR lpszPathName,BOOL bReplace) override;
		BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override; // enabling/disabling ToolBar buttons
		virtual TStdWinError SaveAllModifiedTracks(LPCTSTR lpszPathName,PBackgroundActionCancelable pAction);
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
			Medium::TType supportedMedia;
			Codec::TType supportedCodecs;
			WORD sectorLengthMin,sectorLengthMax;
			bool isReadOnly;

			bool IsRealDevice() const;
		} *PCProperties;

		class CTrackReader{
		public:
			enum TDecoderMethod:BYTE{
				NONE				=0,
				FDD_KEIR_FRASER		=1,
				FDD_MARK_OGDEN		=2,
				FDD_METHODS			=FDD_KEIR_FRASER|FDD_MARK_OGDEN
			};

			struct TProfile sealed{
				static const TProfile HD;		// 3.5" HD or 5.25" HD in 360 RPM drive
				static const TProfile DD;		// 3.5" DD or 5.25" DD in 300 RPM drive
				static const TProfile DD_525;	// 5.25" DD in 360 RPM drive

				TLogTime iwTimeDefault; // inspection window default size
				TLogTime iwTime; // inspection window size; a "1" is expected in its centre
				TLogTime iwTimeMin,iwTimeMax; // inspection window possible time range
				BYTE adjustmentPercentMax; // percentual "speed" in inspection window adjustment
				union{
					struct{
						DWORD nConsecutiveZeros;
					} fraser;
					struct{
						bool up;
						BYTE fCnt, aifCnt, adfCnt, pcCnt;
					} ogden;
				} method;

				inline TProfile(){}
				TProfile(const Medium::TProperties &floppyProps,BYTE iwTimeTolerancePercent);

				void Reset();
			};

			typedef const struct TParseEvent sealed{
				enum TType:BYTE{
					EMPTY,			// used as terminator in a list of Events
					SYNC_3BYTES,	// dw
					MARK_1BYTE,		// b
					PREAMBLE,		// dw (length)
					DATA_OK,		// dw (length)
					DATA_BAD,		// dw (length)
					CRC_OK,			// dw
					CRC_BAD,		// dw
					CUSTOM,			// lpsz; {Custom..255} Types determine length of this structure in Bytes
					LAST
				} type;
				TLogTime tStart, tEnd;
				union{
					BYTE b;
					DWORD dw;
					char lpszCustom[sizeof(DWORD)]; // description of Custom ParseEvent
				};

				static const TParseEvent Empty;
				static const COLORREF TypeColors[LAST];

				static void WriteCustom(TParseEvent *&buffer,TLogTime tStart,TLogTime tEnd,LPCSTR lpszCustom);

				inline TParseEvent(){}
				TParseEvent(TType type,TLogTime tStart,TLogTime tEnd,DWORD data);

				inline bool IsEmpty() const{ return type==EMPTY; }
				inline BYTE GetSize() const{ return std::max<BYTE>( type, sizeof(TParseEvent) ); }
				const TParseEvent *GetNext() const;
				const TParseEvent *GetLast() const;
			} *PCParseEvent;
		protected:
			const PLogTime logTimes; // absolute logical times since the start of recording
			const TDecoderMethod method;
			const bool resetDecoderOnIndex;
			DWORD iNextTime,nLogTimes;
			TLogTime indexPulses[Revolution::MAX+1];
			BYTE iNextIndexPulse,nIndexPulses;
			TProfile profile;
			TLogTime currentTime;
			Codec::TType codec;
			Medium::TType mediumType;
			BYTE nConsecutiveZerosMax; // # of consecutive zeroes to lose synchronization; e.g. 3 for MFM code

			CTrackReader(PLogTime logTimes,DWORD nLogTimes,PCLogTime indexPulses,BYTE nIndexPulses,Medium::TType mediumType,Codec::TType codec,TDecoderMethod method,bool resetDecoderOnIndex);

			WORD ScanFm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,TParseEvent *&pOutParseEvents);
			WORD ScanMfm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,TParseEvent *&pOutParseEvents);
			TFdcStatus ReadDataFm(WORD nBytesToRead,LPBYTE buffer,TParseEvent *&pOutParseEvents);
			TFdcStatus ReadDataMfm(WORD nBytesToRead,LPBYTE buffer,TParseEvent *&pOutParseEvents);
		public:
			typedef const struct TTimeInterval{
				TLogTime tStart; // inclusive
				TLogTime tEnd; // exclusive
				COLORREF color;
			} *PCTimeInterval;

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
			void RewindToIndex(BYTE index){
				// navigates back to the first Flux found just after the index pulse
				SetCurrentTime( GetIndexTime(index) );
			}

			inline
			void RewindToIndexAndProfile(BYTE index,const TProfile &profile){
				// navigates back to the first Flux found just after the index pulse
				SetCurrentTimeAndProfile( GetIndexTime(index), profile );
			}

			inline
			void RewindToIndexAndResetProfile(BYTE index){
				// navigates back to the first Flux found just after the index pulse
				RewindToIndex( index );
				profile.Reset();
			}

			inline
			Codec::TType GetCodec() const{
				// returns currently used/recognized Codec
				return codec;
			}

			inline
			Medium::TType GetMediumType() const{
				// returns currently used/recognized MediumType
				return mediumType;
			}


			void SetCurrentTime(TLogTime logTime);
			void SetCurrentTimeAndProfile(TLogTime logTime,const TProfile &profile);
			TProfile CreateResetProfile() const;
			void TruncateCurrentTime();
			TLogTime GetIndexTime(BYTE index) const;
			TLogTime GetTotalTime() const;
			TLogTime ReadTime();
			void SetCodec(Codec::TType codec);
			void SetMediumType(Medium::TType mediumType);
			bool ReadBit();
			bool ReadBits15(WORD &rOut);
			bool ReadBits16(WORD &rOut);
			bool ReadBits32(DWORD &rOut);
			WORD Scan(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,TParseEvent *pOutParseEvents=nullptr);
			TFdcStatus ReadData(TLogTime idEndTime,const TProfile &idEndProfile,WORD nBytesToRead,LPBYTE buffer,TParseEvent *pOutParseEvents=nullptr);
			BYTE __cdecl ShowModal(PCTimeInterval pIntervals,DWORD nIntervals,UINT messageBoxButtons,bool initAllFeaturesOn,LPCTSTR format,...) const;
			void __cdecl ShowModal(LPCTSTR format,...) const;
		};

		class CTrackReaderWriter:public CTrackReader{
			const DWORD nLogTimesMax;

			bool WriteBits(const bool *bits,DWORD nBits);
			WORD WriteDataFm(WORD nBytesToWrite,PCBYTE buffer,TFdcStatus sr);
			WORD WriteDataMfm(WORD nBytesToWrite,PCBYTE buffer,TFdcStatus sr);
		public:
			static const CTrackReaderWriter Invalid;

			CTrackReaderWriter(DWORD nLogTimesMax,TDecoderMethod method,bool resetDecoderOnIndex);
			CTrackReaderWriter(const CTrackReaderWriter &rTrackReaderWriter,bool shareTimes=true);
			CTrackReaderWriter(CTrackReaderWriter &&rTrackReaderWriter);
			CTrackReaderWriter(const CTrackReader &tr);

			inline
			PLogTime GetBuffer() const{
				// returns the inner buffer
				return logTimes;
			}

			inline
			void AddTime(TLogTime logTime){
				// appends LogicalTime at the end of the Track
				ASSERT( nLogTimes<nLogTimesMax );
				ASSERT( logTime>=0 );
				logTimes[nLogTimes++]=logTime;
			}

			inline
			void TrimToTimesCount(DWORD nKeptLogTimes){
				// discards some tail LogicalTimes, keeping only specified amount of them
				ASSERT( nKeptLogTimes<=nLogTimes ); // can only shrink
				nLogTimes=nKeptLogTimes;
			}

			void AddTimes(PCLogTime logTimes,DWORD nLogTimes);
			void AddIndexTime(TLogTime logTime);
			WORD WriteData(TLogTime idEndTime,const TProfile &idEndProfile,WORD nBytesToWrite,PCBYTE buffer,TFdcStatus sr);
			bool Normalize();
			TStdWinError NormalizeEx(TLogTime timeOffset,bool fitTimesIntoIwMiddles,bool correctCellCountPerRevolution,bool correctRevolutionTime);
		};

		class CSectorDataSerializer abstract:public CFile,public CHexaEditor::IContentAdviser{
		protected:
			CHexaEditor *const pParentHexaEditor;
			const PImage image;
			const BYTE nAvailableRevolutions;
			#if _MFC_VER>=0x0A00
			LONGLONG dataTotalLength;
			LONGLONG position;
			#else
			LONG dataTotalLength;
			LONG position;
			#endif
			TTrack currTrack; // Track (inferred from Position) to currently read from or write to
			Revolution::TType revolution;
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
			virtual void SetCurrentRevolution(Revolution::TType rev)=0;
			virtual TPhysicalAddress GetCurrentPhysicalAddress() const=0;
			virtual DWORD GetSectorStartPosition(RCPhysicalAddress chs,BYTE nSectorsToSkip) const=0;
		};

		static CPtrList known; // list of known Images (registered in CRideApp::InitInstance)
		static CPtrList devices; // list of known Devices (registered in CRideApp::InitInstance)

		static CImage *GetActive();
		static PCProperties DetermineType(LPCTSTR fileName);
		static BYTE PopulateComboBoxWithCompatibleMedia(HWND hComboBox,WORD dosSupportedMedia,PCProperties imageProperties);
		static BYTE PopulateComboBoxWithCompatibleCodecs(HWND hComboBox,WORD dosSupportedCodecs,PCProperties imageProperties);
		static TFormat::TLengthCode GetSectorLengthCode(WORD sectorLength);
		static WORD GetOfficialSectorLength(BYTE sectorLengthCode);
		static UINT AFX_CDECL SaveAllModifiedTracks_thread(PVOID _pCancelableAction);

		const PCProperties properties;
		CMainWindow::CDockableToolBar toolbar;
		PDos dos;

		CImage(PCProperties _properties,bool hasEditableSettings);
		~CImage();

		BOOL OnSaveDocument(LPCTSTR lpszPathName) override sealed; // sealed = override CImage::SaveAllModifiedTracks instead
		bool IsWriteProtected() const;
		bool CanBeModified() const;
		virtual TCylinder GetCylinderCount() const=0;
		virtual THead GetHeadCount() const=0;
		THead GetNumberOfFormattedSides(TCylinder cyl) const;
		TTrack GetTrackCount() const;
		virtual BYTE GetAvailableRevolutionCount() const;
		virtual TSector ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec=nullptr,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const=0;
		virtual TLogTime EstimateNanosecondsPerOneByte() const;
		TSector GetCountOfHealthySectors(TCylinder cyl,THead head) const;
		bool IsTrackHealthy(TCylinder cyl,THead head) const;
		virtual void GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses)=0;
		void BufferTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors);
		void BufferTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors);
		PSectorData GetSectorData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId pid,BYTE nSectorsToSkip,bool silentlyRecoverFromError,PWORD sectorLength,TFdcStatus *pFdcStatus);
		PSectorData GetSectorData(TCylinder cyl,THead head,PCSectorId pid,BYTE nSectorsToSkip,bool silentlyRecoverFromError,PWORD sectorLength,TFdcStatus *pFdcStatus);
		PSectorData GetSectorData(RCPhysicalAddress chs,Revolution::TType rev,BYTE nSectorsToSkip,bool silentlyRecoverFromError,PWORD sectorLength,TFdcStatus *pFdcStatus);
		PSectorData GetSectorData(RCPhysicalAddress chs,BYTE nSectorsToSkip,bool silentlyRecoverFromError,PWORD sectorLength,TFdcStatus *pFdcStatus);
		PSectorData GetHealthySectorData(TCylinder cyl,THead head,PCSectorId pid,PWORD sectorLength);
		PSectorData GetHealthySectorData(RCPhysicalAddress chs,PWORD sectorLength);
		PSectorData GetHealthySectorData(RCPhysicalAddress chs);
		PSectorData GetHealthySectorDataOfUnknownLength(TPhysicalAddress &rChs,PWORD sectorLength);
		virtual TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus)=0;
		void MarkSectorAsDirty(RCPhysicalAddress chs);
		virtual Revolution::TType GetDirtyRevolution(RCPhysicalAddress chs,BYTE nSectorsToSkip) const;
		virtual TStdWinError GetInsertedMediumType(TCylinder cyl,Medium::TType &rOutMediumType) const;
		virtual TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber);
		virtual bool EditSettings(bool initialEditing)=0;
		virtual TStdWinError Reset()=0;
		virtual TStdWinError SaveTrack(TCylinder cyl,THead head) const;
		virtual CTrackReader ReadTrack(TCylinder cyl,THead head) const;
		virtual TStdWinError WriteTrack(TCylinder cyl,THead head,CTrackReader tr);
		virtual TStdWinError FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte)=0;
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
