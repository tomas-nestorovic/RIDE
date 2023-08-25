#include "stdafx.h"

	const CDos::CPathString CDos::CPathString::Empty;
	const CDos::CPathString CDos::CPathString::Unnamed8=_T("Unnamed"); // must not exceed 8 characters!
	const CDos::CPathString CDos::CPathString::Root=_T('\\');
	const CDos::CPathString CDos::CPathString::DotDot=_T("..");


	CDos::CPathString::CPathString(LPCSTR str,int strLength){
		// ctor
		if (strLength<0)
			strLength=::lstrlenA(str);
		const PTCHAR buf=GetBufferSetLength(strLength);
		#ifdef UNICODE
			for( ; strLength>=0; strLength-- ) // sequence may be interrupted by '\0' chars (e.g. Spectrum DOSes)
				buf[strLength]=str[strLength];
		#else
			::memcpy( buf, str, (strLength+1)*sizeof(char) );
		#endif
	}

	CDos::CPathString::CPathString(LPCWSTR str,int strLength){
		// ctor
		if (strLength<0)
			strLength=::lstrlenW(str);
		const PTCHAR buf=GetBufferSetLength(strLength);
		#ifdef UNICODE
			::memcpy( buf, str, (strLength+1)*sizeof(WCHAR) );
		#else
			for( ; strLength>=0; strLength-- ) // sequence may be interrupted by '\0' chars (e.g. Spectrum DOSes)
				buf[strLength]=str[strLength];
		#endif
	}







#ifdef UNICODE
	static_assert( false, "Unicode support not implemented" );
#else
	CString CDos::CPathString::GetAnsi() const{
		int i=GetLength();
		CString result( ' ', i );
		const LPCTSTR buf=*this;
		const PCHAR a=const_cast<PCHAR>((LPCSTR)result);
		while (--i>=0)
			a[i]=buf[i];
		return result;
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
		for( int i=0; i<GetLength(); i++ ) // sequence may be interrupted by '\0' chars (e.g. Spectrum DOSes)
			if (comparer( buf+i, rBuf+i ))
				return false;
			else
				while (buf[++i]); // to not compare N characters long string (N-1)-times
		return true;
	}

	CDos::CPathString CDos::CPathString::EscapeToString() const{
		// returns a string with non-alphanumeric characters substituted with "URL-like" escape sequences
		CPathString result( CString(_T('\0'),16384) );
		PTCHAR pWritten=result;
		for( int i=0; i<GetLength(); i++ ){
			const TCHAR c=operator[](i);
			if (::isalpha(c))
				*pWritten++=c;
			else
				pWritten+=::wsprintf( pWritten, _T("%%%02x"), c );
		}
		return result.TrimToCharExcl(pWritten);
	}

	CDos::CPathString &CDos::CPathString::AppendDotExtension(LPCTSTR extWithoutDot){
		//
		operator+=('.');
		operator+=(extWithoutDot);
		return *this;
	}

	CDos::CPathString &CDos::CPathString::MakeUpper(){
		//
		__super::MakeUpper();
		return *this;
	}

	CDos::CPathString &CDos::CPathString::TrimRight(TCHAR c){
		// trims continuous sequence of specified Character from the end of the string
		for( LPCTSTR p=(LPCTSTR)*this+GetLength(); p!=*this; )
			if (*--p!=c)
				return TrimToCharExcl( p+1 );
		return TrimToLength(0);
	}

	CDos::CPathString &CDos::CPathString::TrimToLength(int nCharsMax){
		// trims the string to specified maximum number of characters
		__super::ReleaseBuffer( std::min(nCharsMax,GetLength()) );
		return *this;
	}

	CDos::CPathString &CDos::CPathString::TrimToCharExcl(LPCTSTR pc){
		// trims the string to specified position (exclusive this position!)
		return TrimToLength( pc-(LPCTSTR)*this );
	}

	bool CDos::CPathString::IsValidFat32LongNameChar(WCHAR c){
		// True <=> specified Character is valid in FAT32 long file names, otherwise False
		static constexpr WCHAR ForbiddenChars[]=L"%#&<>|/";
		return (WORD)c>=32 && !::wcschr(ForbiddenChars,c);
	}

	CDos::CPathString &CDos::CPathString::ExcludeFat32LongNameInvalidChars(){
		// returns this string with FAT32 non-compliant characters excluded
		const LPCTSTR buf=*this;
		PTCHAR pCompliant=*this;
		for( int i=0; i<GetLength(); i++ )
			if (IsValidFat32LongNameChar(buf[i]))
				*pCompliant++=buf[i];
		return TrimToLength( pCompliant-buf );
	}

	CDos::CPathString &CDos::CPathString::Unescape(){
		// unescapes inplace previously escaped term; it holds that unescaped String is never longer than escaped Term
		LPCTSTR term=*this;
		PTCHAR u=*this;
		for( int tmp; const TCHAR c=*term++; )
			if (c!='%') // not the escape character '%'
				*u++=c;
			else if (*term=='%') // the "%%" sequence to express the '%' character
				*u++=c, term++;
			else if (_stscanf(term,_T("%02x"),&tmp)) // a valid "%NN" escape sequence
				*u++=tmp, term+=2;
			else // an invalid "%NN" escape sequence
				break;
		return TrimToCharExcl(u);
	}

	CDos::CPathString &CDos::CPathString::Format(LPCTSTR format,...){
		va_list argList;
		va_start( argList, format );
			FormatV( format, argList );
		va_end(argList);
		return *this;
	}

	CDos::CPathString &CDos::CPathString::FormatLostItem8(int itemId){
		return Format( _T("LOST%04d"), itemId );
	}
