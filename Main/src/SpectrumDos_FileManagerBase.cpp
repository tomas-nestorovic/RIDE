#include "stdafx.h"

	#define INI_MSG_PARAMS	_T("params")

	CSpectrumDos::CSpectrumFileManagerView::CSpectrumFileManagerView(PDos dos,const TZxRom &rZxRom,BYTE supportedDisplayModes,BYTE initialDisplayMode,BYTE nInformation,PCFileInfo informationList)
		// ctor
		: CFileManagerView( dos, supportedDisplayModes, initialDisplayMode, rZxRom.font, 3, nInformation, informationList, 0, NULL )
		, zxRom(rZxRom) {
	}









	#define DOS tab.dos

	PTCHAR CSpectrumDos::CSpectrumFileManagerView::GenerateExportNameAndExtOfNextFileCopy(CDos::PCFile file,bool shellCompliant,PTCHAR pOutBuffer) const{
		// returns the Buffer populated with the export name and extension of the next File's copy in current Directory; returns Null if no further name and extension can be generated
		BYTE tmpDirEntry[2048]; // "big enough" to accommodate any ZX Spectrum DirectoryEntry
		const PDirectoryTraversal pdt=DOS->BeginDirectoryTraversal();
			::memcpy( tmpDirEntry, file, pdt->entrySize );
			const BYTE nameCharsMax=pdt->nameCharsMax;
		DOS->EndDirectoryTraversal(pdt);
		for( BYTE copyNumber=1; copyNumber; copyNumber++ ){
			// . composing the Name for the File copy
			TCHAR bufNameCopy[MAX_PATH], bufExt[MAX_PATH];
			DOS->GetFileNameAndExt(	file,bufNameCopy, bufExt );
			TCHAR postfix[8];
			const BYTE n=::wsprintf(postfix,_T("%c%d"),255,copyNumber); // 255 = token of the "COPY" keyword
			if (::lstrlen(::lstrcat(bufNameCopy,postfix))>nameCharsMax)
				::lstrcpy( &bufNameCopy[nameCharsMax-n], postfix ); // trimming to maximum number of characters
			// . attempting to rename the TemporaryDirectoryEntry
			CDos::PFile fExisting;
			if (DOS->ChangeFileNameAndExt( &tmpDirEntry, bufNameCopy, bufExt, fExisting )==ERROR_SUCCESS)
				// generated a unique Name for the next File copy - returning the final export name and extension
				return DOS->GetFileExportNameAndExt( &tmpDirEntry, shellCompliant, pOutBuffer );
		}
		return NULL; // the Name for the next File copy cannot be generated
	}

	#define EXTENSION_MIN	32
	#define EXTENSION_MAX	127

	bool WINAPI CSpectrumDos::CSpectrumFileManagerView::CSingleCharExtensionEditor::__onChanged__(PVOID file,CPropGridCtrl::TEnum::UValue newExt){
		// changes the single-character Extension of given File
		const PDos dos=CDos::__getFocused__();
		// - getting File's original Name and Extension
		TCHAR bufOldName[MAX_PATH];
		dos->GetFileNameAndExt(file,bufOldName,NULL);
		const TCHAR bufNewExt[]={ newExt.charValue, '\0' };
		// - validating File's new Name and Extension
		const TStdWinError err=dos->ChangeFileNameAndExt(file,bufOldName,bufNewExt,file);
		if (err==ERROR_SUCCESS) // the OldName+NewExtension combination is unique
			return true;
		else{	// at least two Files with the same OldName+NewExtension combination exist
			Utils::Information(FILE_MANAGER_ERROR_RENAMING,err);
			return false;
		}
	}
	static CPropGridCtrl::TEnum::PCValueList WINAPI __createValues__(PVOID file,WORD &rnValues){
		// creates and returns the list of File's possible Extensions
		const PBYTE list=(PBYTE)::malloc( rnValues=EXTENSION_MAX+1-EXTENSION_MIN );
		for( BYTE p=EXTENSION_MIN,*a=list; p<=EXTENSION_MAX; *a++=p++ );
		return list;
	}
	static void WINAPI __freeValues__(PVOID file,CPropGridCtrl::TEnum::PCValueList list){
		// disposes the list of File's possible Extensions
		::free((PVOID)list);
	}
	LPCTSTR WINAPI CSpectrumDos::CSpectrumFileManagerView::CSingleCharExtensionEditor::__getDescription__(PVOID file,CPropGridCtrl::TEnum::UValue extension,PTCHAR buf,short bufCapacity){
		// sets the Buffer to textual description of given Extension and returns its beginning in the Buffer
		const char bufM[2]={ extension.charValue, '\0' };
		return TZxRom::ZxToAscii(bufM,1,buf);
	}
	CFileManagerView::PEditorBase CSpectrumDos::CSpectrumFileManagerView::CSingleCharExtensionEditor::Create(PFile file){
		// creates and returns an Editor of File's single-character Extension
		const PDos dos=CDos::__getFocused__();
		const CSpectrumFileManagerView *const pZxFileManager=(CSpectrumFileManagerView *)dos->pFileManager;
		TCHAR bufExt[2];
		dos->GetFileNameAndExt(file,NULL,bufExt);
		const PEditorBase result=pZxFileManager->__createStdEditor__(
			file, &( data=*bufExt ), sizeof(data),
			CPropGridCtrl::TEnum::DefineConstStringListEditorA( __createValues__, __getDescription__, __freeValues__, __onChanged__ )
		);
		::SendMessage( result->hEditor, WM_SETFONT, (WPARAM)pZxFileManager->rFont.m_hObject, 0 );
		return result;
	}











	bool WINAPI CSpectrumDos::CSpectrumFileManagerView::CVarLengthFileNameEditor::__help__(PVOID,PVOID,short){
		// help
		Utils::Information(_T("You can type in all Spectrum characters, including commands (if in modes K, or E), letters (mode L), capitals (mode C), and UDG symbols (mode G). In each mode, type characters as you would on a classical 48k Spectrum keyboard. Non-printable characters are not supported and cannot be typed in (e.g. those influencing text color).\n\nSwitch between modes using Ctrl+Shift. Use Ctrl alone as the Symbol Shift key. You enter the C mode if CapsLock is on during L mode.\n\nExample:\nSwitch to mode E and press Z - \"LN\" shows up.\nSwitch to mode E again and press Ctrl+Z - \"BEEP\" appears this time."));
		return false; // False = actual editing of value has failed (otherwise the Editor would be closed)
	}

	void CSpectrumDos::CSpectrumFileManagerView::CVarLengthFileNameEditor::__addChar__(char c){
		// adds given Character at Cursor's current Position
		if (length==lengthMax) return; // can't exceed the maximum length
		::memmove( buf+cursor.position+1, buf+cursor.position, length-cursor.position );
		buf[cursor.position++]=c;
		length++;
		//cursor.mod=TCursor::L;
		CEditorBase::pSingleShown->Repaint();
	}

	#define CURSOR_PLACEHOLDER	2 /* dummy value to be replaced with a real Cursor */

	#define IS_CAPSLOCK_ON()	((::GetKeyState(VK_CAPITAL)&1)>0)

	LRESULT CALLBACK CSpectrumDos::CSpectrumFileManagerView::CVarLengthFileNameEditor::__wndProc__(HWND hEditor,UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		const PDos dos=CDos::__getFocused__();
		const CSpectrumFileManagerView *const pZxFileManager=(CSpectrumFileManagerView *)dos->pFileManager;
		CVarLengthFileNameEditor &rEditor=pZxFileManager->varLengthFileNameEditor;
		switch (msg){
			case WM_PAINT:{
				// drawing
				PAINTSTRUCT ps;
				const HDC dc=::BeginPaint(hEditor,&ps);
					::SetBkMode(dc,TRANSPARENT);
					const TZxRom &zxRom=pZxFileManager->zxRom;
					const HGDIOBJ hFont0=::SelectObject(dc,zxRom.font.m_hObject);
						const BYTE c=rEditor.cursor.position;
						RECT r;
						::GetClientRect(hEditor,&r);
						char bufM[MAX_PATH];
							::memcpy( bufM+1, rEditor.buf, rEditor.lengthMax );
							::memmove( bufM, bufM+1, c );
							bufM[c]=CURSOR_PLACEHOLDER; // placeholder of Cursor (drawn below)
						TCHAR bufT[MAX_PATH];
						zxRom.PrintAt(	dc,
										TZxRom::ZxToAscii( bufM,rEditor.length+1, bufT ), // "+1" = Cursor
										r,
										DT_SINGLELINE | DT_LEFT | DT_VCENTER
									);
						r.right=( r.left=(_tcschr(bufT,CURSOR_PLACEHOLDER)-bufT-1)*zxRom.font.charAvgWidth )+zxRom.font.charAvgWidth;
						r.bottom=( r.top=(r.bottom-zxRom.font.charHeight)/2 )+zxRom.font.charHeight;
						::FillRect( dc, &r, CRideBrush::Black );
						::SetTextColor( dc, 0xffffff );
						if (rEditor.cursor.mode==TCursor::LC && IS_CAPSLOCK_ON()){
							// displaying the "C" Mode (Capitals) at place of the "L" mode
							static const char ModeC='C';
							::DrawTextA( dc, &ModeC,1, &r, DT_SINGLELINE|DT_LEFT|DT_VCENTER );
						}else
							// displaying current Mode
							::DrawTextA( dc, (LPCSTR)&rEditor.cursor.mode,1, &r, DT_SINGLELINE|DT_LEFT|DT_VCENTER );
					::SelectObject(dc,hFont0);
				::EndPaint(hEditor,&ps);
				return 0;
			}
			case WM_KEYDOWN:{
				// key pressed
				switch (wParam){
					case VK_TAB:
					case VK_ESCAPE:
					case VK_RETURN:
						// control keys of all Editors
						break;
					case VK_LEFT:
						// moving Cursor one Position to the left
						if (rEditor.cursor.position){
							rEditor.cursor.position--;
							::InvalidateRect(hEditor,NULL,TRUE);
						}
						return 0;
					case VK_RIGHT:
						// moving Cursor one Position to the right
						if (rEditor.cursor.position<rEditor.length){
							rEditor.cursor.position++;
							::InvalidateRect(hEditor,NULL,TRUE);
						}
						return 0;
					case VK_HOME:
					case VK_UP:
						// moving Cursor to the beginning of File Name
						rEditor.cursor.position=0;
						::InvalidateRect(hEditor,NULL,TRUE);
						return 0;
					case VK_END:
					case VK_DOWN:
						// moving Cursor to the end of File Name
						rEditor.cursor.position=rEditor.length;
						::InvalidateRect(hEditor,NULL,TRUE);
						return 0;
					case VK_BACK:
						// deleting the character that preceeds the Cursor (Backspace)
						if (rEditor.cursor.position){
							const BYTE c=--rEditor.cursor.position;
							const PCHAR p=rEditor.buf;
							::memmove( p+c, p+1+c, rEditor.lengthMax-c );
							rEditor.length--;
							::InvalidateRect(hEditor,NULL,TRUE);
						}
						return 0;
					case VK_DELETE:
						// deleting the character that follows the Cursor (Delete)
						if (rEditor.cursor.position<rEditor.length){
							const BYTE c=rEditor.cursor.position;
							const PCHAR p=rEditor.buf;
							::memmove( p+c, p+1+c, ( --rEditor.length )-c );
							::InvalidateRect(hEditor,NULL,TRUE);
						}
						return 0;
					case VK_CONTROL:
					case VK_SHIFT:
						// changing Cursor Mode after pressing Ctrl+Shift
						if (::GetKeyState(VK_CONTROL)<0 && ::GetKeyState(VK_SHIFT)<0){
							static const TCursor::TMode Modes[]={ TCursor::K, TCursor::LC, TCursor::E, TCursor::G };
							TCursor::TMode &rMode=rEditor.cursor.mode;
							BYTE m=0;
							while (Modes[m++]!=rMode);
							if (m==sizeof(Modes)/sizeof(TCursor::TMode))
								m=0;
							rMode=Modes[m];
						}
						//fallthrough
					case VK_CAPITAL:
						// turning CapsLock on and Off
						::InvalidateRect(hEditor,NULL,TRUE); // to update the Cursor (switching between the "L" and "C" Modes)
						break;
					default:
						// adding a character to Buffer
						static const BYTE ConversionAbcModeKL[]={ 226,'*','?',205,200,204,203,'^',172,'-','+','=','.',',',';','"',199,'<',195,'>',197,'/',201,96,198,':' };
						static const BYTE Conversion012ModeKL[]={ '_','!','@','#','$','%','&','\'','(',')' };
						if (wParam==' '){
							rEditor.cursor.mode=TCursor::LC; // switching to Mode "L" if Space is pressed (or alternatively C, if CapsLock on)
							goto addCharInWParam;
						}
						switch (rEditor.cursor.mode){
							case TCursor::K:
								// Cursor in Mode K
								if (::GetKeyState(VK_CONTROL)<0){
									if (wParam>='A' && wParam<='Z')
										rEditor.__addChar__( ConversionAbcModeKL[wParam-'A'] );
									else if (wParam>='0' && wParam<='9')
										rEditor.__addChar__( Conversion012ModeKL[wParam-'0'] );
								}else
									if (wParam>='A' && wParam<='Z')
										rEditor.__addChar__(wParam-'A'+230); // conversion to capital letter
									else if (wParam>='0' && wParam<='9')
addCharInWParam:						rEditor.__addChar__(wParam);
								return 0;
							case TCursor::LC:
								// Cursor in Modes L (or alternatively C, if CapsLock on)
								if (::GetKeyState(VK_CONTROL)<0){
									if (wParam>='A' && wParam<='Z')
										rEditor.__addChar__( ConversionAbcModeKL[wParam-'A'] );
									else if (wParam>='0' && wParam<='9')
										rEditor.__addChar__( Conversion012ModeKL[wParam-'0'] );
								}else
									if (wParam>='A' && wParam<='Z'){
										if (::GetKeyState(VK_SHIFT)>=0 && !IS_CAPSLOCK_ON()) // if Shift not pressed and CapsLock not on...
											wParam|=32; // ... converting to lowercase letter
										goto addCharInWParam;
									}else if (wParam>='0' && wParam<='9')
										goto addCharInWParam;
								return 0;
							case TCursor::E:
								// Cursor in Mode E
								if (::GetKeyState(VK_CONTROL)<0){
									if (wParam>='A' && wParam<='Z'){
										static const BYTE Conversion[]={ '~',220,218,'\\',183,'{','}',216,191,174,170,171,221,222,223,127,181,214,'|',213,']',219,182,217,'[',215 };
										rEditor.__addChar__( Conversion[wParam-'A'] );
									}else if (wParam>='0' && wParam<='9'){
										static const BYTE Conversion[]={ 208,206,168,202,211,212,209,210,169,207 };
										rEditor.__addChar__( Conversion[wParam-'0'] );
									}
								}else
									if (wParam>='A' && wParam<='Z'){
										static const BYTE Conversion[]={ 227,196,224,228,180,188,189,187,175,176,177,192,167,166,190,173,178,186,229,165,194,225,179,185,193,184 };
										rEditor.__addChar__( Conversion[wParam-'A'] );
									}
								return 0;
							case TCursor::G:
								// Cursor in Mode G
								if (wParam>='A' && wParam<='Z')
									rEditor.__addChar__( 144+wParam-'A' );
								else if (wParam>='1' && wParam<='8') // 0 and 9 ignored
									if (::GetKeyState(VK_SHIFT)<0){
										static const BYTE Conversion[]={ 142,141,140,139,138,137,136,143 };
										rEditor.__addChar__( Conversion[wParam-'1'] );
									}else{
										static const BYTE Conversion[]={ 129,130,131,132,133,134,135,128 };
										rEditor.__addChar__( Conversion[wParam-'1'] );
									}
								return 0;
						}
						return 0;
				}
				break;
			}
		}
		return ::CallWindowProc( rEditor.wndProc0, hEditor,msg,wParam,lParam);
	}
	bool WINAPI CSpectrumDos::CSpectrumFileManagerView::CVarLengthFileNameEditor::__onChanged__(PVOID file,HWND,PVOID,short){
		// changes specified File's Name
		const PDos dos=CDos::__getFocused__();
		// - getting File's original Name and Extension
		TCHAR bufOldExt[MAX_PATH];
		dos->GetFileNameAndExt(file,NULL,bufOldExt);
		const CVarLengthFileNameEditor &rEditor=((CSpectrumFileManagerView *)dos->pFileManager)->varLengthFileNameEditor;
		TCHAR bufNewName[MAX_PATH];
		::lstrcpyn( bufNewName, rEditor.buf, rEditor.length+1 );
		// - validating File's new Name and Extension
		const TStdWinError err=dos->ChangeFileNameAndExt(file,bufNewName,bufOldExt,file);
		if (err==ERROR_SUCCESS) // the NewName+OldExtension combination is unique
			return true;
		else{	// at least two Files with the same NewName+OldExtension combination exist
			Utils::Information(FILE_MANAGER_ERROR_RENAMING,err);
			return false;
		}
	}
	HWND WINAPI CSpectrumDos::CSpectrumFileManagerView::CVarLengthFileNameEditor::__create__(PVOID,short,HWND hParent){
		const HWND hEditor=::CreateWindow(	AfxRegisterWndClass(0,app.LoadStandardCursor(IDC_IBEAM),CRideBrush::White),
											NULL, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0,0, 1,1, hParent, 0, AfxGetInstanceHandle(), NULL
										);
		const CSpectrumFileManagerView &rZxFileManager=*(CSpectrumFileManagerView *)CDos::__getFocused__()->pFileManager;
		rZxFileManager.varLengthFileNameEditor.wndProc0=(WNDPROC)::SetWindowLong(hEditor,GWL_WNDPROC,(LONG)__wndProc__);
		return hEditor;
	}
	CFileManagerView::PEditorBase CSpectrumDos::CSpectrumFileManagerView::CVarLengthFileNameEditor::Create(PFile file,BYTE _lengthMax){
		// creates and returns the Editor of File Name
		const PDos dos=CDos::__getFocused__();
		const CSpectrumFileManagerView *const pZxFileManager=(CSpectrumFileManagerView *)dos->pFileManager;
		// - initializing the Editor
		lengthMax=_lengthMax;
		ASSERT(lengthMax<sizeof(buf));
		dos->GetFileNameAndExt(file,buf,NULL);
		length=::lstrlen(buf);
		ASSERT(length<sizeof(buf));
		// - initializing the Cursor
		cursor.mode=TCursor::LC; // "L" Cursor if CapsLock off, "C" Cursor if CapsLock on
		cursor.position=length;
		return pZxFileManager->__createStdEditor__(	file, buf, lengthMax,
													CPropGridCtrl::TCustom::DefineEditor( 0, NULL, __create__, __help__, __onChanged__ )
												);
	}











	CFileManagerView::PEditorBase CSpectrumDos::CSpectrumFileManagerView::CStdParamEditor::Create(PFile file,PWORD pwParam,CPropGridCtrl::TInteger::TOnValueConfirmed fnOnConfirmed){
		// creates and returns the Editor of File Name
		const PDos dos=CDos::__getFocused__();
		const CSpectrumFileManagerView *const pZxFileManager=(CSpectrumFileManagerView *)dos->pFileManager;
		const PEditorBase result=pZxFileManager->__createStdEditorForWordValue__( file, pwParam, fnOnConfirmed );
		::SendMessage( result->hEditor, WM_SETFONT, (WPARAM)pZxFileManager->zxRom.font.m_hObject, 0 );
		return result;
	}
