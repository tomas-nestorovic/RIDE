#pragma once

namespace Side
{
	typedef BYTE T; // value
	typedef BYTE N; // index or count

	enum:N{
		CountMax=64
	};

	class CMap:public Memory::CSharedPodArray<T,N,16>{
	public:
		CMap(N nSides); // identity

		inline CMap(N nSides,const T *sides)
			// ctor (explicit Sides)
			: Memory::CSharedPodArray<T,N,16>( nSides, sides ) {
		}
	};

}

typedef Side::T TSide;
typedef Side::N THead;
typedef const TSide *PCSide;
typedef const THead *PCHead;
