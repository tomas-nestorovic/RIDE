#ifndef PROPGRID_H
#define PROPGRID_H

	#define GET_PROPGRID_INFO(hWnd)			((PPropGridInfo)::GetWindowLong((hWnd),GWL_USERDATA))
	#define GET_PROPGRID_HINSTANCE(hWnd)	((HINSTANCE)::GetWindowLong((hWnd),GWL_HINSTANCE))

	#define CATEGORY_HEIGHT			21

	typedef struct TPropGridInfo sealed{
		struct TCategoryItem; // forward
		typedef TCategoryItem *PCategoryItem;

		typedef const struct TItem{
			const PCategoryItem parentCategory;
			const LPCWSTR name;
			//TPropGridInfo *const pPropGridInfo;
			const struct TValue sealed{
				const PCEditor editor; // either ==TEditor::Category (then this Item is a category), or !=TEditor::Category (then this Item is a property)
				const PropGrid::PValue buffer;
				const PropGrid::PCustomParam param;

				TValue(PCEditor editor,PropGrid::PValue buffer,PropGrid::PCustomParam param);
			} value;
			BYTE disabled; // >0 = this Item cannot be Edited
			TItem *nextInCategory;

			TItem(TPropGridInfo *pPropGridInfo,PCategoryItem parentCategory,LPCWSTR name,PCEditor editor,PropGrid::PValue buffer,PropGrid::PCustomParam param);
			virtual ~TItem();

			virtual void __drawIndentedName__(HDC dc,RECT rc,HFONT hFont) const;
			virtual void __enable__();
			virtual void __disable__();
			BYTE __getLevel__() const;
			bool __isTopLevel__() const;
			bool __isCategory__() const;
		} *PCItem;

		struct TCategoryItem sealed:public TItem{
			TItem *subitems;
			bool expanded; // use TListBox::__expand/collapseCategory__ to modify

			TCategoryItem(TPropGridInfo *pPropGridInfo,PCategoryItem parentCategory,LPCWSTR name,bool initiallyExpanded);
			~TCategoryItem();

			void __drawIndentedName__(HDC dc,RECT rc,HFONT hFont) const override;
			void __enable__() override;
			void __disable__() override;
		};

		static HCURSOR CURSOR_SPLITTER;
		static HBRUSH BRUSH_BLACK;
		static HBRUSH BRUSH_GRAY_DARK;
		static HBRUSH BRUSH_GRAY;
		static HBRUSH BRUSH_GRAY_LIGHT;
		static HBRUSH BRUSH_WHITE;
		static HBRUSH BRUSH_SELECTION;
		static HPEN PEN_BLACK;
		static HPEN PEN_GRAY_DARK;
		static HPEN PEN_GRAY;
		static HPEN PEN_GRAY_LIGHT;
		static HFONT FONT_DEFAULT;
		static HFONT FONT_DEFAULT_BOLD;
		static HBITMAP CHECKBOX_STATES;
		static HBITMAP CHECKBOX_CHECKED;

		static void __init__();
		static void __uninit__();
		static void __scaleLogicalUnit__(HDC dc);
		static LRESULT CALLBACK __wndProc__(HWND hPropGrid,UINT msg,WPARAM wParam,LPARAM lParam);

		const HWND handle;
		struct TListBox sealed{
			static LRESULT CALLBACK __wndProc__(HWND hListBox,UINT msg,WPARAM wParam,LPARAM lParam);

			struct TSplitter sealed{
				static HWND hListBoxWithCurrentlyDraggedSplitter; // handle of the ListBox whose Splitter is being dragged

				int position;

				TSplitter(int position);

				bool __isCloseEnough__(int x) const;
				void __beginDrag__(const TPropGridInfo *pPropGridInfo,int fromPosition);
				void __drawInvertedLine__() const;
				void __endDrag__(int toPosition);
			} splitter;

			const HWND handle;

			TListBox(TPropGridInfo *pPropGridInfo,LONG propGridWidth,LONG propGridHeight);

			void __addItem__(PCItem item) const;
			void __removeItem__(PCItem item) const;
			void __addCategorySubitems__(PCategoryItem category) const;
			void __removeCategorySubitems__(PCategoryItem category) const;
		} listBox;
		TCategoryItem root;

		TPropGridInfo(HWND hPropGrid,LONG propGridWidth,LONG propGridHeight);
	} *PPropGridInfo;

	typedef const TPropGridInfo *PCPropGridInfo;



	extern const struct TRationalNumber:public div_t{
		TRationalNumber();

		int operator*(short i) const;
		bool operator!=(int i) const;
	} LogicalUnitScaleFactor;

#endif // PROPGRID_H
