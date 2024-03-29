#include "stdafx.h"
#include "GDOS.h"

	static PDos __instantiate__(PImage image,PCFormat pFormatBoot){
		return new CGDOS(image,pFormatBoot);
	}
	static constexpr CFormatDialog::TStdFormat StdFormats[]={
		{ _T("DS 80x10"), 0, {Medium::FLOPPY_DD,Codec::MFM,GDOS_CYLINDERS_COUNT-1,2,GDOS_TRACK_SECTORS_COUNT,GDOS_SECTOR_LENGTH_STD_CODE,GDOS_SECTOR_LENGTH_STD,1}, 1, 0, FDD_350_SECTOR_GAP3, 1, GDOS_DIR_FILES_COUNT_MAX }
	};

	TStdWinError CGDOS::__recognizeDisk__(PImage image,PFormat pFormatBoot){
		// returns the result of attempting to recognize Image by this DOS as follows: ERROR_SUCCESS = recognized, ERROR_CANCELLED = user cancelled the recognition sequence, any other error = not recognized
		// - setting up the biggest possible geometry
		//static const TFormat Fmt={ Medium::FLOPPY_DD, 1,1,10, GDOS_SECTOR_LENGTH_STD_CODE,GDOS_SECTOR_LENGTH_STD, 1 };
		//if (image->SetMediumTypeAndGeometry( &fmt, StdSidesMap, 1 )!=ERROR_SUCCESS) return false;
		( *pFormatBoot=StdFormats[0].params.format ).nCylinders++;
		if (const TStdWinError err=image->SetMediumTypeAndGeometry( pFormatBoot, StdSidesMap, Properties.firstSectorNumber ))
			return err;
		if (image->GetCylinderCount()<GDOS_CYLINDERS_COUNT)
			return Utils::ErrorByOs( ERROR_VOLMGR_DISK_LAYOUT_PARTITIONS_TOO_SMALL, ERROR_UNRECOGNIZED_VOLUME );
		// - checking disjunction of File FatPaths in the root Directory (each Sector must be allocated to a single File or be unallocated)
		const CGDOS gdos(image,pFormatBoot);
		TDirectoryEntry::TSectorAllocationBitmap allocatedSectorsOnDisk;
		BYTE nDirectorySectorsBad=0;
		for( TGdosDirectoryTraversal dt(&gdos); dt.AdvanceToNextEntry(); )
			if (dt.entryType!=TDirectoryTraversal::WARNING){
				// root Directory Sector found - evaluating obtained DirectoryItem
				const TDirectoryEntry *const de=(PDirectoryEntry)dt.entry;
				if (de->fileType!=TDirectoryEntry::EMPTY_ENTRY){
					// . checking NumberOfSectors
					if (de->nSectors>GDOS_CYLINDERS_COUNT*2*GDOS_TRACK_SECTORS_COUNT-GDOS_DIR_FILES_COUNT_MAX*sizeof(TDirectoryEntry)/GDOS_SECTOR_LENGTH_STD)
						return ERROR_UNRECOGNIZED_VOLUME;
					// . checking position of the FirstSector
					if (!de->firstSector.__isValid__())
						return ERROR_UNRECOGNIZED_VOLUME;
					// . checking SectorAllocationBitmap
					if (!de->sectorAllocationBitmap.IsDisjunctiveWith(allocatedSectorsOnDisk))
						return ERROR_UNRECOGNIZED_VOLUME;
					allocatedSectorsOnDisk.MergeWith(de->sectorAllocationBitmap);
				}
			}else
				// root Directory Sector not found - if "too many" Sectors not found, it's not a GDOS disk
				if (++nDirectorySectorsBad>3)
					return ERROR_UNRECOGNIZED_VOLUME;
		// - GDOS successfully recognizes the disk
		return ERROR_SUCCESS;
	}

	
	const CDos::TProperties CGDOS::Properties={
		_T("GDOS (experimental)"), // name
		MAKE_DOS_ID('G','D','O','S','_','_','_','_'), // unique identifier
		20, // recognition priority (the bigger the number the earlier the DOS gets crack on the image)
		0, // the Cylinder where usually the Boot Sector (or its backup) is found
		__recognizeDisk__, // recognition function
		__instantiate__, // instantiation function
		Medium::FLOPPY_DD_ANY,
		&MGT::Properties, // the most common Image to contain data for this DOS (e.g. *.D80 Image for MDOS)
		1,	// number of std Formats
		StdFormats, // std Formats
		Codec::MFM, // a set of Codecs this DOS supports
		GDOS_TRACK_SECTORS_COUNT,GDOS_TRACK_SECTORS_COUNT, // range of supported number of Sectors
		GDOS_DIR_FILES_COUNT_MAX/2, // minimal total number of Sectors required
		1, // maximum number of Sector in one Cluster (must be power of 2)
		-1, // maximum size of a Cluster (in Bytes)
		1,1, // range of supported number of allocation tables (FATs)
		GDOS_DIR_FILES_COUNT_MAX,GDOS_DIR_FILES_COUNT_MAX, // range of supported number of root Directory entries
		1,	// lowest Sector number on each Track
		0,0,	// regular Sector and Directory Sector filler Byte
		0,2 // number of reserved Bytes at the beginning and end of each Sector
	};










	void CGDOS::FlushToBootSector() const{
		// flushes internal Format information to the actual Boot Sector's data
		//nop (doesn't have a Boot Sector)
	}

	void CGDOS::InitializeEmptyMedium(CFormatDialog::PCParameters params,CActionProgress &ap){
		// initializes a fresh formatted Medium (Boot, FAT, root dir, etc.)
		// - creating the Boot Sector
		//nop (doesn't have a Boot Sector)
		// - initializing FAT
		//nop (doesn't have a FAT)
		// - empty Directory
		//nop (DirectoryEntry set as Empty during formatting - FillerByte happens to have the same value)
	}









	namespace MGT{
		static const CImage::TProperties Properties={
			MAKE_IMAGE_ID('G','D','O','S','_','M','G','T'), // a unique identifier
			Recognize,	// list of recognized device names
			Instantiate,// instantiation function
			_T("*.mgt"),	// filter
			Medium::FLOPPY_DD_ANY, // supported Media
			Codec::MFM, // supported Codecs
			GDOS_SECTOR_LENGTH_STD, GDOS_SECTOR_LENGTH_STD	// Sector supported min and max length
		};

		LPCTSTR Recognize(PTCHAR){
			static constexpr TCHAR SingleDeviceName[]=_T("MGT image\0");
			return SingleDeviceName;
		}
		PImage Instantiate(LPCTSTR){
			return new CImageRaw( &Properties, false );
		}
	}
