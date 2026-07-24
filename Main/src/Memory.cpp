#include "stdafx.h"

namespace Memory
{
	const CSharedPodArray<SYSTEMTIME,ULONGLONG> UniversalEmptySharedPodArray;



	CSharedBytesGrowing::CSharedBytesGrowing(N nBytesDefault)
		: CSharedBytes(nBytesDefault) { // capacity that can grow
		length=0; // actual # of Bytes
	}

	PBYTE CSharedBytesGrowing::ReserveAnother(N nBytes){
		nBytes+=length; // now min capacity required
		const N lengthOrg=length;
		if (GetLength()*sizeof(TCHAR)<nBytes) // would overflow ?
			return Realloc(nBytes)+lengthOrg;
		else{
			length=nBytes;
			return begin()+lengthOrg;
		}
	}

	CSharedBytes::N CSharedBytesGrowing::AppendRepeated(BYTE value,N count){
		::memset( ReserveAnother(count), value, count );
		return count;
	}

	CSharedBytes::N CSharedBytesGrowing::AppendFormatted(LPCSTR format,...){
		va_list argList;
		va_start( argList, format );
			char tmp[512];
			const N n=::wvsprintfA( tmp, format, argList );
		va_end(argList);
		return Append( tmp, n+1 ); // incl. terminal Null char
	}

	CSharedBytes::N CSharedBytesGrowing::Append(LPCVOID bytes,N nBytes){
		::memcpy( ReserveAnother(nBytes), bytes, nBytes );
		return nBytes;
	}
}
