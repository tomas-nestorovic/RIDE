#include "stdafx.h"

namespace Side
{
	CMap::CMap(N nSides){
		// ctor (identity)
		static_assert( sizeof(N)<=sizeof(T), "" );
		for( N i=0; i<nSides; __super::Append(i++) );
	}

}
