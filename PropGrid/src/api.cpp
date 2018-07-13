#include "stdafx.h"

	#define PG_CLASS_NAME	_T("WCPropGridClass32")

	LPCTSTR WINAPI CPropGridCtrl::GetWindowClass(HINSTANCE hInstance){
		// creates and returns PropertyGrid window class
		WNDCLASS wc;
			::ZeroMemory(&wc,sizeof(wc));
			wc.lpszClassName=PG_CLASS_NAME;
			wc.lpfnWndProc=(WNDPROC)TPropGridInfo::__wndProc__;
			wc.hInstance=hInstance;
		::RegisterClass(&wc);
		return PG_CLASS_NAME;
	}

	HWND WINAPI CPropGridCtrl::Create(HINSTANCE hInstance,LPCTSTR windowName,UINT style,int x,int y,int width,int height,HWND hParent){
		// creates and returns a new instance of the PropertyGrid
		//if (width<=2*CATEGORY_HEIGHT) width=2*CATEGORY_HEIGHT;
		return ::CreateWindow(	GetWindowClass(hInstance), // making sure the window class is always defined
								windowName,
								WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | style,
								x,y, width,height,
								hParent, 0,hInstance,NULL
							);
	}

	static TPropGridInfo::PCategoryItem __inferCategory__(HWND hPropGrid,HANDLE category){
		// determines and returns the Category refered to by the input parameters
		const PPropGridInfo pPropGridInfo=GET_PROPGRID_INFO(hPropGrid);
		if (!category) // if no Category specified ...
			return &pPropGridInfo->root; // ... returning the main Category
		else if (((TPropGridInfo::PCItem)category)->__isCategory__()) // if explicitly specifying a Category ...
			return (TPropGridInfo::PCategoryItem)category; // ... returning it
		else
			return 0; // no Category is refered by the input parameters - quitting with error
	}

	HANDLE WINAPI CPropGridCtrl::AddProperty(HWND hPropGrid,HANDLE category,LPCTSTR name,PValue value,TValueSize valueBytes,PCEditor editor,PCustomParam param){
		// creates, adds into PropertyGrid, and returns a new ValueItem with given Name and Value
		if (const TPropGridInfo::PCategoryItem inferredCategory=__inferCategory__(hPropGrid,category))
			// creating a new ValueItem in the specified Category
			return	new TPropGridInfo::TItem( // is also automatically added to the ListBox if Item's all ParentCategories are Expanded
						GET_PROPGRID_INFO(hPropGrid),
						inferredCategory,
						name,
						(::PCEditor)editor,
						value, valueBytes,
						param
					);
		else
			// no Category is refered to by the input parameters - quitting with failure
			return 0;
	}

	HANDLE WINAPI CPropGridCtrl::AddCategory(HWND hPropGrid,HANDLE category,LPCTSTR name,bool initiallyExpanded){
		// creates, adds into PropertyGrid, and returns a new CategoryItem with given Name
		if (const TPropGridInfo::PCategoryItem inferredCategory=__inferCategory__(hPropGrid,category))
			// creating a new CategoryItem in the specified Category
			return	new TPropGridInfo::TCategoryItem( // is also automatically added to the ListBox if Item's all ParentCategories are Expanded
						GET_PROPGRID_INFO(hPropGrid),
						inferredCategory,
						name,
						initiallyExpanded
					);
		else
			// no Category is refered to by the input parameters - quitting with failure
			return 0;
	}

	HANDLE WINAPI CPropGridCtrl::EnableProperty(HWND hPropGrid,HANDLE propOrCat,bool enabled){
		// enables/disables specified PropertyOrCategory (for Category recurrently all its Subitems), and returns the PropertyOrCategory; an Item must be enabled the same amout of times as it was disabled
		const PPropGridInfo pPropGridInfo=GET_PROPGRID_INFO(hPropGrid);
		TPropGridInfo::TItem *const pItem=	propOrCat
											? (TPropGridInfo::TItem *)propOrCat // enabling/disabling particular Item
											: &pPropGridInfo->root; // enabling/disabling the whole content of specified PropertyGrid
		if (enabled)
			pItem->__enable__();
		else
			pItem->__disable__();
		::InvalidateRect( pPropGridInfo->listBox.handle, NULL, TRUE );
		return pItem;
	}

	void WINAPI CPropGridCtrl::RemoveProperty(HWND hPropGrid,HANDLE propOrCat){
		// removes specified PropertyOrCategory from the PropertyGrid
		// - cancelling any editing
		TEditor::__cancelEditing__();
		// - removing
		const PPropGridInfo pPropGridInfo=GET_PROPGRID_INFO(hPropGrid);
		if (propOrCat){
			// removing only a particular Item
			pPropGridInfo->listBox.__removeItem__( (TPropGridInfo::PCItem)propOrCat ); // removing it from the ListBox
			delete (TPropGridInfo::PCItem)propOrCat; // deleting it; is automatically also removed from its ParentCategory
		}else
			// clearing the whole PropertyGrid
			::SendMessage( pPropGridInfo->listBox.handle, LB_RESETCONTENT, 0,0 );
	}

	static WNDPROC upDownWndProc0;

	static LRESULT CALLBACK __upDown_wndProc__(HWND hUpDown,UINT msg,WPARAM wParam,LPARAM lParam){
		switch (msg){
			case WM_MOUSEACTIVATE:
				// preventing the focus from being stolen by the parent
				return MA_ACTIVATE;
		}
		return ::CallWindowProc(upDownWndProc0,hUpDown,msg,wParam,lParam);
	}

	HWND WINAPI CPropGridCtrl::CreateUpDownControl(HWND hEdit,UINT style,bool bHexadecimal,TInteger::RCUpDownLimits rLimits,int iCurrent){
		// creates and returns an UpDown control attached to the specified Edit-box
		static const INITCOMMONCONTROLSEX icc={ sizeof(INITCOMMONCONTROLSEX), ICC_UPDOWN_CLASS };
		if (::InitCommonControlsEx(&icc)){
			const HWND hUpDown=::CreateWindow(	UPDOWN_CLASS, NULL,
												style | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_HOTTRACK | UDS_NOTHOUSANDS,
												0,0, 10,10, ::GetParent(hEdit), 0,
												(HINSTANCE)::GetWindowLong(hEdit,GWL_HINSTANCE),
												NULL
											);
			upDownWndProc0=(WNDPROC)::SetWindowLong( hUpDown, GWL_WNDPROC, (LONG)__upDown_wndProc__ );
			::SetWindowLong( hEdit, GWL_STYLE, ::GetWindowLong(hEdit,GWL_STYLE)|ES_NUMBER );
			::SendMessage( hUpDown, UDM_SETBUDDY, (WPARAM)hEdit, 0 );
			::SendMessage( hUpDown, UDM_SETBASE, bHexadecimal?16:10, 0 );
			::SendMessage( hUpDown, UDM_SETRANGE32, rLimits.iMax, rLimits.iMin );
			::SendMessage( hUpDown, UDM_SETPOS32, 0, iCurrent );
			return hUpDown;
		}else
			return 0;
	}

	#define ELLIPSIS_BUTTON_WIDTH	20

	HWND WINAPI CPropGridCtrl::BeginEditValue(PValue value,TValueSize valueBytes,PCustomParam param,PCEditor editor,RECT rcEditorRect,DWORD style,HWND hParent,HWND *pOutEllipsisBtn){
		// creates and returns the MainControl (and EllipsisButton) of the Editor for the specified Value
		// - no other Value must be edited
		if (IsValueBeingEdited())
			return 0;
		// - Parent must be specified
		if (!hParent || hParent==INVALID_HANDLE_VALUE)
			return 0;
		// - creating the MainControl (and EllipsisButton)
		new TEditor::TControl( (::PCEditor)editor, value, valueBytes, param, style, hParent );
		// - stretching the MainControl (and EllipsisButton) across whole dedicated rectangle
		const int height=rcEditorRect.bottom-rcEditorRect.top;
		if (TEditor::pSingleShown->hEllipsisBtn){
			rcEditorRect.right-=ELLIPSIS_BUTTON_WIDTH;
			::SetWindowPos(	TEditor::pSingleShown->hEllipsisBtn, 0,
							rcEditorRect.right, rcEditorRect.top,
							ELLIPSIS_BUTTON_WIDTH, height,
							SWP_NOZORDER|SWP_SHOWWINDOW
						);
		}
		::SetWindowPos(	TEditor::pSingleShown->hMainCtrl, 0,
						rcEditorRect.left, rcEditorRect.top,
						rcEditorRect.right-rcEditorRect.left, height,
						SWP_NOZORDER|SWP_SHOWWINDOW
					);
		// - outputting the results
		if (pOutEllipsisBtn)
			*pOutEllipsisBtn=TEditor::pSingleShown->hEllipsisBtn;
		return TEditor::pSingleShown->hMainCtrl;
	}

	bool WINAPI CPropGridCtrl::TryToAcceptCurrentValueAndCloseEditor(){
		// True <=> current Value in the Editor is acceptable, otherwise False (and the Editor remains to exist)
		// - a Value must be actually being edited
		if (!IsValueBeingEdited())
			return true; // "non-existing Value can be accepted" (suitable definition of otherwise underedminable state)
		// - attempting to accept current Value in the Editor
		const HWND hParent=::GetParent( TEditor::pSingleShown->mainControlExists ? TEditor::pSingleShown->hMainCtrl : TEditor::pSingleShown->hEllipsisBtn );
		::SetFocus(hParent); // attempting to accept the Value
		if (!IsValueBeingEdited()){
			// new Value successfully accepted (and propagated to the ValueBuffer)
			::InvalidateRect( hParent, NULL, TRUE ); // repainting PropertyGrid's ListBox (or the Parent in general)
			return true; // Value accepted
		}else
			// Value NOT accepted (and the Editor remains to exist)
			return false;
	}

	bool WINAPI CPropGridCtrl::IsValueBeingEdited(){
		// True <=> some Value is currently being edited, otherwise False
		return TEditor::pSingleShown!=NULL;
	}

	void WINAPI CPropGridCtrl::UndefineEditor(PCEditor editor){
		// destroys the specified Editor
		delete (::PCEditor)editor;
	}
