#ifndef EDITORS_H
#define EDITORS_H

	#define	EDITOR_DEFAULT_HEIGHT	20
	#define EDITOR_STYLE			( WS_CHILD | WS_CLIPSIBLINGS )

	#define STRING_LENGTH_MAX		8192

	#pragma pack(1)
	struct TEditor{
		static const struct TControl sealed{
			const TPropGridInfo::TItem::TValue value;
			const HWND hMainCtrl; // Editor's MainControl handle (WC_EDIT,...)
			const HWND hEllipsisBtn; // optional EllipsisButton handle
			const bool mainControlExists; // True <=> MainControl is an editable control, otherwise False
			const WNDPROC wndProc0;		// Editor's original window procedure
			const WNDPROC ellipsisBtnWndProc0; // EllipsisButton's original window procedure
			//const CPropGridCtrl::PValue origValue; // property's original Value
			//const HWND hUpDown;

			TControl(	PCEditor editor,
						CPropGridCtrl::PValue valueBuffer,
						CPropGridCtrl::TValueSize valueBufferCapacity,
						CPropGridCtrl::PCustomParam param,
						DWORD style,
						HWND hParent
					);
			~TControl();
		} *pSingleShown;

		static void __cancelEditing__();

		const WORD height;
		const bool hasMainControl; // True <=> the Editor features an editable MainControl, otherwise False
		const CPropGridCtrl::TOnEllipsisButtonClicked onEllipsisBtnClicked;
		const CPropGridCtrl::TOnValueChanged onValueChanged;

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
			CPropGridCtrl::TOnEllipsisButtonClicked onEllipsisBtnClicked,
			CPropGridCtrl::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	struct TStringEditor:public TEditor{
		const bool wideChar;
		const CPropGridCtrl::TString::TOnValueConfirmed onValueConfirmed;
	protected:
		static HWND __createEditBox__(HWND hParent,UINT extraStyle);

		void __drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const override;
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
		bool __tryToAcceptMainCtrlValue__() const override;
		LRESULT __mainCtrl_wndProc__(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) const override;

		TStringEditor(
			CPropGridCtrl::TOnEllipsisButtonClicked onEllipsisBtnClicked,
			bool wideChar,
			CPropGridCtrl::TString::TOnValueConfirmed onValueConfirmed,
			CPropGridCtrl::TOnValueChanged onValueChanged
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
			CPropGridCtrl::TOnEllipsisButtonClicked onEllipsisBtnClicked,
			bool wideChar,
			CPropGridCtrl::TString::TOnValueConfirmed onValueConfirmed,
			WCHAR paddingChar,
			CPropGridCtrl::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	struct TDynamicStringEditor sealed:public TStringEditor{
	protected:
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
	public:
		TDynamicStringEditor(
			bool wideChar,
			CPropGridCtrl::TString::TOnValueConfirmed onValueConfirmed,
			CPropGridCtrl::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	struct TBooleanEditor sealed:public TEditor{
		const CPropGridCtrl::TBoolean::TOnValueConfirmed onValueConfirmed;
	protected:
		void __drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const override;
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
		bool __tryToAcceptMainCtrlValue__() const override;
		LRESULT __mainCtrl_wndProc__(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) const override;
	public:
		TBooleanEditor(
			CPropGridCtrl::TBoolean::TOnValueConfirmed onValueConfirmed,
			CPropGridCtrl::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	struct TIntegerEditor sealed:public TStringEditor{
		const BYTE features;
		const CPropGridCtrl::TInteger::TUpDownLimits limits;
		const CPropGridCtrl::TInteger::TOnValueConfirmed onValueConfirmed;
	protected:
		void __drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const override;
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
		bool __tryToAcceptMainCtrlValue__() const override;
		LRESULT __mainCtrl_wndProc__(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) const override;
	public:
		TIntegerEditor(
			BYTE features,
			CPropGridCtrl::TInteger::RCUpDownLimits rLimits,
			CPropGridCtrl::TInteger::TOnValueConfirmed onValueConfirmed,
			CPropGridCtrl::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	struct TCustomEnumEditor sealed:public TEditor{
		const bool wideChar;
		const CPropGridCtrl::TEnum::TGetValueDesc getValueDesc;
		const CPropGridCtrl::TDrawValueHandler drawValue;
		const CPropGridCtrl::TEnum::TGetValueList getValueList;
		const CPropGridCtrl::TEnum::TFreeValueList freeValueList;
		const CPropGridCtrl::TEnum::TOnValueConfirmed onValueConfirmed;
	protected:
		LPCWSTR __getValueDescW__(CPropGridCtrl::PCustomParam param,CPropGridCtrl::TEnum::UValue value,PWCHAR buf,short bufCapacity) const;
		void __drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const override;
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
		bool __tryToAcceptMainCtrlValue__() const override;
	public:
		TCustomEnumEditor(
			WORD height,
			bool wideChar,
			CPropGridCtrl::TEnum::TGetValueDesc getValueDesc,
			CPropGridCtrl::TDrawValueHandler drawValue,
			CPropGridCtrl::TEnum::TGetValueList getValueList,
			CPropGridCtrl::TEnum::TFreeValueList freeValueList,
			CPropGridCtrl::TEnum::TOnValueConfirmed onValueConfirmed,
			CPropGridCtrl::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	struct TFileNameEditor sealed:public TFixedPaddedStringEditor{
		TFileNameEditor(
			bool wideChar,
			CPropGridCtrl::TString::TOnValueConfirmed onValueConfirmed,
			CPropGridCtrl::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	struct THyperlinkEditor sealed:public TEditor{
		const bool wideChar;
		const CPropGridCtrl::THyperlink::TOnHyperlinkClicked onHyperlinkClicked;
	private:
		void __drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const override;
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
		bool __tryToAcceptMainCtrlValue__() const override;
		LRESULT __mainCtrl_wndProc__(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) const override;
	public:
		THyperlinkEditor(
			bool wideChar,
			CPropGridCtrl::THyperlink::TOnHyperlinkClicked onHyperlinkClicked,
			CPropGridCtrl::TOnValueChanged onValueChanged
		);
	};

	#pragma pack(1)
	typedef const struct TCustomEditor sealed:public TEditor{
		const CPropGridCtrl::TDrawValueHandler drawValue;
		const CPropGridCtrl::TCustom::TCreateCustomMainEditor createCustomMainEditor;
		const CPropGridCtrl::TCustom::TOnValueConfirmed onValueConfirmed;
	protected:
		void __drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const override;
		HWND __createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const override;
		bool __tryToAcceptMainCtrlValue__() const override;
	public:
		TCustomEditor(
			WORD height,
			CPropGridCtrl::TDrawValueHandler drawValue,
			CPropGridCtrl::TCustom::TCreateCustomMainEditor createCustomMainEditor,
			CPropGridCtrl::TOnEllipsisButtonClicked onEllipsisBtnClicked,
			CPropGridCtrl::TCustom::TOnValueConfirmed onValueConfirmed,
			CPropGridCtrl::TOnValueChanged onValueChanged
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
