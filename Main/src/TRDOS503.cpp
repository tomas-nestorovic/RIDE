#include "stdafx.h"
#include "TRDOS.h"

	CTRDOS503::TSectorTrackPair CTRDOS503::TSectorTrackPair::operator+(BYTE nSectors) const{
		// computes and returns the [Sector,Track] identifier that follows N Sectors after the current identifier
		const div_t d=div( track*TRDOS503_TRACK_SECTORS_COUNT+sector+nSectors, TRDOS503_TRACK_SECTORS_COUNT );
		const TSectorTrackPair result={ d.rem, d.quot };
		return result;
	}

	short CTRDOS503::TSectorTrackPair::operator-(const TSectorTrackPair other) const{
		// computes and returns the distance (in Sectors) between this and the Other identifier
		return track*TRDOS503_TRACK_SECTORS_COUNT+sector - (other.track*TRDOS503_TRACK_SECTORS_COUNT+other.sector);
	}

	bool CTRDOS503::TSectorTrackPair::operator<(const TSectorTrackPair other) const{
		// True <=> location of this Sector is before the Other Sector, otherwise False
		return *this-other<0;
	}




	const char CTRDOS503::TDirectoryEntry::KnownExtensions[]={ TDirectoryEntry::BASIC_PRG, TDirectoryEntry::DATA_FIELD, TDirectoryEntry::BLOCK, TDirectoryEntry::PRINT };

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

	#define VERIF_MSG_FILE_WRONG_ORDER	_T("File \"%s\" at wrong position in directory, beware deleting it!")

	UINT AFX_CDECL CTRDOS503::TDirectoryEntry::Verification_thread(PVOID pCancelableAction){
		// thread to verify the Directories
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const TSpectrumVerificationParams &vp=*(TSpectrumVerificationParams *)pAction->GetParams();
		vp.fReport.OpenSection(FILESYSTEM_VERIFICATION);
		// - verifying basic Directory information
		const PTRDOS503 trdos=static_cast<PTRDOS503>(vp.dos);
		const PImage image=trdos->image;
		const PCBootSector boot=TBootSector::Get(image);
		if (!boot)
			return vp.TerminateAll( Utils::ErrorByOs(ERROR_VOLMGR_DISK_INVALID,ERROR_UNRECOGNIZED_VOLUME) );
		pAction->SetProgressTarget(	TRDOS503_BOOT_SECTOR_NUMBER-TRDOS503_SECTOR_FIRST_NUMBER
									+
									1 // checking that all DirectoryEntries are valid
								);
		// - Steps 1-N: verifying Directory Sectors readability
		TPhysicalAddress chs={ 0, 0, {0,trdos->GetSideNumber(0),TRDOS503_SECTOR_FIRST_NUMBER,TRDOS503_SECTOR_LENGTH_STD_CODE} };
		while (chs.sectorId.sector<=TRDOS503_BOOT_SECTOR_NUMBER){
			if (!image->GetHealthySectorData(chs))
				vp.fReport.LogWarning( VERIF_MSG_DIR_SECTOR_BAD, (LPCTSTR)chs.sectorId.ToString() );
			pAction->UpdateProgress( chs.sectorId.sector++ );
		}
		// - getting list of Files (present and deleted)
		PDirectoryEntry directory[TRDOS503_FILE_COUNT_MAX];
		const BYTE nFiles=trdos->__getDirectory__(directory);
		pAction->SetProgressTarget(nFiles);
		// - sorting the list of Files by their logical distance from disk begin (InsertSort)
		for( BYTE i=1,j,skipReordering=0; i<nFiles; i++ ){
			const TDirectoryEntry tmp=*directory[ j=i ];
			bool askedAboutReordering=false;
			do{
				if (tmp.first<directory[j-1]->first){
					if (skipReordering){
						vp.fReport.LogWarning( VERIF_MSG_FILE_WRONG_ORDER, (LPCTSTR)trdos->GetFilePresentationNameAndExt(&tmp) );
						break;
					}
					if (!askedAboutReordering){
						const CString msg=Utils::SimpleFormat( _T("File \"%s\" at wrong position in directory"), trdos->GetFilePresentationNameAndExt(&tmp) );
						switch (vp.ConfirmFix(msg,_T("Sorting by first sector suggested."))){
							case IDCANCEL:
								return vp.CancelAll();
							case IDNO:
								skipReordering=true;
								j++; // to always show warning on misorder, see "j--" below
								continue;
						}
						vp.fReport.CloseProblem( askedAboutReordering=true );
					}
					*directory[j]=*directory[j-1];
					trdos->MarkDirectorySectorAsDirty(directory[j]);
				}else
					break;
			}while (--j>0);
			if (askedAboutReordering){
				*directory[j]=tmp;
				trdos->MarkDirectorySectorAsDirty(directory[j]);
			}
		}
		// - verifying that all DirectoryEntries are valid
		for( TTrdosDirectoryTraversal dt(trdos); dt.AdvanceToNextEntry(); )
			if (dt.entryType==TDirectoryTraversal::FILE){
				const PDirectoryEntry de=(PDirectoryEntry)dt.entry;
				const CString strItemId=Utils::SimpleFormat( _T("File \"%s\""), trdos->GetFilePresentationNameAndExt(de) );
				// . verifying Extension
				switch (de->extension){
					case TExtension::BASIC_PRG:
					case TExtension::DATA_FIELD:
					case TExtension::BLOCK:
					case TExtension::PRINT:
						break; //nop
					default:
						vp.fReport.LogWarning( VERIF_MSG_FILE_NONSTANDARD, (LPCTSTR)strItemId );
						break;
				}
				// . verifying Name
				if (de->extension!=TExtension::BASIC_PRG)
					// non-Program Files may contain non-printable characters
					vp.WarnSomeCharactersNonPrintable( strItemId, VERIF_FILE_NAME, de->name, sizeof(de->name), ' ' );
				else if (const TStdWinError err=vp.VerifyAllCharactersPrintable( dt.chs, strItemId, VERIF_FILE_NAME, de->name, sizeof(de->name), ' ' ))
					// Program names are usually typed in by the user and thus may not contain non-printable characters
					return vp.TerminateAll(err);
				// . verifying the start Sector
				if (de->first.track>=trdos->formatBoot.GetCountOfAllTracks()){
					const CFatPath tmp(trdos,de);
					CFatPath::PCItem p; DWORD n;
					tmp.GetItems(p,n);
					vp.fReport.LogWarning( _T("%s: First sector with %s out of disk"), (LPCTSTR)strItemId, (LPCTSTR)p->chs.sectorId.ToString() );
				}
				// . verifying parameter after data
				WORD w; bool AA80=false;
				switch (de->extension){
					case TDirectoryEntry::BASIC_PRG:
					case TDirectoryEntry::DATA_FIELD:
						if (!trdos->__parameterAfterData__(de,false,w,&AA80))
							vp.fReport.LogWarning( _T("%s: Missing Parameter 1 after data"), (LPCTSTR)strItemId );
						else if (!AA80)
							vp.fReport.LogWarning( _T("%s: Parameter 1 after data not prefixed with 0xAA80 mark"), (LPCTSTR)strItemId );
						break;
				}
			}
		// - successfully verified
		return pAction->TerminateWithSuccess();
	}



	#define INI_TRDOS	_T("TRDOS")
	#define INI_ALLOW_ZERO_LENGTH_FILES	_T("fm0files")

	CTRDOS503::CTRDOS503(PImage image,PCFormat pFormatBoot,PCProperties pTrdosProps)
		// ctor
		// - base
		: CSpectrumDos( image, pFormatBoot, TTrackScheme::BY_CYLINDERS,pTrdosProps, IDR_TRDOS, &fileManager, TGetFileSizeOptions::SizeOnDisk, TSectorStatus::UNAVAILABLE )
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
		, importToSysTrack(false) {
		if (formatBoot.mediumType!=Medium::UNKNOWN // may be unknown if creating a new Image
			&&
			image->GetCylinderCount() // Image is initialized (e.g. isn't when reconstructing a temporary TRD Image from an input SCL Image)
		){
			PDirectoryEntry directory[TRDOS503_FILE_COUNT_MAX];
			if (__getDirectory__(directory))
				importToSysTrack=!directory[0]->first.track; // turned on if the first File starts in system Track
		}
	}










	void CTRDOS503::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_TRDOS, messageId );
	}









	TSide CTRDOS503::GetSideNumber(THead head) const{
		return	image->GetSideMap() // Side numbers are ignored by TR-DOS ...
				? image->GetSideMap()[head] // ... so formally prefer numbering by Image, if available ...
				: StdSidesMap[head]; // ... turning to default numbering otherwise (e.g. when creating a new Image)
	}

	bool CTRDOS503::GetSectorStatuses(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PSectorStatus buffer) const{
		// True <=> Statuses of all Sectors in the Track successfully retrieved and populated the Buffer, otherwise False
		const PCBootSector bootSector=GetBootSector();
		if (!bootSector){
			while (nSectors--) *buffer++=TSectorStatus::UNKNOWN;
			return false;
		}
		bool result=true; // assumption (Statuses of all Sectors retrieved)
		// - composing the List of "regions" on disk
		struct{
			TSectorTrackPair start; // length determined as the difference between this and previous "region"
			TSectorStatus status;
		} regions[TRDOS503_FILE_COUNT_MAX*2+2],*pRgn=regions; // "*2" = each File may be followed by a gap consisting of Unavailable Sectors, "+2" = empty space and terminator (see below)
		pRgn->start.sector=TRDOS503_BOOT_SECTOR_NUMBER-TRDOS503_SECTOR_FIRST_NUMBER, pRgn->start.track=0; // just in case the first DirectoryEntry isn't found (as Sector not found)
		// - composing a map of Regions on the disk
		TSectorTrackPair lastRegionStart={0,0};
		for( TTrdosDirectoryTraversal dt(this); dt.AdvanceToNextEntry(); )
			if (dt.entryType!=TDirectoryTraversal::WARNING){
				const PCDirectoryEntry de=(PCDirectoryEntry)dt.entry;
				if ((dt.entryType==TDirectoryTraversal::FILE||dt.entryType==TDirectoryTraversal::CUSTOM) && !de->__isTemporary__()){
					// (A|B)&C: A = existing File, B = Deleted File, C = not a temporary Entry in Directory (see also ImportFile)
					if (de->first<lastRegionStart) // a File further in the Directory is reported to appear earlier on the disk; this shouldn't happen, but if it does ...
						continue; // ... such File simply isn't included in the Regions
					if (lastRegionStart<de->first){ // there is a "gap" of Unavailable Sectors
						pRgn->start=lastRegionStart;
						pRgn++->status=TSectorStatus::UNAVAILABLE;
					}
					pRgn->start = de->first;
					pRgn++->status=	*(PCBYTE)de!=TDirectoryEntry::DELETED
									? TSectorStatus::OCCUPIED
									: TSectorStatus::SKIPPED ;
					lastRegionStart = pRgn->start = de->first+de->nSectors; // for the case that next DirectoryEntry not found (as Sector not found)
				}else{
					// end of Directory, or Directory Sector not found
					if (dt.entryType==TDirectoryTraversal::EMPTY || de->__isTemporary__()) // A|B; A = natural end of Directory, B = a temporary Entry in Directory (see also ImportFile)
						break; // the end of Directory is followed by empty space (see after cycle)
					pRgn++->status=TSectorStatus::UNKNOWN; // Sector and Track set by previous known File, see above
					*pRgn=*(pRgn-1); // for the case that next DirectoryEntry not found (as Sector not found)
					result=false;
				}
			}
		if (lastRegionStart<bootSector->firstFree){ // there is a "gap" of Unavailable Sectors
			pRgn->start=lastRegionStart;
			pRgn++->status=TSectorStatus::UNAVAILABLE;
		}
		pRgn++->status=TSectorStatus::EMPTY, pRgn->start.sector=0, pRgn->start.track=formatBoot.nCylinders*formatBoot.nHeads; // terminator
		// - determining the Statuses of Sectors
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
	bool CTRDOS503::ModifyStdSectorStatus(RCPhysicalAddress,TSectorStatus) const{
		// True <=> the Status of the specified DOS-standard Sector successfully changed, otherwise False
		return true; // True = silently ignoring this request (as there's not FAT) - otherwise the caller might show an error message
	}
	DWORD CTRDOS503::GetFreeSpaceInBytes(TStdWinError &rError) const{
		// computes and returns the empty space on disk
		if (const PCBootSector boot=GetBootSector()){
			rError=ERROR_SUCCESS;
			return boot->nFreeSectors*TRDOS503_SECTOR_LENGTH_STD;
		}else{
			rError=Utils::ErrorByOs( ERROR_VOLMGR_DISK_INVALID, ERROR_UNRECOGNIZED_VOLUME );
			return 0;
		}
	}








	bool CTRDOS503::GetFileFatPath(PCFile file,CFatPath &rFatPath) const{
		// True <=> FatPath of given File (even an erroneous FatPath) successfully retrieved, otherwise False
		// - if queried about the root Directory, populating the FatPath with root Directory Sectors
		CFatPath::TItem item;
		item.chs.sectorId.lengthCode=TRDOS503_SECTOR_LENGTH_STD_CODE;
		if (file==ZX_DIR_ROOT){
			item.chs.cylinder = item.chs.sectorId.cylinder = 0;
			item.chs.sectorId.side=GetSideNumber( item.chs.head=0 );
			for( item.chs.sectorId.sector=TRDOS503_SECTOR_FIRST_NUMBER; item.chs.sectorId.sector<TRDOS503_BOOT_SECTOR_NUMBER; item.chs.sectorId.sector++,item.value++ ) // incrementing Value just to guarantee it's unique for each Sector
				if (!rFatPath.AddItem(&item)) break; // also sets an error in FatPath
			return true;
		}
		// - no FatPath can be retrieved if File is Deleted
		if (*(PCBYTE)file==TDirectoryEntry::DELETED)
			return false;
		// - composing the FatPath
		const PCDirectoryEntry de=(PCDirectoryEntry)file;
		const div_t B=div( de->first.track, formatBoot.nHeads );
			item.chs.cylinder = item.chs.sectorId.cylinder = B.quot,
			item.chs.sectorId.side=GetSideNumber( item.chs.head=B.rem ),
			item.chs.sectorId.sector=TRDOS503_SECTOR_FIRST_NUMBER+de->first.sector;
			BYTE nBytesAfterData;
			item.value=reinterpret_cast<PCDos>(this)->GetFileSize(de,nullptr,&nBytesAfterData) + TRDOS503_SECTOR_LENGTH_STD-1; // "+N" = rounding up
			item.value+=nBytesAfterData;
		for( item.value/=formatBoot.sectorLength; item.value--; ){ // each Item gets a unique Value
			// . adding the Item to the FatPath
			if (!rFatPath.AddItem(&item)) break; // also sets an error in FatPath
			// . VALIDATION: Value must "make sense"
			if (item.chs.sectorId.cylinder>=formatBoot.nCylinders){
				rFatPath.error=CFatPath::TError::VALUE_INVALID;
				break;
			}
			// . determining the PhysicalAddress of the next Sector
			if (++item.chs.sectorId.sector>formatBoot.nSectors){
				item.chs.sectorId.sector=TRDOS503_SECTOR_FIRST_NUMBER;
				if (++item.chs.head==formatBoot.nHeads){
					item.chs.sectorId.side=GetSideNumber( item.chs.head=0 );
					item.chs.sectorId.cylinder=++item.chs.cylinder;
				}else
					item.chs.sectorId.side=GetSideNumber( item.chs.head );
			}
		}
		return true;
	}

	bool CTRDOS503::ModifyFileFatPath(PFile file,const CFatPath &rFatPath) const{
		// True <=> a error-free FatPath of given File successfully written, otherwise False
		return false; // once set, the FatPath cannot be modified
	}

	bool CTRDOS503::GetFileNameOrExt(PCFile file,PPathString pOutName,PPathString pOutExt) const{
		// populates the Buffers with File's name and extension; caller guarantees that the Buffer sizes are at least MAX_PATH characters each
		if (file==ZX_DIR_ROOT){
			if (pOutName)
				*pOutName=CPathString::Root;
			if (pOutExt)
				*pOutExt=CPathString::Empty;
		}else{
			const PCDirectoryEntry de=(PCDirectoryEntry)file;
			if (pOutName)
				( *pOutName=CPathString(de->name,TRDOS503_FILE_NAME_LENGTH_MAX) ).TrimRight(' '); // trimming trailing spaces
			if (pOutExt)
				*pOutExt=de->extension;
		}
		return true; // name relevant
	}
	TStdWinError CTRDOS503::ChangeFileNameAndExt(PFile file,RCPathString newName,RCPathString newExt,PFile &rRenamedFile){
		// tries to change given File's name and extension; returns Windows standard i/o error
		// - can't change root Directory's name
		if (file==ZX_DIR_ROOT)
			return ERROR_ACCESS_DENIED;
		// - checking that the NewName+NewExt combination follows the "8.1" convention
		if (newExt.GetLength()<1)
			return ERROR_BAD_FILE_TYPE;
		if (newName.GetLength()>TRDOS503_FILE_NAME_LENGTH_MAX || newExt.GetLength()>1)
			return ERROR_FILENAME_EXCED_RANGE;
		// - making sure that a File with given NameAndExtension doesn't yet exist
		if ( rRenamedFile=FindFileInCurrentDir(newName,newExt,file) )
			return ERROR_FILE_EXISTS;
		// - getting important information about the File
		const PDirectoryEntry de=(PDirectoryEntry)file;
		const WORD officialFileSize=de->__getOfficialFileSize__(nullptr);
		TStdParameters stdParams=TStdParameters::Default;
			__getStdParameter1__(de,stdParams.param1), __getStdParameter2__(de,stdParams.param2);
		// - renaming
		TDirectoryEntry tmp=*de; // all changes are made to a temporary Entry before they are copied to disk
		tmp.extension=newExt.FirstChar();
		newName.MemcpyAnsiTo( tmp.name, TRDOS503_FILE_NAME_LENGTH_MAX, ' ' );
		// - setting important information about the File
		tmp.parameterA = tmp.parameterB = officialFileSize;
		__setStdParameter1__(&tmp,stdParams.param1), __setStdParameter2__(&tmp,stdParams.param2);
		//if (!__setStdParameter1__(&tmp,stdParams.param1) || !__setStdParameter2__(&tmp,stdParams.param2))
			//return ERROR_DS_SIZELIMIT_EXCEEDED; //ERROR_INCORRECT_SIZE;
		// - marking the corresponding Directory Sector as dirty
		*de=tmp;
		MarkDirectorySectorAsDirty( rRenamedFile=file );
		return ERROR_SUCCESS;
	}
	DWORD CTRDOS503::GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData,TGetFileSizeOptions option) const{
		// determines and returns the size of specified File
		if (pnBytesReservedBeforeData) *pnBytesReservedBeforeData=0;
		if (pnBytesReservedAfterData) *pnBytesReservedAfterData=0;
		const PCDirectoryEntry de=(PCDirectoryEntry)file;
		if (de==ZX_DIR_ROOT)
			return (TRDOS503_BOOT_SECTOR_NUMBER-TRDOS503_SECTOR_FIRST_NUMBER)*TRDOS503_SECTOR_LENGTH_STD;
		else
			switch (option){
				case TGetFileSizeOptions::OfficialDataLength:
					return de->__getOfficialFileSize__(pnBytesReservedAfterData);
				case TGetFileSizeOptions::SizeOnDisk:
					return de->__getFileSizeOnDisk__();
				default:
					ASSERT(FALSE);
					return 0;
			}	
	}

	TStdWinError CTRDOS503::DeleteFile(PFile file){
		// deletes specified File; returns Windows standard i/o error
		if (file==ZX_DIR_ROOT)
			return ERROR_ACCESS_DENIED; // can't delete the root Directory
		if (*(PCBYTE)file!=TDirectoryEntry::DELETED && *(PCBYTE)file!=TDirectoryEntry::END_OF_DIR) // File mustn't be already Deleted (may happen during moving it in FileManager)
			if (const PBootSector boot=GetBootSector()){
				PDirectoryEntry directory[TRDOS503_FILE_COUNT_MAX];
				MarkDirectorySectorAsDirty(file);
				this->boot.MarkSectorAsDirty();
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
				return Utils::ErrorByOs( ERROR_VOLMGR_DISK_INVALID, ERROR_UNRECOGNIZED_VOLUME );
		return ERROR_SUCCESS;
	}

	#define INFO_FILE_EX		_T("T%x")
	
	CDos::CPathString CTRDOS503::GetFileExportNameAndExt(PCFile file,bool shellCompliant) const{
		// returns File name concatenated with File extension for export of the File to another Windows application (e.g. Explorer)
		CPathString result=__super::GetFileExportNameAndExt(file,shellCompliant);
		if (!shellCompliant){
			// exporting to another RIDE instance
			const PCDirectoryEntry de=(PCDirectoryEntry)file;
			TStdParameters params=TStdParameters::Default;
				__getStdParameter1__(de,params.param1), __getStdParameter2__(de,params.param2);
			TUniFileType uts;
				switch (de->extension){
					case TDirectoryEntry::BASIC_PRG	: uts=TUniFileType::PROGRAM; break;
					case TDirectoryEntry::DATA_FIELD: uts=TUniFileType::CHAR_ARRAY; break;
					case TDirectoryEntry::BLOCK		: uts=TUniFileType::BLOCK; break;
					case TDirectoryEntry::PRINT		: uts=TUniFileType::PRINT; break;
					default							: uts=TUniFileType::UNKNOWN; break;
				}
			TCHAR buf[80];
			::wsprintf(	buf+__exportFileInformation__( buf, uts, params, de->__getOfficialFileSize__(nullptr) ),
						INFO_FILE_EX, de->nSectors
					);
			result+=buf;
		}
		return result;
	}

	TStdWinError CTRDOS503::ImportFile(CFile *f,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile){
		// imports specified File (physical or virtual) into the Image; returns Windows standard i/o error
		// - parsing the NameAndExtension into a usable "8.1" form
		CPathString zxName,zxExt; LPCTSTR zxInfo;
		TCHAR buf[16384];
		__parseFat32LongName__(	::lstrcpy(buf,nameAndExtension), zxName, zxExt, zxInfo );
		zxName.TrimToLength(TRDOS503_FILE_NAME_LENGTH_MAX);
		zxExt.TrimToLength(1);
		// - getting import information
		TStdParameters params;	TUniFileType uts;	DWORD dw;
		const LPCTSTR pTrdosSpecificInfo=zxInfo+__importFileInformation__(zxInfo,uts,params,dw);
		const DWORD fileSizeFormal=	pTrdosSpecificInfo>zxInfo // if record on official File size exists in ZxInformation ...
									? dw // ... use that record
									: fileSize; // ... otherwise take as the official File size the actual size of imported file
		// - determining how much space the File will take on disk
		const DWORD fileSizeOnDisk=	uts==TUniFileType::PROGRAM || uts==TUniFileType::NUMBER_ARRAY || uts==TUniFileType::CHAR_ARRAY
									? std::max(fileSizeFormal,fileSize)+4 // "+4" = (WORD)0xAA80 (the mark that introduces a parameter "after" official data) and a WORD parameter
									: fileSize;
		// - checking if there's enough empty space on disk
		TStdWinError err;
		if (fileSizeOnDisk>GetFreeSpaceInBytes(err))
			return ERROR_DISK_FULL;
		// - checking that the File has "correct size", i.e. neither zero (unless allowed) nor too long
		if ( !fileSize&&!zeroLengthFilesEnabled || fileSizeOnDisk>0xff00)
			return ERROR_BAD_LENGTH;
		// - changing the Extension according to the "universal" type valid across ZX platforms (as MDOS File "Picture.B" should be take on the name "Picture.C" under TR-DOS)
		TCHAR uftExt;
		switch (uts){
			case TUniFileType::PROGRAM	: uftExt=TDirectoryEntry::BASIC_PRG; break;
			case TUniFileType::CHAR_ARRAY:uftExt=TDirectoryEntry::DATA_FIELD; break;
			case TUniFileType::BLOCK	:
			case TUniFileType::SCREEN	: uftExt=TDirectoryEntry::BLOCK; break;
			case TUniFileType::PRINT	: uftExt=TDirectoryEntry::PRINT; break;
			default:
				uftExt= zxExt.GetLength() ? zxExt.FirstChar() : TDirectoryEntry::BLOCK;
				break;
		}
		// - initializing the description of File to import
		const PBootSector boot=GetBootSector();
		if (!boot)
			return Utils::ErrorByOs( ERROR_VOLMGR_DISK_INVALID, ERROR_UNRECOGNIZED_VOLUME );
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
		if (err=__importFileData__( f, &tmp, zxName, uftExt, fileSize, false, rFile, fatPath ))
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
		this->boot.MarkSectorAsDirty();
		// - terminating the Directory
		PDirectoryEntry directory[TRDOS503_FILE_COUNT_MAX+1],*p=directory; // "+1" = terminator
		for( directory[__getDirectory__(directory)]=(PDirectoryEntry)boot; *p!=de; p++ ); // Boot Sector as terminator
		*(PBYTE)*++p=TDirectoryEntry::END_OF_DIR;
		MarkDirectorySectorAsDirty(*p);
		// - File successfully imported to Image
		return ERROR_SUCCESS;
	}

	bool CTRDOS503::__parameterAfterData__(PCDirectoryEntry de,bool modify,WORD &rw,bool *pAA80) const{
		// True <=> parameter after given File's data successfully get/set, otherwise False
		WORD officialFileSize=de->__getOfficialFileSize__(nullptr);
		WORD buf[2]={ 0xAA80, rw }; // 0xAA80 = the mark that introduces a parameter "after" official data
		for( BYTE n=4,*p=(PBYTE)buf; n--; officialFileSize++ ){
			const TSector sector=officialFileSize/TRDOS503_SECTOR_LENGTH_STD;
			if (sector>=de->nSectors) return false;
			const TSectorTrackPair A = de->first+sector;
			const div_t B=div(A.track,formatBoot.nHeads);
			const TPhysicalAddress chs={ B.quot, B.rem, { B.quot, GetSideNumber(B.rem), A.sector+TRDOS503_SECTOR_FIRST_NUMBER, TRDOS503_SECTOR_LENGTH_STD_CODE } };
			if (const PSectorData data=image->GetHealthySectorData(chs)){
				if (modify){
					((PBYTE)data)[officialFileSize&(TRDOS503_SECTOR_LENGTH_STD-1)]=*p++;
					image->MarkSectorAsDirty(chs);
				}else
					*p++=((PBYTE)data)[officialFileSize&(TRDOS503_SECTOR_LENGTH_STD-1)];
			}else
				return false;
		}
		if (pAA80) *pAA80=*buf==0xAA80;
		rw=buf[1];
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
				return __parameterAfterData__(de,false,rParam1);
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
				return __parameterAfterData__(de,true,newParam1);
			case TDirectoryEntry::BLOCK:
				// start address
				//fallthrough
			case TDirectoryEntry::PRINT:
				// sequence number
				//fallthrough
			default:
				de->parameterA=newParam1;
				MarkDirectorySectorAsDirty(de);
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
				MarkDirectorySectorAsDirty(de);
				return true;
		}
	}









	TStdWinError CTRDOS503::CreateUserInterface(HWND hTdi){
		// creates DOS-specific Tabs in TDI; returns Windows standard i/o error
		// - base
		if (const TStdWinError err=__super::CreateUserInterface(hTdi))
			return err;
		// - creating the user interface
		CTdiCtrl::AddTabLast( hTdi, BOOT_SECTOR_TAB_LABEL, &boot.tab, false, TDI_TAB_CANCLOSE_NEVER, nullptr );
		CTdiCtrl::AddTabLast( hTdi, FILE_MANAGER_TAB_LABEL, &fileManager.tab, true, TDI_TAB_CANCLOSE_NEVER, nullptr );
		// - informing on how the SCL Images are opened
		if (dynamic_cast<CSCL *>(image)!=nullptr)
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
		: TDirectoryTraversal( ZX_DIR_ROOT, sizeof(TDirectoryEntry) )
		// - initialization
		, trdos(_trdos) , foundEndOfDirectory(false)
		// - getting ready to read the first Directory Sector
		, nRemainingEntriesInSector(0) {
		chs.cylinder = chs.sectorId.cylinder = 0,
		chs.sectorId.side=_trdos->GetSideNumber( chs.head=0 ),
		chs.sectorId.sector=TRDOS503_SECTOR_FIRST_NUMBER-1;
		chs.sectorId.lengthCode=TRDOS503_SECTOR_LENGTH_STD_CODE;
		// - buffering the whole Directory (to eventually speed-up reading from a real floppy)
		TSectorId buffer[TRDOS503_TRACK_SECTORS_COUNT];
		trdos->GetListOfStdSectors(0,0,buffer);
		trdos->image->BufferTrackData( 0, 0, Revolution::ANY_GOOD, buffer, Utils::CByteIdentity(), TRDOS503_BOOT_SECTOR_NUMBER ); // including the Boot Sector (to not have to include another named constant)
	}
	bool CTRDOS503::TTrdosDirectoryTraversal::AdvanceToNextEntry(){
		// True <=> another Entry in current Directory exists (Empty or not), otherwise False
		// - if end of Directory reached, we are done
		if (foundEndOfDirectory)
			return false;
		// - getting the next Sector with Directory
		if (!nRemainingEntriesInSector){
			if (++chs.sectorId.sector>=TRDOS503_BOOT_SECTOR_NUMBER){ // end of Directory
				entryType=TDirectoryTraversal::END;
				foundEndOfDirectory=true;
				return false;
			}
			entry=trdos->image->GetHealthySectorData(chs);
			if (!entry) // Directory Sector not found
				entryType=TDirectoryTraversal::WARNING, warning=ERROR_SECTOR_NOT_FOUND;
			else
				entryType=TDirectoryTraversal::UNKNOWN, entry=(PDirectoryEntry)entry-1; // pointer set "before" the first DirectoryEntry
			nRemainingEntriesInSector=TRDOS503_DIR_SECTOR_ENTRIES_COUNT;
		}
		// - getting the next DirectoryEntry
		entry=(PDirectoryEntry)entry+1, nRemainingEntriesInSector--;
		if (!foundEndOfDirectory && entryType!=TDirectoryTraversal::WARNING)
			if (*(PCBYTE)entry!=TDirectoryEntry::END_OF_DIR)
				entryType =	*(PCBYTE)entry!=TDirectoryEntry::DELETED
							? TDirectoryTraversal::FILE
							: TDirectoryTraversal::CUSTOM;
			else{
				entryType=TDirectoryTraversal::EMPTY;
				foundEndOfDirectory=true;
			}
		return true;
	}

	BYTE CTRDOS503::__getDirectory__(PDirectoryEntry *directory) const{
		// returns the length of available Directory including Deleted Files; assumed that the buffer is big enough to contain maximally occupied Directory
		BYTE nFilesFound=0;
		for( TTrdosDirectoryTraversal dt(this); dt.AdvanceToNextEntry(); )
			if (dt.entryType==TDirectoryTraversal::FILE || dt.entryType==TDirectoryTraversal::CUSTOM)
				// existing or Deleted File
				*directory++=(PDirectoryEntry)dt.entry, nFilesFound++;
		return nFilesFound;
	}

	std::unique_ptr<CDos::TDirectoryTraversal> CTRDOS503::BeginDirectoryTraversal(PCFile directory) const{
		// initiates exploration of specified Directory through a DOS-specific DirectoryTraversal
		ASSERT(directory==ZX_DIR_ROOT);
		return std::unique_ptr<TDirectoryTraversal>( new TTrdosDirectoryTraversal(this) );
	}

	void CTRDOS503::TTrdosDirectoryTraversal::ResetCurrentEntry(BYTE directoryFillerByte){
		// gets current entry to the state in which it would be just after formatting
		if (entryType==TDirectoryTraversal::FILE || entryType==TDirectoryTraversal::EMPTY){
			*(PBYTE)::memset( entry, directoryFillerByte, entrySize )=TDirectoryEntry::DELETED;
			entryType=TDirectoryTraversal::EMPTY;
		}
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
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		const TDefragParams dp=*(TDefragParams *)pAction->GetParams();
		pAction->SetProgressTarget( 1+dp.boot->firstFree.track );
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
			if (pAction->Cancelled) return ERROR_CANCELLED;
			const PDirectoryEntry de=(PDirectoryEntry)*pDe;
			if (*(PCBYTE)de!=TDirectoryEntry::DELETED) // an existing (i.e. non-Deleted) File
				if (pDe!=pDeFree){
					// disk is fragmented
					// : exporting the File into Buffer
					BYTE buf[65536]; // big enough to contain any TR-DOS File data
					LPCTSTR errMsg;
					const DWORD fileExportSize=dp.trdos->ExportFile( de, &CMemFile(buf,sizeof(buf)), sizeof(buf), &errMsg );
					if (errMsg){
						dp.trdos->ShowFileProcessingError(de,errMsg);
						return ERROR_CANCELLED; //TODO: making sure that the disk is in consistent state
					}
					const CPathString tmpName=dp.trdos->GetFileExportNameAndExt(de,false);
					// : importing File data from Buffer to new place in Image
					PDirectoryEntry deFree=*pDeFree++;
					*(PBYTE)deFree=TDirectoryEntry::END_OF_DIR; // marking the DirectoryEntry as the end of Directory (and thus Empty)
					dp.trdos->ImportFile( &CMemFile(buf,sizeof(buf)), fileExportSize, tmpName, 0, (PFile &)deFree );
					// : marking the DirectorySector from which the File was removed as dirty; Directory Sector where the File was moved to marked as dirty during importing its data
					dp.trdos->MarkDirectorySectorAsDirty(de);
				}else{
					// disk (so far) not fragmented - keeping the File where it is
					dp.boot->firstFree = de->first+de->nSectors;
					dp.boot->nFreeSectors-=de->nSectors, dp.boot->nFiles++;
					pDeFree++;
				}
			pAction->UpdateProgress( dp.boot->firstFree.track );
		}
		*(PBYTE)*pDeFree=TDirectoryEntry::END_OF_DIR; // terminating the Directory
		return pAction->TerminateWithSuccess();
	}
	CDos::TCmdResult CTRDOS503::ProcessCommand(WORD cmd){
		// returns the Result of processing a DOS-related command
		switch (cmd){
			case ID_DOS_VERIFY:{
				// volume verification
				static constexpr TVerificationFunctions vf={
					TBootSector::Verification_thread, // Boot Sector
					nullptr, // FAT readability (doesn't have FAT)
					nullptr, // FAT Files OK (doesn't have FAT)
					CrossLinkedFilesVerification_thread, // FAT crossed Files
					nullptr, // FAT lost allocation units (doesn't have FAT)
					TDirectoryEntry::Verification_thread, // Filesystem
					TVerificationFunctions::WholeDiskSurfaceVerification_thread // Volume surface
				};
				__verifyVolume__(
					CVerifyVolumeDialog( TSpectrumVerificationParams(this,vf) )
				);
				return TCmdResult::DONE_REDRAW;
			}
			case ID_DOS_FILE_ZERO_LENGTH:
				// enabling/disabling importing of zero-length Files
				__writeProfileBool__( INI_ALLOW_ZERO_LENGTH_FILES, zeroLengthFilesEnabled=!zeroLengthFilesEnabled );
				return TCmdResult::DONE;
			case ID_SYSTEM:{
				// allowing/disabling importing to zero-th system Track (this command is only available if the disk contains no Files)
				const PBootSector boot=GetBootSector();
				if (importToSysTrack){
					// now on, so turning the switch off (and updating the Boot Sector)
					TSectorId ids[(BYTE)-1];
					TSectorStatus statuses[(BYTE)-1];
					GetSectorStatuses(0,0,GetListOfStdSectors(0,0,ids),ids,statuses);
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
				this->boot.MarkSectorAsDirty();
				return TCmdResult::DONE_REDRAW;
			}
			case ID_DOS_DEFRAGMENT:{
				// defragmenting the disk
				if (image->ReportWriteProtection()) return TCmdResult::DONE;
				const Utils::CVarTempReset<TGetFileSizeOptions> gfs0( getFileSizeDefaultOption, TGetFileSizeOptions::SizeOnDisk ); // during the defragmentation, File size is given by the number of Sectors in FatPath (as some Files lie about its size in their DirectoryEntries as part of copy-protection scheme)
					if (const PBootSector boot=GetBootSector())
						CBackgroundActionCancelable(
							__defragmentation_thread__,
							&TDefragParams( this, boot ),
							THREAD_PRIORITY_BELOW_NORMAL
						).Perform();
					else
						__errorCannotDoCommand__( Utils::ErrorByOs(ERROR_VOLMGR_DISK_INVALID,ERROR_UNRECOGNIZED_VOLUME) );
				return TCmdResult::DONE_REDRAW;
			}
			case ID_DOS_TAKEATOUR:
				// navigating to the online tour on this DOS
				app.GetMainWindow()->OpenApplicationPresentationWebPage(_T("Tour"),_T("TRDOS503/tour.html"));
				return TCmdResult::DONE;
		}
		return __super::ProcessCommand(cmd);
	}
	bool CTRDOS503::UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const{
		// True <=> given Command-specific user interface successfully updated, otherwise False
		switch (cmd){
			case ID_DOS_FILE_ZERO_LENGTH:
				pCmdUI->SetCheck(zeroLengthFilesEnabled);
				return true;
			case ID_SYSTEM:{
				pCmdUI->SetCheck(importToSysTrack);
				TStdWinError err;
				pCmdUI->Enable(	image->properties!=&CSCL::Properties // import to system Track available only for non-SCL Images
								&&
								( importToSysTrack || !GetCountOfItemsInCurrentDir(err) ) // A|B, A = if enabled, the setting can be disabled at any time, B = setting available if the disk yet contains no Files
							);
				return true;
			}
		}
		return __super::UpdateCommandUi(cmd,pCmdUI);
	}









	UINT AFX_CDECL CTRDOS503::CrossLinkedFilesVerification_thread(PVOID pCancelableAction){
		// thread to find and separate cross-linked Files on current volume
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const TSpectrumVerificationParams &vp=*(TSpectrumVerificationParams *)pAction->GetParams();
		vp.fReport.OpenSection(FAT_VERIFICATION_CROSSLINKED);
		const auto trdos=static_cast<CTRDOS503 *>(vp.dos);
		const PImage image=trdos->image;
		const PBootSector boot=TBootSector::Get(image);
		if (!boot)
			return vp.TerminateAll( Utils::ErrorByOs(ERROR_VOLMGR_DISK_INVALID,ERROR_UNRECOGNIZED_VOLUME) );
		// - getting list of Files (present and deleted)
		PDirectoryEntry directory[TRDOS503_FILE_COUNT_MAX];
		const BYTE nFiles=trdos->__getDirectory__(directory);
		pAction->SetProgressTarget(nFiles);
		// - sorting the list of Files by their logical distance from disk begin (InsertSort)
		for( BYTE i=1,j; i<nFiles; i++ ){
			const PDirectoryEntry de=directory[ j=i ];
			bool warnedAboutWrongPosition=false;
			do{
				if (de->first<directory[j-1]->first){
					if (!warnedAboutWrongPosition){
						vp.fReport.LogWarning( VERIF_MSG_FILE_WRONG_ORDER, (LPCTSTR)trdos->GetFilePresentationNameAndExt(de) );
						warnedAboutWrongPosition=true;
					}
					directory[j]=directory[j-1];
				}else
					break;
			}while (--j>0);
			directory[j]=de;
		}
		// - determining the number of Sectors to shift each File's content by
		WORD shiftSectors[TRDOS503_FILE_COUNT_MAX];
		*shiftSectors=0; // first File won't be shifted
		for( BYTE i=1; i<nFiles; i++ ){
			const PCDirectoryEntry dePrev=directory[i-1];
			const TSectorTrackPair endOfPrevFile=dePrev->first+dePrev->nSectors;
			const PCDirectoryEntry de=directory[i];
			shiftSectors[i]=shiftSectors[i-1];
			if (de->first<endOfPrevFile)
				shiftSectors[i]+=endOfPrevFile-de->first;
		}
		// - shifting the Files
		const PCDirectoryEntry deLast=directory[nFiles-1];
		for( BYTE i=nFiles,buffer[65536]; i; pAction->UpdateProgress(nFiles-i) )
			if (const WORD n=shiftSectors[--i]){ // the File is cross-linked with another File
				// . confirming the resolution
				const PDirectoryEntry de=directory[i];
				const CString &&fileName=trdos->GetFilePresentationNameAndExt(de);
				CString msg; LPCTSTR suggestion;
				if (shiftSectors[i-1]<n){
					msg=Utils::SimpleFormat( _T("File \"%s\" in conflict with \"%s\""), fileName, trdos->GetFilePresentationNameAndExt(directory[i-1]) );
					suggestion=VERIF_MSG_FILE_UNCROSS;
				}else{
					msg=Utils::SimpleFormat( _T("File \"%s\" must be shifted to resolve conflicts earlier on the disk"), fileName );
					suggestion=_T("");
				}
				switch (vp.ConfirmFix(msg,suggestion)){
					case IDCANCEL:
						return vp.CancelAll();
					case IDNO:
						return vp.TerminateAndGoToNextAction(ERROR_VALIDATE_CONTINUE);
				}
				// . reading the File
				LPCTSTR err=nullptr;
				const DWORD fileExportSize=trdos->ExportFile( de, &CMemFile(buffer,sizeof(buffer)), sizeof(buffer), &err );
				if (err){
					msg.Format( _T("%s: %s"), (LPCTSTR)fileName, err );
					return vp.TerminateAndGoToNextAction((LPCTSTR)msg);
				}
				// . writing the File to new location
				if (fileExportSize){ // zero-length Files have no Sectors associated with them
					const auto firstFree0=boot->firstFree;
					boot->firstFree=de->first+n;
					CFatPath fatPath( trdos, fileExportSize );
					if (const TStdWinError err=trdos->__importData__( &CMemFile(buffer,fileExportSize), fileExportSize, false, fatPath )){
						// relocation failed - reverting all changes
						boot->firstFree=de->first;
						trdos->__importData__( &CMemFile(buffer,fileExportSize), fileExportSize, false, CFatPath(UCHAR_MAX) );
						boot->firstFree=firstFree0;
						return vp.TerminateAndGoToNextAction(err);
					}
					if (!fatPath.MarkAllSectorsModified(image))
						return vp.TerminateAll(ERROR_FUNCTION_FAILED); // we shouldn't end up here but just to be sure
					de->first=boot->firstFree;
					trdos->MarkDirectorySectorAsDirty(de);
					boot->firstFree=deLast->first+deLast->nSectors;
					trdos->boot.MarkSectorAsDirty();
				}
				vp.fReport.CloseProblem(true);
			}
		// - successfully verified
		return pAction->TerminateWithSuccess();
	}
