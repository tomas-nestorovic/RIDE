#include "stdafx.h"

	#define ZOOM_FACTOR_MAX	24

	class CTrackEditor sealed:public Utils::CRideDialog{
		const CImage::CTrackReader &tr;
		const LPCTSTR caption;
		CMainWindow::CDynMenu menu;
		PLogTime iwEndTimes;
		HANDLE hAutoscrollTimer;
		
		class CTimeEditor sealed:public CScrollView{
			Utils::CTimeline timeline;
			CImage::CTrackReader tr;
			TLogTime scrollTime;
			PCLogTime iwEndTimes; // inspection window end Times (aka. at which Time they end; the end determines the beginning of the immediately next inspection window)
			TLogTime draggedTime; // Time at which left mouse button has been pressed
			struct TTrackPainter sealed{
				const CBackgroundAction action;
				struct{
					CCriticalSection locker;
					WORD id;
					TLogTime timeA,timeZ; // visible region
					BYTE zoomFactor;
				} params;
				CEvent repaintEvent;

				static UINT AFX_CDECL Thread(PVOID _pBackgroundAction){
					// thread to paint the Track according to specified Parameters
					const PCBackgroundAction pAction=(PCBackgroundAction)_pBackgroundAction;
					CTimeEditor &rte=*(CTimeEditor *)pAction->GetParams();
					TTrackPainter &p=rte.painter;
					const Utils::CRidePen penIndex( 2, 0xff0000 );
					for( CImage::CTrackReader tr=rte.tr; true; ){
						// . waiting for next request to paint the Track
						p.repaintEvent.Lock();
						if (!::IsWindow(rte.m_hWnd)) // window closed?
							break;
						// . retrieving the Parameters
						CClientDC dc(&rte);
						p.params.locker.Lock();
							const WORD id=p.params.id;
							TLogTime timeA=p.params.timeA, timeZ=p.params.timeZ;
							rte.OnPrepareDC(&dc);
						p.params.locker.Unlock();
						if (timeA<0 && timeZ<0) // window closing?
							break;
						::SetBkMode( dc, TRANSPARENT );
						// . drawing inspection windows (if any)
						bool continuePainting=true;
						if (rte.iwEndTimes){
							// : determining the first visible inspection window
							DWORD L=0, R=rte.timeline.logTimeLength/tr.profile.iwTimeMin;
							do{
								const DWORD M=(L+R)/2;
								if (rte.iwEndTimes[L]<=timeA && timeA<rte.iwEndTimes[M])
									R=M;
								else
									L=M;
							}while (R-L>1);
							// : drawing visible inspection windows (avoiding the GDI coordinate limitations by moving the viewport origin)
							const CBrush brushDarker(0xE4E4B3), brushLighter(0xECECCE);
							TLogTime tA=rte.iwEndTimes[L], tZ;
							RECT rc={ 0, 1, 0, 40 };
							POINT org;
							::GetViewportOrgEx( dc, &org );
							const int nUnitsA=rte.timeline.GetUnitCount(tA);
							::SetViewportOrgEx( dc, nUnitsA*Utils::LogicalUnitScaleFactor+org.x, org.y, nullptr );
								while (continuePainting && tA<timeZ){
									rc.right=rte.timeline.GetUnitCount( tZ=rte.iwEndTimes[++L] )-nUnitsA;
									p.params.locker.Lock();
										if ( continuePainting=p.params.id==id )
											::FillRect( dc, &rc, L&1?brushLighter:brushDarker );
									p.params.locker.Unlock();
									tA=tZ, rc.left=rc.right;
								}
							::SetViewportOrgEx( dc, org.x, org.y, nullptr );
							if (!continuePainting) // new paint request?
								continue;
						}
						// . drawing Index pulses
						BYTE i=0;
						while (i<tr.GetIndexCount() && tr.GetIndexTime(i)<timeA) // skipping invisible indices before visible region
							i++;
						const HGDIOBJ hPen0=::SelectObject( dc, penIndex );
							::SetTextColor( dc, 0xff0000 );
							for( TCHAR buf[16]; continuePainting && i<tr.GetIndexCount() && tr.GetIndexTime(i)<timeZ; i++ ){ // visible indices
								const int x=rte.timeline.GetUnitCount( tr.GetIndexTime(i) );
								p.params.locker.Lock();
									if ( continuePainting=p.params.id==id ){
										::MoveToEx( dc, x,-60, nullptr );
										::LineTo( dc, x,60 );
										::TextOut( dc, x+4,-60, buf, ::wsprintf(buf,_T("Index %d"),i) );
									}
								p.params.locker.Unlock();
							}
						::SelectObject( dc, hPen0 );
						if (!continuePainting) // new paint request?
							continue;
						// . drawing Times
						tr.SetCurrentTime(timeA);
						for( TLogTime t=timeA; continuePainting && t<timeZ; t=tr.ReadTime() ){
							const int x=rte.timeline.GetUnitCount(t);
							p.params.locker.Lock();
								if ( continuePainting=p.params.id==id ){
									::MoveToEx( dc, x,0, nullptr );
									::LineTo( dc, x,30 );
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
				}
			} painter;

			void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override{
				// request to refresh the display of content
				SetScrollSizes(
					MM_TEXT,
					CSize( timeline.GetUnitCount(), 0 )
				);
			}

			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				switch (msg){
					case WM_CREATE:
						// window created
						painter.action.Resume();
						break;
					case WM_MOUSEACTIVATE:
						// preventing the focus from being stolen by the parent
						return MA_ACTIVATE;
					case WM_LBUTTONDOWN:
						// left mouse button pressed
						SetFocus();
						draggedTime=scrollTime+timeline.GetTime( GET_X_LPARAM(lParam)/Utils::LogicalUnitScaleFactor );
						break;
					case WM_LBUTTONUP:
						// left mouse button released
						draggedTime=-1;
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
						//fallthrough
					}
					case WM_MOUSEMOVE:
						// mouse moved
						if (draggedTime>0) // left mouse button pressed
							SetScrollTime(
								scrollTime
								+
								draggedTime
								-
								(  scrollTime+timeline.GetTime( GET_X_LPARAM(lParam)/Utils::LogicalUnitScaleFactor )  )
							);
						break;
					case WM_DESTROY:
						// window about to be destroyed
						// . letting the Painter finish normally
						painter.params.locker.Lock();
							painter.params.id++;
							painter.params.timeA = painter.params.timeZ = INT_MIN;
						painter.params.locker.Unlock();
						painter.repaintEvent.SetEvent();
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

			void OnPrepareDC(CDC *pDC,CPrintInfo *pInfo=nullptr) override{
				//
				// . base
				__super::OnPrepareDC(pDC,pInfo);
				// . scaling
				Utils::ScaleLogicalUnit(*pDC);
				// . changing the viewport
				CRect rc;
				GetClientRect(&rc);
				pDC->SetViewportOrg( -timeline.GetUnitCount(scrollTime)*Utils::LogicalUnitScaleFactor, rc.Height()/2 );
			}

			void OnDraw(CDC *pDC) override{
				// drawing the LogicalTimes
				// . drawing the Timeline
				const HDC dc=*pDC;
				::SetBkMode( dc, TRANSPARENT );
				painter.params.locker.Lock();
					painter.params.id++;
					timeline.Draw( dc, Utils::CRideFont::StdBold, &painter.params.timeA, &painter.params.timeZ );
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
				, scrollTime(0) , iwEndTimes(nullptr) {
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
				ScrollWindow(	// "base"
								(int)(timeline.GetUnitCount(scrollTime)*Utils::LogicalUnitScaleFactor) - (int)(si.nPos*Utils::LogicalUnitScaleFactor),
								0
							);
				scrollTime=t;
				painter.repaintEvent.SetEvent();
			}

			inline PCLogTime GetInspectionWindowEndTimes() const{
				return iwEndTimes;
			}

			inline void SetInspectionWindowEndTimes(PCLogTime iwEndTimes){
				this->iwEndTimes=iwEndTimes;
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
			// - setting up the Accuracy slider
			SendDlgItemMessage( ID_ACCURACY, TBM_SETRANGEMAX, FALSE, 2*AUTOSCROLL_HALF );
			SendDlgItemMessage( ID_ACCURACY, TBM_SETPOS, TRUE, AUTOSCROLL_HALF );
			SendDlgItemMessage( ID_ACCURACY, TBM_SETTHUMBLENGTH, TRUE, 50 );
			return TRUE;
		}

		BOOL PreTranslateMessage(PMSG pMsg) override{
			// pre-processing the Message
			return	::TranslateAccelerator( m_hWnd, menu.hAccel, pMsg );
		}

		#define AUTOSCROLL_TIMER_ID	0x100000

		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
			// window procedure
			switch (msg){
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
			CImage::CTrackReader tr=rte.tr;
			tr.SetCurrentTime(0);
			tr.profile.Reset();
			const auto nIwsMax=tr.GetTotalTime()/tr.profile.iwTimeMin+2;
			if (rte.iwEndTimes=(PLogTime)::calloc( sizeof(TLogTime), nIwsMax )){
				PLogTime t=rte.iwEndTimes;
				*t++=0; // beginning of the very first inspection window
				for( pAction->SetProgressTarget(tr.GetTotalTime()); tr; pAction->UpdateProgress(*t++=tr.GetCurrentTime()) )
					if (pAction->IsCancelled()){
						::free(rte.iwEndTimes), rte.iwEndTimes=nullptr;
						return ERROR_CANCELLED;
					}else
						tr.ReadBit();
				for( const PLogTime last=rte.iwEndTimes+nIwsMax; t<last; )
					*t++=INT_MAX; // flooding unused part of the buffer with sensible Times
				return pAction->TerminateWithError(ERROR_SUCCESS);
			}else
				return pAction->TerminateWithError(ERROR_NOT_ENOUGH_MEMORY);
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
						case ID_RECOGNIZE:
							pCmdUi->SetCheck( timeEditor.GetInspectionWindowEndTimes()!=nullptr );
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
						case ID_RECOGNIZE:
							if (timeEditor.GetInspectionWindowEndTimes()!=nullptr)
								timeEditor.SetInspectionWindowEndTimes(nullptr);
							else{
								if (iwEndTimes==nullptr)
									CBackgroundActionCancelable( CreateInspectionWindowList_thread, this, THREAD_PRIORITY_LOWEST ).Perform();
								timeEditor.SetInspectionWindowEndTimes(iwEndTimes);
							}
							return TRUE;
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
			, iwEndTimes(nullptr)
			, hAutoscrollTimer(INVALID_HANDLE_VALUE) {
		}

		~CTrackEditor(){
			// dtor
			if (iwEndTimes)
				::free(iwEndTimes);
		}
	};









	void CImage::CTrackReader::ShowModal(LPCTSTR caption) const{
		CTrackEditor( *this, caption ).DoModal();
	}
