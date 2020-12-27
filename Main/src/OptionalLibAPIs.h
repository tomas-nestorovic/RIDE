#ifndef OPTLIBAPIS_H
#define OPTLIBAPIS_H

namespace UxTheme
{
	HTHEME OpenThemeData(HWND hwnd,LPCWSTR pszClassList);
	HRESULT DrawThemeBackground(HTHEME hTheme,HDC hdc,int iPartId,int iStateId,LPCRECT pRect,LPCRECT pClipRect);
	HRESULT CloseThemeData(HTHEME hTheme);
}


#include <setupapi.h>
#include <guiddef.h>

namespace SetupDi
{
	HDEVINFO GetClassDevs(
		__in_opt CONST GUID *ClassGuid,
		__in_opt PCTSTR Enumerator,
		__in_opt HWND hwndParent,
		__in DWORD Flags
    );
	BOOL EnumDeviceInterfaces(
		__in HDEVINFO DeviceInfoSet,
		__in_opt PSP_DEVINFO_DATA DeviceInfoData,
		__in CONST GUID *InterfaceClassGuid,
		__in DWORD MemberIndex,
		__out PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData
    );
	BOOL GetDeviceInterfaceDetail(
		__in HDEVINFO DeviceInfoSet,
		__in PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
		__out_bcount_opt(DeviceInterfaceDetailDataSize) PSP_DEVICE_INTERFACE_DETAIL_DATA DeviceInterfaceDetailData,
		__in DWORD DeviceInterfaceDetailDataSize,
		__out_opt PDWORD RequiredSize, 
		__out_opt PSP_DEVINFO_DATA DeviceInfoData
    );
	BOOL DestroyDeviceInfoList(
		__in HDEVINFO DeviceInfoSet
    );

}


#undef _MP
#include "winusb.h"

namespace WinUsb
{
	BOOL Initialize(
		__in  HANDLE DeviceHandle,
		__out PWINUSB_INTERFACE_HANDLE InterfaceHandle
	);
	BOOL GetAssociatedInterface(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle,
		__in  UCHAR AssociatedInterfaceIndex,
		__out PWINUSB_INTERFACE_HANDLE AssociatedInterfaceHandle
	);
	BOOL SetPipePolicy(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle,
		__in  UCHAR PipeID,
		__in  ULONG PolicyType,
		__in  ULONG ValueLength,
		__in_bcount(ValueLength) PVOID Value
	);
	BOOL Free(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle
    );
	BOOL GetDescriptor(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle,
		__in  UCHAR DescriptorType,
		__in  UCHAR Index,
		__in  USHORT LanguageID,
		__out_bcount_part_opt(BufferLength, *LengthTransferred) PUCHAR Buffer,
		__in  ULONG BufferLength,
		__out PULONG LengthTransferred
    );
	BOOL ReadPipe(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle,
		__in  UCHAR PipeID,
		__out_bcount_part_opt(BufferLength,*LengthTransferred) PUCHAR Buffer,
		__in  ULONG BufferLength,
		__out_opt PULONG LengthTransferred,
		__in_opt LPOVERLAPPED Overlapped
    );
	BOOL WritePipe(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle,
		__in  UCHAR PipeID,
		__in_bcount(BufferLength) PUCHAR Buffer,
		__in  ULONG BufferLength,
		__out_opt PULONG LengthTransferred,
		__in_opt LPOVERLAPPED Overlapped    
    );
	BOOL ControlTransfer(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle,
		__in  WINUSB_SETUP_PACKET SetupPacket,
		__out_bcount_part_opt(BufferLength, *LengthTransferred) PUCHAR Buffer,
		__in  ULONG BufferLength,
		__out_opt PULONG LengthTransferred,
		__in_opt  LPOVERLAPPED Overlapped    
    );

}


#include "CapsLibAll.h"

namespace CAPS
{
	SDWORD Init();
	SDWORD Exit();
	SDWORD AddImage();
	SDWORD RemImage(SDWORD id);
	SDWORD LockImage(SDWORD id, PCHAR name);
	SDWORD LockImageMemory(SDWORD id, PUBYTE buffer, UDWORD length, UDWORD flag);
	SDWORD UnlockImage(SDWORD id);
	SDWORD LoadImage(SDWORD id, UDWORD flag);
	SDWORD GetImageInfo(PCAPSIMAGEINFO pi, SDWORD id);
	SDWORD LockTrack(PVOID ptrackinfo, SDWORD id, UDWORD cylinder, UDWORD head, UDWORD flag);
	SDWORD UnlockTrack(SDWORD id, UDWORD cylinder, UDWORD head);
	SDWORD UnlockAllTracks(SDWORD id);
	PCHAR  GetPlatformName(UDWORD pid);
	SDWORD GetVersionInfo(PVOID pversioninfo, UDWORD flag);
	UDWORD FdcGetInfo(SDWORD iid, PCAPSFDC pc, SDWORD ext);
	SDWORD FdcInit(PCAPSFDC pc);
	void   FdcReset(PCAPSFDC pc);
	void   FdcEmulate(PCAPSFDC pc, UDWORD cyclecnt);
	UDWORD FdcRead(PCAPSFDC pc, UDWORD address);
	void   FdcWrite(PCAPSFDC pc, UDWORD address, UDWORD data);
	SDWORD FdcInvalidateTrack(PCAPSFDC pc, SDWORD drive);
	SDWORD FormatDataToMFM(PVOID pformattrack, UDWORD flag);
	SDWORD GetInfo(PVOID pinfo, SDWORD id, UDWORD cylinder, UDWORD head, UDWORD inftype, UDWORD infid);
	SDWORD SetRevolution(SDWORD id, UDWORD value);
	SDWORD GetImageType(PCHAR name);
	SDWORD GetImageTypeMemory(PUBYTE buffer, UDWORD length);
	SDWORD GetDebugRequest();
}

#endif // OPTLIBAPIS_H
