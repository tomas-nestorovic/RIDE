#include "stdafx.h"

	#define INI_SPECTRUM	_T("ZXSpectrum")

	const RGBQUAD CSpectrumDos::Colors[16]={
		0,0,0,0,	// colors for Bright=0
		192,0,0,0,
		0,0,192,0,
		192,0,192,0,
		0,192,0,0,
		192,192,0,0,
		0,192,192,0,
		192,192,192,0,
		20,20,20,0,	// colors for Bright=1
		255,0,0,0,
		0,0,255,0,
		255,0,255,0,
		0,255,0,0,
		255,255,0,0,
		0,255,255,0,
		255,255,255,0
	};









	CSpectrumDos::UStdParameters::UStdParameters()
		// ctor
		: dw(0x80000000) {
	}









	void CSpectrumDos::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_SPECTRUM, messageId );
	}

	void CSpectrumDos::__parseFat32LongName__(PTCHAR buf,LPCTSTR &rOutName,BYTE nameLengthMax,LPCTSTR &rOutExt,BYTE extLengthMax,LPCTSTR &rOutZxInfo){
		// parses input FAT long name into three components: ZX Name (of LengthMax chars at most), single-char ZX Extension, and ZX Information
		// - finding ZX import information
		rOutZxInfo=nullptr; // assumption (no ZX import information found)
		if (PTCHAR pSpace=_tcsrchr(buf,' ')) // string may be terminated with import information, see CSpectrumDos::__importFileInformation__
			if (pSpace[1]=='Z' && pSpace[2]=='X'){ // ZX import information must be correctly prefixed
				*pSpace++='\0'; // terminating the File's Name+Extension
				rOutZxInfo=pSpace;
			}
		// - parsing the input string
		rOutName=buf; // Name always starts at the beginning of Buffer
		PTCHAR pExt=_tcsrchr(buf,'.');
		if (pExt) // Extension specified (Dot found)
			*pExt++='\0', rOutExt=pExt;
		else // Extension not specified (Dot not found)
			rOutExt=_T("");
		// - unescaping and trimming the Name
		DWORD dw=nameLengthMax;
		::UrlUnescape(	TZxRom::AsciiToZx(buf,buf,nullptr), // converting in place to ZX charset
						nullptr, &dw,
						URL_UNESCAPE_INPLACE // unescaping in place
					);
		for( PTCHAR a=buf,b=a; *a=*b++; a+=*a!='\x1' ); // eliminating 0x01 characters that interrupt ZX keywords (see CSpectrumDos::GetFileExportNameAndExt)
		if (::lstrlen(buf)>nameLengthMax) // Name potentially too long, trimming it
			buf[nameLengthMax]='\0';
		// - unescaping and trimming the Extension
		if (pExt){
			dw=extLengthMax;
			::UrlUnescape(	TZxRom::AsciiToZx(pExt,pExt,nullptr), // converting in place to ZX charset
							nullptr, &dw,
							URL_UNESCAPE_INPLACE // unescaping in place
						);
			for( PTCHAR a=pExt,b=a; *a=*b++; a+=*a!='\x1' ); // eliminating 0x01 characters that interrupt ZX keywords (see CSpectrumDos::GetFileExportNameAndExt)
			if (::lstrlen(pExt)>extLengthMax)
				pExt[extLengthMax]='\0';
		}
	}

	#define INFO_STD	_T(" ZX%c%xL%x")

	int CSpectrumDos::__exportFileInformation__(PTCHAR buf,TUniFileType uniFileType,UStdParameters params,DWORD fileLength){
		// populates the Buffer with File export information in normalized form and returns the number of characters written to the Buffer
		return _stprintf( buf, INFO_STD, uniFileType, params, fileLength );
	}
	int CSpectrumDos::__importFileInformation__(LPCTSTR buf,TUniFileType &rUniFileType,UStdParameters &rParams,DWORD &rFileLength){
		// returns the number of characters recognized as import information normalized form (supplied by ExportFileInformation)
		rUniFileType=TUniFileType::UNKNOWN, rFileLength=0; // initialization
		if (buf){ // Null if File has no import information
			int n=0;
			if (_stscanf( buf, INFO_STD _T("%n"), &rUniFileType, &rParams, &rFileLength, &n ))
				return n;
		}
		return 0;
	}










	CSpectrumDos::CSpectrumDos(PImage image,PCFormat pFormatBoot,TTrackScheme trackAccessScheme,PCProperties properties,UINT nResId,CSpectrumFileManagerView *pFileManager,TGetFileSizeOptions _getFileSizeDefaultOption)
		// ctor
		: CDos(image,pFormatBoot,trackAccessScheme,properties,::lstrcmp,sideMap,nResId,pFileManager,_getFileSizeDefaultOption)
		, trackMap(this) {
		::memcpy( sideMap, StdSidesMap, sizeof(sideMap) ); // mapping Head numbers to Side numbers as the IBM norm dictates
	}

	CSpectrumDos::~CSpectrumDos(){
		// dtor
		if (CTape::pSingleInstance)
			delete CTape::pSingleInstance;
		if (CScreenPreview::pSingleInstance && &CScreenPreview::pSingleInstance->rFileManager==pFileManager)
			CScreenPreview::pSingleInstance->DestroyWindow();
		if (CBasicPreview::pSingleInstance && &CBasicPreview::pSingleInstance->rFileManager==pFileManager)
			CBasicPreview::pSingleInstance->DestroyWindow();
	}









	#define DOS tab.dos

	#define FORMAT_ADDITIONAL_COUNT	2
	#define	UNFORMAT_COUNT			3

	#define TAPE_EXTENSION	_T(".tap")

	CDos::TCmdResult CSpectrumDos::ProcessCommand(WORD cmd){
		// returns the Result of processing a DOS-related command
		switch (cmd){
			case ID_DOS_FILL_EMPTY_SPACE:
				// filling out empty space on disk
				__fillEmptySpace__( CFillEmptySpaceDialog(this) ); // WARNING: It's assumed that "dot" and "dotdot"-like DirectoryEntries are disabled to prevent from unfinite looping when selected to fill Empty DirectoryEntries!
				return TCmdResult::DONE_REDRAW;
			case ID_DOS_FORMAT:{
				// formatting standard Cylinders (i.e. with standard "official" Sectors)
				TCylinder bufCylinders[FDD_CYLINDERS_MAX*2];// a "big enough" Buffer
				THead bufHeads[FDD_CYLINDERS_MAX*2];		// a "big enough" Buffer
				const TCylinder cylMin=min( 1+__getLastOccupiedStdCylinder__(), formatBoot.nCylinders );
				CFormatDialog::TStdFormat additionalFormats[]={
					{ _T("Expand to 40 cylinders"),	cylMin, formatBoot, 1, 0, FDD_SECTOR_GAP3_STD, properties->stdFormats->params.nAllocationTables, properties->nRootDirectoryEntriesMax },
					{ _T("Expand to 80 cylinders"),	cylMin, formatBoot, 1, 0, FDD_SECTOR_GAP3_STD, properties->stdFormats->params.nAllocationTables, properties->nRootDirectoryEntriesMax }
				};
					additionalFormats[0].params.format.nCylinders=39;
					additionalFormats[1].params.format.nCylinders=79;
				CFormatDialog d(this, additionalFormats,
								cylMin&&formatBoot.mediumType!=TMedium::UNKNOWN ? FORMAT_ADDITIONAL_COUNT : 0 // AdditionalFormats available only if Image already formatted before
							);
				return	__showDialogAndFormatStdCylinders__( d, bufCylinders, bufHeads )==ERROR_SUCCESS
						? TCmdResult::DONE_REDRAW
						: TCmdResult::REFUSED;
			}
			case ID_DOS_UNFORMAT:{
				// unformatting Cylinders
				TCylinder bufCylinders[FDD_CYLINDERS_MAX*2];// a "big enough" Buffer
				THead bufHeads[FDD_CYLINDERS_MAX*2];		// a "big enough" Buffer
				const TCylinder cylMin=1+__getLastOccupiedStdCylinder__(), cylMax=image->GetCylinderCount()-1;
				const CUnformatDialog::TStdUnformat stdUnformats[]={
					{ _T("Trim to 40 cylinders"),	40, cylMax },
					{ _T("Trim to 80 cylinders"),	80, cylMax },
					{ STR_TRIM_TO_MIN_NUMBER_OF_CYLINDERS,	cylMin, cylMax }
				};
				return	__unformatStdCylinders__( CUnformatDialog(this,stdUnformats,UNFORMAT_COUNT), bufCylinders, bufHeads )==ERROR_SUCCESS
						? TCmdResult::DONE_REDRAW
						: TCmdResult::REFUSED;
			}
			case ID_FILE_CLOSE:
			case ID_TAPE_CLOSE:
				// closing this Image (or a Tape, if opened)
				if (CTape::pSingleInstance)
					if (CTape::pSingleInstance->image->SaveModified()){
						CTdiCtrl::RemoveTab( TDI_HWND, &CTape::pSingleInstance->pFileManager->tab );
						return TCmdResult::DONE; // the closing command applies only to the open Tape, not to the main disk Image
					}else
						return TCmdResult::REFUSED; // rejected to close the Tape
				break; // closing the main disk Image
			case ID_TAPE_NEW:{
				// creating the underlying Tape file on local disk
				if (CTape::pSingleInstance) // closing the open Tape first
					if (ProcessCommand(ID_FILE_CLOSE)==TCmdResult::REFUSED) // if closing of the open Tape rejected ...
						return TCmdResult::DONE; // ... a new Tape cannot be created
				TCHAR fileName[MAX_PATH];
				*fileName='\0';
				CString title;
					title.LoadString(AFX_IDS_SAVEFILE);
				CFileDialog d( FALSE, TAPE_EXTENSION, nullptr, OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY|OFN_DONTADDTORECENT, _T("Tape (*") TAPE_EXTENSION _T(")|*") TAPE_EXTENSION _T("|") );
					d.m_ofn.lStructSize=sizeof(OPENFILENAME); // to show the "Places bar"
					d.m_ofn.nFilterIndex=1;
					d.m_ofn.lpstrTitle=title;
					d.m_ofn.lpstrFile=fileName;
				if (d.DoModal()==IDOK){
					// . ejecting current Tape (if any)
					if (CTape::pSingleInstance)
						if (ProcessCommand(ID_TAPE_CLOSE)==TCmdResult::REFUSED) // if Tape not ejected ...
							return TCmdResult::DONE; // ... we are done (successfully)
					// . inserting a blank Tape (by creating a new underlying physical file and opening it)
					CFile( fileName, CFile::modeCreate|CFile::shareDenyRead|CFile::typeBinary ).Close(); // creating the underlying file on local disk
					( CTape::pSingleInstance=new CTape(fileName,this,true) )->__toggleWriteProtection__(); // new Tape is not WriteProtected
					return TCmdResult::DONE;
				}else
					return TCmdResult::DONE_REDRAW;
			}
			case ID_TAPE_OPEN:{
				// opening an existing file with Tape
				if (CTape::pSingleInstance) // closing the open Tape first
					if (ProcessCommand(ID_FILE_CLOSE)==TCmdResult::REFUSED) // if closing of the open Tape rejected ...
						return TCmdResult::DONE; // ... a new Tape cannot be open
				TCHAR fileName[MAX_PATH];
				*fileName='\0';
				CString title;
					title.LoadString(AFX_IDS_OPENFILE);
				CFileDialog d( TRUE, TAPE_EXTENSION, nullptr, OFN_FILEMUSTEXIST, _T("Tape (*") TAPE_EXTENSION _T(")|*") TAPE_EXTENSION _T("|") );
					d.m_ofn.lStructSize=sizeof(OPENFILENAME); // to show the "Places bar"
					d.m_ofn.nFilterIndex=1;
					d.m_ofn.lpstrTitle=title;
					d.m_ofn.lpstrFile=fileName;
				if (d.DoModal()==IDOK){
					// . ejecting current Tape (if any)
					if (CTape::pSingleInstance)
						if (ProcessCommand(ID_TAPE_CLOSE)==TCmdResult::REFUSED) // if Tape not ejected ...
							return TCmdResult::DONE; // ... we are done (successfully)
					// . inserting a recorded Tape (by opening its underlying physical file)
					CTape::pSingleInstance=new CTape(fileName,this,true); // inserted Tape is WriteProtected by default
					return TCmdResult::DONE;
				}else
					return TCmdResult::DONE_REDRAW;
			}
			case ID_TAPE_APPEND:{
				// copying content of another tape to the end of this tape
				TCHAR fileName[MAX_PATH];
				*fileName='\0';
				CString title;
					title.LoadString(AFX_IDS_OPENFILE);
				CFileDialog d( TRUE, TAPE_EXTENSION, nullptr, OFN_FILEMUSTEXIST, _T("Tape (*") TAPE_EXTENSION _T(")|*") TAPE_EXTENSION _T("|") );
					d.m_ofn.lStructSize=sizeof(OPENFILENAME); // to show the "Places bar"
					d.m_ofn.nFilterIndex=1;
					d.m_ofn.lpstrTitle=title;
					d.m_ofn.lpstrFile=fileName;
				if (d.DoModal()==IDOK){
					// . opening the Tape to append
					//const std::unique_ptr<const CTape> tape(new CTape(fileName,this,false));
					const CTape *const tape=new CTape(fileName,this,false);
						// . appending each File (or block)
						if (const auto pdt=tape->BeginDirectoryTraversal(ZX_DIR_ROOT))
							for( BYTE blockData[65536]; const PCFile file=pdt->GetNextFileOrSubdir(); ){
								TCHAR nameAndExt[MAX_PATH];
								PFile f;
								if (const TStdWinError err=	CTape::pSingleInstance->ImportFile(
																&CMemFile(blockData,sizeof(blockData)), 
																tape->ExportFile( file, &CMemFile(blockData,sizeof(blockData)), sizeof(blockData), nullptr ),
																tape->GetFileExportNameAndExt( file, false, nameAndExt ),
																0, f
															)
								){
									TCHAR msg[200+MAX_PATH];
									::wsprintf( msg, _T("Failed to append block \"%s\""), nameAndExt );
									Utils::Information( msg, err, _T("Appended only partly.") );
									break;
								}
							}
					CTdiCtrl::RemoveTab( TDI_HWND, &tape->pFileManager->tab );
					image->UpdateAllViews(nullptr);
					return TCmdResult::DONE;
				}else
					return TCmdResult::DONE_REDRAW;
			}
			case ID_ZX_PREVIEWASSCREEN:
				// previewing File(s) on Spectrum screen
				if (CScreenPreview::pSingleInstance)
					CScreenPreview::pSingleInstance->DestroyWindow();
				new CScreenPreview(	__isTapeFileManagerShown__()
									? CTape::pSingleInstance->fileManager
									: *pFileManager
								);
				return TCmdResult::DONE;
			case ID_ZX_PREVIEWASBASIC:
				// previewing File(s) as BASIC program(s)
				if (CBasicPreview::pSingleInstance)
					CBasicPreview::pSingleInstance->DestroyWindow();
				new CBasicPreview(	__isTapeFileManagerShown__()
									? CTape::pSingleInstance->fileManager
									: *pFileManager
								);
				return TCmdResult::DONE;
			default:
				// passing a non-recognized Command to an open Tape first
				if (__isTapeFileManagerShown__() && CTape::pSingleInstance->OnCmdMsg(cmd,CN_COMMAND,nullptr,nullptr))
					return TCmdResult::DONE;
		}
		return CDos::ProcessCommand(cmd);
	}

	PTCHAR CSpectrumDos::GetFileExportNameAndExt(PCFile file,bool shellCompliant,PTCHAR buf) const{
		// populates Buffer with specified File's export name and extension and returns the Buffer; returns Null if File cannot be exported (e.g. a "dotdot" entry in MS-DOS); caller guarantees that the Buffer is at least MAX_PATH characters big
		if (shellCompliant){
			// exporting to non-RIDE target (e.g. to the Explorer); excluding from the Buffer characters that are forbidden in FAT32 long file names
			TCHAR zxNameAndExt[MAX_PATH],pcNameAndExt[MAX_PATH];
			for( PTCHAR p=::lstrcpy(pcNameAndExt,TZxRom::ZxToAscii(GetFileNameWithAppendedExt(file,zxNameAndExt),-1,pcNameAndExt)); const TCHAR c=*p; ) // "lstrcpy" = making sure that the string starts at the beginning of the buffer
				if (__isValidCharInFat32LongName__(c))
					p++; // keeping valid Character
				else
					::lstrcpy(p,1+p); // skipping invalid Character
			if (*pcNameAndExt=='.' || *pcNameAndExt=='\0'){
				// invalid export name - generating an artifical one
				static WORD fileId;
				::wsprintf( buf, _T("File%04d%s"), ++fileId, pcNameAndExt );
			}else
				// valid export name - taking it as the result
				::lstrcpy(buf,pcNameAndExt);
			return buf;
		}else{
			// exporting to another RIDE instance; substituting non-alphanumeric characters with "URL-like" escape sequences
			// . URL-escaping the File name and extension, e.g. "PICTURE01.B" -> "PICTURE%48x%49x.B"
			__super::GetFileExportNameAndExt(file,shellCompliant,buf);
			// . checking that the File name is importable back in the same form, e.g. "PICTURE.B" is not exported as [PI][CTURE.B] where "PI" is a Spectrum keyword
			TCHAR currNameAndExt[MAX_PATH];
			GetFileNameWithAppendedExt(file,currNameAndExt);
			for( TCHAR tmp[MAX_PATH],*p=buf; *p; p++ )
				if (*p!='%'){ // not an escape sequence "%NN"
					LPCTSTR name,ext,zxInfo;
					__parseFat32LongName__( ::lstrcpyn(tmp,buf,p-buf+2), name,-1, ext,-1, zxInfo ); // "+2" = "+1" for including current char "*p" and another "+1" for terminating null char
					if (*ext!='\0')
						::lstrcat( ::lstrcat(tmp,_T(".")), ext );
					if (::strncmp( tmp, currNameAndExt, ::lstrlen(tmp) )){
						// the exported name cannot be imported back in the same form - interrupting the sequence of characters to prevent from keywords being recognized (e.g. "PI" in "PICTURE")
						#define ZX_KEYWORD_INTERRUPTION_CHAR	_T("%01")
						::memcpy( p + sizeof(ZX_KEYWORD_INTERRUPTION_CHAR)/sizeof(TCHAR)-1, p, (::lstrlen(p)+1)*sizeof(TCHAR) );
						::memcpy( p, ZX_KEYWORD_INTERRUPTION_CHAR, sizeof(ZX_KEYWORD_INTERRUPTION_CHAR)-sizeof(TCHAR) );
						p+=sizeof(ZX_KEYWORD_INTERRUPTION_CHAR)/sizeof(TCHAR)-1;
					}
				}else // an escape sequence "%NN" (e.g. "%20" for a space char) - skipping it
					p+=2;
			// . returning a File name and extension that are well importable back
			return buf;
		}
	}

	DWORD CSpectrumDos::GetAttributes(PCFile file) const{
		// maps File's attributes to Windows attributes and returns the result
		return	file!=ZX_DIR_ROOT
				? 0 // none but standard attributes
				: FILE_ATTRIBUTE_DIRECTORY; // root Directory
	}

	bool CSpectrumDos::UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const{
		// True <=> given Command-specific user interface successfully updated, otherwise False
		switch (cmd){
			case ID_TAPE_APPEND:
				pCmdUI->Enable(CTape::pSingleInstance!=nullptr && !CTape::pSingleInstance->IsWriteProtected());
				return true;
			case ID_TAPE_CLOSE:
				pCmdUI->Enable(CTape::pSingleInstance!=nullptr);
				return true;
			default:
				if (__isTapeFileManagerShown__() && CTape::pSingleInstance->OnCmdMsg(cmd,CN_UPDATE_COMMAND_UI,pCmdUI,nullptr))
					return true;
				break;
		}
		return CDos::UpdateCommandUi(cmd,pCmdUI);
	}

	bool CSpectrumDos::__isTapeFileManagerShown__() const{
		// True <=> Tape's FileManager is currently shown in the TDI, otherwise False
		return CTape::pSingleInstance && CTape::pSingleInstance->fileManager.m_hWnd; // A&B, A = Tape inserted, B = Tape's FileManager currently switched to
	}

	bool CSpectrumDos::CanBeShutDown(CFrameWnd* pFrame) const{
		// True <=> this DOS has no dependecies which would require it to remain active, otherwise False (has some dependecies which require the DOS to remain active)
		// - first attempting to close the Tape
		if (CTape::pSingleInstance)
			if (!CTape::pSingleInstance->CanCloseFrame(pFrame))
				return FALSE;
		// - base
		return CDos::CanBeShutDown(pFrame);
	}
