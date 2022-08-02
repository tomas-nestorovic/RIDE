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
		friend struct TInternalTrack;

		Medium::TType forcedMediumType;
	protected:
		class CBitReader sealed{
		// this class is deprecated and should not be used in new code - use CTrackReaderWriter instead!
			PUBYTE pCurrByte;
			UBYTE currBitMask;
			UDWORD nRemainingBits;
		public:
			const UDWORD Count;

			CBitReader(const CapsTrackInfoT2 &cti,UDWORD lockFlags);
			CBitReader(const CBitReader &rBitReader,UDWORD position);

			inline operator bool() const;

			bool ReadBit();
			bool ReadBits16(WORD &rOut);
			bool ReadBits32(DWORD &rOut);
			UDWORD GetPosition() const;
			void SeekTo(UDWORD pos);
			void SeekToBegin();
		};

		typedef struct TInternalSector sealed{
			TSectorId id;
			struct{
				TLogTime idEndTime;
				CTrackReader::TProfile idEndProfile;
				PSectorData data;
				TFdcStatus fdcStatus;

				inline bool HasDataReady() const{ return data!=nullptr || fdcStatus.DescribesMissingDam(); }
				inline bool HasGoodDataReady() const{ return data!=nullptr && fdcStatus.IsWithoutError(); }
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
			CInternalTrack(const CTrackReaderWriter &trw,PInternalSector sectors,TSector nSectors);
		public:
			static CInternalTrack *CreateFrom(const CCapsBase &cb,const CapsTrackInfoT2 *ctiRevs,BYTE nRevs,UDWORD lockFlags);
			static CInternalTrack *CreateFrom(const CCapsBase &cb,const CTrackReaderWriter &trw);

			const TSector nSectors;
			const Utils::CCallocPtr<TInternalSector> sectors;
			bool modified;
			
			~CInternalTrack();

			void ReadSector(TInternalSector &ris,BYTE rev);
			void FlushSectorBuffers();
		} *PInternalTrack;
		typedef const CInternalTrack *PCInternalTrack;

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
			WORD firstSectorTime:1;
			short firstSectorMicroseconds;

			TCorrections(); // no Corrections
			TCorrections(LPCTSTR iniSection,LPCTSTR iniName=_T("crt"));

			void Save(LPCTSTR iniSection,LPCTSTR iniName=_T("crt")) const;
			bool ShowModal(CWnd *pParentWnd);
			TStdWinError ApplyTo(CTrackReaderWriter &trw) const;
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
			enum TFluxDecoder{ // in order of appearance in corresponding combo-box in IDR_KRYOFLUX_ACCESS dialog
				NO_FLUX_DECODER,
				KEIR_FRASER,
				MARK_OGDEN
			} fluxDecoder;
			bool resetFluxDecoderOnIndex;
			enum TCalibrationAfterError{
				NONE				=0,
				ONCE_PER_CYLINDER	=1,
				FOR_EACH_SECTOR		=2
			} mutable calibrationAfterError;
			bool calibrationAfterErrorOnlyForKnownSectors;
			BYTE calibrationStepDuringFormatting;
			TCorrections corrections;
			bool verifyWrittenTracks;
			// volatile (current session only)
			bool doubleTrackStep, userForcedDoubleTrackStep;

			TParams(LPCTSTR iniSectionName);
			~TParams();

			inline BYTE PrecisionToFullRevolutionCount() const{ return precision; }
			CTrackReader::TDecoderMethod GetGlobalFluxDecoder() const;
			bool EditInModalDialog(CCapsBase &rcb,LPCTSTR firmware,bool initialEditing);
		} params;

		const TStdWinError capsLibLoadingError;
		const SDWORD capsDeviceHandle;
		CapsVersionInfo capsVersionInfo;
		CapsImageInfo capsImageInfo;
		PInternalTrack mutable internalTracks[FDD_CYLINDERS_MAX][2]; // "2" = a floppy can have two Sides

		mutable Codec::TType lastSuccessfullCodec;

		CCapsBase(PCProperties properties,char realDriveLetter,bool hasEditableSettings,LPCTSTR iniSectionName);

		virtual TStdWinError UploadFirmware();
		void DestroyAllTracks();
		TStdWinError VerifyTrack(TCylinder cyl,THead head,const CTrackReaderWriter &trwWritten,bool showDiff,std::unique_ptr<CTrackReaderWriter> *ppOutReadTrack,const volatile bool &cancelled) const;
		TStdWinError DetermineMagneticReliabilityByWriting(Medium::TType floppyType,TCylinder cyl,THead head,const volatile bool &cancelled) const;
	public:
		~CCapsBase();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		TCylinder GetCylinderCount() const override sealed;
		THead GetHeadCount() const override sealed;
		BYTE GetAvailableRevolutionCount(TCylinder cyl,THead head) const override sealed;
		TSector ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec=nullptr,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		bool IsTrackScanned(TCylinder cyl,THead head) const override sealed;
		void GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override sealed;
		TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus) override;
		TDataStatus IsSectorDataReady(TCylinder cyl,THead head,RCSectorId id,BYTE nSectorsToSkip,Revolution::TType rev) const override sealed;
		Revolution::TType GetDirtyRevolution(RCPhysicalAddress chs,BYTE nSectorsToSkip) const override sealed;
		TStdWinError GetInsertedMediumType(TCylinder cyl,Medium::TType &rOutMediumType) const override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		TStdWinError Reset() override;
		CTrackReader ReadTrack(TCylinder cyl,THead head) const override;
		bool RequiresFormattedTracksVerification() const override sealed;
	};


#endif // CAPSBASE_H
