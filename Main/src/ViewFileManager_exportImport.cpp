#include "stdafx.h"

	#define IMAGE	tab.image
	#define DOS		IMAGE->dos

	/*UINT AFX_CDECL CFileManagerView::COleVirtualFileDataSource::__fileTransfer_thread__(PVOID _pCancelableAction){
		// transfer of Files
		const TBackgroundActionCancelable *const pAction=(TBackgroundActionCancelable *)_pCancelableAction;
		COleVirtualFileDataSource *const pSrc=(COleVirtualFileDataSource *)pAction->fnParams;
		do{
			if (!pAction->bContinue) return ERROR_CANCELLED;
			::Sleep(1000);
			::Beep(1000,10);
		} while(true);
		return ERROR_SUCCESS;
	}*/
	CFileManagerView::COleVirtualFileDataSource::COleVirtualFileDataSource(CFileManagerView *_fileManager,DROPEFFECT _preferredDropEffect)
		// ctor
		// - initialization
		: fileManager(_fileManager) , preferredDropEffect(_preferredDropEffect) , deleteFromDiskWhenMoved(true)
		, sourceDir(fileManager->DOS->currentDir) {
		fileManager->ownedDataSource=this; // FileManager is the owner of this DataSource
		// - setting the PreferredDropEfect (necessary to recognize between Copying and Cutting)
		const HGLOBAL hPde=::GlobalAlloc( GMEM_SHARE, sizeof(DROPEFFECT) );
		*(DROPEFFECT *)::GlobalLock(hPde)=_preferredDropEffect;
		::GlobalUnlock(hPde);
		CacheGlobalData( CRideApp::cfPreferredDropEffect, hPde );
		// - DataTarget can inform DataSource on certain events
		DelaySetData( CRideApp::cfPasteSucceeded );
		DelaySetData( CRideApp::cfPerformedDropEffect );
		// - composing the list of Files to Copy/Cut - but in reality, transferred will be only Files that will be actually selected
		//DelayRenderData( CRideApp::cfDescriptor );	// commented out because the list would be composed while it might have already been switched to another directory (and current file selection would be lost)
		FORMATETC etc={ CRideApp::cfDescriptor, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
		HGLOBAL hDesc=nullptr;
			OnRenderGlobalData( &etc, &hDesc ); // generating File names for non-RIDE targets (e.g. Explorer)
		CacheGlobalData( CRideApp::cfDescriptor, hDesc );
		etc.cfFormat=CRideApp::cfRideFileList, hDesc=nullptr;
			OnRenderGlobalData( &etc, &hDesc ); // generating File names for another RIDE instance
		CacheGlobalData( CRideApp::cfRideFileList, hDesc );
		// - File data rendered later on background
		etc.cfFormat=CRideApp::cfContent, etc.tymed=TYMED_ISTREAM;
		DelayRenderData( CRideApp::cfContent, &etc ); // data don't exist in physical File and will be first "extracted" from Image by corresponding DOS
	}

	CFileManagerView::COleVirtualFileDataSource::~COleVirtualFileDataSource(){
		// dtor
		if (fileManager->ownedDataSource==this)
			fileManager->ownedDataSource=nullptr; // FileManager ceases to be the Owner of this DataSource
	}






	DWORD CFileManagerView::COleVirtualFileDataSource::__addFileToExport__(PWCHAR relativeDir,CDos::PFile file,LPFILEDESCRIPTORW lpfd,TStdWinError &rOutError){
		// adds given File for exporting by initializing the FileDescriptor structure; returns the number of Files (and Directories) added by this command
		// - checking if the File can be exported
		const auto exportName=fileManager->DOS->GetFileExportNameAndExt(file,fileManager->DOS->generateShellCompliantExportNames);
		if (!exportName.GetLengthW()) return 0;
		const PWCHAR pExportName=::lstrcpyW( relativeDir+::lstrlenW(relativeDir), exportName.GetUnicode() );
		// - adding specified File
		if (lpfd){
			lpfd->dwFlags=FD_ATTRIBUTES|FD_FILESIZE;
			::lstrcpyW( lpfd->cFileName, relativeDir );
			lpfd->dwFileAttributes=fileManager->DOS->GetAttributes(file);
			lpfd->nFileSizeLow=fileManager->DOS->ExportFile(file,nullptr,-1,nullptr); // getting only the export size of Files, not actually exporting any Files
			if (fileManager->DOS->GetFileCreatedTimeStamp( file, lpfd->ftCreationTime ))
				lpfd->dwFlags|=FD_CREATETIME;
			if (fileManager->DOS->GetFileLastReadTimeStamp( file, lpfd->ftLastAccessTime ))
				lpfd->dwFlags|=FD_ACCESSTIME;
			if (fileManager->DOS->GetFileLastWrittenTimeStamp( file, lpfd->ftLastWriteTime ))
				lpfd->dwFlags|=FD_WRITESTIME;
			lpfd++;
			listOfFiles.AddTail(file);
		}
		// - processing recurrently
		DWORD result=1; // this File or Directory
		if (fileManager->DOS->IsDirectory(file)){
			// Directory - adding all its "Subfiles"
			// . switching to the Directory
			const CDos::PFile originalDirectory=fileManager->DOS->currentDir;
			if (rOutError=(fileManager->DOS->*fileManager->pDirectoryStructureManagement->fnChangeCurrentDir)(file))
				return 0; // if error, quit
			::lstrcatW( pExportName, L"\\" );
			// . enumerating "Subfiles"
			if (const auto pdt=fileManager->DOS->BeginDirectoryTraversal())
				while (pdt->AdvanceToNextEntry())
					if (pdt->entryType==CDos::TDirectoryTraversal::FILE || pdt->entryType==CDos::TDirectoryTraversal::SUBDIR){
						const DWORD n=__addFileToExport__( relativeDir, pdt->entry, lpfd, rOutError );
						if (lpfd) lpfd+=n;
						result+=n;
						if (rOutError)
							break; // error - terminate
					}
			// . switching back to OriginalDirectory
			(fileManager->DOS->*fileManager->pDirectoryStructureManagement->fnChangeCurrentDir)(originalDirectory);
		}else
			// File
			rOutError=ERROR_SUCCESS;
		// - recovering the RelativeDirectory and returning the result
		*pExportName='\0';
		return result;
	}

	CDos::PFile CFileManagerView::COleVirtualFileDataSource::__getFile__(int id) const{
		// returns a File with the specified order number; assumed that the order number is always within range
		return (CDos::PFile)listOfFiles.GetAt( listOfFiles.FindIndex(id) );
	}

	bool CFileManagerView::COleVirtualFileDataSource::__isInList__(CDos::PCFile file) const{
		// True <=> specified File is in the list of Files to transfer, otherwise False
		return listOfFiles.Find((PVOID)file)!=nullptr;
	}

	BOOL CFileManagerView::COleVirtualFileDataSource::OnRenderData(LPFORMATETC lpFormatEtc,LPSTGMEDIUM lpStgMedium){
		// delayed rendering of data
		// - preferring IStream whenever it's possible (speed reasons)
		if (lpFormatEtc->tymed&TYMED_ISTREAM) lpFormatEtc->tymed=TYMED_ISTREAM;
		// - rendering
			return __super::OnRenderData(lpFormatEtc,lpStgMedium);
	}

	BOOL CFileManagerView::COleVirtualFileDataSource::OnRenderGlobalData(LPFORMATETC lpFormatEtc, HGLOBAL *phGlobal){
		// generates the list of Files to transfer
		if (lpFormatEtc->cfFormat==CRideApp::cfDescriptor){
			// generating the list of Files for shell or another RIDE instance (i.e. CFSTR_FILEDESCRIPTORW array; in reality, transferred will be only files that will be actually selected
			WCHAR relativeDir[32768]; // big enough to accommodate any path
			// . determining the NumberOfFilesToExport
			TStdWinError err;
			*relativeDir='\0';
			DWORD nFilesToExport=0;
			for( POSITION pos=fileManager->GetFirstSelectedFilePosition(); pos; )
				nFilesToExport+=__addFileToExport__( relativeDir, fileManager->GetNextSelectedFile(pos), nullptr, err );
			// . allocating the FileGroupDescriptor structure
			if (!*phGlobal)
				*phGlobal=::GlobalAlloc( GPTR, sizeof(FILEGROUPDESCRIPTORW)+(nFilesToExport-1)*sizeof(FILEDESCRIPTORW) ); // GHND = allocated memory zeroed
			// . populating the FileGroupDescriptor structure
			*relativeDir='\0';
			const LPFILEGROUPDESCRIPTORW pFgd=(LPFILEGROUPDESCRIPTORW)::GlobalLock(*phGlobal);
				pFgd->cItems=nFilesToExport;
				LPFILEDESCRIPTORW lpfd=pFgd->fgd;
				for( POSITION pos=fileManager->GetFirstSelectedFilePosition(); pos; )
					lpfd+=__addFileToExport__( relativeDir, fileManager->GetNextSelectedFile(pos), lpfd, err );
			::GlobalUnlock(*phGlobal);
			return TRUE;
		}else if (lpFormatEtc->cfFormat==CRideApp::cfRideFileList){
			// generating the list of Files for another RIDE instance
			const Utils::CVarTempReset<bool> gscen0( fileManager->DOS->generateShellCompliantExportNames, false ); // changing underlying DOS' setting is important ...
			const Utils::CVarTempReset<CLIPFORMAT> cf0( lpFormatEtc->cfFormat, CRideApp::cfDescriptor );
			return OnRenderGlobalData( lpFormatEtc, phGlobal); // ... after which we can proceed normally
		}else
			// other form of generating (i.e. other than using CFSTR_FILEDESCRIPTORW)
			return __super::OnRenderGlobalData(lpFormatEtc,phGlobal);
	}

	BOOL CFileManagerView::COleVirtualFileDataSource::OnRenderFileData(LPFORMATETC lpFormatEtc,CFile *pFile){
		// extracts File data from Image into COM's file (usually a IStream wrapper)
		if (lpFormatEtc->cfFormat==CRideApp::cfContent){
			// extracting virtual File's data, i.e. pretending "as if this was a real File" (i.e. CFSTR_FILECONTENTS)
 			const CDos::PCFile file=(CDos::PCFile)__getFile__(lpFormatEtc->lindex);
			if (const DWORD fileExportSize=fileManager->DOS->ExportFile(file,nullptr,-1,nullptr)){
				pFile->SetLength(fileExportSize);
				LPCTSTR errMsg;
				fileManager->DOS->ExportFile(file,pFile,-1,&errMsg);
				if (errMsg){
					fileManager->DOS->ShowFileProcessingError(file,errMsg);
					return FALSE;
				}
			}//else
				//pFile->Write(_T(""),1); // for some reason it's necessary to write at least one Byte
			return TRUE;
		}else
			// other form of exporting (i.e. other than using CFSTR_FILECONTENTS)
			return __super::OnRenderFileData(lpFormatEtc,pFile);
	}

	BOOL CFileManagerView::COleVirtualFileDataSource::OnSetData(LPFORMATETC lpFormatEtc,LPSTGMEDIUM lpStgMedium,BOOL bRelease){
		// DataTarget informs the DataSource
		if (lpFormatEtc->cfFormat==CRideApp::cfPasteSucceeded || lpFormatEtc->cfFormat==CRideApp::cfPerformedDropEffect){
			// DataTarget informs on how objects have been accepted A|B: A = when Pasted, B = when Dropped
			// - if Files moved, deleting Files from Image
			if (*(DROPEFFECT *)::GlobalLock(lpStgMedium->hGlobal)==DROPEFFECT_MOVE){
				fileManager->__deleteFiles__(listOfFiles);
				if (::IsWindow(fileManager->m_hWnd)) fileManager->RefreshDisplay();
			}
			::GlobalUnlock(lpStgMedium->hGlobal);
			::GlobalFree(lpStgMedium->hGlobal);
			// - after Copy/Cut, DataSource clears the clipboard (but not after drag&drop)
			if (preferredDropEffect!=DROPEFFECT_NONE)
				::OleSetClipboard(nullptr);
			return TRUE;
		}else
			return __super::OnSetData(lpFormatEtc,lpStgMedium,bRelease);
	}









	CDos::PFile CFileManagerView::__getDirectoryUnderCursor__(CPoint &rPt) const{
		// determines and returns the Directory currently under cursor (or Null if cursor not above Directory)
		const CListCtrl &lv=GetListCtrl();
		const int i=lv.HitTest(rPt);
		if (i<0) return nullptr; // cursor outside any File or Directory
		const CDos::PFile file=(CDos::PFile)lv.GetItemData(i);
		return	DOS->IsDirectory(file) // the File is actually a Directory
				? file
				: nullptr;
	}

	DROPEFFECT CFileManagerView::OnDragEnter(COleDataObject *pDataObject,DWORD dwKeyState,CPoint point){
		// cursor entered the window's client area
		// - revoking previous File as the DropTarget (if there was any)
		CListCtrl &lv=GetListCtrl();
		if (dropTargetFileId>=0){
			lv.SetItemState( dropTargetFileId, lv.GetItemState(dropTargetFileId,LVNI_STATEMASK)&~LVNI_DROPHILITED, LVNI_STATEMASK );
			dropTargetFileId=-1;
		}
		// - only Shell objects or other FileManager's objects accepted
		if (!pDataObject->GetGlobalData(CF_HDROP) && !pDataObject->GetGlobalData(CRideApp::cfDescriptor))
			return DROPEFFECT_NONE;
		// - own objects accepted only if dragged above Directory that can be potential destination
		const CDos::PCFile directoryUnderCursor=__getDirectoryUnderCursor__(point);
		if (ownedDataSource){ // this FileManager's own objects (dragging initiated in this FileManager)
			if (!directoryUnderCursor) // no DirectoryUnderCursor that potentially could be the destination - assuming the current Directory (identical with SourceDirectory)
				return DROPEFFECT_NONE;
			if (ownedDataSource->__isInList__(directoryUnderCursor)) // destination DirectoryUnderCursor is identical with one of dragged Directories
				return DROPEFFECT_NONE;
		}
		// - highlighting the DirectoryUnderCursor as current destination of Drop
		if (directoryUnderCursor){
			LVFINDINFO lvdi;
				lvdi.flags=LVFI_PARAM, lvdi.lParam=(LPARAM)directoryUnderCursor;
			dropTargetFileId=lv.FindItem(&lvdi);
			lv.SetItemState( dropTargetFileId, lv.GetItemState(dropTargetFileId,LVNI_STATEMASK)|LVNI_DROPHILITED, LVNI_STATEMASK );
		}
		// - objects accepted
		if (ownedDataSource) // in scope of this FileManager, Move is preferred unless a modification key is pressed
			return dwKeyState&MK_CONTROL ? DROPEFFECT_COPY : DROPEFFECT_MOVE;
		else // across applications, Copy is preferred unless a modification key is pressed
			return dwKeyState&MK_SHIFT ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
	}

	DROPEFFECT CFileManagerView::OnDragOver(COleDataObject *pDataObject,DWORD dwKeyState,CPoint point){
		// dragged cursor moved above this window
		return OnDragEnter(pDataObject,dwKeyState,point);
	}

	#define IMPORT_MSG_CANCELLED	_T("Quitting the import.")

	BOOL CFileManagerView::OnDrop(COleDataObject *pDataObject,DROPEFFECT dropEffect,CPoint point){
		// dragged cursor released above window
		BOOL result=FALSE; // assumption (Drop failed)
		SetRedraw(FALSE);
			// - switching to TargetDirectory
			CDos::PFile originalDirectory,targetDirectory=nullptr;
			if (pDirectoryStructureManagement){
				originalDirectory=DOS->currentDir;
				if (targetDirectory=__getDirectoryUnderCursor__(point))
					__switchToDirectory__(targetDirectory);
			}
			// - importing Files
			CDos::PFile importedFile;
			DWORD conflictResolution=TConflictResolution::UNDETERMINED;
			if (HGLOBAL hg=pDataObject->GetGlobalData(CF_HDROP)){
				// importing physical Files (by dragging them from Explorer)
				if (const HDROP hDrop=(HDROP)::GlobalLock(hg)){
					WCHAR buf[MAX_PATH];
					for( UINT n=::DragQueryFile(hDrop,-1,nullptr,0); n; )
						if (::DragQueryFileW(hDrop,--n,buf,ARRAYSIZE(buf)))
							// creating File in Image
							switch (ImportPhysicalFile(buf,importedFile,conflictResolution)){ // shows also error messages
								case ERROR_SUCCESS:
									if (importedFile) // File really imported (e.g. Spectrum Tape may be upon request open in a separate TDI Tab instead of be imported)
										selectedFiles.AddTail(importedFile);
									break;
								case ERROR_WRITE_PROTECT:
									IMAGE->ReportWriteProtection();
									dropEffect=DROPEFFECT_NONE;
									//fallthrough
								case ERROR_CANCELLED:
									goto importQuit1;
								default:
									dropEffect=DROPEFFECT_COPY; // eventual Move effect becomes a Copy effect
									break;
							}
					result=TRUE; // Dropped successfully
importQuit1:		::DragFinish(hDrop);
					::GlobalUnlock(hg);
				}
			}else if (!IMAGE->ReportWriteProtection()){
				// importing virtual Files (by dragging them from FileManager, no matter if this one or across applications)
				if (!( hg=pDataObject->GetGlobalData(CRideApp::cfRideFileList) )) // if RIDE native list of Files not available ...
					hg=pDataObject->GetGlobalData(CRideApp::cfDescriptor); // ... then settle with shell native list of Files
				if (const LPFILEGROUPDESCRIPTORW pfgd=(LPFILEGROUPDESCRIPTORW)::GlobalLock(hg)){
					bool moveWithinCurrentDisk=false; // assumption (Files are NOT moved within current disk but are imported from external source)
					const int nFiles=pfgd->cItems;
					for( int i=0; i<nFiles; ){
						CDos::CPathString fileNameAndExt=pfgd->fgd[i].cFileName;
						if (ownedDataSource) // FileManager is both source and target of File data (e.g. when creating a File copy by pressing Ctrl+C and Ctrl+V)
							if (!pDirectoryStructureManagement || ownedDataSource->sourceDir==DOS->currentDir){
								// source and target Directories are the same
								if (dropEffect==DROPEFFECT_MOVE){
									// moving Files within the same Directory
									dropEffect=DROPEFFECT_COPY; // to not delete the Files
									break; // no need to move anything - Files are already in corrent Directory
								}else{
									// copying Files within the same Directory
									const auto copyNameAndExt=GenerateExportNameAndExtOfNextFileCopy( ownedDataSource->__getFile__(i), false );
									if (copyNameAndExt.GetLengthW()>0) // generating new FileName for each copied File
										fileNameAndExt=copyNameAndExt;
									else{
										// error creating a File copy
										Utils::FatalError(
											Utils::SimpleFormat( _T("Cannot copy \"%s\""), fileNameAndExt ),
											ERROR_CANNOT_MAKE, IMPORT_MSG_CANCELLED
										);
										goto importQuit2;
									}
								}
							}else
								// source and target Directories are different
								if ( moveWithinCurrentDisk=dropEffect==DROPEFFECT_MOVE ){
									// moving Files across Directories
									switch (__moveFile__( i, pfgd->fgd, nFiles, importedFile, conflictResolution )){
										case ERROR_SUCCESS:
											selectedFiles.AddTail(importedFile);
											break;
										case ERROR_CANCELLED:
											goto importQuit2;
									}
									continue;
								}//else
									// copying Files across Directories
									//nop
						switch (__importVirtualFile__(i,fileNameAndExt,pfgd->fgd,nFiles,pDataObject,importedFile,conflictResolution)){
							case ERROR_SUCCESS:
								selectedFiles.AddTail(importedFile);
								break;
							case ERROR_CANCELLED:
								goto importQuit2;
							default:
								dropEffect=DROPEFFECT_COPY; // eventual Move effect becomes a Copy effect
								break;
						}
					}
					result=TRUE; // Dropped successfully
importQuit2:		::GlobalUnlock(hg);
					if (moveWithinCurrentDisk) dropEffect=DROPEFFECT_COPY; // to not delete the Files
				}
				::GlobalFree(hg);
			}else
				// can't import virtual Files as the Image is write-protected
				dropEffect=DROPEFFECT_NONE;
			// - informing DataSource on type of acceptance of Files (for the drag&drop to go the same way as with copy/cut/paste)
			FORMATETC etc={ CRideApp::cfPasteSucceeded, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
			STGMEDIUM stg={ TYMED_HGLOBAL, (HBITMAP)::GlobalAlloc(GMEM_FIXED,sizeof(DWORD)), nullptr };
			*(DROPEFFECT *)::GlobalLock(stg.hGlobal)=dropEffect;
			::GlobalUnlock(stg.hGlobal);
			pDataObject->m_lpDataObject->SetData( &etc, &stg, TRUE );
			// - clearing cliboard after pasting cut Files
			if (dropEffect==DROPEFFECT_MOVE)
				::OleSetClipboard(nullptr);
			// - switching back to OriginalDirectory
			if (targetDirectory){ // it was switched from Original- to TargetDirectory above
				__switchToDirectory__(originalDirectory);
				selectedFiles.RemoveAll();
				RefreshDisplay(); // repopulating the content of the FileManager (empties the SelectedFiles list)
			}
			// - refreshing the FileManager and scrolling to the last imported File
			if (selectedFiles.GetCount()){ // may be empty if importing a single File that is already on the disk, and refusing to rewrite it
				const LVFINDINFO lvfi={ LVFI_PARAM, nullptr, (LPARAM)selectedFiles.GetTail() };
				RefreshDisplay(); // repopulating the content of the FileManager (empties the SelectedFiles list)
				ListView_EnsureVisible( m_hWnd, ListView_FindItem(m_hWnd,0,&lvfi), false ); // scrolling to the last imported File
			}
		SetRedraw(TRUE);
		return result;
	}


	TStdWinError CFileManagerView::__skipNameConflict__(DWORD newFileSize,CDos::PFile conflict,DWORD &rConflictedSiblingResolution) const{
		// resolves conflict of File names by displaying a Dialog; returns Windows standard i/o error (ERROR_SUCCESS = import succeeded, ERROR_CANCELLED = import of a set of Files was cancelled, ERROR_* = other error)
		const bool directory=DOS->IsDirectory(conflict);
		BYTE b;
		switch (rConflictedSiblingResolution&~TConflictResolution::CUSTOM_MASK){
			case TConflictResolution::MERGE	:b=IDYES; break;
			case TConflictResolution::SKIP	:b=IDNO; break;
			default:{
				float higherUnit;	LPCTSTR higherUnitName;
				TCHAR bufOverwrite[80];
					if (directory)
						::lstrcpy(bufOverwrite,_T("Merge existing directory with new one"));
					else{
						Utils::BytesToHigherUnits(newFileSize,higherUnit,higherUnitName);
						_stprintf( bufOverwrite, _T("Replace with new file (%.2f %s)"), higherUnit, higherUnitName );
					}	
				TCHAR bufSkip[80];
					if (directory)
						::lstrcpy(bufSkip,_T("Skip this directory"));
					else{
						Utils::BytesToHigherUnits(DOS->GetFileSize(conflict),higherUnit,higherUnitName);
						_stprintf( bufSkip, _T("Keep current file (%.2f %s)"), higherUnit, higherUnitName );
					}
				CNameConflictResolutionDialog d( DOS->GetFilePresentationNameAndExt(conflict), directory?_T("directory"):_T("file"), bufOverwrite,bufSkip );
				b=d.DoModal();
				if (d.useForAllSubsequentConflicts==BST_CHECKED)
					rConflictedSiblingResolution =	(rConflictedSiblingResolution&TConflictResolution::CUSTOM_MASK)
													|
													( b==IDYES ? TConflictResolution::MERGE : TConflictResolution::SKIP );
			}
		}
		switch (b){
			case IDYES:
				if (directory)
					return ERROR_SUCCESS;
				else{
					const TStdWinError err=DOS->DeleteFile(conflict); // resolving the Conflict by deleting existing File
					if (err!=ERROR_SUCCESS)
						DOS->ShowFileProcessingError(conflict,err);
					return err;
				}
			case IDNO:
				return ERROR_FILE_EXISTS;
			default:
				return ERROR_CANCELLED;
		}
	}

	TStdWinError CFileManagerView::__moveFile__(int &i,LPFILEDESCRIPTORW files,int nFiles,CDos::PFile &rMovedFile,DWORD &rConflictedSiblingResolution){
		// moves virtual File within Image; returns Windows standard i/o error
		const LPFILEDESCRIPTORW lpfd=files+i;
		const LPCWSTR backslash=::StrRChrW(lpfd->cFileName,nullptr,'\\');
		const LPCWSTR fileName= backslash&&pDirectoryStructureManagement ? 1+backslash : lpfd->cFileName;
		const CDos::PFile file=ownedDataSource->__getFile__(i++);
		TStdWinError err=(DOS->*pDirectoryStructureManagement->fnMoveFileToCurrDir)( file, fileName, rMovedFile );
		if (lpfd->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){
			// Directory
			// . determining the range of Files to move in scope of this Directory (all such Files in make up a sequence in the input list that has the same Directory path)
			int j=i;
			for( const int n=::lstrlenW(::lstrcatW(lpfd->cFileName,L"\\")); j<nFiles; j++ )
				if (::StrCmpNW(files[j].cFileName,lpfd->cFileName,n)) break;
			// . resolving conflicts
			if (err==ERROR_FILE_EXISTS) // Directory already exists on the disk
				if (( err=__skipNameConflict__(DOS->GetFileSize(file),rMovedFile,rConflictedSiblingResolution) )==ERROR_SUCCESS){
					// merging current and conflicted Directories
					const CDos::PFile currentDirectory=DOS->currentDir;
					__switchToDirectory__(rMovedFile);
						bool allSubfilesMoved=true; // assumption
						for( DWORD csr=rConflictedSiblingResolution; i<j; ){
							CDos::PFile tmp;
							err=__moveFile__( i, files, nFiles, tmp, csr );
							allSubfilesMoved&=err==ERROR_SUCCESS;
							if (err!=ERROR_SUCCESS && err!=ERROR_FILE_EXISTS) break;
						}
					__switchToDirectory__(currentDirectory);
					if (allSubfilesMoved) DOS->DeleteFile(file);
				}
			// . Directory moved
			i=j;
		}else
			// File
			if (err==ERROR_FILE_EXISTS) // File already exists on the disk
				if (( err=__skipNameConflict__(DOS->GetFileSize(file),rMovedFile,rConflictedSiblingResolution) )==ERROR_SUCCESS)
					err=(DOS->*pDirectoryStructureManagement->fnMoveFileToCurrDir)( file, fileName, rMovedFile );
		return err;
	}

	TStdWinError CFileManagerView::ImportFileAndResolveConflicts(CFile *f,DWORD fileSize,CDos::RCPathString nameAndExtension,DWORD winAttr,const FILETIME &rCreated,const FILETIME &rLastRead,const FILETIME &rLastModified,CDos::PFile &rImportedFile,DWORD &rConflictedSiblingResolution){
		// imports physical or virtual File; returns Windows standard i/o error (ERROR_SUCCESS = imported successfully, ERROR_CANCELLED = import of a set of Files was cancelled, ERROR_* = other error)
		do{
			// - importing
			TStdWinError err;
			if (winAttr&FILE_ATTRIBUTE_DIRECTORY){
				// importing a Directory
				f=nullptr;
				err=pDirectoryStructureManagement	// if the DOS supports Directories ...
					? err=(DOS->*pDirectoryStructureManagement->fnCreateSubdir)( nameAndExtension, winAttr, rImportedFile ) // ... creating the Directory
					: ERROR_NOT_SUPPORTED; // ... otherwise error
			}else{
				// importing a File
				const auto fPosition=f->GetPosition();
				err=DOS->ImportFile( f, fileSize, nameAndExtension, winAttr, rImportedFile );
				f->Seek( fPosition, CFile::begin ); // seeking back in case that we retry the import after conflicts have been resolved
			}
			// - if Directory/File imported successfully, set its time stamps and quit
			if (err==ERROR_SUCCESS){
				DOS->SetFileTimeStamps( rImportedFile, &rCreated, &rLastRead, &rLastModified );
				break;
			}
			// - error while importing Directory/File
			const CString errTxt=Utils::SimpleFormat( _T("Cannot import the %s \"%s\""), f?_T("file"):_T("directory"), nameAndExtension );
			switch (err){
				case ERROR_FILE_EXISTS:
					// Directory/File already exists on disk
					if (( err=__skipNameConflict__(fileSize,rImportedFile,rConflictedSiblingResolution) )==ERROR_SUCCESS)
						if (f) continue; // conflict solved - newly attempting to import the Directory/File (function terminated for Directory)
					return err;
				case ERROR_BAD_LENGTH:
					// File too long or too short
					Utils::Information( errTxt, err, _T("File length either too long or too short.\n") IMPORT_MSG_CANCELLED );
					return err;
				default:
					Utils::Information( errTxt, err, IMPORT_MSG_CANCELLED );
					return err;
			}
		}while (true);
		return ERROR_SUCCESS;
	}

	TStdWinError CFileManagerView::ImportPhysicalFile(CDos::RCPathString shellName,CDos::PFile &rImportedFile,DWORD &rConflictedSiblingResolution){
		// imports physical File with given Name into current Directory; returns Windows standard i/o error
		const CDos::CPathString fileName=shellName.GetFileName();
		const DWORD winAttr=::GetFileAttributesW(shellName.GetUnicode());
		FILETIME created,lastRead,lastModified;
		if (winAttr&FILE_ATTRIBUTE_DIRECTORY){
			// Directory
			if (IMAGE->IsWriteProtected())
				return ERROR_WRITE_PROTECT;
			const HANDLE hDir=shellName.CreateFile( GENERIC_READ, FILE_SHARE_READ, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS );
			::GetFileTime( hDir, &created, &lastRead, &lastModified );
			::CloseHandle(hDir);
			TStdWinError err=ImportFileAndResolveConflicts( nullptr, 0, fileName, winAttr, created, lastRead, lastModified, rImportedFile, rConflictedSiblingResolution );
			if (err==ERROR_SUCCESS){
				// Directory created successfully - recurrently importing all contained Files
				const CDos::PFile currentDirectory=DOS->currentDir;
				__switchToDirectory__(rImportedFile);
					WIN32_FIND_DATAW fd;
					const HANDLE hFindFile=::FindFirstFileW(::lstrcatW(::lstrcpyW(fd.cFileName,shellName.GetUnicode()),L"\\*.*"),&fd);
					if (hFindFile!=INVALID_HANDLE_VALUE){
						for( DWORD csr=rConflictedSiblingResolution; true; ){
							if (::lstrcmpW(fd.cFileName,L".") && ::lstrcmpW(fd.cFileName,L"..")){ // "dot" and "dotdot" entries skipped
								CDos::PFile file;
								err=ImportPhysicalFile( shellName.Clone().AppendBackslashItem(fd.cFileName), file, csr );
								if (err!=ERROR_SUCCESS && err!=ERROR_FILE_EXISTS) break;
							}
							if (!::FindNextFileW(hFindFile,&fd)){
								if (( err=::GetLastError() )==ERROR_NO_MORE_FILES)
									err=ERROR_SUCCESS;
								break;
							}
						}
						::FindClose(hFindFile);
					}
				__switchToDirectory__(currentDirectory);
			}
			return err;
		}else{
			// File
			// . "God Mode" - automatic generation of random Files
			if (app.IsInGodMode()){
				int nItemsToGenerate, fileLengthMin, fileLengthMax, i;
				if (_stscanf( fileName, _T("GODMODE %d F %d %d.%c"), &nItemsToGenerate, &fileLengthMin, &fileLengthMax, &i )==4){
					// processing the request
					char name[16],data[16384];
					if (fileLengthMin<0) fileLengthMin=0;
					if (fileLengthMax>sizeof(data)) fileLengthMax=sizeof(data);
					for( static WORD fileId; nItemsToGenerate--; ){
						const int fileLength=fileLengthMin+::rand()*(fileLengthMax-fileLengthMin)/RAND_MAX;
						do{
							const int nameLength=::wsprintfA( name, "%04X.TMP", fileId++ );
							for( i=0; i<fileLength; i+=::wsprintfA(data+i,"%s DATA ",name) );
							if (const TStdWinError err=DOS->ImportFile( &CMemFile((PBYTE)data,sizeof(data)), fileLength, CDos::CPathString(name,nameLength), 0, rImportedFile ))
								if (err==ERROR_FILE_EXISTS)
									continue; // a File with a given Name already exists - trying another Name
								else
									return err;
							break;
						}while (true);
					}
					return ERROR_SUCCESS;
				}
			}
			// . if the File "looks like an Image", confirming its import by the user
			if (const LPCTSTR extension=shellName.FindLastDot())
				if (CImage::DetermineType(shellName)!=nullptr){
					// : defining the Dialog
					class CPossiblyAnImageDialog sealed:public Utils::CCommandDialog{
						const CString msg;
						BOOL OnInitDialog() override{
							// dialog initialization
							// | base
							const BOOL result=__super::OnInitDialog();
							// | supplying available actions
							AddCommandButton( IDYES, _T("Open it in new instance of ") _T(APP_ABBREVIATION) _T(" (recommended)"), true );
							AddCommandButton( IDNO, _T("Import it to this image anyway") );
							AddCancelButton();
							return result;
						}
					public:
						CPossiblyAnImageDialog(const CString &msg)
							// ctor
							: Utils::CCommandDialog(msg)
							, msg(msg) {
						}
					} d(
						Utils::SimpleFormat( _T("\"%s\" looks like an image."), shellName.GetFileName() )
					);
					// : showing the Dialog and processing its result
					switch (d.DoModal()){
						case IDYES:{
							// opening the File in new instance of the app (this may function only in Release mode, not in Debug mode)
							rImportedFile=nullptr;
							WCHAR buf[MAX_PATH];
							::GetModuleFileNameW( nullptr, buf, ARRAYSIZE(buf) );
							::ShellExecuteW( nullptr, L"open", buf, shellName.GetQuoted().GetUnicode(), nullptr, SW_SHOW );
							return ::GetLastError();
						}
						case IDNO:
							// importing the File to this Image anyway
							break;
						case IDCANCEL:
							// cancelling the remainder of importing
							return ERROR_CANCELLED;
					}
				}
			// . importing the File
			if (IMAGE->IsWriteProtected())
				return ERROR_WRITE_PROTECT;
			CFile f;
			f.m_hFile=(decltype(f.m_hFile))shellName.CreateFile( GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING );
			if (f.m_hFile!=CFile::hFileNull){
				::GetFileTime( (HANDLE)f.m_hFile, &created, &lastRead, &lastModified );
				created=Utils::CRideTime(created).ToTzSpecificLocalTime();
				lastRead=Utils::CRideTime(lastRead).ToTzSpecificLocalTime();
				lastModified=Utils::CRideTime(lastModified).ToTzSpecificLocalTime();
				return ImportFileAndResolveConflicts( &f, f.GetLength(), fileName, winAttr, created, lastRead, lastModified, rImportedFile, rConflictedSiblingResolution );
			}else
				return ::GetLastError();
		}
	}

	#define FD_ATTRIBUTES_MANDATORY	(FD_ATTRIBUTES|FD_FILESIZE)

	TStdWinError CFileManagerView::__importVirtualFile__(int &i,CDos::RCPathString pathAndName,LPFILEDESCRIPTORW files,int nFiles,COleDataObject *pDataObject,CDos::PFile &rImportedFile,DWORD &rConflictedSiblingResolution){
		// imports virtual File with given Name into current Directory; returns Windows standard i/o error
		const LPFILEDESCRIPTORW lpfd=files+i;
		// - making sure that FileDescriptor structure contains all mandatory information
		if ((lpfd->dwFlags&FD_ATTRIBUTES_MANDATORY)!=FD_ATTRIBUTES_MANDATORY)
			return ERROR_NOT_SUPPORTED;
		if (lpfd->nFileSizeHigh)
			return ERROR_FILE_TOO_LARGE;
		// - importing
		const FILETIME &rCreated= lpfd->dwFlags&FD_CREATETIME ? lpfd->ftCreationTime : Utils::CRideTime::None;
		const FILETIME &rLastRead= lpfd->dwFlags&FD_ACCESSTIME ? lpfd->ftLastAccessTime : Utils::CRideTime::None;
		const FILETIME &rLastWritten= lpfd->dwFlags&FD_WRITESTIME ? lpfd->ftLastWriteTime : Utils::CRideTime::None;
		const CDos::CPathString tmp=pathAndName.GetFileName();
		const CDos::CPathString &fileName= tmp.GetLengthW()&&pDirectoryStructureManagement ? tmp : pathAndName;
		const DWORD winAttr=lpfd->dwFileAttributes;
		if (winAttr&FILE_ATTRIBUTE_DIRECTORY){
			// Directory
			// . creating (must be now as below the cFileName member is changed)
			TStdWinError err=ImportFileAndResolveConflicts( nullptr, 0, fileName, winAttr, rCreated, rLastRead, rLastWritten, rImportedFile, rConflictedSiblingResolution );
			// . determining the range of Files to import into this Directory (all such Files in the input list have the same Directory path)
			int j=++i;
			for( const int n=::lstrlenW(::lstrcatW(lpfd->cFileName,L"\\")); j<nFiles; j++ )
				if (::StrCmpNW(files[j].cFileName,lpfd->cFileName,n)) break;
			// . processing recurrently
			if (err==ERROR_SUCCESS){
				const CDos::PFile currentDirectory=DOS->currentDir;
				__switchToDirectory__(rImportedFile);
					DWORD csr=rConflictedSiblingResolution;
					for( CDos::PFile tmp; i<j; ){
						err=__importVirtualFile__( i, files[i].cFileName, files, nFiles, pDataObject, tmp, csr );
						if (err!=ERROR_SUCCESS && err!=ERROR_FILE_EXISTS)
							break;
					}
				__switchToDirectory__(currentDirectory);
			}
			// . Directory imported
			i=j;
			return err;
		}else{
			// File
			FORMATETC etcContent={ CRideApp::cfContent, nullptr, DVASPECT_CONTENT, i++, TYMED_ISTREAM };
			const std::unique_ptr<CFile> f( pDataObject->GetFileData( CRideApp::cfContent, &etcContent ) ); // abstracting virtual data into a File
			f->SeekToBegin();
			return ImportFileAndResolveConflicts( f.get(), lpfd->nFileSizeLow, fileName, winAttr, rCreated, rLastRead, rLastWritten, rImportedFile, rConflictedSiblingResolution );
		}
	}









	afx_msg void CFileManagerView::__onBeginDrag__(NMHDR *pNMHDR,LRESULT *pResult){
		// dragging of Files initiated
		if (auto *const pObj=new COleVirtualFileDataSource(this,DROPEFFECT_NONE)){
			pObj->DoDragDrop( // launching drag&drop
				DROPEFFECT_COPY|( IMAGE->writeProtected ? 0 : DROPEFFECT_MOVE ) // not contained in switch(.) because: (1) drag/drop adopts here the same logic as with copy/cut/paste - see communication of target with source in OnDrop, (2) the result of DoDragDrop is inaccurate - see DataSource's OnSetData
			);
			pObj->ExternalRelease();
		}
		//Utils::InformationWithCheckableShowNoMore(_T("Extra information has been appended to each file name.\n\nTo import the file back in the exact form as on this image, preserve this information."),INI_MSG_FILE_EXPORT_INFO);
	}

	afx_msg void CFileManagerView::__copyFilesToClipboard__(){
		// copies selected Files to clipboard
		( new COleVirtualFileDataSource(this,DROPEFFECT_COPY) )->SetClipboard();
	}

	afx_msg void CFileManagerView::__cutFilesToClipboard__(){
		// moves selected Files from Image to clipboard
		if (IMAGE->ReportWriteProtection()) return;
		( new COleVirtualFileDataSource(this,DROPEFFECT_MOVE) )->SetClipboard();
	}

	afx_msg void CFileManagerView::__pasteFilesFromClipboard__(){
		// pastes Files from clipboard to Image
		COleDataObject odo;
		odo.AttachClipboard();
		if (const HGLOBAL hg=odo.GetGlobalData(CRideApp::cfPreferredDropEffect)){
			OnDrop( &odo, *(DROPEFFECT *)::GlobalLock(hg), CPoint(-1) );
			::GlobalUnlock(hg);
			::GlobalFree(hg);
		}
	}

	afx_msg void CFileManagerView::__fileSelected_updateUI__(CCmdUI *pCmdUI){
		// projecting existence of one or more selected Files into UI
		pCmdUI->Enable( GetFirstSelectedFilePosition()!=nullptr );
	}


	afx_msg void CFileManagerView::__pasteFiles_updateUI__(CCmdUI *pCmdUI){
		// projecting possibility to paste Files into UI
		COleDataObject odo;
		odo.AttachClipboard();
		pCmdUI->Enable(	odo.IsDataAvailable(CF_HDROP)
						||
						odo.IsDataAvailable(CRideApp::cfDescriptor)
						||
						odo.IsDataAvailable(CRideApp::cfRideFileList)
					);
	}











	CFileManagerView::CNameConflictResolutionDialog::CNameConflictResolutionDialog(LPCTSTR _conflictedName,LPCTSTR _conflictedNameType,LPCTSTR _captionForReplaceButton,LPCTSTR _captionForSkipButton)
		// ctor
		// - base
		: Utils::CCommandDialog(IDR_FILEMANAGER_IMPORT_CONFLICT,information)
		// - initialization
		, captionForReplaceButton(_captionForReplaceButton) , captionForSkipButton(_captionForSkipButton)
		, useForAllSubsequentConflicts(BST_UNCHECKED) {
		// - initializing Information (i.e. the main message of the dialog)
		::wsprintf( information, _T("This folder already contains the %s \"%s\"."), _conflictedNameType, _conflictedName );
	}

	BOOL CFileManagerView::CNameConflictResolutionDialog::OnInitDialog(){
		// dialog initialization
		// - base
		const BOOL result=__super::OnInitDialog();
		// - initializing the "Replace" button
		SetDlgItemText( IDYES, captionForReplaceButton );
		ConvertToCommandLikeButton( GetDlgItemHwnd(IDYES) );
		// - initializing the "Skip" button
		SetDlgItemText( IDNO, captionForSkipButton );
		ConvertToCommandLikeButton( GetDlgItemHwnd(IDNO) );
		// - initializing the "Cancel" button
		SetDlgItemText( IDCANCEL, _T("Quit importing") );
		ConvertToCancelLikeButton( GetDlgItemHwnd(IDCANCEL) );
		return result;
	}

	void CFileManagerView::CNameConflictResolutionDialog::DoDataExchange(CDataExchange *pDX){
		// exchange of data from and to controls
		DDX_Check( pDX, ID_APPLY, useForAllSubsequentConflicts );
	}

	LRESULT CFileManagerView::CNameConflictResolutionDialog::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_PAINT:{
				// . base
				__super::WindowProc(msg,wParam,lParam);
				// . drawing curly brackets
				const RECT r1=MapDlgItemClientRect(IDYES), r2=MapDlgItemClientRect(IDNO);
				RECT r={ r1.right+3, r1.top-2, 1000, r2.bottom+2 };
				DrawClosingCurlyBracket( CClientDC(this), r.left, r.top, r.bottom );
				return 0;
			}
		}
		return __super::WindowProc(msg,wParam,lParam);
	}
