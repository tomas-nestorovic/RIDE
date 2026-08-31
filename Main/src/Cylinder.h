#pragma once

namespace Cylinder
{
	struct TGeometry{
		Side::CMap sides;
		Sector::N nSectors; //TODO: ZBR with variable # of Sectors per Cylinder
		Sector::N firstSectorNumber;
		Sector::L sectorLength;
		Sector::LC sectorLengthCode;

		TGeometry(); // initialize non-formatted
		TGeometry(THead nHeads,Sector::N nSectors,Sector::L sectorLength);
	};

}

typedef Cylinder::N TCylinder,*PCylinder;
typedef const TCylinder *PCCylinder;
typedef short &RCylinder;
static_assert( sizeof(TCylinder)==sizeof(RCylinder), "" );
typedef Cylinder::TGeometry TGeometry;
