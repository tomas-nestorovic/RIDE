#ifndef STDAFX_H
#define STDAFX_H

#define _USE_MATH_DEFINES // for M_PI to be defined
#include <cmath>

// use Common-Controls library version 6
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define _WIN32_WINNT 0x0501

#define OEMRESOURCE
#include <afxwin.h>
#include <afxext.h>
#include <afxcview.h>
#include <afxhtml.h>
#include <afxmt.h>
//#include <afxrich.h>
//#include <afxcmn.h>
//#include <afxpriv.h>
//#include <afxtempl.h>
#ifdef RELEASE_MFC42
	#include <crtnew.h>
#endif
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
#include <WinInet.h>

#include "..\..\Tdi\src\api.h"
#include "..\..\PropGrid\src\api.h"

#include "..\res\resource.h"
#include "..\..\Externals\fdrawcmd\fdrawcmd.h"

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

#include "MainWindow.h"
#include "Editor.h"
#include "HexaEditor.h"
#include "Image.h"
#include "DialogFormatting.h"
#include "DialogUnformatting.h"
#include "ImageFloppy.h"
#include "ImageRaw.h"
#include "MGT.h"
#include "Dsk5.h"
#include "FDD.h"
#include "Utils.h"
#include "Dos.h"
#include "Debug.h"
#include "SCL.h"
#include "BackgroundAction.h"
#include "ViewBoot.h"
#include "ViewFileManager.h"
#include "ViewTrackMap.h"
#include "ViewWebPage.h"
#include "ViewDirectoryEntries.h"
#include "ViewDiskBrowser.h"
#include "SpectrumDos.h"
#include "DosUnknown.h"
#include "DialogNewImage.h"
#include "DialogEmptySpaceFilling.h"

#define DLL_UXTHEME		_T("UxTheme.dll")
#define DLL_SHELL32		_T("shell32.dll")
#define DLL_WININET		_T("wininet.dll")

#define APP_ABBREVIATION _T("RIDE")
#define APP_VERSION		_T("1.4.7 debug special")
#define APP_IDENTIFIER	APP_ABBREVIATION APP_VERSION

#define FONT_MS_SANS_SERIF	_T("Microsoft Sans Serif")
#define FONT_SYMBOL			_T("Symbol")
#define FONT_VERDANA		_T("Verdana")
#define FONT_WEBDINGS		_T("Webdings")
#define FONT_WINGDINGS		_T("Wingdings")
#define FONT_COURIER_NEW	_T("Courier New")
#define FONT_LUCIDA_CONSOLE	_T("Lucida Console")

#define GITHUB_API_SERVER_NAME	_T("api.github.com")
#define GITHUB_VERSION_TAG_NAME	"\"tag_name\""

#define ABS(x) ((x)>=0?(x):-(x))

#define ELLIPSIS	_T(". . .")

#define COLOR_WHITE	0xffffff
#define COLOR_BLACK	0
#define COLOR_YELLOW 0xffff

extern CRideApp app;

extern void WINAPI AfxThrowInvalidArgException();

#endif STDAFX_H
