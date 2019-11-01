#ifndef EDITORS_H
#define EDITORS_H

	#define	EDITOR_DEFAULT_HEIGHT	20
	#define EDITOR_STYLE			( WS_CHILD | WS_CLIPSIBLINGS )

	#define STRING_LENGTH_MAX		(PropGrid::TSize)8192

	#pragma pack(1)
	struct TEditor{
		static const struct TControl sealed{
			const TPropGridInfo::TItem::TValue value;
			const HWND hMainCtrl; // Editor's MainControl handle (WC_EDIT,...)
			const HWND hEllipsisBtn; // optional EllipsisButton handle
			const bool mainControlExists; // True <=> MainControl is an editable control, otherwise False
			const WNDPROC wndProc0;		// Editor's original window procedure
			const WNDPROC ellipsisBtnWndProc0; // EllipsisButton's original window procedure
			//const PropGrid::PValue origValue; // property's original Value
			//const HWND hUpDown;

			TControl(	PCEditor editor,
						PropGrid::PValue valueBuffer,
						PropGrid::PCustomParam param,
						DWORD style,
						HWND hParent
					);
			~TControl();
		} *pSingleShown;

		static void __cancelEditing__();

		const WORD height;
		const bool hasMainControl; // True <=> the Editor features an editable MainControl, otherwise False
		const PropGrid::TOnEllipsisButtonClicked onEllipsisBtnClicked;
		const PropGrid::TOnValueChanged onValueChanged;

		virtual void __drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const=0;
		virtual HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const=0;
		virtual bool __tryToAcceptMainCtrlValue__() const=0;
		virtual LRESULT __mainCtrl_wndProc__(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) const;
	protected:
		static bool ignoreRequestToDestroy;
		
		static void __drawString__(LPCSTR text,short textLength,PDRAWITEMSTRUCT pdis);
		static void __drawString__(LPCWSTR text,short textLength,PDRAWITEMSTRUCT pdis);
		static LRESULT CALLBACK __wndProc__(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam);
		static LRESULT CALLBACK __ellipsisBtn_wndProc__(HWND hEllipsisBtn,UINT msg,WPARAM wParam,LPARAM lParam);

		TEditor(
			WORD height,
			bool hasMainControl,
			PropGrid::TSize valueSize,
			PropGrid::TOnEllipsisButtonClicked onEllipsisBtnClicked,
			PropGrid::TOnValueChanged onValueChanged
		);
	public:
		const PropGrid::TSize valueSize;
	};

	#pragma pack(1)
	struct TStringEditor:public TEditor{
		const bool wideChar;
		const PropGrid::String::TOnValueConfirmed onValueConfirmed;
	protected:
		static HWND __createEditBox__(HWND hParent,UINT extraStyle);

		void __drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const override;
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
		bool __tryToAcceptMainCtrlValue__() const override;
		LRESULT __mainCtrl_wndProc__(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) const override;

		TStringEditor(
			PropGrid::TOnEllipsisButtonClicked onEllipsisBtnClicked,
			bool wideChar,
			PropGrid::TSize nCharsMax,
			PropGrid::String::TOnValueConfirmed onValueConfirmed,
			PropGrid::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	struct TFixedPaddedStringEditor:public TStringEditor{
		const WCHAR paddingChar;
	protected:
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
		bool __tryToAcceptMainCtrlValue__() const override;
	public:
		TFixedPaddedStringEditor(
			PropGrid::TOnEllipsisButtonClicked onEllipsisBtnClicked,
			bool wideChar,
			PropGrid::TSize nCharsMax,
			PropGrid::String::TOnValueConfirmed onValueConfirmed,
			WCHAR paddingChar,
			PropGrid::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	struct TDynamicStringEditor sealed:public TStringEditor{
	protected:
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
	public:
		TDynamicStringEditor(
			bool wideChar,
			PropGrid::String::TOnValueConfirmed onValueConfirmed,
			PropGrid::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	struct TBooleanEditor sealed:public TEditor{
		const PropGrid::Boolean::TOnValueConfirmed onValueConfirmed;
		const DWORD reservedValue;
		const bool reservedForTrue; // True <=> the ReservedValue is for True, otherwise the ReservedValue is for False
	protected:
		void __drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const override;
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
		bool __tryToAcceptMainCtrlValue__() const override;
		LRESULT __mainCtrl_wndProc__(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) const override;
	public:
		TBooleanEditor(
			PropGrid::TSize nValueBytes,
			DWORD reservedValue,
			bool reservedForTrue,
			PropGrid::Boolean::TOnValueConfirmed onValueConfirmed,
			PropGrid::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	struct TIntegerEditor sealed:public TStringEditor{
		const PropGrid::TSize nValueBytes;
		const BYTE features;
		const PropGrid::Integer::TUpDownLimits limits;
		const PropGrid::Integer::TOnValueConfirmed onValueConfirmed;
	protected:
		void __drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const override;
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
		bool __tryToAcceptMainCtrlValue__() const override;
		LRESULT __mainCtrl_wndProc__(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) const override;
	public:
		TIntegerEditor(
			PropGrid::TSize nValueBytes,
			BYTE features,
			PropGrid::Integer::RCUpDownLimits rLimits,
			PropGrid::Integer::TOnValueConfirmed onValueConfirmed,
			PropGrid::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	struct TCustomEnumEditor sealed:public TEditor{
		const bool wideChar;
		const PropGrid::Enum::TGetValueDesc getValueDesc;
		const PropGrid::TDrawValueHandler drawValue;
		const PropGrid::Enum::TGetValueList getValueList;
		const PropGrid::Enum::TFreeValueList freeValueList;
		const PropGrid::Enum::TOnValueConfirmed onValueConfirmed;
	protected:
		LPCWSTR __getValueDescW__(PropGrid::PCustomParam param,PropGrid::Enum::UValue value,PWCHAR buf,short bufCapacity) const;
		void __drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const override;
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
		bool __tryToAcceptMainCtrlValue__() const override;
	public:
		TCustomEnumEditor(
			PropGrid::TSize nValueBytes,
			WORD height,
			bool wideChar,
			PropGrid::Enum::TGetValueDesc getValueDesc,
			PropGrid::TDrawValueHandler drawValue,
			PropGrid::Enum::TGetValueList getValueList,
			PropGrid::Enum::TFreeValueList freeValueList,
			PropGrid::Enum::TOnValueConfirmed onValueConfirmed,
			PropGrid::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	struct TFileNameEditor sealed:public TFixedPaddedStringEditor{
		TFileNameEditor(
			bool wideChar,
			PropGrid::TSize nCharsMax,
			PropGrid::String::TOnValueConfirmed onValueConfirmed,
			PropGrid::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	struct THyperlinkEditor sealed:public TEditor{
		const bool wideChar;
		const PropGrid::Hyperlink::TOnHyperlinkClicked onHyperlinkClicked;
	private:
		void __drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const override;
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
		bool __tryToAcceptMainCtrlValue__() const override;
		LRESULT __mainCtrl_wndProc__(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) const override;
	public:
		THyperlinkEditor(
			bool wideChar,
			PropGrid::Hyperlink::TOnHyperlinkClicked onHyperlinkClicked,
			PropGrid::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	typedef const struct TCustomEditor sealed:public TEditor{
		const PropGrid::TDrawValueHandler drawValue;
		const PropGrid::Custom::TCreateCustomMainEditor createCustomMainEditor;
		const PropGrid::Custom::TOnValueConfirmed onValueConfirmed;
	protected:
		void __drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const override;
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
		bool __tryToAcceptMainCtrlValue__() const override;
	public:
		TCustomEditor(
			WORD height,
			PropGrid::TSize nValueBytes,
			PropGrid::TDrawValueHandler drawValue,
			PropGrid::Custom::TCreateCustomMainEditor createCustomMainEditor,
			PropGrid::TOnEllipsisButtonClicked onEllipsisBtnClicked,
			PropGrid::Custom::TOnValueConfirmed onValueConfirmed,
			PropGrid::TOnValueChanged onValueChanged
		);
	} *PCCustomEditor;



	class CRegisteredEditors sealed{
		const struct TListItem sealed{
			const PCEditor pEditor;
			const BYTE editorSizeInBytes;
			const TListItem *const pNext;

			TListItem(PCEditor pEditor,BYTE editorSizeInBytes,const TListItem *pNext);
		} *list;
	public:
		PCEditor __add__(PCEditor definition,BYTE editorSizeInBytes);

		CRegisteredEditors();
		~CRegisteredEditors();
	};

	extern CRegisteredEditors RegisteredEditors;

#endif //EDITORS_H
