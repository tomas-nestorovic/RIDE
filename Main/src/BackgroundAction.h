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


	typedef class CBackgroundActionCancelable:public CBackgroundAction,public Utils::CRideDialog{
	protected:
		int progressTarget;
		const int callerThreadPriorityOrg;
		volatile bool bCancelled;
		mutable volatile bool bTargetStateReached;
		mutable int lastState;
		ITaskbarList3 *pActionTaskbarList;

		CBackgroundActionCancelable(UINT dlgResId);

		void ChangeWorkerPriority(int newPriority);
		BOOL OnInitDialog() override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		CBackgroundActionCancelable(AFX_THREADPROC fnAction,LPCVOID actionParams,int actionThreadPriority);
		~CBackgroundActionCancelable();

		TStdWinError Perform();
		bool IsCancelled() const volatile;
		void SetProgressTarget(int targetState);
		void SetProgressTargetInfinity();
		virtual void UpdateProgress(int state,TBPFLAG status) const;
		void UpdateProgress(int state) const;
		void UpdateProgressFinished() const;
		TStdWinError TerminateWithSuccess();
		TStdWinError TerminateWithError(TStdWinError error);
	} *PBackgroundActionCancelable;


	class CBackgroundMultiActionCancelable sealed:public CBackgroundActionCancelable{
		int actionThreadPriority;
		char nActions,iCurrAction;
		struct{
			AFX_THREADPROC fnAction;
			LPCVOID fnParams;
			LPCTSTR fnName;
		} actions[16];
		ITaskbarList3 *pMultiActionTaskbarList;
		struct{
			int glyphX;
			int charHeight;
			int progressHeight;
			CRect rcActions;
		} painting;

		void __drawAction__(HDC dc,WCHAR wingdingsGlyph,LPCTSTR name,RECT &inOutRc) const;
		BOOL OnInitDialog() override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		CBackgroundMultiActionCancelable(int actionThreadPriority);
		~CBackgroundMultiActionCancelable();

		void AddAction(AFX_THREADPROC fnAction,LPCVOID actionParams,LPCTSTR name);
		void UpdateProgress(int state,TBPFLAG status) const override;
	};

#endif // BACKGROUNDACTION_H