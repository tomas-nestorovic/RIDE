#include "stdafx.h"
#include "GDOS.h"

	static LPCTSTR Recognize(PTCHAR){
		static const char SingleDeviceName[]=_T("MGT image\0");
		return SingleDeviceName;
	}
	static PImage Instantiate(LPCTSTR){
		return new CMGT;
	}
	const CImage::TProperties CMGT::Properties={MAKE_IMAGE_ID('G','D','O','S','_','M','G','T'), // a unique identifier
												Recognize,	// list of recognized device names
												Instantiate,// instantiation function
												_T("*.mgt"),	// filter
												Medium::FLOPPY_DD_ANY, // supported Media
												Codec::MFM, // supported Codecs
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
