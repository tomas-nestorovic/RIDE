#include "stdafx.h"
#include "CapsBase.h"
#include "KryoFluxBase.h"
#include "KryoFluxStreams.h"


	static LPCTSTR Recognize(PTCHAR){
		static constexpr TCHAR SingleDeviceName[]=_T("KryoFlux Stream files\0");
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
	}






	#define TRACK_NAME_PATTERN_EXT	_T(".%c.raw")
	#define TRACK_NAME_PATTERN		_T("%02d") TRACK_NAME_PATTERN_EXT

	CString CKryoFluxStreams::GetStreamFileName(LPCTSTR nameBase,TCylinder cyl,THead head) const{
		CString result;
		result.Format( _T("%s") TRACK_NAME_PATTERN, nameBase, cyl<<(BYTE)params.doubleTrackStep, '0'+head );
		return result;
	};

	bool CKryoFluxStreams::SetNameBase(LPCTSTR fullName){
		// True <=> a valid naming pattern has been recognized, otherwise False
		LPCTSTR trackIdentifier=fullName+::lstrlen(fullName)-2-1-1-1-3; // see the TrackNamingPattern
		int cyl; char head;
		if (trackIdentifier>fullName // sufficiently long name?
			&&
			(	_stscanf( trackIdentifier, TRACK_NAME_PATTERN, &cyl, &head )==2  &&  cyl<FDD_CYLINDERS_MAX // is the Track name fully ...
				||
				_stscanf( trackIdentifier+=2, TRACK_NAME_PATTERN_EXT, &head )>0 // ... or at least partially specified?
			)
			&&
			'0'<=head && head<='1'
		){
			nameBase=CString( fullName, trackIdentifier-fullName );
			return true;
		}
		::SetLastError(ERROR_INVALID_NAME);
		return false;
	}

	BOOL CKryoFluxStreams::OnOpenDocument(LPCTSTR lpszPathName){
		// True <=> Image opened successfully, otherwise False
		// - base
		if (!__super::OnOpenDocument(nullptr) // don't involve CAPS in Image opening
			&&
			::GetLastError()!=ERROR_NOT_SUPPORTED // the CAPS library currently doesn't support reading Stream files
		)
			return FALSE;
		// - initial Stream file must exist (but other Tracks don't)
		CFileException e;
		CFile f;
		if (!f.Open( lpszPathName, CFile::modeRead|CFile::shareDenyWrite|CFile::typeBinary, &e )){
			::SetLastError( e.m_cause );
			return FALSE;
		}
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

	TStdWinError CKryoFluxStreams::SaveAllModifiedTracks(LPCTSTR lpszPathName,CActionProgress &ap){
		// saves all Modified Tracks; returns Windows standard i/o error
		// - recognizing the name pattern
		const CString nameBaseOrg=nameBase;
		if (!SetNameBase(lpszPathName))
			return ERROR_FUNCTION_FAILED;
		// - saving
		for( TCylinder cyl=0; cyl<GetCylinderCount(); ap.UpdateProgress(++cyl) )
			for( THead head=0; head<GetHeadCount(); head++ )
				if (ap.Cancelled)
					return ERROR_CANCELLED;
				else{
					if (const PCInternalTrack pit=GetModifiedTrackSafe(cyl,head)) // Track modified?
							if (const TStdWinError err=SaveTrack( cyl, head, ap.Cancelled ))
								return err;
							else
								continue;
					if (nameBaseOrg!=nameBase) // saving to a different location?
						::CopyFile( GetStreamFileName(nameBaseOrg,cyl,head), GetStreamFileName(cyl,head), FALSE );
				}
		return ERROR_SUCCESS;
	}

	TStdWinError CKryoFluxStreams::SaveTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const{
		// saves the specified Track to the inserted Medium; returns Windows standard i/o error
		if (!*nameBase) // saving without knowing the common prefix for all Stream files is NOT SUPPORTED
			return ERROR_NOT_SUPPORTED; // this error code is required!
		if (const auto pit=GetModifiedTrackSafe(cyl,head)){ // Track modified?
			pit->FlushSectorBuffers(); // convert all modifications into flux transitions
			CFile f; CFileException e;
			if (!f.Open( GetStreamFileName(cyl,head), CFile::modeCreate|CFile::modeWrite|CFile::typeBinary|CFile::shareExclusive, &e ))
				return e.m_cause;
			Utils::CCallocPtr<BYTE> streamData;
			DWORD dataLength;
			PCBYTE data=pit->GetRawDeviceData( KF_STREAM_ID, dataLength );
			if (!data){ // Track has been really modified and original KF Stream disposed ...
				std::swap( streamData, Utils::MakeCallocPtr<BYTE>(KF_BUFFER_CAPACITY) );
				data = streamData;
				if (!streamData)
					return ERROR_NOT_ENOUGH_MEMORY;
				dataLength=TrackToStream( // ... must reconstruct it from current state of the Track
					head && params.flippyDisk
						? CTrackReaderWriter(*pit,false).Reverse()
						: *pit,
					streamData
				);
			}
			if (GetCurrentDiskFreeSpace()<dataLength)
				return ERROR_DISK_FULL;
			f.Write( data, dataLength );
			pit->modified=false;
		}
		return ERROR_SUCCESS;
	}

	CImage::CTrackReader CKryoFluxStreams::ReadTrack(TCylinder cyl,THead head) const{
		// creates and returns a general description of the specified Track, represented using neutral LogicalTimes
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - if Track already read before, returning the result from before
		if (const auto tr=ReadExistingTrack(cyl,head))
			return tr;
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return CTrackReaderWriter::Invalid;
		// - loading the underlying file that contains the specified Track
		if (!*nameBase) // NameBase not set, e.g. when creating a new Image
			return CTrackReaderWriter::Invalid;
		CFileException e;
		CFile f;
		if (!f.Open( GetStreamFileName(cyl,head), CFile::modeRead|CFile::shareDenyWrite|CFile::typeBinary, &e )){
			::SetLastError(e.m_cause);
			return CTrackReaderWriter::Invalid;
		}
		// - making sure the loaded content is a KryoFlux Stream whose data actually make sense
		PInternalTrack &rit=internalTracks[cyl][head];
		const auto fLength=f.GetLength();
		if (const auto data=Utils::MakeCallocPtr<BYTE>(fLength))
			if (f.Read( data, fLength )==fLength)
				if (CTrackReaderWriter trw=StreamToTrack( data, f.GetLength() )){
					// it's a KryoFlux Stream whose data make sense
					if (head && params.flippyDisk)
						trw.Reverse();
					rit=CInternalTrack::CreateFrom( *this, trw );
					return *rit;
				}
		return CTrackReaderWriter::Invalid;
	}

	void CKryoFluxStreams::SetPathName(LPCTSTR lpszPathName,BOOL bAddToMRU){
		__super::SetPathName( lpszPathName, bAddToMRU );
		SetNameBase(lpszPathName);
	}
