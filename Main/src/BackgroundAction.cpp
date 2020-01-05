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
		, bCancelled(false)
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
		// - zeroing the progress-bar
		CProgressCtrl pc;
		pc.Attach( GetDlgItem(ID_STATE)->m_hWnd );
			pc.SetPos(0);
		pc.Detach();
		// - launching the Worker
		Resume();
		return TRUE;
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
		CProgressCtrl pc;
		pc.Attach( GetDlgItem(ID_STATE)->m_hWnd );
			pc.SetRange32( 0, progressTarget=targetState );
		pc.Detach();
	}

	void CBackgroundActionCancelable::SetProgressTargetInfinity(){
		// sets Worker's target progress state to infinity
		SetProgressTarget(INT_MAX);
	}

	void CBackgroundActionCancelable::UpdateProgress(int state) const{
		// refreshes the displaying of actual progress
		if (m_hWnd) // the window doesn't exist if Worker already cancelled but the Worker hasn't yet found out that it can no longer Continue
			if (state<progressTarget)
				// Worker not yet finished - refreshing
				GetDlgItem(ID_STATE)->PostMessage( PBM_SETPOS, state );
			else
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
