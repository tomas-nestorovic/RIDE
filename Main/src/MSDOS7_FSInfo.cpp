#include "stdafx.h"
#include "MSDOS7.h"

	bool CMSDOS7::TFsInfoSector::__recognize__(WORD sectorLength) const{
		// True <=> FS Info Sector recognized, otherwise False
		return	sectorLength>=MSDOS7_SECTOR_LENGTH_STD
				&&
				mark41615252==0x41615252
				&&
				mark61417272==0x61417272
				&&
				markAA550000==0xAA550000;
	}

	void CMSDOS7::TFsInfoSector::__init__(){
		// initializes the FS Info Sector
		::ZeroMemory( this, MSDOS7_SECTOR_LENGTH_STD );
		mark41615252=0x41615252;
		mark61417272=0x61417272;
		nFreeClusters = firstFreeCluster = -1; // "-1" = information not available (will be set, for instance, in __getFirstFreeHealthyCluster__)
		markAA550000=0xAA550000;
	}










	CMSDOS7::CFsInfoView::CFsInfoView(CMSDOS7 *msdos)
		// ctor
		: CCriticalSectorView( msdos, TPhysicalAddress::Invalid ) {
	}

	#define MSDOS	((CMSDOS7 *)tab.dos)
	#define IMAGE	tab.dos->image

	CMSDOS7::PFsInfoSector CMSDOS7::CFsInfoView::GetSectorData() const{
		// returns the data of FS Info Sector; returns Null if Sector doesn't exist or isn't readable
		// - if FS Info Sector doesn't exist, we are done
		if (MSDOS->fat.type!=CFat::FAT32)
			return nullptr;
		// - reading and recognizing the FS Info Sector
		if (const PCBootSector bootSector=MSDOS->boot.GetSectorData())
			if (const PFsInfoSector fsInfo=(PFsInfoSector)MSDOS->__getHealthyLogicalSectorData__(bootSector->fat32.fsInfo))
				if (fsInfo->__recognize__(MSDOS->formatBoot.sectorLength))
					return fsInfo;
				else
					::SetLastError(ERROR_VOLMGR_DISK_LAYOUT_INVALID);
		// - FS Info Sector isn't readable or isn't recognized
		return nullptr;
	}

	void CMSDOS7::CFsInfoView::MarkSectorAsDirty() const{
		// marks the FS Info Sector as dirty
		if (const PCBootSector bootSector=MSDOS->boot.GetSectorData())
			MSDOS->__markLogicalSectorAsDirty__( bootSector->fat32.fsInfo );
	}

	bool WINAPI CMSDOS7::CFsInfoView::__sectorModified__(PropGrid::PCustomParam,int){
		// marking the Boot Sector as dirty
		const PMSDOS7 msdos=(PMSDOS7)CDos::GetFocused();
		msdos->fsInfo.MarkSectorAsDirty();
		msdos->image->UpdateAllViews(nullptr);
		return true;
	}

	void CMSDOS7::CFsInfoView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		// - changing to the FS-Info Sector (might have been switched elsewhere before, e.g. Boot error now recovered)
		if (const PCBootSector bootSector=MSDOS->boot.GetSectorData())
			ChangeToSector(  MSDOS->__logfyz__( bootSector->fat32.fsInfo )  );
		else
			ChangeToSector( TPhysicalAddress::Invalid );
		// - base
		__super::OnUpdate( pSender, lHint, pHint );
		// - populating the PropertyGrid with values from the FS-Info Sector (if any found)
		const PFsInfoSector fsInfo=GetSectorData();
		PropGrid::AddDisabledProperty(	propGrid.m_hWnd, nullptr, _T("Status"),
										fsInfo?"Recognized":"Not recognized",
										PropGrid::String::DefineFixedLengthEditorA(0)
									);
		if (fsInfo){
			// FS-Info Sector found
			const HANDLE hClusters=PropGrid::AddCategory(propGrid.m_hWnd,nullptr,_T("Clusters"));
				const PropGrid::Integer::TUpDownLimits limits={ 2, INT_MAX };
				PropGrid::AddProperty(	propGrid.m_hWnd, hClusters, _T("First free"),
										&fsInfo->firstFreeCluster,
										PropGrid::Integer::DefineEditor( sizeof(TCluster32), limits, __sectorModified__ )
									);
				PropGrid::AddProperty(	propGrid.m_hWnd, hClusters, _T("Count of free"),
										&fsInfo->nFreeClusters,
										PropGrid::Integer::DefineEditor( sizeof(TCluster32), PropGrid::Integer::TUpDownLimits::PositiveInteger, __sectorModified__ )
									);
		}
	}
