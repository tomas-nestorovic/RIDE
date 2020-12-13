#include "stdafx.h"
#include "CapsBase.h"
#include "CtRaw.h"

	static LPCTSTR Recognize(PTCHAR){
		static const char SingleDeviceName[]=_T("CT Raw\0");
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
		TMedium::FLOPPY_ANY, // supported Media
		1,2*6144	// Sector supported min and max length
	};







	#define INI_CTRAW	_T("CtRaw")

	CCtRaw::CCtRaw()
		// ctor
		// - base
		: CCapsBase(&Properties,true) {
		canBeModified=false; // modifications not possible at the moment
	}






	BOOL CCtRaw::OnSaveDocument(LPCTSTR lpszPathName){
		// True <=> this Image has been successfully saved, otherwise False
		::SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	TStdWinError CCtRaw::MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus){
		// marks Sector on a given PhysicalAddress as "dirty", plus sets it the given FdcStatus; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}

	TStdWinError CCtRaw::FormatTrack(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte){
		// formats given Track {Cylinder,Head} to the requested NumberOfSectors, each with corresponding Length and FillerByte as initial content; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}

	TStdWinError CCtRaw::UnformatTrack(TCylinder cyl,THead head){
		// unformats given Track {Cylinder,Head}; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}