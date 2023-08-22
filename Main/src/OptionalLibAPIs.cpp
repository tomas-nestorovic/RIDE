#include "stdafx.h"
#ifdef DEFINE_DEVPROPKEY
	#undef DEFINE_DEVPROPKEY
#endif
#ifdef INITGUID
	#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const DEVPROPKEY DECLSPEC_SELECTANY name = { { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }, pid }
#else
	#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const DEVPROPKEY name
#endif // INITGUID

static PVOID GetProcedure(HMODULE &rhLib,LPCSTR libName,LPCSTR procName){
	if (!rhLib)
		if (!( rhLib=::LoadLibraryA(libName) ))
			return nullptr;
	return ::GetProcAddress( rhLib, procName );
}


namespace UxTheme
{
	static HMODULE hLib;

	inline PVOID GetProcedure(LPCSTR procName){
		return	GetProcedure( hLib, "UxTheme.dll", procName );
	}

	HTHEME OpenThemeData(HWND hwnd,LPCWSTR pszClassList){
		typedef HTHEME (WINAPI *F)(HWND,LPCWSTR);
		if (const F f=(F)GetProcedure("OpenThemeData"))
			return f( hwnd, pszClassList );
		return nullptr;
	}

	HRESULT DrawThemeBackground(HTHEME hTheme,HDC hdc,int iPartId,int iStateId,LPCRECT pRect,LPCRECT pClipRect){
		typedef HRESULT (WINAPI *F)(HTHEME,HDC,int,int,LPCRECT,LPCRECT);
		if (const F f=(F)GetProcedure("DrawThemeBackground"))
			return f( hTheme, hdc, iPartId, iStateId, pRect, pClipRect );
		return S_FALSE;
	}

	HRESULT CloseThemeData(HTHEME hTheme){
		typedef HRESULT (WINAPI *F)(HTHEME);
		if (const F f=(F)GetProcedure("CloseThemeData"))
			return f(hTheme);
		return S_FALSE;
	}

}


namespace SetupDi
{
DEFINE_GUID( GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED );

namespace Lib
{
	static HMODULE hLib;

	inline PVOID GetProcedure(LPCSTR procName){
		return	GetProcedure( hLib, "SetupAPI.dll", procName );
	}

	HDEVINFO GetClassDevs(
		__in_opt CONST GUID &ClassGuid,
		__in_opt PCTSTR Enumerator,
		__in_opt HWND hwndParent,
		__in DWORD Flags
	){
		typedef HDEVINFO (__stdcall *F)(CONST GUID *,PCTSTR,HWND,DWORD);
		#ifdef UNICODE
			if (const F f=(F)GetProcedure("SetupDiGetClassDevsW"))
		#else
			if (const F f=(F)GetProcedure("SetupDiGetClassDevsA"))
		#endif
				return f( &ClassGuid, Enumerator, hwndParent, Flags );
		return nullptr;
	}

	BOOL EnumDeviceInterfaces(
		__in HDEVINFO DeviceInfoSet,
		__in_opt PSP_DEVINFO_DATA DeviceInfoData,
		__in CONST GUID &InterfaceClassGuid,
		__in DWORD MemberIndex,
		__out PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData
    ){
		typedef BOOL (__stdcall *F)(HDEVINFO,PSP_DEVINFO_DATA,CONST GUID *,DWORD,PSP_DEVICE_INTERFACE_DATA);
		if (const F f=(F)GetProcedure("SetupDiEnumDeviceInterfaces"))
			return f( DeviceInfoSet, DeviceInfoData, &InterfaceClassGuid, MemberIndex, DeviceInterfaceData );
		return FALSE;
	}

