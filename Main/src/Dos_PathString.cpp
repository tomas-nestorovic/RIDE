#include "stdafx.h"

	const CDos::CPathString CDos::CPathString::Empty;
	const CDos::CPathString CDos::CPathString::Unnamed8=_T("Unnamed"); // must not exceed 8 characters!
	const CDos::CPathString CDos::CPathString::Root=_T('\\');


	CDos::CPathString::CPathString(LPCSTR str,int strLength){
		// ctor
		const PTCHAR buf=GetBufferSetLength(strLength);
		buf[strLength]='\0';
		#ifdef UNICODE
			while (strLength-->0) // sequence may be interrupted by '\0' chars (e.g. Spectrum DOSes)
				buf[strLength]=str[strLength];
		#else
			::memcpy( buf, str, strLength*sizeof(char) );
		#endif
	}

	CDos::CPathString::CPathString(LPCWSTR str,int strLength){
		// ctor
		const PTCHAR buf=GetBufferSetLength(strLength);
		buf[strLength]='\0';
		#ifdef UNICODE
			::memcpy( buf, str, nCharsInBuf*sizeof(WCHAR) );
		#else
			while (strLength-->0) // sequence may be interrupted by '\0' chars (e.g. Spectrum DOSes)
				buf[strLength]=str[strLength];
		#endif
	}







#ifdef UNICODE
	LPCSTR CDos::CPathString::GetAnsi() const{
		int i=GetLength();
		ansi=CString( ' ', i );
		const LPCTSTR buf=*this;
		const PCHAR a=const_cast<PTCHAR>(buf);
		while (--i>=0)
			a[i]=buf[i];
		return a;
	}
#endif

	void CDos::CPathString::MemcpyAnsiTo(PCHAR buf,BYTE bufCapacity,char padding) const{
		//
		::memcpy(
			::memset( buf, padding, bufCapacity ),
			GetAnsi(),
			std::min<int>( GetLength(), bufCapacity )
		);
	}

	bool CDos::CPathString::Equals(const CPathString &r,TFnCompareNames comparer) const{
		// True <=> the two strings are equal using the specified Comparer, otherwise False
		if (GetLength()!=r.GetLength())
			return false;
		const LPCTSTR buf=*this, rBuf=r;
		for( int i=0; i<GetLength(); i++ )
			if (comparer( buf+i, rBuf+i ))
				return false;
			else
				while (buf[++i]); // to not compare N characters long string (N-1)-times
		return true;
	}

	CString CDos::CPathString::EscapeToString() const{
		// returns a string with non-alphanumeric characters substituted with "URL-like" escape sequences
		CPathString result( CString(_T('\0'),16384) );
		PTCHAR pWritten=const_cast<PTCHAR>((LPCTSTR)result);
		for( int i=0; i<GetLength(); i++ ){
			const TCHAR c=operator[](i);
			if (::isalpha(c))
				*pWritten++=c;
			else
				pWritten+=::wsprintf( pWritten, _T("%%%02x"), c );
		}
		return result.TrimToLength( pWritten-(LPCTSTR)result );
	}

	CDos::CPathString &CDos::CPathString::TrimRight(TCHAR c){
		// trims continuous sequence of specified Character from the end of the string
		for( LPCTSTR p=(LPCTSTR)*this+GetLength(); p!=*this; )
			if (*--p!=c)
				return TrimToLength( p+1-(LPCTSTR)*this );
		__super::Empty();
		return *this;
	}

	CDos::CPathString &CDos::CPathString::TrimToLength(int nCharsMax){
		// trims the string to specified maximum number of characters
		__super::ReleaseBuffer(nCharsMax);
		return *this;
	}

	bool CDos::CPathString::IsValidFat32LongNameChar(WCHAR c){
		// True <=> specified Character is valid in FAT32 long file names, otherwise False
		static constexpr WCHAR ForbiddenChars[]=L"%#&<>|/";
		return (WORD)c>=32 && !::wcschr(ForbiddenChars,c);
	}

	CDos::CPathString &CDos::CPathString::ExcludeFat32LongNameInvalidChars(){
		// returns this string with FAT32 non-compliant characters excluded
		const LPCTSTR buf=*this;
		PTCHAR pCompliant=const_cast<PTCHAR>(buf);
		for( int i=0; i<GetLength(); i++ )
			if (IsValidFat32LongNameChar(buf[i]))
				*pCompliant++=buf[i];
		return TrimToLength( pCompliant-buf );
	}

	CDos::CPathString &CDos::CPathString::Unescape(){
		// unescapes inplace previously escaped term; it holds that unescaped String is never longer than escaped Term
		LPCTSTR term=*this;
		PTCHAR u=const_cast<PTCHAR>(term);
		for( int tmp; const TCHAR c=*term++; )
			if (c!='%') // not the escape character '%'
				*u++=c;
			else if (*term=='%') // the "%%" sequence to express the '%' character
				*u++=c, term++;
			else if (_stscanf(term,_T("%02x"),&tmp)) // a valid "%NN" escape sequence
				*u++=tmp, term+=2;
			else // an invalid "%NN" escape sequence
				break;
		return TrimToLength( u-(LPCTSTR)*this );
	}

	CDos::CPathString &CDos::CPathString::Format(LPCTSTR format,...){
		va_list argList;
		va_start( argList, format );
			FormatV( format, argList );
		va_end(argList);
		return *this;
	}

	CDos::CPathString &CDos::CPathString::FormatLostItem8(int itemId){
		Format( _T("LOST%04d"), itemId );
		return *this;
	}
