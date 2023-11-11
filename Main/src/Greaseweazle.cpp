#include "stdafx.h"
#include "CapsBase.h"
#include "Greaseweazle.h"

	#define INI_GREASEWEAZLE	_T("GrWeV4")

	#define GW_DEVICE_NAME_UNICODE	L"Greaseweazle"
	#define GW_DEVICE_NAME		"Greaseweazle"
	#define GW_DEVICE_NAME_PATTERN	_T(GW_DEVICE_NAME) _T(" floppy drive #%c (%s.sys)")
	static_assert( ARRAYSIZE(GW_DEVICE_NAME_PATTERN)+8<=DEVICE_NAME_CHARS_MAX, "Identifier too long" );

	#define GW_ACCESS_DRIVER_USBSER	_T("usbser")

	#define GW_DRIVES_MAX			2

	#define GW_BUFFER_CAPACITY		1000000

	LPCTSTR CGreaseweazleV4::Recognize(PTCHAR deviceNameList){
		// returns a null-separated list of floppy drives connected via a local Greaseweazle Device
		// - evaluating possibilities how to access Greaseweazle
		ASSERT( deviceNameList!=nullptr );
		if (SetupDi::GetDevicePath( SetupDi::GUID_DEVINTERFACE_USB_DEVICE, GW_DEVICE_NAME_UNICODE ).IsEmpty())
			return nullptr; // Greaseweazle inaccessible
		// - enumerating connected floppy drives
		PTCHAR p=deviceNameList;
			for( BYTE fddId=0; fddId<GW_DRIVES_MAX; fddId++ )
				//if (CGreaseweazleV4( TDriver::USBSER, fddId ).fddFound)
					p+=::wsprintf( p, GW_DEVICE_NAME_PATTERN, fddId+'0', GW_ACCESS_DRIVER_USBSER )+1; // "+1" = null-terminated items		
		// - no further access possibilities
		*p='\0'; // null-terminated list
		return deviceNameList;
	}

	PImage CGreaseweazleV4::Instantiate(LPCTSTR deviceName){
		// creates and returns a Greaseweazle Device instance for a specified floppy drive
		TCHAR fddId, driverStr[16], tmp[MAX_PATH];
		*_tcsrchr( ::lstrcpy(tmp,deviceName), '.' )='\0';
		::_stscanf( tmp, GW_DEVICE_NAME_PATTERN, &fddId, driverStr );
		if (!::lstrcmp(driverStr,GW_ACCESS_DRIVER_USBSER))
			return new CGreaseweazleV4( TDriver::USBSER, fddId-'0' );
		ASSERT(FALSE);
		::SetLastError( ERROR_BAD_DEVICE );
		return nullptr;
	}

	const CImage::TProperties CGreaseweazleV4::Properties={
		MAKE_IMAGE_ID('K','E','I','R','G','W','V','4'), // a unique identifier
		Recognize,	// list of recognized Device names
		Instantiate,	// instantiation function
		nullptr, // filter
		Medium::FLOPPY_ANY, // supported Media
		Codec::FLOPPY_ANY, // supported Codecs
		1,2*6144	// Sector supported min and max length
	};







	#define EXCLUSIVELY_LOCK_DEVICE()	const Utils::CExclusivelyLocked deviceLocker(device.locker)
	// always lock first the Image and THEN the Device, so that the locking is compatible with base classes!

	#define hDevice	device.handle
	#define winusb	device.winusb

	CGreaseweazleV4::CGreaseweazleV4(TDriver driver,BYTE fddId)
		// ctor
		// - base
		: CCapsBase( &Properties, fddId+'0', true, INI_GREASEWEAZLE )
		// - initialization
		, driver(driver) , fddId(fddId)
		, dataBuffer( GW_BUFFER_CAPACITY )
		, sampleClock(0,1) {
		preservationQuality=false;
		informedOnPoorPrecompensation=false;
		// - setting a classical 5.25" floppy geometry
		capsImageInfo.maxcylinder=FDD_CYLINDERS_HD/2+FDD_CYLINDERS_EXTRA - 1; // "-1" = inclusive!
		capsImageInfo.maxhead=2-1; // inclusive!
		// - connecting to a local Greaseweazle Device
		hDevice=INVALID_HANDLE_VALUE;
		Connect();
		//Reset();
	}

	CGreaseweazleV4::~CGreaseweazleV4(){
		// dtor
		EXCLUSIVELY_LOCK_THIS_IMAGE(); // mustn't destroy this instance while it's being used!
		Disconnect();
		DestroyAllTracks(); // see Reset()
	}








	TStdWinError CGreaseweazleV4::Connect(){
		// connects to a local Greaseweazle Device; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		EXCLUSIVELY_LOCK_DEVICE();
		// - connecting to the Device
		ASSERT( hDevice==INVALID_HANDLE_VALUE );
		hDevice=::CreateFile(
					SetupDi::GetDevicePath( SetupDi::GUID_DEVINTERFACE_USB_DEVICE, GW_DEVICE_NAME_UNICODE ),
					GENERIC_READ | GENERIC_WRITE,
					0,
					nullptr, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, nullptr
				);
		if (hDevice==INVALID_HANDLE_VALUE)
			return ::GetLastError();
		// - setting transfer properties
		switch (driver){
			case TDriver::USBSER:{
				DCB comPortParams={ sizeof(DCB) };
				if (!::GetCommState( hDevice, &comPortParams ))
					return ::GetLastError();
				comPortParams.BaudRate=CBR_9600;
				comPortParams.ByteSize=8;
				comPortParams.Parity=NOPARITY;
				comPortParams.StopBits=ONESTOPBIT;
				if (!::SetCommState( hDevice, &comPortParams ))
					return ::GetLastError();
				break;
			}
			default:
				ASSERT(FALSE);
				return ERROR_BAD_DRIVER;
		}
		// - determining version of Device
		static constexpr BYTE FIRMWARE_INFO=0;
		if (const TStdWinError err=SendRequest( TRequest::GET_INFO, &FIRMWARE_INFO, sizeof(FIRMWARE_INFO) ))
			return err;
		static_assert( sizeof(firmwareInfo)==32, "Unexpected firmwareInfo size" );
		if (const TStdWinError err=ReadFull( &firmwareInfo, sizeof(firmwareInfo) ))
			return err;
		if (firmwareInfo.hardwareModel!=4)
			return ERROR_DEVICE_NOT_AVAILABLE;
		sampleClock=Utils::TRationalNumber( TIME_SECOND(1), firmwareInfo.sampleFrequency ).Simplify();
		// - initial settings
		for( BYTE busType=BUS_UNKNOWN+1; busType<BUS_LAST; busType++ ){
			if (const TStdWinError err=SendRequest( TRequest::SET_BUS_TYPE, &busType, sizeof(busType) ))
				return err;
			if (!ReadTrack(0,0))
				continue; // must be able to turn motor on, seek to a Cylinder and read a Track
			UnscanTrack(0,0);
			params.shugartDrive=busType==BUS_SHUGART;
			break;
		}
		// - evaluating connection
		return *this ? ERROR_SUCCESS : ERROR_GEN_FAILURE;
	}

	void CGreaseweazleV4::Disconnect(){
		// disconnects from local Greaseweazle Device
		EXCLUSIVELY_LOCK_THIS_IMAGE(); // mustn't disconnect while Device in use!
		EXCLUSIVELY_LOCK_DEVICE();
		switch (driver){
			case TDriver::USBSER:
				//nop
				break;
			default:
				ASSERT(FALSE);
				return;
		}
		::CloseHandle( hDevice );
		hDevice=INVALID_HANDLE_VALUE;
	}

	CGreaseweazleV4::operator bool() const{
		// True <=> connection to a Greaseweazle Device established, otherwise False
		if (hDevice==INVALID_HANDLE_VALUE)
			return false;
		switch (driver){
			case TDriver::USBSER:
				return true;
			default:
				ASSERT(FALSE);
				::SetLastError( ERROR_BAD_DRIVER );
				return false;
		}
	}

	DWORD CGreaseweazleV4::Read(PVOID buffer,DWORD nBytesFree) const{
		// reads a chunk of data received from the Device; returns the number of Bytes received
		if (!*this || !nBytesFree) // not connected or nothing wanted
			return 0;
		DWORD nBytesTransferred=0;
		switch (driver){
			case TDriver::USBSER:{
				// . wait for the first data Byte to come from the Device
				if (!::ReadFile( hDevice, buffer, sizeof(BYTE), &nBytesTransferred, nullptr ))
					return 0;
				if (!nBytesTransferred) // timeout?
					return 0;
				// . determine how many more Bytes are there waiting to be read
				COMSTAT cs;
				if (!::ClearCommError( hDevice, nullptr, &cs ))
					return 0;
				// . read those waiting Bytes (taking into account Buffer free capacity
				return	::ReadFile(
							hDevice,
							(PBYTE)buffer+1, std::min(nBytesFree-1,cs.cbInQue), // mind the first Byte we've been waiting for above!
							&nBytesTransferred, nullptr
						)!=FALSE
						? nBytesTransferred+1 // plus the first Byte we've been waiting for above
						: 0;
			}
			default:
				ASSERT(FALSE);
				::SetLastError( ERROR_BAD_DRIVER );
				return 0;
		}
	}

	TStdWinError CGreaseweazleV4::ReadFull(PVOID buffer,DWORD nBytes) const{
		// blocks caller until all requested Bytes are read from the Device; returns Windows standard i/o error
		for( PBYTE p=(PBYTE)buffer; nBytes>0; )
			if (const DWORD n=Read( p, nBytes )){
				p+=n;
				if (p-(PBYTE)buffer>=nBytes)
					break;
				nBytes-=n;
			}else
				switch (const TStdWinError err=::GetLastError()){
					case ERROR_IO_PENDING:
						continue;
					default: // i/o operation anything but pending
						return err;
				}
		return ERROR_SUCCESS;
	}

	DWORD CGreaseweazleV4::Write(LPCVOID buffer,DWORD nBytes) const{
		// writes a chunk of data to a Greaseweazle Device; returns the number of Bytes accepted by the Device
		if (!*this) // not connected
			return 0;
		DWORD nBytesTransferred=0;
		switch (driver){
			case TDriver::USBSER:
				return	::WriteFile(
							hDevice,
							buffer, nBytes,
							&nBytesTransferred, nullptr
						)!=FALSE
						? nBytesTransferred
						: 0;
			default:
				ASSERT(FALSE);
				::SetLastError( ERROR_BAD_DRIVER );
				return 0;
		}
	}

	TStdWinError CGreaseweazleV4::WriteFull(LPCVOID buffer,DWORD nBytes) const{
		// blocks caller until all requested Bytes are written to the Device; returns Windows standard i/o error
		for( PCBYTE p=(PCBYTE)buffer; nBytes>0; )
			if (const DWORD n=Write( p, nBytes )){
				p+=n;
				if (p-(PCBYTE)buffer>=nBytes)
					break;
				nBytes-=n;
			}else
				switch (const TStdWinError err=::GetLastError()){
					case ERROR_IO_PENDING:
						continue;
					default: // i/o operation anything but pending
						return err;
				}
		return ERROR_SUCCESS;
	}

	TStdWinError CGreaseweazleV4::SendRequest(TRequest req,LPCVOID params,BYTE paramsLength) const{
		// sends a command to a locally connected Greaseweazle Device; returns Windows standard i/o error
		// - if not connected, we are done
		if (!*this)
			return ERROR_NOT_READY;
		// - composition of Packet to send to the Device
		BYTE packet[UCHAR_MAX];
		const WORD packetLength=paramsLength+2;
		if (packetLength>sizeof(packet))
			return ERROR_INSUFFICIENT_BUFFER;
		*packet=req, packet[1]=packetLength;
		::memcpy( packet+2, params, paramsLength );
		// - sending the Request Packet
		if (const TStdWinError err=WriteFull( packet, packetLength ))
			return err;
		// - waiting for and evaluating a response
		if (const TStdWinError err=ReadFull( packet, 2 ))
			return err;
		if (*packet!=req)
			return ERROR_ASSERTION_FAILURE;
		switch (packet[1]){
			case TResponse::OKAY:
				return ERROR_SUCCESS; // Request successfully sent to the Device
			case TResponse::BAD_COMMAND:
				return ERROR_BAD_COMMAND;
			case TResponse::NO_INDEX:
				return ERROR_INDEX_ABSENT;
			case TResponse::NO_TRK0:
				return ERROR_UNRECOGNIZED_MEDIA;
			case TResponse::FLUX_OVERFLOW:
			case TResponse::FLUX_UNDERFLOW:
				return ERROR_READ_FAULT;
			case TResponse::WRPROT:
				return ERROR_WRITE_PROTECT;
			case TResponse::NO_UNIT:
			case TResponse::NO_BUS:
				return ERROR_INVALID_DRIVE;
			case TResponse::BAD_UNIT:
				return ERROR_INVALID_DRIVE_OBJECT;
			case TResponse::BAD_PIN:
				return ERROR_INVALID_PARAMETER;
			case TResponse::BAD_CYLINDER:
				return ERROR_FLOPPY_WRONG_CYLINDER;
			case TResponse::OUT_OF_SRAM:
			case TResponse::OUT_OF_FLASH:
				return ERROR_NOT_ENOUGH_MEMORY;
			default:
				return ERROR_GEN_FAILURE;
		}
	}

	TStdWinError CGreaseweazleV4::GetDriveInfo(TDriveInfo &rOutDi) const{
		if (const TStdWinError err=SelectDrive())
			return err;
		static constexpr BYTE DRIVE_INFO=7;
		if (const TStdWinError err=SendRequest( TRequest::GET_INFO, &DRIVE_INFO, sizeof(DRIVE_INFO) ))
			return err;
		static_assert( sizeof(rOutDi)==32, "Invalid TDriveInfo size" );
		return ReadFull( &rOutDi, sizeof(rOutDi) );
	}

	TStdWinError CGreaseweazleV4::GetPin(BYTE pin,bool &outValue) const{
		if (const TStdWinError err=SendRequest( TRequest::GET_PIN, &pin, sizeof(pin) ))
			return err;
		if (const TStdWinError err=ReadFull( &pin, sizeof(pin) ))
			return err;
		outValue=pin!=0;
		return ERROR_SUCCESS;
	}

	TStdWinError CGreaseweazleV4::SetMotorOn(bool on) const{
		const BYTE params[]={ fddId, (BYTE)on };
		return	SendRequest( TRequest::MOTOR, &params, sizeof(params) );
	}

	TStdWinError CGreaseweazleV4::SeekTo(TCylinder cyl) const{
		if (const TStdWinError err=SelectDrive())
			return err;
		if (const TStdWinError err=SetMotorOn()) // some Drives require motor to be on before seeking Heads
			return err;
		cyl<<=(BYTE)params.doubleTrackStep;
		return	SendRequest( TRequest::SEEK_ABS, &cyl, sizeof(BYTE) );
	}

	TStdWinError CGreaseweazleV4::SelectHead(THead head) const{
		return	SendRequest( TRequest::HEAD, &head, sizeof(BYTE) );
	}









	BOOL CGreaseweazleV4::OnOpenDocument(LPCTSTR lpszPathName){
		// True <=> Image opened successfully, otherwise False
		// - base
		if (!__super::OnOpenDocument(lpszPathName)
			&&
			::GetLastError()!=ERROR_NOT_SUPPORTED // the CAPS library currently doesn't support SuperCard Pro Streams
		)
			return FALSE;
		// - successfully mounted
		return TRUE; // failure may arise later on when attempting to access the Drive
	}

	TStdWinError CGreaseweazleV4::SeekHeadsHome() const{
		// attempts to send Heads "home"; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_DEVICE();
		if (const TStdWinError err=SeekTo(0)) // Head sent home ...
			return err;
		bool trk0;
		if (const TStdWinError err=GetPin( 26, trk0 ))
			return err;
		if (trk0){ // ... but the drive doesn't signal that
			// Keir Fraser: This can happen with KryoFlux flippy-modded Panasonic drives which may not assert the /TRK0 signal when stepping INWARD from Cylinder -1
			TDriveInfo di;
			if (const TStdWinError err=GetDriveInfo(di))
				return err;
			if (di.isFlippy)
				if (const TStdWinError err=SendRequest( TRequest::NO_CLICK_STEP, nullptr, 0 )) // attempt a fake outward Head step to Cylinder -1
					return err; // older firmwares don't recognize this command
		}
		return ERROR_SUCCESS;
	}

	enum TFluxOp:BYTE{ // Flux read Stream opcodes, preceded by 0xFF byte
		Index	=1, // index information
		Space	=2, // "long" flux addendum (e.g. unformatted area)
		Astable	=3,
		Special	=255
	};

	static int ReadBits28(PCBYTE p){
		return	p[0]>>1 | (p[1]&0xfe)<<6 | (p[2]&0xfe)<<13 | (p[3]&0xfe)<<20;
	}

	CImage::CTrackReaderWriter CGreaseweazleV4::GwV4StreamToTrack(PCBYTE p,DWORD length) const{
		// converts Device Stream to general Track representation
		if (sampleClock==0){
			ASSERT(FALSE); // SampleClock must be queried from the Device before decoding the Stream!
			return CTrackReaderWriter::Invalid;
		}
		if (!length || p[--length]) // invalid last Byte in the Stream (must be 0x00)
			return CTrackReaderWriter::Invalid;
		CTrackReaderWriter result(
			length, // a pessimistic estimation of # of Fluxes
			params.GetGlobalFluxDecoder(), params.resetFluxDecoderOnIndex
		);
		const PCBYTE pEnd=p+length;
		for( int sampleCounter=0,sampleCounterSinceIndex=0; p<pEnd; p++ ){
			const BYTE i=*p;
			if (i<TFluxOp::Special){
				// flux information
				if (i<250)
					// "short" flux (1-249 samples, single Byte; 0 indicates end of Stream)
					sampleCounter+=i;
				else{
					// "long" flux (250-1524 samples, two Bytes)
					if (++p>=pEnd) // unexpected end of Stream?
						return CTrackReaderWriter::Invalid;
					sampleCounter+=250+(i-250)*255+*p-1;
				}
				result.AddTime(
					result.GetLastIndexTime()+sampleClock*( sampleCounterSinceIndex+=sampleCounter )
				);
				sampleCounter=0;
			}else{
				// special information
				if (++p>=pEnd) // unexpected end of Stream? (incl. below Opcode)
					return CTrackReaderWriter::Invalid;
				switch (const TFluxOp opcode=(TFluxOp)*p++){
					case TFluxOp::Index:{ // index information
						if (p+sizeof(int)>=pEnd) // unexpected end of Stream?
							return CTrackReaderWriter::Invalid;
						const int value=ReadBits28(p);
						p+=sizeof(int)-1; // "-1" = see "p++" at the end of cycle
						result.AddIndexTime(
							result.GetLastIndexTime()+sampleClock*( sampleCounterSinceIndex+sampleCounter+value )
						);
						sampleCounterSinceIndex= -(sampleCounter+value);
						break;
					}
					case TFluxOp::Space: // "extra long" flux (1525-(2^28-1), seven Bytes, e.g. unformatted area)
						if (p+sizeof(int)>=pEnd) // unexpected end of Stream?
							return CTrackReaderWriter::Invalid;
						sampleCounter+=ReadBits28(p); // addendum to ...
						p+=sizeof(int)-1; // "-1" = see "p++" at the end of cycle
						break; // ... another flux
					//case TFluxOp::Astable:
						//commented out as Astable is intended for representation of non-formatted areas during writing, so it never appears during reading [personal communication with Keir Fraser]
					default:
						return CTrackReaderWriter::Invalid;
				}
			}
		}
		return result;
	}

	CImage::CTrackReader CGreaseweazleV4::ReadTrack(TCylinder cyl,THead head) const{
		// creates and returns a general description of the specified Track, represented using neutral LogicalTimes
		PInternalTrack &rit=internalTracks[cyl][head];
	{	EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return CTrackReaderWriter::Invalid;
		// - if Track already read before, returning the result from before
		if (rit){
			rit->FlushSectorBuffers(); // convert all modifications into flux transitions
			rit->SetCurrentTime(0); // just to be sure the internal TrackReader is returned in valid state (as invalid state indicates this functionality is not supported)
			return *rit;
		}
	}	// - selecting floppy drive
		PBYTE p=dataBuffer;
	{	EXCLUSIVELY_LOCK_DEVICE();
		// - issuing a Request to the Greaseweazle Device to read fluxes in the specified Track
		if (SeekTo(cyl) || SelectHead(head))
			return CTrackReaderWriter::Invalid;
		#pragma pack(1)
		const struct{
			int sampleCounterInit;
			WORD nIndicesRequested;
		} readParams={
			0,
			std::min<BYTE>( params.PrecisionToFullRevolutionCount(), Revolution::MAX )+1 // N+1 indices = N full revolutions
		};
		static_assert( sizeof(readParams)==6, "" );
		if (const TStdWinError err=SendRequest( TRequest::READ_FLUX, &readParams, sizeof(readParams) ))
			return CTrackReaderWriter::Invalid;
		while (const DWORD nBytesFree=dataBuffer+GW_BUFFER_CAPACITY-p)
			if (const DWORD nBytesRead=Read( p, nBytesFree )){
				p+=nBytesRead;
				if (!p[-1]) // terminal zero, aka. end of Track data?
					break;
			}else
				break;
		if (const TStdWinError err=GetLastFluxOperationError())
			return CTrackReaderWriter::Invalid;
	}	// - making sure the read content is a Greaseweazle Stream whose data actually make sense
		if (CTrackReaderWriter trw=GwV4StreamToTrack( dataBuffer, p-dataBuffer )){
			// it's a Greaseweazle Stream whose data make sense
			EXCLUSIVELY_LOCK_THIS_IMAGE();
			if (rit) // if a Track already emerged between the Image and Device locks, using it
				ASSERT(FALSE); // but this shouldn't happen!
			else
				rit=CInternalTrack::CreateFrom( *this, trw );
			return *rit;
		}
		return CTrackReaderWriter::Invalid;
	}

	static PBYTE WriteBits28(int i,PBYTE p){
		*p++= 1 | i<<1;
		*p++= 1 | i>>6;
		*p++= 1 | i>>13;
		*p++= 1 | i>>20;
		return p;
	}

	DWORD CGreaseweazleV4::TrackToGwV4Stream(CTrackReader tr,PBYTE pOutStream) const{
		// converts general Track representation to Device Stream
		tr.SetCurrentTime(0);
		PBYTE p=pOutStream;
		TLogTime tPrev=0; int prevSampleCounter=0;
		for( bool dummyFluxAppended=false; !dummyFluxAppended; ){
			TLogTime flux; int fluxSampleCounter;
			if (tr){
				const TLogTime tCurr=tr.ReadTime();
				const int currSampleCounter=tCurr/sampleClock;
				flux=tCurr-tPrev, fluxSampleCounter=currSampleCounter-prevSampleCounter;
				tPrev=tCurr, prevSampleCounter=currSampleCounter;
			}else{ // end of Track
				fluxSampleCounter=( flux=TIME_MICRO(100) )/sampleClock; // emit a final dummy Flux; never written to disk, just is sacrificial, ensuring that the real final flux gets written in full
				dummyFluxAppended=true;
			}
			if (fluxSampleCounter<=0)
				ASSERT(FALSE); // tachyon flux!
			else if (fluxSampleCounter<250)
				// "short" flux (1-249 samples, single Byte; 0 indicates end of Stream)
				*p++=fluxSampleCounter;
			else if (flux<TIME_MICRO(150)){
				// just an unusually long flux, i.e. not yet a non-formatted area
                const int high=(fluxSampleCounter-250)/255;
                if (high<5){
					// "long" flux (250-1524 samples, two Bytes)
					*p++=250+high;
					*p++=1+(fluxSampleCounter-250)%255;
				}else{
					// "extra long" flux (1525-(2^28-1), seven Bytes, e.g. unformatted area)
					*p++=TFluxOp::Special;
					*p++=TFluxOp::Space;
					p=WriteBits28( fluxSampleCounter-249, p ); // addendum to a ...
					*p++=249; // ... "short flux"
				}
			}else{
				// non-formatted area, typically part of a copy-protection scheme (e.g. Theme Park Mystery by Image Works, 1990)
				*p++=TFluxOp::Special;
				*p++=TFluxOp::Space;
				p=WriteBits28( fluxSampleCounter, p ); // a long flux to be written ...
				*p++=TFluxOp::Special;
				*p++=TFluxOp::Astable;
				p=WriteBits28( TIME_NANO(1250)/sampleClock, p ); // ... as a sequence of quick fluxes
			}
		}
		*p++=0; // last Byte in the Stream must be 0x00
		return p-pOutStream;
	}

	TStdWinError CGreaseweazleV4::UploadTrack(TCylinder cyl,THead head,CTrackReader tr) const{
		// uploads specified Track to a CAPS-based Device (e.g. KryoFlux); returns Windows standard i/o error
		EXCLUSIVELY_LOCK_DEVICE();
		// - converting the supplied Track to internal data format, below streamed directly to Greaseweazle
		const DWORD nBytesToWrite=TrackToGwV4Stream( tr, dataBuffer );
		// - streaming formatted data to Greaseweazle
		if (const TStdWinError err=SeekTo(cyl))
			return err;
		if (const TStdWinError err=SelectHead(head))
			return err;
		static constexpr struct{
			BYTE cueAtIndex;
			BYTE terminateAtIndex;
		} Params={
			1,// sync with Index
			0 // write whatever is supplied, even if over Index
		};
		if (const TStdWinError err=SendRequest( TRequest::WRITE_FLUX, &Params, sizeof(Params) ))
			return err;
		if (const TStdWinError err=WriteFull( dataBuffer, nBytesToWrite ))
			return err;
		if (const TStdWinError err=ReadFull( dataBuffer, 1 )) // sync with Greaseweazle
			return err;
		return GetLastFluxOperationError();
	}

	bool CGreaseweazleV4::EditSettings(bool initialEditing){
		// True <=> new settings have been accepted (and adopted by this Image), otherwise False
		// - displaying the dialog and processing its result
		TCHAR firmware[80];
		::wsprintf( firmware, _T(GW_DEVICE_NAME) _T(" Firmware %d.%d%c(Main)"), firmwareInfo.major, firmwareInfo.minor, firmwareInfo.isMainFirmware*' ' );
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		const bool result=params.EditInModalDialog( *this, firmware, initialEditing );
		// - if this the InitialEditing, making sure the internal representation is empty
		if (initialEditing)
			DestroyAllTracks();
		return result;
	}

	TStdWinError CGreaseweazleV4::Reset(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - base (already sampled Tracks are unnecessary to be destroyed)
		BYTE tmp[sizeof(internalTracks)];
		::memcpy( tmp, internalTracks, sizeof(internalTracks) );
		::ZeroMemory( internalTracks, sizeof(internalTracks) );
			const TStdWinError err=__super::Reset();
		::memcpy( internalTracks, tmp, sizeof(internalTracks) );
		if (err!=ERROR_SUCCESS)
			return err;
		// - resetting the Greaseweazle Device
		EXCLUSIVELY_LOCK_DEVICE();
		switch (driver){
			case TDriver::USBSER:
				if (!::PurgeComm(
						hDevice,
						PURGE_TXCLEAR | PURGE_TXABORT | PURGE_RXCLEAR | PURGE_RXABORT
					)
				)
					return ::GetLastError();
				else
					break;
			default:
				ASSERT(FALSE);
				return ERROR_BAD_DRIVER;
		}
		if (const TStdWinError err=SendRequest( TRequest::SOFT_RESET, nullptr, 0 ))
			return err;
		Disconnect();
		return Connect();
	}

	void CGreaseweazleV4::SetPathName(LPCTSTR lpszPathName,BOOL bAddToMRU){
		TCHAR copy[DEVICE_NAME_CHARS_MAX+1];
		__super::SetPathName( ::lstrcpy(copy,lpszPathName), bAddToMRU );
		m_strPathName=copy;
	}
