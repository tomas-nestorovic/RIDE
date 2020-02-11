#include "stdafx.h"
#include "BSDOS.h"

	CBSDOS308::CBSDOS308(PImage image,PCFormat pFormatBoot)
		// ctor
		// - base
		: CSpectrumDos( image, pFormatBoot, TTrackScheme::BY_CYLINDERS, &Properties, IDR_BSDOS, &fileManager, TGetFileSizeOptions::OfficialDataLength )
		// - initialization
		, trackMap(this)
		, boot(this)
		, dirsSector(this)
		, fileManager(this) {
		ASSERT( sizeof(TFatValue)==sizeof(WORD) );
		ASSERT( sizeof(TDirectoryEntry)==32 );
		ASSERT( sizeof(CDirsSector::TSlot)==sizeof(DWORD) );
	}











	CBSDOS308::TLogSector CBSDOS308::__fyzlog__(RCPhysicalAddress chs) const{
		// converts PhysicalAddress to LogicalSector number and returns it
		return (chs.cylinder*formatBoot.nHeads+chs.head)*formatBoot.nSectors+chs.sectorId.sector-BSDOS_SECTOR_NUMBER_FIRST;
	}
	TPhysicalAddress CBSDOS308::__logfyz__(TLogSector ls) const{
		// converts LogicalSector number to PhysicalAddress and returns it
		const div_t A=div( ls, formatBoot.nSectors ), B=div( A.quot, formatBoot.nHeads );
		const TPhysicalAddress chs={ B.quot, B.rem, { B.quot, sideMap[B.rem], A.rem+BSDOS_SECTOR_NUMBER_FIRST, BSDOS_SECTOR_LENGTH_STD_CODE } };
		return chs;
	}


	PSectorData CBSDOS308::__getHealthyLogicalSectorData__(TLogSector logSector) const{
		// returns data of LogicalSector, or Null of such Sector is unreadable or doesn't exist
		return image->GetHealthySectorData( __logfyz__(logSector) );
	}
	void CBSDOS308::__markLogicalSectorAsDirty__(TLogSector logSector) const{
		// marks given LogicalSector as dirty
		image->MarkSectorAsDirty( __logfyz__(logSector) );
	}







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

	#define BSDOS_FAT_ITEMS_PER_SECTOR	(BSDOS_SECTOR_LENGTH_STD/sizeof(CBSDOS308::TFatValue))

	CBSDOS308::TFatValue CBSDOS308::__getLogicalSectorFatItem__(TLogSector logSector) const{
		// returns the value in FAT of the specified LogicalSector; returns BSDOS_FAT_ERROR if FAT Sector read error
		if (const PCBootSector bootSector=boot.GetSectorData())
			for( BYTE fatCopy=0; fatCopy<BSDOS_FAT_COPIES_MAX; fatCopy++ ){
				TLogSector lsFat=bootSector->fatStarts[fatCopy];
				if (PCFatValue fat=reinterpret_cast<PCFatValue>( __getHealthyLogicalSectorData__(lsFat) ))
					for( TLogSector index=logSector; lsFat<BSDOS_FAT_ITEMS_PER_SECTOR; index-=BSDOS_FAT_ITEMS_PER_SECTOR ){
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
											if (lsFat>=BSDOS_FAT_ITEMS_PER_SECTOR)
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
					for( TLogSector index=logSector,lsFat=lsFat0; lsFat<BSDOS_FAT_ITEMS_PER_SECTOR; index-=BSDOS_FAT_ITEMS_PER_SECTOR ){
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
			default:
				ASSERT(FALSE);
				break;
		}
		return __setLogicalSectorFatItem__( __fyzlog__(chs), value );
	}

	bool CBSDOS308::ModifyFileFatPath(PFile file,const CFatPath &rFatPath) const{
		// True <=> a error-free FatPath of given File successfully written, otherwise False
		CFatPath::PCItem pItem; DWORD nItems;
		if (rFatPath.GetItems(pItem,nItems)) // if FatPath erroneous ...
			return false; // ... we are done
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







	CBSDOS308::CDirsSector::CDirsSector(const CBSDOS308 *bsdos)
		// ctor
		: bsdos(bsdos) {
	}

	CBSDOS308::CDirsSector::PSlot CBSDOS308::CDirsSector::GetSlots() const{
		// returns pointer to the first Slot in the DIRS Sector, or Null if the DIRS Sector cannot be found
		if (const PCBootSector bootSector=bsdos->boot.GetSectorData())
			if (const PSlot slot=(PSlot)bsdos->__getHealthyLogicalSectorData__(bootSector->dirsLogSector))
				return slot;
		return nullptr;
	}

	void CBSDOS308::CDirsSector::MarkAsDirty() const{
		// marks the DIRS Sector as dirty
		if (const PCBootSector bootSector=bsdos->boot.GetSectorData())
			bsdos->__markLogicalSectorAsDirty__(bootSector->dirsLogSector);
	}

	CBSDOS308::PDirectoryEntry CBSDOS308::CDirsSector::TryGetDirectoryEntry(PCSlot slot) const{
		// attempts to get and return a DirectoryEntry corresponding to the specified Slot; returns Null if DirectoryEntry cannot be retrieved without error
		if (slot->subdirExists){
			WORD w; TFdcStatus sr;
			const PBYTE data=bsdos->image->GetSectorData(	bsdos->__logfyz__(slot->firstSector), 0,
															false, // don't calibrate head, settle with any (already buffered) Data, even erroneous
															&w, &sr
														);
			if (sr.IsWithoutError())
				return (PDirectoryEntry)data; // although above settled with any, returned are only flawless Data
		}
		return nullptr;
	}

	void CBSDOS308::CDirsSector::MarkDirectoryEntryAsDirty(PCSlot slot) const{
		// marks Sector containing the DirectoryEntry as "dirty"
		if (slot->subdirExists)
			bsdos->__markLogicalSectorAsDirty__( slot->firstSector );
	}

	DWORD CBSDOS308::GetAttributes(PCFile file) const{
		// maps File's attributes to Windows attributes and returns the result
		// - root is a Directory
		if (file==ZX_DIR_ROOT)
			return FILE_ATTRIBUTE_DIRECTORY;
		// - root Directory's entries are Subdirectories
		const CDirsSector::PCSlot pSlot=(CDirsSector::PCSlot)file, slots=dirsSector.GetSlots();
		if (slots<=pSlot && pSlot<slots+BSDOS_DIRS_SLOTS_COUNT)
			return FILE_ATTRIBUTE_DIRECTORY;
		// - everything else is a File
		return 0;
	}






	const CBSDOS308::CDirsSector::TSlot CBSDOS308::CDirsSector::TSlot::Empty;

	CBSDOS308::CDirsSector::CTraversal::CTraversal(const CBSDOS308 *bsdos)
		// ctor
		// - base
		: TDirectoryTraversal( ZX_DIR_ROOT, sizeof(TSlot) )
		// - initialization
		, nSlotsRemaining(BSDOS_DIRS_SLOTS_COUNT) {
		if (entry=bsdos->dirsSector.GetSlots()){
			chs=bsdos->__logfyz__( bsdos->boot.GetSectorData()->dirsLogSector );
			entry=((PSlot)entry)-1;
		}
	}

	bool CBSDOS308::CDirsSector::CTraversal::AdvanceToNextEntry(){
		// True <=> found another entry in current Directory (Empty or not), otherwise False
		if (!nSlotsRemaining){ // all Slots traversed
			entryType=TDirectoryTraversal::END;
			return false;
		}if (!entry){ // if DIRS Sector not found ...
			entryType=TDirectoryTraversal::WARNING, warning=ERROR_SECTOR_NOT_FOUND;
			nSlotsRemaining--;
			return false; // ... there are no Slots to traverse
		}
		entryType=	((PSlot)( entry=((PSlot)entry)+1 ))->subdirExists
					? TDirectoryTraversal::SUBDIR
					: TDirectoryTraversal::EMPTY;
		nSlotsRemaining--;
		return true;
	}

	void CBSDOS308::CDirsSector::CTraversal::ResetCurrentEntry(BYTE directoryFillerByte) const{
		// gets current entry to the state in which it would be just after formatting
		//nop (can't reset a root Directory Slot)
	}

	#define INFO_DIR	_T("S%x")

	TStdWinError CBSDOS308::CreateSubdirectory(LPCTSTR name,DWORD winAttr,PFile &rCreatedSubdir){
		// creates a new Subdirectory in CurrentDirectory; returns Windows standard i/o error
		// - cannot create a new Subdirectory elsewhere but in the Root Directory
		if (currentDir!=ZX_DIR_ROOT)
			return ERROR_DIR_NOT_ROOT;
		// - creating a new Subdirectory in the first empty Slot
		TPhysicalAddress chs;
		for( CDirsSector::PSlot slot=dirsSector.GetSlots(); IsDirectory(slot); slot++ )
			if (!slot->subdirExists)
				if (GetFirstEmptyHealthySector(true,chs)==ERROR_SUCCESS){
					// . parsing the Name (can be an import name with escaped Spectrum tokens)
					CPathString zxName,zxExt; LPCTSTR zxInfo;
					TCHAR buf[16384];
					__parseFat32LongName__(	::lstrcpy(buf,name), zxName, zxExt, zxInfo );
					zxName.TrimToLength(ZX_TAPE_FILE_NAME_LENGTH_MAX);
					zxExt.TrimToLength(1);
					// . validating type
					int dirNameChecksum=-1;
					if (zxInfo!=nullptr){
						TUniFileType uts;
						const int n=__importFileInformation__( zxInfo, uts );
						if (uts!=TUniFileType::SUBDIRECTORY)
							return ERROR_CANNOT_MAKE;
						_stscanf( zxInfo+n, INFO_DIR, &dirNameChecksum );
					}
					// . validating Name
					if (zxExt.GetLength())
						return ERROR_INVALID_DATATYPE;
					if (const TStdWinError err=TDirectoryEntry(this,0).file.stdHeader.SetName(zxName))
						return err;
					// . indicating that the Slot is now occupied
					const TLogSector ls=__fyzlog__(chs);
					slot->subdirExists=true;
					slot->firstSector=ls;
					dirsSector.MarkAsDirty();
					rCreatedSubdir=slot; // the output
					// . marking the assigned Sector as Occupied in the FAT
					__setLogicalSectorFatItem__( ls, TFatValue(true,false,BSDOS_SECTOR_LENGTH_STD) ); // guaranteed to always succeed, given an Empty healthy Sector has been found
					// . initializing the content of the assigned Sector
					::ZeroMemory( __getHealthyLogicalSectorData__(ls), BSDOS_SECTOR_LENGTH_STD ); // // guaranteed to always succeed, given an Empty healthy Sector has been found
					const PDirectoryEntry de=dirsSector.TryGetDirectoryEntry(slot);
					de->occupied=true;
					ASSERT( &de->dir.name==&de->file.stdHeader.name );
					de->file.stdHeader.SetName(zxName);
					slot->nameChecksum= dirNameChecksum>=0 ? dirNameChecksum : de->GetDirNameChecksum();
					slot->reserved2=2;
					::memset( de->dir.comment, ' ', sizeof(de->dir.comment) );
					dirsSector.MarkDirectoryEntryAsDirty(slot);
					return ERROR_SUCCESS;
				}else
					return ERROR_DISK_FULL;
		// - Root Directory full
		return ERROR_CANNOT_MAKE;
	}

	TStdWinError CBSDOS308::SwitchToDirectory(PFile slot){
		// changes CurrentDirectory; returns Windows standard i/o error
		currentDir=slot;
		return ERROR_SUCCESS;
	}

	DWORD CBSDOS308::GetDirectoryUid(PCFile slot) const{
		// determines and returns the unique identifier of the Directory specified
		return	slot!=ZX_DIR_ROOT
				? *(PDWORD)slot
				: 0;
	}

	TStdWinError CBSDOS308::MoveFileToCurrentDir(PFile file,LPCTSTR exportFileNameAndExt,PFile &rMovedFile){
		// moves given File to CurrentDirectory; returns Windows standard i/o error
		// - cannot move "to" root Directory
		if (currentDir==ZX_DIR_ROOT)
			return ERROR_DIR_NOT_ROOT;
		// - cannot move a Subdirectory
		if (IsDirectory(file))
			return ERROR_CANNOT_MAKE;
		// - allocating a new DirectoryEntry in current Subdirectory
		const PDirectoryEntry newDe=(PDirectoryEntry)TDirectoryEntry::CTraversal(this,currentDir).AllocateNewEntry();
		if (!newDe)
			return ERROR_CANNOT_MAKE;
		// - copying the DirectoryEntry
		const PDirectoryEntry oldDe=(PDirectoryEntry)file;
		*newDe=*oldDe;
		MarkDirectorySectorAsDirty(newDe);
		// - marking Old DirectoryEntry as Empty
		oldDe->occupied=false;
		MarkDirectorySectorAsDirty(oldDe);
		rMovedFile=newDe;
		return ERROR_SUCCESS;
	}








	CBSDOS308::TDirectoryEntry::TDirectoryEntry(const CBSDOS308 *bsdos,TLogSector firstSector)
		// ctor
		: reserved1(0) , special(false)
		, occupied(true) , fileHasStdHeader(false) , fileHasData(true) {
		file.reserved2=0x401;
		file.reserved3=0;
		file.firstSector=firstSector;
		CFatPath dummy( bsdos->formatBoot.GetCountOfAllSectors() );
		bsdos->GetFileFatPath( this, dummy );
		file.dataLength=BSDOS_SECTOR_LENGTH_STD*dummy.GetNumberOfItems();
	}

	BYTE CBSDOS308::TDirectoryEntry::GetDirNameChecksum() const{
		// computes and returns the Name Checksum
		return __xorChecksum__( dir.name, sizeof(dir.name) );
	}

	CBSDOS308::TDirectoryEntry::CTraversal::CTraversal(const CBSDOS308 *bsdos,PCFile slot)
		// ctor
		// - base
		: TDirectoryTraversal( slot, sizeof(TDirectoryEntry) )
		// - initialization
		, bsdos(bsdos)
		, dirFatPath( bsdos, &TDirectoryEntry(bsdos,((CDirsSector::PCSlot)slot)->firstSector) )
		, nDirSectorsTraversed(0)
		, nRemainingEntriesInSector(0) {
	}

	#define BSDOS_DIR_ENTRIES_PER_SECTOR	(BSDOS_SECTOR_LENGTH_STD/sizeof(CBSDOS308::TDirectoryEntry))

	bool CBSDOS308::TDirectoryEntry::CTraversal::AdvanceToNextEntry(){
		// True <=> found another entry in current Directory (Empty or not), otherwise False
		// - getting the next LogicalSector with Directory
		const bool isDirNameEntry=(nDirSectorsTraversed|nRemainingEntriesInSector)==0;
		if (!nRemainingEntriesInSector){
			CFatPath::PCItem items; DWORD nItems;
			dirFatPath.GetItems(items,nItems);
			if (nDirSectorsTraversed==nItems){ // end of Directory or FatPath invalid
				entryType=TDirectoryTraversal::END;
				return false;
			}
			entry=bsdos->image->GetHealthySectorData( chs=items[nDirSectorsTraversed++].chs );
			if (!entry){ // Sector not found
				entryType=TDirectoryTraversal::WARNING, warning=ERROR_SECTOR_NOT_FOUND;
				return true;
			}else
				entry=(PDirectoryEntry)entry-1; // pointer set "before" the first DirectoryEntry
			nRemainingEntriesInSector=BSDOS_DIR_ENTRIES_PER_SECTOR;
		}
		// - getting the next DirectoryEntry
		entry=(PDirectoryEntry)entry+1;
		if (isDirNameEntry)
			entryType=TDirectoryTraversal::CUSTOM;
		else if (!((PDirectoryEntry)entry)->occupied)
			entryType=TDirectoryTraversal::EMPTY;
		else if (directory==ZX_DIR_ROOT)
			entryType=TDirectoryTraversal::SUBDIR;
		else
			entryType=TDirectoryTraversal::FILE;
		nRemainingEntriesInSector--;
		return true;
	}

	void CBSDOS308::TDirectoryEntry::CTraversal::ResetCurrentEntry(BYTE directoryFillerByte) const{
		// gets current entry to the state in which it would be just after formatting
		if (entry)
			reinterpret_cast<PDirectoryEntry>( ::memset(entry,directoryFillerByte,entrySize) )->occupied=false;
	}

	CDos::PFile CBSDOS308::TDirectoryEntry::CTraversal::AllocateNewEntry(){
		// allocates new DirectoryEntry at the end of CurrentDirectory and returns the DirectoryEntry; returns Null if new DirectoryEntry cannot be allocated (e.g. because disk is full)
		// - reusing the first empty DirectoryEntry from current position
		while (AdvanceToNextEntry())
			if (entry && !((PDirectoryEntry)entry)->occupied)
				return entry;
		// - allocating a new Directory Sector and returning the first DirectoryEntry in it
		TPhysicalAddress chs;
		if (bsdos->GetFirstEmptyHealthySector(true,chs)!=ERROR_SUCCESS)
			return nullptr; // new healthy Sector couldn't be allocated
		const TLogSector newLogSector=bsdos->__fyzlog__(chs);
		CFatPath::PCItem pItem; DWORD n;
		dirFatPath.GetItems(pItem,n);
		if (bsdos->__setLogicalSectorFatItem__( // may fail if the last Sector in Directory is Bad
				bsdos->__fyzlog__(pItem[n-1].chs),
				TFatValue( true, true, newLogSector )
			)
		)
			return nullptr;
		bsdos->__setLogicalSectorFatItem__( // guaranteed to always succeed, given an Empty healthy Sector has been found
			newLogSector,
			TFatValue( true, false, BSDOS_SECTOR_LENGTH_STD )
		);
		// - resetting all DirectoryEntries and returning the first one
		return ::ZeroMemory( bsdos->__getHealthyLogicalSectorData__(newLogSector), BSDOS_SECTOR_LENGTH_STD );
	}

	std::unique_ptr<CDos::TDirectoryTraversal> CBSDOS308::BeginDirectoryTraversal(PCFile directory) const{
		// initiates exploration of specified Directory through a DOS-specific DirectoryTraversal
		if (directory==ZX_DIR_ROOT)
			// traversing the root Directory
			return std::unique_ptr<TDirectoryTraversal>( new CDirsSector::CTraversal(this) );
		else
			// traversing a Subdirectory
			return std::unique_ptr<TDirectoryTraversal>( new TDirectoryEntry::CTraversal(this,directory) );
	}







	#define HEADERLESS_TYPE			_T("Headerless")
	#define HEADERLESS_N_A			_T("N/A")

	bool CBSDOS308::GetFileNameOrExt(PCFile file,PPathString pOutName,PPathString pOutExt) const{
		// populates the Buffers with File's name and extension; caller guarantees that the Buffer sizes are at least MAX_PATH characters each
		if (file==ZX_DIR_ROOT){
			// root Directory
			if (pOutName)
				*pOutName='\\';
			if (pOutExt)
				*pOutExt=_T("");
		}else if (IsDirectory(file)){
			// root Subdirectory
			if (pOutName)
				if (const PCDirectoryEntry de=dirsSector.TryGetDirectoryEntry( (CDirsSector::PCSlot)file )){
					// Directory's name can be retrieved
					ASSERT( &de->dir.name==&de->file.stdHeader.name );
					de->file.stdHeader.GetNameOrExt( pOutName, nullptr );
				}else
					// Directory's first Sector is unreadable
					*pOutName=BSDOS_DIR_CORRUPTED;
			if (pOutExt)
				*pOutExt=_T("");
		}else{
			// File
			const PCDirectoryEntry de=(PCDirectoryEntry)file;
			if (de->fileHasStdHeader)
				// File with a Header
				de->file.stdHeader.GetNameOrExt( pOutName, pOutExt );
			else{
				// Headerless File or Fragment
				if (pOutName){
					TCHAR bufName[16];
					::wsprintf( bufName, _T("%08d"), idHeaderless++ ); // ID padded with zeros to eight digits (to make up an acceptable name even for TR-DOS)
					*pOutName=bufName;
				}
				if (pOutExt)
					*pOutExt=TUniFileType::HEADERLESS;
				return false; // name irrelevant
			}
		}
		return true; // name relevant
	}

	TStdWinError CBSDOS308::ChangeFileNameAndExt(PFile file,RCPathString newName,RCPathString newExt,PFile &rRenamedFile){
		// tries to change given File's name and extension; returns Windows standard i/o error
		if (file==ZX_DIR_ROOT)
			// root Directory
			return ERROR_DIR_NOT_ROOT; // can't rename root Directory
		else if (IsDirectory(file)){
			// root Subdirectory
			// . making sure that a Directory with given NameAndExtension doesn't yet exist
			// commented out as Directories are identified by their index in the DIRS Sector rather than by name
			//if ( rRenamedFile=__findFileInCurrDir__(newName,_T(""),file) )
				//return ERROR_FILE_EXISTS;
			// . renaming
			if (const PDirectoryEntry de=dirsSector.TryGetDirectoryEntry( (CDirsSector::PCSlot)file )){
				// Directory's name can be changed
				ASSERT( &de->dir.name==&de->file.stdHeader.name );
				if (const TStdWinError err=de->file.stdHeader.SetName(newName))
					return err;
				dirsSector.MarkDirectoryEntryAsDirty( (CDirsSector::PCSlot)( rRenamedFile=file ) );
				return ERROR_SUCCESS;
			}else
				// Directory's first Sector is unreadable
				return ERROR_SECTOR_NOT_FOUND; // we shouldn't end up here, but just to be sure
		}else{
			// File with Header
			const PDirectoryEntry de=(PDirectoryEntry)file;
			if (de->fileHasStdHeader){
				// File with Header
				// . Extension must be specified
				if (newExt.GetLength()<1)
					return ERROR_BAD_FILE_TYPE;
				// . making sure that a File with given NameAndExtension doesn't yet exist
				if ( rRenamedFile=FindFileInCurrentDir(newName,newExt,file) )
					return ERROR_FILE_EXISTS;
				// . renaming
				if (const TStdWinError err=de->file.stdHeader.SetName(newName))
					return err;
				if (!de->file.stdHeader.SetFileType((TUniFileType)*newExt))
					de->file.stdHeader.type=(TZxRom::TFileType)*newExt;
				MarkDirectorySectorAsDirty(de);
			}//else
				// Headerless File or Fragment
				//nop - simply ignoring the request (Success to be able to create Headerless File copies)
			rRenamedFile=de;
			return ERROR_SUCCESS;
		}
	}

	DWORD CBSDOS308::GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData,TGetFileSizeOptions option) const{
		// determines and returns the size of specified File
		if (pnBytesReservedBeforeData) *pnBytesReservedBeforeData=0;
		if (pnBytesReservedAfterData) *pnBytesReservedAfterData=0;
		if (file==ZX_DIR_ROOT)
			// root Directory is officially consists only of the DIRS Sector
			return BSDOS_SECTOR_LENGTH_STD;
		else if (IsDirectory(file))
			// root Subdirectory
			return TDirectoryEntry( this, ((CDirsSector::PCSlot)file)->firstSector ).file.dataLength;
		else{
			// File
			const PCDirectoryEntry de=(PCDirectoryEntry)file;
			switch (option){
				case TGetFileSizeOptions::OfficialDataLength:
					return de->file.dataLength;
				case TGetFileSizeOptions::SizeOnDisk:
					return (de->file.dataLength+BSDOS_SECTOR_LENGTH_STD-1)/BSDOS_SECTOR_LENGTH_STD * BSDOS_SECTOR_LENGTH_STD;
				default:
					ASSERT(FALSE);
					return 0;
			}
		}
	}

	void CBSDOS308::GetFileTimeStamps(PCFile file,LPFILETIME pCreated,LPFILETIME pLastRead,LPFILETIME pLastWritten) const{
		// given specific File, populates the Created, LastRead, and LastWritten outputs
		if (pCreated){
			const PCDirectoryEntry de =	IsDirectory(file)
										? dirsSector.TryGetDirectoryEntry( (CDirsSector::PCSlot)file )
										: (PCDirectoryEntry)file;
			*pCreated =	de!=nullptr
						? CMSDOS7::TDateTime(de->dateTimeCreated)
						: TFileDateTime::None;
		}
		if (pLastRead)
			*pLastRead=TFileDateTime::None;
		if (pLastWritten)
			*pLastWritten=TFileDateTime::None;
	}

	void CBSDOS308::SetFileTimeStamps(PFile file,const FILETIME *pCreated,const FILETIME *pLastRead,const FILETIME *pLastWritten){
		// translates the Created, LastRead, and LastWritten intputs into this DOS File time stamps
		if (pCreated){
			const PDirectoryEntry de =	IsDirectory(file)
										? dirsSector.TryGetDirectoryEntry( (CDirsSector::PSlot)file )
										: (PDirectoryEntry)file;
			if (de!=nullptr)
				CMSDOS7::TDateTime(*pCreated).ToDWord( &de->dateTimeCreated );
		}
	}

	TStdWinError CBSDOS308::DeleteFile(PFile file){
		// deletes specified File; returns Windows standard i/o error
		// - can't delete the root Directory
		if (file==ZX_DIR_ROOT)
			return ERROR_ACCESS_DENIED;
		// - deleting a File or root Subdirectory
		CFatPath::PCItem p; DWORD n;
		if (IsDirectory(file)){
			// root Subdirectory
			// . a non-existing Subdirectory is successfully deleted
			const CDirsSector::PSlot slot=(CDirsSector::PSlot)file;
			if (!slot->subdirExists)
				return ERROR_SUCCESS;
			// . getting the Sectors associated with the Subdirectory
			const CFatPath sectors( this, &TDirectoryEntry(this,slot->firstSector) );
			// . if problem with FatPath, the Subdirectory cannot be deleted
			if (const LPCTSTR errMsg=sectors.GetItems(p,n)){
				ShowFileProcessingError(slot,errMsg);
				return ERROR_GEN_FAILURE;
			}
			// . deleting all Files
			for( TDirectoryEntry::CTraversal dt(this,slot); dt.GetNextFileOrSubdir(); )
				switch (dt.entryType){
					case TDirectoryTraversal::FILE:
						// a File must be successfully deletable
						if (const TStdWinError err=DeleteFile(dt.entry))
							return err;
						break;
					case TDirectoryTraversal::WARNING:
						// cannot continue if Directory corrupted
						return dt.warning;
					default:
						return ERROR_GEN_FAILURE;
				}
			// . marking associated Sectors as Empty
			while (n--)
				__setLogicalSectorFatItem__( p++->value, TFatValue::SectorEmpty );
			// . marking Slot as empty
			*slot=CDirsSector::TSlot::Empty;
			dirsSector.MarkAsDirty();
		}else{
			// File
			// . a non-existing File is successfully deleted
			const PDirectoryEntry de=(PDirectoryEntry)file;
			if (!de->occupied)
				return ERROR_SUCCESS; // a non-existing File is successfully deleted
			// . getting the Sectors associated with the File
			const CFatPath sectors( this, de );
			// . if problem with FatPath, the Subdirectory cannot be deleted
			if (const LPCTSTR errMsg=sectors.GetItems(p,n)){
				ShowFileProcessingError(de,errMsg);
				return ERROR_GEN_FAILURE;
			}
			// . marking associated Sectors as Empty
			while (n--)
				__setLogicalSectorFatItem__( p++->value, TFatValue::SectorEmpty );
			// . marking DirectoryEntry as empty
			de->occupied=false;
			MarkDirectorySectorAsDirty(de);
		}
		// - successfully deleted
		return ERROR_SUCCESS;
	}

	CString CBSDOS308::GetFileExportNameAndExt(PCFile file,bool shellCompliant) const{
		// returns File name concatenated with File extension for export of the File to another Windows application (e.g. Explorer)
		CString result=__super::GetFileExportNameAndExt(file,shellCompliant);
		if (!shellCompliant){
			// exporting to another RIDE instance
			TCHAR buf[80];
			if (IsDirectory(file)){
				// root Subdirectory
				const CDirsSector::PCSlot slot=(CDirsSector::PCSlot)file;
				::wsprintf(
					buf+__exportFileInformation__( buf, TUniFileType::SUBDIRECTORY ),
					INFO_DIR, slot->nameChecksum
				);
			}else{
				// File
				const PCDirectoryEntry de=(PCDirectoryEntry)file;
				if (de->fileHasStdHeader)
					// File with a header
					__exportFileInformation__( buf, de->file.stdHeader.GetUniFileType(), de->file.stdHeader.params, de->file.dataLength, de->file.dataFlag );
				else
					// Headerless File
					__exportFileInformation__( buf, TUniFileType::HEADERLESS, TStdParameters::Default, de->file.dataLength, de->file.dataFlag );
			}
			result+=buf;
		}
		return result;
	}

	TStdWinError CBSDOS308::ImportFile(CFile *fIn,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile){
		// imports specified File (physical or virtual) into the Image; returns Windows standard i/o error
		// - converting the NameAndExtension to the "10.1" form usable for Tape
		CPathString zxName,zxExt; LPCTSTR zxInfo;
		TCHAR buf[16384];
		__parseFat32LongName__(	::lstrcpy(buf,nameAndExtension), zxName, zxExt, zxInfo );
		zxName.TrimToLength(ZX_TAPE_FILE_NAME_LENGTH_MAX);
		zxExt.TrimToLength(1);
		// - importing
		TUniFileType uts;
		const int n=__importFileInformation__( zxInfo, uts );
		if (currentDir==ZX_DIR_ROOT){
			// root Directory
			// . only Subdirectories are allowed as items in the root Directory
			if (uts!=TUniFileType::SUBDIRECTORY)
				return ERROR_CANNOT_MAKE;
			// . processing import information
			int dirNameChecksum=-1;
			_stscanf( zxInfo+n, INFO_DIR, &dirNameChecksum );
			// . creating a Subdirectory with given name
			if (const TStdWinError err=CreateSubdirectory( zxName, FILE_ATTRIBUTE_DIRECTORY, rFile ))
				return err;
			if (dirNameChecksum>=0)
				( (CDirsSector::PSlot)rFile )->nameChecksum=dirNameChecksum;
		}else{
			// File
			// . only Files are allowed as items in root Subdirectories
			if (uts==TUniFileType::SUBDIRECTORY)
				return ERROR_CANNOT_MAKE;
			// . initializing the description of File to import
			TDirectoryEntry tmp( this, 0 );
				// : import information
				DWORD dw;
				__importFileInformation__( zxInfo, uts, tmp.file.stdHeader.params, dw, tmp.file.dataFlag );
				// : name and extension
				tmp.fileHasStdHeader=uts!=TUniFileType::HEADERLESS; // and ChangeFileNameAndExt called below by __importFileData__
				// : size
				tmp.file.stdHeader.length = dw ? dw : fileSize;
				tmp.file.dataLength=fileSize;
				// : first logical Sector
				//nop (see below)
			// . changing the Extension according to the "universal" type valid across ZX platforms (as TR-DOS File "Picture.C" should be take on the name "Picture.B" under MDOS)
			TCHAR uftExt;
			switch (uts){
				case TUniFileType::PROGRAM	:
				case TUniFileType::NUMBER_ARRAY:
				case TUniFileType::CHAR_ARRAY:
				case TUniFileType::BLOCK	: uftExt=uts; break;
				case TUniFileType::SCREEN	: uftExt=TUniFileType::BLOCK; break;
				default:
					uftExt= zxExt.GetLength() ? *zxExt : TZxRom::TFileType::HEADERLESS;
					break;
			}
			// . importing to Image
			CFatPath fatPath( this, fileSize );
			if (const TStdWinError err=__importFileData__( fIn, &tmp, zxName, uftExt, fileSize, true, rFile, fatPath ))
				return err;
			// . recording the FatPath in FAT
			ModifyFileFatPath( rFile, fatPath );
		}
		// - successfully imported to the Image
		return ERROR_SUCCESS;
	}






	TStdWinError CBSDOS308::CreateUserInterface(HWND hTdi){
		// creates DOS-specific Tabs in TDI; returns Windows standard i/o error
		if (const TStdWinError err=__super::CreateUserInterface(hTdi))
			return err;
		CTdiCtrl::AddTabLast( hTdi, TRACK_MAP_TAB_LABEL, &trackMap.tab, false, TDI_TAB_CANCLOSE_NEVER, nullptr );
		CTdiCtrl::AddTabLast( hTdi, BOOT_SECTOR_TAB_LABEL, &boot.tab, false, TDI_TAB_CANCLOSE_NEVER, nullptr );
		CTdiCtrl::AddTabLast( hTdi, FILE_MANAGER_TAB_LABEL, &fileManager.tab, true, TDI_TAB_CANCLOSE_NEVER, nullptr );
		return ERROR_SUCCESS;
	}
	CDos::TCmdResult CBSDOS308::ProcessCommand(WORD cmd){
		// returns the Result of processing a DOS-related command
		return __super::ProcessCommand(cmd);
	}

	CBSDOS308::TLogSector CBSDOS308::__getNextHealthySectorWithoutFat__(TLogSector &rStart,TLogSector end) const{
		// determines and returns the first LogicalSector after Start that is well readable; returns 0 if no such LogicalSector exists
		while (++rStart<end)
			if (__getHealthyLogicalSectorData__(rStart))
				return rStart;
		return 0;
	}

/*	void CBSDOS308::InitializeEmptyMedium(CFormatDialog::PCParameters params){
		// initializes a fresh formatted Medium (Boot, FAT, root dir, etc.)
		// - initializing the Boot Sector
		const PBootSector boot=TBootSector::GetData(image);
		if (!boot) // Boot Sector may not be found
			return;
		::ZeroMemory( boot, sizeof(TBootSector) );
			// . signatures
			boot->signature1=0x02;
			//boot->signature2 = boot->signature3 = 0x00; // commented out as see ZeroMemory above
			// . bootstrap
			boot->jmpInstruction.opCode=0x18; // "jr N" instruction
			// . geometry
			FlushToBootSector();
			// . date this disk was formatted
			CMSDOS7::TDateTime( CMSDOS7::TDateTime::GetCurrent() ).ToDWord(&boot->formattedDateTime);
			// . label
			::memcpy(	::memset( boot->diskName, ' ', sizeof(boot->diskName) ),
						VOLUME_LABEL_DEFAULT_ANSI_8CHARS,
						sizeof(VOLUME_LABEL_DEFAULT_ANSI_8CHARS)-1
					);
			// . disk ID
			//TODO
		// - FAT
		TLogSector ls=1;
		const TLogSector nSectorsTotal=formatBoot.GetCountOfAllSectors();
		boot->nSectorsPerFat=(nSectorsTotal+BSDOS_FAT_ITEMS_PER_SECTOR-1)/BSDOS_FAT_ITEMS_PER_SECTOR;
		boot->nBytesInFat=boot->nSectorsPerFat*BSDOS_SECTOR_LENGTH_STD;
		TFatValue fatFirstSector[BSDOS_FAT_ITEMS_PER_SECTOR];
			::ZeroMemory( fatFirstSector, sizeof(fatFirstSector) );
			fatFirstSector[1]=TFatValue::SystemSector;
		for( BYTE fatCopy=0; fatCopy<params->nAllocationTables; fatCopy++ ){
			if (boot->fatStarts[fatCopy]=__getNextHealthySectorWithoutFat__(ls,BSDOS_FAT_ITEMS_PER_SECTOR))
				for( BYTE nSectorsPerFatCopy=boot->nSectorsPerFat; --nSectorsPerFatCopy; ){
					const TLogSector curr=ls;
					if (const TLogSector next=__getNextHealthySectorWithoutFat__(ls,BSDOS_FAT_ITEMS_PER_SECTOR))
						fatFirstSector[curr]=TFatValue(true,true,next);
					else
						return;
				}
			else
				return;
			fatFirstSector[ls]=TFatValue(true,false,BSDOS_SECTOR_LENGTH_STD);
		}
		::memcpy(	__getHealthyLogicalSectorData__(boot->fatStarts[0]),
					fatFirstSector,
					sizeof(fatFirstSector)
				);
		__setLogicalSectorFatItem__( 0, TFatValue( MAKEWORD(0,__getFatChecksum__(0)) ) );
		switch (params->nAllocationTables){
			case 1:
				// just a single FAT copy wanted
				boot->fatStarts[1]=boot->fatStarts[0]; // formally setting the second copy identical to the first one
				break;
			case 2:
				// two FAT copies wanted
				::memcpy(	__getHealthyLogicalSectorData__(boot->fatStarts[1]),
							fatFirstSector,
							sizeof(fatFirstSector)
						);
				__setLogicalSectorFatItem__( 0, TFatValue( MAKEWORD(0,__getFatChecksum__(1)) ) );
				break;
			default:
				ASSERT(FALSE); // we shouldn't end up here!
		}
		// - root Directory
		if (boot->dirsLogSector=__getNextHealthySectorWithoutFat__(ls,nSectorsTotal))
			__setLogicalSectorFatItem__( ls, TFatValue(true,false,BSDOS_SECTOR_LENGTH_STD) );
		else
			return;
	}
*/
	void CBSDOS308::InitializeEmptyMedium(CFormatDialog::PCParameters params){
		// initializes a fresh formatted Medium (Boot, FAT, root dir, etc.)
		// - initializing the Boot Sector
		const PBootSector boot=TBootSector::GetData(image);
		if (!boot) // Boot Sector may not be found
			return;
		::ZeroMemory( boot, sizeof(TBootSector) );
			// . signatures
			boot->signature1=0x02;
			//boot->signature2 = boot->signature3 = 0x00; // commented out as see ZeroMemory above
			// . bootstrap
			boot->jmpInstruction.opCode=0x18; // "jr N" instruction
			// . geometry
			FlushToBootSector();
			// . date this disk was formatted
			CMSDOS7::TDateTime( CMSDOS7::TDateTime::GetCurrent() ).ToDWord(&boot->formattedDateTime);
			// . label
			::memcpy(	::memset( boot->diskName, ' ', sizeof(boot->diskName) ),
						VOLUME_LABEL_DEFAULT_ANSI_8CHARS,
						sizeof(VOLUME_LABEL_DEFAULT_ANSI_8CHARS)-1
					);
			// . disk ID
			Utils::RandomizeData( boot->diskId, sizeof(boot->diskId) );
			boot->diskIdChecksum=__xorChecksum__( boot->diskId, sizeof(boot->diskId) );
		// - FAT
		TLogSector ls=1;
		const TLogSector nSectorsTotal=formatBoot.GetCountOfAllSectors();
		boot->nSectorsPerFat=(nSectorsTotal+BSDOS_FAT_ITEMS_PER_SECTOR-1)/BSDOS_FAT_ITEMS_PER_SECTOR;
		boot->nBytesInFat=boot->nSectorsPerFat*BSDOS_SECTOR_LENGTH_STD;
		TFatValue fatFirstSector[BSDOS_FAT_ITEMS_PER_SECTOR];
			::ZeroMemory( fatFirstSector, sizeof(fatFirstSector) );
			fatFirstSector[1]=TFatValue::SystemSector;
		for( BYTE fatCopy=0,fatSectors[UCHAR_MAX]; fatCopy<BSDOS_FAT_COPIES_MAX; fatCopy++ ){
			if (fatCopy<params->nAllocationTables)
				for( BYTE s=0; s<boot->nSectorsPerFat; s++ )
					if (!( fatSectors[s]=__getNextHealthySectorWithoutFat__(ls,UCHAR_MAX) ))
						return;
			TLogSector curr = boot->fatStarts[fatCopy] = fatSectors[0];
			for( BYTE s=1; s<boot->nSectorsPerFat; s++ ){
				const TLogSector next=fatSectors[s];
				fatFirstSector[curr]=TFatValue( true, true, boot->fatSectorsListing[fatCopy+2*(s-1)]=next );
				curr=next;
			}
			fatFirstSector[ls]=TFatValue(true,false,BSDOS_SECTOR_LENGTH_STD);
		}
		::memcpy(	__getHealthyLogicalSectorData__(boot->fatStarts[0]),
					fatFirstSector,
					sizeof(fatFirstSector)
				);
		::memcpy(	__getHealthyLogicalSectorData__(boot->fatStarts[1]),
					fatFirstSector,
					sizeof(fatFirstSector)
				);
		__setLogicalSectorFatItem__( 0, TFatValue( MAKEWORD(0,__getFatChecksum__(0)) ) ); // both FAT copies are the same at the moment, hence getting checksum of one of them
		for( TLogSector lsUnknown=boot->nBytesInFat/sizeof(TFatValue); lsUnknown>nSectorsTotal; __setLogicalSectorFatItem__(--lsUnknown,TFatValue::SectorUnknown) );
		// - root Directory
		if (boot->dirsLogSector=__getNextHealthySectorWithoutFat__(ls,nSectorsTotal))
			__setLogicalSectorFatItem__( ls, TFatValue(true,false,BSDOS_SECTOR_LENGTH_STD) );
		else
			return;
		// - marking unhealthy Sectors encountered thus far as Bad
		while (--ls>=2)
			if (__getLogicalSectorFatItem__(ls)==TFatValue::SectorEmpty) // an unhealthy Empty Sector
				__setLogicalSectorFatItem__( ls, TFatValue::SectorErrorInDataField );
	}

	bool CBSDOS308::ValidateFormatChangeAndReportProblem(bool reformatting,PCFormat f) const{
		// True <=> specified Format is acceptable, otherwise False (and informing on error)
		// - base
		if (!__super::ValidateFormatChangeAndReportProblem(reformatting,f))
			return false;
		// - adjusting the size of FAT by either adding extra Sectors to it (if formatting) or removing existing Sectors from it (if unformatting)
		if (reformatting){
			// NOT formatting from scratch, just reformatting some Cylinders
			// . BootSector must exist
			const PCBootSector bootSector=boot.GetSectorData();
			if (!bootSector)
				return false;
			// . collecting information for the upcoming Format change
			const DWORD nNewSectorsTotal=f->GetCountOfAllSectors();
			const BYTE nNewFatSectors=(nNewSectorsTotal+BSDOS_FAT_ITEMS_PER_SECTOR-1)/BSDOS_FAT_ITEMS_PER_SECTOR;
			// . the count of Sectors per one FAT copy cannot increase
			if (nNewFatSectors>bootSector->nSectorsPerFat){
				Utils::Information(_T("Not all new cylinders can be added in FAT."));
				return false;
			}
		}//else
			// formatting from scratch (e.g. an empty disk)
			//nop
		// - new Format is acceptable
		return true;
	}







	namespace MBD{
		static PImage __instantiate__(){
			return new CImageRaw( &Properties, true );
		}
		const CImage::TProperties Properties={
			_T("MB-02 image"), // name
			__instantiate__, // instantiation function
			_T("*.mbd"), // filter
			TMedium::FLOPPY_ANY, // supported media
			BSDOS_SECTOR_LENGTH_STD,BSDOS_SECTOR_LENGTH_STD	// min and max sector length
		};
	}
