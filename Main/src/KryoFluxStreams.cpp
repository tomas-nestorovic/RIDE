#include "stdafx.h"
#include "CapsBase.h"
#include "KryoFluxBase.h"
#include "KryoFluxStreams.h"


	static LPCTSTR Recognize(PTCHAR){
		static const char SingleDeviceName[]=_T("KryoFlux Stream files\0");
		return SingleDeviceName;
	}
	static PImage Instantiate(LPCTSTR){
		return new CKryoFluxStreams;
	}
	const CImage::TProperties CKryoFluxStreams::Properties={
		MAKE_IMAGE_ID('C','A','P','S','_','K','F','S'), // a unique identifier
		Recognize,	// list of recognized device names
		Instantiate,	// instantiation function
		_T("*.0.raw") IMAGE_FORMAT_SEPARATOR _T("*.1.raw"), // filter
		TMedium::FLOPPY_ANY, // supported Media
		1,2*6144	// Sector supported min and max length
	};







	CKryoFluxStreams::CKryoFluxStreams()
		// ctor
		// - base
		: CKryoFluxBase( &Properties, Recognize(nullptr) ) {
		// - initialization
		*nameBase='\0';
	}






	#define TRACK_NAME_PATTERN	_T("%02d.%c.raw")

	BOOL CKryoFluxStreams::OnOpenDocument(LPCTSTR lpszPathName){
		// True <=> Image opened successfully, otherwise False
		// - base
		if (!__super::OnOpenDocument(lpszPathName)
			&&
			::GetLastError()!=ERROR_NOT_SUPPORTED // the CAPS library currently doesn't support reading Stream files
		)
			return FALSE;
		// - recognizing the naming pattern
		const LPCTSTR trackIdentifier=lpszPathName+::lstrlen(lpszPathName)-2-1-1-1-3; // see the TrackNamingPattern
		int cyl; char head;
		if (::sscanf( trackIdentifier, TRACK_NAME_PATTERN, &cyl, &head )!=2
			||
			cyl>=FDD_CYLINDERS_MAX
			||
			head<'0' || '1'<head
		){
			::SetLastError(ERROR_INVALID_NAME);
			return FALSE;
		}
		// - setting a classical 3.5" floppy geometry
		if (!capsImageInfo.maxcylinder) // # of Cylinders not yet set (e.g. via confirmed EditSettings dialog)
			capsImageInfo.maxcylinder=FDD_CYLINDERS_MAX-1; // inclusive!
		capsImageInfo.maxhead=2-1; // inclusive!
		// - successfully mounted
		::lstrcpyn( nameBase, lpszPathName, trackIdentifier-lpszPathName+1 );
		return TRUE;
	}

	BOOL CKryoFluxStreams::OnSaveDocument(LPCTSTR lpszPathName){
		// True <=> this Image has been successfully saved, otherwise False
		::SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	TSector CKryoFluxStreams::ScanTrack(TCylinder cyl,THead head,PSectorId bufferId,PWORD bufferLength,PINT startTimesNanoseconds,PBYTE pAvgGap3) const{
		// returns the number of Sectors found in given Track, and eventually populates the Buffer with their IDs (if Buffer!=Null); returns 0 if Track not formatted or not found
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return 0;
		// - if Track already scanned before, returning the result from before
		if (internalTracks[cyl][head]!=nullptr)
			return __super::ScanTrack( cyl, head, bufferId, bufferLength, startTimesNanoseconds, pAvgGap3 );
		// - loading the underlying file that contains the specified Track
		if (!*nameBase) // NameBase not set, e.g. when creating a new Image
			return 0;
		TCHAR filename[MAX_PATH];
		::wsprintf( filename, _T("%s") TRACK_NAME_PATTERN, nameBase, cyl<<(BYTE)params.doubleTrackStep, '0'+head );
		CFileException e;
		CFile f;
		if (!f.Open( filename, CFile::modeRead|CFile::shareDenyWrite|CFile::typeBinary, &e )){
			::SetLastError(e.m_cause);
			return 0;
		}
		// - making sure the loaded content is a KryoFlux Stream whose data actually make sense
		TSector nSectors=0;
		const auto fLength=f.GetLength();
		if (const PBYTE data=(PBYTE)::malloc(fLength)){
			if (f.Read( data, fLength )==fLength){
				const CKfStream kfStream( data, f.GetLength() );
				if (!kfStream.GetError()){
					// it's a KryoFlux Stream whose data make sense
					CTrackReaderWriter trw=kfStream.ToTrack();
						trw.SetMediumType(floppyType);
					internalTracks[cyl][head]=CInternalTrack::CreateFrom( *this, trw );
					nSectors=__super::ScanTrack( cyl, head, bufferId, bufferLength, startTimesNanoseconds, pAvgGap3 );
				}
			}
			::free(data);
		}
		return nSectors;
	}
