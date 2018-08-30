#ifndef BACKGROUNDACTION_H
#define BACKGROUNDACTION_H

	struct TBackgroundAction{
	protected:
		CWinThread *const pWorker;
	public:
		const LPCVOID fnParams;

		TBackgroundAction(AFX_THREADPROC fnAction,LPCVOID actionParams,int actionThreadPriority);
		virtual ~TBackgroundAction(); // virtual in order for the keyword "this" to work in AfxBeginThread when constructing a descendant

		void Resume() const;
		void Suspend() const;
	};


	struct TBackgroundActionCancelable sealed:public TBackgroundAction,public CDialog{
	private:
		DWORD stateOfCompletion;

		void PreInitDialog() override;
		BOOL OnInitDialog() override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		volatile bool bContinue;

		TBackgroundActionCancelable(AFX_THREADPROC fnAction,LPCVOID actionParams,int actionThreadPriority);

		DWORD CarryOut(DWORD _stateOfCompletion);
		DWORD TerminateWithError(DWORD error);
		void UpdateProgress(DWORD state) const;
	};

#endif // BACKGROUNDACTION_H