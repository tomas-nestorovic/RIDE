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











	bool WINAPI CSpectrumDos::CSpectrumFileManagerView::CVarLengthFileNameEditor::__onChanged__(PVOID file,HWND,PVOID,short){
		// changes specified File's Name
		const PDos dos=CDos::__getFocused__();
		const CSpectrumFileManagerView *const pZxFileManager=(CSpectrumFileManagerView *)dos->pFileManager;
		// - getting File's original Name and Extension
		TCHAR bufOldExt[MAX_PATH];
		dos->GetFileNameAndExt(file,NULL,bufOldExt);
		TCHAR bufNewName[MAX_PATH];
		const TZxRom::CLineComposerPropGridEditor &rEditor=pZxFileManager->zxRom.lineComposerPropGridEditor;
		::lstrcpyn( bufNewName, rEditor.GetCurrentZxText(), rEditor.GetCurrentZxTextLength()+1 );
		// - validating File's new Name and Extension
		if (const TStdWinError err=dos->ChangeFileNameAndExt(file,bufNewName,bufOldExt,file)){
			// at least two Files with the same NewName+OldExtension combination exist
			Utils::Information(FILE_MANAGER_ERROR_RENAMING,err);
			return false;
		}else
			// the NewName+OldExtension combination is unique
			return true;
	}

	CFileManagerView::PEditorBase CSpectrumDos::CSpectrumFileManagerView::CVarLengthFileNameEditor::Create(PFile file,BYTE lengthMax,char paddingChar){
		// creates and returns the Editor of File Name
		const PDos dos=CDos::__getFocused__();
		const CSpectrumFileManagerView *const pZxFileManager=(CSpectrumFileManagerView *)dos->pFileManager;
		TCHAR bufOldName[MAX_PATH];
		#ifdef UNICODE
			ASSERT(FALSE);
		#else
			dos->GetFileNameAndExt( file, bufOldName, NULL );
			::memset( bufOldName+::lstrlen(bufOldName), paddingChar, lengthMax ); // guaranteed that LengthMax PaddingChars still fit in the Buffer for any ZX Spectrum derivate
			return pZxFileManager->__createStdEditor__(	file, NULL, 0, // Null = the Editor doesn't directly work with any value, it indirectly keeps its local copy of the value
														pZxFileManager->zxRom.lineComposerPropGridEditor.Create( bufOldName, lengthMax, paddingChar, __onChanged__ )
													);
		#endif
	}











	CFileManagerView::PEditorBase CSpectrumDos::CSpectrumFileManagerView::CStdParamEditor::Create(PFile file,PWORD pwParam,CPropGridCtrl::TInteger::TOnValueConfirmed fnOnConfirmed){
		// creates and returns the Editor of File Name
		const PDos dos=CDos::__getFocused__();
		const CSpectrumFileManagerView *const pZxFileManager=(CSpectrumFileManagerView *)dos->pFileManager;
		const PEditorBase result=pZxFileManager->__createStdEditorForWordValue__( file, pwParam, fnOnConfirmed );
		::SendMessage( result->hEditor, WM_SETFONT, (WPARAM)pZxFileManager->zxRom.font.m_hObject, 0 );
		return result;
	}
