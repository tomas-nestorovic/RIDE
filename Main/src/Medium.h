#pragma once

namespace Medium
{
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
	typedef const struct TProperties sealed{
		static const TProperties FLOPPY_HD_350;
		static const TProperties FLOPPY_HD_525;
		static const TProperties FLOPPY_DD;
		static const TProperties FLOPPY_DD_525;

		LPCTSTR description;
		TCylinder nCylindersMax; // max. # of Cylinders
		THead nHeadsMax; // max. # of Heads
		TRev rps; // Revolutions per second
		TLogTime revolutionTime; // single revolution time [nanoseconds]
		TLogTime cellTime; // single recorded data cell time [nanoseconds]
		Bit::N nCells; // RevolutionTime/CellTime

		bool IsAcceptableRevolutionTime(TLogTime tRevolutionQueried) const;
		bool IsAcceptableCountOfCells(Bit::N nCellsQueried) const;
		inline Time::Decoder::TLimits CreateTimeDecoderLimits(BYTE iwTimeTolerancePercent=4) const{ return Time::Decoder::TLimits(cellTime,iwTimeTolerancePercent); }
	} *PCProperties;

	struct TFormat sealed{
		static const TFormat Unknown;

		union{
			TType supportedMedia;
			TType mediumType;
		};
		union{
			Codec::TType supportedCodecs;
			Codec::TType codecType;
		};
		TCylinder nCylinders;
		THead nHeads;
		TSector nSectors;
		Sector::LC sectorLengthCode;
		Sector::L sectorLength;
		TSector clusterSize; // in Sectors

		inline bool operator==(const TFormat &f) const{
			static_assert( __alignof(TFormat)==1, "see 'memcmp' below" );
			return !::memcmp( this, &f, sizeof(*this) );
		}

		inline operator bool() const{ return !operator==(Unknown); }
		inline Track::N GetTrackCount(TCylinder cyl,THead head) const{ return cyl*nHeads+head; }
		inline DWORD GetSectorCount(TCylinder cyl,THead head) const{ return GetTrackCount(cyl,head)*nSectors; }
		DWORD GetCountOfAllSectors() const;
		WORD GetCountOfSectorsPerCylinder() const;
		Track::N GetCountOfAllTracks() const;
	};

	LPCTSTR GetDescription(TType mediumType);
	PCProperties GetProperties(TType mediumType);
}

typedef Medium::TFormat TFormat,*PFormat;
typedef const Medium::TFormat *PCFormat,&RCFormat;

#define MakeFormatEx(medium,codec,nCyls,nHeads,nSectors,sectorLengthCode,sectorLength,clusterSize)\
	{ medium, codec, nCyls, nHeads, nSectors, sectorLengthCode, sectorLength, clusterSize }

#define MakeFormat(medium,codec,nCyls,nHeads,nSectors,sectorLengthCode,sectorLength,clusterSize)\
	MakeFormatEx( Medium::##medium, Codec::##codec, nCyls, nHeads, nSectors, sectorLengthCode, sectorLength, clusterSize )

#define MakeFdMfmFormat(medium,nCyls,nHeads,nSectors,sectorLengthCode,sectorLength,clusterSize)\
	MakeFormat( FLOPPY_##medium, MFM, nCyls, nHeads, nSectors, sectorLengthCode, sectorLength, clusterSize )

#define MakeFdMfmFormat256(medium,nCyls,nHeads,nSectors)\
	MakeFdMfmFormat( medium, nCyls, nHeads, nSectors, Sector::LC_256, 256, 1 )

#define MakeFdMfmFormat512C(medium,nCyls,nHeads,nSectors,clusterSize)\
	MakeFdMfmFormat( medium, nCyls, nHeads, nSectors, Sector::LC_512, 512, clusterSize )

#define MakeFdMfmFormat512(medium,nCyls,nHeads,nSectors)\
	MakeFdMfmFormat512C( medium, nCyls, nHeads, nSectors, 1 )

#define MakeFdMfmFormat1024(medium,nCyls,nHeads,nSectors)\
	MakeFdMfmFormat( medium, nCyls, nHeads, nSectors, Sector::LC_1024, 1024, 1 )
