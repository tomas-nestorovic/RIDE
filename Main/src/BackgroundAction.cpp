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









	const CBackgroundActionCancelable *CBackgroundActionCancelable::pSingleInstance;

	CBackgroundActionCancelable::CBackgroundActionCancelable(UINT dlgResId)
		// ctor
		: Utils::CRideDialog( dlgResId, CWnd::GetActiveWindow() )
		, callerThreadPriorityOrg( ::GetThreadPriority(::GetCurrentThread()) )
		, pActionTaskbarList(nullptr)
		, bCancelled(false) , bTargetStateReached(false) , lastState(0)
		, progressTarget(INT_MAX) {
	}

	CBackgroundActionCancelable::CBackgroundActionCancelable(AFX_THREADPROC fnAction,LPCVOID actionParams,int actionThreadPriority)
		// ctor
		: Utils::CRideDialog( IDR_ACTION_PROGRESS, CWnd::GetActiveWindow() )
		, callerThreadPriorityOrg( ::GetThreadPriority(::GetCurrentThread()) )
		, pActionTaskbarList(nullptr)
		, bCancelled(false) , bTargetStateReached(false) , lastState(0)
		, progressTarget(INT_MAX) {
		BeginAnother( fnAction, actionParams, actionThreadPriority );
		ChangeWorkerPriority( actionThreadPriority ); // making sure the caller is always responsive by temporarily elevating its priority
	}

	CBackgroundActionCancelable::~CBackgroundActionCancelable(){
		// dtor
		pSingleInstance=nullptr;
		// - clearing taskbar progress overlay
		if (pActionTaskbarList){
			pActionTaskbarList->SetProgressState( *app.m_pMainWnd, TBPF_NOPROGRESS );
			pActionTaskbarList->Release();
		}
		// - recovering caller's original priority
		::SetThreadPriority( ::GetCurrentThread(), callerThreadPriorityOrg );
	}




	BOOL CBackgroundActionCancelable::OnInitDialog(){
		// dialog initialization
		if (SUCCEEDED(::CoCreateInstance( CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskbarList3, (LPVOID *)&pActionTaskbarList )))
			pActionTaskbarList->HrInit();
		::PostMessage( m_hWnd, WM_COMMAND, IDCONTINUE, 0 ); // launching the Worker
		pSingleInstance=this;
		return __super::OnInitDialog();
	}

	LRESULT CBackgroundActionCancelable::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		if (msg==WM_COMMAND)
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
		return __super::WindowProc(msg,wParam,lParam);
	}

	TStdWinError CBackgroundActionCancelable::Perform(){
		// returns the Worker's result; when performing, the actual progress is shown in a modal window
		// - showing modal dialog and performing the Action
		DoModal();
		// - waiting for the already running Worker
		::WaitForSingleObject( *this, INFINITE );
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

	bool CBackgroundActionCancelable::IsCancelled() const volatile{
		// True <=> the Worker can continue (user hasn't cancelled it), otherwise False
		return bCancelled;
	}

	void CBackgroundActionCancelable::SetProgressTarget(int targetState){
		// sets Worker's target progress state, "100% completed"
		::PostMessage(	GetDlgItemHwnd(ID_STATE),
						PBM_SETRANGE32,
						0,	progressTarget=targetState
					);
	}

	void CBackgroundActionCancelable::SetProgressTargetInfinity(){
		// sets Worker's target progress state to infinity
		SetProgressTarget(INT_MAX);
	}

	#define PB_RESOLUTION	100

	void CBackgroundActionCancelable::UpdateProgress(int state,TBPFLAG status) const{
		// refreshes the displaying of actual progress
		if (m_hWnd) // the window doesn't exist if Worker already cancelled but the Worker hasn't yet found out that it can no longer Continue
			if (state<progressTarget){
				// Worker not yet finished - refreshing
				if (state>lastState){ // always progressing towards the Target, never back
					if (state*PB_RESOLUTION/progressTarget > lastState*PB_RESOLUTION/progressTarget){ // preventing from overwhelming the app with messages - target of 60k shouldn't mean 60.000 messages
						GetDlgItem(ID_STATE)->PostMessage( PBM_SETPOS, state );
						if (pActionTaskbarList){
							pActionTaskbarList->SetProgressValue( *app.m_pMainWnd, state, progressTarget );
							pActionTaskbarList->SetProgressState( *app.m_pMainWnd, status );
						}
					}
					lastState=state;
				}
			}else
				// Worker finished - closing the window
				if (!bTargetStateReached){ // not sending the dialog-closure request twice
					bTargetStateReached=true;
					::EnableWindow( m_hWnd, FALSE ); // as about to be destroyed soon, mustn't parent any pop-up windows!
					::PostMessage( m_hWnd, WM_COMMAND, IDOK, 0 );
				}
	}

	void CBackgroundActionCancelable::UpdateProgress(int state) const{
		// refreshes the displaying of actual progress
		UpdateProgress( state, TBPFLAG::TBPF_NORMAL );
	}

	void CBackgroundActionCancelable::UpdateProgressFinished() const{
		// refreshes the displaying of actual progress to "100% completed"
		UpdateProgress(INT_MAX);
	}

	void CBackgroundActionCancelable::SignalPausedProgress(HWND hFromChild){
		// if parented by this dialog (not necessarily directly), signals pause in progress of this Action
		if (pSingleInstance)
			while (hFromChild!=nullptr && hFromChild!=INVALID_HANDLE_VALUE)
				if (hFromChild!=pSingleInstance->m_hWnd)
					hFromChild=::GetParent(hFromChild);
				else{
					const auto lastStateOrg=pSingleInstance->lastState;
					pSingleInstance->lastState=-PB_RESOLUTION;
					return pSingleInstance->UpdateProgress( lastStateOrg, TBPFLAG::TBPF_PAUSED );
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









	CBackgroundMultiActionCancelable::CBackgroundMultiActionCancelable(int actionThreadPriority)
		// ctor
		// - base
		: CBackgroundActionCancelable( IDR_ACTION_SEQUENCE )
		// - initialization
		, actionThreadPriority(  std::max( std::min(actionThreadPriority,THREAD_PRIORITY_TIME_CRITICAL), THREAD_BASE_PRIORITY_MIN )  )
		, nActions(0)
		, pMultiActionTaskbarList(nullptr) {
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

	void CBackgroundMultiActionCancelable::UpdateProgress(int state,TBPFLAG status) const{
		// refreshes the displaying of actual progress
		// - base
		const auto lastStateOrg=lastState;
		__super::UpdateProgress( state, status );
		// - updating taskbar button progress indication
		if (m_hWnd) // the window doesn't exist if Worker already cancelled but the Worker hasn't yet found out that it can no longer Continue
			if (pMultiActionTaskbarList)
				if (state*PB_RESOLUTION/progressTarget > lastStateOrg*PB_RESOLUTION/progressTarget){ // preventing from overwhelming the app with messages - target of 60k shouldn't mean 60.000 messages
					pMultiActionTaskbarList->SetProgressValue( *app.m_pMainWnd, (ULONGLONG)iCurrAction*progressTarget+state, (ULONGLONG)nActions*progressTarget );
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
		pMultiActionTaskbarList=pActionTaskbarList;
		pActionTaskbarList=nullptr;
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
						actionThreadPriority=GetDlgComboBoxSelectedIndex(ID_PRIORITY)+THREAD_BASE_PRIORITY_MIN;
						ChangeWorkerPriority( actionThreadPriority );
						return 0;
					case IDOK:
						// current Action has finished - proceeding with the next one
						if (iCurrAction+1<nActions){
							// . launching the next Action
							lastState=0;
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
							SendDlgItemMessage( ID_STATE, PBM_SETPOS, 0 ); // zeroing the progress-bar
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
