#ifndef VERIFICATIONDIALOG_H
#define VERIFICATIONDIALOG_H

	class CVerifyVolumeDialog sealed:public CDialog{
		BYTE nOptionsChecked;

		void DoDataExchange(CDataExchange *pDX) override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		struct TParams sealed{
			const CDos *const dos;
			int verifyBootSector, verifyFat, verifyFilesystem;
			int verifyVolumeSurface;
			int repairStyle;

			TParams(const CDos *dos);

			BYTE ConfirmFix(LPCTSTR problemDesc,LPCTSTR problemSolvingSuggestion) const;
		} params;

		CVerifyVolumeDialog(const CDos *dos);
	};

#endif // VERIFICATIONDIALOG_H
