#include "stdafx.h"
#include "MSDOS7.h"

	const CDos::CPathString CDos::CPathString::Empty;
	const CDos::CPathString CDos::CPathString::Unnamed8=_T("Unnamed"); // must not exceed 8 characters!
	const CDos::CPathString CDos::CPathString::Root=_T('\\');
	const CDos::CPathString CDos::CPathString::DotDot=_T("..");


	CDos::CPathString::CPathString(WCHAR c){
		// ctor
		Append(c);
	}

	CDos::CPathString::CPathString(LPCWSTR lpsz){
		// ctor
		Append( lpsz, ::lstrlenW(lpsz) );
	}

	CDos::CPathString::CPathString(LPCSTR ansi,int strLength){
		// ctor
		if (strLength<0)
			strLength=::lstrlenA(ansi);
		WCHAR unicode[32768];
		::MultiByteToWideChar( CP_ACP, 0, ansi,strLength, unicode,ARRAYSIZE(unicode) ); // sequence may be interrupted by '\0' chars (e.g. Spectrum DOSes)
		Append( unicode, strLength );
	}

	CDos::CPathString::CPathString(const CString &s){
		// ctor
		*this=CPathString( s, s.GetLength() );
	}

	CDos::CPathString::CPathString(const CPathString &r){
		// copy ctor
		const int n=r.GetLength();
		::memcpy( GetBufferSetLength(n), r, n*sizeof(TCHAR) );
	}








#ifdef UNICODE
	static_assert( false, "Unicode support not implemented" );
#else
	CString CDos::CPathString::GetAnsi() const{
		// returns ANSI version of the underlying string, including any non-printables characters inside (e.g. Null characters)
		CString result;
		const int nCharsW=GetLengthW();
		::WideCharToMultiByte( CP_ACP, 0, // sequence may be interrupted by '\0' chars (e.g. Spectrum DOSes)
			GetUnicode(), nCharsW,
			result.GetBufferSetLength(nCharsW), nCharsW,
			nullptr,nullptr
		);
		return result;
	}

	LPCWSTR CDos::CPathString::GetUnicode() const{
		// returns Unicode version of the underlying string, including any non-printables characters inside (e.g. Null characters)
		const int nCharsW=GetLengthW();
		const PWCHAR result=(PWCHAR)unicode.GetBufferSetLength( (nCharsW+1)*sizeof(WCHAR) );
		::MultiByteToWideChar( CP_UTF8, 0, *this,__super::GetLength(), result,nCharsW ); // sequence may be interrupted by '\0' chars (e.g. Spectrum DOSes)
		result[nCharsW]=L'\0';
		return result;
	}
