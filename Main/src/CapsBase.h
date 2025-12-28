#ifndef CAPSBASE_H
#define CAPSBASE_H

/*
	The KryoFlux support is heavily inspired, or after rewriting fully adopted,
	from Simon Owen's SamDisk

	Copyright (c) 2002-2020 Simon Owen, https://simonowen.com

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.
*/


	class CCapsBase abstract:public CFloppyImage{
		Medium::TType forcedMediumType;
	protected:
		typedef struct TInternalSector sealed{
			TSectorId id;
			struct{
				TLogTime idEndTime;
				CTrackReader::TProfile idEndProfile;
				CTrackReader::TDataParseEvent *peData;
				TLogTime dataFieldEndTime;
				TFdcStatus fdcStatus;

				inline bool HasDataReady() const{ return peData!=nullptr || fdcStatus.DescribesMissingDam(); }
				inline bool HasGoodDataReady() const{ return peData!=nullptr && fdcStatus.IsWithoutError(); }
			} revolutions[Revolution::MAX];
			BYTE nRevolutions;
			BYTE currentRevolution;
			Revolution::TType dirtyRevolution;

			inline WORD GetOfficialSectorLength() const{
				return CFloppyImage::GetOfficialSectorLength( id.lengthCode );
			}

			TLogTime GetAverageIdEndTime(const CTrackReader &tr) const;
		} *PInternalSector;
		typedef const TInternalSector *PCInternalSector,&RCInternalSector;

		typedef class CInternalTrack sealed:public CTrackReaderWriter{
			CParseEventList peOwner;

			CInternalTrack(const CTrackReaderWriter &trw,PInternalSector sectors,TSector nSectors);
		public:
			static CInternalTrack *CreateFrom(const CCapsBase &cb,const CapsTrackInfoT2 *ctiRevs,BYTE nRevs,UDWORD lockFlags);
			static CInternalTrack *CreateFrom(const CCapsBase &cb,CTrackReaderWriter &&trw,Medium::TType floppyType=Medium::UNKNOWN);

			const Utils::CSharedPodArray<TInternalSector,TSector> sectors;
			bool modified;
			
			void ReadSector(TInternalSector &ris,BYTE rev);
			void FlushSectorBuffers();
		} *PInternalTrack;
		typedef const CInternalTrack *PCInternalTrack;

		class CTrackTempReset:public Utils::CVarTempReset<PInternalTrack>{
		public:
			CTrackTempReset(PInternalTrack &rit,PInternalTrack newTrack=nullptr);
			~CTrackTempReset();
		};

		class CPrecompensation sealed{
			Medium::TType floppyType;

			static UINT AFX_CDECL PrecompensationDetermination_thread(PVOID pCancelableAction);
		public:
			const char driveLetter;
			mutable enum TMethodVersion:BYTE{
				None,
				MethodVersion1,
				MethodVersion2,
				MethodLatest	= MethodVersion2,
				Identity
			} methodVersion;
			union{
				struct{
					double coeffs[2][5]; // coefficients for even (0) and odd (1) fluxes
				} v1;
				struct{
					double coeffs[2][2][5]; // coefficients for both Heads (0/1), and even (0) and odd (1) fluxes
				} v2, latest;
			};

			CPrecompensation(char driveLetter);

			void Load(Medium::TType floppyType);
			void Save() const;
			TStdWinError DetermineUsingLatestMethod(const CCapsBase &cb,BYTE nTrials=4);
			void ShowOrDetermineModal(const CCapsBase &cb);
			TStdWinError ApplyTo(const CCapsBase &cb,TCylinder cyl,THead head,CTrackReaderWriter trw) const;
		} precompensation;

		struct TCorrections sealed{
			WORD valid:1;
			WORD use:1;
			WORD indexTiming:1;
			WORD cellCountPerTrack:1;
			WORD fitFluxesIntoIwMiddles:1;
			WORD offsetIndices:1;
			short indexOffsetMicroseconds;

			TCorrections(); // no Corrections
			TCorrections(LPCTSTR iniSection,LPCTSTR iniName=_T("crt"));

			void Save(LPCTSTR iniSection,LPCTSTR iniName=_T("crt")) const;
			bool ShowModal(CWnd *pParentWnd);
			TStdWinError ApplyTo(CTrackReaderWriter &trw) const;
			void EnumSettings(CSettings &rOut) const;
		};

		struct TParams{
			// persistent (saved and loaded)
			LPCTSTR iniSectionName;
			enum TPrecision:char{
				SINGLE	=1,	// one full Revolution (only for writing verification)
				BASIC	=2,	// two full revolutions
				MEDIUM	=4,	// four full revolutions
				ADVANCED=6,	// six full revolutions
				PRESERVATION=8	// eight full revolutions
			} mutable precision;
			CTrackReader::TDecoderMethod fluxDecoder;
			bool resetFluxDecoderOnIndex;
			bool fortyTrackDrive;
			enum TCalibrationAfterError{
				NONE				=0,
				ONCE_PER_CYLINDER	=1,
				FOR_EACH_SECTOR		=2
			} mutable calibrationAfterError;
			bool calibrationAfterErrorOnlyForKnownSectors;
			BYTE calibrationStepDuringFormatting;
			TCorrections corrections;
			bool verifyWrittenTracks, verifyBadSectors;
			// volatile (current session only)
			bool userForcedMedium;
			bool flippyDisk, userForcedFlippyDisk;
			bool doubleTrackStep, userForcedDoubleTrackStep;

			TParams(LPCTSTR iniSectionName);
			~TParams();

			inline BYTE PrecisionToFullRevolutionCount() const{ return precision; }
			bool EditInModalDialog(CCapsBase &rcb,LPCTSTR firmware,bool initialEditing);
			void EnumSettings(CSettings &rOut,bool isRealDevice) const;
		} params;

		const TStdWinError capsLibLoadingError;
		const SDWORD capsDeviceHandle;
		CapsVersionInfo capsVersionInfo;
		struct:CapsImageInfo{
			UDWORD maxcylinderOrg;
		} capsImageInfo;
		PInternalTrack mutable internalTracks[FDD_CYLINDERS_MAX][2]; // "2" = a floppy can have two Sides
		mutable TCylinder lastCalibratedCylinder;
		bool preservationQuality; // Images intended for preservation (e.g. IPF and others) mustn't be re-normalized during DOS recognition
		bool informedOnPoorPrecompensation;

		mutable Codec::TType lastSuccessfullCodec;

		CCapsBase(PCProperties properties,char realDriveLetter,bool hasEditableSettings,LPCTSTR iniSectionName);

		virtual TStdWinError UploadFirmware();
		virtual TStdWinError UploadTrack(TCylinder cyl,THead head,CTrackReader tr) const;
		PInternalTrack GetInternalTrackSafe(TCylinder cyl,THead head) const;
		PInternalTrack GetModifiedTrackSafe(TCylinder cyl,THead head) const;
		bool AnyTrackModified(TCylinder cyl) const;
		void DestroyAllTracks();
		TStdWinError VerifyTrack(TCylinder cyl,THead head,CTrackReaderWriter trwWritten,bool showDiff,std::unique_ptr<CTrackReaderWriter> *ppOutReadTrack,const volatile bool &cancelled) const;
		TStdWinError DetermineMagneticReliabilityByWriting(Medium::TType floppyType,TCylinder cyl,THead head,const volatile bool &cancelled) const;
		BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override;
	public:
		~CCapsBase();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		TCylinder GetCylinderCount() const override sealed;
		THead GetHeadCount() const override sealed;
		BYTE GetAvailableRevolutionCount(TCylinder cyl,THead head) const override sealed;
		TSector ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec=nullptr,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override sealed; // sealed = override ReadTrack method instead!
		bool IsTrackScanned(TCylinder cyl,THead head) const override sealed;
		TStdWinError UnscanTrack(TCylinder cyl,THead head) override sealed;
		void GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses,TLogTime *outDataStarts) override sealed;
		TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus) override sealed;
		TDataStatus IsSectorDataReady(TCylinder cyl,THead head,RCSectorId id,BYTE nSectorsToSkip,Revolution::TType rev) const override sealed;
		Revolution::TType GetDirtyRevolution(RCPhysicalAddress chs,BYTE nSectorsToSkip) const override sealed;
		TStdWinError GetInsertedMediumType(TCylinder cyl,Medium::TType &rOutMediumType) const override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		void EnumSettings(CSettings &rOut) const override;
		TStdWinError Reset() override;
		TStdWinError SaveTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const override;
		CTrackReader ReadTrack(TCylinder cyl,THead head) const override;
		const CTrackReader &ReadExistingTrackUnsafe(TCylinder cyl,THead head) const;
		TStdWinError WriteTrack(TCylinder cyl,THead head,CTrackReader tr) override;
		TStdWinError FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte,const volatile bool &cancelled) override;
		bool RequiresFormattedTracksVerification() const override sealed;
		TStdWinError UnformatTrack(TCylinder cyl,THead head) override;
		TStdWinError MineTrack(TCylinder cyl,THead head,bool autoStartLastConfig) override sealed;
		CString ListUnsupportedFeatures() const override;
	};


#endif // CAPSBASE_H
