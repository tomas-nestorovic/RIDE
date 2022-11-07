#ifndef FDD_H
#define FDD_H

	class CFDD sealed:public CFloppyImage{
	public:
		enum TSupportedDriver{
			DRV_AUTO	=0,
			DRV_FDRAWCMD=1
		};
	private:
		TCHAR devicePatternName[DEVICE_NAME_CHARS_MAX];
		#pragma pack(1)
		struct TParams sealed{
			enum TCalibrationAfterError{
				NONE				=0,
				ONCE_PER_CYLINDER	=1,
				FOR_EACH_SECTOR		=2
			} calibrationAfterError;
			bool calibrationAfterErrorOnlyForKnownSectors;
			BYTE calibrationStepDuringFormatting, nSecondsToTurnMotorOff;
			bool verifyFormattedTracks, verifyWrittenData;

			TParams(); //ctor
			~TParams(); //dtor
		} params;

		enum TDataAddressMark:BYTE{
			DATA_RECORD				=0xfb,
			DELETED_DATA_RECORD		=0xf8,
			DEFECTIVE_DATA_RECORD	=0xd8
		};

		#pragma pack(1)
		typedef struct TInternalTrack sealed{
			const TCylinder cylinder;
			const THead head;
			const Codec::TType codec;
			const TSector nSectors;
			#pragma pack(1)
			struct TSectorInfo sealed{
				BYTE seqNum; // "zero-based" sequence number of this Sector on containing Track
				TSectorId id;
				WORD length;
				TLogTime startNanoseconds,endNanoseconds; // counted from the index pulse
				struct{
					PSectorData data;
					TFdcStatus fdcStatus;

					inline bool HasDataReady() const{ return data!=nullptr || fdcStatus.DescribesMissingDam(); }
					inline bool HasGoodDataReady() const{ return data!=nullptr && fdcStatus.IsWithoutError(); }
				} revolutions[Revolution::MAX]; // a First-In-First-Out buffer of Revolutions
				BYTE nRevolutions;
				BYTE currentRevolution;
				Revolution::TType dirtyRevolution;

				inline bool IsModified() const{ return dirtyRevolution!=Revolution::NONE; }
				TStdWinError SaveToDisk(CFDD *fdd,const TInternalTrack *pit,BYTE nSectorsToSkip,bool verify,const volatile bool &cancelled);
				BYTE VerifySaving(const CFDD *fdd,const TInternalTrack *pit,BYTE nSectorsToSkip);
			};
			const Utils::CCallocPtr<TSectorInfo> sectors;

			TInternalTrack(const CFDD *fdd,TCylinder cyl,THead head,Codec::TType codec,TSector _nSectors,PCSectorId bufferId,PCLogTime sectorStartsNanoseconds); //ctor
			~TInternalTrack(); //dtor

			bool __isIdDuplicated__(PCSectorId id) const;
		} *PInternalTrack;
		typedef const TInternalTrack *PCInternalTrack;

		static UINT AFX_CDECL __save_thread__(PVOID _pCancelableAction);
		static UINT AFX_CDECL FindHealthyTrack_thread(PVOID pCancelableAction);
		static UINT AFX_CDECL DetermineControllerAndOneByteLatency_thread(PVOID pCancelableAction);
		static UINT AFX_CDECL DetermineGap3Latency_thread(PVOID pCancelableAction);
		static UINT AFX_CDECL __formatTracks_thread__(PVOID _pCancelableAction);

		const PVOID dataBuffer; // virtual memory (VirtualAlloc)
		#pragma pack(1)
		struct TFddHead sealed{
			HANDLE handle;
			TSupportedDriver driver;
			bool mutable calibrated, preferRelativeSeeking;
			bool doubleTrackStep, userForcedDoubleTrackStep;
			TCylinder mutable position;
			struct TProfile sealed{
				TLogTime controllerLatency,oneByteLatency,gap3Latency; // in nanoseconds
				
				TProfile();

				bool Load(TCHAR driveLetter,Medium::TType floppyType,TLogTime defaultNanosecondsPerByte);
				void Save(TCHAR driveLetter,Medium::TType floppyType) const;
			} profile;

			TFddHead(); //ctor
			~TFddHead(); //dtor

			bool __seekTo__(TCylinder cyl) const;
			bool __calibrate__();
		} fddHead;
		mutable PInternalTrack internalTracks[FDD_CYLINDERS_MAX][2]; // 2 = a floppy can have two Sides

		TStdWinError __connectToFloppyDrive__(TSupportedDriver _driver);
		void __disconnectFromFloppyDrive__();
		TCHAR GetDriveLetter() const;
		TStdWinError __reset__();
		bool IsFloppyInserted() const;
		TStdWinError SetDataTransferSpeed(Medium::TType floppyType) const;
		void __setSecondsBeforeTurningMotorOff__(BYTE nSeconds) const;
		LPCTSTR __getControllerType__() const;
		PInternalTrack __getScannedTrack__(TCylinder cyl,THead head) const;
		PInternalTrack __scanTrack__(TCylinder cyl,THead head);
		void __setWaitingForIndex__() const;
		TLogTime GetAvgIndexDistance() const;
		void __setNumberOfSectorsToSkipOnCurrentTrack__(BYTE _nSectorsToSkip) const;
		TStdWinError __setTimeBeforeInterruptingTheFdc__(WORD nDataBytesBeforeInterruption,TLogTime nNanosecondsAfterLastDataByteWritten) const;
		TStdWinError __setTimeBeforeInterruptingTheFdc__(WORD nDataBytesBeforeInterruption) const;
		bool __bufferSectorData__(TCylinder cyl,THead head,PCSectorId psi,WORD sectorLength,const TInternalTrack *pit,BYTE nSectorsToSkip,TFdcStatus *pFdcStatus) const;
		bool __bufferSectorData__(RCPhysicalAddress chs,WORD sectorLength,const TInternalTrack *pit,BYTE nSectorsToSkip,TFdcStatus *pFdcStatus) const;
		TStdWinError FormatToOneLongVerifiedSector(RCPhysicalAddress chs,BYTE fillerByte,const volatile bool &cancelled);
		void UnformatInternalTrack(TCylinder cyl,THead head) const;
		void __freeInternalTracks__();
	public:
		static const TProperties Properties;

		CFDD(LPCTSTR deviceName);
		~CFDD();

		BOOL OnOpenDocument(LPCTSTR) override;
		TCylinder GetCylinderCount() const override;
		THead GetHeadCount() const override;
		BYTE GetAvailableRevolutionCount(TCylinder cyl,THead head) const override;
		TStdWinError SeekHeadsHome() const override;
		TSector ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec=nullptr,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		bool IsTrackScanned(TCylinder cyl,THead head) const override;
		void GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses,TLogTime *outDataStarts) override;
		TDataStatus IsSectorDataReady(TCylinder cyl,THead head,RCSectorId id,BYTE nSectorsToSkip,Revolution::TType rev) const override;
		TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus) override;
		Revolution::TType GetDirtyRevolution(RCPhysicalAddress chs,BYTE nSectorsToSkip) const override;
		TStdWinError GetInsertedMediumType(TCylinder cyl,Medium::TType &rOutMediumType) const override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		TStdWinError Reset() override;
		TStdWinError SaveTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const override;
		TStdWinError FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte,const volatile bool &cancelled) override;
		bool RequiresFormattedTracksVerification() const override;
		TStdWinError PresumeHealthyTrackStructure(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,BYTE gap3,BYTE fillerByte) override;
		TStdWinError UnformatTrack(TCylinder cyl,THead head) override;
		void SetPathName(LPCTSTR lpszPathName,BOOL bAddToMRU=TRUE) override;
	};

#endif // FDD_H
