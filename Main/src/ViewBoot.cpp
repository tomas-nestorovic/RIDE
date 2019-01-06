#include "stdafx.h"

	#define INI_BOOT	_T("Boot")

	#define CRITICAL_VALUE_SIDES_COUNT		_T("msgpg1")
	#define CRITICAL_VALUE_SECTORS_COUNT	_T("msgpg2")
	#define CYLINDERS_ADDED_TO_FAT			_T("msgpg3")
	#define CYLINDERS_REMOVED_FROM_FAT		_T("msgpg4")
	#define IMAGE_STRUCTURE_UNAFFECTED		_T("Changing this value does NOT affect the image structure.\n\nTo modify its structure, switch to the \"") TRACK_MAP_TAB_LABEL _T("\" tab.")

	const CBootView *CBootView::pCurrentlyShown;

	#define DOS		tab.dos
	#define IMAGE	DOS->image

	void CBootView::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_BOOT, messageId );
	}

	bool WINAPI CBootView::__bootSectorModified__(CPropGridCtrl::PCustomParam,int){
		// marking the Boot Sector as dirty
		const PDos dos=CDos::__getFocused__();
		dos->FlushToBootSector(); // marking the Boot Sector as dirty
		dos->image->UpdateAllViews(NULL);
		return true;
	}

	bool WINAPI CBootView::__bootSectorModifiedA__(CPropGridCtrl::PCustomParam,LPCSTR,short){
		// marking the Boot Sector as dirty
		return __bootSectorModified__(NULL,0);
	}

	bool WINAPI CBootView::__bootSectorModified__(CPropGridCtrl::PCustomParam,bool){
		// marking the Boot Sector as dirty
		return __bootSectorModified__(NULL,0);
	}

	bool WINAPI CBootView::__bootSectorModified__(CPropGridCtrl::PCustomParam,CPropGridCtrl::TEnum::UValue){
		// marking the Boot Sector as dirty
		return __bootSectorModified__(NULL,0);
	}

	void WINAPI CBootView::__updateBootView__(CPropGridCtrl::PCustomParam){
		// refreshes any current View
		const short iCurSel=CPropGridCtrl::GetCurrentlySelectedProperty( pCurrentlyShown->propGrid.m_hWnd );
			CDos::__getFocused__()->image->UpdateAllViews(NULL);
		CPropGridCtrl::SetCurrentlySelectedProperty( pCurrentlyShown->propGrid.m_hWnd, iCurSel );
	}

	bool WINAPI CBootView::__confirmCriticalValueInBoot__(PVOID criticalValueId,int newValue){
		// informs that a critical value in the Boot is about to be changed, and if confirmed, changes it
		if (Utils::QuestionYesNo(_T("About to modify a critical value in the boot, data at stake if set incorrectly!\n\nContinue?!"),MB_DEFBUTTON2)){
			const PDos dos=CDos::__getFocused__();
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
			dos->image->UpdateAllViews(NULL);
			__informationWithCheckableShowNoMore__(IMAGE_STRUCTURE_UNAFFECTED,(LPCTSTR)criticalValueId);
			return true;
		}else
			return false;
	}

	#define CYLINDER_OPERATION_FINISHED	_T("Cylinder(s) have been %s, respecting the number of heads and sectors in the boot.")

	bool WINAPI CBootView::__updateFatAfterChangingCylinderCount__(PVOID,int newValue){
		// updates the FAT after the number of Cylinders in the FAT has been changed
		const PDos dos=CDos::__getFocused__();
		TFormat fmt=dos->formatBoot;
			fmt.nCylinders=(TCylinder)newValue;
		if (dos->ValidateFormatChangeAndReportProblem(false,&fmt)){
			// new number of Cylinders acceptable
			const TCylinder nCyl0=(TCylinder)dos->formatBoot.nCylinders, cylZ=max(nCyl0,newValue);
			const THead nHeads=dos->formatBoot.nHeads;
			const TTrack nTracksMax=cylZ*nHeads;
			TStdWinError err;
			const PCylinder bufCylinders=(PCylinder)::calloc(nTracksMax,sizeof(TCylinder)); // a "big enough" buffer
			if (( err=::GetLastError() )==ERROR_SUCCESS){
				const PHead bufHeads=(PHead)::calloc(nTracksMax,sizeof(THead)); // a "big enough" buffer
				if (( err=::GetLastError() )==ERROR_SUCCESS){
					// . composing the list of Tracks that will be added to or removed from FAT
					TTrack nTracks=0;
					for( TCylinder cylA=min(nCyl0,newValue); cylA<cylZ; cylA++ )
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
			dos->image->UpdateAllViews(NULL);
			return true;
		}else
			return false;
	}







	static LPCTSTR __getBootSectorHexaEditorLabel__(int recordIndex,PTCHAR buf,BYTE bufCapacity,PVOID param){
		return ((CBootView *)param)->chsBoot.sectorId.ToString(buf);
	}

	#define PROPGRID_WIDTH_DEFAULT	250

	CBootView::CBootView(PDos dos,RCPhysicalAddress rChsBoot)
		// ctor
		: tab(0,0,dos,this) , splitX(PROPGRID_WIDTH_DEFAULT) , chsBoot(rChsBoot)
		, fBoot(dos,rChsBoot) , hexaEditor(this,HEXAEDITOR_RECORD_SIZE_INFINITE,__getBootSectorHexaEditorLabel__) {
	}

	BEGIN_MESSAGE_MAP(CBootView,CView)
		ON_WM_CREATE()
		ON_WM_SIZE()
		ON_WM_KILLFOCUS()
		ON_COMMAND(ID_IMAGE_PROTECT,__toggleWriteProtection__)
		ON_WM_DESTROY()
	END_MESSAGE_MAP()








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

	afx_msg int CBootView::OnCreate(LPCREATESTRUCT lpcs){
		// window created
		// - base
		if (CView::OnCreate(lpcs)==-1)
			return -1;
		// - getting Boot Sector data
		WORD bootSectorDataRealLength=0; // initializing just in case the Boot Sector is not found
		IMAGE->GetSectorData(chsBoot,&bootSectorDataRealLength);
		// - creating the Content
		//CCreateContext cc;
		//cc.m_pCurrentDoc=dos->image;
		content=new CSplitterWnd;
			content->CreateStatic(this,1,2,WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS);//WS_CLIPCHILDREN|
			//content->CreateView(0,0,RUNTIME_CLASS(CPropertyGridView),CSize(splitX,0),&cc);
			//const HWND hPropGrid=content->GetDlgItem( content->IdFromRowCol(0,0) )->m_hWnd;
				propGrid.CreateEx( 0, CPropGridCtrl::GetWindowClass(app.m_hInstance), NULL, AFX_WS_DEFAULT_VIEW&~WS_BORDER, 0,0,PROPGRID_WIDTH_DEFAULT,300, content->m_hWnd, (HMENU)content->IdFromRowCol(0,0) );
				content->SetColumnInfo(0,PROPGRID_WIDTH_DEFAULT*Utils::LogicalUnitScaleFactor,0);
			//content->CreateView(0,1,RUNTIME_CLASS(CHexaEditor),CSize(),&cc); // commented out as created manually below
				hexaEditor.Reset( &fBoot, bootSectorDataRealLength, bootSectorDataRealLength );
				hexaEditor.Create( NULL, NULL, AFX_WS_DEFAULT_VIEW&~WS_BORDER|WS_CLIPSIBLINGS, CFrameWnd::rectDefault, content, content->IdFromRowCol(0,1) );
				//hexaEditor.CreateEx( 0, HEXAEDITOR_BASE_CLASS, NULL, AFX_WS_DEFAULT_VIEW&~WS_BORDER, RECT(), NULL, content->IdFromRowCol(0,1), NULL );
		OnSize( SIZE_RESTORED, lpcs->cx, lpcs->cy );
		// - populating the PropertyGrid with values from Boot Sector
		OnUpdate(NULL,0,NULL);
		// - manually setting that none of the Splitter cells is the actual View
		//nop (see OnKillFocus)
		// - currently it's this Boot that's displayed
		pCurrentlyShown=this;
		return 0;
	}

	void CBootView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		// - getting Boot Sector data
		WORD bootSectorDataRealLength=0; // initializing just in case the Boot Sector is not found
		const PSectorData boot=IMAGE->GetSectorData(chsBoot,&bootSectorDataRealLength);
		// - clearing the PropertyGrid
		CPropGridCtrl::RemoveProperty( propGrid.m_hWnd, NULL );
		// - populating the PropertyGrid with values from the Boot Sector (if any found)
		if (boot){
			// Boot Sector found - populating the PropertyGrid
			// . basic parameters from the Boot Sector
			TCommonBootParameters cbp;
			::ZeroMemory(&cbp,sizeof(cbp));
			GetCommonBootParameters(cbp,boot);
			const TMedium::PCProperties props=TMedium::GetProperties(DOS->formatBoot.mediumType);
			const HANDLE hGeometry= cbp.geometryCategory ? CPropGridCtrl::AddCategory(propGrid.m_hWnd,NULL,_T("Geometry")) : 0;
			if (hGeometry){
				if (cbp.chs){
					__pg_showPositiveInteger__( propGrid.m_hWnd, hGeometry, &DOS->formatBoot.nCylinders, NULL, __updateFatAfterChangingCylinderCount__, props->cylinderRange.iMax, _T("Cylinders") );
					__pg_showPositiveInteger__( propGrid.m_hWnd, hGeometry, &DOS->formatBoot.nHeads, CRITICAL_VALUE_SIDES_COUNT, __confirmCriticalValueInBoot__, props->headRange.iMax, _T("Heads") );
					__pg_showPositiveInteger__( propGrid.m_hWnd, hGeometry, &DOS->formatBoot.nSectors, CRITICAL_VALUE_SECTORS_COUNT, __confirmCriticalValueInBoot__, min(props->sectorRange.iMax,DOS->properties->nSectorsOnTrackMax), _T("Sectors/track") );
				}
				if (cbp.pSectorLength){
					static const CPropGridCtrl::TInteger::TUpDownLimits Limits={128,16384};
					CPropGridCtrl::AddProperty(	propGrid.m_hWnd, hGeometry, _T("Sector size"),
												cbp.pSectorLength, sizeof(WORD),
												CPropGridCtrl::TInteger::DefineEditor(Limits,__bootSectorModified__)
											);
				}
			}
			const HANDLE hVolume= cbp.volumeCategory ? CPropGridCtrl::AddCategory(propGrid.m_hWnd,NULL,_T("Volume")) : 0;
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
											CPropGridCtrl::AddProperty(	propGrid.m_hWnd, NULL,
																		_T("Boot sector"), "Not found!", -1,
																		CPropGridCtrl::TString::DefineFixedLengthEditorA()
																	),
											false
										);
		// - repainting the HexaEditor
		hexaEditor.Invalidate();
		// - reflecting write-protection into the look of controls
		if (IMAGE->IsWriteProtected())
			__updateLookOfControls__();
	}

	afx_msg void CBootView::OnSize(UINT nType,int cx,int cy){
		// window size changed
		content->SetWindowPos( NULL, 0,0, cx,cy, SWP_NOZORDER|SWP_NOMOVE );
	}

	afx_msg void CBootView::OnKillFocus(CWnd *newFocus){
		// window lost focus
		// - manually setting that this Boot is still the active View, regardless of the lost focus
		((CFrameWnd *)app.m_pMainWnd)->SetActiveView(this);
	}

	afx_msg void CBootView::OnDestroy(){
		// window destroyed
		// - saving the Splitter's X position for later
		RECT r;
		content->GetDlgItem( content->IdFromRowCol(0,0) )->GetClientRect(&r);
		splitX=r.right;
		// - disposing the Content
		delete content;
		// - base
		CView::OnDestroy();
		// - no Boot is currently being displayed
		pCurrentlyShown=NULL;
	}

	afx_msg void CBootView::__toggleWriteProtection__(){
		// toggles Image's WriteProtection flag
		IMAGE->__toggleWriteProtection__(); // "base"
		__updateLookOfControls__();
	}
	void CBootView::__updateLookOfControls__(){
		CPropGridCtrl::EnableProperty( propGrid.m_hWnd, NULL, !IMAGE->IsWriteProtected() );
		hexaEditor.SetEditable( !IMAGE->IsWriteProtected() );
	}

	void CBootView::OnDraw(CDC *pDC){
		// drawing
	}

	void CBootView::PostNcDestroy(){
		// self-destruction
		//nop (View destroyed by its owner)
	}

	bool CBootView::__isValueBeingEditedInPropertyGrid__() const{
		// True <=> some value is right now being edited in PropertyGrid, otherwise False
		return CPropGridCtrl::IsValueBeingEdited();
	}
