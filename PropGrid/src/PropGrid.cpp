#include "stdafx.h"

	HCURSOR TPropGridInfo::CURSOR_SPLITTER;
	HBRUSH TPropGridInfo::BRUSH_BLACK;
	HBRUSH TPropGridInfo::BRUSH_GRAY_DARK;
	HBRUSH TPropGridInfo::BRUSH_GRAY;
	HBRUSH TPropGridInfo::BRUSH_GRAY_LIGHT;
	HBRUSH TPropGridInfo::BRUSH_WHITE;
	HBRUSH TPropGridInfo::BRUSH_SELECTION;
	HPEN TPropGridInfo::PEN_BLACK;
	HPEN TPropGridInfo::PEN_GRAY_DARK;
	HPEN TPropGridInfo::PEN_GRAY;
	HPEN TPropGridInfo::PEN_GRAY_LIGHT;
	HFONT TPropGridInfo::FONT_DEFAULT;
	HFONT TPropGridInfo::FONT_DEFAULT_BOLD;
	HBITMAP TPropGridInfo::CHECKBOX_STATES;
	HBITMAP TPropGridInfo::CHECKBOX_CHECKED;



	#define COLOR_AVERAGE(b1,b2,k1,k2)	\
				RGB((GetRValue(b1)*k1+GetRValue(b2)*k2)/(k1+k2),	\
					(GetGValue(b1)*k1+GetGValue(b2)*k2)/(k1+k2),	\
					(GetBValue(b1)*k1+GetBValue(b2)*k2)/(k1+k2)		\
				)

	void TPropGridInfo::__init__(){
		// "static ctor"
		// - cursors
		CURSOR_SPLITTER=::LoadCursor(0,IDC_SIZEWE);
		// - brushes
		BRUSH_BLACK=(HBRUSH)::GetStockObject(BLACK_BRUSH);
		BRUSH_GRAY_DARK=::GetSysColorBrush(COLOR_BTNSHADOW);
		BRUSH_GRAY=::GetSysColorBrush(COLOR_BTNFACE);
			COLORREF clr1=::GetSysColor(COLOR_BTNFACE) , clr2=::GetSysColor(COLOR_BTNHIGHLIGHT);
			LOGBRUSH brush={ BS_SOLID, COLOR_AVERAGE(clr1,clr2,2,5), 0 };
		BRUSH_GRAY_LIGHT=::CreateBrushIndirect(&brush);
		BRUSH_WHITE=(HBRUSH)::GetStockObject(WHITE_BRUSH);
		BRUSH_SELECTION=::GetSysColorBrush(COLOR_ACTIVECAPTION);
		// - pens
		PEN_BLACK=(HPEN)::GetStockObject(BLACK_PEN);
		PEN_GRAY_DARK=::CreatePen(PS_SOLID,1,::GetSysColor(COLOR_BTNSHADOW));
			clr1=::GetSysColor(COLOR_BTNFACE) , clr2=::GetSysColor(COLOR_BTNSHADOW);
		PEN_GRAY=::CreatePen( PS_SOLID, 1, COLOR_AVERAGE(clr1,clr2,1,1) );
		PEN_GRAY_LIGHT=::CreatePen(PS_SOLID,1,::GetSysColor(COLOR_BTNHIGHLIGHT));
		// - fonts
		FONT_DEFAULT=(HFONT)::GetStockObject(DEFAULT_GUI_FONT);
		LOGFONT font;
		::GetObject(FONT_DEFAULT,sizeof(font),&font);
			font.lfWeight=FW_BOLD;
		FONT_DEFAULT_BOLD=::CreateFontIndirect(&font);
		// - bitmaps
		//CHECKBOX_STATES=(HBITMAP)::LoadImage( ComresLib , MAKEINTRESOURCE(2365) , IMAGE_BITMAP , 0,0,LR_LOADTRANSPARENT );
		CHECKBOX_STATES=::LoadBitmap( 0, MAKEINTRESOURCE(OBM_CHECKBOXES) );
		CHECKBOX_CHECKED=::LoadBitmap( 0, MAKEINTRESOURCE(OBM_CHECK) );
	}

	void TPropGridInfo::__uninit__(){
		// "static dtor"
		// - bitmaps
		::DeleteObject(CHECKBOX_CHECKED);
		::DeleteObject(CHECKBOX_STATES);
		// - fonts
		::DeleteObject(FONT_DEFAULT_BOLD);
		// - pens
		::DeleteObject(PEN_GRAY_DARK);
		::DeleteObject(PEN_GRAY);
		::DeleteObject(PEN_GRAY_LIGHT);
		// - brushes
		::DeleteObject(BRUSH_GRAY_DARK);
		::DeleteObject(BRUSH_GRAY);
		::DeleteObject(BRUSH_GRAY_LIGHT);
		::DeleteObject(BRUSH_SELECTION);
		// - cursors
		::DestroyCursor(CURSOR_SPLITTER);
	}

	#define SCREEN_DPI_DEFAULT	96

	static float __getLogicalUnitScaleFactor__(){
		// computes and returns the factor (from (0;oo)) to multiply the size of one logical unit with; returns 1 if the logical unit size doesn't have to be changed
		const HDC screen=::GetDC(NULL);
			const float result=min(	::GetDeviceCaps(screen,LOGPIXELSX)/(float)SCREEN_DPI_DEFAULT,
									::GetDeviceCaps(screen,LOGPIXELSY)/(float)SCREEN_DPI_DEFAULT
								);
		::ReleaseDC(NULL,screen);
		return result;
	}

	const float TPropGridInfo::LogicalUnitScaleFactor=__getLogicalUnitScaleFactor__();

	float TPropGridInfo::__scaleLogicalUnit__(HDC dc){
		// changes given DeviceContext's size of one logical unit; returns the Factor using which the logical unit size has been multiplied with
		if (LogicalUnitScaleFactor!=1){
			::SetMapMode(dc,MM_ISOTROPIC);
			::SetWindowExtEx( dc, SCREEN_DPI_DEFAULT, SCREEN_DPI_DEFAULT, NULL );
			::SetViewportExtEx( dc, ::GetDeviceCaps(dc,LOGPIXELSX), ::GetDeviceCaps(dc,LOGPIXELSY), NULL );
		}
		return LogicalUnitScaleFactor;
	}












	TPropGridInfo::TPropGridInfo(HWND hPropGrid,WORD propGridWidth,WORD propGridHeight)
		// ctor
		: handle(hPropGrid)
		, listBox(this,propGridWidth,propGridHeight)
		, root(this,NULL,NULL,true) {
	}








	LRESULT CALLBACK TPropGridInfo::__wndProc__(HWND hPropGrid,UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		static unsigned int nInstances; // number of created PropertyGrid windows
		switch (msg){
			case WM_CREATE:{
				// window created
				// . initializing resources if creating the first PropertyGrid window
				if (::InterlockedIncrement(&nInstances)==1)
					__init__();
				// . creating internal representation of the PropertyGrid
				RECT r;
				::GetClientRect(hPropGrid,&r);
				::SetWindowLong( hPropGrid, GWL_USERDATA, (LONG)new TPropGridInfo(hPropGrid,r.right,r.bottom) );
				// . base
				break;
			}
			case WM_SETFOCUS:
				// window has received focus
				::SetFocus( GET_PROPGRID_INFO(hPropGrid)->listBox.handle ); // handing the focus over to the ListBox
				return 0;
			case WM_MEASUREITEM:{
				// determining how much space a particular ListBox Item needs to display
				const PMEASUREITEMSTRUCT pmis=(PMEASUREITEMSTRUCT)lParam;
				if (pmis->CtlType==ODT_LISTBOX){
					// measuring invoked by ListBox
					const PCItem pItem=(PCItem)pmis->itemData;
					pmis->itemHeight =	( pItem->__isCategory__() ? EDITOR_DEFAULT_HEIGHT : pItem->value.editor->height )
										+
										1; // width of the horizontal line separating individial ListBox Items
				}else
					// measuring invoked by the Editor before painting a Value (e.g. owner-drawn combo-box of the TEnum Editor)
					pmis->itemHeight = TEditor::pSingleShown->value.editor->height;
				pmis->itemHeight*=LogicalUnitScaleFactor;
				return 0;
			}
			case WM_DRAWITEM:{
				// drawing ListBox Item
				const LPDRAWITEMSTRUCT pdis=(PDRAWITEMSTRUCT)lParam;
				const HDC dc=pdis->hDC;
				const PCItem pItem=(PCItem)pdis->itemData;
				const int iDc0=::SaveDC(dc);
					// . drawing the structure of ListBox (if this drawing has been invoked by ListBox)
					::SetBkMode(dc,TRANSPARENT);
					if (pdis->CtlType==ODT_LISTBOX){
						// drawing invoked by ListBox
						// : if the ListBox is empty, we are done
						if (pdis->itemID<0)
							goto paintingDone;
						// : if nothing is needed to be drawn, we are done
						if (!(pdis->itemAction&(ODA_SELECT|ODA_DRAWENTIRE)))
							goto paintingDone;
						// : drawing the structure of the PropertyGrid Item
						RECT r=pdis->rcItem;
						if (pItem->__isCategory__() && pItem->__isTopLevel__()){
							// drawing a top-level Category
							::FillRect( dc, &r, BRUSH_GRAY );
							pItem->__drawIndentedName__( dc, r, FONT_DEFAULT_BOLD );
							goto paintingDone;
						}else{
							// drawing a non-top-level Category or a Property
							// | drawing left-most vertical ???band (where the main Categories have their "[+/-]" symbols)
							r.right=CATEGORY_HEIGHT*LogicalUnitScaleFactor;
							::FillRect( dc, &r, BRUSH_GRAY );
							// | drawing left part (where the Property Name is shown)
							const int textColorId=	pItem->disabled>0
													? COLOR_GRAYTEXT
													: pdis->itemState & ODS_SELECTED
													? COLOR_CAPTIONTEXT
													: COLOR_BTNTEXT;
							::SetTextColor( dc, ::GetSysColor(textColorId) );
								r.left=r.right, r.right=GET_PROPGRID_INFO(hPropGrid)->listBox.splitter.position;
								::FillRect( dc, &r, pdis->itemState&ODS_SELECTED ? BRUSH_SELECTION : BRUSH_GRAY_LIGHT );
								const int i=r.bottom;
								const int b=r.top+EDITOR_DEFAULT_HEIGHT*LogicalUnitScaleFactor
											+
											1; // width of the horizontal line separating individial ListBox Items
								r.bottom=min(r.bottom,b);
									pItem->__drawIndentedName__( dc, r, FONT_DEFAULT );
								r.bottom=i;
							// | drawing decorations
							::SelectObject(dc,PEN_GRAY);
								::MoveToEx(dc,CATEGORY_HEIGHT*LogicalUnitScaleFactor,r.bottom,NULL);
								::LineTo(dc,CATEGORY_HEIGHT*LogicalUnitScaleFactor,r.top); // vertical band (where the main Categories have their "[+/-]" symbols)
								::LineTo(dc,10000,r.top); // horizontal delimiter of Properties
							::SelectObject(dc,PEN_GRAY_LIGHT); // vertical splitter
								::MoveToEx(dc,r.right-1,r.top,NULL);
								::LineTo(dc,r.right-1,r.bottom);
							::SelectObject(dc,PEN_GRAY_DARK);
								::MoveToEx(dc,r.right,r.top,NULL);
								::LineTo(dc,r.right,r.bottom);
							// | for non-top-level Category, we are done here
							if (pItem->__isCategory__())
								goto paintingDone;
						}
						// : preparing for drawing Property's Value
						::SetViewportOrgEx(	dc, // origin [0,0] goes to the upper left corner of "value part" of the Item
											(1*LogicalUnitScaleFactor+r.right),
											(1*LogicalUnitScaleFactor+r.top),
											NULL
										);
						::OffsetRect( &pdis->rcItem, 0, -r.top );
						pdis->rcItem.bottom--; // width of the horizontal line separating individial ListBox Items
						::SetTextColor( dc, ::GetSysColor( pItem->disabled ? COLOR_GRAYTEXT : COLOR_BTNTEXT ) );
						// . drawing the Property's Value
						pItem->value.editor->__drawValue__( pItem->value, pdis );
					}else{
						// drawing invoked by the Editor during its operation (e.g. owner-drawn combo-box of the TEnum Editor)
						if (pdis->itemState&ODS_SELECTED){
							::FillRect( dc, &pdis->rcItem, BRUSH_SELECTION );
							::SetTextColor( dc, ::GetSysColor(COLOR_HIGHLIGHTTEXT) );
						}else{
							::FillRect( dc, &pdis->rcItem, BRUSH_WHITE );
							::SetTextColor( dc, 0 );
						}
						TEditor::pSingleShown->value.editor->__drawValue__(
							TItem::TValue(	TEditor::pSingleShown->value.editor,
											&pdis->itemData,
											TEditor::pSingleShown->value.bufferCapacity,
											TEditor::pSingleShown->value.param
										),
							pdis
						);
					}
paintingDone:	::RestoreDC(dc,iDc0);
				return 0;
			}
			case WM_SIZE:
				// window size changed
				TEditor::__cancelEditing__(); // cancelling any running editing of a Property
				::SetWindowPos(	GET_PROPGRID_INFO(hPropGrid)->listBox.handle,
								NULL, 0,0, LOWORD(lParam),HIWORD(lParam),
								SWP_NOMOVE | SWP_NOZORDER
							);
				break;
			case WM_PAINT:
				// painting
				::InvalidateRect( GET_PROPGRID_INFO(hPropGrid)->listBox.handle, NULL, TRUE );
				break;
			case WM_DESTROY:
				// closing the PropertyGrid window
				delete GET_PROPGRID_INFO(hPropGrid);
				if (!::InterlockedDecrement(&nInstances))
					__uninit__();
				break;
		}
		return ::DefWindowProc(hPropGrid,msg,wParam,lParam);
	}
