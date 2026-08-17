#include "stdafx.h"

namespace Side
{
	CMap::CMap(N nSides){
		// ctor (identity)
		static_assert( sizeof(N)<=sizeof(T), "" );
		for( N i=0; i<CapacityMin; __super::Append(i++) );
		length=nSides;
	}

	CMap::CMap(N nSides,const T *sides){
		// ctor (cloning)
		::memcpy(
			*this = CMap(nSides), // identity
			sides,
			nSides*sizeof(T)
		);
	}

}
