#include "stdafx.h"
#include "CapsBase.h"
#include "IPF.h"

	static LPCTSTR Recognize(PTCHAR){
		static constexpr TCHAR SingleDeviceName[]=_T("Interchangeable Preservation Format\0");
		return SingleDeviceName;
	}
	static PImage Instantiate(LPCTSTR){
		return new CIpf;
	}
	const CImage::TProperties CIpf::Properties={
		MAKE_IMAGE_ID('C','A','P','S','_','I','P','F'), // a unique identifier
		Recognize,	// list of recognized device names
		Instantiate,	// instantiation function
		_T("*.") _T(CAPS_FILEEXT), // filter
		Medium::FLOPPY_ANY, // supported Media
		Codec::FLOPPY_ANY, // supported Codecs
		1,2*6144,	// Sector supported min and max length
		true		// is read-only; by design, the IPF serves for PRESERVATION and modifications thus should be NOT allowed
	};







	#define INI_IPF	_T("Ipf")

	CIpf::CIpf()
		// ctor
		// - base
		: CCapsBase( &Properties, '\0', true, INI_IPF ) { // '\0' = not a real drive
	}






	BOOL CIpf::OnOpenDocument(LPCTSTR lpszPathName){
		// True <=> Image opened successfully, otherwise False
		// - base
		if (!__super::OnOpenDocument(lpszPathName))
			return FALSE;
		// - warn about a draft version
		if (!capsImageInfo.release || !capsImageInfo.revision)
			draftVersionMessageBar.Show();
		// - successfully mounted
		return TRUE;
	}
