#include "stdafx.h"
#include "TRDOS.h"

	CTRDOS503::TSectorTrackPair CTRDOS503::TSectorTrackPair::operator+(BYTE nSectors) const{
		// computes and returns the [Sector,Track] identifier that follows N Sectors after the current identifier
		const div_t d=div( track*TRDOS503_TRACK_SECTORS_COUNT+sector+nSectors, TRDOS503_TRACK_SECTORS_COUNT );
		const TSectorTrackPair result={ d.rem, d.quot };
		return result;
	}





	WORD CTRDOS503::TDirectoryEntry::__getOfficialFileSize__(PBYTE pnBytesReservedAfterData) const{
		// determines and returns the official File size based on the Extension
		switch (extension){
			case TDirectoryEntry::BASIC_PRG:
				if (pnBytesReservedAfterData) *pnBytesReservedAfterData=4; // 4 = (WORD)0xAA80 (the mark that introduces a parameter "after" official data) and a WORD parameter
				return parameterA;
			case TDirectoryEntry::DATA_FIELD:
				if (pnBytesReservedAfterData) *pnBytesReservedAfterData=4; // 4 = (WORD)0xAA80 (the mark that introduces a parameter "after" official data) and a WORD parameter
				//fallthrough
			case TDirectoryEntry::BLOCK:
			case TDirectoryEntry::PRINT:
				return parameterB;
			default:
				if (pnBytesReservedAfterData) *pnBytesReservedAfterData=0;
				return __getFileSizeOnDisk__();
		}
	}

	WORD CTRDOS503::TDirectoryEntry::__getFileSizeOnDisk__() const{
		// determines and returns the number of Bytes this File occupies on the disk (i.e. including eventual parameter "after" official data)
		return nSectors*TRDOS503_SECTOR_LENGTH_STD;
	}

	void CTRDOS503::TDirectoryEntry::__markTemporary__(){
		// marks this DirectoryEntry as "temporary" (see usage in ImportFile and DeleteFile)
		first.track=0, first.sector=TRDOS503_BOOT_SECTOR_NUMBER-TRDOS503_SECTOR_FIRST_NUMBER;
	}

	bool CTRDOS503::TDirectoryEntry::__isTemporary__() const{
		// True <=> this DirectoryEntry has previously been marked as "temporary" (see usage in ImportFile and DeleteFile)
		return !first.track && first.sector==TRDOS503_BOOT_SECTOR_NUMBER-TRDOS503_SECTOR_FIRST_NUMBER;
	}



	#define INI_TRDOS	_T("TRDOS")
	#define INI_EXPORT_WHOLE_SECTORS	_T("fmxsects")
	#define INI_ALLOW_ZERO_LENGTH_FILES	_T("fm0files")

	CTRDOS503::CTRDOS503(PImage image) // called exclusively by SCL Image!
		// ctor (called exclusively by SCL Image!)
		// - base
		: CSpectrumDos( image, &CTRDOS503::Properties.stdFormats[0].params.format, TTrackScheme::BY_CYLINDERS, &Properties, IDR_TRDOS, &fileManager )
		// - initialization
		, boot(this,TRDOS503_BOOT_LABEL_LENGTH_MAX) , fileManager(this)
		, zeroLengthFilesEnabled(true) // just to be sure (SCL Image expects this setting when loading the content)
		, exportWholeSectors(true)
		, importToSysTrack(false) {
		::ZeroMemory( sideMap, sizeof(sideMap) ); // both Sides of floppy are numbered as zero
		formatBoot.nCylinders++;
	}

	CTRDOS503::CTRDOS503(PImage image,PCFormat pFormatBoot,PCProperties pTrdosProps)
		// ctor
		// - base
		: CSpectrumDos( image, pFormatBoot, TTrackScheme::BY_CYLINDERS,pTrdosProps, IDR_TRDOS, &fileManager )
		// - initialization
		, boot(	this,
				(pTrdosProps==&Properties)*TRDOS503_BOOT_LABEL_LENGTH_MAX
				|
				(pTrdosProps==&CTRDOS504::Properties)*TRDOS504_BOOT_LABEL_LENGTH_MAX
				|
				(pTrdosProps==&CTRDOS505::Properties)*TRDOS505_BOOT_LABEL_LENGTH_MAX
			)
		, fileManager(this)
		, zeroLengthFilesEnabled( __getProfileBool__(INI_ALLOW_ZERO_LENGTH_FILES,false) )
		, exportWholeSectors( __getProfileBool__(INI_EXPORT_WHOLE_SECTORS,true) )
		, importToSysTrack(false) {
		if (formatBoot.mediumType!=TMedium::UNKNOWN){ // may be unknown if creating a new Image
			PDirectoryEntry directory[TRDOS503_FILE_COUNT_MAX];
			if (__getDirectory__(directory))
				importToSysTrack=!directory[0]->first.track; // turned on if the first File starts in system Track
		}
		::ZeroMemory( sideMap, sizeof(sideMap) ); // both Sides of floppy are numbered as zero
	}










	void CTRDOS503::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_TRDOS, messageId );
	}









	bool CTRDOS503::GetSectorStatuses(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PSectorStatus buffer) const{
		// True <=> Statuses of all Sectors in the Track successfully retrieved and populated the Buffer, otherwise False
		bool result=true; // assumption (Statuses of all Sectors retrieved)
		// - composing the List of "regions" on disk
		struct{
			TSectorTrackPair start; // length determined as the difference between this and previous "region"
			TSectorStatus status;
		} regions[TRDOS503_FILE_COUNT_MAX+2],*pRgn=regions; // "+2" = empty space and terminator (see below)
		pRgn->start.sector=TRDOS503_BOOT_SECTOR_NUMBER-TRDOS503_SECTOR_FIRST_NUMBER, pRgn->start.track=0; // just in case the first DirectoryEntry isn't found (as Sector not found)
		// - 
		for( TTrdosDirectoryTraversal dt(this); dt.__existsNextEntry__(); )
			if (dt.entryType!=TDirectoryTraversal::WARNING){
				const PCDirectoryEntry de=(PCDirectoryEntry)dt.entry;
				if (dt.entryType==TDirectoryTraversal::FILE && !de->__isTemporary__()){
					// A&B: A = File (existing or Deleted), B = not a temporary Entry in Directory (see also ImportFile)
					const TSectorTrackPair tmp = pRgn->start = de->first;
					pRgn++->status=	*(PCBYTE)de!=TDirectoryEntry::DELETED
									? TSectorStatus::OCCUPIED
									: TSectorStatus::SKIPPED ;
					pRgn->start = tmp+de->nSectors; // for the case that next DirectoryEntry not found (as Sector not found)
				}else{
					// end of Directory, or Directory Sector not found
					if (dt.entryType==TDirectoryTraversal::EMPTY || de->__isTemporary__()) // A|B; A = natural end of Directory, B = a temporary Entry in Directory (see also ImportFile)
						break; // the end of Directory is followed by empty space (see after cycle)
					pRgn++->status=TSectorStatus::UNKNOWN; // Sector and Track set by previous known File, see above
					*pRgn=*(pRgn-1); // for the case that next DirectoryEntry not found (as Sector not found)
					result=false;
				}
			}
		pRgn++->status=TSectorStatus::EMPTY, pRgn->start.sector=0, pRgn->start.track=formatBoot.nCylinders*formatBoot.nHeads; // terminator
		// - determining the Statuses of Sectors
		const PCBootSector bootSector=__getBootSector__();
		for( const BYTE track=cyl*formatBoot.nHeads+head; nSectors--; bufferId++ ){
			const TSector sector=bufferId->sector;
			if (cyl>=formatBoot.nCylinders || head>=formatBoot.nHeads || bufferId->cylinder!=cyl || sector<TRDOS503_SECTOR_FIRST_NUMBER || formatBoot.nSectors<sector || bufferId->lengthCode!=TRDOS503_SECTOR_LENGTH_STD_CODE)
				*buffer++=TSectorStatus::UNKNOWN; // Sector ID out of official Format - Sector thus Unknown
			else if (!track && (sector<=TRDOS503_BOOT_SECTOR_NUMBER || !importToSysTrack))
				*buffer++=TSectorStatus::SYSTEM; // zeroth Track always contains System Sectors (Directory, Boot, etc.)
			else if (track>bootSector->firstFree.track || track==bootSector->firstFree.track&&sector>bootSector->firstFree.sector)
				*buffer++=TSectorStatus::EMPTY; // Sectors beyond the FirstFree Sector are always reported as Empty, no matter if they are referred from the Directory!
			else{
				for( pRgn=1+regions; track>pRgn->start.track || (track>=pRgn->start.track&&sector>=TRDOS503_SECTOR_FIRST_NUMBER+pRgn->start.sector); pRgn++ );
				*buffer++=(--pRgn)->status;
			}
		}
		return result;
	}
	bool CTRDOS503::ModifyTrackInFat(TCylinder cyl,THead head,PSectorStatus statuses){
		// True <=> Statuses of all Sectors in Track successfully changed, otherwise False; caller guarantees that the number of Statuses corresponds with the number of standard "official" Sectors in the Boot
		return true; // True = silently ignoring this request (as there's not FAT) - otherwise the caller might show an error message
	}
	DWORD CTRDOS503::GetFreeSpaceInBytes(TStdWinError &rError) const{
		// computes and returns the empty space on disk
		if (const PCBootSector boot=__getBootSector__()){
			rError=ERROR_SUCCESS;
			return boot->nFreeSectors*TRDOS503_SECTOR_LENGTH_STD;
		}else{
			rError=ERROR_SECTOR_NOT_FOUND;
			return 0;
		}
	}








	bool CTRDOS503::GetFileFatPath(PCFile file,CFatPath &rFatPath) const{
		// True <=> FatPath of given File (even an erroneous FatPath) successfully retrieved, otherwise False
		// - no FatPath can be retrieved if File is Deleted
		if (*(PCBYTE)file==TDirectoryEntry::DELETED)
			return false;
		// - composing the FatPath
		const PCDirectoryEntry de=(PCDirectoryEntry)file;
		const div_t B=div( de->first.track, formatBoot.nHeads );
		CFatPath::TItem item;
			item.chs.cylinder = item.chs.sectorId.cylinder = B.quot,
			item.chs.sectorId.side=sideMap[ item.chs.head=B.rem ],
			item.chs.sectorId.sector=TRDOS503_SECTOR_FIRST_NUMBER+de->first.sector;
			item.chs.sectorId.lengthCode=TRDOS503_SECTOR_LENGTH_STD_CODE;
			BYTE nBytesAfterData;
			item.value=GetFileDataSize(de,NULL,&nBytesAfterData) + TRDOS503_SECTOR_LENGTH_STD-1; // "+N" = rounding up
			item.value+=nBytesAfterData;
		for( item.value/=formatBoot.sectorLength; item.value--; ){ // each Item gets a unique Value
			// . adding the Item to the FatPath
			if (!rFatPath.AddItem(&item)) break; // also sets an error in FatPath
			// . determining the PhysicalAddress of the next Sector
			if (++item.chs.sectorId.sector>formatBoot.nSectors){
				item.chs.sectorId.sector=TRDOS503_SECTOR_FIRST_NUMBER;
				if (++item.chs.head==formatBoot.nHeads){
					item.chs.sectorId.side=sideMap[ item.chs.head=0 ];
					item.chs.sectorId.cylinder=++item.chs.cylinder;
				}else
					item.chs.sectorId.side=sideMap[item.chs.head];
			}
		}
		return true;
	}

	void CTRDOS503::GetFileNameAndExt(PCFile file,PTCHAR bufName,PTCHAR bufExt) const{
		// populates the Buffers with File's name and extension; caller guarantees that the Buffer sizes are at least MAX_PATH characters each
		const PCDirectoryEntry de=(PCDirectoryEntry)file;
		if (bufName){
			#ifdef UNICODE
				::MultiByteToWideChar( CP_ACP, 0, de->name,TRDOS503_FILE_NAME_LENGTH_MAX+1, tmp,TRDOS503_FILE_NAME_LENGTH_MAX+1 );
				ASSERT(FALSE);
			#else
				::lstrcpyn( bufName, de->name, TRDOS503_FILE_NAME_LENGTH_MAX+1 );
			#endif
			for( PTCHAR p=bufName+TRDOS503_FILE_NAME_LENGTH_MAX; p--!=bufName; ) // trimming trailing spaces
				if (*p==' ') *p='\0'; else break;
		}
		if (bufExt)
			*bufExt++=de->extension, *bufExt='\0';
	}
	TStdWinError CTRDOS503::ChangeFileNameAndExt(PFile file,LPCTSTR newName,LPCTSTR newExt,PFile &rRenamedFile){
		// tries to change given File's name and extension; returns Windows standard i/o error
		ASSERT(newName!=NULL && newExt!=NULL);
		// - checking that the NewName+NewExt combination follows the "8.1" convention
		if (::lstrlen(newName)>TRDOS503_FILE_NAME_LENGTH_MAX || ::lstrlen(newExt)>1)
			return ERROR_FILENAME_EXCED_RANGE;
		// - making sure that a File with given NameAndExtension doesn't yet exist
		if ( rRenamedFile=__findFile__(newName,newExt,file) )
			return ERROR_FILE_EXISTS;
		// - getting important information about the File
		const PDirectoryEntry de=(PDirectoryEntry)file;
		const WORD officialFileSize=de->__getOfficialFileSize__(NULL);
		UStdParameters stdParams;
			__getStdParameter1__(de,stdParams.param1), __getStdParameter2__(de,stdParams.param2);
		// - renaming
		TDirectoryEntry tmp=*de; // all changes are made to a temporary Entry before they are copied to disk
		tmp.extension=*newExt;
		#ifdef UNICODE
			ASSERT(FALSE)
		#else
			::memcpy(	::memset(tmp.name,' ',TRDOS503_FILE_NAME_LENGTH_MAX),
						newName, ::lstrlen(newName)
					);
		#endif
		// - setting important information about the File
		tmp.parameterA = tmp.parameterB = officialFileSize;
		__setStdParameter1__(&tmp,stdParams.param1), __setStdParameter2__(&tmp,stdParams.param2);
		//if (!__setStdParameter1__(&tmp,stdParams.param1) || !__setStdParameter2__(&tmp,stdParams.param2))
			//return ERROR_DS_SIZELIMIT_EXCEEDED; //ERROR_INCORRECT_SIZE;
		// - marking the corresponding Directory Sector as dirty
		*de=tmp;
		__markDirectorySectorAsDirty__( rRenamedFile=file );
		return ERROR_SUCCESS;
	}
	DWORD CTRDOS503::GetFileDataSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData) const{
		// determines and returns the size of specified File's data portion
		if (pnBytesReservedBeforeData) *pnBytesReservedBeforeData=0;
		if (exportWholeSectors){
			if (pnBytesReservedAfterData) *pnBytesReservedAfterData=0;
			return ((PCDirectoryEntry)file)->__getFileSizeOnDisk__();
		}else
			return ((PCDirectoryEntry)file)->__getOfficialFileSize__(pnBytesReservedAfterData);
	}

	TStdWinError CTRDOS503::DeleteFile(PFile file){
		// deletes specified File; returns Windows standard i/o error
		if (*(PCBYTE)file!=TDirectoryEntry::DELETED && *(PCBYTE)file!=TDirectoryEntry::END_OF_DIR) // File mustn't be already Deleted (may happen during moving it in FileManager)
			if (const PBootSector boot=__getBootSector__()){
				PDirectoryEntry directory[TRDOS503_FILE_COUNT_MAX];
				__markDirectorySectorAsDirty__(file);
				image->MarkSectorAsDirty(TBootSector::CHS);
				if (file!=directory[__getDirectory__(directory)-1]){
					// File is not at the end of Directory
					*(PBYTE)file=TDirectoryEntry::DELETED;
					boot->nFilesDeleted++;
				}else{
					// File is at the end of Directory
					const PDirectoryEntry de=(PDirectoryEntry)file;
					*(PBYTE)de=TDirectoryEntry::END_OF_DIR;
					if (!de->__isTemporary__()){ // not a temporary Entry in Directory (see also ImportFile)
						boot->firstFree=de->first, boot->nFreeSectors+=de->nSectors;
						if (!importToSysTrack) // saving data in system Track 0 forbidden ...
							if (!de->first.track){ // ... but the File started in Track 0 ...
								// ... so correcting the information in the Boot Sector
								boot->nFreeSectors-=TRDOS503_TRACK_SECTORS_COUNT-de->first.sector;
								if (de->first.sector+de->nSectors<TRDOS503_TRACK_SECTORS_COUNT)
									boot->nFreeSectors+=TRDOS503_TRACK_SECTORS_COUNT-de->first.sector-de->nSectors;
								boot->firstFree.track=1, boot->firstFree.sector=0;
							}
						boot->nFiles--;
					}
				}
			}else
				return ERROR_DEVICE_NOT_AVAILABLE;
		return ERROR_SUCCESS;
	}

	#define INFO_FILE_EX		_T("S%x")
	
	PTCHAR CTRDOS503::GetFileExportNameAndExt(PCFile file,bool shellCompliant,PTCHAR buf) const{
		// populates Buffer with specified File's export name and extension and returns the Buffer; returns Null if File cannot be exported (e.g. a "dotdot" entry in MS-DOS); caller guarantees that the Buffer is at least MAX_PATH characters big
		const PCDirectoryEntry de=(PCDirectoryEntry)file;
		__super::GetFileExportNameAndExt(de,shellCompliant,buf);
		if (!shellCompliant){
			// exporting to another RIDE instance
			UStdParameters params;
				__getStdParameter1__(de,params.param1), __getStdParameter2__(de,params.param2);
			TUniFileType uts;
				switch (de->extension){
					case TDirectoryEntry::BASIC_PRG	: uts=TUniFileType::PROGRAM; break;
					case TDirectoryEntry::DATA_FIELD: uts=TUniFileType::CHAR_ARRAY; break;
					case TDirectoryEntry::BLOCK		: uts=TUniFileType::BLOCK; break;
					case TDirectoryEntry::PRINT		: uts=TUniFileType::PRINT; break;
					default							: uts=TUniFileType::UNKNOWN; break;
				}
			const PTCHAR p=buf+::lstrlen(buf);
			::wsprintf(	p+__exportFileInformation__( p, uts, params, de->__getOfficialFileSize__(NULL) ),
						INFO_FILE_EX, de->nSectors
					);
		}
		return buf;
	}

	TStdWinError CTRDOS503::ImportFile(CFile *f,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile){
		// imports specified File (physical or virtual) into the Image; returns Windows standard i/o error
		// - parsing the NameAndExtension into a usable "8.1" form
		LPCTSTR zxName,zxExt,zxInfo;
		TCHAR buf[MAX_PATH];
		__parseFat32LongName__(	::lstrcpy(buf,nameAndExtension),
								zxName, TRDOS503_FILE_NAME_LENGTH_MAX,
								zxExt, 1,
								zxInfo
							);
		// - getting import information
		UStdParameters params;	TUniFileType uts;	DWORD dw;
		const LPCTSTR pTrdosSpecificInfo=zxInfo+__importFileInformation__(zxInfo,uts,params,dw);
		const DWORD fileSizeFormal=	pTrdosSpecificInfo>zxInfo // if record on official File size exists in ZxInformation ...
									? dw // ... use that record
									: fileSize; // ... otherwise take as the official File size the actual size of imported file
		// - determining how much space the File will take on disk
		const DWORD fileSizeOnDisk=	uts==TUniFileType::PROGRAM || uts==TUniFileType::NUMBER_ARRAY || uts==TUniFileType::CHAR_ARRAY
									? max(fileSizeFormal,fileSize)+4 // "+4" = (WORD)0xAA80 (the mark that introduces a parameter "after" official data) and a WORD parameter
									: fileSize;
		// - checking if there's enough empty space on disk
		TStdWinError err;
		if (fileSizeOnDisk>GetFreeSpaceInBytes(err))
			return ERROR_DISK_FULL;
		// - checking that the File has "correct size", i.e. neither zero (unless allowed) nor too long
		if ( !fileSize&&!zeroLengthFilesEnabled || fileSizeOnDisk>0xff00)
			return ERROR_BAD_LENGTH;
		// - changing the Extension according to the "universal" type valid across ZX platforms (as MDOS File "Picture.B" should be take on the name "Picture.C" under TR-DOS)
		TCHAR uftExt[]={ *zxExt, '\0' };
		switch (uts){
			case TUniFileType::PROGRAM	: *uftExt=TDirectoryEntry::BASIC_PRG; break;
			case TUniFileType::CHAR_ARRAY:*uftExt=TDirectoryEntry::DATA_FIELD; break;
			case TUniFileType::BLOCK	:
			case TUniFileType::SCREEN	: *uftExt=TDirectoryEntry::BLOCK; break;
			case TUniFileType::PRINT	: *uftExt=TDirectoryEntry::PRINT; break;
		}
		// - initializing the description of File to import
		const PBootSector boot=__getBootSector__();
		if (!boot)
			return ERROR_DEVICE_NOT_AVAILABLE;
		TDirectoryEntry tmp;
			::ZeroMemory(&tmp,sizeof(tmp));
			// . name
			//nop (ChangeFileNameAndExt called below by ImportFile)
			*tmp.name=TDirectoryEntry::END_OF_DIR;
			// . Size
			//nop (set below)
			// . FirstTrack and -Sector
			//nop
			tmp.__markTemporary__(); // identification of a temporary Entry in Directory (see also DeleteFile)
		// - importing to Image
		CFatPath fatPath(this,fileSizeOnDisk);
		err=__importFileData__( f, &tmp, zxName, uftExt, fileSize, rFile, fatPath );
		if (err!=ERROR_SUCCESS)
			return err;
		// - finishing initialization of DirectoryEntry of successfully imported File
		const PDirectoryEntry de=(PDirectoryEntry)rFile;
			// . Size
			CFatPath::PCItem item; DWORD n;
			fatPath.GetItems(item,n);
			de->nSectors= fileSizeOnDisk ? n : 0; // 0 = zero-length File has no Sectors
			de->parameterA = de->parameterB = fileSizeFormal;
			// . FirstTrack and -Sector
			de->first.track=item->chs.GetTrackNumber(formatBoot.nHeads); // parametrized version to avoid auto-determination of active DOS (no active DOS may exist if called from within SCL container)
			de->first.sector=item->chs.sectorId.sector-TRDOS503_SECTOR_FIRST_NUMBER;
			// . additional File information
			__setStdParameter1__(de,params.param1);
			__setStdParameter2__(de,params.param2);
			// . processing SpecificInfo (if any)
			if (pTrdosSpecificInfo){
				int i=de->nSectors;
				_stscanf(pTrdosSpecificInfo,INFO_FILE_EX,&i);
				de->nSectors=i;
			}
		// - modifying the Boot Sector
		boot->firstFree = de->first+de->nSectors;
		boot->nFreeSectors-=de->nSectors, boot->nFiles++;
		image->MarkSectorAsDirty(TBootSector::CHS);
		// - terminating the Directory
		PDirectoryEntry directory[TRDOS503_FILE_COUNT_MAX+1],*p=directory; // "+1" = terminator
		for( directory[__getDirectory__(directory)]=(PDirectoryEntry)boot; *p!=de; p++ ); // Boot Sector as terminator
		*(PBYTE)*++p=TDirectoryEntry::END_OF_DIR;
		__markDirectorySectorAsDirty__(*p);
		// - File successfully imported to Image
		return ERROR_SUCCESS;
	}

	bool CTRDOS503::__parameterAfterData__(PCDirectoryEntry de,bool modify,PWORD pw) const{
		// True <=> parameter after given File's data successfully get/set, otherwise False
		WORD officialFileSize=de->__getOfficialFileSize__(NULL);
		WORD buf[2]={ 0xAA80, *pw }; // 0xAA80 = the mark that introduces a parameter "after" official data
		for( BYTE n=4,*p=(PBYTE)buf; n--; officialFileSize++ ){
			const TSector sector=officialFileSize/TRDOS503_SECTOR_LENGTH_STD;
			if (sector>=de->nSectors) return false;
			const TSectorTrackPair A = de->first+sector;
			const div_t B=div(A.track,formatBoot.nHeads);
			const TPhysicalAddress chs={ B.quot, B.rem, { B.quot, sideMap[B.rem], A.sector+TRDOS503_SECTOR_FIRST_NUMBER, TRDOS503_SECTOR_LENGTH_STD_CODE } };
			if (const PSectorData data=image->GetSectorData(chs)){
				if (modify){
					((PBYTE)data)[officialFileSize&(TRDOS503_SECTOR_LENGTH_STD-1)]=*p++;
					image->MarkSectorAsDirty(chs);
				}else
					*p++=((PBYTE)data)[officialFileSize&(TRDOS503_SECTOR_LENGTH_STD-1)];
			}else
				return false;
		}
		*pw=buf[1];
		return true;
	}
	bool CTRDOS503::__getStdParameter1__(PCDirectoryEntry de,WORD &rParam1) const{
		// True <=> Spectrum's StandardParameter 1 successfully retrieved, otherwise False
		switch (de->extension){
			case TDirectoryEntry::BASIC_PRG:
				// auto-start line stored "after" Basic
				//fallthrough
			case TDirectoryEntry::DATA_FIELD:
				// stream name
				return __parameterAfterData__(de,false,&rParam1);
			case TDirectoryEntry::BLOCK:
				// start address
				//fallthrough
			case TDirectoryEntry::PRINT:
				// sequence number
				//fallthrough
			default:
				rParam1=de->parameterA;
				return true;
		}
	}
	bool CTRDOS503::__setStdParameter1__(PDirectoryEntry de,WORD newParam1){
		// True <=> Spectrum's StandardParameter 1 successfully changed, otherwise False
		switch (de->extension){
			case TDirectoryEntry::BASIC_PRG:
				// auto-start line stored "after" Basic
				//fallthrough
			case TDirectoryEntry::DATA_FIELD:
				// stream name
				return __parameterAfterData__(de,true,&newParam1);
			case TDirectoryEntry::BLOCK:
				// start address
				//fallthrough
			case TDirectoryEntry::PRINT:
				// sequence number
				//fallthrough
			default:
				de->parameterA=newParam1;
				__markDirectorySectorAsDirty__(de);
				return true;
		}
	}
	bool CTRDOS503::__getStdParameter2__(PCDirectoryEntry de,WORD &rParam2) const{
		// True <=> Spectrum's StandardParameter 1 successfully retrieved, otherwise False
		switch (de->extension){
			case TDirectoryEntry::DATA_FIELD:
				// File length - cannot be retrieved
				//fallthrough
			case TDirectoryEntry::BLOCK:
				// File length - cannot be retrieved
				return false;
			case TDirectoryEntry::BASIC_PRG:
				// length of Basic Program without variables
				//fallthrough
			case TDirectoryEntry::PRINT:
				// print length (0..4096)
				//fallthrough
			default:
				rParam2=de->parameterB;
				return true;
		}
	}
	bool CTRDOS503::__setStdParameter2__(PDirectoryEntry de,WORD newParam2){
		// True <=> Spectrum's StandardParameter 2 successfully changed, otherwise False
		switch (de->extension){
			case TDirectoryEntry::DATA_FIELD:
				// File length - cannot be changed
				//fallthrough
			case TDirectoryEntry::BLOCK:
				// File length - cannot be changed
				return false;
			case TDirectoryEntry::BASIC_PRG:
				// length of Basic Program without variables
				//fallthrough
			case TDirectoryEntry::PRINT:
				// print length (0..4096)
				//fallthrough
			default:
				de->parameterB=newParam2;
				__markDirectorySectorAsDirty__(de);
				return true;
		}
	}









	TStdWinError CTRDOS503::CreateUserInterface(HWND hTdi){
		// creates DOS-specific Tabs in TDI; returns Windows standard i/o error
		// - base
		CSpectrumDos::CreateUserInterface(hTdi); // guaranteed to always return ERROR_SUCCESS
		// - creating the user interface
		CTdiCtrl::AddTabLast( hTdi, TRACK_MAP_TAB_LABEL, &trackMap.tab, false, NULL, NULL );
		CTdiCtrl::AddTabLast( hTdi, BOOT_SECTOR_TAB_LABEL, &boot.tab, false, NULL, NULL );
		CTdiCtrl::AddTabLast( hTdi, FILE_MANAGER_TAB_LABEL, &fileManager.tab, true, NULL, NULL );
		// - informing on how the SCL Images are opened
		if (dynamic_cast<CSCL *>(image)!=NULL)
			if (!image->GetPathName().IsEmpty()){
				TCHAR buf[200];
				::wsprintf( buf, _T("SCL images are always opened by the top-positioned TR-DOS in the recognition sequence (currently \"%s\")."), properties->name );
				__informationWithCheckableShowNoMore__( buf, _T("sclnfo") );
			}
		// - user interface created successfully
		return ERROR_SUCCESS;
	}








	CTRDOS503::TTrdosDirectoryTraversal::TTrdosDirectoryTraversal(const CTRDOS503 *_trdos)
		// ctor
		// - base
		: TDirectoryTraversal(sizeof(TDirectoryEntry),TRDOS503_FILE_NAME_LENGTH_MAX)
		// - initialization
		, trdos(_trdos) , foundEndOfDirectory(false)
		// - getting ready to read the first Directory Sector
		, nRemainingEntriesInSector(0) {
		chs.cylinder = chs.sectorId.cylinder = 0,
		chs.sectorId.side=_trdos->sideMap[ chs.head=0 ],
		chs.sectorId.sector=TRDOS503_SECTOR_FIRST_NUMBER-1;
		chs.sectorId.lengthCode=TRDOS503_SECTOR_LENGTH_STD_CODE;
		// - buffering the whole Directory (to eventually speed-up reading from a real floppy)
		TSectorId buffer[TRDOS503_TRACK_SECTORS_COUNT];
		trdos->__getListOfStdSectors__(0,0,buffer);
		trdos->image->BufferTrackData( 0, 0, buffer, Utils::CByteIdentity(), TRDOS503_BOOT_SECTOR_NUMBER, true ); // including the Boot Sector (to not have to include another named constant)		
	}
	bool CTRDOS503::TTrdosDirectoryTraversal::__existsNextEntry__(){
		// True <=> another Entry in current Directory exists (Empty or not), otherwise False
		// - getting the next Sector with Directory
		if (!nRemainingEntriesInSector){
			if (++chs.sectorId.sector>=TRDOS503_BOOT_SECTOR_NUMBER){ // end of Directory
				foundEndOfDirectory=true;
				return false;
			}
			entry=trdos->image->GetSectorData(chs);
			if (!entry){ // Directory Sector not found
				entryType=TDirectoryTraversal::WARNING, warning=ERROR_SECTOR_NOT_FOUND;
				return true;
			}else
				entry=(PDirectoryEntry)entry-1; // pointer set "before" the first DirectoryEntry
			nRemainingEntriesInSector=TRDOS503_DIR_SECTOR_ENTRIES_COUNT;
		}
		// - getting the next DirectoryEntry
		entry=(PDirectoryEntry)entry+1, nRemainingEntriesInSector--;
		if (!foundEndOfDirectory)
			if (*(PCBYTE)entry!=TDirectoryEntry::END_OF_DIR)
				entryType=TDirectoryTraversal::FILE;
			else{
				entryType=TDirectoryTraversal::EMPTY;
				foundEndOfDirectory=true;
			}
		return true;
	}

	BYTE CTRDOS503::__getDirectory__(PDirectoryEntry *directory) const{
		// returns the length of available Directory including Deleted Files; assumed that the buffer is big enough to contain maximally occupied Directory
		BYTE nFilesFound=0;
		for( TTrdosDirectoryTraversal dt(this); dt.__existsNextEntry__(); )
			if (dt.entryType==TDirectoryTraversal::FILE)
				*directory++=(PDirectoryEntry)dt.entry, nFilesFound++;
		return nFilesFound;
	}

	CDos::PDirectoryTraversal CTRDOS503::BeginDirectoryTraversal() const{
		// initiates exploration of current Directory through a DOS-specific DirectoryTraversal
		return new TTrdosDirectoryTraversal(this);
	}
	bool CTRDOS503::TTrdosDirectoryTraversal::AdvanceToNextEntry(){
		// True <=> found another Entry in current Directory (Empty or not), otherwise False
		while (__existsNextEntry__())
			switch (entryType){
				case TDirectoryTraversal::FILE:
					// File (existing or Deleted)
					if (*(PCBYTE)entry!=TDirectoryEntry::DELETED) return true; // publishing only existing Files
					break;
				case TDirectoryTraversal::EMPTY:
					// the first Empty Entry indicates the end of Directory
					return true;
				//default:
					// error
					//nop (continuing to search Files to publish)
			}
		return false;
	}

	void CTRDOS503::TTrdosDirectoryTraversal::ResetCurrentEntry(BYTE directoryFillerByte) const{
		// gets current entry to the state in which it would be just after formatting
		::memset( entry, directoryFillerByte, entrySize );
		*(PBYTE)entry=TDirectoryEntry::END_OF_DIR;
	}












	struct TDefragParams sealed{
		CTRDOS503 *const trdos;
		const CTRDOS503::PBootSector boot;

		TDefragParams(CTRDOS503 *trdos,CTRDOS503::PBootSector boot)
			: trdos(trdos) , boot(boot) {
		}
	};
	UINT AFX_CDECL CTRDOS503::__defragmentation_thread__(PVOID _pCancelableAction){
		// thread to defragment the disk
		const TBackgroundActionCancelable *const pAction=(TBackgroundActionCancelable *)_pCancelableAction;
		const TDefragParams dp=*(TDefragParams *)pAction->fnParams;
		// - getting the list of Files
		PDirectoryEntry directory[TRDOS503_FILE_COUNT_MAX],*pDeFree=directory;
		BYTE nFiles=dp.trdos->__getDirectory__(directory);
		directory[nFiles]=(PDirectoryEntry)dp.boot; // for the case that it's not necessary to defragment the disk
		// - resetting relevant information in Boot Sector
		dp.boot->nFiles = dp.boot->nFilesDeleted = 0;
		if (dp.trdos->importToSysTrack){
			dp.boot->firstFree.track=0, dp.boot->firstFree.sector=TRDOS503_BOOT_SECTOR_NUMBER;
			dp.boot->nFreeSectors =	dp.trdos->formatBoot.GetCountOfAllSectors()-TRDOS503_SECTOR_RESERVED_COUNT
									+
									TRDOS503_TRACK_SECTORS_COUNT+TRDOS503_SECTOR_FIRST_NUMBER-TRDOS503_BOOT_SECTOR_NUMBER;
		}else{
			dp.boot->firstFree.track=1, dp.boot->firstFree.sector=0;
			dp.boot->nFreeSectors =	dp.trdos->formatBoot.GetCountOfAllSectors()-TRDOS503_SECTOR_RESERVED_COUNT;
			for( BYTE n=0; n<nFiles && !directory[n]->first.track; n++ ){
				// keeping a File that starts at zeroth system Track where it is
				dp.boot->firstFree = directory[n]->first+directory[n]->nSectors;
				if (*(PCBYTE)directory[n]==TDirectoryEntry::DELETED){
					if (dp.boot->firstFree.track){ // if already outside zeroth system Track ...
						dp.boot->firstFree.track=1, dp.boot->firstFree.sector=0; // ... then we can defragment to first official data Sector
						break;
					}
					dp.boot->nFilesDeleted++;
				}
				dp.boot->nFiles++;
			}
		}
		nFiles-=dp.boot->nFiles, pDeFree+=dp.boot->nFiles;
		// - defragmenting
		for( const PDirectoryEntry *pDe=directory+dp.boot->nFiles; nFiles--; pDe++ ){
			if (!pAction->bContinue) return ERROR_CANCELLED;
			const PDirectoryEntry de=(PDirectoryEntry)*pDe;
			if (*(PCBYTE)de!=TDirectoryEntry::DELETED) // an existing (i.e. non-Deleted) File
				if (pDe!=pDeFree){
					// disk is fragmented
					// : exporting the File into Buffer
					BYTE buf[65536]; // big enough to contain any TR-DOS File data
					LPCTSTR errMsg;
					const DWORD fileExportSize=dp.trdos->ExportFile( de, &CMemFile(buf,sizeof(buf)), -1, &errMsg );
					if (errMsg){
						dp.trdos->__showFileProcessingError__(de,errMsg);
						return ERROR_CANCELLED; //TODO: making sure that the disk is in consistent state
					}
					TCHAR tmpName[MAX_PATH];
					dp.trdos->GetFileExportNameAndExt(de,false,tmpName);
					// : importing File data from Buffer to new place in Image
					PDirectoryEntry deFree=*pDeFree++;
					*(PBYTE)deFree=TDirectoryEntry::END_OF_DIR; // marking the DirectoryEntry as the end of Directory (and thus Empty)
					dp.trdos->ImportFile( &CMemFile(buf,sizeof(buf)), fileExportSize, tmpName, 0, (PFile &)deFree );
					// : marking the DirectorySector from which the File was removed as dirty; Directory Sector where the File was moved to marked as dirty during importing its data
					dp.trdos->__markDirectorySectorAsDirty__(de);
				}else{
					// disk (so far) not fragmented - keeping the File where it is
					dp.boot->firstFree = de->first+de->nSectors;
					dp.boot->nFreeSectors-=de->nSectors, dp.boot->nFiles++;
					pDeFree++;
				}
			pAction->UpdateProgress( dp.boot->firstFree.track );
		}
		*(PBYTE)*pDeFree=TDirectoryEntry::END_OF_DIR; // terminating the Directory
		pAction->UpdateProgress(-1); // "-1" = completed
		return ERROR_SUCCESS;
	}
	CDos::TCmdResult CTRDOS503::ProcessCommand(WORD cmd){
		// returns the Result of processing a DOS-related command
		switch (cmd){
			case ID_TRDOS_FILE_LENGTH_FROM_DIRENTRY:
				// export File size given by informatin in DirectoryEntry
				__writeProfileBool__( INI_EXPORT_WHOLE_SECTORS, exportWholeSectors=false );
				return TCmdResult::DONE;
			case ID_TRDOS_FILE_LENGTH_OCCUPIED_SECTORS:
				// export File size given by number of Sectors in FatPath
				__writeProfileBool__( INI_EXPORT_WHOLE_SECTORS, exportWholeSectors=true );
				return TCmdResult::DONE;
			case ID_DOS_FILE_ZERO_LENGTH:
				// enabling/disabling importing of zero-length Files
				__writeProfileBool__( INI_ALLOW_ZERO_LENGTH_FILES, zeroLengthFilesEnabled=!zeroLengthFilesEnabled );
				return TCmdResult::DONE;
			case ID_SYSTEM:{
				// allowing/disabling importing to zero-th system Track (this command is only available if the disk contains no Files)
				const PBootSector boot=__getBootSector__();
				if (importToSysTrack){
					// now on, so turning the switch off (and updating the Boot Sector)
					TSectorId ids[(BYTE)-1];
					TSectorStatus statuses[(BYTE)-1];
					GetSectorStatuses(0,0,__getListOfStdSectors__(0,0,ids),ids,statuses);
					for( BYTE n=TRDOS503_TRACK_SECTORS_COUNT; n>0; )
						if (statuses[--n]==TSectorStatus::EMPTY){
							boot->firstFree.track=1, boot->firstFree.sector=0;
							boot->nFreeSectors--;
						}
					importToSysTrack=false;
				}else{
					// now off, so turning the switch on (and updating the Boot Sector)
					importToSysTrack=true; // this situation is ONLY available if the disk contains no Files
					__warnOnEnteringCriticalConfiguration__(importToSysTrack);
					boot->firstFree.track=0, boot->firstFree.sector=TRDOS503_BOOT_SECTOR_NUMBER;
					boot->nFreeSectors+=TRDOS503_TRACK_SECTORS_COUNT-TRDOS503_BOOT_SECTOR_NUMBER;
				}
				image->MarkSectorAsDirty(TBootSector::CHS);
				return TCmdResult::DONE_REDRAW;
			}
			case ID_DOS_DEFRAGMENT:{
				// defragmenting the disk
				if (image->__reportWriteProtection__()) return TCmdResult::DONE;
				const bool ews0=exportWholeSectors;
					exportWholeSectors=true; // during the defragmentation, File size is given by the number of Sectors in FatPath (as some Files lie about its size in their DirectoryEntries as part of copy-protection scheme)
					if (const PBootSector boot=__getBootSector__())
						TBackgroundActionCancelable(
							__defragmentation_thread__,
							&TDefragParams( this, boot ),
							THREAD_PRIORITY_BELOW_NORMAL
						).CarryOut(1+boot->firstFree.track);
					else
						__errorCannotDoCommand__(ERROR_DEVICE_NOT_AVAILABLE);
				exportWholeSectors=ews0;
				return TCmdResult::DONE_REDRAW;
			}
			case ID_DOS_TAKEATOUR:
				// navigating to the online tour on this DOS
				((CMainWindow *)app.m_pMainWnd)->OpenApplicationPresentationWebPage(_T("Tour"),_T("TRDOS503/tour.html"));
				return TCmdResult::DONE;
		}
		return CSpectrumDos::ProcessCommand(cmd);
	}
	bool CTRDOS503::UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const{
		// True <=> given Command-specific user interface successfully updated, otherwise False
		switch (cmd){
			case ID_TRDOS_FILE_LENGTH_FROM_DIRENTRY:
				pCmdUI->SetRadio(!exportWholeSectors);
				return true;
			case ID_TRDOS_FILE_LENGTH_OCCUPIED_SECTORS:
				pCmdUI->SetRadio(exportWholeSectors);
				return true;
			case ID_DOS_FILE_ZERO_LENGTH:
				pCmdUI->SetCheck(zeroLengthFilesEnabled);
				return true;
			case ID_SYSTEM:{
				pCmdUI->SetCheck(importToSysTrack);
				TStdWinError err;
				pCmdUI->Enable(	importToSysTrack || !GetCountOfItemsInCurrentDir(err) ); // A|B, A = if enabled, the setting can be disabled at any time, B = setting available if the disk yet contains no Files
				return true;
			}
		}
		return CSpectrumDos::UpdateCommandUi(cmd,pCmdUI);
	}
