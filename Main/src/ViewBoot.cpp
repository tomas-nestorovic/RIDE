#include "stdafx.h"

	#define INI_BOOT	_T("Boot")

	#define CRITICAL_VALUE_SIDES_COUNT		_T("msgpg1")
	#define CRITICAL_VALUE_SECTORS_COUNT	_T("msgpg2")
	#define CRITICAL_VALUE_SECTOR_SIZE		_T("msgpg5")
	#define CRITICAL_VALUE_CLUSTER_SIZE		_T("msgpg6")
	#define CYLINDERS_ADDED_TO_FAT			_T("msgpg3")
	#define CYLINDERS_REMOVED_FROM_FAT		_T("msgpg4")
	#define IMAGE_STRUCTURE_UNAFFECTED		_T("Changing this value does NOT affect the image structure.\n\nTo modify its structure, switch to the \"") TRACK_MAP_TAB_LABEL _T("\" tab.")

	#define IMAGE	tab.image
	#define DOS		IMAGE->dos

	void CBootView::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_BOOT, messageId );
	}

	void WINAPI CBootView::__bootSectorModified__(PropGrid::PCustomParam){
		// marking the Boot Sector as dirty
		const PDos dos=CDos::GetFocused();
		dos->FlushToBootSector(); // forcing internal (=correct) values
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
		if (Utils::QuestionYesNo(_T("About to modify a critical value, data at stake if set incorrectly!\n\nContinue?!"),MB_DEFBUTTON2)){
			const PDos dos=CDos::GetFocused();
			// . composition of the new Format
			TFormat fmt=dos->formatBoot;
			if (criticalValueId==CRITICAL_VALUE_SIDES_COUNT)
				fmt.nHeads=(THead)newValue;
			else if (criticalValueId==CRITICAL_VALUE_SECTORS_COUNT)
				fmt.nSectors=(TSector)newValue;
			else if (criticalValueId==CRITICAL_VALUE_SECTOR_SIZE)
				fmt.sectorLength=(WORD)newValue;
			else if (criticalValueId==CRITICAL_VALUE_CLUSTER_SIZE)
				fmt.clusterSize=(TSector)newValue;
			// . try to accept the new Format
			if (!dos->ChangeFormatAndReportProblem( true, true, fmt, DOS_MSG_HIT_ESC ))
				return false;
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
		// - make sure affected Cylinders are Empty
		const TCylinder nCyl0=dos->formatBoot.nCylinders, cylA=std::min((int)nCyl0,newValue), cylZ=std::max((int)nCyl0,newValue);
		// - validate and adopt new Format (eventually extending existing FAT to accommodate new Cylinders, or shrinking FAT to spare space on the disk)
		TFormat fmt=dos->formatBoot;
			fmt.nCylinders=(TCylinder)newValue;
		if (!dos->ChangeFormatAndReportProblem( true, true, fmt, DOS_MSG_HIT_ESC ))
			return false;
		// - adding to or removing from FAT
		TCHAR bufMsg[500];
		if (newValue>nCyl0){
			// adding Cylinders to FAT
			::wsprintf( bufMsg, CYLINDER_OPERATION_FINISHED, _T("added to FAT as empty") );
			if (dos->AddStdCylindersToFatAsEmpty( cylA, cylZ-1, CActionProgress::None ))
				__informationWithCheckableShowNoMore__(bufMsg,CYLINDERS_ADDED_TO_FAT);
			else{
				::lstrcat( bufMsg, _T("\n\n") FAT_SECTOR_UNMODIFIABLE );
				Utils::Information( bufMsg, ::GetLastError() );
			}
		}else if (newValue<nCyl0){
			// removing Cylinders from FAT
			::wsprintf( bufMsg, CYLINDER_OPERATION_FINISHED, _T("removed from FAT") );
			dos->RemoveStdCylindersFromFat( cylA, cylZ-1, CActionProgress::None ); // no error checking as its assumed that some Cylinders couldn't be marked in (eventually shrunk) FAT as Unavailable
			__informationWithCheckableShowNoMore__(bufMsg,CYLINDERS_REMOVED_FROM_FAT);
		}
		// - accepting the new number of Cylinders
		dos->formatBoot.nCylinders=newValue;
		dos->FlushToBootSector();
		dos->image->UpdateAllViews(nullptr);
		return true;
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

	void CBootView::OnSectorChanging() const{
		// custom action performed whenever the Sector data have been modified
		DOS->FlushToBootSector(); // not allowing to change DOS-critical values otherwise than through the PropertyGrid
	}

	void CBootView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		// - base
		__super::OnUpdate( pSender, lHint, pHint );
		// - getting Boot Sector data
		WORD bootSectorDataRealLength=0; // initializing just in case the Boot Sector is not found
		const PSectorData boot=IMAGE->GetHealthySectorData( GetPhysicalAddress(), &bootSectorDataRealLength );
		// - populating the PropertyGrid with values from the Boot Sector (if any found)
		if (boot){
			// Boot Sector found - populating the PropertyGrid
			// . basic parameters from the Boot Sector
			TCommonBootParameters cbp;
			::ZeroMemory(&cbp,sizeof(cbp));
			GetCommonBootParameters(cbp,boot);
			const Medium::PCProperties props=Medium::GetProperties(DOS->formatBoot.mediumType);
			const HANDLE hGeometry= cbp.geometryCategory ? PropGrid::AddCategory(propGrid.m_hWnd,nullptr,_T("Geometry")) : 0;
			if (hGeometry){
				if (cbp.chs){
					__pg_showPositiveInteger__( propGrid.m_hWnd, hGeometry, &DOS->formatBoot.nCylinders, nullptr, __updateFatAfterChangingCylinderCount__, props->cylinderRange.iMax, _T("Cylinders") );
					__pg_showPositiveInteger__( propGrid.m_hWnd, hGeometry, &DOS->formatBoot.nHeads, CRITICAL_VALUE_SIDES_COUNT, __confirmCriticalValueInBoot__, props->headRange.iMax, _T("Heads") );
					__pg_showPositiveInteger__( propGrid.m_hWnd, hGeometry, &DOS->formatBoot.nSectors, CRITICAL_VALUE_SECTORS_COUNT, __confirmCriticalValueInBoot__, std::min(props->sectorRange.iMax,(int)DOS->properties->nSectorsOnTrackMax), _T("Sectors/track") );
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
											PropGrid::String::DefineFixedLengthEditorA(-1)
										);
	}
