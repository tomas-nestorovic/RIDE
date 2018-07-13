#include "stdafx.h"

	static bool WINAPI __alwaysAccept__(CPropGridCtrl::PCustomParam,CPropGridCtrl::TEnum::UValue){
		return true; // new Value is by default always accepted
	}

	static void WINAPI __dontDisposeValues__(CPropGridCtrl::PCustomParam,CPropGridCtrl::TEnum::PCValueList){
		//nop (the allocated ValueList remains in memory (e.g. because it's been allocated in protected read-only space using as "static const")
	}

	TCustomEnumEditor::TCustomEnumEditor(	WORD height,
											bool wideChar,
											CPropGridCtrl::TEnum::TGetValueDesc getValueDesc,
											CPropGridCtrl::TDrawValueHandler drawValue,
											CPropGridCtrl::TEnum::TGetValueList getValueList,
											CPropGridCtrl::TEnum::TFreeValueList freeValueList,
											CPropGridCtrl::TEnum::TOnValueConfirmed onValueConfirmed
										)
		// ctor
		: TEditor( height, true, NULL )
		, wideChar(wideChar)
		, getValueDesc(getValueDesc)
		, drawValue(drawValue)
		, getValueList(getValueList)
		, freeValueList( freeValueList ? freeValueList : __dontDisposeValues__ )
		, onValueConfirmed( onValueConfirmed ? onValueConfirmed : __alwaysAccept__ ) {
	}

	void TCustomEnumEditor::__drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const{
		// draws the Value into the specified rectangle
		if (drawValue!=NULL)
			// custom-drawn (aka. owner-drawn) Value
			drawValue( value.param, value.buffer, value.bufferCapacity, pdis );
		else{
			// string-described Value
			CPropGridCtrl::TEnum::UValue uValue;
				uValue.longValue=0;
			::memcpy( &uValue, value.buffer, value.bufferCapacity );
			WCHAR desc[STRING_LENGTH_MAX];
			__drawString__(	__getValueDescW__( value.param, uValue, desc, sizeof(desc)/sizeof(WCHAR) ), -1,
							pdis
						);
		}
	}

	LPCWSTR TCustomEnumEditor::__getValueDescW__(CPropGridCtrl::PCustomParam param,CPropGridCtrl::TEnum::UValue value,PWCHAR buf,short bufCapacity) const{
		// returns the textual description of the given Value
		if (wideChar)
			return ((CPropGridCtrl::TEnum::TGetValueDescW)getValueDesc)( param, value, buf, bufCapacity );
		else{
			char bufA[STRING_LENGTH_MAX];
			::MultiByteToWideChar(	CP_ACP, 0,
									((CPropGridCtrl::TEnum::TGetValueDescA)getValueDesc)( param, value, bufA, sizeof(bufA) ), -1,
									buf, bufCapacity
								);
			return buf;
		}
	}

	HWND TCustomEnumEditor::__createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const{
		// creates, initializes with current Value, and returns Editor's MainControl
		// - checking the BufferCapacity
		if (value.bufferCapacity>sizeof(CPropGridCtrl::TEnum::UValue))
			return 0; // error - unsupported BufferCapacity (must be from the set {1,...,4})
		// - creating the ComboBox
		const HWND hComboBox=::CreateWindow(WC_COMBOBOX,
											NULL, // descendant sets the edit-box content
											CBS_DROPDOWNLIST | WS_VSCROLL | EDITOR_STYLE
												| ( drawValue!=NULL ? CBS_OWNERDRAWFIXED : 0 ),
											0,0, 1,1,
											hParent, 0, GET_PROPGRID_HINSTANCE(hParent), NULL
										);
		// . increasing the height of the ComboBox to accommodate also the drop-down list
		RECT r;
			::GetClientRect(hComboBox,&r);
		::SetWindowPos( hComboBox,0, 0,0, r.right,8*r.bottom, SWP_NOMOVE ); // "8*" = approximately 8 items
		// . populating the ComboBox with Values
		WORD nValues;
		const CPropGridCtrl::TEnum::PCValueList valueList=getValueList( value.param, nValues );
			CPropGridCtrl::TEnum::UValue uValue; // actual Value extracted from the ValueBytes below
				uValue.longValue=0;
			for( const BYTE *valueBytes=(PBYTE)valueList; nValues--; valueBytes+=value.bufferCapacity ){ // let's treat the ValueList as an array of Bytes in the form of [Value1,Value2,...,ValueN} where each Value occupies the same number of Bytes (e.g. 2 Bytes)
				::memcpy( &uValue, valueBytes, value.bufferCapacity );
				if (drawValue!=NULL)
					// owner-drawn Value
					ComboBox_AddItemData( hComboBox, uValue.longValue );
				else{
					// string Value
					WCHAR descW[100];
					ComboBox_SetItemData(	hComboBox,
											::SendMessageW( hComboBox, CB_ADDSTRING, 0, (LPARAM)__getValueDescW__(value.param,uValue,descW,sizeof(descW)/sizeof(WCHAR)) ),
											uValue.longValue
										);
				}
			}
		freeValueList( value.param , valueList );
		// . selecting the default Value in the ComboBox (or leaving it empty if it doesn't match any predefined Values from the List)
		::memcpy( &uValue, value.buffer, value.bufferCapacity );
		int i=ComboBox_GetCount(hComboBox);
		while (i--)
			if (ComboBox_GetItemData(hComboBox,i)==uValue.longValue) break;
		ComboBox_SetCurSel(hComboBox,i);
		return hComboBox;
	}

	bool TCustomEnumEditor::__tryToAcceptMainCtrlValue__() const{
		// True <=> Editor's current Value is acceptable, otherwise False
		const HWND hComboBox=TEditor::pSingleShown->hMainCtrl;
		CPropGridCtrl::TEnum::UValue uValue; // actual Value extracted from the ValueBytes below
			uValue.longValue=ComboBox_GetItemData( hComboBox, ComboBox_GetCurSel(hComboBox) );
		ignoreRequestToDestroy=true;
			const TPropGridInfo::TItem::TValue &value=TEditor::pSingleShown->value;
			const bool accepted=onValueConfirmed( value.param, uValue );
			if (accepted)
				::memcpy( value.buffer, &uValue, value.bufferCapacity );
		ignoreRequestToDestroy=false;
		return accepted;
	}









	

	CPropGridCtrl::PCEditor CPropGridCtrl::TEnum::DefineConstStringListEditorA(TGetValueList getValueList,TGetValueDescA getValueDesc,TFreeValueList freeValueList,TOnValueConfirmed onValueConfirmed){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TCustomEnumEditor(	EDITOR_DEFAULT_HEIGHT,
											false, (CPropGridCtrl::TEnum::TGetValueDesc)getValueDesc,
											NULL,
											getValueList, freeValueList,
											onValueConfirmed
										),
					sizeof(TCustomEnumEditor)
				);
	}

	CPropGridCtrl::PCEditor CPropGridCtrl::TEnum::DefineConstCustomListEditor(WORD height,TGetValueList getValueList,TDrawValueHandler drawValue,TFreeValueList freeValueList,TOnValueConfirmed onValueConfirmed){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TCustomEnumEditor(	height>0 ? height : EDITOR_DEFAULT_HEIGHT,
											false, NULL,
											drawValue,
											getValueList, freeValueList,
											onValueConfirmed
										),
					sizeof(TCustomEnumEditor)
				);
	}
