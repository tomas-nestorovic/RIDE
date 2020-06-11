#ifndef REALDEVICESELECTIONDIALOG_H
#define REALDEVICESELECTIONDIALOG_H

	class CRealDeviceSelectionDialog sealed:public CDialog{
		const CDos::PCProperties dosProps;

		void refreshListOfDevices();
		BOOL OnInitDialog() override;
		BOOL OnCommand(WPARAM wParam,LPARAM lParam) override;
		BOOL OnNotify(WPARAM wParam,LPARAM lParam,LRESULT *pResult) override;
	public:
		CImage::PCProperties deviceProps;
		TCHAR deviceName[DEVICE_NAME_CHARS_MAX];

		CRealDeviceSelectionDialog(CDos::PCProperties dosProps);
	};

#endif // REALDEVICESELECTIONDIALOG_H
