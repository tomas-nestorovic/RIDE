#include "stdafx.h"

	#define INI_FILEMANAGER	_T("FileManager")

	#define ORDER_NONE		255
	#define ORDER_ASCENDING	128
	#define ORDER_COLUMN_ID	(~ORDER_ASCENDING)

	void CFileManagerView::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_FILEMANAGER, messageId );
	}








	CFileManagerView::CFileManagerView(PDos _dos,BYTE _supportedDisplayModes,BYTE _initialDisplayMode,const CFont &rFont,BYTE reportModeRowHeightAdjustment,BYTE _nInformation,PCFileInfo _informationList,BYTE _nameColumnId,const TDirectoryStructureManagement *_pDirectoryStructureManagement)
		// ctor
		// - initialization
		: tab( IDR_FILEMANAGER, IDR_FILEMANAGER, ID_FILE, _dos, this )
		, rFont(rFont)
		, reportModeRowHeightAdjustment(reportModeRowHeightAdjustment)
		, nInformation(_nInformation) , informationList(_informationList) , nameColumnId(_nameColumnId)
		, supportedDisplayModes(_supportedDisplayModes) , displayMode(_initialDisplayMode)
		, ordering(ORDER_NONE) , focusedFile(-1) , scrollY(0) , ownedDataSource(nullptr)
		, pDirectoryStructureManagement(_pDirectoryStructureManagement) {
		// - switching to default DisplayMode
/*		const WORD id=displayMode+ID_FILEMANAGER_BIG_ICONS;
		CToolBarCtrl &tb=toolbar.GetToolBarCtrl();
		tb.SetState( id, tb.GetState(id)|TBSTATE_CHECKED );*/
	}

	BEGIN_MESSAGE_MAP(CFileManagerView,CListView)
		ON_WM_CREATE()
		ON_WM_MOUSEACTIVATE()
		ON_WM_CHAR()
		ON_WM_MEASUREITEM_REFLECT()
		ON_COMMAND_RANGE(ID_FILEMANAGER_BIG_ICONS,ID_FILEMANAGER_LIST,__changeDisplayMode__)
			ON_UPDATE_COMMAND_UI_RANGE(ID_FILEMANAGER_BIG_ICONS,ID_FILEMANAGER_LIST,__changeDisplayMode_updateUI__)
		ON_COMMAND(ID_FILEMANAGER_FILE_EDIT,__editNameOfSelectedFile__)
			ON_UPDATE_COMMAND_UI(ID_FILEMANAGER_FILE_EDIT,__imageWritableAndFileSelected_updateUI__)
		ON_NOTIFY_REFLECT(LVN_COLUMNCLICK,__onColumnClick__)
		ON_NOTIFY_REFLECT(NM_DBLCLK,__onDblClick__)
		ON_NOTIFY_REFLECT(LVN_ENDLABELEDIT,__onEndLabelEdit__)
		ON_COMMAND(ID_FILEMANAGER_FILE_COMPARE,__compareFiles__)
		ON_COMMAND(ID_EDIT_SELECT_ALL,__selectAllFilesInCurrentDir__)
		ON_COMMAND(ID_EDIT_SELECT_NONE,__unselectAllFilesInCurrentDir__)
		ON_COMMAND(ID_EDIT_SELECT_INVERSE,__invertSelectionInCurrentDir__)
		ON_COMMAND(ID_FILEMANAGER_FILE_DELETE,__deleteSelectedFilesUponConfirmation__)
			ON_UPDATE_COMMAND_UI(ID_FILEMANAGER_FILE_DELETE,__imageWritableAndFileSelected_updateUI__)
		ON_COMMAND(ID_EDIT_COPY,__copyFilesToClipboard__)
			ON_UPDATE_COMMAND_UI(ID_EDIT_COPY,__fileSelected_updateUI__)
		ON_COMMAND(ID_EDIT_CUT,__cutFilesToClipboard__)
			ON_UPDATE_COMMAND_UI(ID_EDIT_CUT,__imageWritableAndFileSelected_updateUI__)
		ON_COMMAND(ID_EDIT_PASTE,__pasteFilesFromClipboard__)
			ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE,__pasteFiles_updateUI__)
		ON_NOTIFY_REFLECT(LVN_BEGINDRAG,__onBeginDrag__)
		ON_COMMAND(ID_FILEMANAGER_REFRESH,__refreshDisplay__)
		ON_COMMAND(ID_FILEMANAGER_SUBDIR_CREATE,__createSubdirectory__)
			ON_UPDATE_COMMAND_UI(ID_FILEMANAGER_SUBDIR_CREATE,__createSubdirectory_updateUI__)
		ON_COMMAND(ID_FILEMANAGER_DIR_HEXAMODE,__browseCurrentDirInHexaMode__)
		ON_COMMAND(ID_FILEMANAGER_FILE_INFORMATION,__showSelectionProperties__)
			ON_UPDATE_COMMAND_UI(ID_FILEMANAGER_FILE_INFORMATION,__fileSelected_updateUI__)
		ON_WM_DESTROY()
	END_MESSAGE_MAP()

	CFileManagerView::~CFileManagerView(){
		// dtor
		if (ownedDataSource){
			::OleSetClipboard(nullptr);
			delete ownedDataSource;
		}
		while (ownedDirEntryViews.GetCount())
			CTdiCtrl::RemoveTab( TDI_HWND, &((CDirEntriesView *)ownedDirEntryViews.GetHead())->tab );
	}











