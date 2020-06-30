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
		, calibrationStepDuringFormatting( app.GetProfileInt(INI_FDD,INI_CALIBRATE_FORMATTING,0) )
		, verifyFormattedTracks( app.GetProfileInt(INI_FDD,INI_VERIFY_FORMATTING,true)!=0 )
		, verifyWrittenData( app.GetProfileInt(INI_FDD,INI_VERIFY_WRITTEN_DATA,false)!=0 )
		, nSecondsToTurningMotorOff( app.GetProfileInt(INI_FDD,INI_MOTOR_OFF_SECONDS,2) ) // 0 = 1 second, 1 = 2 seconds, 2 = 3 seconds
		, readWholeTrackAsFirstSector(false) {
	}

	CFDD::TParams::~TParams(){
		// dtor
		app.WriteProfileInt( INI_FDD, INI_CALIBRATE_SECTOR_ERROR,calibrationAfterError );
		app.WriteProfileInt( INI_FDD, INI_CALIBRATE_FORMATTING,calibrationStepDuringFormatting );
		app.WriteProfileInt( INI_FDD, INI_MOTOR_OFF_SECONDS,nSecondsToTurningMotorOff );
		app.WriteProfileInt( INI_FDD, INI_VERIFY_FORMATTING,verifyFormattedTracks );
		app.WriteProfileInt( INI_FDD, INI_VERIFY_WRITTEN_DATA,verifyWrittenData );
	}







	#define _HANDLE		fddHead.handle
	#define DRIVER		fddHead.driver

	#define __REFER_TO_TRACK(fdd,cyl,head)\
				fdd->internalTracks[cyl*2+head] /* 2 = max number of Sides on a floppy */

	TStdWinError CFDD::TInternalTrack::TSectorInfo::__saveToDisk__(CFDD *fdd,const TInternalTrack *pit,BYTE nSectorsToSkip,bool verify){
		// saves this Sector to inserted floppy; returns Windows standard i/o error
		LOG_SECTOR_ACTION(&id,_T("TStdWinError CFDD::TInternalTrack::TSectorInfo::__saveToDisk__"));
		// - seeking the Head
		if (!fdd->fddHead.__seekTo__(pit->cylinder))
			return LOG_ERROR(::GetLastError());
		// - saving
		char nSilentRetrials=1;
		do{
			// : taking into account the NumberOfSectorsToSkip in current Track
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
					if (fdcStatus.reg2 & FDC_ST2_CRC_ERROR_IN_DATA)
						if ( err=fdd->__setTimeBeforeInterruptingTheFdc__(length) )
							return err;
					if (id.lengthCode>fdd->__getMaximumSectorLengthCode__())
						fdd->__setWaitingForIndex__();
					// . writing Sector
					FD_READ_WRITE_PARAMS rwp={ FD_OPTION_MFM, pit->head, id.cylinder,id.side,id.sector,id.lengthCode, id.sector+1, 1, 0xff };
					LOG_ACTION(_T("DeviceIoControl fdcCommand"));
					err=::DeviceIoControl( fdd->_HANDLE, fdcCommand, &rwp,sizeof(rwp), ::memcpy(fdd->dataBuffer,data,length),__getOfficialSectorLength__(id.lengthCode), &nBytesTransferred, nullptr )!=0
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
						__REFER_TO_TRACK(fdd,cyl,head)=nullptr; // detaching the Track internal representation for it to be not destroyed during reformatting of the Track
						err=fdd->FormatTrack(	cyl, head, pit->nSectors, bufferId, bufferLength, bufferStatus,
												FDD_SECTOR_GAP3_STD, // if gap too small (e.g. 10) it's highly likely that sectors would be missed in a single disk revolution (so for instance, reading 9 sectors would require 9 disk revolutions)
												fdd->dos->properties->sectorFillerByte
											);
						if (err!=ERROR_SUCCESS){ // if formatting failed ...
terminateWithError:			fdd->__unformatInternalTrack__(cyl,head); // disposing any new InternalTrack representation
							__REFER_TO_TRACK(fdd,cyl,head)=(PInternalTrack)pit; // re-attaching the original InternalTrack representation
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
						fdd->__unformatInternalTrack__(cyl,head); // disposing any new InternalTrack representation
						__REFER_TO_TRACK(fdd,cyl,head)=(PInternalTrack)pit; // re-attaching the original InternalTrack representation
						// . writing the 0..(K-1)-th Sectors back to the above reformatted Track, leaving K+1..N-th Sectors unwritten (caller's duty); we re-attempt to write this K-th Sector in the next iteration
						for( TSector s=0; s<nSectorsToSkip; s++ ){
							const TPhysicalAddress chs={ cyl, head, pit->sectors[s].id };
							if ( err=pit->sectors[s].__saveToDisk__(fdd,pit,s,verify) ) // if Sector not writeable even after reformatting the Track ...
								return LOG_ERROR(err); // ... there's nothing else to do but terminating with Error
						}
						// . trying to write this K-th Sector once more, leaving K+1..N-th Sectors unwritten (caller's duty)
						continue;
					}
				}
			// : verifying the writing
			else
				if (verify)
					switch (__verifySaving__(fdd,pit,nSectorsToSkip)){
						case IDIGNORE:	break;
						case IDABORT:	return LOG_ERROR(ERROR_CANCELLED);
						case IDRETRY:	continue;
					}
			break;
		}while (true);
		return ERROR_SUCCESS;
	}

	BYTE CFDD::TInternalTrack::TSectorInfo::__verifySaving__(const CFDD *fdd,const TInternalTrack *pit,BYTE nSectorsToSkip){
		// verifies the saving made by during calling to __saveToDisk__
		LOG_SECTOR_ACTION(&id,_T("verifying the writing"));
		const TPhysicalAddress chs={ pit->cylinder, pit->head, id };
		TFdcStatus sr;
		fdd->__bufferSectorData__( chs, length, pit, nSectorsToSkip, &sr );
		if (fdcStatus.DescribesDataFieldCrcError()^sr.DescribesDataFieldCrcError() // Data written with/without error than desired
			||
			::memcmp(data,fdd->dataBuffer,length) // Data written with error
		){
			TCHAR buf[80];
			::wsprintf( buf, _T("Verification failed for sector with %s on Track %d."), (LPCTSTR)id.ToString(), chs.GetTrackNumber(2) );
			const BYTE result=Utils::AbortRetryIgnore( buf, MB_DEFBUTTON2 );
			modified=result==IDIGNORE; // saved successfully if commanded to ignore any errors
			return result;
		}else{
			modified=false; // saved successfully, so the Sector is no longer Modified
			return IDIGNORE; // do nothing after return
		}
	}

	typedef WORD TCrc;

	#define ALLOCATE_SECTOR_DATA(length)	(PSectorData)::malloc(length)
	#define FREE_SECTOR_DATA(data)			::free(data)

	#define SECTOR_LENGTH_MAX	16384

	static const TFdcStatus TrackRawContentIoError(FDC_ST1_DATA_ERROR,FDC_ST2_CRC_ERROR_IN_DATA);

	CFDD::TInternalTrack::TInternalTrack(const CFDD *fdd,TCylinder cyl,THead head,TSector _nSectors,PCSectorId bufferId,PCINT sectorStartsMicroseconds)
		// ctor
		// - initialization
		: cylinder(cyl) , head(head)
		, nSectors(_nSectors) , sectors((TSectorInfo *)::ZeroMemory(::calloc(_nSectors,sizeof(TSectorInfo)),_nSectors*sizeof(TSectorInfo))) {
		TInternalTrack::TSectorInfo *psi=sectors;
		for( BYTE s=0; s<nSectors; psi++->seqNum=s++ ){
			psi->length=fdd->__getUsableSectorLength__(( psi->id=*bufferId++ ).lengthCode );
			if (sectorStartsMicroseconds>(PCINT)0x100) // if start times provided (that is, if no Gap3 information from <0;255> provided) ...
				psi->startMicroseconds=*sectorStartsMicroseconds++; // ... they are used
			else // if no start times provided (that is, if just Gap3 information from <0;255> provided) ...
				if (s) // ... then simply inferring them
					psi->startMicroseconds=	sectors[s-1].endMicroseconds
											+
											(BYTE)sectorStartsMicroseconds * fdd->fddHead.profile.oneByteLatency;	// default inter-sector Gap3 length in microseconds
				else
					psi->startMicroseconds=0; // the first Sector starts immediately after the index pulse
			psi->endMicroseconds=	psi->startMicroseconds // inferring end of Sector from its lengths and general IBM track layout specification
									+
									(	12	// N Bytes 0x00 (see IBM's track layout specification)
										+
										3	// 0xA1A1A1 mark with corrupted clock
										+
										1	// Sector ID Address Mark
										+
										4	// Sector ID
										+
										sizeof(TCrc) // Sector ID CRC
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
										sizeof(TCrc) // data CRC
									) *
									fdd->fddHead.profile.oneByteLatency; // usually 32 microseconds
		}
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
		LOG_ACTION(_T("TStdWinError CFDD::TInternalTrack::__saveRawContentToDisk__"));
		if (!rawContent.data)
			return LOG_ERROR(ERROR_INVALID_DATA);
		// - preparing error message (should any error occur)
		TPhysicalAddress chs={ cyl, head }; // ID will be below set to the first Sector on the Track
		TCHAR error[80];
		::wsprintf( error, _T("Cannot save raw content of Track %d"), chs.GetTrackNumber(2) ); // 2 = max number of Sides on a floppy
		// - extracting first Sector ID
		PCSectorData pIdField=nullptr; // assumption (ID Field not found and RawContent is not valid)
		bool idFieldOk=false;
		PCSectorData p=rawContent.data;
		for( const PCSectorData pMax=p+500; p<pMax; p++ )
			if (const BYTE nBytesOfGap=TInternalTrack::TRawContent::__containsBufferGap__(p))
				if (const BYTE nBytesOfSectorId=TInternalTrack::TRawContent::__containsBufferSectorId__( p+=nBytesOfGap ,&chs.sectorId,&idFieldOk)){
					pIdField = p+=nBytesOfSectorId; break;
				}
		if (!pIdField){ // ID Field not found
			Utils::Information(error,_T("Cannot find first sector ID in raw content."),SAVING_END);
			return LOG_ERROR(ERROR_REQUEST_REFUSED);
		}/*else if (chs.sectorId!=sectors[0].id){ // ID Field found but doesn't match the first Sector on the Track
			TCHAR cause[150],id1[30],id1raw[30];
			::wsprintf( cause, _T("Earlier scanned first sector %s does not match the current first sector %s in the raw content."), sectors[0].id.ToString(id1), chs.__vratCisloStopy__(2), chs.sectorId.ToString(id1raw) );
			Utils::Information(error,cause,SAVING_END);
			return LOG_ERROR(ERROR_CANCELLED);
		}*/
		// - extracting Data Address Mark (DAM) of the first Sector
		PCSectorData pDam=nullptr; // assumption (DAM not found and RawContent not valid)
		for( const PCSectorData pMax=p+100; p<pMax; p++ )
			if (const BYTE nBytesOfGap=TInternalTrack::TRawContent::__containsBufferGap__(p))
				if ((*(PDWORD)(p+=nBytesOfGap)&SECTOR_SYNCHRONIZATION_MASK)==SECTOR_SYNCHRONIZATION){
					pDam = p+3; break;
				}
		if (!pDam || *pDam!=TDataAddressMark::DATA_RECORD && *pDam!=TDataAddressMark::DELETED_DATA_RECORD && *pDam!=TDataAddressMark::DEFECTIVE_DATA_RECORD){ // DAM not found
			Utils::Information(error,_T("Cannot find first sector DAM in raw content (first sector in the track must have a DAM, others don't)."),SAVING_END);
			return LOG_ERROR(ERROR_REQUEST_REFUSED);
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
			const TSectorInfo si={	// if uncommented, revise the correspondence to actual structure members
									rawContent.id,
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
											3, 60, 0xe5,
											TFdIdHeader(sectors[0].id)
										};
					FD_SHORT_WRITE_PARAMS swp={ sizeof(FD_ID_HEADER), fdd->params.controllerLatency+(sizeof(TCrc)+1)*fdd->params.oneByteLatency }; // "+1" = just to be sure
					::DeviceIoControl( fdd->_HANDLE, IOCTL_FD_SET_SHORT_WRITE, &swp,sizeof(swp), nullptr,0, &nBytesTransferred, nullptr );
					::DeviceIoControl( fdd->_HANDLE, IOCTL_FDCMD_FORMAT_TRACK, &fp,sizeof(fp)+8, nullptr,0, &nBytesTransferred, nullptr ); // cannot use IF because DeviceIoControl returns an error when formatting with bad CRC
					break;
				}
				default:
					ASSERT(FALSE);
					return LOG_ERROR(ERROR_DEVICE_NOT_AVAILABLE);
			}
