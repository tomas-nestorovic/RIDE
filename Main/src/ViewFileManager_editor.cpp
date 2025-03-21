#include "stdafx.h"

	#define INI_MSG_FILE_SELECTION_NONE		_T("fmselnone")
	#define INI_MSG_FILE_SELECTION_TOO_MANY	_T("fmselmany")
	#define INI_MSG_FILE_EDITING			_T("fmedit")
	

	static BYTE editedInformationId;

	const CFileManagerView::CEditorBase *CFileManagerView::CEditorBase::pSingleShown;

	CFileManagerView::CEditorBase::CEditorBase(CDos::PFile file,PVOID value,PropGrid::PCEditor editor,RCFileManagerView parent)
		// ctor
		// - initialization
		: file(file) , parent(parent)
		, hEditor(0) , hEllipsisButton(0) { // created below
		pSingleShown=this;
		// - determining the area dedicated for the Editor
		CListCtrl &lv=parent.GetListCtrl();
		LVFINDINFO lvdi={ LVFI_PARAM, nullptr, (LPARAM)file };
		const char columnId=parent.__columnIdFromFileInfo__(editedInformationId);
		CRect rcEditorArea;
		lv.GetSubItemRect( lv.FindItem(&lvdi), columnId, LVIR_BOUNDS, rcEditorArea );
		if (!columnId)	// the width of whole File item returned for zeroth Information
			rcEditorArea.right=lv.GetColumnWidth(0);
		// - creating the Editor
		(HWND)hEditor=PropGrid::BeginEditValue( value, file, editor, rcEditorArea, 0, parent.m_hWnd, (HWND *)&hEllipsisButton );
		wndProc0=Utils::SubclassWindowW( hEditor, __wndProc__ );
		SetWindowFont( hEditor, parent.font, FALSE );
		ellipsisButtonWndProc0=Utils::SubclassWindowW( hEllipsisButton, __ellipsisButton_wndProc__ );
	}

	CFileManagerView::CEditorBase::~CEditorBase(){
		// dtor
		// - revoking the subclassing (for the Editor to be NOT able to receive any custom messages)
		Utils::SubclassWindowW( hEditor, wndProc0 );
		Utils::SubclassWindowW( hEllipsisButton, ellipsisButtonWndProc0 );
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
					::CallWindowProcW(pSingleShown->wndProc0,hEditor,msg,wParam,lParam);
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
				::CallWindowProcW(pSingleShown->wndProc0,hEditor,msg,wParam,lParam);
				// . destroying
				delete pSingleShown; pSingleShown=nullptr;
				return 0;
		}
		return ::CallWindowProcW(pSingleShown->wndProc0,hEditor,msg,wParam,lParam);
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
					::CallWindowProcW(pSingleShown->ellipsisButtonWndProc0,hEllipsisButton,msg,wParam,lParam);
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
		return ::CallWindowProcW(pSingleShown->ellipsisButtonWndProc0,hEllipsisButton,msg,wParam,lParam);
	}

	void CFileManagerView::CEditorBase::Repaint() const{
		// repaints the Editor
		::InvalidateRect( hEditor, nullptr, TRUE );
	}










	#define IMAGE	tab.image
	#define DOS		IMAGE->dos

	void CFileManagerView::CValueEditorBase::DrawRedHighlight(LPDRAWITEMSTRUCT pdis){
		// 
		::FillRect( pdis->hDC, &pdis->rcItem, CBrush(Utils::GetBlendedColor(::GetBkColor(pdis->hDC),COLOR_RED,.92f)) );
	}

	CFileManagerView::PEditorBase CFileManagerView::CValueEditorBase::CreateStdEditor(CDos::PFile file,PropGrid::PValue value,PropGrid::PCEditor editor){
		// creates and returns PropertyGrid's specified built-in Editor
		return new CEditorBase( file, value, editor, *CDos::GetFocused()->pFileManager );
	}

	CFileManagerView::PEditorBase CFileManagerView::CValueEditorBase::CreateStdEditorWithEllipsis(CDos::PFile file,PropGrid::TOnEllipsisButtonClicked buttonAction){
		// creates and returns an Editor that contains only PropertyGrid's standard EllipsisButton and misses the main control; the EllipsisButton triggers an edit dialog with given ID
		return CreateStdEditorWithEllipsis( file, file, 0, buttonAction );
	}

	CFileManagerView::PEditorBase CFileManagerView::CValueEditorBase::CreateStdEditorWithEllipsis(CDos::PFile file,PropGrid::PValue value,PropGrid::TSize valueSize,PropGrid::TOnEllipsisButtonClicked buttonAction){
		// creates and returns an Editor that contains only PropertyGrid's standard EllipsisButton and misses the main control; the EllipsisButton triggers an edit dialog with given ID
		return CreateStdEditor(	file, value,
								PropGrid::Custom::DefineEditor( 0, valueSize, nullptr, nullptr, buttonAction )
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
		if (IMAGE->ReportWriteProtection())
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
			for( editedInformationId=nInformation; editedInformationId--; )
				if (reportModeDisplayedInfos&1<<editedInformationId)
					if ((informationList+editedInformationId)->flags&TFileInfo::FileName)
						break;
			editedInformationId--;
			__editFileInformation__( file, SEARCH_DIRECTION_RIGHT );
		}else{
			lv.SetFocus();	// to edit File's label, the ListCtrl must be fokused
			const HWND hEdit=(HWND)::SendMessageW( lv.m_hWnd, LVM_EDITLABELW, fileId, 0 );
			CEditorBase::pSingleShown=(PEditorBase)Utils::SubclassWindowW( hEdit, __editLabel_wndProc__ );
		}
		// - emptying the clipboard
		if (ownedDataSource) ::OleSetClipboard(nullptr);
	}
	LRESULT WINAPI CFileManagerView::__editLabel_wndProc__(HWND hEdit,UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure of label Editor (in DisplayMode other than Report)
		if (msg==WM_MOUSEACTIVATE)
			// preventing the focus to be stolen by the parent (for some reason, the TdiView receives focus if the Editor is clicked)
			return MA_ACTIVATE;
		else
			return ::CallWindowProcW( (WNDPROC)CEditorBase::pSingleShown, hEdit, msg, wParam, lParam );
	}

	void CFileManagerView::__editFileInformation__(CDos::PFile file,BYTE editableInformationSearchDirection) const{
		// creates Editor of given Information; if Information not editable, searches the nearest editable Information in given Direction {Left,Right}
		for( BYTE n=nInformation; n>0; n-- ){
			if (( editedInformationId+=editableInformationSearchDirection )==nInformation)
				editedInformationId=0;
			else if (editedInformationId==(BYTE)-1)
				editedInformationId=nInformation-1;
			if (reportModeDisplayedInfos&1<<editedInformationId)
				if (CreateFileInformationEditor(file,editedInformationId))
					break;
		}
	}

	void CFileManagerView::__editNextFileInformation__(CDos::PFile file) const{
		// creates Editor for next editable Information (i.e. assumed that "some" Editor of given File already exists)
		if (PropGrid::TryToAcceptCurrentValueAndCloseEditor())
			__editFileInformation__( file, SEARCH_DIRECTION_RIGHT );
	}
	void CFileManagerView::__editPreviousFileInformation__(CDos::PFile file) const{
		// creates Editor for previous editable Information (i.e. assumed that "some" Editor of given File already exists)
		if (PropGrid::TryToAcceptCurrentValueAndCloseEditor())
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
		if (__fileInfoFromColumnId__(lpia->iSubItem)->flags&TFileInfo::FileName)
			if (DOS->IsDirectory(file))
				return SwitchToDirectory(file);
		// - editing Information on which it was clicked (if editable)
		if (displayMode==LVS_REPORT){
			// current DisplayMode is Report
			if (!CEditorBase::pSingleShown && lpia->iSubItem>-1){
				// !A&B; A = no Editor shown, B = double-clicked on File Information (column)
				if (IMAGE->ReportWriteProtection()) return; // if Image WriteProtected, informing and quitting
				const auto fileInfo=__fileInfoFromColumnId__(lpia->iSubItem);
				if (!CreateFileInformationEditor(file, editedInformationId=fileInfo-informationList )){
					TCHAR buf[200];
					_stprintf(buf,_T("Information in the \"%s\" column cannot be edited."),fileInfo->informationName);
					Utils::Information(buf);
				}
			}
		}else
			// DisplayMode is other than Report - only File name can be edited
			__editNameOfSelectedFile__();
	}

	afx_msg void CFileManagerView::__navigateBack__(){
		// switching to previous Directory
		const CDos::PFile currDir=DOS->currentDir;
		__switchToDirectory__(previousDirectories.RemoveHead());
		GetListCtrl().SendMessage( LVM_SCROLL, 0, -__getVerticalScrollPos__() ); // resetting the scroll position to zero pixels
		selectedFiles.AddHead( focusedFile=currDir );
		RefreshDisplay();
	}

	afx_msg void CFileManagerView::__navigateBack_updateUI__(CCmdUI *pCmdUI){
		// projects possibility to navigate back into the UI
		pCmdUI->Enable( !previousDirectories.IsEmpty() );
	}

	afx_msg void CFileManagerView::__onEndLabelEdit__(NMHDR *pNMHDR,LRESULT *pResult){
		// end of File label editing (occurs in DisplayMode other than Report)
		const NMLVDISPINFOW *const lpdi=(NMLVDISPINFOW *)pNMHDR;
		if (const LPCWSTR label=lpdi->item.pszText){ // editing NOT cancelled
			CDos::PFile renamedFile=(CDos::PFile)lpdi->item.lParam;
			CDos::CPathString bufName=label;
			const CDos::CPathString bufExt=bufName.DetachExtension();
			const TStdWinError err=DOS->ChangeFileNameAndExt( (CDos::PFile)lpdi->item.lParam, bufName, bufExt, renamedFile );
			if ( *pResult=err==ERROR_SUCCESS )
				__replaceFileDisplay__( (CDos::PFile)lpdi->item.lParam, renamedFile );
			else
				Utils::Information(FILE_MANAGER_ERROR_RENAMING,err);
		}
		CEditorBase::pSingleShown=nullptr;
	}








	CFileManagerView::PEditorBase CFileManagerView::CIntegerEditor::Create(CDos::PFile file,PBYTE pByte,PropGrid::Integer::TOnValueConfirmed fnOnConfirmed){
		// creates and returns PropertyGrid's build-in Editor for editing specified Byte value
		return CreateStdEditor(	file, pByte,
								PropGrid::Integer::DefineByteEditor(fnOnConfirmed,PropGrid::Integer::ALIGN_RIGHT)
							);
	}

	CFileManagerView::PEditorBase CFileManagerView::CIntegerEditor::Create(CDos::PFile file,PWORD pWord,PropGrid::Integer::TOnValueConfirmed fnOnConfirmed){
		// creates and returns PropertyGrid's build-in Editor for editing specified Word value
		return CreateStdEditor(	file, pWord,
								PropGrid::Integer::DefineWordEditor(fnOnConfirmed,PropGrid::Integer::ALIGN_RIGHT)
							);
	}

	void CFileManagerView::CIntegerEditor::DrawReportModeCell(int number,LPDRAWITEMSTRUCT pdis,bool highlightRed){
		// directly draws the integral Number
		if (highlightRed)
			DrawRedHighlight(pdis);
		TCHAR buf[16];
		::DrawText( pdis->hDC, _itot(number,buf,10),-1, &pdis->rcItem, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
	}

	void CFileManagerView::CIntegerEditor::DrawReportModeCellWithCheckmark(int number,bool checkmark,LPDRAWITEMSTRUCT pdis){
		// draws ActualChecksum and, based on the Correctness, either a "check" or "cross" symbol next to it
		// - Number
		RECT &r=pdis->rcItem;
		const LONG statusWidth=(r.bottom-r.top)*4/3;
		r.right-=statusWidth;
		DrawReportModeCell( number, pdis, !checkmark );
		// - "check" or "cross" symbol
		const HDC dc=pdis->hDC;
		r.left=std::max(r.right,r.left), r.right+=statusWidth;
		const int color0 =	pdis->itemState&ODS_SELECTED
							? ::GetTextColor(dc)
							: ::SetTextColor( dc, checkmark?0xa0ffa0:0xff );
			const HGDIOBJ hFont0=::SelectObject( dc, Utils::CRideFont::Webdings120 );
				constexpr WCHAR StatusCorrect=0xf061, StatusIncorrect=0xf072;
				::DrawTextW( dc, checkmark?&StatusCorrect:&StatusIncorrect,1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
			::SelectObject(dc,hFont0);
		::SetTextColor( dc, color0 );
	}
