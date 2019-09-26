#include "stdafx.h"
#include "MSDOS7.h"

	const CMSDOS7::CFat::TType CMSDOS7::CFat::Types[]={ FAT12, FAT16, FAT32, UNDETERMINED };

	CMSDOS7::CFat::CFat(const CMSDOS7 &msdos)
		// ctor
		: msdos(msdos) , type(UNDETERMINED) {
	}

	#define FAT12_CLUSTERS_COUNT_MAX	4085
	#define FAT16_CLUSTERS_COUNT_MAX	65525
	#define FAT32_CLUSTERS_COUNT_MAX	0x03ffffff

	CMSDOS7::CFat::TType CMSDOS7::CFat::GetFatType(TCluster32 nClusters){
		// determines and returns the FAT Type
		if (nClusters<FAT12_CLUSTERS_COUNT_MAX)
			return FAT12;
		else if (nClusters<FAT16_CLUSTERS_COUNT_MAX)
			return FAT16;
		else
			return FAT32;
	}

	CMSDOS7::TCluster32 CMSDOS7::CFat::GetMinCountOfClusters() const{
		// returns the minimum number of Clusters that must be addressed in current FAT Type ("must be addressed" = otherwise another Type of FAT would have to be used)
		switch (type){
			case FAT12: return 0;
			case FAT16: return FAT12_CLUSTERS_COUNT_MAX+1;
			case FAT32:	return FAT16_CLUSTERS_COUNT_MAX+1;
		}
		ASSERT(FALSE);
		return 0;
	}

	CMSDOS7::TCluster32 CMSDOS7::CFat::GetMaxCountOfClusters() const{
		// returns the maximum number of Clusters that can be addressed in current FAT Type
		switch (type){
			case FAT12: return FAT12_CLUSTERS_COUNT_MAX;
			case FAT16: return FAT16_CLUSTERS_COUNT_MAX;
			case FAT32: return FAT32_CLUSTERS_COUNT_MAX;
		}
		ASSERT(FALSE);
		return 0;
	}

	DWORD CMSDOS7::CFat::GetClusterValue(TCluster32 cluster) const{
		// returns the Value recorded in FAT for given Cluster; returns Error if Cluster not found
		if (const PCBootSector bootSector=msdos.boot.GetSectorData()){
			DWORD value=0;
			int positionInFat=cluster*type/2; // Type = number of half-bytes; Int to disambiguate "div_t"
			for( BYTE n=(type+1)/2,*v=(PBYTE)&value; n--; ){
				const div_t d=div( positionInFat++, msdos.formatBoot.sectorLength );
				for( BYTE nFatCopies=bootSector->nFatCopies; nFatCopies--; ){
					const TLogSector32 ls=	bootSector->nReservedSectors	// BootSector, etc.
											+
											d.quot+nFatCopies*bootSector->__getCountOfSectorsInOneFatCopy__(); // FAT
					if (const PCSectorData fatData=msdos.__getHealthyLogicalSectorData__(ls)){
						*v++=fatData[d.rem];
						goto nextByte;
					}
				}
				return MSDOS7_FAT_ERROR;
nextByte:		;
			}
			switch (type){
				case FAT12:
					value=	cluster&1
							? value>>4	// FAT12 value at odd address
							: value&0xfff;// FAT12 value at even address
					return	value>=0xff0
							? 0x0ffff000|value
							: value;
				case FAT16:
					return	value>=0xfff0
							? 0x0fff0000|value
							: value;
				case FAT32:
					return value&0x0fffffff;
				default:
					ASSERT(FALSE);
			}
		}
		return MSDOS7_FAT_ERROR;
	}

	bool CMSDOS7::CFat::SetClusterValue(TCluster32 cluster,DWORD newValue) const{
		// True <=> NewValue of given Cluster successfully written into at least one copy of FAT, otherwise False
		if (const PCBootSector bootSector=msdos.boot.GetSectorData()){
			// . modifying the FSInfo Sector
			if (const PFsInfoSector fsInfo=msdos.fsInfo.GetSectorData())
				if (newValue==MSDOS7_FAT_CLUSTER_EMPTY && cluster<fsInfo->firstFreeCluster){
					fsInfo->firstFreeCluster=cluster;
					msdos.fsInfo.MarkSectorAsDirty();
				}else if (newValue!=MSDOS7_FAT_CLUSTER_EMPTY && cluster==fsInfo->firstFreeCluster){
					fsInfo->firstFreeCluster++;
					msdos.fsInfo.MarkSectorAsDirty();
				}
			// . adjusting the NewValue and Mask
			DWORD mask;
			switch (type){
				case FAT12:
					if (cluster&1) // FAT12 value at odd address
						newValue<<=4,	mask=0xf;
					else // FAT12 value at even address
						newValue&=0xfff,mask=0xf000;
					break;
				case FAT16:
					mask=0; break;
				case FAT32:
					newValue&=0x0fffffff, mask=0xf0000000; break;
				default:
					ASSERT(FALSE);
					return false;
			}
			// . recording NewValue in FAT copies
			int positionInFat=cluster*type/2; // Type = number of half-bytes; Int to disambiguate "div_t"
			for( BYTE n=(type+1)/2,*v=(PBYTE)&newValue,*m=(PBYTE)&mask; n--; v++,m++ ){
				const div_t d=div( positionInFat++, msdos.formatBoot.sectorLength );
				bool byteWritten=false; // assumption (current Byte of NewValue failed to write into any of FAT copies)
				for( BYTE nFatCopies=bootSector->nFatCopies; nFatCopies--; ){
					const TLogSector32 ls=	bootSector->nReservedSectors	// BootSector etc.
											+
											d.quot+nFatCopies*bootSector->__getCountOfSectorsInOneFatCopy__(); // FAT
					if (const PSectorData fatData=msdos.__getHealthyLogicalSectorData__(ls)){
						fatData[d.rem] = fatData[d.rem]&*m | *v;
						byteWritten=true;
						msdos.__markLogicalSectorAsDirty__(ls);
					}
				}
				if (!byteWritten)
					return false;
			}
			return true; // NewValue written into available copies of FAT
		}else
			return false;
	}

	bool CMSDOS7::CFat::FreeChainOfClusters(TCluster32 cluster) const{
		// True <=> the whole chain of Clusters until the EOF mark has been recorded as Free, beginning with the specified Cluster, otherwise False
		for( TCluster32 next; cluster!=MSDOS7_FAT_CLUSTER_EOF; cluster=next ){
			next=GetClusterValue(cluster);
			if (next==MSDOS7_FAT_ERROR) // if Next Cluster cannot be read ...
				return false; // ... the chain is broken
			if (!SetClusterValue( cluster, MSDOS7_FAT_CLUSTER_EMPTY )) // if Cluster cannot be recorded as Free in any of FAT copies ...
				return false; // ... the chain is broken
		}
		return true;
	}
