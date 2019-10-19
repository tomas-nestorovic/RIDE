#include "stdafx.h"

	#define INI_BOOT	_T("Boot")

	#define CRITICAL_VALUE_SIDES_COUNT		_T("msgpg1")
	#define CRITICAL_VALUE_SECTORS_COUNT	_T("msgpg2")
	#define CYLINDERS_ADDED_TO_FAT			_T("msgpg3")
	#define CYLINDERS_REMOVED_FROM_FAT		_T("msgpg4")
	#define IMAGE_STRUCTURE_UNAFFECTED		_T("Changing this value does NOT affect the image structure.\n\nTo modify its structure, switch to the \"") TRACK_MAP_TAB_LABEL _T("\" tab.")

	#define DOS		tab.dos
	#define IMAGE	DOS->image

	void CBootView::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_BOOT, messageId );
	}

	bool WINAPI CBootView::__bootSectorModified__(CPropGridCtrl::PCustomParam,int){
		// marking the Boot Sector as dirty
		const PDos dos=CDos::GetFocused();
		dos->FlushToBootSector(); // marking the Boot Sector as dirty
		dos->image->UpdateAllViews(nullptr);
		return true;
	}

	bool WINAPI CBootView::__bootSectorModifiedA__(CPropGridCtrl::PCustomParam,LPCSTR,short){
		// marking the Boot Sector as dirty
		return __bootSectorModified__(nullptr,0);
	}

	bool WINAPI CBootView::__bootSectorModified__(CPropGridCtrl::PCustomParam,bool){
		// marking the Boot Sector as dirty
		return __bootSectorModified__(nullptr,0);
	}

	bool WINAPI CBootView::__bootSectorModified__(CPropGridCtrl::PCustomParam,CPropGridCtrl::TEnum::UValue){
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









	static void __pg_showPositiveInteger__(HWND hPropGrid,HANDLE hCategory,PVOID pInteger,LPCTSTR criticalValueId,CPropGridCtrl::TInteger::TOnValueConfirmed fn,int maxValue,LPCTSTR caption){
		// shows Integer in value in PropertyGrid's specified Category
		if (const PCBYTE pZeroByte=(PCBYTE)::memchr(&maxValue,0,sizeof(maxValue))){
			const CPropGridCtrl::TInteger::TUpDownLimits limits={ 1, maxValue };
			CPropGridCtrl::AddProperty(	hPropGrid, hCategory, caption,
										pInteger, pZeroByte-(PCBYTE)&maxValue,
										CPropGridCtrl::TInteger::DefineEditor( limits, fn ),
										(PVOID)criticalValueId
									);
		}else
			CPropGridCtrl::AddProperty(	hPropGrid, hCategory, caption,
										pInteger, sizeof(DWORD),
										CPropGridCtrl::TInteger::DefineEditor( CPropGridCtrl::TInteger::PositiveIntegerLimits, fn ),
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
			const HANDLE hGeometry= cbp.geometryCategory ? CPropGridCtrl::AddCategory(propGrid.m_hWnd,nullptr,_T("Geometry")) : 0;
			if (hGeometry){
				if (cbp.chs){
					__pg_showPositiveInteger__( propGrid.m_hWnd, hGeometry, &DOS->formatBoot.nCylinders, nullptr, __updateFatAfterChangingCylinderCount__, props->cylinderRange.iMax, _T("Cylinders") );
					__pg_showPositiveInteger__( propGrid.m_hWnd, hGeometry, &DOS->formatBoot.nHeads, CRITICAL_VALUE_SIDES_COUNT, __confirmCriticalValueInBoot__, props->headRange.iMax, _T("Heads") );
					__pg_showPositiveInteger__( propGrid.m_hWnd, hGeometry, &DOS->formatBoot.nSectors, CRITICAL_VALUE_SECTORS_COUNT, __confirmCriticalValueInBoot__, std::min<int>(props->sectorRange.iMax,DOS->properties->nSectorsOnTrackMax), _T("Sectors/track") );
				}
				if (cbp.pSectorLength){
					static const CPropGridCtrl::TInteger::TUpDownLimits Limits={128,16384};
					CPropGridCtrl::AddProperty(	propGrid.m_hWnd, hGeometry, _T("Sector size"),
												cbp.pSectorLength, sizeof(WORD),
												CPropGridCtrl::TInteger::DefineEditor(Limits,__bootSectorModified__)
											);
				}
			}
			const HANDLE hVolume= cbp.volumeCategory ? CPropGridCtrl::AddCategory(propGrid.m_hWnd,nullptr,_T("Volume")) : 0;
			if (hVolume){
				if (cbp.label.length)
					CPropGridCtrl::AddProperty(	propGrid.m_hWnd, hVolume, _T("Label"),
												cbp.label.bufferA, cbp.label.length,
												CPropGridCtrl::TString::DefineFixedLengthEditorA( cbp.label.onLabelConfirmedA?cbp.label.onLabelConfirmedA:__bootSectorModifiedA__, cbp.label.fillerByte )
											);
				if (cbp.id.buffer){
					const CPropGridCtrl::TInteger::TUpDownLimits limits={ 0, (UINT)-1>>8*(sizeof(UINT)-cbp.id.bufferCapacity) };
					CPropGridCtrl::AddProperty(	propGrid.m_hWnd, hVolume, _T("ID"),
												cbp.id.buffer, cbp.id.bufferCapacity,
												CPropGridCtrl::TInteger::DefineEditor(limits,__bootSectorModified__)
											);
				}
			}
			// . DOS-specific parameters of Boot
			AddCustomBootParameters(propGrid.m_hWnd,hGeometry,hVolume,cbp,boot);
		}else
			// Boot Sector not found - informing through PropertyGrid
			CPropGridCtrl::EnableProperty(	propGrid.m_hWnd,
											CPropGridCtrl::AddProperty(	propGrid.m_hWnd, nullptr,
																		_T("Boot sector"), "Not found!", -1,
																		CPropGridCtrl::TString::DefineFixedLengthEditorA()
																	),
											false
										);
	}
