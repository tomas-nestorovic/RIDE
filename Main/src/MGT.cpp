#include "stdafx.h"
#include "GDOS.h"

	static PImage __instantiate__(){
		return new CMGT;
	}
	const CImage::TProperties CMGT::Properties={_T("MGT"),// name
												__instantiate__,// instantiation function
												_T("*.mgt"),	// filter
												TMedium::FLOPPY_DD, // supported Media
												GDOS_SECTOR_LENGTH_STD, GDOS_SECTOR_LENGTH_STD	// Sector supported min and max length
											};





	CMGT::CMGT()
		// ctor
		: CImageRaw(&Properties,true) {
	}







	TStdWinError CMGT::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return __setMediumTypeAndGeometry__(pFormat,sideMap,firstSectorNumber);
	}