	BOOL GetDeviceInterfaceDetail(
		__in HDEVINFO DeviceInfoSet,
		__in PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
		__out_bcount_opt(DeviceInterfaceDetailDataSize) PSP_DEVICE_INTERFACE_DETAIL_DATA DeviceInterfaceDetailData,
		__in DWORD DeviceInterfaceDetailDataSize,
		__out_opt PDWORD RequiredSize, 
		__out_opt PSP_DEVINFO_DATA DeviceInfoData
    ){
		typedef BOOL (__stdcall *F)(HDEVINFO,PSP_DEVICE_INTERFACE_DATA,PSP_DEVICE_INTERFACE_DETAIL_DATA,DWORD,PDWORD,PSP_DEVINFO_DATA);
		#ifdef UNICODE
			if (const F f=(F)GetProcedure("SetupDiGetDeviceInterfaceDetailW"))
		#else
			if (const F f=(F)GetProcedure("SetupDiGetDeviceInterfaceDetailA"))
		#endif
				return f( DeviceInfoSet, DeviceInterfaceData, DeviceInterfaceDetailData, DeviceInterfaceDetailDataSize, RequiredSize, DeviceInfoData );
		return FALSE;
	}

	BOOL GetDevicePropertyW(
		__in HDEVINFO DeviceInfoSet,
		__in PSP_DEVINFO_DATA DeviceInfoData,
		__in CONST DEVPROPKEY &PropertyKey,
		__out DEVPROPTYPE &PropertyType,
		__out_bcount_opt(PropertyBufferSize) PBYTE PropertyBuffer,
		__in DWORD PropertyBufferSize,
		__out_opt PDWORD RequiredSize,
		__in DWORD Flags
	){
		typedef BOOL (__stdcall *F)(HDEVINFO,PSP_DEVINFO_DATA,CONST DEVPROPKEY *,DEVPROPTYPE *,PBYTE,DWORD,PDWORD,DWORD);
		if (const F f=(F)GetProcedure("SetupDiGetDevicePropertyW"))
			return f( DeviceInfoSet, DeviceInfoData, &PropertyKey, &PropertyType, PropertyBuffer, PropertyBufferSize, RequiredSize, Flags );
		return FALSE;
	}

	BOOL GetDeviceRegistryPropertyW(
		__in HDEVINFO DeviceInfoSet,
		__in PSP_DEVINFO_DATA DeviceInfoData,
		__in DWORD Property,
		__out_opt PDWORD PropertyRegDataType, 
		__out_bcount_opt(PropertyBufferSize) PBYTE PropertyBuffer,
		__in DWORD PropertyBufferSize,
		__out_opt PDWORD RequiredSize 
	){
		typedef BOOL (__stdcall *F)(HDEVINFO,PSP_DEVINFO_DATA,DWORD,PDWORD,PBYTE,DWORD,PDWORD);
		if (const F f=(F)GetProcedure("SetupDiGetDeviceRegistryPropertyW"))
			return f( DeviceInfoSet, DeviceInfoData, Property, PropertyRegDataType, PropertyBuffer, PropertyBufferSize, RequiredSize );
		return FALSE;
	}

	BOOL DestroyDeviceInfoList(
		__in HDEVINFO DeviceInfoSet
	){
		typedef BOOL (__stdcall *F)(HDEVINFO);
		if (const F f=(F)GetProcedure("SetupDiDestroyDeviceInfoList"))
			return f(DeviceInfoSet);
		return FALSE;
	}
} // namespace Lib


	DEFINE_DEVPROPKEY( DEVPKEY_Device_BusReportedDeviceDesc, 0x540b947e, 0x8b40, 0x45bc, 0xa8, 0xa2, 0x6a, 0x0b, 0x89, 0x4c, 0xbd, 0xa2, 4 );

