#include "stdafx.h"

	HWND TPropGridInfo::TListBox::TSplitter::hListBoxWithCurrentlyDraggedSplitter;

	TPropGridInfo::TListBox::TSplitter::TSplitter(int position)
		// ctor
		: position(position) {
	}




	void TPropGridInfo::TListBox::TSplitter::__beginDrag__(const TPropGridInfo *pPropGridInfo,int fromPosition){
		// begins dragging of the Splitter in specified ListBox and FromPosition
		// - initializing
		hListBoxWithCurrentlyDraggedSplitter=pPropGridInfo->listBox.handle;
		position=fromPosition;
		// - setting apropriate cursor
		::SetCursor(CURSOR_SPLITTER);
		// - painting the DraggedSplitter
		__drawInvertedLine__();
		// - constraining the cursor to the ListBox area
		RECT r;
		::GetWindowRect( hListBoxWithCurrentlyDraggedSplitter, &r );
			r.left+=LogicalUnitScaleFactor*CATEGORY_HEIGHT;
		::ClipCursor(&r);
		// - wanting to know when the mouse button is released
		::SetCapture(hListBoxWithCurrentlyDraggedSplitter);
	}

	#define SPLITTER_TOLERANCE	5

	bool TPropGridInfo::TListBox::TSplitter::__isCloseEnough__(int x) const{
		// True <=> X is close enough to the Splitter's Position, otherwise False
		return ::abs(x-position)<=LogicalUnitScaleFactor*SPLITTER_TOLERANCE;
	}

	void TPropGridInfo::TListBox::TSplitter::__drawInvertedLine__() const{
		// draws the Splitter by inverting a vertical line at the Splitter's current Position
		const HDC dc=GetDC(hListBoxWithCurrentlyDraggedSplitter);
			RECT r;
			::GetClientRect( hListBoxWithCurrentlyDraggedSplitter, &r );
			const int mode0=::SetROP2(dc,R2_NOT);
				::MoveToEx( dc, position, r.top, nullptr );
				::LineTo( dc, position, r.bottom );
			::SetROP2(dc,mode0);
		ReleaseDC(hListBoxWithCurrentlyDraggedSplitter,dc);
	}

	void TPropGridInfo::TListBox::TSplitter::__endDrag__(int toPosition){
		// terminating the dragging of the Splitter
		// - releasing the mouse capture
		::ReleaseCapture();
		// . removing the constraining of the cursor to the ListBox area
		::ClipCursor(nullptr);
		// - uninitializing
		position=toPosition;
		hListBoxWithCurrentlyDraggedSplitter=(HWND)INVALID_HANDLE_VALUE;
	}











	static WNDPROC wndProc0; // ListBox's original window procedure

	TPropGridInfo::TListBox::TListBox(TPropGridInfo *pPropGridInfo,LONG propGridWidth,LONG propGridHeight)
		// ctor
		: handle( ::CreateWindow(	WC_LISTBOX, nullptr,
									WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWVARIABLE | LBS_WANTKEYBOARDINPUT,
									0,0, propGridWidth,propGridHeight,
									pPropGridInfo->handle, 0,
									GET_PROPGRID_HINSTANCE(pPropGridInfo->handle),
									nullptr
								)
				)
		, splitter( LogicalUnitScaleFactor*(propGridWidth-LogicalUnitScaleFactor*CATEGORY_HEIGHT)/2 ) {
		wndProc0=(WNDPROC)SubclassWindow(handle,__wndProc__);
		SetWindowFont(handle,FONT_DEFAULT,0);
		::SetWindowLong( handle, GWL_USERDATA, (LONG)pPropGridInfo );
	}





	LRESULT CALLBACK TPropGridInfo::TListBox::__wndProc__(HWND hListBox,UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		const PPropGridInfo pPropGridInfo=GET_PROPGRID_INFO(hListBox);
		const int x=GET_X_LPARAM(lParam);
		switch (msg){
			case WM_GETDLGCODE:
				// the ListBox must receive all keyboard input (it may not receive a Tab keystroke if part of a dialog or CControlBar)
				return DLGC_WANTALLKEYS;
			case WM_MOUSEACTIVATE:
				// preventing the focus from being stolen by the parent
				return MA_ACTIVATE;
			case WM_LBUTTONDOWN:{
				// left mouse button pressed
				// . if cursor not above any Item, ignoring the click
				const int i=::SendMessage(hListBox,LB_ITEMFROMPOINT,0,lParam);
				if (i<0)
					return 0;
				// . if cursor above the Splitter, this is the beginning of its dragging
				if (pPropGridInfo->listBox.splitter.__isCloseEnough__(x)){
					// : cancelling any editing
					TEditor::__cancelEditing__();
					// : initiating the dragging
					pPropGridInfo->listBox.splitter.__beginDrag__(pPropGridInfo,x);
					break;
				}
				// . if cursor above a Category, its expansion/collapsing
				TItem *const pItem=(TItem *)ListBox_GetItemData(hListBox,i);
				if (pItem && pItem->__isCategory__()){
					// : cancelling any editing
					TEditor::__cancelEditing__();
					// : expanding/collapsing the Category
					const PCategoryItem c=(PCategoryItem)pItem;
					if (c->expanded=!c->expanded)
						pPropGridInfo->listBox.__addCategorySubitems__(c);
					else
						pPropGridInfo->listBox.__removeCategorySubitems__(c);
					// : painting the result
					::InvalidateRect(hListBox,nullptr,TRUE);
					break;
				}
				break;
			}
			case WM_MOUSEMOVE:
				// mouse moved
				if (TSplitter::hListBoxWithCurrentlyDraggedSplitter==hListBox){
					// Splitter of this PropertyGrid is being dragged
					// . erasing the Splitter from its current Position
					pPropGridInfo->listBox.splitter.__drawInvertedLine__();
					// . painting the Splitter to a new Position
					pPropGridInfo->listBox.splitter.position=x;
					pPropGridInfo->listBox.splitter.__drawInvertedLine__();
					return 0;
				}else if (pPropGridInfo->listBox.splitter.__isCloseEnough__(x))
					// moving above the Splitter
					::SetCursor(CURSOR_SPLITTER); // indicating the possibility to begin to drag the Splitter
				break;
			case WM_LBUTTONUP:
				// left mouse button released
				// . if Splitter of this PropertyGrid is being dragged, terminating the dragging
				if (TSplitter::hListBoxWithCurrentlyDraggedSplitter==hListBox){
					// : terminating the dragging
					pPropGridInfo->listBox.splitter.__endDrag__(x);
					// : painting the result
					::InvalidateRect(hListBox,nullptr,TRUE);
					return 0;
				}
				// . if clicked on Item's Value part, beginning to edit the Value using the corresponding Editor
				if (x>pPropGridInfo->listBox.splitter.position){
					const int i=::SendMessage(hListBox,LB_ITEMFROMPOINT,0,lParam);
					ListBox_SetCurSel(hListBox,i); // otherwise the selection would be set no earlier than in the base window procedure which is too late (see GetCurSel below)
					if (i>=0) // yes, an Item has been selected
						goto editItem;
				}
				break;
			case WM_KEYDOWN:
				// a key has been pressed
				if (wParam==VK_TAB){
editItem:			// Tab key pressed - beginning to edit the Value using the corresponding Editor
					// . cancelling any editing
					TEditor::__cancelEditing__();
					// . showing the Editor of the Item's Value (if the Item can be edited)
					const int i=ListBox_GetCurSel(hListBox);
					const PCItem pItem=(PCItem)ListBox_GetItemData(hListBox,i);
					if (!pItem->disabled && !pItem->__isCategory__()){ // yes, can be edited
						// : determining the Item's Value area
						RECT r;
						ListBox_GetItemRect(hListBox,i,&r);
						r.left=pPropGridInfo->listBox.splitter.position;
						::InflateRect(&r,-1,-1); // excluding lines separating individial Items
						// : initiating the editing
						PropGrid::BeginEditValue(
							pItem->value.buffer,
							pItem->value.param,
							pItem->value.editor,
							r,
							WS_CHILD | WS_CLIPSIBLINGS,
							pPropGridInfo->handle,
							nullptr
						);
						return 0;
					}
				}
				break;
			case LB_RESETCONTENT:
				// destroying the PropertyGrid
				// . removing all Items
				for( int i=ListBox_GetCount(hListBox); i--; delete (PCItem)ListBox_GetItemData(hListBox,i) ); // also removes the Subitem from the PropertyGrid instance
				// . base
				break;
		}
		return ::CallWindowProc(wndProc0,hListBox,msg,wParam,lParam);
	}




	void TPropGridInfo::TListBox::__addCategorySubitems__(PCategoryItem category) const{
		// expands the Category in the ListBox
		// . adding Category's immediate Subitems into the ListBox (recurrently)
		for( PCItem item=category->subitems; item; item=item->nextInCategory ){
			__addItem__(item);
			if (item->__isCategory__()){
				const PCategoryItem subcategory=(PCategoryItem)item;
				if (subcategory->expanded)
					__addCategorySubitems__(subcategory);
			}
		}
		// . painting the result
		::InvalidateRect(handle,nullptr,TRUE);
	}

	void TPropGridInfo::TListBox::__removeCategorySubitems__(PCategoryItem category) const{
		// collapses the Category in the ListBox
		// . removing Category's immediate Subitems from the ListBox (recurrently)
		for( PCItem item=category->subitems; item; item=item->nextInCategory ){
			__removeItem__(item);
			if (item->__isCategory__())
				__removeCategorySubitems__( (PCategoryItem)item );
		}
		// . painting the result
		::InvalidateRect(handle,nullptr,TRUE);
	}

	void TPropGridInfo::TListBox::__addItem__(PCItem item) const{
		// adds the Item into the ListBox at logically correct position
		// - if Item's "right successor" exists (in the Category-Values hierarchy), adding the Item at its position
		PCItem c=item->parentCategory;
		do{
			if (c->nextInCategory){
				ListBox_InsertItemData(	handle,
										ListBox_SelectItemData(handle,0,c->nextInCategory),
										item
									);
				return;
			}
		}while ( c=c->parentCategory );
		// - Item's "right successor" doesn't exist in the hierarchy, adding the Item to the end of the ListBox
		ListBox_AddItemData(handle,item);
	}

	void TPropGridInfo::TListBox::__removeItem__(PCItem item) const{
		// removes the Item from the ListBox
		ListBox_DeleteString( handle, ListBox_FindItemData(handle,0,item) );
	}
