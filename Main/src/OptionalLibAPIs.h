#ifndef OPTLIBAPIS_H
#define OPTLIBAPIS_H

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
