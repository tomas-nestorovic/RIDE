#include "stdafx.h"

namespace Cylinder
{
	TGeometry::TGeometry()
		// ctor (initialize non-formatted)
		: sides(1) // avoid client's division by zero, e.g. in 'CTRDOS503::GetLastOccupiedStdCylinder'
		, nSectors(1)
		, firstSectorNumber(1)
		, sectorLength(-1)
		, sectorLengthCode(0) {
	}

	TGeometry::TGeometry(THead nHeads,Sector::N nSectors,Sector::L sectorLength)
		// ctor
		: sides(nHeads)
		, nSectors(nSectors)
		, firstSectorNumber(1)
		, sectorLength(sectorLength)
		, sectorLengthCode( Sector::GetLengthCode(sectorLength) ) {
	}

}
