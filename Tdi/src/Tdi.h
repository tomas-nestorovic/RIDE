#ifndef TDI_H
#define TDI_H

	#pragma pack(1)
	typedef const struct TTabInfo sealed{
		const CTdiCtrl::TTab::TCanBeClosed fnCanBeClosed;
		const CTdiCtrl::TTab::TOnClosing fnOnTabClosing; // can be Null
		const CTdiCtrl::TTab::PContent content;

		TTabInfo(CTdiCtrl::TTab::TCanBeClosed fnCanBeClosed,CTdiCtrl::TTab::TOnClosing fnOnTabClosing,CTdiCtrl::TTab::PContent content);
	} *PCTabInfo;



	typedef struct TDraggedTabInfo sealed{
		int tabId;
		TCHAR caption[50];
		TCITEM data;
		RECT targetArea;

		TDraggedTabInfo(HWND hTdi,int _tabId);
	} *PDraggedTabInfo;



	#define GET_TDI_INFO(hTdi)	( (PTdiInfo)::GetWindowLong(hTdi,GWL_USERDATA) )

	typedef struct TTdiInfo sealed{
		static unsigned int nInstances;
		static HFONT fontWebdings;

		static LRESULT CALLBACK __wndProc__(HWND hTdi,UINT msg,WPARAM wParam,LPARAM lParam);

		const HWND handle; // handle of this TDI
		const HWND hBtnCloseCurrentTab;
		const CTdiCtrl::TParams params;
		const WNDPROC wndProc0; // original procedure of the control from which the TDI is subclassed
		CTdiCtrl::TTab::PContent currentTabContent;

		TTdiInfo(HINSTANCE hInstance,HWND hTdi,CTdiCtrl::PCParams params);
		~TTdiInfo();

		void __hideCurrentContent__();
		void __switchToTab__(int i);
		void __fitContentToTdiCanvas__() const;
	} *PTdiInfo;


#endif // TDI_H
