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
		_T("*.") CAPS_FILEEXT, // filter
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
			Utils::Warning(_T("This IPF file is a draft!"));
		// - successfully mounted
		return TRUE;
	}

	TStdWinError CIpf::MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus){
		// marks Sector on a given PhysicalAddress as "dirty", plus sets it the given FdcStatus; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}

	TStdWinError CIpf::WriteTrack(TCylinder cyl,THead head,CTrackReader tr){
		// converts general description of the specified Track into Image-specific representation; caller may provide Invalid TrackReader to check support of this feature; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}

	TStdWinError CIpf::FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte,const volatile bool &cancelled){
		// formats given Track {Cylinder,Head} to the requested NumberOfSectors, each with corresponding Length and FillerByte as initial content; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}

	TStdWinError CIpf::UnformatTrack(TCylinder cyl,THead head){
		// unformats given Track {Cylinder,Head}; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}
