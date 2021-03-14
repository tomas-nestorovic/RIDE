#ifndef REALDEVICESELECTIONDIALOG_H
#define REALDEVICESELECTIONDIALOG_H

	class CRealDeviceSelectionDialog sealed:public Utils::CRideDialog{
		const CDos::PCProperties dosProps;
		CRideApp::CRecentFileListEx mru;

		void refreshListOfDevices();
		BOOL OnInitDialog() override;
		void OnOK() override;
		BOOL OnCommand(WPARAM wParam,LPARAM lParam) override;
		BOOL OnNotify(WPARAM wParam,LPARAM lParam,LRESULT *pResult) override;
	public:
		CImage::PCProperties deviceProps;
		TCHAR deviceName[DEVICE_NAME_CHARS_MAX];

		CRealDeviceSelectionDialog(CDos::PCProperties dosProps);
	};

#endif // REALDEVICESELECTIONDIALOG_H
