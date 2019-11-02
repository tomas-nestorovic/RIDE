#include "stdafx.h"

	#define INI_BOOT	_T("Boot")

	#define CRITICAL_VALUE_SIDES_COUNT		_T("msgpg1")
	#define CRITICAL_VALUE_SECTORS_COUNT	_T("msgpg2")
	#define CRITICAL_VALUE_SECTOR_SIZE		_T("msgpg5")
	#define CRITICAL_VALUE_CLUSTER_SIZE		_T("msgpg6")
	#define CYLINDERS_ADDED_TO_FAT			_T("msgpg3")
	#define CYLINDERS_REMOVED_FROM_FAT		_T("msgpg4")
	#define IMAGE_STRUCTURE_UNAFFECTED		_T("Changing this value does NOT affect the image structure.\n\nTo modify its structure, switch to the \"") TRACK_MAP_TAB_LABEL _T("\" tab.")

	#define DOS		tab.dos
	#define IMAGE	DOS->image

	void CBootView::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_BOOT, messageId );
	}

	void WINAPI CBootView::__bootSectorModified__(PropGrid::PCustomParam){
		// marking the Boot Sector as dirty
		const PDos dos=CDos::GetFocused();
		dos->FlushToBootSector(); // marking the Boot Sector as dirty
		dos->image->UpdateAllViews(nullptr);
	}

	bool WINAPI CBootView::__bootSectorModified__(PropGrid::PCustomParam,int){
		// marking the Boot Sector as dirty
		__bootSectorModified__(nullptr);
		return true;
	}

	bool WINAPI CBootView::__bootSectorModifiedA__(PropGrid::PCustomParam,LPCSTR,short){
		// marking the Boot Sector as dirty
		return __bootSectorModified__(nullptr,0);
	}

	bool WINAPI CBootView::__bootSectorModified__(PropGrid::PCustomParam,bool){
		// marking the Boot Sector as dirty
		return __bootSectorModified__(nullptr,0);
	}

	bool WINAPI CBootView::__bootSectorModified__(PropGrid::PCustomParam,PropGrid::Enum::UValue){
		// marking the Boot Sector as dirty
		return __bootSectorModified__(nullptr,0);
	}

	bool WINAPI CBootView::__confirmCriticalValueInBoot__(PVOID criticalValueId,int newValue){
		// informs that a critical value in the Boot is about to be changed, and if confirmed, changes it
		if (Utils::QuestionYesNo(_T("About to modify a critical value in the boot, data at stake if set incorrectly!\n\nContinue?!"),MB_DEFBUTTON2)){
			const PDos dos=CDos::GetFocused();
			// . validating the new format
			TFormat fmt=dos->formatBoot;
			if (criticalValueId==CRITICAL_VALUE_SIDES_COUNT)
				fmt.nHeads=(THead)newValue;
			else if (criticalValueId==CRITICAL_VALUE_SECTORS_COUNT)
				fmt.nSectors=(TSector)newValue;
			else if (criticalValueId==CRITICAL_VALUE_SECTOR_SIZE)
				fmt.sectorLength=(WORD)newValue;
			else if (criticalValueId==CRITICAL_VALUE_CLUSTER_SIZE)
				fmt.clusterSize=(TSector)newValue;
			if (!dos->ValidateFormatChangeAndReportProblem(false,&fmt))
				return false;
			// . accepting the new format
			dos->formatBoot=fmt;
			dos->FlushToBootSector();
			dos->image->UpdateAllViews(nullptr);
			__informationWithCheckableShowNoMore__(IMAGE_STRUCTURE_UNAFFECTED,(LPCTSTR)criticalValueId);
			return true;
		}else
			return false;
	}

	#define CYLINDER_OPERATION_FINISHED	_T("Cylinder(s) have been %s, respecting the number of heads and sectors in the boot.")

	bool WINAPI CBootView::__updateFatAfterChangingCylinderCount__(PVOID,int newValue){
		// updates the FAT after the number of Cylinders in the FAT has been changed
		const PDos dos=CDos::GetFocused();
		TFormat fmt=dos->formatBoot;
			fmt.nCylinders=(TCylinder)newValue;
		if (dos->ValidateFormatChangeAndReportProblem(false,&fmt)){
			// new number of Cylinders acceptable
			const TCylinder nCyl0=(TCylinder)dos->formatBoot.nCylinders, cylZ=std::max<int>(nCyl0,newValue);
			const THead nHeads=dos->formatBoot.nHeads;
			const TTrack nTracksMax=cylZ*nHeads;
			TStdWinError err;
			const PCylinder bufCylinders=(PCylinder)::calloc(nTracksMax,sizeof(TCylinder)); // a "big enough" buffer
			if (( err=::GetLastError() )==ERROR_SUCCESS){
				const PHead bufHeads=(PHead)::calloc(nTracksMax,sizeof(THead)); // a "big enough" buffer
				if (( err=::GetLastError() )==ERROR_SUCCESS){
					// . composing the list of Tracks that will be added to or removed from FAT
					TTrack nTracks=0;
					for( TCylinder cylA=std::min<int>(nCyl0,newValue); cylA<cylZ; cylA++ )
						for( THead head=0; head<nHeads; head++,nTracks++ )
							bufCylinders[nTracks]=cylA, bufHeads[nTracks]=head;
					// . adding to or removing from FAT
					if (( err=dos->__areStdCylindersEmpty__(nTracks,bufCylinders) )==ERROR_EMPTY){
						// none of the Cylinders contains reachable data
						dos->formatBoot.nCylinders=newValue; // accepting the new number of Cylinders
						TCHAR bufMsg[500],*operationDesc;
						if (newValue>nCyl0){ // adding Tracks to FAT
							operationDesc=_T("added to FAT as empty");
							::wsprintf( bufMsg, CYLINDER_OPERATION_FINISHED, operationDesc );
							if (dos->__addStdTracksToFatAsEmpty__(nTracks,bufCylinders,bufHeads))
								__informationWithCheckableShowNoMore__(bufMsg,CYLINDERS_ADDED_TO_FAT);
							else{
errorFAT:						::wsprintf( bufMsg+::lstrlen(bufMsg), _T("\n\n") FAT_SECTOR_UNMODIFIABLE, operationDesc );
								Utils::Information(bufMsg,::GetLastError());
							}
						}else if (newValue<nCyl0){ // removing Tracks from FAT
							operationDesc=_T("removed from FAT");
							::wsprintf( bufMsg, CYLINDER_OPERATION_FINISHED, operationDesc );
							if (dos->__removeStdTracksFromFat__(nTracks,bufCylinders,bufHeads))
								__informationWithCheckableShowNoMore__(bufMsg,CYLINDERS_REMOVED_FROM_FAT);
							else
								goto errorFAT;
						}
						dos->FlushToBootSector();
					}
					::free(bufHeads);
				}
				::free(bufCylinders);
			}
			if (err!=ERROR_EMPTY){
				if (err==ERROR_NOT_EMPTY)
					Utils::Information(DOS_ERR_CANNOT_ACCEPT_VALUE,DOS_ERR_CYLINDERS_NOT_EMPTY,DOS_MSG_HIT_ESC);
				else
					Utils::Information(DOS_ERR_CANNOT_ACCEPT_VALUE,err,DOS_MSG_HIT_ESC);
				return false;
			}
			dos->image->UpdateAllViews(nullptr);
			return true;
		}else
			return false;
	}








	CBootView::CBootView(PDos dos,RCPhysicalAddress rChsBoot)
		// ctor
		: CCriticalSectorView(dos,rChsBoot) {
	}









	static void __pg_showPositiveInteger__(HWND hPropGrid,HANDLE hCategory,PVOID pInteger,LPCTSTR criticalValueId,PropGrid::Integer::TOnValueConfirmed fn,int maxValue,LPCTSTR caption){
		// shows Integer in value in PropertyGrid's specified Category
		const PropGrid::Integer::TUpDownLimits limits={ 1, maxValue };
		BYTE size;
		if (maxValue<=(BYTE)-1)
			size=sizeof(BYTE);
		else if (maxValue<=(WORD)-1)
			size=sizeof(WORD);
		else
			size=sizeof(DWORD);
		PropGrid::AddProperty(	hPropGrid, hCategory, caption,
								pInteger, 
								PropGrid::Integer::DefineEditor( size, limits, fn ),
								(PVOID)criticalValueId
							);
	}

	void CBootView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		// - base
		__super::OnUpdate( pSender, lHint, pHint );
		// - getting Boot Sector data
		WORD bootSectorDataRealLength=0; // initializing just in case the Boot Sector is not found
		const PSectorData boot=IMAGE->GetHealthySectorData(chs,&bootSectorDataRealLength);
		// - populating the PropertyGrid with values from the Boot Sector (if any found)
		if (boot){
			// Boot Sector found - populating the PropertyGrid
			// . basic parameters from the Boot Sector
			TCommonBootParameters cbp;
			::ZeroMemory(&cbp,sizeof(cbp));
			GetCommonBootParameters(cbp,boot);
			const TMedium::PCProperties props=TMedium::GetProperties(DOS->formatBoot.mediumType);
			const HANDLE hGeometry= cbp.geometryCategory ? PropGrid::AddCategory(propGrid.m_hWnd,nullptr,_T("Geometry")) : 0;
			if (hGeometry){
				if (cbp.chs){
					__pg_showPositiveInteger__( propGrid.m_hWnd, hGeometry, &DOS->formatBoot.nCylinders, nullptr, __updateFatAfterChangingCylinderCount__, props->cylinderRange.iMax, _T("Cylinders") );
					__pg_showPositiveInteger__( propGrid.m_hWnd, hGeometry, &DOS->formatBoot.nHeads, CRITICAL_VALUE_SIDES_COUNT, __confirmCriticalValueInBoot__, props->headRange.iMax, _T("Heads") );
					__pg_showPositiveInteger__( propGrid.m_hWnd, hGeometry, &DOS->formatBoot.nSectors, CRITICAL_VALUE_SECTORS_COUNT, __confirmCriticalValueInBoot__, std::min<int>(props->sectorRange.iMax,DOS->properties->nSectorsOnTrackMax), _T("Sectors/track") );
				}
				if (cbp.sectorLength)
					__pg_showPositiveInteger__( propGrid.m_hWnd, hGeometry, &DOS->formatBoot.sectorLength, CRITICAL_VALUE_SECTOR_SIZE, __confirmCriticalValueInBoot__, 16384, _T("Sector size") );
			}
			const HANDLE hVolume= cbp.volumeCategory ? PropGrid::AddCategory(propGrid.m_hWnd,nullptr,_T("Volume")) : 0;
			if (hVolume){
				if (cbp.label.length)
					PropGrid::AddProperty(	propGrid.m_hWnd, hVolume, _T("Label"),
											cbp.label.bufferA,
											PropGrid::String::DefineFixedLengthEditorA( cbp.label.length, cbp.label.onLabelConfirmedA?cbp.label.onLabelConfirmedA:__bootSectorModifiedA__, cbp.label.fillerByte )
										);
				if (cbp.id.buffer){
					const PropGrid::Integer::TUpDownLimits limits={ 0, (UINT)-1>>8*(sizeof(UINT)-cbp.id.bufferCapacity) };
					PropGrid::AddProperty(	propGrid.m_hWnd, hVolume, _T("ID"),
											cbp.id.buffer,
											PropGrid::Integer::DefineEditor( cbp.id.bufferCapacity, limits, __bootSectorModified__ )
										);
				}
				if (cbp.clusterSize)
					__pg_showPositiveInteger__( propGrid.m_hWnd, hVolume, &DOS->formatBoot.clusterSize, CRITICAL_VALUE_CLUSTER_SIZE, __confirmCriticalValueInBoot__, DOS->properties->clusterSizeMax, _T("Cluster size") );
			}
			// . DOS-specific parameters of Boot
			AddCustomBootParameters(propGrid.m_hWnd,hGeometry,hVolume,cbp,boot);
		}else
			// Boot Sector not found - informing through PropertyGrid
			PropGrid::AddDisabledProperty(	propGrid.m_hWnd, nullptr,
											_T("Boot sector"), "Not found!",
											PropGrid::String::DefineFixedLengthEditorA(0)
										);
	}
