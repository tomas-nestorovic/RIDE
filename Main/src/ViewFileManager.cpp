#include "stdafx.h"
#include "ViewFatHexa.h"

	const CFileManagerView *CFileManagerView::pCurrentlyShown;

	#define INI_FILEMANAGER	_T("FileManager")

	#define INI_MSG_CAPABILITIES _T("fmcaps")
	#define INI_MSG_DIR_GO_BACK	_T("fmparent")

	#define ORDER_NONE		255
	#define ORDER_ASCENDING	128
	#define ORDER_FILEINFO_ID	(~ORDER_ASCENDING)

	void CFileManagerView::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_FILEMANAGER, messageId );
	}








	CFileManagerView::CFileManagerView(PDos _dos,BYTE _supportedDisplayModes,BYTE _initialDisplayMode,const Utils::CRideFont &font,BYTE reportModeRowHeightAdjustment,BYTE _nInformation,PCFileInfo _informationList,PCDirectoryStructureManagement pDirectoryStructureManagement)
		// ctor
		// - initialization
		: tab( IDR_FILEMANAGER, IDR_FILEMANAGER, ID_FILE, _dos->image, this )
		, font(font)
		, reportModeRowHeightAdjustment(reportModeRowHeightAdjustment)
		, nInformation(_nInformation) , informationList(_informationList)
		, supportedDisplayModes(_supportedDisplayModes) , displayMode(_initialDisplayMode)
		, reportModeDisplayedInfosPrev(0) // no columns have been shown previously ...
		, reportModeDisplayedInfos(-1) // ... and now wanting to show them all
		, ordering(ORDER_NONE) , focusedFile(nullptr) , scrollY(0) , ownedDataSource(nullptr)
		, mnuGeneralContext(IDR_FILEMANAGER_GENERAL_CONTEXT)
		, mnuFocusedContext(IDR_FILEMANAGER_FOCUSED_CONTEXT)
		, fatEntryYahelDefinition(nullptr) // not supported
		, informOnCapabilities(true)
		, pDirectoryStructureManagement(pDirectoryStructureManagement) {
		// - switching to default DisplayMode
/*		const WORD id=displayMode+ID_FILEMANAGER_BIG_ICONS;
		CToolBarCtrl &tb=toolbar.GetToolBarCtrl();
		tb.SetState( id, tb.GetState(id)|TBSTATE_CHECKED );*/
	}

	BEGIN_MESSAGE_MAP(CFileManagerView,CListView)
		ON_WM_CREATE()
		ON_WM_MOUSEACTIVATE()
		ON_WM_CHAR()
		ON_WM_CONTEXTMENU()
		ON_WM_MEASUREITEM_REFLECT()
		ON_COMMAND_RANGE(ID_FILEMANAGER_BIG_ICONS,ID_FILEMANAGER_LIST,__changeDisplayMode__)
			ON_UPDATE_COMMAND_UI_RANGE(ID_FILEMANAGER_BIG_ICONS,ID_FILEMANAGER_LIST,__changeDisplayMode_updateUI__)
		ON_COMMAND(ID_FILEMANAGER_FILE_EDIT,__editNameOfSelectedFile__)
			ON_UPDATE_COMMAND_UI(ID_FILEMANAGER_FILE_EDIT,__imageWritableAndFileSelected_updateUI__)
		ON_NOTIFY_REFLECT(LVN_COLUMNCLICK,__onColumnClick__)
		ON_NOTIFY_REFLECT(NM_DBLCLK,__onDblClick__)
		ON_COMMAND(ID_NAVIGATE_UP,__navigateBack__)
			ON_UPDATE_COMMAND_UI(ID_NAVIGATE_UP,__navigateBack_updateUI__)
		ON_NOTIFY_REFLECT(LVN_ENDLABELEDITW,__onEndLabelEdit__)
		ON_COMMAND(ID_FILEMANAGER_FILE_COMPARE,__compareFiles__)
		ON_COMMAND(ID_EDIT_SELECT_ALL,__selectAllFilesInCurrentDir__)
		ON_COMMAND(ID_EDIT_SELECT_NONE,__unselectAllFilesInCurrentDir__)
		ON_COMMAND(ID_EDIT_SELECT_INVERSE,__invertSelectionInCurrentDir__)
		ON_COMMAND(ID_EDIT_SELECT_TOGGLE,__toggleFocusedItemSelection__)
		ON_COMMAND(ID_FILEMANAGER_FILE_DELETE,__deleteSelectedFilesUponConfirmation__)
			ON_UPDATE_COMMAND_UI(ID_FILEMANAGER_FILE_DELETE,__imageWritableAndFileSelected_updateUI__)
		ON_COMMAND(ID_EDIT_COPY,__copyFilesToClipboard__)
			ON_UPDATE_COMMAND_UI(ID_EDIT_COPY,__fileSelected_updateUI__)
		ON_COMMAND(ID_EDIT_CUT,__cutFilesToClipboard__)
			ON_UPDATE_COMMAND_UI(ID_EDIT_CUT,__imageWritableAndFileSelected_updateUI__)
		ON_COMMAND(ID_EDIT_PASTE,__pasteFilesFromClipboard__)
			ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE,__pasteFiles_updateUI__)
		ON_NOTIFY_REFLECT(LVN_BEGINDRAG,__onBeginDrag__)
		ON_COMMAND(ID_FILEMANAGER_REFRESH,RefreshDisplay)
		ON_COMMAND(ID_FILEMANAGER_SUBDIR_CREATE,__createSubdirectory__)
			ON_UPDATE_COMMAND_UI(ID_FILEMANAGER_SUBDIR_CREATE,__createSubdirectory_updateUI__)
		ON_COMMAND(ID_DIRECTORY,GoToFocusedFileDirectoryEntry)
		ON_COMMAND(ID_FAT,BrowseFocusedFileFatLinkage)
			ON_UPDATE_COMMAND_UI(ID_FAT,BrowseFocusedFileFatLinkage_updateUI)
		ON_COMMAND(ID_FILEMANAGER_DIR_HEXAMODE,BrowseCurrentDirInHexaMode)
		ON_COMMAND(ID_SECTOR,GoToFocusedFileFirstSector)
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
		if (*app.m_pMainWnd){ // app NOT closing
			while (ownedTabs.GetCount())
				CTdiCtrl::RemoveTab( TDI_HWND, ownedTabs.RemoveHead() );
			while (ownedWindows.GetCount())
				ownedWindows.RemoveHead()->DestroyWindow();
		}
	}











