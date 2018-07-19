#ifndef UNFORMATDIALOG_H
#define UNFORMATDIALOG_H

	#define STR_TRIM_TO_MIN_NUMBER_OF_CYLINDERS	_T("Trim to minimum # of cylinders")

	class CUnformatDialog sealed:public CDialog{
		DECLARE_MESSAGE_MAP()
	public:
		typedef const struct TStdUnformat sealed{
			LPCTSTR name;
			TCylinder cylA,cylZ;
		} *PCStdUnformat;
	private:
		const PDos dos;
		const PCStdUnformat stdUnformats;
		const BYTE nStdUnformats;
	protected:
		void PreInitDialog() override;
		void DoDataExchange(CDataExchange *pDX) override;
		afx_msg void OnPaint();
		afx_msg void __onUnformatChanged__();
		afx_msg void __recognizeStandardUnformat__();
		afx_msg void __recognizeStandardUnformatAndRepaint__();
		afx_msg void __warnOnPossibleInconsistency__();
	public:
		TCylinder cylA,cylZ;
		int updateBoot, removeTracksFromFat;

		CUnformatDialog(PDos _dos,PCStdUnformat _stdUnformats,BYTE _nStdUnformats);
	};

#endif // UNFORMATDIALOG_H
