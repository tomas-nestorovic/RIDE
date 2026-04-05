#include "stdafx.h"

namespace Sector
{
	const TId TId::Invalid={ -1, -1, -1, -1 };
	
	N TId::CountAppearances(const TId *ids,N nIds,const TId &id){
		// returns the # of appearances of specified ID
		N nAppearances=0;
		while (nIds--)
			nAppearances+=*ids++==id;
		return nAppearances;
	}

	CString TId::List(PCSectorId ids,N nIds,N iHighlight,char highlightBullet){
		// creates and returns a List of Sector IDs in order as provided
		ASSERT( iHighlight>=nIds || highlightBullet );
		if (!nIds)
			return _T("- [none]\r\n");
		CString list;
		list.Format( _T("- [%d sectors, chronologically]\r\n"), nIds );
		for( N i=0; i<nIds; i++ ){
			TCHAR duplicateId[8];
			if (const N nDuplicates=CountAppearances( ids, i, ids[i] ))
				::wsprintf( duplicateId, _T(" (%d)"), nDuplicates+1 );
			else
				*duplicateId='\0';
			CString tmp;
			tmp.Format( _T("%c %s%s\r\n"), i!=iHighlight?'-':highlightBullet, ids[i].ToString(), duplicateId );
			list+=tmp;
		}
		return list;
	}

	bool TId::operator==(const TId &id2) const{
		// True <=> Sector IDs are equal, otherwise False
		return	cylinder==id2.cylinder
				&&
				side==id2.side
				&&
				sector==id2.sector
				&&
				lengthCode==id2.lengthCode;
	}

	TId &TId::operator=(const FD_ID_HEADER &rih){
		// assigns Simon Owen's definition of ID to this ID and returns it
		cylinder=rih.cyl, side=rih.head, sector=rih.sector, lengthCode=rih.size;
		return *this;
	}

	TId &TId::operator=(const FD_TIMED_ID_HEADER &rtih){
		// assigns Simon Owen's definition of ID to this ID and returns it
		cylinder=rtih.cyl, side=rtih.head, sector=rtih.sector, lengthCode=rtih.size;
		return *this;
	}

	CString TId::ToString() const{
		// returns a string describing the Sector's ID
		CString result;
		result.Format(_T("ID={%d,%d,%d,%d}"),cylinder,side,sector,lengthCode);
		return result;
	}





	const TPhysicalAddress TPhysicalAddress::Invalid={ -1, -1, TId::Invalid };

	bool TPhysicalAddress::operator==(const TPhysicalAddress &chs2) const{
		// True <=> PhysicalAddresses are equal, otherwise False
		return	cylinder==chs2.cylinder
				&&
				head==chs2.head
				&&
				sectorId==chs2.sectorId;
	}

	TTrack TPhysicalAddress::GetTrackNumber() const{
		// determines and returns the Track number based on DOS's current Format
		return GetTrackNumber( CImage::GetActive()->GetHeadCount() );
	}

	CString TPhysicalAddress::GetTrackIdDesc(THead nHeads) const{
		// returns a string identifying current Track
		if (!nHeads)
			nHeads=CImage::GetActive()->GetHeadCount();
		CString desc;
		desc.Format( _T("Track %d (Cyl=%d, Head=%d)"), GetTrackNumber(nHeads), cylinder, head );
		return desc;
	}

	TTrack TPhysicalAddress::GetTrackNumber(THead nHeads) const{
		// determines and returns the Track number based on the specified NumberOfHeads
		return GetTrackNumber( cylinder, head, nHeads );
	}





	#define LENGTH_CODE_BASE	0x80

	L GetLength(LC lengthCode){
		// returns the official size in Bytes of a Sector with the given LengthCode
		return LENGTH_CODE_BASE<<lengthCode;
	}

	LC GetLengthCode(L length){
		// returns the code of the SectorLength
		LC lengthCode=0;
		while (length>LENGTH_CODE_BASE) length>>=1, lengthCode++;
		return lengthCode;
	}


}
