#include "stdafx.h"
#include "BSDOS.h"

	CBSDOS308::TFatValue::TFatValue(){
		// ctor
	}

	CBSDOS308::TFatValue::TFatValue(bool occupied,bool continuous,WORD info)
		// ctor
		: info(info) , continuous(continuous) , occupied(occupied) {
	}

	CBSDOS308::TFatValue::TFatValue(WORD w){
		// ctor
		*(PWORD)this=w;
		//info=w, continuous=(w&0x4000)!=0, occupied=(short)w<0;
	}

	CBSDOS308::TFatValue::operator WORD() const{
		return *(PCWORD)this;
	}











	inline
	static bool isValidFatSectorNumber(CBSDOS308::TLogSector lsFat){
		return BSDOS_FAT_LOGSECTOR_MIN<=lsFat && lsFat<BSDOS_FAT_LOGSECTOR_MAX;
	}

	CBSDOS308::TFatValue CBSDOS308::__getLogicalSectorFatItem__(TLogSector logSector) const{
		// returns the value in FAT of the specified LogicalSector; returns BSDOS_FAT_ERROR if FAT Sector read error
		if (const PCBootSector bootSector=boot.GetSectorData())
			for( BYTE fatCopy=0; fatCopy<BSDOS_FAT_COPIES_MAX; fatCopy++ ){
				TLogSector lsFat=bootSector->fatStarts[fatCopy];
				if (PCFatValue fat=reinterpret_cast<PCFatValue>( __getHealthyLogicalSectorData__(lsFat) ))
					for( TLogSector index=logSector; isValidFatSectorNumber(lsFat); index-=BSDOS_FAT_ITEMS_PER_SECTOR ){
						const TFatValue value=fat[lsFat];
						if (!value.occupied)
							break; // next FAT copy
						if (index<BSDOS_FAT_ITEMS_PER_SECTOR)
							// navigated to the correct FAT Sector - reading Index-th value from it
							if ((value.continuous || index*sizeof(TFatValue)<value.info)
								&&
								( fat=reinterpret_cast<PCFatValue>(__getHealthyLogicalSectorData__(lsFat)) )
							)
								return fat[index];
							else
								break; // next FAT copy
						if (!value.continuous)
							break; // next FAT copy
						lsFat=value.info;
					}
			}
		return TFatValue(TFatValue::FatError); // FAT i/o error
	}

	bool CBSDOS308::GetSectorStatuses(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PSectorStatus buffer) const{
		// True <=> Statuses of all Sectors in the Track successfully retrieved and populated the Buffer, otherwise False
		bool result=true; // assumption (statuses of all Sectors successfully retrieved)
		if (const PCBootSector bootSector=boot.GetSectorData())
			for( const TLogSector logSectorBase=(formatBoot.nHeads*cyl+head)*formatBoot.nSectors-BSDOS_SECTOR_NUMBER_FIRST; nSectors--; bufferId++ ){
				const TSector secNum=bufferId->sector;
				if (cyl>=formatBoot.nCylinders || head>=formatBoot.nHeads || bufferId->cylinder!=cyl || bufferId->side!=sideMap[head] || secNum>formatBoot.nSectors || !secNum || bufferId->lengthCode!=BSDOS_SECTOR_LENGTH_STD_CODE) // condition for Sector must be ">", not ">=" (Sectors numbered from 1 - see also "|!id")
					// Sector number out of official Format
					*buffer++=TSectorStatus::UNKNOWN;
				else{
					// getting Sector Status from FAT
					const TLogSector ls=secNum+logSectorBase;
					if (ls<2) // first two Sectors ...
systemSector:			*buffer++=TSectorStatus::SYSTEM; // ... are always reserved for system usage, regardless of the value stored in FAT
					else
						switch (const TFatValue fatValue=__getLogicalSectorFatItem__(ls)){
							case TFatValue::FatError:
								result=false;
								//fallthrough
							case TFatValue::SectorUnavailable:
								*buffer++=TSectorStatus::UNAVAILABLE;
								break;
							case TFatValue::SystemSector:
								goto systemSector;
							case TFatValue::SectorErrorInDataField:
							case TFatValue::SectorNotFound:
								*buffer++=TSectorStatus::BAD;
								break;
							case TFatValue::SectorUnknown:
								*buffer++=TSectorStatus::UNKNOWN;
								break;
							default:
								// . if Sector is reported Empty, we are done
								if (!fatValue.occupied){
									*buffer++=TSectorStatus::EMPTY;
									break;
								}
								// . DIRS Sector is a System one
								if (ls==bootSector->dirsLogSector)
									goto systemSector;
								// . FAT Sectors are System ones
								for( BYTE fatCopy=0; fatCopy<BSDOS_FAT_COPIES_MAX; fatCopy++ ){
									TLogSector lsFat=bootSector->fatStarts[fatCopy];
									if (const PCFatValue fat=reinterpret_cast<PCFatValue>(__getHealthyLogicalSectorData__(lsFat)))
										for( BYTE n=bootSector->nSectorsPerFat; n>0; n-- ){
											if (lsFat>=BSDOS_FAT_LOGSECTOR_MAX)
												break; // next FAT copy
											const TFatValue value=fat[lsFat];
											if (!value.occupied)
												break; // next FAT copy
											if (ls==lsFat)
												goto systemSector;										
											if (!value.continuous)
												break; // next FAT copy
											lsFat=value.info;
										}
								}								
								// . the Sector is Occupied
								*buffer++=TSectorStatus::OCCUPIED;
								break;
						}
				}
			}
		else
			while (nSectors--)
				*buffer++=TSectorStatus::UNKNOWN; // all Sector are Unknown by default
		return result;
	}

	bool CBSDOS308::__setLogicalSectorFatItem__(TLogSector logSector,TFatValue newValue) const{
		// returns the value in FAT of the specified LogicalSector; returns BSDOS_FAT_ERROR if FAT Sector read error
		bool valueWritten=false; // assumption (the Value couldn't be written into any FatCopy)
		if (const PCBootSector bootSector=boot.GetSectorData())
			for( BYTE fatCopy=0; fatCopy<BSDOS_FAT_COPIES_MAX; fatCopy++ ){
				const TLogSector lsFat0=bootSector->fatStarts[fatCopy];
				if (PFatValue fat=reinterpret_cast<PFatValue>(__getHealthyLogicalSectorData__(lsFat0)))
					for( TLogSector index=logSector,lsFat=lsFat0; isValidFatSectorNumber(lsFat); index-=BSDOS_FAT_ITEMS_PER_SECTOR ){
						const TFatValue value=fat[lsFat];
						if (!value.occupied)
							break; // next FAT copy
						if (index<BSDOS_FAT_ITEMS_PER_SECTOR){
							// navigated to the correct FAT Sector - writing NewValue at Index-th position
							BYTE &rFatChecksum=fat[0].upperByte;
							if ((value.continuous || index*sizeof(TFatValue)<value.info)
								&&
								( fat=reinterpret_cast<PFatValue>(__getHealthyLogicalSectorData__(lsFat)) )
							){
								if (logSector>0){
									const TFatValue oldValue=fat[index];
									rFatChecksum-=oldValue.lowerByte+oldValue.upperByte;
									rFatChecksum+=newValue.lowerByte+newValue.upperByte;
								}
								fat[index]=newValue;
								valueWritten=true;
								__markLogicalSectorAsDirty__(lsFat);
								__markLogicalSectorAsDirty__(lsFat0);
							}
							break; // next FAT copy
						}
						if (!value.continuous)
							break; // next FAT copy
						lsFat=value.info;
					}
			}
		return valueWritten;
	}

	bool CBSDOS308::ModifyStdSectorStatus(RCPhysicalAddress chs,TSectorStatus status) const{
		// True <=> the Status of the specified DOS-standard Sector successfully changed, otherwise False
		TFatValue value=TFatValue::SectorUnknown;
		switch (status){
			case TSectorStatus::UNAVAILABLE	: value=TFatValue::SectorUnavailable; break;
			case TSectorStatus::BAD			: value=TFatValue::SectorErrorInDataField; break;
			case TSectorStatus::EMPTY		: value=TFatValue::SectorEmpty; break;
			case TSectorStatus::UNKNOWN		: value=TFatValue::SectorUnknown; break;
			default:
				ASSERT(FALSE);
				break;
		}
		return __setLogicalSectorFatItem__( __fyzlog__(chs), value );
	}

	bool CBSDOS308::ModifyFileFatPath(PFile file,const CFatPath &rFatPath) const{
		// True <=> a error-free FatPath of given File successfully written, otherwise False
		CFatPath::PCItem pItem; DWORD nItems;
		if (rFatPath.GetItems(pItem,nItems)){ // if new FatPath erroneous ...
			ASSERT(false); // this shouldn't happen
			return false; // ... we are done
		}
		if (IsDirectory(file)){
			// root Subdirectory
			TLogSector ls = ((CDirsSector::PSlot)file)->firstSector = __fyzlog__(pItem->chs);
			for( WORD h; --nItems; ls=h )
				__setLogicalSectorFatItem__( ls, TFatValue(true,true,h=__fyzlog__((++pItem)->chs)) ); // no need to test FAT Sector existence (already tested above)
			__setLogicalSectorFatItem__(ls, // terminating the FatPath in FAT
										TFatValue( true, false, BSDOS_SECTOR_LENGTH_STD )
									);
		}else{
			const PDirectoryEntry de=(PDirectoryEntry)file;
			if (DWORD fileSize=de->file.dataLength){
				TLogSector ls = de->file.firstSector = __fyzlog__(pItem->chs);
				for( WORD h; --nItems; ls=h,fileSize-=BSDOS_SECTOR_LENGTH_STD ) // all Sectors but the last one are Occupied in FatPath
					__setLogicalSectorFatItem__( ls, TFatValue(true,true,h=__fyzlog__((++pItem)->chs)) ); // no need to test FAT Sector existence (already tested above)
				__setLogicalSectorFatItem__(ls, // terminating the FatPath in FAT
											TFatValue( true, false, fileSize )
										);
			}else // zero-length Files are in NOT in the FAT
				de->file.firstSector=boot.GetSectorData()->dirsLogSector; // some sensible value
		}
		return true;
	}

	BYTE CBSDOS308::__getFatChecksum__(BYTE fatCopy) const{
		// computes and returns the checksum of specified FAT copy
		if (const PCBootSector bootSector=boot.GetSectorData()){
			CFileReaderWriter frw( this, &TDirectoryEntry(this,bootSector->fatStarts[fatCopy]) );
			frw.Seek( 2, CFile::SeekPosition::begin );
			BYTE result=(4096-frw.GetLength())*0xff;
			for( TFatValue v; frw.GetPosition()<frw.GetLength(); )
				if (frw.Read(&v,sizeof(v))==sizeof(v))
					result+=v.lowerByte+v.upperByte;
				else
					result+=2*0xff;
			return result;
		}else
			return 0;
	}

	CBSDOS308::TLogSector CBSDOS308::__getEmptyHealthyFatSector__(bool allowFileFragmentation) const{
		// 
		// - first, searching for a healthy Sector that is reported Empty in existing FAT
		TPhysicalAddress chs;
		if (const TStdWinError err=GetFirstEmptyHealthySector(true,chs)) // if error finding (any) Empty healthy Sector ...
			return 0; // ... we are unsuccessfully done
		const TLogSector ls=__fyzlog__(chs);
		if (ls<BSDOS_FAT_LOGSECTOR_MAX)
			return ls;
		// - if fragmentation of existing Files not Allowed, we are unsuccessfully done
		if (!allowFileFragmentation)
			return 0;
		// - next, fragmenting an existing File, re-allocating one of its Sectors that begins before BSDOS_FAT_LOGSECTOR_MAX Sector
		for( CDirsSector::CTraversal rt(this); const PCFile subdir=rt.GetNextFileOrSubdir(); ) // root traversal
			for( TDirectoryEntry::CTraversal dt(this,subdir); const PFile file=dt.GetNextFileOrSubdir(); ){ // Subdirectory traversal
				const CFatPath fatPath(this,file);
				CFatPath::PItem pItem; DWORD nItems;
				if (fatPath.GetItems(pItem,nItems))
					continue; // can't re-allocate a Sector of a File whose FatPath is erroneous
				for( ; nItems--; pItem++ ){
					const TLogSector ls=__fyzlog__(pItem->chs);
					if (ls<BSDOS_FAT_LOGSECTOR_MAX) // found a Sector that might be moved to make space for a FAT Sector
						if (const PCSectorData data=__getHealthyLogicalSectorData__(ls)){ // the Sector is healthy
							// . finding a replacement Empty healthy Sector
							//nop (already found above)
							// . copying existing Data to the above Empty Sector
							const TLogSector lsEmpty=__fyzlog__(chs);
							::memcpy(	__getHealthyLogicalSectorData__(lsEmpty), // readability guaranteed
										data,
										formatBoot.sectorLength
									);
							//__markLogicalSectorAsDirty__(lsEmpty); // postponed until below
							// . changing File's FatPath
							pItem->chs=chs;
							if (!ModifyFileFatPath( file, fatPath )) // if FAT non-modifiable ...
								break; // ... better attempting to re-allocate Sector of another File
							__markLogicalSectorAsDirty__(lsEmpty);
							// . successfully re-allocated a healthy Sector, making it now Empty
							__setLogicalSectorFatItem__( ls, TFatValue::SectorEmpty );
							return ls;
						}
				}
			}
		// - no healthy Sector could be re-allocated
		return 0;
	}

	bool CBSDOS308::GetFileFatPath(PCFile file,CFatPath &rFatPath) const{
		// True <=> FatPath of given File (even an erroneous FatPath) successfully retrieved, otherwise False
		// - root Directory consists only of the DIRS sector
		CFatPath::TItem item;
		if (file==ZX_DIR_ROOT){
			if (const PCBootSector bootSector=boot.GetSectorData()){
				item.chs=__logfyz__( item.value=bootSector->dirsLogSector );
				rFatPath.AddItem(&item);
			}else
				rFatPath.error=CFatPath::TError::VALUE_BAD_SECTOR;
			return true;
		}
		// - query on root Subdirectory is translated to a query on File
		if (IsDirectory(file))
			return GetFileFatPath( &TDirectoryEntry(this,((CDirsSector::PCSlot)file)->firstSector), rFatPath );
		// - no FatPath can be retrieved if DirectoryEntry is Empty
		const PCDirectoryEntry de=(PCDirectoryEntry)file;
		if (!de->occupied){
			rFatPath.error=CFatPath::TError::FILE;
			return false;
		}
		// - FatPath has no Items if File has no data or its length is zero
		if (!de->fileHasData || !de->file.dataLength)
			return true;
		// - extracting the FatPath from FAT
		const TLogSector logSectorMax=formatBoot.GetCountOfAllSectors();
		TFatValue fatValue = item.value = de->file.firstSector;
		do{
			// . determining Sector's PhysicalAddress
			item.chs=__logfyz__(item.value);
			// . adding the Item to the FatPath
			if (!rFatPath.AddItem(&item)) break; // also sets an error in FatPath
			// . VALIDATION: Value must "make sense"
			fatValue=__getLogicalSectorFatItem__(item.value);
			if (fatValue==TFatValue::FatError){ // if FAT Sector cannot be read ...
				rFatPath.error=CFatPath::TError::SECTOR; // ... setting corresponding error ...
				break; // ... and quitting
			}
			if (fatValue>=TFatValue::SectorErrorInDataField){
				rFatPath.error=CFatPath::TError::VALUE_BAD_SECTOR;
				break;
			}
			if (!fatValue.occupied
				||
				fatValue.continuous && !(2<fatValue.info && fatValue.info<logSectorMax) // value must be within range
				||
				!fatValue.continuous && fatValue.info>BSDOS_SECTOR_LENGTH_STD
			){
				rFatPath.error=CFatPath::TError::VALUE_INVALID;
				break;
			}
			item.value=fatValue.info;
		}while (fatValue.continuous); // until the natural correct end of a File is found
		return true; // FatPath (with or without error) successfully extracted from FAT
	}
