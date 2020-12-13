#include "stdafx.h"

	#define ZOOM_FACTOR_MAX	24

	#define TIME_HEIGHT		30
	#define IW_HEIGHT		(TIME_HEIGHT+10)
	#define INDEX_HEIGHT	64
	#define LINE_EXTENSION	5
	#define SPACING_HEIGHT	(IW_HEIGHT+LINE_EXTENSION)
	#define IW_TIME_HEIGHT	(SPACING_HEIGHT+20)
	#define EVENT_HEIGHT	30

	typedef CImage::CTrackReader::TParseEvent TParseEvent,*PParseEvent;
	typedef const TParseEvent *PCParseEvent;

	class CTrackEditor sealed:public Utils::CRideDialog{
		const CImage::CTrackReader &tr;
		const LPCTSTR caption;
		CMainWindow::CDynMenu menu;
		HANDLE hAutoscrollTimer;

		enum TCursorFeatures:BYTE{
			TIME	=1,
			SPACING	=2,
			INSPECT	=4,
			STRUCT	=8,
			DEFAULT	= TIME//|SPACING
		};
		
		class CTimeEditor sealed:public CScrollView{
			Utils::CTimeline timeline;
			CImage::CTrackReader tr;
			TLogTime scrollTime;
			PCLogTime iwEndTimes; // inspection window end Times (aka. at which Time they end; the end determines the beginning of the immediately next inspection window)
			CImage::CTrackReader::PCParseEvent parseEvents;
			TLogTime draggedTime; // Time at which left mouse button has been pressed
			TLogTime cursorTime; // Time over which the cursor hovers
			BYTE cursorFeatures; // OR-ed TCursorFeatures values
			bool cursorFeaturesShown; // internally used for painting
			struct TTrackPainter sealed{
				const CBackgroundAction action;
				struct{
					mutable CCriticalSection locker;
					WORD id;
					TLogTime timeA,timeZ; // visible region
					BYTE zoomFactor;
				} params;
				mutable CEvent repaintEvent;

				static UINT AFX_CDECL Thread(PVOID _pBackgroundAction){
					// thread to paint the Track according to specified Parameters
					const PCBackgroundAction pAction=(PCBackgroundAction)_pBackgroundAction;
					const CTimeEditor &te=*(CTimeEditor *)pAction->GetParams();
					const TTrackPainter &p=te.painter;
					const CBrush iwBrushDarker(0xE4E4B3), iwBrushLighter(0xECECCE);
					const Utils::CRidePen penIndex( 2, 0xff0000 );
					const Utils::CRideBrush parseEventBrushes[TParseEvent::LAST]={
						TParseEvent::TypeColors[0],
						TParseEvent::TypeColors[1],
						TParseEvent::TypeColors[2],
						TParseEvent::TypeColors[3],
						TParseEvent::TypeColors[4],
						TParseEvent::TypeColors[5],
						TParseEvent::TypeColors[6],
						TParseEvent::TypeColors[7]
					};
					for( CImage::CTrackReader tr=te.tr; true; ){
						// . waiting for next request to paint the Track
						p.repaintEvent.Lock();
						if (!::IsWindow(te.m_hWnd)) // window closed?
							break;
						// . retrieving the Parameters
						CClientDC dc( const_cast<CTimeEditor *>(&te) );
						p.params.locker.Lock();
							const WORD id=p.params.id;
							TLogTime timeA=p.params.timeA, timeZ=p.params.timeZ;
							te.PrepareDC(&dc);
						p.params.locker.Unlock();
						if (timeA<0 && timeZ<0) // window closing?
							break;
						::SetBkMode( dc, TRANSPARENT );
						// . drawing inspection windows (if any)
						bool continuePainting=true;
						if (te.IsFeatureShown(TCursorFeatures::INSPECT)){
							// : determining the first visible inspection window
							int L=te.GetInspectionWindow(timeA);
							// : drawing visible inspection windows (avoiding the GDI coordinate limitations by moving the viewport origin)
							TLogTime tA=te.iwEndTimes[L], tZ;
							RECT rc={ 0, 1, 0, IW_HEIGHT };
							POINT org;
							::GetViewportOrgEx( dc, &org );
							const int nUnitsA=te.timeline.GetUnitCount(tA);
							::SetViewportOrgEx( dc, te.timeline.GetUnitCount(tA-tr.profile.iwTimeDefault/2)*Utils::LogicalUnitScaleFactor+org.x, org.y, nullptr );
								while (continuePainting && tA<timeZ){
									rc.right=te.timeline.GetUnitCount( tZ=te.iwEndTimes[++L] )-nUnitsA;
									p.params.locker.Lock();
										if ( continuePainting=p.params.id==id )
											::FillRect( dc, &rc, L&1?iwBrushLighter:iwBrushDarker );
									p.params.locker.Unlock();
									tA=tZ, rc.left=rc.right;
								}
							::SetViewportOrgEx( dc, org.x, org.y, nullptr );
							if (!continuePainting) // new paint request?
								continue;
						}
						// . drawing ParseEvents
						if (te.IsFeatureShown(TCursorFeatures::STRUCT)){
							PCParseEvent pe=te.GetParseEvents();
							const Utils::CRideFont &font=Utils::CRideFont::Std;
							const auto dcSettings0=::SaveDC(dc);
								POINT org;
								::GetViewportOrgEx( dc, &org );
								const int nUnitsA=te.timeline.GetUnitCount(te.GetScrollTime());
								::SetViewportOrgEx( dc, 0, org.y, nullptr );
								::SelectObject( dc, font );
								::SetBkMode( dc, OPAQUE );
								while (continuePainting && !pe->IsEmpty()){
									const TLogTime a=std::max(timeA,pe->tStart), z=std::min(timeZ,pe->tEnd);
									if (a<z){ // ParseEvent visible
										const int xa=te.timeline.GetUnitCount(a)-nUnitsA, xz=te.timeline.GetUnitCount(z)-nUnitsA;
										char label[80];
										switch (pe->type){
											case TParseEvent::SYNC_3BYTES:
												::wsprintfA( label, _T("0x%06X sync"), pe->dw);
												break;
											case TParseEvent::MARK_1BYTE:
												::wsprintfA( label, _T("0x%02X mark"), pe->dw );
												break;
											case TParseEvent::PREAMBLE:
												::wsprintfA( label, _T("Preamble (%d Bytes)"), pe->dw );
												break;
											case TParseEvent::DATA:
												::wsprintfA( label, _T("Data (%d Bytes)"), pe->dw);
												break;
											case TParseEvent::CRC_OK:
												::wsprintfA( label, _T("0x%X ok CRC"), pe->dw);
												break;
											case TParseEvent::CRC_BAD:
												::wsprintfA( label, _T("0x%X bad CRC"), pe->dw );
												break;
											default:
												::lstrcpyA( label, pe->lpszCustom );
												break;
										}
										RECT rcLabel={ te.timeline.GetUnitCount(pe->tStart)-nUnitsA, -1000, xz, -EVENT_HEIGHT-3 };
										p.params.locker.Lock();
											if ( continuePainting=p.params.id==id ){
												const BYTE i=std::min<BYTE>(TParseEvent::LAST-1,pe->type);
												::SelectObject( dc, parseEventBrushes[i] );
												::BitBlt( dc, xa,-EVENT_HEIGHT, xz-xa,EVENT_HEIGHT, CClientDC(nullptr), 0,0, 0xa000c9 ); // ternary raster operation "dest AND pattern" (excluding "src", hence the use of screen DC)
												::SetTextColor( dc, TParseEvent::TypeColors[i] );
												::DrawTextA( dc, label,-1, &rcLabel, DT_LEFT|DT_BOTTOM|DT_SINGLELINE );
											}
										p.params.locker.Unlock();
									}
									pe=pe->GetNext();
								}
							::RestoreDC( dc, dcSettings0 );
							if (!continuePainting) // new paint request?
								continue;
						}
						// . drawing Index pulses
						BYTE i=0;
						while (i<tr.GetIndexCount() && tr.GetIndexTime(i)<timeA) // skipping invisible indices before visible region
							i++;
						const auto dcSettings0=::SaveDC(dc);
							::SelectObject( dc, penIndex );
							::SetTextColor( dc, 0xff0000 );
							::SelectObject( dc, Utils::CRideFont::Std );
							for( TCHAR buf[16]; continuePainting && i<tr.GetIndexCount() && tr.GetIndexTime(i)<timeZ; i++ ){ // visible indices
								const int x=te.timeline.GetUnitCount( tr.GetIndexTime(i) );
								p.params.locker.Lock();
									if ( continuePainting=p.params.id==id ){
										::MoveToEx( dc, x,-INDEX_HEIGHT, nullptr );
										::LineTo( dc, x,INDEX_HEIGHT );
										::TextOut( dc, x+4,-INDEX_HEIGHT, buf, ::wsprintf(buf,_T("Index %d"),i) );
									}
								p.params.locker.Unlock();
							}
						::RestoreDC(dc,dcSettings0);
						if (!continuePainting) // new paint request?
							continue;
						// . drawing Times
						tr.SetCurrentTime(timeA);
						for( TLogTime t=timeA; continuePainting && t<=timeZ; t=tr.ReadTime() ){
							const int x=te.timeline.GetUnitCount(t);
							p.params.locker.Lock();
								if ( continuePainting=p.params.id==id ){
									::MoveToEx( dc, x,0, nullptr );
									::LineTo( dc, x,TIME_HEIGHT );
								}
							p.params.locker.Unlock();
						}
						if (!continuePainting) // new paint request?
							continue;
					}
					return ERROR_SUCCESS;
				}

				TTrackPainter(const CTimeEditor &te)
					// ctor
					: action( Thread, &te, THREAD_PRIORITY_IDLE ) {
					params.timeA = params.timeZ = 0;
				}
			} painter;

			void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override{
				// request to refresh the display of content
				SetScrollSizes(
					MM_TEXT,
					CSize( timeline.GetUnitCount(), 0 )
				);
			}

			int GetInspectionWindow(TLogTime logTime) const{
				// returns the index of inspection window at specified LogicalTime
				ASSERT( iwEndTimes!=nullptr );
				int L=0, R=timeline.logTimeLength/tr.profile.iwTimeMin;
				do{
					const DWORD M=(L+R)/2;
					if (iwEndTimes[L]<=logTime && logTime<iwEndTimes[M])
						R=M;
					else
						L=M;
				}while (R-L>1);
				return L;
			}

			void PaintCursorFeaturesInverted(bool show){
				// paints CursorTime by inverting pixels; painting twice the same CursorTime shows nothing
				if ((show^cursorFeaturesShown)!=0 && cursorFeatures!=0){
					CClientDC dc(this);
					PrepareDC(&dc);
					::SetROP2( dc, R2_NOT );
					const auto &font=Utils::CRideFont::Std;
					const HDC dcMem=::CreateCompatibleDC(dc);
						::SetTextColor( dcMem, COLOR_WHITE );
						::SetBkMode( dcMem, TRANSPARENT );
						Utils::ScaleLogicalUnit(dcMem);
						const HGDIOBJ hFont0=::SelectObject( dcMem, font );
							TCHAR label[32];
							const int x=timeline.GetUnitCount(cursorTime);
							// . painting vertical line to indicate current position on the Timeline
							if (IsFeatureShown(TCursorFeatures::TIME)){
								::MoveToEx( dc, x, -500, nullptr );
								::LineTo( dc, x, 500 );
								const int nLabelChars=timeline.TimeToReadableString(cursorTime,label);
								const SIZE sz=font.GetTextSize( label, nLabelChars );
								const HGDIOBJ hBmp0=::SelectObject( dcMem, ::CreateCompatibleBitmap(dc,sz.cx,sz.cy) );
									::TextOut( dcMem, 0,0, label,nLabelChars );
									::BitBlt( dc, x+2,-80, sz.cx,sz.cy, dcMem, 0,0, SRCINVERT );
								::DeleteObject( ::SelectObject(dcMem,hBmp0) );
							}
							// . painting space between neighboring Times at current position
							if (IsFeatureShown(TCursorFeatures::SPACING) && cursorTime<timeline.logTimeLength){
								tr.SetCurrentTime(cursorTime);
								tr.TruncateCurrentTime();
								const TLogTime a=tr.GetCurrentTime(), z=tr.ReadTime();
								const int xa=timeline.GetUnitCount(a), xz=timeline.GetUnitCount(z);
								const int nLabelChars=timeline.TimeToReadableString(z-a,label);
								const SIZE sz=font.GetTextSize( label, nLabelChars );
								const HGDIOBJ hBmp0=::SelectObject( dcMem, ::CreateCompatibleBitmap(dc,sz.cx,sz.cy) );
									::TextOut( dcMem, 0,0, label,nLabelChars );
									::BitBlt( dc, (xz+xa-sz.cx)/2,SPACING_HEIGHT+LINE_EXTENSION/2, sz.cx,sz.cy, dcMem, 0,0, SRCINVERT );
								::DeleteObject( ::SelectObject(dcMem,hBmp0) );
								::MoveToEx( dc, xa, TIME_HEIGHT, nullptr );
								::LineTo( dc, xa, SPACING_HEIGHT+LINE_EXTENSION );
								::MoveToEx( dc, xz, TIME_HEIGHT, nullptr );
								::LineTo( dc, xz, SPACING_HEIGHT+LINE_EXTENSION );
								::MoveToEx( dc, xa-LINE_EXTENSION, SPACING_HEIGHT, nullptr );
								::LineTo( dc, xz+LINE_EXTENSION, SPACING_HEIGHT );
							}
							// . painting inspection window size at current position
							if (IsFeatureShown(TCursorFeatures::INSPECT) && cursorTime<timeline.logTimeLength){
								const int i=GetInspectionWindow(cursorTime+tr.profile.iwTimeDefault/2);
								const TLogTime a=iwEndTimes[i]-tr.profile.iwTimeDefault/2, z=iwEndTimes[i+1]-tr.profile.iwTimeDefault/2;
								const int xa=timeline.GetUnitCount(a), xz=timeline.GetUnitCount(z);
								const int nLabelChars=timeline.TimeToReadableString(z-a,label);
								const SIZE sz=font.GetTextSize( label, nLabelChars );
								const HGDIOBJ hBmp0=::SelectObject( dcMem, ::CreateCompatibleBitmap(dc,sz.cx,sz.cy) );
									::TextOut( dcMem, 0,0, label,nLabelChars );
									::BitBlt( dc, (xz+xa-sz.cx)/2,IW_TIME_HEIGHT+LINE_EXTENSION/2, sz.cx,sz.cy, dcMem, 0,0, SRCINVERT );
								::DeleteObject( ::SelectObject(dcMem,hBmp0) );
								::MoveToEx( dc, xa, IW_HEIGHT, nullptr );
								::LineTo( dc, xa, IW_TIME_HEIGHT+LINE_EXTENSION );
								::MoveToEx( dc, xz, IW_HEIGHT, nullptr );
								::LineTo( dc, xz, IW_TIME_HEIGHT+LINE_EXTENSION );
								::MoveToEx( dc, xa-LINE_EXTENSION, IW_TIME_HEIGHT, nullptr );
								::LineTo( dc, xz+LINE_EXTENSION, IW_TIME_HEIGHT );
							}
						::SelectObject( dcMem, hFont0 );
					::DeleteDC(dcMem);
				}
				cursorFeaturesShown=show;
			}

			inline TLogTime ClientPixelToTime(int pixel) const{
				return	std::min(
							scrollTime + timeline.GetTime( pixel/Utils::LogicalUnitScaleFactor ),
							timeline.logTimeLength
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
						POINT cursor;
						GetCursorPos(&cursor);
						ScreenToClient(&cursor);
						const int nUnitsX=cursor.x/Utils::LogicalUnitScaleFactor;
						if ((short)HIWORD(wParam)<0 && timeline.zoomFactor<ZOOM_FACTOR_MAX)
							// want zoom out and still can zoom out
							SetZoomFactor( timeline.zoomFactor+1, nUnitsX );
						else if ((short)HIWORD(wParam)>0 && timeline.zoomFactor>0)
							// want zoom in and still can zoom in
							SetZoomFactor( timeline.zoomFactor-1, nUnitsX );
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
								SetScrollTime(timeline.logTimeLength);
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
							painter.params.timeA = painter.params.timeZ = INT_MIN;
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
				SetScrollTime( timeline.GetTime(si.nPos) );
				return TRUE;
			}

			void PrepareDC(CDC *pDC) const{
				//
				// . scaling
				Utils::ScaleLogicalUnit(*pDC);
				// . changing the viewport
				CRect rc;
				GetClientRect(&rc);
				pDC->SetViewportOrg( -timeline.GetUnitCount(scrollTime)*Utils::LogicalUnitScaleFactor, rc.Height()/2 );
			}

			void OnPrepareDC(CDC *pDC,CPrintInfo *pInfo=nullptr) override{
				//
				__super::OnPrepareDC(pDC,pInfo);
				PrepareDC(pDC);
			}

			void OnDraw(CDC *pDC) override{
				// drawing the LogicalTimes
				// . hiding CursorTime information
				PaintCursorFeaturesInverted(false);
				// . drawing the Timeline
				const HDC dc=*pDC;
				::SetBkMode( dc, TRANSPARENT );
				painter.params.locker.Lock();
					painter.params.id++;
					timeline.Draw( dc, Utils::CRideFont::Std, &painter.params.timeA, &painter.params.timeZ );
					painter.params.zoomFactor=timeline.zoomFactor;
				painter.params.locker.Unlock();
				// . drawing the rest in parallel thread due to computational complexity if painting the whole Track
				painter.repaintEvent.SetEvent();
			}

			void PostNcDestroy() override{
				// self-destruction
				//nop (View destroyed by its owner)
			}
		public:
			CTimeEditor(const CImage::CTrackReader &tr)
				// ctor
				: timeline( tr.GetTotalTime(), 1, 10 )
				, tr(tr)
				, painter(*this)
				, draggedTime(-1)
				, cursorTime(-1) , cursorFeaturesShown(false) , cursorFeatures(TCursorFeatures::DEFAULT)
				, scrollTime(0) , iwEndTimes(nullptr) , parseEvents(nullptr) {
			}
			
			~CTimeEditor(){
				// dtor
				if (parseEvents)
					::free((PVOID)parseEvents);
				if (iwEndTimes)
					::free((PVOID)iwEndTimes);
			}

			void OnInitialUpdate() override{
				// called after window creation
				__super::OnInitialUpdate();
				SetZoomFactor( timeline.zoomFactor, 0 ); // initialization
				painter.action.Resume();
			}

			inline const Utils::CTimeline &GetTimeline() const{
				return timeline;
			}

			void SetZoomFactor(BYTE newZoomFactor,int focusUnitX){
				const TLogTime t=scrollTime+timeline.GetTime(focusUnitX);
				timeline.zoomFactor=newZoomFactor;
				OnUpdate( nullptr, 0, nullptr );
				SetScrollTime(  timeline.GetTime( timeline.GetUnitCount(t)-focusUnitX )  );
				Invalidate();
				CRect rc;
				GetClientRect(&rc);
				m_lineDev.cx=std::max( timeline.GetUnitCount(tr.profile.iwTimeDefault)*Utils::LogicalUnitScaleFactor, 1.f ); // in device units
				m_pageDev.cx=rc.Width()*.9f; // in device units
			}

			void SetZoomFactorCenter(BYTE newZoomFactor){
				CRect rc;
				GetClientRect(&rc);
				SetZoomFactor( newZoomFactor, rc.Width()/(Utils::LogicalUnitScaleFactor*2) );
			}

			inline TLogTime GetScrollTime() const{
				return scrollTime;
			}

			void SetScrollTime(TLogTime t){
				if (t<0) t=0;
				else if (t>timeline.logTimeLength) t=timeline.logTimeLength;
				SCROLLINFO si={ sizeof(si) };
					si.fMask=SIF_POS;
					si.nPos=timeline.GetUnitCount(t);
				SetScrollInfo( SB_HORZ, &si, TRUE );
				painter.params.locker.Lock();
					painter.params.id++; // stopping current painting
				painter.params.locker.Unlock();
				PaintCursorFeaturesInverted(false);
				ScrollWindow(	// "base"
								(int)(timeline.GetUnitCount(scrollTime)*Utils::LogicalUnitScaleFactor) - (int)(si.nPos*Utils::LogicalUnitScaleFactor),
								0
							);
				scrollTime=t;
				painter.repaintEvent.SetEvent();
			}

			TLogTime GetCenterTime() const{
				CRect rc;
				GetClientRect(&rc);
				return scrollTime+timeline.GetTime( rc.Width()/(Utils::LogicalUnitScaleFactor*2.f) );
			}

			void SetCenterTime(TLogTime t){
				scrollTime=0; // base time for GetCenterTime
				SetScrollTime( t-GetCenterTime() );
			}

			inline PCLogTime GetInspectionWindowEndTimes() const{
				return iwEndTimes;
			}

			inline void SetInspectionWindowEndTimes(PCLogTime iwEndTimes){
				ASSERT( this->iwEndTimes==nullptr ); // can set only once
				this->iwEndTimes=iwEndTimes; // now responsible for disposing!
			}

			inline CImage::CTrackReader::PCParseEvent GetParseEvents() const{
				return parseEvents;
			}

			void SetParseEvents(CImage::CTrackReader::PCParseEvent buffer){
				ASSERT( parseEvents==nullptr ); // can set only once
				const auto nBytes=(PCBYTE)buffer->GetLast()->GetNext()-(PCBYTE)buffer+sizeof(CImage::CTrackReader::TParseEvent); // "+sizeof" = including the terminal None ParseEvent
				parseEvents=(CImage::CTrackReader::PCParseEvent)::memcpy( ::malloc(nBytes), buffer, nBytes );
			}

			inline bool IsFeatureShown(TCursorFeatures cf) const{
				return (cursorFeatures&cf)!=0;
			}

			void ToggleFeature(TCursorFeatures cf){
				painter.params.locker.Lock();
					const bool cfs0=cursorFeaturesShown;
					PaintCursorFeaturesInverted(false);
						if (IsFeatureShown(cf))
							cursorFeatures&=~cf;
						else
							cursorFeatures|=cf;
					PaintCursorFeaturesInverted(cfs0);
				painter.params.locker.Unlock();
			}

			void ShowAllFeatures(){
				painter.params.locker.Lock();
					PaintCursorFeaturesInverted(false);
						cursorFeatures=-1;
					Invalidate();
				painter.params.locker.Unlock();
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
			if (rte.timeEditor.GetInspectionWindowEndTimes()!=nullptr) // already set?
				return pAction->TerminateWithSuccess();
			CImage::CTrackReader tr=rte.tr;
			tr.SetCurrentTime(0);
			tr.profile.Reset();
			const auto nIwsMax=tr.GetTotalTime()/tr.profile.iwTimeMin+2;
			if (const PLogTime iwEndTimes=(PLogTime)::calloc( sizeof(TLogTime), nIwsMax )){
				PLogTime t=iwEndTimes;
				*t++=0; // beginning of the very first inspection window
				for( pAction->SetProgressTarget(tr.GetTotalTime()); tr; pAction->UpdateProgress(*t++=tr.GetCurrentTime()) )
					if (pAction->IsCancelled()){
						::free(iwEndTimes);
						return ERROR_CANCELLED;
					}else
						tr.ReadBit();
				for( const PLogTime last=iwEndTimes+nIwsMax; t<last; )
					*t++=INT_MAX; // flooding unused part of the buffer with sensible Times
				rte.timeEditor.SetInspectionWindowEndTimes(iwEndTimes);
				return pAction->TerminateWithSuccess();
			}else
				return pAction->TerminateWithError(ERROR_NOT_ENOUGH_MEMORY);
		}

		static UINT AFX_CDECL CreateParseEventsList_thread(PVOID _pCancelableAction){
			// thread to create list of inspection windows used to recognize data in the Track
			const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
			CTrackEditor &rte=*(CTrackEditor *)pAction->GetParams();
			if (rte.timeEditor.GetParseEvents()!=nullptr) // already set?
				return pAction->TerminateWithSuccess();
			CImage::CTrackReader tr=rte.tr;
			TParseEvent peBuffer[5000]; // capacity should suffice for any Track of any platform
			const auto iii=sizeof(peBuffer);
			BYTE dummy[16384]; // big enough to contain data of Sector of any floppy type
			TSectorId ids[DEVICE_REVOLUTIONS_MAX*(TSector)-1]; TLogTime idEnds[DEVICE_REVOLUTIONS_MAX*(TSector)-1]; CImage::CTrackReader::TProfile idProfiles[DEVICE_REVOLUTIONS_MAX*(TSector)-1];
			const WORD nSectorsFound=tr.Scan( ids, idEnds, idProfiles, (TFdcStatus *)dummy, peBuffer );
			pAction->SetProgressTarget(tr.GetTotalTime());
			for( WORD s=0; s<nSectorsFound; s++,pAction->UpdateProgress(tr.GetCurrentTime()) ){
				// . if cancelled, we are done
				if (pAction->IsCancelled())
					return ERROR_CANCELLED;
				// . getting ParseEvents in Sector data
				TParseEvent peData[32]; // should suffice for Events in data part of Sectors of any platform
				tr.ReadData( idEnds[s], idProfiles[s], CImage::GetOfficialSectorLength(ids[s].lengthCode), dummy, peData );
				// . merging the two lists of ParseEvents (MergeSort)
				int nBufferEventBytes=(PCBYTE)peBuffer->GetLast()->GetNext()-(PCBYTE)peBuffer+sizeof(TParseEvent); // including the terminating Null ParseEvent
				int nDataEventBytes=(PCBYTE)peData->GetLast()->GetNext()-(PCBYTE)peData;
				for( PCParseEvent pe1=peBuffer,pe2=peData; nBufferEventBytes&&nDataEventBytes; pe1=pe1->GetNext() )
					if (pe1->tStart<=pe2->tStart) // should never be equal, but just in case
						nBufferEventBytes-=pe1->GetSize();
					else{
						const BYTE pe2Size=pe2->GetSize();
						::memmove( (PBYTE)pe1+pe2Size, pe1, nBufferEventBytes );
						::memcpy( (PBYTE)pe1, pe2, pe2Size );
						nDataEventBytes-=pe2Size, pe2=pe2->GetNext();
					}
			}
			rte.timeEditor.SetParseEvents(peBuffer);
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
							pCmdUi->Enable( timeEditor.GetTimeline().zoomFactor>0 );
							return TRUE;
						case ID_ZOOM_OUT:
							pCmdUi->Enable( timeEditor.GetTimeline().zoomFactor<ZOOM_FACTOR_MAX );
							//fallthrough
						case ID_ZOOM_FIT:
						case IDCANCEL:
							return TRUE;
						case ID_TIME:
							pCmdUi->SetCheck( timeEditor.IsFeatureShown(TCursorFeatures::TIME) );
							return TRUE;
						case ID_GAP:
							pCmdUi->SetCheck( timeEditor.IsFeatureShown(TCursorFeatures::SPACING) );
							return TRUE;
						case ID_RECOGNIZE:
							pCmdUi->SetCheck( timeEditor.IsFeatureShown(TCursorFeatures::INSPECT) );
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
						case ID_FILE_SHIFT_DOWN:
							pCmdUi->Enable( timeEditor.GetParseEvents() && timeEditor.GetCenterTime()>timeEditor.GetParseEvents()->tStart );
							return TRUE;
						case ID_FILE_SHIFT_UP:
							pCmdUi->Enable( timeEditor.GetParseEvents() && timeEditor.GetCenterTime()<timeEditor.GetParseEvents()->GetLast()->tStart );
							return TRUE;
						case ID_DOWN:
							pCmdUi->Enable( timeEditor.GetScrollTime()>0 );
							return TRUE;
						case ID_UP:
							pCmdUi->Enable( timeEditor.GetScrollTime()<tr.GetTotalTime() );
							return TRUE;
					}
					break;
				}
				case CN_COMMAND:
					// command
					switch (nID){
						case ID_ZOOM_IN:
							timeEditor.SetZoomFactorCenter( timeEditor.GetTimeline().zoomFactor-1 );
							return TRUE;
						case ID_ZOOM_OUT:
							timeEditor.SetZoomFactorCenter( timeEditor.GetTimeline().zoomFactor+1 );
							return TRUE;
						case ID_ZOOM_FIT:{
							CRect rc;
							timeEditor.GetClientRect(&rc);
							timeEditor.SetZoomFactorCenter( timeEditor.GetTimeline().GetZoomFactorToFitWidth(rc.Width()/Utils::LogicalUnitScaleFactor,ZOOM_FACTOR_MAX) );
							return TRUE;
						}
						case IDCANCEL:
							EndDialog(nID);
							return TRUE;
						case ID_TIME:
							timeEditor.ToggleFeature(TCursorFeatures::TIME);
							return TRUE;
						case ID_GAP:
							timeEditor.ToggleFeature(TCursorFeatures::SPACING);
							return TRUE;
						case ID_RECOGNIZE:
							if (!timeEditor.IsFeatureShown(TCursorFeatures::INSPECT)) // currently hidden, so want now show the Feature
								if (timeEditor.GetInspectionWindowEndTimes()==nullptr) // data to display not yet received
									if (CBackgroundActionCancelable( CreateInspectionWindowList_thread, this, THREAD_PRIORITY_LOWEST ).Perform()!=ERROR_SUCCESS)
										return TRUE;
							timeEditor.ToggleFeature(TCursorFeatures::INSPECT);
							timeEditor.Invalidate();
							return TRUE;
						case ID_SYSTEM:
							if (!timeEditor.IsFeatureShown(TCursorFeatures::STRUCT)) // currently hidden, so want now show the Feature
								if (timeEditor.GetParseEvents()==nullptr) // data to display not yet received
									if (CBackgroundActionCancelable( CreateParseEventsList_thread, this, THREAD_PRIORITY_LOWEST ).Perform()!=ERROR_SUCCESS)
										return TRUE;
							timeEditor.ToggleFeature(TCursorFeatures::STRUCT);
							timeEditor.Invalidate();
							return TRUE;
						case ID_TRACK:{
							CBackgroundMultiActionCancelable bmac(THREAD_PRIORITY_LOWEST);
								bmac.AddAction( CreateInspectionWindowList_thread, this, _T("Inspection") );
								bmac.AddAction( CreateParseEventsList_thread, this, _T("Structure") );
							if (bmac.Perform()==ERROR_SUCCESS)
								timeEditor.ShowAllFeatures();
							return TRUE;
						}
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
						case ID_FILE_SHIFT_DOWN:{
							PCParseEvent pe=timeEditor.GetParseEvents(), prev=nullptr;
							for( const TLogTime tCenter=timeEditor.GetCenterTime(); !pe->IsEmpty()&&tCenter>pe->tStart; pe=pe->GetNext() )
								prev=pe;
							if (prev!=nullptr)
								timeEditor.SetCenterTime( prev->tStart );
							return TRUE;
						}
						case ID_FILE_SHIFT_UP:{
							PCParseEvent pe=timeEditor.GetParseEvents();
							for( const TLogTime tCenter=timeEditor.GetCenterTime(); !pe->IsEmpty()&&tCenter>=pe->tStart; pe=pe->GetNext() );
							if (!pe->IsEmpty())
								timeEditor.SetCenterTime( pe->tStart );
							return TRUE;
						}
						//case ID_DOWN:	// commented out as coped with already in WM_KEYDOWN handler
							//return TRUE;
						//case ID_UP:	// commented out as coped with already in WM_KEYDOWN handler
							//return TRUE;
					}
					break;
			}
			return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
		}

	public:
		CTrackEditor(const CImage::CTrackReader &tr,LPCTSTR caption)
			// ctor
			// - base
			: Utils::CRideDialog(IDR_TRACK_EDITOR)
			// - initialization
			, tr(tr)
			, caption(caption) , menu( IDR_TRACK_EDITOR )
			, timeEditor(tr)
			, hAutoscrollTimer(INVALID_HANDLE_VALUE) {
		}
	};









	void CImage::CTrackReader::ShowModal(LPCTSTR caption) const{
		CTrackEditor( *this, caption ).DoModal();
	}
