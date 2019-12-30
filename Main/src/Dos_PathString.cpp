#include "stdafx.h"

	CDos::CPathString::CPathString()
		// ctor
		: nCharsInBuf(0) {
		*buf='\0';
	}

	CDos::CPathString::CPathString(TCHAR c,short nRepeats)
		// ctor
		: nCharsInBuf(nRepeats) {
		ASSERT(nCharsInBuf<MAX_PATH);
		#ifdef UNICODE
			ASSERT(FALSE);
		#else
			::memset( buf, c, nCharsInBuf*sizeof(char) );
		#endif
		buf[nCharsInBuf]='\0';
	}

	CDos::CPathString::CPathString(LPCSTR str)
		// ctor
		: nCharsInBuf(::lstrlenA(str)) {
		ASSERT(nCharsInBuf<MAX_PATH);
		#ifdef UNICODE
			ASSERT(FALSE);
		#else
			::lstrcpyA( buf, str );
		#endif
	}

	CDos::CPathString::CPathString(LPCSTR str,short strLength)
		// ctor
		: nCharsInBuf(strLength) {
		ASSERT(nCharsInBuf<MAX_PATH);
		#ifdef UNICODE
			ASSERT(FALSE);
		#else
			::memcpy( buf, str, nCharsInBuf*sizeof(char) );
		#endif
		buf[nCharsInBuf]='\0';
	}

	CDos::CPathString::operator LPCTSTR() const{
		return buf;
	}

	TCHAR CDos::CPathString::operator[](int i) const{
		ASSERT(i<MAX_PATH);
		return buf[i];
	}

/*	TCHAR &CDos::CPathString::operator[](int i){
		ASSERT(i<MAX_PATH);
		return buf[i];
	}*/

	CDos::CPathString &CDos::CPathString::operator+=(TCHAR c){
		// returns this string appended with specified character
		if (nCharsInBuf+1<MAX_PATH){
			buf[nCharsInBuf++]=c;
			buf[nCharsInBuf]='\0';
		}else
			ASSERT(FALSE);
		return *this;
	}

	CDos::CPathString &CDos::CPathString::operator+=(const CPathString &r){
		// returns this string appended with specified string
		r.CopyNullTerminatedTo( buf+nCharsInBuf, MAX_PATH-nCharsInBuf );
		return *this;
	}

/*	bool CDos::CPathString::operator==(const CPathString &r) const{
		//
		if (nCharsInBuf!=r.nCharsInBuf)
			return false;
		for( short i=0; i<nCharsInBuf; i++ )
			if (buf[i]!=r.buf[i])
				return false;
		return true;
	}*/

	short CDos::CPathString::GetLength() const{
		return nCharsInBuf;
	}

	LPCTSTR CDos::CPathString::GetString() const{
		return buf;
	}

	bool CDos::CPathString::Equals(const CPathString &r,TFnCompareNames comparer) const{
		// True <=> the two strings are equal using the specified Comparer, otherwise False
		if (nCharsInBuf!=r.nCharsInBuf)
			return false;
		for( short i=0; i<nCharsInBuf; i++ )
			if (comparer( buf+i, r.buf+i ))
				return false;
			else
				while (buf[++i]); // to not compare N characters long string (N-1)-times
		return true;
	}

	short CDos::CPathString::EscapeNullTerminatedTo(PTCHAR buffer,short bufferCharCapacity) const{
		// returns the number of characters written to the Buffer when substituting non-alphanumeric characters with "URL-like" escape sequences
		PTCHAR pWritten=buffer;
		if (bufferCharCapacity>0){
			// at least terminating null-character fits in
			for( short i=0; i<nCharsInBuf; i++ ){
				#ifdef UNICODE
					WORD c=buf[i];
				#else
					BYTE c=buf[i];
				#endif
				if (::isalpha(c)){
					if (--bufferCharCapacity>0)
						*pWritten++=c;
					else
						break;
				}else
					if ((bufferCharCapacity-=3)>0)
						pWritten+=::wsprintf( pWritten, _T("%%%02x"), c );
					else
						break;
			}
			*pWritten='\0';
		}
		return pWritten-buffer;
	}

	PTCHAR CDos::CPathString::CopyNullTerminatedTo(PTCHAR buffer,short bufferCharCapacity) const{
		// copies the string to specified Buffer and returns pointer to the Buffer
		const short nCharsToCopy=std::min<short>(nCharsInBuf,bufferCharCapacity-1);
		if (nCharsToCopy>0)
			static_cast<PTCHAR>(::memcpy( buffer, buf, nCharsToCopy*sizeof(TCHAR) ))[nCharsToCopy]='\0';
		return buffer;
	}

	CDos::CPathString &CDos::CPathString::LowerCase(){
		// converts the string to lower-case letters
		for( short i=0; i<nCharsInBuf; i++ )
			::CharLowerBuff( buf+i, 1 );
		return *this;
	}

	CDos::CPathString &CDos::CPathString::TrimRight(TCHAR c){
		// trims continuous sequence of specified Character from the end of the string
		for( PTCHAR p=buf+nCharsInBuf; p--!=buf; nCharsInBuf-- )
			if (*p==c) *p='\0'; else break;
		return *this;
	}

	bool CDos::CPathString::IsValidFat32LongNameChar(WCHAR c){
		// True <=> specified Character is valid in FAT32 long file names, otherwise False
		static const WCHAR ForbiddenChars[]=L"%#&<>|/";
		return (WORD)c>=32 && !::wcschr(ForbiddenChars,c);
	}

	CDos::CPathString &CDos::CPathString::ExcludeFat32LongNameInvalidChars(){
		// returns this string with FAT32 non-compliant characters excluded
		PTCHAR pCompliant=buf;
		for( short i=0; i<nCharsInBuf; i++ )
			if (IsValidFat32LongNameChar(buf[i]))
				*pCompliant++=buf[i];
		*pCompliant='\0';
		nCharsInBuf=pCompliant-buf;
		return *this;
	}

	CDos::CPathString CDos::CPathString::Unescape(LPCTSTR term){
		// unescapes previously escaped Term and returns the resulting String; it holds that unescaped String is never longer than escaped Term
		TCHAR buf[MAX_PATH];
		return CPathString( buf, Unescape(buf,term) );
	}

	short CDos::CPathString::Unescape(PTCHAR buf,LPCTSTR term){
		// unescapes previously escaped Term and returns the number of characters written into the buffer; it holds that unescaped string is never longer than escaped Term
		PTCHAR u=buf;
		for( int tmp; const TCHAR c=*term++; )
			if (c!='%') // not the escape character '%'
				*u++=c;
			else if (*term=='%') // the "%%" sequence to express the '%' character
				*u++=c, term++;
			else if (_stscanf(term,_T("%02x"),&tmp)) // a valid "%NN" escape sequence
				*u++=tmp, term+=2;
			else // an invalid "%NN" escape sequence
				break;
		*u='\0'; // terminating whatever has been unescaped
		return u-buf;
	}
