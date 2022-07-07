#include "stdafx.h"

	struct TFdIdHeader sealed:public FD_ID_HEADER{
		TFdIdHeader(const TSectorId &rid){
			cyl=rid.cylinder, head=rid.side, sector=rid.sector, size=rid.lengthCode;
		}
	};

	
	#define INI_FDD	_T("FDD")

	#define INI_LATENCY_DETERMINED		_T("latdet")
	#define INI_LATENCY_CONTROLLER		_T("latfdc")
	#define INI_LATENCY_1BYTE			_T("lat1b")
	#define INI_LATENCY_GAP3			_T("latg3")
	#define INI_CALIBRATE_SECTOR_ERROR	_T("clberr")
	#define INI_CALIBRATE_SECTOR_ERROR_KNOWN _T("clbknw")
	#define INI_CALIBRATE_FORMATTING	_T("clbfmt")
	#define INI_MOTOR_OFF_SECONDS		_T("mtroff")
	#define INI_TRACK_DOUBLE_STEP		_T("dbltrk")
	#define INI_VERIFY_FORMATTING		_T("vrftr")
	#define INI_VERIFY_WRITTEN_DATA		_T("vrfdt")
	#define INI_SEEKING					_T("seek")

	#define INI_MSG_RESET		_T("msgrst")
	#define INI_MSG_LATENCY		_T("msglat")

	static void __informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_FDD, messageId );
	}

	CFDD::TParams::TParams()
		// ctor
		: calibrationAfterError( (TCalibrationAfterError)app.GetProfileInt(INI_FDD,INI_CALIBRATE_SECTOR_ERROR,TCalibrationAfterError::ONCE_PER_CYLINDER) )
		, calibrationAfterErrorOnlyForKnownSectors( app.GetProfileInt(INI_FDD,INI_CALIBRATE_SECTOR_ERROR_KNOWN,0)!=0 )
		, calibrationStepDuringFormatting( app.GetProfileInt(INI_FDD,INI_CALIBRATE_FORMATTING,0) )
		, verifyFormattedTracks( app.GetProfileInt(INI_FDD,INI_VERIFY_FORMATTING,true)!=0 )
		, verifyWrittenData( app.GetProfileInt(INI_FDD,INI_VERIFY_WRITTEN_DATA,false)!=0 )
		, nSecondsToTurnMotorOff( app.GetProfileInt(INI_FDD,INI_MOTOR_OFF_SECONDS,2) ) { // 0 = 1 second, 1 = 2 seconds, 2 = 3 seconds
	}

	CFDD::TParams::~TParams(){
		// dtor
		app.WriteProfileInt( INI_FDD, INI_CALIBRATE_SECTOR_ERROR,calibrationAfterError );
		app.WriteProfileInt( INI_FDD, INI_CALIBRATE_SECTOR_ERROR_KNOWN, calibrationAfterErrorOnlyForKnownSectors );
		app.WriteProfileInt( INI_FDD, INI_CALIBRATE_FORMATTING,calibrationStepDuringFormatting );
		app.WriteProfileInt( INI_FDD, INI_MOTOR_OFF_SECONDS,nSecondsToTurnMotorOff );
		app.WriteProfileInt( INI_FDD, INI_VERIFY_FORMATTING,verifyFormattedTracks );
		app.WriteProfileInt( INI_FDD, INI_VERIFY_WRITTEN_DATA,verifyWrittenData );
	}







	#define _HANDLE		fddHead.handle
	#define DRIVER		fddHead.driver

	TStdWinError CFDD::TInternalTrack::TSectorInfo::SaveToDisk(CFDD *fdd,const TInternalTrack *pit,BYTE nSectorsToSkip,bool verify,const volatile bool &cancelled){
		// saves this Sector to inserted floppy; returns Windows standard i/o error
		LOG_SECTOR_ACTION(&id,_T("TStdWinError CFDD::TInternalTrack::TSectorInfo::__saveToDisk__"));
		// - seeking the Head
		if (!fdd->fddHead.__seekTo__(pit->cylinder))
			return LOG_ERROR(::GetLastError());
		// - spreading Dirty Data to all buffered Revolutions
		if (IsModified())
			for( BYTE r=0; r<nRevolutions; r++ )
				if (const PSectorData d=revolutions[r].data)
					::memcpy( d, revolutions[dirtyRevolution].data, length );
		// - saving
		const auto &rev=revolutions[currentRevolution]; // see spreading of Dirty Data above
		char nSilentRetrials=1;
		do{
			if (cancelled)
				return LOG_ERROR(ERROR_CANCELLED);
			// : taking into account the NumberOfSectorsToSkip in current Track
			if (pit->__isIdDuplicated__(&id)) // to speed matters up, only if ID is duplicated in Track
				fdd->__setNumberOfSectorsToSkipOnCurrentTrack__(nSectorsToSkip);
			// : saving
			TStdWinError err;
			switch (fdd->DRIVER){
				case DRV_FDRAWCMD:{
					// . preparing for reproduction of requested i/o errors
					DWORD fdcCommand=IOCTL_FDCMD_WRITE_DATA,nBytesTransferred;
					if (rev.fdcStatus.DescribesDeletedDam())
						fdcCommand=IOCTL_FDCMD_WRITE_DELETED_DATA;
					if (rev.fdcStatus.reg2 & FDC_ST2_CRC_ERROR_IN_DATA)
						if ( err=fdd->__setTimeBeforeInterruptingTheFdc__(length) )
							return err;
					if (id.lengthCode>fdd->GetMaximumSectorLengthCode())
						fdd->__setWaitingForIndex__();
					// . writing Sector
					FD_READ_WRITE_PARAMS rwp={ FD_OPTION_MFM, pit->head, id.cylinder,id.side,id.sector,id.lengthCode, id.sector+1, 1, 0xff };
					LOG_ACTION(_T("DeviceIoControl fdcCommand"));
					err=::DeviceIoControl( fdd->_HANDLE, fdcCommand, &rwp,sizeof(rwp), ::memcpy(fdd->dataBuffer,rev.data,length),GetOfficialSectorLength(id.lengthCode), &nBytesTransferred, nullptr )!=0
						? ERROR_SUCCESS
						: LOG_ERROR(::GetLastError());
					// . cleaning up after reproduction of requested i/o errors
					//nop
					break;
				}
				default:
					ASSERT(FALSE);
					return LOG_ERROR(ERROR_DEVICE_NOT_AVAILABLE);
			}
			if (err!=ERROR_SUCCESS)
				switch (nSilentRetrials-->0 // for positive NumberOfSilentRetrials ...
						? IDRETRY // ... simply silently retrying to write the Sector
						: Utils::AbortRetryIgnore(err,MB_DEFBUTTON2) // ... otherwise asking the user what to do
				){
					case IDIGNORE:
						// ignoring the Error
						break;
					case IDABORT:
						// aborting the saving
						return err;
					default:{
						// re-attempting to save the Sector
						LOG_ACTION(_T("re-attempting to save the Sector"));
						// . checking whether (paradoxically) the Track is healthy
						const TCylinder cyl=pit->cylinder;
						const THead head=pit->head;
						if (!fdd->IsTrackHealthy(cyl,head)) // if the Track contains damaged Sectors ...
							continue; // ... trying to write the Sector once more while leaving the Track as it is, including damaged Sectors
						// . checking if this HEALTHY Sector has been found on the Track
						if (err!=ERROR_SECTOR_NOT_FOUND) // if something else but Sector-not-found happened (e.g. disk is write-protected) ...
							continue; // ... trying to write the Sector once more while leaving the Track as it is
						// . reading all Sectors
						//nop (already done above when checking if the Track is healthy)
						// To recap: A healthy Sector has been read, yet it cannot be written back - WE END UP HERE ONLY WHEN DUMPING AN IMAGE TO A FLOPPY WITH "Reformat just bad tracks" TICKED
						// . reformatting the Track
						TSectorId bufferId[(BYTE)-1]; WORD bufferLength[(BYTE)-1]; TFdcStatus bufferStatus[(BYTE)-1];
						for( TSector n=0; n<pit->nSectors; n++ )
							bufferId[n]=pit->sectors[n].id, bufferLength[n]=pit->sectors[n].length, bufferStatus[n]=TFdcStatus::WithoutError;
						fdd->internalTracks[cyl][head]=nullptr; // detaching the Track internal representation for it to be not destroyed during reformatting of the Track
						err=fdd->FormatTrack(
							cyl, head, pit->codec, pit->nSectors, bufferId, bufferLength, bufferStatus,
							fdd->floppyType==Medium::FLOPPY_DD_525 ? FDD_525_SECTOR_GAP3 : FDD_350_SECTOR_GAP3, // if gap too small (e.g. 10) it's highly likely that sectors would be missed in a single disk revolution (so for instance, reading 9 sectors would require 9 disk revolutions)
							fdd->dos->properties->sectorFillerByte,
							cancelled
						);
						if (err!=ERROR_SUCCESS){ // if formatting failed ...
terminateWithError:			fdd->UnformatInternalTrack(cyl,head); // disposing any new InternalTrack representation
							fdd->internalTracks[cyl][head]=(PInternalTrack)pit; // re-attaching the original InternalTrack representation
							return LOG_ERROR(err); // ... there's nothing else to do but terminating with Error
						}
						// . if this is the K-th Sector, making sure that the 0..(K-1)-th Sectors have been formatted well
						for( TSector s=0; s<nSectorsToSkip; s++ ){
							TFdcStatus sr;
							if (!fdd->__bufferSectorData__(cyl,head,&pit->sectors[s].id,pit->sectors[s].length,pit,s,&sr) || !sr.IsWithoutError()){ // if Sector not readable even after reformatting the Track ...
								err=::GetLastError();
								goto terminateWithError; // ... there's nothing else to do but terminating with Error
							}
						}
						// . disposing the new InternalTrack representations (as it's been worked only with the original one), restoring the original
						fdd->UnformatInternalTrack(cyl,head); // disposing any new InternalTrack representation
						fdd->internalTracks[cyl][head]=(PInternalTrack)pit; // re-attaching the original InternalTrack representation
						// . writing the 0..(K-1)-th Sectors back to the above reformatted Track, leaving K+1..N-th Sectors unwritten (caller's duty); we re-attempt to write this K-th Sector in the next iteration
						for( TSector s=0; s<nSectorsToSkip; s++ )
							if ( err=pit->sectors[s].SaveToDisk(fdd,pit,s,verify,cancelled) ) // if Sector not writeable even after reformatting the Track ...
								return LOG_ERROR(err); // ... there's nothing else to do but terminating with Error
						// . trying to write this K-th Sector once more, leaving K+1..N-th Sectors unwritten (caller's duty)
						continue;
					}
				}
			// : verifying the writing
			else
				if (verify && !cancelled)
					switch (VerifySaving(fdd,pit,nSectorsToSkip)){
						case IDIGNORE:	break;
						case IDABORT:	return LOG_ERROR(ERROR_CANCELLED);
						case IDRETRY:	continue;
					}
			break;
		}while (true);
		return ERROR_SUCCESS;
	}

	BYTE CFDD::TInternalTrack::TSectorInfo::VerifySaving(const CFDD *fdd,const TInternalTrack *pit,BYTE nSectorsToSkip){
		// verifies the saving made by during a call to SaveToDisk
		LOG_SECTOR_ACTION(&id,_T("verifying the writing"));
		const auto &rev=revolutions[ IsModified() ? dirtyRevolution : currentRevolution ];
		const TPhysicalAddress chs={ pit->cylinder, pit->head, id };
		TFdcStatus sr;
		fdd->__bufferSectorData__( chs, length, pit, nSectorsToSkip, &sr );
		if (rev.fdcStatus.DescribesDataFieldCrcError()^sr.DescribesDataFieldCrcError() // Data written with/without error than desired
			||
			::memcmp(rev.data,fdd->dataBuffer,length) // Data written with error
		){
			TCHAR buf[80];
			::wsprintf( buf, _T("Verification failed for sector with %s on Track %d."), (LPCTSTR)id.ToString(), chs.GetTrackNumber(2) );
			const BYTE result=Utils::AbortRetryIgnore( buf, MB_DEFBUTTON2 );
			if (result==IDIGNORE)
				dirtyRevolution=Revolution::NONE; // saved successfully if commanded to ignore any errors, otherwise the Sector remains marked Modified
			return result;
		}else{
			dirtyRevolution=Revolution::NONE; // saved successfully, so the Sector has no longer any DirtyRevolution
			return IDIGNORE; // do nothing after return
		}
	}

	#define ALLOCATE_SECTOR_DATA(length)	(PSectorData)::malloc(length)
	#define FREE_SECTOR_DATA(data)			::free(data)

	#define SECTOR_LENGTH_MAX	16384

	CFDD::TInternalTrack::TInternalTrack(const CFDD *fdd,TCylinder cyl,THead head,Codec::TType codec,TSector _nSectors,PCSectorId bufferId,PCLogTime sectorStartsNanoseconds)
		// ctor
		// - initialization
		: cylinder(cyl) , head(head) , codec(codec)
		, nSectors(_nSectors) , sectors( nSectors, 0 ) {
		TInternalTrack::TSectorInfo *psi=sectors;
		for( TSector s=0; s<nSectors; psi++->seqNum=s++ ){
			psi->length=fdd->GetUsableSectorLength(( psi->id=*bufferId++ ).lengthCode );
			for( BYTE r=0; r<Revolution::MAX; r++ )
				psi->revolutions[r].fdcStatus=TFdcStatus::Unknown; // not yet attempted for reading
			psi->dirtyRevolution=Revolution::NONE;
			if (sectorStartsNanoseconds>(PCLogTime)0x100) // if start times provided (that is, if no Gap3 information from <0;255> Bytes provided) ...
				psi->startNanoseconds=*sectorStartsNanoseconds++; // ... they are used
			else // if no start times provided (that is, if just Gap3 information from <0;255> Bytes provided) ...
				if (s) // ... then simply inferring them
					psi->startNanoseconds=	sectors[s-1].endNanoseconds
											+
											(BYTE)sectorStartsNanoseconds * fdd->fddHead.profile.oneByteLatency;	// default inter-sector Gap3 length in nanoseconds
				else
					psi->startNanoseconds=0; // the first Sector starts immediately after the index pulse
			psi->endNanoseconds=	psi->startNanoseconds // inferring end of Sector from its lengths and general IBM track layout specification
									+
									(	12	// N Bytes 0x00 (see IBM's track layout specification)
										+
										3	// 0xA1A1A1 mark with corrupted clock
										+
										1	// Sector ID Address Mark
										+
										4	// Sector ID
										+
										sizeof(TCrc16) // Sector ID CRC
										+
										22	// Gap2: N Bytes 0x4E
										+
										12	// Gap2: N Bytes 0x00
										+
										3	// Gap2: 0xA1A1A1 mark with corrupted clock
										+
										1	// Data Address Mark
										+
										psi->length// data
										+
										sizeof(TCrc16) // data CRC
									) *
									fdd->fddHead.profile.oneByteLatency; // usually 32 microseconds
		}
	}

	CFDD::TInternalTrack::~TInternalTrack(){
		// dtor
		const TSectorInfo *psi=sectors;
		for( TSector n=nSectors; n--; psi++ )
			for( BYTE r=0; r<psi->nRevolutions; r++ )
				if (const PVOID data=psi->revolutions[r].data)
					FREE_SECTOR_DATA(data);
	}

	bool CFDD::TInternalTrack::__isIdDuplicated__(PCSectorId pid) const{
		// True <=> at least two Sectors on the Track have the same ID, otherwise False
		BYTE nAppearances=0;
		const TSectorInfo *psi=sectors;
		for( BYTE n=nSectors; n--; nAppearances+=*pid==psi++->id );
		return nAppearances>1;
	}










	CFDD::TFddHead::TFddHead()
		// ctor
		: handle(INVALID_HANDLE_VALUE) // see Reset
		, calibrated(false) , position(0)
		, doubleTrackStep( app.GetProfileInt(INI_FDD,INI_TRACK_DOUBLE_STEP,false)!=0 )
		, userForcedDoubleTrackStep(false) // True once the ID_40D80 button in Settings dialog is pressed
		, preferRelativeSeeking( app.GetProfileInt(INI_FDD,INI_SEEKING,0)!=0 ) {
	}

	CFDD::TFddHead::~TFddHead(){
		// dtor
		app.WriteProfileInt( INI_FDD, INI_TRACK_DOUBLE_STEP, doubleTrackStep );
		app.WriteProfileInt( INI_FDD, INI_SEEKING, preferRelativeSeeking );
	}

	CFDD::TFddHead::TProfile::TProfile()
		// ctor (the defaults for a 2DD floppy)
		: controllerLatency(TIME_MICRO(86))
		, oneByteLatency(FDD_NANOSECONDS_PER_DD_BYTE)
		, gap3Latency( oneByteLatency*FDD_350_SECTOR_GAP3 * 4/5 ) { // "4/5" = giving the FDC 20% tolerance for Gap3
	}

	static void GetFddProfileName(PTCHAR buf,TCHAR driveLetter,Medium::TType floppyType){
		::wsprintf( buf, INI_FDD _T("_%c_%d"), driveLetter, floppyType );
	}

	bool CFDD::TFddHead::TProfile::Load(TCHAR driveLetter,Medium::TType floppyType,TLogTime defaultNanosecondsPerByte){
		// True <=> explicit Profile for specified Drive/FloppyType exists and loaded, otherwise False
		TCHAR iniSection[16];
		GetFddProfileName( iniSection, driveLetter, floppyType );
		switch (floppyType){
			default:
				ASSERT(FALSE);
				//fallthrough
			case Medium::FLOPPY_DD:
				controllerLatency=app.GetProfileInt( iniSection, INI_LATENCY_CONTROLLER, TIME_MICRO(86) );
				break;
			case Medium::FLOPPY_DD_525:
				controllerLatency=app.GetProfileInt( iniSection, INI_LATENCY_CONTROLLER, TIME_MICRO(86)*5/6 );
				break;
			case Medium::FLOPPY_HD_525:
			case Medium::FLOPPY_HD_350:
				controllerLatency=app.GetProfileInt( iniSection, INI_LATENCY_CONTROLLER, TIME_MICRO(86)/2 );
				break;
		}
		oneByteLatency=app.GetProfileInt( iniSection, INI_LATENCY_1BYTE, defaultNanosecondsPerByte );
		gap3Latency=app.GetProfileInt( iniSection, INI_LATENCY_GAP3, oneByteLatency*FDD_350_SECTOR_GAP3*4/5 ); // "4/5" = giving the FDC 20% tolerance for Gap3
		return app.GetProfileInt( iniSection, INI_LATENCY_CONTROLLER, 0 )>0; // True <=> previously determined values used, otherwise False
	}

	void CFDD::TFddHead::TProfile::Save(TCHAR driveLetter,Medium::TType floppyType) const{
		// saves the Profile for specified Drive and FloppyType
		TCHAR iniSection[16];
		GetFddProfileName( iniSection, driveLetter, floppyType );
		app.WriteProfileInt( iniSection, INI_LATENCY_CONTROLLER, controllerLatency );
		app.WriteProfileInt( iniSection, INI_LATENCY_1BYTE, oneByteLatency );
		app.WriteProfileInt( iniSection, INI_LATENCY_GAP3, gap3Latency );
	}

	bool CFDD::TFddHead::SeekHome(){
		// True <=> Head seeked "home", otherwise False and reporting the error
		return __seekTo__(0);
	}

	bool CFDD::TFddHead::__seekTo__(TCylinder cyl){
		// True <=> Head seeked to specified Cylinder, otherwise False and reporting the error
		if (cyl!=position){
			LOG_CYLINDER_ACTION(cyl,_T("bool CFDD::TFddHead::__seekTo__"));
			calibrated=false; // the Head is no longer Calibrated after changing to a different Cylinder
			do{
				DWORD nBytesTransferred;
				bool seeked;
				switch (driver){
					case DRV_FDRAWCMD:
						if (preferRelativeSeeking && cyl>position){ // RelativeSeeking is allowed only to higher Cylinder numbers
							FD_RELATIVE_SEEK_PARAMS rsp={ FD_OPTION_MT|FD_OPTION_DIR, 0, (cyl-position)<<(BYTE)doubleTrackStep };
							LOG_ACTION(_T("DeviceIoControl FD_RELATIVE_SEEK_PARAMS"));
							seeked=::DeviceIoControl( handle, IOCTL_FDCMD_RELATIVE_SEEK, &rsp,sizeof(rsp), nullptr,0, &nBytesTransferred, nullptr )!=0;
							LOG_BOOL(seeked);
						}else{
							FD_SEEK_PARAMS sp={ cyl<<(BYTE)doubleTrackStep, 0 };
							LOG_ACTION(_T("DeviceIoControl FD_SEEK_PARAMS"));
							seeked=::DeviceIoControl( handle, IOCTL_FDCMD_SEEK, &sp,sizeof(sp), nullptr,0, &nBytesTransferred, nullptr )!=0;
							LOG_BOOL(seeked);
						}
						break;
					default:
						ASSERT(FALSE);
						return LOG_BOOL(false);
				}
				if (seeked)
					position=cyl;
				else if (Utils::RetryCancel(::GetLastError()))
					continue;
				else
					return LOG_BOOL(false);
				break;
			}while (true);
		}
		return true;
	}

	bool CFDD::TFddHead::__calibrate__(){
		// True <=> Head successfully sent home (thus Calibrated), otherwise False
		LOG_ACTION("bool CFDD::TFddHead::__calibrate__()");
		position=0;
		DWORD nBytesTransferred;
		switch (driver){
			case DRV_FDRAWCMD:
				return LOG_BOOL(::DeviceIoControl( handle, IOCTL_FDCMD_RECALIBRATE, nullptr,0, nullptr,0, &nBytesTransferred, nullptr )!=0);
			default:
				ASSERT(FALSE);
				return LOG_BOOL(false);
		}
	}










	#define FDD_FDRAWCMD_NAME_PATTERN _T("Internal floppy drive %c: (fdrawcmd.sys)")

	static LPCTSTR Recognize(PTCHAR deviceNameList){
		PTCHAR p=deviceNameList;
		// - Simon Owen's fdrawcmd.sys
		for( TCHAR drvId[]=_T("\\\\.\\fdraw0"); drvId[9]<'4'; drvId[9]++ ){
			const HANDLE hFdd=::CreateFile( drvId, GENERIC_READ|GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr); //FILE_SHARE_WRITE
			if (hFdd!=INVALID_HANDLE_VALUE){ // Driver installed and exclusive access allowed to the A: drive
				p+=::wsprintf( p, FDD_FDRAWCMD_NAME_PATTERN, drvId[9]-'0'+'A' ) + 1;
				::CloseHandle(hFdd);
			}else
				break;
		}
		// - TODO: other drivers for internal floppy drives
		//
		// - no further drives found
		*p='\0';
		return deviceNameList;
	}
	static PImage Instantiate(LPCTSTR deviceName){
		return new CFDD(deviceName);
	}
	const CImage::TProperties CFDD::Properties={
		MAKE_IMAGE_ID('I','n','t','P','c','F','d','d'), // a unique identifier
		Recognize,	// list of recognized device names
		Instantiate,	// instantiation function
		nullptr, // filter
		Medium::FLOPPY_ANY, // supported Media
		Codec::FLOPPY_IBM, // supported Codecs
		128,SECTOR_LENGTH_MAX	// Sector supported min and max length
	};

	#define FDD_THREAD_PRIORITY_DEFAULT	THREAD_PRIORITY_ABOVE_NORMAL

	CFDD::CFDD(LPCTSTR deviceName)
		// ctor
		// - base
		: CFloppyImage(&Properties,true)
		// - initialization
		, dataBuffer( ::VirtualAlloc(nullptr,SECTOR_LENGTH_MAX,MEM_COMMIT,PAGE_READWRITE) ) {
		::lstrcpy( devicePatternName, deviceName );
		LOG_ACTION(_T("CFDD::ctor"));
		::ZeroMemory( internalTracks, sizeof(internalTracks) );
		// - connecting to the Drive
		Reset();
	}

	CFDD::~CFDD(){
		// dtor
		LOG_ACTION(_T("CFDD::dtor"));
		// - disconnecting from the Drive
		__disconnectFromFloppyDrive__();
		// - disposing all InternalTracks
		__freeInternalTracks__();
		// - freeing virtual memory
		::VirtualFree(dataBuffer,0,MEM_RELEASE);
	}








	TStdWinError CFDD::__connectToFloppyDrive__(TSupportedDriver _driver){
		// True <=> a connection to the Drive has been established using the Driver, otherwise False
		_HANDLE=INVALID_HANDLE_VALUE; // initialization
		switch ( DRIVER=_driver ){
			case DRV_AUTO:
				// automatic recognition of installed Drivers
				if (const TStdWinError err=__connectToFloppyDrive__(DRV_FDRAWCMD))
					return err;
				else
					return ERROR_SUCCESS;
			case DRV_FDRAWCMD:{
				// Simon Owen's Driver
				LOG_ACTION(_T("TStdWinError CFDD::__connectToFloppyDrive__ DRV_FDRAWCMD"));
				// . connecting to "A:"
				do{
					TCHAR drvId[16];
					::wsprintf( drvId, _T("\\\\.\\fdraw%c"), GetDriveLetter()-'A'+'0' );
					_HANDLE=::CreateFile( drvId, GENERIC_READ|GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr); //FILE_SHARE_WRITE
					if (_HANDLE!=INVALID_HANDLE_VALUE) break; // Driver installed and exclusive access allowed to the A: drive
error:				switch (const TStdWinError err=::GetLastError()){
						case ERROR_FILE_NOT_FOUND: // "file not found" error ...
							return LOG_ERROR(ERROR_NOT_READY); // ... replaced with the "device not ready" error
						case ERROR_ALREADY_EXISTS: // "file already exists" error ...
							if (!Utils::RetryCancel(_T("Exclusive access to the floppy drive required. Close all applications that may prevent it and try again.")))
								return LOG_ERROR(ERROR_ACCESS_DENIED); // ... replaced with the "access denied" error
							else
								continue;
						default:
							return LOG_ERROR(err);
					}
				}while (true);
				// . verifying the Driver's version
				DWORD nBytesTransferred,version;
				::DeviceIoControl( _HANDLE, IOCTL_FDRAWCMD_GET_VERSION, nullptr,0, &version,sizeof(version), &nBytesTransferred, nullptr );
				if (version<FDRAWCMD_VERSION) // old Driver version
					return LOG_ERROR(ERROR_BAD_DRIVER);
				// . synchronizing with index pulse
				__setWaitingForIndex__();
				// . turning off the automatic recognition of floppy inserted into Drive (having turned it on creates problems when used on older Drives [Simon Owen])
				static constexpr BYTE RecognizeInsertedFloppy=FALSE;
				if (!::DeviceIoControl( _HANDLE, IOCTL_FD_SET_DISK_CHECK, (PVOID)&RecognizeInsertedFloppy,1, nullptr,0, &nBytesTransferred, nullptr)){
					__disconnectFromFloppyDrive__();
					goto error;
				}
				// . logging the recognized floppy drive controller
				LOG_MESSAGE(__getControllerType__());
				return ERROR_SUCCESS;
			}
			default:
				ASSERT(FALSE);
				return LOG_ERROR(ERROR_DEVICE_NOT_AVAILABLE);
		}
	}

	void CFDD::__disconnectFromFloppyDrive__(){
		// disconnects from the Drive (if previously connected to)
		LOG_ACTION(_T("void CFDD::__disconnectFromFloppyDrive__"));
		if (_HANDLE!=INVALID_HANDLE_VALUE){
			::CloseHandle(_HANDLE);
			_HANDLE=INVALID_HANDLE_VALUE;
		}
	}

	TCHAR CFDD::GetDriveLetter() const{
		// returns the DriveLetter of currently accessed floppy drive
		TCHAR driveLetter;
		_stscanf( devicePatternName, FDD_FDRAWCMD_NAME_PATTERN, &driveLetter );
		return driveLetter;
	}




	CFDD::PInternalTrack CFDD::__getScannedTrack__(TCylinder cyl,THead head) const{
		// returns Internal representation of the Track
		return	cyl<FDD_CYLINDERS_MAX && head<2
				? internalTracks[cyl][head]
				: nullptr;
	}

	void CFDD::UnformatInternalTrack(TCylinder cyl,THead head) const{
		// disposes buffered InternalTrack
		if (const PInternalTrack tmp=__getScannedTrack__(cyl,head))
			delete tmp, internalTracks[cyl][head]=nullptr;
	}

	void CFDD::__freeInternalTracks__(){
		// disposes all InternalTracks
		LOG_ACTION(_T("void CFDD::__freeInternalTracks__"));
		for( TCylinder cyl=0; cyl<FDD_CYLINDERS_MAX; cyl++ )
			UnformatInternalTrack(cyl,0), UnformatInternalTrack(cyl,1);
	}




	BOOL CFDD::OnOpenDocument(LPCTSTR){
		// True <=> Image opened successfully, otherwise False
		return TRUE; // always successfull; failure may arise later on when attempting to access the Drive in exclusive mode
	}



	TStdWinError CFDD::SaveTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const{
		// saves the specified Track to the inserted Medium; returns Windows standard i/o error
		LOG_TRACK_ACTION(cyl,head,_T("UINT CFDD::SaveTrack"));
		if (TInternalTrack *const pit=__getScannedTrack__(cyl,head)){
			// . saving (and verifying) data of Track's all Modified Sectors
			bool allSectorsProcessed;
			do{
				// : saving
				BYTE justSavedSectors[(TSector)-1];
				::ZeroMemory(justSavedSectors,pit->nSectors);
				do{
					if (cancelled)
						return ERROR_CANCELLED;
					allSectorsProcessed=true; // assumption
					TLogTime lastSectorEndNanoseconds=TIME_SECOND(-1); // minus one second
					for( TSector n=0; n<pit->nSectors; n++ ){
						TInternalTrack::TSectorInfo &si=pit->sectors[n];
						if (si.IsModified() && !justSavedSectors[n]){
							if (si.startNanoseconds-lastSectorEndNanoseconds>=fddHead.profile.gap3Latency) // sufficient distance between this and previously saved Sectors, so both of them can be processed in a single disk revolution
								if (const TStdWinError err=si.SaveToDisk( const_cast<CFDD *>(this), pit, n, false, cancelled )) // False = verification carried out below
									return err;
								else{
									if (!params.verifyWrittenData)
										si.dirtyRevolution=Revolution::NONE; // no longer Modified if Verification turned off
									lastSectorEndNanoseconds=si.endNanoseconds;
									justSavedSectors[n]=true;
								}
							allSectorsProcessed=false; // will need one more cycle iteration to eventually find out that all Sectors are processed OK
						}
					}
				}while (!allSectorsProcessed);
				// : verification
				do{
					if (cancelled)
						return ERROR_CANCELLED;
					allSectorsProcessed=true; // assumption
					TLogTime lastSectorEndNanoseconds=TIME_SECOND(-1); // minus one second
					for( TSector n=0; n<pit->nSectors; n++ ){
						TInternalTrack::TSectorInfo &si=pit->sectors[n];
						if (si.IsModified())
							if (si.startNanoseconds-lastSectorEndNanoseconds>=fddHead.profile.gap3Latency){ // sufficient distance between this and previously saved Sectors, so both of them can be processed in a single disk revolution
								if (cancelled || si.VerifySaving(this,pit,n)==IDABORT)
									return ERROR_CANCELLED;
								lastSectorEndNanoseconds=si.endNanoseconds;
								allSectorsProcessed=false; // will need one more cycle iteration to eventually find out that all Sectors are processed OK
							}
					}
				}while (!allSectorsProcessed);
			}while (!allSectorsProcessed);
		}
		return ERROR_SUCCESS;
	}


	TCylinder CFDD::GetCylinderCount() const{
		// determines and returns the actual number of Cylinders in the Image
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		LOG_ACTION(_T("TCylinder CFDD::GetCylinderCount"));
		return	GetNumberOfFormattedSides(0) // if zeroth Cylinder exists ...
				? (FDD_CYLINDERS_HD>>(BYTE)fddHead.doubleTrackStep)+FDD_CYLINDERS_EXTRA // ... then it's assumed that there is the maximum number of Cylinders available (the actual number may be adjusted by systematically scanning the Tracks)
				: 0; // ... otherwise the floppy is considered not formatted
	}

	THead CFDD::GetHeadCount() const{
		// determines and returns the actual number of Heads in the Image
		//EXCLUSIVELY_LOCK_THIS_IMAGE();
		LOG_ACTION(_T("THead CFDD::GetHeadCount"));
		return 2; // latest PC floppy drives had two Heads
	}

	BYTE CFDD::GetAvailableRevolutionCount(TCylinder cyl,THead head) const{
		// returns the number of data variations of one Sector that are guaranteed to be distinct
		return Revolution::INFINITY;
	}

	TStdWinError CFDD::SeekHeadsHome(){
		// attempts to send Heads "home"; returns Windows standard i/o error
		return	fddHead.SeekHome() ? ERROR_SUCCESS : ::GetLastError();
	}

	CFDD::PInternalTrack CFDD::__scanTrack__(TCylinder cyl,THead head){
		// scans given Track and returns the number of discovered Sectors; returns Null if Track cannot be scanned (e.g. due to an hardware error or "out-of-range" error)
		// - attempting to scan the specified Track
		if (cyl<FDD_CYLINDERS_MAX && head<2)
			// Track can be scanned
			if (const PInternalTrack pit=__getScannedTrack__(cyl,head))
				// Track has been already scanned before - returning it
				return pit;
			else
				// Track has not yet been scanned - attempting to scan it now
				if (fddHead.__seekTo__(cyl)){
					// successfully seeked to the given Cylinder
					LOG_TRACK_ACTION(cyl,head,_T("CFDD::PInternalTrack CFDD::__scanTrack__"));
					DWORD nBytesTransferred;
					switch (DRIVER){
						case DRV_FDRAWCMD:{
							FD_SCAN_PARAMS sp={ FD_OPTION_MFM, head };
							#pragma pack(1)
							struct{
								BYTE n;
								BYTE firstSeen;
								DWORD trackTime;
								FD_TIMED_ID_HEADER header[(TSector)-1];
							} sectors;
							if (!::DeviceIoControl( _HANDLE, IOCTL_FD_TIMED_SCAN_TRACK, &sp,sizeof(sp), &sectors,sizeof(sectors), &nBytesTransferred, nullptr ))
								break;
							TSectorId bufferId[(TSector)-1]; TLogTime sectorTimes[(TSector)-1];
							for( BYTE n=0; n<sectors.n; n++ )
								bufferId[n]=sectors.header[n], sectorTimes[n]=TIME_MICRO( sectors.header[n].reltime );
							return internalTracks[cyl][head] = new TInternalTrack( this, cyl, head, Codec::MFM, sectors.n, bufferId, sectorTimes );
						}
						default:
							ASSERT(FALSE);
							break;
					}
				}
		// - Track cannot be scanned
		return nullptr;
	}

	TSector CFDD::ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec,PSectorId bufferId,PWORD bufferLength,PLogTime startTimesNanoseconds,PBYTE pAvgGap3) const{
		// returns the number of Sectors found in given Track, and eventually populates the Buffer with their IDs (if Buffer!=Null); returns 0 if Track not formatted or not found
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (const PInternalTrack pit=((CFDD *)this)->__scanTrack__(cyl,head)){
			// Track managed to be scanned
			const TInternalTrack::TSectorInfo *psi=pit->sectors;
			for( TSector s=0; s<pit->nSectors; s++,psi++ ){
				if (bufferId)
					*bufferId++=psi->id;
				if (bufferLength)
					*bufferLength++=psi->length;
				if (startTimesNanoseconds)
					*startTimesNanoseconds++=psi->startNanoseconds;
			}
			if (pCodec)
				*pCodec=Codec::MFM; // TODO: currently only MFM support implemented
			if (pAvgGap3)
				if (pit->nSectors>1){
					TLogTime nsSum=0; // sum of Gap3 nanoseconds
					const TInternalTrack::TSectorInfo *psi=pit->sectors;
					for( TSector s=0; s<pit->nSectors-1; nsSum-=psi->endNanoseconds,s++,psi++,nsSum+=psi->startNanoseconds );
					*pAvgGap3=nsSum/((pit->nSectors-1)*fddHead.profile.oneByteLatency);
				}else
					*pAvgGap3= floppyType==Medium::FLOPPY_DD_525 ? FDD_525_SECTOR_GAP3 : FDD_350_SECTOR_GAP3;
			return pit->nSectors;
		}else
			// Track failed to be scanned
			return 0;
	}

	bool CFDD::IsTrackScanned(TCylinder cyl,THead head) const{
		// True <=> Track exists and has already been scanned, otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return	__getScannedTrack__(cyl,head)!=nullptr;
	}

	void CFDD::__setWaitingForIndex__() const{
		// sets waiting for the index pulse before executing the next command
		LOG_ACTION(_T("void CFDD::__setWaitingForIndex__"));
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:{
				::DeviceIoControl( _HANDLE, IOCTL_FD_WAIT_INDEX, nullptr,0, nullptr,0, &nBytesTransferred, nullptr );
				break;
			}
			default:
				ASSERT(FALSE);
		}
	}

	TLogTime CFDD::GetAvgIndexDistance() const{
		// given at least two indices, computes and returns the average distance between them, otherwise 0
		LOG_ACTION(_T("LPCTSTR CFDD::GetAvgIndexDistance"));
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:{
				// "setup and measurement process for each request takes 4 disk rotations" (Simon Owen)
				DWORD usRevolution; // Revolution time in microseconds
				::DeviceIoControl( _HANDLE, IOCTL_FD_GET_TRACK_TIME, nullptr,0, &usRevolution,sizeof(usRevolution), &nBytesTransferred, nullptr );
				return TIME_MICRO(usRevolution);
			}
			default:
				ASSERT(FALSE);
				return 0;
		}
	}

	void CFDD::__setNumberOfSectorsToSkipOnCurrentTrack__(BYTE nSectorsToSkip) const{
		// True <=> the NumberOfSectors to be skipped on the current Track successfully set, otherwise False
		LOG_ACTION(_T("void CFDD::__setNumberOfSectorsToSkipOnCurrentTrack__"));
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:{
				FD_SECTOR_OFFSET_PARAMS sop={ nSectorsToSkip };
				::DeviceIoControl( _HANDLE, IOCTL_FD_SET_SECTOR_OFFSET, &sop,sizeof(sop), nullptr,0, &nBytesTransferred, nullptr );
				break;
			}
			default:
				ASSERT(FALSE);
		}
	}

	TStdWinError CFDD::__setTimeBeforeInterruptingTheFdc__(WORD nDataBytesBeforeInterruption,TLogTime nNanosecondsAfterLastDataByteWritten) const{
		// registers a request to interrupt the following write/format command after specified NumberOfBytes plus additional NumberOfNanoseconds; returns Windows standard i/o error
		LOG_ACTION(_T("TStdWinError CFDD::__setTimeBeforeInterruptingTheFdc__"));
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:{
				FD_SHORT_WRITE_PARAMS swp={ nDataBytesBeforeInterruption, nNanosecondsAfterLastDataByteWritten/1000 }; // "/1000" = nanoseconds to microseconds
				return	::DeviceIoControl( _HANDLE, IOCTL_FD_SET_SHORT_WRITE, &swp,sizeof(swp), nullptr,0, &nBytesTransferred, nullptr )!=0
						? ERROR_SUCCESS
						: LOG_ERROR(::GetLastError());
			}
			default:
				ASSERT(FALSE);
				return LOG_ERROR(ERROR_DEVICE_NOT_AVAILABLE);
		}
	}

	TStdWinError CFDD::__setTimeBeforeInterruptingTheFdc__(WORD nDataBytesBeforeInterruption) const{
		// registers a request to interrupt the following write/format command after specified NumberOfBytes plus additional NumberOfNanosends; returns Windows standard i/o error
		return __setTimeBeforeInterruptingTheFdc__( nDataBytesBeforeInterruption, fddHead.profile.controllerLatency );
	}

	bool CFDD::__bufferSectorData__(TCylinder cyl,THead head,PCSectorId psi,WORD sectorLength,const TInternalTrack *pit,BYTE nSectorsToSkip,TFdcStatus *pFdcStatus) const{
		// True <=> requested Sector found in currently seeked Track and data of the Sector have been buffered in the internal DataBuffer, otherwise False
		LOG_SECTOR_ACTION(psi,_T("bool CFDD::__bufferSectorData__"));
		// - taking into account the NumberOfSectorsToSkip
		if (pit->__isIdDuplicated__(psi)) // to speed matters up, only if ID is duplicated in the Track
			__setNumberOfSectorsToSkipOnCurrentTrack__(nSectorsToSkip);
		// - waiting for the index pulse
		//__setWaitingForIndex__(); // commented out as caller is to decide whether this is needed or not, and eventually call it
		// - buffering Sector's data
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:{
				LOG_ACTION(_T("DeviceIoControl IOCTL_FDCMD_READ_DATA"));
				FD_READ_WRITE_PARAMS rwp={ FD_OPTION_MFM|FD_OPTION_SK, head, psi->cylinder,psi->side,psi->sector,psi->lengthCode, psi->sector+1, 1, 0xff };
				if (!::DeviceIoControl( _HANDLE, IOCTL_FDCMD_READ_DATA, &rwp,sizeof(rwp), dataBuffer,SECTOR_LENGTH_MAX, &nBytesTransferred, nullptr )){
					// Sector read with errors
					// | getting FdcStatus
					LOG_ACTION(_T("DeviceIoControl IOCTL_FD_GET_RESULT"));
					FD_CMD_RESULT cmdRes;
					::DeviceIoControl( _HANDLE, IOCTL_FD_GET_RESULT, nullptr,0, &cmdRes,sizeof(cmdRes), &nBytesTransferred, nullptr );
					*pFdcStatus=TFdcStatus(cmdRes.st1,cmdRes.st2);
					// | if "Deleted DAM" is one of the errors, repeating reading with appropriate command
					if (pFdcStatus->DescribesDeletedDam()){
						{	LOG_ACTION(_T("DeviceIoControl IOCTL_FDCMD_READ_DELETED_DATA"));
							::DeviceIoControl( _HANDLE, IOCTL_FDCMD_READ_DELETED_DATA, &rwp,sizeof(rwp), dataBuffer,SECTOR_LENGTH_MAX, &nBytesTransferred, nullptr );
						}
						{	LOG_ACTION(_T("DeviceIoControl IOCTL_FD_GET_RESULT"));
							::DeviceIoControl( _HANDLE, IOCTL_FD_GET_RESULT, nullptr,0, &cmdRes,sizeof(cmdRes), &nBytesTransferred, nullptr );
						}
						*pFdcStatus=TFdcStatus( cmdRes.st1, cmdRes.st2|FDC_ST2_DELETED_DAM );
					}
				}else
					*pFdcStatus=TFdcStatus::WithoutError;
				return true;
			}
			default:
				ASSERT(FALSE);
				return LOG_BOOL(false);
		}
	}

	bool CFDD::__bufferSectorData__(RCPhysicalAddress chs,WORD sectorLength,const TInternalTrack *pit,BYTE nSectorsToSkip,TFdcStatus *pFdcStatus) const{
		// True <=> requested Sector found in currently seeked Track and data of the Sector have been buffered in the internal DataBuffer, otherwise False
		return __bufferSectorData__( chs.cylinder, chs.head, &chs.sectorId, sectorLength, pit, nSectorsToSkip, pFdcStatus );
	}

	void CFDD::GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses){
		// populates output buffers with specified Sectors' data, usable lengths, and FDC statuses; ALWAYS attempts to buffer all Sectors - caller is then to sort out eventual read errors (by observing the FDC statuses); caller can call ::GetLastError to discover the error for the last Sector in the input list
		ASSERT( outBufferData!=nullptr && outBufferLengths!=nullptr && outFdcStatuses!=nullptr );
		// - initializing the output buffers with data retrieval failure (assumption)
		::ZeroMemory( outBufferData, nSectors*sizeof(PSectorData) );
		for( TSector i=nSectors; i>0; outFdcStatuses[--i]=TFdcStatus::SectorNotFound );
		// - getting the real data for the Sectors
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		LOG_TRACK_ACTION(cyl,head,_T("void CFDD::GetTrackData"));
		if (const PInternalTrack pit=__scanTrack__(cyl,head)){
			// . Planning the requested Sectors retrieval
			#ifdef LOGGING_ENABLED			
				TCHAR buf[4000];
				for( TCHAR n=0,*p=buf; n<pit->nSectors; n++ ){
					const int i=::wsprintf(p,_T("%d:<%d,%d> "),n,pit->sectors[n].startNanoseconds,pit->sectors[n].endNanoseconds);
					p+=i;
				}
				LOG_MESSAGE(buf);
			#endif
			struct TPlanStep sealed{
				TInternalTrack::TSectorInfo *psi;
				BYTE indexIntoOutputBuffers;
			} plan[(TSector)-1], *planEnd=plan;
			BYTE alreadyPlannedSectors[(TSector)-1];
			::ZeroMemory(alreadyPlannedSectors,nSectors);
			for( BYTE nSectorsToPlan=nSectors; planEnd-plan<nSectors && nSectorsToPlan; nSectorsToPlan-- ){ // A&B, A = all Sectors requested to read planned, B = all Sectors are planned in N iterations in the worst case (preventing infinite loop in case that at least one Sector isn't found on the Track)
				TLogTime lastSectorEndNanoseconds=TIME_SECOND(-1); // minus one second
				for( TSector n=0; n<pit->nSectors; n++ ){
					TInternalTrack::TSectorInfo &si=pit->sectors[n];
					for( TSector s=0; s<nSectors; s++ )
						if (!alreadyPlannedSectors[s] && bufferId[s]==si.id && bufferNumbersOfSectorsToSkip[s]<=n)
							// one of the Sectors requested to read
							if (si.startNanoseconds-lastSectorEndNanoseconds>=fddHead.profile.gap3Latency){ // sufficient distance between this and previously read Sectors, so both of them can be read in a single disk revolution
								planEnd->psi=&si, planEnd->indexIntoOutputBuffers=s;
								planEnd++;
								lastSectorEndNanoseconds=si.endNanoseconds;
								alreadyPlannedSectors[s]=true;
								break;
							}
				}
			}
			// . executing the above composed Plan
			for( const TPlanStep *pPlanStep=plan; pPlanStep<planEnd; pPlanStep++ ){
				TInternalTrack::TSectorInfo &rsi=*pPlanStep->psi;
				const BYTE index=pPlanStep->indexIntoOutputBuffers;
				const WORD length = outBufferLengths[index] = rsi.length;
				// : selecting Data by Revolution
				if (rsi.IsModified())
					// DirtyRevolution is obligatory for any subsequent data requests
					rsi.currentRevolution=rsi.dirtyRevolution;
				else
					switch (rev){
						case Revolution::CURRENT:
							// getting Current Revolution
							break;
						default:
							if (rev<rsi.nRevolutions){
								// getting particular existing Revolution
								rsi.currentRevolution=rev;
								break;
							}else if (rev<Revolution::MAX){
								// getting particular non-existing Revolution by subsequently requesting Next Revolutions
								rsi.nRevolutions = rsi.currentRevolution = rev;
								//fallthrough
							}else{
								ASSERT(FALSE); // we shouldn't end up here!
								::SetLastError( ERROR_BAD_COMMAND );
								return;
							}
							//fallthrough
						case Revolution::NEXT:{
							// getting Next Revolution
							// > if the Next Revolution maps to an existing Revolution, return it
							if (++rsi.currentRevolution<rsi.nRevolutions)
								break;
							// > if an exceeding Revolution requested, push the oldest one out of the buffer
							if (rsi.nRevolutions==Revolution::MAX){
								FREE_SECTOR_DATA( rsi.revolutions[0].data );
								::memmove(
									rsi.revolutions, rsi.revolutions+1,
									(Revolution::MAX-1)*sizeof(*rsi.revolutions)
								);
								rsi.nRevolutions--;
							}
							// > attempt for the Data
							auto &rev=rsi.revolutions[ rsi.currentRevolution=rsi.nRevolutions ]; // the last Revolution, below set empty
								rev.data=nullptr; // Sector with given ID physically not found or has no DAM)
								rev.fdcStatus=TFdcStatus::Unknown; // attempt for the data below
							rsi.nRevolutions++;
							break;
						}
						case Revolution::ANY_GOOD:{
							// getting latest Revolution with healthy Data, eventually accessing the device for one more Revolution
							// > checking if we already have healthy Data in the buffer
							bool healthyDataExist=false;
							for( rsi.currentRevolution=rsi.nRevolutions; rsi.currentRevolution>0; ) // check latest Revolution first
								if ( healthyDataExist=rsi.revolutions[--rsi.currentRevolution].HasGoodDataReady() )
									break;
							if (healthyDataExist)
								break;
							// > trying several Next Revolutions if they contain healthy Data
							rsi.currentRevolution=rsi.nRevolutions; // let the attempts be brand new Revolutions
							for( char nTrials=3; true; ){
								WORD w;
								if (GetSectorData( cyl, head, Revolution::NEXT, &rsi.id, rsi.seqNum, &w, &TFdcStatus() ))
									if (rsi.revolutions[rsi.currentRevolution].fdcStatus.IsWithoutError())
										break;
								if (--nTrials==0)
									break;
								if (nTrials==1){ // calibrating Head for the last Trial
									const TPhysicalAddress chs={ cyl, head, rsi.id };
									const bool knownSectorBad =	params.calibrationAfterErrorOnlyForKnownSectors && dos && dos->properties!=&CUnknownDos::Properties
																? dos->GetSectorStatus(chs)!=CDos::TSectorStatus::UNKNOWN
																: true;
									switch (params.calibrationAfterError){
										case TParams::TCalibrationAfterError::FOR_EACH_SECTOR:
											// calibrating for each bad Sector
											fddHead.calibrated=false;
											//fallthrough
										case TParams::TCalibrationAfterError::ONCE_PER_CYLINDER:
											// calibrating only once for the whole Cylinder
											if (!fddHead.calibrated){
												fddHead.__calibrate__(), fddHead.__seekTo__(cyl);
												fddHead.calibrated=true;
											}
											break;
									}
								}
							}
							break;
						}
					}
				// : if not yet attempted for the data, doing so now
				auto &rev=rsi.revolutions[rsi.currentRevolution];
				if (rev.fdcStatus.ToWord()==TFdcStatus::Unknown.ToWord()){
					if (!fddHead.__seekTo__(cyl))
						return; // Sectors cannot be found as Head cannot be seeked
					if (__bufferSectorData__( cyl, head, &rsi.id, length, pit, bufferNumbersOfSectorsToSkip[index], &rev.fdcStatus ) // yes, Sector found ...
						&&
						!rev.fdcStatus.DescribesMissingDam() // ... and has a DAM
					)
						rev.data=(PSectorData)::memcpy( ALLOCATE_SECTOR_DATA(length), dataBuffer, length );
				}
				// : returning (any) Data
				outFdcStatuses[index]=rev.fdcStatus;
				outBufferData[index]=rev.data;
			}
		}else
			LOG_MESSAGE(_T("Track not found"));
		// - outputting the result
		::SetLastError( outBufferData[nSectors-1] ? ERROR_SUCCESS : ERROR_SECTOR_NOT_FOUND );
	}

	TDataStatus CFDD::IsSectorDataReady(TCylinder cyl,THead head,RCSectorId id,BYTE nSectorsToSkip,Revolution::TType rev) const{
		// True <=> specified Sector's data variation (Revolution) has been buffered, otherwise False
		ASSERT( rev<Revolution::MAX );
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (const PCInternalTrack pit=__getScannedTrack__(cyl,head)) // Track has already been scanned
			while (nSectorsToSkip<pit->nSectors){
				const auto &ris=pit->sectors[nSectorsToSkip++];
				if (ris.id==id)
					if (rev>=ris.nRevolutions)
						break;
					else if (ris.revolutions[rev].HasGoodDataReady())
						return TDataStatus::READY_HEALTHY;
					else if (ris.revolutions[rev].HasDataReady())
						return TDataStatus::READY;
					else
						break;
			}
		return TDataStatus::NOT_READY;
	}

	TStdWinError CFDD::MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus){
		// marks Sector with specified PhysicalAddress as "dirty", plus sets it the given FdcStatus; returns Windows standard i/o error
		LOG_SECTOR_ACTION(&chs.sectorId,_T("TStdWinError CFDD::MarkSectorAsDirty"));
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (const PInternalTrack pit=__getScannedTrack__(chs.cylinder,chs.head)){ // Track has already been scanned
			// . Modifying data of Sector requested in current Track
			while (nSectorsToSkip<pit->nSectors){
				auto &ris=pit->sectors[nSectorsToSkip++];
				if (ris.id==chs.sectorId){
					ASSERT( !ris.IsModified() || ris.dirtyRevolution==ris.currentRevolution ); // no Revolution yet marked as "dirty" or marking "dirty" the same Revolution
					if (ris.currentRevolution<ris.nRevolutions){ // Sector must be buffered to mark it Modified
						auto &rev=ris.revolutions[ ris.dirtyRevolution=(Revolution::TType)ris.currentRevolution ];
						rev.fdcStatus=*pFdcStatus;
						SetModifiedFlag();
					}
						// : informing on unability to reproduce some errors (until all errors are supported in the future)
						if (pFdcStatus->reg1 & (FDC_ST1_NO_DATA)){
							TCHAR buf[200];
							::wsprintf( buf, _T("Not all errors can be reproduced on a real floppy for sector with %s."), (LPCTSTR)chs.sectorId.ToString() );
							Utils::Information(buf);
							return LOG_ERROR(ERROR_BAD_COMMAND);
						}
					return ERROR_SUCCESS;
				}
			}
			return ERROR_SECTOR_NOT_FOUND; // unknown Sector queried
		}else
			return ERROR_BAD_ARGUMENTS; // Track must be scanned first!
	}

	Revolution::TType CFDD::GetDirtyRevolution(RCPhysicalAddress chs,BYTE nSectorsToSkip) const{
		// returns the Revolution that has been marked as "dirty"
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (const PInternalTrack pit=__getScannedTrack__(chs.cylinder,chs.head)){ // Track has already been scanned
			const TInternalTrack::TSectorInfo *psi=pit->sectors;
			for( TSector n=pit->nSectors; n--; psi++ )
				if (nSectorsToSkip)
					nSectorsToSkip--;
				else if (psi->id==chs.sectorId)
					return	psi->dirtyRevolution;
		}
		return Revolution::NONE; // unknown Sector not Modified
	}

	TStdWinError CFDD::SetDataTransferSpeed(Medium::TType _floppyType) const{
		// sets TransferSpeed for given FloppyType; returns Windows standard i/o error
		DWORD nBytesTransferred;
		BYTE transferSpeed;
		#ifdef LOGGING_ENABLED
			LOG_ACTION(_T("TStdWinError CFDD::SetDataTransferSpeed"));
			LOG_MESSAGE(Medium::GetDescription(_floppyType));
		#endif
		switch (_floppyType){
			case Medium::FLOPPY_DD:
				// 3.5" or 5.25" 2DD floppy in 300 RPM drive
				switch (DRIVER){
					case DRV_FDRAWCMD:{
						transferSpeed=FD_RATE_250K;
fdrawcmd:				return	::DeviceIoControl( _HANDLE, IOCTL_FD_SET_DATA_RATE, &transferSpeed,1, nullptr,0, &nBytesTransferred, nullptr )
								? ERROR_SUCCESS
								: LOG_ERROR(::GetLastError());
					}
					default:
						ASSERT(FALSE);
						break;
				}
				break;
			case Medium::FLOPPY_HD_525:
			case Medium::FLOPPY_HD_350:
				// HD floppy
				switch (DRIVER){
					case DRV_FDRAWCMD:
						transferSpeed=FD_RATE_500K;
						goto fdrawcmd;
					default:
						ASSERT(FALSE);
						break;
				}
				break;
			case Medium::FLOPPY_DD_525:
				// 5.25" HD floppy in 360 RPM drive
				switch (DRIVER){
					case DRV_FDRAWCMD:
						transferSpeed=FD_RATE_300K;
						goto fdrawcmd;
					default:
						ASSERT(FALSE);
						break;
				}
				break;
		}
		return LOG_ERROR(ERROR_DEVICE_NOT_AVAILABLE);
	}

	TStdWinError CFDD::GetInsertedMediumType(TCylinder cyl,Medium::TType &rOutMediumType) const{
		// True <=> Medium inserted in the Drive and recognized, otherwise False
		LOG_ACTION(_T("TStdWinError CFDD::GetInsertedMediumType"));
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - disk must be inserted
		if (!IsFloppyInserted())
			return LOG_ERROR(ERROR_NO_MEDIA_IN_DRIVE);
		// - enumerating possible floppy Types and attempting to recognize some Sectors
		WORD highestScore=0; // arbitering the MediumType by the HighestScore and indices distance
		Medium::TType bestMediumType=Medium::UNKNOWN;
		const TLogTime avgIndexDistance=GetAvgIndexDistance(); // this is time-consuming, so doing it just once and re-using the result
		for( DWORD type=1; type!=0; type<<=1 )
			if (type&Medium::FLOPPY_ANY){
				if (SetDataTransferSpeed( (Medium::TType)type )) // error?
					continue;
				const PInternalTrack pit=internalTracks[cyl][0];
				internalTracks[cyl][0]=nullptr; // forcing a new scan
					if (WORD score= internalTracks[cyl][0]->nSectors + 32*GetCountOfHealthySectors(cyl,0)){
						if (avgIndexDistance){ // measurement supported/succeeded?
							const Medium::PCProperties mp=Medium::GetProperties((Medium::TType)type);
							if (avgIndexDistance/10*9<mp->revolutionTime && mp->revolutionTime<avgIndexDistance/10*11) // 10% tolerance (don't set more for indices on 300 RPM drive appear only 16% slower than on 360 RPM drive!)
								score|=0x8000;
						}
						if (score>highestScore)
							highestScore=score, bestMediumType=(Medium::TType)type;
					}
					UnformatInternalTrack(cyl,0);
				internalTracks[cyl][0]=pit;
			}
		// - reverting to original data transfer speed
		if (floppyType!=Medium::UNKNOWN)
			SetDataTransferSpeed( floppyType );
		// - Medium (possibly) recognized
		rOutMediumType=bestMediumType; // may be Medium::UNKNOWN
		return ERROR_SUCCESS;
	}

	TStdWinError CFDD::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		LOG_ACTION(_T("TStdWinError CFDD::SetMediumTypeAndGeometry"));
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - base
		if (const TStdWinError err=__super::SetMediumTypeAndGeometry(pFormat,sideMap,firstSectorNumber))
			return LOG_ERROR(err);
		// - setting the transfer speed according to current FloppyType (DD/HD)
		__freeInternalTracks__();
		if (floppyType!=Medium::UNKNOWN
			&&
			(floppyType&Medium::FLOPPY_ANY)!=0 // set in base method to "pFormat->mediumType"
		)
			return SetDataTransferSpeed(floppyType);
		// - automatically recognizing the Type of inserted floppy
		floppyType=Medium::UNKNOWN;
		if (const TStdWinError err=GetInsertedMediumType( 0, floppyType ))
			return LOG_ERROR(err);
		if (floppyType==Medium::UNKNOWN)
			return LOG_ERROR(ERROR_BAD_COMMAND);
		return SetDataTransferSpeed(floppyType);
	}

	void CFDD::__setSecondsBeforeTurningMotorOff__(BYTE nSeconds) const{
		// sets given NumberOfSeconds before turning the motor off
		LOG_ACTION(_T("void CFDD::__setSecondsBeforeTurningMotorOff__"));
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:
				::DeviceIoControl( _HANDLE, IOCTL_FD_SET_MOTOR_TIMEOUT, &++nSeconds,sizeof(nSeconds), nullptr,0, &nBytesTransferred, nullptr );
				break;
			default:
				ASSERT(FALSE);
		}
	}

	bool CFDD::IsFloppyInserted() const{
		// True <=> (some) floppy is inserted in the Drive, otherwise False
		LOG_ACTION(_T("bool CFDD::IsFloppyInserted"));
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:
				return LOG_BOOL(::DeviceIoControl( _HANDLE, IOCTL_FD_CHECK_DISK, nullptr,0, nullptr,0, &nBytesTransferred, nullptr )!=0
								? ::GetLastError()==ERROR_SUCCESS
								: false
							);
			default:
				ASSERT(FALSE);
				return LOG_BOOL(false);
		}
	}

	LPCTSTR CFDD::__getControllerType__() const{
		// determines and returns the controller type of the Drive
		LOG_ACTION(_T("LPCTSTR CFDD::__getControllerType__"));
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:{
				FD_FDC_INFO fdcInfo;
				::DeviceIoControl( _HANDLE, IOCTL_FD_GET_FDC_INFO, nullptr,0, &fdcInfo,sizeof(fdcInfo), &nBytesTransferred, nullptr );
				switch (fdcInfo.ControllerType){
					case FDC_TYPE_NORMAL	: return _T("Normal");
					case FDC_TYPE_ENHANCED	: return _T("Enhanced");
					case FDC_TYPE_82077		: return _T("Intel 82077");
					case FDC_TYPE_82077AA	: return _T("Intel 82077AA");
					case FDC_TYPE_82078_44	: return _T("Intel 82078 (44 pin)");
					case FDC_TYPE_82078_64	: return _T("Intel 82078 (64 pin)");
					default					: return _T("Unknown");
				}
			}
			default:
				ASSERT(FALSE);
				return nullptr;
		}
	}

	#define TEST_BYTE	'A'

	#pragma pack(1)
	struct TLatencyParams sealed:public TPhysicalAddress{ // addressing healthy Track to use for computation of the latencies
		CFDD *const fdd;
		const WORD nsAccuracy; // accuracy in nanoseconds
		const BYTE nRepeats;
		TLogTime outControllerLatency;	// nanoseconds
		TLogTime out1ByteLatency;		// nanoseconds
		TLogTime outGap3Latency;		// nanoseconds

		TLatencyParams(CFDD *fdd,WORD nsAccuracy,BYTE nRepeats)
			: fdd(fdd)
			, nsAccuracy(nsAccuracy) , nRepeats(nRepeats)
			, outControllerLatency(0) , out1ByteLatency(0) , outGap3Latency(0) {
			cylinder=FDD_CYLINDERS_HD/2, head=0; // a healthy Track will be found when determining the controller latency
			sectorId.lengthCode=fdd->GetMaximumSectorLengthCode(); // test Sector (any values will do, so initializing only the LengthCode)
		}
	};
	UINT AFX_CDECL CFDD::FindHealthyTrack_thread(PVOID pCancelableAction){
		// thread to find healthy Track by writing a test Sector in it (DD = 4kB, HD = 8kB)
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		TLatencyParams &lp=*(TLatencyParams *)pAction->GetParams();
		const TCylinder cylMax=lp.cylinder;
		const WORD testSectorLength=lp.fdd->GetOfficialSectorLength( lp.sectorId.lengthCode );
		pAction->SetProgressTarget( cylMax+1 );
		EXCLUSIVELY_LOCK_IMAGE(*lp.fdd); // locking the access so that no one can disturb during the testing
		for( BYTE nSuccessfullWritings=0; true; ){
			if (pAction->Cancelled) return LOG_ERROR(ERROR_CANCELLED);
			// . seeking Head to the particular Cylinder
			if (!lp.fdd->fddHead.__seekTo__(lp.cylinder))
				return LOG_ERROR(pAction->TerminateWithError(ERROR_REQUEST_REFUSED));
			// . formatting Track to a single Sector
	{		const Utils::CVarTempReset<PInternalTrack> it0( lp.fdd->internalTracks[lp.cylinder][lp.head], nullptr ); // not scanned yet
			const Utils::CVarTempReset<bool> vft0( lp.fdd->params.verifyFormattedTracks, false );
				const TStdWinError err=lp.fdd->FormatTrack( lp.cylinder, lp.head, Codec::MFM, 1,&lp.sectorId,&testSectorLength,&TFdcStatus::WithoutError, FDD_350_SECTOR_GAP3, 0, pAction->Cancelled );
			lp.fdd->UnformatInternalTrack(lp.cylinder,lp.head); // disposing any new InternalTrack representation
			if (err!=ERROR_SUCCESS)
				return LOG_ERROR(pAction->TerminateWithError(err));
	}		// . verifying the single formatted Sector
			TFdcStatus sr;
			lp.fdd->__bufferSectorData__( lp.cylinder, lp.head, &lp.sectorId, testSectorLength, &TInternalTrack(lp.fdd,lp.cylinder,lp.head,Codec::MFM,1,&lp.sectorId,(PCLogTime)FDD_350_SECTOR_GAP3), 0, &sr );
			if (sr.IsWithoutError()) // a healthy Track ...
				if (++nSuccessfullWritings==3) // ... whose healthiness can be repeatedly confirmed has been found
					return pAction->TerminateWithSuccess(); // using it for computation of all latencies
				else
					continue;
			// . attempting to create a Sector WithoutErrors on another Track
			if (!--lp.cylinder)
				break;
			nSuccessfullWritings=0;
			pAction->UpdateProgress( cylMax-lp.cylinder );
		}
		return LOG_ERROR( pAction->TerminateWithError(ERROR_REQUEST_REFUSED) );
	}
	UINT AFX_CDECL CFDD::DetermineControllerAndOneByteLatency_thread(PVOID pCancelableAction){
		// thread to automatically determine the controller and one Byte write latencies
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		TLatencyParams &lp=*(TLatencyParams *)pAction->GetParams();
		pAction->SetProgressTarget( lp.nRepeats*2 ); // 2 = number of steps of a single trial
		// - defining the Interruption
		struct TInterruption sealed{
		private:
			const PBackgroundActionCancelable pAction;
			CFDD *const fdd;
			const TCylinder &rCyl;
			const THead &rHead;
			const PVOID sectorDataToWrite;
			const WORD nsAccuracy; // nanoseconds
		public:
			const TSectorId sectorId;
			const WORD sectorLength;
			TLogTime nNanoseconds;

			TInterruption(const PBackgroundActionCancelable pAction,const TLatencyParams &lp)
				// ctor
				: pAction(pAction)
				, fdd(lp.fdd)
				, sectorId(lp.sectorId) , sectorLength(lp.fdd->GetOfficialSectorLength(lp.sectorId.lengthCode))
				, rCyl(lp.cylinder) , rHead(lp.head) // a healthy Track
				, sectorDataToWrite( ::VirtualAlloc(nullptr,SECTOR_LENGTH_MAX,MEM_COMMIT,PAGE_READWRITE) )
				, nsAccuracy(lp.nsAccuracy) , nNanoseconds(0) {
				::memset( sectorDataToWrite, TEST_BYTE, sectorLength );
			}
			~TInterruption(){
				// dtor
				::VirtualFree(sectorDataToWrite,0,MEM_RELEASE);
			}

			TStdWinError __writeSectorData__(WORD nBytesToWrite) const{
				// writes SectorData to the current Track and interrupts the controller after specified NumberOfBytesToWrite and Nanoseconds; returns Windows standard i/o error
				// : setting controller interruption to the specified NumberOfBytesToWrite and Nanoseconds
				if (const TStdWinError err=fdd->__setTimeBeforeInterruptingTheFdc__( nBytesToWrite, nNanoseconds ))
					return err;
				// : writing
				DWORD nBytesTransferred;
				switch (fdd->DRIVER){
					case DRV_FDRAWCMD:{
						FD_READ_WRITE_PARAMS rwp={ FD_OPTION_MFM, rHead, sectorId.cylinder,sectorId.side,sectorId.sector,sectorId.lengthCode, sectorId.sector+1, FDD_350_SECTOR_GAP3/2, sectorId.lengthCode?0xff:0x80 };
						return	::DeviceIoControl( fdd->_HANDLE, IOCTL_FDCMD_WRITE_DATA, &rwp,sizeof(rwp), sectorDataToWrite,sectorLength, &nBytesTransferred, nullptr )!=0
								? ERROR_SUCCESS
								: LOG_ERROR(::GetLastError());
					}
					default:
						ASSERT(FALSE);
						return LOG_ERROR(ERROR_NOT_SUPPORTED);
				}
			}
			WORD __getNumberOfWrittenBytes__() const{
				// counts and returns the number of TestBytes actually written in the most recent call to __writeSectorData__
				PCBYTE p=(PCBYTE)fdd->dataBuffer;
				for( fdd->__bufferSectorData__(rCyl,rHead,&sectorId,sectorLength,&TInternalTrack(fdd,rCyl,rHead,Codec::MFM,1,&sectorId,(PCLogTime)FDD_350_SECTOR_GAP3),0,&TFdcStatus()); *p==TEST_BYTE; p++ );
				return p-(PCBYTE)fdd->dataBuffer;
			}
			TStdWinError __setInterruptionToWriteSpecifiedNumberOfBytes__(WORD nBytes){
				// sets this Interruption so that the specified NumberOfBytes is written to current Track; returns Windows standard i/o error
				// : initialization using the default NumberOfNanoseconds
				nNanoseconds=TIME_MICRO(20);
				// : increasing the NumberOfNanoseconds until the specified NumberOfBytes is written for the first time
				do{
					if (pAction->Cancelled) return ERROR_CANCELLED;
					nNanoseconds+=nsAccuracy;
					if (const TStdWinError err=__writeSectorData__(nBytes))
						return err;
				}while (__getNumberOfWrittenBytes__()<nBytes);
				const TLogTime nNanosecondsA=nNanoseconds;
				// : increasing the NumberOfNanoseconds until a higher NumberOfBytes is written for the first time
				do{
					if (pAction->Cancelled) return ERROR_CANCELLED;
					nNanoseconds+=nsAccuracy;
					if (const TStdWinError err=__writeSectorData__(nBytes))
						return err;
				}while (__getNumberOfWrittenBytes__()<=nBytes);
				// : the resulting NumberOfNanoseconds is the average of when the NumberOfBytes has been written for the first and last time
				nNanoseconds=(nNanosecondsA+nNanoseconds)/2;
				return ERROR_SUCCESS;
			}
		} interruption( pAction, lp );
		// - testing
		EXCLUSIVELY_LOCK_IMAGE(*lp.fdd); // locking the access so that no one can disturb during the testing
		if (!lp.fdd->fddHead.__seekTo__(lp.cylinder)) // seeking Head to the particular healthy Track
			return LOG_ERROR(pAction->TerminateWithError(ERROR_REQUEST_REFUSED));
		for( BYTE c=lp.nRepeats,state=0; c--; ){
			// . STEP 1: experimentally determining the ControllerLatency
			if (pAction->Cancelled) return LOG_ERROR(ERROR_CANCELLED);
			const WORD nBytes=interruption.sectorLength/2;
			if (const TStdWinError err=interruption.__setInterruptionToWriteSpecifiedNumberOfBytes__(nBytes))
				return LOG_ERROR(pAction->TerminateWithError(err));
			const TLogTime nControllerNanoseconds=interruption.nNanoseconds;
			lp.outControllerLatency+=nControllerNanoseconds; // below divided by the number of attempts to get an average
/*
{TCHAR buf[80];
::wsprintf(buf,_T("nMicrosecondsA=%d"),(int)(nControllerMicroseconds*1000));
Utils::Information(buf);}
//*/
			pAction->UpdateProgress(++state);
			// . STEP 2: experimentally determining the latency of one Byte
			if (pAction->Cancelled) return LOG_ERROR(ERROR_CANCELLED);
			const TLogTime p = interruption.nNanoseconds = TIME_MILLI(65); // let's see how many Bytes are written during the 65 millisecond time frame
			if (const TStdWinError err=interruption.__writeSectorData__(nBytes))
				return LOG_ERROR(pAction->TerminateWithError(err));
			const int n=interruption.__getNumberOfWrittenBytes__();
			const TLogTime nNanosecondsPerByte=( p - nControllerNanoseconds ) / ( n - nBytes );
/*
{TCHAR buf[80];
::wsprintf(buf,_T("oneByteLatency=%d"),(int)(nMicrosecondsPerByte*1000));
Utils::Information(buf);}
//*/
			//lp.out1ByteLatency+=( n - sectorLength ) / ( p - nMicrosecondsZ );
			lp.out1ByteLatency+=nNanosecondsPerByte; // below divided by the number of attempts to get an average
			pAction->UpdateProgress(++state);
		}
		// - computing the final latency values
		lp.outControllerLatency/=lp.nRepeats, lp.out1ByteLatency/=lp.nRepeats;
		return ERROR_SUCCESS;
	}
	UINT AFX_CDECL CFDD::DetermineGap3Latency_thread(PVOID pCancelableAction){
		// thread to automatically determine the Gap3 latency
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		TLatencyParams &lp=*(TLatencyParams *)pAction->GetParams();
		const BYTE targetGap3= lp.fdd->floppyType==Medium::FLOPPY_DD_525 ? FDD_525_SECTOR_GAP3 : FDD_350_SECTOR_GAP3;
		pAction->SetProgressTarget( targetGap3 );
		EXCLUSIVELY_LOCK_IMAGE(*lp.fdd); // locking the access so that no one can disturb during the testing
		if (!lp.fdd->fddHead.__seekTo__(lp.cylinder)) // seeking Head to the particular healthy Track
			return LOG_ERROR(pAction->TerminateWithError(ERROR_REQUEST_REFUSED));
		for( BYTE gap3=1; gap3<targetGap3; pAction->UpdateProgress(gap3+=3) ){
			if (pAction->Cancelled) return LOG_ERROR(ERROR_CANCELLED);
			// . STEP 1: writing two test Sectors
			static constexpr TSectorId SectorIds[]={ {1,0,1,2}, {1,0,2,2} };
			static constexpr WORD SectorLengths[]={ 512, 512 };
			static const TFdcStatus SectorStatuses[]={ TFdcStatus::WithoutError, TFdcStatus::WithoutError };
	{		const Utils::CVarTempReset<bool> vft0( lp.fdd->params.verifyFormattedTracks, false );
			if (const TStdWinError err=lp.fdd->FormatTrack( lp.cylinder, lp.head, Codec::MFM, 2, SectorIds, SectorLengths, SectorStatuses, gap3, TEST_BYTE, pAction->Cancelled ))
				return pAction->TerminateWithError(err);
	}		// . STEP 2: reading the Sectors
			BYTE c=0;
			while (c<lp.nRepeats){
				if (pAction->Cancelled) return LOG_ERROR(ERROR_CANCELLED);
				// . STEP 2.1: scanning the Track and seeing how distant the two test Sectors are on it
				lp.fdd->UnformatInternalTrack(lp.cylinder,lp.head); // disposing internal information on actual Track format
				const TInternalTrack *const pit=lp.fdd->__scanTrack__(lp.cylinder,lp.head);
				// : STEP 2.2: reading the first formatted Sector
				WORD w;
				lp.fdd->GetHealthySectorData( lp.cylinder, lp.head, &SectorIds[0], &w );
				// : STEP 2.3: Reading the second formatted Sector and measuring how long the reading took
				const Utils::CRideTime startTime;
					lp.fdd->GetHealthySectorData( lp.cylinder, lp.head, &SectorIds[1], &w );
				const Utils::CRideTime endTime;
				const TLogTime deltaNanoseconds=TIME_MILLI( (endTime-startTime).ToMilliseconds() );
				// . STEP 2.4: determining if the readings took more than just one disk revolution or more
				if (deltaNanoseconds>=pit->sectors[1].endNanoseconds-pit->sectors[0].endNanoseconds+TIME_MILLI(4)) // 4e6 = allowing circa 120 Bytes as a limit of detecting a single disk revolution
					break;
				c++;
			}
			if (c==lp.nRepeats){
				// both Sectors were successfully read in a single disk revolution in all N repeats
				lp.outGap3Latency=gap3*lp.out1ByteLatency+lp.outControllerLatency; // "+N" = just to be sure the correct minimum Gap3 has been found
				return pAction->TerminateWithSuccess();
			}
		}
		lp.outGap3Latency=targetGap3*lp.out1ByteLatency;
		return ERROR_SUCCESS;
	}

	bool CFDD::EditSettings(bool initialEditing){
		// True <=> new settings have been accepted (and adopted by this Image), otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - defining the Dialog
		class CSettingDialog sealed:public Utils::CRideDialog{
			const bool initialEditing;
			CFDD *const fdd;
			TCHAR doubleTrackDistanceTextOrg[80];

			bool IsDoubleTrackDistanceForcedByUser() const{
				// True <=> user has manually overridden DoubleTrackDistance setting, otherwise False
				return ::lstrlen(doubleTrackDistanceTextOrg)!=::GetWindowTextLength( GetDlgItemHwnd(ID_40D80) );
			}

			void __refreshMediumInformation__(){
				// detects a floppy in the Drive and attempts to recognize its Type
				// . making sure that a floppy is in the Drive
				ShowDlgItem( ID_INFORMATION, false );
				fdd->floppyType=Medium::UNKNOWN; // assumption (floppy not inserted or not recognized)
				static constexpr WORD Interactivity[]={ ID_LATENCY, ID_NUMBER2, ID_GAP, 0 };
				if (!EnableDlgItems( Interactivity, fdd->GetInsertedMediumType(0,fdd->floppyType)==ERROR_SUCCESS ))
					SetDlgItemText( ID_MEDIUM, _T("Not inserted") );
				// . attempting to recognize any previous format on the floppy
				else
					switch (fdd->floppyType){
						case Medium::FLOPPY_DD_525:
							SetDlgItemText( ID_MEDIUM, _T("5.25\" DD formatted, 360 RPM drive") );
							if (EnableDlgItem( ID_40D80, initialEditing )){
								fdd->fddHead.SeekHome();
								const Utils::CVarTempReset<bool> dts0( fdd->fddHead.doubleTrackStep, false );
								const Utils::CVarTempReset<PInternalTrack> pit0( fdd->internalTracks[1][0], nullptr ); // forcing new scan
								if (const PInternalTrack pit=fdd->__scanTrack__(1,0))
									CheckDlgButton( ID_40D80, !ShowDlgItem(ID_INFORMATION,pit->nSectors>0) );
								fdd->UnformatInternalTrack(1,0);
								fdd->fddHead.SeekHome();
							}
							break;
						case Medium::FLOPPY_DD:
							SetDlgItemText( ID_MEDIUM, _T("3.5\"/5.25\" DD formatted, 300 RPM drive") );
							CheckDlgButton( ID_40D80, false );
							EnableDlgItem( ID_40D80, initialEditing );
							break;
						case Medium::FLOPPY_HD_350:
							SetDlgItemText( ID_MEDIUM, _T("3.5\" HD formatted") );
							CheckDlgButton( ID_40D80, false );
							EnableDlgItem( ID_40D80, initialEditing );
							break;
						case Medium::FLOPPY_HD_525:
							SetDlgItemText( ID_MEDIUM, _T("5.25\" HD formatted") );
							CheckDlgButton( ID_40D80, false );
							EnableDlgItem( ID_40D80, initialEditing );
							break;
						default:
							SetDlgItemText( ID_MEDIUM, _T("Not formatted or faulty") );
							CheckDlgButton( ID_40D80, false );
							EnableDlgItem( ID_40D80, initialEditing );
							break;
					}
				// . loading the Profile associated with the current drive and FloppyType
				const RECT rcWarning=MapDlgItemClientRect(ID_INSTRUCTION);
				RECT rcMessage=MapDlgItemClientRect(ID_AUTO);
				if (ShowDlgItem(  ID_INSTRUCTION,  !profile.Load( fdd->GetDriveLetter(), fdd->floppyType, fdd->EstimateNanosecondsPerOneByte() )  ))
					rcMessage.left=rcWarning.right;
				else
					rcMessage.left=rcWarning.left;
				SetDlgItemPos( ID_AUTO, rcMessage );
				__exchangeLatency__( &CDataExchange(this,FALSE) );
				// . forcing redrawing (as the new text may be shorter than the original text, leaving the original partly visible)
				GetDlgItem(ID_MEDIUM)->Invalidate();
			}
			void PreInitDialog() override{
				// dialog initialization
				// . base
				__super::PreInitDialog();
				// . displaying controller information
				SetDlgItemText( ID_SYSTEM, fdd->__getControllerType__() );
				// . if DoubleTrackStep changed manually, adjusting the text of the ID_40D80 checkbox
				GetDlgItemText( ID_40D80,  doubleTrackDistanceTextOrg, sizeof(doubleTrackDistanceTextOrg)/sizeof(TCHAR) );
				if (fdd->fddHead.userForcedDoubleTrackStep)
					WindowProc( WM_COMMAND, ID_40D80, 0 );
				CheckDlgButton( ID_40D80, fdd->fddHead.doubleTrackStep );
				// . some settings are changeable only during InitialEditing
				EnableDlgItem( ID_READABLE, params.calibrationAfterError!=TParams::TCalibrationAfterError::NONE );
				// . displaying inserted Medium information
				SetDlgItemSingleCharUsingFont( // a warning that a 40-track disk might have been misrecognized
					ID_INFORMATION,
					L'\xf0ea', (HFONT)Utils::CRideFont(FONT_WEBDINGS,175,false,true).Detach()
				);
				SetDlgItemSingleCharUsingFont( // a warning that pre-compensation not up-to-date
					ID_INSTRUCTION,
					L'\xf0ea', (HFONT)Utils::CRideFont(FONT_WEBDINGS,175,false,true).Detach()
				);
				__refreshMediumInformation__();
				// . adjusting calibration possibilities
				extern CDos::PCProperties manuallyForceDos;
				if (EnableDlgItem( ID_READABLE,
						!fdd->dos && manuallyForceDos!=&CUnknownDos::Properties // DOS now yet known: either automatic DOS recognition, or manual selection of DOS but Unknown
						||
						fdd->dos && fdd->dos->properties!=&CUnknownDos::Properties // DOS already known: it's NOT the Unknown DOS
					)
				)
					CheckDlgButton( ID_READABLE, false ); // this option is never ticked for Unknown DOS
			}
			void __exchangeLatency__(CDataExchange* pDX){
				// exchange of latency-related data from and to controls
				float tmp=profile.controllerLatency/1e3f;
					DDX_Text( pDX,	ID_LATENCY,	tmp );
				profile.controllerLatency=tmp*1e3f;
				tmp=profile.oneByteLatency/1e3f;
					DDX_Text( pDX,	ID_NUMBER2,	tmp );
				profile.oneByteLatency=tmp*1e3f;
				tmp=profile.gap3Latency/1e3f;
					DDX_Text( pDX,	ID_GAP,		tmp );
				profile.gap3Latency=tmp*1e3f;
			}
			void DoDataExchange(CDataExchange* pDX) override{
				// exchange of data from and to controls
				// . latency values
				__exchangeLatency__(pDX);
				// . CalibrationAfterError
				int tmp=params.calibrationAfterError;
				DDX_Radio( pDX,	ID_NONE,		tmp );
				params.calibrationAfterError=(TParams::TCalibrationAfterError)tmp;
				tmp=params.calibrationAfterErrorOnlyForKnownSectors;
				DDX_Check( pDX, ID_READABLE,	tmp );
				params.calibrationAfterErrorOnlyForKnownSectors=tmp!=0;
				// . CalibrationStepDuringFormatting
				EnableDlgItem( ID_NUMBER, tmp=params.calibrationStepDuringFormatting!=0 );
				DDX_Radio( pDX,	ID_ZERO,		tmp );
				if (tmp)
					DDX_Text( pDX,	ID_NUMBER,	tmp=params.calibrationStepDuringFormatting );
				else
					SetDlgItemInt(ID_NUMBER,4,FALSE);
				params.calibrationStepDuringFormatting=tmp;
				// . NumberOfSecondsToTurnMotorOff
				tmp=params.nSecondsToTurnMotorOff;
				DDX_CBIndex( pDX, ID_ROTATION,	tmp );
				params.nSecondsToTurnMotorOff=tmp;
				// . FormattedTracksVerification
				tmp=params.verifyFormattedTracks;
				DDX_Check( pDX,	ID_VERIFY_TRACK,	tmp );
				params.verifyFormattedTracks=tmp!=0;
				// . WrittenDataVerification
				tmp=params.verifyWrittenData;
				DDX_Check( pDX,	ID_VERIFY_SECTOR,	tmp );
				params.verifyWrittenData=tmp!=0;
				// . RelativeSeekingVerification
				tmp=fdd->fddHead.preferRelativeSeeking;
				DDX_Check( pDX,	ID_SEEK,	tmp );
				fdd->fddHead.preferRelativeSeeking=tmp!=0;
			}
			afx_msg void OnPaint(){
				// drawing
				// - base
				__super::OnPaint();
				// - drawing of curly brackets
				WrapDlgItemsByClosingCurlyBracketWithText( ID_LATENCY, ID_GAP, nullptr, 0 );
				WrapDlgItemsByClosingCurlyBracketWithText( ID_NONE, ID_READABLE, _T("on read error"), 0 );
				WrapDlgItemsByClosingCurlyBracketWithText( ID_ZERO, ID_CYLINDER_N, _T("when formatting"), 0 );
			}
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				switch (msg){
					case WM_PAINT:
						OnPaint();
						return 0;
					case WM_NOTIFY:
						if (wParam==ID_AUTO && ((LPNMHDR)lParam)->code==NM_CLICK){
autodetermineLatencies:		// automatic determination of write latency values
							// . defining the Dialog
							class CLatencyAutoDeterminationDialog sealed:public Utils::CRideDialog{
							public:
								int floppyType,usAccuracy,nRepeats;

								CLatencyAutoDeterminationDialog(const CFDD *fdd,CWnd *parent)
									: Utils::CRideDialog(IDR_FDD_LATENCY,parent) , floppyType(0) , usAccuracy(2) , nRepeats(3) {
									switch (fdd->floppyType){
										case Medium::FLOPPY_DD: floppyType=0; break;
										case Medium::FLOPPY_DD_525: floppyType=1; break;
										case Medium::FLOPPY_HD_525: floppyType=2; break;
										case Medium::FLOPPY_HD_350: floppyType=3; break;
									}
								}
								void PreInitDialog() override{
									__super::PreInitDialog(); // base
									PopulateDlgComboBoxWithSequenceOfNumbers( ID_ACCURACY, 1,_T("(highest)"), 6,_T("(lowest)") );
									PopulateDlgComboBoxWithSequenceOfNumbers( ID_TEST, 1,_T("(worst)"), 9,_T("(best)") );
								}
								void DoDataExchange(CDataExchange *pDX) override{
									DDX_CBIndex( pDX, ID_MEDIUM,	floppyType );
									DDX_CBIndex( pDX, ID_ACCURACY,	usAccuracy );
									DDX_CBIndex( pDX, ID_TEST,		nRepeats );
								}
							} d(fdd,this);
							// . showing the Dialog and processing its result
							if (d.DoModal()==IDOK){
								__informationWithCheckableShowNoMore__( _T("Windows is NOT a real-time system! Computed latency will be valid only if using the floppy drive in very similar conditions as when they were computed (current conditions)!"), INI_MSG_LATENCY );
								if (Utils::InformationOkCancel(_T("Insert an empty disk that you don't mind writing to, and hit OK."))){
									// : composing a parallel multi-action
									CBackgroundMultiActionCancelable bmac( THREAD_PRIORITY_TIME_CRITICAL );
										Medium::TType floppyType=Medium::UNKNOWN;
										switch (d.floppyType){
											case 0: floppyType=Medium::FLOPPY_DD; break;
											case 1: floppyType=Medium::FLOPPY_DD_525; break;
											case 2: floppyType=Medium::FLOPPY_HD_525; break;
											case 3: floppyType=Medium::FLOPPY_HD_350; break;
										}
										TLatencyParams lp( fdd, TIME_MICRO(1+d.usAccuracy), 1+d.nRepeats );
										bmac.AddAction( FindHealthyTrack_thread, &lp, _T("Searching for healthy track") );
										bmac.AddAction( DetermineControllerAndOneByteLatency_thread, &lp, _T("Determining controller latencies") );
										bmac.AddAction( DetermineGap3Latency_thread, &lp, _T("Determining minimal Gap3 size") );
									// : backing up existing InternalTracks and performing the multi-action
									BYTE internalTracksOrg[sizeof(fdd->internalTracks)];
									::memcpy( internalTracksOrg, fdd->internalTracks, sizeof(internalTracksOrg) );
									::ZeroMemory( fdd->internalTracks, sizeof(internalTracksOrg) );
										const auto floppyTypeOrg=fdd->floppyType;
										fdd->SetDataTransferSpeed( fdd->floppyType=floppyType ); // setting transfer speed according to selected FloppyType
											fdd->locker.Unlock(); // giving way to parallel thread
												const TStdWinError err=bmac.Perform();
											fdd->locker.Lock();
											fdd->__freeInternalTracks__();
										fdd->SetDataTransferSpeed( fdd->floppyType=floppyTypeOrg ); // reverting to original FloppyType
									::memcpy( fdd->internalTracks, internalTracksOrg, sizeof(internalTracksOrg) );
									// : reporting on problems and quitting
									if (err){
										Utils::FatalError(_T("Couldn't autodetermine"),err);
										break;
									}
									// : saving determined latencies to corresponding floppy Profile
									TFddHead::TProfile tmp;
										tmp.Load( fdd->GetDriveLetter(), floppyType, FDD_NANOSECONDS_PER_DD_BYTE );
											tmp.controllerLatency=lp.outControllerLatency;
											tmp.oneByteLatency=lp.out1ByteLatency;
											tmp.gap3Latency=lp.outGap3Latency;
										tmp.Save( fdd->GetDriveLetter(), floppyType );
										TCHAR iniSection[16];
										GetFddProfileName( iniSection, fdd->GetDriveLetter(), floppyType );
										app.WriteProfileInt( iniSection, INI_LATENCY_DETERMINED, TRUE ); // latencies hereby at least once determined
									// : adopting the found latencies to current floppy Profile
									if (floppyType==fdd->floppyType){
										fdd->fddHead.profile = profile = tmp;
										__exchangeLatency__( &CDataExchange(this,FALSE) );
									}
								}
							}
						}
						break;
					case WM_COMMAND:
						switch (wParam){
							case ID_RECOVER:
								// refreshing information on (inserted) floppy
								if (initialEditing) // if no Tracks are yet formatted ...
									SetDlgItemText( ID_40D80, doubleTrackDistanceTextOrg ); // ... then resetting the flag that user has overridden DoubleTrackDistance
								__refreshMediumInformation__();
								break;
							case ID_40D80:{
								// track distance changed manually
								TCHAR buf[sizeof(doubleTrackDistanceTextOrg)/sizeof(TCHAR)+20];
								SetDlgItemText( ID_40D80, ::lstrcat(::lstrcpy(buf,doubleTrackDistanceTextOrg),_T(" (user forced)")) );
								ShowDlgItem( ID_INFORMATION, false ); // user manually revised the Track distance, so no need to continue display the warning
								break;
							}
							case ID_NONE:
							case ID_CYLINDER:
							case ID_SECTOR:
								// adjusting possibility to edit controls that depend on CalibrationAfterError
								EnableDlgItem( ID_READABLE, wParam!=ID_NONE );
								break;
							case ID_ZERO:
							case ID_CYLINDER_N:
								// adjusting possibility to edit the CalibrationStep according to selected option
								EnableDlgItem( ID_NUMBER, wParam!=ID_ZERO );
								break;
							case IDOK:
								// attempting to confirm the Dialog
								fdd->fddHead.doubleTrackStep=IsDlgButtonChecked( ID_40D80 )!=BST_UNCHECKED;
								fdd->fddHead.userForcedDoubleTrackStep=IsDoubleTrackDistanceForcedByUser();
								TCHAR iniSection[16];
								GetFddProfileName( iniSection, fdd->GetDriveLetter(), fdd->floppyType );
								if (!app.GetProfileInt( iniSection, INI_LATENCY_DETERMINED, FALSE ))
									switch (Utils::QuestionYesNoCancel(_T("Latencies not yet determined, I/O operations may perform suboptimal.\n\nAutodetermine latencies now?"),MB_DEFBUTTON1)){
										case IDYES:
											msg=WM_PAINT; // changing the Message to one that won't close the Dialog
											goto autodetermineLatencies;
										case IDCANCEL:
											Utils::Information(_T("Okay, won't bother you again. Please click on the \"Autodetermine\" button later - it pays off!"));
											app.WriteProfileInt( iniSection, INI_LATENCY_DETERMINED, TRUE ); // pretend that latencies hereby determined
											break;
									}
								break;
						}
						break;
				}
				return __super::WindowProc(msg,wParam,lParam);
			}
		public:
			TParams params;
			TFddHead::TProfile profile;

			CSettingDialog(CFDD *_fdd,bool initialEditing)
				// ctor
				: Utils::CRideDialog(IDR_FDD_ACCESS) , fdd(_fdd) , params(_fdd->params) , profile(_fdd->fddHead.profile) , initialEditing(initialEditing) {
			}
		} d(this,initialEditing);
		// - showing the Dialog and processing its result
		LOG_DIALOG_DISPLAY(_T("CSettingDialog"));
		const auto floppyTypeOrg=floppyType;
			const bool dialogConfirmed=d.DoModal()==IDOK;
		SetDataTransferSpeed( floppyType=floppyTypeOrg ); // reverting to original FloppyType, should it be changed in the Dialog
		if (dialogConfirmed){
			params=d.params;
			__setSecondsBeforeTurningMotorOff__(params.nSecondsToTurnMotorOff);
			__informationWithCheckableShowNoMore__( _T("To spare both floppy and drive, all activity is buffered: CHANGES (WRITINGS, DELETIONS) MADE TO THE FLOPPY ARE SAVED ONLY WHEN YOU COMMAND SO (Ctrl+S). If you don't save them, they will NOT appear on the disk next time. FORMATTING DESTROYS THE CONTENT IMMEDIATELLY!"), INI_MSG_RESET );
			return true;
		}else
			return LOG_BOOL(false);
	}

	TStdWinError CFDD::Reset(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		// - resetting
		LOG_ACTION(_T("TStdWinError CFDD::Reset"));
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - disposing all InternalTracks
		__freeInternalTracks__();
		// - re-connecting to the Drive
		__disconnectFromFloppyDrive__();
		if (const TStdWinError err=__connectToFloppyDrive__(DRV_AUTO))
			return LOG_ERROR(err);
		// - sending Head home
		if (!fddHead.__calibrate__())
			return LOG_ERROR(::GetLastError());
		// - successfully reset
		return ERROR_SUCCESS;
	}

	static BYTE ReportSectorVerificationError(RCPhysicalAddress chs,const volatile bool &cancelled){
		TStdWinError err=::GetLastError();
		if (err==ERROR_SUCCESS) // if hardware itself reports no error ...
			err=ERROR_DS_COMPARE_FALSE; // ... then the data are simply wrongly written
		TCHAR buf[100],sug[480];
		::wsprintf(
			sug, _T("- Has the correct medium been set in the \"%s\" dialog?\n- For copy-protected schemes, simply retrying often helps."),
			Utils::CRideDialog::GetDialogTemplateCaptionText( IDR_DOS_FORMAT, buf, sizeof(buf)/sizeof(TCHAR) )
		);
		::wsprintf( buf, _T("Track %d verification failed for sector with %s"), chs.GetTrackNumber(2), (LPCTSTR)chs.sectorId.ToString() );
		return	cancelled
				? IDABORT
				: Utils::AbortRetryIgnore( buf, err, MB_DEFBUTTON2, sug );
	}

	TStdWinError CFDD::FormatToOneLongVerifiedSector(RCPhysicalAddress chs,BYTE fillerByte,const volatile bool &cancelled){
		// creates (and eventually verifies) a single long Sector with the given ID; returns Windows standard i/o error
		LOG_TRACK_ACTION(chs.cylinder,chs.head,_T("TStdWinError CFDD::__formatToOneLongVerifiedSector__"));
		do{
			// . navigating the Head above corresponding Cylinder
			if (!fddHead.__seekTo__(chs.cylinder))
				return LOG_ERROR(::GetLastError());
			// . formatting Track to a single Sector
			__setWaitingForIndex__();
			FD_FORMAT_PARAMS fmt={	FD_OPTION_MFM, chs.head,
									1, // the Sector length doesn't matter, the important thing is to correctly create its ID Field
									1, 1, fillerByte,
									TFdIdHeader(chs.sectorId)
								};
			DWORD nBytesTransferred;
			{	LOG_ACTION(_T("DeviceIoControl IOCTL_FDCMD_FORMAT_TRACK"));
				if (!::DeviceIoControl( _HANDLE, IOCTL_FDCMD_FORMAT_TRACK, &fmt,sizeof(fmt), nullptr,0, &nBytesTransferred, nullptr ))
					return LOG_ERROR(::GetLastError());
			}
			// . verifying the single formatted Sector
			if (params.verifyFormattedTracks){
				LOG_ACTION(_T("format verification"));
				// : writing FillerByte as test data
				const FD_ID_HEADER &rih=fmt.Headers[0];
				WORD sectorBytes=GetUsableSectorLength(rih.size);
				__setTimeBeforeInterruptingTheFdc__( sectorBytes, fddHead.profile.controllerLatency+1*fddHead.profile.oneByteLatency ); // "X*" = reserve to guarantee that really all test data written
				FD_READ_WRITE_PARAMS rwp={ FD_OPTION_MFM|FD_OPTION_SK, chs.head, rih.cyl,rih.head,rih.sector,rih.size, rih.sector+1, 1, 0xff };
				{	LOG_ACTION(_T("DeviceIoControl IOCTL_FDCMD_WRITE_DATA"));
					if (!::DeviceIoControl( _HANDLE, IOCTL_FDCMD_WRITE_DATA, &rwp,sizeof(rwp), ::memset(dataBuffer,fillerByte,sectorBytes),GetOfficialSectorLength(rih.size), &nBytesTransferred, nullptr ))
						return LOG_ERROR(::GetLastError());
				}
				// . reading test data
				{	LOG_ACTION(_T("DeviceIoControl IOCTL_FDCMD_READ_DATA"));
					::DeviceIoControl( _HANDLE, IOCTL_FDCMD_READ_DATA, &rwp,sizeof(rwp), dataBuffer,SECTOR_LENGTH_MAX, &nBytesTransferred, nullptr ); // cannot use IF as test data always read with a CRC error in this case
				}
				for( PCBYTE p=(PCBYTE)dataBuffer; sectorBytes && *p++==fillerByte; sectorBytes-- );
				if (sectorBytes) // error reading the test data
					if (::GetLastError()==ERROR_FLOPPY_ID_MARK_NOT_FOUND) // this is an error for which it usually suffices ...
						continue; // ... to repeat the formatting cycle
					else
						switch (ReportSectorVerificationError(chs,cancelled)){
							case IDABORT:	return LOG_ERROR(ERROR_CANCELLED);
							case IDRETRY:	continue;
							case IDIGNORE:	break;
						}
			}else
				// if verification turned off, assuming well formatted Track structure, hence avoiding the need of its scanning
				internalTracks[chs.cylinder][chs.head] = new TInternalTrack( this, chs.cylinder, chs.head, Codec::MFM, 1, &chs.sectorId, (PCLogTime)FDD_350_SECTOR_GAP3 ); // Gap3 = calculate Sector start times from information of this Gap3 and individual Sector lengths
			// . Track formatted successfully
			break;
		}while (true);
		return ERROR_SUCCESS;
	}

	static bool __mustSectorBeFormattedIndividually__(PCFdcStatus sr){
		// True <=> Sector with provided i/o errors cannot be formatted in a sequence with other Sectors, otherwise False
		return sr->DescribesMissingDam() || sr->DescribesIdFieldCrcError();
	}

	typedef BYTE TIdAddressMark;

	#define SYNCHRONIZATION_BYTES_COUNT		12+3 /* 12x 0x00 + 3x 0xA1 */
	#define	GAP2_BYTES_COUNT				22	 /* 22x 0x4E */
	#define RESERVED_SECTOR_LENGTH_CODE		0

	#define NUMBER_OF_BYTES_OCCUPIED_BY_ID\
		( SYNCHRONIZATION_BYTES_COUNT + sizeof(TIdAddressMark) + sizeof(FD_ID_HEADER) + sizeof(TCrc16) )

	#define NUMBER_OF_OCCUPIED_BYTES(nSectors,sectorLength,gap3,withoutLastGap3)\
		( nSectors*( NUMBER_OF_BYTES_OCCUPIED_BY_ID + GAP2_BYTES_COUNT + SYNCHRONIZATION_BYTES_COUNT + sizeof(TDataAddressMark) + sectorLength + sizeof(TCrc16) + gap3 )  -  withoutLastGap3*gap3 )

	TStdWinError CFDD::FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte,const volatile bool &cancelled){
		// formats given Track {Cylinder,Head} to the requested NumberOfSectors, each with corresponding Length and FillerByte as initial content; returns Windows standard i/o error
		LOG_TRACK_ACTION(cyl,head,_T("TStdWinError CFDD::FormatTrack"));
		#ifdef LOGGING_ENABLED
			TCHAR formatTrackParams[200];
			::wsprintf(formatTrackParams,_T("Cyl=%d, Head=%d, codec=%d, nSectors=%d, gap3=%d, fillerByte=%d"),cyl,head,codec,nSectors,gap3,fillerByte);
			LOG_MESSAGE(formatTrackParams);
		#endif
		if (codec!=Codec::MFM)
			return LOG_ERROR(ERROR_NOT_SUPPORTED);
		if (nSectors>FDD_SECTORS_MAX)
			return LOG_ERROR(ERROR_BAD_COMMAND);
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (!nSectors) // formatting to zero Sectors ...
			return UnformatTrack(cyl,head); // ... defined as unformatting the Track
		// - Head calibration
		if (params.calibrationStepDuringFormatting)
			if (cyl%params.calibrationStepDuringFormatting==0 && !head)
				if (fddHead.__calibrate__())
					fddHead.calibrated=true;
				else
error:				return LOG_ERROR(::GetLastError());
		// - moving Head above the corresponding Cylinder
		if (!fddHead.__seekTo__(cyl))
			goto error;
		// - determining the FormatStyle used for formatting the Track
		enum TFormatStyle{
			STANDARD,
			ONE_LONG_SECTOR,
			CUSTOM
		} formatStyle;
		const BYTE referenceLengthCode=bufferId->lengthCode;
		if (nSectors==1 && referenceLengthCode>GetMaximumSectorLengthCode())
			formatStyle=TFormatStyle::ONE_LONG_SECTOR;
		else{
			formatStyle=TFormatStyle::STANDARD; // assumption
			const WORD referenceLength=*bufferLength;
			PCSectorId pid=bufferId; PCWORD pLength=bufferLength; PCFdcStatus pFdcStatus=bufferFdcStatus;
			for( BYTE n=nSectors; n--; pid++,pLength++,pFdcStatus++ )
				if (pid->lengthCode!=referenceLengthCode || *pLength!=referenceLength || !*pLength || !pFdcStatus->IsWithoutError() || *pLength!=GetOfficialSectorLength(pid->lengthCode)){
					formatStyle=TFormatStyle::CUSTOM;
					break;
				}
		}
		// - formatting using selected FormatStyle and eventually verifying the Track
		DWORD nBytesTransferred;
		switch (formatStyle){
			case TFormatStyle::STANDARD:{
				// . composing the structure
				LOG_MESSAGE(_T("TFormatStyle::STANDARD"));
				#pragma pack(1)
				struct{
					FD_FORMAT_PARAMS params;
					FD_ID_HEADER sectors[(TSector)-1];
				} fmt;
					fmt.params.flags=FD_OPTION_MFM;
					fmt.params.phead=head;
					fmt.params.size=referenceLengthCode;
					fmt.params.sectors=nSectors;
					fmt.params.gap=gap3;
					fmt.params.fill=fillerByte;
				PFD_ID_HEADER pih=fmt.params.Headers;	PCSectorId pId=bufferId;
				for( TSector n=nSectors; n--; *pih++=TFdIdHeader(*pId++) );
formatStandardWay:
				// . disposing internal information on actual Track format
				UnformatInternalTrack(cyl,head);
				// . formatting the Track
				{	LOG_ACTION(_T("DeviceIoControl IOCTL_FDCMD_FORMAT_TRACK"));
					if (!::DeviceIoControl( _HANDLE, IOCTL_FDCMD_FORMAT_TRACK, &fmt,sizeof(fmt), nullptr,0, &nBytesTransferred, nullptr ))
						goto error;
				}
				// . verifying the Track (if requested to)
				if (params.verifyFormattedTracks){
					LOG_ACTION(_T("track verification"));
					PVOID dummyBuffer[(TSector)-1];
					TFdcStatus statuses[(TSector)-1];
					GetTrackData( cyl, head, Revolution::ANY_GOOD, bufferId, Utils::CByteIdentity(), nSectors, (PSectorData *)dummyBuffer, (PWORD)dummyBuffer, statuses ); // "DummyBuffer" = throw away any outputs
					for( TSector n=0; n<nSectors; n++ ){
						const TPhysicalAddress chs={ cyl, head, bufferId[n] };
						if (!statuses[n].IsWithoutError())
							switch (ReportSectorVerificationError(chs,cancelled)){
								case IDABORT:	return LOG_ERROR(ERROR_CANCELLED);
								case IDRETRY:	goto formatStandardWay;
								case IDIGNORE:	break;
							}
					}
				}else
					PresumeHealthyTrackStructure(cyl,head,nSectors,bufferId,gap3,fillerByte);
				// . Track formatted successfully
				break;
			}
			case TFormatStyle::ONE_LONG_SECTOR:{
				// . disposing internal information on actual Track format
				LOG_MESSAGE(_T("TFormatStyle::ONE_LONG_SECTOR"));
				UnformatInternalTrack(cyl,head);
				// . formatting the Track
				const TPhysicalAddress chs={ cyl, head, *bufferId };
				if (FormatToOneLongVerifiedSector( chs, fillerByte, cancelled )!=ERROR_SUCCESS)
					goto error;
				// . Track formatted successfully
				break;
			}
			case TFormatStyle::CUSTOM:{
				// . disposing internal information on actual Track format
				LOG_MESSAGE(_T("TFormatStyle::CUSTOM"));
				UnformatInternalTrack(cyl,head);
				// . verifying Track surface (if requested to) by writing maximum number of known Bytes to it and trying to read them back
				if (params.verifyFormattedTracks){
					const TPhysicalAddress chs={ cyl, head, {0,0,0,GetMaximumSectorLengthCode()+1} };
					if (FormatToOneLongVerifiedSector( chs, fillerByte, cancelled )!=ERROR_SUCCESS)
						goto error;
				}
				// . creating the PlanOfFormatting
				#pragma pack(1)
				struct TFormatStep sealed{
					BYTE sectorLengthCode;
					BYTE nSectorsOnTrack, nLastSectorsValid;
					PCSectorId validSectorIDs;
					BYTE gap3;
					struct{
						WORD nBytes;
						TLogTime nNanoseconds;
					} interruption;

					WORD __getNumberOfNecessaryBytes__() const{
						// determines and returns the number of Bytes that are necessary to perform this Step
						return NUMBER_OF_OCCUPIED_BYTES(nSectorsOnTrack,GetOfficialSectorLength(sectorLengthCode),gap3,true);
					}
					void __debug__() const{
						TCHAR buf[200];
						::wsprintf(buf,_T("sectorLengthCode=%d\nnSectorsOnTrack=%d\nnLastSectorsValid=%d\ngap3=%d\n,nBytes=%d\nnNanoseconds=%d"),
							sectorLengthCode,nSectorsOnTrack,nLastSectorsValid,gap3,interruption.nBytes,interruption.nNanoseconds
						);
						Utils::Information(buf);
					}
				} formatPlan[(TSector)-1],*pFormatStep=formatPlan;
				WORD nBytesReserved; // reserved block of Bytes at the beginning of Track represents an area formatted in the next Step (as Sectors are formatted "backwards")
				BYTE n=nSectors;
				PCWORD pLength=bufferLength; PCFdcStatus pFdcStatus=bufferFdcStatus;
				if (!__mustSectorBeFormattedIndividually__(pFdcStatus)){
					// there is a sequence of Sectors at the beginning of Track that can be all formatted in a single Step
					pFormatStep->sectorLengthCode=GetSectorLengthCode(*pLength);
					pFormatStep->nLastSectorsValid=1;
					pFormatStep->validSectorIDs=bufferId;
					pFormatStep->gap3=gap3;
					for( const WORD refLength=*pLength; --n; pFormatStep->nLastSectorsValid++ ){
						const WORD length=*++pLength;
						const TFdcStatus sr=*++pFdcStatus;
						if (length!=refLength || __mustSectorBeFormattedIndividually__(&sr))
							break;
					}
					bufferId+=pFormatStep->nLastSectorsValid;
					pFormatStep->interruption.nBytes=( pFormatStep->nSectorsOnTrack=pFormatStep->nLastSectorsValid )*sizeof(FD_ID_HEADER);
					pFormatStep->interruption.nNanoseconds=fddHead.profile.controllerLatency+*bufferLength/2*fddHead.profile.oneByteLatency;
//pFormatStep->__debug__();
					nBytesReserved=pFormatStep++->__getNumberOfNecessaryBytes__();
				}else
					// right the first Sector in the Track must be formatted individually
					nBytesReserved=0;
				const WORD reservedSectorLength=GetOfficialSectorLength(RESERVED_SECTOR_LENGTH_CODE);
				while (n--){ // for each Sector that must be formatted individually
					// : treating an unprocessed Sector as a single-item sequence to be formatted in this Step
					const WORD sectorLength=*pLength++;
					pFormatStep->sectorLengthCode=RESERVED_SECTOR_LENGTH_CODE;
					pFormatStep->nLastSectorsValid=1;
					pFormatStep->validSectorIDs=bufferId;
					// : determining how many Sectors of prior known constant length fit in the Reserved area
					const BYTE nReservedSectors= (nBytesReserved+gap3) / NUMBER_OF_OCCUPIED_BYTES(1,reservedSectorLength,gap3,false);
					pFormatStep->interruption.nBytes=( pFormatStep->nSectorsOnTrack=nReservedSectors+pFormatStep->nLastSectorsValid )*sizeof(FD_ID_HEADER);
					// : computing DeltaGap3 that aids to "stretch" the above determined NumberOfReservedSectors across the whole length of Reserved area
//const WORD tmp=NUMBER_OF_OCCUPIED_BYTES(nReservedSectors,reservedSectorLength,gap3,false);
					const BYTE deltaGap3=	nReservedSectors
											?	( nBytesReserved+gap3 - NUMBER_OF_OCCUPIED_BYTES(nReservedSectors,reservedSectorLength,gap3,false) + nReservedSectors-1 )
												/
												nReservedSectors
											:	0;
/*
{TCHAR buf[80];
::wsprintf(buf,_T("sectorLength=%d\n\nz predchoziho kroku:\nnBytesReserved=%d, tmp=%d, deltaGap3=%d"),sectorLength,nBytesReserved,tmp,deltaGap3);
Utils::Information(buf);}
*/
					pFormatStep->gap3=gap3+deltaGap3;
					// : setting up the Interruption according to the requested i/o errors
					const TFdcStatus sr(*pFdcStatus++);
					WORD nBytesFormatted;
					if (sr.DescribesMissingDam()){
						// Sector misses its data part
						pFormatStep->interruption.nNanoseconds=fddHead.profile.controllerLatency+(GAP2_BYTES_COUNT+SYNCHRONIZATION_BYTES_COUNT-2)*fddHead.profile.oneByteLatency; // "-2" = damaging the A1A1A1 mark
						/*if (sr.DescribesIdFieldCrcError()){ // commented out as SamDisk doesn't reproduce this error neither [info by Simon Owen]
							// Sector misses its data part, and its ID Field is corrupted
							*(pFormatStep+1)=*pFormatStep; // the preceeding Step (as formatted "backwards") is creation of a correct ID Field
							pFormatStep++->interruption.nMicroseconds=params.controllerLatency+0.5*params.oneByteLatency; // the currect Step is creation of a corrupted ID Field; ".5*" = just to be sure
						}*/
						nBytesFormatted=gap3+NUMBER_OF_BYTES_OCCUPIED_BY_ID;
						const WORD minBytesNeeded=NUMBER_OF_OCCUPIED_BYTES(1,reservedSectorLength,gap3,true);
						if (nBytesReserved+nBytesFormatted<minBytesNeeded)
							nBytesFormatted=minBytesNeeded-nBytesReserved;
						//if (!nReservedSectors) pFormatStep->gap3=gap3+nBytesReserved;
					}else{
						// Sector is complete
						pFormatStep->interruption.nNanoseconds=fddHead.profile.controllerLatency+sectorLength/2*fddHead.profile.oneByteLatency;
						/*if (sr.DescribesIdFieldCrcError()){ // commented out as SamDisk doesn't reproduce this error neither [info by Simon Owen]
							// Sector misses its data part, and its ID Field is corrupted
							*(pFormatStep+1)=*pFormatStep; // the preceeding Step (as formatted "backwards") is creation of a correct ID Field
							pFormatStep++->interruption.nMicroseconds=params.controllerLatency+0.5*params.oneByteLatency; // the currect Step is creation of a corrupted ID Field; ".5*" = just to be sure
						}*/
						nBytesFormatted=NUMBER_OF_OCCUPIED_BYTES(1,sectorLength,gap3,false);
					}
					bufferId+=pFormatStep->nLastSectorsValid;
					// : next Sector
					nBytesReserved+=nBytesFormatted;
//pFormatStep->__debug__();
					pFormatStep++;
//{TCHAR buf[80];
//::wsprintf(buf,_T("nBytesReserved=%d"),nBytesReserved);
//Utils::Information(buf);}
				}
				bufferId-=nSectors; // recovering variable's original value
formatCustomWay:
				// . unformatting the Track
				UnformatTrack(cyl,head);
				// . executing the PlanOfFormatting - decremental formatting (i.e. formatting "backwards")
				for( const TFormatStep *pfs=pFormatStep; pfs-->formatPlan; ){
					if (cancelled)
						return LOG_ERROR(ERROR_CANCELLED);
					__setWaitingForIndex__();
					// : setting shortened formatting
					__setTimeBeforeInterruptingTheFdc__( pfs->interruption.nBytes, pfs->interruption.nNanoseconds );
					// : formatting
					#pragma pack(1)
					struct{
						FD_FORMAT_PARAMS fp;
						FD_ID_HEADER pole[(TSector)-1];
					} buffer;
						buffer.fp.flags=FD_OPTION_MFM;
						buffer.fp.phead=head;
						buffer.fp.size=pfs->sectorLengthCode;
						buffer.fp.sectors=pfs->nSectorsOnTrack;
						buffer.fp.gap=pfs->gap3;
						buffer.fp.fill=fillerByte;
						for( BYTE n=0; n<pfs->nLastSectorsValid; n++ )
							buffer.fp.Headers[pfs->nSectorsOnTrack-pfs->nLastSectorsValid+n]=TFdIdHeader( pfs->validSectorIDs[n] );
					LOG_ACTION(_T("DeviceIoControl IOCTL_FDCMD_FORMAT_TRACK"));
					if (!::DeviceIoControl( _HANDLE, IOCTL_FDCMD_FORMAT_TRACK, &buffer,sizeof(buffer), nullptr,0, &nBytesTransferred, nullptr ))
						return LOG_ERROR(::GetLastError());
				}
				// . verifying the Track (if requested to)
				if (params.verifyFormattedTracks){
					LOG_ACTION(_T("track verification"));
					const TInternalTrack it( this, cyl, head, Codec::MFM, nSectors, bufferId, nullptr ); // nullptr = calculate Sector start times from information on default Gap3 and individual Sector lengths
					for( TSector n=0; n<nSectors; n++ ){
						const TPhysicalAddress chs={ cyl, head, bufferId[n] };
						TFdcStatus sr;
						__bufferSectorData__( chs, bufferLength[n], &it, n, &sr );
						if (bufferFdcStatus[n].DescribesMissingDam()^sr.DescribesMissingDam())
							switch (ReportSectorVerificationError(chs,cancelled)){
								case IDABORT:	return LOG_ERROR(ERROR_CANCELLED);
								case IDRETRY:	goto formatCustomWay;
								case IDIGNORE:	break;
							}
					}
				}else
					// if verification turned off, assuming well formatted Track structure, hence avoiding the need of its scanning
					internalTracks[cyl][head] = new TInternalTrack( this, cyl, head, Codec::MFM, nSectors, bufferId, (PCLogTime)gap3 ); // Gap3 = calculate Sector start times from information of this Gap3 and individual Sector lengths
				// . Track formatted successfully
//Utils::Information("formatted OK - ready to break");
				break;
			}
		}
		// - it's not necessary to calibrate the Head for this Track
		fddHead.calibrated=true;
		return ERROR_SUCCESS;
	}

	bool CFDD::RequiresFormattedTracksVerification() const{
		// True <=> the Image requires its newly formatted Tracks be verified, otherwise False (and caller doesn't have to carry out verification)
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return params.verifyFormattedTracks;
	}

	TStdWinError CFDD::PresumeHealthyTrackStructure(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,BYTE gap3,BYTE fillerByte){
		// without formatting it, presumes that given Track contains specified Sectors that are well readable and writeable; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		LOG_TRACK_ACTION(cyl,head,_T("TStdWinError CFDD::PresumeHealthyTrackStructure"));
		// - disposing internal information on actual Track format
		UnformatInternalTrack(cyl,head);
		// - explicitly setting Track structure
		TInternalTrack::TSectorInfo *psi=( internalTracks[cyl][head] = new TInternalTrack( this, cyl, head, Codec::MFM, nSectors, bufferId, (PCLogTime)gap3 ) )->sectors; // Gap3 = calculate Sector start times from information of this Gap3 and individual Sector lengths
		for( TSector n=nSectors; n--; psi++->nRevolutions=1 )
			psi->revolutions[0].data=(PSectorData)::memset( ALLOCATE_SECTOR_DATA(psi->length), fillerByte, psi->length );
		// - presumption done
		return ERROR_SUCCESS;
	}

	TStdWinError CFDD::UnformatTrack(TCylinder cyl,THead head){
		// unformats given Track {Cylinder,Head}; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		LOG_TRACK_ACTION(cyl,head,_T("TStdWinError CFDD::UnformatTrack"));
		// - moving Head above the corresponding Cylinder
		if (!fddHead.__seekTo__(cyl))
error:		return LOG_ERROR(::GetLastError());
		// - unformatting (approached by formatting single long Sector that is longer than one spin of the floppy - after index pulse, it rewrites its own header)
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:{
				const BYTE LengthCode=GetSectorLengthCode(16384); // 16kB long Sector that rewrites its own header
				FD_FORMAT_PARAMS fmt={	FD_OPTION_MFM, head, LengthCode, 1, 50, 0,
										{cyl,head,0,LengthCode}
									};
				LOG_ACTION(_T("DeviceIoControl IOCTL_FDCMD_FORMAT_TRACK"));
				if (::DeviceIoControl( _HANDLE, IOCTL_FDCMD_FORMAT_TRACK, &fmt,sizeof(fmt), nullptr,0, &nBytesTransferred, nullptr )!=0){
					UnformatInternalTrack(cyl,head); // disposing internal information on actual Track format
					return ERROR_SUCCESS;
				}else
					goto error;
			}
			default:
				ASSERT(FALSE);
				return LOG_ERROR(ERROR_DEVICE_NOT_AVAILABLE);
		}
	}

	void CFDD::SetPathName(LPCTSTR lpszPathName,BOOL bAddToMRU){
		__super::SetPathName( lpszPathName, bAddToMRU );
		m_strPathName=lpszPathName;
	}
