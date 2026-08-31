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

	class CGeometryReaderWriter abstract:public Sector::CReaderWriter,protected TGeometry{
	protected:
		CGeometryReaderWriter(PImage image,Yahel::TPosition dataTotalLength,const Yahel::TInterval<char> &padding,const TRev &nDiscoveredRevolutions,FOnWritten onWritten,const TGeometry &g);
	public:
		const Sector::L usableSectorLength;

		// Yahel::Stream::IAdvisor methods
		Yahel::TRow LogicalPositionToRow(Yahel::TPosition logPos,WORD nBytesInRow) override;
		Yahel::TPosition RowToLogicalPosition(Yahel::TRow row,WORD nBytesInRow) override;
		void GetRecordInfo(Yahel::TPosition logPos,Yahel::PPosition pOutRecordStartLogPos,Yahel::PPosition pOutRecordLength,bool *pOutDataReady) override;

		// other
		Yahel::TPosition GetSectorStartPosition(const TPhysicalAddress &chs,Sector::N nSectorsToSkip) const override;
		void GetPhysicalAddress(Yahel::TPosition pos,TPhysicalAddress &outChs,Sector::N &outSectorIndex,Sector::PL pOutOffset) const override;
	};

}

typedef Cylinder::N TCylinder,*PCylinder;
typedef const TCylinder *PCCylinder;
typedef short &RCylinder;
static_assert( sizeof(TCylinder)==sizeof(RCylinder), "" );
typedef Cylinder::TGeometry TGeometry;
