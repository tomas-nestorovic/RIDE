#include "stdafx.h"
#include "MDOS2.h"

	#define INI_MDOS2	_T("MDOS2")

	#define INI_DEFAULT_ATTRIBUTES	_T("defattr")

	#define FAT_LOGSECTOR_FIRST		1

	CMDOS2::CMDOS2(PImage image,PCFormat pFormatBoot)
		// ctor
		// - base
		: CSpectrumDos( image, pFormatBoot, TTrackScheme::BY_CYLINDERS, &Properties, IDR_MDOS, &fileManager, TGetFileSizeOptions::OfficialDataLength )
		// - initialization
		, boot(this) , fileManager(this) , version(AUTODETECT) {
		deDefault.attributes=__getProfileInt__(INI_DEFAULT_ATTRIBUTES,0);
		__recognizeVersion__(); // recognition of MDOS on inserted disk
	}










	void CMDOS2::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_MDOS2, messageId );
	}

	void CMDOS2::__recognizeVersion__(){
		// recognizes MDOS Version based on readability of Sectors under Head 1, and adjusts the Side number for Head 1
		// - attempting to get Sectors under Head 1 using Version 1.0
		TPhysicalAddress chs={	0, 1,
								{ 0, TVersion::VERSION_1, formatBoot.nSectors, MDOS2_SECTOR_LENGTH_STD_CODE }
							};
		for( ; chs.sectorId.sector; chs.sectorId.sector-- )
			if (image->GetSectorData(chs)!=NULL){ // at least one Sector is readable = Version 1.0 successfully recognized
				sideMap[1]=TVersion::VERSION_1;
				return;
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

	PSectorData CMDOS2::__getLogicalSectorData__(TLogSector logSector) const{
		// returns data of LogicalSector, or Null of such Sector is unreadable or doesn't exist
		return image->GetSectorData( __logfyz__(logSector) );
	}
	void CMDOS2::__markLogicalSectorAsDirty__(TLogSector logSector) const{
		// marks given LogicalSector as dirty
		image->MarkSectorAsDirty( __logfyz__(logSector) );
	}

	#define FAT_ITEMS_IN_SECTOR	341 /* # of items in FAT12's single Sector */

	WORD CMDOS2::__getLogicalSectorFatItem__(TLogSector logSector) const{
		// returns the value in FAT of the specified LogicalSector; returns MDOS2_FAT_ERROR if FAT Sector read error
		div_t d=div( logSector, FAT_ITEMS_IN_SECTOR ); // determining the item ID in FAT Sector
		if (PCSectorData itemAddress=__getLogicalSectorData__(FAT_LOGSECTOR_FIRST+d.quot)){
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
		const TLogSector lsFat=FAT_LOGSECTOR_FIRST+d.quot;
		if (PSectorData itemAddress=__getLogicalSectorData__(lsFat)){
			itemAddress+=( d.rem*=3 )/2;
			if (d.rem&1){ // item on odd address in FAT Sector
				*itemAddress=( *itemAddress&0xf0 )|( value12>>8 ); // not using "(value&0xf)>>8" as assumed that the Value is always only 12-bit
				itemAddress[1]=value12;
			}else{ // item on even address in FAT Sector
				itemAddress[1]=( itemAddress[1]&0xf )|( (value12&0xf00)>>4 );
				*itemAddress=value12;
			}
			__markLogicalSectorAsDirty__(lsFat);
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
	bool CMDOS2::ModifyTrackInFat(TCylinder cyl,THead head,PSectorStatus statuses){
		// True <=> Statuses of all Sectors in Track successfully changed, otherwise False; caller guarantees that the number of Statuses corresponds with the number of standard "official" Sectors in the Boot
		bool result=true; // assumption (Statuses of all Sectors successfully modified)
		TSector nSectors=formatBoot.nSectors,s=0;
		for( const TLogSector logSectorBase=(cyl*formatBoot.nHeads+head)*nSectors; nSectors--; ){
			WORD value;
			switch (*statuses++){
				case TSectorStatus::UNAVAILABLE	: value=MDOS2_FAT_SECTOR_UNAVAILABLE; break;
				case TSectorStatus::BAD			: value=MDOS2_FAT_SECTOR_BAD; break;
				case TSectorStatus::EMPTY		: value=MDOS2_FAT_SECTOR_EMPTY; break;
				case TSectorStatus::RESERVED	: value=MDOS2_FAT_SECTOR_RESERVED; break;
				default:
					ASSERT(FALSE);
			}
			result&=__setLogicalSectorFatItem__( logSectorBase+(s++), value );
		}
		return result;
	}







	bool CMDOS2::GetFileFatPath(PCFile file,CFatPath &rFatPath) const{
		// True <=> FatPath of given File (even an erroneous FatPath) successfully retrieved, otherwise False
		// - if queried about the root Directory, populating the FatPath with root Directory Sectors
		CFatPath::TItem item;
		if (file==ZX_DIR_ROOT){
			for( item.value=MDOS2_DIR_LOGSECTOR_FIRST; item.value<MDOS2_DATA_LOGSECTOR_FIRST; item.value++ ){
				item.chs=__logfyz__(item.value);
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
		const TLogSector logSectorMax=formatBoot.GetCountOfAllSectors();
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

	void CMDOS2::GetFileNameAndExt(PCFile file,PTCHAR bufName,PTCHAR bufExt) const{
		// populates the Buffers with File's name and extension; caller guarantees that the Buffer sizes are at least MAX_PATH characters each
		const PCDirectoryEntry de=(PCDirectoryEntry)file;
		if (bufName)
			if (de==ZX_DIR_ROOT)
				::lstrcpy( bufName, _T("\\") );
			else{
				#ifdef UNICODE
					::MultiByteToWideChar( CP_ACP, 0, de->name,MDOS2_FILE_NAME_LENGTH_MAX+1, buf,MDOS2_FILE_NAME_LENGTH_MAX+1 );
					ASSERT(FALSE)
				#else
					::lstrcpynA( bufName, de->name, MDOS2_FILE_NAME_LENGTH_MAX+1 );
				#endif
			}
		if (bufExt){
			if (de!=ZX_DIR_ROOT)
				*bufExt++=de->extension;
			*bufExt='\0';
		}
	}
	TStdWinError CMDOS2::ChangeFileNameAndExt(PFile file,LPCTSTR newName,LPCTSTR newExt,PFile &rRenamedFile){
		// tries to change given File's name and extension; returns Windows standard i/o error
		ASSERT(newName!=NULL && newExt!=NULL);
		// - can't change root Directory's name
		if (file==ZX_DIR_ROOT)
			return ERROR_DIRECTORY;
		// - checking that the NewName+NewExt combination follows the "10.1" convention
		if (::lstrlen(newName)>MDOS2_FILE_NAME_LENGTH_MAX || ::lstrlen(newExt)>1)
			return ERROR_FILENAME_EXCED_RANGE;
		// - making sure that a File with given NameAndExtension doesn't yet exist
		if ( rRenamedFile=__findFile__(newName,newExt,file) )
			return ERROR_FILE_EXISTS;
		// - renaming
		const PDirectoryEntry de=(PDirectoryEntry)file;
		de->extension=*newExt;
		#ifdef UNICODE
			ASSERT(FALSE)
		#else
			::memcpy(	::memset(de->name,0,MDOS2_FILE_NAME_LENGTH_MAX),
						newName, ::lstrlen(newName)
					);
		#endif
		__markDirectorySectorAsDirty__( rRenamedFile=file );
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
					return MAKELONG( de->lengthLow, de->lengthHigh );
				case TGetFileSizeOptions::SizeOnDisk:
					return (MAKELONG(de->lengthLow,de->lengthHigh)+MDOS2_SECTOR_LENGTH_STD-1)/MDOS2_SECTOR_LENGTH_STD * MDOS2_SECTOR_LENGTH_STD;
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
				__showFileProcessingError__(file,errMsg);
				return ERROR_GEN_FAILURE;
			}else{
				// . deleting from FAT
				while (n--)
					__setLogicalSectorFatItem__( item++->value, MDOS2_FAT_SECTOR_EMPTY );
				// . deleting from root Directory
				*(PBYTE)file=TDirectoryEntry::EMPTY_ENTRY;
				__markDirectorySectorAsDirty__(file);
			}
		}
		return ERROR_SUCCESS;
	}

	#define INFO_ATTRIBUTES	_T("T%x")

	PTCHAR CMDOS2::GetFileExportNameAndExt(PCFile file,bool shellCompliant,PTCHAR buf) const{
		// populates Buffer with specified File's export name and extension and returns the Buffer; returns Null if File cannot be exported (e.g. a "dotdot" entry in MS-DOS); caller guarantees that the Buffer is at least MAX_PATH characters big
		const PDirectoryEntry de=(PDirectoryEntry)file;
		__super::GetFileExportNameAndExt(de,shellCompliant,buf);
		if (!shellCompliant){
			// exporting to another RIDE instance
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
			const PTCHAR p=buf+::lstrlen(buf);
			_stprintf(	p + __exportFileInformation__( p, uts, de->params, GetFileOfficialSize(de) ),
						INFO_ATTRIBUTES, de->attributes
					);
		}
		return buf;
	}

	TStdWinError CMDOS2::ImportFile(CFile *f,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile){
		// imports specified File (physical or virtual) into the Image; returns Windows standard i/o error
		// - parsing the NameAndExtension into a usable "10.1" form
		LPCTSTR zxName,zxExt,zxInfo;
		TCHAR buf[MAX_PATH];
		__parseFat32LongName__(	::lstrcpy(buf,nameAndExtension),
								zxName, MDOS2_FILE_NAME_LENGTH_MAX,
								zxExt, 1,
								zxInfo
							);
		// - initializing the description of File to import
		TDirectoryEntry tmp; // the description
			::ZeroMemory(&tmp,sizeof(tmp));
			// . importing information
			int attr=deDefault.attributes;
			tmp.params=UStdParameters(); TUniFileType uts; DWORD dw; // initialization
			if (const int n=__importFileInformation__(zxInfo,uts,tmp.params,dw)){
				if (dw) fileSize=dw;
				_stscanf( zxInfo+n, INFO_ATTRIBUTES, &attr );
			}
			tmp.attributes=attr;
			// . name
			//nop (ChangeFileNameAndExt called below by ImportFile)
			// . size
			union{
				WORD d[2];
				DWORD nBytesToImport;
			};
			nBytesToImport=fileSize;
			tmp.lengthLow=d[0], tmp.lengthHigh=d[1];
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
			default							: tmp.extension=*zxExt; break;
		}
		// - importing to Image
		CFatPath fatPath(this,fileSize);
		const TCHAR uftExt[2]={ tmp.extension, '\0' };
		const TStdWinError err=__importFileData__( f, &tmp, zxName, uftExt, fileSize, rFile, fatPath );
		if (err!=ERROR_SUCCESS)
			return err;
		// - finishing initialization of DirectoryEntry of successfully imported File
		const PDirectoryEntry de=(PDirectoryEntry)rFile;
			// . FirstLogicalSector
			CFatPath::PCItem item; DWORD n;
			fatPath.GetItems(item,n);
			TLogSector ls = de->firstLogicalSector = __fyzlog__(item->chs);
		// - recording the FatPath in FAT
		for( WORD h; --n; ls=h ) // all Sectors but the last one are Occupied in FatPath
			__setLogicalSectorFatItem__( ls, h=__fyzlog__((++item)->chs) ); // no need to test FAT Sector existence (already tested above)
		__setLogicalSectorFatItem__(ls, // terminating the FatPath in FAT
									item->value==TSectorStatus::RESERVED
										? MDOS2_FAT_SECTOR_RESERVED
										: item->value+MDOS2_FAT_SECTOR_EOF
								);
		// - File successfully imported to Image
		return ERROR_SUCCESS;
	}










	TStdWinError CMDOS2::CreateUserInterface(HWND hTdi){
		// creates DOS-specific Tabs in TDI; returns Windows standard i/o error
		CSpectrumDos::CreateUserInterface(hTdi); // guaranteed to always return ERROR_SUCCESS
		CTdiCtrl::AddTabLast( hTdi, TRACK_MAP_TAB_LABEL, &trackMap.tab, false, TDI_TAB_CANCLOSE_NEVER, NULL );
		CTdiCtrl::AddTabLast( hTdi, BOOT_SECTOR_TAB_LABEL, &boot.tab, false, TDI_TAB_CANCLOSE_NEVER, NULL );
		CTdiCtrl::AddTabLast( hTdi, FILE_MANAGER_TAB_LABEL, &fileManager.tab, true, TDI_TAB_CANCLOSE_NEVER, NULL );
		return ERROR_SUCCESS;
	}

	CDos::TCmdResult CMDOS2::ProcessCommand(WORD cmd){
		// returns the Result of processing a DOS-related command
		switch (cmd){
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
				((CMainWindow *)app.m_pMainWnd)->OpenApplicationPresentationWebPage(_T("Tour"),_T("MDOS2/tour.html"));
				return TCmdResult::DONE;
		}
		return CSpectrumDos::ProcessCommand(cmd);
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
					pCmdUI->SetText("(None)...");
				return true;
		}
		return CSpectrumDos::UpdateCommandUi(cmd,pCmdUI);
	}











	bool CMDOS2::TDirectoryEntry::__editAttributesViaDialog__(){
		// True <=> modified File Attributes confirmed, otherwise False
		// - defining the Dialog
		static const WORD Controls[]={ ID_HIDDEN, ID_SYSTEM, ID_PROTECTED, ID_ARCHIVE, ID_READABLE, ID_WRITABLE, ID_EXECUTABLE, ID_DELETABLE };
		class CAttributesDialog sealed:public CDialog{
			void DoDataExchange(CDataExchange *pDX) override{
				// exchange of data from and to controls
				if (pDX->m_bSaveAndValidate)
					for( BYTE i=0; i<8; attributes=(attributes<<1)|(BYTE)Button_GetCheck(::GetDlgItem(m_hWnd,Controls[i++])) );
				else
					for( BYTE i=8; i--; CheckDlgButton(Controls[i],attributes&1),attributes>>=1 );
			}
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				if (msg==WM_PAINT){
					// drawing
					// . base
					CDialog::WindowProc(msg,wParam,lParam);
					// . drawing curly brackets
					const CWnd *const p1=GetDlgItem(ID_READABLE), *p2=GetDlgItem(ID_DELETABLE);
					RECT r1,r2;
					p1->GetClientRect(&r1), p1->MapWindowPoints(this,&r1);
					p2->GetClientRect(&r2), p2->MapWindowPoints(this,&r2);
					Utils::DrawClosingCurlyBracket(CClientDC(this),
													r1.right+8,
													r1.top-3, r2.bottom+3
												);
					return 0;
				}else if (msg==WM_NOTIFY && wParam==ID_ATTRIBUTE)
					// notification regarding the "RWED" Attributes
					if (((LPNMHDR)lParam)->code==NM_CLICK)
						for( BYTE i=4; i<8; CheckDlgButton(Controls[i++],BST_CHECKED) );
				return CDialog::WindowProc(msg,wParam,lParam);
			}
		public:
			BYTE attributes;
			CAttributesDialog(BYTE _attributes)
				// ctor
				: CDialog(IDR_MDOS_FILE_ATTRIBUTES_EDITOR)
				, attributes(_attributes) {
			}
		} d( attributes );
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			attributes=d.attributes;
			((PMDOS2)CImage::__getActive__()->dos)->__markDirectorySectorAsDirty__(this);
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











	#define DIR_SECTOR_ENTRIES_COUNT	(MDOS2_SECTOR_LENGTH_STD/sizeof(TDirectoryEntry))	/* number of Entries in one Directory Sector */

	CMDOS2::TMdos2DirectoryTraversal::TMdos2DirectoryTraversal(const CMDOS2 *_mdos2)
		// ctor
		: TDirectoryTraversal(sizeof(TDirectoryEntry),MDOS2_FILE_NAME_LENGTH_MAX)
		, mdos2(_mdos2) {
		__reinitToFirstEntry__();
	}
	void CMDOS2::TMdos2DirectoryTraversal::__reinitToFirstEntry__(){
		// (re)initializes to the beginning of the Directory
		dirSector=MDOS2_DIR_LOGSECTOR_FIRST, nRemainingEntriesInSector=0;
	}
	bool CMDOS2::TMdos2DirectoryTraversal::__existsNextEntry__(){
		// True <=> another Entry in current Directory exists (Empty or not), otherwise False
		// - getting the next LogicalSector with Directory
		if (!nRemainingEntriesInSector){
			if (dirSector==MDOS2_DATA_LOGSECTOR_FIRST) return false; // end of Directory
			chs=mdos2->__logfyz__(dirSector);
			entry=mdos2->__getLogicalSectorData__(dirSector++);
			if (!entry){ // LogicalSector not found
				entryType=TDirectoryTraversal::WARNING, warning=ERROR_SECTOR_NOT_FOUND;
				return true;
			}else
				entry=(PDirectoryEntry)entry-1; // pointer set "before" the first DirectoryEntry
			nRemainingEntriesInSector=DIR_SECTOR_ENTRIES_COUNT;
		}
		// - getting the next DirectoryEntry
		entryType=	((PDirectoryEntry)( entry=(PDirectoryEntry)entry+1 ))->extension!=TDirectoryEntry::EMPTY_ENTRY
					? TDirectoryTraversal::FILE
					: TDirectoryTraversal::EMPTY;
		nRemainingEntriesInSector--;
		return true;
	}

	CDos::PDirectoryTraversal CMDOS2::BeginDirectoryTraversal() const{
		// initiates exploration of current Directory through a DOS-specific DirectoryTraversal
		return new TMdos2DirectoryTraversal(this);
	}
	bool CMDOS2::TMdos2DirectoryTraversal::AdvanceToNextEntry(){
		// True <=> found another entry in current Directory (Empty or not), otherwise False
		return __existsNextEntry__();
	}

	void CMDOS2::TMdos2DirectoryTraversal::ResetCurrentEntry(BYTE directoryFillerByte) const{
		// gets current entry to the state in which it would be just after formatting
		*(PBYTE)::memset( entry, directoryFillerByte, entrySize )=TDirectoryEntry::EMPTY_ENTRY;
	}


/*
https://groups.google.com/forum/#!topic/microsoft.public.vb.winapi/4AX6WY-_UbY

    tabs:integer;
begin
     hListBox:=CreateWindow('listbox',nil,WS_CHILD or WS_VISIBLE or LBS_USETABSTOPS,10,10,150,90,Handle,0,hInstance,nil);
     tabs:=40;
     SendMessage(hListBox, LB_SETTABSTOPS, 1, cardinal(@tabs) );
*/