#endif

	int CDos::CPathString::GetLengthW() const{
		// returns the length of the string in Unicode charset, including any non-printables characters inside (e.g. Null characters)
		return ::MultiByteToWideChar( CP_UTF8, 0, *this,__super::GetLength(), nullptr,0 ); // sequence may be interrupted by '\0' chars (e.g. Spectrum DOSes)
	}

	PTCHAR CDos::CPathString::GetBuffer() const{
		// returns pointer to internal representation of the string
		// --- MODIFY WITH CAUTION !! ---
		const PTCHAR result=const_cast<PTCHAR>( __super::operator LPCTSTR() );
		result[__super::GetLength()]='\0'; // explicitly terminate the string with null character
		return result;
	}

	char CDos::CPathString::FirstCharA() const{
		// returns first character of ANSI version of the string
		return *GetAnsi();
	}

	void CDos::CPathString::MemcpyAnsiTo(PCHAR buf,BYTE bufCapacity,char padding) const{
		// copies the ANSI version of the string to provided Buffer (incl. any non-printables characters inside, e.g. Null characters), filling the rest of its Capacity with given Padding character
		const auto ansi=GetAnsi();
		::memcpy( // sequence may be interrupted by '\0' chars (e.g. Spectrum DOSes)
			::memset( buf, padding, bufCapacity ),
			ansi,
			std::min<int>( ansi.GetLength(), bufCapacity )
		);
	}

	PTCHAR CDos::CPathString::FindLast(TCHAR c) const{
		// finds and returns pointer to the last occurence of specified Character; any character is a valid input, even non-printables
		for( int i=__super::GetLength(); --i>=0; ) // sequence may be interrupted by '\0' chars (e.g. Spectrum DOSes)
			if (operator[](i)==c)
				return GetBuffer()+i;
		return nullptr;
	}

	int CDos::CPathString::Compare(const CPathString &other,TFnCompareNames comparer) const{
		// returns an integer indicating if 'this' string preceedes, equals to, or succeeds the 'other' string, including any non-printables characters inside (e.g. Null characters)
		const int nCommonW=std::min( GetLengthW(), other.GetLengthW() );
		const LPCWSTR buf=GetUnicode(), rBuf=other.GetUnicode();
		for( int i=0; i<nCommonW; i++ ) // sequence may be interrupted by '\0' chars (e.g. Spectrum DOSes)
			if (const int res=comparer( buf+i, rBuf+i, nCommonW-i ))
				return res;
			else
				while (buf[++i]); // to not compare N characters long string (N-1)-times
		return GetLengthW()-other.GetLengthW();
	}

	int CDos::CPathString::Compare(const CPathString &other) const{
		// returns an integer indicating if 'this' string preceedes, equals to, or succeeds the 'other' string, including any non-printables characters inside (e.g. Null characters)
		return Compare( other, ::StrCmpNW );
	}

	int CDos::CPathString::CompareI(const CPathString &other) const{
		// returns an integer indicating if 'this' string preceedes, equals to, or succeeds the 'other' string, including any non-printables characters inside (e.g. Null characters)
		return Compare( other, ::StrCmpNIW );
	}

	CDos::CPathString CDos::CPathString::GetTail(TCHAR fromLast) const{
		// returns substring from last occurence of specified Character, excluding this character, but preserving any non-printables characters (e.g. Null characters)
		CPathString tail;
		if (LPCTSTR pLast=FindLast(fromLast)){
			const int n= __super::GetLength() - (++pLast-(LPCTSTR)*this); // "++" = pass the found last character
			::memcpy( tail.GetBufferSetLength(n), pLast, n*sizeof(TCHAR) ); // sequence may be interrupted by '\0' chars (e.g. Spectrum DOSes)
		}
		return tail;
	}

	CDos::CPathString CDos::CPathString::GetQuoted() const{
		// returns a copy of this string put into quotes, including any non-printable characters inside
		return CPathString(L'\"').Append(*this).Append(L'\"');
	}

	CDos::CPathString CDos::CPathString::DetachExtension(){
		// provided it begins with a period '.', separates and returns Extension in its own string (incl. any non-printables characters inside, e.g. Null characters)
		CPathString ext;
		if (const LPCTSTR pExt=FindLastDot()){
			ext=GetTail('.');
			TrimToCharExcl(pExt);
		}
		return ext;
	}

	CDos::CPathString &CDos::CPathString::Prepend(LPCTSTR lpsz){
		// prepends this string with specified term
		return *this=CPathString().Append(lpsz).Append(*this);
	}

	CDos::CPathString &CDos::CPathString::Append(const CPathString &r){
		// appends specified term to this string
		return Append( r.GetUnicode(), r.GetLengthW() );
	}

	CDos::CPathString &CDos::CPathString::Append(WCHAR c){
		// appends specified term to this string
		return Append( &c, 1 );
	}

	CDos::CPathString &CDos::CPathString::Append(LPCTSTR lpsz){
		// appends specified term to this string
		const int n=::lstrlen(lpsz);
		return Append( CPathString(lpsz,n).GetUnicode(), n );
	}

	CDos::CPathString &CDos::CPathString::Append(LPCWSTR str,int strLength){
		// appends specified term to this string
		const int nNewCharsUtf8=::WideCharToMultiByte( CP_UTF8, 0, str,strLength, nullptr,0, nullptr,nullptr );
		const int nCurrCharsUtf8=__super::GetLength();
		const PTCHAR p=GetBufferSetLength( nCurrCharsUtf8+nNewCharsUtf8 );
		::WideCharToMultiByte( CP_UTF8, 0, str,strLength, p+nCurrCharsUtf8,nNewCharsUtf8, nullptr,nullptr );
		return *this;
	}

	CDos::CPathString &CDos::CPathString::AppendBackslashItem(LPCWSTR itemWithoutBackslash){
		// appends backslash '\' and specified term to this string
		return Append(L'\\').Append( itemWithoutBackslash, ::lstrlenW(itemWithoutBackslash) );
	}

	CDos::CPathString &CDos::CPathString::AppendDotExtensionIfAny(LPCWSTR extWithoutDot){
		// appends period '.' and specified term to this string
		if (extWithoutDot && *extWithoutDot)
			return Append(L'.').Append( extWithoutDot, ::lstrlenW(extWithoutDot) );
		else
			return *this;
	}

	CDos::CPathString &CDos::CPathString::AppendDotExtensionIfAny(RCPathString extWithoutDot){
		// appends period '.' and specified term to this string
		if (const int nNewCharsUtf8=extWithoutDot.GetLength()){
			Append(L'.');
			const int nCurrCharsUtf8=__super::GetLength();
			const PTCHAR p=GetBufferSetLength( nCurrCharsUtf8+nNewCharsUtf8 );
			::memcpy( p+nCurrCharsUtf8, extWithoutDot, nNewCharsUtf8 );
		}
		return *this;
	}

	CDos::CPathString &CDos::CPathString::MakeUpper(){
		// converts this string to upper-case
		const int nCharsW=GetLengthW();
		const LPCWSTR u=GetUnicode();
		::CharUpperBuffW( (PWCHAR)u, nCharsW ); // sequence may be interrupted by '\0' chars (e.g. Spectrum DOSes)
		const int nCharsUtf8=::WideCharToMultiByte( CP_UTF8, 0, u,nCharsW, nullptr,0, nullptr,nullptr );
		::WideCharToMultiByte( CP_UTF8, 0, u,nCharsW, GetBufferSetLength(nCharsUtf8),nCharsUtf8, nullptr,nullptr );
		return *this;
	}

	CDos::CPathString &CDos::CPathString::TrimRightW(WCHAR c){
		// trims continuous sequence of specified Character from the end of the string
		const LPCWSTR unicode=GetUnicode();
		int n=GetLengthW();
		__super::Empty();
		while (n>0)
			if (unicode[--n]!=c)
				return Append( unicode, n+1 );
		return *this;
	}

	CDos::CPathString &CDos::CPathString::TrimToLengthW(int nCharsMaxW){
		// trims the string to specified maximum number of characters
		const LPCWSTR unicode=GetUnicode();
		nCharsMaxW=std::min( nCharsMaxW, GetLengthW() );
		__super::Empty();
		return Append( unicode, nCharsMaxW );
	}

	CDos::CPathString &CDos::CPathString::TrimToCharExcl(LPCTSTR pc){
		// trims the string to specified position (exclusive this position!)
		const int nCharsMax=std::min( pc-(LPCTSTR)*this, __super::GetLength() );
		ReleaseBuffer( nCharsMax );
		return *this;
	}

	bool CDos::CPathString::ContainsInvalidChars(TFnValidChar isCharValid) const{
		// True <=> at least one character is invalid, otherwise False
		const LPCWSTR p=GetUnicode();
		for( int i=GetLengthW(); i>0; )
			if (!isCharValid(p[--i]))
				return true;
		return false;
	}

	bool CDos::CPathString::ContainsFat32ShortNameInvalidChars() const{
		// True <=> at least one character is not compliant with FAT32 short names convension, otherwise False
		return ContainsInvalidChars( CMSDOS7::UDirectoryEntry::TShortNameEntry::IsCharacterValid );
	}

	bool CDos::CPathString::ContainsFat32LongNameInvalidChars() const{
		// True <=> at least one character is not compliant with FAT32 long names convension, otherwise False
		return ContainsInvalidChars( CMSDOS7::UDirectoryEntry::TLongNameEntry::IsCharacterValid );
	}

	CDos::CPathString &CDos::CPathString::ExcludeFat32LongNameInvalidChars(){
		// returns this string with FAT32 non-compliant characters excluded
		const LPCWSTR buf=GetUnicode();
		PWCHAR pCompliant=const_cast<PWCHAR>(buf);
		for( int i=0; i<GetLengthW(); i++ )
			if (CMSDOS7::UDirectoryEntry::TLongNameEntry::IsCharacterValid(buf[i]))
				*pCompliant++=buf[i];
		__super::Empty();
		return Append( buf, pCompliant-buf );
	}

	CDos::CPathString &CDos::CPathString::Escape(){
		// replaces inplace non-alphanumeric characters with "URL-like" escape sequences "%NN"
		TCHAR tmp[32768], *pWritten=tmp;
		for( int i=0; i<__super::GetLength(); i++ ){
			const TCHAR c=operator[](i);
			if (0<c && c<=127 && ::IsCharAlpha(c))
				*pWritten++=c;
			else
				pWritten+=::wsprintf( pWritten, _T("%%%02x"), (BYTE)c );
		}
		const int n=pWritten-tmp;
		if (const int nAddedCharsW= n - __super::GetLength()){ // added any escape sequences?
			::memcpy( GetBufferSetLength(n), tmp, (n+1)*sizeof(TCHAR) );
		}
		return *this;
	}

	CDos::CPathString &CDos::CPathString::Unescape(){
		// unescapes inplace previously escaped term; it holds that unescaped String is never longer than escaped Term
		LPCTSTR term=*this;
		PTCHAR u=GetBuffer();
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
			CString tmp;
			tmp.FormatV( format, argList );
		va_end(argList);
		return Append( (LPCTSTR)tmp );
	}

	CDos::CPathString &CDos::CPathString::FormatLostItem8(int itemId){
		return Format( _T("LOST%04d"), itemId );
	}

	HANDLE CDos::CPathString::CreateFile(DWORD dwDesiredAccess,DWORD dwShareMode,DWORD dwCreationDisposition,DWORD dwFlagsAndAttributes) const{
		return ::CreateFileW( GetUnicode(), dwDesiredAccess, dwShareMode, nullptr, dwCreationDisposition, dwFlagsAndAttributes, 0 );
	}
