#include "stdafx.h"
#include "TRDOS.h"

	static PDos __instantiate__(PImage image,PCFormat pFormatBoot){
		return new CTRDOS504( image, pFormatBoot, &CTRDOS504::Properties );
	}

	TStdWinError CTRDOS504::__recognizeDisk__(PImage image,PFormat pFormatBoot){
		// returns the result of attempting to recognize Image by this DOS as follows: ERROR_SUCCESS = recognized, ERROR_CANCELLED = user cancelled the recognition sequence, any other error = not recognized
		if (const TStdWinError err=__super::__recognizeDisk__(image,pFormatBoot))
			return err;
		else if (((PCBootSector)image->GetHealthySectorData(TBootSector::CHS))->__getLabelLengthEstimation__()==TRDOS504_BOOT_LABEL_LENGTH_MAX)
			return ERROR_SUCCESS;
		else
			return ERROR_UNRECOGNIZED_VOLUME;
	}

	const CDos::TProperties CTRDOS504::Properties={
		TRDOS_NAME_BASE _T(" 5.04"), // name
		MAKE_DOS_ID('T','R','D','O','S','5','0','4'), // unique identifier
		42, // recognition priority (the bigger the number the earlier the DOS gets crack on the image)
		__recognizeDisk__, // recognition function
		__instantiate__, // instantiation function
		TMedium::FLOPPY_DD_ANY,
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








	CTRDOS504::CTRDOS504(PImage image,PCFormat pFormatBoot,PCProperties pTrdosProps)
		// ctor
		: CTRDOS503( image, pFormatBoot, pTrdosProps ) {
	}

