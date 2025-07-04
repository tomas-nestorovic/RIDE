#include "stdafx.h"

	static bool WINAPI __alwaysAccept__(PropGrid::PCustomParam,PropGrid::Enum::UValue){
		return true; // new Value is by default always accepted
	}

	static void WINAPI __dontDisposeValues__(PropGrid::PCustomParam,PropGrid::Enum::PCValueList){
		//nop (the allocated ValueList remains in memory (e.g. because it's been allocated in protected read-only space using as "static const")
	}

	TCustomEnumEditor::TCustomEnumEditor(	PropGrid::TSize nValueBytes,
											WORD height,
											bool wideChar,
											PropGrid::Enum::TGetValueDesc getValueDesc,
											PropGrid::TDrawValueHandler drawValue,
											PropGrid::Enum::TGetValueList getValueList,
											PropGrid::Enum::TFreeValueList freeValueList,
											PropGrid::Enum::TOnValueConfirmed onValueConfirmed,
											PropGrid::TOnValueChanged onValueChanged
										)
		// ctor
		: TEditor( height, true, std::min<PropGrid::TSize>(nValueBytes,sizeof(PropGrid::Enum::UValue)), nullptr, onValueChanged )
		, wideChar(wideChar)
		, getValueDesc(getValueDesc)
		, drawValue(drawValue)
		, getValueList(getValueList)
		, freeValueList( freeValueList ? freeValueList : __dontDisposeValues__ )
		, onValueConfirmed( onValueConfirmed ? onValueConfirmed : __alwaysAccept__ ) {
	}

	void TCustomEnumEditor::__drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const{
		// draws the Value into the specified rectangle
		if (drawValue!=nullptr)
			// custom-drawn (aka. owner-drawn) Value
			drawValue( value.param, value.buffer, value.editor->valueSize, pdis );
		else{
			// string-described Value
			PropGrid::Enum::UValue uValue;
				uValue.longValue=0;
			::memcpy( &uValue, value.buffer, valueSize );
			WCHAR desc[STRING_LENGTH_MAX+1];
			__drawString__(	__getValueDescW__( value.param, uValue, desc, ARRAYSIZE(desc) ), -1,
							pdis
						);
		}
	}

	LPCWSTR TCustomEnumEditor::__getValueDescW__(PropGrid::PCustomParam param,PropGrid::Enum::UValue value,PWCHAR buf,short bufCapacity) const{
		// returns the textual description of the given Value
		if (wideChar)
			return ((PropGrid::Enum::TGetValueDescW)getValueDesc)( param, value, buf, bufCapacity );
		else{
			char bufA[STRING_LENGTH_MAX+1];
			::MultiByteToWideChar(	CP_ACP, 0,
									((PropGrid::Enum::TGetValueDescA)getValueDesc)( param, value, bufA, sizeof(bufA) ), -1,
									buf, bufCapacity
								);
			return buf;
		}
	}

	HWND TCustomEnumEditor::__createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const{
		// creates, initializes with current Value, and returns Editor's MainControl
		// - creating the ComboBox
		const HWND hComboBox=::CreateWindow(WC_COMBOBOX,
											nullptr, // descendant sets the edit-box content
											CBS_DROPDOWNLIST | WS_VSCROLL | EDITOR_STYLE
												| ( drawValue!=nullptr ? CBS_OWNERDRAWFIXED : 0 ),
											0,0, 1,1,
											hParent, 0, GET_PROPGRID_HINSTANCE(hParent), nullptr
										);
		// . increasing the height of the ComboBox to accommodate also the drop-down list
		RECT r;
			::GetClientRect(hComboBox,&r);
		::SetWindowPos( hComboBox,0, 0,0, r.right,8*r.bottom, SWP_NOMOVE ); // "8*" = approximately 8 items
		// . populating the ComboBox with Values
		PropGrid::Enum::UValue uValueCurr; // actual Value extracted from the ValueBytes below
			uValueCurr.longValue=0;
			::memcpy( &uValueCurr.longValue, value.buffer, valueSize );
		WORD nValues;
		const PropGrid::Enum::PCValueList valueList=getValueList( value.param, uValueCurr, nValues );
			PropGrid::Enum::UValue uValue; // actual Value extracted from the ValueBytes below
				uValue.longValue=0;
			for( const BYTE *valueBytes=(PBYTE)valueList; nValues--; valueBytes+=valueSize ){ // let's treat the ValueList as an array of Bytes in the form of [Value1,Value2,...,ValueN} where each Value occupies the same number of Bytes (e.g. 2 Bytes)
				::memcpy( &uValue, valueBytes, valueSize );
				if (drawValue!=nullptr)
					// owner-drawn Value
					ComboBox_AddItemData( hComboBox, uValue.longValue );
				else{
					// string Value
					WCHAR descW[100];
					ComboBox_SetItemData(	hComboBox,
											::SendMessageW( hComboBox, CB_ADDSTRING, 0, (LPARAM)__getValueDescW__(value.param,uValue,descW,ARRAYSIZE(descW)) ),
											uValue.longValue
										);
				}
			}
		freeValueList( value.param , valueList );
		// . selecting the default Value in the ComboBox (or leaving it empty if it doesn't match any predefined Values from the List)
		::memcpy( &uValue, value.buffer, valueSize );
		int i=ComboBox_GetCount(hComboBox);
		while (i--)
			if (ComboBox_GetItemData(hComboBox,i)==uValue.longValue) break;
		ComboBox_SetCurSel(hComboBox,i);
		return hComboBox;
	}

	bool TCustomEnumEditor::__tryToAcceptMainCtrlValue__() const{
		// True <=> Editor's current Value is acceptable, otherwise False
		const HWND hComboBox=TEditor::pSingleShown->hMainCtrl;
		PropGrid::Enum::UValue uValue; // actual Value extracted from the ValueBytes below
			uValue.longValue=ComboBox_GetItemData( hComboBox, ComboBox_GetCurSel(hComboBox) );
		ignoreRequestToDestroy=true;
			const TPropGridInfo::TItem::TValue &value=TEditor::pSingleShown->value;
			const bool accepted=onValueConfirmed( value.param, uValue );
			if (accepted)
				::memcpy( value.buffer, &uValue, valueSize );
		ignoreRequestToDestroy=false;
		return accepted;
	}









	

	PropGrid::PCEditor PropGrid::Enum::DefineConstStringListEditorA(TSize nValueBytes,TGetValueList getValueList,TGetValueDescA getValueDesc,TFreeValueList freeValueList,TOnValueConfirmed onValueConfirmed,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TCustomEnumEditor(	nValueBytes,
											EDITOR_DEFAULT_HEIGHT,
											false, (PropGrid::Enum::TGetValueDesc)getValueDesc,
											nullptr,
											getValueList, freeValueList,
											onValueConfirmed,
											onValueChanged
										),
					sizeof(TCustomEnumEditor)
				);
	}

	PropGrid::PCEditor PropGrid::Enum::DefineConstStringListEditorW(TSize nValueBytes,TGetValueList getValueList,TGetValueDescW getValueDesc,TFreeValueList freeValueList,TOnValueConfirmed onValueConfirmed,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TCustomEnumEditor(	nValueBytes,
											EDITOR_DEFAULT_HEIGHT,
											true, (PropGrid::Enum::TGetValueDesc)getValueDesc,
											nullptr,
											getValueList, freeValueList,
											onValueConfirmed,
											onValueChanged
										),
					sizeof(TCustomEnumEditor)
				);
	}

	PropGrid::PCEditor PropGrid::Enum::DefineConstCustomListEditor(WORD height,TSize nValueBytes,TGetValueList getValueList,TDrawValueHandler drawValue,TFreeValueList freeValueList,TOnValueConfirmed onValueConfirmed,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TCustomEnumEditor(	nValueBytes,
											height>0 ? height : EDITOR_DEFAULT_HEIGHT,
											false, nullptr,
											drawValue,
											getValueList, freeValueList,
											onValueConfirmed,
											onValueChanged
										),
					sizeof(TCustomEnumEditor)
				);
	}