	CString GetDevicePath(const GUID &interfaceGuid,LPCWSTR deviceNameSubstr){
		// determines and returns the path of a locally connected Device; returns Null if device not found
		const HDEVINFO hDevList=Lib::GetClassDevs( interfaceGuid, nullptr, nullptr, DIGCF_PRESENT|DIGCF_DEVICEINTERFACE );
		if (hDevList==INVALID_HANDLE_VALUE) // not connected to this computer
			return _T("");
		struct{
			SP_DEVICE_INTERFACE_DETAIL_DATA detail;
			TCHAR buffer[2000];
		} str;
		str.detail.cbSize=sizeof(str.detail);
		bool found=false; // assumption (no Device with such substring in location name)
		for( DWORD i=0,dw; !found; i++ ){
			SP_DEVICE_INTERFACE_DATA devIntfData={ sizeof(devIntfData) };
			if (!Lib::EnumDeviceInterfaces( hDevList, nullptr, interfaceGuid, i, &devIntfData ))
				break; // no more Devices with given Guid to enumerate
			SP_DEVINFO_DATA did={ sizeof(did) };
			if (!Lib::GetDeviceInterfaceDetail( hDevList, &devIntfData, &str.detail, sizeof(str), nullptr, &did ))
				continue; // failed to retrieve this Device's path
			if (deviceNameSubstr){
				WCHAR str[128];
				DEVPROPTYPE propType;
				if (// Windows 7 and later
					!Lib::GetDevicePropertyW( hDevList, &did, DEVPKEY_Device_BusReportedDeviceDesc, propType, (PBYTE)str, sizeof(str), nullptr, 0 )
					&&
					// Windows Vista and earlier
					!Lib::GetDeviceRegistryPropertyW( hDevList, &did, SPDRP_LOCATION_INFORMATION, &dw, (PBYTE)str, sizeof(str), nullptr )
				)
					continue; // failed to obtain the necessary property
				if (!::StrStrIW( str, deviceNameSubstr ))
					continue; // not containing the searched substring
			}
			found=true;
		}
		Lib::DestroyDeviceInfoList(hDevList);
		return	found ? str.detail.DevicePath : _T("");
	}
}


namespace WinUsb
{
namespace Lib
{
	static HMODULE hLib;

	inline PVOID GetProcedure(LPCSTR procName){
		return	GetProcedure( hLib, "WinUsb.dll", procName );
	}

	BOOL Initialize(
		__in  HANDLE DeviceHandle,
		__out PWINUSB_INTERFACE_HANDLE InterfaceHandle
	){
		typedef BOOL (__stdcall *F)(HANDLE,PWINUSB_INTERFACE_HANDLE);
		if (const F f=(F)GetProcedure("WinUsb_Initialize"))
			return f( DeviceHandle, InterfaceHandle );
		return FALSE;
	}

	BOOL GetAssociatedInterface(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle,
		__in  UCHAR AssociatedInterfaceIndex,
		__out PWINUSB_INTERFACE_HANDLE AssociatedInterfaceHandle
	){
		typedef BOOL (__stdcall *F)(WINUSB_INTERFACE_HANDLE,UCHAR,PWINUSB_INTERFACE_HANDLE);
		if (const F f=(F)GetProcedure("WinUsb_GetAssociatedInterface"))
			return f( InterfaceHandle, AssociatedInterfaceIndex, AssociatedInterfaceHandle );
		return FALSE;
	}

	BOOL SetPipePolicy(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle,
		__in  UCHAR PipeID,
		__in  ULONG PolicyType,
		__in  ULONG ValueLength,
		__in_bcount(ValueLength) PVOID Value
	){
		typedef BOOL (__stdcall *F)(WINUSB_INTERFACE_HANDLE,UCHAR,ULONG,ULONG,PVOID);
		if (const F f=(F)GetProcedure("WinUsb_SetPipePolicy"))
			return f( InterfaceHandle, PipeID, PolicyType, ValueLength, Value );
		return FALSE;
	}

	BOOL Free(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle
    ){
		typedef BOOL (__stdcall *F)(WINUSB_INTERFACE_HANDLE);
		if (const F f=(F)GetProcedure("WinUsb_Free"))
			return f(InterfaceHandle);
		return FALSE;
	}

