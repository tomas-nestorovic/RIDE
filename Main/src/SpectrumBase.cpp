#include "stdafx.h"

	#define INI_SPECTRUM	_T("ZXSpectrum")

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








	void CSpectrumBase::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_SPECTRUM, messageId );
	}

	void CSpectrumBase::__parseFat32LongName__(PTCHAR buf,LPCTSTR &rOutName,BYTE nameLengthMax,LPCTSTR &rOutExt,BYTE extLengthMax,LPCTSTR &rOutZxInfo){
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
		if (buf){ // Null if File has no import information
			int n=0;
			if (_stscanf( buf, INFO_UNI _T("%n"), &rUniFileType, &n ))
				return n;
		}
		return 0;
	}

	int CSpectrumBase::__importFileInformation__(LPCTSTR buf,TUniFileType &rUniFileType,TStdParameters &rParams,DWORD &rFileLength){
		// returns the number of characters recognized as import information normalized form (supplied by ExportFileInformation)
		rParams=TStdParameters::Default, rFileLength=0; // initialization
		if (buf){ // Null if File has no import information
			const int N=__importFileInformation__( buf, rUniFileType );
			int n=0;
			_stscanf( buf+N, INFO_STD _T("%n"), &rParams, &rFileLength, &n );
			return N+n;
		}
		return 0;
	}

	int CSpectrumBase::__importFileInformation__(LPCTSTR buf,TUniFileType &rUniFileType,TStdParameters &rParams,DWORD &rFileLength,BYTE &rDataFlag){
		// returns the number of characters recognized as import information normalized form (supplied by ExportFileInformation)
		rDataFlag=TZxRom::TStdBlockFlag::DATA; // assumption (block featuring Header has been saved using standard routine in ROM)
		if (buf){ // Null if File has no import information
			const int N=__importFileInformation__( buf, rUniFileType, rParams, rFileLength );
			int n=0,tmp=rDataFlag;
			_stscanf( buf+N, INFO_FLAG _T("%n"), &tmp, &n );
			rDataFlag=tmp;
			return N+n;
		}
		return 0;
	}








	CSpectrumBase::CSpectrumBase(PImage image,PCFormat pFormatBoot,TTrackScheme trackAccessScheme,PCProperties properties,UINT nResId,CSpectrumBaseFileManagerView *pFileManager,TGetFileSizeOptions _getFileSizeDefaultOption)
		// ctor
		: CDos(image,pFormatBoot,trackAccessScheme,properties,::lstrcmp,sideMap,nResId,pFileManager,_getFileSizeDefaultOption) {
		::memcpy( sideMap, StdSidesMap, sizeof(sideMap) ); // mapping Head numbers to Side numbers as the IBM norm dictates
	}

	CSpectrumBase::~CSpectrumBase(){
		// dtor
		if (CScreenPreview::pSingleInstance && &CScreenPreview::pSingleInstance->rFileManager==pFileManager)
			CScreenPreview::pSingleInstance->DestroyWindow();
		if (CBasicPreview::pSingleInstance && &CBasicPreview::pSingleInstance->rFileManager==pFileManager)
			CBasicPreview::pSingleInstance->DestroyWindow();
	}









	PTCHAR CSpectrumBase::GetFileExportNameAndExt(PCFile file,bool shellCompliant,PTCHAR buf) const{
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
				if (CScreenPreview::pSingleInstance)
					CScreenPreview::pSingleInstance->DestroyWindow();
				new CScreenPreview(	*CFileManagerView::pCurrentlyShown );
				return TCmdResult::DONE;
			case ID_ZX_PREVIEWASBASIC:
				// previewing File(s) as BASIC program(s)
				if (CBasicPreview::pSingleInstance)
					CBasicPreview::pSingleInstance->DestroyWindow();
				new CBasicPreview( *CFileManagerView::pCurrentlyShown );
				return TCmdResult::DONE;
		}
		return __super::ProcessCommand(cmd);
	}
