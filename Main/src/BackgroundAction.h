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



	typedef class CActionProgress{
	protected:
		const CActionProgress *parent;
		const int parentProgressBegin, parentProgressInc; // the beginning and increment in total progress (see BeginSubtask method)
		int targetProgress; // call SetProgressTarget or ctor
		mutable int currProgress;

		CActionProgress(const CActionProgress *parent,const volatile bool &cancelled,int parentProgressBegin,int parentProgressInc);
		CActionProgress(const CActionProgress &r); // can't copy!
	public:
		const volatile bool &Cancelled;

		CActionProgress(CActionProgress &&r);
		~CActionProgress();

		virtual void SetProgressTarget(int targetProgress);
		virtual void UpdateProgress(int newProgress,TBPFLAG status=TBPFLAG::TBPF_NORMAL) const;
		CActionProgress CreateSubactionProgress(int thisProgressIncrement,int subactionProgressTarget=INT_MAX) const;
	} *PActionProgress; // call UpdateProgress method with progress from <0;ProgressTarget)



	typedef class CBackgroundActionCancelable:public CBackgroundAction,public CActionProgress,public Utils::CRideDialog{
	protected:
		const int callerThreadPriorityOrg;
		volatile bool bCancelled;
		mutable volatile bool bTargetStateReached;
		ITaskbarList3 *pActionTaskbarList;

		CBackgroundActionCancelable(UINT dlgResId);

		void ChangeWorkerPriority(int newPriority);
		BOOL OnInitDialog() override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		static const CBackgroundActionCancelable *pSingleInstance;

		static void SignalPausedProgress(HWND hFromChild);

		CBackgroundActionCancelable(AFX_THREADPROC fnAction,LPCVOID actionParams,int actionThreadPriority);
		~CBackgroundActionCancelable();

		TStdWinError Perform();
		void SetProgressTarget(int targetProgress) override;
		void SetProgressTargetInfinity();
		void UpdateProgress(int newProgress,TBPFLAG status=TBPFLAG::TBPF_NORMAL) const override;
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
		void UpdateProgress(int newProgress,TBPFLAG status) const override;
	};

#endif // BACKGROUNDACTION_H