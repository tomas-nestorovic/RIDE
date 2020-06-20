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
			float controllerLatency,oneByteLatency,gap3Latency;
			enum TCalibrationAfterError{
				NONE				=0,
				ONCE_PER_CYLINDER	=1,
				FOR_EACH_SECTOR		=2
			} calibrationAfterError;
			BYTE calibrationStepDuringFormatting, nSecondsToTurningMotorOff;
			bool verifyFormattedTracks, verifyWrittenData;
			bool readWholeTrackAsFirstSector; // for experimental purposes only - set never to True in Release mode!

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
			const TSector nSectors;
			#pragma pack(1)
			struct TSectorInfo sealed{
				BYTE seqNum; // "zero-based" sequence number of this Sector on containing Track
				TSectorId id;
				WORD length;
				int startMicroseconds,endMicroseconds; // counted from the index pulse
				PSectorData data;
				TFdcStatus fdcStatus;
				bool modified;

				TStdWinError __saveToDisk__(CFDD *fdd,const TInternalTrack *pit,BYTE nSectorsToSkip,bool verify);
				BYTE __verifySaving__(const CFDD *fdd,const TInternalTrack *pit,BYTE nSectorsToSkip);
			} *const sectors;
			#pragma pack(1)
			struct TRawContent sealed{
				static void __generateGap__(PSectorData &buffer,BYTE nBytes_0x4E);
				static BYTE __containsBufferGap__(PCSectorData buffer);
				static void __generateSectorId__(PSectorData &buffer,PCSectorId id,PCFdcStatus pFdcStatus);
				static BYTE __containsBufferSectorId__(PCSectorData buffer,TSectorId *id,bool *crcOk);
				static void __generateSectorDefaultData__(PSectorData &buffer,TDataAddressMark dam,WORD sectorLength,BYTE fillerByte,PCFdcStatus pFdcStatus);

				TSectorId id;
				WORD length128; // multiple of 128
				PSectorData data;
				bool modified;

				TRawContent();
				~TRawContent();
			} rawContent;

			TInternalTrack(const CFDD *fdd,TCylinder cyl,THead head,TSector _nSectors,PCSectorId bufferId,PCINT sectorStartsMicroseconds); //ctor
			~TInternalTrack(); //dtor

			bool __isIdDuplicated__(PCSectorId id) const;
			bool __canRawDumpBeCreated__() const;
			TStdWinError __saveRawContentToDisk__(CFDD *fdd,TCylinder cyl,THead head) const;
		} *PInternalTrack;

		static UINT AFX_CDECL __save_thread__(PVOID _pCancelableAction);
		static UINT AFX_CDECL __determineControllerAndOneByteLatency_thread__(PVOID _pCancelableAction);
		static UINT AFX_CDECL __determineGap3Latency_thread__(PVOID _pCancelableAction);
		static UINT AFX_CDECL __formatTracks_thread__(PVOID _pCancelableAction);

		const PVOID dataBuffer; // virtual memory (VirtualAlloc)
		#pragma pack(1)
		struct TFddHead sealed{
			HANDLE handle;
			TSupportedDriver driver;
			bool calibrated, preferRelativeSeeking;
			bool doubleTrackStep, userForcedDoubleTrackStep;
			TCylinder position;

			TFddHead(); //ctor
			~TFddHead(); //dtor

			bool SeekHome();
			bool __seekTo__(TCylinder cyl);
			bool __calibrate__();
		} fddHead;
		PInternalTrack internalTracks[FDD_CYLINDERS_MAX*2]; // "*2" = a floppy can have two Sides

		TStdWinError __connectToFloppyDrive__(TSupportedDriver _driver);
		void __disconnectFromFloppyDrive__();
		TStdWinError __reset__();
		bool __isFloppyInserted__() const;
		TStdWinError __setDataTransferSpeed__(TMedium::TType _floppyType);
		TStdWinError __setAndEvaluateDataTransferSpeed__(TMedium::TType _floppyType);
		void __setSecondsBeforeTurningMotorOff__(BYTE nSeconds) const;
		LPCTSTR __getControllerType__() const;
		bool __showSettingDialog__();
		BYTE __getMaximumSectorLengthCode__() const;
		PInternalTrack __getScannedTrack__(TCylinder cyl,THead head) const;
		PInternalTrack __scanTrack__(TCylinder cyl,THead head);
		void __setWaitingForIndex__() const;
		void __setNumberOfSectorsToSkipOnCurrentTrack__(BYTE _nSectorsToSkip) const;
		TStdWinError __setTimeBeforeInterruptingTheFdc__(WORD nDataBytesBeforeInterruption,WORD nMicrosecondsAfterLastDataByteWritten) const;
		TStdWinError __setTimeBeforeInterruptingTheFdc__(WORD nDataBytesBeforeInterruption) const;
		bool __bufferSectorData__(TCylinder cyl,THead head,PCSectorId psi,WORD sectorLength,const TInternalTrack *pit,BYTE nSectorsToSkip,TFdcStatus *pFdcStatus) const;
		bool __bufferSectorData__(RCPhysicalAddress chs,WORD sectorLength,const TInternalTrack *pit,BYTE nSectorsToSkip,TFdcStatus *pFdcStatus) const;
		TStdWinError __formatToOneLongVerifiedSector__(RCPhysicalAddress chs,BYTE fillerByte);
		void __unformatInternalTrack__(TCylinder cyl,THead head);
		void __freeInternalTracks__();
	public:
		static const TProperties Properties;

		CFDD(LPCTSTR deviceName);
		~CFDD();

		BOOL OnOpenDocument(LPCTSTR) override;
		BOOL OnSaveDocument(LPCTSTR) override;
		TCylinder GetCylinderCount() const override;
		THead GetNumberOfFormattedSides(TCylinder cyl) const override;
		TSector ScanTrack(TCylinder cyl,THead head,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PINT startTimesMicroseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		void GetTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override;
		TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus) override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		void EditSettings() override;
		TStdWinError Reset() override;
		TStdWinError SaveTrack(TCylinder cyl,THead head) override;
		TStdWinError FormatTrack(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte) override;
		bool RequiresFormattedTracksVerification() const override;
		TStdWinError PresumeHealthyTrackStructure(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,BYTE gap3,BYTE fillerByte) override;
		TStdWinError UnformatTrack(TCylinder cyl,THead head) override;
		void SetTitle(LPCTSTR) override;
		void SetPathName(LPCTSTR, BOOL = TRUE) override;
	};

#endif // FDD_H