	BOOL GetDescriptor(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle,
		__in  UCHAR DescriptorType,
		__in  UCHAR Index,
		__in  USHORT LanguageID,
		__out_bcount_part_opt(BufferLength, *LengthTransferred) PUCHAR Buffer,
		__in  ULONG BufferLength,
		__out PULONG LengthTransferred
    ){
		typedef BOOL (__stdcall *F)(WINUSB_INTERFACE_HANDLE,UCHAR,UCHAR,USHORT,PUCHAR,ULONG,PULONG);
		if (const F f=(F)GetProcedure("WinUsb_GetDescriptor"))
			return f( InterfaceHandle, DescriptorType, Index, LanguageID, Buffer, BufferLength, LengthTransferred );
		return FALSE;
	}

	BOOL ReadPipe(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle,
		__in  UCHAR PipeID,
		__out_bcount_part_opt(BufferLength,*LengthTransferred) PUCHAR Buffer,
		__in  ULONG BufferLength,
		__out_opt PULONG LengthTransferred,
		__in_opt LPOVERLAPPED Overlapped
    ){
		typedef BOOL (__stdcall *F)(WINUSB_INTERFACE_HANDLE,UCHAR,PUCHAR,ULONG,PULONG,LPOVERLAPPED);
		if (const F f=(F)GetProcedure("WinUsb_ReadPipe"))
			return f( InterfaceHandle, PipeID, Buffer, BufferLength, LengthTransferred, Overlapped );
		return FALSE;
	}

	BOOL WritePipe(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle,
		__in  UCHAR PipeID,
		__in_bcount(BufferLength) PUCHAR Buffer,
		__in  ULONG BufferLength,
		__out_opt PULONG LengthTransferred,
		__in_opt LPOVERLAPPED Overlapped    
    ){
		typedef BOOL (__stdcall *F)(WINUSB_INTERFACE_HANDLE,UCHAR,PUCHAR,ULONG,PULONG,LPOVERLAPPED);
		if (const F f=(F)GetProcedure("WinUsb_WritePipe"))
			return f( InterfaceHandle, PipeID, Buffer, BufferLength, LengthTransferred, Overlapped );
		return FALSE;
	}

	BOOL ControlTransfer(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle,
		__in  WINUSB_SETUP_PACKET SetupPacket,
		__out_bcount_part_opt(BufferLength, *LengthTransferred) PUCHAR Buffer,
		__in  ULONG BufferLength,
		__out_opt PULONG LengthTransferred,
		__in_opt  LPOVERLAPPED Overlapped    
    ){
		typedef BOOL (__stdcall *F)(WINUSB_INTERFACE_HANDLE,WINUSB_SETUP_PACKET,PUCHAR,ULONG,PULONG,LPOVERLAPPED);
		if (const F f=(F)GetProcedure("WinUsb_ControlTransfer"))
			return f( InterfaceHandle, SetupPacket, Buffer, BufferLength, LengthTransferred, Overlapped );
		return FALSE;
	}

	BOOL ResetPipe(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle,
		__in  UCHAR PipeID
	){
		typedef BOOL (__stdcall *F)(WINUSB_INTERFACE_HANDLE,UCHAR);
		if (const F f=(F)GetProcedure("WinUsb_ResetPipe"))
			return f( InterfaceHandle, PipeID );
		return FALSE;
	}

	BOOL AbortPipe(
		__in  WINUSB_INTERFACE_HANDLE InterfaceHandle,
		__in  UCHAR PipeID
	){
		typedef BOOL (__stdcall *F)(WINUSB_INTERFACE_HANDLE,UCHAR);
		if (const F f=(F)GetProcedure("WinUsb_AbortPipe"))
			return f( InterfaceHandle, PipeID );
		return FALSE;
	}
} // namespace Lib


	void TDevInterfaceHandle::Clear(){
		hLibrary = hDeviceInterface = INVALID_HANDLE_VALUE;
	}

