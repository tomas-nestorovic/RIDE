#ifndef NEWIMAGEDIALOG_H
#define NEWIMAGEDIALOG_H

	class CNewImageDialog sealed:public CDialog{
		BOOL OnInitDialog() override;
		BOOL OnCommand(WPARAM wParam,LPARAM lParam) override;
		BOOL OnNotify(WPARAM wParam,LPARAM lParam,LRESULT *pResult) override;
	public:
		CImage::TFnInstantiate fnImage;
		CDos::TFnInstantiate fnDos;

		CNewImageDialog();
	};

#endif // NEWIMAGEDIALOG_H

