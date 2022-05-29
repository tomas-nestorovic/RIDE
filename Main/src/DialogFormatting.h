#ifndef FORMATDIALOG_H
#define FORMATDIALOG_H

	#define WARNING_MSG_CONSISTENCY_AT_STAKE	_T("Consistency at stake!")

	class CFormatDialog sealed:public Utils::CRideDialog{
		DECLARE_MESSAGE_MAP()
	public:
		#pragma pack(1)
		typedef const struct TParameters sealed{
			TCylinder cylinder0;
			TFormat format;
			BYTE interleaving;
			BYTE skew;
			BYTE gap3;
			BYTE nAllocationTables;
			WORD nRootDirectoryEntries;
		} *PCParameters;
		#pragma pack(1)
		typedef const struct TStdFormat sealed{
			LPCTSTR name;
			TParameters params;
		} *PCStdFormat;
	private:
		void PreInitDialog() override;
		void DoDataExchange(CDataExchange *pDX) override;
		BOOL OnNotify(WPARAM wParam,LPARAM lParam,LRESULT *pResult) override;
		afx_msg void OnPaint();
		afx_msg void __onMediumOrEncodingChanged__();
		afx_msg void __onFormatChanged__();
		afx_msg void __recognizeStandardFormat__();
		afx_msg void __recognizeStandardFormatAndRepaint__();
		afx_msg void __toggleReportingOnFormatting__();
	public:
		const PDos dos;
		const PCStdFormat additionalFormats;
		const BYTE nAdditionalFormats;
		TParameters params;
		int updateBoot, addTracksToFat, showReportOnFormatting;

		CFormatDialog(PDos _dos,PCStdFormat _additionalFormats,BYTE _nAdditionalFormats);
	};

#endif // FORMATDIALOG_H
