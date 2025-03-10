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









	static const bool NotCancelled;

	#define PB_INFINITY	INT_MAX

	CActionProgress CActionProgress::None( nullptr, NotCancelled, 0, INT_MAX );

	CActionProgress::CActionProgress(const CActionProgress *parent,const volatile bool &cancelled,int parentProgressBegin,int parentProgressInc)
		// ctor
		: parent(parent)
		, Cancelled(cancelled)
		, parentProgressBegin(parentProgressBegin) , parentProgressInc(parentProgressInc)
		, targetProgress(PB_INFINITY)
		, currProgress(0) {
	}

	CActionProgress::CActionProgress(CActionProgress &&r)
		// move ctor
		: parent(r.parent)
		, Cancelled(r.Cancelled)
		, parentProgressBegin(r.parentProgressBegin) , parentProgressInc(r.parentProgressInc)
		, targetProgress(r.targetProgress)
		, currProgress(r.currProgress) {
		r.parent=&None;
	}

	CActionProgress::~CActionProgress(){
		// dtor
		UpdateProgress( parentProgressBegin+parentProgressInc );
	}

	void CActionProgress::SetProgressTarget(int targetProgress){
		// sets Worker's target progress state, "100% completed"
		this->targetProgress=targetProgress;
		this->currProgress=0;
	}

	void CActionProgress::UpdateProgress(int newProgress,TBPFLAG status) const{
		// refreshes the displaying of actual progress
		if (this==&CActionProgress::None) // do nothing for the terminal
			return;
		if (newProgress<=currProgress) // always proceed towards the Target, never back
			return;
		if (currProgress==targetProgress) // don't propagate finished Action twice to its Parent
			return;
		currProgress=std::min( newProgress, targetProgress );
		parent->UpdateProgress( // reflect progress of this Action in its Parent
			parentProgressBegin + ::MulDiv(parentProgressInc,currProgress,targetProgress),
			status
		);
	}

	void CActionProgress::IncrementProgress(int increment) const{
		ASSERT( increment>0 );
		UpdateProgress( currProgress+increment );
	}

	CActionProgress CActionProgress::CreateSubactionProgress(int thisProgressIncrement,int subactionProgressTarget) const{
		// creates and returns a SubactionProgress; for it, call again UpdateProgress with values from <0,TargetProgress>
		ASSERT( thisProgressIncrement>=0 );
		ASSERT( currProgress+thisProgressIncrement<=targetProgress );
		CActionProgress tmp( this, Cancelled, currProgress, thisProgressIncrement );
			tmp.SetProgressTarget( subactionProgressTarget );
		return tmp;
	}










	CActionProgressBar::CActionProgressBar(const volatile bool &cancelled)
		// ctor
		: CActionProgress( &None, cancelled, 0, INT_MAX )
		, hProgressBar(0) {
	}

	void CActionProgressBar::SetProgressTarget(int targetProgress){
		// sets Worker's target progress state, "100% completed"
		__super::SetProgressTarget(targetProgress);
		auto pbStyle=::GetWindowLong( hProgressBar, GWL_STYLE );
			if (targetProgress==PB_INFINITY)
				pbStyle|=PBS_MARQUEE;
			else
				pbStyle&=~PBS_MARQUEE;
		::SetWindowLong( hProgressBar, GWL_STYLE, pbStyle );
		::PostMessage( hProgressBar, PBM_SETMARQUEE, targetProgress==PB_INFINITY, 0 );
		::PostMessage( hProgressBar, PBM_SETRANGE32, 0, targetProgress );
		::PostMessage( hProgressBar, PBM_SETPOS, 0, 0 ); // zeroing the progress-bar
	}

	void CActionProgressBar::SetProgressTargetInfinity(){
		// sets Worker's target progress state to infinity
		SetProgressTarget(PB_INFINITY);
	}

	#define PB_RESOLUTION	100

	bool CActionProgressBar::IsVisibleProgress(int newProgress) const{
		// True <=> the NewProgress visibly changes, otherwise False; this is to prevent from overwhelming the app with messages - target of 60k mustn't mean 60.000 messages!
		return newProgress*PB_RESOLUTION/targetProgress > currProgress*PB_RESOLUTION/targetProgress; // always progressing towards the Target, never back
	}

	void CActionProgressBar::UpdateProgress(int newProgress,TBPFLAG status) const{
		// refreshes the displaying of actual progress
		if (hProgressBar) // the window doesn't exist if Worker already cancelled but the Worker hasn't yet found out that it can no longer Continue
			if (newProgress<targetProgress){ // not yet finished?
				if (IsVisibleProgress(newProgress))
					::PostMessage( hProgressBar, PBM_SETPOS, newProgress, 0 );
				__super::UpdateProgress( newProgress, status );	
			}
	}










	const CBackgroundActionCancelable *CBackgroundActionCancelable::pSingleInstance;

	CBackgroundActionCancelable::CBackgroundActionCancelable(UINT dlgResId)
		// ctor
		: Utils::CRideDialog( dlgResId, CWnd::GetActiveWindow() )
		, CActionProgressBar( bCancelled )
		, callerThreadPriorityOrg( ::GetThreadPriority(::GetCurrentThread()) )
		, bCancelled(false) , bTargetStateReached(false) {
	}

	CBackgroundActionCancelable::CBackgroundActionCancelable(AFX_THREADPROC fnAction,LPCVOID actionParams,int actionThreadPriority)
		// ctor
		: Utils::CRideDialog( IDR_ACTION_PROGRESS, CWnd::GetActiveWindow() )
		, CActionProgressBar( bCancelled )
		, callerThreadPriorityOrg( ::GetThreadPriority(::GetCurrentThread()) )
		, bCancelled(false) , bTargetStateReached(false) {
		BeginAnother( fnAction, actionParams, actionThreadPriority );
		ChangeWorkerPriority( actionThreadPriority ); // making sure the caller is always responsive by temporarily elevating its priority
	}

	CBackgroundActionCancelable::~CBackgroundActionCancelable(){
		// dtor
		pSingleInstance=nullptr;
		// - clearing taskbar progress overlay
		if (pActionTaskbarList)
			pActionTaskbarList->SetProgressState( *app.m_pMainWnd, TBPF_NOPROGRESS );
		// - recovering caller's original priority
		::SetThreadPriority( ::GetCurrentThread(), callerThreadPriorityOrg );
	}




	BOOL CBackgroundActionCancelable::OnInitDialog(){
		// dialog initialization
		if (SUCCEEDED(pActionTaskbarList.CoCreateInstance( CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER )))
			pActionTaskbarList->HrInit();
		hProgressBar=GetDlgItemHwnd(ID_STATE);
		SetTimer( ID_Y, 1000, nullptr );
		::PostMessage( m_hWnd, WM_COMMAND, IDCONTINUE, 0 ); // launching the Worker
		pSingleInstance=this;
		return __super::OnInitDialog();
	}

	LRESULT CBackgroundActionCancelable::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_COMMAND:
				switch (LOWORD(wParam)){
					case IDCANCEL:
						// cancelling the Worker and whole dialog
						EnableWindow(FALSE); // as about to be destroyed soon, mustn't parent any pop-up windows!
						bCancelled=true;
						break;
					case IDCONTINUE:
						// resuming the Worker
						EnableWindow(); // may parent pop-up windows
						Resume();
						return 0;
				}
				break;
			case WM_TIMER:{
				// timer tick
				const auto elapsedTime=Utils::CRideTime()-startTime;
				TCHAR caption[50];
				::wsprintf( caption, _T("Please wait... (%d:%02d)"), elapsedTime.wMinute, elapsedTime.wSecond );
				SetWindowText( caption );
				break;
			}
		}
		return __super::WindowProc(msg,wParam,lParam);
	}

	TStdWinError CBackgroundActionCancelable::Perform(bool suspendAllViews){
		// returns the Worker's result; when performing, the actual progress is shown in a modal window
		// - showing modal dialog and performing the Action
		if (suspendAllViews)
			if (CImage *const active=CImage::GetActive())
				active->SetRedrawToAllViews(false);
		startTime=Utils::CRideTime();
		DoModal();
		// - waiting for the already running Worker
		::WaitForSingleObject( *this, INFINITE );
		duration=Utils::CRideTime()-startTime;
		if (suspendAllViews)
			if (CImage *const active=CImage::GetActive()){
				active->SetRedrawToAllViews(true);
				active->UpdateAllViews(nullptr);
			}
		// - returning the Result
		DWORD result=ERROR_SUCCESS;
		::GetExitCodeThread( *this, &result );
		return result;
	}

	void CBackgroundActionCancelable::ChangeWorkerPriority(int newPriority){
		// changes Worker's priority
		// - Worker's priority
		::SetThreadPriority( *this, newPriority );
		// - making sure the caller is always responsive by temporarily elevating its priority
		if (newPriority>callerThreadPriorityOrg+2)
			::SetThreadPriority(
				::GetCurrentThread(),
				std::max(  THREAD_PRIORITY_NORMAL,  std::min( newPriority-2, THREAD_PRIORITY_HIGHEST )  )
			);
	}

	void CBackgroundActionCancelable::SetProgressTarget(int targetProgress){
		// sets Worker's target progress state, "100% completed"
		__super::SetProgressTarget(targetProgress);
		if (pActionTaskbarList)
			pActionTaskbarList->SetProgressState( *app.m_pMainWnd,
				targetProgress==PB_INFINITY ? TBPFLAG::TBPF_INDETERMINATE : TBPFLAG::TBPF_NORMAL
			);
	}

	void CBackgroundActionCancelable::UpdateProgress(int newProgress,TBPFLAG status) const{
		// refreshes the displaying of actual progress
		if (m_hWnd) // the window doesn't exist if Worker already cancelled but the Worker hasn't yet found out that it can no longer Continue
			if (newProgress<targetProgress){
				// Worker not yet finished - refreshing
				if (IsVisibleProgress(newProgress))
					if (pActionTaskbarList){
						pActionTaskbarList->SetProgressValue( *app.m_pMainWnd, newProgress, targetProgress );
						pActionTaskbarList->SetProgressState( *app.m_pMainWnd, status );
					}
				__super::UpdateProgress( newProgress, status );	
			}else
				// Worker finished - closing the window
				if (!bTargetStateReached){ // not sending the dialog-closure request twice
					bTargetStateReached=true;
					::EnableWindow( m_hWnd, FALSE ); // as about to be destroyed soon, mustn't parent any pop-up windows!
					::PostMessage( m_hWnd, WM_COMMAND, IDOK, 0 );
				}
	}

	void CBackgroundActionCancelable::UpdateProgressFinished() const{
		// refreshes the displaying of actual progress to "100% completed"
		UpdateProgress(PB_INFINITY);
	}

	void CBackgroundActionCancelable::SignalPausedProgress(HWND hFromChild){
		// if parented by this dialog (not necessarily directly), signals pause in progress of this Action
		if (pSingleInstance)
			while (hFromChild!=nullptr && hFromChild!=INVALID_HANDLE_VALUE)
				if (hFromChild!=pSingleInstance->m_hWnd)
					hFromChild=::GetParent(hFromChild);
				else{
					const auto currProgressOrg=pSingleInstance->currProgress;
					pSingleInstance->currProgress=-PB_RESOLUTION;
					return pSingleInstance->UpdateProgress( currProgressOrg, TBPFLAG::TBPF_PAUSED );
				}
	}

	TStdWinError CBackgroundActionCancelable::TerminateWithSuccess(){
		// initiates successfull termination of the Worker
		UpdateProgressFinished();
		return ERROR_SUCCESS;
	}

	TStdWinError CBackgroundActionCancelable::TerminateWithError(TStdWinError error){
		// initiates the termination of the Worker with specified Error
		bCancelled=true;
		if (m_hWnd)
			PostMessage( WM_COMMAND, IDCANCEL );
		return error;
	}

	TStdWinError CBackgroundActionCancelable::TerminateWithLastError(){
		// initiates the termination of the Worker with last Windows standard i/o error
		return TerminateWithError( ::GetLastError() );
	}









	CBackgroundMultiActionCancelable::CBackgroundMultiActionCancelable(int actionThreadPriority)
		// ctor
		// - base
		: CBackgroundActionCancelable( IDR_ACTION_SEQUENCE )
		// - initialization
		, actionThreadPriority(  std::max( std::min(actionThreadPriority,THREAD_PRIORITY_TIME_CRITICAL), THREAD_BASE_PRIORITY_MIN )  )
		, nActions(0) {
		// - making sure the caller is always responsive by temporarily elevating its priority
		ChangeWorkerPriority( actionThreadPriority );
	}

	CBackgroundMultiActionCancelable::~CBackgroundMultiActionCancelable(){
		// dtor
		// - clearing taskbar progress overlay
		if (pMultiActionTaskbarList)
			pActionTaskbarList=pMultiActionTaskbarList; // up to the base (see also ctor)
	}

	void CBackgroundMultiActionCancelable::AddAction(AFX_THREADPROC fnAction,LPCVOID actionParams,LPCTSTR name){
		// orders another Action at the end of the list of Actions
		if (fnAction==nullptr)
			return;
		auto &r=actions[nActions++];
		r.fnAction=fnAction;
		r.fnParams=actionParams;
		r.fnName=name;
	}

	void CBackgroundMultiActionCancelable::UpdateProgress(int newProgress,TBPFLAG status) const{
		// refreshes the displaying of actual progress
		// - base
		const auto currProgressOrg=currProgress;
		__super::UpdateProgress( newProgress, status );
		// - updating taskbar button progress indication
		if (m_hWnd) // the window doesn't exist if Worker already cancelled but the Worker hasn't yet found out that it can no longer Continue
			if (pMultiActionTaskbarList)
				if (newProgress*PB_RESOLUTION/targetProgress > currProgressOrg*PB_RESOLUTION/targetProgress){ // preventing from overwhelming the app with messages - target of 60k shouldn't mean 60.000 messages
					pMultiActionTaskbarList->SetProgressValue( *app.m_pMainWnd, (ULONGLONG)iCurrAction*targetProgress+newProgress, (ULONGLONG)nActions*targetProgress );
					pMultiActionTaskbarList->SetProgressState( *app.m_pMainWnd, status );
				}
	}

	#define PADDING_ACTION		8
	#define PADDING_STATUS		3

	BOOL CBackgroundMultiActionCancelable::OnInitDialog(){
		// dialog initialization
		// - initializing the Painting information
		const CPoint pt=MapDlgItemClientOrigin(ID_INFORMATION);
		painting.glyphX=pt.x+14;
		painting.charHeight=Utils::CRideFont(m_hWnd).charHeight;
		CRect rc=GetDlgItemClientRect(ID_STATE);
		painting.progressHeight=rc.Height();
		rc.bottom += nActions*(painting.charHeight+PADDING_ACTION) + 2*PADDING_STATUS;
		rc.OffsetRect( MapDlgItemClientOrigin(ID_STATE) );
		painting.rcActions=rc;
		const int rcActionsHeight=painting.rcActions.Height();
		// - adjusting the size of window to display all Action Names
		GetWindowRect(&rc);
		SetWindowPos(	nullptr, 0, 0,
						rc.Width(), rc.Height()+rcActionsHeight,
						SWP_NOZORDER|SWP_NOMOVE
					);
		OffsetDlgItem( ID_STANDARD, 0, rcActionsHeight );
		OffsetDlgItem( ID_PRIORITY, 0, rcActionsHeight );
		OffsetDlgItem( IDCANCEL, 0, rcActionsHeight );
		// - base (launching the first Action)
		iCurrAction=-1;
		::PostMessage( m_hWnd, WM_COMMAND, IDOK, 0 ); // next Action ...
		const BOOL result=__super::OnInitDialog(); // ... and its launch
		// - take control over taskbar button progress overlay
		pMultiActionTaskbarList.Attach( pActionTaskbarList.Detach() );
		// - displaying thread Priority
		if (actionThreadPriority<=THREAD_BASE_PRIORITY_MAX)
			// time-non-critical Actions - user is free to change Worker's priority
			SendDlgItemMessage( ID_PRIORITY, CB_SETCURSEL, actionThreadPriority-THREAD_BASE_PRIORITY_MIN ); // zero-based index
		else{
			// time-critical Actions - user must not change Worker's priority
			SendDlgItemMessage(	ID_PRIORITY, CB_SETCURSEL,
								SendDlgItemMessage( ID_PRIORITY, CB_ADDSTRING, 0, (LPARAM)_T("Real-time") )
							);
			EnableDlgItem( ID_PRIORITY, false );
		}
		return result;
	}

	void CBackgroundMultiActionCancelable::__drawAction__(HDC dc,WCHAR wingdingsGlyph,LPCTSTR name,RECT &inOutRc) const{
		// draws Action
		const HGDIOBJ hFont0=::SelectObject( dc, Utils::CRideFont::Wingdings105 );
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
						actionThreadPriority=GetDlgComboBoxSelectedIndex(ID_PRIORITY)+THREAD_BASE_PRIORITY_MIN;
						ChangeWorkerPriority( actionThreadPriority );
						return 0;
					case IDOK:
						// current Action has finished - proceeding with the next one
						if (iCurrAction+1<nActions){
							// . launching the next Action
							currProgress=0;
							const auto &r=actions[++iCurrAction];
							BeginAnother( r.fnAction, r.fnParams, actionThreadPriority );
							bTargetStateReached=false;
							::PostMessage( m_hWnd, WM_COMMAND, IDCONTINUE, 0 ); // launching the Worker
							// . repositioning the progress-bar
							const int y=painting.rcActions.top
										+
										iCurrAction*(painting.charHeight+PADDING_ACTION)
										+
										painting.charHeight+PADDING_STATUS;
							SetDlgItemPos( ID_STATE, painting.rcActions.left, y );
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
				const CRideDC dc(*this);
				::SetBkMode( dc, TRANSPARENT );
				// . painting
				BYTE i=0;
					// Actions already completed
					while (i<iCurrAction)
						__drawAction__(	dc, 0xf0fc, actions[i++].fnName, rc );
				const Utils::CRideFont fontBold(m_hWnd,true);
				const HGDIOBJ hFont0=::SelectObject( dc, fontBold );
					// Action currently in progress
					__drawAction__(	dc, 0xf0e0, actions[i].fnName, rc );
					rc.top+=PADDING_STATUS+painting.progressHeight+PADDING_STATUS;
				::SelectObject( dc, hFont0 );
					// Actions yet to be performed
					while (++i<nActions)
						__drawAction__(	dc, 0xf09f, actions[i].fnName, rc );
				return result;
			}
		}
		return __super::WindowProc(msg,wParam,lParam);
	}
