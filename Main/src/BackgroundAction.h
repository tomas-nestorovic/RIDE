#ifndef BACKGROUNDACTION_H
#define BACKGROUNDACTION_H

	typedef const class CBackgroundAction{
	protected:
		std::unique_ptr<CWinThread> pWorker;
		LPCVOID fnParams;

		CBackgroundAction();
	public:
		CBackgroundAction(AFX_THREADPROC fnAction,LPCVOID actionParams,int actionThreadPriority);
		virtual ~CBackgroundAction(); // virtual in order for the keyword "this" to work in AfxBeginThread when constructing a descendant

		LPCVOID GetParams() const;
		void Resume() const;
		void Suspend() const;
		void BeginAnother(AFX_THREADPROC fnAction,LPCVOID actionParams,int actionThreadPriority);
		operator HANDLE() const;
	} *PCBackgroundAction;


	typedef class CBackgroundActionCancelable:public CBackgroundAction,public CDialog{
		int progressTarget;
	protected:
		volatile bool bCancelled;

		CBackgroundActionCancelable(UINT dlgResId);

		BOOL OnInitDialog() override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		CBackgroundActionCancelable(AFX_THREADPROC fnAction,LPCVOID actionParams,int actionThreadPriority);

		TStdWinError Perform();
		bool IsCancelled() const volatile;
		void SetProgressTarget(int targetState);
		void SetProgressTargetInfinity();
		void UpdateProgress(int state) const;
		void UpdateProgressFinished() const;
		TStdWinError TerminateWithError(TStdWinError error);
	} *PBackgroundActionCancelable;

#endif // BACKGROUNDACTION_H