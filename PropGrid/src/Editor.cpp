#include "stdafx.h"

	const TEditor::TControl *TEditor::pSingleShown;

	TEditor::TControl::TControl(PCEditor editor,
								PropGrid::PValue valueBuffer,
								PropGrid::PCustomParam param,
								DWORD style,
								HWND hParent
							)
		// ctor
		// - creating Editor's GUI
		: value(editor,valueBuffer,param)
		, hMainCtrl(nullptr) // set below
		, hEllipsisBtn(	editor->onEllipsisBtnClicked!=nullptr
						? ::CreateWindow(	WC_BUTTON, _T("..."),
											EDITOR_STYLE, 0,0, 1,1,
											hParent , 0,GET_PROPGRID_HINSTANCE(hParent),nullptr
										)
						: nullptr
					)
		, mainControlExists( editor->hasMainControl )
		, wndProc0(nullptr) , ellipsisBtnWndProc0(nullptr) { // set below
		pSingleShown=this;
		(HWND)hMainCtrl=editor->hasMainControl
						? editor->__createMainControl__( value, hParent )
						: ::CreateWindow(	WC_STATIC, nullptr, WS_CHILD,
											0,0, 100,100,
											HWND_MESSAGE, // "Message-Only Window" (invisible window that only processes messages)
											0,GET_PROPGRID_HINSTANCE(hParent), nullptr
										);

		(WNDPROC)wndProc0=(WNDPROC)::SetWindowLongW( hMainCtrl, GWLP_WNDPROC, (LPARAM)editor->__wndProc__ );
		(WNDPROC)ellipsisBtnWndProc0=SubclassWindow(hEllipsisBtn,__ellipsisBtn_wndProc__);
		::SendMessageW( hMainCtrl, WM_SETFONT, (WPARAM)TPropGridInfo::FONT_DEFAULT, 0 );
		::BringWindowToTop(hMainCtrl);
		::SendMessageW( hEllipsisBtn, WM_SETFONT, (WPARAM)TPropGridInfo::FONT_DEFAULT, 0 );
		::SetWindowLong( hEllipsisBtn, GWL_USERDATA, (LONG)editor->onEllipsisBtnClicked );
		::BringWindowToTop(hEllipsisBtn);
		// - focusing the MainControl
		::SetFocus( mainControlExists ? hMainCtrl : hEllipsisBtn );
		//::SetCapture(_hMainCtrl); // commented out, otherwise the Ellipsis button won't be clickable by mouse
		__nop();
	}

	TEditor::TControl::~TControl(){
		// dtor
		const HWND hParent=::GetParent( mainControlExists ? hMainCtrl : hEllipsisBtn );
		if (hEllipsisBtn)
			::DestroyWindow(hEllipsisBtn);
		::DestroyWindow(hMainCtrl);
		::InvalidateRect( hParent, nullptr, TRUE );
	}










	bool TEditor::ignoreRequestToDestroy;

	TEditor::TEditor(	WORD height,
						bool hasMainControl,
						PropGrid::TSize valueSize,
						PropGrid::TOnEllipsisButtonClicked onEllipsisBtnClicked,
						PropGrid::TOnValueChanged onValueChanged
					)
		// ctor
		: height(height)
		, hasMainControl(hasMainControl)
		, valueSize(valueSize)
		, onEllipsisBtnClicked(onEllipsisBtnClicked)
		, onValueChanged(onValueChanged) {
	}




	void TEditor::__drawString__(LPCSTR text,short textLength,PDRAWITEMSTRUCT pdis){
		// draws Text into the specified Rectangle
		WCHAR bufW[STRING_LENGTH_MAX+1];
		__drawString__(	bufW,
						::MultiByteToWideChar( CP_ACP, 0, text,textLength, bufW,STRING_LENGTH_MAX+1 ),
						pdis
					);
	}

	void TEditor::__drawString__(LPCWSTR text,short textLength,PDRAWITEMSTRUCT pdis){
		// draws Text into the specified Rectangle
		RECT r={ PROPGRID_CELL_MARGIN_LEFT, PROPGRID_CELL_MARGIN_TOP+pdis->rcItem.top, pdis->rcItem.right, pdis->rcItem.bottom };
		WCHAR buf[STRING_LENGTH_MAX+1];
		::DrawTextW(pdis->hDC,
					::lstrcpynW( buf, text, textLength+(BYTE)(textLength>=0) ), // TextLength that is "-1" will be "-1"; TextLength that is N will be N+1
					-1,
					&r,
					DT_SINGLELINE | DT_LEFT | DT_VCENTER
				);
	}

	LRESULT CALLBACK TEditor::__wndProc__(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		return	pSingleShown->value.editor->__mainCtrl_wndProc__(
					hWnd, // may be the handle of either MainControl or EllipsisButton
					msg, wParam, lParam
				);
	}

	LRESULT TEditor::__mainCtrl_wndProc__(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) const{
		// window procedure
		switch (msg){
			case WM_MOUSEACTIVATE:
				// preventing the focus from being stolen by the parent
				return MA_ACTIVATE;
			case WM_KEYDOWN:
				// a key has been pressed
				switch (wParam){
					case VK_TAB:
						// the Tab key - testing the Shift+Tab combination
						if (::GetKeyState(VK_SHIFT)>=0 && pSingleShown->hEllipsisBtn){	// Shift NOT pressed and the EllipsisButton exists - focusing on the EllipsisButton
							::SetFocus( pSingleShown->hEllipsisBtn ); // switching to the EllipsisButton
							return 0;
						}//else // terminating the editing by confirming the current Value
							//fallthrough
					case VK_RETURN:
						// the Enter key - confirming the current Value
						PropGrid::TryToAcceptCurrentValueAndCloseEditor(); // on success also destroys the Editor
						return 0; // cannot Break (and CallWindowProc below) as the Editor may no longer exist at this moment
					case VK_ESCAPE:
						// the Escape key - cancelling the Item editing
						__cancelEditing__();
						return 0; // cannot Break (and CallWindowProc below) as the Editor no longer exists at this moment
				}
				break;
			case WM_CHAR:
				// printable character
				if (wParam==VK_TAB) return 0; // ignoring the Tab (as already processed in WM_KEYDOWN)
				break;
			case WM_GETDLGCODE:
				// the Editor must receive all keyboard input (it may not receive a Tab keystroke if part of a dialog or CControlBar)
				return DLGC_WANTALLKEYS;
			case WM_KILLFOCUS:{
				// the MainControl or EllipsisButton has lost the focus
				// . if the focus has been handed over to the EllipsisButton, doing nothing (as the focus remains within the Editor components)
				if (pSingleShown->hEllipsisBtn && ::GetFocus()==pSingleShown->hEllipsisBtn)
					break;
				// . if attempting to leave the Editor, attempting to accept the new Value
				PropGrid::TOnValueChanged onValueChanged=nullptr; // assumption (Value didn't change)
				PropGrid::PCustomParam param;
				if (::IsWindowVisible(pSingleShown->hMainCtrl)) // yes, attepting to leave the Editor; must use the "pSingleShown->hMainCtrl" construct to refer to the MainControl as "hWnd" may refer to either the MainControl or EllipsisButton (see EllipsisButton's window procedure)
					if (__tryToAcceptMainCtrlValue__()){ // if Value acceptable ...
						onValueChanged=pSingleShown->value.editor->onValueChanged; // ... letting the caller know once the editing has definitely ended
						param=pSingleShown->value.param;
					}else{ // otherwise, if Value not acceptable ...
						::SetFocus(hWnd); // ... focusing back on either the MainControl or EllipsisButton
						return 0;
					}
				// . destroying the Editor
				delete pSingleShown;
				pSingleShown=nullptr;
				// . letting the caller know the editing has definitely ended
				if (onValueChanged)
					onValueChanged(param);
				return 0;
			}
		}
		return ::CallWindowProcW(pSingleShown->wndProc0,hWnd,msg,wParam,lParam);
	}



	
	LRESULT CALLBACK TEditor::__ellipsisBtn_wndProc__(HWND hEllipsisBtn,UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_MOUSEACTIVATE:
				// preventing the focus from being stolen by the parent
				return MA_ACTIVATE;
			case WM_CAPTURECHANGED:{
				// EllipsisButton clicked (either by left mouse button or using space-bar)
				// . removing the subclassing, carrying out the Action, renewing the subclassing (as it's important that the EllipsisButton doesn't know that it's eventually lost the focus [e.g. due to a shown dialog]; knowing it's lost he focus would result in preliminary destroying the Editor)
				const WNDPROC wndProc=SubclassWindow(hEllipsisBtn,pSingleShown->ellipsisBtnWndProc0); // removing the subclassing
					const bool b=( (PropGrid::TOnEllipsisButtonClicked)::GetWindowLong(hEllipsisBtn,GWL_USERDATA) )( // carrying out the Action
										pSingleShown->value.param,
										pSingleShown->value.buffer,
										pSingleShown->value.editor->valueSize
								);
				SubclassWindow(hEllipsisBtn,wndProc); // renewing the subclassing
				::SetFocus(hEllipsisBtn); // renewing the focus, should it be lost during the Action
				// . evaluating the Action's result
				if (b){
					// Action succeeded (e.g. a dialog confirmed by the OK button) - attempting to accept the new Value
					const auto onValueChanged =	!pSingleShown->mainControlExists // the event explicitly fires ...
												? pSingleShown->value.editor->onValueChanged // ... only if there's no MainControl
												: nullptr; // ... otherwise, firing of the event is left upon the MainControl
					const auto param=pSingleShown->value.param;
					PropGrid::TryToAcceptCurrentValueAndCloseEditor(); // on success also destroys the Editor
					if (onValueChanged)
						onValueChanged(param);
					return 0;
				}else{
					// Action failed (e.g. a dialog dismissed by the Cancel button)
					::SetFocus( pSingleShown->mainControlExists ? pSingleShown->hMainCtrl : pSingleShown->hEllipsisBtn ); // refocusing primarily on the MainControl again (if it exists)
					break;
				}
			}
			case WM_KEYDOWN:
				// a key has been pressed
				switch (wParam){
					case VK_TAB:
						// the Tab key - testing the Shift+Tab combination
						if (::GetKeyState(VK_SHIFT)<0 && pSingleShown->mainControlExists) // Shift pressed and the MainControl exists
							::SetFocus( pSingleShown->hMainCtrl ); // switching to the MainControl
						else
							PropGrid::TryToAcceptCurrentValueAndCloseEditor(); // on success also destroys the Editor
						return 0;
					case VK_SPACE:
						// Space-bar (click on the EllipsisButton catched in WM_CAPTURECHANGED)
						break;
					case VK_ESCAPE:
						// the Escape key - cancelling the Item editing
						__cancelEditing__();
						return 0; // cannot Break (and CallWindowProc below) as the Editor no longer exists at this moment
					default:
						// forwarding the key to the MainControl
						return ::CallWindowProcW((WNDPROC)::GetWindowLongW(pSingleShown->hMainCtrl,GWL_WNDPROC),
												pSingleShown->hMainCtrl,
												msg, wParam, lParam
											);
				}
				break;
			case WM_KILLFOCUS:
				// EllipsisButton has lost the focus
				// . if the focus has been handed over to the MainControl, doing nothing (as the focus remains within the Editor components)
				if (pSingleShown->hMainCtrl && ::GetFocus()==pSingleShown->hMainCtrl)
					break;
				// . forwarding the message to the MainControl
				return __wndProc__( hEllipsisBtn, WM_KILLFOCUS, wParam, lParam );
		}
		return ::CallWindowProcW(pSingleShown->ellipsisBtnWndProc0,hEllipsisBtn,msg,wParam,lParam);
	}





	void TEditor::__cancelEditing__(){
		// cancels editing of Item's Value
		if (pSingleShown){
			// . preventing the current Value to be attempted for acceptance
			if (pSingleShown->mainControlExists){
				::SetFocus(pSingleShown->hMainCtrl);
				::ShowWindow(pSingleShown->hMainCtrl,SW_HIDE);
			}else
				::ShowWindow(pSingleShown->hEllipsisBtn,SW_HIDE);
			// . generating the WM_KILLFOCUS to destroy the Editor's Instance
			//nop (done in ShowWindow above)
		}
	}





	CRegisteredEditors RegisteredEditors;

	PCEditor CRegisteredEditors::__add__(PCEditor definition,BYTE editorSizeInBytes){
		// returns the Editor with given Definition
		// - checking if an Editor with given Definition has already been registered before
		for( const TListItem *pDef=list; pDef!=nullptr; pDef=pDef->pNext )
			if (pDef->editorSizeInBytes==editorSizeInBytes)
				if (!::memcmp(pDef->pEditor,definition,editorSizeInBytes)){
					// an Editor with given Definition already registered
					delete definition; // deleting redundant definition
					return pDef->pEditor; // returning the previously registered Editor
				}
		// - the Definition is unique, enrolling it into the List, and returning it
		return ( list=new TListItem(definition,editorSizeInBytes,list) )->pEditor;
	}

	CRegisteredEditors::TListItem::TListItem(PCEditor pEditor,BYTE editorSizeInBytes,const TListItem *pNext)
		// ctor
		: pEditor(pEditor) , editorSizeInBytes(editorSizeInBytes) , pNext(pNext) {
	}

	CRegisteredEditors::CRegisteredEditors()
		// ctor
		: list(nullptr) {
	}

	CRegisteredEditors::~CRegisteredEditors(){
		// dtor
		while (const TListItem *const p=list){
			list=p->pNext;
			delete p->pEditor;
			delete p;
		}
	}
