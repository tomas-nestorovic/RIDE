#include "stdafx.h"

	#define ITEM_LEVEL_TOP			1

	TPropGridInfo::TItem::TValue::TValue(PCEditor editor,PropGrid::PValue buffer,PropGrid::PCustomParam param)
		// ctor
		: editor(editor) , buffer(buffer) , param(param) {
	}



	TPropGridInfo::TItem::TItem(TPropGridInfo *pPropGridInfo,PCategoryItem parentCategory,LPCWSTR name,PCEditor editor,PropGrid::PValue buffer,PropGrid::PCustomParam param)
		// ctor
		// - initialization
		: parentCategory(parentCategory)
		, name(::lstrcpyW((PWCHAR)::malloc((1+::lstrlenW(name))*sizeof(WCHAR)),name))
		, value(editor,buffer,param)
		, disabled( parentCategory ? parentCategory->disabled : 0 )
		, nextInCategory(nullptr) {
		// - adding this Item to the end of the ParentCategory
		if (parentCategory){ // NOT PropertyGrid's Root Category
			TItem **a=&parentCategory->subitems;
			while (TItem *const p=*a) a=&p->nextInCategory;
			*a=this;
		//}
		// - redrawing the ListBox if all ParentCategories are Expanded
		//if (parentCategory){ // NOT PropertyGrid's Root Category
			const TCategoryItem *c=parentCategory;
			while (c)
				if (c->expanded) c=c->parentCategory; else break;
			if (!c)
				pPropGridInfo->listBox.__addItem__(this);
		}
	}

	TPropGridInfo::TItem::~TItem(){
		// dtor
		// - removing this Item from the ParentCategory
		if (parentCategory){ // NOT PropertyGrid's Root Category
			TItem **a=&parentCategory->subitems;
			while (*a!=this) a=&(*a)->nextInCategory;
			*a=nextInCategory;
		}
		// - uninitialization
		::free((PVOID)name);
	}

	void TPropGridInfo::TItem::__drawIndentedName__(HDC dc,RECT rc,HFONT hFont) const{
		// draws indented Item's Name into the Rectangle using specified Font
		const HGDIOBJ hFont0=::SelectObject(dc,hFont);
			rc.left=LogicalUnitScaleFactor
					*
					( CATEGORY_HEIGHT + (__getLevel__()-ITEM_LEVEL_TOP)*EDITOR_DEFAULT_HEIGHT + PROPGRID_CELL_MARGIN_LEFT );
			::DrawTextW(dc,
						name, -1, &rc,
						DT_SINGLELINE | DT_LEFT | DT_VCENTER
					);
		::SelectObject(dc,hFont0);
	}

	void TPropGridInfo::TItem::__enable__(){
		// enables this Item (i.e. "unlocks" this Item, making it eventually editable)
		if (disabled>0)
			disabled--;
	}

	void TPropGridInfo::TItem::__disable__(){
		// disables this Item (i.e. "locks" this Item, making it not editable)
		if (++disabled)
			TEditor::__cancelEditing__();
	}

	BYTE TPropGridInfo::TItem::__getLevel__() const{
		// determines and returns the "sub-level" of this Item
		BYTE result=ITEM_LEVEL_TOP;
		for( const TCategoryItem *c=parentCategory; c=c->parentCategory; result++ );
		return result;
	}

	bool TPropGridInfo::TItem::__isTopLevel__() const{
		// True <=> this Item is in the top level (e.g. a top-level Category)
		return __getLevel__()==ITEM_LEVEL_TOP;
	}

	bool TPropGridInfo::TItem::__isCategory__() const{
		// True <=> this Item is actually a Category, otherwise False
		return value.editor==nullptr;
	}












	TPropGridInfo::TCategoryItem::TCategoryItem(TPropGridInfo *pPropGridInfo,PCategoryItem parentCategory,LPCWSTR name,bool initiallyExpanded)
		// ctor
		// - base
		: TItem( pPropGridInfo, parentCategory, name, nullptr, nullptr, nullptr )
		// - initialization
		, subitems(nullptr)
		, expanded(initiallyExpanded) {
	}

	TPropGridInfo::TCategoryItem::~TCategoryItem(){
		// dtor
		while (subitems)
			delete subitems; // also removes the Subitem from the list
	}

	#define CATEGORY_SYMBOL_SIZE	11

	void TPropGridInfo::TCategoryItem::__drawIndentedName__(HDC dc,RECT rc,HFONT hFont) const{
		// draws indented Category's Name and icon on Expansion ("[+/-]") into the Rectangle using specified Font
		// - name
		TItem::__drawIndentedName__(dc,rc,hFont);
		// - icon on Expansion ("[+/-]")
		const int iDc0=::SaveDC(dc);
			::SetViewportOrgEx(	dc,
								LogicalUnitScaleFactor*(CATEGORY_HEIGHT+(__getLevel__()-2*ITEM_LEVEL_TOP)*EDITOR_DEFAULT_HEIGHT),
								rc.top,
								nullptr
							);
			__scaleLogicalUnit__(dc);
			// . drawing the "[-]" icon
			::Rectangle(dc,
						(CATEGORY_HEIGHT-CATEGORY_SYMBOL_SIZE)/2, (CATEGORY_HEIGHT-CATEGORY_SYMBOL_SIZE)/2,
						(CATEGORY_HEIGHT+CATEGORY_SYMBOL_SIZE)/2, (CATEGORY_HEIGHT+CATEGORY_SYMBOL_SIZE)/2
					);
			::MoveToEx( dc, CATEGORY_HEIGHT/2-2, CATEGORY_HEIGHT/2, nullptr );
			::LineTo( dc, CATEGORY_HEIGHT/2+2+1, CATEGORY_HEIGHT/2 ); // "+1" because the last point of a line isn't drawn
			// . if the Category is collapsed, changing the above "[-]" icon to "[+]"
			if (!expanded){ // is collapsed
				::MoveToEx( dc, CATEGORY_HEIGHT/2, CATEGORY_HEIGHT/2-2, nullptr );
				::LineTo( dc, CATEGORY_HEIGHT/2, CATEGORY_HEIGHT/2+2+1 ); // "+1" because the last point of a line isn't drawn
			}
		::RestoreDC(dc,iDc0);
	}

	void TPropGridInfo::TCategoryItem::__enable__(){
		// enables this Item (i.e. "unlocks" this Item, making it eventually editable)
		// - base
		__super::__enable__();
		// - enabling all Subitems in this Category
		for( TItem *p=subitems; p!=nullptr; p=p->nextInCategory )
			p->__enable__();
	}

	void TPropGridInfo::TCategoryItem::__disable__(){
		// disables this Item (i.e. "locks" this Item, making it not editable)
		// - base
		__super::__disable__();
		// - disabling all Subitems in this Category
		for( TItem *p=subitems; p!=nullptr; p=p->nextInCategory )
			p->__disable__();
	}
