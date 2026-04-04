#include "stdafx.h"
namespace YC=Yahel::Checksum;

namespace Checksum
{
	W GetCrcIbm3740(W seed,LPCVOID bytes,WORD nBytes){
		// computes and returns CRC-CCITT (0xFFFF) of data with a given Length in Buffer
		return YC::Compute(
			YC::TParams(YC::TParams::Ibm3740,seed), bytes, nBytes
		);
	}

	W GetCrcIbm3740(LPCVOID bytes,WORD nBytes){
		// computes and returns CRC-CCITT (0xFFFF) of data with a given Length in Buffer
		static const YC::TParams Params( YC::TParams::Ibm3740, 0xffff );
		return YC::Compute( Params, bytes, nBytes );
	}

}
