#include "stdafx.h"
#include "BSDOS.h"

	CBSDOS308::CBSDOS308(PImage image,PCFormat pFormatBoot)
		// ctor
		// - base
		: CSpectrumDos( image, pFormatBoot, TTrackScheme::BY_CYLINDERS, &Properties, IDR_BSDOS, &fileManager, TGetFileSizeOptions::OfficialDataLength, TSectorStatus::UNKNOWN )
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
		switch (cmd){
			case ID_DOS_VERIFY:{
				// volume verification
				static const TVerificationFunctions vf={
					TBootSector::Verification_thread, // Boot Sector
					nullptr, // FAT readability
					TVerificationFunctions::ReportOnFilesWithBadFatPath_thread, // FAT Files OK
					TVerificationFunctions::FloppyCrossLinkedFilesVerification_thread, // FAT crossed Files
					TVerificationFunctions::FloppyLostSectorsVerification_thread, // FAT lost allocation units
					nullptr, // Filesystem
					TVerificationFunctions::WholeDiskSurfaceVerification_thread // Volume surface
				};
				__verifyVolume__(
					CVerifyVolumeDialog( TSpectrumVerificationParams(this,vf) )
				);
				return TCmdResult::DONE_REDRAW;
			}
		}
		return __super::ProcessCommand(cmd);
	}

	CBSDOS308::TLogSector CBSDOS308::__getNextHealthySectorWithoutFat__(TLogSector &rStart,TLogSector end) const{
		// determines and returns the first LogicalSector after Start that is well readable; returns 0 if no such LogicalSector exists
		while (++rStart<end)
			if (__getHealthyLogicalSectorData__(rStart))
				return rStart;
		return 0;
	}

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
		TFatValue fatFirstSector[BSDOS_FAT_LOGSECTOR_MAX];
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
			const PBootSector bootSector=boot.GetSectorData();
			if (!bootSector)
				return false;
			// . briefly checking the state of FATs
			#define MSG_MAIN		_T("Can't change disk format")
			#define MSG_SUGGESTION	_T("Run disk verification and try again.")
			TDirectoryEntry deFats[]={ TDirectoryEntry(this,bootSector->fatStarts[0]), TDirectoryEntry(this,bootSector->fatStarts[1]) };
			CFatPath fats[]={ CFatPath(this,&deFats[0]), CFatPath(this,&deFats[1]) };
			if (fats[0].GetNumberOfItems()!=fats[1].GetNumberOfItems()
				||
				bootSector->fatStarts[0]>=BSDOS_FAT_LOGSECTOR_MAX || bootSector->fatStarts[1]>=BSDOS_FAT_LOGSECTOR_MAX
				||
				!fats[0].AreAllSectorsReadable(this) || !fats[1].AreAllSectorsReadable(this)
			){
				Utils::Information(MSG_MAIN,_T("FATs don't seem intact"),MSG_SUGGESTION);
				return false;
			}
			if (::memcmp( __getHealthyLogicalSectorData__(bootSector->fatStarts[0]), __getHealthyLogicalSectorData__(bootSector->fatStarts[1]), BSDOS_SECTOR_LENGTH_STD )){ // FAT copies are not identical (readability guaranteed by above actions); comparing their first Sectors suffices for this operation
				Utils::Information(MSG_MAIN,_T("FATs are not identical"),MSG_SUGGESTION);
				return false;
			}
			// . collecting information for the upcoming Format change
			const DWORD nNewSectorsTotal=f->GetCountOfAllSectors();
			const WORD nNewFatSectors=(nNewSectorsTotal+BSDOS_FAT_ITEMS_PER_SECTOR-1)/BSDOS_FAT_ITEMS_PER_SECTOR;
			// . adjusting the FAT
			deFats[0].file.dataLength = deFats[1].file.dataLength = nNewFatSectors*BSDOS_SECTOR_LENGTH_STD;
			if (nNewFatSectors<bootSector->nSectorsPerFat)
				// new FAT is shorter than the original one
				for( BYTE i=0; i<BSDOS_FAT_COPIES_MAX; i++ ){
					while (fats[i].GetNumberOfItems()>nNewFatSectors)
						ModifyStdSectorStatus( fats[i].PopItem()->chs, TSectorStatus::EMPTY );
					ModifyFileFatPath( &deFats[i], fats[i] ); // guaranteed to succeed
				}
			else if (nNewFatSectors>bootSector->nSectorsPerFat){
				// new FAT is longer than the original one - TRANSACTIONALLY allocating new Sectors to each of FAT copies
				bool allowFileFragmentation=false;
				CFatPath fats[]={ CFatPath(this,&deFats[0]), CFatPath(this,&deFats[1]) };
				for( BYTE i=0; i<BSDOS_FAT_COPIES_MAX; i++ )
					for( CFatPath::TItem item; fats[i].GetNumberOfItems()<nNewFatSectors; )
						if (const TLogSector ls=__getEmptyHealthyFatSector__(allowFileFragmentation)){
							// new healthy FAT Sector successfully allocated
							__setLogicalSectorFatItem__( ls, TFatValue::SectorErrorInDataField ); // reserving the Sector
							bootSector->fatSectorsListing[(fats[i].GetNumberOfItems()-1)*2+i]=ls;
							item.chs=__logfyz__(ls);
							fats[i].AddItem(&item);
						}else if (allowFileFragmentation){
							// no new healthy FAT Sectors can be allocated despite possibility to fragment Files
							Utils::FatalError( _T("Can't allocate necessary FAT sectors"), ERROR_FILE_SYSTEM_LIMITATION, MSG_SUGGESTION );
							return false;
						}else
							// no new healthy FAT Sectors can be allocated without fragmenting some Files
							if (Utils::QuestionYesNo( _T("Can't allocate necessary FAT sectors.\nFragment some files in favor of FAT?"), MB_DEFBUTTON2 ))
								allowFileFragmentation=true; // allowing fragmentation and trying again
							else
								return false;
				for( BYTE i=0; i<BSDOS_FAT_COPIES_MAX; i++ ){
					for( WORD s=bootSector->nSectorsPerFat; s<nNewFatSectors; s++ )
						::ZeroMemory(
							__getHealthyLogicalSectorData__(__fyzlog__(fats[i].GetItem(s)->chs)), // guaranteed to succeed
							BSDOS_SECTOR_LENGTH_STD
						);
					ModifyFileFatPath( &deFats[i], fats[i] ); // guaranteed to succeed
				}
				ModifyFileFatPath( &deFats[0], fats[0] ); // letting second FAT copy know Sectors allocated to first FAT copy; guaranteed to succeed
			}
			bootSector->nBytesInFat=( bootSector->nSectorsPerFat=nNewFatSectors )*BSDOS_SECTOR_LENGTH_STD;
			boot.MarkSectorAsDirty();
			__setLogicalSectorFatItem__( 0, TFatValue( MAKEWORD(0,__getFatChecksum__(0)) ) ); // both FAT copies are the same at the moment, hence getting checksum of one of them
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
