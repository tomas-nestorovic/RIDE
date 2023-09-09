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
	#define TDI_TAB_CANCLOSE_NEVER		nullptr


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
		static void WINAPI InsertTabA(HWND hTdi,int iIndex,LPCSTR tabName,TTab::PContent tabContent,bool makeCurrent,TTab::TCanBeClosed fnCanBeClosed,TTab::TOnClosing fnOnTabClosing);
		static void WINAPI InsertTabW(HWND hTdi,int iIndex,LPCWSTR tabName,TTab::PContent tabContent,bool makeCurrent,TTab::TCanBeClosed fnCanBeClosed,TTab::TOnClosing fnOnTabClosing);
		static void WINAPI AddTabLastA(HWND hTdi,LPCSTR tabName,TTab::PContent tabContent,bool makeCurrent,TTab::TCanBeClosed fnCanBeClosed,TTab::TOnClosing fnOnTabClosing);
		static void WINAPI AddTabLastW(HWND hTdi,LPCWSTR tabName,TTab::PContent tabContent,bool makeCurrent,TTab::TCanBeClosed fnCanBeClosed,TTab::TOnClosing fnOnTabClosing);
		static void WINAPI UpdateTabCaptionA(HWND hTdi,TTab::PContent tabContent,LPCSTR tabNewName);
		static void WINAPI UpdateTabCaptionW(HWND hTdi,TTab::PContent tabContent,LPCWSTR tabNewName);
		static void WINAPI RemoveTab(HWND hTdi,int tabId);
		static void WINAPI RemoveTab(HWND hTdi,TTab::PContent tabContent);
		static void WINAPI RemoveCurrentTab(HWND hTdi);
		static void WINAPI SwitchToTab(HWND hTdi,int tabId);
		static void WINAPI SwitchToTab(HWND hTdi,TTab::PContent tabContent);
		static void WINAPI SwitchToNextTab(HWND hTdi);
		static void WINAPI SwitchToPrevTab(HWND hTdi);
	};

#ifdef UNICODE
#else
	#define InsertTab		InsertTabA
	#define AddTabLast		AddTabLastA
	#define UpdateTabCaption UpdateTabCaptionA
#endif

#endif // TDI_API_H
