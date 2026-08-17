#pragma once

namespace Side
{
	typedef BYTE T; // value
	typedef BYTE N; // index or count

	class CMap:public Memory::CSharedPodArray<T,N,16>{
	public:
		enum:N{
			CapacityMin=64
		};

		CMap(N nSides); // identity
		CMap(N nSides,const T *sides); // cloning
	};

}

typedef Side::T TSide;
typedef Side::N THead;
typedef const TSide *PCSide;
typedef const THead *PCHead;
