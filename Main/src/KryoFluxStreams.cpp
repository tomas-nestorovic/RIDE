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
		Medium::FLOPPY_ANY, // supported Media
		Codec::FLOPPY_ANY, // supported Codecs
		1,2*6144	// Sector supported min and max length
	};







	CKryoFluxStreams::CKryoFluxStreams()
		// ctor
		// - base
		: CKryoFluxBase( &Properties, '\0', Recognize(nullptr) ) { // '\0' = not a real drive
		// - initialization
		*nameBase='\0';
	}






	#define TRACK_NAME_PATTERN	_T("%02d.%c.raw")

	bool CKryoFluxStreams::SetNameBase(LPCTSTR fullName){
		// True <=> a valid naming pattern has been recognized, otherwise False
		const LPCTSTR trackIdentifier=fullName+::lstrlen(fullName)-2-1-1-1-3; // see the TrackNamingPattern
		int cyl; char head;
		if (_stscanf( trackIdentifier, TRACK_NAME_PATTERN, &cyl, &head )!=2
			||
			cyl>=FDD_CYLINDERS_MAX
			||
			head<'0' || '1'<head
		){
			::SetLastError(ERROR_INVALID_NAME);
			return false;
		}
		::lstrcpyn( nameBase, fullName, trackIdentifier-fullName+1 );
		return true;
	}

	BOOL CKryoFluxStreams::OnOpenDocument(LPCTSTR lpszPathName){
		// True <=> Image opened successfully, otherwise False
		// - base
		if (!__super::OnOpenDocument(lpszPathName)
			&&
			::GetLastError()!=ERROR_NOT_SUPPORTED // the CAPS library currently doesn't support reading Stream files
		)
			return FALSE;
		// - recognizing the name pattern
		if (!SetNameBase(lpszPathName))
			return FALSE;
		// - setting a classical 3.5" floppy geometry
		if (!capsImageInfo.maxcylinder) // # of Cylinders not yet set (e.g. via confirmed EditSettings dialog)
			capsImageInfo.maxcylinder=FDD_CYLINDERS_MAX-1; // inclusive!
		capsImageInfo.maxhead=2-1; // inclusive!
		// - confirming initial settings
		if (!EditSettings(true)){ // dialog cancelled?
			::SetLastError( ERROR_CANCELLED );
			return FALSE;
		}
		// - successfully mounted
		return TRUE;
	}

	TStdWinError CKryoFluxStreams::SaveAllModifiedTracks(LPCTSTR lpszPathName,PBackgroundActionCancelable pAction){
		// saves all Modified Tracks; returns Windows standard i/o error
		// - recognizing the name pattern
		if (!SetNameBase(lpszPathName))
			return ERROR_FUNCTION_FAILED;
		// - saving
		return __super::SaveAllModifiedTracks( lpszPathName, pAction );
	}

	TStdWinError CKryoFluxStreams::SaveTrack(TCylinder cyl,THead head) const{
		// saves the specified Track to the inserted Medium; returns Windows standard i/o error
		if (const auto pit=internalTracks[cyl][head])
			if (pit->modified){
				pit->FlushSectorBuffers(); // convert all modifications into flux transitions
				TCHAR fileName[MAX_PATH];
				::wsprintf( fileName, _T("%s") TRACK_NAME_PATTERN, nameBase, cyl, '0'+head );
				CFile f; CFileException e;
				if (!f.Open( fileName, CFile::modeCreate|CFile::modeWrite|CFile::typeBinary|CFile::shareExclusive, &e ))
					return e.m_cause;
				if (const auto data=Utils::MakeCallocPtr<BYTE>(KF_BUFFER_CAPACITY)){
					f.Write( data, TrackToStream(*pit,data) );
					pit->modified=false;
				}else
					return ERROR_NOT_ENOUGH_MEMORY;
			}
		return ERROR_SUCCESS;
	}

	TStdWinError CKryoFluxStreams::UploadFirmware(){
		// uploads firmware to a KryoFlux-based device; returns Windows standard i/o error
		return ERROR_SUCCESS; // Stream files don't require firmware
	}

	TSector CKryoFluxStreams::ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec,PSectorId bufferId,PWORD bufferLength,PLogTime startTimesNanoseconds,PBYTE pAvgGap3) const{
		// returns the number of Sectors found in given Track, and eventually populates the Buffer with their IDs (if Buffer!=Null); returns 0 if Track not formatted or not found
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return 0;
		// - if Track already scanned before, returning the result from before
		if (internalTracks[cyl][head]!=nullptr)
			return __super::ScanTrack( cyl, head, pCodec, bufferId, bufferLength, startTimesNanoseconds, pAvgGap3 );
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
		if (const auto data=Utils::MakeCallocPtr<BYTE>(fLength))
			if (f.Read( data, fLength )==fLength)
				if (CTrackReaderWriter trw=StreamToTrack( data, f.GetLength() )){
					// it's a KryoFlux Stream whose data make sense
					if (floppyType!=Medium::UNKNOWN){ // may be unknown if Medium is still being recognized
						trw.SetMediumType(floppyType);
						if (dos!=nullptr) // DOS already known
							params.corrections.ApplyTo(trw);
						//the following commented out as it brings little to no readability improvement and leaves Tracks influenced by the MediumType
						//else if (params.corrections.indexTiming) // DOS still being recognized ...
							//trw.Normalize(); // ... hence can only improve readability by adjusting index-to-index timing
					}
					internalTracks[cyl][head]=CInternalTrack::CreateFrom( *this, trw );
					nSectors=__super::ScanTrack( cyl, head, pCodec, bufferId, bufferLength, startTimesNanoseconds, pAvgGap3 );
				}
		return nSectors;
	}

	void CKryoFluxStreams::SetPathName(LPCTSTR lpszPathName,BOOL bAddToMRU){
		__super::SetPathName( lpszPathName, bAddToMRU );
		SetNameBase(lpszPathName);
	}
