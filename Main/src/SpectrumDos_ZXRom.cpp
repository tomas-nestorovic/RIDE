#include "stdafx.h"

	CSpectrumDos::TZxRom::TZxRom()
		// ctor
		// - creating the Font using which all text will be printed on screen
		: font(FONT_COURIER_NEW,100,false,true) {
		//: font(_T("DejaVu Sans Mono Bold"),90,false,true,65) {
		//: font(_T("DejaVu Sans Mono"),86,false,true,60) {
	}








	double CSpectrumDos::TZxRom::TNumberInternalForm::ToDouble() const{
		// returns the PC floating-point version of the ZX Spectrum number (even if the NumberInternalForm represents an integer)
		// - attempting to convert the NumberInternalForm to a PC integer
		if (!(*bytes|bytes[4])) // yes, seems like an integer representation, as Bytes[0]==Bytes[4]==0
			switch (bytes[1]){
				case 0: // a non-negative integer from {0,...,65535}
					return *(PCWORD)&bytes[2];
				case 255: // a negative integer from {-65535,...,-1}
					return *(PCWORD)&bytes[2]-(int)65536;
			}
		// - converting the NumberInternalForm to a PC floating point number
		/*
			ZX Spectrum's NumberInternalForm (8 bit exponent, sign bit, 23 bit mantissa)
			 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
			+---------------%-+-------------%---------------%---------------%---------------+
			| Two's exponent|S|          normalized positive mantissa (big endian)          |
			+---------------%-+-------------%---------------%---------------%---------------+

			PC's IEEE-754, 64-bit (sign bit, 11 bit exponent, 52 bit mantissa)
			 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 
			+-+-------------%-------+-------%---------------%---------------%---------------%---------------%---------------%---------------+
			|S| Exponent with bias  |                                  positive mantissa (little endian)                                    |
			+-+-------------%-------+-------%---------------%---------------%---------------%---------------%---------------%---------------+
		*/
		if (!mantissa) // ZX Spectrum's "zero" ...
			return 0.; // ... directly converts to PC's "zero"
		DWORD tmpMantissa=mantissa|0x80000000; // ZX Mantissa's most significant bit is always 1
		if ((int)mantissa<0) // if the ZX floating point number is negative ...
			tmpMantissa=-tmpMantissa; // ... negating its Mantissa (two's complement)
		BYTE tmpExponent=exponent;
		while (tmpMantissa<0x80000000) // normalizing the ZX Mantissa (it might have been negated above, so the most significant bit is no longer a 1)
			tmpMantissa<<=1, tmpExponent++;
		union{
			WORD words[4];
			unsigned long long uint64;
			double ieee754;
		} tmp;
		tmpMantissa<<=1; // dismissing the most significant bit which (thanks to the normalization above) is always 1
		tmp.uint64=(long long)tmpMantissa<<(52-sizeof(tmpMantissa)*8);
		tmp.words[3]|=(tmpExponent+895-1)<<4; // 895 = difference between ZX exponent bias (128) and IEEE-754 (64-bit) exponent bias (1023); TODO: need to explain "-1"
		if ((int)mantissa<0)
			tmp.words[3]|=0x8000;
		return tmp.ieee754;
	}








	const LPCSTR CSpectrumDos::TZxRom::Keywords[]={
		_T("RND"), _T("INKEY$"), _T("PI"), _T("FN "), _T("POINT "), _T("SCREEN$ "), _T("ATTR "), _T("AT "), _T("TAB "), _T("VAL$ "), _T("CODE "), _T("VAL "), _T("LEN "), _T("SIN "), _T("COS "), _T("TAN "), _T("ASN "), _T("ACS "), _T("ATN "), _T("LN "), _T("EXP "), _T("INT "), _T("SQR "), _T("SGN "), _T("ABS "), _T("PEEK "), _T("IN "), _T("USR "), _T("STR$ "), _T("CHR$ "), _T("NOT "), _T("BIN "),
		_T(" OR "), _T(" AND "), _T("<="), _T(">="), _T("<>"), _T(" LINE "), _T(" THEN "), _T(" TO "), _T(" STEP "), _T(" DEF FN "), _T(" CAT "), _T(" FORMAT "), _T(" MOVE "), _T(" ERASE "), _T(" OPEN #"), _T(" CLOSE #"), _T(" MERGE "), _T(" VERIFY "), _T(" BEEP "), _T(" CIRCLE "), _T(" INK "), _T(" PAPER "), _T(" FLASH "), _T(" BRIGHT "), _T(" INVERSE "), _T(" OVER "), _T(" OUT "), _T(" LPRINT "), _T(" LLIST "), _T(" STOP "), _T(" READ "), _T(" DATA "), _T(" RESTORE "),
		_T(" NEW "), _T(" BORDER "), _T(" CONTINUE "), _T(" DIM "), _T(" REM "), _T(" FOR "), _T(" GO TO "), _T(" GO SUB "), _T(" INPUT "), _T(" LOAD "), _T(" LIST "), _T(" LET "), _T(" PAUSE "), _T(" NEXT "), _T(" POKE "), _T(" PRINT "), _T(" PLOT "), _T(" RUN "), _T(" SAVE "), _T(" RANDOMIZE "), _T(" IF "), _T(" CLS "), _T(" DRAW "), _T(" CLEAR "), _T(" RETURN "), _T(" COPY ")
	};

	// KEYWORD_TOKEN_FIRST corresponds to the "RND" Keyword in ZX Spectrum charset
	#define KEYWORD_TOKEN_FIRST	165

	PTCHAR CSpectrumDos::TZxRom::ZxToAscii(LPCSTR zx,BYTE zxLength,PTCHAR bufT){
		// converts text from Spectrum character set to PC's current character set and returns the result in Buffer
		bufT[0]=' '; // initialization
		PTCHAR t=1+bufT;
		while (zxLength--){
			const BYTE z=*zx++;
			if (!z)
				*t++=255; // null character
			else if (z==96)
				*t++=(char)163; // Pound sign, �
			else if (z<=126)
				*t++=z; // the same as ASCII up to character 126 (0x7e)
			else if (z==127)
				*t++=(char)169; // copyright sign, �
			else if (z<=143)
				*t++=z; // UDG graphics
			else if (z<=164)
				*t++=z-79; // UDG characters 'A'-'U'
			else{
				const LPCSTR K=Keywords[z-KEYWORD_TOKEN_FIRST];
				if (*(t-1)==' ' && *K==' ')
					::lstrcpy( t, 1+K ); // two consecutive spaces are not printed
				else
					::lstrcpy( t, K );
				t+=::lstrlen(t);
			}
		}
		*t='\0';
		return 1+bufT; // "1+" = see initialization above
	}

	PTCHAR CSpectrumDos::TZxRom::AsciiToZx(LPCTSTR pc,PCHAR zx,PBYTE pOutZxLength){
		// converts text from PC's current character set to Spectrum character set and returns the result in Buffer
		PCHAR buf=zx;
		for( ; const TCHAR c=*pc; pc++ )
			if (c==-93) // Pound sign, �
				*buf++=96;
			else if (c==-87) // copyright sign, �
				*buf++=127;
			else
				*buf++=c;
		*buf='\0';
		if (pOutZxLength)
			*pOutZxLength=buf-zx;
		return zx;
	}

	bool CSpectrumDos::TZxRom::IsStdUdgSymbol(BYTE s){
		// True <=> given Character is a standard UDG symbol, otherwise False
		return 127<s && s<144;
	}

	LPCSTR CSpectrumDos::TZxRom::GetKeywordTranscript(BYTE k){
		// returns the textual representation of the given Keyword, or Null if the character is not a Keyword character
		return	k>=KEYWORD_TOKEN_FIRST
				? Keywords[k-KEYWORD_TOKEN_FIRST]
				: nullptr;
	}

	WORD CSpectrumDos::TZxRom::PrintAt(HDC dc,LPCSTR zx,BYTE zxLength,RECT r,UINT drawTextFormat) const{
		// returns the number of ASCII characters to which the input ZX code has been converted and printed inside the given Rectangle
		TCHAR buf[3000]; // a big-enough buffer to accommodate 255-times the keyword RANDOMIZE
		const PCHAR pAscii=ZxToAscii( zx, zxLength, buf );
		WORD nAsciiChars=::lstrlen(pAscii);
		if (drawTextFormat&DT_RIGHT){
			drawTextFormat&=~DT_RIGHT;
			r.left=r.right-nAsciiChars*font.charAvgWidth;
		}
		for( WORD i=pAscii-buf,const iEnd=i+nAsciiChars; i<iEnd; i++ ){
			BYTE c=buf[i]; // cannot use TCHAR because must use non-signed type
			if (c<' ' || c==255){
				// non-printable character (255 = replacement for null character 0x00)
				TCHAR tmp[4];
				::wsprintf( tmp, _T("%02X"), c<' '?c:0 );
				RECT rHexa=r;
					rHexa.right=r.left+2*font.charAvgWidth;
				::FillRect( dc, &rHexa, CBrush(Utils::GetBlendedColor( ::GetBkColor(dc), 0xc8c8c8, .5f )) );
				rHexa.right--;
				::DrawText( dc, tmp, -1, &r, drawTextFormat );
				const CPen delimiterPen( PS_DOT, 1, COLOR_WHITE );
				const HGDIOBJ hPen0=::SelectObject( dc, delimiterPen );
					::MoveToEx( dc, rHexa.right, r.top, nullptr );
					::LineTo( dc, rHexa.right, r.bottom );
				::SelectObject(dc,hPen0);
				nAsciiChars++; // one extra character is used to represent both half-Bytes
				r.left+=font.charAvgWidth; // adjustment for the second printed hexa-character made below
			}else if (!IsStdUdgSymbol(c))
				// directly printable character
				::DrawTextA( dc, (LPCSTR)&c,1, &r, drawTextFormat );
			else{
				// UDG character - composed from four 0x2588 characters printed in half scale
				const int mapMode0=::SetMapMode(dc,MM_ANISOTROPIC);
					// . printing in half scale (MM_ISOTROPIC)
					::SetWindowExtEx(dc,200,200,nullptr);
					::SetViewportExtEx(dc,100,100,nullptr);
					// . printing UDG
					static const WCHAR w=0x2588;
					c-=128; // lower four bits encode which "quadrants" of UDG character are shown (e.g. the UDG character of "L" shape = 2+4+8 = 14)
					const int L=r.left*2, T=r.top*2, L2=L+font.charAvgWidth, T2=T+font.charHeight-2;
					if (c&1) // upper right quadrant
						::TextOutW(dc,L2,T,&w,1);
					if (c&2) // upper left quadrant
						::TextOutW(dc,L,T,&w,1);
					if (c&4) // lower right quadrant
						::TextOutW(dc,L2,T2,&w,1);
					if (c&8) // lower left quadrant
						::TextOutW(dc,L,T2,&w,1);
				::SetMapMode(dc,mapMode0);
			}
			r.left+=font.charAvgWidth;
		}
		return nAsciiChars;
	}










	HWND WINAPI CSpectrumBase::TZxRom::CLineComposerPropGridEditor::__create__(PropGrid::PValue value,PropGrid::TSize combinedValue,HWND hParent){
		// - initializing the Editor
		CLineComposerPropGridEditor &rEditor=((CSpectrumBaseFileManagerView *)CDos::GetFocused()->pFileManager)->zxRom.lineComposerPropGridEditor;
		rEditor.length = rEditor.lengthMax = LOBYTE(combinedValue);
		ASSERT(rEditor.length<sizeof(rEditor.buf));
		for( rEditor.paddingChar=HIBYTE(combinedValue); rEditor.length; )
			if (((PCHAR)value)[--rEditor.length]!=rEditor.paddingChar){
				::memcpy( rEditor.buf, value, ++rEditor.length );
				break;
			}
		// - initializing the Cursor
		rEditor.cursor.mode=TCursor::LC; // "L" Cursor if CapsLock off, "C" Cursor if CapsLock on
		rEditor.cursor.position=rEditor.length;
		// - returning the initialized Editor
		const HWND hEditor=::CreateWindow(	AfxRegisterWndClass(0,app.LoadStandardCursor(IDC_IBEAM),Utils::CRideBrush::White),
											nullptr, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0,0, 1,1, hParent, 0, AfxGetInstanceHandle(), nullptr
										);
		(WNDPROC)::SetWindowLong(hEditor,GWL_WNDPROC,(LONG)__wndProc__);
		return rEditor.handle = hEditor;
	}

	void WINAPI CSpectrumBase::TZxRom::CLineComposerPropGridEditor::__drawValue__(PropGrid::PCustomParam,PropGrid::PCValue value,PropGrid::TSize combinedValue,PDRAWITEMSTRUCT pdis){
		const TZxRom &rZxRom=((CSpectrumBaseFileManagerView *)CDos::GetFocused()->pFileManager)->zxRom;
		const HGDIOBJ hFont0=::SelectObject( pdis->hDC, rZxRom.font.m_hObject );
			pdis->rcItem.left+=PROPGRID_CELL_MARGIN_LEFT;
			rZxRom.PrintAt(	pdis->hDC, (LPCSTR)value, LOBYTE(combinedValue),
							pdis->rcItem,
							DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX
						);
		::SelectObject(pdis->hDC,hFont0);
	}

	bool WINAPI CSpectrumBase::TZxRom::CLineComposerPropGridEditor::__onChanged__(PropGrid::PCustomParam,HWND,PropGrid::PValue value){
		const PDos dos=CDos::GetFocused();
		const CLineComposerPropGridEditor &rEditor=((CSpectrumBaseFileManagerView *)dos->pFileManager)->zxRom.lineComposerPropGridEditor;
		::memcpy(	::memset( value, rEditor.paddingChar, rEditor.lengthMax ),
					rEditor.buf,
					rEditor.length
				);
		dos->image->UpdateAllViews(nullptr);
		return true; // new text always accepted
	}

	#define IS_CAPSLOCK_ON()	((::GetKeyState(VK_CAPITAL)&1)>0)

	LRESULT CALLBACK CSpectrumBase::TZxRom::CLineComposerPropGridEditor::__wndProc__(HWND hEditor,UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		const TZxRom &rZxRom=((CSpectrumBaseFileManagerView *)CDos::GetFocused()->pFileManager)->zxRom;
		CLineComposerPropGridEditor &rEditor=rZxRom.lineComposerPropGridEditor;
		switch (msg){
			case WM_PAINT:{
				// drawing
				PAINTSTRUCT ps;
				const HDC dc=::BeginPaint(hEditor,&ps);
					::SetBkMode(dc,TRANSPARENT);
					const HGDIOBJ hFont0=::SelectObject( dc, rZxRom.font.m_hObject );
						RECT r;
						::GetClientRect(hEditor,&r);
						// . printing content BEFORE Cursor
						const WORD nAsciiChars=rZxRom.PrintAt(
							dc, rEditor.buf, rEditor.cursor.position,
							r,
							DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX
						);
						// . printing content AFTER Cursor
						r.left+=(nAsciiChars+1)*rZxRom.font.charAvgWidth; // "+1" = Cursor
						rZxRom.PrintAt(	dc, rEditor.buf+rEditor.cursor.position, rEditor.length-rEditor.cursor.position,
										r,
										DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX
									);
						// . printing Cursor
						r.right=r.left;
						r.left-=rZxRom.font.charAvgWidth;
						const COLORREF color0=::SetTextColor( dc, COLOR_WHITE );
							//r.bottom=( r.top=(r.bottom-rZxRom.font.charHeight)/2 )+rZxRom.font.charHeight;
							::FillRect( dc, &r, Utils::CRideBrush::Black );
							if (rEditor.cursor.mode==TCursor::LC && IS_CAPSLOCK_ON()){
								// displaying the "C" Mode (Capitals) at place of the "L" mode
								static const char ModeC='C';
								::DrawTextA( dc, &ModeC,1, &r, DT_SINGLELINE|DT_LEFT|DT_VCENTER );
							}else
								// displaying current Mode
								::DrawTextA( dc, (LPCSTR)&rEditor.cursor.mode,1, &r, DT_SINGLELINE|DT_LEFT|DT_VCENTER );
						::SetTextColor(dc,color0);
					::SelectObject(dc,hFont0);
				::EndPaint(hEditor,&ps);
				return 0;
			}
			case WM_KEYDOWN:{
				// key pressed
				switch (wParam){
					case VK_TAB:
					case VK_ESCAPE:
					case VK_RETURN:
						// control keys of all Editors
						break;
					case VK_LEFT:
						// moving Cursor one Position to the left
						if (rEditor.cursor.position){
							rEditor.cursor.position--;
cursorMoved:				rEditor.hexaLow=true; // specifically for hexa-mode X
							::InvalidateRect(hEditor,nullptr,TRUE);
						}
						return 0;
					case VK_RIGHT:
						// moving Cursor one Position to the right
						if (rEditor.cursor.position<rEditor.length){
							rEditor.cursor.position++;
							goto cursorMoved;
						}
						return 0;
					case VK_HOME:
					case VK_UP:
						// moving Cursor to the beginning of File Name
						rEditor.cursor.position=0;
						goto cursorMoved;
					case VK_END:
					case VK_DOWN:
						// moving Cursor to the end of File Name
						rEditor.cursor.position=rEditor.length;
						goto cursorMoved;
					case VK_BACK:
						// deleting the character that preceeds the Cursor (Backspace)
						if (rEditor.cursor.position){
							const BYTE c=--rEditor.cursor.position;
							const PCHAR p=rEditor.buf;
							::memmove( p+c, p+1+c, rEditor.lengthMax-c );
							rEditor.length--;
							goto cursorMoved;
						}
						return 0;
					case VK_DELETE:
						// deleting the character that follows the Cursor (Delete)
						if (rEditor.cursor.position<rEditor.length){
							const BYTE c=rEditor.cursor.position;
							const PCHAR p=rEditor.buf;
							::memmove( p+c, p+1+c, ( --rEditor.length )-c );
							goto cursorMoved;
						}
						return 0;
					case VK_CONTROL:
					case VK_SHIFT:
						// changing Cursor Mode after pressing Ctrl+Shift
						if (::GetKeyState(VK_CONTROL)<0 && ::GetKeyState(VK_SHIFT)<0){
							static const TCursor::TMode Modes[]={ TCursor::K, TCursor::LC, TCursor::E, TCursor::G, TCursor::X };
							TCursor::TMode &rMode=rEditor.cursor.mode;
							BYTE m=0;
							while (Modes[m++]!=rMode);
							if (m==sizeof(Modes)/sizeof(TCursor::TMode))
								m=0;
							rMode=Modes[m];
							rEditor.hexaLow=true; // specifically for hexa-mode X
						}
						//fallthrough
					case VK_CAPITAL:
						// turning CapsLock on and Off
						::InvalidateRect(hEditor,nullptr,TRUE); // to update the Cursor (switching between the "L" and "C" Modes)
						break;
					default:
						// adding a character to Buffer
						static const BYTE ConversionAbcModeKL[]={ 226,'*','?',205,200,204,203,'^',172,'-','+','=','.',',',';','"',199,'<',195,'>',197,'/',201,96,198,':' };
						static const BYTE Conversion012ModeKL[]={ '_','!','@','#','$','%','&','\'','(',')' };
						if (wParam==' '){
							rEditor.cursor.mode=TCursor::LC; // switching to Mode "L" if Space is pressed (or alternatively C, if CapsLock on)
							goto addCharInWParam;
						}
						switch (rEditor.cursor.mode){
							case TCursor::K:
								// Cursor in Mode K
								if (::GetKeyState(VK_CONTROL)<0){
									if (wParam>='A' && wParam<='Z')
										rEditor.__addChar__( ConversionAbcModeKL[wParam-'A'] );
									else if (wParam>='0' && wParam<='9')
										rEditor.__addChar__( Conversion012ModeKL[wParam-'0'] );
								}else
									if (wParam>='A' && wParam<='Z')
										rEditor.__addChar__(wParam-'A'+230); // conversion to capital letter
									else if (wParam>='0' && wParam<='9')
addCharInWParam:						rEditor.__addChar__(wParam);
								return 0;
							case TCursor::LC:
								// Cursor in Modes L (or alternatively C, if CapsLock on)
								if (::GetKeyState(VK_CONTROL)<0){
									if (wParam>='A' && wParam<='Z')
										rEditor.__addChar__( ConversionAbcModeKL[wParam-'A'] );
									else if (wParam>='0' && wParam<='9')
										rEditor.__addChar__( Conversion012ModeKL[wParam-'0'] );
								}else
									if (wParam>='A' && wParam<='Z'){
										if (::GetKeyState(VK_SHIFT)>=0 && !IS_CAPSLOCK_ON()) // if Shift not pressed and CapsLock not on...
											wParam|=32; // ... converting to lowercase letter
										goto addCharInWParam;
									}else if (wParam>='0' && wParam<='9')
										goto addCharInWParam;
								return 0;
							case TCursor::E:
								// Cursor in Mode E
								if (::GetKeyState(VK_CONTROL)<0){
									if (wParam>='A' && wParam<='Z'){
										static const BYTE Conversion[]={ '~',220,218,'\\',183,'{','}',216,191,174,170,171,221,222,223,127,181,214,'|',213,']',219,182,217,'[',215 };
										rEditor.__addChar__( Conversion[wParam-'A'] );
									}else if (wParam>='0' && wParam<='9'){
										static const BYTE Conversion[]={ 208,206,168,202,211,212,209,210,169,207 };
										rEditor.__addChar__( Conversion[wParam-'0'] );
									}
								}else
									if (wParam>='A' && wParam<='Z'){
										static const BYTE Conversion[]={ 227,196,224,228,180,188,189,187,175,176,177,192,167,166,190,173,178,186,229,165,194,225,179,185,193,184 };
										rEditor.__addChar__( Conversion[wParam-'A'] );
									}
								return 0;
							case TCursor::G:
								// Cursor in Mode G
								if (wParam>='A' && wParam<='Z')
									rEditor.__addChar__( 144+wParam-'A' );
								else if (wParam>='1' && wParam<='8') // 0 and 9 ignored
									if (::GetKeyState(VK_SHIFT)<0){
										static const BYTE Conversion[]={ 142,141,140,139,138,137,136,143 };
										rEditor.__addChar__( Conversion[wParam-'1'] );
									}else{
										static const BYTE Conversion[]={ 129,130,131,132,133,134,135,128 };
										rEditor.__addChar__( Conversion[wParam-'1'] );
									}
								return 0;
							case TCursor::X:{
								// Cursor in hexa-mode
								if ('A'<=wParam && wParam<='F')
									wParam-='A'-10;
								else if ('0'<=wParam && wParam<='9')
									wParam-='0';
								else
									return 0;
								if (rEditor.hexaLow){ // beginning writing a new Byte
									if (!rEditor.__addChar__('\0')) // if new Byte can't be added ...
										return 0; // ... we are done
								}else // finishing writing current Byte
									::InvalidateRect(hEditor,nullptr,TRUE);
								char &r=rEditor.buf[rEditor.cursor.position-1];
								r<<=4;
								r|=wParam;
								rEditor.hexaLow=!rEditor.hexaLow;
								return 0;
							}
						}
						return 0;
				}
				break;
			}
		}
		return ::CallWindowProc( ::DefWindowProc, hEditor, msg, wParam, lParam );
	}

	static bool WINAPI __help__(PVOID,PVOID,short){
		// help
		Utils::Information(_T("You can type in all Spectrum characters, including commands (if in modes K, or E), letters (mode L), capitals (mode C), and UDG symbols (mode G). In each mode, type characters as you would on a classical 48k Spectrum keyboard. Non-printable characters are not supported and cannot be typed in (e.g. those influencing text color).\n\nSwitch between modes using Ctrl+Shift. Use Ctrl alone as the Symbol Shift key. You enter the C mode if CapsLock is on during L mode.\n\nExample:\nSwitch to mode E and press Z - \"LN\" shows up.\nSwitch to mode E again and press Ctrl+Z - \"BEEP\" appears this time."));
		return false; // False = actual editing of value has failed (otherwise the Editor would be closed)
	}

	bool CSpectrumBase::TZxRom::CLineComposerPropGridEditor::__addChar__(char c){
		// True <=> given Character has been added at Cursor's current Position, otherwise False
		if (length==lengthMax) return false; // can't exceed the maximum length
		::memmove( buf+cursor.position+1, buf+cursor.position, length-cursor.position );
		buf[cursor.position++]=c;
		length++;
		//cursor.mod=TCursor::L;
		::InvalidateRect( handle, nullptr, TRUE );
		return true;
	}

	PropGrid::PCEditor CSpectrumBase::TZxRom::CLineComposerPropGridEditor::Define(BYTE nCharsMax,char paddingChar,PropGrid::Custom::TOnValueConfirmed onValueConfirmed,PropGrid::TOnValueChanged onValueChanged){
		// creates and returns the ZX Spectrum line Editor
		return PropGrid::Custom::DefineEditor( 0, MAKEWORD(nCharsMax,paddingChar), __drawValue__, __create__, __help__, onValueConfirmed?onValueConfirmed:__onChanged__, onValueChanged );
	}

	LPCSTR CSpectrumBase::TZxRom::CLineComposerPropGridEditor::GetCurrentZxText() const{
		// returns Byte representation of current state of the edited line
		return buf;
	}

	BYTE CSpectrumBase::TZxRom::CLineComposerPropGridEditor::GetCurrentZxTextLength() const{
		// returns the length of Byte representation of current state of the edited line
		return length;
	}
