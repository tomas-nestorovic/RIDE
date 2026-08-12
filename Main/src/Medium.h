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

	struct TFormatDef sealed{ // "definition"
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

		bool operator==(const TFormatDef &f) const;
	};

	struct TFormat:public Sector::TSameLengthParams{
		TType mediumType;
		Codec::TType codecType;
		TCylinder nCylinders;
		Side::CMap sides;
		TSector clusterSize;

		TFormat(); // initialize to Unknown
		TFormat(const TFormatDef &f);

		inline operator bool() const{ return mediumType!=UNKNOWN; }
		inline void Invalidate(){ mediumType=UNKNOWN; }
		inline Track::N GetTrackCount(TCylinder cyl,THead head) const{ return cyl*sides.length+head; }
		inline DWORD GetSectorCount(TCylinder cyl,THead head) const{ return GetTrackCount(cyl,head)*nSectors; }
		inline Track::N GetCountOfAllTracks() const{ return (Track::N)nCylinders*sides.length; }
		inline WORD GetCountOfSectorsPerCylinder() const{ return (WORD)sides.length*nSectors; }
		inline DWORD GetCountOfAllSectors() const{ return (DWORD)nCylinders*GetCountOfSectorsPerCylinder(); }

		TFormatDef GetDef() const;
	};

	LPCTSTR GetDescription(TType mediumType);
	PCProperties GetProperties(TType mediumType);
}

typedef Medium::TFormat TFormat,*PFormat;
typedef const Medium::TFormat *PCFormat,&RCFormat;

#define DefFormatEx(medium,codec,nCyls,nHeads,nSectors,sectorLengthCode,sectorLength,clusterSize)\
	{ medium, codec, nCyls, nHeads, nSectors, sectorLengthCode, sectorLength, clusterSize }

#define DefFormat(medium,codec,nCyls,nHeads,nSectors,sectorLengthCode,sectorLength,clusterSize)\
	DefFormatEx( Medium::##medium, Codec::##codec, nCyls, nHeads, nSectors, sectorLengthCode, sectorLength, clusterSize )

#define DefFdMfmFormat(medium,nCyls,nHeads,nSectors,sectorLengthCode,sectorLength,clusterSize)\
	DefFormat( FLOPPY_##medium, MFM, nCyls, nHeads, nSectors, sectorLengthCode, sectorLength, clusterSize )

#define DefFdMfmFormat256(medium,nCyls,nHeads,nSectors)\
	DefFdMfmFormat( medium, nCyls, nHeads, nSectors, Sector::LC_256, 256, 1 )

#define DefFdMfmFormat512C(medium,nCyls,nHeads,nSectors,clusterSize)\
	DefFdMfmFormat( medium, nCyls, nHeads, nSectors, Sector::LC_512, 512, clusterSize )

#define DefFdMfmFormat512(medium,nCyls,nHeads,nSectors)\
	DefFdMfmFormat512C( medium, nCyls, nHeads, nSectors, 1 )

#define DefFdMfmFormat1024(medium,nCyls,nHeads,nSectors)\
	DefFdMfmFormat( medium, nCyls, nHeads, nSectors, Sector::LC_1024, 1024, 1 )
