#include "stdafx.h"

	LPCTSTR CDos::CFatPath::GetErrorDesc(TError error){
		// returns textual description of Error
		switch (error){
			case OK				: break;
			case SECTOR			: return _T("Invalid or unreadable FAT sector.");
			case VALUE_CYCLE	: return _T("Cyclic path in the FAT.");
			case VALUE_INVALID	: return _T("Invalid value in the FAT.");
			case VALUE_BAD_SECTOR: return _T("Data sector marked as bad.");
			case LENGTH			: return _T("Incorrect FAT path length.");
			case FILE			: return _T("Invalid file descriptor.");
			default:
				ASSERT(FALSE); // unknown Error
		}
		return nullptr;
	}








	CDos::CFatPath::CFatPath(DWORD nItemsMax)
		// ctor for Dummy object which has no Buffer and just counts the Items (allocation units)
		: buffer(0)
		, nItems(0) , pLastItem(nullptr) , error(TError::OK) {
		buffer.length=nItemsMax;
	}

	CDos::CFatPath::CFatPath(const CDos *dos,PCFile file)
		// ctor for exporting a File in Image
		: buffer(	dos->formatBoot.clusterSize // "clusterSize+" = to round up to whole multiples of ClusterSize
					+
					dos->GetFileSizeOnDisk(file)/dos->formatBoot.sectorLength
					+
					1 ) // "1" = for the case that caller wanted to extend the File content (e.g. before calling CDos::ShiftFileContent)
		, nItems(0) , pLastItem(buffer) , error(TError::OK) {
		if (!dos->GetFileFatPath(file,*this))
			error=TError::FILE;
	}
	CDos::CFatPath::CFatPath(const CDos *dos,DWORD fileSize)
		// ctor for importing a File to Image
		: buffer(	dos->formatBoot.clusterSize // "clusterSize+" = to round up to whole multiples of ClusterSize
					+
					fileSize/(dos->formatBoot.sectorLength-dos->properties->dataBeginOffsetInSector-dos->properties->dataEndOffsetInSector) )
		, nItems(0) , pLastItem(buffer) , error(TError::OK) {
	}
	CDos::CFatPath::CFatPath(const CDos *dos,RCPhysicalAddress chs)
		// ctor for editing a Sector (e.g. Boot Sector)
		: buffer(1)
		, nItems(0) , pLastItem(buffer) , error(TError::OK) {
		const TItem p={ 0, chs };
		AddItem(&p);
	}










	bool CDos::CFatPath::AddItem(PCItem pItem){
		// True <=> FatPath extended with given Item is valid, otherwise False
		// - VALIDATION: FatPath must be acyclic (detected by wanting to exceed the expected number of Items)
		if (nItems==buffer.length){ error=VALUE_CYCLE; return false; }
		// - extending FatPath
		if (pLastItem!=nullptr)
			*pLastItem++=*pItem;
		nItems++;
		return true; // CDos-derivate performs additional validation
	}

	CDos::CFatPath::PCItem CDos::CFatPath::PopItem(){
		// removes and returns the last Item; returns Null if no Items in the FatPath
		if (nItems>0){
			nItems--;
			return --pLastItem;
		}else
			return nullptr;
	}

	LPCTSTR CDos::CFatPath::GetItems(PCItem &rBuffer,DWORD &rnItems) const{
		// retrieves pointer to the first Item in the Buffer and their number; returns textual description of this FatPath's error (or Null if error-less)
		rBuffer=buffer, rnItems=GetNumberOfItems();
		return GetErrorDesc();
	}

	LPCTSTR CDos::CFatPath::GetItems(PItem &rBuffer,DWORD &rnItems) const{
		// retrieves pointer to the first Item in the Buffer and their number; returns textual description of this FatPath's error (or Null if error-less)
		rBuffer=buffer, rnItems=GetNumberOfItems();
		return GetErrorDesc();
	}

	CDos::CFatPath::PItem CDos::CFatPath::GetItem(DWORD i) const{
		// returns I-th Item or Null
		if (i>=nItems) // request out of bounds
			return nullptr;
		return buffer+i;
	}

	CDos::CFatPath::PItem CDos::CFatPath::GetHealthyItem(DWORD i) const{
		// returns I-th Item or Null
		if (error) // FatPath erroneous
			return nullptr;
		return GetItem(i);
	}

	bool CDos::CFatPath::ContainsSector(RCPhysicalAddress chs) const{
		// True <=> the FatPath has the specified Sector linked in, otherwise False
		for( PCItem p=buffer; p<pLastItem; p++ )
			if (p->chs==chs)
				return true;
		if (!buffer)
			ASSERT(FALSE); // it's senseless to call this method for a dummy object!
		return false;
	}

	bool CDos::CFatPath::AreAllSectorsReadable(const CDos *dos) const{
		// True <=> all Sectors refered in the FatPath are healthy, otherwise False
		PCItem p; DWORD n;
		if (GetItems(p,n)) // error
			return false;
		while (n--)
			if (!dos->image->GetHealthySectorData(p++->chs))
				return false;
		return true;
	}

	bool CDos::CFatPath::MarkAllSectorsModified(PImage image) const{
		// True <=> all Sectors refered in the FatPath are healthy, otherwise False
		PCItem p; DWORD n;
		if (GetItems(p,n)) // error
			return false;
		while (n--)
			image->MarkSectorAsDirty(p++->chs);
		return true;
	}

	DWORD CDos::CFatPath::GetPhysicalAddresses(TPhysicalAddress *pOutChs) const{
		// returns the NumberOfItems of Items written to the output buffer; returns zero if the FatPath is erroneous
		if (error)
			return 0;
		for( DWORD i=0; i<nItems; *pOutChs++=buffer[i++].chs );
		return nItems;
	}

	LPCTSTR CDos::CFatPath::GetErrorDesc() const{
		// returns textual description of this FatPath's error (or Null if error-less)
		return GetErrorDesc(error);
	}
