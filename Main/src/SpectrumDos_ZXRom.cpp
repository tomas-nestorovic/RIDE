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
		for( LPCSTR m=zx; const BYTE z=*m; m++ )
			if (!(zxLength--))
				break;
			else if (z==96)
				*t++=163; // Pound sign, £
			else if (z<=126)
				*t++=z; // the same as ASCII up to character 126 (0x7e)
			else if (z==127)
				*t++=169; // copyright sign, ©
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
		*t='\0';
		return 1+bufT; // "1+" = see initialization above
	}

	PTCHAR CSpectrumDos::TZxRom::AsciiToZx(LPCTSTR pc,PCHAR zx,PBYTE pOutZxLength){
		// converts text from PC's current character set to Spectrum character set and returns the result in Buffer
		PCHAR buf=zx;
		for( bool prevCharIsSpace=true; const TCHAR c=*pc; pc++ )
			if (c==-93) // Pound sign, £
				*buf++=96, prevCharIsSpace=false;
			else if (c==-87) // copyright sign, ©
				*buf++=127, prevCharIsSpace=false;
			else{
				BYTE token=KEYWORD_TOKEN_FIRST;
				do{
					LPCSTR K=Keywords[token-KEYWORD_TOKEN_FIRST];
					K += *K==' '&&prevCharIsSpace; // skipping Keyword's initial space if the PreviousCharacter was a space
					BYTE N=::lstrlenA(K);
					N -= K[N-1]==' '&&pc[N-1]=='\0'; // skipping Keyword's trailing space should the match be found at the end of the PC text
					if (!::strncmp(pc,K,N)){
						pc+=N-1; // "-1" = see "pc++" in the For cycle
						prevCharIsSpace=K[N-1]==' ';
						break;
					}
				}while (++token);
				if (token) // a Keyword with given Token found in the input PC text
					*buf++=token;
				else // no Keyword match found in the input PC text
					*buf++=c, prevCharIsSpace=false;
			}
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
				: NULL;
	}

	void CSpectrumDos::TZxRom::PrintAt(HDC dc,LPCTSTR buf,RECT r,UINT drawTextFormat) const{
		// prints text in Buffer inside the given Rectangle
		BYTE n=::lstrlen(buf);
		if (drawTextFormat&DT_RIGHT){
			drawTextFormat&=~DT_RIGHT;
			r.left=r.right-n*font.charAvgWidth;
		}
		while (n--){
			WORD c=(BYTE)*buf++; // cannot use TCHAR because must use non-signed type
			if (!IsStdUdgSymbol(c))
				// directly printable character
				::DrawText( dc, (LPCTSTR)&c,1, &r, drawTextFormat );
			else{
				// UDG character - composed from four 0x2588 characters printed in half scale
				const int mapMode0=::SetMapMode(dc,MM_ANISOTROPIC);
					// . printing in half scale (MM_ISOTROPIC)
					::SetWindowExtEx(dc,200,200,NULL);
					::SetViewportExtEx(dc,100,100,NULL);
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
	}
