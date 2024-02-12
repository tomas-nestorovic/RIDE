#include "stdafx.h"

	#define INI_TRACKMAP		_T("TrackMap")

	#define INI_FILE_SELECTION_COLOR	_T("fscol")
	#define INI_MSG_CAPABILITIES _T("caps")

	#define VIEW_PADDING		25
	#define VIEW_HEADER_HEIGHT	(TRACK_HEIGHT+VIEW_PADDING/2)

	#define TRACK_HEIGHT		17
	#define TRACK0_Y			(VIEW_PADDING+VIEW_HEADER_HEIGHT)
	#define TRACK_PADDING_RIGHT	100

	#define SECTOR_HEIGHT		(TRACK_HEIGHT-6)
	#define SECTOR1_X			(VIEW_PADDING+100)
	#define SECTOR_MARGIN		5




	#define IMAGE	tab.image
	#define DOS		IMAGE->dos

	CTrackMapView::CTrackMapView(PImage image)
		// ctor
		// - initialization
		: tab( IDR_TRACKMAP, IDR_TRACKMAP, ID_CYLINDER, image, this )
		, displayType(TDisplayType::STATUS) , showSectorNumbers(false) , showTimed(false) , fitLongestTrackInWindow(false) , showSelectedFiles(true) , iScrollX(0) , iScrollY(0) , scanner(this)
		, fileSelectionColor( app.GetProfileInt(INI_TRACKMAP,INI_FILE_SELECTION_COLOR,::GetSysColor(COLOR_ACTIVECAPTION)) )
		, longestTrack(0,0) , longestTrackNanoseconds(0)
		, informOnCapabilities(true)
		, zoomLengthFactor(3) {
		::ZeroMemory( rainbowBrushes, sizeof(rainbowBrushes) );
		// - launching the Scanner of Tracks
		scanner.action.Resume();
	}

	CTrackMapView::TTrackScanner::TTrackScanner(const CTrackMapView *pvtm)
		// ctor
		: action( Thread, pvtm, THREAD_PRIORITY_IDLE ) {
		params.nHeads=-1; // Head count undetermined yet
	}

	#define WM_TRACK_SCANNED	WM_USER+1

	BEGIN_MESSAGE_MAP(CTrackMapView,CScrollView)
		ON_WM_CREATE()
		ON_WM_SIZE()
		ON_WM_HSCROLL()
		ON_WM_VSCROLL()
		ON_WM_MOUSEMOVE()
		ON_WM_LBUTTONUP()
		ON_WM_RBUTTONUP()
		ON_WM_MOUSEWHEEL()
		ON_WM_DESTROY()
		ON_MESSAGE(WM_TRACK_SCANNED,DrawTrack)
		ON_COMMAND_RANGE(ID_TRACKMAP_STATUS,ID_TRACKMAP_BAD_DATA,ChangeDisplayType)
			ON_UPDATE_COMMAND_UI_RANGE(ID_TRACKMAP_STATUS,ID_TRACKMAP_BAD_DATA,__changeDisplayType_updateUI__)
		ON_COMMAND(ID_TRACKMAP_NUMBERING,ToggleSectorNumbering)
			ON_UPDATE_COMMAND_UI(ID_TRACKMAP_NUMBERING,__toggleSectorNumbering_updateUI__)
		ON_COMMAND(ID_TRACKMAP_TIMING,ToggleTiming)
			ON_UPDATE_COMMAND_UI(ID_TRACKMAP_TIMING,__toggleTiming_updateUI__)
		ON_COMMAND(ID_ZOOM_IN,ZoomIn)
			ON_UPDATE_COMMAND_UI(ID_ZOOM_IN,__zoomIn_updateUI__)
		ON_COMMAND(ID_ZOOM_OUT,ZoomOut)
			ON_UPDATE_COMMAND_UI(ID_ZOOM_OUT,__zoomOut_updateUI__)
		ON_COMMAND(ID_ZOOM_FIT,ZoomFitWidth)
			ON_UPDATE_COMMAND_UI(ID_ZOOM_FIT,__zoomFitWidth_updateUI__)
		ON_COMMAND(ID_FILE,ShowSelectedFiles)
			ON_UPDATE_COMMAND_UI(ID_FILE,__showSelectedFiles_updateUI__)
		ON_COMMAND(ID_COLOR,ChangeFileSelectionColor)
		ON_COMMAND(ID_TRACKMAP_STATISTICS,ShowDiskStatistics)
		ON_COMMAND(ID_REFRESH,RefreshDisplay)
	END_MESSAGE_MAP()

	CTrackMapView::~CTrackMapView(){
		// dtor
		// - saving settings
		app.WriteProfileInt( INI_TRACKMAP, INI_FILE_SELECTION_COLOR, fileSelectionColor );
		// - waiting for the Scanner to finish properly
{		EXCLUSIVELY_LOCK(scanner.params);
			scanner.params.nHeads=0; // 0 = terminate the Scanner
			scanner.scanNextTrack.SetEvent();
}
		::WaitForSingleObject( scanner.action, INFINITE );
	}








	#define ZOOM_FACTOR_MAX	8

	#define CAN_ZOOM_IN		(zoomLengthFactor>0)
	#define CAN_ZOOM_OUT	(zoomLengthFactor<ZOOM_FACTOR_MAX)

	inline
	CTrackMapView::TTrackLength CTrackMapView::TTrackLength::FromTime(TLogTime nNanosecondsTotal,TLogTime nNanosecondsPerByte){
		return TTrackLength( 0, nNanosecondsTotal/nNanosecondsPerByte );
	}

	inline
	CTrackMapView::TTrackLength::TTrackLength(TSector nSectors,int nBytes)
		// ctor
		: nSectors(nSectors) , nBytes(nBytes) {
	}

	inline
	int CTrackMapView::TTrackLength::GetUnitCount(BYTE zoomFactor) const{
		return	Utils::CTimeline(nBytes,1,zoomFactor).GetUnitCount() + nSectors*SECTOR_MARGIN;
	}

	BYTE CTrackMapView::TTrackLength::GetZoomFactorToFitWidth(int pixelWidth) const{
		BYTE zoomLengthFactor=0;
		const int nUnits=pixelWidth/Utils::LogicalUnitScaleFactor-SECTOR1_X;
		while (GetUnitCount(zoomLengthFactor)>nUnits && CAN_ZOOM_OUT)
			zoomLengthFactor++;
		return zoomLengthFactor;
	}

	inline
	bool CTrackMapView::TTrackLength::operator<(const TTrackLength &r) const{
		return	GetUnitCount(0) < r.GetUnitCount(0);
	}








	void CTrackMapView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		// - updating the logical dimensions
		UpdateLogicalDimensions();
		// - base
		__super::OnUpdate(pSender,lHint,pHint);
	}

	void CTrackMapView::UpdateLogicalDimensions(){
		// adjusts logical dimensions to accommodate the LongestTrack in the Image horizontally, and all Tracks in the Image vertically
		SetScrollSizes(
			MM_TEXT,
			CSize(
				Utils::LogicalUnitScaleFactor*(
					showTimed
					? SECTOR1_X + TTrackLength::FromTime(longestTrackNanoseconds,IMAGE->EstimateNanosecondsPerOneByte()).GetUnitCount(zoomLengthFactor) + SECTOR_MARGIN
					: SECTOR1_X + longestTrack.GetUnitCount(zoomLengthFactor)
				),
				Utils::LogicalUnitScaleFactor*(
					VIEW_PADDING*2+VIEW_HEADER_HEIGHT+IMAGE->GetTrackCount()*TRACK_HEIGHT
				)
			)
		);
	}

	BOOL CTrackMapView::OnScroll(UINT nScrollCode, UINT nPos, BOOL bDoScroll){
		// scrolls the View's content in given way
		SCROLLINFO si={ sizeof(si) };
		// - horizontal ScrollBar
		GetScrollInfo( SB_HORZ, &si, SIF_POS|SIF_TRACKPOS|SIF_RANGE|SIF_PAGE );
		int iScroll0=si.nPos;
		switch (LOBYTE(nScrollCode)){
			case SB_LEFT		: si.nPos=0;			break;
			case SB_RIGHT		: si.nPos=INT_MAX;		break;
			case SB_LINELEFT	: si.nPos-=m_lineDev.cx;break;
			case SB_LINERIGHT	: si.nPos+=m_lineDev.cx;break;
			case SB_PAGELEFT	: si.nPos-=m_pageDev.cx;break;
			case SB_PAGERIGHT	: si.nPos+=m_pageDev.cx;break;
			case SB_THUMBTRACK	: si.nPos=si.nTrackPos;	break;
		}
		if (si.nPos<0) si.nPos=0;
		else if (si.nPos>si.nMax-si.nPage) si.nPos=si.nMax-si.nPage;
		ScrollWindow(	// "base"
						(iScroll0-si.nPos),//*Utils::GetLogicalUnitScaleFactor(CClientDC(this))
						0
					); 
		SetScrollInfo(SB_HORZ,&si,TRUE);
		// - vertical ScrollBar
		GetScrollInfo( SB_VERT, &si, SIF_POS|SIF_TRACKPOS|SIF_RANGE|SIF_PAGE );
		iScroll0=si.nPos;
		switch (HIBYTE(nScrollCode)){
			case SB_TOP		: si.nPos=0;				break;
			case SB_BOTTOM	: si.nPos=INT_MAX;			break;
			case SB_LINEUP	: si.nPos-=m_lineDev.cy;	break;
			case SB_LINEDOWN: si.nPos+=m_lineDev.cy;	break;
			case SB_PAGEUP	: si.nPos-=m_pageDev.cy;	break;
			case SB_PAGEDOWN: si.nPos+=m_pageDev.cy;	break;
			case SB_THUMBTRACK:	si.nPos=si.nTrackPos;	break;
		}
		if (si.nPos<0) si.nPos=0;
		else if (si.nPos>si.nMax-si.nPage) si.nPos=si.nMax-si.nPage;
		ScrollWindow(	// "base"
						0,
						(iScroll0-si.nPos)//*Utils::GetLogicalUnitScaleFactor(CClientDC(this))
					); 
		SetScrollInfo(SB_VERT,&si,TRUE);
		return TRUE;
	}


	struct TTrackInfo sealed{
		TCylinder cylinder;	THead head;
		TSector nSectors;
		TSectorId bufferId[(TSector)-1];
		PSectorData bufferSectorData[(TSector)-1];
		WORD bufferLength[(TSector)-1];
		TLogTime bufferStartNanoseconds[(TSector)-1];
	};
	UINT AFX_CDECL CTrackMapView::TTrackScanner::Thread(PVOID _pBackgroundAction){
		// scanning of Tracks
		const PCBackgroundAction pAction=(PCBackgroundAction)_pBackgroundAction;
		CTrackMapView *const pvtm=(CTrackMapView *)pAction->GetParams();
		TTrackScanner &rts=pvtm->scanner;
		const PImage image=pvtm->IMAGE;
		const Utils::CByteIdentity sectorIdAndPositionIdentity;
		for( TTrackInfo ti; true; ){
			// . waiting for request to scan the next Track
			rts.scanNextTrack.Lock();
			// . getting the TrackNumber to scan
			TTrackInfo tmp;
			TTrack trackNumber;
	{		EXCLUSIVELY_LOCK(rts.params);
			const THead nHeads=rts.params.nHeads;
			if (nHeads==0) // "nHeads==0" if disk without any Track (e.g. when opening RawImage of zero length, or if opening a corrupted DSK Image)
				break;
			trackNumber=rts.params.x;
			const div_t d=div(trackNumber,nHeads);
			tmp.cylinder=d.quot, tmp.head=d.rem; // syncing with Params due to comparison in drawing routine
	}		// . scanning the Track to draw its Sector Statuses
			PREVENT_FROM_DESTRUCTION(*image);
			if (!::IsWindow(pvtm->m_hWnd)) // TrackMap may not exist if, for instance, switched to another view while still scanning some Track(s)
				continue;
			//if (pvtm->displayType==TDisplayType::STATUS) // commented out because this scanning always needed
			tmp.nSectors=image->ScanTrack( tmp.cylinder, tmp.head, nullptr, tmp.bufferId, tmp.bufferLength, tmp.bufferStartNanoseconds );
			// . scanning the Track to draw its Sector data
			if (pvtm->displayType>=TDisplayType::DATA_OK_ONLY){
				TFdcStatus statuses[(TSector)-1];
				if (!::IsWindow(pvtm->m_hWnd)) // TrackMap may not exist if, for instance, switched to another view while still scanning some Track(s)
					continue;
				TLogTime tDataStarts[(TSector)-1];
				image->GetTrackData( tmp.cylinder, tmp.head, Revolution::CURRENT, tmp.bufferId, sectorIdAndPositionIdentity, tmp.nSectors, tmp.bufferSectorData, tmp.bufferLength, statuses, tDataStarts );
				for( TSector n=0; n<tmp.nSectors; n++ )
					if (pvtm->displayType!=TDisplayType::DATA_ALL && !statuses[n].IsWithoutError())
						tmp.bufferSectorData[n]=nullptr;
			}
			// . sending scanned information for drawing
			EXCLUSIVELY_LOCK(rts.params);
			if (trackNumber!=rts.params.x) // did anything change since we last owned the Params?
				continue;
			if (::IsWindow(pvtm->m_hWnd)) // TrackMap may not exist if, for instance, switched to another view while still scanning some Track(s)
				pvtm->PostMessage( WM_TRACK_SCANNED, 0, (LPARAM)::memcpy(&ti,&tmp,sizeof(tmp)) );
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

	void CTrackMapView::TimesToPixels(TSector nSectors,PLogTime pInOutBuffer,PCWORD pInSectorLengths) const{
		// converts times (in nanoseconds) in Buffer to pixels
		if (showTimed){
			const TLogTime nNanosecondsPerByte=IMAGE->EstimateNanosecondsPerOneByte();
			for( TSector s=0; s<nSectors; s++ )
				pInOutBuffer[s] =	SECTOR1_X + (pInOutBuffer[s]/nNanosecondsPerByte>>zoomLengthFactor);
		}else
			for( TSector s=0; s<nSectors; s++ )
				pInOutBuffer[s] =	s>0
									? pInOutBuffer[s-1]+(pInSectorLengths[s-1]>>zoomLengthFactor)+SECTOR_MARGIN
									: SECTOR1_X;
	}

	static constexpr int Tabs[]={ VIEW_PADDING, VIEW_PADDING+60, SECTOR1_X };

	afx_msg LRESULT CTrackMapView::DrawTrack(WPARAM,LPARAM pTrackInfo){
		// draws scanned Track
		// - adjusting logical dimensions to accommodate the LongestTrack
		const TTrackInfo &rti=*(TTrackInfo *)pTrackInfo;
		int nBytesOnTrack=0;
		for( TSector s=rti.nSectors; s>0; nBytesOnTrack+=rti.bufferLength[--s] );
		bool outdated=false;
		const TTrackLength tmp( rti.nSectors, nBytesOnTrack );
		if (longestTrack<tmp){
			longestTrack=tmp;
			outdated|=!showTimed;
		}
		const TLogTime nNanosecondsPerByte=IMAGE->EstimateNanosecondsPerOneByte();
		const TLogTime nNanosecondsOnTrack =rti.nSectors>0
											? rti.bufferStartNanoseconds[rti.nSectors-1]+rti.bufferLength[rti.nSectors-1]*nNanosecondsPerByte
											: 0;
		if (longestTrackNanoseconds<nNanosecondsOnTrack){
			longestTrackNanoseconds=nNanosecondsOnTrack;
			outdated|=showTimed;
		}
		if (outdated)
			if (fitLongestTrackInWindow){
				CRect rc;
				GetClientRect(&rc);
				zoomLengthFactor =	showTimed
									? TTrackLength::FromTime(longestTrackNanoseconds,nNanosecondsPerByte).GetZoomFactorToFitWidth(rc.Width())
									: longestTrack.GetZoomFactorToFitWidth(rc.Width());
				Invalidate();
				return 0;
			}else
				UpdateLogicalDimensions();
		// - drawing
		scanner.params.locker.Lock();
			const THead nHeads=scanner.params.nHeads;
			const TTrack trackNumber=rti.cylinder*nHeads+rti.head;
			const bool expectedTrackReceived=scanner.params.x==trackNumber;
		scanner.params.locker.Unlock();
		if (expectedTrackReceived){
			// received scanned information of expected Track to draw
			CClientDC dc(this);
			OnPrepareDC(&dc);
			// . basic drawing
			CRect rc;
			GetClientRect(&rc);
			::SetBkMode(dc,TRANSPARENT);
			const HGDIOBJ font0=::SelectObject(dc,Utils::CRideFont::Std);
				TCHAR buf[16];
				// : drawing Cylinder and Side numbers
				const int y=TRACK0_Y+trackNumber*TRACK_HEIGHT;
				if (const THead head=rti.head)
					::wsprintf(buf,_T("\t\t%d"),head);
				else
					::wsprintf(buf,_T("\t%d\t0"),rti.cylinder);
				::TabbedTextOut( dc, 0,y, buf,-1, 3,Tabs, 0 );
				// : drawing Sectors
				iScrollX=GetScrollPos(SB_HORZ)/Utils::LogicalUnitScaleFactor;
				int sectorStartPixels[(TSector)-1];
				TimesToPixels( rti.nSectors, (PINT)::memcpy(sectorStartPixels,rti.bufferStartNanoseconds,rti.nSectors*sizeof(int)), rti.bufferLength );
				RECT r={ SECTOR1_X, y+(TRACK_HEIGHT-SECTOR_HEIGHT)/2, SECTOR1_X, y+(TRACK_HEIGHT+SECTOR_HEIGHT)/2 };
				const HGDIOBJ hBrush0=::SelectObject(dc,Utils::CRideBrush::White);
					if (displayType==TDisplayType::STATUS){
						// drawing Sector Statuses
						CDos::TSectorStatus statuses[(TSector)-1];
						DOS->GetSectorStatuses( rti.cylinder, rti.head, rti.nSectors, rti.bufferId, statuses );
						for( TSector s=0; s<rti.nSectors; s++ ){
							r.left=sectorStartPixels[s];
							r.right=r.left+1+(rti.bufferLength[s]>>zoomLengthFactor); // "1+" = to correctly display a zero-length Sector
							if (iScrollX<r.right || r.left<iScrollX+rc.Width()){
								// Sector in horizontally visible part of the TrackMap
								const CBrush brush(statuses[s]);
								const HGDIOBJ hBrush0=::SelectObject(dc,brush);
									dc.Rectangle(&r);
									if (showSectorNumbers) // drawing Sector numbers
										::DrawText( dc, _itot(rti.bufferId[s].sector,buf,10),-1, &r, DT_CENTER|DT_VCENTER|DT_SINGLELINE );
								::SelectObject(dc,hBrush0);
							}
						}
					}else
						// drawing Sector data
						for( TSector s=0; s<rti.nSectors; s++ ){
							r.left=sectorStartPixels[s];
							r.right=r.left+1+(rti.bufferLength[s]>>zoomLengthFactor); // "1+" = to correctly display a zero-length Sector
							WORD w=rti.bufferLength[s]>>zoomLengthFactor;
							if (r.right+w<=iScrollX || iScrollX+rc.Width()<=r.left)
								// Sector out of horizontally visible part of the TrackMap
								continue;
							if (PCBYTE sample=(PCBYTE)rti.bufferSectorData[s]){
								// Sector found - drawing its data
								RECT rcSample=r;
								for( rcSample.right=rcSample.left+2; w--; sample+=(1<<zoomLengthFactor),rcSample.left++,rcSample.right++ )
									::FillRect( dc, &rcSample, rainbowBrushes[*sample] );
								::SelectObject( dc, ::GetStockObject(NULL_BRUSH) );
								dc.Rectangle(&r);
							}else{
								// Sector not found - drawing crossing-out
								const HGDIOBJ hPen0=::SelectObject(dc,Utils::CRidePen::RedHairline);
									::SelectObject( dc, Utils::CRideBrush::White );
									dc.Rectangle(&r);
									::MoveToEx( dc, r.left, r.top, nullptr );
									::LineTo( dc, r.right, r.bottom );
									::MoveToEx( dc, r.left, r.bottom, nullptr );
									::LineTo( dc, r.right, r.top);
								::SelectObject(dc,hPen0);
							}
						}
				::SelectObject(dc,hBrush0);
			::SelectObject(dc,font0);
			// . drawing current selection in FileManager
			if (showSelectedFiles)
				if (const CFileManagerView *const pFileManager=DOS->pFileManager){
					::SelectObject(dc,::GetStockObject(NULL_BRUSH));
					const LOGPEN pen={ PS_SOLID, 2,2, fileSelectionColor };
					const HGDIOBJ hPen0=::SelectObject( dc, ::CreatePenIndirect(&pen) );
						for( POSITION pos=pFileManager->GetFirstSelectedFilePosition(); pos; ){
							const CDos::CFatPath fatPath( DOS, pFileManager->GetNextSelectedFile(pos) );
							CDos::CFatPath::PCItem item; DWORD n;
							if (!fatPath.GetItems(item,n)) // FatPath valid
								for( ; n--; item++ )
									if (trackNumber==item->chs.GetTrackNumber(nHeads)){
										// this Sector (in currently drawn Track) belongs to one of selected Files
										TSector s=0;
										for( PCSectorId pRefId=&item->chs.sectorId; s<rti.nSectors; s++ )
											if (*pRefId==rti.bufferId[s])
												break;
										r.left=sectorStartPixels[s];
										r.right=r.left+1+(rti.bufferLength[s]>>zoomLengthFactor); // "1+" = to correctly display a zero-length Sector
										dc.Rectangle(&r);
									}
						}
					::DeleteObject( ::SelectObject(dc,hPen0) );
				}
			// . next Track
			EXCLUSIVELY_LOCK(scanner.params);
				scanner.params.x++;
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
		const HGDIOBJ font0=::SelectObject(dc,Utils::CRideFont::StdBold);
			::TabbedTextOut( dc, 0,VIEW_PADDING, _T("\tCylinder\tHead"),-1, 2,Tabs, 0 );
			if (showTimed){
				const Utils::CRideFont &rFont=Utils::CRideFont::StdBold;
				CPoint newOrg=pDC->GetViewportOrg();
				newOrg.Offset( Utils::LogicalUnitScaleFactor*SECTOR1_X, VIEW_PADDING+rFont.charHeight );
				const POINT oldOrg=pDC->SetViewportOrg(newOrg);
					Utils::CTimeline( longestTrackNanoseconds, IMAGE->EstimateNanosecondsPerOneByte(), zoomLengthFactor ).Draw( *pDC, rFont );
				pDC->SetViewportOrg(oldOrg);
			}else
				::TabbedTextOut( dc, 0,VIEW_PADDING, _T("\t\t\tSectors"),-1, 3,Tabs, 0 );
		::SelectObject(dc,font0);
		// - determining the range of Tracks to scan
		const int iScrollY=GetScrollPos(SB_VERT)/Utils::LogicalUnitScaleFactor;
		CRect r;
		GetClientRect(&r);
		const TTrack nTracks=IMAGE->GetTrackCount();
		EXCLUSIVELY_LOCK(scanner.params);
			scanner.params.a=std::max( 0, (iScrollY-TRACK0_Y)/TRACK_HEIGHT );
			scanner.params.z=std::min<LONG>( nTracks, std::max( 0L, Utils::RoundDivUp<LONG>(iScrollY+r.Height()/Utils::LogicalUnitScaleFactor-TRACK0_Y,TRACK_HEIGHT) ) );
			scanner.params.x=scanner.params.a;
		// - launching the Scanner of Tracks
		scanner.scanNextTrack.SetEvent();
	}

	void CTrackMapView::PostNcDestroy(){
		// self-destruction
		//nop (View destroyed by its owner)
	}

	void CTrackMapView::ResetStatusBarMessage() const{
		// updates the MainWindow's StatusBar when cursor isn't over any Sector
		CMainWindow::__setStatusBarText__(nullptr);
	}

	BOOL CTrackMapView::Create(LPCTSTR lpszClassName,LPCTSTR lpszWindowName,DWORD dwStyle,const RECT &rect,CWnd *pParentWnd,UINT nID,CCreateContext *pContext){
		// window creation
		// - base
		const BOOL result=__super::Create( lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext );
		// - informing on TrackMap's capabilities
		if (informOnCapabilities){
			informOnCapabilities=false;
			TDI_INSTANCE->RedrawWindow(); // make sure TDI's whole client area is up-to-date before showing the following message
			Utils::InformationWithCheckableShowNoMore( _T("Use the \"") TRACK_MAP_TAB_LABEL _T("\" tab to explore the structure of the disk, incl. hidden features. Right-click a track or a sector for more operations."), INI_TRACKMAP, INI_MSG_CAPABILITIES );
		}
		return result;
	}

	afx_msg int CTrackMapView::OnCreate(LPCREATESTRUCT lpcs){
		// window created
		// - base
		if (__super::OnCreate(lpcs)==-1) return -1;
		OnInitialUpdate(); // because isn't called automatically by OnCreate; calls SetScrollSizes via OnUpdate
		// - setting number of Heads
{		EXCLUSIVELY_LOCK(scanner.params);
			if (scanner.params.nHeads) // no Heads if wanting to terminate the Scanner
				scanner.params.nHeads=IMAGE->GetHeadCount();
}		// - recovering the scroll position
		SetScrollPos( SB_HORZ, iScrollX, FALSE );
		SetScrollPos( SB_VERT, iScrollY, FALSE );
		// - updating the MainWindow's StatusBar
		ResetStatusBarMessage();
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
		return 0;
	}

	afx_msg void CTrackMapView::OnSize(UINT nType,int cx,int cy){
		// window size changed
		__super::OnSize( nType, cx, cy );
		if (fitLongestTrackInWindow)
			zoomLengthFactor =	showTimed
								? TTrackLength::FromTime(longestTrackNanoseconds,IMAGE->EstimateNanosecondsPerOneByte()).GetZoomFactorToFitWidth(cx)
								: longestTrack.GetZoomFactorToFitWidth(cx);
	}

	CTrackMapView::TCursorPos CTrackMapView::GetPhysicalAddressAndNanosecondsFromPoint(POINT point,TPhysicalAddress &rOutChs,BYTE &rnOutSectorsToSkip,int &rOutNanoseconds){
		// True <=> given actual scroll position, the Point falls into a Sector, otherwise False
		CClientDC dc(this);
		OnPrepareDC(&dc);
		dc.DPtoLP(&point);
		point.y-=TRACK0_Y;
		rOutNanoseconds=-1; // initialization (time could not be determined)
		if (point.y>=0 && point.y<IMAGE->GetTrackCount()*TRACK_HEIGHT){
			// cursor over a Track
			// . estimating the time on timeline at which the cursor points to
			if (showTimed){
				const TLogTime ns=((point.x-SECTOR1_X)<<zoomLengthFactor)*IMAGE->EstimateNanosecondsPerOneByte();
				rOutNanoseconds= ns<=longestTrackNanoseconds ? ns : -1;
			}
			// . determining the Sector on which the cursor hovers
			const TTrack track=point.y/TRACK_HEIGHT;
			const div_t d=div( track, scanner.params.nHeads );
			rOutChs.cylinder=d.quot, rOutChs.head=d.rem;
			if (point.x<SECTOR1_X) // in "Cylinder" or "Head" columns?
				return TCursorPos::TRACK;
			TSectorId bufferId[(TSector)-1];
			WORD bufferLength[(TSector)-1];
			TLogTime bufferStarts[(TSector)-1];
			const TSector nSectors=IMAGE->ScanTrack( d.quot, d.rem, nullptr, bufferId, bufferLength, bufferStarts );
			TimesToPixels( nSectors, bufferStarts, bufferLength );
			for( TSector s=0; s<nSectors; s++ )
				if (bufferStarts[s]<=point.x && point.x<=bufferStarts[s]+(bufferLength[s]>>zoomLengthFactor)){
					// cursor over a Sector
					rOutChs.sectorId=bufferId[s];
					rnOutSectorsToSkip=s;
					return TCursorPos::SECTOR;
				}
			return TCursorPos::TRACK;
		}
		return TCursorPos::NONE;
	}

	afx_msg void CTrackMapView::OnMouseMove(UINT nFlags,CPoint point){
		// cursor moved over this view
		TPhysicalAddress chs; BYTE nSectorsToSkip; TLogTime nanoseconds;
		const bool cursorOverSector=GetPhysicalAddressAndNanosecondsFromPoint(point,chs,nSectorsToSkip,nanoseconds)==TCursorPos::SECTOR;
		TCHAR buf[80], *p=buf; *p='\0';
		if (showTimed && nanoseconds>=0){
			// cursor in timeline range
			float unit; TCHAR unitPrefix;
			if (nanoseconds>TIME_MILLI(1))
				unit=nanoseconds/1e6f, unitPrefix='m';
			else if (nanoseconds>TIME_MICRO(1))
				unit=nanoseconds/1e3f, unitPrefix='µ';
			else
				unit=nanoseconds, unitPrefix='n';
			p+=_stprintf( buf, _T("T = approx. %.2f %cs%c  "), unit, unitPrefix, cursorOverSector?',':'\0' );
		}
		if (cursorOverSector)
			// cursor over a Sector
			::wsprintf( p, _T("Tr%d, %s: %s"), chs.GetTrackNumber(scanner.params.nHeads), (LPCTSTR)chs.sectorId.ToString(), DOS->GetSectorStatusText(chs) );
		CMainWindow::__setStatusBarText__(buf);
	}

	afx_msg void CTrackMapView::OnLButtonUp(UINT nFlags,CPoint point){
		// left mouse button released
		TPhysicalAddress chs; BYTE nSectorsToSkip; int nanoseconds;
		switch (GetPhysicalAddressAndNanosecondsFromPoint(point,chs,nSectorsToSkip,nanoseconds)){
			case TCursorPos::TRACK:
				// clicked on a Track
				if (app.IsInGodMode())
					if (const auto tr=IMAGE->ReadTrack( chs.cylinder, chs.head ))
						tr.ShowModal( _T("Track %d  (Cyl=%d, Head=%d)"), chs.GetTrackNumber(scanner.params.nHeads), chs.cylinder, chs.head );
				break;
			case TCursorPos::SECTOR:
				// clicked on a Sector
				if (app.IsInGodMode() && !IMAGE->IsWriteProtected()){
					TFdcStatus sr;
					IMAGE->GetSectorData( chs, nSectorsToSkip, Revolution::CURRENT, nullptr, &sr );
					if (!sr.IsWithoutError()){
						if (Utils::QuestionYesNo(_T("Unformat this track?"),MB_DEFBUTTON1))
							if (const TStdWinError err=IMAGE->UnformatTrack( chs.cylinder, chs.head ))
								return Utils::FatalError( _T("Can't unformat"), err );
					}else if (Utils::QuestionYesNo(_T("Make this sector unreadable?"),MB_DEFBUTTON1))
						if (const TStdWinError err=IMAGE->MarkSectorAsDirty( chs, nSectorsToSkip, &TFdcStatus::DeletedDam ))
							return Utils::FatalError( _T("Can't make unreadable"), err );
					Invalidate();
				}
				break;
		}
	}

	afx_msg void CTrackMapView::OnRButtonUp(UINT nFlags,CPoint point){
		// right mouse button released
		Utils::CRideContextMenu mnu( IDR_TRACKMAP_CONTEXT );
		TPhysicalAddress chs; BYTE nSectorsToSkip; int nanoseconds;
		switch (GetPhysicalAddressAndNanosecondsFromPoint(point,chs,nSectorsToSkip,nanoseconds)){
			case TCursorPos::SECTOR:
				// clicked on a Sector
				mnu.EnableMenuItem( ID_SECTOR, MF_BYCOMMAND|MF_ENABLED );
				if (IMAGE->ReadTrack( chs.cylinder, chs.head ))
					mnu.EnableMenuItem( ID_DATAFIELD, MF_BYCOMMAND|MF_ENABLED );
				//fallthrough
			case TCursorPos::TRACK:
				// clicked on a Track
				// . Track hexa-preview
				mnu.EnableMenuItem( ID_TRACK, MF_BYCOMMAND|MF_ENABLED );
				// . Track low-level timing
				if (IMAGE->ReadTrack( chs.cylinder, chs.head )) // CANNOT alternatively call WriteTrack with Invalid data for some containers (e.g. *.IPF) don't allow for writing!
					mnu.EnableMenuItem( ID_HEAD, MF_BYCOMMAND|MF_ENABLED );
				// . Track rescan
				if (!IMAGE->properties->IsRealDevice() // possible only for real Devices
					||
					DOS->properties!=&CUnknownDos::Properties // "unscanning" a Track is safe only when no other operations with the disk are possible
				)
					mnu.ModifyMenu( ID_ACCURACY, MF_BYCOMMAND|MF_GRAYED, 0, _T("Can't rescan any track") );
				else if (IMAGE->IsTrackScanned( chs.cylinder, chs.head ))
					mnu.EnableMenuItem( ID_ACCURACY, MF_BYCOMMAND|MF_ENABLED );
				// . Track mining
				if (IMAGE->MineTrack( TPhysicalAddress::Invalid.cylinder, TPhysicalAddress::Invalid.head )==ERROR_NOT_SUPPORTED )
					mnu.ModifyMenu( ID_AUTO, MF_BYCOMMAND|MF_GRAYED, 0, _T("Can't mine any track") );
				else
					mnu.EnableMenuItem( ID_AUTO, MF_BYCOMMAND|MF_ENABLED );
				break;
			default:
				return;
		}
		ClientToScreen(&point);
		switch (mnu.TrackPopupMenu( TPM_RETURNCMD, point.x, point.y, this )){
			case ID_SECTOR:
				// display hexa-data of Sector
				CDiskBrowserView::CreateAndSwitchToTab( IMAGE, chs, nSectorsToSkip );
				break;
			case ID_DATAFIELD:
				// display low-level timing of Sector data
				IMAGE->ShowModalTrackTimingAt( chs, 0, 0, Revolution::ANY_GOOD );
				break;
			case ID_TRACK:{
				// display hexa-data of Track
				TCHAR caption[80];
				::wsprintf( caption, _T("Track %d (Cyl %d, Head %d)"), chs.GetTrackNumber(), chs.cylinder, chs.head );
				TSectorId ids[(TSector)-1];
				IMAGE->ScanTrack( chs.cylinder, chs.head, nullptr, ids );
				chs.sectorId=*ids;
				CTdiCtrl::AddTabLast(
					TDI_HWND, caption,
					&(new CDiskBrowserView( IMAGE, chs, 0 ))->tab,
					true, TDI_TAB_CANCLOSE_ALWAYS, CMainWindow::CTdiView::TTab::OnOptionalTabClosing
				);
				break;
			}
			case ID_HEAD:
				// display low-level Track timing
				if (const auto tr=IMAGE->ReadTrack( chs.cylinder, chs.head ))
					tr.ShowModal( _T("Track %d  (Cyl=%d, Head=%d)"), chs.GetTrackNumber(scanner.params.nHeads), chs.cylinder, chs.head );
				break;
			case ID_ACCURACY:
				// rescan Track
				if (IMAGE->IsTrackDirty( chs.cylinder, chs.head )){
					CString msg;
					msg.Format( _T("Track %d (Cyl=%d, Head=%d) has modifications, rescanning discards them.\n\nContinue?"), chs.GetTrackNumber(scanner.params.nHeads), chs.cylinder, chs.head );
					if (!Utils::QuestionYesNo( msg, MB_DEFBUTTON2 ))
						break;
				}
				if (const TStdWinError err=IMAGE->UnscanTrack( chs.cylinder, chs.head ))
					Utils::Information( _T("Can't rescan"), err );
				else{
					RECT rcTrack;
						GetClientRect(&rcTrack);
						rcTrack.top= TRACK0_Y + (chs.cylinder*scanner.params.nHeads+chs.head)*TRACK_HEIGHT;
						rcTrack.bottom=rcTrack.top+TRACK_HEIGHT;
						Utils::ScaleLogicalUnit( (PINT)&rcTrack, 4 );	static_assert( sizeof(int)==sizeof(LONG), "" );
						::OffsetRect( &rcTrack, 0, -GetScrollPos(SB_VERT) );
					::InvalidateRect( *this, &rcTrack, TRUE );
				}
				break;
			case ID_AUTO:
				// Track mining
				if (const TStdWinError err=IMAGE->MineTrack( chs.cylinder, chs.head ))
					Utils::Information( _T("Mining failed"), err );
				break;
		}
	}

	afx_msg BOOL CTrackMapView::OnMouseWheel(UINT nFlags,short delta,CPoint point){
		// mouse wheel was rotated
		if (nFlags&MK_CONTROL){
			// Ctrl key is down - changing the zoom
			if (delta>0)
				ZoomIn();
			else
				ZoomOut();
			return TRUE;
		}else
			// Ctrl key is not down - scrolling the content
			return __super::OnMouseWheel( nFlags, delta, point );
	}

	afx_msg void CTrackMapView::OnDestroy(){
		// window destroyed
		// - saving scrolling position for later
		iScrollX=GetScrollPos(SB_HORZ);
		iScrollY=GetScrollPos(SB_VERT);
		// - disposing the RainbowPens (assuming that the FillerByte color is a stock object)
		for( int t=TRACK_MAP_COLORS_COUNT; t--; ::DeleteObject(rainbowBrushes[t]) );
		// - base
		EXCLUSIVELY_LOCK_IMAGE(*IMAGE);
		__super::OnDestroy();
	}

	afx_msg void CTrackMapView::ChangeDisplayType(UINT id){
		// DisplayType changed
		displayType=(TDisplayType)id;
		Invalidate(FALSE);
	}
	afx_msg void CTrackMapView::__changeDisplayType_updateUI__(CCmdUI *pCmdUI){
		// projecting DisplayType into UI
		pCmdUI->SetRadio( displayType==pCmdUI->m_nID );
	}

	afx_msg void CTrackMapView::ToggleSectorNumbering(){
		// commanded toggling of ShowingSectorNumbers
		showSectorNumbers=!showSectorNumbers;
		Invalidate(TRUE);
	}
	afx_msg void CTrackMapView::__toggleSectorNumbering_updateUI__(CCmdUI *pCmdUI){
		// projecting SectorNumbering into UI
		pCmdUI->SetCheck(showSectorNumbers);
		pCmdUI->Enable(displayType==TDisplayType::STATUS);
	}

	afx_msg void CTrackMapView::ToggleTiming(){
		// commanded to toggle timed display of Sectors
		showTimed=!showTimed;
		OnUpdate( nullptr, 0, nullptr );
	}
	afx_msg void CTrackMapView::__toggleTiming_updateUI__(CCmdUI *pCmdUI){
		// projecting possibility of timed display of Sectors
		pCmdUI->SetCheck(showTimed);
		pCmdUI->Enable(longestTrackNanoseconds>0);
	}

	afx_msg void CTrackMapView::ZoomOut(){
		// zooms out the view
		if (CAN_ZOOM_OUT){
			fitLongestTrackInWindow=false;
			zoomLengthFactor++;
			UpdateLogicalDimensions();
			Invalidate(TRUE);
		}
	}
	afx_msg void CTrackMapView::__zoomOut_updateUI__(CCmdUI *pCmdUI){
		// projects possibility to even more zoom out the view
		pCmdUI->Enable( CAN_ZOOM_OUT );
	}

	afx_msg void CTrackMapView::ZoomIn(){
		// zooms in the view
		if (CAN_ZOOM_IN){
			fitLongestTrackInWindow=false;
			zoomLengthFactor--;
			UpdateLogicalDimensions();
			Invalidate(TRUE);
		}
	}
	afx_msg void CTrackMapView::__zoomIn_updateUI__(CCmdUI *pCmdUI){
		// projects possibility to even more zoom in the view
		pCmdUI->Enable( CAN_ZOOM_IN );
	}

	afx_msg void CTrackMapView::ZoomFitWidth(){
		// zooms in the view
		if ( fitLongestTrackInWindow=!fitLongestTrackInWindow ){
			CRect rc;
			GetClientRect(&rc);
			zoomLengthFactor =	showTimed
								? TTrackLength::FromTime(longestTrackNanoseconds,IMAGE->EstimateNanosecondsPerOneByte()).GetZoomFactorToFitWidth(rc.Width())
								: longestTrack.GetZoomFactorToFitWidth(rc.Width());
			UpdateLogicalDimensions();
			Invalidate(TRUE);
		}
	}
	afx_msg void CTrackMapView::__zoomFitWidth_updateUI__(CCmdUI *pCmdUI){
		// projects possibility to even more zoom in the view
		pCmdUI->SetCheck( fitLongestTrackInWindow );
	}

	afx_msg void CTrackMapView::ShowSelectedFiles(){
		// toggles display of Sectors occupied by one of Files selected in the FileManager
		if (DOS->pFileManager!=nullptr)
			showSelectedFiles=!showSelectedFiles;
		Invalidate(TRUE);
	}
	afx_msg void CTrackMapView::__showSelectedFiles_updateUI__(CCmdUI *pCmdUI){
		pCmdUI->SetCheck( showSelectedFiles );
		pCmdUI->Enable( DOS->pFileManager!=nullptr ? DOS->pFileManager->GetCountOfSelectedFiles()>0 : false );
	}

	afx_msg void CTrackMapView::ChangeFileSelectionColor(){
		// displays the Color Picker dialog to select a new color for File selection display
		CColorDialog d( fileSelectionColor );
		if (d.DoModal()==IDOK){
			fileSelectionColor=d.GetColor();
			Invalidate(TRUE);
		}
	}

	afx_msg void CTrackMapView::RefreshDisplay(){
		// refreshing the View
		OnUpdate(nullptr,0,nullptr);
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
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		TStatisticParams &rsp=*(TStatisticParams *)pAction->GetParams();
		const PCImage image=rsp.dos->image;
		pAction->SetProgressTarget( image->GetCylinderCount() );
		for( TCylinder nCylinders=image->GetCylinderCount(),cyl=0; cyl<nCylinders; pAction->UpdateProgress(++cyl) )
			for( THead nHeads=image->GetNumberOfFormattedSides(cyl),head=0; head<nHeads; head++,rsp.nTracksFormatted++ ){
				if (pAction->Cancelled) return ERROR_CANCELLED;
				TSectorId bufferId[(TSector)-1];
				WORD bufferLength[(TSector)-1];
				TSector nSectors=image->ScanTrack(cyl,head,nullptr,bufferId,bufferLength);
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
	afx_msg void CTrackMapView::ShowDiskStatistics(){
		// shows statistics on Tracks and their Sectors in current disk
		// - collecting statistics on Tracks and their Sectors
		TStatisticParams sp(DOS);
		if (const TStdWinError err=	CBackgroundActionCancelable(
										__trackStatistics_thread__,
										&sp,
										THREAD_PRIORITY_BELOW_NORMAL
									).Perform()
		)
			return Utils::Information(_T("Cannot create statistics"),err);
		// - showing collected statistics
		TCHAR buf[1000];
		::wsprintf( buf, _T("TRACK STATISTICS\n\n\nTotal number of tracks: %d\n- formatted: %d\n\nTotal number of sectors: %d\n- system: %d\n- erroneous: %d\n- occupied: %d\n- reserved: %d\n- unreachable: %d\n- unknown: %d\n- empty: %d\n"), IMAGE->GetTrackCount(), sp.nTracksFormatted, sp.sectors );
		Utils::Information(buf);
	}
