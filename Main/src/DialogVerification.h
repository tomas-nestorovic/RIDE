#ifndef VERIFICATIONDIALOG_H
#define VERIFICATIONDIALOG_H

	class CVerifyVolumeDialog sealed:public CDialog{
		const CDos *const dos;
		BYTE nOptionsChecked;

		void DoDataExchange(CDataExchange *pDX) override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		int verifyBootSector, verifyFat, verifyFilesystem;
		int verifyDiskSurface;
		int repairStyle;

		CVerifyVolumeDialog(const CDos *dos);
	};

#endif // VERIFICATIONDIALOG_H
