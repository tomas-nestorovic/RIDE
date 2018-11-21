#include "stdafx.h"

	#define CRC16_POLYNOM 0x1021

	WORD CFloppyImage::__getCrcCcitt__(PCSectorData buffer,WORD length){
		// computes and returns CRC-CCITT (0xFFFF) of data with a given Length in Buffer
		WORD result=0xFFFF;
		while (length--){
			BYTE x = result>>8 ^ *buffer++;
			x ^= x>>4;
			result = (result<<8) ^ (WORD)(x<<12) ^ (WORD)(x<<5) ^ (WORD)x;
		}
		return (LOBYTE(result)<<8) + HIBYTE(result);
	}









	CFloppyImage::CFloppyImage(PCProperties properties,bool hasEditableSettings)
		// ctor
		: CImage(properties,hasEditableSettings)
		, floppyType(TMedium::UNKNOWN) {
	}








	WORD CFloppyImage::__getUsableSectorLength__(BYTE sectorLengthCode) const{
		// determines and returns usable portion of a Sector based on supplied LenghtCode and actual FloppyType
		const WORD officialLength=__getOfficialSectorLength__(sectorLengthCode);
		if (floppyType==TMedium::FLOPPY_DD || floppyType==TMedium::UNKNOWN) // Unknown = if FloppyType not set (e.g. if DOS Unknown), the floppy is by default considered as a one with the lowest capacity
			return min( 6144, officialLength );
		else
			return officialLength;
	}

	TStdWinError CFloppyImage::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		const TExclusiveLocker locker;
		floppyType=pFormat->mediumType;
		return ERROR_SUCCESS;
	}
