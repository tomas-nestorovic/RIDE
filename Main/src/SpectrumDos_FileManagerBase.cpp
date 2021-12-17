#include "stdafx.h"

	#define INI_MSG_PARAMS	_T("params")

	CSpectrumBase::CSpectrumBaseFileManagerView::CSpectrumBaseFileManagerView(PDos dos,const TZxRom &rZxRom,BYTE supportedDisplayModes,BYTE initialDisplayMode,BYTE nInformation,PCFileInfo informationList,BYTE nameCharsMax,PCDirectoryStructureManagement pDirManagement)
		// ctor
		// - base
		: CFileManagerView( dos, supportedDisplayModes, initialDisplayMode, rZxRom.font, 3, nInformation, informationList, pDirManagement )
		// - initialization
		, zxRom(rZxRom) , nameCharsMax(nameCharsMax)
		, singleCharExtEditor(*this)
		, varLengthCommandLineEditor(*this) {
		// - adjusting context menu
		const Utils::CRideContextMenu mnuTmp( IDR_MDOS );
		mnuContext.AppendMenu( MF_STRING, ID_ZX_PREVIEWASSCREEN, mnuTmp.GetMenuStringByCmd(ID_ZX_PREVIEWASSCREEN) );
		mnuContext.AppendMenu( MF_STRING, ID_ZX_PREVIEWASBASIC, mnuTmp.GetMenuStringByCmd(ID_ZX_PREVIEWASBASIC) );
		mnuContext.AppendMenu( MF_STRING, ID_ZX_PREVIEWASASSEMBLER, mnuTmp.GetMenuStringByCmd(ID_ZX_PREVIEWASASSEMBLER) );
	}

	CSpectrumDos::CSpectrumFileManagerView::CSpectrumFileManagerView(PDos dos,const TZxRom &rZxRom,BYTE supportedDisplayModes,BYTE initialDisplayMode,BYTE nInformation,PCFileInfo informationList,BYTE nameCharsMax,PCDirectoryStructureManagement pDirManagement)
		// ctor
		: CSpectrumBaseFileManagerView( dos, rZxRom, supportedDisplayModes, initialDisplayMode, nInformation, informationList, nameCharsMax, pDirManagement ) {
	}








	#define IMAGE	tab.image
	#define DOS		IMAGE->dos

	PTCHAR CSpectrumBase::CSpectrumBaseFileManagerView::GenerateExportNameAndExtOfNextFileCopy(CDos::PCFile file,bool shellCompliant,PTCHAR pOutBuffer) const{
		// returns the Buffer populated with the export name and extension of the next File's copy in current Directory; returns Null if no further name and extension can be generated
		// - if File Name+Ext combination is irrelevant (headerless Files), returning current export Name+Ext
		CPathString fileName, fileExt;
		if (!DOS->GetFileNameOrExt( file, &fileName, &fileExt )) // name irrelevant
			return ::lstrcpy(  pOutBuffer,  DOS->GetFileExportNameAndExt( file, shellCompliant )  );
		// - generating a unique File Name+Ext
		if (const auto pdt=DOS->BeginDirectoryTraversal()){
			BYTE tmpDirEntry[2048]; // "big enough" to accommodate any ZX Spectrum DirectoryEntry
			::memcpy( tmpDirEntry, file, pdt->entrySize );
			for( BYTE copyNumber=1; copyNumber; copyNumber++ ){
				// . composing the Name for the File copy
				TCHAR postfix[8];
				const BYTE n=::wsprintf(postfix,_T("%c%d"),255,copyNumber); // 255 = token of the "COPY" keyword
				CPathString bufCopyName=fileName;
				bufCopyName.TrimToLength(nameCharsMax-n)+=postfix;
				// . attempting to rename the TemporaryDirectoryEntry
				CDos::PFile fExisting;
				switch (DOS->ChangeFileNameAndExt( &tmpDirEntry, bufCopyName, fileExt, fExisting )){
					case ERROR_SUCCESS:
						// generated a unique Name for the next File copy - returning the final export name and extension
						return ::lstrcpy(  pOutBuffer,  DOS->GetFileExportNameAndExt( &tmpDirEntry, shellCompliant )  );
					case ERROR_CANNOT_MAKE:
						// Directory full
						return nullptr;
				}
			}
		}
		return nullptr; // the Name for the next File copy cannot be generated
	}

	TStdWinError CSpectrumDos::CSpectrumFileManagerView::ImportPhysicalFile(LPCTSTR pathAndName,CDos::PFile &rImportedFile,DWORD &rConflictedSiblingResolution){
		// dragged cursor released above window
		// - if the File "looks like an Tape Image", confirming its import by the user
		if (const LPCTSTR extension=_tcsrchr(pathAndName,'.')){
			TCHAR ext[MAX_PATH];
			if (!::lstrcmp(::CharLower(::lstrcpy(ext,extension)),_T(".tap"))){
				// . defining the Dialog
				TCHAR buf[MAX_PATH+80];
				::wsprintf( buf, _T("\"%s\" looks like a tape."), _tcsrchr(pathAndName,'\\')+1 );
				class CPossiblyATapeDialog sealed:public Utils::CCommandDialog{
					BOOL OnInitDialog() override{
						// dialog initialization
						// : base
						const BOOL result=__super::OnInitDialog();
						// : supplying available actions
						AddCommandButton( IDYES, _T("Open it in a new tab (recommended)"), true );
						AddCommandButton( IDNO, _T("Import it to this image anyway") );
						AddCancelButton();
						return result;
					}
				public:
					CPossiblyATapeDialog(LPCTSTR msg)
						// ctor
						: Utils::CCommandDialog(msg) {
					}
				} d(buf);
				// . showing the Dialog and processing its result
				switch (d.DoModal()){
					case IDYES:{
						// opening the File in a new TDI Tab
						// : ejecting current Tape (if any)
						if (CTape::pSingleInstance)
							if (DOS->ProcessCommand(ID_TAPE_CLOSE)==TCmdResult::REFUSED) // if Tape not ejected ...
								return ERROR_CANCELLED; // ... we are done
						// : inserting a recorded Tape (by opening its underlying physical file)
						CTape::pSingleInstance=new CTape( pathAndName, (CSpectrumDos *)DOS, false ); // inserted Tape is WriteProtected by default
						rImportedFile=nullptr; // File processed another way than importing
						return ERROR_SUCCESS;
					}
					case IDNO:
						// importing the File to this Image anyway
						break;
					case IDCANCEL:
						// cancelling the remainder of importing
						return ERROR_CANCELLED;
				}
			}
		}
		// - importing the File
		return __super::ImportPhysicalFile( pathAndName, rImportedFile, rConflictedSiblingResolution );
	}







	CSpectrumBase::CSpectrumBaseFileManagerView::CSingleCharExtensionEditor::CSingleCharExtensionEditor(const CSpectrumBaseFileManagerView &rZxFileManager)
		// ctor
		: rZxFileManager(rZxFileManager) {
	}

	#define EXTENSION_MIN	32
	#define EXTENSION_MAX	127

	bool WINAPI CSpectrumBase::CSpectrumBaseFileManagerView::CSingleCharExtensionEditor::__onChanged__(PVOID file,PropGrid::Enum::UValue newExt){
		// changes the single-character Extension of given File
		const PDos dos=CDos::GetFocused();
		// - getting File's original Name
		CPathString oldName;
		dos->GetFileNameOrExt(file,&oldName,nullptr);
		// - validating File's new Name and Extension
		if (const TStdWinError err=dos->ChangeFileNameAndExt( file, oldName, newExt.charValue, file )){
			// at least two Files with the same OldName+NewExtension combination exist
			Utils::Information(FILE_MANAGER_ERROR_RENAMING,err);
			return false;
		}else
			// the OldName+NewExtension combination is unique
			return true;
	}
	static PropGrid::Enum::PCValueList WINAPI __createValues__(PVOID file,WORD &rnValues){
		// creates and returns the list of File's possible Extensions
		const PBYTE list=(PBYTE)::malloc( rnValues=EXTENSION_MAX+1-EXTENSION_MIN );
		for( BYTE p=EXTENSION_MIN,*a=list; p<=EXTENSION_MAX; *a++=p++ );
		return list;
	}
	static void WINAPI __freeValues__(PVOID file,PropGrid::Enum::PCValueList list){
		// disposes the list of File's possible Extensions
		::free((PVOID)list);
	}
	LPCTSTR WINAPI CSpectrumBase::CSpectrumBaseFileManagerView::CSingleCharExtensionEditor::__getDescription__(PVOID file,PropGrid::Enum::UValue extension,PTCHAR buf,short bufCapacity){
		// sets the Buffer to textual description of given Extension and returns its beginning in the Buffer
		return TZxRom::ZxToAscii( &extension.charValue, 1, buf );
	}
	CFileManagerView::PEditorBase CSpectrumBase::CSpectrumBaseFileManagerView::CSingleCharExtensionEditor::Create(PFile file) const{
		// creates and returns an Editor of File's single-character Extension
		CPathString ext;
		rZxFileManager.DOS->GetFileNameOrExt(file,nullptr,&ext);
		const PEditorBase result=CreateStdEditor(
			file, &( data=*ext ),
			PropGrid::Enum::DefineConstStringListEditorA( sizeof(data), __createValues__, __getDescription__, __freeValues__, __onChanged__ )
		);
		::SendMessage( result->hEditor, WM_SETFONT, (WPARAM)rZxFileManager.rFont.m_hObject, 0 );
		return result;
	}
	void CSpectrumBase::CSpectrumBaseFileManagerView::CSingleCharExtensionEditor::DrawReportModeCell(BYTE extension,LPDRAWITEMSTRUCT pdis,LPCSTR knownExtensions) const{
		// directly draws File's single-character Extension
		if (knownExtensions) // want to highlight in red unknown Extensions
			if (!::strchr(knownExtensions,extension))
				DrawRedHighlight(pdis);
		rZxFileManager.zxRom.PrintAt( pdis->hDC, (LPCSTR)&extension, 1, pdis->rcItem, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
	}










	CSpectrumBase::CSpectrumBaseFileManagerView::CVarLengthCommandLineEditor::CVarLengthCommandLineEditor(const CSpectrumBaseFileManagerView &rZxFileManager)
		// ctor
		: rZxFileManager(rZxFileManager) {
	}

	bool WINAPI CSpectrumBase::CSpectrumBaseFileManagerView::CVarLengthCommandLineEditor::__onCmdLineConfirmed__(PVOID file,HWND,PVOID value){
		// overwrites old command line with new one
		const PDos dos=CDos::GetFocused();
		const CSpectrumBaseFileManagerView *const pZxFileManager=(CSpectrumBaseFileManagerView *)dos->pFileManager;
		const TZxRom::CLineComposerPropGridEditor &rEditor=pZxFileManager->zxRom.lineComposerPropGridEditor;
		ASSERT( sizeof(*rEditor.GetCurrentZxText())==sizeof(char) );
		::memcpy( value, rEditor.GetCurrentZxText(), rEditor.GetCurrentZxTextLength() );
		return true;
	}

	CFileManagerView::PEditorBase CSpectrumBase::CSpectrumBaseFileManagerView::CVarLengthCommandLineEditor::Create(PFile file,PCHAR cmd,BYTE cmdLengthMax,char paddingChar,PropGrid::TOnValueChanged onChanged) const{
		// creates and returns the Editor of Spectrum command line
		ASSERT(cmdLengthMax<sizeof(bufOldCmd)/sizeof(TCHAR));
		#ifdef UNICODE
			ASSERT(FALSE);
		#else
			::memcpy( bufOldCmd, cmd, cmdLengthMax );
			return	CreateStdEditor( 
						file,
						cmd,
						TZxRom::CLineComposerPropGridEditor::Define( cmdLengthMax, paddingChar, __onCmdLineConfirmed__, onChanged )
					);
		#endif
	}

	bool WINAPI CSpectrumBase::CSpectrumBaseFileManagerView::CVarLengthCommandLineEditor::__onFileNameConfirmed__(PVOID file,HWND,PVOID){
		// changes specified File's Name
		const PDos dos=CDos::GetFocused();
		const CSpectrumBaseFileManagerView *const pZxFileManager=(CSpectrumBaseFileManagerView *)dos->pFileManager;
		// - getting File's original Extension
		CPathString oldExt;
		dos->GetFileNameOrExt(file,nullptr,&oldExt);
		// - validating File's new Name+Extension combination
		const TZxRom::CLineComposerPropGridEditor &rEditor=pZxFileManager->zxRom.lineComposerPropGridEditor;
		if (const TStdWinError err=dos->ChangeFileNameAndExt( file, CPathString(rEditor.GetCurrentZxText(),rEditor.GetCurrentZxTextLength()), oldExt, file )){
			// at least two Files with the same NewName+OldExtension combination exist
			Utils::Information(FILE_MANAGER_ERROR_RENAMING,err);
			return false;
		}else
			// the NewName+OldExtension combination is unique
			return true;
	}

	CFileManagerView::PEditorBase CSpectrumBase::CSpectrumBaseFileManagerView::CVarLengthCommandLineEditor::CreateForFileName(PFile file,BYTE fileNameLengthMax,char paddingChar,PropGrid::TOnValueChanged onChanged) const{
		// creates and returns the Editor of File Name
		ASSERT(fileNameLengthMax<sizeof(bufOldCmd)/sizeof(TCHAR));
		CPathString oldName;
		rZxFileManager.DOS->GetFileNameOrExt( file, &oldName, nullptr );
		::memset( bufOldCmd, paddingChar, fileNameLengthMax );
		#ifdef UNICODE
			ASSERT(FALSE);
		#else
			::memcpy( bufOldCmd, oldName, oldName.GetLength() );
		#endif
		return	CreateStdEditor(
					file,
					bufOldCmd,
					TZxRom::CLineComposerPropGridEditor::Define( fileNameLengthMax, paddingChar, __onFileNameConfirmed__, onChanged )
				);
	}

	void CSpectrumBase::CSpectrumBaseFileManagerView::CVarLengthCommandLineEditor::DrawReportModeCell(LPCSTR cmd,BYTE cmdLength,char paddingChar,LPDRAWITEMSTRUCT pdis) const{
		// directly draws FileName
		CPathString tmp( cmd, cmdLength );
		rZxFileManager.zxRom.PrintAt( pdis->hDC, tmp, tmp.TrimRight(paddingChar).GetLength(), pdis->rcItem, DT_SINGLELINE|DT_VCENTER );
	}
