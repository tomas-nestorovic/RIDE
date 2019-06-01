#include "stdafx.h"
#include "TRDOS.h"

	static PDos __instantiate__(PImage image,PCFormat pFormatBoot){
		return new CTRDOS505(image,pFormatBoot);
	}

	TStdWinError CTRDOS505::__recognizeDisk__(PImage image,PFormat pFormatBoot){
		// returns the result of attempting to recognize Image by this DOS as follows: ERROR_SUCCESS = recognized, ERROR_CANCELLED = user cancelled the recognition sequence, any other error = not recognized
		if (const TStdWinError err=CTRDOS503::__recognizeDisk__(image,pFormatBoot)) // explicitly calling TR-DOS 5.03 recognition routine, thus bypassing TR-DOS 5.04 recognition routine
			return err;
		else if (((PCBootSector)image->GetSectorData(TBootSector::CHS))->__getLabelLengthEstimation__()==TRDOS505_BOOT_LABEL_LENGTH_MAX)
			return ERROR_SUCCESS;
		else
			return ERROR_UNRECOGNIZED_VOLUME;
	}

	const CDos::TProperties CTRDOS505::Properties={
		TRDOS_NAME_BASE _T(" 5.05"), // name
		MAKE_DOS_ID('T','R','D','O','S','5','0','5'), // unique identifier
		44, // recognition priority (the bigger the number the earlier the DOS gets crack on the image)
		__recognizeDisk__, // recognition function
		__instantiate__, // instantiation function
		TMedium::FLOPPY_DD,
		&TRD::Properties, // the most common Image to contain data for this DOS (e.g. *.D80 Image for MDOS)
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








	CTRDOS505::CTRDOS505(PImage image,PCFormat pFormatBoot)
		// ctor
		: CTRDOS504( image, pFormatBoot, &Properties ) {
	}

