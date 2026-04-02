#include "stdafx.h"

namespace Medium
{
	TIwProfile::TIwProfile(TLogTime iwTimeDefault,BYTE iwTimeTolerancePercent)
		// ctor
		: iwTimeDefault(iwTimeDefault)
		, iwTime(iwTimeDefault)
		, iwTimeMin( iwTimeDefault*(100-iwTimeTolerancePercent)/100 )
		, iwTimeMax( iwTimeDefault*(100+iwTimeTolerancePercent)/100 ) {
	}

	void TIwProfile::ClampIwTime(){
		// keep the inspection window size within limits
		if (iwTime<iwTimeMin)
			iwTime=iwTimeMin;
		else if (iwTime>iwTimeMax)
			iwTime=iwTimeMax;
	}





	bool TProperties::IsAcceptableRevolutionTime(TLogTime tRevolutionQueried) const{
		return revolutionTime/10*9<tRevolutionQueried && tRevolutionQueried<revolutionTime/10*11; // 10% tolerance (don't set more for indices on 300 RPM drive appear only 16% slower than on 360 RPM drive!)
	}

	bool TProperties::IsAcceptableCountOfCells(Bit::N nCellsQueried) const{
		return nCells/100*85<nCellsQueried && nCellsQueried<nCells/100*115; // 20% (or more) is too much - a 360rpm drive is 20% faster than 300rpm drive which would introduce confusion to the rest of app
	}





	LPCTSTR GetDescription(TType mediumType){
		// returns the string description of a given MediumType
		if (const PCProperties p=GetProperties(mediumType))
			return p->description;
		else{
			ASSERT(FALSE); // ending up here isn't a problem but always requires attention!
			return _T("Unknown medium");
		}
	}

	const TProperties TProperties::FLOPPY_HD_350={
		_T("3.5\" HD floppy"), // description
		{ 1, FDD_CYLINDERS_MAX }, // supported range of Cylinders (min and max)
		{ 1, 2 },	// supported range of Heads (min and max)
		{ 1, FDD_SECTORS_MAX }, // supported range of Sectors (min and max)
		5, // Revolutions per second
		TIME_SECOND(1)/5, // single revolution time [nanoseconds]
		TIME_MICRO(1), // single recorded data cell time [nanoseconds] = 1 second / 500kb = 2 탎 -> 1 탎 for MFM encoding
		200000 // RevolutionTime/CellTime
	};

	const TProperties TProperties::FLOPPY_HD_525={
		_T("5.25\" HD floppy, 360 RPM drive"), // description
		{ 1, FDD_CYLINDERS_MAX }, // supported range of Cylinders (min and max)
		{ 1, 2 },	// supported range of Heads (min and max)
		{ 1, FDD_SECTORS_MAX }, // supported range of Sectors (min and max)
		6, // Revolutions per second
		TIME_SECOND(1)/6, // single revolution time [nanoseconds]
		TIME_MICRO(1), // single recorded data cell time [nanoseconds] = same as 3.5" HD floppies
		166666 // RevolutionTime/CellTime
	};

	const TProperties TProperties::FLOPPY_DD={
		_T("3.x\"/5.25\" 2DD floppy, 300 RPM drive"), // description
		{ 1, FDD_CYLINDERS_MAX }, // supported range of Cylinders (min and max)
		{ 1, 2 },	// supported range of Heads (min and max)
		{ 1, FDD_SECTORS_MAX }, // supported range of Sectors (min and max)
		5, // Revolutions per second
		TIME_SECOND(1)/5, // single revolution time [nanoseconds]
		TIME_MICRO(2), // single recorded data cell time [nanoseconds] = 1 second / 250kb = 4 탎 -> 2 탎 for MFM encoding
		100000 // RevolutionTime/CellTime
	};

	const TProperties TProperties::FLOPPY_DD_525={
		_T("5.25\" 2DD floppy, 360 RPM drive"), // description
		{ 1, FDD_CYLINDERS_MAX }, // supported range of Cylinders (min and max)
		{ 1, 2 },	// supported range of Heads (min and max)
		{ 1, FDD_SECTORS_MAX }, // supported range of Sectors (min and max)
		6, // Revolutions per second
		TIME_MICRO(166600), // single revolution time [nanoseconds], rounded TIME_SECOND(1)/6
		TIME_MICRO(2)*5/6, // single recorded data cell time [nanoseconds] = 1 second / 300kb = 3.333 탎 -> 1.666 탎 for MFM encoding
		100000 // RevolutionTime/CellTime
	};

	PCProperties GetProperties(TType mediumType){
		// returns properties of a given MediumType
		switch (mediumType){
			case FLOPPY_DD_525:
				return &TProperties::FLOPPY_DD_525;
			case FLOPPY_DD:
				return &TProperties::FLOPPY_DD;
			case FLOPPY_HD_525:
				return &TProperties::FLOPPY_HD_525;
			case FLOPPY_HD_350:
				return &TProperties::FLOPPY_HD_350;
			case HDD_RAW:{
				static constexpr TProperties P={
					_T("Hard disk (without MBR support)"), // description
					{ 1, HDD_CYLINDERS_MAX },// supported range of Cylinders (min and max)
					{ 1, HDD_HEADS_MAX },	// supported range of Heads (min and max)
					{ 1, (TSector)-1 },	// supported range of Sectors (min and max)
					0, // N/A - single revolution time [nanoseconds]
					0, // N/A - single recorded data cell time [nanoseconds]
					0 // N/A - RevolutionTime/CellTime
				};
				return &P;
			}
			default:
				ASSERT(FALSE);
				return nullptr;
		}
	}
}
