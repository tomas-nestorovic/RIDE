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






	CGeometryReaderWriter::CGeometryReaderWriter(PImage image,Yahel::TPosition dataTotalLength,const Yahel::TInterval<char> &padding,const TRev &nDiscoveredRevolutions,FOnWritten onWritten,const TGeometry &g)
		// ctor
		: Sector::CReaderWriter( image, dataTotalLength, padding, nDiscoveredRevolutions, onWritten )
		, TGeometry(g)
		, usableSectorLength( g.sectorLength-padding.GetLength() ) {
	}



	Yahel::TPosition CGeometryReaderWriter::GetSectorStartPosition(RCPhysicalAddress chs,Sector::N nSectorsToSkip) const{
		// computes and returns the position of the first Byte of the Sector at the PhysicalAddress
		return Yahel::TPosition( chs.GetTrackNumber(sides.length)*nSectors + chs.sectorId.sector-firstSectorNumber )*sectorLength;
	}

	void CGeometryReaderWriter::GetPhysicalAddress(Yahel::TPosition pos,TPhysicalAddress &outChs,Sector::N &outSectorIndex,Sector::PL pOutOffset) const{
		// determines the PhysicalAddress that contains the specified LogicalPosition
		const auto &&s=div( pos, usableSectorLength ); // Quot = # of Sectors to skip, Rem = the first Byte to read in the Sector yet to be computed
		if (pOutOffset)
			*pOutOffset=s.rem-sector.padding.a; // start Padding provided as NEGATIVE!
		const auto &&t=div( s.quot, nSectors ); // Quot = # of Tracks to skip, Rem = the zero-based Sector index on a Track yet to be computed
		outSectorIndex=t.rem;
		const auto &&h=div( t.quot, sides.length ); // Quot = # of Cylinders to skip, Rem = Head in a Cylinder
		outChs.sectorId.cylinder = outChs.cylinder = h.quot;
		outChs.sectorId.side=sides[ outChs.head = h.rem ];
		outChs.sectorId.sector=firstSectorNumber+sector.indexOnTrack;
		outChs.sectorId.lengthCode=sectorLengthCode;
	}

	Yahel::TRow CGeometryReaderWriter::LogicalPositionToRow(Yahel::TPosition logPos,WORD nBytesInRow){
		// computes and returns the row containing the specified LogicalPosition
		const auto &&d=div( logPos, usableSectorLength );
		const auto nRowsPerRecord=Utils::RoundDivUp( usableSectorLength, nBytesInRow );
		return d.quot*nRowsPerRecord + d.rem/nBytesInRow;
	}

	Yahel::TPosition CGeometryReaderWriter::RowToLogicalPosition(Yahel::TRow row,WORD nBytesInRow){
		// converts Row begin (i.e. its first Byte) to corresponding logical position in underlying File and returns the result
		const auto nRowsPerRecord=Utils::RoundDivUp( usableSectorLength, nBytesInRow );
		const auto &&d=div( row, nRowsPerRecord );
		return d.quot*usableSectorLength + d.rem*nBytesInRow;
	}

	void CGeometryReaderWriter::GetRecordInfo(Yahel::TPosition logPos,Yahel::PPosition pOutRecordStartLogPos,Yahel::PPosition pOutRecordLength,bool *pOutDataReady){
		// retrieves the start logical position and length of the Record pointed to by the input LogicalPosition
		if (pOutRecordStartLogPos)
			*pOutRecordStartLogPos = logPos/usableSectorLength*usableSectorLength;
		if (pOutRecordLength)
			*pOutRecordLength = usableSectorLength;
		if (pOutDataReady)
			*pOutDataReady=true;
	}

}
