#ifndef TDI_API_H
#define TDI_API_H

	#ifdef _DEBUG
		#define TDI_DECLSPEC
	#elif TDI_EXPORT
		#define TDI_DECLSPEC __declspec(dllexport)
	#else
		#define TDI_DECLSPEC __declspec(dllimport)
	#endif


	#define TDI_TAB_CANCLOSE_ALWAYS		(CTdiCtrl::TTab::TCanBeClosed)TRUE
	#define TDI_TAB_CANCLOSE_NEVER		NULL


	class TDI_DECLSPEC CTdiCtrl sealed{
	public:
		typedef PVOID PCustomParam;

		struct TDI_DECLSPEC TTab{
			typedef LPCVOID PContent;

			typedef void (WINAPI *TShowContent)(PCustomParam,PContent);
			typedef void (WINAPI *THideContent)(PCustomParam,PContent);
			typedef void (WINAPI *TRepaintContent)(PContent);
			typedef HWND (WINAPI *TGetContentHwnd)(PContent);

			typedef bool (WINAPI *TCanBeClosed)(PContent);
			typedef void (WINAPI *TOnClosing)(PContent);
		};

		typedef const struct TDI_DECLSPEC TParams sealed{
			const PCustomParam customParam;
			const TTab::TShowContent fnShowContent;
			const TTab::THideContent fnHideContent;
			const TTab::TRepaintContent fnRepaintContent;
			const TTab::TGetContentHwnd fnGetHwnd;

			TParams(PCustomParam customParam,TTab::TShowContent fnShowContent,TTab::THideContent fnHideContent,TTab::TRepaintContent fnRepaintContent,TTab::TGetContentHwnd fnGetHwnd);
		} *PCParams;

		static HWND WINAPI Create(HINSTANCE hInstance,LPCTSTR windowName,UINT style,int width,int height,HWND hParent,UINT id,PCParams params);
		static void WINAPI SubclassWnd(HINSTANCE hInstance,HWND hTabCtrl,PCParams params);
		static TTab::PContent GetTabContent(HWND hTdi,int iIndex);
		static bool WINAPI GetCurrentTabContentRect(HWND hTdi,LPRECT pOutRect);
		static void WINAPI InsertTab(HWND hTdi,int iIndex,LPCTSTR tabName,TTab::PContent tabContent,bool makeCurrent,TTab::TCanBeClosed fnCanBeClosed,TTab::TOnClosing fnOnTabClosing);
		static void WINAPI AddTabLast(HWND hTdi,LPCTSTR tabName,TTab::PContent tabContent,bool makeCurrent,TTab::TCanBeClosed fnCanBeClosed,TTab::TOnClosing fnOnTabClosing);
		static void WINAPI UpdateTabCaption(HWND hTdi,TTab::PContent tabContent,LPCTSTR tabNewName);
		static bool WINAPI GetCurrentTab(HWND hTdi,TTab::PContent *outTabContent);
		static void WINAPI RemoveTab(HWND hTdi,int tabId);
		static void WINAPI RemoveTab(HWND hTdi,TTab::PContent tabContent);
		static void WINAPI RemoveCurrentTab(HWND hTdi);
		static void WINAPI SwitchToTab(HWND hTdi,TTab::PContent tabContent);
		static void WINAPI SwitchToNextTab(HWND hTdi);
		static void WINAPI SwitchToPrevTab(HWND hTdi);
	};

#endif // TDI_API_H
