#include "stdafx.h"

	#define INI_MSG	_T("tapeinit")

	#define FILE_LENGTH_MAX	0xff80

	static constexpr TFormat TapeFormat={
		Medium::FLOPPY_DD_525, // no need to create a new Medium Type for a Tape
		Codec::ANY, // no need to create a new Codec Type for a Tape
		1, 1, 1, TFormat::LENGTHCODE_128,FILE_LENGTH_MAX, 1 // Tape Blocks are not divided into Sectors (thus here set a single "Sector" with maximum length)
	};

	CSpectrumDos::CTape *CSpectrumDos::CTape::pSingleInstance;

	#define TAB_LABEL	_T("Tape")

	static bool WINAPI CanTapeBeClosed(CTdiCtrl::TTab::PContent tab){
		const PImage tape=CDos::GetFocused()->image;
		return tape->SaveModified()!=FALSE;
	}

	static void WINAPI OnTapeClosing(CTdiCtrl::TTab::PContent tab){
		delete ((CMainWindow::CTdiView::PTab)tab)->image->dos;
	}

	CSpectrumDos::CTape::CTape(LPCTSTR fileName,const CSpectrumDos *diskDos,bool makeCurrentTab)
		// ctor
		// - base
		: CSpectrumBase( this, &TapeFormat, TTrackScheme::BY_CYLINDERS, diskDos->properties, 0, &fileManager, TGetFileSizeOptions::OfficialDataLength, TSectorStatus::UNAVAILABLE )
		, CImageRaw(&CImageRaw::Properties,false) // "some" Image
		// - initialization
		, fileManager( this, diskDos->zxRom, fileName, makeCurrentTab ) {
		dos=this; // linking the DOS and Image
		(HACCEL)menu.hAccel=diskDos->menu.hAccel; // for DiskDos accelerators to work even if switched to Tape
		SetPathName(fileName,FALSE);
		// - showing the TapeFileManager
		TCHAR buf[MAX_PATH];
		::wsprintf( buf, TAB_LABEL _T(" \"%s\""), (LPCTSTR)fileManager.f.GetFileName() );
		CTdiCtrl::AddTabLast( TDI_HWND, buf, &fileManager.tab, makeCurrentTab, CanTapeBeClosed, OnTapeClosing );
		CSpectrumDos::__informationWithCheckableShowNoMore__( _T("Use the \"") TAB_LABEL _T("\" tab to transfer files from/to the open disk image or between two tapes (open in two instances of ") APP_ABBREVIATION _T(").\n\nHeaderless files:\n- are transferred to disk with dummy names,\n- are used on tape to store \"tape-unfriendly\" data from a disk (sequential files, etc.)."), INI_MSG );
		// - adding this Tape to most recently used ones
		TCHAR fileNameCopy[MAX_PATH];
		diskDos->mruTapes.Add( ::lstrcpy(fileNameCopy,fileName) ); // creating a copy as MFC may (for some reason) corrupt the original string
	}

	CSpectrumDos::CTape::~CTape(){
		// dtor
		if (app.m_pMainWnd) // MainWindow still exists
			CTdiCtrl::RemoveTab( TDI_HWND, &fileManager.tab );
		if (pSingleInstance==this)
			pSingleInstance=nullptr; // no longer accepting any requests
		dos=nullptr; // to not destroy the Image (as DOS and Image are one structure in memory that is disposed at once)
		(HACCEL)menu.hAccel=0; // for DiskDos accelerators to be not destroyed
	}









	void CSpectrumDos::CTape::GetTrackData(TCylinder cyl,THead head,Revolution::TType,PCSectorId,PCBYTE,TSector,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses){
		// populates output buffers with specified Sectors' data, usable lengths, and FDC statuses; ALWAYS attempts to buffer all Sectors - caller is then to sort out eventual read errors (by observing the FDC statuses); caller can call ::GetLastError to discover the error for the last Sector in the input list
		ASSERT( outBufferData!=nullptr && outBufferLengths!=nullptr && outFdcStatuses!=nullptr );
		*outFdcStatuses=TFdcStatus::WithoutError; // assumption
		switch (head){
			case 0: // want Data
				if (cyl<fileManager.nFiles)
					*outBufferData=fileManager.files[cyl]->data, *outBufferLengths=fileManager.files[cyl]->dataLength;
				else
					*outBufferData=nullptr, *outBufferLengths=0;
				break;
			default:
				ASSERT(FALSE);
				//fallthrough
			case 1: // want Header
				if (cyl<fileManager.nFiles)
					*outBufferData=(PSectorData)fileManager.files[cyl]->GetHeader();
				else
					*outBufferData=nullptr, *outFdcStatuses=TFdcStatus::IdFieldCrcError;
				*outBufferLengths=sizeof(THeader);
				break;
		}
	}

	TStdWinError CSpectrumDos::CTape::MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus){
		// marks Sector on a given PhysicalAddress as "dirty", plus sets it the given FdcStatus; returns Windows standard i/o error
		if (chs.cylinder<fileManager.nFiles){
			fileManager.files[chs.cylinder]->dataChecksumStatus=TTapeFile::TDataChecksumStatus::UNDETERMINED; // the Checksum needs re-comparison
			m_bModified=true;
			return ERROR_SUCCESS;
		}else
			return ERROR_FILE_NOT_FOUND;
	}








	void CSpectrumDos::CTape::FlushToBootSector() const{
		// flushes internal Format information to the actual Boot Sector's data
		//nop (not applicable for Tape)
	}





	bool CSpectrumDos::CTape::GetSectorStatuses(TCylinder,THead,TSector,PCSectorId,PSectorStatus) const{
		// True <=> Statuses of all Sectors in the Track successfully retrieved and populated the Buffer, otherwise False
		return true; // see FormatBoot initialization in ctor
	}
	bool CSpectrumDos::CTape::ModifyStdSectorStatus(RCPhysicalAddress,TSectorStatus) const{
		// True <=> the Status of the specified DOS-standard Sector successfully changed, otherwise False
		//nop (not applicable for Tape)
		const_cast<CTape *>(this)->m_bModified=TRUE;
		return true;
	}
	bool CSpectrumDos::CTape::GetFileFatPath(PCFile file,CFatPath &rFatPath) const{
		// True <=> FatPath of given File (even an erroneous FatPath) successfully retrieved, otherwise False
		// - if queried about the root Directory, populating the FatPath with root Directory Sectors
		if (file==ZX_DIR_ROOT){
			CFatPath::TItem item;
				item.chs.head=1; // want Header
			for( item.chs.cylinder=0; item.chs.cylinder<fileManager.nFiles; item.chs.cylinder++ )
				rFatPath.AddItem(&item);
			return true;
		}
		// - extracting the FatPath
		for( TTapeTraversal tt(fileManager); tt.AdvanceToNextEntry(); )
			if (tt.entry==file){
				const CFatPath::TItem item={ tt.chs.cylinder, tt.chs };
				rFatPath.AddItem(&item);
				return true;
			}
		return false;
	}

	bool CSpectrumDos::CTape::ModifyFileFatPath(PFile file,const CFatPath &rFatPath) const{
		// True <=> a error-free FatPath of given File successfully written, otherwise False
		return false; // operation not applicable for a Tape
	}

	DWORD CSpectrumDos::CTape::GetFreeSpaceInBytes(TStdWinError &rError) const{
		// computes and returns the empty space on disk
		rError=ERROR_SUCCESS;
		return ( ZX_TAPE_FILE_COUNT_MAX - fileManager.nFiles )*FILE_LENGTH_MAX;
	}





	#define HEADERLESS_EXTENSION	'H'
	#define HEADERLESS_N_A			_T("N/A")

	const TCHAR CSpectrumDos::CTape::Extensions[ZX_TAPE_EXTENSION_STD_COUNT]={
		ZX_TAPE_EXTENSION_PROGRAM,
		ZX_TAPE_EXTENSION_NUMBERS,
		ZX_TAPE_EXTENSION_CHARS,
		ZX_TAPE_EXTENSION_BYTES
	};

	void CSpectrumDos::CTape::THeader::GetNameOrExt(PPathString pOutName,PPathString pOutExt) const{
		// populates the Buffers with File's name and extension; caller guarantees that the Buffer sizes are at least MAX_PATH characters each
		if (pOutName)
			// Name wanted - removing trailing spaces
			( *pOutName=CPathString(name,sizeof(name)) ).TrimRight(' ');
		if (pOutExt)
			// Extension wanted - trying to map Type to one of UniversalFileTypes
			*pOutExt=	type<ZX_TAPE_EXTENSION_STD_COUNT
						? Extensions[type]
						: type;
	}

	TStdWinError CSpectrumDos::CTape::THeader::SetName(RCPathString newName){
		// tries to change given File's name and extension; returns Windows standard i/o error
		// - checking that the NewName+NewExt combination follows the "10.1" convention
		if (newName.GetLength()>ZX_TAPE_FILE_NAME_LENGTH_MAX)
			return ERROR_FILENAME_EXCED_RANGE;
		// - renaming
		#ifdef UNICODE
			ASSERT(FALSE)
		#else
			::memcpy(	::memset(name,' ',ZX_TAPE_FILE_NAME_LENGTH_MAX),
						newName, newName.GetLength()
					);
		#endif
		// - successfully renamed
		return ERROR_SUCCESS;
	}

	CSpectrumDos::TUniFileType CSpectrumDos::CTape::THeader::GetUniFileType() const{
		// maps FileType to global UniFileType
		return	type<ZX_TAPE_EXTENSION_STD_COUNT
				? (TUniFileType)Extensions[type]
				: TUniFileType::UNKNOWN;
	}

	bool CSpectrumDos::CTape::THeader::SetFileType(TUniFileType uts){
		// maps specified UniFileType to particular standard Spectrum FileType
		switch (uts){
			case TUniFileType::PROGRAM		: type=TZxRom::TFileType::PROGRAM; return true;
			case TUniFileType::CHAR_ARRAY	: type=TZxRom::TFileType::CHAR_ARRAY; return true;
			case TUniFileType::NUMBER_ARRAY	: type=TZxRom::TFileType::NUMBER_ARRAY; return true;
			case TUniFileType::BLOCK:
			case TUniFileType::SCREEN		: type=TZxRom::TFileType::CODE; return true;
		}
		return false;
	}

	bool CSpectrumDos::CTape::GetFileNameOrExt(PCFile file,PPathString pOutName,PPathString pOutExt) const{
		// populates the Buffers with File's name and extension; caller guarantees that the Buffer sizes are at least MAX_PATH characters each
		if (file==ZX_DIR_ROOT){
			if (pOutName)
				*pOutName='\\';
			if (pOutExt)
				*pOutExt=_T("");
		}else{
			const PCTapeFile tf=(PCTapeFile)file;
			if (const PCHeader h=tf->GetHeader())
				// File with a Header
				h->GetNameOrExt( pOutName, pOutExt );
			else{
				// Headerless File or Fragment
				if (pOutName)
					pOutName->Format( _T("%08d"), idHeaderless++ ); // ID padded with zeros to eight digits (to make up an acceptable name even for TR-DOS)
				if (pOutExt)
					*pOutExt=HEADERLESS_EXTENSION;
				return false; // name irrelevant
			}
		}
		return true; // name relevant
	}

	TStdWinError CSpectrumDos::CTape::ChangeFileNameAndExt(PFile file,RCPathString newName,RCPathString newExt,PFile &rRenamedFile){
		// tries to change given File's name and extension; returns Windows standard i/o error
		// - can't change root Directory's name
		if (file==ZX_DIR_ROOT)
			return ERROR_ACCESS_DENIED;
		// - renaming
		if (const PHeader h=((PTapeFile)( rRenamedFile=file ))->GetHeader()){
			// File with a Header
			// . Extension must be specified
			if (newExt.GetLength()<1)
				return ERROR_BAD_FILE_TYPE;
			// . making sure that a File with given NameAndExtension doesn't yet exist 
			//nop (Files on tape may he equal names)
			// . renaming
			if (const TStdWinError err=h->SetName(newName))
				return err;
			if (!h->SetFileType((TUniFileType)*newExt))
				return ERROR_BAD_FILE_TYPE;
			m_bModified=TRUE;
			return ERROR_SUCCESS;
		}else
			// Headerless File or Fragment
			return ERROR_SUCCESS; // simply ignoring the request (Success to be able to create headerless File copies in the Tape)
	}
	DWORD CSpectrumDos::CTape::GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData,TGetFileSizeOptions option) const{
		// determines and returns the size of specified File
		if (pnBytesReservedBeforeData) *pnBytesReservedBeforeData=0;
		if (pnBytesReservedAfterData) *pnBytesReservedAfterData=0;
		const PCTapeFile tf=(PCTapeFile)file;
		if (tf==ZX_DIR_ROOT)
			// to allow for browsing Headers as "Directory entries", pretend there is a root Directory that occupies a whole multiple of Header sizes
			return fileManager.nFiles*sizeof(THeader);
		else
			// File with or without a Header, or a Fragment
			return tf->dataLength;
	}

	DWORD CSpectrumDos::CTape::GetAttributes(PCFile file) const{
		// maps File's attributes to Windows attributes and returns the result
		return	file!=ZX_DIR_ROOT
				? 0 // none but standard attributes
				: FILE_ATTRIBUTE_DIRECTORY; // root Directory
	}

	TStdWinError CSpectrumDos::CTape::DeleteFile(PFile file){
		// deletes specified File; returns Windows standard i/o error
		if (file==ZX_DIR_ROOT)
			return ERROR_ACCESS_DENIED; // can't delete the root Directory
		for( TTapeTraversal tt(fileManager); tt.AdvanceToNextEntry(); )
			if (tt.entry==file){
				PPTapeFile a=fileManager.files+tt.fileId, b=a;
				::free(*b++), fileManager.nFiles--;
				for( short n=fileManager.nFiles-tt.fileId; n--; *a++=*b++ );
				m_bModified=TRUE;
				break;
			}
		return ERROR_SUCCESS;
	}

	#define EXPORT_INFO_TAPE	_T("S%x")

	CString CSpectrumDos::CTape::GetFileExportNameAndExt(PCFile file,bool shellCompliant) const{
		// returns File name concatenated with File extension for export of the File to another Windows application (e.g. Explorer)
		CString result=__super::GetFileExportNameAndExt(file,shellCompliant);
		if (!shellCompliant){
			const PCTapeFile tf=(PCTapeFile)file;
			TCHAR buf[80];
			if (const PCHeader h=tf->GetHeader())
				// File with a Header
				::wsprintf( buf+__exportFileInformation__(buf,h->GetUniFileType(),h->params,h->length,tf->dataBlockFlag), EXPORT_INFO_TAPE, tf->dataChecksum );
			else if (tf->type==TTapeFile::HEADERLESS)
				// Headerless File
				::wsprintf( buf+__exportFileInformation__(buf,TUniFileType::HEADERLESS,TStdParameters::Default,tf->dataLength,tf->dataBlockFlag), EXPORT_INFO_TAPE, tf->dataChecksum );
			else
				// Fragment
				__exportFileInformation__(buf,TUniFileType::FRAGMENT,TStdParameters::Default,tf->dataLength);
			result+=buf;
		}
		return result;
	}

	#define NUMBER_OF_BYTES_TO_ALLOCATE_FILE(dataLength)\
		(sizeof(TTapeFile)+dataLength)

	static BYTE __getChecksum__(BYTE flag,PCBYTE data,WORD nBytes){
		// computes and returns the Checksum based on specified Flag and Data
		while (nBytes--) flag^=*data++;
		return flag;
	}

	TStdWinError CSpectrumDos::CTape::ImportFile(CFile *f,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile){
		// imports specified File (physical or virtual) into the Image; returns Windows standard i/o error
		// - checking if there's an empty slot in Tape's "Directory"
		if (fileManager.nFiles==ZX_TAPE_FILE_COUNT_MAX)
			return Utils::ErrorByOs( ERROR_VOLMGR_DISK_NOT_ENOUGH_SPACE, ERROR_CANNOT_MAKE );
		// - checking if File length is within range
		if (fileSize>FILE_LENGTH_MAX)
			return ERROR_FILE_TOO_LARGE;
		// - converting the NameAndExtension to the "10.1" form usable for Tape
		CPathString zxName,zxExt; LPCTSTR zxInfo;
		TCHAR buf[16384];
		__parseFat32LongName__(	::lstrcpy(buf,nameAndExtension), zxName, zxExt, zxInfo );
		zxName.TrimToLength(ZX_TAPE_FILE_NAME_LENGTH_MAX);
		zxExt.TrimToLength(1);
		// - processing import information
		TStdParameters u=TStdParameters::Default;
		TUniFileType uts=TUniFileType::HEADERLESS;
		int blockChecksum=-1; DWORD officialFileSize=fileSize; BYTE blockFlag;
		if (const int n=__importFileInformation__(zxInfo,uts,u,officialFileSize,blockFlag)){
			if (uts==TUniFileType::SCREEN)
				uts=TUniFileType::BLOCK;
			_stscanf( zxInfo+n, EXPORT_INFO_TAPE, &blockChecksum );
		}
		// - with user's intervention resolving the case that reported size is different from real size
		if (officialFileSize!=fileSize){
			// : defining the Dialog
			CString msg;
			TTapeFile tmp;
				tmp.type=TTapeFile::TType::STD_HEADER;
				tmp.stdHeader.SetName(zxName);
				tmp.stdHeader.SetFileType(uts);
			msg.Format( _T("Real (%d) and reported (%d) sizes of \"%s\" differ."), fileSize, officialFileSize, (LPCTSTR)GetFilePresentationNameAndExt(&tmp) );
			class CSuggestionDialog sealed:public Utils::CCommandDialog{
				const bool offerFileSplit;
				const TUniFileType uts;

				BOOL OnInitDialog() override{
					// dialog initialization
					// > base
					const BOOL result=__super::OnInitDialog();
					// > supplying available actions
					AddCommandButton( IDYES, _T("Import as-is (recommended)"), true );
					AddCommandButton( IDNO, _T("Set reported size as real size (no Tape loading error)") );
					if (offerFileSplit){
						TZxRom::TFileType t=TZxRom::TFileType::HEADERLESS; // assumption
						for( BYTE type=ZX_TAPE_EXTENSION_STD_COUNT; type--; )
							if (Extensions[type]==uts){
								// File can be imported with Header
								t=(TZxRom::TFileType)type;
								break;
							}
						TCHAR buf[200];
						::wsprintf( buf, _T("Import as two separate blocks (%s and ") ZX_TAPE_HEADERLESS_STR _T(")"), TZxRom::GetFileTypeName(t) );
						AddCommandButton( IDRETRY, buf );
					}
					AddCancelButton( _T("Cancel import") );
					return result;
				}
			public:
				CSuggestionDialog(LPCTSTR msg,bool offerFileSplit,TUniFileType uts)
					// ctor
					: Utils::CCommandDialog(msg)
					, offerFileSplit(offerFileSplit) , uts(uts) {
				}
			} d( msg, officialFileSize<fileSize, uts );
			// : showing the Dialog and processing its result
			switch (d.DoModal()){
				case IDNO:
					officialFileSize=fileSize;
					//fallthrough
				case IDYES:
					break;
				case IDRETRY:{
					// > Part 1: Program/Code/Chars/Numbers
					if (const TStdWinError err=ImportFile( f, officialFileSize, nameAndExtension, winAttr, rFile ))
						return err;
					// > Part 2: Headerless remainder
					fileSize-=officialFileSize;
					TCHAR tmp[MAX_PATH];
					__exportFileInformation__( ::lstrcpy(tmp,_T("H"))+1, TUniFileType::HEADERLESS, TStdParameters::Default, fileSize );
					return ImportFile( f, fileSize, tmp, winAttr, rFile );
				}
				default:
					return ERROR_CANCELLED;
			}
		}
		// - creating File Header (if Extension known)
		const PTapeFile tf=fileManager.files[fileManager.nFiles++]=(PTapeFile)::malloc( NUMBER_OF_BYTES_TO_ALLOCATE_FILE(fileSize) );
			tf->type =	uts==TUniFileType::FRAGMENT // if explicitly annotated as a Fragment ...
						? TTapeFile::FRAGMENT // ... importing it as a Fragment
						: TTapeFile::HEADERLESS; // ... otherwise defaulting to the Headerless File, unless below recognized otherwise
			tf->dataBlockFlag=blockFlag;
			//tf->dataChecksum=blockChecksum; // commented out as set later
			tf->dataChecksumStatus=TTapeFile::TDataChecksumStatus::UNDETERMINED;
			tf->dataLength=fileSize;
		rFile=tf;
		for( BYTE type=ZX_TAPE_EXTENSION_STD_COUNT; type--; )
			if (Extensions[type]==uts){
				// File can be imported with Header
				tf->type=TTapeFile::STD_HEADER; // the File isn't Headerless as assumed above
				const PHeader h=tf->GetHeader();
				// . Extension
				h->type=(TZxRom::TFileType)type;
				// . Name
				#ifdef UNICODE
					ASSERT(FALSE);
				#else
					::memcpy(	::memset(h->name,' ',ZX_TAPE_FILE_NAME_LENGTH_MAX),
								zxName, zxName.GetLength()
							);
				#endif
				// . Size
				h->length=officialFileSize;
				// . Parameters
				h->params=u;
				break;
			}
		// - importing File Data
		f->Read( tf->data, tf->dataLength );
		tf->dataChecksum =	blockChecksum>=0
							? blockChecksum
							: __getChecksum__( tf->dataBlockFlag, tf->data, tf->dataLength );
		// - File successfully imported into Tape
		m_bModified=TRUE;
		return ERROR_SUCCESS;
	}







	CSpectrumDos::CTape::TTapeTraversal::TTapeTraversal(const CTapeFileManagerView &rFileManager)
		// ctor
		: TDirectoryTraversal( ZX_DIR_ROOT, sizeof(THeader) )
		, rFileManager(rFileManager)
		, fileId(-1) {
		chs.head=0; // want File Data; won't change throughout the traversal
		entryType=TDirectoryTraversal::FILE; // won't change throughout the traversal
	}
	
	std::unique_ptr<CDos::TDirectoryTraversal> CSpectrumDos::CTape::BeginDirectoryTraversal(PCFile directory) const{
		// initiates exploration of specified Directory through a DOS-specific DirectoryTraversal
		ASSERT(directory==ZX_DIR_ROOT);
		return std::unique_ptr<TDirectoryTraversal>( new TTapeTraversal(fileManager) );
	}
	bool CSpectrumDos::CTape::TTapeTraversal::AdvanceToNextEntry(){
		// True <=> found another entry in current Directory (Empty or not), otherwise False
		if (++fileId<rFileManager.nFiles){
			entry=rFileManager.files[ chs.cylinder=fileId ];
			return true;
		}else
			return false;
	}

	void CSpectrumDos::CTape::TTapeTraversal::ResetCurrentEntry(BYTE directoryFillerByte) const{
		// gets current entry to the state in which it would be just after formatting
		if (entryType==TDirectoryTraversal::FILE)
			::memset( entry, directoryFillerByte, entrySize );
	}






	CSpectrumDos::CTape::PHeader CSpectrumDos::CTape::TTapeFile::GetHeader(){
		// returns this File's Header, or Null if this File is Headerless
		return	type==STD_HEADER
				? &stdHeader
				: nullptr;
	}

	CSpectrumDos::CTape::PCHeader CSpectrumDos::CTape::TTapeFile::GetHeader() const{
		// returns this File's Header, or Null if this File is Headerless
		return const_cast<TTapeFile *>(this)->GetHeader();
	}

	

	
	
	
	
	BOOL CSpectrumDos::CTape::DoSave(LPCTSTR,BOOL){
		// True <=> Image successfully saved, otherwise False
		fileManager.f.SetLength(0); // rewriting Tape's underlying physical file
		for( short n=0; n<fileManager.nFiles; ){
			const PCTapeFile tf=fileManager.files[n++];
			if (const PCHeader h=tf->GetHeader()){
				// File features a standard Header
				static constexpr WORD BlockLength=2+sizeof(THeader); // "+2" = Flag and Checksum
				fileManager.f.Write( &BlockLength, sizeof(BlockLength) );
				static constexpr BYTE Flag=TZxRom::TStdBlockFlag::HEADER;
				fileManager.f.Write( &Flag, sizeof(Flag) );
				fileManager.f.Write( h, sizeof(THeader) );
				const BYTE checksum=__getChecksum__(TZxRom::TStdBlockFlag::HEADER,(PCBYTE)h,sizeof(THeader));
				fileManager.f.Write( &checksum, sizeof(checksum) );
			}
			if (tf->type!=TTapeFile::FRAGMENT){
				// "full-blown" data block
				const WORD d=tf->dataLength, blockLength=2+d; // "+2" = Flag and Checksum
				fileManager.f.Write( &blockLength, sizeof(blockLength) );
				fileManager.f.Write( &tf->dataBlockFlag, 1 );
				fileManager.f.Write( tf->data, d );
				fileManager.f.Write( &tf->dataChecksum, 1 );
			}else{
				// data Fragment
				fileManager.f.Write( &tf->dataLength, sizeof(WORD) );
				fileManager.f.Write( tf->data, tf->dataLength );
			}
		}
		m_bModified=FALSE;
		return TRUE;
	}

	CDos::TCmdResult CSpectrumDos::CTape::ProcessCommand(WORD cmd){
		// returns the Result of processing a DOS-related command
		switch (cmd){
			case ID_FILE_SHIFT_UP:{
				// shifting selected Files "up" (i.e. towards the beginning of Tape)
				if (!fileManager.m_hWnd) break; // giving up this command if FileManager not switched to
				if (ReportWriteProtection()) return TCmdResult::DONE;
				const CListCtrl &lv=fileManager.GetListCtrl();
				short iPrevSelected=-1;
				for( POSITION pos=lv.GetFirstSelectedItemPosition(); pos; ){
					const short iSelected=lv.GetNextSelectedItem(pos);
					PTapeFile *const a=fileManager.files+iSelected;
					fileManager.selectedFiles.AddTail(*a);
					if (iPrevSelected<iSelected-1){ // before the current Selected File is at least one unselected File to whose position the current Selected File can be shifted
						PTapeFile *const b=a-1, tmp=*a;
						*a=*b; *b=tmp;
						iPrevSelected=iSelected-1;
					}else
						iPrevSelected=iSelected;
				}
				m_bModified=TRUE;
				fileManager.RefreshDisplay();
				return TCmdResult::DONE; // cannot use DONE_REDRAW as Tape is a companion to a disk Image
			}
			case ID_FILE_SHIFT_DOWN:{
				// shifting selected Files "down" (i.e. towards the end of Tape)
				if (!fileManager.m_hWnd) break; // giving up this command if FileManager not switched to
				if (ReportWriteProtection()) return TCmdResult::DONE;
				// . reversing the list of Selected Files
				const CListCtrl &lv=fileManager.GetListCtrl();
				CFileManagerView::CFileList selectedIndices;
				for( POSITION pos=lv.GetFirstSelectedItemPosition(); pos; selectedIndices.AddHead((PFile)lv.GetNextSelectedItem(pos)) );
				// . shifting
				for( short iNextSelected=fileManager.nFiles; selectedIndices.GetCount(); ){
					const short iSelected=(short)selectedIndices.RemoveHead();
					PTapeFile *const a=fileManager.files+iSelected;
					fileManager.selectedFiles.AddTail(*a);
					if (iSelected+1<iNextSelected){ // after the current Selected File is at least one unselected File to whose position the current Selected File can be shifted
						PTapeFile *const b=a+1, tmp=*a;
						*a=*b; *b=tmp;
						iNextSelected=iSelected+1;
					}else
						iNextSelected=iSelected;
				}
				m_bModified=TRUE;
				fileManager.RefreshDisplay();
				return TCmdResult::DONE; // cannot use DONE_REDRAW as Tape is a companion to a disk Image
			}
			case ID_COMPUTE_CHECKSUM:{
				// recomputes the Checksum for selected Files
				if (!fileManager.m_hWnd) break; // giving up this command if FileManager not switched to
				if (ReportWriteProtection()) return TCmdResult::DONE;
				const CListCtrl &lv=fileManager.GetListCtrl();
				for( POSITION pos=lv.GetFirstSelectedItemPosition(); pos; ){
					const PTapeFile tf=fileManager.files[lv.GetNextSelectedItem(pos)];
					tf->dataChecksum=__getChecksum__( tf->dataBlockFlag, tf->data, tf->dataLength );
					tf->dataChecksumStatus=TTapeFile::TDataChecksumStatus::UNDETERMINED;
				}
				m_bModified=TRUE;
				fileManager.Invalidate();
				return TCmdResult::DONE; // cannot use DONE_REDRAW as Tape is a companion to a disk Image
			}
			case ID_FILE_SAVE:
				// saving the Tape to the open underlying physical file
				DoSave(nullptr,FALSE);
				return TCmdResult::DONE;
		}
		return __super::ProcessCommand(cmd);
	}

	bool CSpectrumDos::CTape::UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const{
		// True <=> given Command-specific user interface successfully updated, otherwise False
		switch (cmd){
			case ID_FILE_SHIFT_UP:
			case ID_FILE_SHIFT_DOWN:
			case ID_COMPUTE_CHECKSUM:
				if (!fileManager.m_hWnd) break; // giving up this command if FileManager not switched to
				pCmdUI->Enable( fileManager.GetListCtrl().GetSelectedCount() );
				return true;
		}
		return __super::UpdateCommandUi(cmd,pCmdUI);
	}

	void CSpectrumDos::CTape::InitializeEmptyMedium(CFormatDialog::PCParameters){
		// initializes a fresh formatted Medium (Boot, FAT, root dir, etc.)
		//nop
	}











	#define IMAGE	tab.image
	#define DOS		IMAGE->dos

	#define INFORMATION_COUNT		8
	#define INFORMATION_TYPE		0 /* column to sort by */
	#define INFORMATION_NAME		1 /* column to sort by */
	#define INFORMATION_SIZE		2 /* column to sort by */
	#define INFORMATION_SIZE_REPORTED 3 /* column to sort by */
	#define INFORMATION_PARAM_1		4 /* column to sort by */
	#define INFORMATION_PARAM_2		5 /* column to sort by */
	#define INFORMATION_FLAG		6 /* column to sort by */
	#define INFORMATION_CHECKSUM	7 /* column to sort by */

	const CFileManagerView::TFileInfo CSpectrumDos::CTape::CTapeFileManagerView::InformationList[INFORMATION_COUNT]={
		{ _T("Type"),		100,	TFileInfo::AlignRight },
		{ _T("Name"),		180,	TFileInfo::AlignLeft|TFileInfo::FileName },
		{ _T("Size"),		60,		TFileInfo::AlignRight },
		{ _T("Reported size"), 90,	TFileInfo::AlignRight },
		{ ZX_PARAMETER_1,	75,		TFileInfo::AlignRight },
		{ ZX_PARAMETER_2,	75,		TFileInfo::AlignRight },
		{ _T("Block flag"),	75,		TFileInfo::AlignRight },
		{ _T("Checksum"),	75,		TFileInfo::AlignRight }
	};

	CSpectrumDos::CTape::CTapeFileManagerView::CTapeFileManagerView(CTape *tape,const TZxRom &rZxRom,LPCTSTR fileName,bool makeCurrentTab)
		// ctor
		// - base
		: CSpectrumBaseFileManagerView( tape, rZxRom, REPORT, LVS_REPORT, INFORMATION_COUNT,InformationList, ZX_TAPE_FILE_NAME_LENGTH_MAX )
		// - creating Tape's ToolBar (its positioning in WM_CREATE to be shown "after" the TapeFileManager's ToolBar)
		, toolbar( IDR_ZX_TAPE, ID_TAPE_OPEN ) // "some" unique ID
		// - initialization
		, nFiles(0)
		, f( fileName, CFile::modeReadWrite|CFile::shareExclusive|CFile::typeBinary ) {
		informOnCapabilities=false; // don't show default message on what the FileManager can do (showed customized later)
		// - loading the Tape's content
		for( WORD blockLength; f.Read(&blockLength,sizeof(blockLength))==sizeof(blockLength); )
			if (nFiles==ZX_TAPE_FILE_COUNT_MAX){
				// ERROR: too many Files on the Tape
error:			Utils::Information(_T("The tape is corrupted."));
				break;
			}else if (blockLength>=2){
				// File (with or without a Header)
				BYTE flag=TZxRom::TStdBlockFlag::DATA;
				if (!f.Read(&flag,1)) goto error;
				bool hasHeader=false; // assumption (this is a Headerless data block)
				THeader header;
				if (flag==TZxRom::TStdBlockFlag::HEADER && blockLength==sizeof(THeader)+2){ // "+2" = Flag and Checksum
					// File with (potential) Header
					if (f.Read(&header,sizeof(header))!=sizeof(header)) goto error;
					BYTE headerChecksum;
					if (!f.Read(&headerChecksum,sizeof(BYTE))) goto error; // ERROR: Header data must be followed by a Checksum
					if (__getChecksum__(TZxRom::TStdBlockFlag::HEADER,(PCBYTE)&header,sizeof(header))==headerChecksum){
						// the block has a valid Checksum and thus describes a standard Header
						if (f.Read(&blockLength,sizeof(blockLength))!=sizeof(WORD)) goto error; // ERROR: Header must be followed by another block (Data)
						if (blockLength<2) goto error; // ERROR: Fragment not expected here
						if (!f.Read(&flag,1)) goto error; // ERROR: Header must be followed by another block (Data)
						if (flag==TZxRom::TStdBlockFlag::HEADER){
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
					tf->dataChecksumStatus=TTapeFile::TDataChecksumStatus::UNDETERMINED;
					tf->dataLength=blockLength;
					::memcpy( tf->data, dataBuffer, blockLength );
			}else{
				// Fragment
				BYTE dataBuffer[2];
				if (f.Read(&dataBuffer,blockLength)!=blockLength) goto error;
				const PTapeFile tf = files[nFiles++] = (PTapeFile)::malloc( NUMBER_OF_BYTES_TO_ALLOCATE_FILE(blockLength) );
					tf->type=TTapeFile::FRAGMENT;
					tf->dataBlockFlag=TZxRom::TStdBlockFlag::DATA;
					tf->dataChecksum=__getChecksum__(TZxRom::TStdBlockFlag::DATA,dataBuffer,blockLength);
					tf->dataLength=blockLength;
					::memcpy( tf->data, dataBuffer, blockLength );
			}
	}
	CSpectrumDos::CTape::CTapeFileManagerView::~CTapeFileManagerView(){
		// dtor
		for( PPTapeFile s=files; nFiles--; ::free(*s++) );
	}







	BOOL CSpectrumDos::CTape::CTapeFileManagerView::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		switch (nCode){
			case CN_COMMAND:
				// command
				switch (nID){
					case ID_DIRECTORY:
						// navigation to focused Tape block
						//fallthrough
					case ID_FILEMANAGER_DIR_HEXAMODE:{
						// browsing of Tape Headers in hexa mode
						const auto sl0=DOS->formatBoot.sectorLength;
						DOS->formatBoot.sectorLength=sizeof(THeader);
							const BOOL result=__super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
						DOS->formatBoot.sectorLength=sl0;
						return result;
					}
				}
				break;
		}
		return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	}

	LRESULT CSpectrumDos::CTape::CTapeFileManagerView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_CREATE:
				// TapeFileManager just shown
				// . base
				if (const LRESULT err=__super::WindowProc(msg,wParam,lParam))
					return err;
				// . showing the Tape's ToolBar "after" the TapeFileManager's ToolBar
				toolbar.Show( tab.toolbar );
				return 0;
			case WM_DESTROY:
				// TapeFileManager destroyed - hiding the Tape's ToolBar
				toolbar.Hide();
				break;
		}
		return __super::WindowProc(msg,wParam,lParam);
	}

	void CSpectrumDos::CTape::CTapeFileManagerView::DrawReportModeCell(PCFileInfo pFileInfo,LPDRAWITEMSTRUCT pdis) const{
		// drawing File information
		RECT &r=pdis->rcItem;
		const HDC dc=pdis->hDC;
		const PTapeFile tf=(PTapeFile)pdis->itemData;
		const HGDIOBJ hFont0=::SelectObject(dc,zxRom.font);
			if (const PCHeader h=tf->GetHeader()){
				// File with Header
				// . color distinction of Files based on their Type
				if ((pdis->itemState&ODS_SELECTED)==0)
					switch (h->type){
						case TZxRom::PROGRAM		: ::SetTextColor(dc,FILE_MANAGER_COLOR_EXECUTABLE); break;
						case TZxRom::NUMBER_ARRAY	: ::SetTextColor(dc,0xff00); break;
						case TZxRom::CHAR_ARRAY		: ::SetTextColor(dc,0xff00ff); break;
						//case TZxRom::CODE			: break;
					}
				// . drawing Information
				switch (pFileInfo-InformationList){
					case INFORMATION_TYPE:{
						// Type
						r.right-=5;
						stdTapeHeaderTypeEditor.DrawReportModeCell( h->type, pdis );
						break;
					}
					case INFORMATION_NAME:
						// Name
						varLengthCommandLineEditor.DrawReportModeCell( h->name, ZX_TAPE_FILE_NAME_LENGTH_MAX, ' ', pdis );
						break;
					case INFORMATION_SIZE:
						// Size
						integerEditor.DrawReportModeCell( tf->dataLength, pdis, tf->dataLength!=h->length );
						break;
					case INFORMATION_SIZE_REPORTED:
						// Size
						integerEditor.DrawReportModeCell( h->length, pdis, tf->dataLength!=h->length );
						break;
					case INFORMATION_PARAM_1:
						// start address / Basic start line
						integerEditor.DrawReportModeCell( h->params.param1, pdis );
						break;
					case INFORMATION_PARAM_2:
						// length of Basic Program without variables
						integerEditor.DrawReportModeCell( h->params.param2, pdis );
						break;
					case INFORMATION_FLAG:
						// block Flag
						integerEditor.DrawReportModeCell( tf->dataBlockFlag, pdis, tf->dataBlockFlag!=TZxRom::TStdBlockFlag::DATA );
						break;
					case INFORMATION_CHECKSUM:{
drawChecksum:			// checksum
						if (tf->dataChecksumStatus==TTapeFile::TDataChecksumStatus::UNDETERMINED)
							tf->dataChecksumStatus=	tf->dataChecksum==__getChecksum__(tf->dataBlockFlag,tf->data,tf->dataLength)
													? TTapeFile::TDataChecksumStatus::CORRECT
													: TTapeFile::TDataChecksumStatus::INCORRECT;
						integerEditor.DrawReportModeCellWithCheckmark( tf->dataChecksum, tf->dataChecksumStatus==TTapeFile::TDataChecksumStatus::CORRECT, pdis );
						break;
					}
				}
			}else if (tf->type==TTapeFile::HEADERLESS){
				// Headerless File
				// . color distinction of Files based on their Type
				if ((pdis->itemState&ODS_SELECTED)==0)
					::SetTextColor(dc,0x999999);
				// . drawing Information
				switch (pFileInfo-InformationList){
					case INFORMATION_TYPE:
						// Type
						r.right-=5;
						stdTapeHeaderTypeEditor.DrawReportModeCell( TZxRom::TFileType::HEADERLESS, pdis );
						break;
					case INFORMATION_SIZE:
						// Size
						integerEditor.DrawReportModeCell( tf->dataLength, pdis );
						break;
					case INFORMATION_FLAG:
						// block Flag
						integerEditor.DrawReportModeCell( tf->dataBlockFlag, pdis, tf->dataBlockFlag!=TZxRom::TStdBlockFlag::DATA );
						break;
					case INFORMATION_CHECKSUM:
						// checksum
						goto drawChecksum;
				}
			}else{
				// Fragment
				// . color distinction of Files based on their Type
				if ((pdis->itemState&ODS_SELECTED)==0)
					::SetTextColor(dc,0x994444);
				// . drawing Information
				switch (pFileInfo-InformationList){
					case INFORMATION_TYPE:
						// Type
						r.right-=5;
						stdTapeHeaderTypeEditor.DrawReportModeCell( TZxRom::TFileType::FRAGMENT, pdis );
						break;
					case INFORMATION_SIZE:
						// Size
						integerEditor.DrawReportModeCell( tf->dataLength, pdis );
						break;
				}
			}
		::SelectObject(dc,hFont0);
	}

	int CSpectrumDos::CTape::CTapeFileManagerView::CompareFiles(PCFile file1,PCFile file2,BYTE information) const{
		// determines the order of given Files by the specified Information
		const PCTapeFile tf1=(PCTapeFile)file1, tf2=(PCTapeFile)file2;
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
				return DOS->GetFileOfficialSize(file1)-DOS->GetFileOfficialSize(file2);
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

	bool WINAPI CSpectrumDos::CTape::CTapeFileManagerView::__checksumModified__(PVOID file,int){
		// marks the TapeFile's data Checksum as modified
		((PTapeFile)file)->dataChecksumStatus=TTapeFile::TDataChecksumStatus::UNDETERMINED; // the Checksum needs re-comparison
		__markDirectorySectorAsDirty__(file);
		return true;
	}

	CFileManagerView::PEditorBase CSpectrumDos::CTape::CTapeFileManagerView::CreateFileInformationEditor(CDos::PFile file,BYTE infoId) const{
		// creates and returns Editor of File's selected Information; returns Null if Information cannot be edited
		const PTapeFile tf=(PTapeFile)file;
		// - parameters specific for given Tape File
		if (const PHeader h=tf->GetHeader())
			// File with Header
			switch (infoId){
				case INFORMATION_TYPE:
					return stdTapeHeaderTypeEditor.Create(	file, h->type,
															tf->dataLength>=2 ? CStdTapeHeaderBlockTypeEditor::STD_AND_HEADERLESS : CStdTapeHeaderBlockTypeEditor::STD_AND_HEADERLESS_AND_FRAGMENT,
															__tapeBlockTypeModified__
														);
				case INFORMATION_NAME:
					return varLengthCommandLineEditor.CreateForFileName( file, ZX_TAPE_FILE_NAME_LENGTH_MAX, ' ' );
				case INFORMATION_SIZE_REPORTED:
					return integerEditor.Create( file, &h->length );
				case INFORMATION_PARAM_1:
					return integerEditor.Create( file, &h->params.param1 );
				case INFORMATION_PARAM_2:
					return integerEditor.Create( file, &h->params.param2 );
				case INFORMATION_FLAG:
					return integerEditor.Create( file, &tf->dataBlockFlag );
				case INFORMATION_CHECKSUM:
					return integerEditor.Create( file, &tf->dataChecksum, __checksumModified__ );
			}
		else if (tf->type==TTapeFile::HEADERLESS)
			// Headerless File
			switch (infoId){
				case INFORMATION_TYPE:
					return stdTapeHeaderTypeEditor.Create(	file, TZxRom::HEADERLESS,
															tf->dataLength>=2 ? CStdTapeHeaderBlockTypeEditor::STD_AND_HEADERLESS : CStdTapeHeaderBlockTypeEditor::STD_AND_HEADERLESS_AND_FRAGMENT,
															__tapeBlockTypeModified__
														);
				case INFORMATION_FLAG:
					return integerEditor.Create( file, &tf->dataBlockFlag );
				case INFORMATION_CHECKSUM:
					return integerEditor.Create( file, &tf->dataChecksum, __checksumModified__ );
			}
		else
			// Fragment
			switch (infoId){
				case INFORMATION_TYPE:
					return stdTapeHeaderTypeEditor.Create( file, TZxRom::FRAGMENT, CStdTapeHeaderBlockTypeEditor::STD_AND_HEADERLESS_AND_FRAGMENT, __tapeBlockTypeModified__ );
			}
		return nullptr;
	}




















	bool WINAPI CSpectrumDos::CTape::CTapeFileManagerView::__tapeBlockTypeModified__(PVOID file,PropGrid::Enum::UValue newType){
		// changes the Type of File
		const PDos dos=CDos::GetFocused();
		const CSpectrumBaseFileManagerView *const pZxFileManager=(CSpectrumBaseFileManagerView *)dos->pFileManager;
		const PTapeFile tf=(PTapeFile)file;
		if (PHeader h=tf->GetHeader())
			// File with Header
			switch ((TZxRom::TFileType)newType.charValue){
				case TZxRom::FRAGMENT:
					if (Utils::QuestionYesNo(_T("Sure to convert to fragment?"),MB_DEFBUTTON2))
						tf->type=TTapeFile::FRAGMENT;
					break;
				case TZxRom::HEADERLESS:
					if (Utils::QuestionYesNo(_T("Sure to dispose the header?"),MB_DEFBUTTON2))
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
					h->length=tf->dataLength, h->params=TStdParameters::Default;
					h->SetName(_T("Unnamed"));
					h->type=(TZxRom::TFileType)newType.charValue;
					break;
				}
			}
		__markDirectorySectorAsDirty__(file);
		return true;
	}

	PropGrid::Enum::PCValueList WINAPI CSpectrumBase::CSpectrumBaseFileManagerView::CStdTapeHeaderBlockTypeEditor::__createValues__(PVOID file,WORD &rnValues){
		// returns the list of standard File Types
		static constexpr TZxRom::TFileType List[]={
			TZxRom::PROGRAM,
			TZxRom::NUMBER_ARRAY,
			TZxRom::CHAR_ARRAY,
			TZxRom::CODE,
			TZxRom::HEADERLESS,
			TZxRom::FRAGMENT
		};
		const CSpectrumBaseFileManagerView *const pZxFileManager=(CSpectrumBaseFileManagerView *)((CSpectrumDos *)CDos::GetFocused())->pFileManager;
		rnValues=4+pZxFileManager->stdTapeHeaderTypeEditor.types;
		return List;
	}

	LPCTSTR WINAPI CSpectrumBase::CSpectrumBaseFileManagerView::CStdTapeHeaderBlockTypeEditor::__getDescription__(PVOID file,PropGrid::Enum::UValue stdType,PTCHAR,short){
		// returns the textual description of the specified Type
		return TZxRom::GetFileTypeName((TZxRom::TFileType)stdType.charValue);
	}

	CFileManagerView::PEditorBase CSpectrumBase::CSpectrumBaseFileManagerView::CStdTapeHeaderBlockTypeEditor::Create(PFile file,TZxRom::TFileType type,TDisplayTypes _types,PropGrid::Enum::TOnValueConfirmed onChanged) const{
		// creates and returns an Editor of standard File Type
		types=_types;
		const PEditorBase result=CreateStdEditor(
			file, &( data=type ),
			PropGrid::Enum::DefineConstStringListEditorA( sizeof(data), __createValues__, __getDescription__, nullptr, onChanged )
		);
		RCFileManagerView &rfm=*CDos::GetFocused()->pFileManager;
		::SendMessage( CEditorBase::pSingleShown->hEditor, WM_SETFONT, (WPARAM)rfm.rFont.m_hObject, 0 );
		return result;
	}

	void CSpectrumBase::CSpectrumBaseFileManagerView::CStdTapeHeaderBlockTypeEditor::DrawReportModeCell(BYTE type,LPDRAWITEMSTRUCT pdis){
		// directly draws the block Type
		PropGrid::Enum::UValue v;
			v.longValue=type;
		if (!TZxRom::IsKnownFileType((TZxRom::TFileType)v.longValue))
			DrawRedHighlight(pdis);
		::DrawText( pdis->hDC, __getDescription__(nullptr,v,nullptr,0),-1, &pdis->rcItem, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
	}