/*	// commented out due to described reason below
	BOOL CFileManagerView::PreCreateWindow(CREATESTRUCT &cs){
		// adjusting the instantiation
		if (!CListView::PreCreateWindow(cs)) return FALSE;
		//cs.style|=LVS_OWNERDRAWFIXED; // commented out as set in ChangeDisplayMode
		return TRUE;
	}*/

	#define DOS		tab.dos
	#define IMAGE	DOS->image

	void CFileManagerView::OnUpdate(CView *pSender,LPARAM iconType,CObject *icons){
		// request to refresh the display of content
		// - emptying the FileManager
		CListCtrl &lv=GetListCtrl();
		lv.DeleteAllItems();
		nativeOrderOfFiles.RemoveAll(), nativelyLastFile=0;
		// - assigning the list of Icons
		ImageList_Destroy(
			ListView_SetImageList(m_hWnd,icons,iconType) // list of Icons for all DisplayModes but LVS_REPORT
		);
		// - populating the FileManager with new content
		if (const CDos::PDirectoryTraversal pdt=DOS->BeginDirectoryTraversal()){
			while (pdt->GetNextFileOrSubdir())
				switch (pdt->entryType){
					case CDos::TDirectoryTraversal::SUBDIR:
						// subdirectory
					case CDos::TDirectoryTraversal::FILE:
						// File
						__addFileToTheEndOfList__(pdt->entry);
						break;
					case CDos::TDirectoryTraversal::WARNING:
						// error
						//nop (info added to StatusBar in summary below)
						break;
				}
			DOS->EndDirectoryTraversal(pdt);
		}
		// - applying Order to Files
		__order__();
		// - restoring File selection
		__restoreFileSelection__();
		lv.SetItemState(focusedFile,LVIS_FOCUSED,LVIS_FOCUSED);
		// - reporting in StatusBar
		__updateSummaryInStatusBar__();
	}

	void CFileManagerView::__addToTheEndAndSelectFile__(CDos::PFile file){
		// adds the File to the end of the list and selects it
		// - adding to the end
		__addFileToTheEndOfList__(file);
		// - selecting
		CListCtrl &lv=GetListCtrl();
		LVFINDINFO lvdi;
			lvdi.flags=LVFI_PARAM, lvdi.lParam=(LPARAM)file;
		lv.SetItemState( lv.FindItem(&lvdi), LVIS_FOCUSED|LVIS_SELECTED, LVNI_STATEMASK );
		// - scrolling down for the File to become visible
		lv.SendMessage( LVM_SCROLL, 0, 32000 );
	}

	void CFileManagerView::__replaceFileDisplay__(CDos::PCFile fileToHide,CDos::PFile fileToShow){
		// removes FileToHide and replaces it by FileToShow; the FileToShow will become selected and focused
		//if (fileToHide==fileToShow) return; // commented out for also other but Report modes to be refreshed
		CListCtrl &lv=GetListCtrl();
		lv.SetRedraw(FALSE);
			// . removing FileToHide
			LVFINDINFO lvdi;
				lvdi.flags=LVFI_PARAM, lvdi.lParam=(LPARAM)fileToHide;
			lv.DeleteItem( lv.FindItem(&lvdi) );
			// . adding FileToShow to the end of the list
			__addToTheEndAndSelectFile__(fileToShow);
		lv.SetRedraw(TRUE);
	}

	void CFileManagerView::PostNcDestroy(){
		// self-destruction
		//nop (View destroyed by its owner)
	}

	void CFileManagerView::__updateSummaryInStatusBar__() const{
		// creates and in MainWindow's StatusBar shows a report on Files in current Directory
		TStdWinError errFat,errDir;
		float freeSpace; LPCTSTR unit;
		Utils::BytesToHigherUnits( DOS->GetFreeSpaceInBytes(errFat), freeSpace, unit );
		TCHAR buf[200];
		_stprintf(buf,_T("%d files, %.2f %s of free space"),DOS->GetCountOfItemsInCurrentDir(errDir),freeSpace,unit);
		if (errFat!=ERROR_SUCCESS) ::lstrcat( buf, _T(", issues with FAT") );
		if (errDir!=ERROR_SUCCESS) ::lstrcat( buf, _T(", issues with the directory"));
		CMainWindow::__setStatusBarText__(buf);
	}

	#define ORDER_NONE_SYMBOL		' '

	afx_msg int CFileManagerView::OnCreate(LPCREATESTRUCT lpcs){
		// window created
		// - base
		if (CListView::OnCreate(lpcs)==-1) return -1;
		CListCtrl &lv=GetListCtrl();
		// - registering the FileManager as a target of drag&drop
		dropTarget.Register(this);
		DragAcceptFiles(); // to not pass the WM_DROPFILES message to the MainWindow (which would attempt to open the dropped File as an Image)
		// - initializing FileManager with available Information on Files
		TCHAR buf[80];	*buf=ORDER_NONE_SYMBOL;
		PCFileInfo info=informationList;
		for( int i=0; i<nInformation; i++,info++ ){
			::lstrcpy(buf+1,info->informationName);
			lv.InsertColumn( i, buf, info->aligning, info->columnWidthDefault*Utils::LogicalUnitScaleFactor );
		}
		// - populating the FileManager and applying Ordering to Files
		__changeDisplayMode__(displayMode+ID_FILEMANAGER_BIG_ICONS); // calls OnInitialUpdate/OnUpdate
		// - restoring scroll position
		lv.SendMessage( LVM_SCROLL, 0, scrollY );
		// - no File has so far been the target of Drop action
		dropTargetFileId=-1;
		return 0;
	}
	void CFileManagerView::__restoreFileSelection__(){
		// re-selects Files that already have been selected in the past (before switching to another View)
		LVFINDINFO lvdi; lvdi.flags=LVFI_PARAM;
		for( CListCtrl &lv=GetListCtrl(); selectedFiles.GetCount(); ){
			lvdi.lParam=(LPARAM)selectedFiles.RemoveHead();
			lv.SetItemState( lv.FindItem(&lvdi), LVIS_SELECTED, LVNI_STATEMASK );
		}
	}

	#define ERROR_MSG_CANT_CHANGE_DIRECTORY	_T("Cannot change the directory")

	void CFileManagerView::__switchToDirectory__(CDos::PFile directory) const{
		// changes the current Directory
		if (DOS->IsDirectory(directory)){
			const TStdWinError err=(DOS->*pDirectoryStructureManagement->fnChangeCurrentDir)(directory);
			if (err!=ERROR_SUCCESS)
				Utils::FatalError(ERROR_MSG_CANT_CHANGE_DIRECTORY,err);
		}
	}
	TStdWinError CFileManagerView::__switchToDirectory__(PTCHAR path) const{
		// changes the current Directory; assumed that the Path is terminated by backslash; returns Windows standard i/o error
		while (const PTCHAR backslash=_tcschr(path,'\\')){
			// . switching to Subdirectory
			*backslash='\0';
				TCHAR buf[MAX_PATH], *pDot=_tcsrchr(::lstrcpy(buf,path),'.');
				if (pDot) *pDot='\0'; else pDot=_T(".");
				const CDos::PFile subdirectory=DOS->__findFileInCurrDir__(buf,1+pDot,nullptr);
				const TStdWinError err=	subdirectory
										? (DOS->*pDirectoryStructureManagement->fnChangeCurrentDir)(subdirectory)
										: ERROR_FILE_NOT_FOUND;
			*backslash='\\';
			if (err!=ERROR_SUCCESS){
				Utils::FatalError(ERROR_MSG_CANT_CHANGE_DIRECTORY,err);
				return err; // remains switched to the last Subdirectory that could be accessed
			}
			// . next Subdirectory in the Path
			path=1+backslash;
		}
		return ERROR_SUCCESS;
	}

	afx_msg int CFileManagerView::OnMouseActivate(CWnd *topParent,UINT nHitTest,UINT message){
		// activates the window to prevent from stealing the focus by the parent window
		return MA_ACTIVATE;
	}

	afx_msg void CFileManagerView::OnChar(UINT nChar,UINT nRepCnt,UINT nFlags){
		// character processing
		switch (nChar){
			case VK_RETURN:
				// Enter - switching to selected Directory
				if (POSITION pos=GetFirstSelectedFilePosition()){
					__switchToDirectory__(GetNextSelectedFile(pos));
					GetListCtrl().SendMessage( LVM_SCROLL, 0, -__getVerticalScrollPos__() ); // resetting the scroll position to zero pixels
					__refreshDisplay__();
				}
				break;
			default:
				CListView::OnChar(nChar,nRepCnt,nFlags);
		}
	}

	afx_msg void CFileManagerView::MeasureItem(LPMEASUREITEMSTRUCT pmis){
		// determining the row size in Report DisplayMode given the Font size
		LOGFONT lf;
		rFont.GetObject(sizeof(lf),&lf);
		pmis->itemHeight=	( lf.lfHeight<0 ? -lf.lfHeight : lf.lfHeight )
							+
							reportModeRowHeightAdjustment*Utils::LogicalUnitScaleFactor; // e.g., for the underscore "_" to be visible as well
	}

	afx_msg void CFileManagerView::__changeDisplayMode__(UINT id){
		// switches to the specified new DisplayMode
		if (( displayMode=(TDisplayMode)(id-ID_FILEMANAGER_BIG_ICONS) )==LVS_REPORT){
			ModifyStyle( LVS_TYPEMASK|LVS_EDITLABELS, LVS_REPORT|LVS_OWNERDRAWFIXED );
			/*RECT r;
			GetClientRect(&r);
			SetWindowPos( nullptr, 0,0, r.right,r.bottom, SWP_NOMOVE|SWP_NOZORDER ); // to generate WM_MEASUREITEM
			*/
		}else
			ModifyStyle( LVS_TYPEMASK|LVS_OWNERDRAWFIXED, displayMode|LVS_EDITLABELS );
		OnInitialUpdate();
	}
	afx_msg void CFileManagerView::__changeDisplayMode_updateUI__(CCmdUI *pCmdUI){
		// projecting current DisplayMode into UI
		// - ticking currently selected DisplayMode
		pCmdUI->SetCheck( pCmdUI->m_nID-ID_FILEMANAGER_BIG_ICONS==displayMode );
		// - projecting SupportedDisplayModes into UI
		pCmdUI->Enable( ( supportedDisplayModes>>(pCmdUI->m_nID-ID_FILEMANAGER_BIG_ICONS) )&1 );
	}

	afx_msg void CFileManagerView::__imageWritableAndFileSelected_updateUI__(CCmdUI *pCmdUI){
		// projecting File selection into UI
		pCmdUI->Enable( !IMAGE->writeProtected && GetFirstSelectedFilePosition()!=nullptr );
	}

	afx_msg void CFileManagerView::__selectAllFilesInCurrentDir__(){
		// selects all Files in current Directory
		GetListCtrl().SetItemState(-1,LVIS_SELECTED,LVIS_SELECTED);
	}
	afx_msg void CFileManagerView::__unselectAllFilesInCurrentDir__(){
		// unselects all Files in current Directory
		//GetListCtrl().SetItemState(-1,~LVIS_SELECTED,LVIS_SELECTED);
		GetListCtrl().SetItemState( -1, 0, LVNI_STATEMASK );
	}
	afx_msg void CFileManagerView::__invertSelectionInCurrentDir__(){
		// inverts File selection in current Directory
		SetRedraw(FALSE);
			CListCtrl &lv=GetListCtrl();
			for( int i=lv.GetItemCount(); i--; lv.SetItemState( i, ~lv.GetItemState(i,LVIS_SELECTED), LVIS_SELECTED ) );
		SetRedraw(TRUE);
	}

	afx_msg void CFileManagerView::__deleteSelectedFilesUponConfirmation__(){
		// shows confirmation dialog and eventually deletes selected Files
		// - at least one File must be selected
		if (!GetListCtrl().GetSelectedCount()) return;
		// - if Image WriteProtected, no Files can be deleted
		if (IMAGE->__reportWriteProtection__()) return;
		// - deleting upon confirmation
		if (Utils::QuestionYesNo( _T("Selected item(s) will be deleted.\n\nContinue?"), MB_DEFBUTTON2 )){
			// . deleting
			TFileList filesToDelete;
			for( POSITION pos=GetFirstSelectedFilePosition(); pos; filesToDelete.AddTail(GetNextSelectedFile(pos)) );
			__deleteFiles__(filesToDelete);
			if (::IsWindow(m_hWnd)) __refreshDisplay__();
			// . emptying the clipboard
			if (ownedDataSource) ::OleSetClipboard(nullptr);
		}
	}

	void CFileManagerView::__deleteFiles__(TFileList &rFileList){
		// deletes Files in the List
		while (rFileList.GetCount()){ // Files deleted in reversed order ("from rear")
			const CDos::PFile fileToDelete=rFileList.RemoveTail();
			const TStdWinError err=DOS->DeleteFile(fileToDelete);
			if (err!=ERROR_SUCCESS){
				DOS->__showFileProcessingError__(fileToDelete,err);
				break;
			}
		}
	}




	void CFileManagerView::DrawItem(LPDRAWITEMSTRUCT lpdi){
		// draws File item
		// - if no need to draw the item, quit
		if (lpdi->itemID<0) return; // if ListView empty, quit
		if (!(lpdi->itemAction&(ODA_SELECT|ODA_DRAWENTIRE))) return; // if no need to draw, quit
		const HDC dc=lpdi->hDC;
		// - drawing the item background based on item's selection
		const CListCtrl &lv=GetListCtrl();
		if (lpdi->itemState&ODS_SELECTED || lv.GetItemState(lpdi->itemID,LVNI_DROPHILITED)){
			::FillRect( dc, &lpdi->rcItem, CRideBrush::Selection );
			::SetTextColor( dc, COLOR_WHITE );
		}else{
			::FillRect( dc, &lpdi->rcItem, CRideBrush::White );
			::SetTextColor( dc, COLOR_BLACK );
		}
		// - drawing Information
		int tabs[30];
		for( int i=0,pos=0; i<nInformation; i++ )
			tabs[i]= pos+=lv.GetColumnWidth(i) ;
		const HGDIOBJ hFont0=::SelectObject(dc,rFont.m_hObject);
			DrawFileInfo(lpdi,tabs);
		::SelectObject(dc,hFont0);
		// - drawing the focus
		if (lpdi->itemState&ODS_FOCUS)
			::DrawFocusRect( dc, &lpdi->rcItem );
	}

	struct TOrderInfo sealed{
		BYTE ordering;
		RCFileManagerView rFileManager;
	};
	int CALLBACK CFileManagerView::__orderFiles__(LPARAM file1,LPARAM file2,LPARAM orderingInfo){
		// determines the order of given Files based on actually selected Ordering
		const TOrderInfo *const ri=(TOrderInfo *)orderingInfo;
		if (ri->ordering==ORDER_NONE)
			return ri->rFileManager.__getNativeOrderOfFile__((CDos::PCFile)file1)-ri->rFileManager.__getNativeOrderOfFile__((CDos::PCFile)file2);
		else if (ri->ordering&ORDER_ASCENDING)
			return ri->rFileManager.CompareFiles( (CDos::PCFile)file1, (CDos::PCFile)file2, ri->ordering&ORDER_COLUMN_ID );
		else
			return ri->rFileManager.CompareFiles( (CDos::PCFile)file2, (CDos::PCFile)file1, ri->ordering&ORDER_COLUMN_ID  );
	}
	afx_msg void CFileManagerView::__onColumnClick__(NMHDR *pNMHDR,LRESULT *pResult){
		// clicked on column label - ordering Files by given column
		// - removing the Order sign from column it was originally Ordered by (the "+" or "-" symbols)
		if (ordering!=ORDER_NONE){
			CListCtrl &lv=GetListCtrl();
			TCHAR buf[80];
			LVCOLUMN lvc;
				lvc.mask=LVCF_TEXT, lvc.pszText=buf, lvc.cchTextMax=sizeof(buf)/sizeof(TCHAR);
			lv.GetColumn( ordering&ORDER_COLUMN_ID, &lvc );
			*buf=ORDER_NONE_SYMBOL;
			lv.SetColumn( ordering&ORDER_COLUMN_ID, &lvc );
		}
		// - determining the Order type
		const LPNMLISTVIEW pnmlv=(LPNMLISTVIEW)pNMHDR;
		const BYTE columnId=pnmlv->iSubItem;
		if ((ordering&ORDER_COLUMN_ID)!=columnId)
			// Ordering by given column ascending
			ordering=columnId|ORDER_ASCENDING;
		else if (ordering&ORDER_ASCENDING)
			// Ordering by given column descending
			ordering=ordering&~ORDER_ASCENDING;
		else
			// Ordering turned off (keeping Files in the order they were discovered in current Directory)
			ordering=ORDER_NONE;
		// - applying the Ordering to Files
		__order__(); // sets also the Ordering sign for given column (the "+" or "-" symbols)
		*pResult=0;
	}
	void CFileManagerView::__order__() const{
		// performs the Ordering of Files in the FileManager
		CListCtrl &lv=GetListCtrl();
		// - removing the Order sign from column by which it was originally ordered by (the "+" or "-" symbols)
		//nop (see OnColumnClick)
		// - setting the Order sign in column by which it is newly ordered by (the "+" or "-" symbols)
		if (ordering!=ORDER_NONE){
			TCHAR buf[80];
			LVCOLUMN lvc;
				lvc.mask=LVCF_TEXT, lvc.pszText=buf, lvc.cchTextMax=sizeof(buf)/sizeof(TCHAR);
			lv.GetColumn( ordering&ORDER_COLUMN_ID, &lvc );
			*buf= ordering&ORDER_ASCENDING ? '+' : '—' ;
			lv.SetColumn( ordering&ORDER_COLUMN_ID, &lvc );
		}
		// - ordering
		const TOrderInfo ri={ ordering, *this };
		lv.SortItems(__orderFiles__,(LPARAM)&ri);
	}

	WORD CFileManagerView::__getNativeOrderOfFile__(CDos::PCFile file) const{
		// determines and returns the native order of given File (i.e. the order in which it was discovered in current Directory)
		WORD result;
		if (!nativeOrderOfFiles.Lookup((PVOID)file,result))
			ASSERT(FALSE);
		return result;
	}

	void CFileManagerView::__addFileToTheEndOfList__(CDos::PCFile file){
		// adds given File to the end of the list
		TCHAR bufName[MAX_PATH];
		LVITEM lvi={ LVIF_TEXT|LVIF_PARAM, nativelyLastFile++ };
			nativeOrderOfFiles[(PVOID)file]=lvi.iItem;
			lvi.pszText=DOS->GetFileNameWithAppendedExt( (CDos::PCFile)( lvi.lParam=(LPARAM)file ), bufName );
		GetListCtrl().InsertItem(&lvi);
	}

	bool CFileManagerView::__markDirectorySectorAsDirty__(PVOID dirEntry,int){
		// marks Directory Sector that contains specified DirectoryEntry as "dirty"
		CDos::__getFocused__()->__markDirectorySectorAsDirty__(dirEntry);
		return true;
	}

	int CFileManagerView::__getVerticalScrollPos__() const{
		// computes and returns the number of pixels scrolled to vertically
		const CListCtrl &lv=GetListCtrl();
		RECT r;
		lv.GetItemRect(0,&r,LVIR_BOUNDS);
		return lv.GetScrollPos(SB_VERT)*(r.bottom-r.top);
	}

	afx_msg void CFileManagerView::OnDestroy(){
		// window destroyed
		dropTarget.Revoke();
		// - saving the scroll position for later
		scrollY=__getVerticalScrollPos__();
		//scrollToIndex=lv.GetTopIndex();
		// - storing current File selection into auxiliary list
		const CListCtrl &lv=GetListCtrl();
		for( POSITION pos=lv.GetFirstSelectedItemPosition(); pos; selectedFiles.AddTail((PVOID)lv.GetItemData(lv.GetNextSelectedItem(pos))) );
		// - storing currently FocusedFile
		focusedFile=-1; // assumption (no File in Focus)
		for( int n=lv.GetItemCount(); n--; )
			if (lv.GetItemState(n,LVIS_FOCUSED)){ focusedFile=n; break; }
		// - destroying the FileComparisonDialog (if shown)
		if (CFileComparisonDialog::pSingleInstance)
			CFileComparisonDialog::pSingleInstance->SendMessage( WM_COMMAND, IDCANCEL, 0 );
		// - base
		CListView::OnDestroy();
	}

	POSITION CFileManagerView::GetFirstSelectedFilePosition() const{
		// returns the position of first selected File
		if (::IsWindow(m_hWnd))
			return GetListCtrl().GetFirstSelectedItemPosition();
		else
			return selectedFiles.GetHeadPosition();
	}
	CDos::PFile CFileManagerView::GetNextSelectedFile(POSITION &pos) const{
		// returns next selected File
		if (::IsWindow(m_hWnd)){
			const CListCtrl &lv=GetListCtrl();
			return (CDos::PFile)lv.GetItemData( lv.GetNextSelectedItem(pos) );
		}else
			return (CDos::PFile)selectedFiles.GetNext(pos);
	}
	POSITION CFileManagerView::GetLastSelectedFilePosition() const{
		// returns the position of last selected File
		if (::IsWindow(m_hWnd)){
			const CListCtrl &lv=GetListCtrl();
			POSITION result=lv.GetFirstSelectedItemPosition();
			for( POSITION p=result; p; lv.GetNextSelectedItem(p) )
				result=p;
			return result;
		}else
			return selectedFiles.GetTailPosition();
	}
	CDos::PFile CFileManagerView::GetPreviousSelectedFile(POSITION &pos) const{
		// returns previous selected File
		if (::IsWindow(m_hWnd)){
			const CListCtrl &lv=GetListCtrl();
			POSITION prev=nullptr;
			for( POSITION p=lv.GetFirstSelectedItemPosition(); p!=pos; lv.GetNextSelectedItem(p) )
				prev=p;
			const CDos::PFile result=(CDos::PFile)lv.GetItemData( lv.GetNextSelectedItem(pos) );
			pos=prev;
			return result;
		}else
			return (CDos::PFile)selectedFiles.GetPrev(pos);
	}
	DWORD CFileManagerView::GetCountOfSelectedFiles() const{
		// returns the number of selected Files
		if (::IsWindow(m_hWnd))
			return GetListCtrl().GetSelectedCount();
		else
			return selectedFiles.GetCount();
	}




	afx_msg void CFileManagerView::__createSubdirectory__(){
		// initiates the creation of Subdirectory in current Directory
		// - if Image WriteProtected, quit
		if (IMAGE->__reportWriteProtection__()) return;
		// - definition and instantiation of Dialog
		class CNewSubdirectoryDialog sealed:public CDialog{
			const PDos dos;

			void OnOK() override{
				// the Name of new Subdirectory confirmed
				TCHAR name[MAX_PATH];
				GetDlgItemText( ID_DIRECTORY, name, MAX_PATH );
				if (const TStdWinError err=(dos->*dos->pFileManager->pDirectoryStructureManagement->fnCreateSubdir)(name,FILE_ATTRIBUTE_DIRECTORY,subdirectory))
					Utils::Information(_T("Cannot create the directory"),err);
				else{
					FILETIME ft;
					::GetSystemTimeAsFileTime(&ft);
					dos->SetFileTimeStamps( subdirectory, &ft, &ft, &ft );
					CDialog::OnOK();
				}
			}
		public:
			CDos::PFile subdirectory;

			CNewSubdirectoryDialog(PDos _dos)
				// ctor
				: CDialog(IDR_FILEMANAGER_SUBDIR_NEW) , dos(_dos) {
			}
		} d(DOS);
		// - showing the Dialog and processing its result (plus updating the FileManager)
		if (d.DoModal()==IDOK){ // Subdirectory created - showing it
			__unselectAllFilesInCurrentDir__(); // cancelling any existing selection
			__addToTheEndAndSelectFile__(d.subdirectory);
		}
	}
	afx_msg void CFileManagerView::__createSubdirectory_updateUI__(CCmdUI *pCmdUI) const{
		// projecting possibility to create Subdirectories into UI
		pCmdUI->Enable(pDirectoryStructureManagement!=nullptr);
	}

	static void WINAPI __onDirEntriesViewClosing__(LPCVOID tab){
		delete ((CMainWindow::CTdiView::PTab)tab)->view;
	}
	afx_msg void CFileManagerView::__browseCurrentDirInHexaMode__(){
		// opens a new Tab with DirectoryEntries listed in an HexaEditor instance
		CDirEntriesView *const deView=new CDirEntriesView( DOS, DOS->currentDir );
		TCHAR label[80+MAX_PATH];
		::wsprintf( label, _T("Dir \"%s\""), DOS->GetFileNameWithAppendedExt(DOS->currentDir,label+80) );
		CTdiCtrl::AddTabLast( TDI_HWND, label, &deView->tab, true, TDI_TAB_CANCLOSE_ALWAYS, __onDirEntriesViewClosing__ );
		ownedDirEntryViews.AddTail(deView);
	}



	struct TSelectionStatistics sealed{
		RCFileManagerView rFileManager;
		const TCylinder finishedState;
		DWORD nFiles, nDirectories, totalSizeInBytes, totalSizeOnDiskInBytes;

		TSelectionStatistics(RCFileManagerView rFileManager)
			// ctor
			: rFileManager(rFileManager) , nFiles(0) , nDirectories(0) , totalSizeInBytes(0) , totalSizeOnDiskInBytes(0) , finishedState(1+rFileManager.IMAGE->GetCylinderCount()) {
		}
	};

	UINT AFX_CDECL CFileManagerView::__selectionPropertyStatistics_thread__(PVOID _pCancelableAction){
		// collects Statistics on current Selection
		const TBackgroundActionCancelable *const pAction=(TBackgroundActionCancelable *)_pCancelableAction;
		// - processing recurrently (see below Collector's ctor)
		struct TStatisticsCollector sealed{
			typedef const struct TDirectoryEtc sealed{
				DWORD dirId;
				const TDirectoryEtc *parent;
			} *PCDirectoryEtc;

			TSelectionStatistics &rStatistics;
			RCFileManagerView rFileManager;
			const TBackgroundActionCancelable *const pAction;
			const PDos dos;
			const CFileManagerView::TDirectoryStructureManagement *const pDirStructMan;
			TCylinder state;

			TStatisticsCollector(const TBackgroundActionCancelable *pAction)
				// ctor
				// . initialization
				: rStatistics(*(TSelectionStatistics *)pAction->fnParams) , rFileManager(rStatistics.rFileManager) , pAction(pAction) , dos(rFileManager.DOS) , pDirStructMan(rFileManager.pDirectoryStructureManagement) , state(0) {
				// . recurrently collecting Statistics on selected Files
				const CDos::PFile currentDirectory=dos->currentDir;
				const TDirectoryEtc dirEtc={ dos->currentDirId, nullptr };
				for( POSITION pos=rFileManager.GetFirstSelectedFilePosition(); pos; ){
					if (!pAction->bContinue) break;
					if (const CDos::PDirectoryTraversal pdt=dos->BeginDirectoryTraversal(currentDirectory)){
						for( const CDos::PCFile file=rFileManager.GetNextSelectedFile(pos); pdt->GetNextFileOrSubdir()!=file; );
						__countInFile__(pdt,&dirEtc);
						dos->EndDirectoryTraversal(pdt);
					}
				}
				if (pDirStructMan!=nullptr)
					(dos->*pDirStructMan->fnChangeCurrentDir)(currentDirectory); // because it could recurrently be switched to another but CurrentDirectory
			}

			void __countInFile__(CDos::PDirectoryTraversal pdt,PCDirectoryEtc dirPath){
				// counts the File into Statistics; recurrently processes Subdirectories
				if (pdt->entryType==CDos::TDirectoryTraversal::FILE)
					// File
					rStatistics.nFiles++, rStatistics.totalSizeInBytes+=dos->GetFileOfficialSize(pdt->entry), rStatistics.totalSizeOnDiskInBytes+=dos->GetFileSizeOnDisk(pdt->entry);
				else{
					// Directory - processing it recurrently
					// . switching to the Directory
					if ((dos->*pDirStructMan->fnChangeCurrentDir)(pdt->entry)!=ERROR_SUCCESS)
						return;
					// . preventing from cycling infinitely (e.g. because of encountering a dotdot entry ".." in MS-DOS Directory)
					const TDirectoryEtc dirEtc={ dos->currentDirId, dirPath };
					for( PCDirectoryEtc pde=dirPath; pde!=nullptr; pde=pde->parent )
						if (pde->dirId==dirEtc.dirId) return;
					// . involving Directory into Statistics
					if (const CDos::PDirectoryTraversal subPdt=dos->BeginDirectoryTraversal(pdt->entry)){
						for( rStatistics.nDirectories++,rStatistics.totalSizeInBytes+=dos->GetFileOfficialSize(pdt->entry),rStatistics.totalSizeOnDiskInBytes+=dos->GetFileSizeOnDisk(pdt->entry); subPdt->GetNextFileOrSubdir()!=nullptr; ){
							if (!pAction->bContinue) break;
							if (subPdt->entryType!=CDos::TDirectoryTraversal::WARNING)
								__countInFile__(subPdt,&dirEtc);
						}
						pAction->UpdateProgress( state=max(state,subPdt->chs.cylinder) );
						dos->EndDirectoryTraversal(subPdt);
					}
				}
			}
		} tmp(pAction);
		// - Statistics collected
		pAction->UpdateProgress(tmp.rStatistics.finishedState);
		return pAction->bContinue ? ERROR_SUCCESS : ERROR_CANCELLED;
	}

	afx_msg void CFileManagerView::__showSelectionProperties__() const{
		// shows properties of currently selected Files (and Directories)
		TSelectionStatistics statistics(*this);
		TBackgroundActionCancelable bac( __selectionPropertyStatistics_thread__, &statistics, THREAD_PRIORITY_BELOW_NORMAL );
		if (bac.CarryOut(statistics.finishedState)==ERROR_SUCCESS){
			TCHAR buf[1024];
			::wsprintf( buf, _T("SELECTION Properties\n\n- recurrently selected %d file(s), %d folders\n- of %d Bytes in total size\n- which occupy %d Bytes on the disk."), statistics.nFiles, statistics.nDirectories, statistics.totalSizeInBytes, statistics.totalSizeOnDiskInBytes );
			Utils::Information(buf);
		}
	}

	afx_msg void CFileManagerView::__refreshDisplay__(){
		// refreshing the View
		// - saving the scroll position for later
		scrollY=__getVerticalScrollPos__();
		// - refreshing
		OnUpdate(nullptr,0,nullptr);
		// - restoring the scroll position
		GetListCtrl().SendMessage( LVM_SCROLL, 0, scrollY );
	}




/*
- drag drop
	. zprava po dokonceni exportu (drop i paste)
- wchar
- zobrazeni obrazku
	. DirectShow
- zobrazeni vybranych v mape stop
	. mark in the map
	. klavesove skrolovani
	. fatPath souboru spolecne s chybou
X upravy limitnich hodnot gkfm v pg
- mdos += defragmentace
*/
