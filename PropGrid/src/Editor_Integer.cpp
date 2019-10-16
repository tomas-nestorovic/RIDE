#include "stdafx.h"

	static bool WINAPI __alwaysAccept__(CPropGridCtrl::PCustomParam,int){
		return true; // new Value is by default always accepted
	}

	TIntegerEditor::TIntegerEditor(	BYTE features,
									CPropGridCtrl::TInteger::RCUpDownLimits rLimits,
									CPropGridCtrl::TInteger::TOnValueConfirmed onValueConfirmed,
									CPropGridCtrl::TOnValueChanged onValueChanged
								)
		// ctor
		: TStringEditor( nullptr, false, nullptr, onValueChanged )
		, features(features)
		, limits(rLimits)
		, onValueConfirmed( onValueConfirmed ? onValueConfirmed : __alwaysAccept__ ) {
	}

	void TIntegerEditor::__drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const{
		// draws the Value into the specified rectangle
		int i=0;
		::memcpy( &i, value.buffer, std::min<CPropGridCtrl::TBufferCapacity>(value.bufferCapacity,sizeof(i)) );
		TCHAR buf[16];
		__drawString__( buf,
						::wsprintf( buf, features&CPropGridCtrl::TInteger::HEXADECIMAL?_T("0x%X"):_T("%d"), i ),
						pdis
					);
	}

	HWND TIntegerEditor::__createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const{
		// creates, initializes with current Value, and returns Editor's MainControl
		// - creating the edit-box
		const HWND hEdit=__createEditBox__( hParent, features&CPropGridCtrl::TInteger::ALIGN_RIGHT );
		// - attaching an UpDownControl to the edit-box
		int i=0;
		::memcpy( &i, value.buffer, std::min<CPropGridCtrl::TBufferCapacity>(value.bufferCapacity,sizeof(i)) );
		CPropGridCtrl::CreateUpDownControl(	hEdit,
											EDITOR_STYLE,
											features&CPropGridCtrl::TInteger::HEXADECIMAL,
											limits,
											i
										);
		return hEdit;
	}

	bool TIntegerEditor::__tryToAcceptMainCtrlValue__() const{
		// True <=> Editor's current Value is acceptable, otherwise False
		const HWND hEdit=TEditor::pSingleShown->hMainCtrl;
		TCHAR buf[16];
		::GetWindowText( hEdit, buf, sizeof(buf)/sizeof(TCHAR) );
		const int i=_tcstol( buf, nullptr, features&CPropGridCtrl::TInteger::HEXADECIMAL?16:10 );
		const bool outOfRange=	features&CPropGridCtrl::TInteger::HEXADECIMAL
								? (UINT)i<(UINT)limits.iMin || (UINT)i>(UINT)limits.iMax
								: i<limits.iMin || i>limits.iMax;
		if (outOfRange){
			TCHAR buf[80];
			::wsprintf( buf, _T("Number must be between %d and %d."), limits );
			::MessageBox( hEdit, buf, nullptr, MB_OK|MB_ICONEXCLAMATION );
			return false;
		}
		ignoreRequestToDestroy=true;
			const TPropGridInfo::TItem::TValue &value=TEditor::pSingleShown->value;
			const bool accepted=onValueConfirmed( value.param, i );
			if (accepted)
				::memcpy( value.buffer, &i, std::min<CPropGridCtrl::TBufferCapacity>(value.bufferCapacity,sizeof(i)) );
		ignoreRequestToDestroy=false;
		return accepted;
	}

	LRESULT TIntegerEditor::__mainCtrl_wndProc__(HWND hEdit,UINT msg,WPARAM wParam,LPARAM lParam) const{
		// window procedure
		switch (msg){
			case WM_PASTE:
				// pasting from the clipboard - preventing to paste non-number content or number that violates the Limits
				if (::OpenClipboard(0))
					if (const HGLOBAL h=::GetClipboardData(CF_TEXT)){
						// . getting the content from the clipboard and checking if it all characters are digits
						PTCHAR t;
						const int i=_tcstol( (LPCTSTR)::GlobalLock(h), &t, features&CPropGridCtrl::TInteger::HEXADECIMAL?16:10 );
						while (::isspace(*t)) t++;
						const TCHAR nondigit=*t;
						::GlobalUnlock(h);
						::CloseClipboard();
						// . preventing to paste non-number content
						if (nondigit)
							return 0;
						// . preventing to paste an out-of-range number
						if (features&CPropGridCtrl::TInteger::HEXADECIMAL){
							if ((UINT)i<(UINT)limits.iMin || (UINT)i>(UINT)limits.iMax)
								return 0;
						}else
							if (i<limits.iMin || i>limits.iMax)
								return 0;
					}
				break;
		}
		return __super::__mainCtrl_wndProc__(hEdit,msg,wParam,lParam);
	}









	const CPropGridCtrl::TInteger::TUpDownLimits CPropGridCtrl::TInteger::PositiveByteLimits={ 1, (BYTE)-1 };
	const CPropGridCtrl::TInteger::TUpDownLimits CPropGridCtrl::TInteger::PositiveWordLimits={ 1, (WORD)-1 };
	const CPropGridCtrl::TInteger::TUpDownLimits CPropGridCtrl::TInteger::PositiveIntegerLimits={ 1, INT_MAX };
	const CPropGridCtrl::TInteger::TUpDownLimits CPropGridCtrl::TInteger::NonNegativeIntegerLimits={ 0, INT_MAX };
	const CPropGridCtrl::TInteger::TUpDownLimits CPropGridCtrl::TInteger::NegativeIntegerLimits={ INT_MIN, -1 };

	CPropGridCtrl::PCEditor CPropGridCtrl::TInteger::DefineEditor(RCUpDownLimits rLimits,TOnValueConfirmed onValueConfirmed,BYTE features,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TIntegerEditor( features, rLimits, onValueConfirmed, onValueChanged ),
					sizeof(TIntegerEditor)
				);
	}

	CPropGridCtrl::PCEditor CPropGridCtrl::TInteger::DefineByteEditor(TOnValueConfirmed onValueConfirmed,BYTE features,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		static const TUpDownLimits limits={ 0, (BYTE)-1 };
		return DefineEditor( limits, onValueConfirmed, features, onValueChanged );
	}

	CPropGridCtrl::PCEditor CPropGridCtrl::TInteger::DefinePositiveByteEditor(TOnValueConfirmed onValueConfirmed,BYTE features,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return DefineEditor( PositiveByteLimits, onValueConfirmed, features, onValueChanged );
	}

	CPropGridCtrl::PCEditor CPropGridCtrl::TInteger::DefineWordEditor(TOnValueConfirmed onValueConfirmed,BYTE features,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		static const TUpDownLimits limits={ 0, (WORD)-1 };
		return DefineEditor( limits, onValueConfirmed, features, onValueChanged );
	}

	CPropGridCtrl::PCEditor CPropGridCtrl::TInteger::DefinePositiveWordEditor(TOnValueConfirmed onValueConfirmed,BYTE features,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return DefineEditor( PositiveWordLimits, onValueConfirmed, features, onValueChanged );
	}
