#ifndef DSK_H
#define DSK_H

	#define DSK_REV5_TRACKS_MAX			204
	#define DSK_TRACKINFO_SECTORS_MAX	29

	class CDsk5 sealed:public CFloppyImage{
		#pragma pack(1)
		typedef struct TSectorInfo sealed{
			BYTE cylinderNumber;
			BYTE sideNumber;
			BYTE sectorNumber;
			BYTE sectorLengthCode;
			TFdcStatus fdcStatus;
			WORD rev5_sectorLength;

			bool operator==(const TSectorId &rSectorId) const;
			bool __hasValidSectorLengthCode__() const;
			inline bool HasGoodDataReady() const{ return fdcStatus.IsWithoutError(); }
		} *PSectorInfo;

		#pragma pack(1)
		typedef struct TTrackInfo sealed{
			char header[12];
			DWORD reserved1;
			BYTE cylinderNumber;
			BYTE headNumber;
			WORD reserved2;
			BYTE std_sectorMaxLengthCode;
			BYTE nSectors;
			BYTE gap3;
			BYTE fillerByte;
			TSectorInfo sectorInfo[DSK_TRACKINFO_SECTORS_MAX];

			bool ReadAndValidate(CFile &f);
		} *PTrackInfo;

		struct TParams sealed{
			bool rev5;
			bool preserveEmptyTracks;

			TParams();
		} params;
		#pragma pack(1)
		struct TDiskInfo sealed{
			char header[34];
			char creator[14];
			BYTE nCylinders;
			BYTE nHeads;
			WORD std_trackLength;
			BYTE rev5_trackOffsets256[DSK_REV5_TRACKS_MAX];

			TDiskInfo(const TParams &rParams);
		} diskInfo;
		PTrackInfo tracks[DSK_REV5_TRACKS_MAX]; // each TrackInfo followed by data of its Sectors
		#ifdef _DEBUG
		struct TSectorDebug sealed{
			bool modified;
			TCrc16 crc16;
		} *tracksDebug[DSK_REV5_TRACKS_MAX]; // debug information on Sectors as they appear on the Track
		#endif

		static PImage Instantiate(LPCTSTR);

		CDsk5(PCProperties properties);

		PTrackInfo __findTrack__(TCylinder cyl,THead head) const;
		WORD __getSectorLength__(const TSectorInfo *si) const;
		WORD __getTrackLength256__(const TTrackInfo *ti) const;
		void __freeAllTracks__();
		TStdWinError SaveAllModifiedTracks(LPCTSTR lpszPathName,CActionProgress &ap) override;
	public:
		static const TProperties Properties;

		~CDsk5();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		TCylinder GetCylinderCount() const override;
		THead GetHeadCount() const override;
		TSector ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec=nullptr,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		bool IsTrackScanned(TCylinder cyl,THead head) const override;
		TStdWinError UnscanTrack(TCylinder cyl,THead head) override;
		void GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,PSectorData *outBufferData,PByteInfo *outByteInfos,PWORD outBufferLengths,TFdcStatus *outFdcStatuses,TLogTime *outDataStarts) override;
		TDataStatus IsSectorDataReady(TCylinder cyl,THead head,RCSectorId id,BYTE nSectorsToSkip,Revolution::TType rev) const override;
		TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus,bool flush) override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		void EnumSettings(CSettings &rOut) const override;
		TStdWinError Reset() override;
		TStdWinError FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte,const volatile bool &cancelled) override;
		TStdWinError UnformatTrack(TCylinder cyl,THead head) override;
	};

#endif // DSK_H
