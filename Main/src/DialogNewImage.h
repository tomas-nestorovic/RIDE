#ifndef NEWIMAGEDIALOG_H
#define NEWIMAGEDIALOG_H

	class CNewImageDialog sealed:public CDialog{
		void __refreshListOfContainers__();
		BOOL OnInitDialog() override;
		BOOL OnCommand(WPARAM wParam,LPARAM lParam) override;
		BOOL OnNotify(WPARAM wParam,LPARAM lParam,LRESULT *pResult) override;
	public:
		CImage::TFnInstantiate fnImage;
		CDos::PCProperties dosProps;

		CNewImageDialog();
	};

#endif // NEWIMAGEDIALOG_H