	bool TDevInterfaceHandle::ConnectToInterface(HANDLE hDevice,UCHAR interfaceIndex){
		// 
		ASSERT( hLibrary==INVALID_HANDLE_VALUE && hDeviceInterface==INVALID_HANDLE_VALUE );
		return	Lib::Initialize( hDevice, &hLibrary )!=0
				&&
				Lib::GetAssociatedInterface( hLibrary, interfaceIndex, &hDeviceInterface )!=0;
	}

	void TDevInterfaceHandle::DisconnectFromInterface(){
		//
		if (hDeviceInterface!=INVALID_HANDLE_VALUE)
			Lib::Free( hDeviceInterface );
		if (hLibrary!=INVALID_HANDLE_VALUE)
			Lib::Free( hLibrary );
		Clear();
	}

	LPCSTR TDevInterfaceHandle::GetProductName(PCHAR productNameBuffer,BYTE productNameBufferLength) const{
		// determines and returns the product introduced upon the device connection
		DWORD nBytesTransferred;
		USB_DEVICE_DESCRIPTOR desc={ sizeof(desc), USB_DEVICE_DESCRIPTOR_TYPE };
		if (Lib::GetDescriptor(
				hLibrary,
				USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, (PUCHAR)&desc, sizeof(desc),
				&nBytesTransferred
			)!=0
			&&
			desc.iProduct>0
		){
			struct{
				USB_STRING_DESCRIPTOR desc;
				WCHAR buf[MAX_PATH];
			} strW;
			if (Lib::GetDescriptor(
					hLibrary,
					USB_STRING_DESCRIPTOR_TYPE, desc.iProduct, 0, (PUCHAR)&strW, sizeof(strW),
					&nBytesTransferred
				)!=0
				&&
				strW.desc.bLength>2
			){
				productNameBuffer[
					::WideCharToMultiByte( CP_ACP, 0, strW.desc.bString,(strW.desc.bLength-2)/sizeof(WCHAR), productNameBuffer,productNameBufferLength, nullptr,nullptr )
				]='\0';
				return productNameBuffer;
			}
		}
		return nullptr;
	}

	void TDevInterfaceHandle::SetPipePolicy(UCHAR bulkPipeId,BYTE enable,DWORD msTimeout) const{
		Lib::SetPipePolicy( hDeviceInterface, bulkPipeId, SHORT_PACKET_TERMINATE, sizeof(enable), &enable );
		Lib::SetPipePolicy( hDeviceInterface, bulkPipeId, AUTO_CLEAR_STALL, sizeof(enable), &enable );
		Lib::SetPipePolicy( hDeviceInterface, bulkPipeId, PIPE_TRANSFER_TIMEOUT, sizeof(msTimeout), &msTimeout ); // milliseconds
	}

	void TDevInterfaceHandle::ClearIoPipes(UCHAR bulkInPipeId,UCHAR bulkOutPipeId) const{
		// clears i/o pipes to communicate with the device
		while (!Lib::AbortPipe( hDeviceInterface, bulkOutPipeId ));
		Lib::ResetPipe( hDeviceInterface, bulkOutPipeId );
		while (!Lib::AbortPipe( hDeviceInterface, bulkInPipeId ));
		Lib::ResetPipe( hDeviceInterface, bulkInPipeId );
	}

}


namespace CAPS
{
	static HMODULE hLib;

	typedef SDWORD (__cdecl *CAPSHOOKN)(...);
	typedef PCHAR  (__cdecl *CAPSHOOKS)(...);

	inline PVOID GetProcedure(LPCSTR procName){
		return	GetProcedure( hLib, "CAPSimg.dll", procName );
	}

	SDWORD GetVersionInfo(PVOID pversioninfo, UDWORD flag){
		if (const auto f=(CAPSHOOKN)GetProcedure("CAPSGetVersionInfo"))
			return f( pversioninfo, flag );
		return imgeUnsupported;
	}

