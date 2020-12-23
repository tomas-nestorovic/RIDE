#ifndef OPTLIBAPIS_H
#define OPTLIBAPIS_H

#include "CapsLibAll.h"

namespace CAPS
{
	SDWORD __cdecl Init();
	SDWORD __cdecl Exit();
	SDWORD __cdecl AddImage();
	SDWORD __cdecl RemImage(SDWORD id);
	SDWORD __cdecl LockImage(SDWORD id, PCHAR name);
	SDWORD __cdecl LockImageMemory(SDWORD id, PUBYTE buffer, UDWORD length, UDWORD flag);
	SDWORD __cdecl UnlockImage(SDWORD id);
	SDWORD __cdecl LoadImage(SDWORD id, UDWORD flag);
	SDWORD __cdecl GetImageInfo(PCAPSIMAGEINFO pi, SDWORD id);
	SDWORD __cdecl LockTrack(PVOID ptrackinfo, SDWORD id, UDWORD cylinder, UDWORD head, UDWORD flag);
	SDWORD __cdecl UnlockTrack(SDWORD id, UDWORD cylinder, UDWORD head);
	SDWORD __cdecl UnlockAllTracks(SDWORD id);
	PCHAR  __cdecl GetPlatformName(UDWORD pid);
	SDWORD __cdecl GetVersionInfo(PVOID pversioninfo, UDWORD flag);
	UDWORD __cdecl FdcGetInfo(SDWORD iid, PCAPSFDC pc, SDWORD ext);
	SDWORD __cdecl FdcInit(PCAPSFDC pc);
	void   __cdecl FdcReset(PCAPSFDC pc);
	void   __cdecl FdcEmulate(PCAPSFDC pc, UDWORD cyclecnt);
	UDWORD __cdecl FdcRead(PCAPSFDC pc, UDWORD address);
	void   __cdecl FdcWrite(PCAPSFDC pc, UDWORD address, UDWORD data);
	SDWORD __cdecl FdcInvalidateTrack(PCAPSFDC pc, SDWORD drive);
	SDWORD __cdecl FormatDataToMFM(PVOID pformattrack, UDWORD flag);
	SDWORD __cdecl GetInfo(PVOID pinfo, SDWORD id, UDWORD cylinder, UDWORD head, UDWORD inftype, UDWORD infid);
	SDWORD __cdecl SetRevolution(SDWORD id, UDWORD value);
	SDWORD __cdecl GetImageType(PCHAR name);
	SDWORD __cdecl GetImageTypeMemory(PUBYTE buffer, UDWORD length);
	SDWORD __cdecl GetDebugRequest();
}

#endif // OPTLIBAPIS_H
