#include "stdafx.h"

	#define INI_MSG_FILE_SELECTION_NONE		_T("fmselnone")
	#define INI_MSG_FILE_SELECTION_TOO_MANY	_T("fmselmany")
	#define INI_MSG_FILE_EDITING			_T("fmedit")
	

	static BYTE editedInformationId;

	const CFileManagerView::CEditorBase *CFileManagerView::CEditorBase::pSingleShown;

	CFileManagerView::CEditorBase::CEditorBase(CDos::PFile file,PVOID value,short valueSize,CPropGridCtrl::PCEditor editor,RCFileManagerView parent)
		// ctor
		// - initialization
		: file(file) , parent(parent)
		, hEditor(0) , hEllipsisButton(0) { // created below
		pSingleShown=this;
		// - determining the area dedicated for the Editor
		CListCtrl &lv=parent.GetListCtrl();
		LVFINDINFO lvdi={ LVFI_PARAM, NULL, (LPARAM)file };
		CRect rcEditorArea;
		lv.GetSubItemRect( lv.FindItem(&lvdi), editedInformationId, LVIR_BOUNDS, rcEditorArea );
		if (!editedInformationId)	// the width of whole File item returned for zeroth Information
			rcEditorArea.right=lv.GetColumnWidth(0);
		// - creating the Editor
		(HWND)hEditor=CPropGridCtrl::BeginEditValue( value, valueSize, file, editor, rcEditorArea, 0, parent.m_hWnd, (HWND *)&hEllipsisButton );
		wndProc0=(WNDPROC)::SetWindowLong(hEditor,GWL_WNDPROC,(long)__wndProc__);
		::SendMessage( hEditor, WM_SETFONT, (WPARAM)parent.rFont.m_hObject, 0 );
		ellipsisButtonWndProc0=(WNDPROC)::SetWindowLong(hEllipsisButton,GWL_WNDPROC,(long)__ellipsisButton_wndProc__);
	}

	CFileManagerView::CEditorBase::~CEditorBase(){
		// dtor
		// - revoking the subclassing (for the Editor to be NOT able to receive any custom messages)
		::SetWindowLong( hEditor, GWL_WNDPROC, (long)wndProc0 );
		::SetWindowLong( hEllipsisButton, GWL_WNDPROC, (long)ellipsisButtonWndProc0 );
	}

	LRESULT CALLBACK CFileManagerView::CEditorBase::__wndProc__(HWND hEditor,UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_KEYDOWN:
				// key pressed
				if (wParam==VK_TAB){
					// tab - testing the Shift+Tab combination
					const CDos::PFile file=pSingleShown->file;
					const RCFileManagerView parent=pSingleShown->parent;
					// . base
					::CallWindowProc(pSingleShown->wndProc0,hEditor,msg,wParam,lParam);
					// . if this Editor no longer exists, we can edit the next/previous Information on current File
					if (!pSingleShown)
						if (::GetKeyState(VK_SHIFT)<0)	// Shift pressed = editing previous editable Information on current File
							parent.__editPreviousFileInformation__(file);
						else	// Shift not pressed = editing next editable Information on current File
							parent.__editNextFileInformation__(file);
					return 0;
				}else
					break;
			case WM_NCDESTROY:
				// about to be destroyed
				// . base
				::CallWindowProc(pSingleShown->wndProc0,hEditor,msg,wParam,lParam);
				// . destroying
				delete pSingleShown; pSingleShown=NULL;
				return 0;
		}
		return ::CallWindowProc(pSingleShown->wndProc0,hEditor,msg,wParam,lParam);
	}

	LRESULT CALLBACK CFileManagerView::CEditorBase::__ellipsisButton_wndProc__(HWND hEllipsisButton,UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_KEYDOWN:
				// key pressed
				if (wParam==VK_TAB){
					// tab - testing the Shift+Tab combination
					const CDos::PFile file=pSingleShown->file;
					const RCFileManagerView parent=pSingleShown->parent;
					// . base
					::CallWindowProc(pSingleShown->ellipsisButtonWndProc0,hEllipsisButton,msg,wParam,lParam);
					// . if this Editor no longer exists, we can edit the next/previous Information on current File
					if (!pSingleShown)
						if (::GetKeyState(VK_SHIFT)<0) // Shift pressed = editing previous editable Information on current File
							parent.__editPreviousFileInformation__(file);
						else // Shift not pressed = editing next editable Information on current File
							parent.__editNextFileInformation__(file);
					return 0;
				}else
					break;
		}
		return ::CallWindowProc(pSingleShown->ellipsisButtonWndProc0,hEllipsisButton,msg,wParam,lParam);
	}

	void CFileManagerView::CEditorBase::Repaint() const{
		// repaints the Editor
		::InvalidateRect( hEditor, NULL, TRUE );
	}










	#define DOS		tab.dos
	#define IMAGE	DOS->image

	CFileManagerView::PEditorBase CFileManagerView::__createStdEditor__(CDos::PFile file,PVOID value,short valueSize,CPropGridCtrl::PCEditor editor) const{
		// creates and returns PropertyGrid's specified built-in Editor
		return new CEditorBase( file, value, valueSize, editor, *this );
	}

	CFileManagerView::PEditorBase CFileManagerView::__createStdEditorWithEllipsis__(CDos::PFile file,CPropGridCtrl::TOnEllipsisButtonClicked buttonAction) const{
		// creates and returns an Editor that contains only PropertyGrid's standard EllipsisButton and misses the main control; the EllipsisButton triggers an edit dialog with given ID
		return __createStdEditor__(	file, file, 0,
									CPropGridCtrl::TCustom::DefineEditor( 0, NULL, NULL, buttonAction )
								);
	}

	CFileManagerView::PEditorBase CFileManagerView::__createStdEditorForByteValue__(CDos::PFile file,PBYTE pByte,CPropGridCtrl::TInteger::TOnValueConfirmed fnOnValueConfirmed) const{
		// creates and returns PropertyGrid's build-in Editor for editing specified Byte value
		return __createStdEditor__( file, pByte, sizeof(BYTE),
									CPropGridCtrl::TInteger::DefineByteEditor(fnOnValueConfirmed,CPropGridCtrl::TInteger::ALIGN_RIGHT)
								);
	}

	CFileManagerView::PEditorBase CFileManagerView::__createStdEditorForWordValue__(CDos::PFile file,PWORD pWord,CPropGridCtrl::TInteger::TOnValueConfirmed fnOnValueConfirmed) const{
		// creates and returns PropertyGrid's build-in Editor for editing specified Word value
		return __createStdEditor__( file, pWord, sizeof(WORD),
									CPropGridCtrl::TInteger::DefineWordEditor(fnOnValueConfirmed,CPropGridCtrl::TInteger::ALIGN_RIGHT)
								);
	}

	#define SEARCH_DIRECTION_RIGHT	1
	#define SEARCH_DIRECTION_LEFT	-1

	afx_msg void CFileManagerView::__editNameOfSelectedFile__(){
		// creates an Editor of the name of currently selected File
		// - no Editor must be shown, otherwise editing already in progress
		if (CEditorBase::pSingleShown)
			return;
		// - Image mustn't be WriteProtected, otherwise quitting
		if (IMAGE->__reportWriteProtection__())
			return;
		// - at least one File must be selected
		CListCtrl &lv=GetListCtrl();
		POSITION pos=lv.GetFirstSelectedItemPosition();
		if (!pos){
			__informationWithCheckableShowNoMore__( _T("No file to edit information of selected."), INI_MSG_FILE_SELECTION_NONE );
			return;
		}
		const int fileId=lv.GetNextSelectedItem(pos);
		const CDos::PFile file=(CDos::PFile)lv.GetItemData(fileId);
		if (pos)
			__informationWithCheckableShowNoMore__( _T("Several files selected for editing - about to edit the first one only."), INI_MSG_FILE_SELECTION_TOO_MANY );
		// - making sure only currently edited File is selected
		__unselectAllFilesInCurrentDir__();
		lv.SetItemState(fileId,LVIS_SELECTED,LVIS_SELECTED);
		// - creating Editor for File's first editable Information
		if (displayMode==LVS_REPORT){
			__informationWithCheckableShowNoMore__( _T("About to edit the name.\n\nNavigate between additional editable information forth and back by pressing Tab or Ctrl+Tab.\nYou eventually can also double-click on a file information to edit it (but double-clicking on a directory name shows its content)."), INI_MSG_FILE_EDITING );
			editedInformationId=nameColumnId-1;
			__editFileInformation__( file, SEARCH_DIRECTION_RIGHT );
		}else{
			lv.SetFocus();	// to edit File's label, the ListCtrl must be fokused
			const HWND hEdit=ListView_EditLabel( lv.m_hWnd, fileId );
			CEditorBase::pSingleShown=(PEditorBase)::SetWindowLong( hEdit, GWL_WNDPROC, (long)__editLabel_wndProc__ );
		}
		// - emptying the clipboard
		if (ownedDataSource) ::OleSetClipboard(NULL);
	}
	LRESULT WINAPI CFileManagerView::__editLabel_wndProc__(HWND hEdit,UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure of label Editor (in DisplayMode other than Report)
		if (msg==WM_MOUSEACTIVATE)
			// preventing the focus to be stolen by the parent (for some reason, the TdiView receives focus if the Editor is clicked)
			return MA_ACTIVATE;
		else
			return ::CallWindowProc( (WNDPROC)CEditorBase::pSingleShown, hEdit, msg, wParam, lParam );
	}

	void CFileManagerView::__editFileInformation__(CDos::PFile file,BYTE editableInformationSearchDirection) const{
		// creates Editor of given Information; if Information not editable, searches the nearest editable Information in given Direction {Left,Right}
		BYTE n=nInformation;
		do{
			if (( editedInformationId+=editableInformationSearchDirection )==nInformation)
				editedInformationId=0;
			else if(editedInformationId==(BYTE)-1)
				editedInformationId=nInformation-1;
		}while (!CreateFileInformationEditor(file,editedInformationId) && --n);
	}

	void CFileManagerView::__editNextFileInformation__(CDos::PFile file) const{
		// creates Editor for next editable Information (i.e. assumed that "some" Editor of given File already exists)
		if (CPropGridCtrl::TryToAcceptCurrentValueAndCloseEditor())
			__editFileInformation__( file, SEARCH_DIRECTION_RIGHT );
	}
	void CFileManagerView::__editPreviousFileInformation__(CDos::PFile file) const{
		// creates Editor for previous editable Information (i.e. assumed that "some" Editor of given File already exists)
		if (CPropGridCtrl::TryToAcceptCurrentValueAndCloseEditor())
			__editFileInformation__( file, SEARCH_DIRECTION_LEFT );
	}

	afx_msg void CFileManagerView::__onDblClick__(NMHDR *pNMHDR,LRESULT *pResult){
		// window double-clicked
		// - if no File double-clicked, quit
		const LPNMITEMACTIVATE lpia=(LPNMITEMACTIVATE)pNMHDR;
		if (lpia->iItem<0) return;
		const CListCtrl &lv=GetListCtrl();
		const CDos::PFile file=(CDos::PFile)lv.GetItemData(lpia->iItem);
		// - displaying the content of Subdirectory
		if (lpia->iSubItem==nameColumnId)
			if (DOS->IsDirectory(file)){
				__switchToDirectory__(file);
				__refreshDisplay__();
				return;
			}
		// - editing Information on which it was clicked (if editable)
		if (displayMode==LVS_REPORT){
			// current DisplayMode is Report
			if (!CEditorBase::pSingleShown && lpia->iSubItem>-1){
				// !A&B; A = no Editor shown, B = double-clicked on File Information (column)
				if (IMAGE->__reportWriteProtection__()) return; // if Image WriteProtected, informing and quitting
				if (!CreateFileInformationEditor(file, editedInformationId=lpia->iSubItem )){
					TCHAR buf[200];
					_stprintf(buf,_T("Information in the \"%s\" column cannot be edited."),informationList[editedInformationId].informationName);
					TUtils::Information(buf);
				}
			}
		}else
			// DisplayMode is other than Report - only File name can be edited
			__editNameOfSelectedFile__();
	}

	afx_msg void CFileManagerView::__onEndLabelEdit__(NMHDR *pNMHDR,LRESULT *pResult){
		// end of File label editing (occurs in DisplayMode other than Report)
		const NMLVDISPINFO *const lpdi=(NMLVDISPINFO *)pNMHDR;
		if (const LPCTSTR label=lpdi->item.pszText){ // editing NOT cancelled
			CDos::PFile renamedFile;
			TCHAR buf[MAX_PATH], *pDot=_tcsrchr(::lstrcpy(buf,label),'.');
			if (pDot) *pDot='\0'; else pDot=_T(".");
			const TStdWinError err=DOS->ChangeFileNameAndExt( (CDos::PFile)lpdi->item.lParam, buf, pDot+1, renamedFile );
			if ( *pResult=err==ERROR_SUCCESS )
				__replaceFileDisplay__( (CDos::PFile)lpdi->item.lParam, renamedFile );
			else
				TUtils::Information(FILE_MANAGER_ERROR_RENAMING,err);
		}
		CEditorBase::pSingleShown=NULL;
	}
