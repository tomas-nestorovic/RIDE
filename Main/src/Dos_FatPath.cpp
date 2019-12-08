#include "stdafx.h"

	LPCTSTR CDos::CFatPath::GetErrorDesc(TError error){
		// returns textual description of Error
		switch (error){
			case OK				: break;
			case SECTOR			: return _T("FAT sector not found or read with CRC error.");
			case VALUE_CYCLE	: return _T("Cyclic path in the FAT.");
			case VALUE_INVALID	: return _T("Invalid value in the FAT.");
			case VALUE_BAD_SECTOR: return _T("Data sector marked as bad.");
			case FILE			: return _T("Invalid file descriptor.");
			default:
				ASSERT(FALSE); // unknown Error
		}
		return nullptr;
	}








	CDos::CFatPath::CFatPath(DWORD nItemsMax)
		// ctor for Dummy object which has no Buffer and just counts the Items (allocation units)
		: nItemsMax(nItemsMax) , buffer(nullptr)
		, nItems(0) , pLastItem(nullptr) {
	}

	CDos::CFatPath::CFatPath(const CDos *dos,PCFile file)
		// ctor for exporting a File in Image
		: nItemsMax(dos->formatBoot.clusterSize // "clusterSize+" = to round up to whole multiples of ClusterSize
					+
					dos->GetFileSizeOnDisk(file)/dos->formatBoot.sectorLength
					+
					1 ) // "1" = for the case that caller wanted to extend the File content (e.g. before calling CDos::ShiftFileContent)
		, buffer((TItem *)::calloc( nItemsMax, sizeof(TItem) ))
		, nItems(0) , pLastItem(buffer) , error(TError::OK) {
		if (!dos->GetFileFatPath(file,*this))
			error=TError::FILE;
	}
	CDos::CFatPath::CFatPath(const CDos *dos,DWORD fileSize)
		// ctor for importing a File to Image
		: nItemsMax(dos->formatBoot.clusterSize // "clusterSize+" = to round up to whole multiples of ClusterSize
					+
					fileSize/(dos->formatBoot.sectorLength-dos->properties->dataBeginOffsetInSector-dos->properties->dataEndOffsetInSector) )
		, buffer((TItem *)::calloc( nItemsMax, sizeof(TItem) ))
		, nItems(0) , pLastItem(buffer) , error(TError::OK) {
	}
	CDos::CFatPath::CFatPath(const CDos *dos,RCPhysicalAddress chs)
		// ctor for editing a Sector (e.g. Boot Sector)
		: nItemsMax(1)
		, buffer((TItem *)::malloc(sizeof(TItem)))
		, nItems(0) , pLastItem(buffer) , error(TError::OK) {
		const TItem p={ 0, chs };
		AddItem(&p);
	}

	CDos::CFatPath::~CFatPath(){
		// dtor
		if (buffer!=nullptr)
			::free(buffer);
	}









	bool CDos::CFatPath::AddItem(PCItem pItem){
		// True <=> FatPath extended with given Item is valid, otherwise False
		// - VALIDATION: FatPath must be acyclic (detected by wanting to exceed the expected number of Items)
		if (nItems==nItemsMax){ error=VALUE_CYCLE; return false; }
		// - extending FatPath
		if (pLastItem!=nullptr)
			*pLastItem++=*pItem;
		nItems++;
		return true; // CDos-derivate performs additional validation
	}

	LPCTSTR CDos::CFatPath::GetItems(PCItem &rBuffer,DWORD &rnItems) const{
		// retrieves pointer to the first Item in the Buffer and their number; returns textual description of this FatPath's error (or Null if error-less)
		rBuffer=buffer, rnItems=GetNumberOfItems();
		return GetErrorDesc();
	}

	DWORD CDos::CFatPath::GetNumberOfItems() const{
		// returns the NumberOfItems currenty in the FileFatPath
		return nItems;
	}

	LPCTSTR CDos::CFatPath::GetErrorDesc() const{
		// returns textual description of this FatPath's error (or Null if error-less)
		return GetErrorDesc(error);
	}
