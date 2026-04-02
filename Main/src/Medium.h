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
	struct TIwProfile{
		TLogTime iwTimeDefault; // inspection window default size
		TLogTime iwTime; // inspection window size; a "1" is expected in its centre
		TLogTime iwTimeMin,iwTimeMax; // inspection window possible time range

		TIwProfile(TLogTime iwTimeDefault,BYTE iwTimeTolerancePercent=0);

		void ClampIwTime();
		inline TLogTime PeekNextIwTime(TLogTime tIwCurr) const{ return tIwCurr+iwTime; }
	};

	#pragma pack(1)
	typedef const struct TProperties sealed{
		static const TProperties FLOPPY_HD_350;
		static const TProperties FLOPPY_HD_525;
		static const TProperties FLOPPY_DD;
		static const TProperties FLOPPY_DD_525;

		LPCTSTR description;
		PropGrid::Integer::TUpDownLimits cylinderRange, headRange, sectorRange; // supported range of Cylinders/Heads/Sectors (min and max)
		Revolution::N rps; // Revolutions per second
		TLogTime revolutionTime; // single revolution time [nanoseconds]
		TLogTime cellTime; // single recorded data cell time [nanoseconds]
		Bit::N nCells; // RevolutionTime/CellTime

		bool IsAcceptableRevolutionTime(TLogTime tRevolutionQueried) const;
		bool IsAcceptableCountOfCells(Bit::N nCellsQueried) const;
		inline TIwProfile CreateIwProfile(BYTE iwTimeTolerancePercent=4) const{ return TIwProfile(cellTime,iwTimeTolerancePercent); }
	} *PCProperties;

	LPCTSTR GetDescription(TType mediumType);
	PCProperties GetProperties(TType mediumType);
}
