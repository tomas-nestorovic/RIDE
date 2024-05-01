#include "stdafx.h"

	const RGBQUAD CSpectrumBase::Colors[16]={
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

	DWORD CSpectrumBase::idHeaderless=1;










	const CSpectrumBase::TStdParameters CSpectrumBase::TStdParameters::Default={ 0, 0x8000 };








	CSpectrumBase::TSpectrumVerificationParams::TSpectrumVerificationParams(CSpectrumBase *dos,const TVerificationFunctions &rvf)
		// ctor
		: CVerifyVolumeDialog::TParams(dos,rvf) {
	}

	static CString getMessageIfSomeCharsNonPrintable(LPCTSTR locationName,LPCTSTR valueName,PCHAR zx,BYTE zxLength,char paddingChar){
		// composes and returns a message describing a problem that some non-printable characters are contained in specified ZX buffer
		// - composing a list of unique Nonprintable ZX characters
		char nonprintables[128], nNonprintables=0;
		for( BYTE i=0; i<zxLength; i++ ){
			const char c=zx[i];
			if (!CSpectrumBase::TZxRom::IsPrintable(c) && c!=paddingChar)
				if (!::memchr(nonprintables,c,nNonprintables))
					nonprintables[nNonprintables++]=c;
		}
		// - composing the Message
		CString msg;
		if (nNonprintables)
			msg.Format(	_T("The \"%s\" field %s%s contains non-printable character%c %s"),
						valueName,
						locationName ? _T("in the ") : _T(""),
						locationName ? locationName : _T(""),
						nNonprintables>1 ? 's' : ' ',
						(LPCTSTR)Utils::BytesToHexaText( nonprintables, nNonprintables, true )
					);
		return msg;
	}

	TStdWinError CSpectrumBase::TSpectrumVerificationParams::VerifyAllCharactersPrintable(RCPhysicalAddress chs,LPCTSTR chsName,LPCTSTR valueName,PCHAR zx,BYTE zxLength,char paddingChar) const{
		// confirms a ZX text field contains only printable characters; if not, presents the problem using standard formulation, and returns Windows standard i/o error
		// - if no Nonprintable characters found, we are successfully done
		const CString msg=getMessageIfSomeCharsNonPrintable( chsName, valueName, zx, zxLength, paddingChar );
		if (!msg.GetLength())
			return ERROR_SUCCESS;
		// - composing and presenting the problem
		switch (ConfirmFix( msg, _T("Their removal is suggested.") )){
			case IDYES:{
				// removing all non-printable characters from the pointed-to buffer
				PCHAR pPrintable=zx;
				for( BYTE i=0; i<zxLength; i++ )
					if (TZxRom::IsPrintable(zx[i]))
						*pPrintable++=zx[i];
				::memset( pPrintable, paddingChar, zxLength-(pPrintable-zx) );
				dos->image->MarkSectorAsDirty(chs);
				fReport.CloseProblem(true);
				//fallthrough
			}
			case IDNO:
				return ERROR_SUCCESS; // even if fix rejected, this value has been verified
			default:
				return ERROR_CANCELLED;
		}
	}

	bool CSpectrumBase::TSpectrumVerificationParams::WarnSomeCharactersNonPrintable(LPCTSTR locationName,LPCTSTR valueName,PCHAR zx,BYTE zxLength,char paddingChar) const{
		// logs a warning if some characters in the ZX text field contains non-printable characters, listing them; returns Windows standard i/o error
		// - if no Nonprintable characters found, we are successfully done
		const CString msg=getMessageIfSomeCharsNonPrintable( locationName, valueName, zx, zxLength, paddingChar );
		if (!msg.GetLength())
			return false;
		// - composing and logging the problem
		fReport.LogWarning(msg);
		switch (repairStyle){
			default:
				ASSERT(FALSE);
			case 0:
				// automatic fixing of each Problem
				break;
			case 1:
				// fixing only manually confirmed Problems
				Utils::Information(msg);
				//fallthrough
			case 2:
				// automatic rejection of fix to any Problem
				break;
		}
		return true;
	}








	void CSpectrumBase::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_SPECTRUM, messageId );
	}

	CString CSpectrumBase::ParseFat32LongName(RCPathString shellName,RPathString rOutName,RPathString rOutExt){
		// parses input FAT long name into three components: ZX Name (of LengthMax chars at most), single-char ZX Extension, and ZX Information (returned)
		CPathString buf=shellName.Clone();
		// - finding ZX import information
		CString zxInfo;
		if (PTCHAR pSpace=buf.FindLast(' ')) // string may be terminated with import information, see CSpectrumDos::__importFileInformation__
			if (pSpace[1]=='Z' && pSpace[2]=='X'){ // ZX import information must be correctly prefixed
				*pSpace++='\0'; // terminating the File's Name+Extension
				zxInfo=pSpace;
			}
		// - finding and trimming the Extension
		char zx[1024]; // should suffice for any filename of any Spectrum DOS
		buf=TZxRom::AsciiToZx( buf.GetAnsi(), zx, nullptr );
		rOutExt=buf.DetachExtension().Unescape();
		rOutName=buf.Unescape();
		return zxInfo;
	}

	#define INFO_UNI	_T(" ZX%c")
	#define INFO_STD	_T("%xL%x")
	#define INFO_FLAG	_T("G%u")

	int CSpectrumBase::__exportFileInformation__(PTCHAR buf,TUniFileType uniFileType){
		// populates the Buffer with File export information in normalized form and returns the number of characters written to the Buffer
		return _stprintf( buf, INFO_UNI, uniFileType );
	}

	int CSpectrumBase::__exportFileInformation__(PTCHAR buf,TUniFileType uniFileType,TStdParameters params,DWORD fileLength){
		// populates the Buffer with File export information in normalized form and returns the number of characters written to the Buffer
		const int N=__exportFileInformation__( buf, uniFileType );
		return N + _stprintf( buf+N, INFO_STD, params, fileLength );
	}

	int CSpectrumBase::__exportFileInformation__(PTCHAR buf,TUniFileType uniFileType,TStdParameters params,DWORD fileLength,BYTE dataFlag){
		// populates the Buffer with File export information in normalized form and returns the number of characters written to the Buffer
		const int N=__exportFileInformation__( buf, uniFileType, params, fileLength );
		return N + _stprintf( buf+N, INFO_FLAG, dataFlag );
	}

	int CSpectrumBase::__importFileInformation__(LPCTSTR buf,TUniFileType &rUniFileType){
		// returns the number of characters recognized as import information normalized form (supplied by ExportFileInformation)
		rUniFileType=TUniFileType::UNKNOWN; // initialization
		if (buf && *buf){ // Null if File has no import information
			int n=0;
			if (_stscanf( buf, INFO_UNI _T("%n"), &rUniFileType, &n )>0)
				return n;
		}
		return 0;
	}

	int CSpectrumBase::__importFileInformation__(LPCTSTR buf,TUniFileType &rUniFileType,TStdParameters &rParams,DWORD &rFileLength){
		// returns the number of characters recognized as import information normalized form (supplied by ExportFileInformation)
		rParams=TStdParameters::Default, rFileLength=0; // initialization
		if (const int N=__importFileInformation__( buf, rUniFileType )){ // Null if File has no import information
			int n=0;
			if (_stscanf( buf+N, INFO_STD _T("%n"), &rParams, &rFileLength, &n )==2)
				return N+n;
		}
		return 0;
	}

	int CSpectrumBase::__importFileInformation__(LPCTSTR buf,TUniFileType &rUniFileType,TStdParameters &rParams,DWORD &rFileLength,BYTE &rDataFlag){
		// returns the number of characters recognized as import information normalized form (supplied by ExportFileInformation)
		rDataFlag=TZxRom::TStdBlockFlag::DATA; // assumption (block featuring Header has been saved using standard routine in ROM)
		if (const int N=__importFileInformation__( buf, rUniFileType, rParams, rFileLength )){ // Null if File has no import information
			int n=0,tmp=rDataFlag;
			if (_stscanf( buf+N, INFO_FLAG _T("%n"), &tmp, &n )>0){
				rDataFlag=tmp;
				return N+n;
			}
		}
		return 0;
	}








	CSpectrumBase::CSpectrumBase(PImage image,PCFormat pFormatBoot,TTrackScheme trackAccessScheme,PCProperties properties,UINT nResId,CSpectrumBaseFileManagerView *pFileManager,TGetFileSizeOptions _getFileSizeDefaultOption,TSectorStatus unformatFatStatus)
		// ctor
		: CDos(image,pFormatBoot,trackAccessScheme,properties,::StrCmpNW,sideMap,nResId,pFileManager,_getFileSizeDefaultOption,unformatFatStatus) {
		::memcpy( sideMap, StdSidesMap, sizeof(sideMap) ); // mapping Head numbers to Side numbers as the IBM norm dictates
	}

	CSpectrumBase::~CSpectrumBase(){
		// dtor
		if (CScreenPreview::pSingleInstance && &CScreenPreview::pSingleInstance->rFileManager==pFileManager)
			CScreenPreview::pSingleInstance->DestroyWindow();
		if (CBasicPreview::pSingleInstance && &CBasicPreview::pSingleInstance->rFileManager==pFileManager)
			CBasicPreview::pSingleInstance->DestroyWindow();
		if (CAssemblerPreview::pSingleInstance && &CAssemblerPreview::pSingleInstance->rFileManager==pFileManager)
			CAssemblerPreview::pSingleInstance->DestroyWindow();
	}









	CDos::CPathString CSpectrumBase::GetFilePresentationNameAndExt(PCFile file) const{
		// returns File name concatenated with File extension for presentation of the File to the user
		CPathString name,ext;
		GetFileNameOrExt( file, &name, &ext );
		name=TZxRom::ZxToAscii( name.GetAnsi() );
		ext=TZxRom::ZxToAscii( ext.GetAnsi() );
		name.AppendDotExtensionIfAny( ext ).Escape();
		CPathString semigraphics;
		const CString ansi=name.GetAnsi();
		for( LPCSTR p=ansi; char c=*p++; )
			if (TZxRom::IsStdUdgSymbol(c)){
				static constexpr WCHAR Unicode[]={
					L'\x2002', // en space
					L'\x259d', L'\x2598', L'\x2580', L'\x2597', L'\x2590', L'\x259a', L'\x259c',
					L'\x2596', L'\x259e', L'\x258c', L'\x259b', L'\x2584', L'\x259f', L'\x2599',
					L'\x2588'
				};
				semigraphics.Append( Unicode[c&0xf] ); // lower four bits encode which "quadrants" of UDG character are shown (e.g. the UDG character of "L" shape = 2+4+8 = 14)
			}else
				semigraphics.Append( (BYTE)c );
		return semigraphics;
	}

	CDos::CPathString CSpectrumBase::GetFileExportNameAndExt(PCFile file,bool shellCompliant) const{
		// returns File name concatenated with File extension for export of the File to another Windows application (e.g. Explorer)
		if (shellCompliant){
			// exporting to non-RIDE target (e.g. to the Explorer); excluding from the Buffer characters that are forbidden in FAT32 long file names
			CPathString fileName,fileExt;
			GetFileNameOrExt( file, &fileName, &fileExt );
			fileName.ExcludeFat32LongNameInvalidChars(), fileExt.ExcludeFat32LongNameInvalidChars();
			CPathString zxName=TZxRom::ZxToAscii( fileName.GetAnsi() );
			if (zxName.GetLengthW())
				// valid export name - taking it as the result
				return zxName.AppendDotExtensionIfAny(
					TZxRom::ZxToAscii( fileExt.GetAnsi() )
				);
			else
				// invalid export name - generating an artifical one
				return __super::GetFileExportNameAndExt(file,shellCompliant);
		}else
			// exporting to another RIDE instance; substituting non-alphanumeric characters with "URL-like" escape sequences
			return __super::GetFileExportNameAndExt(file,shellCompliant);
	}

	DWORD CSpectrumBase::GetAttributes(PCFile file) const{
		// maps File's attributes to Windows attributes and returns the result
		return	file!=ZX_DIR_ROOT
				? 0 // none but standard attributes
				: FILE_ATTRIBUTE_DIRECTORY; // root Directory
	}

	CDos::TCmdResult CSpectrumBase::ProcessCommand(WORD cmd){
		// returns the Result of processing a DOS-related command
		switch (cmd){
			case ID_ZX_PREVIEWASSCREEN:
				// previewing File(s) on Spectrum screen
				new CScreenPreview(	CFileManagerView::pCurrentlyShown ? *CFileManagerView::pCurrentlyShown : *CDos::GetFocused()->pFileManager );
				return TCmdResult::DONE;
			case ID_ZX_PREVIEWASBASIC:
				// previewing File(s) as BASIC program(s)
				if (CBasicPreview::pSingleInstance)
					CBasicPreview::pSingleInstance->DestroyWindow();
				new CBasicPreview( CFileManagerView::pCurrentlyShown ? *CFileManagerView::pCurrentlyShown : *CDos::GetFocused()->pFileManager );
				return TCmdResult::DONE;
			case ID_ZX_PREVIEWASASSEMBLER:
				// previewing File as Z80 assembler
				if (CAssemblerPreview::pSingleInstance)
					CAssemblerPreview::pSingleInstance->DestroyWindow();
				CAssemblerPreview::CreateInstance( CFileManagerView::pCurrentlyShown ? *CFileManagerView::pCurrentlyShown : *CDos::GetFocused()->pFileManager );
				return TCmdResult::DONE;
		}
		return __super::ProcessCommand(cmd);
	}