	SDWORD Init(){
		if (const auto fnInit=(CAPSHOOKN)GetProcedure("CAPSInit"))
			return fnInit();
		return imgeUnsupported;
	}

	SDWORD Exit(){
		if (const auto fnExit=(CAPSHOOKN)GetProcedure("CAPSExit"))
			return fnExit();
		return imgeUnsupported;
	}

	SDWORD AddImage(){
		if (const auto f=(CAPSHOOKN)GetProcedure("CAPSAddImage"))
			return f();
		return imgeUnsupported;
	}

	SDWORD RemImage(SDWORD id){
		if (const auto f=(CAPSHOOKN)GetProcedure("CAPSRemImage"))
			return f(id);
		return imgeUnsupported;
	}

	SDWORD LockImage(SDWORD id, LPCSTR name){
		if (const auto f=(CAPSHOOKN)GetProcedure("CAPSLockImage"))
			return f( id, name );
		return imgeUnsupported;
	}

	SDWORD UnlockImage(SDWORD id){
		if (const auto f=(CAPSHOOKN)GetProcedure("CAPSUnlockImage"))
			return f(id);
		return imgeUnsupported;
	}

	SDWORD GetImageInfo(PCAPSIMAGEINFO pi, SDWORD id){
		if (const auto f=(CAPSHOOKN)GetProcedure("CAPSGetImageInfo"))
			return f( pi, id );
		return imgeUnsupported;
	}

	SDWORD LockTrack(PVOID ptrackinfo, SDWORD id, UDWORD cylinder, UDWORD head, UDWORD flag){
		if (const auto f=(CAPSHOOKN)GetProcedure("CAPSLockTrack"))
			return f( ptrackinfo, id, cylinder, head, flag );
		return imgeUnsupported;
	}

	SDWORD UnlockTrack(SDWORD id, UDWORD cylinder, UDWORD head){
		if (const auto f=(CAPSHOOKN)GetProcedure("CAPSUnlockTrack"))
			return f( id, cylinder, head );
		return imgeUnsupported;
	}

	SDWORD UnlockAllTracks(SDWORD id){
		if (const auto f=(CAPSHOOKN)GetProcedure("CAPSUnlockAllTracks"))
			return f(id);
		return imgeUnsupported;
	}

	PCHAR GetPlatformName(UDWORD pid){
		if (const auto f=(CAPSHOOKS)GetProcedure("CAPSGetPlatformName"))
			return f(pid);
		return nullptr;
	}

	SDWORD FormatDataToMFM(PVOID pformattrack, UDWORD flag){
		if (const auto f=(CAPSHOOKN)GetProcedure("CAPSFormatDataToMFM"))
			return f( pformattrack, flag );
		return imgeUnsupported;
	}


/*	UDWORD __cdecl FdcGetInfo(SDWORD iid, PFDC pc, SDWORD ext);
	SDWORD __cdecl FdcInit(PFDC pc);
	void   __cdecl FdcReset(PFDC pc);
	void   __cdecl FdcEmulate(PFDC pc, UDWORD cyclecnt);
	UDWORD __cdecl FdcRead(PFDC pc, UDWORD address);
	void   __cdecl FdcWrite(PFDC pc, UDWORD address, UDWORD data);
	SDWORD __cdecl FdcInvalidateTrack(PFDC pc, SDWORD drive);
	SDWORD __cdecl GetInfo(PVOID pinfo, SDWORD id, UDWORD cylinder, UDWORD head, UDWORD inftype, UDWORD infid);
	SDWORD __cdecl SetRevolution(SDWORD id, UDWORD value);
	SDWORD __cdecl GetImageType(PCHAR name);
	SDWORD __cdecl GetImageTypeMemory(PUBYTE buffer, UDWORD length);
	SDWORD __cdecl GetDebugRequest();

	//*/
}