Utils::Information("--- EVERYTHING OK ---");
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
		TCrc crc=GetCrc16Ccitt(buffer-8,8);
		if (pFdcStatus->DescribesDataFieldCrcError()) crc=~crc;
		*(TCrc *)buffer=crc, buffer+=sizeof(TCrc); // CRC
	}
	BYTE CFDD::TInternalTrack::TRawContent::__containsBufferSectorId__(PCSectorData buffer,TSectorId *outId,bool *outCrcOk){
		// returns the number of Bytes of a Sector ID recognized at the front of the Buffer; returns 0 if no Sector ID recognized
		if (*(PDWORD)buffer==SECTOR_SYNC_ID_ADDRESS_MARK){
			buffer+=sizeof(DWORD);
			outId->cylinder=*buffer++, outId->side=*buffer++, outId->sector=*buffer++, outId->lengthCode=*buffer++;
			*outCrcOk=GetCrc16Ccitt(buffer-8,8)==*(TCrc *)buffer;
			return 3+1+4+2; // 0xA1A1A1 synchronization + ID Address Mark + ID itself + CRC
		}else
			return 0;
	}

	void CFDD::TInternalTrack::TRawContent::__generateSectorDefaultData__(PSectorData &buffer,TDataAddressMark dam,WORD sectorLength,BYTE fillerByte,PCFdcStatus pFdcStatus){
		// generates into the Buffer a Sector with a given Length and with the FillerByte as the default content
		*(PDWORD)buffer= SECTOR_SYNCHRONIZATION | dam<<24, buffer+=sizeof(DWORD);
		buffer=(PSectorData)::memset( buffer, fillerByte, sectorLength )+sectorLength;
		sectorLength+=3+1; // 0xA1A1A1 synchronization + DAM
		TCrc crc=GetCrc16Ccitt(buffer-sectorLength,sectorLength);
		if (pFdcStatus->DescribesDataFieldCrcError()) crc=~crc;
		*(TCrc *)buffer=crc, buffer+=sizeof(TCrc); // CRC
	}

	CFDD::TInternalTrack::TRawContent::TRawContent()
		// ctor
		: data(nullptr) // Null <=> RawContent not available
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
		// ctor
		: controllerLatency(86)
		, oneByteLatency(32)
		, gap3Latency( .8f * oneByteLatency*FDD_SECTOR_GAP3_STD ) { // ".8" = giving the FDC 20% tolerance for Gap3
	}

	static void GetFddProfileName(PTCHAR buf,TCHAR driveLetter,TMedium::TType floppyType){
		::wsprintf( buf, INI_FDD _T("_%c_%d"), driveLetter, floppyType );
	}

	void CFDD::TFddHead::TProfile::Load(TCHAR driveLetter,TMedium::TType floppyType){
		// loads the Profile for specified Drive and FloppyType
		TCHAR iniSection[16];
		GetFddProfileName( iniSection, driveLetter, floppyType );
		controllerLatency=app.GetProfileInt( iniSection, INI_LATENCY_CONTROLLER, 86000 )/1000.0f;
		oneByteLatency=app.GetProfileInt( iniSection, INI_LATENCY_1BYTE, 32000 )/1000.0f;
		gap3Latency=app.GetProfileInt( iniSection, INI_LATENCY_GAP3, FDD_SECTOR_GAP3_STD*4/5*32000 )/1000.0f; // "4/5" = giving the FDC 20% tolerance for Gap3
	}

	void CFDD::TFddHead::TProfile::Save(TCHAR driveLetter,TMedium::TType floppyType) const{
		// saves the Profile for specified Drive and FloppyType
		TCHAR iniSection[16];
		GetFddProfileName( iniSection, driveLetter, floppyType );
		app.WriteProfileInt( iniSection, INI_LATENCY_CONTROLLER, controllerLatency*1000 );
		app.WriteProfileInt( iniSection, INI_LATENCY_1BYTE, oneByteLatency*1000 );
		app.WriteProfileInt( iniSection, INI_LATENCY_GAP3, gap3Latency*1000 );
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
							FD_RELATIVE_SEEK_PARAMS rsp={ FD_OPTION_MT|FD_OPTION_DIR, 0, (cyl-position)<<doubleTrackStep };
							LOG_ACTION(_T("DeviceIoControl FD_RELATIVE_SEEK_PARAMS"));
							seeked=::DeviceIoControl( handle, IOCTL_FDCMD_RELATIVE_SEEK, &rsp,sizeof(rsp), nullptr,0, &nBytesTransferred, nullptr )!=0;
							LOG_BOOL(seeked);
						}else{
							FD_SEEK_PARAMS sp={ cyl<<doubleTrackStep, 0 };
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
		TMedium::FLOPPY_ANY, // supported Media
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
		// - creating a temporary file in order to not break the Document-View architecture
		TCHAR tmpFileName[MAX_PATH];
		::GetTempPath(MAX_PATH,tmpFileName);
		::GetTempFileName( tmpFileName, nullptr, FALSE, tmpFileName );
		const CFile fTmp( m_strPathName=tmpFileName, CFile::modeCreate );
		// - connecting to the Drive
		__reset__();
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
		// - deleting the temporary file (created in ctor to not break the Document-View architecture)
		CFile::Remove(m_strPathName);
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
				static const BYTE RecognizeInsertedFloppy=FALSE;
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



	#define REFER_TO_TRACK(cyl,head)	__REFER_TO_TRACK(this,cyl,head)

	CFDD::PInternalTrack CFDD::__getScannedTrack__(TCylinder cyl,THead head) const{
		// returns Internal representation of the Track
		return	cyl<FDD_CYLINDERS_MAX
				? REFER_TO_TRACK(cyl,head)
				: nullptr;
	}

	void CFDD::__unformatInternalTrack__(TCylinder cyl,THead head){
		// disposes buffered InternalTrack
		if (const PInternalTrack tmp=__getScannedTrack__(cyl,head))
			delete tmp, REFER_TO_TRACK(cyl,head)=nullptr;
	}

	void CFDD::__freeInternalTracks__(){
		// disposes all InternalTracks
		LOG_ACTION(_T("void CFDD::__freeInternalTracks__"));
		for( TCylinder cyl=0; cyl<FDD_CYLINDERS_MAX; cyl++ )
			__unformatInternalTrack__(cyl,0), __unformatInternalTrack__(cyl,1);
	}




	BOOL CFDD::OnOpenDocument(LPCTSTR){
		// True <=> Image opened successfully, otherwise False
		return TRUE; // always successfull; failure may arise later on when attempting to access the Drive in exclusive mode
	}



	TStdWinError CFDD::SaveTrack(TCylinder cyl,THead head){
		// saves the specified Track to the inserted Medium; returns Windows standard i/o error
		LOG_TRACK_ACTION(cyl,head,_T("UINT CFDD::SaveTrack"));
		if (TInternalTrack *const pit=__getScannedTrack__(cyl,head)){
			// . saving RawContent of the Track
			/*if (pid->rawContent.bModified){
				const TStdWinError err=pid->__saveRawContentToDisk__( up.fdd, cyl, head );
				if (err==ERROR_SUCCESS){
					pid->rawContent.modified=false;
				}else
					return pAction->TerminateWithError(err);
			}*/
			// . saving (and verifying) data of Track's all Modified Sectors
			bool allSectorsProcessed;
			do{
				// : saving
				BYTE justSavedSectors[(TSector)-1];
				::ZeroMemory(justSavedSectors,pit->nSectors);
				do{
					allSectorsProcessed=true; // assumption
					int lastSectorEndMicroseconds=INT_MIN/2;
					for( TSector n=0; n<pit->nSectors; n++ ){
						TInternalTrack::TSectorInfo &si=pit->sectors[n];
						if (si.modified && !justSavedSectors[n]){
							if (si.startMicroseconds-lastSectorEndMicroseconds>=fddHead.profile.gap3Latency) // sufficient distance between this and previously saved Sectors, so both of them can be processed in a single disk revolution
								if (const TStdWinError err=si.__saveToDisk__( this, pit, n, false )) // False = verification carried out below
									return err;
								else{
									si.modified=params.verifyWrittenData; // no longer Modified if Verification turned off
									lastSectorEndMicroseconds=si.endMicroseconds;
									justSavedSectors[n]=true;
								}
							allSectorsProcessed=false; // will need one more cycle iteration to eventually find out that all Sectors are processed OK
						}
					}
				}while (!allSectorsProcessed);
				// : verification
				do{
					allSectorsProcessed=true; // assumption
					int lastSectorEndMicroseconds=INT_MIN/2;
					for( TSector n=0; n<pit->nSectors; n++ ){
						TInternalTrack::TSectorInfo &si=pit->sectors[n];
						if (si.modified)
							if (si.startMicroseconds-lastSectorEndMicroseconds>=fddHead.profile.gap3Latency){ // sufficient distance between this and previously saved Sectors, so both of them can be processed in a single disk revolution
								if (si.__verifySaving__( this, pit, n )==IDABORT)
									return ERROR_CANCELLED;
								lastSectorEndMicroseconds=si.endMicroseconds;
								allSectorsProcessed=false; // will need one more cycle iteration to eventually find out that all Sectors are processed OK
							}
					}
				}while (!allSectorsProcessed);
			}while (!allSectorsProcessed);
		}
		return ERROR_SUCCESS;
	}


	struct TSaveParams sealed{
		CFDD *const fdd;

		TSaveParams(CFDD *fdd)
			: fdd(fdd) {
		}
	};
	UINT AFX_CDECL CFDD::__save_thread__(PVOID _pCancelableAction){
		// thread to save InternalTracks (particularly their Modified Sectors) on inserted floppy
		LOG_ACTION(_T("UINT CFDD::__save_thread__"));
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		const TSaveParams sp=*(TSaveParams *)pAction->GetParams();
		pAction->SetProgressTarget(FDD_CYLINDERS_MAX);
		for( TCylinder cyl=0; cyl<FDD_CYLINDERS_MAX; pAction->UpdateProgress(++cyl) )
			for( THead head=0; head<2; head++ ){ // 2 = max number of Sides on a floppy
				if (pAction->IsCancelled()) return LOG_ERROR(ERROR_CANCELLED);
				if (const TStdWinError err=sp.fdd->SaveTrack(cyl,head))
					return pAction->TerminateWithError(err);
			}
		return ERROR_SUCCESS;
	}
	BOOL CFDD::OnSaveDocument(LPCTSTR){
		// True <=> this Image has been successfully saved, otherwise False
		const TStdWinError err=	CBackgroundActionCancelable(
									__save_thread__,
									&TSaveParams( this ),
									FDD_THREAD_PRIORITY_DEFAULT
								).Perform();
		::SetLastError(err);
		if (err==ERROR_SUCCESS){
			m_bModified=FALSE;
			return true;
		}else
			return false;
	}


	TCylinder CFDD::GetCylinderCount() const{
		// determines and returns the actual number of Cylinders in the Image
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		LOG_ACTION(_T("TCylinder CFDD::GetCylinderCount"));
		return	GetNumberOfFormattedSides(0) // if zeroth Cylinder exists ...
				? FDD_CYLINDERS_MAX>>fddHead.doubleTrackStep // ... then it's assumed that there is the maximum number of Cylinders available (the actual number may be adjusted by systematically scanning the Tracks)
				: 0; // ... otherwise the floppy is considered not formatted
	}

	THead CFDD::GetNumberOfFormattedSides(TCylinder cyl) const{
		// determines and returns the number of Sides formatted on given Cylinder; returns 0 iff Cylinder not formatted
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		LOG_CYLINDER_ACTION(cyl,_T("THead CFDD::GetNumberOfFormattedSides"));
		if (ScanTrack(cyl,1)!=0)
			return 2;
		else
			return ScanTrack(cyl,0)!=0;
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
							TSectorId bufferId[(TSector)-1]; int sectorTimes[(TSector)-1];
							for( BYTE n=0; n<sectors.n; n++ )
								bufferId[n]=sectors.header[n], sectorTimes[n]=sectors.header[n].reltime;
							return REFER_TO_TRACK(cyl,head) = new TInternalTrack( this, cyl, head, sectors.n, bufferId, sectorTimes );
						}
						default:
							ASSERT(FALSE);
							break;
					}
				}
		// - Track cannot be scanned
		return nullptr;
	}

	TSector CFDD::ScanTrack(TCylinder cyl,THead head,PSectorId bufferId,PWORD bufferLength,PINT startTimesMicroseconds,PBYTE pAvgGap3) const{
		// returns the number of Sectors found in given Track, and eventually populates the Buffer with their IDs (if Buffer!=Null); returns 0 if Track not formatted or not found
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (const PInternalTrack pit=((CFDD *)this)->__scanTrack__(cyl,head)){
			// Track managed to be scanned
			if (const bool rawDumpExists= params.readWholeTrackAsFirstSector && pit->__canRawDumpBeCreated__()){
				if (bufferId)
					*bufferId++=pit->rawContent.id;
				if (bufferLength)
					*bufferLength++=__getUsableSectorLength__(pit->rawContent.id.lengthCode);
				if (startTimesMicroseconds)
					*startTimesMicroseconds++=0; // not applicable, so "some" sensible value
			}
			const TInternalTrack::TSectorInfo *psi=pit->sectors;
			for( TSector s=0; s<pit->nSectors; s++,psi++ ){
				if (bufferId)
					*bufferId++=psi->id;
				if (bufferLength)
					*bufferLength++=psi->length;
				if (startTimesMicroseconds)
					*startTimesMicroseconds++=psi->startMicroseconds;
			}
			if (pAvgGap3)
				if (pit->nSectors>1){
					int usSum=0; // sum of Gap3 Microseconds
					const TInternalTrack::TSectorInfo *psi=pit->sectors;
					for( TSector s=0; s<pit->nSectors-1; usSum-=psi->endMicroseconds,s++,psi++,usSum+=psi->startMicroseconds );
					*pAvgGap3=usSum/((pit->nSectors-1)*fddHead.profile.oneByteLatency);
				}else
					*pAvgGap3=FDD_SECTOR_GAP3_STD;
			return pit->nSectors;
		}else
			// Track failed to be scanned
			return 0;
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

	TStdWinError CFDD::__setTimeBeforeInterruptingTheFdc__(WORD nDataBytesBeforeInterruption,WORD nMicrosecondsAfterLastDataByteWritten) const{
		// registers a request to interrupt the following write/format command after specified NumberOfBytes plus additional NumberOfMicrosends; returns Windows standard i/o error
		LOG_ACTION(_T("TStdWinError CFDD::__setTimeBeforeInterruptingTheFdc__"));
		DWORD nBytesTransferred;
		switch (DRIVER){
			case DRV_FDRAWCMD:{
				FD_SHORT_WRITE_PARAMS swp={ nDataBytesBeforeInterruption, nMicrosecondsAfterLastDataByteWritten };
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
		// registers a request to interrupt the following write/format command after specified NumberOfBytes plus additional NumberOfMicrosends; returns Windows standard i/o error
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

	void CFDD::GetTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses){
		// populates output buffers with specified Sectors' data, usable lengths, and FDC statuses; ALWAYS attempts to buffer all Sectors - caller is then to sort out eventual read errors (by observing the FDC statuses); caller can call ::GetLastError to discover the error for the last Sector in the input list
		ASSERT( outBufferData!=nullptr && outBufferLengths!=nullptr && outFdcStatuses!=nullptr );
		// - initializing the output buffers with data retrieval failure (assumption)
		::ZeroMemory( outBufferData, nSectors*sizeof(PSectorData) );
		for( TSector i=nSectors; i>0; outFdcStatuses[--i]=TFdcStatus::SectorNotFound );
		// - getting the real data for the Sectors
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		LOG_TRACK_ACTION(cyl,head,_T("void CFDD::GetTrackData"));
		if (const PInternalTrack pit=__scanTrack__(cyl,head)){
			// . getting Track's RawContent
			/* // TODO: separate into a new method, e.g. virtual CImage::GetTrackRawContent, when implementing support for Kryoflux
			if (params.readWholeTrackAsFirstSector && pit->__canRawDumpBeCreated__())
				if (*bufferId==pit->rawContent.id){
					if (!pit->rawContent.data){ // Track RawContent not yet read
						// : buffering each Sector in the Track (should the RawContent be Modified and subsequently saved - in such case, the RawContent must be saved first before saving any of its Sectors)
						for( BYTE i=0; i<pit->nSectors; ){
							const TPhysicalAddress tmp={ chs.cylinder, chs.head, pit->sectors[i].id };
							GetSectorData( tmp, ++i, true, sectorLength, pFdcStatus ); // "1+i" = the first Sector represents the RawContent
						}
						// : reading the Track
						DWORD nBytesTransferred;
						const PCSectorId pid=&pit->sectors[0].id;
						FD_READ_WRITE_PARAMS rwp={ FD_OPTION_MFM|FD_OPTION_SK, head, pid->cylinder,pid->side,pid->sector,7, pid->sector+1, 1, 0xff }; // 7 = reads 16384 Bytes beginning with the first Data Field
						if (::DeviceIoControl( _HANDLE, IOCTL_FDCMD_READ_TRACK, &rwp,sizeof(rwp), dataBuffer,SECTOR_LENGTH_MAX, &nBytesTransferred, nullptr )!=0){
							// Track read - reconstructing the gaps and ID that preceed the first physical Sector (because reading of the Track begun from its data but the ID Field itself wasn't read)
							PSectorData pData = pit->rawContent.data = ALLOCATE_SECTOR_DATA(pit->rawContent.length128);
							TInternalTrack::TRawContent::__generateGap__(pData,40);
							TInternalTrack::TRawContent::__generateSectorId__(pData,pid,&TFdcStatus::WithoutError);
							TInternalTrack::TRawContent::__generateGap__(pData,22);
							TInternalTrack::TRawContent::__generateSectorDefaultData__( pData, TDataAddressMark::DATA_RECORD, 0, 0, &TFdcStatus::WithoutError );
							::memcpy( pData-sizeof(TCrc), dataBuffer, __getUsableSectorLength__(pit->rawContent.id.lengthCode) ); // assumed that UsableSectorLength < OfficialSectorLength (and thus not written outside allocated memory)
						}//else
							//return nullptr; // commented out as it holds that "pid->rawContent.data==Null"
					}
					bufferId++, nSectors--;
					*outBufferData++=pit->rawContent.data;
					*outBufferLengths++=__getUsableSectorLength__(pit->rawContent.id.lengthCode);
					*outFdcStatuses++=TFdcStatus::WithoutError;
				}
			//*/
			// . Planning the requested Sectors retrieval
			#ifdef LOGGING_ENABLED			
				TCHAR buf[4000];
				for( TCHAR n=0,*p=buf; n<pit->nSectors; n++ ){
					const int i=::wsprintf(p,_T("%d:<%d,%d> "),n,pit->sectors[n].startMicroseconds,pit->sectors[n].endMicroseconds);
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
				int lastSectorEndMicroseconds=INT_MIN/2;
				for( TSector n=0; n<pit->nSectors; n++ ){
					TInternalTrack::TSectorInfo &si=pit->sectors[n];
					for( TSector s=0; s<nSectors; s++ )
						if (!alreadyPlannedSectors[s] && bufferId[s]==si.id) // one of the Sectors requested to read
							if (si.startMicroseconds-lastSectorEndMicroseconds>=fddHead.profile.gap3Latency){ // sufficient distance between this and previously read Sectors, so both of them can be read in a single disk revolution
								planEnd->psi=&si, planEnd->indexIntoOutputBuffers=s;
								planEnd++;
								lastSectorEndMicroseconds=si.endMicroseconds;
								alreadyPlannedSectors[s]=true;
								break;
							}
				}
			}
			// . executing the above composed Plan
			for( const TPlanStep *pPlanStep=plan; pPlanStep<planEnd; pPlanStep++ ){
				TInternalTrack::TSectorInfo *const psi=pPlanStep->psi;
				const BYTE index=pPlanStep->indexIntoOutputBuffers;
				const WORD length = outBufferLengths[index] = psi->length;
				// : if Data already read WithoutError, returning them
				if (psi->data || psi->fdcStatus.DescribesMissingDam()) // A|B, A = some data exist, B = reattempting to read the DAM-less Sector only if automatic recovery desired
					if (psi->fdcStatus.IsWithoutError() || !silentlyRecoverFromErrors){ // A|B, A = returning error-free data, B = settling with any data if automatic recovery not desired
returnData:				outFdcStatuses[index]=psi->fdcStatus;
						outBufferData[index]=psi->data;
						continue;
					}else // disposing previous erroneous Data
						FREE_SECTOR_DATA(psi->data), psi->data=nullptr;
				// : seeking Head to the given Cylinder
				if (!fddHead.__seekTo__(cyl))
					return; // Sectors cannot be found as Head cannot be seeked
				// : initial attempt to retrieve the Data
				if (!__bufferSectorData__(cyl,head,&psi->id,length,pit,bufferNumbersOfSectorsToSkip[index],&psi->fdcStatus))
					continue; // if a Sector with given ID physically not found in the Track, proceed with the next Planned Sector
				// : recovering from errors
				if (!psi->fdcStatus.IsWithoutError() && !psi->modified) // no Data, or Data with errors for a not-yet-Modified Sector
					if (silentlyRecoverFromErrors && !fddHead.calibrated)
						if (fddHead.__calibrate__() && fddHead.__seekTo__(cyl)){
							fddHead.calibrated=params.calibrationAfterError!=TParams::TCalibrationAfterError::FOR_EACH_SECTOR;
							__bufferSectorData__(cyl,head,&psi->id,length,pit,bufferNumbersOfSectorsToSkip[index],&psi->fdcStatus);
						}
				if (!psi->fdcStatus.DescribesMissingDam())
					psi->data=(PSectorData)::memcpy( ALLOCATE_SECTOR_DATA(length), dataBuffer, length );
				goto returnData; // returning (any) Data
			}
		}else
			LOG_MESSAGE(_T("Track not found"));
		// - outputting the result
		::SetLastError( outBufferData[nSectors-1] ? ERROR_SUCCESS : ERROR_SECTOR_NOT_FOUND );
	}

	TStdWinError CFDD::MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus){
		// marks Sector with specified PhysicalAddress as "dirty", plus sets it the given FdcStatus; returns Windows standard i/o error
		LOG_SECTOR_ACTION(&chs.sectorId,_T("TStdWinError CFDD::MarkSectorAsDirty"));
		EXCLUSIVELY_LOCK_THIS_IMAGE();
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
							TCHAR buf[200];
							::wsprintf( buf, _T("Not all errors can be reproduced on a real floppy for sector with %s."), (LPCTSTR)chs.sectorId.ToString() );
							Utils::Information(buf);
							return LOG_ERROR(ERROR_BAD_COMMAND);
						}

						break;
					}
				}else
					nSectorsToSkip--;
		}
		return ERROR_SUCCESS;
	}

	TStdWinError CFDD::__setDataTransferSpeed__(TMedium::TType _floppyType){
		// sets TransferSpeed for given FloppyType; returns Windows standard i/o error
		DWORD nBytesTransferred;
		BYTE transferSpeed;
		#ifdef LOGGING_ENABLED
			LOG_ACTION(_T("TStdWinError CFDD::__setDataTransferSpeed__"));
			LOG_MESSAGE(TMedium::GetDescription(_floppyType));
		#endif
		switch (_floppyType){
			case TMedium::FLOPPY_DD_350:
				// 3.5" 2DD floppy
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
			case TMedium::FLOPPY_HD_350:
				// 3.5" HD floppy
				switch (DRIVER){
					case DRV_FDRAWCMD:
						transferSpeed=FD_RATE_500K;
						goto fdrawcmd;
					default:
						ASSERT(FALSE);
						break;
				}
				break;
			case TMedium::FLOPPY_DD_525:
				// 5.25" HD floppy
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

	TStdWinError CFDD::__setAndEvaluateDataTransferSpeed__(TMedium::TType _floppyType){
		// sets TransferSpeed for given FloppyType and checks suitability by scanning zeroth Track; returns Windows standard i/o error
		LOG_ACTION(_T("TStdWinError CFDD::__setAndEvaluateDataTransferSpeed__"));
		// - setting
		if (const TStdWinError err=__setDataTransferSpeed__(_floppyType))
			return LOG_ERROR(err);
		// - scanning zeroth Track - if it can be read, we have set the correct transfer speed
		const PInternalTrack pit0=REFER_TO_TRACK(0,0); // backing up original Track, if any
			REFER_TO_TRACK(0,0)=nullptr; // the Track hasn't been scanned yet
			const TInternalTrack *const pit=__scanTrack__(0,0);
			const TStdWinError result =	pit && pit->nSectors // if Track can be scanned and its Sectors recognized ...
										? ERROR_SUCCESS	// ... then the TransferSpeed has been set correctly
										: LOG_ERROR(ERROR_INVALID_DATA);
			__unformatInternalTrack__(0,0);
		REFER_TO_TRACK(0,0)=pit0; // restoring original Track
		return result;
	}

	TStdWinError CFDD::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		LOG_ACTION(_T("TStdWinError CFDD::SetMediumTypeAndGeometry"));
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - base
		if (const TStdWinError err=CFloppyImage::SetMediumTypeAndGeometry(pFormat,sideMap,firstSectorNumber))
			return LOG_ERROR(err);
		// - setting the transfer speed according to current FloppyType (DD/HD)
		__freeInternalTracks__();
		switch (floppyType){ // set in base method to "pFormat->mediumType"
			case TMedium::FLOPPY_DD_525:
			case TMedium::FLOPPY_DD_350:
			case TMedium::FLOPPY_HD_350:
				// determining if corresponding FloppyType is inserted
				return __setDataTransferSpeed__(floppyType);
			default:
				// automatically recognizing the Type of inserted floppy
				if (__setAndEvaluateDataTransferSpeed__( floppyType=TMedium::FLOPPY_DD_525 )==ERROR_SUCCESS) return ERROR_SUCCESS;
				if (__setAndEvaluateDataTransferSpeed__( floppyType=TMedium::FLOPPY_DD_350 )==ERROR_SUCCESS) return ERROR_SUCCESS;
				if (__setAndEvaluateDataTransferSpeed__( floppyType=TMedium::FLOPPY_HD_350 )==ERROR_SUCCESS) return ERROR_SUCCESS;
				return LOG_ERROR(ERROR_BAD_COMMAND);
		}
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

	TStdWinError CFDD::__reset__(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		LOG_ACTION(_T("TStdWinError CFDD::__reset__"));
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - disposing all InternalTracks
		__freeInternalTracks__();
		// - re-connecting to the Drive
		__disconnectFromFloppyDrive__();
		if (const TStdWinError err=__connectToFloppyDrive__(DRV_AUTO))
			return LOG_ERROR(err);
		// - sending Head home
		return fddHead.__calibrate__() ? ERROR_SUCCESS : LOG_ERROR(::GetLastError());
	}

	bool CFDD::__isFloppyInserted__() const{
		// True <=> (some) floppy is inserted in the Drive, otherwise False
		LOG_ACTION(_T("bool CFDD::__isFloppyInserted__"));
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
	struct TLatencyParams sealed{
		CFDD *const fdd;
		const TMedium::TType floppyType;
		const BYTE usAccuracy; // accuracy in microseconds
		const BYTE nRepeats;
		TCylinder cyl;	// healthy Track to use for computation of the latencies
		THead head;		// healthy Track to use for computation of the latencies
		float outControllerLatency; // microseconds
		float out1ByteLatency;		// microseconds
		float outGap3Latency;		// microseconds

		TLatencyParams(CFDD *fdd,TMedium::TType floppyType,BYTE usAccuracy,BYTE nRepeats)
			: fdd(fdd)
			, floppyType(floppyType)
			, usAccuracy(usAccuracy) , nRepeats(nRepeats)
			, cyl(0) , head(0) // a healthy Track will be found when determining the controller latency
			, outControllerLatency(0) , out1ByteLatency(0) , outGap3Latency(0) {
		}
	};
	UINT AFX_CDECL CFDD::__determineControllerAndOneByteLatency_thread__(PVOID _pCancelableAction){
		// thread to automatically determine the controller and one Byte write latencies
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		TLatencyParams &lp=*(TLatencyParams *)pAction->GetParams();
		pAction->SetProgressTarget( lp.nRepeats*3 ); // 3 = number of steps of a single trial
		// - defining the Interruption
		struct TInterruption sealed{
		private:
			const PBackgroundActionCancelable pAction;
			CFDD *const fdd;
			TCylinder &rCyl;
			THead &rHead;
			const PVOID sectorDataToWrite;
			const BYTE usAccuracy; // microseconds
		public:
			const WORD sectorLength;
			TSectorId sectorId;
			WORD nMicroseconds;

			TInterruption(const PBackgroundActionCancelable pAction,TLatencyParams &lp)
				// ctor
				: pAction(pAction)
				, fdd(lp.fdd) , sectorLength(lp.floppyType==TMedium::FLOPPY_HD_350?8192:4096)
				, rCyl(lp.cyl) , rHead(lp.head) // a healthy Track yet to be found
				, sectorDataToWrite( ::VirtualAlloc(nullptr,SECTOR_LENGTH_MAX,MEM_COMMIT,PAGE_READWRITE) )
				, usAccuracy(lp.usAccuracy) , nMicroseconds(0) {
				sectorId.lengthCode=__getSectorLengthCode__(sectorLength); // any Cylinder/Side/Sector values will do, so letting them uninitialized
				::memset( sectorDataToWrite, TEST_BYTE, sectorLength );
			}
			~TInterruption(){
				// dtor
				::VirtualFree(sectorDataToWrite,0,MEM_RELEASE);
			}

			TStdWinError __writeSectorData__(WORD nBytesToWrite) const{
				// writes SectorData to the current Track and interrupts the controller after specified NumberOfBytesToWrite and Microseconds; returns Windows standard i/o error
				// : setting controller interruption to the specified NumberOfBytesToWrite and Microseconds
				if (const TStdWinError err=fdd->__setTimeBeforeInterruptingTheFdc__( nBytesToWrite, nMicroseconds ))
					return err;
				// : writing
				DWORD nBytesTransferred;
				switch (fdd->DRIVER){
					case DRV_FDRAWCMD:{
						FD_READ_WRITE_PARAMS rwp={ FD_OPTION_MFM, rHead, sectorId.cylinder,sectorId.side,sectorId.sector,sectorId.lengthCode, sectorId.sector+1, FDD_SECTOR_GAP3_STD/2, sectorId.lengthCode?0xff:0x80 };
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
				for( fdd->__bufferSectorData__(rCyl,rHead,&sectorId,sectorLength,&TInternalTrack(fdd,rCyl,rHead,1,&sectorId,(PCINT)FDD_SECTOR_GAP3_STD),0,&TFdcStatus()); *p==TEST_BYTE; p++ );
				return p-(PCBYTE)fdd->dataBuffer;
			}
			TStdWinError __setInterruptionToWriteSpecifiedNumberOfBytes__(WORD nBytes){
				// sets this Interruption so that the specified NumberOfBytes is written to current Track; returns Windows standard i/o error
				// : initialization using the default NumberOfMicroseconds
				nMicroseconds=20;
				// : increasing the NumberOfMicroseconds until the specified NumberOfBytes is written for the first time
				do{
					if (pAction->IsCancelled()) return ERROR_CANCELLED;
					nMicroseconds+=usAccuracy;
					if (const TStdWinError err=__writeSectorData__(nBytes))
						return err;
				}while (__getNumberOfWrittenBytes__()<nBytes);
				const float nMicrosecondsA=nMicroseconds;
				// : increasing the NumberOfMicroseconds until a higher NumberOfBytes is written for the first time
				do{
					if (pAction->IsCancelled()) return ERROR_CANCELLED;
					nMicroseconds+=usAccuracy;
					if (const TStdWinError err=__writeSectorData__(nBytes))
						return err;
				}while (__getNumberOfWrittenBytes__()<=nBytes);
				// : the resulting NumberOfMicroseconds is the average of when the NumberOfBytes has been written for the first and last time
				nMicroseconds=(nMicrosecondsA+nMicroseconds)/2;
				return ERROR_SUCCESS;
			}
		} interruption( pAction, lp );
		// - testing
		const TExclusiveLocker locker(lp.fdd); // locking the access so that no one can disturb during the testing
		for( BYTE c=lp.nRepeats,state=0; c--; ){
			// . STEP 1: writing the test Sector (DD = 4kB, HD = 8kB)
			do{
				if (pAction->IsCancelled()) return LOG_ERROR(ERROR_CANCELLED);
				// : seeking Head to the particular Cylinder
				if (!lp.fdd->fddHead.__seekTo__(lp.cyl))
					return LOG_ERROR(pAction->TerminateWithError(ERROR_REQUEST_REFUSED));
				// : formatting Track to a single Sector
				const bool vft0=lp.fdd->params.verifyFormattedTracks;
				lp.fdd->params.verifyFormattedTracks=false;
					const TStdWinError err=lp.fdd->FormatTrack( lp.cyl, lp.head, 1,&interruption.sectorId,&interruption.sectorLength,&TFdcStatus::WithoutError, FDD_SECTOR_GAP3_STD, 0 );
				lp.fdd->params.verifyFormattedTracks=vft0;
				if (err!=ERROR_SUCCESS)
					return LOG_ERROR(pAction->TerminateWithError(err));
				// : verifying the single formatted Sector
				TFdcStatus sr;
				lp.fdd->__bufferSectorData__( lp.cyl, lp.head, &interruption.sectorId, interruption.sectorLength, &TInternalTrack(lp.fdd,lp.cyl,lp.head,1,&interruption.sectorId,(PCINT)FDD_SECTOR_GAP3_STD), 0, &sr );
				if (sr.IsWithoutError())
					break; // yes, a healthy Track has been found - using it for computation of all latencies
				// : attempting to create a Sector WithoutErrors on another Track
				if (++lp.cyl==FDD_CYLINDERS_MAX)
					return LOG_ERROR(pAction->TerminateWithError(ERROR_REQUEST_REFUSED));
			}while (true);
			pAction->UpdateProgress(++state);
			// . STEP 2: experimentally determining the ControllerLatency
			if (pAction->IsCancelled()) return LOG_ERROR(ERROR_CANCELLED);
			const WORD nBytes=interruption.sectorLength/2;
			if (const TStdWinError err=interruption.__setInterruptionToWriteSpecifiedNumberOfBytes__(nBytes))
				return LOG_ERROR(pAction->TerminateWithError(err));
			const float nControllerMicroseconds=interruption.nMicroseconds;
			lp.outControllerLatency+=nControllerMicroseconds; // below divided by the number of attempts to get an average
/*
{TCHAR buf[80];
::wsprintf(buf,_T("nMicrosecondsA=%d"),(int)(nControllerMicroseconds*1000));
Utils::Information(buf);}
//*/
			pAction->UpdateProgress(++state);
			// . STEP 3: experimentally determining the latency of one Byte
			if (pAction->IsCancelled()) return LOG_ERROR(ERROR_CANCELLED);
			const float p = interruption.nMicroseconds = 65000;
			if (const TStdWinError err=interruption.__writeSectorData__(nBytes))
				return LOG_ERROR(pAction->TerminateWithError(err));
			const float n=interruption.__getNumberOfWrittenBytes__();
			const float nMicrosecondsPerByte=( p - nControllerMicroseconds ) / ( n - nBytes );
/*
{TCHAR buf[80];
::wsprintf(buf,_T("oneByteLatency=%d"),(int)(nMicrosecondsPerByte*1000));
Utils::Information(buf);}
//*/
			//lp.out1ByteLatency+=( n - sectorLength ) / ( p - nMicrosecondsZ );
			lp.out1ByteLatency+=nMicrosecondsPerByte; // below divided by the number of attempts to get an average
			pAction->UpdateProgress(++state);
		}
		// - computing the final latency values
		lp.outControllerLatency/=lp.nRepeats, lp.out1ByteLatency/=lp.nRepeats;
		return ERROR_SUCCESS;
	}
	UINT AFX_CDECL CFDD::__determineGap3Latency_thread__(PVOID _pCancelableAction){
		// thread to automatically determine the Gap3 latency
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		TLatencyParams &lp=*(TLatencyParams *)pAction->GetParams();
		pAction->SetProgressTarget( FDD_SECTOR_GAP3_STD );
		const TExclusiveLocker locker(lp.fdd); // locking the access so that no one can disturb during the testing
		for( BYTE gap3=1; gap3<FDD_SECTOR_GAP3_STD; pAction->UpdateProgress(gap3+=3) ){
			if (pAction->IsCancelled()) return LOG_ERROR(ERROR_CANCELLED);
			// . STEP 1: writing two test Sectors
			static const TSectorId SectorIds[]={ {1,0,1,2}, {1,0,2,2} };
			static const WORD SectorLengths[]={ 512, 512 };
			static const TFdcStatus SectorStatuses[]={ TFdcStatus::WithoutError, TFdcStatus::WithoutError };
			const bool vft0=lp.fdd->params.verifyFormattedTracks;
			lp.fdd->params.verifyFormattedTracks=false;
				const TStdWinError err=lp.fdd->FormatTrack( lp.cyl, lp.head, 2, SectorIds, SectorLengths, SectorStatuses, gap3, TEST_BYTE );
			lp.fdd->params.verifyFormattedTracks=vft0;
			if (err!=ERROR_SUCCESS)
				return pAction->TerminateWithError(err);
			// . STEP 2: reading the Sectors
			BYTE c=0;
			while (c<lp.nRepeats){
				if (pAction->IsCancelled()) return LOG_ERROR(ERROR_CANCELLED);
				// . STEP 2.1: scanning the Track and seeing how distant the two test Sectors are on it
				lp.fdd->__unformatInternalTrack__(lp.cyl,lp.head); // disposing internal information on actual Track format
				const TInternalTrack *const pit=lp.fdd->__scanTrack__(lp.cyl,lp.head);
				// : STEP 2.2: reading the first formatted Sector
				WORD w;
				lp.fdd->GetHealthySectorData( lp.cyl, lp.head, &SectorIds[0], &w );
				// : STEP 2.3: Reading the second formatted Sector and measuring how long the reading took
				const Utils::CLocalTime startTime;
					lp.fdd->GetHealthySectorData( lp.cyl, lp.head, &SectorIds[1], &w );
				const Utils::CLocalTime endTime;
				const int deltaMicroseconds=(endTime-startTime).ToMilliseconds()*1000;
				// . STEP 2.4: determining if the readings took more than just one disk revolution or more
				if (deltaMicroseconds>=pit->sectors[1].endMicroseconds-pit->sectors[0].endMicroseconds+4000) // 4000 = allowing circa 30 Bytes as a limit of detecting a single disk revolution
					break;
				c++;
			}
			if (c==lp.nRepeats){
				// both Sectors were successfully read in a single disk revolution in all N repeats
				lp.outGap3Latency=gap3*lp.out1ByteLatency+lp.outControllerLatency; // "+N" = just to be sure the correct minimum Gap3 has been found
				return pAction->TerminateWithError(ERROR_SUCCESS);
			}
		}
		lp.outGap3Latency=FDD_SECTOR_GAP3_STD*lp.out1ByteLatency;
		return ERROR_SUCCESS;
	}

	bool CFDD::__showSettingDialog__(){
		// True <=> the SettingDialog shown and new values adopted, otherwise False
		// - defining the Dialog
		class CSettingDialog sealed:public CDialog{
			CFDD *const fdd;
			TCHAR doubleTrackDistanceTextOrg[80];

			bool IsDoubleTrackDistanceForcedByUser() const{
				// True <=> user has manually overridden DoubleTrackDistance setting, otherwise False
				return ::lstrlen(doubleTrackDistanceTextOrg)!=::GetWindowTextLength( ::GetDlgItem(m_hWnd,ID_40D80) );
			}

			bool AreSomeTracksFormatted() const{
				// True <=> some Tracks are known to be already formatted, otherwise False
				bool result=false;
				for( BYTE t=sizeof(fdd->internalTracks)/sizeof(PInternalTrack); t>0; result|=fdd->internalTracks[--t]!=nullptr );
				return result;
			}

			void __refreshMediumInformation__(){
				// detects a floppy in the Drive and attempts to recognize its Type
				// . making sure that a floppy is in the Drive
				fdd->floppyType=TMedium::UNKNOWN; // assumption (floppy not inserted or not recognized)
				if (!fdd->__isFloppyInserted__())
					SetDlgItemText( ID_MEDIUM, _T("Not inserted") );
				// . attempting to recognize any previous format on the floppy
				else
					if (fdd->__setAndEvaluateDataTransferSpeed__(TMedium::FLOPPY_DD_525)==ERROR_SUCCESS){
						fdd->floppyType=TMedium::FLOPPY_DD_525;
						SetDlgItemText( ID_MEDIUM, _T("5.25\" DD formatted") );
						if (Utils::EnableDlgControl( m_hWnd, ID_40D80, !AreSomeTracksFormatted() )){
							fdd->fddHead.SeekHome();
							const bool doubleTrackStep0=fdd->fddHead.doubleTrackStep;
								fdd->fddHead.doubleTrackStep=false;
								const PInternalTrack pit0=__REFER_TO_TRACK(fdd,1,0); // backing up original Track, if any
									__REFER_TO_TRACK(fdd,1,0)=nullptr; // the Track hasn't been scanned yet
									const PInternalTrack pit=fdd->__scanTrack__(1,0);
										CheckDlgButton( ID_40D80, pit->nSectors==0 );
									fdd->__unformatInternalTrack__(1,0);
								__REFER_TO_TRACK(fdd,1,0)=pit0; // restoring original Track
							fdd->fddHead.doubleTrackStep=doubleTrackStep0;
							fdd->fddHead.SeekHome();
						}
					}else if (fdd->__setAndEvaluateDataTransferSpeed__(TMedium::FLOPPY_DD_350)==ERROR_SUCCESS){
						fdd->floppyType=TMedium::FLOPPY_DD_350;
						SetDlgItemText( ID_MEDIUM, _T("3.5\" DD formatted") );
						CheckDlgButton( ID_40D80,  Utils::EnableDlgControl( m_hWnd, ID_40D80, false )  );
					}else if (fdd->__setAndEvaluateDataTransferSpeed__(TMedium::FLOPPY_HD_350)==ERROR_SUCCESS){
						fdd->floppyType=TMedium::FLOPPY_HD_350;
						SetDlgItemText( ID_MEDIUM, _T("3.5\"/5.25\" HD formatted") );
						CheckDlgButton( ID_40D80, false );
						Utils::EnableDlgControl( m_hWnd, ID_40D80, true );
					}else{
						SetDlgItemText( ID_MEDIUM, _T("Not formatted or faulty") );
						CheckDlgButton( ID_40D80, false );
						Utils::EnableDlgControl( m_hWnd, ID_40D80, true );
					}
				// . loading the Profile associated with the current drive and FloppyType
				profile.Load( fdd->GetDriveLetter(), fdd->floppyType );
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
				// . displaying inserted Medium information
				__refreshMediumInformation__();
			}
			void __exchangeLatency__(CDataExchange* pDX){
				// exchange of latency-related data from and to controls
				DDX_Text( pDX,	ID_LATENCY,	profile.controllerLatency );
				DDX_Text( pDX,	ID_NUMBER2,	profile.oneByteLatency );
				DDX_Text( pDX,	ID_GAP,		profile.gap3Latency );
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
				Utils::WrapControlsByClosingCurlyBracketWithText( this, GetDlgItem(ID_LATENCY), GetDlgItem(ID_GAP), nullptr, 0 );
				Utils::WrapControlsByClosingCurlyBracketWithText( this, GetDlgItem(ID_NONE), GetDlgItem(ID_SECTOR), _T("if error encountered"), 0 );
				Utils::WrapControlsByClosingCurlyBracketWithText( this, GetDlgItem(ID_ZERO), GetDlgItem(ID_CYLINDER_N), _T("when formatting"), 0 );
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
							class CLatencyAutoDeterminationDialog sealed:public CDialog{
							public:
								int floppyType,usAccuracy,nRepeats;

								CLatencyAutoDeterminationDialog(const CFDD *fdd,CWnd *parent)
									: CDialog(IDR_FDD_LATENCY,parent) , floppyType(0) , usAccuracy(2) , nRepeats(3) {
									switch (fdd->floppyType){
										case TMedium::FLOPPY_DD_525: floppyType=0; break;
										case TMedium::FLOPPY_DD_350: floppyType=1; break;
										case TMedium::FLOPPY_HD_350: floppyType=2; break;
									}
								}
								void PreInitDialog() override{
									__super::PreInitDialog(); // base
									Utils::PopulateComboBoxWithSequenceOfNumbers( GetDlgItem(ID_ACCURACY)->m_hWnd, 1,nullptr, 6,nullptr );
									Utils::PopulateComboBoxWithSequenceOfNumbers( GetDlgItem(ID_TEST)->m_hWnd, 1,_T("(worst)"), 9,_T("(best)") );
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
								if (Utils::InformationOkCancel(_T("Insert an empty disk and hit OK."))){
									CBackgroundMultiActionCancelable bmac( THREAD_PRIORITY_TIME_CRITICAL );
										TMedium::TType floppyType=TMedium::UNKNOWN;
										switch (d.floppyType){
											case 0: floppyType=TMedium::FLOPPY_DD_525; break;
											case 1: floppyType=TMedium::FLOPPY_DD_350; break;
											case 2: floppyType=TMedium::FLOPPY_HD_350; break;
										}
										TLatencyParams lp( fdd, floppyType, 1+d.usAccuracy, 1+d.nRepeats );
										bmac.AddAction( __determineControllerAndOneByteLatency_thread__, &lp, _T("Determining controller latencies") );
										bmac.AddAction( __determineGap3Latency_thread__, &lp, _T("Determining minimal Gap3 size") );
									if (const TStdWinError err=bmac.Perform()){
										Utils::FatalError(_T("Couldn't autodetermine"),err);
										break;
									}
									profile.controllerLatency=lp.outControllerLatency;
									profile.oneByteLatency=lp.out1ByteLatency;
									profile.gap3Latency=lp.outGap3Latency;
									__exchangeLatency__( &CDataExchange(this,FALSE) );
									TCHAR iniSection[16];
									GetFddProfileName( iniSection, fdd->GetDriveLetter(), fdd->floppyType );
									app.WriteProfileInt( iniSection, INI_LATENCY_DETERMINED, TRUE ); // latencies hereby at least once determined
								}
							}
						}
						break;
					case WM_COMMAND:
						switch (wParam){
							case ID_RECOVER:
								// refreshing information on (inserted) floppy
								if (!AreSomeTracksFormatted()) // if no Tracks are yet formatted ...
									SetDlgItemText( ID_40D80, doubleTrackDistanceTextOrg ); // ... then resetting the flag that user has overridden DoubleTrackDistance
								__refreshMediumInformation__();
								break;
							case ID_40D80:{
								// track distance changed manually
								TCHAR buf[sizeof(doubleTrackDistanceTextOrg)/sizeof(TCHAR)+20];
								SetDlgItemText( ID_40D80, ::lstrcat(::lstrcpy(buf,doubleTrackDistanceTextOrg),_T(" (user forced)")) );
								break;
							}
							case ID_ZERO:
							case ID_CYLINDER_N:
								// adjusting possibility to edit the CalibrationStep according to selected option
								GetDlgItem(ID_NUMBER)->EnableWindow(wParam!=ID_ZERO);
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
				return CDialog::WindowProc(msg,wParam,lParam);
			}
		public:
			TParams params;
			TFddHead::TProfile profile;

			CSettingDialog(CFDD *_fdd)
				// ctor
				: CDialog(IDR_FDD_ACCESS) , fdd(_fdd) , params(_fdd->params) , profile(_fdd->fddHead.profile) {
			}
		} d(this);
		// - showing the Dialog and processing its result
		LOG_DIALOG_DISPLAY(_T("CSettingDialog"));
		if (d.DoModal()==IDOK){
			params=d.params;
			( fddHead.profile=d.profile ).Save( GetDriveLetter(), floppyType );
			__setSecondsBeforeTurningMotorOff__(params.nSecondsToTurningMotorOff);
			return true;
		}else
			return LOG_BOOL(false);
	}

	void CFDD::EditSettings(){
		// displays dialog with editable settings and reflects changes made by the user into the Image's inner state
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		__showSettingDialog__();
	}

	TStdWinError CFDD::Reset(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		// - displaying message
		__informationWithCheckableShowNoMore__( _T("To spare both floppy and drive, all activity is buffered: CHANGES (WRITINGS, DELETIONS) MADE TO THE FLOPPY ARE SAVED ONLY WHEN YOU COMMAND SO (Ctrl+S). If you don't save them, they will NOT appear on the disk next time. FORMATTING DESTROYS THE CONTENT IMMEDIATELLY!"), INI_MSG_RESET );
		// - resetting
		LOG_ACTION(_T("TStdWinError CFDD::Reset"));
		const TStdWinError err=__reset__();
		#ifndef _DEBUG // checking Error only in Release mode (in Debug mode despite Error wanting to proceed up the setting dialog)
			if (err!=ERROR_SUCCESS)
				return LOG_ERROR(err);
		#endif
		// - showing the settings and applying its results to the Drive access
		return __showSettingDialog__() ? ERROR_SUCCESS : LOG_ERROR(ERROR_CANCELLED);
	}

	BYTE CFDD::__getMaximumSectorLengthCode__() const{
		// returns the maximum LengthCode given the actual FloppyType
		return 5+(floppyType==TMedium::FLOPPY_HD_350);
	}

	static BYTE __reportSectorVerificationError__(RCPhysicalAddress chs){
		CDialog d( IDR_DOS_FORMAT );
		d.ShowWindow(SW_HIDE);
		TCHAR buf[100],sug[480];
		d.GetWindowText( buf, sizeof(buf)/sizeof(TCHAR) );
		::wsprintf( sug, _T("- Has the correct medium been set in the \"%s\" dialog?\n- For copy-protected schemes, simply retrying often helps."), buf );
		::wsprintf( buf, _T("Track %d verification failed for sector with %s"), chs.GetTrackNumber(2), (LPCTSTR)chs.sectorId.ToString() );
		return Utils::AbortRetryIgnore( buf, ::GetLastError(), MB_DEFBUTTON2, sug );
	}

	TStdWinError CFDD::__formatToOneLongVerifiedSector__(RCPhysicalAddress chs,BYTE fillerByte){
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
				WORD sectorBytes=__getUsableSectorLength__(rih.size);
				__setTimeBeforeInterruptingTheFdc__( sectorBytes, fddHead.profile.controllerLatency+1*fddHead.profile.oneByteLatency ); // "X*" = reserve to guarantee that really all test data written
				FD_READ_WRITE_PARAMS rwp={ FD_OPTION_MFM|FD_OPTION_SK, chs.head, rih.cyl,rih.head,rih.sector,rih.size, rih.sector+1, 1, 0xff };
				{	LOG_ACTION(_T("DeviceIoControl IOCTL_FDCMD_WRITE_DATA"));
					if (!::DeviceIoControl( _HANDLE, IOCTL_FDCMD_WRITE_DATA, &rwp,sizeof(rwp), ::memset(dataBuffer,fillerByte,sectorBytes),__getOfficialSectorLength__(rih.size), &nBytesTransferred, nullptr ))
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
						switch (__reportSectorVerificationError__(chs)){
							case IDABORT:	return LOG_ERROR(ERROR_CANCELLED);
							case IDRETRY:	continue;
							case IDIGNORE:	break;
						}
			}else
				// if verification turned off, assuming well formatted Track structure, hence avoiding the need of its scanning
				REFER_TO_TRACK(chs.cylinder,chs.head) = new TInternalTrack( this, chs.cylinder, chs.head, 1, &chs.sectorId, (PCINT)FDD_SECTOR_GAP3_STD ); // Gap3 = calculate Sector start times from information of this Gap3 and individual Sector lengths
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
		LOG_TRACK_ACTION(cyl,head,_T("TStdWinError CFDD::FormatTrack"));
		#ifdef LOGGING_ENABLED
			TCHAR formatTrackParams[200];
			::wsprintf(formatTrackParams,_T("Cyl=%d, Head=%d, nSectors=%d, gap3=%d, fillerByte=%d"),cyl,head,nSectors,gap3,fillerByte);
			LOG_MESSAGE(formatTrackParams);
		#endif
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
				__unformatInternalTrack__(cyl,head);
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
					GetTrackData( cyl, head, bufferId, Utils::CByteIdentity(), nSectors, false, (PSectorData *)dummyBuffer, (PWORD)dummyBuffer, statuses ); // "DummyBuffer" = throw away any outputs
					for( TSector n=0; n<nSectors; n++ ){
						const TPhysicalAddress chs={ cyl, head, bufferId[n] };
						if (!statuses[n].IsWithoutError())
							switch (__reportSectorVerificationError__(chs)){
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
				__unformatInternalTrack__(cyl,head);
				// . formatting the Track
				const TPhysicalAddress chs={ cyl, head, *bufferId };
				if (__formatToOneLongVerifiedSector__( chs, fillerByte )!=ERROR_SUCCESS)
					goto error;
				// . Track formatted successfully
				break;
			}
			case TFormatStyle::CUSTOM:{
				// . disposing internal information on actual Track format
				LOG_MESSAGE(_T("TFormatStyle::CUSTOM"));
				__unformatInternalTrack__(cyl,head);
				// . verifying Track surface (if requested to) by writing maximum number of known Bytes to it and trying to read them back
				if (params.verifyFormattedTracks){
					const TPhysicalAddress chs={ cyl, head, {0,0,0,__getMaximumSectorLengthCode__()+1} };
					if (__formatToOneLongVerifiedSector__( chs, fillerByte )!=ERROR_SUCCESS)
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
						Utils::Information(buf);
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
					pFormatStep->interruption.nMicroseconds=fddHead.profile.controllerLatency+*bufferLength/2*fddHead.profile.oneByteLatency;
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
Utils::Information(buf);}
*/
					pFormatStep->gap3=gap3+deltaGap3;
					// : setting up the Interruption according to the requested i/o errors
					const TFdcStatus sr(*pFdcStatus++);
					WORD nBytesFormatted;
					if (sr.DescribesMissingDam()){
						// Sector misses its data part
						pFormatStep->interruption.nMicroseconds=fddHead.profile.controllerLatency+(GAP2_BYTES_COUNT+SYNCHRONIZATION_BYTES_COUNT-2)*fddHead.profile.oneByteLatency; // "-2" = damaging the A1A1A1 mark
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
						pFormatStep->interruption.nMicroseconds=fddHead.profile.controllerLatency+sectorLength/2*fddHead.profile.oneByteLatency;
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
					__setWaitingForIndex__();
					// : setting shortened formatting
					__setTimeBeforeInterruptingTheFdc__( pfs->interruption.nBytes, pfs->interruption.nMicroseconds );
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
					const TInternalTrack it( this, cyl, head, nSectors, bufferId, nullptr ); // nullptr = calculate Sector start times from information on default Gap3 and individual Sector lengths
					for( TSector n=0; n<nSectors; n++ ){
						const TPhysicalAddress chs={ cyl, head, bufferId[n] };
						TFdcStatus sr;
						__bufferSectorData__( chs, bufferLength[n], &it, n, &sr );
						if (bufferFdcStatus[n].DescribesMissingDam()^sr.DescribesMissingDam())
							switch (__reportSectorVerificationError__(chs)){
								case IDABORT:	return LOG_ERROR(ERROR_CANCELLED);
								case IDRETRY:	goto formatCustomWay;
								case IDIGNORE:	break;
							}
					}
				}else
					// if verification turned off, assuming well formatted Track structure, hence avoiding the need of its scanning
					REFER_TO_TRACK(cyl,head) = new TInternalTrack( this, cyl, head, nSectors, bufferId, (PCINT)gap3 ); // Gap3 = calculate Sector start times from information of this Gap3 and individual Sector lengths
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
		__unformatInternalTrack__(cyl,head);
		// - explicitly setting Track structure
		TInternalTrack::TSectorInfo *psi=( REFER_TO_TRACK(cyl,head) = new TInternalTrack( this, cyl, head, nSectors, bufferId, (PCINT)gap3 ) )->sectors; // Gap3 = calculate Sector start times from information of this Gap3 and individual Sector lengths
		for( TSector n=nSectors; n--; psi++ )
			psi->data=(PSectorData)::memset( ALLOCATE_SECTOR_DATA(psi->length), fillerByte, psi->length );
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
				const BYTE lengthCode=__getSectorLengthCode__(16384); // 16kB long Sector that rewrites its own header
				FD_FORMAT_PARAMS fmt={	FD_OPTION_MFM, head, lengthCode, 1, 50, 0,
										{cyl,head,0,lengthCode}
									};
				LOG_ACTION(_T("DeviceIoControl IOCTL_FDCMD_FORMAT_TRACK"));
				if (::DeviceIoControl( _HANDLE, IOCTL_FDCMD_FORMAT_TRACK, &fmt,sizeof(fmt), nullptr,0, &nBytesTransferred, nullptr )!=0){
					__unformatInternalTrack__(cyl,head); // disposing internal information on actual Track format
					return ERROR_SUCCESS;
				}else
					goto error;
			}
			default:
				ASSERT(FALSE);
				return LOG_ERROR(ERROR_DEVICE_NOT_AVAILABLE);
		}
	}

	void CFDD::SetPathName(LPCTSTR,BOOL bAddToMRU){
		const auto tmpFile0=m_strPathName;
			__super::SetPathName( devicePatternName, bAddToMRU );
		m_strPathName=tmpFile0;
	}
