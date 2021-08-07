#ifndef STDAFX_H
#define STDAFX_H

// use Common-Controls library version 6
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define OEMRESOURCE	/* to enable using of predefined OBM_* resources */
#include <windows.h>
#include <WindowsX.h>
#include <cstdio>
#include <tchar.h>
#include <commctrl.h>
#include <shlwapi.h>

#include <algorithm>
//#include <cmath>

typedef const struct TEditor *PCEditor; // forward

#include "api.h"
#include "PropGrid.h"
#include "Editors.h"

#undef min
#undef max

#ifdef RELEASE_MFC42
	#if _MSC_VER<=1600
		#define noexcept
	#endif

	void __cdecl operator delete(PVOID ptr, UINT sz) noexcept;
#endif

#endif
