#include "stdafx.h"

	CBackgroundAction::CBackgroundAction(){
		// ctor
	}

	CBackgroundAction::CBackgroundAction(AFX_THREADPROC fnAction,LPCVOID actionParams,int actionThreadPriority){
		// ctor
		BeginAnother( fnAction, actionParams, actionThreadPriority );
	}

	CBackgroundAction::~CBackgroundAction(){
		// dtor
		// - forced termination of the Worker (if this cannot be done, it's necessary to sort it out in descendant's dtor)
		if (pWorker)
			::TerminateThread( *this, ERROR_SUCCESS );
	}



	LPCVOID CBackgroundAction::GetParams() const{
		// returns the Parameters which the Worker was launched with
		return fnParams;
	}

	void CBackgroundAction::Resume() const{
		// resumes Worker's activity
		if (pWorker)
			pWorker->ResumeThread();
	}
	void CBackgroundAction::Suspend() const{
		// suspends Worker's activity
		if (pWorker)
			pWorker->SuspendThread();
	}

	void CBackgroundAction::BeginAnother(AFX_THREADPROC fnAction,LPCVOID actionParams,int actionThreadPriority){
		// waits until the current Worker has finished and launches another Worker
		// - waiting
		if (pWorker)
			::WaitForSingleObject( *this, INFINITE );
		// - launching new
		fnParams=actionParams;
		pWorker.reset(
			AfxBeginThread( fnAction, this, actionThreadPriority, 0, CREATE_SUSPENDED ) // the object must have a vtable for the keyword "this" to work in AfxBeginThread
		);
		pWorker->m_bAutoDelete=FALSE;
	}

	CBackgroundAction::operator HANDLE() const{
		// gets the Worker thread handle
		return pWorker ? pWorker->m_hThread : INVALID_HANDLE_VALUE;
	}









	CBackgroundActionCancelable::CBackgroundActionCancelable(UINT dlgResId)
		// ctor
		: CDialog( dlgResId, app.m_pMainWnd )
		, bCancelled(false) , lastState(0)
		, progressTarget(INT_MAX) {
	}

	CBackgroundActionCancelable::CBackgroundActionCancelable(AFX_THREADPROC fnAction,LPCVOID actionParams,int actionThreadPriority)
		// ctor
		: CDialog( IDR_ACTION_PROGRESS, app.m_pMainWnd )
		, bCancelled(false)
		, progressTarget(INT_MAX) {
		BeginAnother( fnAction, actionParams, actionThreadPriority );
	}




	BOOL CBackgroundActionCancelable::OnInitDialog(){
		// dialog initialization
		// - launching the Worker
		Resume();
		return __super::OnInitDialog();
	}

	LRESULT CBackgroundActionCancelable::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		if (msg==WM_COMMAND && wParam==IDCANCEL)
			bCancelled=true; // cancelling the Worker and whole dialog
		return __super::WindowProc(msg,wParam,lParam);
	}

	TStdWinError CBackgroundActionCancelable::Perform(){
		// returns the Worker's result; when performing, the actual progress is shown in a modal window
		// - showing modal dialog and performing the Action
		DoModal();
		// - waiting for the already running Worker
		::WaitForSingleObject( *this, INFINITE );
		// - returning the Result
		TStdWinError result;
		::GetExitCodeThread( *this, &result );
		return result;
	}

	bool CBackgroundActionCancelable::IsCancelled() const volatile{
		// True <=> the Worker can continue (user hasn't cancelled it), otherwise False
		return bCancelled;
	}

	void CBackgroundActionCancelable::SetProgressTarget(int targetState){
		// sets Worker's target progress state, "100% completed"
		SendDlgItemMessage( ID_STATE, PBM_SETRANGE32, 0, progressTarget=targetState );
	}

	void CBackgroundActionCancelable::SetProgressTargetInfinity(){
		// sets Worker's target progress state to infinity
		SetProgressTarget(INT_MAX);
	}

	#define PB_RESOLUTION	100

	void CBackgroundActionCancelable::UpdateProgress(int state) const{
		// refreshes the displaying of actual progress
		if (m_hWnd) // the window doesn't exist if Worker already cancelled but the Worker hasn't yet found out that it can no longer Continue
			if (state<progressTarget){
				// Worker not yet finished - refreshing
				if (state>lastState){ // always progressing towards the Target, never back
					if (state*PB_RESOLUTION/progressTarget > lastState*PB_RESOLUTION/progressTarget) // preventing from overwhelming the with messages - target of 60k shouldn't mean 60.000 messages
						GetDlgItem(ID_STATE)->PostMessage( PBM_SETPOS, state );
					lastState=state;
				}
			}else
				// Worker finished - closing the window
				::PostMessage( m_hWnd, WM_COMMAND, IDOK, 0 );
	}

	void CBackgroundActionCancelable::UpdateProgressFinished() const{
		// refreshes the displaying of actual progress to "100% completed"
		UpdateProgress(INT_MAX);
	}

	TStdWinError CBackgroundActionCancelable::TerminateWithError(TStdWinError error){
		// initiates the termination of the Worker with specified Error
		bCancelled=true;
		PostMessage( WM_COMMAND, IDCANCEL );
		return error;
	}









	CBackgroundMultiActionCancelable::CBackgroundMultiActionCancelable(int actionThreadPriority)
		// ctor
		: CBackgroundActionCancelable( IDR_ACTION_SEQUENCE )
		, actionThreadPriority(  std::max( std::min(actionThreadPriority,THREAD_BASE_PRIORITY_MAX), THREAD_BASE_PRIORITY_MIN )  )
		, nActions(0) {
	}

	void CBackgroundMultiActionCancelable::AddAction(AFX_THREADPROC fnAction,LPCVOID actionParams,LPCTSTR name){
		// orders another Action at the end of the list of Actions
		auto &r=actions[nActions++];
		r.fnAction=fnAction;
		r.fnParams=actionParams;
		r.fnName=name;
	}

	#define PADDING_ACTION		5
	#define PADDING_STATUS		3

	BOOL CBackgroundMultiActionCancelable::OnInitDialog(){
		// dialog initialization
		// - initializing the Painting information
		CPoint pt(0,0);
		GetDlgItem(ID_INFORMATION)->MapWindowPoints( this, &pt, 1 );
		painting.glyphX=pt.x+14;
		painting.charHeight=Utils::CRideFont(m_hWnd).charHeight;
		CRect rc;
		GetDlgItem(ID_STATE)->GetClientRect(&rc);
		painting.progressHeight=rc.Height();
		rc.bottom += nActions*(painting.charHeight+PADDING_ACTION) + 2*PADDING_STATUS;
		GetDlgItem(ID_STATE)->MapWindowPoints( this, &(pt=CPoint(0,0)), 1 );
		rc.OffsetRect(pt);
		painting.rcActions=rc;
		const int rcActionsHeight=painting.rcActions.Height();
		// - adjusting the size of window to display all Action Names
		GetWindowRect(&rc);
		SetWindowPos(	nullptr, 0, 0,
						rc.Width(), rc.Height()+rcActionsHeight,
						SWP_NOZORDER|SWP_NOMOVE
					);
		Utils::OffsetDlgControl( m_hWnd, ID_STANDARD, 0, rcActionsHeight );
		Utils::OffsetDlgControl( m_hWnd, ID_PRIORITY, 0, rcActionsHeight );
		Utils::OffsetDlgControl( m_hWnd, IDCANCEL, 0, rcActionsHeight );
		// - launching the first Action in combo-box
		iCurrAction=-1;
		SendMessage( WM_COMMAND, IDOK );
		// - base
		const BOOL result=__super::OnInitDialog();
		// - displaying thread Priority
		SendDlgItemMessage( ID_PRIORITY, CB_SETCURSEL, actionThreadPriority-THREAD_BASE_PRIORITY_MIN ); // zero-based index
		return result;
	}

	void CBackgroundMultiActionCancelable::__drawAction__(HDC dc,WCHAR wingdingsGlyph,LPCTSTR name,RECT &inOutRc) const{
		// draws Action
		const Utils::CRideFont glyphFont( FONT_WINGDINGS, 105 );
		const HGDIOBJ hFont0=::SelectObject( dc, glyphFont );
			::TextOutW( dc, painting.glyphX, inOutRc.top, &wingdingsGlyph, 1 );
		::SelectObject( dc, hFont0 );
		::DrawText( dc, name, -1, &inOutRc, DT_LEFT|DT_TOP );
		inOutRc.top+=painting.charHeight+PADDING_ACTION;
	}

	LRESULT CBackgroundMultiActionCancelable::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_COMMAND:
				// processing a command
				switch (wParam){
					case MAKELONG(ID_PRIORITY,CBN_SELCHANGE):
						// Action Priority has been changed
						actionThreadPriority=SendDlgItemMessage(ID_PRIORITY,CB_GETCURSEL)+THREAD_BASE_PRIORITY_MIN;
						::SetThreadPriority( *this, actionThreadPriority );
						return 0;
					case IDOK:
						// current Action has finished - proceeding with the next one
						if (++iCurrAction<nActions){
							// . launching the next Action
							lastState=0;
							auto &r=actions[iCurrAction];
							BeginAnother( r.fnAction, r.fnParams, actionThreadPriority );
							Resume();
							// . repositioning the progress-bar
							const int y=painting.rcActions.top
										+
										iCurrAction*(painting.charHeight+PADDING_ACTION)
										+
										painting.charHeight+PADDING_STATUS;
							SendDlgItemMessage( ID_STATE, PBM_SETPOS, 0 ); // zeroing the progress-bar
							GetDlgItem(ID_STATE)->SetWindowPos( nullptr, painting.rcActions.left, y, 0, 0, SWP_NOZORDER|SWP_NOSIZE );
							// . repainting the list of Actions
							Invalidate();
							return 0;
						}else
							break;
				}
				break;
			case WM_PAINT:{
				// drawing
				// . basic painting
				const LRESULT result=__super::WindowProc(msg,wParam,lParam);
				// . preparing for drawing Actions
				RECT rc=painting.rcActions;
				const CClientDC dc(this);
				::SetBkMode( dc, TRANSPARENT );
				// . painting
				BYTE i=0;
				const Utils::CRideFont font(m_hWnd);
				const HGDIOBJ hFont0=::SelectObject( dc, font );
					// Actions already completed
					while (i<iCurrAction)
						__drawAction__(	dc, 0xf0fc, actions[i++].fnName, rc );
				const Utils::CRideFont fontBold(m_hWnd,true);
				::SelectObject( dc, fontBold );
					// Action currently in progress
					__drawAction__(	dc, 0xf0e0, actions[i].fnName, rc );
					rc.top+=PADDING_STATUS+painting.progressHeight+PADDING_STATUS;
				::SelectObject( dc, font );
					// Actions yet to be performed
					while (++i<nActions)
						__drawAction__(	dc, 0xf09f, actions[i].fnName, rc );
				::SelectObject( dc, hFont0 );
				return result;
			}
		}
		return __super::WindowProc(msg,wParam,lParam);
	}
