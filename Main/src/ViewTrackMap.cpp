#include "stdafx.h"

	#define INI_TRACKMAP		_T("TrackMap")

	#define VIEW_WIDTH			600
	#define VIEW_PADDING		25
	#define VIEW_HEADER_HEIGHT	(TRACK_HEIGHT+VIEW_PADDING/2)

	#define TRACK_HEIGHT		17
	#define TRACK0_Y			(VIEW_PADDING+VIEW_HEADER_HEIGHT)

	#define SECTOR_HEIGHT		(TRACK_HEIGHT-6)
	#define SECTOR1_X			(VIEW_PADDING+100)
	#define SECTOR_MARGIN		5




	CTrackMapView::CTrackMapView(PDos _dos)
		// ctor
		: tab( IDR_TRACKMAP, IDR_TRACKMAP, ID_CYLINDER, _dos, this )
		, displayType(TDisplayType::STATUS) , showSectorNumbers(false) , highlightBadSectors(false) , iScrollY(0) , scanner(this) {
		::ZeroMemory( rainbowBrushes, sizeof(rainbowBrushes) );
	}

	CTrackMapView::TTrackScanner::TTrackScanner(const CTrackMapView *pvtm)
		// ctor
		: action( __thread__, pvtm, THREAD_PRIORITY_IDLE ) {
	}

	#define WM_TRACK_SCANNED	WM_USER+1

	BEGIN_MESSAGE_MAP(CTrackMapView,CScrollView)
		ON_WM_CREATE()
		ON_WM_VSCROLL()
		ON_WM_MOUSEMOVE()
		ON_WM_DESTROY()
		ON_MESSAGE(WM_TRACK_SCANNED,__drawTrack__)
		ON_COMMAND_RANGE(ID_TRACKMAP_STATUS,ID_TRACKMAP_BAD_DATA,__changeDisplayType__)
			ON_UPDATE_COMMAND_UI_RANGE(ID_TRACKMAP_STATUS,ID_TRACKMAP_BAD_DATA,__changeDisplayType_updateUI__)
		ON_COMMAND(ID_TRACKMAP_NUMBERING,__toggleSectorNumbering__)
			ON_UPDATE_COMMAND_UI(ID_TRACKMAP_NUMBERING,__toggleSectorNumbering_updateUI__)
		ON_COMMAND(ID_TRACKMAP_STATISTICS,__showDiskStatistics__)
	END_MESSAGE_MAP()









	#define DOS		tab.dos
	#define IMAGE	DOS->image

	void CTrackMapView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		// - updating the logical dimensions
		SetScrollSizes(MM_TEXT,CSize( VIEW_WIDTH, Utils::LogicalUnitScaleFactor*(VIEW_PADDING*2+VIEW_HEADER_HEIGHT+IMAGE->GetTrackCount()*TRACK_HEIGHT) ));
		// - base
		CScrollView::OnUpdate(pSender,lHint,pHint);
	}

	BOOL CTrackMapView::OnScroll(UINT nScrollCode, UINT nPos, BOOL bDoScroll){
		// scrolls the View's content in given way
		// - vertical ScrollBar
		SCROLLINFO ysi;
		GetScrollInfo( SB_VERT, &ysi, SIF_POS|SIF_TRACKPOS|SIF_RANGE|SIF_PAGE );
		int iScrollY0=ysi.nPos;
		switch (HIBYTE(nScrollCode)){
			case SB_TOP		: ysi.nPos=0;				break;
			case SB_BOTTOM	: ysi.nPos=INT_MAX;			break;
			case SB_LINEUP	: ysi.nPos-=m_lineDev.cy;	break;
			case SB_LINEDOWN: ysi.nPos+=m_lineDev.cy;	break;
			case SB_PAGEUP	: ysi.nPos-=m_pageDev.cy;	break;
			case SB_PAGEDOWN: ysi.nPos+=m_pageDev.cy;	break;
			case SB_THUMBTRACK:	ysi.nPos=ysi.nTrackPos;	break;
		}
		if (ysi.nPos<0) ysi.nPos=0;
		else if (ysi.nPos>ysi.nMax-ysi.nPage) ysi.nPos=ysi.nMax-ysi.nPage;
		ScrollWindow(	// "base"
						0,
						(iScrollY0-ysi.nPos)//*Utils::GetLogicalUnitScaleFactor(CClientDC(this))
					); 
		SetScrollInfo(SB_VERT,&ysi,TRUE);
		return TRUE;
	}


	static THead __getNumberOfFormattedSidesInImage__(PCImage image){
		// estimates and returns the number of formatted Sides in the Image by observing the count of formatted Sides in zeroth Cylinder
		return image->GetNumberOfFormattedSides(0);
	}

	struct TTrackInfo sealed{
		TCylinder cylinder;	THead head;
		TSector nSectors;
		TSectorId bufferId[(TSector)-1];
		PSectorData bufferSectorData[(TSector)-1];
		WORD bufferLength[(TSector)-1];
	};
	UINT AFX_CDECL CTrackMapView::TTrackScanner::__thread__(PVOID _pBackgroundAction){
		// scanning of Tracks
		const TBackgroundAction *const pAction=(TBackgroundAction *)_pBackgroundAction;
		CTrackMapView *const pvtm=(CTrackMapView *)pAction->fnParams;
		TTrackScanner &rts=pvtm->scanner;
		const PImage image=pvtm->IMAGE;
		const Utils::CByteIdentity sectorIdAndPositionIdentity;
		for( TTrackInfo si; const THead nSides=__getNumberOfFormattedSidesInImage__(image); ){ // "nSides==0" if disk without any Track (e.g. when opening RawImage of zero length, or if opening a corrupted DSK Image)
			// . waiting for request to scan the next Track
			rts.scanNextTrack.Lock();
			// . getting the TrackNumber to scan
			rts.params.criticalSection.Lock();
				const TTrack trackNumber=rts.params.x;
			rts.params.criticalSection.Unlock();
			const div_t d=div(trackNumber,nSides);
			// . scanning the Track to draw its Sector Statuses
			si.cylinder=d.quot, si.head=d.rem;
			//if (pvtm->displayType==TDisplayType::STATUS) // commented out because this scanning always needed
				si.nSectors=image->ScanTrack( si.cylinder, si.head, si.bufferId, si.bufferLength );
			// . scanning the Track to draw its Sector data
			if (pvtm->displayType>=TDisplayType::DATA_OK_ONLY){
				TFdcStatus statuses[(TSector)-1];
				image->GetTrackData( si.cylinder, si.head, si.bufferId, sectorIdAndPositionIdentity, si.nSectors, false, si.bufferSectorData, si.bufferLength, statuses );
				for( TSector n=0; n<si.nSectors; n++ )
					if (pvtm->displayType!=TDisplayType::DATA_ALL && !statuses[n].IsWithoutError())
						si.bufferSectorData[n]=nullptr;
			}
			// . sending scanned information for drawing
			if (::IsWindow(pvtm->m_hWnd)) // TrackMap may not exist if, for instance, switched to another view while still scanning some Track(s)
				pvtm->PostMessage( WM_TRACK_SCANNED, trackNumber, (LPARAM)&si );
		}
		return ERROR_SUCCESS;
	}

	void CTrackMapView::OnPrepareDC(CDC *pDC,CPrintInfo *pInfo){
		//
		// - base
		__super::OnPrepareDC(pDC,pInfo);
		// - scaling
		Utils::ScaleLogicalUnit(*pDC);
	}

	#define SECTOR_LENGTH_FACTOR		3

	static const int Tabs[]={ VIEW_PADDING, VIEW_PADDING+60, SECTOR1_X };

	afx_msg LRESULT CTrackMapView::__drawTrack__(WPARAM trackNumber,LPARAM pTrackInfo){
		// draws scanned Track
		// - drawing
		if (scanner.params.x==(TTrack)trackNumber){
			// received scanned information of expected Track to draw
			CClientDC dc(this);
			OnPrepareDC(&dc);
			// . basic drawing
			::SetBkMode(dc,TRANSPARENT);
			const HGDIOBJ font0=::SelectObject(dc,CRideFont::Std);
				const TTrackInfo &rti=*(TTrackInfo *)pTrackInfo;
				TCHAR buf[16];
				// : drawing Cylinder and Side numbers
				const int y=TRACK0_Y+trackNumber*TRACK_HEIGHT;
				if (const THead head=rti.head)
					::wsprintf(buf,_T("\t\t%d"),head);
				else
					::wsprintf(buf,_T("\t%d\t0"),rti.cylinder);
				::TabbedTextOut( dc, 0,y, buf,-1, 3,Tabs, 0 );
				// : drawing Sectors
				PCSectorId pId=rti.bufferId;
				PCWORD pLength=rti.bufferLength;
				TSector nSectors=rti.nSectors;
				RECT r={ SECTOR1_X, y+(TRACK_HEIGHT-SECTOR_HEIGHT)/2, SECTOR1_X, y+(TRACK_HEIGHT+SECTOR_HEIGHT)/2 };
				const HGDIOBJ hBrush0=::SelectObject(dc,CRideBrush::White);
					if (displayType==TDisplayType::STATUS){
						// drawing Sector Statuses
						CDos::TSectorStatus statuses[(TSector)-1],*ps=statuses;
						DOS->GetSectorStatuses( rti.cylinder, rti.head, nSectors, pId, statuses );
						for( ; nSectors--; r.left=r.right+=SECTOR_MARGIN ){
							r.right+=1+(*pLength++>>SECTOR_LENGTH_FACTOR); // "1+" = to correctly display a zero-length Sector
							const CBrush brush(*ps++);
							const HGDIOBJ hBrush0=::SelectObject(dc,brush);
							dc.Rectangle(&r);
							if (showSectorNumbers) // drawing Sector numbers
								::DrawText( dc, _itot(pId++->sector,buf,10),-1, &r, DT_CENTER|DT_VCENTER|DT_SINGLELINE );
							r.right--; // compensating for correctly displaying zero-length Sectors
						}
					}else
						// drawing Sector data
						for( const PCSectorData *pData=rti.bufferSectorData; nSectors--; r.left=r.right+=SECTOR_MARGIN ){
							const WORD w=*pLength++>>SECTOR_LENGTH_FACTOR;
							if (PCBYTE sample=(PCBYTE)*pData++){
								// Sector found - drawing its data
								RECT rcSample=r;
									rcSample.right=rcSample.left+2;
								for( WORD n=w; n--; sample+=(1<<SECTOR_LENGTH_FACTOR),rcSample.left++,rcSample.right++ )
									::FillRect( dc, &rcSample, rainbowBrushes[*sample] );
								r.right+=1+w; // "1+" = to correctly display a zero-length Sector
								::FrameRect( dc, &r, CRideBrush::Black );
								//dc.Rectangle(&r);
							}else{
								// Sector not found - drawing crossing-out
								const HGDIOBJ hPen0=::SelectObject(dc,CRidePen::RedHairline);
									r.right+=1+w; // "1+" = to correctly display a zero-length Sector
									dc.Rectangle(&r);
									::MoveToEx( dc, r.left, r.top, nullptr );
									::LineTo( dc, r.right, r.bottom );
									::MoveToEx( dc, r.left, r.bottom, nullptr );
									::LineTo( dc, r.right, r.top);
								::SelectObject(dc,hPen0);
							}
							r.right--; // compensating for correctly displaying zero-length Sectors
						}
				::SelectObject(dc,hBrush0);
			::SelectObject(dc,font0);
			// . drawing current selection in FileManager
			if (const CFileManagerView *const pFileManager=DOS->pFileManager){
				::SelectObject(dc,::GetStockObject(NULL_BRUSH));
				const LOGPEN pen={ PS_SOLID, 2,2, ::GetSysColor(COLOR_ACTIVECAPTION) };
				const HGDIOBJ hPen0=::SelectObject( dc, ::CreatePenIndirect(&pen) );
					for( POSITION pos=pFileManager->GetFirstSelectedFilePosition(); pos; ){
						const CDos::CFatPath fatPath( DOS, pFileManager->GetNextSelectedFile(pos) );
						CDos::CFatPath::PCItem item; DWORD n;
						if (!fatPath.GetItems(item,n)) // FatPath valid
							for( const THead nSides=__getNumberOfFormattedSidesInImage__(IMAGE); n--; item++ )
								if (trackNumber==item->chs.GetTrackNumber(nSides)){
									// this Sector (in currently drawn Track) belongs to one of selected Files
									PCSectorId pRefId=&item->chs.sectorId, bufferId=rti.bufferId;
									PCWORD pw=rti.bufferLength;
									r.left=SECTOR1_X;
									for( TSector nSectors=rti.nSectors; nSectors--; r.left+=(*pw++>>SECTOR_LENGTH_FACTOR)+SECTOR_MARGIN )
										if (*pRefId==*bufferId++)
											break;
									r.right=r.left+1+(*pw>>SECTOR_LENGTH_FACTOR); // "1+" = to correctly display a zero-length Sector
									dc.Rectangle(&r);
								}
					}
				::DeleteObject( ::SelectObject(dc,hPen0) );
			}
			// . next Track
			scanner.params.criticalSection.Lock();
				scanner.params.x++;
			scanner.params.criticalSection.Unlock();
		}
		// - if not all requested Tracks scanned yet, proceeding with scanning the next Track
		if (scanner.params.x<scanner.params.z)
			scanner.scanNextTrack.SetEvent();
		return 0;
	}

	void CTrackMapView::OnDraw(CDC *pDC){
		// drawing the disk TrackMap
		pDC->SetBkMode(TRANSPARENT);
		// - drawing TrackMap header
		const HDC dc=*pDC;
		const HGDIOBJ font0=::SelectObject(dc,CRideFont::StdBold);
			::TabbedTextOut( dc, 0,VIEW_PADDING, _T("\tCylinder\tHead\tSectors"),-1, 3,Tabs, 0 );
		::SelectObject(dc,font0);
		// - determining the range of Tracks to scan
		const int iScrollY=GetScrollPos(SB_VERT)/Utils::LogicalUnitScaleFactor;
		RECT r;
		GetClientRect(&r);
		const TTrack nTracks=IMAGE->GetTrackCount();
		scanner.params.criticalSection.Lock();
			scanner.params.a=max( 0, (iScrollY-TRACK0_Y)/TRACK_HEIGHT );
			scanner.params.z=min( nTracks, max( 0, (iScrollY+r.bottom-r.top-TRACK0_Y+TRACK_HEIGHT-1)/TRACK_HEIGHT ) );
			scanner.params.x=scanner.params.a;
		scanner.params.criticalSection.Unlock();
		// - launching the Scanner of Tracks
		scanner.scanNextTrack.SetEvent();
	}

	void CTrackMapView::PostNcDestroy(){
		// self-destruction
		//nop (View destroyed by its owner)
	}

	void CTrackMapView::__updateStatusBarIfCursorOutsideAnySector__() const{
		// updates the MainWindow's StatusBar when cursor isn't over any Sector
		CMainWindow::__setStatusBarText__(nullptr);
	}


	afx_msg int CTrackMapView::OnCreate(LPCREATESTRUCT lpcs){
		// window created
		// - base
		if (CScrollView::OnCreate(lpcs)==-1) return -1;
		OnInitialUpdate(); // because isn't called automatically by OnCreate; calls SetScrollSizes via OnUpdate
		// - recovering the scroll position
		SetScrollPos( SB_VERT, iScrollY, FALSE );
		// - updating the MainWindow's StatusBar
		__updateStatusBarIfCursorOutsideAnySector__();
		// - creating RainbowBrushes, see "Using out-of-phase sine waves to make rainbows" at http://krazydad.com/tutorials/makecolors.php
		for( int t=TRACK_MAP_COLORS_COUNT; t--; )
			rainbowBrushes[t]=(HBRUSH)CBrush( RGB(	128 + 127*sin( (float)2*M_PI*t/TRACK_MAP_COLORS_COUNT ),
													128 + 127*sin( (float)2*M_PI/(3*TRACK_MAP_COLORS_COUNT) * (3*t+TRACK_MAP_COLORS_COUNT) ),
													128 + 127*sin( (float)2*M_PI/(3*TRACK_MAP_COLORS_COUNT) * (3*t+2*TRACK_MAP_COLORS_COUNT) )
												)
											).Detach();
		// - the FillerByte is always represented in stock white
		const BYTE fillerByte=DOS->properties->sectorFillerByte;
		::DeleteObject(rainbowBrushes[fillerByte]);
		rainbowBrushes[fillerByte]=(HBRUSH)::GetStockObject(WHITE_BRUSH);
		// - launching the Scanner of Tracks (if not yet launched)
		scanner.action.Resume();
		return 0;
	}

	afx_msg void CTrackMapView::OnMouseMove(UINT nFlags,CPoint point){
		// cursor moved over this view
		CClientDC dc(this);
		OnPrepareDC(&dc);
		dc.DPtoLP(&point);
		point.y-=TRACK0_Y;
		const PCImage image=IMAGE;
		if (point.y>=0 && point.y<image->GetTrackCount()*TRACK_HEIGHT){
			// cursor over a Track
			const TTrack track=point.y/TRACK_HEIGHT;
			const THead nSides=__getNumberOfFormattedSidesInImage__(image);
			point.x-=SECTOR1_X;
			const div_t d=div((int)track,nSides);
			TSectorId bufferId[(TSector)-1],*pId=bufferId;
			WORD bufferLength[(TSector)-1],*pLength=bufferLength;
			TSector nSectors=image->ScanTrack( d.quot, d.rem, bufferId, bufferLength );
			CDos::TSectorStatus statuses[(TSector)-1],*pStatus=statuses;
			DOS->GetSectorStatuses( d.quot, d.rem, nSectors, bufferId, statuses );
			for( int xL=0,xR=0; nSectors--; pStatus++,pId++ ){
				xR+=*pLength++>>SECTOR_LENGTH_FACTOR;
				if (point.x>=xL && point.x<=xR){
					// cursor over a Sector
					TCHAR buf[40],tmp[30];
					::wsprintf(buf,_T("Tr%d, %s: "),track,pId->ToString(tmp));
					switch (*pStatus){
						case CDos::TSectorStatus::SYSTEM	: ::lstrcat(buf,_T("System")); break;
						case CDos::TSectorStatus::UNAVAILABLE: ::lstrcat(buf,_T("Unavailable")); break;
						case CDos::TSectorStatus::SKIPPED	: ::lstrcat(buf,_T("Skipped")); break;
						case CDos::TSectorStatus::BAD		: ::lstrcat(buf,_T("Bad")); break;
						case CDos::TSectorStatus::OCCUPIED	: ::lstrcat(buf,_T("Occupied")); break;
						case CDos::TSectorStatus::RESERVED	: ::lstrcat(buf,_T("Reserved")); break;
						case CDos::TSectorStatus::EMPTY		: ::lstrcat(buf,_T("Empty")); break;
						default								: ::lstrcat(buf,_T("Unknown")); break;
					}
					CMainWindow::__setStatusBarText__(buf);
					return;
				}
				xL=xR+=SECTOR_MARGIN;
			}
		}
		__updateStatusBarIfCursorOutsideAnySector__();
	}

	afx_msg void CTrackMapView::OnDestroy(){
		// window destroyed
		// - saving scrolling position for later
		iScrollY=GetScrollPos(SB_VERT);
		// - disposing the RainbowPens (assuming that the FillerByte color is a stock object)
		for( int t=TRACK_MAP_COLORS_COUNT; t--; ::DeleteObject(rainbowBrushes[t]) );
		// - base
		CScrollView::OnDestroy();
	}

	afx_msg void CTrackMapView::__changeDisplayType__(UINT id){
		// DisplayType changed
		displayType=(TDisplayType)id;
		Invalidate(FALSE);
	}
	afx_msg void CTrackMapView::__changeDisplayType_updateUI__(CCmdUI *pCmdUI) const{
		// projecting DisplayType into UI
		pCmdUI->SetRadio( displayType==pCmdUI->m_nID );
	}

	afx_msg void CTrackMapView::__toggleSectorNumbering__(){
		// commanded toggling of ShowingSectorNumbers
		showSectorNumbers=!showSectorNumbers;
		Invalidate(TRUE);
	}
	afx_msg void CTrackMapView::__toggleSectorNumbering_updateUI__(CCmdUI *pCmdUI) const{
		// projecting SectorNumbering into UI
		pCmdUI->SetCheck(showSectorNumbers);
		pCmdUI->Enable(displayType==TDisplayType::STATUS);
	}


	struct TStatisticParams sealed{
		const CDos *const dos;
		TTrack nTracksFormatted;
		struct{
			UINT nTotally,nSystem,nBad,nOccupied,nReserved,nInaccessible,nUnknown,nFree;
		} sectors;

		TStatisticParams(const CDos *dos)
			: dos(dos) , nTracksFormatted(0) {
			::ZeroMemory(&sectors,sizeof(sectors));
		}
	};
	static UINT AFX_CDECL __trackStatistics_thread__(PVOID _pCancelableAction){
		// creates and shows statistics on Tracks and their Sectors in current disk
		const TBackgroundActionCancelable *const pAction=(TBackgroundActionCancelable *)_pCancelableAction;
		TStatisticParams &rsp=*(TStatisticParams *)pAction->fnParams;
		const PCImage image=rsp.dos->image;
		for( TCylinder nCylinders=image->GetCylinderCount(),cyl=0; cyl<nCylinders; pAction->UpdateProgress(++cyl) )
			for( THead nHeads=image->GetNumberOfFormattedSides(cyl),head=0; head<nHeads; head++,rsp.nTracksFormatted++ ){
				if (!pAction->bContinue) return ERROR_CANCELLED;
				TSectorId bufferId[(TSector)-1];
				WORD bufferLength[(TSector)-1];
				TSector nSectors=image->ScanTrack(cyl,head,bufferId,bufferLength);
				rsp.sectors.nTotally+=nSectors;
				CDos::TSectorStatus statuses[(TSector)-1];
				for( rsp.dos->GetSectorStatuses(cyl,head,nSectors,bufferId,statuses); nSectors--; )
					switch (statuses[nSectors]){
						case CDos::TSectorStatus::SYSTEM	:rsp.sectors.nSystem++; break;
						case CDos::TSectorStatus::BAD		:rsp.sectors.nBad++; break;
						case CDos::TSectorStatus::OCCUPIED	:rsp.sectors.nOccupied++; break;
						case CDos::TSectorStatus::RESERVED	:rsp.sectors.nReserved++; break;
						case CDos::TSectorStatus::EMPTY		:rsp.sectors.nFree++; break;
						case CDos::TSectorStatus::SKIPPED	:
						case CDos::TSectorStatus::UNAVAILABLE:rsp.sectors.nInaccessible++; break;
						case CDos::TSectorStatus::UNKNOWN	:rsp.sectors.nUnknown++; break;
						#ifdef _DEBUG
							default: ASSERT(FALSE); // unknown Status
						#endif
					}
			}
		return ERROR_SUCCESS;
	}
	afx_msg void CTrackMapView::__showDiskStatistics__() const{
		// shows statistics on Tracks and their Sectors in current disk
		// - collecting statistics on Tracks and their Sectors
		TStatisticParams sp(DOS);
		if (const TStdWinError err=TBackgroundActionCancelable(__trackStatistics_thread__,&sp,THREAD_PRIORITY_BELOW_NORMAL).CarryOut(IMAGE->GetCylinderCount()))
			return Utils::Information(_T("Cannot create statistics"),err);
		// - showing collected statistics
		TCHAR buf[1000];
		::wsprintf( buf, _T("TRACK STATISTICS\n\n\nTotal number of tracks: %d\n- formatted: %d\n\nTotal number of sectors: %d\n- system: %d\n- erroneous: %d\n- occupied: %d\n- reserved: %d\n- unreachable: %d\n- unknown: %d\n- empty: %d\n"), IMAGE->GetTrackCount(), sp.nTracksFormatted, sp.sectors );
		Utils::Information(buf);
	}
