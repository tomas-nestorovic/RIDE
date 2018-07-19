#include "stdafx.h"
#include "TRDOS.h"

	static PDos __instantiate__(PImage image,PCFormat pFormatBoot){
		return new CTRDOS504(image,pFormatBoot);
	}

	bool CTRDOS504::__recognizeDisk__(PImage image,PFormat pFormatBoot){
		// True <=> DOS recognizes its own disk, otherwise False
		if (__super::__recognizeDisk__(image,pFormatBoot))
			return ((PCBootSector)image->GetSectorData(TBootSector::CHS))->label[TRDOS504_BOOT_LABEL_LENGTH_MAX]=='\0';
		else
			return false;
	}

	const CDos::TProperties CTRDOS504::Properties={
		_T("TR-DOS 5.04"), // name
		MAKE_DOS_ID('T','R','D','O','S','5','0','4'), // unique identifier
		3, // recognition priority
		__recognizeDisk__, // recognition function
		__instantiate__, // instantiation function
		TMedium::FLOPPY_DD,
		4,	// number of std Formats
		StdFormats, // std Formats
		TRDOS503_TRACK_SECTORS_COUNT,TRDOS503_TRACK_SECTORS_COUNT, // range of supported number of Sectors
		TRDOS503_SECTOR_RESERVED_COUNT, // minimal total number of Sectors required
		1, // maximum number of Sector in one Cluster (must be power of 2)
		-1, // maximum size of a Cluster (in Bytes)
		0,0, // range of supported number of allocation tables (FATs)
		128,128, // range of supported number of root Directory entries
		TRDOS503_SECTOR_FIRST_NUMBER,	// lowest Sector number on each Track
		0,TDirectoryEntry::END_OF_DIR,	// regular Sector and Directory Sector filler Byte
		0,0 // number of reserved Bytes at the beginning and end of each Sector
	};








	CTRDOS504::CTRDOS504(PImage image,PCFormat pFormatBoot)
		// ctor
		: CTRDOS503( image, pFormatBoot, &Properties ) {
	}

