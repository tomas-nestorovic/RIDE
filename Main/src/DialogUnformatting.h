#ifndef UNFORMATDIALOG_H
#define UNFORMATDIALOG_H

	#define STR_TRIM_TO_MIN_NUMBER_OF_CYLINDERS	_T("Trim to minimum # of cylinders")

	class CUnformatDialog sealed:protected Utils::CRideDialog{
		DECLARE_MESSAGE_MAP()
	public:
		typedef const struct TStdUnformat sealed{
			LPCTSTR name;
			TCylinder cylA,cylZ;
		} *PCStdUnformat;
	private:
		const PCStdUnformat stdUnformats;
		const BYTE nStdUnformats;
		bool updateBoot, removeTracksFromFat;

		static UINT AFX_CDECL UnformatTracks_thread(PVOID pCancelableAction);
		static UINT AFX_CDECL UnregisterStdCylinders_thread(PVOID pCancelableAction);
	protected:
		void PreInitDialog() override;
		void DoDataExchange(CDataExchange *pDX) override;
		afx_msg void OnPaint();
		afx_msg void __onUnformatChanged__();
		afx_msg void __recognizeStandardUnformat__();
		afx_msg void __recognizeStandardUnformatAndRepaint__();
		afx_msg void __warnOnPossibleInconsistency__();
	public:
		struct TParams{
			const PDos dos;
			const PCHead specificHeadOnly;
			TCylinder cylA,cylZInclusive;

			TParams(PDos dos,PCHead specificHeadOnly);
		} params;

		CUnformatDialog(PDos dos,PCStdUnformat stdUnformats,BYTE nStdUnformats);

		TStdWinError ShowModalAndUnformatStdCylinders();
	};

#endif // UNFORMATDIALOG_H
