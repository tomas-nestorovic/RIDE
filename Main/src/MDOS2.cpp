#include "stdafx.h"
#include "MDOS2.h"

	#define INI_MDOS2	_T("MDOS2")

	#define INI_DEFAULT_ATTRIBUTES	_T("defattr")

	#define FAT_LOGSECTOR_FIRST		1

	CMDOS2::CMDOS2(PImage image,PCFormat pFormatBoot)
		// ctor
		// - base
		: CSpectrumDos( image, pFormatBoot, TTrackScheme::BY_CYLINDERS, &Properties, IDR_MDOS, &fileManager, TGetFileSizeOptions::OfficialDataLength, TSectorStatus::UNAVAILABLE )
		// - initialization
		, boot(this) , fileManager(this) , version(AUTODETECT) {
		deDefault.attributes=__getProfileInt__(	INI_DEFAULT_ATTRIBUTES,
												TDirectoryEntry::TAttribute::READABLE | TDirectoryEntry::TAttribute::WRITEABLE | TDirectoryEntry::TAttribute::EXECUTABLE | TDirectoryEntry::TAttribute::DELETABLE
											);
		if (formatBoot.mediumType!=Medium::UNKNOWN)
			__recognizeVersion__(); // recognition of MDOS on inserted disk
		else
			sideMap[1]=TVersion::VERSION_2; // disk not inserted, assuming Version 2
	}










	void CMDOS2::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_MDOS2, messageId );
	}

	void CMDOS2::__recognizeVersion__(){
		// recognizes MDOS Version based on readability of Sectors under Head 1, and adjusts the Side number for Head 1
		// - attempting to get Sectors under Head 1 using Version 1.0
		TSectorId ids[(TSector)-1];
		const TSector nSectors=image->ScanTrack( 0, 1, nullptr, ids );
		for( TSector s=0; s<nSectors; s++ ){
			const TSectorId idRef={ 0, TVersion::VERSION_1, ids[s].sector, MDOS2_SECTOR_LENGTH_STD_CODE };
			if (idRef.sector<MDOS2_TRACK_SECTORS_MAX && ids[s]==idRef){
				sideMap[1]=TVersion::VERSION_1;
				return;
			}
		}
		// - recognized Version 2.0
		sideMap[1]=TVersion::VERSION_2;
	}

	CMDOS2::TLogSector CMDOS2::__fyzlog__(RCPhysicalAddress chs) const{
		// converts PhysicalAddress to LogicalSector number and returns it
		return (chs.cylinder*formatBoot.nHeads+chs.head)*formatBoot.nSectors+chs.sectorId.sector-1; // "-1" = Sectors numbered from 1
	}
	TPhysicalAddress CMDOS2::__logfyz__(TLogSector ls) const{
		// converts LogicalSector number to PhysicalAddress and returns it
		const div_t A=div( ls, formatBoot.nSectors ), B=div( A.quot, formatBoot.nHeads );
		const TPhysicalAddress chs={ B.quot, B.rem, { B.quot, sideMap[B.rem], A.rem+1, MDOS2_SECTOR_LENGTH_STD_CODE } }; // "+1" = Sectors numbered from 1
		return chs;
	}

	PSectorData CMDOS2::__getHealthyLogicalSectorData__(TLogSector logSector) const{
		// returns data of LogicalSector, or Null of such Sector is unreadable or doesn't exist
		return image->GetHealthySectorData( __logfyz__(logSector) );
	}
	void CMDOS2::__markLogicalSectorAsDirty__(TLogSector logSector) const{
		// marks given LogicalSector as dirty
		image->MarkSectorAsDirty( __logfyz__(logSector) );
	}

	#define FAT_ITEMS_IN_SECTOR	341 /* # of items in FAT12's single Sector */

	WORD CMDOS2::__getLogicalSectorFatItem__(TLogSector logSector) const{
		// returns the value in FAT of the specified LogicalSector; returns MDOS2_FAT_ERROR if FAT Sector read error
		div_t d=div( logSector, FAT_ITEMS_IN_SECTOR ); // determining the item ID in FAT Sector
		const TPhysicalAddress chs={ 0, 0, {0,sideMap[0],1+FAT_LOGSECTOR_FIRST+d.quot,MDOS2_SECTOR_LENGTH_STD_CODE} }; // "+1" = Sectors numbered from 1
		if (PCSectorData itemAddress=image->GetHealthySectorData(chs)){
			itemAddress+=( d.rem*=3 )/2;
			if (d.rem&1) // item on odd address in FAT Sector
				return (*itemAddress&0xf)*256 + itemAddress[1];
			else // item on even address in FAT Sector
				return (itemAddress[1]>>4)*256 + *itemAddress;
		}else
			return MDOS2_FAT_ERROR; // FAT i/o error
	}
	bool CMDOS2::__setLogicalSectorFatItem__(TLogSector logSector,WORD value12) const{
		// True <=> LogicalSector item set in FAT to the specified Value, otherwise False; assumed that the Value contains only 12-bit number
		ASSERT((value12&0xf000)==0); // must always be a 12-bit Value only
		div_t d=div( logSector, FAT_ITEMS_IN_SECTOR );	// determining the item ID in FAT Sector
		const TPhysicalAddress chs={ 0, 0, {0,sideMap[0],1+FAT_LOGSECTOR_FIRST+d.quot,MDOS2_SECTOR_LENGTH_STD_CODE} }; // "+1" = Sectors numbered from 1
		if (PSectorData itemAddress=image->GetHealthySectorData(chs)){
			itemAddress+=( d.rem*=3 )/2;
			if (d.rem&1){ // item on odd address in FAT Sector
				*itemAddress=( *itemAddress&0xf0 )|( value12>>8 ); // not using "(value&0xf)>>8" as assumed that the Value is always only 12-bit
				itemAddress[1]=value12;
			}else{ // item on even address in FAT Sector
				itemAddress[1]=( itemAddress[1]&0xf )|( (value12&0xf00)>>4 );
				*itemAddress=value12;
			}
			image->MarkSectorAsDirty(chs);
			return true;
		}else
			return false; // FAT i/o error
	}










	bool CMDOS2::GetSectorStatuses(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PSectorStatus buffer) const{
		// True <=> Statuses of all Sectors in the Track successfully retrieved and populated the Buffer, otherwise False
		bool result=true; // assumption (statuses of all Sectors successfully retrieved)
		for( const TLogSector logSectorBase=(cyl*formatBoot.nHeads+head)*formatBoot.nSectors-1; nSectors--; bufferId++ ){ // "-1" = Sectors numbered from 1
			const TSector id=bufferId->sector;
			if (cyl>=formatBoot.nCylinders || head>=formatBoot.nHeads || bufferId->cylinder!=cyl || bufferId->side!=sideMap[head] || id>formatBoot.nSectors || !id || bufferId->lengthCode!=2) // condition for Sector must be ">", not ">=" (Sectors numbered from 1 - see also "|!id")
				// Sector number out of official Format
				*buffer++=TSectorStatus::UNKNOWN;
			else{
				// getting Sector Status from FAT
				const TLogSector ls=id+logSectorBase;
				switch (__getLogicalSectorFatItem__(ls)){
					case MDOS2_FAT_ERROR			: *buffer++=TSectorStatus::UNAVAILABLE; result=false;	break;
					case MDOS2_FAT_SECTOR_SYSTEM	: *buffer++= ls<MDOS2_DATA_LOGSECTOR_FIRST ? TSectorStatus::SYSTEM : TSectorStatus::UNAVAILABLE; break;
					case MDOS2_FAT_SECTOR_BAD		: *buffer++=TSectorStatus::BAD;		break;
					case MDOS2_FAT_SECTOR_RESERVED	: *buffer++=TSectorStatus::RESERVED;break; // zero-length File, or a File with error during saving
					case MDOS2_FAT_SECTOR_EMPTY		: *buffer++=TSectorStatus::EMPTY;	break;
					default							: *buffer++=TSectorStatus::OCCUPIED;break;
				}
			}
		}
		return result;
	}
	bool CMDOS2::ModifyStdSectorStatus(RCPhysicalAddress chs,TSectorStatus status) const{
		// True <=> the Status of the specified DOS-standard Sector successfully changed, otherwise False
		WORD value;
		switch (status){
			case TSectorStatus::UNAVAILABLE:
				value=MDOS2_FAT_SECTOR_UNAVAILABLE;
				break;
			case TSectorStatus::EMPTY:
				value=MDOS2_FAT_SECTOR_EMPTY;
				break;
			default:
				ASSERT(FALSE);
			case TSectorStatus::BAD:
				value=MDOS2_FAT_SECTOR_BAD;
				break;
		}
		return __setLogicalSectorFatItem__( __fyzlog__(chs), value );
	}







	bool CMDOS2::GetFileFatPath(PCFile file,CFatPath &rFatPath) const{
		// True <=> FatPath of given File (even an erroneous FatPath) successfully retrieved, otherwise False
		// - if queried about the root Directory, populating the FatPath with root Directory Sectors
		CFatPath::TItem item;
		if (file==ZX_DIR_ROOT){
			static const TLogSector DirSectorOrder[]={ 0, 2, 4, 6, 1, 3, 5, 7 }; // Directory Sectors are not traversed linearly but rather "interleaved"
			static_assert( ARRAYSIZE(DirSectorOrder)==MDOS2_DATA_LOGSECTOR_FIRST-MDOS2_DIR_LOGSECTOR_FIRST, "" );
			for each( const TLogSector order in DirSectorOrder ){
				item.chs=__logfyz__(
					item.value = MDOS2_DIR_LOGSECTOR_FIRST+order
				);
				if (!rFatPath.AddItem(&item)) break; // also sets an error in FatPath
			}
			return true;
		}
		// - no FatPath can be retrieved if DirectoryEntry is Empty
		if (*(PBYTE)file==TDirectoryEntry::EMPTY_ENTRY)
			return false;
		// - if File has no Sectors, we are done (may happen due to a failure during importing)
		if (!( item.value=((PDirectoryEntry)file)->firstLogicalSector ))
			return true;
		// - extracting the FatPath from FAT
		const DWORD logSectorMax=formatBoot.GetCountOfAllSectors();
		do{
			// . determining Sector's PhysicalAddress
			item.chs=__logfyz__(item.value);
			// . adding the Item to the FatPath
			if (!rFatPath.AddItem(&item)) break; // also sets an error in FatPath
			// . VALIDATION: Value must "make sense"
			if (!(	(item.value>=MDOS2_DATA_LOGSECTOR_FIRST && item.value<logSectorMax) // value must be within range
					||
					item.value==MDOS2_FAT_SECTOR_RESERVED // zero-length File
					||
					(item.value>=MDOS2_FAT_SECTOR_EOF && item.value<=MDOS2_FAT_SECTOR_EOF+MDOS2_SECTOR_LENGTH_STD-1) // last Sector in non-zero-length File
			)){
				rFatPath.error=CFatPath::TError::VALUE_INVALID;
				break;
			}
			// . VALIDATION: next Item can be retrieved
			if (( item.value=__getLogicalSectorFatItem__(item.value) )==MDOS2_FAT_ERROR){ // if FAT Sector with next Item cannot be read ...
				rFatPath.error=CFatPath::TError::SECTOR; // ... setting corresponding error ...
				break; // ... and quitting
			}
		}while (item.value<MDOS2_FAT_SECTOR_EOF && item.value!=MDOS2_FAT_SECTOR_RESERVED); // A&B; A = end-of-file mark, B = zero-length File
		return true; // FatPath (with or without error) successfully extracted from FAT
	}

	bool CMDOS2::ModifyFileFatPath(PFile file,const CFatPath &rFatPath) const{
		// True <=> a error-free FatPath of given File successfully written, otherwise False
		CFatPath::PCItem pItem; DWORD nItems;
		if (rFatPath.GetItems(pItem,nItems)) // if FatPath erroneous ...
			return false; // ... we are done
		const PDirectoryEntry de=(PDirectoryEntry)file;
		const DWORD fileLength=de->GetLength();
		if (fileLength>0 && nItems>0){
			// non-zero-length File
			TLogSector ls = de->firstLogicalSector =  pItem->chs ? __fyzlog__(pItem->chs) : pItem->value;
			for( WORD h; --nItems; ls=h ) // all Sectors but the last one are Occupied in FatPath
				__setLogicalSectorFatItem__( ls, // no need to test FAT Sector existence (already tested above)
					h = (++pItem)->chs ? __fyzlog__(pItem->chs) : pItem->value
				); 
			__setLogicalSectorFatItem__(ls, // terminating the FatPath in FAT
										fileLength%MDOS2_SECTOR_LENGTH_STD+MDOS2_FAT_SECTOR_EOF
									);
			MarkDirectorySectorAsDirty(de);
			return true;
		}else if (!fileLength && nItems==1){
			// zero-length File
			__setLogicalSectorFatItem__(
				de->firstLogicalSector = pItem->chs ? __fyzlog__(pItem->chs) : pItem->value,
				MDOS2_FAT_SECTOR_RESERVED
			);
			MarkDirectorySectorAsDirty(de);
			return true;
		}else
			// erroneous assignment of a FatPath to a File
			return false;
	}

	UINT AFX_CDECL CMDOS2::FatVerification_thread(PVOID pCancelableAction){
		// thread to verify the readability and correctness of the FAT
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const TSpectrumVerificationParams &vp=*(TSpectrumVerificationParams *)pAction->GetParams();
		vp.fReport.OpenSection(FAT_VERIFICATION_READABILITY);
		// - verifying basic FAT information
		const PMDOS2 mdos=static_cast<PMDOS2>(vp.dos);
		const PImage image=mdos->image;
		const PCBootSector boot=(PBootSector)image->GetHealthySectorData(TBootSector::CHS);
		if (!boot)
			return vp.TerminateAll(ERROR_UNRECOGNIZED_VOLUME);
		pAction->SetProgressTarget(	MDOS2_DIR_LOGSECTOR_FIRST-FAT_LOGSECTOR_FIRST
									+
									1 // checking that all Sectors beyond the official format are marked as Unknown
								);
		int step=0;
		// - Steps 1-N: verifying FAT Sectors readability
		TPhysicalAddress chs={ 0, 0, {0,mdos->sideMap[0],FAT_LOGSECTOR_FIRST,MDOS2_SECTOR_LENGTH_STD_CODE} };
		while (chs.sectorId.sector++<MDOS2_DIR_LOGSECTOR_FIRST){ // "++" = Sectors numbered from 1
			if (!image->GetHealthySectorData(chs))
				vp.fReport.LogWarning( VERIF_MSG_FAT_SECTOR_BAD, (LPCTSTR)chs.sectorId.ToString() );
			pAction->UpdateProgress(++step);
		}
		// - checking that all Sectors beyond the official format are marked as Unavailable
		for( TLogSector ls=mdos->formatBoot.GetCountOfAllSectors(),fixConfirmation=0; ls<1705; ls++ ){ // 1705 = max number of items in FAT12
			const WORD value=mdos->__getLogicalSectorFatItem__(ls);
			if (value!=MDOS2_FAT_ERROR && value!=MDOS2_FAT_SECTOR_UNAVAILABLE){
				if (!fixConfirmation) // not yet asked about what to do
					fixConfirmation=vp.ConfirmFix( _T("Sectors beyond the official format aren't marked as \"unavailable\""), _T("") );
				switch (fixConfirmation){
					case IDCANCEL:
						return vp.CancelAll();
					case IDNO:
						continue;
				}
				if (!mdos->__setLogicalSectorFatItem__( ls, MDOS2_FAT_SECTOR_UNAVAILABLE ))
					return vp.TerminateAll(ERROR_FUNCTION_FAILED); // we shouldn't end up here but just to be sure
				vp.fReport.CloseProblem(true);
			}
		}
		return pAction->TerminateWithSuccess();
	}

	bool CMDOS2::GetFileNameOrExt(PCFile file,PPathString pOutName,PPathString pOutExt) const{
		// populates the Buffers with File's name and extension; caller guarantees that the Buffer sizes are at least MAX_PATH characters each
		if (file==ZX_DIR_ROOT){
			if (pOutName)
				*pOutName=CPathString::Root;
			if (pOutExt)
				*pOutExt=CPathString::Empty;
		}else{
			const PCDirectoryEntry de=(PCDirectoryEntry)file;
			if (pOutName)
				*pOutName=CPathString( de->name, MDOS2_FILE_NAME_LENGTH_MAX ).TrimRightNull(); // trimming trailing null-characters
			if (pOutExt)
				*pOutExt=de->extension;
		}
		return true; // name relevant
	}
	TStdWinError CMDOS2::ChangeFileNameAndExt(PFile file,RCPathString newName,RCPathString newExt,PFile &rRenamedFile){
		// tries to change given File's name and extension; returns Windows standard i/o error
		// - can't change root Directory's name
		if (file==ZX_DIR_ROOT)
			return ERROR_ACCESS_DENIED;
		// - checking that the NewName+NewExt combination follows the "10.1" convention
		if (newExt.GetLengthW()<1)
			return ERROR_BAD_FILE_TYPE;
		if (newName.GetLengthW()>MDOS2_FILE_NAME_LENGTH_MAX || newExt.GetLengthW()>1)
			return ERROR_FILENAME_EXCED_RANGE;
		// - making sure that a File with given NameAndExtension doesn't yet exist
		if ( rRenamedFile=FindFileInCurrentDir(newName,newExt,file) )
			return ERROR_FILE_EXISTS;
		// - renaming
		const PDirectoryEntry de=(PDirectoryEntry)file;
		de->extension=newExt.FirstCharA();
		newName.MemcpyAnsiTo( de->name, '\0' );
		MarkDirectorySectorAsDirty( rRenamedFile=file );
		return ERROR_SUCCESS;
	}
	DWORD CMDOS2::GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData,TGetFileSizeOptions option) const{
		// determines and returns the size of specified File
		if (pnBytesReservedBeforeData) *pnBytesReservedBeforeData=0;
		if (pnBytesReservedAfterData) *pnBytesReservedAfterData=0;
		const PCDirectoryEntry de=(PCDirectoryEntry)file;
		if (de==ZX_DIR_ROOT)
			return (MDOS2_DATA_LOGSECTOR_FIRST-MDOS2_DIR_LOGSECTOR_FIRST)*MDOS2_SECTOR_LENGTH_STD;
		else
			switch (option){
				case TGetFileSizeOptions::OfficialDataLength:
					return de->GetLength();
				case TGetFileSizeOptions::SizeOnDisk:
					return Utils::RoundUpToMuls( de->GetLength(), (DWORD)MDOS2_SECTOR_LENGTH_STD );
				default:
					ASSERT(FALSE);
					return 0;
			}
	}

	TStdWinError CMDOS2::DeleteFile(PFile file){
		// deletes specified File; returns Windows standard i/o error
		if (file==ZX_DIR_ROOT)
			return ERROR_ACCESS_DENIED; // can't delete the root Directory
		if (*(PBYTE)file!=TDirectoryEntry::EMPTY_ENTRY){ // File mustn't be already deleted (may happen during moving it in FileManager)
			const CFatPath fatPath(this,file);
			CFatPath::PCItem item; DWORD n;
			if (const LPCTSTR errMsg=fatPath.GetItems(item,n)){
				ShowFileProcessingError(file,errMsg);
				return ERROR_GEN_FAILURE;
			}else{
				// . deleting from FAT
				while (n--)
					__setLogicalSectorFatItem__( item++->value, MDOS2_FAT_SECTOR_EMPTY );
				// . deleting from root Directory
				*(PBYTE)file=TDirectoryEntry::EMPTY_ENTRY;
				MarkDirectorySectorAsDirty(file);
			}
		}
		return ERROR_SUCCESS;
	}

	#define INFO_ATTRIBUTES	_T("R%x")

	CDos::CPathString CMDOS2::GetFileExportNameAndExt(PCFile file,bool shellCompliant) const{
		// returns File name concatenated with File extension for export of the File to another Windows application (e.g. Explorer)
		CPathString result=__super::GetFileExportNameAndExt(file,shellCompliant);
		if (!shellCompliant){
			// exporting to another RIDE instance
			const PCDirectoryEntry de=(PCDirectoryEntry)file;
			TUniFileType uts;
			switch (de->extension){
				case TDirectoryEntry::PROGRAM		: uts=TUniFileType::PROGRAM; break;
				case TDirectoryEntry::CHAR_ARRAY	: uts=TUniFileType::CHAR_ARRAY; break;
				case TDirectoryEntry::NUMBER_ARRAY	: uts=TUniFileType::NUMBER_ARRAY; break;
				case TDirectoryEntry::BLOCK			: uts=TUniFileType::BLOCK; break;
				case TDirectoryEntry::SNAPSHOT		: uts=TUniFileType::SNAPSHOT_48k; break;
				case TDirectoryEntry::SEQUENTIAL	: uts=TUniFileType::SEQUENTIAL; break;
				default								: uts=TUniFileType::UNKNOWN; break;
			}
			TCHAR buf[80];
			_stprintf(	buf + __exportFileInformation__( buf, uts, de->params, GetFileOfficialSize(de) ),
						INFO_ATTRIBUTES, de->attributes
					);
			result.Append(buf);
		}
		return result;
	}

	TStdWinError CMDOS2::ImportFile(CFile *f,DWORD fileSize,RCPathString nameAndExtension,DWORD winAttr,PFile &rFile){
		// imports specified File (physical or virtual) into the Image; returns Windows standard i/o error
		// - parsing the NameAndExtension into a usable "10.1" form
		CPathString zxName,zxExt;
		const CString zxInfo=ParseFat32LongName( nameAndExtension, zxName, zxExt );
		zxName.TrimToLengthW(MDOS2_FILE_NAME_LENGTH_MAX);
		zxExt.TrimToLengthW(1);
		// - initializing the description of File to import
		TDirectoryEntry tmp; // the description
			::ZeroMemory(&tmp,sizeof(tmp));
			// . import information
			int attr=deDefault.attributes;
			TUniFileType uts; DWORD dw;
			if (const int n=__importFileInformation__(zxInfo,uts,tmp.params,dw)){
				if (dw) fileSize=dw;
				_stscanf( (LPCTSTR)zxInfo+n, INFO_ATTRIBUTES, &attr );
			}
			tmp.attributes=attr;
			// . name
			//nop (ChangeFileNameAndExt called below by ImportFile)
			// . size
			tmp.SetLength(fileSize);
			// . FirstLogicalSector
			//nop (set below)
		// - changing the Extension according to the "universal" type valid across ZX platforms (as TR-DOS File "Picture.C" should be take on the name "Picture.B" under MDOS)
		switch (uts){
			case TUniFileType::PROGRAM		: tmp.extension=TDirectoryEntry::PROGRAM; break;
			case TUniFileType::CHAR_ARRAY	: tmp.extension=TDirectoryEntry::CHAR_ARRAY; break;
			case TUniFileType::NUMBER_ARRAY	: tmp.extension=TDirectoryEntry::NUMBER_ARRAY; break;
			case TUniFileType::BLOCK:
			case TUniFileType::SCREEN		: tmp.extension=TDirectoryEntry::BLOCK; break;
			case TUniFileType::SNAPSHOT_48k	: tmp.extension=TDirectoryEntry::SNAPSHOT; break;
			case TUniFileType::SEQUENTIAL	: tmp.extension=TDirectoryEntry::SEQUENTIAL; break;
			default:
				tmp.extension= zxExt.GetLengthW() ? zxExt.FirstCharA() : TDirectoryEntry::BLOCK;
				break;
		}
		// - importing to Image
		CFatPath fatPath(this,fileSize);
		if (const TStdWinError err=__importFileData__( f, &tmp, zxName, tmp.extension, fileSize, true, rFile, fatPath ))
			return err;
		// - recording the FatPath in FAT
		ModifyFileFatPath( rFile, fatPath );
		// - File successfully imported to Image
		return ERROR_SUCCESS;
	}










	TStdWinError CMDOS2::CreateUserInterface(HWND hTdi){
		// creates DOS-specific Tabs in TDI; returns Windows standard i/o error
		if (const TStdWinError err=__super::CreateUserInterface(hTdi))
			return err;
		CTdiCtrl::AddTabLast( hTdi, BOOT_SECTOR_TAB_LABEL, &boot.tab, false, TDI_TAB_CANCLOSE_NEVER, nullptr );
		CTdiCtrl::AddTabLast( hTdi, FILE_MANAGER_TAB_LABEL, &fileManager.tab, true, TDI_TAB_CANCLOSE_NEVER, nullptr );
		return ERROR_SUCCESS;
	}

	CDos::TCmdResult CMDOS2::ProcessCommand(WORD cmd){
		// returns the Result of processing a DOS-related command
		switch (cmd){
			case ID_ZX_PREVIEWASSCREEN:{
				// previewing File(s) on Spectrum screen
				static constexpr TFilePreviewOffsetByFileType Offsets[]={
					{ TDirectoryEntry::SNAPSHOT, 128, true }
				};
				if (CScreenPreview::pSingleInstance)
					CScreenPreview::pSingleInstance->DestroyWindow();
				CScreenPreview::pOffsetsByFileType=Offsets;
				break; // call base
			}
			case ID_ZX_PREVIEWASBASIC:{
				// previewing File(s) as Basic program listing
				static constexpr TFilePreviewOffsetByFileType Offsets[]={
					{ TDirectoryEntry::SNAPSHOT, 7499, true }
				};
				if (CBasicPreview::pSingleInstance)
					CBasicPreview::pSingleInstance->DestroyWindow();
				CBasicPreview::pOffsetsByFileType=Offsets;
				break; // call base
			}
			case ID_DOS_VERIFY:{
				// volume verification
				static constexpr TVerificationFunctions vf={
					TBootSector::Verification_thread, // Boot Sector
					FatVerification_thread, // FAT readability
					TVerificationFunctions::ReportOnFilesWithBadFatPath_thread, // FAT Files OK
					TVerificationFunctions::FloppyCrossLinkedFilesVerification_thread, // FAT crossed Files
					TVerificationFunctions::FloppyLostSectorsVerification_thread, // FAT lost allocation units
					TDirectoryEntry::Verification_thread, // Filesystem
					TVerificationFunctions::WholeDiskSurfaceVerification_thread // Volume surface
				};
				VerifyVolume(
					CVerifyVolumeDialog( TSpectrumVerificationParams(this,vf) )
				);
				return TCmdResult::DONE_REDRAW;
			}
			case ID_MDOS_AUTODETECT:
				// autodetecting MDOS Version
				version=TVersion::AUTODETECT;
				__recognizeVersion__();
				return TCmdResult::DONE_REDRAW;
			case ID_MDOS_VERSION1:
				// forcing MDOS Version 1.0
				sideMap[1] = version = TVersion::VERSION_1;
				return TCmdResult::DONE_REDRAW;
			case ID_MDOS_VERSION2:
				// forcing MDOS Version 2.0
				sideMap[1] = version = TVersion::VERSION_2;
				return TCmdResult::DONE_REDRAW;
			case ID_MDOS_IMPORTATTRIBUTES_CUSTOM:
				// setting new default attributes
				if (deDefault.__editAttributesViaDialog__())
					__writeProfileInt__( INI_DEFAULT_ATTRIBUTES, deDefault.attributes );
				return TCmdResult::DONE;
			case ID_DOS_TAKEATOUR:
				// navigating to the online tour on this DOS
				app.GetMainWindow()->OpenApplicationPresentationWebPage(_T("Tour"),_T("MDOS2/tour.html"));
				return TCmdResult::DONE;
		}
		return __super::ProcessCommand(cmd);
	}

	bool CMDOS2::UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const{
		// True <=> given Command-specific user interface successfully updated, otherwise False
		switch (cmd){
			case ID_MDOS_AUTODETECT:
				pCmdUI->SetRadio(version==TVersion::AUTODETECT);
				return true;
			case ID_MDOS_VERSION1:
				pCmdUI->SetRadio(version==TVersion::VERSION_1);
				return true;
			case ID_MDOS_VERSION2:
				pCmdUI->SetRadio(version==TVersion::VERSION_2);
				return true;
			case ID_MDOS_IMPORTATTRIBUTES_CUSTOM:
				if (deDefault.attributes!=0){
					TCHAR buf[16];
					buf[0]='(';
						deDefault.__attributes2text__(buf+1,false);
					pCmdUI->SetText( ::lstrcat(buf,_T(")...")) );
				}else
					pCmdUI->SetText(_T("(None)..."));
				return true;
		}
		return __super::UpdateCommandUi(cmd,pCmdUI);
	}











	const char CMDOS2::TDirectoryEntry::KnownExtensions[]={ TDirectoryEntry::PROGRAM, TDirectoryEntry::CHAR_ARRAY, TDirectoryEntry::NUMBER_ARRAY, TDirectoryEntry::BLOCK, TDirectoryEntry::SNAPSHOT, TDirectoryEntry::SEQUENTIAL };

	DWORD CMDOS2::TDirectoryEntry::GetLength() const{
		// determines and returns the File length
		return MAKELONG(lengthLow,lengthHigh);
	}

	void CMDOS2::TDirectoryEntry::SetLength(DWORD fileLength){
		// sets new FileLength
		register union{
			WORD d[2];
			DWORD nBytesToImport;
		};
		nBytesToImport=fileLength;
		lengthLow=d[0], lengthHigh=d[1];
	}

	bool CMDOS2::TDirectoryEntry::__editAttributesViaDialog__(){
		// True <=> modified File Attributes confirmed, otherwise False
		// - defining the Dialog
		static constexpr WORD Controls[]={ ID_HIDDEN, ID_SYSTEM, ID_PROTECTED, ID_ARCHIVE, ID_READABLE, ID_WRITABLE, ID_EXECUTABLE, ID_DELETABLE };
		class CAttributesDialog sealed:public Utils::CRideDialog{
			void DoDataExchange(CDataExchange *pDX) override{
				// exchange of data from and to controls
				if (pDX->m_bSaveAndValidate)
					for( BYTE i=0; i<8; attributes=(attributes<<1)|(BYTE)IsDlgItemChecked(Controls[i++]) );
				else
					for( BYTE i=8; i--; CheckDlgButton(Controls[i],attributes&1),attributes>>=1 );
			}
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				if (msg==WM_PAINT){
					// drawing
					// . base
					__super::WindowProc(msg,wParam,lParam);
					// . drawing curly brackets
					const RECT r1=MapDlgItemClientRect(ID_READABLE), r2=MapDlgItemClientRect(ID_DELETABLE);
					DrawClosingCurlyBracket(
						CClientDC(this),
						r1.right+8,
						r1.top-3, r2.bottom+3
					);
					return 0;
				}else if (msg==WM_NOTIFY && wParam==ID_ATTRIBUTE)
					// notification regarding the "RWED" Attributes
					if (GetClickedHyperlinkId(lParam))
						for( BYTE i=4; i<8; CheckDlgButton(Controls[i++],BST_CHECKED) );
				return __super::WindowProc(msg,wParam,lParam);
			}
		public:
			BYTE attributes;
			CAttributesDialog(BYTE _attributes)
				// ctor
				: Utils::CRideDialog(IDR_MDOS_FILE_ATTRIBUTES_EDITOR)
				, attributes(_attributes) {
			}
		} d( attributes );
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			attributes=d.attributes;
			CDos::GetFocused()->MarkDirectorySectorAsDirty(this);
			return true;
		}else
			return false;
	}

	PTCHAR CMDOS2::TDirectoryEntry::__attributes2text__(PTCHAR buf,bool inclDashes) const{
		// converts Attributes to textual form and returns the result
		PTCHAR t=::lstrcpy(buf,_T("HSPARWED"));
		for( BYTE n=8,a=attributes; n--; a<<=1,t++ )
			if (!(a&128)) *t='-';
		if (!inclDashes){
			TCHAR c,*pTrg=buf,*pSrc=buf;
			do{
				c=*pSrc++;
				if (c!='-') *pTrg++=c;
			} while (c);
		}
		return buf;
	}

	UINT AFX_CDECL CMDOS2::TDirectoryEntry::Verification_thread(PVOID pCancelableAction){
		// thread to verify the Directories
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const TSpectrumVerificationParams &vp=*(TSpectrumVerificationParams *)pAction->GetParams();
		vp.fReport.OpenSection(FILESYSTEM_VERIFICATION);
		// - verifying basic Directory information
		const PMDOS2 mdos=static_cast<PMDOS2>(vp.dos);
		const PImage image=mdos->image;
		const PCBootSector boot=(PBootSector)image->GetHealthySectorData(TBootSector::CHS);
		if (!boot)
			return vp.TerminateAll( Utils::ErrorByOs(ERROR_VOLMGR_DISK_INVALID,ERROR_UNRECOGNIZED_VOLUME) );
		pAction->SetProgressTarget(	MDOS2_DATA_LOGSECTOR_FIRST-MDOS2_DIR_LOGSECTOR_FIRST
									+
									1 // checking that all DirectoryEntries are valid
								);
		int step=0;
		// - Steps 1-N: verifying Directory Sectors readability
		for( TLogSector ls=MDOS2_DIR_LOGSECTOR_FIRST; ls<MDOS2_DATA_LOGSECTOR_FIRST; ls++ ){
			const TPhysicalAddress chs=mdos->__logfyz__(ls);
			if (!image->GetHealthySectorData(chs))
				vp.fReport.LogWarning( VERIF_MSG_DIR_SECTOR_BAD, (LPCTSTR)chs.sectorId.ToString() );
		}
		// - verifying that all DirectoryEntries are valid
		for( TMdos2DirectoryTraversal dt(mdos); dt.AdvanceToNextEntry(); )
			if (dt.entryType==TDirectoryTraversal::FILE){
				const PDirectoryEntry de=(PDirectoryEntry)dt.entry;
				const CString strItemId=Utils::SimpleFormat( _T("File \"%s\""), mdos->GetFilePresentationNameAndExt(de) );
				// . verifying "the" and "by" Extension
				switch (de->extension){
					case TExtension::PROGRAM:
					case TExtension::SNAPSHOT:
						// executables
						// : executable Names are usually typed in by the user and may thus not contain non-printable characters
						if (const TStdWinError err=vp.VerifyAllCharactersPrintable( dt.chs, strItemId, VERIF_FILE_NAME, de->name, sizeof(de->name), '\0' ))
							return vp.TerminateAll(err);
						// : executable Attributes must indicate at least visibility, readability, and executability
						#define ATTR_WHITE	(TDirectoryEntry::TAttribute::READABLE | TDirectoryEntry::TAttribute::EXECUTABLE)
						if ((de->attributes&ATTR_WHITE)!=ATTR_WHITE){
							const CString msg=Utils::SimpleFormat( _T("%s: Readability and/or executability attributes not set"), strItemId );
							switch (vp.ConfirmFix( msg, _T("") )){
								case IDCANCEL:
									return vp.CancelAll();
								case IDYES:
									de->attributes|=ATTR_WHITE;
									mdos->MarkDirectorySectorAsDirty(de);
									vp.fReport.CloseProblem(true);
									break;
							}
						}
						if (de->attributes&TDirectoryEntry::TAttribute::HIDDEN)
							vp.fReport.LogWarning( _T("%s: The program is hidden"), (LPCTSTR)strItemId );
						break;
					case TExtension::CHAR_ARRAY:
					case TExtension::NUMBER_ARRAY:
					case TExtension::BLOCK:
					case TExtension::SEQUENTIAL:
						// non-executables
						// : non-executables may contain non-printable characters in Names
						vp.WarnSomeCharactersNonPrintable( strItemId, VERIF_FILE_NAME, de->name, sizeof(de->name), '\0' );
						// : non-executable's Attributes must indicate at least readability
						if ((de->attributes&TDirectoryEntry::TAttribute::READABLE)==0)
							vp.fReport.LogWarning( _T("%s: Readability attribute not set"), (LPCTSTR)strItemId ); // warning suffices - sometimes Files are not intended to be actually read (e.g. "Zadaj RUN" Files in Ultrasoft titles)
						break;
					default:
						// unknown File type
						vp.fReport.LogWarning( VERIF_MSG_FILE_NONSTANDARD, (LPCTSTR)strItemId );
						vp.WarnSomeCharactersNonPrintable( strItemId, VERIF_FILE_NAME, de->name, sizeof(de->name), '\0' );
						break;
				}
				// . verifying Length
				if (const CFatPath &&fatPath=CFatPath(mdos,de)){
					DWORD lengthFromFat=0;
					if (const DWORD nItems=fatPath.GetNumberOfItems()){
						// a valid File (even a zero-length one) has always at least one Sector affiliated
						if (mdos->GetSectorStatus(fatPath.GetHealthyItem(nItems-1)->chs)!=TSectorStatus::RESERVED){
							// a non-zero-length File
							const WORD lastSectorBytes=mdos->__getLogicalSectorFatItem__(mdos->__fyzlog__(fatPath.GetHealthyItem(nItems-1)->chs))-MDOS2_FAT_SECTOR_EOF;
							lengthFromFat= (nItems-(lastSectorBytes>0))*MDOS2_SECTOR_LENGTH_STD + lastSectorBytes; // e.g., 1024 Bytes should be stored in nItems=2 Sectors (lastSectorBytes = 1024%512 = 0), NOT in three Sectors!
						}
						if (de->GetLength()!=lengthFromFat){
							const CString errMsg=Utils::SimpleFormat( VERIF_MSG_ITEM_BAD_LENGTH, strItemId );
							switch (vp.ConfirmFix( errMsg, VERIF_MSG_FILE_LENGTH_FROM_FAT )){
								case IDCANCEL:
									return vp.CancelAll();
								case IDNO:
									break;
								case IDYES:
									de->SetLength(lengthFromFat);
									mdos->MarkDirectorySectorAsDirty(de);
									vp.fReport.CloseProblem(true);
									break;
							}
						}
					}else{
						// an invalid File has no Sectors affiliated; it's now known that anybody would ever do any tweaks to a Directory, hence this is highly likely an error in filesystem
						const CString errMsg=Utils::SimpleFormat( VERIF_MSG_ITEM_NO_SECTORS, strItemId );
						switch (vp.ConfirmFix( errMsg, VERIF_MSG_FILE_DELETE )){
							case IDCANCEL:
								return vp.CancelAll();
							case IDNO:
								break;
							case IDYES:
								if (const TStdWinError err=mdos->DeleteFile(de)) // an error shouldn't occur but just to be sure
									vp.fReport.LogWarning( _T("%s: Can't delete the file"), (LPCTSTR)strItemId );
								else
									vp.fReport.CloseProblem(true);
								continue; // the File no longer exists
						}
					}
				}else
					vp.fReport.LogWarning( VERIF_MSG_ITEM_FAT_ERROR, (LPCTSTR)strItemId, fatPath.GetErrorDesc() );
				// . verifying the starting Sector
				if (de->firstLogicalSector>=mdos->formatBoot.GetCountOfAllSectors())
					vp.fReport.LogWarning( _T("%s: First sector with %s out of disk"), (LPCTSTR)strItemId, (LPCTSTR)mdos->__logfyz__(de->firstLogicalSector).sectorId.ToString() );
			}
		// - successfully verified
		return pAction->TerminateWithSuccess();
	}










	#define DIR_SECTOR_ENTRIES_COUNT	(MDOS2_SECTOR_LENGTH_STD/sizeof(TDirectoryEntry))	/* number of Entries in one Directory Sector */

	CMDOS2::TMdos2DirectoryTraversal::TMdos2DirectoryTraversal(const CMDOS2 *mdos2)
		// ctor
		: TDirectoryTraversal( ZX_DIR_ROOT, sizeof(TDirectoryEntry) )
		, rootDirSectors( mdos2, ZX_DIR_ROOT )
		, mdos2(mdos2) {
		__reinitToFirstEntry__();
	}
	void CMDOS2::TMdos2DirectoryTraversal::__reinitToFirstEntry__(){
		// (re)initializes to the beginning of the Directory
		entryType=TDirectoryTraversal::UNKNOWN;
		iDirSector=0, nRemainingEntriesInSector=0;
	}

	bool CMDOS2::TMdos2DirectoryTraversal::AdvanceToNextEntry(){
		// True <=> another Entry in current Directory exists (Empty or not), otherwise False
		// - getting the next LogicalSector with Directory
		if (!nRemainingEntriesInSector){
			if (iDirSector==rootDirSectors.GetNumberOfItems()){ // end of Directory
				entryType=TDirectoryTraversal::END;
				return false;
			}
			entry=mdos2->image->GetHealthySectorData(
				chs = rootDirSectors.GetItem(iDirSector++)->chs
			);
			if (!entry) // LogicalSector not found
				entryType=TDirectoryTraversal::WARNING, warning=ERROR_SECTOR_NOT_FOUND;
			else
				entryType=TDirectoryTraversal::UNKNOWN, entry=(PDirectoryEntry)entry-1; // pointer set "before" the first DirectoryEntry
			nRemainingEntriesInSector=DIR_SECTOR_ENTRIES_COUNT;
		}
		// - getting the next DirectoryEntry
		if (entryType!=TDirectoryTraversal::WARNING)
			entryType=	((PDirectoryEntry)( entry=(PDirectoryEntry)entry+1 ))->extension!=TDirectoryEntry::EMPTY_ENTRY
						? TDirectoryTraversal::FILE
						: TDirectoryTraversal::EMPTY;
		nRemainingEntriesInSector--;
		return true;
	}

	std::unique_ptr<CDos::TDirectoryTraversal> CMDOS2::BeginDirectoryTraversal(PCFile directory) const{
		// initiates exploration of specified Directory through a DOS-specific DirectoryTraversal
		ASSERT(directory==ZX_DIR_ROOT);
		return std::unique_ptr<TDirectoryTraversal>( new TMdos2DirectoryTraversal(this) );
	}

	void CMDOS2::TMdos2DirectoryTraversal::ResetCurrentEntry(BYTE directoryFillerByte){
		// gets current entry to the state in which it would be just after formatting
		if (entryType==TDirectoryTraversal::FILE || entryType==TDirectoryTraversal::EMPTY){
			*(PBYTE)::memset( entry, directoryFillerByte, entrySize )=TDirectoryEntry::EMPTY_ENTRY;
			entryType=TDirectoryTraversal::EMPTY;
		}
	}


/*
https://groups.google.com/forum/#!topic/microsoft.public.vb.winapi/4AX6WY-_UbY

    tabs:integer;
begin
     hListBox:=CreateWindow('listbox',nil,WS_CHILD or WS_VISIBLE or LBS_USETABSTOPS,10,10,150,90,Handle,0,hInstance,nil);
     tabs:=40;
     SendMessage(hListBox, LB_SETTABSTOPS, 1, cardinal(@tabs) );
*/
