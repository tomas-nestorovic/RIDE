#include "stdafx.h"

static PVOID GetProcedure(HMODULE &rhLib,LPCTSTR libName,LPCTSTR procName){
	if (!rhLib)
		if (!( rhLib=::LoadLibrary(libName) ))
			return nullptr;
	return ::GetProcAddress( rhLib, procName );
}


namespace CAPS
{
	static HMODULE hLib;

	typedef SDWORD (__cdecl *CAPSHOOKN)(...);
	typedef PCHAR  (__cdecl *CAPSHOOKS)(...);

	inline PVOID GetProcedure(LPCTSTR procName){
		return	GetProcedure( hLib, _T("CAPSimg.dll"), procName );
	}

	SDWORD GetVersionInfo(PVOID pversioninfo, UDWORD flag){
		if (const auto f=(CAPSHOOKN)GetProcedure(_T("CAPSGetVersionInfo")))
			return f( pversioninfo, flag );
		return imgeUnsupported;
	}

	SDWORD Init(){
		if (const auto fnInit=(CAPSHOOKN)GetProcedure(_T("CAPSInit")))
			return fnInit();
		return imgeUnsupported;
	}

	SDWORD Exit(){
		if (const auto fnExit=(CAPSHOOKN)GetProcedure(_T("CAPSExit")))
			return fnExit();
		return imgeUnsupported;
	}

	SDWORD AddImage(){
		if (const auto f=(CAPSHOOKN)GetProcedure(_T("CAPSAddImage")))
			return f();
		return imgeUnsupported;
	}

	SDWORD RemImage(SDWORD id){
		if (const auto f=(CAPSHOOKN)GetProcedure(_T("CAPSRemImage")))
			return f(id);
		return imgeUnsupported;
	}

	SDWORD LockImage(SDWORD id, PCHAR name){
		if (const auto f=(CAPSHOOKN)GetProcedure(_T("CAPSLockImage")))
			return f( id, name );
		return imgeUnsupported;
	}

	SDWORD UnlockImage(SDWORD id){
		if (const auto f=(CAPSHOOKN)GetProcedure(_T("CAPSUnlockImage")))
			return f(id);
		return imgeUnsupported;
	}

	SDWORD GetImageInfo(PCAPSIMAGEINFO pi, SDWORD id){
		if (const auto f=(CAPSHOOKN)GetProcedure(_T("CAPSGetImageInfo")))
			return f( pi, id );
		return imgeUnsupported;
	}

	SDWORD LockTrack(PVOID ptrackinfo, SDWORD id, UDWORD cylinder, UDWORD head, UDWORD flag){
		if (const auto f=(CAPSHOOKN)GetProcedure(_T("CAPSLockTrack")))
			return f( ptrackinfo, id, cylinder, head, flag );
		return imgeUnsupported;
	}

	SDWORD UnlockTrack(SDWORD id, UDWORD cylinder, UDWORD head){
		if (const auto f=(CAPSHOOKN)GetProcedure(_T("CAPSUnlockTrack")))
			return f( id, cylinder, head );
		return imgeUnsupported;
	}

	SDWORD UnlockAllTracks(SDWORD id){
		if (const auto f=(CAPSHOOKN)GetProcedure(_T("CAPSUnlockAllTracks")))
			return f(id);
		return imgeUnsupported;
	}

	PCHAR GetPlatformName(UDWORD pid){
		if (const auto f=(CAPSHOOKS)GetProcedure(_T("CAPSGetPlatformName")))
			return f(pid);
		return nullptr;
	}

	SDWORD FormatDataToMFM(PVOID pformattrack, UDWORD flag){
		if (const auto f=(CAPSHOOKN)GetProcedure(_T("CAPSFormatDataToMFM")))
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
