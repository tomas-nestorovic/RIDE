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
	protected:
		class CBitReader sealed{
		// this class is deprecated and should not be used in new code - use CTrackReaderWriter instead!
			PUBYTE pCurrByte;
			UBYTE currBitMask;
			UDWORD nRemainingBits;
		public:
			const UDWORD Count;

			CBitReader(const CapsTrackInfo &cti,UDWORD lockFlags);
			CBitReader(const CapsTrackInfo &cti,UDWORD revolution,UDWORD lockFlags);
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
			} revolutions[DEVICE_REVOLUTIONS_MAX];
			BYTE nRevolutions;
			BYTE currentRevolution;

			TLogTime GetAverageIdEndTime(const CTrackReader &tr) const;
		} *PInternalSector;
		typedef const TInternalSector *PCInternalSector,&RCInternalSector;

		typedef class CInternalTrack sealed:public CTrackReaderWriter{
			CInternalTrack(const CTrackReaderWriter &trw,PInternalSector sectors,TSector nSectors);
		public:
			static CInternalTrack *CreateFrom(const CCapsBase &cb,const CapsTrackInfo &cti,UDWORD lockFlags);
			static CInternalTrack *CreateFrom(const CCapsBase &cb,const CTrackReaderWriter &trw);

			const PInternalSector sectors;
			const TSector nSectors;
			
			~CInternalTrack();

			void ReadSector(TInternalSector &ris);
		} *PInternalTrack;
		typedef const CInternalTrack *PCInternalTrack;

		const TStdWinError capsLibLoadingError;
		const SDWORD capsDeviceHandle;
		CapsVersionInfo capsVersionInfo;
		CapsImageInfo capsImageInfo;
		PInternalTrack mutable internalTracks[FDD_CYLINDERS_MAX][2]; // "2" = a floppy can have two Sides

		mutable Codec::TType lastSuccessfullCodec;

		CCapsBase(PCProperties properties,bool hasEditableSettings);

		void DestroyAllTracks();
	public:
		~CCapsBase();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		//BOOL OnSaveDocument(LPCTSTR lpszPathName) override;
		TCylinder GetCylinderCount() const override;
		THead GetNumberOfFormattedSides(TCylinder cyl) const override;
		TSector ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec=nullptr,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		void GetTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override;
		TStdWinError GetInsertedMediumType(TCylinder cyl,Medium::TType &rOutMediumType) const override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		TStdWinError Reset() override;
		CTrackReader ReadTrack(TCylinder cyl,THead head) const override;
	};


#endif // CAPSBASE_H
