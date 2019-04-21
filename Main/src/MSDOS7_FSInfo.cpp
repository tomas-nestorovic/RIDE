#include "stdafx.h"
#include "MSDOS7.h"

	bool CMSDOS7::TFsInfoSector::__recognize__(WORD sectorLength) const{
		// True <=> FS Info Sector recognized, otherwise False
		return	sectorLength>=MSDOS7_SECTOR_LENGTH_STD
				&&
				mark41615252==0x41615252
				&&
				mark61417272==0x61417272
				&&
				markAA550000==0xAA550000;
	}

	void CMSDOS7::TFsInfoSector::__init__(){
		// initializes the FS Info Sector
		::ZeroMemory( this, MSDOS7_SECTOR_LENGTH_STD );
		mark41615252=0x41615252;
		mark61417272=0x61417272;
		nFreeClusters = firstFreeCluster = -1; // "-1" = information not available (will be set, for instance, in __getFirstFreeHealthyCluster__)
		markAA550000=0xAA550000;
	}










	CMSDOS7::CFsInfo::CFsInfo(const CMSDOS7 &msdos)
		// ctor
		: msdos(msdos) {
	}


	CMSDOS7::PFsInfoSector CMSDOS7::CFsInfo::GetSectorData() const{
		// returns the data of FS Info Sector; returns Null if Sector doesn't exist or isn't readable
		// - if FS Info Sector doesn't exist, we are done
		if (msdos.fat.type!=CFat::FAT32)
			return nullptr;
		// - reading and recognizing the FS Info Sector
		if (const PCBootSector bootSector=msdos.boot.GetSectorData())
			if (const PFsInfoSector fsInfo=(PFsInfoSector)msdos.__getLogicalSectorData__(bootSector->fat32.fsInfo))
				if (fsInfo->__recognize__(msdos.formatBoot.sectorLength))
					return fsInfo;
		// - FS Info Sector isn't readable or isn't recognized
		return nullptr;
	}

	void CMSDOS7::CFsInfo::MarkSectorAsDirty() const{
		// marks the FS Info Sector as dirty
		if (const PCBootSector bootSector=msdos.boot.GetSectorData())
			msdos.__markLogicalSectorAsDirty__( bootSector->fat32.fsInfo );
	}