/*	// commented out due to described reason below
	BOOL CFileManagerView::PreCreateWindow(CREATESTRUCT &cs){
		// adjusting the instantiation
		if (!__super::PreCreateWindow(cs)) return FALSE;
		//cs.style|=LVS_OWNERDRAWFIXED; // commented out as set in ChangeDisplayMode
		return TRUE;
	}*/

	#define IMAGE	tab.image
	#define DOS		IMAGE->dos

	#define ORDER_NONE_SYMBOL		' '

	void CFileManagerView::OnUpdate(CView *pSender,LPARAM iconType,CObject *icons){
		// request to refresh the display of content
		// - base
		__super::OnUpdate( pSender, iconType, icons );
		// - emptying the FileManager
		CListCtrl &lv=GetListCtrl();
		lv.DeleteAllItems();
		nativeOrderOfFiles.RemoveAll(), nativelyLastFile=0;
		// - assigning the list of Icons
		ImageList_Destroy(
			ListView_SetImageList(m_hWnd,icons,iconType) // list of Icons for all DisplayModes but LVS_REPORT
		);
		// - displaying Information on Files in individual columns
		if (reportModeDisplayedInfos!=reportModeDisplayedInfosPrev){
			// . removing all previous columns
			while (lv.DeleteColumn(0));
			// . adding a new set of columns
			TCHAR buf[80];	*buf=ORDER_NONE_SYMBOL;
			PCFileInfo info=informationList;
			for( int i=0; i<nInformation; i++,info++ )
				if ((reportModeDisplayedInfos&1<<i)!=0){
					::lstrcpy(buf+1,info->informationName);
					lv.InsertColumn( i, buf, info->flags&(TFileInfo::AlignLeft|TFileInfo::AlignRight), Utils::LogicalUnitScaleFactor*info->columnWidthDefault );
				}
			reportModeDisplayedInfosPrev=reportModeDisplayedInfos;
		}
		// - populating the FileManager with new content
		if (const auto pdt=DOS->BeginDirectoryTraversal())
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
		// - applying Order to Files
		__order__();
		// - restoring File selection
		__restoreFileSelection__();
		LVFINDINFO lvdi; lvdi.flags=LVFI_PARAM;
		lvdi.lParam=(LPARAM)focusedFile;
		lv.SetItemState( lv.FindItem(&lvdi), LVIS_FOCUSED, LVNI_FOCUSED );
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
		TStdWinError errFat=ERROR_SUCCESS, errDir=ERROR_SUCCESS;
		TCHAR buf[200];
		::wsprintf( buf,
			_T("%d files, %s of free space%s%s"),
			DOS->GetCountOfItemsInCurrentDir(errDir), Utils::BytesToHigherUnits(DOS->GetFreeSpaceInBytes(errFat)),
			errFat ? _T(", issues with FAT") : _T(""),
			errDir ? _T(", issues with the directory") : _T("")
		);
		CMainWindow::__setStatusBarText__(buf);
	}

	BOOL CFileManagerView::Create(LPCTSTR lpszClassName,LPCTSTR lpszWindowName,DWORD dwStyle,const RECT &rect,CWnd *pParentWnd,UINT nID,CCreateContext *pContext){
		// window creation
		// - base
		const BOOL result=__super::Create( lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext );
		// - informing on FileManager's capabilities
		if (informOnCapabilities){
			informOnCapabilities=false;
			TDI_INSTANCE->RedrawWindow(); // make sure TDI's whole client area is up-to-date before showing the following message
			__informationWithCheckableShowNoMore__( _T("After unlocking the image for writing, work with the \"") FILE_MANAGER_TAB_LABEL _T("\" tab as you would in Explorer, renaming, copying, pasting, moving, and deleting files."), INI_MSG_CAPABILITIES );
		}
		return result;
	}

	afx_msg int CFileManagerView::OnCreate(LPCREATESTRUCT lpcs){
		// window created
		// - base
		if (__super::OnCreate(lpcs)==-1) return -1;
		CListCtrl &lv=GetListCtrl();
		// - registering the FileManager as a target of drag&drop
		dropTarget.Register(this);
		DragAcceptFiles(); // to not pass the WM_DROPFILES message to the MainWindow (which would attempt to open the dropped File as an Image)
		// - populating the FileManager and applying Ordering to Files
		app.GetMainWindow()->SetActiveView(this); // so that all File Manager's initialization chores are performed, e.g. MeasureItem method called
		__changeDisplayMode__(displayMode+ID_FILEMANAGER_BIG_ICONS); // calls OnInitialUpdate/OnUpdate
		// - restoring scroll position
		lv.SendMessage( LVM_SCROLL, 0, scrollY );
		// - no File has so far been the target of Drop action
		dropTargetFileId=-1;
		// - currently it's this FileManager that's displayed
		pCurrentlyShown=this;
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

	void CFileManagerView::RevokeOwnership(CWnd *pWnd){
		if (const POSITION pos=ownedWindows.Find(pWnd))
			ownedWindows.RemoveAt(pos);
	}

	#define ERROR_MSG_CANT_CHANGE_DIRECTORY	_T("Cannot change the directory")

	void CFileManagerView::__switchToDirectory__(CDos::PFile directory) const{
		// changes the current Directory
		if (DOS->IsDirectory(directory))
			if (const TStdWinError err=(DOS->*pDirectoryStructureManagement->fnChangeCurrentDir)(directory))
				Utils::FatalError(ERROR_MSG_CANT_CHANGE_DIRECTORY,err);
	}

	/*TStdWinError CFileManagerView::__switchToDirectory__(PTCHAR path) const{
		// changes the current Directory; assumed that the Path is terminated by backslash; returns Windows standard i/o error
		while (const PTCHAR backslash=::StrChr(path,'\\')){
			// . switching to Subdirectory
			*backslash='\0';
				TCHAR buf[MAX_PATH], *pDot=_tcsrchr(::lstrcpy(buf,path),'.');
				if (pDot) *pDot='\0'; else pDot=_T(".");
				const CDos::PFile subdirectory=DOS->FindFileInCurrentDir(buf,1+pDot,nullptr);
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
	}*/

	void CFileManagerView::SwitchToDirectory(CDos::PFile directory){
		// puts current Directory onto top of PreviousDirectories stack and switches to the Directory specified
		previousDirectories.AddHead(DOS->currentDir);
		__switchToDirectory__(directory);
		if (::GetWindowThreadProcessId(m_hWnd,nullptr)==::GetCurrentThreadId()){
			// can change controls only from within the thread that created them
			GetListCtrl().SendMessage( LVM_SCROLL, 0, -__getVerticalScrollPos__() ); // resetting the scroll position to zero pixels
			RefreshDisplay();
			__informationWithCheckableShowNoMore__( _T("See \"") FILE_MANAGER_TAB_LABEL _T("\" menu on how to navigate back."), INI_MSG_DIR_GO_BACK );
		}
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
					const CDos::PFile item=GetNextSelectedFile(pos);
					if (DOS->IsDirectory(item))
						SwitchToDirectory(item);
				}
				break;
			default:
				__super::OnChar(nChar,nRepCnt,nFlags);
		}
	}

	afx_msg void CFileManagerView::OnContextMenu(CWnd *pWndRightClicked,CPoint point){
		// right mouse button released
		__super::OnContextMenu( pWndRightClicked, point );
		CListCtrl &lv=GetListCtrl();
		RECT rcHeader;
		lv.GetHeaderCtrl()->GetClientRect(&rcHeader);
		Utils::CRideContextMenu *pContextMenu;
		if (!GetCountOfSelectedFiles()){
			// no Files selected
			pContextMenu=&mnuGeneralContext;
			if ((point.x|point.y)<0){ // context menu invoked via keyboard?
				point.x=rcHeader.left, point.y=rcHeader.bottom; // show context menu just under the header
				ClientToScreen(&point);
			}
		}else{
			// some Files selected
			pContextMenu=&mnuFocusedContext;
			if ((point.x|point.y)<0){ // context menu invoked via keyboard?
				for( POSITION pos=lv.GetFirstSelectedItemPosition(); pos; ){
					int iSelected=lv.GetNextSelectedItem(pos);
					lv.GetItemPosition( iSelected, &point );
					if (point.y>=rcHeader.bottom)
						break; // show context menu at first visible File
				}
				ClientToScreen(&point);
			}
		}
		pContextMenu->UpdateUi(this);
		SendMessage(
			WM_COMMAND,
			pContextMenu->TrackPopupMenu( TPM_RETURNCMD, point.x, point.y, this )
		);
	}

	afx_msg void CFileManagerView::MeasureItem(LPMEASUREITEMSTRUCT pmis){
		// determining the row size in Report DisplayMode given the Font size
		LOGFONT lf;
		font.GetObject(sizeof(lf),&lf);
		pmis->itemHeight=	( lf.lfHeight<0 ? -lf.lfHeight : lf.lfHeight )
							+
							Utils::LogicalUnitScaleFactor*reportModeRowHeightAdjustment; // e.g., for the underscore "_" to be visible as well
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

	afx_msg void CFileManagerView::__toggleFocusedItemSelection__(){
		// toggles selection status of currently focused File, and moves focus to the next File
		CListCtrl &lv=GetListCtrl();
		if (const int nItems=lv.GetItemCount()){
			int iFocused=std::max( 0, lv.GetNextItem(-1,LVNI_FOCUSED) );
			if (lv.GetSelectedCount()!=1 || lv.GetNextItem(-1,LVNI_SELECTED)!=iFocused) // A|B; A = multiple items already selected (maybe by sequentially toggling their statuses), B = the focused and first selected items are different (user selected one item, now wants to select another)
				lv.SetItemState( iFocused, ~lv.GetItemState(iFocused,LVIS_SELECTED), LVIS_SELECTED );
			lv.SetItemState( std::min(++iFocused,nItems-1), LVIS_FOCUSED, LVIS_FOCUSED );
		}
	}

	afx_msg void CFileManagerView::__deleteSelectedFilesUponConfirmation__(){
		// shows confirmation dialog and eventually deletes selected Files
		// - at least one File must be selected
		if (!GetListCtrl().GetSelectedCount()) return;
		// - if Image WriteProtected, no Files can be deleted
		if (IMAGE->ReportWriteProtection()) return;
		// - deleting upon confirmation
		if (Utils::QuestionYesNo( _T("Selected item(s) will be deleted.\n\nContinue?"), MB_DEFBUTTON2 )){
			// . deleting
			CFileList filesToDelete;
			for( POSITION pos=GetFirstSelectedFilePosition(); pos; filesToDelete.AddTail(GetNextSelectedFile(pos)) );
			__deleteFiles__(filesToDelete);
			if (::IsWindow(m_hWnd)) RefreshDisplay();
			// . emptying the clipboard
			if (ownedDataSource) ::OleSetClipboard(nullptr);
		}
	}

	void CFileManagerView::__deleteFiles__(CFileList &rFileList){
		// deletes Files in the List
		while (rFileList.GetCount()){ // Files deleted in reversed order ("from rear")
			const CDos::PFile fileToDelete=rFileList.RemoveTail();
			if (const TStdWinError err=DOS->DeleteFile(fileToDelete)){
				DOS->ShowFileProcessingError(fileToDelete,err);
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
			::SetBkColor( dc, Utils::CRideBrush::Selection );
			::FillRect( dc, &lpdi->rcItem, Utils::CRideBrush::Selection );
			::SetTextColor( dc, COLOR_WHITE );
		}else{
			::SetBkColor( dc, COLOR_WHITE );
			::FillRect( dc, &lpdi->rcItem, Utils::CRideBrush::White );
			::SetTextColor( dc, COLOR_BLACK );
		}
		// - drawing Information
		const HGDIOBJ hFont0=::SelectObject(dc,font);
			DRAWITEMSTRUCT dis=*lpdi;
			for( BYTE i=0,iColumn=0; i<nInformation; i++ )
				if (reportModeDisplayedInfos&1<<i){
					dis.rcItem.right=dis.rcItem.left+lv.GetColumnWidth(iColumn++);
					const RECT tmp=dis.rcItem;
						DrawReportModeCell( informationList+i, &dis );
					dis.rcItem=tmp;
					dis.rcItem.left=dis.rcItem.right;
				}
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
			return ri->rFileManager.CompareFiles( (CDos::PCFile)file1, (CDos::PCFile)file2, ri->ordering&ORDER_FILEINFO_ID );
		else
			return ri->rFileManager.CompareFiles( (CDos::PCFile)file2, (CDos::PCFile)file1, ri->ordering&ORDER_FILEINFO_ID );
	}
	afx_msg void CFileManagerView::__onColumnClick__(NMHDR *pNMHDR,LRESULT *pResult){
		// clicked on column label - ordering Files by given column
		// - removing the Order sign from column it was originally Ordered by (the "+" or "-" symbols)
		if (ordering!=ORDER_NONE){
			const char columnId=__columnIdFromFileInfo__( ordering&ORDER_FILEINFO_ID );
			if (columnId>=0){
				CListCtrl &lv=GetListCtrl();
				TCHAR buf[80];
				LVCOLUMN lvc;
					lvc.mask=LVCF_TEXT, lvc.pszText=buf, lvc.cchTextMax=ARRAYSIZE(buf);
				lv.GetColumn( columnId, &lvc );
				*buf=ORDER_NONE_SYMBOL;
				lv.SetColumn( columnId, &lvc );
			}
		}
		// - determining the Order type
		const LPNMLISTVIEW pnmlv=(LPNMLISTVIEW)pNMHDR;
		const BYTE fileInfoIndex=__fileInfoFromColumnId__(pnmlv->iSubItem)-informationList;
		if ((ordering&ORDER_FILEINFO_ID)!=fileInfoIndex)
			// Ordering by given column ascending
			ordering=fileInfoIndex|ORDER_ASCENDING;
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
			const char columnId=__columnIdFromFileInfo__( ordering&ORDER_FILEINFO_ID );
			if (columnId>=0){
				TCHAR buf[80];
				LVCOLUMN lvc;
					lvc.mask=LVCF_TEXT, lvc.pszText=buf, lvc.cchTextMax=ARRAYSIZE(buf);
				lv.GetColumn( columnId, &lvc );
				*buf= ordering&ORDER_ASCENDING ? '+' : '–' ;
				lv.SetColumn( columnId, &lvc );
			}
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
		const CDos::CPathString name=DOS->GetFilePresentationNameAndExt(file);
		LVITEMW lvi={ LVIF_TEXT|LVIF_PARAM, nativelyLastFile++ };
			nativeOrderOfFiles[(PVOID)file]=lvi.iItem;
			lvi.lParam=(LPARAM)file;
			lvi.pszText=const_cast<PWCHAR>( name.GetUnicode() );
		GetListCtrl().SendMessage( LVM_INSERTITEMW, 0, (LPARAM)&lvi );
	}

	void CFileManagerView::__markDirectorySectorAsDirty__(PVOID dirEntry){
		// marks Directory Sector that contains specified DirectoryEntry as "dirty"
		CDos::GetFocused()->MarkDirectorySectorAsDirty(dirEntry);
	}

	bool CFileManagerView::__markDirectorySectorAsDirty__(PVOID dirEntry,int){
		// marks Directory Sector that contains specified DirectoryEntry as "dirty"
		__markDirectorySectorAsDirty__(dirEntry);
		return true;
	}

	int CFileManagerView::__getVerticalScrollPos__() const{
		// computes and returns the number of pixels scrolled to vertically
		const CListCtrl &lv=GetListCtrl();
		RECT r;
		lv.GetItemRect(0,&r,LVIR_BOUNDS);
		return lv.GetScrollPos(SB_VERT)*(r.bottom-r.top);
	}

	CFileManagerView::PCFileInfo CFileManagerView::__fileInfoFromColumnId__(BYTE columnId) const{
		// for a specified column index determines and returns FileInfo structure 
		for( BYTE i=0; i<nInformation; i++ )
			if ((reportModeDisplayedInfos&1<<i)!=0)
				if (!columnId)
					return informationList+i;
				else
					columnId--;
		ASSERT(FALSE); // we should never end up here, but just to be sure
		return nullptr;
	}

	char CFileManagerView::__columnIdFromFileInfo__(BYTE fileInfoIndex) const{
		// determines and returns the column index that contains the specified FileInfo; returns -1 if the column is not displayed
		char result=-1;
		if ((reportModeDisplayedInfos&1<<fileInfoIndex)!=0)
			do{
				result+=(reportModeDisplayedInfos&1<<fileInfoIndex)!=0;
			}while (fileInfoIndex-->0);
		return result;
	}

	char CFileManagerView::__columnIdFromFileInfo__(PCFileInfo fi) const{
		// determines and returns the column index that contains the specified FileInfo; returns -1 if the column is not displayed
		return __columnIdFromFileInfo__(fi-informationList);
	}

	afx_msg void CFileManagerView::OnDestroy(){
		// window destroyed
		dropTarget.Revoke();
		// - displaying the same set of columns the next time it's switched to the File Manager
		reportModeDisplayedInfosPrev=~reportModeDisplayedInfos;
		// - saving the scroll position for later
		scrollY=__getVerticalScrollPos__();
		//scrollToIndex=lv.GetTopIndex();
		// - storing current File selection into auxiliary list
		const CListCtrl &lv=GetListCtrl();
		for( POSITION pos=lv.GetFirstSelectedItemPosition(); pos; selectedFiles.AddTail((PVOID)lv.GetItemData(lv.GetNextSelectedItem(pos))) );
		// - storing currently FocusedFile
		const int iFocused=lv.GetNextItem(-1,LVNI_FOCUSED);
		focusedFile= iFocused>=0 ? (CDos::PFile)lv.GetItemData(iFocused) : nullptr;
		// - destroying the FileComparisonDialog (if shown)
		if (CFileComparisonDialog::pSingleInstance)
			CFileComparisonDialog::pSingleInstance->SendMessage( WM_COMMAND, IDCANCEL, 0 );
		// - no FileManager is currently displayed
		pCurrentlyShown=nullptr;
		// - base
		__super::OnDestroy();
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
			return selectedFiles.GetNext(pos);
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
	void CFileManagerView::SelectFiles(const CFileList &selection){
		// changes SelectedFile
		selectedFiles.RemoveAll();
		for( POSITION pos=selection.GetHeadPosition(); pos; selectedFiles.AddTail(selection.GetNext(pos)) );
		RefreshDisplay();
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
		if (IMAGE->ReportWriteProtection()) return;
		// - definition and instantiation of Dialog
		class CNewSubdirectoryDialog sealed:public Utils::CRideDialog{
			const PDos dos;

			void OnOK() override{
				// the Name of new Subdirectory confirmed
				WCHAR name[MAX_PATH];
				GetDlgItemTextW( ID_DIRECTORY, name );
				if (const TStdWinError err=(dos->*dos->pFileManager->pDirectoryStructureManagement->fnCreateSubdir)(name,FILE_ATTRIBUTE_DIRECTORY,subdirectory))
					Utils::Information(_T("Cannot create the directory"),err);
				else{
					const FILETIME ft=Utils::CRideTime();
					dos->SetFileTimeStamps( subdirectory, &ft, &ft, &ft );
					__super::OnOK();
				}
			}
		public:
			CDos::PFile subdirectory;

			CNewSubdirectoryDialog(PDos _dos)
				// ctor
				: Utils::CRideDialog(IDR_FILEMANAGER_SUBDIR_NEW) , dos(_dos) {
			}
		} d(DOS);
		// - showing the Dialog and processing its result (plus updating the FileManager)
		if (d.DoModal()==IDOK){ // Subdirectory created - showing it
			__unselectAllFilesInCurrentDir__(); // cancelling any existing selection
			__addToTheEndAndSelectFile__(d.subdirectory);
		}
	}
	afx_msg void CFileManagerView::__createSubdirectory_updateUI__(CCmdUI *pCmdUI){
		// projecting possibility to create Subdirectories into UI
		pCmdUI->Enable(pDirectoryStructureManagement!=nullptr);
	}

	void CFileManagerView::BrowseCurrentDirInHexaMode(CDos::PCFile fileToSeekTo){
		CDirEntriesView *const deView=new CDirEntriesView( DOS, DOS->currentDir, fileToSeekTo );
		CTdiCtrl::AddTabLastW( TDI_HWND,
			DOS->GetFilePresentationNameAndExt(DOS->currentDir).Prepend(_T("Dir \"")).Append(L'\"').GetUnicode(),
			&deView->tab, true, TDI_TAB_CANCLOSE_ALWAYS, CMainWindow::CTdiView::TTab::OnOptionalTabClosing
		);
		ownedTabs.AddTail( &deView->tab );
	}

	afx_msg void CFileManagerView::BrowseCurrentDirInHexaMode(){
		// opens a new Tab with DirectoryEntries listed in an HexaEditor instance
		BrowseCurrentDirInHexaMode(nullptr);
	}

	afx_msg void CFileManagerView::GoToFocusedFileDirectoryEntry(){
		// opens a new Tab with DirectoryEntries listed in an HexaEditor instance
		const CListCtrl &lv=GetListCtrl();
		const int iFocused=lv.GetNextItem(-1,LVNI_FOCUSED);
		BrowseCurrentDirInHexaMode(
			iFocused>=0 // is there one File focused?
			? (CDos::PCFile)lv.GetItemData(iFocused)
			: nullptr
		);
	}

	afx_msg void CFileManagerView::BrowseFocusedFileFatLinkage(){
		// opens a new Tab with FAT linkage in a HexaEditor instance
		const CListCtrl &lv=GetListCtrl();
		const int iFocused=lv.GetNextItem(-1,LVNI_FOCUSED);
		if (iFocused>=0){ // is there one File focused?
			const CDos::PFile file=(CDos::PFile)lv.GetItemData(iFocused);
			auto *const pView=new CFatHexaView( DOS, file, fatEntryYahelDefinition );
				CTdiCtrl::AddTabLastW( TDI_HWND,
					DOS->GetFilePresentationNameAndExt(file).Prepend( _T("FAT \"") ).Append(L'\"').GetUnicode(),
					&pView->tab, true, TDI_TAB_CANCLOSE_ALWAYS, CMainWindow::CTdiView::TTab::OnOptionalTabClosing
				);
			ownedTabs.AddTail( &pView->tab );						
		}
	}

	afx_msg void CFileManagerView::BrowseFocusedFileFatLinkage_updateUI(CCmdUI *pCmdUI){
		pCmdUI->Enable(
			fatEntryYahelDefinition // can browse File FAT linkages?
			&&
			GetListCtrl().GetNextItem(-1,LVNI_FOCUSED)>=0 // is there a File currently focused?
		);
	}

	afx_msg void CFileManagerView::GoToFocusedFileFirstSector(){
		// opens a new Tab with Sectors listed in an HexaEditor instance
		const CListCtrl &lv=GetListCtrl();
		const int iFocused=lv.GetNextItem(-1,LVNI_FOCUSED);
		if (iFocused>=0){ // is there one File focused?
			const CDos::PCFile f=(CDos::PCFile)lv.GetItemData(iFocused);
			const CDos::CFatPath fatPath=CDos::CFatPath( DOS, f );
			CDos::CFatPath::PCItem p; DWORD n;
			if (const LPCTSTR err=fatPath.GetItems( p, n )){
				const CString msg=Utils::SimpleFormat( _T("Error in FAT for \"%s\":%s\n\nGo to its first sector anyway?"), DOS->GetFilePresentationNameAndExt(f), err );
				if (!Utils::QuestionYesNo(msg,MB_DEFBUTTON1))
					return;
			}else if (!n)
				return Utils::Information(
					Utils::SimpleFormat( _T("Item \"%s\" occupies no sectors."), DOS->GetFilePresentationNameAndExt(f) )
				);
			ownedTabs.AddTail(
				&CDiskBrowserView::CreateAndSwitchToTab( IMAGE, p->chs, 0 ).tab
			);
		}
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
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		// - processing recurrently (see below Collector's ctor)
		struct TStatisticsCollector sealed{
			typedef const struct TDirectoryEtc sealed{
				DWORD dirId;
				const TDirectoryEtc *parent;
			} *PCDirectoryEtc;

			TSelectionStatistics &rStatistics;
			RCFileManagerView rFileManager;
			const PBackgroundActionCancelable pAction;
			const PDos dos;
			const CFileManagerView::PCDirectoryStructureManagement pDirStructMan;
			TCylinder state;

			TStatisticsCollector(PBackgroundActionCancelable pAction)
				// ctor
				// . initialization
				: rStatistics(*(TSelectionStatistics *)pAction->GetParams())
				, pAction(pAction) , state(0)
				, dos(rFileManager.DOS) , rFileManager(rStatistics.rFileManager) , pDirStructMan(rFileManager.pDirectoryStructureManagement) {
				pAction->SetProgressTarget( rStatistics.finishedState );
				// . recurrently collecting Statistics on selected Files
				const CDos::PFile currentDirectory=dos->currentDir;
				const TDirectoryEtc dirEtc={ dos->GetDirectoryUid(currentDirectory), nullptr };
				for( POSITION pos=rFileManager.GetFirstSelectedFilePosition(); pos; ){
					if (pAction->Cancelled) break;
					if (const auto pdt=dos->BeginDirectoryTraversal(currentDirectory)){
						for( const CDos::PCFile file=rFileManager.GetNextSelectedFile(pos); pdt->GetNextFileOrSubdir()!=file; );
						__countInFile__(pdt,&dirEtc);
					}
				}
				if (pDirStructMan!=nullptr)
					(dos->*pDirStructMan->fnChangeCurrentDir)(currentDirectory); // because it could recurrently be switched to another but CurrentDirectory
			}

			void __countInFile__(const std::unique_ptr<CDos::TDirectoryTraversal> &pdt,PCDirectoryEtc dirPath){
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
					const TDirectoryEtc dirEtc={ dos->GetDirectoryUid(dos->currentDir), dirPath };
					for( PCDirectoryEtc pde=dirPath; pde!=nullptr; pde=pde->parent )
						if (pde->dirId==dirEtc.dirId) return;
					// . involving Directory into Statistics
					if (const auto subPdt=dos->BeginDirectoryTraversal(pdt->entry)){
						for( rStatistics.nDirectories++,rStatistics.totalSizeInBytes+=dos->GetFileOfficialSize(pdt->entry),rStatistics.totalSizeOnDiskInBytes+=dos->GetFileSizeOnDisk(pdt->entry); subPdt->GetNextFileOrSubdir()!=nullptr; ){
							if (pAction->Cancelled) break;
							if (subPdt->entryType!=CDos::TDirectoryTraversal::WARNING)
								__countInFile__(subPdt,&dirEtc);
						}
						pAction->UpdateProgress( state=std::max(state,subPdt->chs.cylinder) );
					}
				}
			}
		} tmp(pAction);
		// - Statistics collected
		pAction->UpdateProgressFinished();
		return pAction->Cancelled ? ERROR_CANCELLED : ERROR_SUCCESS;
	}

	afx_msg void CFileManagerView::__showSelectionProperties__(){
		// shows properties of currently selected Files (and Directories)
		TSelectionStatistics statistics(*this);
		if (CBackgroundActionCancelable(
				__selectionPropertyStatistics_thread__,
				&statistics,
				THREAD_PRIORITY_BELOW_NORMAL
			).Perform()==ERROR_SUCCESS
		){
			TCHAR buf[1024];
			::wsprintf( buf, _T("SELECTION Properties\n\n- recurrently selected %d file(s), %d folders\n- of %d Bytes in total size\n- which occupy %d Bytes on the disk."), statistics.nFiles, statistics.nDirectories, statistics.totalSizeInBytes, statistics.totalSizeOnDiskInBytes );
			Utils::Information(buf);
		}
	}

	afx_msg void CFileManagerView::RefreshDisplay(){
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
