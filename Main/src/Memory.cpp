#include "stdafx.h"

namespace Memory
{
	const CSharedPodArray<SYSTEMTIME,ULONGLONG> UniversalEmptySharedPodArray;



	CSharedBytes::N CSharedBytes::AppendRepeated(BYTE value,N count){
		::memset( ReserveAnother(count), value, count );
		return count;
	}

	CSharedBytes::N CSharedBytes::AppendFormatted(LPCSTR format,...){
		va_list argList;
		va_start( argList, format );
			char tmp[512];
			const N n=::wvsprintfA( tmp, format, argList );
		va_end(argList);
		return Append( tmp, n+1 ); // incl. terminal Null char
	}

	CSharedBytes::N CSharedBytes::Append(LPCVOID bytes,N nBytes){
		::memcpy( ReserveAnother(nBytes), bytes, nBytes );
		return nBytes;
	}
}
