#ifndef VERIFICATIONDIALOG_H
#define VERIFICATIONDIALOG_H

	struct TVerificationFunctions sealed{
		static UINT AFX_CDECL FloppyCrossLinkedFilesVerification_thread(PVOID pCancelableAction);
		static UINT AFX_CDECL FloppyLostSectorsVerification_thread(PVOID pCancelableAction);

		static UINT AFX_CDECL WholeDiskSurfaceVerification_thread(PVOID pCancelableAction);

		AFX_THREADPROC fnBootSector;
		AFX_THREADPROC fnFatValues;
		AFX_THREADPROC fnFatCrossedFiles;
		AFX_THREADPROC fnFatLostAllocUnits;
		AFX_THREADPROC fnFilesystem;
		AFX_THREADPROC fnVolumeSurface;
	};




	class CVerifyVolumeDialog sealed:public CDialog{
		BYTE nOptionsChecked;

		void DoDataExchange(CDataExchange *pDX) override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		struct TParams sealed{
			const PDos dos;
			int verifyBootSector, verifyFat, verifyFilesystem;
			int verifyVolumeSurface;
			int repairStyle;
			const TVerificationFunctions verificationFunctions;
			mutable class CReportFile sealed:public CFile{
				bool itemListBegun, problemOpen, inProblemSolvingSection;
			public:
				CReportFile();

				void AddSection(LPCTSTR name,bool problemSolving=true);
				void OpenProblem(LPCTSTR problemDesc);
				void CloseProblem(bool solved);
				void Close() override;
			} fReport;

			TParams(CDos *dos,const TVerificationFunctions &rvf);

			BYTE ConfirmFix(LPCTSTR problemDesc,LPCTSTR problemSolvingSuggestion) const;
		} params;

		CVerifyVolumeDialog(CDos *dos,const TVerificationFunctions &rvf);
	};

#endif // VERIFICATIONDIALOG_H
