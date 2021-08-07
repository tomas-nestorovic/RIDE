#ifndef STDAFX_H
#define STDAFX_H

#define OEMRESOURCE	/* aby mozno pouzit preddefinovane OBM_* zdroje */
#include <windows.h>
#include <WindowsX.h>
#include <tchar.h>
#include <commctrl.h>

#include "api.h"
#include "Tdi.h"

#ifdef RELEASE_MFC42
	#if _MSC_VER<=1600
		#define noexcept
	#endif

	void __cdecl operator delete(PVOID ptr, UINT sz) noexcept;
#endif

#endif
