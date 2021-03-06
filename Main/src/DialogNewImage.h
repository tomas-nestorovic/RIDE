#ifndef NEWIMAGEDIALOG_H
#define NEWIMAGEDIALOG_H

	class CNewImageDialog sealed:public Utils::CRideDialog{
		void __refreshListOfContainers__();
		BOOL OnInitDialog() override;
		BOOL OnCommand(WPARAM wParam,LPARAM lParam) override;
		BOOL OnNotify(WPARAM wParam,LPARAM lParam,LRESULT *pResult) override;
		void OnOK() override;
	public:
		CImage::TFnInstantiate fnImage;
		TCHAR deviceName[DEVICE_NAME_CHARS_MAX];
		CDos::PCProperties dosProps;

		CNewImageDialog();
	};

#endif // NEWIMAGEDIALOG_H

