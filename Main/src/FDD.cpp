#include "stdafx.h"

//#define FDD_DEBUG

#ifdef FDD_DEBUG
TCHAR bufDebug[MAX_PATH];

void __debug__(LPCTSTR text){
		if (*bufDebug){
			CFile f(bufDebug,CFile::modeCreate|CFile::modeWrite|CFile::modeNoTruncate);
			f.SeekToEnd();
			f.Write(text,::lstrlen(text));
			TCHAR newLine[3];
			f.Write(newLine,::lstrlen(::lstrcpy(newLine,"\n")));
		}
}
#endif

	static void __assign__(FD_ID_HEADER &rih,PCSectorId pid){
		rih.cyl=pid->cylinder, rih.head=pid->side, rih.sector=pid->sector, rih.size=pid->lengthCode;
	}

	
	#define INI_FDD	_T("FDD")

	#define INI_LATENCY_CONTROLLER		_T("latfdc")
	#define INI_LATENCY_1BYTE			_T("lat1b")
	#define INI_CALIBRATE_SECTOR_ERROR	_T("clberr")
	#define INI_CALIBRATE_FORMATTING	_T("clbfmt")
	#define INI_MOTOR_OFF_SECONDS		_T("mtroff")
	#define INI_VERIFY_FORMATTING		_T("vrftr")
	#define INI_VERIFY_WRITTEN_DATA		_T("vrfdt")
	#define INI_SEEKING					_T("seek")

	#define INI_MSG_RESET		_T("msgrst")
	#define INI_MSG_LATENCY		_T("msglat")

	static void __informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		TUtils::InformationWithCheckableShowNoMore( text, INI_FDD, messageId );
	}

	CFDD::TParams::TParams()
		// ctor
		: controllerLatency( app.GetProfileInt(INI_FDD,INI_LATENCY_CONTROLLER,86000)/1000.0 )
		, oneByteLatency( app.GetProfileInt(INI_FDD,INI_LATENCY_1BYTE,32000)/1000.0 )
		, calibrationAfterError( (TCalibrationAfterError)app.GetProfileInt(INI_FDD,INI_CALIBRATE_SECTOR_ERROR,TCalibrationAfterError::ONCE_PER_CYLINDER) )
		, calibrationStepDuringFormatting( app.GetProfileInt(INI_FDD,INI_CALIBRATE_FORMATTING,0) )
		, verifyFormattedTracks( app.GetProfileInt(INI_FDD,INI_VERIFY_FORMATTING,true)!=0 )
		, verifyWrittenData( app.GetProfileInt(INI_FDD,INI_VERIFY_WRITTEN_DATA,false)!=0 )
		, nSecondsToTurningMotorOff( app.GetProfileInt(INI_FDD,INI_MOTOR_OFF_SECONDS,2) ) // 0 = 1 second, 1 = 2 seconds, 2 = 3 seconds
		, readWholeTrackAsFirstSector(false) {
	}

	CFDD::TParams::~TParams(){
		// dtor
		app.WriteProfileInt( INI_FDD, INI_LATENCY_CONTROLLER, controllerLatency*1000 );
		app.WriteProfileInt( INI_FDD, INI_LATENCY_1BYTE, oneByteLatency*1000 );
		app.WriteProfileInt( INI_FDD, INI_CALIBRATE_SECTOR_ERROR,calibrationAfterError );
		app.WriteProfileInt( INI_FDD, INI_CALIBRATE_FORMATTING,calibrationStepDuringFormatting );
		app.WriteProfileInt( INI_FDD, INI_MOTOR_OFF_SECONDS,nSecondsToTurningMotorOff );
		app.WriteProfileInt( INI_FDD, INI_VERIFY_FORMATTING,verifyFormattedTracks );
		app.WriteProfileInt( INI_FDD, INI_VERIFY_WRITTEN_DATA,verifyWrittenData );
	}







	#define _HANDLE		fddHead.handle
	#define DRIVER		fddHead.driver

	TStdWinError CFDD::TInternalTrack::TSectorInfo::__saveToDisk__(CFDD *fdd,const TInternalTrack *pit,BYTE nSectorsToSkip) const{
		// saves this Sector to inserted floppy; returns Windows standard i/o error
		// - seeking the Head
		if (!fdd->fddHead.__seekTo__(pit->cylinder))
			return ::GetLastError();
		// - saving
		do{
			// : considering the NumberOfSectorsToSkip in current Track
			if (pit->__isIdDuplicated__(&id)) // to speed matters up, only if ID is duplicated in Track
				fdd->__setNumberOfSectorsToSkipOnCurrentTrack__(nSectorsToSkip);
			// : saving
			TStdWinError err;
			switch (fdd->DRIVER){
				case DRV_FDRAWCMD:{
					// . preparing for reproduction of requested i/o errors
					DWORD fdcCommand=IOCTL_FDCMD_WRITE_DATA,nBytesTransferred;
					if (fdcStatus.DescribesDeletedDam())
						fdcCommand=IOCTL_FDCMD_WRITE_DELETED_DATA;
					if (fdcStatus.reg2 & FDC_ST2_CRC_ERROR_IN_DATA){
						FD_SHORT_WRITE_PARAMS swp={ length, fdd->params.controllerLatency };
						::DeviceIoControl( fdd->_HANDLE, IOCTL_FD_SET_SHORT_WRITE, &swp,sizeof(swp), NULL,0, &nBytesTransferred, NULL );
					}
					if (id.lengthCode>fdd->__getMaximumSectorLengthCode__())
						fdd->__setWaitingForIndex__();
					// . writing Sector
					FD_READ_WRITE_PARAMS rwp={ FD_OPTION_MFM, pit->head, id.cylinder,id.side,id.sector,id.lengthCode, id.sector+1, 5, 0xff };
					err=::DeviceIoControl( fdd->_HANDLE, fdcCommand, &rwp,sizeof(rwp), ::memcpy(fdd->dataBuffer,data,length),__getOfficialSectorLength__(id.lengthCode), &nBytesTransferred, NULL )!=0
						? ERROR_SUCCESS
						: ::GetLastError();
					// . cleaning up after reproduction of requested i/o errors
					//nop
					// . verifying the writing
					if (fdd->params.verifyWrittenData){
						const TPhysicalAddress chs={ pit->cylinder, pit->head, id };
						TFdcStatus sr;
						fdd->__bufferSectorData__( chs, length, pit, nSectorsToSkip, &sr );
						if (fdcStatus.DescribesDataFieldCrcError()^sr.DescribesDataFieldCrcError()){
							TCHAR buf[80],tmp[30];
							::wsprintf(buf,_T("Verification failed for sector with %s on Track %d."),chs.sectorId.ToString(tmp),chs.GetTrackNumber(2));
							switch (TUtils::AbortRetryIgnore( buf, MB_DEFBUTTON2 )){
								case IDIGNORE:	break;
								case IDABORT:	return ERROR_CANCELLED;
								case IDRETRY:	continue;
							}
						}
					}
					break;
				}
				default:
					ASSERT(FALSE);
					return ERROR_DEVICE_NOT_AVAILABLE;
			}
			if (err!=ERROR_SUCCESS)
				switch (TUtils::AbortRetryIgnore(err,MB_DEFBUTTON2)){
					case IDIGNORE:	break;
					case IDABORT:	return err;
					default:		continue;
				}
			break;
		}while (true);
		return ERROR_SUCCESS;
	}

	typedef WORD TCrc;

	#define ALLOCATE_SECTOR_DATA(length)	(PSectorData)::malloc(length)
	#define FREE_SECTOR_DATA(data)			::free(data)

	#define SECTOR_LENGTH_MAX	16384

	static const TFdcStatus TrackRawContentIoError(FDC_ST1_DATA_ERROR,FDC_ST2_CRC_ERROR_IN_DATA);

	CFDD::TInternalTrack::TInternalTrack(const CFDD *fdd,TCylinder cyl,THead head,TSector _nSectors,PCSectorId bufferId)
		// ctor
		// - initialization
		: cylinder(cyl) , head(head)
		, nSectors(_nSectors) , sectors((TSectorInfo *)::ZeroMemory(::calloc(_nSectors,sizeof(TSectorInfo)),_nSectors*sizeof(TSectorInfo))) {
		TInternalTrack::TSectorInfo *psi=sectors;
		for( BYTE s=0; s<_nSectors; psi++->seqNum=s++ )
			psi->length=fdd->__getUsableSectorLength__(( psi->id=*bufferId++ ).lengthCode );
		// - determining which Sector numbers are already taken on the Track
		bool numbersTaken[(TSector)-1+1];
		::ZeroMemory(numbersTaken,sizeof(numbersTaken));
		for( BYTE n=nSectors; n--; numbersTaken[(--bufferId)->sector]=true );
		// - choosing the Sector ID under which the whole Track RawContent will appear
		rawContent.id.cylinder=cyl, rawContent.id.side=head;
		for( rawContent.id.sector=0; numbersTaken[rawContent.id.sector]; rawContent.id.sector++ );
		rawContent.length128=__getOfficialSectorLength__( rawContent.id.lengthCode=1+fdd->__getMaximumSectorLengthCode__() ); // "1+" = for the Sector to cover the whole Track
	}

	CFDD::TInternalTrack::~TInternalTrack(){
		// dtor
		const TSectorInfo *psi=sectors;
		for( TSector n=nSectors; n--; psi++ )
			if (psi->data) FREE_SECTOR_DATA(psi->data);
		::free(sectors);
	}

	bool CFDD::TInternalTrack::__isIdDuplicated__(PCSectorId pid) const{
		// True <=> at least two Sectors on the Track have the same ID, otherwise False
		BYTE nAppearances=0;
		const TSectorInfo *psi=sectors;
		for( BYTE n=nSectors; n--; nAppearances+=*pid==psi++->id );
		return nAppearances>1;
	}

	bool CFDD::TInternalTrack::__canRawDumpBeCreated__() const{
		// True <=> RawContent can be created (if not already created), otherwise False
		return nSectors!=0;
	}

	#define SECTOR_SYNCHRONIZATION		0xa1a1a1
	#define SECTOR_SYNCHRONIZATION_MASK	0xffffff

	#define SAVING_END	_T("Quitting saving.")

	TStdWinError CFDD::TInternalTrack::__saveRawContentToDisk__(CFDD *fdd,TCylinder cyl,THead head) const{
		// writes this RawContent to the Track given by current {Cylinder,Head} pair; returns Windows standard i/o error
		if (!rawContent.data)
			return ERROR_INVALID_DATA;
		// - preparing error message (should any error occur)
		TPhysicalAddress chs={ cyl, head }; // ID will be below set to the first Sector on the Track
		TCHAR error[80];
		::wsprintf( error, _T("Cannot save raw content of Track %d"), chs.GetTrackNumber(2) ); // 2 = max number of Sides on a floppy
		// - extracting first Sector ID
		PCSectorData pIdField=NULL; // assumption (ID Field not found and RawContent is not valid)
		bool idFieldOk=false;
		PCSectorData p=rawContent.data;
		for( const PCSectorData pMax=p+500; p<pMax; p++ )
			if (const BYTE nBytesOfGap=TInternalTrack::TRawContent::__containsBufferGap__(p))
				if (const BYTE nBytesOfSectorId=TInternalTrack::TRawContent::__containsBufferSectorId__( p+=nBytesOfGap ,&chs.sectorId,&idFieldOk)){
					pIdField = p+=nBytesOfSectorId; break;
				}
		if (!pIdField){ // ID Field not found
			TUtils::Information(error,_T("Cannot find first sector ID in raw content."),SAVING_END);
			return ERROR_REQUEST_REFUSED;
		}/*else if (chs.sectorId!=sectors[0].id){ // ID Field found but doesn't match the first Sector on the Track
			TCHAR cause[150],id1[30],id1raw[30];
			::wsprintf( cause, _T("Earlier scanned first sector %s does not match the current first sector %s in the raw content."), sectors[0].id.ToString(id1), chs.__vratCisloStopy__(2), chs.sectorId.ToString(id1raw) );
			TUtils::Information(error,cause,SAVING_END);
			return ERROR_CANCELLED;
		}*/
		// - extracting Data Address Mark (DAM) of the first Sector
		PCSectorData pDam=NULL; // assumption (DAM not found and RawContent not valid)
		for( const PCSectorData pMax=p+100; p<pMax; p++ )
			if (const BYTE nBytesOfGap=TInternalTrack::TRawContent::__containsBufferGap__(p))
				if ((*(PDWORD)(p+=nBytesOfGap)&SECTOR_SYNCHRONIZATION_MASK)==SECTOR_SYNCHRONIZATION){
					pDam = p+3; break;
				}
		if (!pDam || *pDam!=TDataAddressMark::DATA_RECORD && *pDam!=TDataAddressMark::DELETED_DATA_RECORD && *pDam!=TDataAddressMark::DEFECTIVE_DATA_RECORD){ // DAM not found
			TUtils::Information(error,_T("Cannot find first sector DAM in raw content (first sector in the track must have a DAM, others don't)."),SAVING_END);
			return ERROR_REQUEST_REFUSED;
		}
		// - saving
/*		const PCSectorData pData=1+pDam;
		DWORD nBytesTransferred;
		do{
			// . formatting to one long Sector
			chs.sectorId=rawContent.id;
			TStdWinError err=fdd->__formatToOneLongVerifiedSector__( chs, 0xe5 );
			if (err!=ERROR_SUCCESS) return err;
			// . saving RawContent
			const TSectorInfo si={	rawContent.id,
									fdd->__getUsableSectorLength__(rawContent.id.lengthCode),
									(PSectorData)pData,
									TFdcStatus(
										FDC_ST1_DATA_ERROR,
										FDC_ST2_CRC_ERROR_IN_DATA | (*pDam==TDataAddressMark::DATA_RECORD?0:FDC_ST2_DELETED_DAM)
									)
								};
			if (( err=si.__saveToDisk__(fdd,cyl,head) )!=ERROR_SUCCESS) return err;
			// . replacing the first ID
			switch (fdd->DRIVER){
				case DRV_FDRAWCMD:{
					FD_FORMAT_PARAMS fp={	FD_OPTION_MFM, chs.head,
											1, // Sector length doesn't matter, important is to create the ID Field
											3, 60, 0xe5
										};
					__assign__( fp.Headers[0], &sectors[0].id );
					FD_SHORT_WRITE_PARAMS swp={ sizeof(FD_ID_HEADER), fdd->params.controllerLatency+(sizeof(TCrc)+1)*fdd->params.oneByteLatency }; // "+1" = just to be sure
					::DeviceIoControl( fdd->_HANDLE, IOCTL_FD_SET_SHORT_WRITE, &swp,sizeof(swp), NULL,0, &nBytesTransferred, NULL );
					::DeviceIoControl( fdd->_HANDLE, IOCTL_FDCMD_FORMAT_TRACK, &fp,sizeof(fp)+8, NULL,0, &nBytesTransferred, NULL ); // cannot use IF because DeviceIoControl returns an error when formatting with bad CRC
					break;
				}
				default:
					ASSERT(FALSE);
					return ERROR_DEVICE_NOT_AVAILABLE;
			}
TUtils::Information("--- EVERYTHING OK ---");
			// . verifying that the first Sector on Track is reachable (i.e. hasn't been rewritten by the ID, see above)
			if (const TFdcStatus fdcStatus=fdd->__bufferSectorData__(chs,fdd->__getUsableSectorLength__(chs.sectorId.lengthCode)))
				if (fdcStatus.reg1 & FDC_ST1_NO_DATA) // if first Sector on Track not reachable ...
					continue; // ... it suffices to repeat the saving cycle
					
			// . saved ok
			break;
		}while (true);*/
		return ERROR_SUCCESS;
	}








	void CFDD::TInternalTrack::TRawContent::__generateGap__(PBYTE &buffer,BYTE nBytes_0x4E){
		// generates into the Buffer data that represent a gap on the Track
		::memset(buffer,0x4e,nBytes_0x4E), buffer+=nBytes_0x4E;
		::memset(buffer,0x00,12), buffer+=12;
	}
	BYTE CFDD::TInternalTrack::TRawContent::__containsBufferGap__(PCSectorData buffer){
		// returns the number of Bytes of the gap recognized at the front of the Buffer; returns 0 if no gap recognized
		BYTE n=0; while (*buffer==0x4e) n++,buffer++;
		//if (n<22) return 0; // commented out as (theoretically) there may be no 0x4e Bytes
		BYTE m=0; while (*buffer==0x00) m++,buffer++;
		if (m!=12) return 0;
		return m+n;
	}

	#define SECTOR_ID_ADDRESS_MARK		0xfe
	#define SECTOR_SYNC_ID_ADDRESS_MARK	( SECTOR_SYNCHRONIZATION | SECTOR_ID_ADDRESS_MARK<<24 )

	void CFDD::TInternalTrack::TRawContent::__generateSectorId__(PBYTE &buffer,PCSectorId id,PCFdcStatus pFdcStatus){
		// generates Sector ID into the Buffer
		*(PDWORD)buffer=SECTOR_SYNC_ID_ADDRESS_MARK, buffer+=sizeof(DWORD);
		*buffer++=id->cylinder, *buffer++=id->side, *buffer++=id->sector, *buffer++=id->lengthCode;
		TCrc crc=__getCrcCcitt__(buffer-8,8);
		if (pFdcStatus->DescribesDataFieldCrcError()) crc=~crc;
		*(TCrc *)buffer=crc, buffer+=sizeof(TCrc); // CRC
	}
	BYTE CFDD::TInternalTrack::TRawContent::__containsBufferSectorId__(PCSectorData buffer,TSectorId *outId,bool *outCrcOk){
		// returns the number of Bytes of a Sector ID recognized at the front of the Buffer; returns 0 if no Sector ID recognized
		if (*(PDWORD)buffer==SECTOR_SYNC_ID_ADDRESS_MARK){
			buffer+=sizeof(DWORD);
			outId->cylinder=*buffer++, outId->side=*buffer++, outId->sector=*buffer++, outId->lengthCode=*buffer++;
			*outCrcOk=__getCrcCcitt__(buffer-8,8)==*(TCrc *)buffer;
			return 3+1+4+2; // 0xA1A1A1 synchronization + ID Address Mark + ID itself + CRC
		}else
			return 0;
	}

	void CFDD::TInternalTrack::TRawContent::__generateSectorDefaultData__(PSectorData &buffer,TDataAddressMark dam,WORD sectorLength,BYTE fillerByte,PCFdcStatus pFdcStatus){
		// generates into the Buffer a Sector with a given Length and with the FillerByte as the default content
		*(PDWORD)buffer= SECTOR_SYNCHRONIZATION | dam<<24, buffer+=sizeof(DWORD);
		buffer=(PSectorData)::memset( buffer, fillerByte, sectorLength )+sectorLength;
		sectorLength+=3+1; // 0xA1A1A1 synchronization + DAM
		TCrc crc=__getCrcCcitt__(buffer-sectorLength,sectorLength);
		if (pFdcStatus->DescribesDataFieldCrcError()) crc=~crc;
		*(TCrc *)buffer=crc, buffer+=sizeof(TCrc); // CRC
	}

	CFDD::TInternalTrack::TRawContent::TRawContent()
		// ctor
		: data(NULL) // Null <=> RawContent not available
		, modified(false) {
	}

	CFDD::TInternalTrack::TRawContent::~TRawContent(){
		// dtor
		if (data)
			FREE_SECTOR_DATA(data);
	}










	CFDD::TFddHead::TFddHead()
		// ctor
		: handle(INVALID_HANDLE_VALUE) // see Reset
		, calibrated(false) , position(0)
		, preferRelativeSeeking( app.GetProfileInt(INI_FDD,INI_SEEKING,0)!=0 ) {
	}

	CFDD::TFddHead::~TFddHead(){
		// dtor
		app.WriteProfileInt( INI_FDD, INI_SEEKING, preferRelativeSeeking );
	}

	bool CFDD::TFddHead::__seekTo__(TCylinder cyl){
		// True <=> Head seeked to specified Cylinder, otherwise False and reporting the error
		if (cyl!=position){
			calibrated=false; // the Head is no longer Calibrated after changing to a different Cylinder
			do{
				DWORD nBytesTransferred;
				bool seeked;
				switch (driver){
					case DRV_FDRAWCMD:
						if (preferRelativeSeeking && cyl>position){ // RelativeSeeking is allowed only to higher Cylinder numbers
							FD_RELATIVE_SEEK_PARAMS rsp={ FD_OPTION_MT|FD_OPTION_DIR, 0, cyl-position };
							seeked=::DeviceIoControl( handle, IOCTL_FDCMD_RELATIVE_SEEK, &rsp,sizeof(rsp), NULL,0, &nBytesTransferred, NULL )!=0;
						}else{
							FD_SEEK_PARAMS sp={ cyl, 0 };
							seeked=::DeviceIoControl( handle, IOCTL_FDCMD_SEEK, &sp,sizeof(sp), NULL,0, &nBytesTransferred, NULL )!=0;
						}
						break;
					default:
						ASSERT(FALSE);
						return false;
				}
				if (seeked)
					position=cyl;
				else if (TUtils::RetryCancel(::GetLastError()))
					continue;
				else
					return false;
				break;
			}while (true);
		}
		return true;
	}

	bool CFDD::TFddHead::__calibrate__(){
		// True <=> Head successfully sent home (thus Calibrated), otherwise False
		position=0;
		DWORD nBytesTransferred;
		switch (driver){
			case DRV_FDRAWCMD:
				return ::DeviceIoControl( handle, IOCTL_FDCMD_RECALIBRATE, NULL,0, NULL,0, &nBytesTransferred, NULL )!=0;
			default:
				ASSERT(FALSE);
				return false;
		}
	}










	static PImage __instantiate__(){
		return new CFDD;
	}
	const CImage::TProperties CFDD::Properties={
		_T("[ Physical floppy drive A: ]"),	// name
		__instantiate__,	// instantiation function
		NULL, // filter
		TMedium::FLOPPY_ANY, // supported Media
		128,SECTOR_LENGTH_MAX	// Sector supported min and max length
	};

	#define FDD_THREAD_PRIORITY_DEFAULT	THREAD_PRIORITY_ABOVE_NORMAL

	CFDD::CFDD()
		// ctor
		// - base
		: CFloppyImage(&Properties,true)
		// - initialization
		, dataBuffer( ::VirtualAlloc(NULL,SECTOR_LENGTH_MAX,MEM_COMMIT,PAGE_READWRITE) ) {
		::ZeroMemory( internalTracks, sizeof(internalTracks) );
		// - creating a temporary file in order to not break the Document-View architecture
		TCHAR tmpFileName[MAX_PATH];
		::GetTempPath(MAX_PATH,tmpFileName);
		::GetTempFileName( tmpFileName, NULL, TRUE, tmpFileName );
		const CFile fTmp( m_strPathName=tmpFileName, CFile::modeCreate );
		// - connecting to the Drive
		__reset__();
	}

	CFDD::~CFDD(){
		// dtor
#ifdef FDD_DEBUG
__debug__("fdd dtor");
#endif
		// - disconnecting from the Drive
		__disconnectFromFloppyDrive__();
		// - disposing all InternalTracks
		__freeInternalTracks__();
		// - freeing virtual memory
		::VirtualFree(dataBuffer,0,MEM_RELEASE);
		// - deleting the temporary file (created in ctor to not break the Document-View architecture)
		CFile::Remove(m_strPathName);
	}








	TStdWinError CFDD::__connectToFloppyDrive__(TSupportedDriver _driver){
		// True <=> a connection to the Drive has been established using the Driver, otherwise False
		_HANDLE=INVALID_HANDLE_VALUE; // initialization
		switch ( DRIVER=_driver ){
			case DRV_AUTO:{
				// automatic recognition of installed Drivers
				TStdWinError err;
				if (( err=__connectToFloppyDrive__(DRV_FDRAWCMD) )==ERROR_SUCCESS)
					return ERROR_SUCCESS;
				else
					return err;
			}
			case DRV_FDRAWCMD:{
				// Simon Owen's Driver
				// . connecting to "A:"
				do{
					_HANDLE=::CreateFile( _T("\\\\.\\fdraw0"), GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL); //FILE_SHARE_WRITE
					if (_HANDLE!=INVALID_HANDLE_VALUE) break; // Driver installed and exclusive access allowed to the A: drive
error:				switch (const TStdWinError err=::GetLastError()){
						case ERROR_FILE_NOT_FOUND: // "file not found" error ...
							return ERROR_NOT_READY; // ... replaced with the "device not ready" error
						case ERROR_ALREADY_EXISTS: // "file already exists" error ...
							if (!TUtils::RetryCancel(_T("Exclusive access to the floppy drive required. Close all applications that may prevent it and try again.")))
								return ERROR_ACCESS_DENIED; // ... replaced with the "access denied" error
							else
								continue;
						default:
							return err;
					}
				}while (true);
				// . verifying the Driver's version
				DWORD nBytesTransferred,version;
				::DeviceIoControl( _HANDLE, IOCTL_FDRAWCMD_GET_VERSION, NULL,0, &version,sizeof(version), &nBytesTransferred, NULL );
				if (version<FDRAWCMD_VERSION) // old Driver version
					return ERROR_BAD_DRIVER;
				// . synchronizing with index pulse
				__setWaitingForIndex__();
				// . turning off the automatic recognition of floppy inserted into Drive (having turned it on creates problems when used on older Drives [Simon Owen])
				static const BYTE RecognizeInsertedFloppy=FALSE;
				if (!::DeviceIoControl( _HANDLE, IOCTL_FD_SET_DISK_CHECK, (PVOID)&RecognizeInsertedFloppy,1, NULL,0, &nBytesTransferred, NULL))
					goto error;
				return ERROR_SUCCESS;
			}
			default:
				ASSERT(FALSE);
				return ERROR_DEVICE_NOT_AVAILABLE;
		}
	}

	void CFDD::__disconnectFromFloppyDrive__(){
		// disconnects from the Drive (if previously connected to)
		if (_HANDLE!=INVALID_HANDLE_VALUE){
			::CloseHandle(_HANDLE);
			_HANDLE=INVALID_HANDLE_VALUE;
		}
	}


	#define REFER_TO_TRACK(cyl,head)	internalTracks[cyl*2+head] /* 2 = max number of Sides on a floppy */

	CFDD::PInternalTrack CFDD::__getScannedTrack__(TCylinder cyl,THead head) const{
		// returns Internal representation of the Track
		return	cyl<FDD_CYLINDERS_MAX
				? REFER_TO_TRACK(cyl,head)
				: NULL;
	}

	void CFDD::__unformatInternalTrack__(TCylinder cyl,THead head){
		// disposes buffered InternalTrack
		if (const PInternalTrack tmp=__getScannedTrack__(cyl,head))
			delete tmp, REFER_TO_TRACK(cyl,head)=NULL;
	}

	void CFDD::__freeInternalTracks__(){
		// disposes all InternalTracks
#ifdef FDD_DEBUG
__debug__("fdd __freeInternalTracks__");
#endif
		for( TCylinder cyl=0; cyl<FDD_CYLINDERS_MAX; cyl++ )
			__unformatInternalTrack__(cyl,0), __unformatInternalTrack__(cyl,1);
	}




	BOOL CFDD::OnOpenDocument(LPCTSTR){
		// True <=> Image opened successfully, otherwise False
#ifdef FDD_DEBUG
__debug__("fdd OnOpenDocument");
#endif
		return TRUE; // always successfull; failure may arise later on when attempting to access the Drive in exclusive mode
	}




	struct TSaveParams sealed{
		CFDD *const fdd;

		TSaveParams(CFDD *fdd)
			: fdd(fdd) {
		}
	};
	UINT AFX_CDECL CFDD::__save_thread__(PVOID _pCancelableAction){
		// thread to save InternalTracks (particularly their Modified Sectors) on inserted floppy
		TBackgroundActionCancelable *const pAction=(TBackgroundActionCancelable *)_pCancelableAction;
		const TSaveParams sp=*(TSaveParams *)pAction->fnParams;
		for( TCylinder cyl=0; cyl<FDD_CYLINDERS_MAX; pAction->UpdateProgress(++cyl) )
			for( THead head=0; head<2; head++ ) // 2 = max number of Sides on a floppy
				if (TInternalTrack *const pit=sp.fdd->__getScannedTrack__(cyl,head)){
					// . saving RawContent of the Track
					/*if (pid->rawContent.bModified){
						const TStdWinError err=pid->__saveRawContentToDisk__( up.fdd, cyl, head );
						if (err==ERROR_SUCCESS){
							pid->rawContent.modified=false;
						}else
							return pAction->TerminateWithError(err);
					}*/
					// . saving data of Track's all Modified Sectors
					TInternalTrack::TSectorInfo *psi=pit->sectors;
					for( TSector n=0; n<pit->nSectors; psi++,n++ )
						if (psi->modified){ // Sector has been Modified since it's was last saved
							if (!pAction->bContinue) return ERROR_CANCELLED;
#ifdef _DEBUG
{TCHAR buf[200];
::wsprintf(buf,_T("fdd __save_thread__ chs=%d(%d),%d(%d),%d"),cyl,psi->id.cylinder,head,psi->id.side,psi->id.sector);
TUtils::Information(buf);}
#endif
							// : saving
							const TStdWinError err=psi->__saveToDisk__( sp.fdd, pit, n );
							// : evaluating the result
							if (err==ERROR_SUCCESS)
								psi->modified=false;
							else
								return pAction->TerminateWithError(err);
						}
				}
		sp.fdd->m_bModified=FALSE;
		return ERROR_SUCCESS;
	}
	BOOL CFDD::OnSaveDocument(LPCTSTR){
		// True <=> this Image has been successfully saved, otherwise False
		const TStdWinError err=	TBackgroundActionCancelable(
									__save_thread__,
									&TSaveParams( this ),
									FDD_THREAD_PRIORITY_DEFAULT
								).CarryOut(FDD_CYLINDERS_MAX);
		::SetLastError(err);
		return err==ERROR_SUCCESS;
	}


	TCylinder CFDD::GetCylinderCount() const{
		// determines and returns the actual number of Cylinders in the Image
		return	GetNumberOfFormattedSides(0) // if zeroth Cylinder exists ...
				? FDD_CYLINDERS_MAX // ... then it's assumed that there is the maximum number of Cylinders available (the actual number may be adjusted by systematically scanning the Tracks)
				: 0; // ... otherwise the floppy is considered not formatted
	}

	THead CFDD::GetNumberOfFormattedSides(TCylinder cyl) const{
		// determines and returns the number of Sides formatted on given Cylinder; returns 0 iff Cylinder not formatted
		return (ScanTrack(cyl,0,NULL,NULL)!=0) + (ScanTrack(cyl,1,NULL,NULL)!=0);
	}

	CFDD::PInternalTrack CFDD::__scanTrack__(TCylinder cyl,THead head){
		// scans given Track and returns the number of discovered Sectors; returns Null if Track cannot be scanned (e.g. due to an hardware error or "out-of-range" error)
		// - attempting to scan the specified Track
		if (cyl<FDD_CYLINDERS_MAX)
			// Track can be scanned
			if (const PInternalTrack pit=__getScannedTrack__(cyl,head))
				// Track has been already scanned before - returning it
				return pit;
			else
				// Track has not yet been scanned - attempting to scan it now
#ifdef FDD_DEBUG
TCHAR buf[200];
::wsprintf(buf,_T("fdd __scanTrack__ cyl=%d,head=%d"),cyl,head);
__debug__(buf);
#endif
				if (fddHead.__seekTo__(cyl)){
					// successfully seeked to the given Cylinder
					DWORD nBytesTransferred;
					switch (DRIVER){
						case DRV_FDRAWCMD:{
							FD_SCAN_PARAMS sp={ FD_OPTION_MFM, head };
							#pragma pack(1)
							struct{
								BYTE n;
								FD_ID_HEADER header[(TSector)-1];
							} sectors;
							if (!::DeviceIoControl( _HANDLE, IOCTL_FD_SCAN_TRACK, &sp,sizeof(sp), &sectors,sizeof(sectors), &nBytesTransferred, NULL ))
								break;
							TSectorId bufferId[(TSector)-1];
							for( BYTE n=0; n<sectors.n; bufferId[n]=sectors.header[n],n++ );
							return REFER_TO_TRACK(cyl,head) = new TInternalTrack( this, cyl, head, sectors.n, bufferId );
						}
						default:
							ASSERT(FALSE);
							break;
					}
				}
		// - Track cannot be scanned
		return NULL;
	}

	TSector CFDD::ScanTrack(TCylinder cyl,THead head,PSectorId bufferId,PWORD bufferLength) const{
		// returns the number of Sectors found in given Track, and eventually populates the Buffer with their IDs (if Buffer!=Null); returns 0 if Track not formatted or not found
		if (const PInternalTrack pit=((CFDD *)this)->__scanTrack__(cyl,head)){
			// Track managed to be scanned
			const bool rawDumpExists= params.readWholeTrackAsFirstSector && pit->__canRawDumpBeCreated__();
			const TSector n=rawDumpExists+pit->nSectors;
			if (bufferId){
				if (rawDumpExists)
					*bufferLength++=__getUsableSectorLength__(( *bufferId++=pit->rawContent.id ).lengthCode);
				const TInternalTrack::TSectorInfo *psi=pit->sectors;
				for( TSector s=n; s--; psi++ )
					*bufferId++=psi->id, *bufferLength++=psi->length;
			}
			return n;
		}else
			// Track failed to be scanned
			return 0;
	}

	void CFDD::__setWaitingForIndex__() const{
		// sets waiting for the index pulse before executing the next command
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:{
				::DeviceIoControl( _HANDLE, IOCTL_FD_WAIT_INDEX, NULL,0, NULL,0, &nBytesTransferred, NULL );
				break;
			}
			default:
				ASSERT(FALSE);
		}
	}

	void CFDD::__setNumberOfSectorsToSkipOnCurrentTrack__(BYTE nSectorsToSkip) const{
		// True <=> the NumberOfSectors to be skipped on the current Track successfully set, otherwise False
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:{
				FD_SECTOR_OFFSET_PARAMS sop={ nSectorsToSkip };
				::DeviceIoControl( _HANDLE, IOCTL_FD_SET_SECTOR_OFFSET, &sop,sizeof(sop), NULL,0, &nBytesTransferred, NULL );
				break;
			}
			default:
				ASSERT(FALSE);
		}
	}

	bool CFDD::__bufferSectorData__(RCPhysicalAddress chs,WORD sectorLength,const TInternalTrack *pit,BYTE nSectorsToSkip,TFdcStatus *pFdcStatus) const{
		// True <=> requested Sector found in currently seeked Track and data of the Sector have been buffered in the internal DataBuffer, otherwise False
		// - taking into account the NumberOfSectorsToSkip
		if (pit->__isIdDuplicated__(&chs.sectorId)) // to speed matters up, only if ID is duplicated in the Track
			__setNumberOfSectorsToSkipOnCurrentTrack__(nSectorsToSkip);
		// - waiting for the index pulse
		//__setWaitingForIndex__(); // commented out as caller is to decide whether this is needed or not, and eventually call it
		// - buffering Sector's data
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:{
				FD_READ_WRITE_PARAMS rwp={ FD_OPTION_MFM|FD_OPTION_SK, chs.head, chs.sectorId.cylinder,chs.sectorId.side,chs.sectorId.sector,chs.sectorId.lengthCode, chs.sectorId.sector+1, 5, 0xff };
				if (!::DeviceIoControl( _HANDLE, IOCTL_FDCMD_READ_DATA, &rwp,sizeof(rwp), dataBuffer,SECTOR_LENGTH_MAX, &nBytesTransferred, NULL )){
					// Sector read with errors
					// | getting FdcStatus
					FD_CMD_RESULT cmdRes;
					::DeviceIoControl( _HANDLE, IOCTL_FD_GET_RESULT, NULL,0, &cmdRes,sizeof(cmdRes), &nBytesTransferred, NULL );
					*pFdcStatus=TFdcStatus(cmdRes.st1,cmdRes.st2);
					// | if "Deleted DAM" is one of the errors, repeating reading with appropriate command
					if (pFdcStatus->DescribesDeletedDam()){
						::DeviceIoControl( _HANDLE, IOCTL_FDCMD_READ_DELETED_DATA, &rwp,sizeof(rwp), dataBuffer,SECTOR_LENGTH_MAX, &nBytesTransferred, NULL );
						::DeviceIoControl( _HANDLE, IOCTL_FD_GET_RESULT, NULL,0, &cmdRes,sizeof(cmdRes), &nBytesTransferred, NULL );
						*pFdcStatus=TFdcStatus( cmdRes.st1, cmdRes.st2|FDC_ST2_DELETED_DAM );
					}
				}
				return true;
			}
			default:
				ASSERT(FALSE);
				return false;
		}
	}

	PSectorData CFDD::GetSectorData(RCPhysicalAddress chs,BYTE nSectorsToSkip,bool recoverFromError,PWORD sectorLength,TFdcStatus *pFdcStatus){
		// returns Data of a Sector on a given PhysicalAddress; returns Null iff Sector not found or Track not formatted
		if (const PInternalTrack pit=__scanTrack__(chs.cylinder,chs.head)){
			// . getting Track's RawContent
			if (params.readWholeTrackAsFirstSector && pit->__canRawDumpBeCreated__())
				if (chs.sectorId==pit->rawContent.id){
					if (!pit->rawContent.data){ // Track RawContent not yet read
						// : buffering each Sector in the Track (should the RawContent be Modified and subsequently saved - in such case, the RawContent must be saved first before saving any of its Sectors)
						for( BYTE i=0; i<pit->nSectors; ){
							const TPhysicalAddress tmp={ chs.cylinder, chs.head, pit->sectors[i].id };
							GetSectorData( tmp, ++i, true, sectorLength, pFdcStatus ); // "1+i" = the first Sector represents the RawContent
						}
						// : reading the Track
						DWORD nBytesTransferred;
						const PCSectorId pid=&pit->sectors[0].id;
						FD_READ_WRITE_PARAMS rwp={ FD_OPTION_MFM|FD_OPTION_SK, chs.head, pid->cylinder,pid->side,pid->sector,7, pid->sector+1, 5, 0xff }; // 7 = reads 16384 Bytes beginning with the first Data Field
						if (::DeviceIoControl( _HANDLE, IOCTL_FDCMD_READ_TRACK, &rwp,sizeof(rwp), dataBuffer,SECTOR_LENGTH_MAX, &nBytesTransferred, NULL )!=0){
							// Track read - reconstructing the gaps and ID that preceed the first physical Sector (because reading of the Track begun from its data but the ID Field itself wasn't read)
							PSectorData pData = pit->rawContent.data = ALLOCATE_SECTOR_DATA(pit->rawContent.length128);
							TInternalTrack::TRawContent::__generateGap__(pData,40);
							TInternalTrack::TRawContent::__generateSectorId__(pData,pid,&TFdcStatus::WithoutError);
							TInternalTrack::TRawContent::__generateGap__(pData,22);
							TInternalTrack::TRawContent::__generateSectorDefaultData__( pData, TDataAddressMark::DATA_RECORD, 0, 0, &TFdcStatus::WithoutError );
							::memcpy( pData-sizeof(TCrc), dataBuffer, __getUsableSectorLength__(pit->rawContent.id.lengthCode) ); // assumed that UsableSectorLength < OfficialSectorLength (and thus not written outside allocated memory)
						}//else
							//return NULL; // commented out as it holds that "pid->rawContent.data==Null"
					}
					*sectorLength=__getUsableSectorLength__(pit->rawContent.id.lengthCode), *pFdcStatus=TrackRawContentIoError;
					return pit->rawContent.data;
				}else if (nSectorsToSkip)
					nSectorsToSkip--;
			// . retrieving data of Sector requested in current Track
			TInternalTrack::TSectorInfo *psi=pit->sectors;
			for( TSector n=pit->nSectors,nSkipping=nSectorsToSkip; n--; psi++ )
				if (!nSkipping){
					if (psi->id==chs.sectorId){
						*sectorLength=psi->length;
						// : if Data already read WithoutError, returning them
						if (psi->data)
							if (psi->fdcStatus.IsWithoutError()){ // returning error-free data
returnData:						*pFdcStatus=psi->fdcStatus;
								return psi->data;
							}else // disposing previous erroneous Data
								FREE_SECTOR_DATA(psi->data), psi->data=NULL;
						// : seeking Head to the given Cylinder
						if (!fddHead.__seekTo__(chs.cylinder))
							return NULL; // Sector cannot be found as Head cannot be seeked
						// : initial attempt to retrieve the Data
						if (!__bufferSectorData__(chs,*sectorLength,pit,nSectorsToSkip,&psi->fdcStatus))
							return NULL;
						// : recovering from errors
						if (!psi->fdcStatus.IsWithoutError()) // no Data, or Data with errors
							if (recoverFromError && !fddHead.calibrated)
								if (fddHead.__calibrate__() && fddHead.__seekTo__(chs.cylinder)){
									fddHead.calibrated=params.calibrationAfterError!=TParams::TCalibrationAfterError::FOR_EACH_SECTOR;
									__bufferSectorData__(chs,*sectorLength,pit,nSectorsToSkip,&psi->fdcStatus);
								}
						if (!psi->fdcStatus.DescribesMissingDam())
							psi->data=(PSectorData)::memcpy( ALLOCATE_SECTOR_DATA(*sectorLength), dataBuffer, *sectorLength );
						goto returnData; // returning (any) Data
					}
				}else
					nSkipping--;
		}
		// Sector not found
		if (::GetLastError()==ERROR_SUCCESS)
			::SetLastError(ERROR_SECTOR_NOT_FOUND);
		return NULL;
	}

	TStdWinError CFDD::MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus){
		// marks Sector with specified PhysicalAddress as "dirty", plus sets it the given FdcStatus; returns Windows standard i/o error
		if (const PInternalTrack pit=__getScannedTrack__(chs.cylinder,chs.head)){ // Track has already been scanned
			// . Modifying Track's RawContent
			if (params.readWholeTrackAsFirstSector && pit->__canRawDumpBeCreated__())
				if (chs.sectorId==pit->rawContent.id){
					// : marking each Sector in the Track as Modified (each of them has been already buffered in GetSectorData)
					const TInternalTrack::TSectorInfo *psi=pit->sectors;
					for( BYTE i=0; i<pit->nSectors; psi++ ){
						const TPhysicalAddress tmp={ chs.cylinder, chs.head, psi->id };
						MarkSectorAsDirty( tmp, ++i, &psi->fdcStatus ); // "1+i" = the first Sector represents the RawContent
					}
					// : marking the RawContent as Modified
					pit->rawContent.modified=true;
					return ERROR_SUCCESS;
				}else if (nSectorsToSkip)
					nSectorsToSkip--;
			// . Modifying data of Sector requested in current Track
			TInternalTrack::TSectorInfo *psi=pit->sectors;
			for( TSector n=pit->nSectors; n--; psi++ )
				if (!nSectorsToSkip){
					if (psi->id==chs.sectorId){
						if (psi->data){ // Sector must have been buffered in the past before it can be Modified
							psi->fdcStatus=*pFdcStatus;
							m_bModified = psi->modified = true;
						}
						
						// : informing on unability to reproduce some errors (until all errors are supported in the future)
						if (pFdcStatus->reg1 & (FDC_ST1_NO_DATA)){
							TCHAR buf[200],tmp[30];
							::wsprintf( buf, _T("Not all errors can be reproduced on a real floppy for sector with %s."), chs.sectorId.ToString(tmp) );
							TUtils::Information(buf);
							return ERROR_BAD_COMMAND;
						}

						break;
					}
				}else
					nSectorsToSkip--;
		}
		return ERROR_SUCCESS;
	}

	TStdWinError CFDD::__setDataTransferSpeed__(TMedium::TType _floppyType){
		// sets TransferSpeed for given FloppyType and checks suitability by scanning zeroth Track; returns Windows standard i/o error
		DWORD nBytesTransferred;
		BYTE transferSpeed;
		switch (_floppyType){
			case TMedium::FLOPPY_DD:
				// 2DD floppy
				switch (DRIVER){
					case DRV_FDRAWCMD:{
						transferSpeed=FD_RATE_250K;
fdrawcmd:				// . setting
						if (!::DeviceIoControl( _HANDLE, IOCTL_FD_SET_DATA_RATE, &transferSpeed,1, NULL,0, &nBytesTransferred, NULL ))
							return ::GetLastError();
						// . scanning zeroth Track - if it can be read, we have set the correct TransferSpeed
						__unformatInternalTrack__(0,0); // initialization
						const TInternalTrack *const pit=__scanTrack__(0,0);
						return	pit && pit->nSectors // if Track can be scanned and its Sectors recognized ...
								? ERROR_SUCCESS	// ... then the TransferSpeed has been set correctly
								: ERROR_INVALID_DATA;
					}
					default:
						ASSERT(FALSE);
						break;
				}
				break;
			case TMedium::FLOPPY_HD:
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
		}
		return ERROR_DEVICE_NOT_AVAILABLE;
	}

	TStdWinError CFDD::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		// - base
		const TStdWinError err=CFloppyImage::SetMediumTypeAndGeometry(pFormat,sideMap,firstSectorNumber);
		if (err!=ERROR_SUCCESS)
			return err;
		// - setting the transfer speed according to current FloppyType (DD/HD)
		switch (floppyType){
			case TMedium::FLOPPY_DD:
			case TMedium::FLOPPY_HD:
				// determining if corresponding FloppyType is inserted
				return __setDataTransferSpeed__(floppyType);
			default:
				// automatically recognizing the Type of inserted floppy
				if (__setDataTransferSpeed__( floppyType=TMedium::FLOPPY_DD )==ERROR_SUCCESS) return ERROR_SUCCESS;
				if (__setDataTransferSpeed__( floppyType=TMedium::FLOPPY_HD )==ERROR_SUCCESS) return ERROR_SUCCESS;
				return ERROR_BAD_COMMAND;
		}
	}

	void CFDD::__setSecondsBeforeTurningMotorOff__(BYTE nSeconds) const{
		// sets given NumberOfSeconds before turning the motor off
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:
				::DeviceIoControl( _HANDLE, IOCTL_FD_SET_MOTOR_TIMEOUT, &++nSeconds,sizeof(nSeconds), NULL,0, &nBytesTransferred, NULL );
				break;
			default:
				ASSERT(FALSE);
		}
	}

	TStdWinError CFDD::__reset__(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		// - disposing all InternalTracks
		__freeInternalTracks__();
		// - re-connecting to the Drive
		__disconnectFromFloppyDrive__();
		const TStdWinError err=__connectToFloppyDrive__(DRV_AUTO);
		if (err!=ERROR_SUCCESS)
			return err;
		// - sending Head home
		return fddHead.__calibrate__() ? ERROR_SUCCESS : ::GetLastError();
	}

	bool CFDD::__isFloppyInserted__() const{
		// True <=> (some) floppy is inserted in the Drive, otherwise False
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:
				return	::DeviceIoControl( _HANDLE, IOCTL_FD_CHECK_DISK, NULL,0, NULL,0, &nBytesTransferred, NULL )!=0
						? ::GetLastError()==ERROR_SUCCESS
						: false;
			default:
				ASSERT(FALSE);
				return false;
		}
	}

	LPCTSTR CFDD::__getControllerType__() const{
		// determines and returns the controller type of the Drive
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:{
				FD_FDC_INFO fdcInfo;
				::DeviceIoControl( _HANDLE, IOCTL_FD_GET_FDC_INFO, NULL,0, &fdcInfo,sizeof(fdcInfo), &nBytesTransferred, NULL );
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
				return NULL;
		}
	}

	#define TEST_BYTE	'A'

	#pragma pack(1)
	struct TLatencyParams sealed{
		CFDD *const fdd;
		const bool ddFloppy; // double density floppy
		const BYTE usAccuracy; // accuracy in microseconds
		const BYTE nRepeats;
		float outControllerLatency; // microseconds
		float out1ByteLatency;		// microseconds

		TLatencyParams(CFDD *fdd,bool ddFloppy,BYTE usAccuracy,BYTE nRepeats)
			: fdd(fdd)
			, ddFloppy(ddFloppy)
			, usAccuracy(usAccuracy) , nRepeats(nRepeats)
			, outControllerLatency(0) , out1ByteLatency(0) {
		}
	};
	UINT AFX_CDECL CFDD::__determineLatency_thread__(PVOID _pCancelableAction){
		// thread to automatically determine the write latency
		TBackgroundActionCancelable *const pAction=(TBackgroundActionCancelable *)_pCancelableAction;
		TLatencyParams &lp=*(TLatencyParams *)pAction->fnParams;
		// - defining the Interruption
		struct TInterruption sealed{
		private:
			const TBackgroundActionCancelable *const pAction;
			CFDD *const fdd;
			const PVOID sectorDataToWrite;
			const BYTE usAccuracy; // microseconds
		public:
			TPhysicalAddress chs;
			const WORD sectorLength;
			WORD nMicroseconds;

			TInterruption(const TBackgroundActionCancelable *pAction,CFDD *_fdd,WORD _sectorLength,BYTE _usAccuracy)
				// ctor
				: pAction(pAction)
				, fdd(_fdd) , sectorLength(_sectorLength)
				, sectorDataToWrite( ::VirtualAlloc(NULL,SECTOR_LENGTH_MAX,MEM_COMMIT,PAGE_READWRITE) )
				, usAccuracy(_usAccuracy) , nMicroseconds(0) {
				::ZeroMemory(&chs,sizeof(chs)), chs.sectorId.lengthCode=__getSectorLengthCode__(sectorLength);
				::memset( sectorDataToWrite, TEST_BYTE, sectorLength );
			}
			~TInterruption(){
				// dtor
				::VirtualFree(sectorDataToWrite,0,MEM_RELEASE);
			}

			void __writeSectorData__(WORD nBytesToWrite) const{
				// writes SectorData to the actual PhysicalAddress and interrupts the controller after specified NumberOfBytesToWrite and Microseconds
				switch (fdd->DRIVER){
					case DRV_FDRAWCMD:{
						// : setting controller interruption to the specified NumberOfBytesToWrite and Microseconds
						DWORD nBytesTransferred;
						FD_SHORT_WRITE_PARAMS swp={ nBytesToWrite, nMicroseconds };
						::DeviceIoControl( fdd->_HANDLE, IOCTL_FD_SET_SHORT_WRITE, &swp,sizeof(swp), NULL,0, &nBytesTransferred, NULL );
						// : writing
						FD_READ_WRITE_PARAMS rwp={ FD_OPTION_MFM, chs.head, chs.sectorId.cylinder,chs.sectorId.side,chs.sectorId.sector,chs.sectorId.lengthCode, chs.sectorId.sector+1, FDD_SECTOR_GAP3_STD, 0xff };
						::DeviceIoControl( fdd->_HANDLE, IOCTL_FDCMD_WRITE_DATA, &rwp,sizeof(rwp), sectorDataToWrite,sectorLength, &nBytesTransferred, NULL );
						break;
					}
					default:
						ASSERT(FALSE);
						break;
				}
			}
			WORD __getNumberOfWrittenBytes__() const{
				// counts and returns the number of TestBytes actually written in the most recent call to __writeSectorData__
				PCBYTE p=(PCBYTE)fdd->dataBuffer;
				for( fdd->__bufferSectorData__(chs,sectorLength,&TInternalTrack(fdd,chs.cylinder,chs.head,1,&chs.sectorId),0,&TFdcStatus()); *p==TEST_BYTE; p++ );
				return p-(PCBYTE)fdd->dataBuffer;
			}
			float __setInterruptionToWriteSpecifiedNumberOfBytes__(WORD nBytes){
				// sets this Interruption so that the specified NumberOfBytes is written to actual PhysicalAddress; returns the NumberOfMicroseconds set
				// : initialization using the default NumberOfMicroseconds
				nMicroseconds=20;
				// : increasing the NumberOfMicroseconds until the specified NumberOfBytes is written for the first time
				do{
					if (!pAction->bContinue) return 1;
					nMicroseconds+=usAccuracy;
					__writeSectorData__(nBytes);
				}while (__getNumberOfWrittenBytes__()<nBytes);
				const float nMicrosecondsA=nMicroseconds;
				// : increasing the NumberOfMicroseconds until a higher NumberOfBytes is written for the first time
				do{
					if (!pAction->bContinue) return 1;
					nMicroseconds+=usAccuracy;
					__writeSectorData__(nBytes);
				}while (__getNumberOfWrittenBytes__()<=nBytes);
				// : the resulting NumberOfMicroseconds is the average of when the NumberOfBytes has been written for the first and last time
				return nMicroseconds=(nMicrosecondsA+nMicroseconds)/2;
			}
		} interruption( pAction, lp.fdd, lp.ddFloppy?4096:8192, lp.usAccuracy );
		// - testing
		BYTE state=0;
		for( BYTE c=lp.nRepeats; c--; ){
			// . writing the testing Sector (DD = 4kB, HD = 8kB)
			TStdWinError err;
			do{
				// : seeking Head to the particular Cylinder
				TPhysicalAddress &chs=interruption.chs;
				if (!lp.fdd->fddHead.__seekTo__(chs.cylinder))
					return pAction->TerminateWithError(ERROR_REQUEST_REFUSED);
				// : formatting Track to a single Sector
				const bool vft0=lp.fdd->params.verifyFormattedTracks;
				lp.fdd->params.verifyFormattedTracks=false;
					err=lp.fdd->FormatTrack( chs.cylinder, chs.head, 1,&chs.sectorId,&interruption.sectorLength,&TFdcStatus::WithoutError, FDD_SECTOR_GAP3_STD, 0 );
				lp.fdd->params.verifyFormattedTracks=vft0;
				if (err!=ERROR_SUCCESS)
					return pAction->TerminateWithError(err);
				// : verifying the single formatted Sector
				TFdcStatus sr;
				lp.fdd->__bufferSectorData__( chs, interruption.sectorLength, &TInternalTrack(lp.fdd,chs.cylinder,chs.head,1,&chs.sectorId), 0, &sr );
				if (sr.IsWithoutError())
					break;
				// : attempting to create a Sector WithoutErrors on another Track
				if (++chs.cylinder==FDD_CYLINDERS_MAX)
					return pAction->TerminateWithError(ERROR_REQUEST_REFUSED);
			}while (true);
			pAction->UpdateProgress(++state);
			// . experimentally determining parameters from sample area of TestBytes
				// : determining the ControllerLatency
				const WORD nBytes=interruption.sectorLength/2;
				const float nControllerMicroseconds=interruption.__setInterruptionToWriteSpecifiedNumberOfBytes__(nBytes);
				lp.outControllerLatency+=nControllerMicroseconds; // below divided by the number of attempts to get an average
/*
{TCHAR buf[80];
::wsprintf(buf,_T("nMicrosecondsA=%d"),(int)(nControllerMicroseconds*1000));
TUtils::Information(buf);}
//*/
				pAction->UpdateProgress(++state);
				// : determining the latency of one Byte
				const float p = interruption.nMicroseconds = 65000;
				interruption.__writeSectorData__(nBytes);
				const float n=interruption.__getNumberOfWrittenBytes__();
				const float nMicrosecondsPerByte=( p - nControllerMicroseconds ) / ( n - nBytes );
/*
{TCHAR buf[80];
::wsprintf(buf,_T("oneByteLatency=%d"),(int)(nMicrosecondsPerByte*1000));
TUtils::Information(buf);}
//*/
				//lp.out1ByteLatency+=( n - sectorLength ) / ( p - nMicrosecondsZ );
				lp.out1ByteLatency+=nMicrosecondsPerByte; // below divided by the number of attempts to get an average
				pAction->UpdateProgress(++state);
			pAction->UpdateProgress(++state);
			if (!pAction->bContinue) return ERROR_CANCELLED;
		}
		// - computing the final latency values
		lp.outControllerLatency/=lp.nRepeats, lp.out1ByteLatency/=lp.nRepeats;
		pAction->UpdateProgress(++state);
		return ERROR_SUCCESS;
	}

	bool CFDD::__showSettingDialog__(){
		// True <=> the SettingDialog shown and new values adopted, otherwise False
		// - defining the Dialog
		class CSettingDialog sealed:public CDialog{
			CFDD *const fdd;

			void __refreshMediumInformation__(){
				// detects a floppy in the Drive and attempts to recognize its Type
				// . making sure that a floppy is in the Drive
				fdd->floppyType=TMedium::UNKNOWN; // assumption (floppy not inserted or not recognized)
				if (!fdd->__isFloppyInserted__())
					SetDlgItemText( ID_MEDIUM, _T("Not inserted") );
				// . attempting to recognize any previous format on the floppy
				else
					if (fdd->__setDataTransferSpeed__(TMedium::FLOPPY_DD)==ERROR_SUCCESS){
						fdd->floppyType=TMedium::FLOPPY_DD;
						SetDlgItemText( ID_MEDIUM, _T("DD formatted") );
					}else if (fdd->__setDataTransferSpeed__(TMedium::FLOPPY_HD)==ERROR_SUCCESS){
						fdd->floppyType=TMedium::FLOPPY_HD;
						SetDlgItemText( ID_MEDIUM, _T("HD formatted") );
					}else
						SetDlgItemText( ID_MEDIUM, _T("Not formatted or faulty") );
				// . forcing redrawing (as the new text may be shorter than the original text, leaving the original partly visible)
				GetDlgItem(ID_MEDIUM)->Invalidate();
			}
			void PreInitDialog() override{
				// dialog initialization
				// . base
				CDialog::PreInitDialog();
				// . displaying controller information
				SetDlgItemText( ID_SYSTEM, fdd->__getControllerType__() );
				// . displaying inserted Medium information
				__refreshMediumInformation__();
			}
			void __exchangeLatency__(CDataExchange* pDX){
				// exchange of latency-related data from and to controls
				DDX_Text( pDX,	ID_LATENCY,	params.controllerLatency );
				DDX_Text( pDX,	ID_NUMBER2,	params.oneByteLatency );
			}
			void DoDataExchange(CDataExchange* pDX) override{
				// exchange of data from and to controls
				// . latency values
				__exchangeLatency__(pDX);
				// . CalibrationAfterError
				int tmp=params.calibrationAfterError;
				DDX_Radio( pDX,	ID_NONE,		tmp );
				params.calibrationAfterError=(TParams::TCalibrationAfterError)tmp;
				// . CalibrationStepDuringFormatting
				GetDlgItem(ID_NUMBER)->EnableWindow( tmp=params.calibrationStepDuringFormatting!=0 );
				DDX_Radio( pDX,	ID_ZERO,		tmp );
				if (tmp)
					DDX_Text( pDX,	ID_NUMBER,	tmp=params.calibrationStepDuringFormatting );
				else
					SetDlgItemInt(ID_NUMBER,4,FALSE);
				params.calibrationStepDuringFormatting=tmp;
				// . NumberOfSecondsToTurningMotorOff
				tmp=params.nSecondsToTurningMotorOff;
				DDX_CBIndex( pDX, ID_ROTATION,	tmp );
				params.nSecondsToTurningMotorOff=tmp;
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
				// . ReadingOfWholeTracksAsFirstSectors
				tmp=params.readWholeTrackAsFirstSector;
				DDX_Check( pDX,	ID_TRACK,		tmp );
				params.readWholeTrackAsFirstSector=tmp!=0;
			}
			afx_msg void OnPaint(){
				// drawing
				// - base
				CDialog::OnPaint();
				// - drawing of curly brackets
				TUtils::WrapControlsByClosingCurlyBracketWithText( this, GetDlgItem(ID_LATENCY), GetDlgItem(ID_NUMBER2), NULL, 0 );
				TUtils::WrapControlsByClosingCurlyBracketWithText( this, GetDlgItem(ID_NONE), GetDlgItem(ID_SECTOR), _T("if error encountered"), 0 );
				TUtils::WrapControlsByClosingCurlyBracketWithText( this, GetDlgItem(ID_ZERO), GetDlgItem(ID_CYLINDER_N), _T("when formatting"), 0 );
			}
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				switch (msg){
					case WM_PAINT:
						OnPaint();
						return 0;
					case WM_NOTIFY:
						if (wParam==ID_AUTO && ((LPNMHDR)lParam)->code==NM_CLICK){
							// automatic determination of write latency values
							// . defining the Dialog
							class CLatencyAutoDeterminationDialog sealed:public CDialog{
							public:
								int floppyType,usAccuracy,nRepeats;

								CLatencyAutoDeterminationDialog(const CFDD *fdd,CWnd *parent)
									: CDialog(IDR_FDD_LATENCY,parent) , floppyType(-1) , usAccuracy(2) , nRepeats(3) {
									switch (fdd->floppyType){
										case TMedium::FLOPPY_DD: floppyType=0; break;
										case TMedium::FLOPPY_HD: floppyType=1; break;
									}
								}
								void PreInitDialog() override{
									CDialog::PreInitDialog(); // base
									TUtils::PopulateComboBoxWithSequenceOfNumbers( GetDlgItem(ID_ACCURACY)->m_hWnd, 1,NULL, 6,NULL );
									TUtils::PopulateComboBoxWithSequenceOfNumbers( GetDlgItem(ID_TEST)->m_hWnd, 1,_T("(worst)"), 9,_T("(best)") );
								}
								void DoDataExchange(CDataExchange *pDX) override{
									DDX_Radio( pDX, ID_FLOPPY_DD,	floppyType );
										DDV_MinMaxInt( pDX, floppyType, 0,1 );
									DDX_CBIndex( pDX, ID_ACCURACY,	usAccuracy );
									DDX_CBIndex( pDX, ID_TEST,		nRepeats );
								}
							} d(fdd,this);
							// . showing the Dialog and processing its result
							if (d.DoModal()==IDOK){
								__informationWithCheckableShowNoMore__( _T("Windows is NOT a real-time system! Computed latency will be valid only if you will use the floppy drive in very similar conditions as they were computed in (current conditions)!"), INI_MSG_LATENCY );
								if (TUtils::InformationOkCancel(_T("Insert an empty disk and hit OK."))){
									TLatencyParams lp( fdd, d.floppyType==0, 1+d.usAccuracy, 1+d.nRepeats );
									const TStdWinError err=	TBackgroundActionCancelable(
																__determineLatency_thread__,
																&lp,
																FDD_THREAD_PRIORITY_DEFAULT
															).CarryOut(d.nRepeats*4+1); // 4 = number of steps of a single attempt, 1 = computation of final latency values
									if (err==ERROR_SUCCESS){
										params.controllerLatency=lp.outControllerLatency;
										params.oneByteLatency=lp.out1ByteLatency;
										__exchangeLatency__( &CDataExchange(this,FALSE) );
									}else
										TUtils::FatalError(_T("Couldn't autodetermine"),err);
								}
							}
						}
						break;
					case WM_COMMAND:
						switch (wParam){
							case ID_RECOVER:
								// refreshing information on (inserted) floppy
								__refreshMediumInformation__();
								break;
							case ID_ZERO:
							case ID_CYLINDER_N:
								// adjusting possibility to edit the CalibrationStep according to selected option
								GetDlgItem(ID_NUMBER)->EnableWindow(wParam!=ID_ZERO);
								break;
						}
						break;
				}
				return CDialog::WindowProc(msg,wParam,lParam);
			}
		public:
			TParams params;

			CSettingDialog(CFDD *_fdd)
				// ctor
				: CDialog(IDR_FDD_ACCESS) , fdd(_fdd) , params(_fdd->params) {
			}
		} d(this);
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			params=d.params;
			__setSecondsBeforeTurningMotorOff__(params.nSecondsToTurningMotorOff);
			return true;
		}else
			return false;
	}

	void CFDD::EditSettings(){
		// displays dialog with editable settings and reflects changes made by the user into the Image's inner state
		__showSettingDialog__();
	}

	TStdWinError CFDD::Reset(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		// - displaying message
		__informationWithCheckableShowNoMore__( _T("Only 3.5\" internal drives mapped as \"A:\" are supported. To spare the floppy and drive, all activity is buffered.\nThe following applies:\n\n- Changes made to the floppy are saved only when you command so (Ctrl+S). If you don't save them, they will NOT appear on the disk!\n\n- Formatting destroys the content immediately."), INI_MSG_RESET );
		// - resetting
		const TStdWinError err=__reset__();
		#ifndef _DEBUG // checking Error only in Release mode (in Debug mode despite Error wanting to proceed up the setting dialog)
			if (err!=ERROR_SUCCESS)
				return err;
		#endif
		// - showing the settings and applying its results to the Drive access
		return __showSettingDialog__() ? ERROR_SUCCESS : ERROR_CANCELLED;
	}

	BYTE CFDD::__getMaximumSectorLengthCode__() const{
		// returns the maximum LengthCode given the actual FloppyType
		return 5+(floppyType==TMedium::FLOPPY_HD);
	}

	static BYTE __reportSectorVerificationError__(RCPhysicalAddress chs){
		TCHAR buf[100],tmp[30];
		::wsprintf(buf,_T("Track %d verification failed for sector with %s"),chs.GetTrackNumber(2),chs.sectorId.ToString(tmp));
		return TUtils::AbortRetryIgnore( buf, ::GetLastError(), MB_DEFBUTTON2, _T("For copy-protected schemes, simply retrying usually helps.") );
	}

	TStdWinError CFDD::__formatToOneLongVerifiedSector__(RCPhysicalAddress chs,BYTE fillerByte){
		// creates (and eventually verifies) a single long Sector with the given ID; returns Windows standard i/o error
		do{
			// . navigating the Head above corresponding Cylinder
			if (!fddHead.__seekTo__(chs.cylinder))
				return ::GetLastError();
			// . formatting Track to a single Sector
			__setWaitingForIndex__();
			FD_FORMAT_PARAMS fmt={	FD_OPTION_MFM, chs.head,
									1, // the Sector length doesn't matter, the important thing is to correctly create its ID Field
									1, 1, fillerByte
								};
			const PFD_ID_HEADER pih=fmt.Headers;
				__assign__( *pih, &chs.sectorId );
			DWORD nBytesTransferred;
			if (!::DeviceIoControl( _HANDLE, IOCTL_FDCMD_FORMAT_TRACK, &fmt,sizeof(fmt), NULL,0, &nBytesTransferred, NULL ))
				return ::GetLastError();
			// . verifying the single formatted Sector
			if (params.verifyFormattedTracks){
				// : writing FillerByte as test data
				FD_SHORT_WRITE_PARAMS swp={ __getUsableSectorLength__(pih->size), params.controllerLatency+1*params.oneByteLatency }; // "X*" = reserve to guarantee that really all test data written
				::DeviceIoControl( _HANDLE, IOCTL_FD_SET_SHORT_WRITE, &swp,sizeof(swp), NULL,0, &nBytesTransferred, NULL );
				FD_READ_WRITE_PARAMS rwp={ FD_OPTION_MFM|FD_OPTION_SK, chs.head, pih->cyl,pih->head,pih->sector,pih->size, pih->sector+1, 5, 0xff };
				if (!::DeviceIoControl( _HANDLE, IOCTL_FDCMD_WRITE_DATA, &rwp,sizeof(rwp), ::memset(dataBuffer,fillerByte,swp.length),__getOfficialSectorLength__(pih->size), &nBytesTransferred, NULL ))
					return ::GetLastError();
				// . reading test data
				::DeviceIoControl( _HANDLE, IOCTL_FDCMD_READ_DATA, &rwp,sizeof(rwp), dataBuffer,SECTOR_LENGTH_MAX, &nBytesTransferred, NULL ); // cannot use IF as test data always read with a CRC error in this case
				for( PCBYTE p=(PCBYTE)dataBuffer; swp.length && *p++==fillerByte; swp.length-- );
				if (swp.length) // error reading the test data
					if (::GetLastError()==ERROR_FLOPPY_ID_MARK_NOT_FOUND) // this is an error for which it usually suffices ...
						continue; // ... to repeat the formatting cycle
					else
						switch (__reportSectorVerificationError__(chs)){
							case IDABORT:	return ERROR_CANCELLED;
							case IDRETRY:	continue;
							case IDIGNORE:	break;
						}
			}
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
		( SYNCHRONIZATION_BYTES_COUNT + sizeof(TIdAddressMark) + sizeof(FD_ID_HEADER) + sizeof(TCrc) )

	#define NUMBER_OF_OCCUPIED_BYTES(nSectors,sectorLength,gap3,withoutLastGap3)\
		( nSectors*( NUMBER_OF_BYTES_OCCUPIED_BY_ID + GAP2_BYTES_COUNT + SYNCHRONIZATION_BYTES_COUNT + sizeof(TDataAddressMark) + sectorLength + sizeof(TCrc) + gap3 )  -  withoutLastGap3*gap3 )

	TStdWinError CFDD::FormatTrack(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte){
		// formats given Track {Cylinder,Head} to the requested NumberOfSectors, each with corresponding Length and FillerByte as initial content; returns Windows standard i/o error
		if (nSectors>FDD_SECTORS_MAX)
			return ERROR_BAD_COMMAND;
		if (!nSectors) // formatting to zero Sectors ...
			return UnformatTrack(cyl,head); // ... defined as unformatting the Track
		// - Head calibration
		if (params.calibrationStepDuringFormatting)
			if (cyl%params.calibrationStepDuringFormatting==0 && !head)
				if (fddHead.__calibrate__())
					fddHead.calibrated=true;
				else
error:				return ::GetLastError();
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
		if (nSectors==1 && referenceLengthCode>__getMaximumSectorLengthCode__())
			formatStyle=TFormatStyle::ONE_LONG_SECTOR;
		else{
			formatStyle=TFormatStyle::STANDARD; // assumption
			const WORD referenceLength=*bufferLength;
			PCSectorId pid=bufferId; PCWORD pLength=bufferLength; PCFdcStatus pFdcStatus=bufferFdcStatus;
			for( BYTE n=nSectors; n--; pid++,pLength++,pFdcStatus++ )
				if (pid->lengthCode!=referenceLengthCode || *pLength!=referenceLength || !*pLength || !pFdcStatus->IsWithoutError() || *pLength!=__getOfficialSectorLength__(pid->lengthCode)){
					formatStyle=TFormatStyle::CUSTOM;
					break;
				}
		}
		// - formatting using selected FormatStyle and eventually verifying the Track
		const TInternalTrack it( this, cyl, head, nSectors, bufferId );
		DWORD nBytesTransferred;
		switch (formatStyle){
			case TFormatStyle::STANDARD:{
				// . composing the structure
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
				for( TSector n=nSectors; n--; __assign__(*pih++,pId++) );
formatStandardWay:
				// . unformatting the Track
				__unformatInternalTrack__(cyl,head);
				// . formatting the Track
				if (!::DeviceIoControl( _HANDLE, IOCTL_FDCMD_FORMAT_TRACK, &fmt,sizeof(fmt), NULL,0, &nBytesTransferred, NULL ))
					goto error;
				// . verifying the Track (if requested to)
				if (params.verifyFormattedTracks)
					for( TSector n=0; n<nSectors; n++ ){
						const TPhysicalAddress chs={ cyl, head, bufferId[n] };
						TFdcStatus sr;
						__bufferSectorData__( chs, *bufferLength, &it, n, &sr );
						if (!sr.IsWithoutError())
							switch (__reportSectorVerificationError__(chs)){
								case IDABORT:	return ERROR_CANCELLED;
								case IDRETRY:	goto formatStandardWay;
								case IDIGNORE:	break;
							}
					}
				// . Track formatted successfully
				break;
			}
			case TFormatStyle::ONE_LONG_SECTOR:{
				// . unformatting the Track
				__unformatInternalTrack__(cyl,head);
				// . formatting the Track
				const TPhysicalAddress chs={ cyl, head, *bufferId };
				const TStdWinError err=__formatToOneLongVerifiedSector__( chs, fillerByte );
				if (err!=ERROR_SUCCESS)
					goto error;
				// . Track formatted successfully
				break;
			}
			case TFormatStyle::CUSTOM:{
				// . unformatting the Track
				__unformatInternalTrack__(cyl,head);
				// . verifying Track surface (if requested to) by writing maximum number of known Bytes to it and trying to read them back
				if (params.verifyFormattedTracks){
					const TPhysicalAddress chs={ cyl, head, {0,0,0,__getMaximumSectorLengthCode__()+1} };
					const TStdWinError err=__formatToOneLongVerifiedSector__( chs, fillerByte );
					if (err!=ERROR_SUCCESS)
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
						WORD nBytes,nMicroseconds;
					} interruption;

					WORD __getNumberOfNecessaryBytes__() const{
						// determines and returns the number of Bytes that are necessary to perform this Step
						return NUMBER_OF_OCCUPIED_BYTES(nSectorsOnTrack,__getOfficialSectorLength__(sectorLengthCode),gap3,true);
					}
					void __debug__() const{
						TCHAR buf[200];
						::wsprintf(buf,_T("sectorLengthCode=%d\nnSectorsOnTrack=%d\nnLastSectorsValid=%d\ngap3=%d\n,nBytes=%d\nnMicroseconds=%d"),
							sectorLengthCode,nSectorsOnTrack,nLastSectorsValid,gap3,interruption.nBytes,interruption.nMicroseconds
						);
						TUtils::Information(buf);
					}
				} formatPlan[(TSector)-1],*pFormatStep=formatPlan;
				WORD nBytesReserved; // reserved block of Bytes at the beginning of Track represents an area formatted in the next Step (as Sectors are formatted "backwards")
				BYTE n=nSectors;
				PCWORD pLength=bufferLength; PCFdcStatus pFdcStatus=bufferFdcStatus;
				if (!__mustSectorBeFormattedIndividually__(pFdcStatus)){
					// there is a sequence of Sectors at the beginning of Track that can be all formatted in a single Step
					pFormatStep->sectorLengthCode=__getSectorLengthCode__(*pLength);
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
					pFormatStep->interruption.nMicroseconds=params.controllerLatency+*bufferLength/2*params.oneByteLatency;
//pFormatStep->__debug__();
					nBytesReserved=pFormatStep++->__getNumberOfNecessaryBytes__();
				}else
					// right the first Sector in the Track must be formatted individually
					nBytesReserved=0;
				const WORD reservedSectorLength=__getOfficialSectorLength__(RESERVED_SECTOR_LENGTH_CODE);
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
TUtils::Information(buf);}
*/
					pFormatStep->gap3=gap3+deltaGap3;
					// : setting up the Interruption according to the requested i/o errors
					const TFdcStatus sr(*pFdcStatus++);
					WORD nBytesFormatted;
					if (sr.DescribesMissingDam()){
						// Sector misses its data part
						pFormatStep->interruption.nMicroseconds=params.controllerLatency+(GAP2_BYTES_COUNT+SYNCHRONIZATION_BYTES_COUNT-2)*params.oneByteLatency; // "-2" = damaging the A1A1A1 mark
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
						pFormatStep->interruption.nMicroseconds=params.controllerLatency+sectorLength/2*params.oneByteLatency;
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
//TUtils::Information(buf);}
				}
				bufferId-=nSectors;
formatCustomWay:
				// . unformatting the Track
				UnformatTrack(cyl,head);
				// . executing the PlanOfFormatting - decremental formatting (i.e. formatting "backwards")
				for( const TFormatStep *pfs=pFormatStep; pfs-->formatPlan; ){
					__setWaitingForIndex__();
					// : setting shortened formatting
					FD_SHORT_WRITE_PARAMS swp={ pfs->interruption.nBytes, pfs->interruption.nMicroseconds };
					::DeviceIoControl( _HANDLE, IOCTL_FD_SET_SHORT_WRITE, &swp,sizeof(swp), NULL,0, &nBytesTransferred, NULL );
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
							__assign__( buffer.fp.Headers[pfs->nSectorsOnTrack-pfs->nLastSectorsValid+n], &pfs->validSectorIDs[n] );
					if (!::DeviceIoControl( _HANDLE, IOCTL_FDCMD_FORMAT_TRACK, &buffer,sizeof(buffer), NULL,0, &nBytesTransferred, NULL ))
						return ::GetLastError();
				}
				// . verifying the Track (if requested to)
				if (params.verifyFormattedTracks)
					for( TSector n=0; n<nSectors; n++ ){
						const TPhysicalAddress chs={ cyl, head, bufferId[n] };
						TFdcStatus sr;
						__bufferSectorData__( chs, bufferLength[n], &it, n, &sr );
						if (bufferFdcStatus[n].DescribesMissingDam()^sr.DescribesMissingDam())
							switch (__reportSectorVerificationError__(chs)){
								case IDABORT:	return ERROR_CANCELLED;
								case IDRETRY:	goto formatCustomWay;
								case IDIGNORE:	break;
							}
					}
				// . Track formatted successfully
//TUtils::Information("formatted OK - ready to break");
				break;
			}
		}
		// - the Track structure is explicitly given after (successfull) formatting, so we can spare ourselves its eventual scanning
		if (!REFER_TO_TRACK(cyl,head)) // Track not scanned (due to verification turned off)
			REFER_TO_TRACK(cyl,head) = new TInternalTrack( this, cyl, head, nSectors, bufferId );
		// - it's not necessary to calibrate the Head for this Track
		fddHead.calibrated=true;
		return ERROR_SUCCESS;
	}

	bool CFDD::RequiresFormattedTracksVerification() const{
		// True <=> the Image requires its newly formatted Tracks be verified, otherwise False (and caller doesn't have to carry out verification)
		return params.verifyFormattedTracks;
	}

	TStdWinError CFDD::UnformatTrack(TCylinder cyl,THead head){
		// unformats given Track {Cylinder,Head}; returns Windows standard i/o error
		// - moving Head above the corresponding Cylinder
		if (!fddHead.__seekTo__(cyl))
error:		return ::GetLastError();
		// - unformatting (approached by formatting single long Sector that is longer than one spin of the floppy - after index pulse, it rewrites its own header)
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:{
				const BYTE lengthCode=__getSectorLengthCode__(16384); // 16kB long Sector that rewrites its own header
				FD_FORMAT_PARAMS fmt={	FD_OPTION_MFM, head, lengthCode, 1, 50, 0,
										{cyl,head,0,lengthCode}
									};
				if (::DeviceIoControl( _HANDLE, IOCTL_FDCMD_FORMAT_TRACK, &fmt,sizeof(fmt), NULL,0, &nBytesTransferred, NULL )!=0){
					__unformatInternalTrack__(cyl,head);
					return ERROR_SUCCESS;
				}else
					goto error;
			}
			default:
				ASSERT(FALSE);
				return ERROR_DEVICE_NOT_AVAILABLE;
		}
	}

	void CFDD::SetTitle(LPCTSTR){
		CDocument::SetTitle(FDD_A_LABEL);
	}

	void CFDD::SetPathName(LPCTSTR,BOOL){
		//CDocument::SetPathName(tmpFileName,FALSE);
	}
