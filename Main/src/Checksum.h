#pragma once

namespace Checksum
{
	typedef WORD W;

	W GetCrcIbm3740(W seed,LPCVOID bytes,WORD nBytes);
	W GetCrcIbm3740(LPCVOID bytes,WORD nBytes);

}
