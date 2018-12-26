#ifndef BOOTVIEW_H
#define BOOTVIEW_H

	#define BOOT_SECTOR_TAB_LABEL	_T("Boot sector")
	#define BOOT_SECTOR_ADVANCED	_T("Advanced")

	class CBootView:public CView{
		DECLARE_MESSAGE_MAP()
	private:
		static void __informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId);
		static bool WINAPI __confirmCriticalValueInBoot__(PVOID criticalValueId,int newValue);
		static bool WINAPI __updateFatAfterChangingCylinderCount__(PVOID,int newValue);

		CDos::CFileReaderWriter fBoot;
		CSplitterWnd *content; // newly created for whenever Boot is switched to in TDI
		CWnd propGrid;
		CHexaEditor hexaEditor;
		int splitX;

		afx_msg int OnCreate(LPCREATESTRUCT lpcs);
		afx_msg void OnSize(UINT nType,int cx,int cy);
		afx_msg void OnKillFocus(CWnd *newFocus);
		afx_msg void OnDestroy();
		afx_msg void __toggleWriteProtection__();
		void __updateLookOfControls__();
	protected:
		typedef struct TCommonBootParameters sealed{
			unsigned geometryCategory	:1;
				unsigned chs:1;
				PWORD pSectorLength; // non-Null pointer = added to PropertyGrid
			unsigned volumeCategory	:1;
				struct{
					BYTE length; // non-zero = this structure is valid and the Label will be added to PropertyGrid
					PCHAR bufferA;
					char fillerByte;
					CPropGridCtrl::TString::TOnValueConfirmedA onLabelConfirmedA;
				} label;
				struct{
					PVOID buffer; // non-Null = this structure is valid and ID will be added to PropertyGrid
					BYTE bufferCapacity;
				} id;
		} &RCommonBootParameters;

		CBootView(PDos dos,RCPhysicalAddress rChsBoot);

		void OnDraw(CDC *pDC) override sealed;
		void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override;
		void PostNcDestroy() override;
		virtual void GetCommonBootParameters(RCommonBootParameters rParam,PSectorData boot)=0;
		virtual void AddCustomBootParameters(HWND hPropGrid,HANDLE hGeometry,HANDLE hVolume,const TCommonBootParameters &rParam,PSectorData boot)=0;
	public:
		static const CBootView *pCurrentlyShown; // Boot that is currently shown (a multi-volume disk may have several Boots, one for each volume)

		static bool WINAPI __bootSectorModified__(CPropGridCtrl::PCustomParam,int);
		static bool WINAPI __bootSectorModifiedA__(CPropGridCtrl::PCustomParam,LPCSTR,short);
		static bool WINAPI __bootSectorModified__(CPropGridCtrl::PCustomParam,bool);
		static bool WINAPI __bootSectorModified__(CPropGridCtrl::PCustomParam,CPropGridCtrl::TEnum::UValue);

		const TPhysicalAddress chsBoot;
		const CMainWindow::CTdiView::TTab tab;

		bool __isValueBeingEditedInPropertyGrid__() const;
	};

#endif // BOOTVIEW_H
