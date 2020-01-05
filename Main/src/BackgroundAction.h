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


	typedef class CBackgroundActionCancelableBase abstract:public CBackgroundAction,public CDialog{
		volatile bool bContinue;
	protected:
		int progressTarget;

		BOOL OnInitDialog() override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		CBackgroundActionCancelableBase(UINT dlgResId);

		virtual TStdWinError Perform()=0;
		bool CanContinue() const volatile;
		void SetProgressTarget(int targetState);
		void SetProgressTargetInfinity();
		virtual void UpdateProgress(int state) const=0;
		void UpdateProgressFinished() const;
		TStdWinError TerminateWithError(TStdWinError error);
	} *PBackgroundActionCancelableBase;


	class CBackgroundActionCancelable sealed:public CBackgroundActionCancelableBase{
	public:
		CBackgroundActionCancelable(AFX_THREADPROC fnAction,LPCVOID actionParams,int actionThreadPriority);

		TStdWinError Perform() override;
		void UpdateProgress(int state) const override;
	};

#endif // BACKGROUNDACTION_H