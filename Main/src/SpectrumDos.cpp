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




	CSpectrumDos::TZxRom::TZxRom()
		// ctor
		// - creating the Font using which all text will be printed on screen
		: font(FONT_COURIER_NEW,100,false,true) {
		//: font(_T("DejaVu Sans Mono Bold"),90,false,true,65) {
		//: font(_T("DejaVu Sans Mono"),86,false,true,60) {
	}

	const LPCSTR CSpectrumDos::TZxRom::Keywords[]={
		_T("RND"), _T("INKEY$"), _T("PI"), _T("FN "), _T("POINT "), _T("SCREEN$ "), _T("ATTR "), _T("AT "), _T("TAB "), _T("VAL$ "), _T("CODE "), _T("VAL "), _T("LEN "), _T("SIN "), _T("COS "), _T("TAN "), _T("ASN "), _T("ACS "), _T("ATN "), _T("LN "), _T("EXP "), _T("INT "), _T("SQR "), _T("SGN "), _T("ABS "), _T("PEEK "), _T("IN "), _T("USR "), _T("STR$ "), _T("CHR$ "), _T("NOT "), _T("BIN "),
		_T(" OR "), _T(" AND "), _T("<="), _T(">="), _T("<>"), _T(" LINE "), _T(" THEN "), _T(" TO "), _T(" STEP "), _T(" DEF FN "), _T(" CAT "), _T(" FORMAT "), _T(" MOVE "), _T(" ERASE "), _T(" OPEN #"), _T(" CLOSE #"), _T(" MERGE "), _T(" VERIFY "), _T(" BEEP "), _T(" CIRCLE "), _T(" INK "), _T(" PAPER "), _T(" FLASH "), _T(" BRIGHT "), _T(" INVERSE "), _T(" OVER "), _T(" OUT "), _T(" LPRINT "), _T(" LLIST "), _T(" STOP "), _T(" READ "), _T(" DATA "), _T(" RESTORE "),
		_T(" NEW "), _T(" BORDER "), _T(" CONTINUE "), _T(" DIM "), _T(" REM "), _T(" FOR "), _T(" GO TO "), _T(" GO SUB "), _T(" INPUT "), _T(" LOAD "), _T(" LIST "), _T(" LET "), _T(" PAUSE "), _T(" NEXT "), _T(" POKE "), _T(" PRINT "), _T(" PLOT "), _T(" RUN "), _T(" SAVE "), _T(" RANDOMIZE "), _T(" IF "), _T(" CLS "), _T(" DRAW "), _T(" CLEAR "), _T(" RETURN "), _T(" COPY ")
	};

	// KEYWORD_TOKEN_FIRST corresponds to the "RND" Keyword in ZX Spectrum charset
	#define KEYWORD_TOKEN_FIRST	165

	PTCHAR CSpectrumDos::TZxRom::ZxToAscii(LPCSTR zx,BYTE zxLength,PTCHAR bufT){
		// converts text from Spectrum character set to PC's current character set and returns the result in Buffer
		bufT[0]=' '; // initialization
		PTCHAR t=1+bufT;
		for( LPCSTR m=zx; const BYTE z=*m; m++ )
			if (!(zxLength--))
				break;
			else if (z==96)
				*t++=163; // Pound sign, £
			else if (z<=126)
				*t++=z; // the same as ASCII up to character 126 (0x7e)
			else if (z==127)
				*t++=169; // copyright sign, ©
			else if (z<=143)
				*t++=z; // UDG graphics
			else if (z<=164)
				*t++=z-79; // UDG characters 'A'-'U'
			else{
				const LPCSTR K=Keywords[z-KEYWORD_TOKEN_FIRST];
				if (*(t-1)==' ' && *K==' ')
					::lstrcpy( t, 1+K ); // two consecutive spaces are not printed
				else
					::lstrcpy( t, K );
				t+=::lstrlen(t);
			}
		*t='\0';
		return 1+bufT; // "1+" = see initialization above
	}

	PTCHAR CSpectrumDos::TZxRom::AsciiToZx(LPCTSTR pc,PCHAR zx,PBYTE pOutZxLength){
		// converts text from PC's current character set to Spectrum character set and returns the result in Buffer
		PCHAR buf=zx;
		for( bool prevCharIsSpace=true; const TCHAR c=*pc; pc++ )
			if (c==-93) // Pound sign, £
				*buf++=96, prevCharIsSpace=false;
			else if (c==-87) // copyright sign, ©
				*buf++=127, prevCharIsSpace=false;
			else{
				BYTE token=KEYWORD_TOKEN_FIRST;
				do{
					LPCSTR K=Keywords[token-KEYWORD_TOKEN_FIRST];
					K += *K==' '&&prevCharIsSpace; // skipping Keyword's initial space if the PreviousCharacter was a space
					BYTE N=::lstrlenA(K);
					N -= K[N-1]==' '&&pc[N-1]=='\0'; // skipping Keyword's trailing space should the match be found at the end of the PC text
					if (!::strncmp(pc,K,N)){
						pc+=N-1; // "-1" = see "pc++" in the For cycle
						prevCharIsSpace=K[N-1]==' ';
						break;
					}
				}while (++token);
				if (token) // a Keyword with given Token found in the input PC text
					*buf++=token;
				else // no Keyword match found in the input PC text
					*buf++=c, prevCharIsSpace=false;
			}
		*buf='\0';
		if (pOutZxLength)
			*pOutZxLength=buf-zx;
		return zx;
	}

	bool CSpectrumDos::TZxRom::IsStdUdgSymbol(BYTE s){
		// True <=> given Character is a standard UDG symbol, otherwise False
		return 127<s && s<144;
	}

	LPCSTR CSpectrumDos::TZxRom::GetKeywordTranscript(BYTE k){
		// returns the textual representation of the given Keyword, or Null if the character is not a Keyword character
		return	k>=KEYWORD_TOKEN_FIRST
				? Keywords[k-KEYWORD_TOKEN_FIRST]
				: NULL;
	}

	void CSpectrumDos::TZxRom::PrintAt(HDC dc,LPCTSTR buf,RECT r,UINT drawTextFormat) const{
		// prints text in Buffer inside the given Rectangle
		BYTE n=::lstrlen(buf);
		if (drawTextFormat&DT_RIGHT){
			drawTextFormat&=~DT_RIGHT;
			r.left=r.right-n*font.charAvgWidth;
		}
		while (n--){
			WORD c=(BYTE)*buf++; // cannot use TCHAR because must use non-signed type
			if (!IsStdUdgSymbol(c))
				// directly printable character
				::DrawText( dc, (LPCTSTR)&c,1, &r, drawTextFormat );
			else{
				// UDG character - composed from four 0x2588 characters printed in half scale
				const int mapMode0=::SetMapMode(dc,MM_ANISOTROPIC);
					// . printing in half scale (MM_ISOTROPIC)
					::SetWindowExtEx(dc,200,200,NULL);
					::SetViewportExtEx(dc,100,100,NULL);
					// . printing UDG
					static const WCHAR w=0x2588;
					c-=128; // lower four bits encode which "quadrants" of UDG character are shown (e.g. the UDG character of "L" shape = 2+4+8 = 14)
					const int L=r.left*2, T=r.top*2, L2=L+font.charAvgWidth, T2=T+font.charHeight-2;
					if (c&1) // upper right quadrant
						::TextOutW(dc,L2,T,&w,1);
					if (c&2) // upper left quadrant
						::TextOutW(dc,L,T,&w,1);
					if (c&4) // lower right quadrant
						::TextOutW(dc,L2,T2,&w,1);
					if (c&8) // lower left quadrant
						::TextOutW(dc,L,T2,&w,1);
				::SetMapMode(dc,mapMode0);
			}
			r.left+=font.charAvgWidth;
		}
	}








	CSpectrumDos::UStdParameters::UStdParameters()
		// ctor
		: dw(0x80000000) {
	}









	void CSpectrumDos::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		TUtils::InformationWithCheckableShowNoMore( text, INI_SPECTRUM, messageId );
	}

	void CSpectrumDos::__parseFat32LongName__(PTCHAR buf,LPCTSTR &rOutName,BYTE nameLengthMax,LPCTSTR &rOutExt,BYTE extLengthMax,LPCTSTR &rOutZxInfo){
		// parses input FAT long name into three components: ZX Name (of LengthMax chars at most), single-char ZX Extension, and ZX Information
		// - finding ZX import information
		rOutZxInfo=NULL; // assumption (no ZX import information found)
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
		::UrlUnescape(	TZxRom::AsciiToZx(buf,buf,NULL), // converting in place to ZX charset
						NULL, &dw,
						URL_UNESCAPE_INPLACE // unescaping in place
					);
		if (::lstrlen(buf)>nameLengthMax) // Name potentially too long, trimming it
			buf[nameLengthMax]='\0';
		// - unescaping and trimming the Extension
		if (pExt){
			dw=extLengthMax;
			::UrlUnescape(	TZxRom::AsciiToZx(buf,buf,NULL), // converting in place to ZX charset
							NULL, &dw,
							URL_UNESCAPE_INPLACE // unescaping in place
						);
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










	CSpectrumDos::CSpectrumDos(PImage image,PCFormat pFormatBoot,TTrackScheme trackAccessScheme,PCProperties properties,UINT nResId,CSpectrumFileManagerView *pFileManager)
		// ctor
		: CDos(image,pFormatBoot,trackAccessScheme,properties,::lstrcmp,sideMap,nResId,pFileManager)
		, pSingleTape(NULL) , trackMap(this) {
		::memcpy( sideMap, StdSidesMap, sizeof(sideMap) ); // mapping Head numbers to Side numbers as the IBM norm dictates
	}

	CSpectrumDos::~CSpectrumDos(){
		// dtor
		if (pSingleTape)
			delete pSingleTape;
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
				if (pSingleTape)
					if (pSingleTape->ProcessCommand(ID_FILE_CLOSE)==TCmdResult::REFUSED) // if closing an open Tape rejected ...
						return TCmdResult::DONE; // ... the main disk Image cannot be closed neither
					else{ // ... otherwise closing the open Tape
						delete pSingleTape, pSingleTape=NULL;
						if (CScreenPreview::pSingleInstance && CScreenPreview::pSingleInstance->rFileManager.DOS==pSingleTape)
							CScreenPreview::pSingleInstance->DestroyWindow();
						if (CBasicPreview::pSingleInstance && &CBasicPreview::pSingleInstance->rFileManager==pFileManager)
							CBasicPreview::pSingleInstance->DestroyWindow();
						return TCmdResult::DONE_REDRAW; // only the Tape has been closed, not the main disk Image!
					}
				break; // closing the main disk Image
			case ID_TAPE_NEW:{
				// creating the underlying Tape file on local disk
				TCHAR fileName[MAX_PATH];
				*fileName='\0';
				CString title;
					title.LoadString(AFX_IDS_SAVEFILE);
				CFileDialog d( FALSE, TAPE_EXTENSION, NULL, OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY|OFN_DONTADDTORECENT, _T("Tape (*") TAPE_EXTENSION _T(")|*") TAPE_EXTENSION _T("|") );
					d.m_ofn.lStructSize=sizeof(OPENFILENAME); // to show the "Places bar"
					d.m_ofn.nFilterIndex=1;
					d.m_ofn.lpstrTitle=title;
					d.m_ofn.lpstrFile=fileName;
				if (d.DoModal()==IDOK){
					// . ejecting current Tape (if any)
					if (pSingleTape)
						if (ProcessCommand(ID_TAPE_CLOSE)!=TCmdResult::DONE_REDRAW) // if Tape not ejected ...
							return TCmdResult::DONE; // ... we are done (successfully)
					// . inserting a blank Tape (by creating a new underlying physical file and opening it)
					CFile( fileName, CFile::modeCreate|CFile::shareDenyRead|CFile::typeBinary ).Close(); // creating the underlying file on local disk
					( pSingleTape=new CTape(fileName,this) )->__toggleWriteProtection__(); // new Tape is not WriteProtected
					return TCmdResult::DONE;
				}else
					return TCmdResult::DONE_REDRAW;
			}
			case ID_TAPE_OPEN:{
				// opening an existing file with Tape
				TCHAR fileName[MAX_PATH];
				*fileName='\0';
				CString title;
					title.LoadString(AFX_IDS_OPENFILE);
				CFileDialog d( TRUE, TAPE_EXTENSION, NULL, OFN_FILEMUSTEXIST, _T("Tape (*") TAPE_EXTENSION _T(")|*") TAPE_EXTENSION _T("|") );
					d.m_ofn.lStructSize=sizeof(OPENFILENAME); // to show the "Places bar"
					d.m_ofn.nFilterIndex=1;
					d.m_ofn.lpstrTitle=title;
					d.m_ofn.lpstrFile=fileName;
				if (d.DoModal()==IDOK){
					// . ejecting current Tape (if any)
					if (pSingleTape)
						if (ProcessCommand(ID_TAPE_CLOSE)!=TCmdResult::DONE_REDRAW) // if Tape not ejected ...
							return TCmdResult::DONE; // ... we are done (successfully)
					// . inserting a recorded Tape (by opening its underlying physical file)
					pSingleTape=new CTape(fileName,this); // inserted Tape is WriteProtected by default
					return TCmdResult::DONE;
				}else
					return TCmdResult::DONE_REDRAW;
			}
			case ID_ZX_PREVIEWASSCREEN:
				// previewing File(s) on Spectrum screen
				if (CScreenPreview::pSingleInstance)
					CScreenPreview::pSingleInstance->DestroyWindow();
				new CScreenPreview(	__isTapeFileManagerShown__()
									? pSingleTape->fileManager
									: *pFileManager
								);
				return TCmdResult::DONE;
			case ID_ZX_PREVIEWASBASIC:
				// previewing File(s) as BASIC program(s)
				if (CBasicPreview::pSingleInstance)
					CBasicPreview::pSingleInstance->DestroyWindow();
				new CBasicPreview(	__isTapeFileManagerShown__()
									? pSingleTape->fileManager
									: *pFileManager
								);
				return TCmdResult::DONE;
			default:
				// passing a non-recognized Command to an open Tape first
				if (__isTapeFileManagerShown__() && pSingleTape->OnCmdMsg(cmd,CN_COMMAND,NULL,NULL))
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
		}else
			// exporting to another RIDE instance; substituting non-alphanumeric characters with "URL-like" escape sequences
			return __super::GetFileExportNameAndExt(file,shellCompliant,buf);
	}

	DWORD CSpectrumDos::GetAttributes(PCFile file) const{
		// maps File's attributes to Windows attributes and returns the result
		return 0; // none but standard attributes
	}

	bool CSpectrumDos::UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const{
		// True <=> given Command-specific user interface successfully updated, otherwise False
		switch (cmd){
			case ID_TAPE_CLOSE:
				pCmdUI->Enable(pSingleTape!=NULL);
				return true;
			default:
				if (__isTapeFileManagerShown__() && pSingleTape->OnCmdMsg(cmd,CN_UPDATE_COMMAND_UI,pCmdUI,NULL))
					return true;
				break;
		}
		return CDos::UpdateCommandUi(cmd,pCmdUI);
	}

	bool CSpectrumDos::__isTapeFileManagerShown__() const{
		// True <=> Tape's FileManager is currently shown in the TDI, otherwise False
		return pSingleTape && pSingleTape->fileManager.m_hWnd; // A&B, A = Tape inserted, B = Tape's FileManager currently switched to
	}

	bool CSpectrumDos::CanBeShutDown(CFrameWnd* pFrame) const{
		// True <=> this DOS has no dependecies which would require it to remain active, otherwise False (has some dependecies which require the DOS to remain active)
		// - first attempting to close the Tape
		if (pSingleTape)
			if (!pSingleTape->CanCloseFrame(pFrame))
				return FALSE;
		// - base
		return CDos::CanBeShutDown(pFrame);
	}
