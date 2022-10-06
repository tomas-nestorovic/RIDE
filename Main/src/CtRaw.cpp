#include "stdafx.h"
#include "CapsBase.h"
#include "CtRaw.h"

	static LPCTSTR Recognize(PTCHAR){
		static constexpr TCHAR SingleDeviceName[]=_T("CT Raw\0");
		return SingleDeviceName;
	}
	static PImage Instantiate(LPCTSTR){
		return new CCtRaw;
	}
	const CImage::TProperties CCtRaw::Properties={
		MAKE_IMAGE_ID('C','A','P','S','_','R','A','W'), // a unique identifier
		Recognize,	// list of recognized device names
		Instantiate,	// instantiation function
		_T("*.raw"), // filter
		Medium::FLOPPY_ANY, // supported Media
		Codec::FLOPPY_ANY, // supported Codecs
		1,2*6144,	// Sector supported min and max length
		true		// is read-only; modifications not possible at the moment
	};







	#define INI_CTRAW	_T("CtRaw")

	CCtRaw::CCtRaw()
		// ctor
		// - base
		: CCapsBase( &Properties, '\0', true, INI_CTRAW ) { // '\0' = not a real drive
	}
