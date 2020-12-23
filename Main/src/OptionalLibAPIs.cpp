#include "stdafx.h"

namespace CAPS
{
	static HMODULE hLib;

	typedef SDWORD (__cdecl *CAPSHOOKN)(...);
	typedef PCHAR  (__cdecl *CAPSHOOKS)(...);

	static CAPSHOOKN fnAddImage,fnRemImage,fnLockImage,fnUnlockImage,fnGetImageInfo;
	static CAPSHOOKN fnLockTrack,fnUnlockTrack,fnUnlockAllTracks;

	SDWORD __cdecl GetVersionInfo(PVOID pversioninfo, UDWORD flag){
		// - loading the library
		if (!hLib)
			if (!( hLib=::LoadLibrary(_T("CAPSimg.dll")) ))
				return imgeUnsupported;
		// - binding to its often used functions (less often functions are bound when they are called)
		fnAddImage=(CAPSHOOKN)::GetProcAddress(hLib,_T("CAPSAddImage"));
		fnRemImage=(CAPSHOOKN)::GetProcAddress(hLib,_T("CAPSRemImage"));
		fnLockImage=(CAPSHOOKN)::GetProcAddress(hLib,_T("CAPSLockImage"));
		fnUnlockImage=(CAPSHOOKN)::GetProcAddress(hLib,_T("CAPSUnlockImage"));
		fnGetImageInfo=(CAPSHOOKN)::GetProcAddress(hLib,_T("CAPSGetImageInfo"));
		fnLockTrack=(CAPSHOOKN)::GetProcAddress(hLib,_T("CAPSLockTrack"));
		fnUnlockTrack=(CAPSHOOKN)::GetProcAddress(hLib,_T("CAPSUnlockTrack"));
		fnUnlockAllTracks=(CAPSHOOKN)::GetProcAddress(hLib,_T("CAPSUnlockAllTracks"));
		// - performing this function
		if (const CAPSHOOKN fnGetVersionInfo=(CAPSHOOKN)::GetProcAddress(hLib,_T("CAPSGetVersionInfo")))
			return fnGetVersionInfo( pversioninfo, flag );
		else
			return imgeUnsupported;
	}

	SDWORD __cdecl Init(){
		if (hLib)
			if (const CAPSHOOKN fnInit=(CAPSHOOKN)::GetProcAddress(hLib,_T("CAPSInit")))
				return fnInit();
		return imgeUnsupported;
	}

	SDWORD __cdecl Exit(){
		SDWORD result=imgeUnsupported;
		if (hLib){
			if (const CAPSHOOKN fnExit=(CAPSHOOKN)::GetProcAddress(hLib,_T("CAPSExit")))
				result=fnExit();
			::FreeLibrary(hLib);
		}
		return result;
	}

	SDWORD __cdecl AddImage(){
		return	fnAddImage ? fnAddImage() : imgeUnsupported;
	}

	SDWORD __cdecl RemImage(SDWORD id){
		return	fnRemImage ? fnRemImage(id) : imgeUnsupported;
	}

	SDWORD __cdecl LockImage(SDWORD id, PCHAR name){
		return	fnLockImage ? fnLockImage(id,name) : imgeUnsupported;
	}

	SDWORD __cdecl UnlockImage(SDWORD id){
		return	fnUnlockImage ? fnUnlockImage(id) : imgeUnsupported;
	}

	SDWORD __cdecl GetImageInfo(PCAPSIMAGEINFO pi, SDWORD id){
		return	fnGetImageInfo ? fnGetImageInfo(pi,id) : imgeUnsupported;
	}

	SDWORD __cdecl LockTrack(PVOID ptrackinfo, SDWORD id, UDWORD cylinder, UDWORD head, UDWORD flag){
		return	fnLockTrack
				? fnLockTrack( ptrackinfo, id, cylinder, head, flag )
				: imgeUnsupported;
	}

	SDWORD __cdecl UnlockTrack(SDWORD id, UDWORD cylinder, UDWORD head){
		return	fnUnlockTrack ? fnUnlockTrack(id,cylinder,head) : imgeUnsupported;
	}

	SDWORD __cdecl UnlockAllTracks(SDWORD id){
		return	fnUnlockAllTracks ? fnUnlockAllTracks(id) : imgeUnsupported;
	}

	PCHAR  __cdecl GetPlatformName(UDWORD pid){
		if (hLib)
			if (const CAPSHOOKS fnGetPlatformName=(CAPSHOOKS)::GetProcAddress(hLib,_T("CAPSGetPlatformName")))
				return fnGetPlatformName(pid);
		return nullptr;
	}

	SDWORD __cdecl FormatDataToMFM(PVOID pformattrack, UDWORD flag){
		if (hLib)
			if (const CAPSHOOKN fnFormatDataToMFM=(CAPSHOOKN)::GetProcAddress(hLib,_T("CAPSFormatDataToMFM")))
				return fnFormatDataToMFM(pformattrack,flag);
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
