#include "stdafx.h"
#include "Charting.h"

using namespace Charting;

	#define ZOOM_FACTOR_MAX	24

	#define TIME_HEIGHT		30
	#define IW_HEIGHT		(TIME_HEIGHT+10)
	#define INDEX_HEIGHT	64
	#define METADATA_HEIGHT	80
	#define LINE_EXTENSION	5
	#define SPACING_HEIGHT	(IW_HEIGHT+LINE_EXTENSION)
	#define IW_TIME_HEIGHT	(SPACING_HEIGHT+20)
	#define EVENT_HEIGHT	30

	const TLogTimeInterval TLogTimeInterval::Invalid( INT_MAX, INT_MIN );

	typedef CImage::CTrackReader::TParseEvent TParseEvent,*PParseEvent;
	typedef const TParseEvent *PCParseEvent;

	typedef CImage::CTrackReader::TParseEventPtr TParseEventPtr;

	typedef CImage::CTrackReader::TRegion TRegion,*PRegion;
	typedef CImage::CTrackReader::PCRegion PCRegion;

	#define MSG_FUZZY_NAVIGATION	_T("This fuzzy bit has no counterpart in any revolution")

	#define INI_SECTION	_T("imgte")
	#define INI_DECADIC	_T("dec")

	class CTrackEditor sealed:public Utils::CRideDialog{
		const CImage::CTrackReader &tr;
		const UINT messageBoxButtons;
		const bool initAllFeaturesOn;
		const TLogTime tInitScrollTo;
		const CString caption;
		CMainWindow::CDynMenu menu;
		HANDLE hAutoscrollTimer;
		struct{
			BYTE oneOkPercent; // a window with logical "1" appearing less than this value is considered OK, otherwise Bad
			Utils::CCopyList<TLogTimeInterval> badBlocks; // blocks of consecutive InspectionWindows evaluated as Bad
		} iwInfo;

		enum TCursorFeatures:BYTE{
			TIME	=1,
			SPACING	=2,
			INSPECT	=4,
			STRUCT	=8,
			REGIONS	=16,
			METADATA=32,
			DEFAULT	= TIME//|SPACING
		};

		typedef CImage::CTrackReader::CBitSequence CBitSequence;

		typedef CImage::CTrackReader::CBitSequence::TBit TInspectionWindow;
			// "uid" = Revolution-wide unique identifier; corresponding bits across Revolutions have the same unique identifier
		typedef const TInspectionWindow *PCInspectionWindow;
		
		class CTimeEditor sealed:public CScrollView{
			Utils::CTimeline timeline;
			CImage::CTrackReader tr;
			TLogTime scrollTime;
			std::unique_ptr<CImage::CTrackReader::CBitSequence> inspectionWindows;
			CImage::CTrackReader::CParseEventList parseEvents;
			TLogTime draggedTime; // Time at which left mouse button has been pressed
			TLogTime cursorTime; // Time over which the cursor hovers
			BYTE cursorFeatures; // OR-ed TCursorFeatures values
			bool cursorFeaturesShown; // internally used for painting
			struct TTrackPainter sealed{
				const CBackgroundAction action;
				struct{
					mutable CCriticalSection locker;
					WORD id;
					TLogTimeInterval visible; // visible region
					BYTE zoomFactor;
				} params;
				mutable CEvent repaintEvent;

				static UINT AFX_CDECL Thread(PVOID _pBackgroundAction){
					// thread to paint the Track according to specified Parameters
					const PCBackgroundAction pAction=(PCBackgroundAction)_pBackgroundAction;
					const CTimeEditor &te=*(CTimeEditor *)pAction->GetParams();
					const TTrackPainter &p=te.painter;
					const Utils::CRideBrush iwBrushes[2][2]={
						{ std::move(Utils::CRideBrush((COLORREF)0xE4E4B3)), std::move(Utils::CRideBrush((COLORREF)0xECECCE)) },	//  "OK" even and odd InspectionWindows
						{ std::move(Utils::CRideBrush((COLORREF)0x7E7EEA)), std::move(Utils::CRideBrush((COLORREF)0xB6B6EA)) }	// "BAD" even and odd InspectionWindows
					};
					const Utils::CRideBrush parseEventBrushes[TParseEvent::LAST]={
						TParseEvent::TypeColors[0],
						TParseEvent::TypeColors[1],
						TParseEvent::TypeColors[2],
						TParseEvent::TypeColors[3],
						TParseEvent::TypeColors[4],
						TParseEvent::TypeColors[5],
						TParseEvent::TypeColors[6],
						TParseEvent::TypeColors[7],
						TParseEvent::TypeColors[8],
						TParseEvent::TypeColors[9],
						TParseEvent::TypeColors[10],
						TParseEvent::TypeColors[11],
						TParseEvent::TypeColors[12]
					};
					const TLogTime iwTimeDefaultHalf=te.tr.GetCurrentProfile().iwTimeDefault/2;
					for( CImage::CTrackReader tr=te.tr; true; ){
						// . waiting for next request to paint the Track
						p.repaintEvent.Lock();
						if (!::IsWindow(te.m_hWnd)) // window closed?
							break;
						// . retrieving the Parameters
						p.params.locker.Lock();
							const WORD id=p.params.id;
							const TLogTimeInterval visible=p.params.visible;
						p.params.locker.Unlock();
						if (visible.tStart<0 && visible.tEnd<0) // window closing?
							break;
						const CClientDC dc( const_cast<CTimeEditor *>(&te) );
						const auto &&g=te.timeline.CreateGraphics(dc);
						bool continuePainting=true;
						// . drawing inspection windows (if any)
						if (te.IsFeatureShown(TCursorFeatures::INSPECT)){
							// : determining the first visible inspection window
							PCInspectionWindow piw=te.GetInspectionWindow(visible.tStart);
							int i=piw-te.GetInspectionWindows();
							// : drawing visible inspection windows (avoiding the GDI coordinate limitations by moving the viewport origin)
							RECT rc={ te.timeline.GetClientUnits(piw->time), 1, 0, IW_HEIGHT };
							do{
								rc.right=te.timeline.GetClientUnits(piw[1].time);
								EXCLUSIVELY_LOCK(p.params);
									if ( continuePainting=p.params.id==id ){
										::FillRect( dc, &rc, iwBrushes[piw->bad][i++&1] );
										#ifdef _DEBUG
											TCHAR uid[8];
											::DrawText( dc, _itot(piw->uid%100,uid,10), -1, &rc, DT_SINGLELINE|DT_CENTER );
										#endif
									}
								rc.left=rc.right;
							}while (continuePainting && (++piw)->time<visible.tEnd);
							if (!continuePainting) // new paint request?
								continue;
						}
						// . drawing ParseEvents
						if (te.IsFeatureShown(TCursorFeatures::STRUCT)){
							const auto &peList=te.GetParseEvents();
							const auto dcSettings0=::SaveDC(dc);
								::SelectObject( dc, Utils::CRidePen::BlackHairline );
								const LPCTSTR byteInfoFormat= te.decadicByteValues ? _T("%c\n%d") : _T("%c\n$%02X");
								TCHAR label[80];
								const SIZE byteInfoSizeMin=te.timeline.font.GetTextSize(  label,  ::wsprintf( label, byteInfoFormat, 'M', 255 )  );
								const int nUnitsPerByte=te.timeline.GetUnitCount( CImage::GetActive()->EstimateNanosecondsPerOneByte() );
								const enum{ BI_NONE, BI_MINIMAL, BI_FULL } showByteInfo = nUnitsPerByte>byteInfoSizeMin.cx ? BI_FULL : nUnitsPerByte>1 ? BI_MINIMAL : BI_NONE;
								for( auto it=peList.GetIterator(); continuePainting&&it; ){
									const TParseEventPtr pe=it++->second;
									if (const auto ti=pe->Intersect(visible)){ // ParseEvent visible?
										const int xa=te.timeline.GetClientUnits(ti.tStart), xz=te.timeline.GetClientUnits(ti.tEnd);
										RECT rcLabel={ te.timeline.GetClientUnits(pe->tStart+iwTimeDefaultHalf), -1000, xz, -EVENT_HEIGHT-3 };
										const COLORREF textColor=TParseEvent::TypeColors[pe->type];
								{		EXCLUSIVELY_LOCK(p.params);
											if ( continuePainting=p.params.id==id ){
												::SelectObject( dc, parseEventBrushes[pe->type] );
												::PatBlt( dc, xa,-EVENT_HEIGHT, xz-xa,EVENT_HEIGHT, 0xa000c9 ); // ternary raster operation "dest AND pattern"
												::SetTextColor( dc, textColor );
												::DrawText( dc, pe->GetDescription(),-1, &rcLabel, DT_LEFT|DT_BOTTOM|DT_SINGLELINE );
											}
								}		if (!continuePainting) // new paint request?
											break;
										if (showByteInfo && pe->IsDataAny()){
											const COLORREF textColorBlend=Utils::GetBlendedColor( textColor, COLOR_BLACK );
											const auto &bis=pe.data->byteInfos;
											int i=0;
											while (bis[i].tStart<ti.tStart) i++; // skip invisible part
											const int fullBiLineHeight=rcLabel.bottom-te.timeline.font.charHeight;
											rcLabel.top=fullBiLineHeight-byteInfoSizeMin.cy, rcLabel.bottom=-EVENT_HEIGHT+Utils::CRideFont::Small.charHeight;
											while (continuePainting && bis[i].tStart<ti.tEnd && i<pe->dw){ // draw visible part
												const auto &bi=bis[i];
												EXCLUSIVELY_LOCK(p.params);
												if ( continuePainting=p.params.id==id )
													switch (showByteInfo){
														case BI_MINIMAL:
															g.PerpLine( bi.tStart, -EVENT_HEIGHT-2, -EVENT_HEIGHT+2 );
															break;
														case BI_FULL:{
															rcLabel.left=2+g.PerpLine( bi.tStart, 0, fullBiLineHeight );
															::DrawText(
																dc,
																label,	::wsprintf( label, byteInfoFormat, ::isprint(bi.value)?bi.value:'?', bi.value ),
																&rcLabel, DT_LEFT|DT_TOP
															);
															const COLORREF tc0=::SetTextColor( dc, textColorBlend );
																const HGDIOBJ hFont0=::SelectObject( dc, Utils::CRideFont::Small );
																	::DrawText( dc, label, ::wsprintf(label,_T("+$%X"),i), &rcLabel, DT_BOTTOM|DT_SINGLELINE );
																::SelectObject( dc, hFont0 );
															::SetTextColor( dc, tc0 );
															break;
														}
														default:
															ASSERT(FALSE); break;
													}
												i++;
											}
										}
									}
								}
							::RestoreDC( dc, dcSettings0 );
							if (!continuePainting) // new paint request?
								continue;
						}
						// . drawing Regions
						if (te.IsFeatureShown(TCursorFeatures::REGIONS) && te.pRegions){
							for( DWORD iRegion=0; continuePainting&&iRegion<te.nRegions; iRegion++ ){
								const TRegion &rgn=te.pRegions[iRegion];
								if (const auto ti=rgn.Add(iwTimeDefaultHalf).Intersect(visible)){ // offset Region visible?
									EXCLUSIVELY_LOCK(p.params);
									if ( continuePainting=p.params.id==id )
										g.Rect( ti.tStart, ti.tEnd, TIME_HEIGHT, TIME_HEIGHT+6, Utils::CRideBrush(rgn.color) );
								}
							}
							if (!continuePainting) // new paint request?
								continue;
						}
						// . drawing Index pulses
						const auto dcSettings0=::SaveDC(dc);
							::SelectObject( dc, te.penIndex );
							::SetTextColor( dc, COLOR_BLUE );
							for( BYTE i=0; continuePainting && i<tr.GetIndexCount() && tr.GetIndexTime(i)<visible.tEnd; i++ ){ // visible indices
								const TLogTime tIndex=tr.GetIndexTime(i);
								if (!visible.Contains(tIndex))
									continue; // skip invisible index
								EXCLUSIVELY_LOCK(p.params);
								if ( continuePainting=p.params.id==id )
									g.PerpLineAndText(
										tr.GetIndexTime(i), INDEX_HEIGHT, -INDEX_HEIGHT,
										_T("Index %d"), i
									);
							}
						::RestoreDC(dc,dcSettings0);
						if (!continuePainting) // new paint request?
							continue;
						// . drawing MetaData
						tr.SetCurrentTime(visible.tStart-1);
						if (te.IsFeatureShown(TCursorFeatures::METADATA)){
							const SIZE metaDataSizeMin=te.fontMetaData.GetTextSize( _T("M") );
							const TLogValue metaDataLengthMin=te.timeline.GetValue( metaDataSizeMin.cy );
							const auto dcSettings0=::SaveDC(dc);
								::SelectObject( dc, te.penMetaData );
								::SelectObject( dc, te.fontMetaData );
								::SetTextColor( dc, 0x666666 );
								TCHAR label[80];
								for( auto it=tr.GetCurrentTimeMetaDataIterator(); it!=tr.GetMetaData().cend(); it++ ){
									if (visible.tEnd<it->tStart)
										break;
									EXCLUSIVELY_LOCK(p.params);
									if ( continuePainting=p.params.id==id )
										if (it->GetLength()>metaDataLengthMin){
											//const int bitrate=TIME_SECOND(1)/it->GetBitTimeAvg();
											const int bitrate=TIME_MILLI(1)/it->GetBitTimeAvg(); // kilobits
											const int nLabelChars=::wsprintf( label, _T("Fuzzy %d kbps")+6*!it->isFuzzy, bitrate );
											::TextOut( dc,
												1+g.PerpLine( it->tStart, METADATA_HEIGHT, -METADATA_HEIGHT ),
												-METADATA_HEIGHT+te.fontMetaData.GetTextSize( label, nLabelChars ).cx,
												label, nLabelChars
											);
										}else
											g.PerpLine( it->tStart, -METADATA_HEIGHT, -METADATA_HEIGHT+6 );
								}
							::RestoreDC(dc,dcSettings0);
						}
						if (!continuePainting) // new paint request?
							continue;
						// . drawing Times
						while (continuePainting && tr.GetCurrentTime()<visible.tEnd){
							EXCLUSIVELY_LOCK(p.params);
							if ( continuePainting=p.params.id==id )
								g.PerpLine( tr.ReadTime(), TIME_HEIGHT );
						}
						if (!continuePainting) // new paint request?
							continue;
					}
					return ERROR_SUCCESS;
				}

				TTrackPainter(const CTimeEditor &te)
					// ctor
					: action( Thread, &te, THREAD_PRIORITY_IDLE ) {
					params.visible.tStart = params.visible.tEnd = 0;
				}
			} painter;
		public:
			void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override{
				// request to refresh the display of content
				const auto page=timeline.GetScrollPos(
					timeline.GetValueFromPixel( Utils::TClientRect(m_hWnd).Width() )
				);
				SetScrollSizes(
					MM_TEXT,
					CSize( timeline.GetScrollMax(), 0 ), // total
					sizeDefault, // page (see comment below)
					CSize( std::max(timeline.GetUnitCount(tr.GetCurrentProfile().iwTimeDefault),1), 0 ) // line
				);
				SCROLLINFO si;
				GetScrollInfo( SB_HORZ, &si );
				si.nPage = m_pageDev.cx = page; // no matter what input above, this value is reset in an undesired way by MFC
				SetScrollInfo( SB_HORZ, &si );
			}

			PCInspectionWindow GetInspectionWindow(TLogTime logTime) const{
				// returns the InspectionWindow at specified LogicalTime
				ASSERT( inspectionWindows );
				return inspectionWindows->Find(logTime);
			}

			PCInspectionWindow GetInspectionWindow(int uid,TLogTime tRevStart,TLogTime tRevEnd) const{
				// searching specified time interval, returns the InspectionWindow with specified UniqueIdentifier
				ASSERT( inspectionWindows );
				const auto it=std::lower_bound(
					inspectionWindows->Find(tRevStart),
					inspectionWindows->Find(tRevEnd),
					uid,
					[](const TInspectionWindow &iw,int uid){
						return std::abs(iw.uid)<uid;
					}
				);
				return	it!=inspectionWindows->end() ? it : nullptr;
			}
		private:
			void PaintCursorFeaturesInverted(bool show){
				// paints CursorTime by inverting pixels; painting twice the same CursorTime shows nothing
				if ((show^cursorFeaturesShown)!=0 && cursorFeatures!=0){
					CClientDC dc(this);
					const auto &&g=timeline.CreateGraphics(dc);
					::SetROP2( dc, R2_NOT );
					// . painting vertical line to indicate current position on the Timeline
					if (IsFeatureShown(TCursorFeatures::TIME))
						g.PerpLineAndTextIndirect(
							cursorTime, -500, 500, -80, timeline.font, timeline.ValueToReadableString(cursorTime)
						);
					// . painting space between neighboring Times at current position
					if (IsFeatureShown(TCursorFeatures::SPACING) && cursorTime<tr.GetLastTime()){
						tr.SetCurrentTime(cursorTime);
						const TLogTime a=tr.TruncateCurrentTime(), z=tr.ReadTime();
						g.DimensioningIndirect(
							a, z, TIME_HEIGHT, SPACING_HEIGHT, timeline.ValueToReadableString(z-a), LINE_EXTENSION
						);
					}
					// . painting inspection window size at current position
					if (IsFeatureShown(TCursorFeatures::INSPECT) && cursorTime<tr.GetLastTime())
						if (const PCInspectionWindow piw=GetInspectionWindow(cursorTime)){
							const TLogTime a=piw->time, z=piw[1].time;
							g.DimensioningIndirect(
								a, z, IW_HEIGHT, IW_TIME_HEIGHT, timeline.ValueToReadableString(z-a), LINE_EXTENSION
							);
						}
				}
				cursorFeaturesShown=show;
			}

			TLogTime ClientPixelToTime(long pixel) const{
				return	std::min(
							scrollTime + timeline.GetValueFromPixel(pixel),
							timeline.GetLength()
						);
			}

			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				switch (msg){
					case WM_MOUSEACTIVATE:
						// preventing the focus from being stolen by the parent
						return MA_ACTIVATE;
					case WM_LBUTTONDOWN:
						// left mouse button pressed
						SetFocus();
						draggedTime=ClientPixelToTime( GET_X_LPARAM(lParam) );
						//fallthrough
					case WM_ERASEBKGND:
						// drawing the background
						PaintCursorFeaturesInverted(false);
						break;
					case WM_MOUSEWHEEL:{
						// mouse wheel was rotated
						if (::GetAsyncKeyState(VK_CONTROL)>=0){ // if Ctrl+W NOT pressed
							const TLogTime dt=(scrollTime-GetCenterTime())*(short)HIWORD(wParam)/WHEEL_DELTA;
							SetScrollTime( scrollTime+dt );
							return 0;
						}
						POINT cursor;
						::GetCursorPos(&cursor);
						ScreenToClient(&cursor);
						const int nUnitsX=cursor.x/Utils::LogicalUnitScaleFactor;
						if ((short)HIWORD(wParam)<0 && timeline.GetZoomFactor()<ZOOM_FACTOR_MAX)
							// want zoom out and still can zoom out
							SetZoomFactor( timeline.GetZoomFactor()+1, nUnitsX );
						else if ((short)HIWORD(wParam)>0 && timeline.GetZoomFactor()>0)
							// want zoom in and still can zoom in
							SetZoomFactor( timeline.GetZoomFactor()-1, nUnitsX );
						else
							// can't process the zoom request
							return 0;
						lParam=cursor.x;
						//fallthrough
					}
					case WM_LBUTTONUP:
						// left mouse button released
						draggedTime=-1;
						//fallthrough
					case WM_MOUSEMOVE:
						// mouse moved
						if (draggedTime>=0) // left mouse button pressed
							SetScrollTime(
								scrollTime
								+
								draggedTime
								-
								ClientPixelToTime( GET_X_LPARAM(lParam) )
							);
						else{
							PaintCursorFeaturesInverted(false);
								cursorTime=ClientPixelToTime( GET_X_LPARAM(lParam) );
							PaintCursorFeaturesInverted(true);
						}
						break;
					case WM_KEYDOWN:
						// key pressed
						switch (wParam){
							case VK_HOME:
								SetScrollTime(0);
								break;
							case VK_END:
								SetScrollTime(timeline.GetLength());
								break;
							case VK_PRIOR:	// page up
								OnScroll( SB_PAGELEFT, 0 );
								break;
							case VK_NEXT:	// page down
								OnScroll( SB_PAGERIGHT, 0 );
								break;
							case VK_LEFT:
								OnScroll( SB_LINELEFT, 0 );
								break;
							case VK_RIGHT:
								OnScroll( SB_LINERIGHT, 0 );
								break;
						}
						break;
					case WM_DESTROY:
						// window about to be destroyed
						// . letting the Painter finish normally
						painter.params.locker.Lock();
							painter.params.id++;
							painter.params.visible.tStart = painter.params.visible.tEnd = INT_MIN;
						painter.params.locker.Unlock();
						painter.repaintEvent.SetEvent();
						::WaitForSingleObject( painter.action, INFINITE );
						// . base
						break;
				}
				return __super::WindowProc( msg, wParam, lParam );
			}

			BOOL OnScroll(UINT nScrollCode,UINT nPos,BOOL bDoScroll=TRUE) override{
				// scrolls the View's content in given way
				SCROLLINFO si={ sizeof(si) };
				// . horizontal ScrollBar
				GetScrollInfo( SB_HORZ, &si, SIF_POS|SIF_TRACKPOS|SIF_RANGE|SIF_PAGE );
				switch (LOBYTE(nScrollCode)){
					case SB_LEFT		: si.nPos=0;			break;
					case SB_RIGHT		: si.nPos=INT_MAX;		break;
					case SB_LINELEFT	: si.nPos-=m_lineDev.cx;break;
					case SB_LINERIGHT	: si.nPos+=m_lineDev.cx;break;
					case SB_PAGELEFT	: si.nPos-=m_pageDev.cx;break;
					case SB_PAGERIGHT	: si.nPos+=m_pageDev.cx;break;
					case SB_THUMBPOSITION:	// "thumb" released
					case SB_THUMBTRACK	: si.nPos=si.nTrackPos;	break;
				}
				si.nPos=std::min( si.nPos, si.nMax-(int)si.nPage );
				SetScrollTime( timeline.GetValueFromScroll(si.nPos) );
				return TRUE;
			}

			void OnDraw(CDC *pDC) override{
				// drawing the LogicalTimes
				// . hiding CursorTime information
				PaintCursorFeaturesInverted(false);
				// . drawing the Timeline
				const HDC dc=*pDC;
				::SetBkMode( dc, TRANSPARENT );
				::SetViewportOrgEx( dc, 0, Utils::TClientRect(m_hWnd).Height()/2, nullptr );
				EXCLUSIVELY_LOCK(painter.params);
					painter.params.id++; // stop current painting
					const TLogInterval drawn=timeline.Draw( dc, scrollTime, -1, 0, nullptr );
					painter.params.visible.tStart=drawn.a; // good-enough for drawing Indices (an Index at Time=0 -> the caption begins to appear as scrolled towards Time=0)
					painter.params.visible.tEnd=drawn.z;
					painter.params.zoomFactor=timeline.GetZoomFactor();
				// . drawing the rest in parallel thread due to computational complexity if painting the whole Track
				painter.repaintEvent.SetEvent();
			}

			void PostNcDestroy() override{
				// self-destruction
				//nop (View destroyed by its owner)
			}
		public:
			const PCRegion pRegions;
			const DWORD nRegions;
			const Utils::CRidePen penIndex;
			const Utils::CRidePen penMetaData;
			const Utils::CRideFont fontMetaData;
			bool decadicByteValues;

			CTimeEditor(const CImage::CTrackReader &tr,CImage::CTrackReader::PCRegion pRegions,DWORD nRegions)
				// ctor
				: timeline( tr.GetTotalTime(), 1, 10 )
				, tr(tr)
				, pRegions(pRegions) , nRegions(nRegions) // up to the caller to dispose allocated Regions!
				, penIndex( 2, COLOR_BLUE )
				, penMetaData( 1, COLOR_BLACK, PS_DOT )
				, fontMetaData( Utils::CRideFont::Small.CreateRotated(90) )
				, painter(*this)
				, decadicByteValues( app.GetProfileBool(INI_SECTION,INI_DECADIC) )
				, draggedTime(-1)
				, cursorTime(-1) , cursorFeaturesShown(false)
				, cursorFeatures( TCursorFeatures::DEFAULT | tr.GetMetaData()*TCursorFeatures::METADATA )
				, scrollTime(0) {
			}
			
			~CTimeEditor(){
				// dtor
				//if (pRegions)
					//::free((PVOID)pRegions); // commented out as it's up to the caller to dispose allocated Regions
			}

			void OnInitialUpdate() override{
				// called after window creation
				__super::OnInitialUpdate();
				SetZoomFactor( timeline.GetZoomFactor(), 0 ); // initialization
				painter.action.Resume();
			}

			inline const Utils::CTimeline &GetTimeline() const{
				return timeline;
			}

			void SetZoomFactor(BYTE newZoomFactor,int focusUnitX){
				const TLogTime t=scrollTime+timeline.GetTime(focusUnitX);
				timeline.SetZoomFactor( newZoomFactor );
				OnUpdate( nullptr, 0, nullptr );
				SetScrollTime(  timeline.GetTime( timeline.GetUnitCount(t)-focusUnitX )  );
				Invalidate();
			}

			void SetZoomFactorCenter(BYTE newZoomFactor){
				CRect rc;
				GetClientRect(&rc);
				SetZoomFactor( newZoomFactor, rc.Width()/(Utils::LogicalUnitScaleFactor*2) );
			}

			TLogTime GetClientCursorTime() const{
				POINT cursor;
				::GetCursorPos(&cursor);
				ScreenToClient(&cursor);
				CRect rc;
				GetClientRect(&rc);
				return	rc.PtInRect(cursor) // over client?
						? cursorTime
						: GetCenterTime();
			}

			inline TLogTime GetScrollTime() const{
				return scrollTime;
			}

			void SetScrollTime(TLogTime t){
				if (t<0) t=0;
				else if (t>timeline.GetLength()) t=timeline.GetLength();
				SetScrollPos( SB_HORZ, timeline.GetScrollPos(t) );
				if (const long scroll=timeline.GetPixelCount(scrollTime)-timeline.GetPixelCount(t)){ // scrolling needed?
					EXCLUSIVELY_LOCK(painter.params);
					painter.params.id++; // stop current painting
					PaintCursorFeaturesInverted(false);
					ScrollWindow( scroll, 0 ); // "base"
					scrollTime=t;
					POINT cursor;
					::GetCursorPos(&cursor);
					ScreenToClient(&cursor);
					cursorTime=ClientPixelToTime( cursor.x );
				}
				//painter.repaintEvent.SetEvent(); // commented out as drawing invoked by 'ScrollWindow'
			}

			TLogTime GetCenterTime() const{
				CRect rc;
				GetClientRect(&rc);
				return scrollTime+timeline.GetValueFromPixel( rc.Width()/2 );
			}

			void SetCenterTime(TLogTime t){
				SetScrollTime( t-(GetCenterTime()-scrollTime) );
			}

			inline PCInspectionWindow GetInspectionWindows() const{
				return inspectionWindows.get() ? inspectionWindows->begin() : nullptr;
			}

			inline void SetInspectionWindows(std::unique_ptr<CBitSequence> &&list){
				inspectionWindows.reset( list.release() );
			}

			inline const CImage::CTrackReader::CParseEventList &GetParseEvents() const{
				return parseEvents;
			}

			void SetParseEvents(const CImage::CTrackReader::CParseEventList &list){
				ASSERT( parseEvents.GetCount()==0 ); // can set only once
				parseEvents.Add( list );
			}

			inline bool IsFeatureShown(TCursorFeatures cf) const{
				return (cursorFeatures&cf)!=0;
			}

			void ToggleFeature(TCursorFeatures cf){
				EXCLUSIVELY_LOCK(painter.params);
					const bool cfs0=cursorFeaturesShown;
					PaintCursorFeaturesInverted(false);
						if (IsFeatureShown(cf))
							cursorFeatures&=~cf;
						else
							cursorFeatures|=cf;
					PaintCursorFeaturesInverted(cfs0);
			}

			void ShowAllFeatures(){
				EXCLUSIVELY_LOCK(painter.params);
					PaintCursorFeaturesInverted(false);
						cursorFeatures=-1;
					Invalidate();
			}
		} timeEditor;

		#define AUTOSCROLL_HALF	64

		BOOL OnInitDialog() override{
			// dialog initialization
			// - base
			__super::OnInitDialog();
			// - setting window Caption
			SetWindowText(caption);
			// - adding menu to this dialog (and extending its bottom to compensate for shrunk client area)
			SetMenu(&menu);
			CRect rc;
			GetWindowRect(&rc);
			rc.bottom+=::GetSystemMetrics(SM_CYMENU);
			SetWindowPos( nullptr, 0,0, rc.Width(),rc.Height(), SWP_NOZORDER|SWP_NOMOVE );
			// - creating the TimeEditor
			timeEditor.Create( nullptr, nullptr, WS_CHILD|WS_VISIBLE, MapDlgItemClientRect(ID_TRACK), this, 0 );
			timeEditor.OnInitialUpdate(); // because hasn't been called automatically
			timeEditor.SetFocus();
			// - setting up the Accuracy slider
			SendDlgItemMessage( ID_ACCURACY, TBM_SETRANGEMAX, FALSE, 2*AUTOSCROLL_HALF );
			SendDlgItemMessage( ID_ACCURACY, TBM_SETPOS, TRUE, AUTOSCROLL_HALF );
			SendDlgItemMessage( ID_ACCURACY, TBM_SETTHUMBLENGTH, TRUE, 50 );
			// - displaying requested "MessageBox-like" set of buttons
			switch (messageBoxButtons){
				case MB_OK:
					break;
				case MB_ABORTRETRYIGNORE:{
					static constexpr WORD RetryIgnoreIds[]={ IDRETRY, IDOK, 0 };
					ShowDlgItems( RetryIgnoreIds, EnableDlgItems(RetryIgnoreIds,true) );
					SetDlgItemText( IDOK, _T("Ignore") );
					break;
				}
				default:
					ASSERT(FALSE); break; // we shouldn't end up here - all used options must be covered!
			}
			// - if specific initial position specified, scrolling to it
			if (tInitScrollTo>0)
				timeEditor.SetCenterTime( tInitScrollTo );
			// - if Regions are specified, navigating to the first of them
			else if (timeEditor.pRegions)
				timeEditor.SetCenterTime( timeEditor.pRegions->tStart );
			// - if requested, displaying all Features
			if (initAllFeaturesOn)
				SendMessage( WM_COMMAND, ID_TRACK );
			return FALSE; // False = focus already set manually
		}

		BOOL PreTranslateMessage(PMSG pMsg) override{
			// pre-processing the Message
			return	::TranslateAccelerator( m_hWnd, menu.hAccel, pMsg );
		}

		#define AUTOSCROLL_TIMER_ID	0x100000

		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
			// window procedure
			switch (msg){
				case WM_INITMENU:{
					CMenu tmp;
					tmp.Attach( (HMENU)wParam );
						Utils::CRideContextMenu::UpdateUI( this, &tmp );
					tmp.Detach();
					return 0;
				}
				case WM_TIMER:
					// timer tick
					wParam=TB_THUMBTRACK, lParam=(LPARAM)GetDlgItemHwnd(ID_ACCURACY);
					//fallthrough
				case WM_HSCROLL:{
					// the Accuracy slider has been used
					if (lParam==0) // this control's native scrollbar
						break;
					int i;
					switch (LOWORD(wParam)){
						case TB_THUMBPOSITION:
							// "thumb" released
							::KillTimer( m_hWnd, AUTOSCROLL_TIMER_ID );
							hAutoscrollTimer=INVALID_HANDLE_VALUE;
							i=HIWORD(wParam);
							SendDlgItemMessage( ID_ACCURACY, TBM_SETPOS, TRUE, AUTOSCROLL_HALF );
							timeEditor.SetFocus();
							break;
						case TB_THUMBTRACK:
							// "thumb" dragged
							if (hAutoscrollTimer==INVALID_HANDLE_VALUE) // timer not yet set
								hAutoscrollTimer=(HANDLE)SetTimer( AUTOSCROLL_TIMER_ID, 50, nullptr );
							if (msg!=WM_TIMER) // waiting for the automatic scroll event to occur
								return TRUE;
							i=SendDlgItemMessage( ID_ACCURACY, TBM_GETPOS );
							break;
						default:
							// ignoring all other events that may occur for a slider
							SendDlgItemMessage( ID_ACCURACY, TBM_SETPOS, TRUE, AUTOSCROLL_HALF );
							return TRUE;
					}
					timeEditor.SetScrollTime( timeEditor.GetScrollTime()+timeEditor.GetTimeline().GetTime(i-AUTOSCROLL_HALF) );
					break;
				}
			}
			return __super::WindowProc( msg, wParam, lParam );
		}

		static UINT AFX_CDECL CreateInspectionWindowList_thread(PVOID _pCancelableAction){
			// thread to create list of inspection windows used to recognize data in the Track
			const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
			CTrackEditor &rte=*(CTrackEditor *)pAction->GetParams();
			if (rte.timeEditor.GetInspectionWindows()!=nullptr) // already set?
				return pAction->TerminateWithSuccess();
			pAction->SetProgressTarget(3);
			// - Step 1: recognize all Bits
			const auto &tr=rte.tr;
			const auto &&resetProfile=tr.CreateResetProfile();
			std::unique_ptr<CImage::CTrackReader::CBitSequence> bits(
				new CImage::CTrackReader::CBitSequence(
					tr, 0, resetProfile, tr.GetTotalTime(), rte.iwInfo.oneOkPercent
				)
			);
			if (!bits.get())
				return pAction->TerminateWithLastError();
			pAction->IncrementProgress();
			// - Step 2: offset all Bits, producing InspectionWindows beginnings
			bits->OffsetAll( -resetProfile.iwTimeDefault/2 );
			pAction->IncrementProgress();
			// - Step 3: populating the list of BadBlocks, i.e. Bits that are reported as Bad
			auto &badBlocks=rte.iwInfo.badBlocks;
			BYTE iwStatuses=0; // last 8 InspectionWindows statuses (0 = ok, 1 = bad)
			for( auto *p=bits->GetBits(),*pLast=p+bits->GetBitCount(); p<pLast; p++ ){
				if (p->bad){
					const TLogTime tBitEnd=p[1].time;
					if (iwStatuses&3){ // between this and the previous bad InspectionWindow is at most one ok InspectionWindow
						badBlocks.GetTail().tEnd=tBitEnd; // extending an existing BadBlock
						p[-1].bad=true; // involving the previous InspectionWindow into the BadBlock
					}else
						badBlocks.AddTail(  TLogTimeInterval( p->time, tBitEnd )  );
				}
				p->uid=INT_MIN; // Bits across various Revolutions not yet linked
				iwStatuses = (iwStatuses<<1) | (BYTE)p->bad;
			}
			rte.timeEditor.SetInspectionWindows( std::move(bits) );
			return pAction->TerminateWithSuccess();
		}

		static UINT AFX_CDECL CreateParseEventsList_thread(PVOID _pCancelableAction){
			// thread to create list of inspection windows used to recognize data in the Track
			const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
			CTrackEditor &rte=*(CTrackEditor *)pAction->GetParams();
			if (rte.timeEditor.GetParseEvents().GetCount()>0) // already set?
				return pAction->TerminateWithSuccess();
			CImage::CTrackReader tr=rte.tr; // copy
			CImage::CTrackReader::CParseEventList peList;
			// (1) Each decoder (e.g. Fraser's, Ogden's, etc.) begins by stepping to the NEXT InspectionWindow
			// (2) All ParseEvents have Start/End set to BEFORE this step took place
			// (3) So, all ParseEvents are one actual InspectionWindow size BEHIND!
			// (4) To compensate for (3) and be in sync with decoders, we should step the ParseEvent as 'tStart+tIwStart' and 'tEnd+tIwEnd', but we don't have 'tIwStart' and 'tIwEnd'
			// (5) So, we assume that all ParseEvents are 'tIwDefault' behind and would step 'tStart+tIwDefault' and 'tEnd+tIwDefault'
			// (6) But at the same time, all InspectionWindows are shifted by '-tIwDefault/2' (because the first IW begins right with 'tIwDefault' size)
			// (7) Hence, to compensate for (3) and (6), the ParseEvents are stepped 'tStart+tIwDefault-tIwDefault/2' and 'tEnd+tIwDefault-tIwDefault/2'
			const TLogTime tIwOffset=tr.CreateResetProfile().iwTimeDefault/2; // see (7)
			for each( const auto &pair in tr.ScanAndAnalyze(*pAction) ){
				auto &pe=*const_cast<TParseEvent *>(pair.second);
				pe.tStart+=tIwOffset, pe.tEnd+=tIwOffset;
				if (pe.IsDataAny()){
					auto &dpe=(CImage::CTrackReader::TDataParseEvent &)pe;
					for( auto i=dpe.GetByteCount(); i>0; dpe.byteInfos[--i].tStart+=tIwOffset );
				}
				peList.Add(pe);
			}
			rte.timeEditor.SetParseEvents(peList);
			return pAction->TerminateWithSuccess();
		}

		static UINT AFX_CDECL CreateMatchingBitsInfo_thread(PVOID _pCancelableAction){
			// thread to create list of inspection windows used to recognize data in the Track
			const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
			const CTrackEditor &te=*(const CTrackEditor *)pAction->GetParams();
			ASSERT( te.timeEditor.GetInspectionWindows()!=nullptr ); // must be set!
			const CImage::CTrackReader &tr=te.tr;
			if (tr.GetIndexCount()<3) // at least two full Revolution must exist ...
				return pAction->TerminateWithSuccess(); // ... otherwise matching bits can't be linked together
			pAction->SetProgressTarget(tr.GetTotalTime());
			const auto &peList=te.timeEditor.GetParseEvents();
			const TLogTime iwTimeTolerance=tr.GetCurrentProfile().iwTimeMin/4;
			for( BYTE i=1; i<tr.GetIndexCount(); i++ ){
				TInspectionWindow *iw=const_cast<TInspectionWindow *>(te.timeEditor.GetInspectionWindow( tr.GetIndexTime(i-1) ));
				const PCInspectionWindow iwRevEnd=te.timeEditor.GetInspectionWindow( tr.GetIndexTime(i) );
				int uid=1;
				do{
					const auto it=peList.FindByEnd( iw->time+iwTimeTolerance, CImage::CTrackReader::TParseEvent::FUZZY_OK, CImage::CTrackReader::TParseEvent::FUZZY_BAD );
					const TLogTimeInterval &tiFuzzy= it ? it->second->Add(-iwTimeTolerance) : TLogTimeInterval::Invalid;
					while (iw<iwRevEnd && iw->time<tiFuzzy.tStart) // assigning InspectionWindows BEFORE the next Fuzzy event their UniqueIdentifiers
						iw++->uid=uid++;
					while (iw<iwRevEnd && iw->time<=tiFuzzy.tEnd) // assigning InspectionWindows BEFORE the next Fuzzy event negative UniqueIdentifiers of the last non-Fuzzy InspectionWindow
						iw++->uid=-uid;
					uid++;
					pAction->UpdateProgress(iw->time);
				}while (iw<iwRevEnd);
			}
			return pAction->TerminateWithSuccess();
		}

		BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
			// command processing
			switch (nCode){
				case CN_UPDATE_COMMAND_UI:{
					// update
					CCmdUI *const pCmdUi=(CCmdUI *)pExtra;
					switch (nID){
						case ID_ZOOM_IN:
							pCmdUi->Enable( timeEditor.GetTimeline().GetZoomFactor()>0 );
							return TRUE;
						case ID_ZOOM_OUT:
							pCmdUi->Enable( timeEditor.GetTimeline().GetZoomFactor()<ZOOM_FACTOR_MAX );
							//fallthrough
						case ID_ZOOM_FIT:
						case ID_REFRESH:
						case IDCANCEL:
							return TRUE;
						case ID_ZOOM_PART:
							pCmdUi->Enable( tr.GetIndexCount()>=2 );
							return TRUE;
						case ID_TIME:
							pCmdUi->SetCheck( timeEditor.IsFeatureShown(TCursorFeatures::TIME) );
							return TRUE;
						case ID_ARCHIVE:
							pCmdUi->Enable( tr.GetMetaData() );
							pCmdUi->SetCheck( tr.GetMetaData() && timeEditor.IsFeatureShown(TCursorFeatures::METADATA) );
							return TRUE;
						case ID_GAP:
							pCmdUi->SetCheck( timeEditor.IsFeatureShown(TCursorFeatures::SPACING) );
							return TRUE;
						case ID_RECOGNIZE:
							pCmdUi->SetCheck( timeEditor.IsFeatureShown(TCursorFeatures::INSPECT) );
							return TRUE;
						case ID_INTERLEAVE:
							pCmdUi->Enable( timeEditor.pRegions!=nullptr );
							pCmdUi->SetCheck( timeEditor.IsFeatureShown(TCursorFeatures::REGIONS) );
							return TRUE;
						case ID_SYSTEM:
							pCmdUi->SetCheck( timeEditor.IsFeatureShown(TCursorFeatures::STRUCT) );
							//fallthrough
						case ID_TRACK:
							return TRUE;
						case ID_PREV:
							pCmdUi->Enable( tr.GetIndexCount()>0 && timeEditor.GetCenterTime()>tr.GetIndexTime(0) );
							return TRUE;
						case ID_NEXT:
							pCmdUi->Enable( tr.GetIndexCount()>0 && timeEditor.GetCenterTime()<tr.GetIndexTime(tr.GetIndexCount()-1) );
							return TRUE;
						case ID_META_PREV:
							pCmdUi->Enable( timeEditor.GetScrollTime()>0 && tr.GetMetaData() && timeEditor.GetCenterTime()>tr.GetMetaData().cbegin()->tStart );
							return TRUE;
						case ID_META_NEXT:
							pCmdUi->Enable( timeEditor.GetScrollTime()<tr.GetTotalTime() && timeEditor.GetCenterTime()<tr.GetMetaData().GetLast()->GetStartTimeSafe() );
							return TRUE;
						case ID_FILE_SHIFT_DOWN:
							if (const auto it=timeEditor.GetParseEvents().GetFirstByStart())
								pCmdUi->Enable( it->second->tStart<timeEditor.GetCenterTime() );
							else
								pCmdUi->Enable( FALSE );
							return TRUE;
						case ID_FILE_SHIFT_UP:
							if (const auto it=timeEditor.GetParseEvents().GetLastByStart())
								pCmdUi->Enable( timeEditor.GetCenterTime()<it->second->tStart );
							else
								pCmdUi->Enable( FALSE );
							return TRUE;
						case ID_PREV_PANE:
							if (const auto it=timeEditor.GetParseEvents().FindByStart(0,TParseEvent::FUZZY_OK,TParseEvent::FUZZY_BAD))
								pCmdUi->Enable( it->second->tStart<timeEditor.GetCenterTime() );
							else
								pCmdUi->Enable( FALSE );
							return TRUE;						
						case ID_NEXT_PANE:
							pCmdUi->Enable( timeEditor.GetParseEvents().FindByStart(timeEditor.GetCenterTime()+1,TParseEvent::FUZZY_OK,TParseEvent::FUZZY_BAD) );
							return TRUE;
						case ID_RECORD_PREV:
							pCmdUi->Enable( timeEditor.pRegions && timeEditor.GetCenterTime()>timeEditor.pRegions->tStart );
							return TRUE;
						case ID_RECORD_NEXT:
							pCmdUi->Enable( timeEditor.pRegions && timeEditor.GetCenterTime()<timeEditor.pRegions[timeEditor.nRegions-1].tStart );
							return TRUE;
						case ID_DOWN:
							pCmdUi->Enable( timeEditor.GetScrollTime()>0 );
							return TRUE;
						case ID_UP:
							pCmdUi->Enable( timeEditor.GetScrollTime()<tr.GetTotalTime() );
							return TRUE;
						case ID_PREV_BAD:
							pCmdUi->Enable( timeEditor.GetScrollTime()>0 && timeEditor.GetInspectionWindows()!=nullptr && iwInfo.badBlocks.GetCount()>0 && iwInfo.badBlocks.GetHead().tStart<timeEditor.GetCenterTime() );
							return TRUE;
						case ID_NEXT_BAD:
							pCmdUi->Enable( timeEditor.GetScrollTime()<tr.GetTotalTime() && timeEditor.GetInspectionWindows()!=nullptr && iwInfo.badBlocks.GetCount()>0 && timeEditor.GetCenterTime()<iwInfo.badBlocks.GetTail().tStart );
							return TRUE;
						case ID_REVOLUTION_PREV:{
							const TLogTime tCursor=timeEditor.GetClientCursorTime();
							pCmdUi->Enable( tr.GetIndexCount()>=3 && tr.GetIndexTime(1)<=tCursor && tCursor<tr.GetIndexTime(tr.GetIndexCount()-1) );
							return TRUE;
						}
						case ID_REVOLUTION_NEXT:{
							const TLogTime tCursor=timeEditor.GetClientCursorTime();
							pCmdUi->Enable( tr.GetIndexCount()>=3 && tr.GetIndexTime(0)<=tCursor && tCursor<tr.GetIndexTime(tr.GetIndexCount()-2) );
							return TRUE;
						}
					}
					break;
				}
				case CN_COMMAND:
					// command
					switch (nID){
						case IDRETRY:
						case IDOK:
							EndDialog(nID);
							return TRUE;
						case ID_ZOOM_IN:
							timeEditor.SetZoomFactor(
								timeEditor.GetTimeline().GetZoomFactor()-1,
								timeEditor.GetTimeline().GetUnitCount(timeEditor.GetClientCursorTime()-timeEditor.GetScrollTime())
							);
							return TRUE;
						case ID_ZOOM_OUT:
							timeEditor.SetZoomFactor(
								timeEditor.GetTimeline().GetZoomFactor()+1,
								timeEditor.GetTimeline().GetUnitCount(timeEditor.GetClientCursorTime()-timeEditor.GetScrollTime())
							);
							return TRUE;
						case ID_ZOOM_PART:{
							BYTE rev=0;
							for( const TLogTime t=timeEditor.GetCenterTime(); rev<tr.GetIndexCount()&&t>tr.GetIndexTime(rev); rev++ );
							rev+=rev==0; // if before the first Revolution, pointing at the end of the first Revolution
							rev-=rev==tr.GetIndexCount(); // if after after the last Revolution, pointing at the end of the last Revolution
							CRect rc;
							timeEditor.GetClientRect(&rc);
							timeEditor.SetZoomFactorCenter(
								timeEditor.GetTimeline().GetZoomFactorToFitWidth(
									tr.GetIndexTime(rev)-tr.GetIndexTime(rev-1), // Revolution time
									rc.Width(), // # of Pixels to fit the Revolution to
									ZOOM_FACTOR_MAX
								)
							);
							timeEditor.SetCenterTime( (tr.GetIndexTime(rev)+tr.GetIndexTime(rev-1))/2 );
							return TRUE;
						}
						case ID_ZOOM_FIT:{
							CRect rc;
							timeEditor.GetClientRect(&rc);
							timeEditor.SetZoomFactorCenter( timeEditor.GetTimeline().GetZoomFactorToFitWidth(rc.Width(),ZOOM_FACTOR_MAX) );
							return TRUE;
						}
						case ID_REFRESH:
							timeEditor.Invalidate();
							return TRUE;
						case IDCANCEL:
							EndDialog(nID);
							return TRUE;
						case ID_TIME:
							timeEditor.ToggleFeature(TCursorFeatures::TIME);
							return TRUE;
						case ID_ARCHIVE:
							timeEditor.ToggleFeature(TCursorFeatures::METADATA);
							timeEditor.Invalidate();
							return TRUE;
						case ID_GAP:
							timeEditor.ToggleFeature(TCursorFeatures::SPACING);
							return TRUE;
						case ID_RECOGNIZE:
							#define ACTION_INSPECTION_DESC _T("Inspection")
							#define ACTION_NAVIGATION_DESC _T("Navigation")
							if (!timeEditor.IsFeatureShown(TCursorFeatures::INSPECT)) // currently hidden, so want now show the Feature
								if (timeEditor.GetInspectionWindows()==nullptr) // data to display not yet received
									if (timeEditor.GetParseEvents().GetCount()>0){ // has parsing been already made?
										CBackgroundMultiActionCancelable bmac(THREAD_PRIORITY_LOWEST);
											bmac.AddAction( CreateInspectionWindowList_thread, this, ACTION_INSPECTION_DESC );
											bmac.AddAction( CreateMatchingBitsInfo_thread, this, ACTION_NAVIGATION_DESC );
										if (bmac.Perform()!=ERROR_SUCCESS)
											return TRUE;
									}else
										if (CBackgroundActionCancelable( CreateInspectionWindowList_thread, this, THREAD_PRIORITY_LOWEST ).Perform()!=ERROR_SUCCESS)
											return TRUE;
							timeEditor.ToggleFeature(TCursorFeatures::INSPECT);
							timeEditor.Invalidate();
							return TRUE;
						case ID_INTERLEAVE:
							timeEditor.ToggleFeature(TCursorFeatures::REGIONS);
							timeEditor.Invalidate();
							return TRUE;
						case ID_SYSTEM:
							#define ACTION_PARSING_DESC _T("Structure")
							if (!timeEditor.IsFeatureShown(TCursorFeatures::STRUCT)) // currently hidden, so want now show the Feature
								if (timeEditor.GetParseEvents().GetCount()==0) // data to display not yet received
									if (timeEditor.GetInspectionWindows()){ // is inspection already known?
										CBackgroundMultiActionCancelable bmac(THREAD_PRIORITY_LOWEST);
											bmac.AddAction( CreateParseEventsList_thread, this, ACTION_PARSING_DESC );
											bmac.AddAction( CreateMatchingBitsInfo_thread, this, ACTION_NAVIGATION_DESC );
										if (bmac.Perform()!=ERROR_SUCCESS)
											return TRUE;
									}else
										if (CBackgroundActionCancelable( CreateParseEventsList_thread, this, THREAD_PRIORITY_LOWEST ).Perform()!=ERROR_SUCCESS)
											return TRUE;
							timeEditor.ToggleFeature(TCursorFeatures::STRUCT);
							timeEditor.Invalidate();
							return TRUE;
						case ID_TRACK:{
							CBackgroundMultiActionCancelable bmac(THREAD_PRIORITY_LOWEST);
								bmac.AddAction( CreateInspectionWindowList_thread, this, ACTION_INSPECTION_DESC );
								bmac.AddAction( CreateParseEventsList_thread, this, ACTION_PARSING_DESC );
								bmac.AddAction( CreateMatchingBitsInfo_thread, this, ACTION_NAVIGATION_DESC );
							if (bmac.Perform()==ERROR_SUCCESS)
								timeEditor.ShowAllFeatures();
							return TRUE;
						}
						case ID_NAVIGATE_ADDRESS:{
							static constexpr TCHAR Units[]=_T("nums"); // in order of scale ascending
							class CGotoTimeDialog sealed:public Utils::CRideDialog{
								const TLogTime tMax;

								TLogTime ParseTime() const{
									TCHAR buf[80];
									GetDlgItemText( ID_TIME, buf );
									LPCTSTR p=::CharLower(buf);
									TLogTime tResult=-1; // assumption (no or invalid time entered)
									char iLastUnitUsed=100; // no unit yet used
									for( int t,i,u=0; *p; p+=i ){
										const int nItems=_stscanf(p,_T("%d%n%c%n"),&t,&i,&u,&i);
										if (nItems<0)
											break; // no (further) input
										if (!nItems)
											return -1; // invalid character in input
										if (nItems==1 || ::IsCharSpace(u)) // no unit specifier ...
											u='n'; // ... defaults to Nanoseconds
										if (const LPCTSTR pUnit=::StrChr( Units, u )){
											const char iUnit=pUnit-Units;
											if (iUnit>=iLastUnitUsed)
												return -1; // mustn't use bigger (or the same) units after smaller have been used
											iLastUnitUsed=iUnit;
										}else
											return-1; // unknown unit used
										if (tResult<0)
											tResult=0; // the user input is at least partially valid
										LONGLONG t64=t; // temporarily a 64-bit precision
										switch (u){
											case 's': t64=TIME_SECOND(t64);	break;
											case 'm': t64=TIME_MILLI(t64);	break;
											case 'µ':
											case 'u': t64=TIME_MICRO(t64);	break;
											default: t64=TIME_NANO(t64);	break;
										}
										if (tResult+t64>INT_MAX)
											return -1; // specified time is out of range
										tResult+=t64;
									}
									return tResult;
								}

								void DoDataExchange(CDataExchange *pDX) override{
									__super::DoDataExchange(pDX);
									if (pDX->m_bSaveAndValidate){
										tCenter=ParseTime();
										if (tCenter<0)
											pDX->Fail();
									}else{
										char iLargestUnit=-1;
										short nUnits[ARRAYSIZE(Units)]; // 0 = nanoseconds, 1 = microseconds, etc.
										::ZeroMemory( nUnits, sizeof(nUnits) );
										do{
											const div_t d=div( tCenter, 1000 );
											nUnits[++iLargestUnit]=d.rem;
											tCenter=d.quot;
										}while (tCenter>0);
										TCHAR buf[80], *p=buf;
										do{
											p+=::wsprintf( p, _T("%d%c "), nUnits[iLargestUnit], Units[iLargestUnit] );
										}while (iLargestUnit-->0);
										SetDlgItemText( ID_TIME, buf );
									}
								}

								LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
									switch (msg){
										case WM_COMMAND:
											if (wParam==MAKELONG(ID_TIME,EN_CHANGE))
												EnableDlgItem( IDOK, ParseTime()>=0 );
											break;
									}
									return __super::WindowProc( msg, wParam, lParam );
								}
							public:
								TLogTime tCenter;

								CGotoTimeDialog(CWnd *pParent,TLogTime timeCenter,TLogTime timeMax)
									: Utils::CRideDialog( IDR_TRACK_EDITOR_GOTO_TIME, pParent )
									, tMax(timeMax)
									, tCenter( std::min(timeCenter,timeMax) ) {
								}
							} d( this, timeEditor.GetCenterTime(), tr.GetTotalTime() );
							if (d.DoModal()==IDOK)
								timeEditor.SetCenterTime( d.tCenter );
							return TRUE;
						}
						case ID_INFORMATION:
							Utils::Information( _T("Each track item has a reference key, usually the first letter in its name. Keys to the left and right serve for navigation to previous and next item, respectively:\n\n- reference key I = Index, U/O = previous/next index\n- R = Revolution, E/T\n- B = Bad window, V/N\n- V = eVent, C/B\n- F = Fuzzy event, D/G\n- K = marK, J/L\n- D = meta-Data, S/F") );
							return TRUE;
						case ID_PREV:{
							BYTE i=0;
							for( const TLogTime tCenter=timeEditor.GetCenterTime(); i<tr.GetIndexCount()&&tCenter>tr.GetIndexTime(i); i++ );
							if (i>0)
								timeEditor.SetCenterTime( tr.GetIndexTime(i-1) );
							return TRUE;
						}
						case ID_NEXT:{
							BYTE i=0;
							for( const TLogTime tCenter=timeEditor.GetCenterTime(); i<tr.GetIndexCount()&&tCenter>=tr.GetIndexTime(i); i++ );
							if (i<tr.GetIndexCount())
								timeEditor.SetCenterTime( tr.GetIndexTime(i) );
							return TRUE;
						}
						case ID_META_PREV:{
							auto it=tr.GetMetaData().lower_bound( TLogTimeInterval(timeEditor.GetCenterTime(),INT_MAX) );
							if (it!=tr.GetMetaData().cbegin())
								timeEditor.SetCenterTime( (--it)->tStart );
							return TRUE;
						}
						case ID_META_NEXT:{
							const auto it=tr.GetMetaData().upper_bound( TLogTimeInterval(timeEditor.GetCenterTime(),INT_MAX) );
							if (it!=tr.GetMetaData().cend())
								timeEditor.SetCenterTime( it->tStart );
							return TRUE;
						}
						case ID_FILE_SHIFT_DOWN:
							timeEditor.SetCenterTime(
								(--timeEditor.GetParseEvents().FindByStart( timeEditor.GetCenterTime() ))->second->tStart
							);
							return TRUE;
						case ID_FILE_SHIFT_UP:
							timeEditor.SetCenterTime(
								timeEditor.GetParseEvents().FindByStart( timeEditor.GetCenterTime()+1 )->second->tStart
							);
							return TRUE;
						case ID_PREV_PANE:{
							auto it=timeEditor.GetParseEvents().FindByStart(timeEditor.GetCenterTime());
							do{
								it--;
							}while (!it->second->IsFuzzy());
							timeEditor.SetCenterTime( it->second->tStart );
							return TRUE;
						}
						case ID_NEXT_PANE:
							timeEditor.SetCenterTime(
								timeEditor.GetParseEvents().FindByStart( timeEditor.GetCenterTime()+1, TParseEvent::FUZZY_OK, TParseEvent::FUZZY_BAD )->second->tStart
							);
							return TRUE;
						case ID_RECORD_PREV:{
							WORD i=0;
							for( const TLogTime tCenter=timeEditor.GetCenterTime(); i<timeEditor.nRegions&&tCenter>timeEditor.pRegions[i].tStart; i++ );
							if (i>0)
								timeEditor.SetCenterTime( timeEditor.pRegions[i-1].tStart );
							return TRUE;
						}
						case ID_RECORD_NEXT:{
							WORD i=0;
							for( const TLogTime tCenter=timeEditor.GetCenterTime(); i<timeEditor.nRegions&&tCenter>=timeEditor.pRegions[i].tStart; i++ );
							if (i<timeEditor.nRegions)
								timeEditor.SetCenterTime( timeEditor.pRegions[i].tStart );
							return TRUE;
						}
						//case ID_DOWN:	// commented out as coped with already in WM_KEYDOWN handler
							//return TRUE;
						//case ID_UP:	// commented out as coped with already in WM_KEYDOWN handler
							//return TRUE;
						case ID_PREV_BAD:{
							const auto &badBlocks=iwInfo.badBlocks;
							POSITION pos=badBlocks.GetTailPosition();
							for( const TLogTime tCenter=timeEditor.GetCenterTime(); pos&&tCenter<=badBlocks.GetAt(pos).tStart; badBlocks.GetPrev(pos) );
							if (pos)
								timeEditor.SetCenterTime( badBlocks.GetAt(pos).tStart );
							return TRUE;
						}
						case ID_NEXT_BAD:{
							const auto &badBlocks=iwInfo.badBlocks;
							POSITION pos=badBlocks.GetHeadPosition();
							for( const TLogTime tCenter=timeEditor.GetCenterTime(); pos&&badBlocks.GetAt(pos).tStart<=tCenter; badBlocks.GetNext(pos) );
							if (pos)
								timeEditor.SetCenterTime( badBlocks.GetAt(pos).tStart );
							return TRUE;
						}
						case ID_REVOLUTION_PREV:{
							const TLogTime tCursor=timeEditor.GetClientCursorTime();
							BYTE r=1;
							while (tr.GetIndexTime(r)<tCursor)
								r++;
							// . try to navigate to corresponding InspectionWindow in the previous Revolution
							if (const PCInspectionWindow piwCursor=timeEditor.GetInspectionWindow(tCursor)) // has interpretation of the Times been made?
								if (piwCursor->uid!=INT_MIN){ // the same bits across various Revolutions already linked together via their UniqueIdentifiers?
									if (piwCursor->uid<0){ // cursor pointing at a Fuzzy region?
										Utils::Information(MSG_FUZZY_NAVIGATION);
										return TRUE;
									}
									const PCInspectionWindow piw=timeEditor.GetInspectionWindow( piwCursor->uid, tr.GetIndexTime(r-2), tr.GetIndexTime(r-1) );
									if (piw->uid!=piwCursor->uid){
										Utils::Information( _T("This bit has no counterpart in previous revolution.") );
										return TRUE;
									}
									const TLogTime tIwCursorStart=piwCursor->time, tIwCursorLength=piwCursor->GetLength();
									const TLogTime tIwStart=piw->time, tIwLength=piw->GetLength();
									const TLogTime t= tIwStart + (LONGLONG)(tCursor-tIwCursorStart)*tIwLength/tIwCursorLength;
									timeEditor.SetScrollTime( t - (tCursor-timeEditor.GetScrollTime()) );
									return TRUE;
								}
							// . navigate to proportionally corresponding Time in the previous Revolution
							const TLogTime tCurrRevLength=tr.GetIndexTime(r)-tr.GetIndexTime(r-1);
							const TLogTime tPrevRevLength=tr.GetIndexTime(r-1)-tr.GetIndexTime(r-2);
							const TLogTime t= tr.GetIndexTime(r-2) + (LONGLONG)(tCursor-tr.GetIndexTime(r-1))*tPrevRevLength/tCurrRevLength;
							timeEditor.SetScrollTime( t - (tCursor-timeEditor.GetScrollTime()) );
							return TRUE;
						}
						case ID_REVOLUTION_NEXT:{
							const TLogTime tCursor=timeEditor.GetClientCursorTime();
							BYTE r=tr.GetIndexCount();
							while (tCursor<tr.GetIndexTime(--r));
							// . try to navigate to corresponding InspectionWindow in the next Revolution
							if (const PCInspectionWindow piwCursor=timeEditor.GetInspectionWindow(tCursor)) // has interpretation of the Times been made?
								if (piwCursor->uid!=INT_MIN){ // the same bits across various Revolutions already linked together via their UniqueIdentifiers?
									if (piwCursor->uid<0){ // cursor pointing at a Fuzzy region?
										Utils::Information(MSG_FUZZY_NAVIGATION);
										return TRUE;
									}
									const PCInspectionWindow piw=timeEditor.GetInspectionWindow( piwCursor->uid, tr.GetIndexTime(r+1), tr.GetIndexTime(r+2) );
									if (piw->uid!=piwCursor->uid){
										Utils::Information( _T("This bit has no counterpart in next revolution.") );
										return TRUE;
									}
									const TLogTime tIwCursorStart=piwCursor->time, tIwCursorLength=piwCursor->GetLength();
									const TLogTime tIwStart=piw->time, tIwLength=piw->GetLength();
									const TLogTime t= tIwStart + (LONGLONG)(tCursor-tIwCursorStart)*tIwLength/tIwCursorLength;
									timeEditor.SetScrollTime( t - (tCursor-timeEditor.GetScrollTime()) );
									return TRUE;
								}
							// . navigate to proportionally corresponding Time in the next Revolution
							const TLogTime tCurrRevLength=tr.GetIndexTime(r+1)-tr.GetIndexTime(r);
							const TLogTime tNextRevLength=tr.GetIndexTime(r+2)-tr.GetIndexTime(r+1);
							const TLogTime t= tr.GetIndexTime(r+1) + (LONGLONG)(tCursor-tr.GetIndexTime(r))*tNextRevLength/tCurrRevLength;
							timeEditor.SetScrollTime( t - (tCursor-timeEditor.GetScrollTime()) );
							return TRUE;
						}
						case ID_INDICATOR_REC:
							if (const Utils::CSingleNumberDialog &&d=Utils::CSingleNumberDialog(
									_T("Decoding evaluation"),
									_T("Window bad if '1' off center more than [%]:"),
									PropGrid::Integer::TUpDownLimits::Percent, iwInfo.oneOkPercent, false, this
								)
							)
								if (iwInfo.oneOkPercent!=d.Value || !timeEditor.GetInspectionWindows()){
									iwInfo.oneOkPercent=d.Value;
									if (timeEditor.IsFeatureShown(TCursorFeatures::INSPECT))
										timeEditor.ToggleFeature(TCursorFeatures::INSPECT); // declaring the feature hidden ...
									timeEditor.SetInspectionWindows(nullptr); // ... and disposing previous
									iwInfo.badBlocks.RemoveAll();
									if (CBackgroundActionCancelable( CreateInspectionWindowList_thread, this, THREAD_PRIORITY_LOWEST ).Perform()!=ERROR_SUCCESS)
										return TRUE;
									timeEditor.ToggleFeature(TCursorFeatures::INSPECT);
									timeEditor.Invalidate();
								}
							return TRUE;
						case ID_CHART:{
							// modal display of scatter plot of time differences
							CImage::CTrackReader tr=this->tr;
							tr.SetCurrentTimeAndProfile( 0, tr.CreateResetProfile() );
							const auto deltaTimes=Utils::MakeCallocPtr<TLogPoint,TIndex>( tr.GetTimesCount() );
								PLogPoint pLastItem=deltaTimes;
								for( TLogTime t0=0; tr; pLastItem++ ){
									const TLogTime t = pLastItem->x = tr.ReadTime();
									pLastItem->y=t-t0; // delta Time
									t0=t;
								}
								const Utils::CRidePen dotPen( 2, 0x2020ff );
								auto deltaTimeSeries=CChartView::CXyPointSeries(
									pLastItem-deltaTimes, deltaTimes, dotPen
								);
							const auto indexTimes=Utils::MakeCallocPtr<TLogPoint,TIndex>( tr.GetIndexCount() );
								for( BYTE i=0; i<tr.GetIndexCount(); i++ ){
									auto &r=indexTimes[i];
										r.x=tr.GetIndexTime(i);
										r.y=TIME_MILLI(200); // should suffice for any Medium
								}
								if (indexTimes.length)
									std::swap( *indexTimes.get(), indexTimes.get()[tr.GetIndexCount()-1] ); // swapping first and last index pulse to disable application of percentiles, unapplicable to Index pulses
								auto indexTimeSeries=CChartView::CXyOrderedBarSeries(
									tr.GetIndexCount(), indexTimes, timeEditor.penIndex
								);
							const auto &peList=timeEditor.GetParseEvents();
								class CXyParseEventSeries:public CChartView::CXyGraphics{
									const Utils::CRideFont font;
									CBrush peBrushes[TParseEvent::LAST];
								public:
									const CImage::CTrackReader::CParseEventList &peList;

									CXyParseEventSeries(const CImage::CTrackReader::CParseEventList &peList)
										// ctor
										: peList(peList) , font(Utils::CRideFont::Std.CreateRotated(90)) {
										for( BYTE i=0; i<TParseEvent::LAST; i++ )
											peBrushes[i].CreateSolidBrush(
												Utils::GetBlendedColor( TParseEvent::TypeColors[i], COLOR_WHITE, 0.075f )
											);
										visible=peList.GetCount()>0;
									}
									TIndex GetItemCount() const{
										// returns the number of available items
										return peList.GetCount();
									}
									void DrawAsync(const CChartView::CPainter &p,HDC dc,const CActionProgress &ap) const override{
										// asynchronous drawing; always compare actual drawing ID with the one on start
										const auto &di=*(const CChartView::CXyDisplayInfo *)&p.di;
										const HGDIOBJ hFont0=::SelectObject( dc, font );
											const HGDIOBJ hBrush0=::SelectObject( dc, ::GetStockObject(NULL_BRUSH) );
												for( auto it=peList.GetIterator(); it; ap.IncrementProgress() ){
													const TParseEvent &pe=*it++->second;
													EXCLUSIVELY_LOCK(p);
													if (ap.Cancelled)
														break;
													CRect rc( di.GetClientUnits(pe.tStart,0).x, 0, di.GetClientUnits(pe.tEnd,0).x, di.GetClientUnits(0,1).y );
													::SelectObject( dc, peBrushes[pe.type] );
													::PatBlt( dc, rc.left,rc.top, rc.Width(),rc.Height(), 0xa000c9 ); // ternary raster operation "dest AND pattern"
													::SetTextColor( dc, Utils::GetBlendedColor(TParseEvent::TypeColors[pe.type],COLOR_WHITE,0.5f) );
													::DrawText( dc, pe.GetDescription(),-1, &rc, DT_LEFT|DT_BOTTOM|DT_SINGLELINE );
												}
											::SelectObject( dc, hBrush0 );
										::SelectObject( dc, hFont0 );
									}
								} peSeries(peList);
							class CScatterPlotDialog sealed:public CChartDialog{
								const CMainWindow::CDynMenu menu;
								const CImage::CTrackReader &tr;
								Revolution::TType revolution;
								const CChartView::PCGraphics graphicsBegin; // the following is an implicit array of Graphics ...
								CXyParseEventSeries &peSeries;
								CChartView::CXyOrderedBarSeries &indexSeries;
								CChartView::CXyPointSeries &deltaTimeSeries;
								CChartView::CXyDisplayInfo di; // ... and this indicates its end
							public:
								CScatterPlotDialog(const CImage::CTrackReader &tr,CChartView::CXyPointSeries &deltaTimeSeries,CXyParseEventSeries &peSeries,CChartView::CXyOrderedBarSeries &indexSeries)
									: CChartDialog(di)
									, menu(IDR_SCATTERPLOT)
									, tr(tr) , revolution(Revolution::NONE) // show whole Track
									, graphicsBegin(nullptr)
									, deltaTimeSeries(deltaTimeSeries) , peSeries(peSeries) , indexSeries(indexSeries)
									, di(
										CChartView::TMargin::Default,
										&graphicsBegin+1, (CChartView::PCGraphics *)&di-&graphicsBegin-1,
										Utils::CRideFont::StdBold,
										's', tr.GetTotalTime(), Utils::CTimeline::TimePrefixes,
										's', INT_MIN, Utils::CTimeline::TimePrefixes
									){
									m_bAutoMenuEnable=FALSE; // we are not set up for that
								}

								BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) override{
									const BOOL result=__super::OnCreateClient( lpcs, pContext );
									if (CMenu *const pFrameMenu=GetMenu())
										if (Utils::CRideContextMenu subMenu=*pFrameMenu->GetSubMenu(0)){
											// . prepend chart-specific menu
											subMenu.Prepend( IDR_SCATTERPLOT );
											// . update available Revolutions submenu
											for( TCHAR rev=Revolution::R2,cmdStr[16]; rev<indexSeries.GetItemCount(); rev++ ){ 
												::wsprintf( cmdStr, _T("%c\t%c"), '0'+rev, '0'+rev );
												subMenu.InsertAfter( ID_DEFAULT1+rev-2, MF_BYCOMMAND, ID_DEFAULT1+rev-1, cmdStr );
											}
										}
									return result;
								}

								BOOL PreTranslateMessage(PMSG pMsg) override{
									if (::TranslateAccelerator( m_hWnd, menu.hAccel, pMsg ))
										return TRUE;
									return __super::PreTranslateMessage(pMsg); // base
								}

								BOOL OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo) override{
									switch (nCode){
										case CN_UPDATE_COMMAND_UI:
											// update
											switch (nID){
												case ID_BUFFER:
													((CCmdUI *)pExtra)->SetCheck( deltaTimeSeries.visible );
													return TRUE;
												case ID_ROTATION:
													((CCmdUI *)pExtra)->SetCheck( indexSeries.visible );
													return TRUE;
												case ID_SYSTEM:
													((CCmdUI *)pExtra)->Enable( peSeries.peList.GetCount()>0 );
													((CCmdUI *)pExtra)->SetCheck( peSeries.visible );
													return TRUE;
												case ID_TRACK:
													((CCmdUI *)pExtra)->SetCheck( revolution==Revolution::NONE );
													return TRUE;
												case ID_DEFAULT1:
												case ID_DEFAULT2:
												case ID_DEFAULT3:
												case ID_DEFAULT4:
												case ID_DEFAULT5:
												case ID_DEFAULT6:
												case ID_DEFAULT7:
												case ID_DEFAULT8:
													((CCmdUI *)pExtra)->SetCheck( revolution==nID-ID_DEFAULT1 );
													return TRUE;
											}
											break;
										case CN_COMMAND:
											// command
											switch (nID){
												case ID_BUFFER:
													deltaTimeSeries.visible=!deltaTimeSeries.visible;
													Invalidate();
													return TRUE;
												case ID_ROTATION:
													indexSeries.visible=!indexSeries.visible;
													Invalidate();
													return TRUE;
												case ID_SYSTEM:
													peSeries.visible=!peSeries.visible;
													Invalidate();
													return TRUE;
												case ID_TRACK:
													di.xAxisFocus.a=0;
													di.xAxisFocus.z=LogValueMax;
													revolution=Revolution::NONE;
													Invalidate();
													return TRUE;
												case ID_DEFAULT1:
												case ID_DEFAULT2:
												case ID_DEFAULT3:
												case ID_DEFAULT4:
												case ID_DEFAULT5:
												case ID_DEFAULT6:
												case ID_DEFAULT7:
												case ID_DEFAULT8:
													if (nID-ID_DEFAULT1<tr.GetIndexCount()-1){
														di.xAxisFocus.a=tr.GetIndexTime(nID-ID_DEFAULT1);
														di.xAxisFocus.z=tr.GetIndexTime(nID-ID_DEFAULT1+1);
														revolution=Revolution::TType(nID-ID_DEFAULT1);
														Invalidate();
													}
													return TRUE;
											}
											break;
									}
									return __super::OnCmdMsg( nID, nCode, pExtra, pHandlerInfo );
								}
							} d( tr, deltaTimeSeries, peSeries, indexTimeSeries );
							d.ShowModal( caption, this, 800,600 );
							return TRUE;
						}
						case ID_HISTOGRAM:{
							CImage::CTrackReader tr=this->tr;
							TCHAR caption[80];
							TLogTime tBegin,tEnd;
							if (tr.GetIndexCount()>=2){ // will populate the Histogram with Times between first and last Index
								tBegin=tr.GetIndexTime(0), tEnd=tr.GetIndexTime(tr.GetIndexCount()-1);
								::wsprintf( caption, _T("Timing histogram for region between Indices 0 and %d"), tr.GetIndexCount()-1 );
							}else{ // will populate the Histogram with all Times the TrackReader provides
								tBegin=0, tEnd=tr.GetTotalTime();
								::lstrcpy( caption, _T("Timing histogram for whole track") );
							}
							tr.SetCurrentTimeAndProfile( tBegin, tr.CreateResetProfile() );
							const auto data=Utils::MakeCallocPtr<TLogPoint,TIndex>( tr.GetTimesCount() );
								PLogPoint pLastItem=data;
								for( TLogTime t0=tBegin; tr; pLastItem++ ){
									const TLogTime t = pLastItem->x = tr.ReadTime();
									if (tr.GetCurrentTime()>tEnd)
										break;
									pLastItem->y=t-t0;
									t0=t;
								}
								const Utils::CRidePen barPen( 2, 0x2020ff );
								const auto h=CChartView::CXyPointSeries(
									pLastItem-data, data, barPen
								).CreateYxHistogram(1); // due to integral rounding, individual Times may be off by 1; amending for this by merging neighboring Times
								pLastItem=data;
								for each( const auto &p in h ){
									pLastItem->x=p.first;
									pLastItem++->y=p.second;
								}
								const auto xySeries=CChartView::CXyOrderedBarSeries(
									pLastItem-data, data, barPen, _T("Flux")
								);
							const CChartView::PCGraphics graphics[]={ &xySeries };
							CChartDialog(
								CChartView::CXyDisplayInfo(
									CChartView::TMargin::Default,
									graphics, ARRAYSIZE(graphics),
									Utils::CRideFont::StdBold,
									's', INT_MIN, Utils::CTimeline::TimePrefixes,
									'\0', INT_MIN, Utils::CAxis::CountPrefixes
								)
							).ShowModal(
								caption, this, 800,600
							);
							return TRUE;
						}
						case ID_NUMBER:
							// decadic or hexa-decimal Byte values
							app.WriteProfileInt( INI_SECTION, INI_DECADIC,
								timeEditor.decadicByteValues=!timeEditor.decadicByteValues
							);
							timeEditor.Invalidate();
							return TRUE;
						case ID_FILE_SAVE_AS:
							// export Track timing
							class CExportDialog sealed:public Utils::CRideDialog{
								const CImage::CTrackReader &tr;
								int iContent, iRange, singleRevolution;
								TCHAR separator;

								void DoDataExchange(CDataExchange *pDX) override{
									// transferring data to and from controls
									const HWND hMedium=GetDlgItemHwnd(ID_MEDIUM);
									if (pDX->m_bSaveAndValidate){
										pDX->PrepareEditCtrl(ID_FILE);
										if (filename==ELLIPSIS){
											Utils::Information( _T("File not specified.") );
											pDX->Fail();
										}
										separator=GetDlgComboBoxSelectedValue(ID_SEPARATOR);
									}else{
										SetDlgItemText( ID_FILE, ELLIPSIS );
										static constexpr WORD SingleRevIds[]={ ID_ORDER, ID_NUMBER, 0 };
										if (EnableDlgItems( SingleRevIds, tr.GetIndexCount()>0 ))
											PopulateDlgComboBoxWithSequenceOfNumbers(
												ID_NUMBER,
												0, nullptr,
												tr.GetIndexCount()-1, nullptr
											);
										EnableDlgItem( ID_ROTATION, tr.GetIndexCount()>1 );
										CComboBox cb;
										cb.Attach( GetDlgItemHwnd(ID_SEPARATOR) );
											cb.SetItemData( cb.AddString(_T("Comma , (0x2C)")), ',' );
											cb.SetItemData( cb.AddString(_T("Semicolon ; (0x3B)")), ';' );
											cb.SetItemData( cb.AddString(_T("Space (0x20)")), ' ' );
											cb.SetItemData( cb.AddString(_T("Tab ¬ (0x09)")), '\t' );
											cb.SetItemData( cb.AddString(_T("Car.return CR (0x0D)")), '\r' );
											cb.SetItemData( cb.AddString(_T("Line feed LF (0x0A)")), '\n' );
											cb.SetCurSel(0);
										cb.Detach();
									}
									DDX_Radio( pDX, ID_TIME, iContent );
									DDX_Radio( pDX, ID_TRACK, iRange );
									DDX_CBIndex( pDX, ID_NUMBER, singleRevolution );
								}
								BOOL OnCommand(WPARAM wParam, LPARAM lParam) override{
									// WM_COMMAND message processing
									switch (wParam){
										case ID_FILE:{
											const CString newFilename=Utils::DoPromptSingleTypeFileName( _T("trackTiming") TXT_EXTENSION, TXT_FILTER, OFN_HIDEREADONLY|OFN_DONTADDTORECENT );
											if (!newFilename.IsEmpty())
												SetDlgItemCompactPath( ID_FILE, filename=newFilename );
											break;
										}
										case MAKELONG(ID_NUMBER,CBN_SELCHANGE):
											CheckRadioButton( ID_TRACK, ID_ORDER, ID_ORDER );
											break;
									}
									return __super::OnCommand( wParam, lParam );
								}
							public:
								CString filename;

								CExportDialog(const CImage::CTrackReader &tr)
									: Utils::CRideDialog( IDR_TRACK_EXPORT )
									, filename(ELLIPSIS)
									, tr(tr)
									, iContent(0) , iRange(0) // by default export all low-level timing
									, singleRevolution(0) {
								}

								static UINT AFX_CDECL Thread(PVOID pCancelableAction){
									// thread to export the Track timing
									CBackgroundActionCancelable &bac=*(PBackgroundActionCancelable)pCancelableAction;
									const CExportDialog &d=*(CExportDialog *)bac.GetParams();
									CImage::CTrackReader tr=d.tr;
									TLogTime tStart,tEnd;
									switch (d.iRange){
										default:
											ASSERT(FALSE); // we shouldn't end up here!
											//fallthrough
										case 0: // whole Track
											tStart=0;
											tEnd=tr.GetTotalTime();
											break;
										case 1: // all full Revolutions
											tStart=tr.GetIndexTime(0);
											tEnd=tr.GetLastIndexTime();
											break;
										case 2: // single Revolution identified by initial Index
											tStart=tr.GetIndexTime(d.singleRevolution);
											tEnd=tr.GetIndexTime(d.singleRevolution+1);
											break;
									}
									bac.SetProgressTarget( tEnd-tStart );
									tr.SetCurrentTime( tStart );
									CFile f;
									CFileException e;
									if (!f.Open( d.filename, CFile::modeCreate|CFile::modeWrite|CFile::typeBinary|CFile::shareExclusive ))
										return bac.TerminateWithError( e.m_cause );
									switch (d.iContent){
										case 0: // low-level timing
											for( TLogTime tPrev=tStart; tr; bac.UpdateProgress(tPrev-tStart) ){
												const TLogTime t=tr.ReadTime();
												Utils::WriteToFile( f, t-tPrev );
												if (t>=tEnd)
													break;
												Utils::WriteToFile( f, d.separator );
												tPrev=t;
											}
											break;
										case 1: // inspection window sizes
											for( ; tr; bac.UpdateProgress(tr.GetCurrentTime()-tStart) ){
												tr.ReadBit();
												Utils::WriteToFile( f, tr.GetCurrentProfile().iwTime );
												if (tr.GetCurrentTime()>=tEnd)
													break;
												Utils::WriteToFile( f, d.separator );
											}
											break;
									}
									return bac.TerminateWithSuccess();
								}
							} d( tr );
							if (d.DoModal()==IDOK)
								if (const TStdWinError err=CBackgroundActionCancelable( CExportDialog::Thread, &d, THREAD_PRIORITY_IDLE ).Perform())
									Utils::FatalError( _T("Can't export"), err );
							return TRUE;
					}
					break;
			}
			return FALSE;
		}

	public:
		CTrackEditor(const CImage::CTrackReader &tr,PCRegion pRegions,DWORD nRegions,UINT messageBoxButtons,bool initAllFeaturesOn,TLogTime tScrollTo,LPCTSTR captionFormat,va_list argList)
			// ctor
			// - base
			: Utils::CRideDialog( IDR_TRACK_EDITOR, CWnd::FromHandle(app.GetEnabledActiveWindow()) )
			// - initialization
			, caption( Utils::SimpleFormat(captionFormat,argList) )
			, tr(tr)
			, menu( IDR_TRACK_EDITOR ) , messageBoxButtons(messageBoxButtons) , initAllFeaturesOn(initAllFeaturesOn) , tInitScrollTo(tScrollTo)
			, timeEditor( tr, pRegions, nRegions )
			, hAutoscrollTimer(INVALID_HANDLE_VALUE) {
			iwInfo.oneOkPercent=50;
		}
	};









	BYTE __cdecl CImage::CTrackReader::ShowModal(PCRegion pRegions,DWORD nRegions,UINT messageBoxButtons,bool initAllFeaturesOn,TLogTime tScrollTo,LPCTSTR format,...) const{
		va_list argList;
		va_start( argList, format );
			const BYTE result=CTrackEditor( *this, pRegions, nRegions, messageBoxButtons, initAllFeaturesOn, tScrollTo, format, argList ).DoModal();
		va_end(argList);
		return result;
	}

	void __cdecl CImage::CTrackReader::ShowModal(LPCTSTR format,...) const{
		va_list argList;
		va_start( argList, format );
			CTrackEditor( *this, nullptr, 0, MB_OK, false, 0, format, argList ).DoModal();
		va_end(argList);
	}
