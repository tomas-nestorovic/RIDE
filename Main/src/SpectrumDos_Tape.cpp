#include "stdafx.h"

	#define INI_MSG	_T("tapeinit")

	#define FILE_LENGTH_MAX	0xffff

	static const TFormat TapeFormat={
		TMedium::FLOPPY_DD, // no need to create a new Medium Type for a Tape
		1, 1, 1, TFormat::LENGTHCODE_128,FILE_LENGTH_MAX, 1 // to correctly compute free space in GetSectorStatuses a GetFileFatPath
	};

	CSpectrumDos::CTape::CTape(LPCTSTR fileName,const CSpectrumDos *diskDos)
		// ctor
		// - base
		: CDos( this, &TapeFormat, TTrackScheme::BY_CYLINDERS, diskDos->properties, ::lstrcmp, StdSidesMap, 0, &fileManager ) // StdSidesMap = "some" Sides
		, CImageRaw(&CImageRaw::Properties,false) // "some" Image
		// - initialization
		, fileManager(this,diskDos->zxRom,fileName) {
		dos=this; // linking the DOS and Image
		(HACCEL)menu.hAccel=diskDos->menu.hAccel; // for DiskDos accelerators to work even if switched to Tape
		SetPathName(fileName,FALSE);
	}

	CSpectrumDos::CTape::~CTape(){
		// dtor
		if (CScreenPreview::pSingleInstance && CScreenPreview::pSingleInstance->rFileManager.tab.dos==this)
			CScreenPreview::pSingleInstance->DestroyWindow();
		if (CBasicPreview::pSingleInstance && &CBasicPreview::pSingleInstance->rFileManager==pFileManager)
			CBasicPreview::pSingleInstance->DestroyWindow();
		dos=NULL; // to not destroy the Image (as DOS and Image are one structure in memory that is disposed at once)
		(HACCEL)menu.hAccel=0; // for DiskDos accelerators to be not destroyed
	}









	PSectorData CSpectrumDos::CTape::GetSectorData(RCPhysicalAddress chs,BYTE nSectorsToSkip,bool recoverFromError,PWORD sectorLength,TFdcStatus *pFdcStatus){
		// returns Data of a Sector on a given PhysicalAddress; returns Null if Sector not found or Track not formatted
		*sectorLength=formatBoot.sectorLength;
		*pFdcStatus=TFdcStatus::WithoutError;
		return chs.cylinder<fileManager.nFiles ? &fileManager.files[chs.cylinder]->data : NULL;
	}









	void CSpectrumDos::CTape::FlushToBootSector() const{
		// flushes internal Format information to the actual Boot Sector's data
		//nop (not applicable for Tape)
	}





	bool CSpectrumDos::CTape::GetSectorStatuses(TCylinder,THead,TSector,PCSectorId,PSectorStatus) const{
		// True <=> Statuses of all Sectors in the Track successfully retrieved and populated the Buffer, otherwise False
		return true; // see FormatBoot initialization in ctor
	}
	bool CSpectrumDos::CTape::ModifyTrackInFat(TCylinder,THead,PSectorStatus){
		// True <=> Statuses of all Sectors in Track successfully changed, otherwise False; caller guarantees that the number of Statuses corresponds with the number of standard "official" Sectors in the Boot
		//nop (not applicable for Tape)
		return m_bModified=TRUE;
	}
	bool CSpectrumDos::CTape::GetFileFatPath(PCFile file,CFatPath &rFatPath) const{
		// True <=> FatPath of given File (even an erroneous FatPath) successfully retrieved, otherwise False
		if ((TTapeFileId)file<fileManager.nFiles){
			CFatPath::TItem item;
				item.chs.cylinder=(TTapeFileId)file;
			rFatPath.AddItem(&item);
			return true;
		}else
			return false;
	}
	DWORD CSpectrumDos::CTape::GetFreeSpaceInBytes(TStdWinError &rError) const{
		// computes and returns the empty space on disk
		rError=ERROR_SUCCESS;
		return ( ZX_TAPE_FILE_COUNT_MAX - fileManager.nFiles )*FILE_LENGTH_MAX;
	}





	enum TStd:BYTE{
		HEADER	=0,
		DATA	=255
	};

	#define HEADERLESS_EXTENSION	'H'
	#define HEADERLESS_TYPE			_T("Headerless")
	#define HEADERLESS_N_A			_T("N/A")

	#define FRAGMENT_TYPE			_T("Fragment")

	#define EXTENSION_PROGRAM	TUniFileType::PROGRAM
	#define EXTENSION_NUMBERS	TUniFileType::NUMBER_ARRAY
	#define EXTENSION_CHARS		TUniFileType::CHAR_ARRAY
	#define EXTENSION_BYTES		TUniFileType::BLOCK

	const TCHAR CSpectrumDos::CTape::Extensions[ZX_TAPE_EXTENSION_STD_COUNT]={
		EXTENSION_PROGRAM,
		EXTENSION_NUMBERS,
		EXTENSION_CHARS,
		EXTENSION_BYTES
	};

	void CSpectrumDos::CTape::GetFileNameAndExt(PCFile file,PTCHAR bufName,PTCHAR bufExt) const{
		// populates the Buffers with File's name and extension; caller guarantees that the Buffer sizes are at least MAX_PATH characters each
		const PCTapeFile tf=fileManager.files[(TTapeFileId)file];
		if (const PCHeader h=tf->GetHeader()){
			// File with a Header
			if (bufName){
				BYTE nameLength=ZX_TAPE_FILE_NAME_LENGTH_MAX;
				while (nameLength-- && h->name[nameLength]==' ');
				::lstrcpyn( bufName, h->name, (BYTE)(2+nameLength) );
			}
			if (bufExt)
				*bufExt++=Extensions[h->type], *bufExt='\0';
		}else{
			// Headerless File or Fragment
			static DWORD idHeaderless=1;
			if (bufName)
				::wsprintf( bufName, _T("%08d"), idHeaderless++ ); // ID padded with zeros to eight digits (to make up an acceptable name even for TR-DOS)
			if (bufExt)
				*bufExt++=HEADERLESS_EXTENSION, *bufExt='\0';
		}
	}

	TStdWinError CSpectrumDos::CTape::ChangeFileNameAndExt(PFile file,LPCTSTR newName,LPCTSTR newExt,PFile &rRenamedFile){
		// tries to change given File's name and extension; returns Windows standard i/o error
		if (const PHeader h=fileManager.files[ (TTapeFileId)(rRenamedFile=file) ]->GetHeader()){
			// File with a Header
			// - checking that the NewName+NewExt combination follows the "10.1" convention
			if (::lstrlen(newName)>ZX_TAPE_FILE_NAME_LENGTH_MAX || ::lstrlen(newExt)>1)
				return ERROR_FILENAME_EXCED_RANGE;
			// . making sure that a File with given NameAndExtension doesn't yet exist 
			//nop (Files on tape may he equal names)
			// . renaming
			switch (*newExt){
				case EXTENSION_PROGRAM	: h->type=TZxRom::TFileType::PROGRAM;	break;
				case EXTENSION_NUMBERS	: h->type=TZxRom::TFileType::NUMBER_ARRAY;break;
				case EXTENSION_CHARS	: h->type=TZxRom::TFileType::CHAR_ARRAY;break;
				case EXTENSION_BYTES	: h->type=TZxRom::TFileType::CODE;		break;
				default:
					return ERROR_BAD_FILE_TYPE;
			}
			#ifdef UNICODE
				ASSERT(FALSE)
			#else
				::memcpy(	::memset(h->name,' ',ZX_TAPE_FILE_NAME_LENGTH_MAX),
							newName, ::lstrlen(newName)
						);
			#endif
			m_bModified=TRUE;
			return ERROR_SUCCESS;
		}else
			// Headerless File or Fragment
			return ERROR_BAD_FILE_TYPE;
	}
	DWORD CSpectrumDos::CTape::GetFileDataSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData) const{
		// determines and returns the size of specified File's data portion
		if (pnBytesReservedBeforeData) *pnBytesReservedBeforeData=0;
		if (pnBytesReservedAfterData) *pnBytesReservedAfterData=0;
		const PCTapeFile tf=fileManager.files[(TTapeFileId)file];
		if (const PCHeader h=tf->GetHeader())
			// File with a Header
			return h->length;
		else
			// Headerless File or Fragment
			return tf->dataLength;
	}

	DWORD CSpectrumDos::CTape::GetAttributes(PCFile file) const{
		// maps File's attributes to Windows attributes and returns the result
		return 0; // none but standard attributes
	}

	TStdWinError CSpectrumDos::CTape::DeleteFile(PFile file){
		// deletes specified File; returns Windows standard i/o error
		PPTapeFile a=fileManager.files+(TTapeFileId)file, b=a;
		::free(*b++), fileManager.nFiles--;
		for( TTapeFileId n=fileManager.nFiles-(TTapeFileId)file; n--; *a++=*b++ );
		m_bModified=TRUE;
		return ERROR_SUCCESS;
	}

	#define EXPORT_INFO_TAPE2	_T("G%uS%x")

	PTCHAR CSpectrumDos::CTape::GetFileExportNameAndExt(PCFile file,bool shellCompliant,PTCHAR buf) const{
		// populates Buffer with specified File's export name and extension and returns the Buffer; returns Null if File cannot be exported (e.g. a "dotdot" entry in MS-DOS); caller guarantees that the Buffer is at least MAX_PATH characters big
		const PTCHAR p=buf+::lstrlen( __super::GetFileExportNameAndExt(file,shellCompliant,buf) );
		const PCTapeFile tf=fileManager.files[(TTapeFileId)file];
		if (const PCHeader h=tf->GetHeader())
			// File with a Header
			::wsprintf( p+__exportFileInformation__(p,(TUniFileType)Extensions[h->type],h->params,tf->dataLength), EXPORT_INFO_TAPE2, tf->dataBlockFlag, tf->dataChecksum );
		else if (!tf->type==TTapeFile::HEADERLESS)
			// Headerless File
			::wsprintf( p+__exportFileInformation__(p,TUniFileType::HEADERLESS,UStdParameters(),tf->dataLength), EXPORT_INFO_TAPE2, tf->dataBlockFlag, tf->dataChecksum );
		else
			// Fragment
			__exportFileInformation__(p,TUniFileType::FRAGMENT,UStdParameters(),tf->dataLength);
		return buf;
	}

	#define NUMBER_OF_BYTES_TO_ALLOCATE_FILE(dataLength)\
		(sizeof(TTapeFile)+dataLength)

	TStdWinError CSpectrumDos::CTape::ImportFile(CFile *f,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile){
		// imports specified File (physical or virtual) into the Image; returns Windows standard i/o error
		// - checking if there's an empty slot in Tape's "Directory"
		if (fileManager.nFiles==ZX_TAPE_FILE_COUNT_MAX)
			return ERROR_CANNOT_MAKE;
		// - checking if File length is within range
		if (fileSize>FILE_LENGTH_MAX)
			return ERROR_BAD_LENGTH;
		// - converting the NameAndExtension to the "10.1" form usable for Tape
		LPCTSTR zxName,zxExt,zxInfo;
		TCHAR buf[MAX_PATH];
		__parseFat32LongName__(	::lstrcpy(buf,nameAndExtension),
								zxName, ZX_TAPE_FILE_NAME_LENGTH_MAX,
								zxExt, 1,
								zxInfo
							);
		// - processing import information
		TCHAR uftExt[]={ *zxExt, '\0' };
		UStdParameters u;	TUniFileType uts;
		DWORD blockFlag=TStd::DATA; // assumption (block featuring Header has been saved using standard routine in ROM)
		DWORD blockChecksum=0, dw;
		if (const int n=__importFileInformation__(zxInfo,uts,u,dw)){
			switch (uts){
				case TUniFileType::SCREEN:
					*uftExt=TUniFileType::BLOCK;
					//fallthrough
				case TUniFileType::PROGRAM:
				case TUniFileType::NUMBER_ARRAY:
				case TUniFileType::CHAR_ARRAY:
				case TUniFileType::BLOCK:
					break;
				default:
					*uftExt=TUniFileType::HEADERLESS;
					break;
			}
			if (dw) fileSize=dw;
			_stscanf( zxInfo+n, EXPORT_INFO_TAPE2, &blockFlag, &blockChecksum );
		}
		// - creating File Header (if Extension known)
		const PTapeFile tf=fileManager.files[(TTapeFileId)( rFile=(PFile)fileManager.nFiles++ )]=(PTapeFile)::malloc( NUMBER_OF_BYTES_TO_ALLOCATE_FILE(fileSize) );
			tf->type =	uts==TUniFileType::FRAGMENT // if explicitly annotated as a Fragment ...
						? TTapeFile::FRAGMENT // ... importing it as a Fragment
						: TTapeFile::HEADERLESS; // ... otherwise defaulting to the Headerless File, unless below recognized otherwise
			tf->dataBlockFlag=blockFlag;
			tf->dataChecksum=blockChecksum;
			tf->dataLength=fileSize;
		for( BYTE type=ZX_TAPE_EXTENSION_STD_COUNT; type--; )
			if (Extensions[type]==*uftExt){
				// File can be imported with Header
				tf->type=TTapeFile::STD_HEADER;
				const PHeader h=tf->GetHeader();
				// . Extension
				h->type=(TZxRom::TFileType)type;
				// . Name
				#ifdef UNICODE
					ASSERT(FALSE);
				#else
					::memcpy(	::memset(h->name,' ',ZX_TAPE_FILE_NAME_LENGTH_MAX),
								zxName, ::lstrlen(zxName)
							);
				#endif
				// . Size
				h->length=fileSize;
				// . Parameters
				h->params=u;
				break;
			}
		// - importing File Data
		f->Read( &tf->data, fileSize );
		// - File successfully imported into Tape
		m_bModified=TRUE;
		return ERROR_SUCCESS;
	}







	CSpectrumDos::CTape::TTapeTraversal::TTapeTraversal(const CTapeFileManagerView &rFileManager)
		// ctor
		: TDirectoryTraversal(0,ZX_TAPE_FILE_NAME_LENGTH_MAX) , rFileManager(rFileManager) {
		fileId=1; // Files numbered from 1
		entryType=TDirectoryTraversal::FILE;
	}
	
	CDos::PDirectoryTraversal CSpectrumDos::CTape::BeginDirectoryTraversal() const{
		// initiates exploration of current Directory through a DOS-specific DirectoryTraversal
		return new TTapeTraversal(fileManager);
	}
	bool CSpectrumDos::CTape::TTapeTraversal::AdvanceToNextEntry(){
		// True <=> found another entry in current Directory (Empty or not), otherwise False
		if (fileId<rFileManager.nFiles)
			return ( entry=(PFile)fileId++ )!=NULL;
		else
			return false;
	}

	void CSpectrumDos::CTape::TTapeTraversal::ResetCurrentEntry(BYTE directoryFillerByte) const{
		// gets current entry to the state in which it would be just after formatting
		//nop (doesn't have a Directory)
	}






	CSpectrumDos::CTape::PHeader CSpectrumDos::CTape::TTapeFile::GetHeader(){
		// returns this File's Header, or Null if this File is Headerless
		return	type==STD_HEADER
				? &stdHeader
				: NULL;
	}

	CSpectrumDos::CTape::PCHeader CSpectrumDos::CTape::TTapeFile::GetHeader() const{
		// returns this File's Header, or Null if this File is Headerless
		return const_cast<TTapeFile *>(this)->GetHeader();
	}

	

	
	
	
	
	static BYTE __getChecksum__(BYTE flag,PCBYTE data,WORD nBytes){
		// computes and returns the Checksum based on specified Flag and Data
		while (nBytes--) flag^=*data++;
		return flag;
	}
	BOOL CSpectrumDos::CTape::DoSave(LPCTSTR,BOOL){
		// True <=> Image successfully saved, otherwise False
		fileManager.f.SetLength(0); // rewriting Tape's underlying physical file
		for( TTapeFileId n=1; n<fileManager.nFiles; ){ // Files numbered from 1
			const PCTapeFile tf=fileManager.files[n++];
			if (const PCHeader h=tf->GetHeader()){
				// File features a standard Header
				static const WORD BlockLength=2+sizeof(THeader); // "+2" = Flag and Checksum
				fileManager.f.Write( &BlockLength, sizeof(BlockLength) );
				static const BYTE Flag=TStd::HEADER;
				fileManager.f.Write( &Flag, sizeof(Flag) );
				fileManager.f.Write( h, sizeof(THeader) );
				const BYTE checksum=__getChecksum__(TStd::HEADER,(PCBYTE)h,sizeof(THeader));
				fileManager.f.Write( &checksum, sizeof(checksum) );
			}
			if (tf->type!=TTapeFile::FRAGMENT){
				// "full-blown" data block
				const WORD d=tf->dataLength, blockLength=2+d; // "+2" = Flag and Checksum
				fileManager.f.Write( &blockLength, sizeof(blockLength) );
				fileManager.f.Write( &tf->dataBlockFlag, 1 );
				fileManager.f.Write( &tf->data, d );
				fileManager.f.Write( &tf->dataChecksum, 1 );
			}else{
				// data Fragment
				fileManager.f.Write( &tf->dataLength, sizeof(WORD) );
				fileManager.f.Write( &tf->data, tf->dataLength );
			}
		}
		m_bModified=FALSE;
		return TRUE;
	}

	static bool WINAPI __canTapeBeClosed__(LPCVOID tab){
		return CImage::__getActive__()->dos->ProcessCommand(ID_FILE_CLOSE)==CDos::TCmdResult::DONE;
	}

	CDos::TCmdResult CSpectrumDos::CTape::ProcessCommand(WORD cmd){
		// returns the Result of processing a DOS-related command
		switch (cmd){
			case ID_FILE_SHIFT_UP:{
				// shifting selected Files "up" (i.e. towards the beginning of Tape)
				if (__reportWriteProtection__()) return TCmdResult::DONE;
				TTapeFileId iPrevSelected=0;
				for( POSITION pos=fileManager.GetFirstSelectedFilePosition(); pos; ){
					const TTapeFileId iSelected=(TTapeFileId)fileManager.GetNextSelectedFile(pos);
					if (iPrevSelected<iSelected-1){ // before the current Selected File is at least one unselected File to whose position the current Selected File can be shifted
						PTapeFile *a=fileManager.files+iSelected, *b=a-1, tmp=*a;
						*a=*b; *b=tmp;
						fileManager.selectedFiles.AddTail( (PVOID)(iPrevSelected=iSelected-1) );
					}else
						fileManager.selectedFiles.AddTail( (PVOID)(iPrevSelected=iSelected) );
				}
				m_bModified=TRUE;
				fileManager.__refreshDisplay__();
				return TCmdResult::DONE; // cannot use DONE_REDRAW as Tape is a companion to a disk Image
			}
			case ID_FILE_SHIFT_DOWN:{
				// shifting selected Files "down" (i.e. towards the end of Tape)
				if (__reportWriteProtection__()) return TCmdResult::DONE;
				// . reversing the list of Selected Files
				CFileManagerView::TFileList selected;
				for( POSITION pos=fileManager.GetFirstSelectedFilePosition(); pos; selected.AddHead(fileManager.GetNextSelectedFile(pos)) );
				// . shifting
				for( TTapeFileId iNextSelected=1+fileManager.GetListCtrl().GetItemCount(); selected.GetCount(); ){
					const TTapeFileId iSelected=(TTapeFileId)selected.RemoveHead();
					if (iSelected+1<iNextSelected){ // after the current Selected File is at least one unselected File to whose position the current Selected File can be shifted
						PTapeFile *a=fileManager.files+iSelected, *b=a+1, tmp=*a;
						*a=*b; *b=tmp;
						fileManager.selectedFiles.AddTail( (PVOID)(iNextSelected=iSelected+1) );
					}else
						fileManager.selectedFiles.AddTail( (PVOID)(iNextSelected=iSelected) );
				}
				m_bModified=TRUE;
				fileManager.__refreshDisplay__();
				return TCmdResult::DONE; // cannot use DONE_REDRAW as Tape is a companion to a disk Image
			}
			case ID_FILE_SAVE:
				// saving the Tape to the open underlying physical file
				DoSave(NULL,FALSE);
				return TCmdResult::DONE;
			case ID_FILE_CLOSE:
				// ejecting the Tape
				return SaveModified() ? TCmdResult::DONE : TCmdResult::REFUSED;
		}
		return CDos::ProcessCommand(cmd);
	}

	bool CSpectrumDos::CTape::UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const{
		// True <=> given Command-specific user interface successfully updated, otherwise False
		switch (cmd){
			case ID_FILE_SHIFT_UP:
			case ID_FILE_SHIFT_DOWN:
				pCmdUI->Enable( fileManager.GetListCtrl().GetSelectedCount() );
				return true;
		}
		return CDos::UpdateCommandUi(cmd,pCmdUI);
	}

	void CSpectrumDos::CTape::InitializeEmptyMedium(CFormatDialog::PCParameters){
		// initializes a fresh formatted Medium (Boot, FAT, root dir, etc.)
		//nop
	}











	#define DOS	tab.dos

	#define TAB_LABEL	_T("Tape")

	#define INFORMATION_COUNT	7
	#define INFORMATION_TYPE	0 /* column to sort by */
	#define INFORMATION_NAME	1 /* column to sort by */
	#define INFORMATION_SIZE	2 /* column to sort by */
	#define INFORMATION_PARAM_1	3 /* column to sort by */
	#define INFORMATION_PARAM_2	4 /* column to sort by */
	#define INFORMATION_FLAG	5 /* column to sort by */
	#define INFORMATION_CHECKSUM 6 /* column to sort by */

	static const CFileManagerView::TFileInfo InformationList[INFORMATION_COUNT]={
		{ _T("Type"),		LVCFMT_RIGHT,	100 },
		{ _T("Name"),		LVCFMT_LEFT,	180 },
		{ _T("Size"),		LVCFMT_RIGHT,	60 },
		{ ZX_PARAMETER_1,	LVCFMT_RIGHT,	75 },
		{ ZX_PARAMETER_2,	LVCFMT_RIGHT,	75 },
		{ _T("Block flag"),	LVCFMT_RIGHT,	75 },
		{ _T("Checksum"),	LVCFMT_RIGHT,	75 }
	};

	CSpectrumDos::CTape::CTapeFileManagerView::CTapeFileManagerView(CTape *tape,const TZxRom &rZxRom,LPCTSTR fileName)
		// ctor
		// - base
		: CSpectrumFileManagerView( tape, rZxRom, REPORT, LVS_REPORT, INFORMATION_COUNT,InformationList )
		// - initialization
		, nFiles(1) // Files numbered from 1
		, f( fileName, CFile::modeReadWrite|CFile::shareExclusive|CFile::typeBinary )
		// - creating Tape's ToolBar (its positioning in WM_CREATE to be shown "after" the TapeFileManager's ToolBar)
		, toolbar( IDR_ZX_TAPE, ID_TAPE_OPEN ) { // "some" unique ID
		// - loading the Tape's content
		files[0]=NULL; // 0 index not used, Files numbered from 1
		for( WORD blockLength; f.Read(&blockLength,sizeof(blockLength))==sizeof(blockLength); )
			if (nFiles==ZX_TAPE_FILE_COUNT_MAX){
				// ERROR: too many Files on the Tape
error:			TUtils::Information(_T("The tape is corrupted."));
				break;
			}else if (blockLength>=2){
				// File (with or without a Header)
				BYTE flag=TStd::DATA;
				if (!f.Read(&flag,1)) goto error;
				bool hasHeader=false; // assumption (this is a Headerless data block)
				THeader header;
				if (flag==TStd::HEADER && blockLength==sizeof(THeader)+2){ // "+2" = Flag and Checksum
					// File with (potential) Header
					if (f.Read(&header,sizeof(header))!=sizeof(header)) goto error;
					BYTE headerChecksum;
					if (!f.Read(&headerChecksum,sizeof(BYTE))) goto error; // ERROR: Header data must be followed by a Checksum
					if (__getChecksum__(TStd::HEADER,(PCBYTE)&header,sizeof(header))==headerChecksum){
						// the block has a valid Checksum and thus describes a standard Header
						if (f.Read(&blockLength,sizeof(blockLength))!=sizeof(WORD)) goto error; // ERROR: Header must be followed by another block (Data)
						if (blockLength<2) goto error; // ERROR: Fragment not expected here
						if (!f.Read(&flag,1)) goto error; // ERROR: Header must be followed by another block (Data)
						if (flag==TStd::HEADER){
							// a standard Header cannot be followed by a block that is also a standard Header
							f.Seek(sizeof(flag)+sizeof(blockLength),CFile::current); // reverting the reading
							goto putHeaderBack;
						}
						hasHeader=true;
					}else{
putHeaderBack:			// the block has an invalid Checksum and thus cannot be considered a valid standard Header
						f.Seek(sizeof(header)+sizeof(headerChecksum),CFile::current); // reverting the reading
						// handling the block as Headerless data
					}
				}
				blockLength-=2; // "-2" = Flag already read and Checksum at the end read separately
				BYTE dataBuffer[(WORD)-1];
				if (f.Read(dataBuffer,blockLength)!=blockLength) goto error;
				BYTE dataChecksum;
				if (!f.Read(&dataChecksum,sizeof(BYTE))) goto error; // data must be followed by their Checksum
				const PTapeFile tf = files[nFiles++] = (PTapeFile)::malloc( NUMBER_OF_BYTES_TO_ALLOCATE_FILE(blockLength) );
					tf->type= hasHeader ? TTapeFile::STD_HEADER : TTapeFile::HEADERLESS;
					tf->stdHeader=header;
					tf->dataBlockFlag=flag;
					tf->dataChecksum=dataChecksum;
					tf->dataLength=blockLength;
					::memcpy( &tf->data, dataBuffer, blockLength );
			}else{
				// Fragment
				BYTE dataBuffer[2];
				if (f.Read(&dataBuffer,blockLength)!=blockLength) goto error;
				const PTapeFile tf = files[nFiles++] = (PTapeFile)::malloc( NUMBER_OF_BYTES_TO_ALLOCATE_FILE(blockLength) );
					tf->type=TTapeFile::FRAGMENT;
					tf->dataBlockFlag=TStd::DATA;
					tf->dataChecksum=__getChecksum__(TStd::DATA,dataBuffer,blockLength);
					tf->dataLength=blockLength;
					::memcpy( &tf->data, dataBuffer, blockLength );
			}
		// - showing the TapeFileManager
		TCHAR buf[MAX_PATH];
		::wsprintf( buf, TAB_LABEL _T(" \"%s\""), (LPCTSTR)f.GetFileName() );
		CTdiCtrl::AddTabLast( TDI_HWND, buf, &tab, true, __canTapeBeClosed__, NULL );
		CSpectrumDos::__informationWithCheckableShowNoMore__( _T("Use the \"") TAB_LABEL _T("\" tab to transfer files from/to the open floppy image or between two tapes (open in two instances of the application).\n\nHeaderless files:\n- are transferred to a floppy with a dummy name,\n- are used on tape to store \"tape-unfriendly\" data from a floppy (sequential files, etc.)."), INI_MSG );
	}
	CSpectrumDos::CTape::CTapeFileManagerView::~CTapeFileManagerView(){
		// dtor
		if (app.m_pMainWnd) // MainWindow still exists
			CTdiCtrl::RemoveTab( TDI_HWND, &tab );
		for( PPTapeFile s=files; nFiles--; ::free(*s++) );
	}







	LRESULT CSpectrumDos::CTape::CTapeFileManagerView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_CREATE:{
				// TapeFileManager just shown
				// . base
				CFileManagerView::WindowProc(msg,wParam,lParam);
				// . showing the Tape's ToolBar "after" the TapeFileManager's ToolBar
				toolbar.__show__( tab.toolbar );
				return 0;
			}
			case WM_DESTROY:
				// TapeFileManager destroyed - hiding the Tape's ToolBar
				toolbar.__hide__();
				break;
		}
		return CFileManagerView::WindowProc(msg,wParam,lParam);
	}

	void CSpectrumDos::CTape::CTapeFileManagerView::DrawFileInfo(LPDRAWITEMSTRUCT pdis,const int *tabs) const{
		// drawing File information
		RECT r=pdis->rcItem;
		const HDC dc=pdis->hDC;
		const PCTapeFile tf=files[pdis->itemData];
		const HGDIOBJ hFont0=::SelectObject(dc,zxRom.font);
			if (const PCHeader h=tf->GetHeader()){
				// File with Header
				// . color distinction of Files based on their Type
				if (!pdis->itemState&ODS_SELECTED)
					switch (h->type){
						case TZxRom::PROGRAM		: ::SetTextColor(dc,FILE_MANAGER_COLOR_EXECUTABLE); break;
						case TZxRom::NUMBER_ARRAY	: ::SetTextColor(dc,0xff00); break;
						case TZxRom::CHAR_ARRAY		: ::SetTextColor(dc,0xff00ff); break;
						//case TZxRom::CODE			: break;
					}
				// . COLUMN: Type
				r.right=*tabs-5;
					CPropGridCtrl::TEnum::UValue v;
						v.longValue=h->type;
					::DrawText( dc, CStdHeaderTypeEditor::__getDescription__(NULL,v,NULL,0),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
				r.left=*tabs++;
				// . COLUMN: Name
				r.right=*tabs++;
					TCHAR bufT[MAX_PATH];
					zxRom.PrintAt( dc, TZxRom::ZxToAscii(h->name,ZX_TAPE_FILE_NAME_LENGTH_MAX,bufT), r, DT_SINGLELINE|DT_VCENTER );
				r.left=r.right;
				// . COLUMN: Size
				r.right=*tabs++;
					::DrawText( dc, _itot(h->length,bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
				r.left=r.right;
				// . COLUMN: Param 1
				r.right=*tabs++;
					::DrawText( dc, _itot(h->params.param1,bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
				r.left=r.right;
				// . COLUMN: Param 2
				r.right=*tabs++;
					::DrawText( dc, _itot(h->params.param2,bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
				r.left=r.right;
				// . COLUMN: block Flag
				r.right=*tabs++;
					::DrawText( dc, _itot(tf->dataBlockFlag,bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
				r.left=r.right;
				// . COLUMN: checksum
				r.right=*tabs++;
					::DrawText( dc, _itot(tf->dataChecksum,bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
			}else if (tf->type==TTapeFile::HEADERLESS){
				// Headerless File
				// . color distinction of Files based on their Type
				if (!pdis->itemState&ODS_SELECTED)
					::SetTextColor(dc,0x999999);
				// . COLUMN: Type
				r.right=*tabs-5;
					::DrawText( dc, HEADERLESS_TYPE,-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
				r.left=*tabs++;
				// . COLUMN: Name
				r.right=*tabs++;
					//zxRom.PrintAt( dc, HEADERLESS_N_A, r, DT_SINGLELINE|DT_VCENTER );
				r.left=r.right;
				// . COLUMN: Size
				r.right=*tabs++;
					TCHAR bufT[8];
					::DrawText( dc, _itot(tf->dataLength,bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
				r.left=r.right;
				// . COLUMN: Param 1
				r.right=*tabs++;
				r.left=r.right;
				// . COLUMN: Param 2
				r.right=*tabs++;
				r.left=r.right;
				// . COLUMN: block Flag
				r.right=*tabs++;
					::DrawText( dc, _itot(tf->dataBlockFlag,bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
				r.left=r.right;
				// . COLUMN: checksum
				r.right=*tabs++;
					::DrawText( dc, _itot(tf->dataChecksum,bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
			}else{
				// Fragment
				// . color distinction of Files based on their Type
				if (!pdis->itemState&ODS_SELECTED)
					::SetTextColor(dc,0x994444);
				// . COLUMN: Type
				r.right=*tabs-5;
					::DrawText( dc, FRAGMENT_TYPE,-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
				r.left=*tabs++;
				// . COLUMN: Name
				r.right=*tabs++;
					//zxRom.PrintAt( dc, HEADERLESS_N_A, r, DT_SINGLELINE|DT_VCENTER );
				r.left=r.right;
				// . COLUMN: Size
				r.right=*tabs++;
					TCHAR bufT[8];
					::DrawText( dc, _itot(tf->dataLength,bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
				//r.left=r.right;
				// . COLUMN: Param 1
				//r.right=*tabs++;
				//r.left=r.right;
				// . COLUMN: Param 2
				//r.right=*tabs++;
				//r.left=r.right;
				// . COLUMN: block Flag
				//nop
			}
		::SelectObject(dc,hFont0);
	}

	int CSpectrumDos::CTape::CTapeFileManagerView::CompareFiles(PCFile file1,PCFile file2,BYTE information) const{
		// determines the order of given Files by the specified Information
		const PCTapeFile tf1=files[(TTapeFileId)file1], tf2=files[(TTapeFileId)file2];
		const PCHeader h1=tf1->GetHeader(), h2=tf2->GetHeader();
		switch (information){
			case INFORMATION_TYPE:{
				const BYTE typ1= h1 ? h1->type : HEADERLESS_EXTENSION;
				const BYTE typ2= h2 ? h2->type : HEADERLESS_EXTENSION;
				return typ1-typ2;
			}
			case INFORMATION_NAME:{
				const LPCSTR j1= h1 ? h1->name : HEADERLESS_N_A;
				const LPCSTR j2= h2 ? h2->name : HEADERLESS_N_A;
				return ::strncmp(j1,j2,ZX_TAPE_FILE_NAME_LENGTH_MAX);
			}
			case INFORMATION_SIZE:
				return DOS->GetFileDataSize(file1)-DOS->GetFileDataSize(file2);
			case INFORMATION_PARAM_1:{
				const WORD p1= h1 ? h1->params.param1 : -1;
				const WORD p2= h2 ? h2->params.param1 : -1;
				return p1-p2;
			}
			case INFORMATION_PARAM_2:{
				const WORD p1= h1 ? h1->params.param2 : -1;
				const WORD p2= h2 ? h2->params.param2 : -1;
				return p1-p2;
			}
			case INFORMATION_FLAG:
				return tf1->dataBlockFlag-tf2->dataBlockFlag;
			case INFORMATION_CHECKSUM:
				return tf1->dataChecksum-tf2->dataChecksum;
		}
		return 0;
	}

	bool CSpectrumDos::CTape::__markAsDirty__(PVOID,int){
		// marks the Tape as dirty
		__getFocused__()->image->SetModifiedFlag(TRUE);
		return true;
	}

	CFileManagerView::PEditorBase CSpectrumDos::CTape::CTapeFileManagerView::CreateFileInformationEditor(CDos::PFile file,BYTE infoId) const{
		// creates and returns Editor of File's selected Information; returns Null if Information cannot be edited
		const PTapeFile tf=files[(TTapeFileId)file];
		// - parameters specific for given Tape File
		if (const PHeader h=tf->GetHeader())
			// File with Header
			switch (infoId){
				case INFORMATION_TYPE:
					return stdHeaderTypeEditor.Create(	file, h->type,
														tf->dataLength>=2 ? CStdHeaderTypeEditor::STD_AND_HEADERLESS : CStdHeaderTypeEditor::STD_AND_HEADERLESS_AND_FRAGMENT
													);
				case INFORMATION_NAME:
					return varLengthFileNameEditor.Create( file, ZX_TAPE_FILE_NAME_LENGTH_MAX );
				case INFORMATION_PARAM_1:
					return stdParamEditor.Create( file, &h->params.param1, __markAsDirty__ );
				case INFORMATION_PARAM_2:
					return stdParamEditor.Create( file, &h->params.param2, __markAsDirty__ );
				case INFORMATION_FLAG:
					return __createStdEditorForByteValue__( file, &tf->dataBlockFlag, __markAsDirty__ );
				case INFORMATION_CHECKSUM:
					return __createStdEditorForByteValue__( file, &tf->dataChecksum, __markAsDirty__ );
			}
		else if (tf->type==TTapeFile::HEADERLESS)
			// Headerless File
			switch (infoId){
				case INFORMATION_TYPE:
					return stdHeaderTypeEditor.Create(	file, TZxRom::HEADERLESS,
														tf->dataLength>=2 ? CStdHeaderTypeEditor::STD_AND_HEADERLESS : CStdHeaderTypeEditor::STD_AND_HEADERLESS_AND_FRAGMENT
													);
				case INFORMATION_FLAG:
					return __createStdEditorForByteValue__( file, &tf->dataBlockFlag, __markAsDirty__ );
				case INFORMATION_CHECKSUM:
					return __createStdEditorForByteValue__( file, &tf->dataChecksum, __markAsDirty__ );
			}
		else
			// Fragment
			switch (infoId){
				case INFORMATION_TYPE:
					return stdHeaderTypeEditor.Create( file, TZxRom::FRAGMENT, CStdHeaderTypeEditor::STD_AND_HEADERLESS_AND_FRAGMENT );
			}
		return NULL;
	}




















	#define QUESTION_ON_DISPOSING_HEADER	_T("Sure to convert to fragment?")

	bool WINAPI CSpectrumDos::CTape::CTapeFileManagerView::CStdHeaderTypeEditor::__onChanged__(PVOID file,CPropGridCtrl::TEnum::UValue newType){
		// changes the Type of File
		const PDos dos=CDos::__getFocused__();
		CTapeFileManagerView *const pTapeFileManager=(CTapeFileManagerView *)dos->pFileManager;
		const PTapeFile tf=pTapeFileManager->files[(TTapeFileId)file];
		if (PHeader h=tf->GetHeader())
			// File with Header
			switch ((TZxRom::TFileType)newType.charValue){
				case TZxRom::FRAGMENT:
					if (TUtils::QuestionYesNo(QUESTION_ON_DISPOSING_HEADER,MB_DEFBUTTON2))
						tf->type=TTapeFile::FRAGMENT;
					break;
				case TZxRom::HEADERLESS:
					if (TUtils::QuestionYesNo(QUESTION_ON_DISPOSING_HEADER,MB_DEFBUTTON2))
						tf->type=TTapeFile::HEADERLESS;
					break;
				default:					
					h->type=(TZxRom::TFileType)newType.charValue;
					break;
			}
		else
			// Headerless File or Fragment
			switch ((TZxRom::TFileType)newType.charValue){
				case TZxRom::FRAGMENT:
					tf->type=TTapeFile::FRAGMENT;
					break;
				case TZxRom::HEADERLESS:
					tf->type=TTapeFile::HEADERLESS;
					break;
				default:{
					tf->type=TTapeFile::STD_HEADER;
					h=tf->GetHeader();
					h->length=tf->dataLength, h->params=UStdParameters();
					const TCHAR newExt[]={ Extensions[ h->type=(TZxRom::TFileType)newType.charValue ],'\0' };
					dos->ChangeFileNameAndExt(file,_T("Unnamed"),newExt,file); // always succeeds as all inputs are set correctly
					break;
				}
			}
		return __markAsDirty__(file,0);
	}
	CPropGridCtrl::TEnum::PCValueList WINAPI CSpectrumDos::CTape::CTapeFileManagerView::CStdHeaderTypeEditor::__createValues__(PVOID file,WORD &rnValues){
		// returns the list of standard File Types
		static const TZxRom::TFileType List[]={
			TZxRom::PROGRAM,
			TZxRom::NUMBER_ARRAY,
			TZxRom::CHAR_ARRAY,
			TZxRom::CODE,
			TZxRom::HEADERLESS,
			TZxRom::FRAGMENT
		};
		const CTapeFileManagerView *const pTapeFileManager=(CTapeFileManagerView *)((CSpectrumDos *)CDos::__getFocused__())->pFileManager;
		rnValues=4+pTapeFileManager->stdHeaderTypeEditor.types;
		return List;
	}
	LPCTSTR WINAPI CSpectrumDos::CTape::CTapeFileManagerView::CStdHeaderTypeEditor::__getDescription__(PVOID file,CPropGridCtrl::TEnum::UValue stdType,PTCHAR,short){
		// returns the textual description of the specified Type
		switch ((TZxRom::TFileType)stdType.charValue){
			case TZxRom::PROGRAM		: return _T("Program");
			case TZxRom::NUMBER_ARRAY	: return _T("Numbers");
			case TZxRom::CHAR_ARRAY		: return _T("Characters");
			case TZxRom::CODE			: return _T("Bytes");
			case TZxRom::HEADERLESS		: return HEADERLESS_TYPE;
			case TZxRom::FRAGMENT		: return FRAGMENT_TYPE;
			default:
				return _T("<Unknown>");
		}
	}
	CFileManagerView::PEditorBase CSpectrumDos::CTape::CTapeFileManagerView::CStdHeaderTypeEditor::Create(PFile file,TZxRom::TFileType type,TDisplayTypes _types){
		// creates and returns an Editor of standard File Type
		const PDos dos=CDos::__getFocused__();
		CSpectrumFileManagerView *const pZxFileManager=(CSpectrumFileManagerView *)dos->pFileManager;
		types=_types;
		const PEditorBase result=pZxFileManager->__createStdEditor__(
			file, &( data=type ), sizeof(data),
			CPropGridCtrl::TEnum::DefineConstStringListEditorA( __createValues__, __getDescription__, NULL, __onChanged__ )
		);
		::SendMessage( CEditorBase::pSingleShown->hEditor, WM_SETFONT, (WPARAM)pZxFileManager->rFont.m_hObject, 0 );
		return result;
	}
