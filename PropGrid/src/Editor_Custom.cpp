#include "stdafx.h"

	static bool WINAPI __alwaysAccept__(PropGrid::PCustomParam,HWND,PropGrid::PValue){
		return true; // new Value is by default always accepted
	}

	TCustomEditor::TCustomEditor(	WORD height,
									PropGrid::TSize nValueBytes,
									PropGrid::TDrawValueHandler drawValue,
									PropGrid::Custom::TCreateCustomMainEditor createCustomMainEditor,
									PropGrid::TOnEllipsisButtonClicked onEllipsisBtnClicked,
									PropGrid::Custom::TOnValueConfirmed onValueConfirmed,
									PropGrid::TOnValueChanged onValueChanged
								)
		// ctor
		: TEditor( height, createCustomMainEditor!=nullptr, nValueBytes, onEllipsisBtnClicked, onValueChanged )
		, drawValue(drawValue)
		, createCustomMainEditor(createCustomMainEditor)
		, onValueConfirmed( onValueConfirmed ? onValueConfirmed : __alwaysAccept__ ) {
	}

	void TCustomEditor::__drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const{
		// draws the Value into the specified rectangle
		drawValue( value.param, value.buffer, value.editor->valueSize, pdis );
	}

	HWND TCustomEditor::__createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const{
		// creates, initializes with current Value, and returns Editor's MainControl
		// - creating the MainControl
		const HWND hMainCtrl=createCustomMainEditor( value.buffer, value.editor->valueSize, hParent );
		// - adjusting the MainControl's style
		::SetWindowLong(hMainCtrl, GWL_STYLE,
						::GetWindowLong(hMainCtrl,GWL_STYLE)
							& ~(WS_CAPTION | WS_BORDER | WS_OVERLAPPED)
							| EDITOR_STYLE
					);
		// - returning the MainControl
		return hMainCtrl;
	}

	bool TCustomEditor::__tryToAcceptMainCtrlValue__() const{
		// True <=> Editor's current Value is acceptable, otherwise False
		ignoreRequestToDestroy=true;
			const TPropGridInfo::TItem::TValue &value=TEditor::pSingleShown->value;
			const bool accepted=onValueConfirmed( value.param, TEditor::pSingleShown->hMainCtrl, value.buffer );
		ignoreRequestToDestroy=false;
		return accepted;
	}









	PropGrid::PCEditor PropGrid::Custom::DefineEditor(WORD height,TSize nValueBytes,TDrawValueHandler drawValue,TCreateCustomMainEditor createCustomMainEditor,TOnEllipsisButtonClicked onEllipsisBtnClicked,TOnValueConfirmed onValueConfirmed,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TCustomEditor(	height ? height : EDITOR_DEFAULT_HEIGHT,
										nValueBytes,
										drawValue,
										createCustomMainEditor, onEllipsisBtnClicked,
										onValueConfirmed,
										onValueChanged
									),
					sizeof(TCustomEditor)
				);
	}
