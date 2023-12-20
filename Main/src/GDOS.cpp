#include "stdafx.h"
#include "GDOS.h"

	#define INI_GDOS	_T("GDOS")

	#define INI_ALLOW_ZERO_LENGTH_FILES	_T("fm0files")

	CGDOS::TSectorInfo::TSectorInfo(){
		// ctor
	}

	CGDOS::TSectorInfo::TSectorInfo(BYTE cyl,BYTE head,BYTE sector)
		// ctor
		: track((head!=0)<<7|cyl) , sector(sector) {
	}

	bool CGDOS::TSectorInfo::operator<(const TSectorInfo &rsi2) const{
		// True <=> this Sector physically preceeds Sector2 on the disk, otherwise False
		return	track<rsi2.track
				||
				track==rsi2.track && sector<rsi2.sector;
	}

	bool CGDOS::TSectorInfo::__isValid__() const{
		// True <=> information on this Sector is valid, otherwise False
		return	sector>=Properties.firstSectorNumber && sector<=GDOS_TRACK_SECTORS_COUNT // correct (not only data) Sector numbers
				&&
				(	track>=GDOS_DIR_FILES_COUNT_MAX*sizeof(TDirectoryEntry)/GDOS_SECTOR_LENGTH_STD/GDOS_TRACK_SECTORS_COUNT && track<GDOS_CYLINDERS_COUNT // correct data Cylinders for Head 0
					||
					track>=128 && track<128+GDOS_CYLINDERS_COUNT // correct data Cylinders for Head 1
				);
	}

	TPhysicalAddress CGDOS::TSectorInfo::__getChs__() const{
		// converts to and returns the PhysicalAddress of this Sector
		const BYTE cyl=track&127, head=(track&128)!=0;
		const TPhysicalAddress chs={ cyl, head, {cyl,CImage::GetActive()->dos->sideMap[head],sector,GDOS_SECTOR_LENGTH_STD_CODE} };
		return chs;
	}

	void CGDOS::TSectorInfo::__setChs__(RCPhysicalAddress chs){
		// sets this Sector according to the PhysicalAddress
		track=(chs.head!=0)<<7|chs.cylinder, sector=chs.sectorId.sector;
	}

	void CGDOS::TSectorInfo::__setEof__(){
		// marks this Sector as the last Sector of a File
		::ZeroMemory( this, sizeof(*this) );
	}










	WORD CGDOS::TDirectoryEntry::TSectorAllocationBitmap::__sectorChs2sectorId__(RCPhysicalAddress chs){
		// converts Sector's PhysicalAddress to bit sequence number in the SectorAllocationTable
		return (chs.head*GDOS_CYLINDERS_COUNT+chs.cylinder)*GDOS_TRACK_SECTORS_COUNT+chs.sectorId.sector-Properties.firstSectorNumber-GDOS_DIR_FILES_COUNT_MAX*sizeof(TDirectoryEntry)/GDOS_SECTOR_LENGTH_STD;
	}

	CGDOS::TDirectoryEntry::TSectorAllocationBitmap::TSectorAllocationBitmap(){
		// ctor
		::ZeroMemory(this,sizeof(*this));
	}

	bool CGDOS::TDirectoryEntry::TSectorAllocationBitmap::IsSectorAllocated(RCPhysicalAddress chs) const{
		// True <=> Sector with the given PhysicalAddress is Occupied, otherwise False
		const div_t d=div(__sectorChs2sectorId__(chs),8); // 8 = number of bits in a Byte
		return ( allocated[d.quot]&(1<<d.rem) )!=0;
	}

	void CGDOS::TDirectoryEntry::TSectorAllocationBitmap::SetSectorAllocation(RCPhysicalAddress chs,bool isOccupied){
		// for Sector with given PhysicalAddress sets it's Occupation bit in the Table
		const div_t d=div(__sectorChs2sectorId__(chs),8); // 8 = number of bits in a Byte
		if (isOccupied)
			allocated[d.quot]|=1<<d.rem;
		else
			allocated[d.quot]&=~(1<<d.rem);
	}

	bool CGDOS::TDirectoryEntry::TSectorAllocationBitmap::IsDisjunctiveWith(const TSectorAllocationBitmap &rSab2) const{
		// True <=> the two SectorAllocationBitmaps describe two Files with disjunctive sets of Sectors, otherwise False
		for( BYTE n=sizeof(allocated); n--; )
			if (allocated[n]&rSab2.allocated[n]) return false;
		return true;
	}

	void CGDOS::TDirectoryEntry::TSectorAllocationBitmap::MergeWith(const TSectorAllocationBitmap &rSab2){
		// merges the provided SectorAllocationBitmap with this one
		for( BYTE n=sizeof(allocated); n--; allocated[n]|=rSab2.allocated[n] );
	}










	CGDOS::CGDOS(PImage image,PCFormat pFormatBoot)
		// ctor
		// - base
		: CSpectrumDos( image, pFormatBoot, TTrackScheme::BY_SIDES, &Properties, IDR_GDOS, &fileManager, TGetFileSizeOptions::OfficialDataLength, TSectorStatus::UNAVAILABLE )
		// - initialization
		, zeroLengthFilesEnabled( __getProfileBool__(INI_ALLOW_ZERO_LENGTH_FILES,false) )
		, fileManager(this) {
	}

	void CGDOS::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_GDOS, messageId );
	}









	bool CGDOS::GetSectorStatuses(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PSectorStatus buffer) const{
		// True <=> Statuses of all Sectors in the Track successfully retrieved and populated the Buffer, otherwise False
		bool result=true; // assumption (Statuses of all Sectors successfully retrieved)
		TPhysicalAddress chs={ cyl, head };
		while (nSectors--){
			const TSector id=( chs.sectorId=*bufferId++ ).sector;
			if (cyl>=formatBoot.nCylinders || head>=formatBoot.nHeads || chs.sectorId.cylinder!=cyl || chs.sectorId.side!=sideMap[head] || id>GDOS_TRACK_SECTORS_COUNT || !id || chs.sectorId.lengthCode!=GDOS_SECTOR_LENGTH_STD_CODE) // condition for Sector must be ">", not ">=" (Sectors numbered from 1 - see also "|!id")
				*buffer++=TSectorStatus::UNKNOWN; // Sector ID out of official Format - Sector thus Unknown
			else if (cyl<GDOS_DIR_FILES_COUNT_MAX*sizeof(TDirectoryEntry)/GDOS_SECTOR_LENGTH_STD/GDOS_TRACK_SECTORS_COUNT && !head)
				*buffer++=TSectorStatus::SYSTEM;
			else{
				*buffer=TSectorStatus::EMPTY; // assumption
				for( TGdosDirectoryTraversal dt(this); dt.AdvanceToNextEntry(); )
					if (dt.entryType==TDirectoryTraversal::FILE){
						if (((PDirectoryEntry)dt.entry)->sectorAllocationBitmap.IsSectorAllocated(chs)){
							*buffer=TSectorStatus::OCCUPIED;
							break;
						}
					}else if (dt.entryType==TDirectoryTraversal::WARNING)
						result=false;
				buffer++;
			}
		}
		return result;
	}

	bool CGDOS::ModifyFileFatPath(PFile file,const CFatPath &rFatPath) const{
		// True <=> a error-free FatPath of given File successfully written, otherwise False
		CFatPath::PCItem pItem; DWORD nItems;
		if (rFatPath.GetItems(pItem,nItems)) // if FatPath erroneous ...
			return false; // ... we are done
		const PDirectoryEntry de=(PDirectoryEntry)file;
		TSectorInfo *psi=&de->firstSector;
		while (nItems--){
			psi->__setChs__(pItem->chs);
			de->sectorAllocationBitmap.SetSectorAllocation(pItem->chs,true);
			psi=&((PGdosSectorData)image->GetHealthySectorData(pItem++->chs))->nextSector;
		}
		psi->__setEof__();
		MarkDirectorySectorAsDirty(de);
		return true;
	}

	bool CGDOS::ModifyStdSectorStatus(RCPhysicalAddress chs,TSectorStatus status) const{
		// True <=> the Status of the specified DOS-standard Sector successfully changed, otherwise False
		bool result=true; // assumption (Statuses of all Sectors successfully modified)
		for( TGdosDirectoryTraversal dt(this); dt.AdvanceToNextEntry(); )
			if (dt.entryType==TDirectoryTraversal::FILE)
				((PDirectoryEntry)dt.entry)->sectorAllocationBitmap.SetSectorAllocation( chs, status!=TSectorStatus::EMPTY );
			else if (dt.entryType==TDirectoryTraversal::WARNING)
				result=false;
		return result;
	}









	bool CGDOS::GetFileFatPath(PCFile file,CFatPath &rFatPath) const{
		// True <=> FatPath of given File (even an erroneous FatPath) successfully retrieved, otherwise False
		const PCDirectoryEntry de=(PCDirectoryEntry)file;
		// - if queried about the root Directory, populating the FatPath with root Directory Sectors
		CFatPath::TItem item;
		if (de==ZX_DIR_ROOT){
			item.chs.sectorId.lengthCode=GDOS_SECTOR_LENGTH_STD_CODE;
			item.chs.cylinder = item.chs.sectorId.cylinder = 0;
			item.chs.sectorId.side=sideMap[ item.chs.head=0 ];
			item.chs.sectorId.sector=Properties.firstSectorNumber;
			while (rFatPath.AddItem(&item)){ // also sets an error in FatPath
				if (++item.chs.sectorId.sector>GDOS_TRACK_SECTORS_COUNT){
					item.chs.sectorId.sector=Properties.firstSectorNumber;
					if (( item.chs.sectorId.cylinder=++item.chs.cylinder )==GDOS_DIR_FILES_COUNT_MAX*sizeof(TDirectoryEntry)/GDOS_SECTOR_LENGTH_STD/GDOS_TRACK_SECTORS_COUNT) // end of Directory
						break; // end of Directory
				}
				item.value++;
			}
			return true;
		}
		// - no FatPath can be retrieved if DirectoryEntry is Empty
		if (de->fileType==TDirectoryEntry::EMPTY_ENTRY)
			return false;
		// - if File has no Sectors, we are done (may happen due to a failure during importing)
		if (!de->firstSector.__isValid__())
			return true;
		// - extracting the FatPath
		const TSectorInfo *psi=&de->firstSector, lastValidSector(formatBoot.nCylinders,formatBoot.nHeads,Properties.firstSectorNumber);
		do{
			// . determining Sector's PhysicalAddress
			item.chs=psi->__getChs__();
			// . adding the Item to the FatPath
			if (!rFatPath.AddItem(&item)) break; // also sets an error in FatPath
			// . VALIDATION: Value must "make sense"
			if (!(*psi<lastValidSector)){ // Sector's position on the disk
				rFatPath.error=CFatPath::TError::VALUE_INVALID;
				break;
			}
			// . VALIDATION: next Sector can be retrieved
			if (const PCGdosSectorData sd=(PCGdosSectorData)image->GetHealthySectorData(item.chs))
				psi=&sd->nextSector;
			else
				break;
		}while (psi->__isValid__()); // if invalid, we have reached the end of File
		if (rFatPath.GetNumberOfItems()!=de->nSectors)
			rFatPath.error=CFatPath::TError::SECTOR; // Sector not found
		return true;
	}

	#define EXTENSION_BASIC			_T("BAS")
	#define EXTENSION_NUMBER_ARRAY	_T("D-ARRAY")
	#define EXTENSION_CHAR_ARRAY	_T("$-ARRAY")
	#define EXTENSION_CODE			_T("CDE")
	#define EXTENSION_SNAPSHOT_48K	_T("SNP-48k")
	#define EXTENSION_MICRODRIVE	_T("MD-FILE")
	#define EXTENSION_SCREEN		_T("SCREEN$")
	#define EXTENSION_SPECIAL		_T("SPECIAL")
	#define EXTENSION_SNAPSHOT_128K	_T("SNP-128k")
	#define EXTENSION_OPENTYPE		_T("OPENTYPE")
	#define EXTENSION_EXECUTE		_T("EXECUTE")

	bool CGDOS::TDirectoryEntry::__isStandardRomFile__() const{
		// True <=> this DirectoryItem describes a standard ROM file, otherwise False
		return BASIC<=fileType&&fileType<=CODE || fileType==SCREEN;
	}

	#define EXTENSION_UNKNOWN	_T("UNKNOWN_%03d")

	LPCTSTR CGDOS::TDirectoryEntry::__getFileTypeDesc__(PTCHAR buffer) const{
		// populates the Buffer with textual representation of FileType and returns the Buffer
		switch (fileType){
			case BASIC			: return EXTENSION_BASIC;
			case NUMBER_ARRAY	: return EXTENSION_NUMBER_ARRAY;
			case CHAR_ARRAY		: return EXTENSION_CHAR_ARRAY;
			case CODE			: return EXTENSION_CODE;
			case SNAPSHOT_48K	: return EXTENSION_SNAPSHOT_48K;
			case MICRODRIVE		: return EXTENSION_MICRODRIVE;
			case SCREEN			: return EXTENSION_SCREEN;
			case SPECIAL		: return EXTENSION_SPECIAL;
			case SNAPSHOT_128K	: return EXTENSION_SNAPSHOT_128K;
			case OPENTYPE		: return EXTENSION_OPENTYPE;
			case EXECUTE		: return EXTENSION_EXECUTE;
			default:
				::wsprintf(buffer,EXTENSION_UNKNOWN,fileType);
				return buffer;
		}
	}

	void CGDOS::TDirectoryEntry::GetNameOrExt(PPathString pOutName,PPathString pOutExt) const{
		// populates the Buffers with File's name and extension; caller guarantees that the Buffer sizes are at least MAX_PATH characters each
		if (pOutName)
			(  *pOutName=CPathString( name, sizeof(name) )  ).TrimRightSpace();
		if (pOutExt)
			*pOutExt=fileType;
	}

	bool CGDOS::GetFileNameOrExt(PCFile file,PPathString pOutName,PPathString pOutExt) const{
		// populates the Buffers with File's name and extension; caller guarantees that the Buffer sizes are at least MAX_PATH characters each
		if (file==ZX_DIR_ROOT){
			if (pOutName)
				*pOutName=CPathString::Root;
			if (pOutExt)
				*pOutExt=CPathString::Empty;
		}else
			((PCDirectoryEntry)file)->GetNameOrExt(pOutName,pOutExt);
		return true; // name relevant
	}

	CDos::CPathString CGDOS::GetFilePresentationNameAndExt(PCFile file) const{
		// returns File name concatenated with File extension for presentation of the File to the user
		CPathString result=__super::GetFilePresentationNameAndExt(file);
		result.DetachExtension(); // detach e.g. ".%07" ...
		TCHAR bufExt[16];
		return result.AppendDotExtensionIfAny( ((PCDirectoryEntry)file)->__getFileTypeDesc__(bufExt) ); // ... and replace it with e.g. ".SCREEN$"
	}

	void CGDOS::TDirectoryEntry::SetNameAndExt(RCPathString newName,RCPathString newExt){
		// sets File's Name and Type based on the Buffer content
		// - setting the Name trimmed to 10 characters at most
		newName.MemcpyAnsiTo( name, sizeof(name), ' ' );
		// - setting FileType
		fileType=(TFileType)newExt.FirstCharA();
		// - setting up StandardParameters for a StandardZxType
		//nop (up to the caller)
	}

	PWORD CGDOS::TDirectoryEntry::__getStdParameter1__(){
		// returns StandardParameter1 (if any)
		switch (fileType){
			case TDirectoryEntry::BASIC			: return &etc.stdZxType.basic.autorunLine;
			case TDirectoryEntry::NUMBER_ARRAY	: return &etc.stdZxType.numberArray.startInMemory;
			case TDirectoryEntry::CHAR_ARRAY	: return &etc.stdZxType.charArray.startInMemory;
			case TDirectoryEntry::CODE			:
			case TDirectoryEntry::SCREEN		: return &etc.stdZxType.code.startInMemory;
			default:
				return nullptr;
		}
	}

	void CGDOS::TDirectoryEntry::__setStdParameter1__(WORD param1){
		// sets StandardParameter1 (if possible)
		switch (fileType){
			case TDirectoryEntry::BASIC			: etc.stdZxType.basic.autorunLine=param1; break;
			case TDirectoryEntry::NUMBER_ARRAY	: etc.stdZxType.numberArray.startInMemory=param1; break;
			case TDirectoryEntry::CHAR_ARRAY	: etc.stdZxType.charArray.startInMemory=param1; break;
			case TDirectoryEntry::CODE			:
			case TDirectoryEntry::SCREEN		: etc.stdZxType.code.startInMemory=param1; break;
		}
	}

	PWORD CGDOS::TDirectoryEntry::__getStdParameter2__(){
		// returns StandardParameter2 (if any)
		switch (fileType){
			case TDirectoryEntry::BASIC			: return &etc.stdZxType.basic.lengthWithoutVariables;
			case TDirectoryEntry::NUMBER_ARRAY	: return &etc.stdZxType.numberArray.name;
			case TDirectoryEntry::CHAR_ARRAY	: return &etc.stdZxType.charArray.name;
			case TDirectoryEntry::CODE			:
			case TDirectoryEntry::SCREEN		: return &etc.stdZxType.code.autostartAddress;
			default:
				return nullptr;
		}
	}

	void CGDOS::TDirectoryEntry::__setStdParameter2__(WORD param2){
		// sets StandardParameter2 (if possible)
		switch (fileType){
			case TDirectoryEntry::BASIC			: etc.stdZxType.basic.lengthWithoutVariables=param2; break;
			case TDirectoryEntry::NUMBER_ARRAY	: etc.stdZxType.numberArray.name=param2; break;
			case TDirectoryEntry::CHAR_ARRAY	: etc.stdZxType.charArray.name=param2; break;
			case TDirectoryEntry::CODE			:
			case TDirectoryEntry::SCREEN		: etc.stdZxType.code.autostartAddress=param2; break;
		}
	}

	TStdWinError CGDOS::ChangeFileNameAndExt(PFile file,RCPathString newName,RCPathString newExt,PFile &rRenamedFile){
		// tries to change given File's name and extension; returns Windows standard i/o error
		// - can't change root Directory's name
		if (file==ZX_DIR_ROOT)
			return ERROR_ACCESS_DENIED;
		// - checking that the NewName+NewExt combination follows the "10.1" convention
		if (newExt.GetLengthW()<1)
			return ERROR_BAD_FILE_TYPE;
		if (newName.GetLengthW()>GDOS_FILE_NAME_LENGTH_MAX || newExt.GetLengthW()>1)
			return ERROR_FILENAME_EXCED_RANGE;
		// - making sure that a File with given NameAndExtension doesn't yet exist
		if ( rRenamedFile=FindFileInCurrentDir(newName,newExt,file) )
			return ERROR_FILE_EXISTS;
		// - extracting important information about the File before renaming it (e.g. standard parameters)
		const PDirectoryEntry de=(PDirectoryEntry)file;
		const DWORD dataSize=de->__getDataSize__(nullptr);
		TStdParameters stdParams=TStdParameters::Default;
			if (const PCWORD pw=de->__getStdParameter1__()) stdParams.param1=*pw;
			if (const PCWORD pw=de->__getStdParameter2__()) stdParams.param2=*pw;
		// - renaming
		de->SetNameAndExt(newName,newExt);
		// - modifying information of a standard ROM File (e.g. standard parameters)
		if (de->__isStandardRomFile__()){
			de->etc.stdZxType.romId=(TZxRom::TFileType)std::min<BYTE>(de->fileType-1,TZxRom::TFileType::CODE); // min = retyping Screen to Code
			de->__setStdParameter1__(stdParams.param1), de->__setStdParameter2__(stdParams.param2);
			de->__setDataSizeByFileType__(dataSize);
		}
		// - marking the Sector as dirty
		MarkDirectorySectorAsDirty( rRenamedFile=file );
		return ERROR_SUCCESS;
	}

	DWORD CGDOS::GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData,TGetFileSizeOptions option) const{
		// determines and returns the size of specified File
		if (pnBytesReservedAfterData) *pnBytesReservedAfterData=0;
		if (pnBytesReservedBeforeData) *pnBytesReservedBeforeData=0;
		const PCDirectoryEntry de=(PCDirectoryEntry)file;
		if (de==ZX_DIR_ROOT)
			return GDOS_DIR_FILES_COUNT_MAX*sizeof(TDirectoryEntry);
		else
			switch (option){
				case TGetFileSizeOptions::OfficialDataLength:
					return de->__getDataSize__(pnBytesReservedBeforeData);
				case TGetFileSizeOptions::SizeOnDisk:
					return de->nSectors*GDOS_SECTOR_LENGTH_STD;
				default:
					ASSERT(FALSE);
					return 0;
			}
	}

	TStdWinError CGDOS::DeleteFile(PFile file){
		// deletes specified File; returns Windows standard i/o error
		const PDirectoryEntry de=(PDirectoryEntry)file;
		if (de==ZX_DIR_ROOT)
			return ERROR_ACCESS_DENIED; // can't delete the root Directory
		if (de->fileType!=TDirectoryEntry::EMPTY_ENTRY) // File mustn't be already deleted (may happen during moving it in FileManager)
			if (const LPCTSTR errMsg=CFatPath(this,de).GetErrorDesc()){
				ShowFileProcessingError(de,errMsg);
				return ERROR_GEN_FAILURE;
			}else{
				// deleting from Directory
				de->fileType=TDirectoryEntry::EMPTY_ENTRY;
				MarkDirectorySectorAsDirty(de);
			}
		return ERROR_SUCCESS;
	}

	CDos::CPathString CGDOS::GetFileExportNameAndExt(PCFile file,bool shellCompliant) const{
		// returns File name concatenated with File extension for export of the File to another Windows application (e.g. Explorer)
		const PDirectoryEntry de=(PDirectoryEntry)file;
		CPathString result=__super::GetFileExportNameAndExt(de,shellCompliant);
		if (shellCompliant){
			// exporting to non-RIDE target (e.g. to the Explorer); excluding from the Buffer characters that are forbidden in FAT32 long file names
			TCHAR bufExt[16];
			if (de!=ZX_DIR_ROOT){
				result.DetachExtension();
				result.AppendDotExtensionIfAny( de->__getFileTypeDesc__(bufExt) );
			}
		}else{
			// exporting to another RIDE instance
			TUniFileType uts;
			switch (de->fileType){
				case TDirectoryEntry::BASIC			: uts=TUniFileType::PROGRAM; break;
				case TDirectoryEntry::CHAR_ARRAY	: uts=TUniFileType::CHAR_ARRAY; break;
				case TDirectoryEntry::NUMBER_ARRAY	: uts=TUniFileType::NUMBER_ARRAY; break;
				case TDirectoryEntry::CODE			: uts=TUniFileType::BLOCK; break;
				case TDirectoryEntry::SCREEN		: uts=TUniFileType::SCREEN; break;
				case TDirectoryEntry::SNAPSHOT_48K	: uts=TUniFileType::SNAPSHOT_48k; break;
				case TDirectoryEntry::SNAPSHOT_128K	: uts=TUniFileType::SNAPSHOT_128k; break;
				case TDirectoryEntry::OPENTYPE		: uts=TUniFileType::SEQUENTIAL; break;
				default								: uts=TUniFileType::UNKNOWN; break;
			}
			TStdParameters stdParams=TStdParameters::Default;
				if (const PCWORD pw=de->__getStdParameter1__()) stdParams.param1=*pw;
				if (const PCWORD pw=de->__getStdParameter2__()) stdParams.param2=*pw;
			TCHAR buf[80];
			__exportFileInformation__( buf, uts, stdParams, GetFileOfficialSize(de) );
			result.Append(buf);
		}
		return result;
	}

	DWORD CGDOS::ExportFile(PCFile file,CFile *fOut,DWORD nBytesToExportMax,LPCTSTR *pOutError) const{
		// exports data portion of specfied File (data portion size determined by GetFileSize); returns the export size of specified File
		const PCDirectoryEntry de=(PCDirectoryEntry)file;
		if (fOut){
			// . exporting File's data
			__super::ExportFile( file, fOut, nBytesToExportMax, pOutError );
			// . exporting Etc data stored in DirectoryEntry
			if (const DWORD nBytesReserved=nBytesToExportMax-fOut->GetLength())
				fOut->Write( &de->etc, std::min<UINT>(nBytesReserved,sizeof(de->etc)) );
		}
		return reinterpret_cast<PCDos>(this)->GetFileSize(de)+sizeof(de->etc);
	}

	TStdWinError CGDOS::ImportFile(CFile *f,DWORD fileSize,RCPathString nameAndExtension,DWORD winAttr,PFile &rFile){
		// imports specified File (physical or virtual) into the Image; returns Windows standard i/o error
		// - parsing the NameAndExtension into a usable "10.1" form
		CPathString zxName,zxExt;
		const CString zxInfo=ParseFat32LongName( nameAndExtension, zxName, zxExt );
		zxName.TrimToLengthW(GDOS_FILE_NAME_LENGTH_MAX);
		zxExt.TrimToLengthW(10); // 10 = Extension may be represented as text, e.g. "$-ARRAY", not just a single char, e.g. '0x03' (both representations are equal!)
		// - getting import information
		TStdParameters params; TUniFileType uts; DWORD dw;
		if (const int n=__importFileInformation__(zxInfo,uts,params,dw))
			if (dw) fileSize=dw;
		// - initializing the description of File to import
		TDirectoryEntry tmp;
			::ZeroMemory(&tmp,sizeof(tmp));
			// . name
			tmp.SetNameAndExt( zxName, zxExt );
			switch (zxExt.GetLengthW()){
				case 0:
					// no Extension provided - considering the File as Special
					tmp.fileType=TDirectoryEntry::SPECIAL;
					//fallthrough
				case 1:
					// a single-char Extension provided - doing nothing
					break;
				default:{
					// multi-char Extension provided - recognizing its text
					for( tmp.fileType=(TDirectoryEntry::TFileType)1; tmp.fileType; tmp.fileType=(TDirectoryEntry::TFileType)((BYTE)tmp.fileType+1) ){
						TCHAR bufExt[16];
						if (!::lstrcmpi(zxExt,tmp.__getFileTypeDesc__(bufExt)))
							break;
					}
					if (!tmp.fileType)
						tmp.fileType=TDirectoryEntry::SPECIAL;
					break;
				}
			}
			// . changing the Extension according to the "universal" type valid across ZX platforms (as TR-DOS File "SomeData.C" should be take on the name "SomeData.CDE" under GDOS)
			switch (uts){
				case TUniFileType::PROGRAM		: tmp.fileType=TDirectoryEntry::BASIC; break;
				case TUniFileType::CHAR_ARRAY	: tmp.fileType=TDirectoryEntry::CHAR_ARRAY; break;
				case TUniFileType::NUMBER_ARRAY	: tmp.fileType=TDirectoryEntry::NUMBER_ARRAY; break;
				case TUniFileType::BLOCK		: tmp.fileType=TDirectoryEntry::CODE; break;
				case TUniFileType::SNAPSHOT_48k	: tmp.fileType=TDirectoryEntry::SNAPSHOT_48K; break;
				case TUniFileType::SNAPSHOT_128k: tmp.fileType=TDirectoryEntry::SNAPSHOT_128K; break;
				case TUniFileType::SEQUENTIAL	: tmp.fileType=TDirectoryEntry::OPENTYPE; break;
			}
			// . size
			//nop (set below)
			// . additional information on File
			tmp.__setStdParameter1__(params.param1), tmp.__setStdParameter2__(params.param2);
		// - checking if there's enough empty space on disk
		BYTE offset;
		tmp.__getDataSize__(&offset); // only to receive the Offset
		TStdWinError err;
		if (offset+fileSize>GetFreeSpaceInBytes(err))
			return ERROR_DISK_FULL;
		// - checking that the File has "correct size", i.e. neither zero (unless allowed) nor too long
		if ( !fileSize&&!zeroLengthFilesEnabled || offset+fileSize>(WORD)-1*GDOS_SECTOR_LENGTH_STD)
			return ERROR_BAD_LENGTH;
		// - importing to Image
		CFatPath fatPath(this,offset+fileSize);
		if (err=__importFileData__( f, &tmp, zxName, tmp.fileType, fileSize, true, rFile, fatPath ))
			return err;
		// - finishing initialization of DirectoryEntry of successfully imported File
		const PDirectoryEntry de=(PDirectoryEntry)rFile;
			// . size
			CFatPath::PCItem item; DWORD n;
			fatPath.GetItems(item,n);
			de->nSectors = n = offset+fileSize ? n : 0; // 0 = zero-length File has no Sectors
			de->__setDataSizeByFileType__(fileSize);
			// . importing Etc data (if any)
			if (f->GetPosition()<f->GetLength())
				f->Read( &de->etc, sizeof(de->etc) );
			// . if standard FileType, creating standard data at the beginning of File
			if (offset){ // File must have standard data at the beginning ...
				ASSERT(offset==sizeof(TStdZxTypeData)); // ... and should thus be of standard FileType
				__shiftFileContent__(fatPath,offset); // shifting File's content to make space at the beginning
				( (PGdosSectorData)image->GetHealthySectorData(item->chs) )->stdZxType=de->etc.stdZxType; // creating the standard data
			}
			// . initializing File's SectorAllocationBitmap and interconnecting Sectors that contain File's data
			ModifyFileFatPath( de, fatPath );
		// - File successfully imported to Image
		return ERROR_SUCCESS;
	}











	TStdWinError CGDOS::CreateUserInterface(HWND hTdi){
		// creates DOS-specific Tabs in TDI; returns Windows standard i/o error
		if (const TStdWinError err=__super::CreateUserInterface(hTdi))
			return err;
		CTdiCtrl::AddTabLast( hTdi, FILE_MANAGER_TAB_LABEL, &fileManager.tab, true, TDI_TAB_CANCLOSE_NEVER, nullptr );
		return ERROR_SUCCESS;
	}

	CDos::TCmdResult CGDOS::ProcessCommand(WORD cmd){
		// returns the Result of processing a DOS-related command
		switch (cmd){
			case ID_DOS_FILE_ZERO_LENGTH:
				// enabling/disabling importing of zero-length Files
				__writeProfileBool__( INI_ALLOW_ZERO_LENGTH_FILES, zeroLengthFilesEnabled=!zeroLengthFilesEnabled );
				return TCmdResult::DONE;
			case ID_DOS_TAKEATOUR:
				// navigating to the online tour on this DOS
				app.GetMainWindow()->OpenApplicationPresentationWebPage(_T("Tour"),_T("GDOS/tour.html"));
				return TCmdResult::DONE;
		}
		return __super::ProcessCommand(cmd);
	}

	bool CGDOS::UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const{
		// True <=> given Command-specific user interface successfully updated, otherwise False
		switch (cmd){
			case ID_DOS_FILE_ZERO_LENGTH:
				pCmdUI->SetCheck(zeroLengthFilesEnabled);
				return true;
		}
		return __super::UpdateCommandUi(cmd,pCmdUI);
	}









	DWORD CGDOS::TDirectoryEntry::__getDataSize__(PBYTE pnBytesReservedBeforeData) const{
		// based on FileType, determines and returns the size in Bytes of File's data portion
		switch (fileType){
			case TDirectoryEntry::BASIC:
			case TDirectoryEntry::NUMBER_ARRAY:
			case TDirectoryEntry::CHAR_ARRAY:
			case TDirectoryEntry::CODE:
			case TDirectoryEntry::SCREEN:
				if (pnBytesReservedBeforeData) *pnBytesReservedBeforeData=sizeof(TStdZxTypeData);
				return etc.stdZxType.basic.length;
			case TDirectoryEntry::EXECUTE:
				if (pnBytesReservedBeforeData) *pnBytesReservedBeforeData=0;
				return GDOS_SECTOR_LENGTH_STD-sizeof(TSectorInfo);
			/*case TDirectoryEntry::SNAPSHOT_48K:
			case TDirectoryEntry::MICRODRIVE:
			case TDirectoryEntry::SPECIAL:
			case TDirectoryEntry::SNAPSHOT_128K:
			case TDirectoryEntry::OPENTYPE:*/
			default:
				if (pnBytesReservedBeforeData) *pnBytesReservedBeforeData=0;
				return nSectors*(GDOS_SECTOR_LENGTH_STD-sizeof(TSectorInfo));
		}
	}

	void CGDOS::TDirectoryEntry::__setDataSizeByFileType__(DWORD size){
		// based on FileType (set earlier by caller), sets the size in Bytes of File's data portion
		switch (fileType){
			case TDirectoryEntry::BASIC:
			case TDirectoryEntry::NUMBER_ARRAY:
			case TDirectoryEntry::CHAR_ARRAY:
			case TDirectoryEntry::CODE:
			case TDirectoryEntry::SCREEN:
				etc.stdZxType.basic.length=size;
				//fallthrough
			case TDirectoryEntry::EXECUTE:
				break;
			/*case TDirectoryEntry::SNAPSHOT_48K:
			case TDirectoryEntry::MICRODRIVE:
			case TDirectoryEntry::SPECIAL:
			case TDirectoryEntry::SNAPSHOT_128K:
			case TDirectoryEntry::OPENTYPE:*/
			default:
				nSectors=Utils::RoundDivUp<DWORD>( size, GDOS_SECTOR_LENGTH_STD-sizeof(TSectorInfo) );
				break;
		}
	}










	std::unique_ptr<CDos::TDirectoryTraversal> CGDOS::BeginDirectoryTraversal(PCFile directory) const{
		// initiates exploration of specified Directory through a DOS-specific DirectoryTraversal
		ASSERT(directory==ZX_DIR_ROOT);
		return std::unique_ptr<TDirectoryTraversal>( new TGdosDirectoryTraversal(this) );
	}

	CGDOS::TGdosDirectoryTraversal::TGdosDirectoryTraversal(const CGDOS *gdos)
		// ctor
		// - base
		: TDirectoryTraversal( ZX_DIR_ROOT, sizeof(TDirectoryEntry) )
		// - initialization
		, gdos(gdos)
		// - getting ready to read the first Directory Sector
		, nRemainingEntriesInSector(0) {
		chs.cylinder = chs.sectorId.cylinder = 0;
		chs.sectorId.side=gdos->sideMap[ chs.head=0 ];
		chs.sectorId.sector=Properties.firstSectorNumber-1;
		chs.sectorId.lengthCode=GDOS_SECTOR_LENGTH_STD_CODE;
	}

	bool CGDOS::TGdosDirectoryTraversal::AdvanceToNextEntry(){
		// True <=> another Entry in current Directory exists (Empty or not), otherwise False
		// - getting the next Sector with Directory
		if (!nRemainingEntriesInSector){
			if (++chs.sectorId.sector>GDOS_TRACK_SECTORS_COUNT){
				chs.sectorId.sector=Properties.firstSectorNumber;
				if (( chs.sectorId.cylinder=++chs.cylinder )==GDOS_DIR_FILES_COUNT_MAX*sizeof(TDirectoryEntry)/GDOS_SECTOR_LENGTH_STD/GDOS_TRACK_SECTORS_COUNT){ // end of Directory
					entryType=TDirectoryTraversal::END;
					return false; // end of Directory
				}
			}
			entry=gdos->image->GetHealthySectorData(chs);
			if (!entry) // Directory Sector not found
				entryType=TDirectoryTraversal::WARNING, warning=ERROR_SECTOR_NOT_FOUND;
			else
				entryType=TDirectoryTraversal::UNKNOWN, entry=(PDirectoryEntry)entry-1; // pointer set "before" the first DirectoryEntry
			nRemainingEntriesInSector=GDOS_SECTOR_LENGTH_STD/sizeof(TDirectoryEntry);
		}
		// - getting the next DirectoryEntry
		if (entryType!=TDirectoryTraversal::WARNING)
			entryType=	((PDirectoryEntry)( entry=(PDirectoryEntry)entry+1 ))->fileType!=TDirectoryEntry::EMPTY_ENTRY
						? TDirectoryTraversal::FILE
						: TDirectoryTraversal::EMPTY;
		nRemainingEntriesInSector--;
		return true;
	}

	void CGDOS::TGdosDirectoryTraversal::ResetCurrentEntry(BYTE directoryFillerByte){
		// gets current entry to the state in which it would be just after formatting
		if (entryType==TDirectoryTraversal::FILE || entryType==TDirectoryTraversal::EMPTY){
			*(PBYTE)::memset( entry, directoryFillerByte, entrySize )=TDirectoryEntry::EMPTY_ENTRY;
			entryType=TDirectoryTraversal::EMPTY;
		}
	}

