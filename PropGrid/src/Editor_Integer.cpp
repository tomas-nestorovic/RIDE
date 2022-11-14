#include "stdafx.h"

	static bool WINAPI __alwaysAccept__(PropGrid::PCustomParam,int){
		return true; // new Value is by default always accepted
	}

	TIntegerEditor::TIntegerEditor(	PropGrid::TSize nValueBytes,
									BYTE features,
									PropGrid::Integer::RCUpDownLimits rLimits,
									PropGrid::Integer::TOnValueConfirmed onValueConfirmed,
									PropGrid::TOnValueChanged onValueChanged
								)
		// ctor
		: TStringEditor( nullptr, false, STRING_LENGTH_MAX, nullptr, onValueChanged )
		, nValueBytes( std::min<PropGrid::TSize>(nValueBytes,sizeof(int)) )
		, features(features)
		, limits(rLimits)
		, onValueConfirmed( onValueConfirmed ? onValueConfirmed : __alwaysAccept__ ) {
	}

	void TIntegerEditor::__drawValue__(const TPropGridInfo::TItem::TValue &value,PDRAWITEMSTRUCT pdis) const{
		// draws the Value into the specified rectangle
		int i=0;
		::memcpy( &i, value.buffer, nValueBytes );
		TCHAR buf[16];
		__drawString__( buf,
						::wsprintf( buf, features&PropGrid::Integer::HEXADECIMAL?_T("0x%X"):_T("%d"), i ),
						pdis
					);
	}

	HWND TIntegerEditor::__createMainControl__(const TPropGridInfo::TItem::TValue &value,HWND hParent) const{
		// creates, initializes with current Value, and returns Editor's MainControl
		// - creating the edit-box
		const HWND hEdit=__createEditBox__( hParent, features&PropGrid::Integer::ALIGN_RIGHT );
		// - attaching an UpDownControl to the edit-box
		int i=0;
		::memcpy( &i, value.buffer, nValueBytes );
		PropGrid::CreateUpDownControl(	hEdit,
											EDITOR_STYLE,
											features&PropGrid::Integer::HEXADECIMAL,
											limits,
											i
										);
		return hEdit;
	}

	bool TIntegerEditor::__tryToAcceptMainCtrlValue__() const{
		// True <=> Editor's current Value is acceptable, otherwise False
		const HWND hEdit=TEditor::pSingleShown->hMainCtrl;
		TCHAR buf[16];
		const int nChars=::GetWindowText( hEdit, buf, ARRAYSIZE(buf) );
		static_assert( PropGrid::Integer::HEXADECIMAL==1, "PropGrid::Integer::HEXADECIMAL==1" );
		const int i=_tcstol( buf, nullptr, 10+6*(features&PropGrid::Integer::HEXADECIMAL) );
		const bool outOfRange=	features&PropGrid::Integer::HEXADECIMAL
								? nChars>sizeof(int)*2 || !limits.ContainsUnsigned(i)
								: nChars>11 || !limits.Contains(i); // "11" as in "-1234567890"
		if (outOfRange){
			TCHAR buf[80], strHexaMin[16], strHexaMax[16];
			::wsprintf( buf, _T("0x%%0%dX"), ::lstrlen(_itot(limits.iMin|limits.iMax,strHexaMax,16)) );
			::wsprintf( strHexaMin, buf, limits.iMin );
			::wsprintf( strHexaMax, buf, limits.iMax );
			::wsprintf( buf, _T("Number must be between %d (%s) and %d (%s)."), limits.iMin, strHexaMin, limits.iMax, strHexaMax );
			::MessageBox( hEdit, buf, nullptr, MB_OK|MB_ICONEXCLAMATION );
			return false;
		}
		ignoreRequestToDestroy=true;
			const TPropGridInfo::TItem::TValue &value=TEditor::pSingleShown->value;
			const bool accepted=onValueConfirmed( value.param, i );
			if (accepted)
				::memcpy( value.buffer, &i, nValueBytes );
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
						const int i=_tcstol( (LPCTSTR)::GlobalLock(h), &t, features&PropGrid::Integer::HEXADECIMAL?16:10 );
						while (::isspace(*t)) t++;
						const TCHAR nondigit=*t;
						::GlobalUnlock(h);
						::CloseClipboard();
						// . preventing from pasting non-number content
						if (nondigit)
							return 0;
						// . preventing from pasting an out-of-range number
						if (features&PropGrid::Integer::HEXADECIMAL){
							if (!limits.ContainsUnsigned(i))
								return 0;
						}else
							if (!limits.Contains(i))
								return 0;
					}
				break;
		}
		return __super::__mainCtrl_wndProc__(hEdit,msg,wParam,lParam);
	}









	const PropGrid::Integer::TUpDownLimits PropGrid::Integer::TUpDownLimits::Byte={ 0, (BYTE)-1 };
	const PropGrid::Integer::TUpDownLimits PropGrid::Integer::TUpDownLimits::Word={ 0, (WORD)-1 };
	const PropGrid::Integer::TUpDownLimits PropGrid::Integer::TUpDownLimits::PositiveByte={ 1, (BYTE)-1 };
	const PropGrid::Integer::TUpDownLimits PropGrid::Integer::TUpDownLimits::PositiveWord={ 1, (WORD)-1 };
	const PropGrid::Integer::TUpDownLimits PropGrid::Integer::TUpDownLimits::PositiveInteger={ 1, INT_MAX };
	const PropGrid::Integer::TUpDownLimits PropGrid::Integer::TUpDownLimits::NonNegativeInteger={ 0, INT_MAX };
	const PropGrid::Integer::TUpDownLimits PropGrid::Integer::TUpDownLimits::NegativeInteger={ INT_MIN, -1 };
	const PropGrid::Integer::TUpDownLimits PropGrid::Integer::TUpDownLimits::Percent={ 0, 100 };
	const PropGrid::Integer::TUpDownLimits PropGrid::Integer::TUpDownLimits::PositivePercent={ 1, 100 };

	bool PropGrid::Integer::TUpDownLimits::Contains(int value) const{
		// True <=> this range contains the Value (inclusive), otherwise False
		return	iMin<=value && value<=iMax;
	}

	bool PropGrid::Integer::TUpDownLimits::ContainsUnsigned(UINT value) const{
		// True <=> this range contains the Value (inclusive), otherwise False
		return	(UINT)iMin<=value && value<=(UINT)iMax;
	}

	PropGrid::PCEditor PropGrid::Integer::DefineEditor(TSize nValueBytes,RCUpDownLimits rLimits,TOnValueConfirmed onValueConfirmed,BYTE features,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return	RegisteredEditors.__add__(
					new TIntegerEditor( nValueBytes, features, rLimits, onValueConfirmed, onValueChanged ),
					sizeof(TIntegerEditor)
				);
	}

	PropGrid::PCEditor PropGrid::Integer::DefineByteEditor(TOnValueConfirmed onValueConfirmed,BYTE features,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return DefineEditor( sizeof(BYTE), TUpDownLimits::Byte, onValueConfirmed, features, onValueChanged );
	}

	PropGrid::PCEditor PropGrid::Integer::DefinePositiveByteEditor(TOnValueConfirmed onValueConfirmed,BYTE features,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return DefineEditor( sizeof(BYTE), TUpDownLimits::PositiveByte, onValueConfirmed, features, onValueChanged );
	}

	PropGrid::PCEditor PropGrid::Integer::DefineWordEditor(TOnValueConfirmed onValueConfirmed,BYTE features,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return DefineEditor( sizeof(WORD), TUpDownLimits::Word, onValueConfirmed, features, onValueChanged );
	}

	PropGrid::PCEditor PropGrid::Integer::DefinePositiveWordEditor(TOnValueConfirmed onValueConfirmed,BYTE features,TOnValueChanged onValueChanged){
		// creates and returns an Editor with specified parameters
		return DefineEditor( sizeof(WORD), TUpDownLimits::PositiveWord, onValueConfirmed, features, onValueChanged );
	}
