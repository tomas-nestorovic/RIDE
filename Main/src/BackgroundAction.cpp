#include "stdafx.h"

	TBackgroundAction::TBackgroundAction(AFX_THREADPROC fnAction,LPCVOID _fnParams)
		// ctor
		// - initialization
		: fnParams(_fnParams)
		// - creating a suspended Worker
		, pWorker( AfxBeginThread( fnAction, this, THREAD_PRIORITY_IDLE, 0, CREATE_SUSPENDED ) ) { // the object must have a vtable for the keyword "this" to work in AfxBeginThread
		pWorker->m_bAutoDelete=FALSE;
	}

	TBackgroundAction::~TBackgroundAction(){
		// dtor
		// - forced termination of the Worker (if this cannot be done, it's necessary to sort it out in descendant's dtor)
		::TerminateThread( pWorker->m_hThread, 0 );
		delete pWorker;
	}



	void TBackgroundAction::Resume() const{
		// resumes Worker's activity
		pWorker->ResumeThread();
	}
	void TBackgroundAction::Suspend() const{
		// suspends Worker's activity
		pWorker->SuspendThread();
	}










	TBackgroundActionCancelable::TBackgroundActionCancelable(AFX_THREADPROC fnAction,LPCVOID fnParams)
		// ctor
		: TBackgroundAction(fnAction,fnParams) , CDialog(IDR_ACTION_PROGRESS,app.m_pMainWnd) , bContinue(true) {
	}



	BOOL TBackgroundActionCancelable::OnInitDialog(){
		// dialog initialization
		// - launching the Worker
		pWorker->ResumeThread();
		return TRUE;
	}

	DWORD TBackgroundActionCancelable::CarryOut(DWORD _stateOfCompletion){
		// returns the Action's result; when performing the Action, the actual progress is shown in a modal window
		// - Action initialization
		stateOfCompletion=_stateOfCompletion;
		const HANDLE hWorker=pWorker->m_hThread;
		// - showing modal dialog (if none exists so far)
		DoModal();
		// - waiting for the Worker
		::WaitForSingleObject(hWorker,INFINITE);
		// - returning the result
		DWORD result;
		::GetExitCodeThread(hWorker,&result);
		return result;
	}

	DWORD TBackgroundActionCancelable::TerminateWithError(DWORD error){
		// initiates the termination of the Action with specified Error
		bContinue=false;
		::PostMessage( m_hWnd, WM_COMMAND, IDCANCEL, 0 );
		return error;
	}

	void TBackgroundActionCancelable::UpdateProgress(DWORD state) const{
		// refreshes the displaying of actual progress
		if (m_hWnd) // the window doesn't exist if Action already cancelled but the Worker hasn't yet found out that it can no longer Continue
			if (state<stateOfCompletion)
				// Action not yet finished - refreshing
				::PostMessage( GetDlgItem(ID_STATE)->m_hWnd, PBM_SETPOS, state, 0 );
			else
				// Action completed - closing the window
				::PostMessage( m_hWnd, WM_COMMAND, IDOK, 0 );
	}

	void TBackgroundActionCancelable::PreInitDialog(){
		// dialog initialization
		CDialog::PreInitDialog();
		CProgressCtrl pc;
		pc.Attach( GetDlgItem(ID_STATE)->m_hWnd );
			pc.SetRange32( 0, stateOfCompletion );
			pc.SetPos(0);
		pc.Detach();
	}

	LRESULT TBackgroundActionCancelable::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		if (msg==WM_COMMAND && wParam==IDCANCEL)
			bContinue=false; // cancelling the Action
		return CDialog::WindowProc(msg,wParam,lParam);
	}
