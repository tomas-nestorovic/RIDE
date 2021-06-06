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

			const PInternalSector sectors;
			const TSector nSectors;
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

			TCorrections(LPCTSTR iniSection,LPCTSTR iniName=_T("crt"));

			void Save(LPCTSTR iniSection,LPCTSTR iniName=_T("crt")) const;
			bool ShowModal(CWnd *pParentWnd);
			TStdWinError ApplyTo(CTrackReaderWriter &trw) const;
		};

		const TStdWinError capsLibLoadingError;
		const SDWORD capsDeviceHandle;
		CapsVersionInfo capsVersionInfo;
		CapsImageInfo capsImageInfo;
		PInternalTrack mutable internalTracks[FDD_CYLINDERS_MAX][2]; // "2" = a floppy can have two Sides

		mutable Codec::TType lastSuccessfullCodec;

		CCapsBase(PCProperties properties,char realDriveLetter,bool hasEditableSettings);

		void DestroyAllTracks();
		TStdWinError VerifyTrack(TCylinder cyl,THead head,const CTrackReaderWriter &trwWritten,bool showDiff) const;
		TStdWinError DetermineMagneticReliabilityByWriting(Medium::TType floppyType,TCylinder cyl,THead head) const;
	public:
		~CCapsBase();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		TCylinder GetCylinderCount() const override;
		THead GetHeadCount() const override;
		BYTE GetAvailableRevolutionCount() const override;
		TSector ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec=nullptr,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		void GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override;
		Revolution::TType GetDirtyRevolution(RCPhysicalAddress chs,BYTE nSectorsToSkip) const override;
		TStdWinError GetInsertedMediumType(TCylinder cyl,Medium::TType &rOutMediumType) const override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		TStdWinError Reset() override;
		CTrackReader ReadTrack(TCylinder cyl,THead head) const override;
	};


#endif // CAPSBASE_H
