#ifndef BOOTVIEW_H
#define BOOTVIEW_H

	#define BOOT_SECTOR_TAB_LABEL	_T("Boot sector")
	#define BOOT_SECTOR_ADVANCED	_T("Advanced")
	#define BOOT_SECTOR_UPDATE_ONLINE	_T("Update on-line")
	#define BOOT_SECTOR_UPDATE_ONLINE_HYPERLINK	_T("<a>Update on-line</a>")

	class CBootView:public CCriticalSectorView{
		static void __informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId);
		static bool WINAPI __confirmCriticalValueInBoot__(PVOID criticalValueId,int newValue);
		static bool WINAPI __updateFatAfterChangingCylinderCount__(PVOID,int newValue);
	protected:
		typedef struct TCommonBootParameters sealed{
			unsigned geometryCategory	:1;
				unsigned chs:1;
				unsigned sectorLength:1;
			unsigned volumeCategory	:1;
				struct{
					BYTE length; // non-zero = this structure is valid and the Label will be added to PropertyGrid
					PCHAR bufferA;
					char fillerByte;
					PropGrid::String::TOnValueConfirmedA onLabelConfirmedA;
				} label;
				struct{
					PVOID buffer; // non-Null = this structure is valid and ID will be added to PropertyGrid
					BYTE bufferCapacity;
				} id;
				unsigned clusterSize:1;
		} &RCommonBootParameters;

		CBootView(PDos dos,RCPhysicalAddress rChsBoot);

		void OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint) override;
		virtual void GetCommonBootParameters(RCommonBootParameters rParam,PSectorData boot)=0;
		virtual void AddCustomBootParameters(HWND hPropGrid,HANDLE hGeometry,HANDLE hVolume,const TCommonBootParameters &rParam,PSectorData boot)=0;
	public:
		static bool WINAPI __bootSectorModified__(PropGrid::PCustomParam,int);
		static bool WINAPI __bootSectorModifiedA__(PropGrid::PCustomParam,LPCSTR,short);
		static bool WINAPI __bootSectorModified__(PropGrid::PCustomParam,bool);
		static bool WINAPI __bootSectorModified__(PropGrid::PCustomParam,PropGrid::Enum::UValue);
	};

#endif // BOOTVIEW_H
