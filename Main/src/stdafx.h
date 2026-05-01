#ifndef STDAFX_H
#define STDAFX_H

#include <cstdint>
#define _USE_MATH_DEFINES // for M_PI to be defined
#include <cmath>
#ifdef RELEASE_MFC42	// preventing from usage of C++ exceptions in releases compiled against legacy MFC 4.2
	#define _HAS_EXCEPTIONS 0
	#include <xstddef>
	#define _RAISE(x)
	#define _RERAISE
	#define _THROW(x,y)
	#define _THROW_NCEE(x,y)
	#define _HAS_EXCEPTIONS 1
	#include <exception>
	#define _HAS_EXCEPTIONS 0
	#include <typeinfo>
	#define bad_cast(x)
	#define bad_typeid(x)
	#define _CRTIMP2_PURE
	#include <xutility>
	#define _Xout_of_range(x)
	#define _Xlength_error(x)
	#define _Xbad_alloc()
	#define _Xinvalid_argument(x)
#endif
#include <algorithm>
#include <memory>
#include <map>
#include <set>

// use Common-Controls library version 6
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define OEMRESOURCE
#include <afxwin.h>
#include <afxpriv.h>
#include <afxext.h>
#include <afxcview.h>
#include <afxhtml.h>
#include <afxmt.h>
//#include <afxrich.h>
//#include <afxcmn.h>
#include <afxpriv.h>
//#include <afxtempl.h>
#ifdef RELEASE_MFC42
	#include <crtnew.h>
#endif
#include <atlbase.h>
#include <Windows.h>
#include <WindowsX.h>
//#define _WIN32_WINNT 0x501
#include <CommCtrl.h>
//#include <objbase.h>
#include <afxole.h>
#include <ShlObj.h>
#include <Shlwapi.h>
//#include <Uxtheme.h>
#include <Vsstyle.h>
#include <MsHTML.h>
#include <msxml6.h>
#include <WinInet.h>

#include "..\..\Tdi\src\api.h"
#include "..\..\PropGrid\src\api.h"
#include "..\..\YAHEL\Yahel\src\api.h"

#include "..\res\resource.h"
#include "..\..\Externals\fdrawcmd\fdrawcmd.h"

#include "OptionalLibAPIs.h"

#if _MFC_VER>=0x0A00
#else
	// reverting definitions for modern MFC to their states valid for Windows XP and earlier
	#define ERROR_UNRECOGNIZED_VOLUME        1005L
#endif

#undef min
#undef max
#undef INFINITY
#undef SubclassWindow

#if _MSC_VER<=1600
	#define constexpr const
#endif

typedef WORD TCylinder,*PCylinder; typedef short &RCylinder;
typedef const TCylinder *PCCylinder;
typedef BYTE THead,*PHead,TSide,*PSide;
typedef const THead *PCHead;
typedef const TSide *PCSide;

class CImage; // forward
typedef CImage *PImage;
typedef const CImage *PCImage;

class CDos; // forward
typedef CDos *PDos;
typedef const CDos *PCDos;

class CRideApp; // forward
class CBootView; // forward
class CTrackMapView; // forward
class CFileManagerView; // forward
class CFillEmptySpaceDialog; // forward

typedef const BYTE *PCBYTE;
typedef const WORD *PCWORD;
typedef const int *PCINT;

typedef int TLogValue,*PLogValue;
typedef const TLogValue *PCLogValue;
typedef Yahel::TInterval<TLogValue> TLogInterval,*PLogInterval;

constexpr TLogValue LogValueMax=INT_MAX;

typedef struct TLogPoint{
	TLogValue x,y;

	TLogValue ManhattanDistance(const TLogPoint &other) const;
} *PLogPoint;
typedef const TLogPoint *PCLogPoint;

#pragma warning( disable : 4228 ) // non-standard language extension
#pragma warning( disable : 4341 ) // pre-C++14 enums shouldn't be signed

#define ELLIPSIS	_T(". . .")

#define COLOR_WHITE	0xffffff
#define COLOR_BLACK	0
#define COLOR_YELLOW 0xffff
#define COLOR_RED	0xff
#define COLOR_BLUE	0xff0000

#define TXT_EXTENSION	_T(".txt")
#define TXT_FILTER		_T("Plain text (*") TXT_EXTENSION _T(")|*") TXT_EXTENSION _T("|")

#define STR_CANCEL		_T("Cancel")

namespace Bit
{
	typedef int N; // index or count, negative to indicate invalidity
	typedef WORD TPattern;
}
namespace Track
{
	typedef int N; // index or count
}

#include "Utils.h"
#include "BackgroundAction.h"
#include "Diff.h"
#include "Time.h"
#include "Checksum.h"
#include "Bit.h"
#include "MainWindow.h"
#include "HexaEditor.h"
#include "Revolution.h"
#include "Sector.h"
#include "Codec.h"
#include "Medium.h"
#include "TrackEvent.h"
#include "Track.h"
#include "ViewTrackMap.h"
#include "Image.h"
#include "DialogFormatting.h"
#include "DialogVerification.h"
#include "ImageFloppy.h"
#include "ImageRaw.h"
#include "Dsk5.h"
#include "FDD.h"
#include "Dos.h"
#include "Debug.h"
#include "SCL.h"
#include "ViewCriticalSector.h"
#include "ViewBoot.h"
#include "ViewFileManager.h"
#include "ViewWebPage.h"
#include "ViewDirectoryEntries.h"
#include "ViewDiskBrowser.h"
#include "SpectrumDos.h"
#include "DosUnknown.h"
#include "Editor.h"
#include "DialogNewImage.h"
#include "DialogUnformatting.h"
#include "DialogEmptySpaceFilling.h"
#include "DialogRealDeviceSelection.h"

#define DLL_SHELL32		_T("shell32")
#define DLL_WININET		_T("wininet")

#define APP_FULLNAME	_T("Real and Imaginary Disk Editor")
#define APP_ABBREVIATION "RIDE"
#define APP_VERSION		"1.7.12 debug special"
//#define APP_SPECIAL_VER
#define APP_IDENTIFIER	APP_ABBREVIATION APP_VERSION
#define APP_CLASSNAME	_T("Afx:tomascz.") _T(APP_ABBREVIATION)

#define FONT_MS_SANS_SERIF	_T("Microsoft Sans Serif")
#define FONT_SYMBOL			_T("Symbol")
#define FONT_VERDANA		_T("Verdana")
#define FONT_WEBDINGS		_T("Webdings")
#define FONT_WINGDINGS		_T("Wingdings")
#define FONT_COURIER_NEW	_T("Courier New")
#define FONT_LUCIDA_CONSOLE	_T("Lucida Console")

#define GITHUB_HTTPS_NAME		_T("https://github.com")
#define GITHUB_REPOSITORY		GITHUB_HTTPS_NAME _T("/tomas-nestorovic/RIDE")
#define GITHUB_API_SERVER_NAME	_T("api.github.com")
#define GITHUB_VERSION_TAG_NAME	"\"tag_name\""

extern CRideApp app;

extern void WINAPI AfxThrowInvalidArgException();

#ifdef RELEASE_MFC42
	#if _MSC_VER<=1600
		#define noexcept
	#endif

	void __cdecl operator delete(PVOID ptr, UINT sz) noexcept;
#endif

#endif STDAFX_H
