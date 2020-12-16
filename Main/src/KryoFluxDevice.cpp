#include "stdafx.h"
#include "CapsBase.h"
#include "KryoFluxBase.h"
#include "KryoFluxDevice.h"

/*
	The KryoFlux support is heavily inspired, or after rewriting fully adopted,
	from Simon Owen's SamDisk

	Copyright (c) 2002-2020 Simon Owen, https://simonowen.com

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.
*/


	#define KF_DEVICE_NAME_ANSI		"KryoFlux DiskSystem"
	#define KF_DEVICE_NAME_PATTERN	_T("KryoFlux floppy drive #%c (%s.sys)")

	#define KF_ACCESS_DRIVER_WINUSB	_T("winusb")

	#define KF_DRIVES_MAX			2

	DEFINE_GUID( GUID_KRYOFLUX, 0x9E09C9CD, 0x5068, 0x4b31, 0x82, 0x89, 0xE3, 0x63, 0xE4, 0xE0, 0x62, 0xAC );

	LPCTSTR CKryoFluxDevice::GetDevicePath(TDriver driver,PTCHAR devicePathBuf){
		// determines and returns the path of a locally connected KryoFlux device; returns Null if device not found
		*devicePathBuf='\0'; // initialization
		switch (driver){
			case TDriver::WINUSB:{
				const HDEVINFO hDevInfo=::SetupDiGetClassDevs( &GUID_KRYOFLUX, nullptr, nullptr, DIGCF_PRESENT|DIGCF_DEVICEINTERFACE );
				if (hDevInfo==INVALID_HANDLE_VALUE) // never connected to this computer
					return nullptr;
				SP_DEVINFO_DATA devInfoData={ sizeof(devInfoData) };
				SP_DEVICE_INTERFACE_DATA devIntfData={ sizeof(devIntfData) };
				if (::SetupDiEnumDeviceInterfaces( hDevInfo, nullptr, &GUID_KRYOFLUX, 0, &devIntfData )!=0){
					// yes, currently connected to this computer
					struct{
						SP_DEVICE_INTERFACE_DETAIL_DATA detail;
						TCHAR buffer[200];
					} str;
					DWORD dwSize=0;
					str.detail.cbSize=sizeof(str.detail);
					if (::SetupDiGetDeviceInterfaceDetail( hDevInfo, &devIntfData, &str.detail, sizeof(str), &dwSize, nullptr )!=0)
						::lstrcpy( devicePathBuf, str.detail.DevicePath );
				}
				::SetupDiDestroyDeviceInfoList(hDevInfo);
				break;
			}
			default:
				ASSERT(FALSE); // all available access possibilities should be covered!
				return nullptr;
		}
		return	*devicePathBuf!='\0' ? devicePathBuf : nullptr;
	}

	#define KF_FIRMWARE_LOAD_ADDR	0x202000
    #define KF_FIRMWARE_EXEC_ADDR	KF_FIRMWARE_LOAD_ADDR
	#define KF_BUFFER_CAPACITY		1000000

	LPCTSTR CKryoFluxDevice::Recognize(PTCHAR deviceNameList){
		// returns a null-separated list of floppy drives connected via a local KryoFlux device
		// - evaluating possibilities how to access KryoFlux
		ASSERT( deviceNameList!=nullptr );
		TDriver driver;
		if (GetDevicePath( TDriver::WINUSB, deviceNameList )!=nullptr)
			driver=TDriver::WINUSB;
		else
			return nullptr; // KryoFlux inaccessible
		// - checking if firmware loaded
		if (CKryoFluxDevice tmp=CKryoFluxDevice( driver, 0 )) // connected ...
			while (const TStdWinError err=tmp.UploadFirmware()){ // ... but firmware failed to load
				if (Utils::QuestionYesNo( _T("KryoFlux found but without firmware loaded. Load it manually?\n\nUnless you move the firmware file, this step is needed only once."), MB_DEFBUTTON1 )){
					TCHAR fileName[MAX_PATH];
					*fileName='\0';
					CFileDialog d( TRUE, _T(".bin"), nullptr, OFN_FILEMUSTEXIST, _T("Firmware (*.bin)|*.bin|") );
						d.m_ofn.lStructSize=sizeof(OPENFILENAME); // to show the "Places bar"
						d.m_ofn.nFilterIndex=1;
						d.m_ofn.lpstrTitle=_T("Select KryoFlux firmware file");
						d.m_ofn.lpstrFile=::lstrcpy( fileName, _T("firmware_kf_usb_rosalie.bin") );
					if (d.DoModal()==IDOK){
						tmp.params.firmwareFileName=fileName;
						continue;
					}
				}
				return nullptr;
			}
		// - enumerating connected floppy drives
		PTCHAR p=deviceNameList;
			for( BYTE fddId=0; fddId<KF_DRIVES_MAX; fddId++ )
				//if (CKryoFluxDevice( TDriver::WINUSB, fddId ).fddFound)
					p+=::wsprintf( p, KF_DEVICE_NAME_PATTERN, fddId+'0', KF_ACCESS_DRIVER_WINUSB )+1; // "+1" = null-terminated items		
		// - no further access possibilities
		*p='\0'; // null-terminated list
		return deviceNameList;
	}

	PImage CKryoFluxDevice::Instantiate(LPCTSTR deviceName){
		// creates and returns a KryoFluxDevice instance for a specified floppy drive
		TCHAR fddId, driverStr[16], tmp[MAX_PATH];
		*_tcsrchr( ::lstrcpy(tmp,deviceName), '.' )='\0';
		::sscanf( tmp, KF_DEVICE_NAME_PATTERN, &fddId, driverStr );
		if (!::lstrcmp(driverStr,KF_ACCESS_DRIVER_WINUSB))
			return new CKryoFluxDevice( TDriver::WINUSB, fddId-'0' );
		ASSERT(FALSE);
		::SetLastError( ERROR_BAD_DEVICE );
		return nullptr;
	}

	const CImage::TProperties CKryoFluxDevice::Properties={
		MAKE_IMAGE_ID('C','A','P','S','_','K','F','D'), // a unique identifier
		Recognize,	// list of recognized device names
		Instantiate,	// instantiation function
		nullptr, // filter
		TMedium::FLOPPY_ANY, // supported Media
		Codec::FLOPPY_ANY, // supported Codecs
		1,2*6144	// Sector supported min and max length
	};







	#define KF_INTERFACE		1
	#define KF_EP_BULK_OUT		0x01
	#define KF_EP_BULK_IN		0x82

	CKryoFluxDevice::CKryoFluxDevice(TDriver driver,BYTE fddId)
		// ctor
		// - base
		: CKryoFluxBase( &Properties, firmwareVersion )
		// - initialization
		, driver(driver) , fddId(fddId)
		, dataBuffer( (PBYTE)::malloc(KF_BUFFER_CAPACITY) )
		, fddFound(false)
		// - connecting to a local KryoFlux device
		, hDevice(INVALID_HANDLE_VALUE) {
		winusb.hLibrary = winusb.hDeviceInterface = INVALID_HANDLE_VALUE;
		Connect();
		// - setting a classical 5.25" floppy geometry
		capsImageInfo.maxcylinder=FDD_CYLINDERS_MAX/2-1; // inclusive!
		capsImageInfo.maxhead=2-1; // inclusive!
	}

	CKryoFluxDevice::~CKryoFluxDevice(){
		// dtor
		EXCLUSIVELY_LOCK_THIS_IMAGE(); // mustn't destroy this instance while it's being used!
		Disconnect();
		DestroyAllTracks(); // see Reset()
		::free( dataBuffer );
	}








	bool CKryoFluxDevice::Connect(){
		// True <=> successfully connected to a local KryoFlux device, otherwise False
		// - connecting to the device
		ASSERT( hDevice==INVALID_HANDLE_VALUE );
		hDevice=::CreateFile(
					GetDevicePath(driver,(PTCHAR)dataBuffer),
					GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_WRITE | FILE_SHARE_READ,
					nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr
				);
		// - setting transfer properties
		if (hDevice!=INVALID_HANDLE_VALUE){
			DWORD timeout=1500; // milliseconds
			BYTE enable=1;
			switch (driver){
				case TDriver::WINUSB:
					ASSERT( winusb.hLibrary==INVALID_HANDLE_VALUE && winusb.hDeviceInterface==INVALID_HANDLE_VALUE );
					if (::WinUsb_Initialize( hDevice, &winusb.hLibrary )!=0
						&&
						::WinUsb_GetAssociatedInterface( winusb.hLibrary, KF_INTERFACE-1, &winusb.hDeviceInterface )!=0
					){
						::WinUsb_SetPipePolicy( winusb.hDeviceInterface, KF_EP_BULK_IN, SHORT_PACKET_TERMINATE, sizeof(enable), &enable );
						::WinUsb_SetPipePolicy( winusb.hDeviceInterface, KF_EP_BULK_IN, AUTO_CLEAR_STALL, sizeof(enable), &enable );
						::WinUsb_SetPipePolicy( winusb.hDeviceInterface, KF_EP_BULK_IN, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout );

						::WinUsb_SetPipePolicy( winusb.hDeviceInterface, KF_EP_BULK_OUT, SHORT_PACKET_TERMINATE, sizeof(enable), &enable );
						::WinUsb_SetPipePolicy( winusb.hDeviceInterface, KF_EP_BULK_OUT, AUTO_CLEAR_STALL, sizeof(enable), &enable );
						::WinUsb_SetPipePolicy( winusb.hDeviceInterface, KF_EP_BULK_OUT, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout );
					}
					break;
				default:
					ASSERT(FALSE);
					return false;
			}
		}
		// - selecting floppy drive
		SendRequest( TRequest::DEVICE, fddId ); // not checking for success as firmware may not yet have been loaded
		fddFound =	internalTracks[0][0]!=nullptr // floppy drive already found before disconnecting from KryoFlux?
					? true
					: ScanTrack(0,0)>0 || internalTracks[0][0]!=nullptr;
		// - evaluating connection
		return *this;
	}

	void CKryoFluxDevice::Disconnect(){
		// disconnects from local KryoFlux device
		EXCLUSIVELY_LOCK_THIS_IMAGE(); // mustn't disconnect while device in use!
		fddFound=false;
		switch (driver){
			case TDriver::WINUSB:
				if (winusb.hDeviceInterface!=INVALID_HANDLE_VALUE)
					::WinUsb_Free( winusb.hDeviceInterface );
				if (winusb.hLibrary!=INVALID_HANDLE_VALUE)
					::WinUsb_Free( winusb.hLibrary );
				winusb.hLibrary = winusb.hDeviceInterface = INVALID_HANDLE_VALUE;
				break;
			default:
				ASSERT(FALSE);
				break;
		}
		::CloseHandle( hDevice );
		hDevice=INVALID_HANDLE_VALUE;
	}

	CKryoFluxDevice::operator bool() const{
		// True <=> connection to a KryoFlux device established, otherwise False
		if (hDevice==INVALID_HANDLE_VALUE)
			return false;
		switch (driver){
			case TDriver::WINUSB:
				return	winusb.hLibrary!=INVALID_HANDLE_VALUE && winusb.hDeviceInterface!=INVALID_HANDLE_VALUE;
			default:
				ASSERT(FALSE);
				return false;
		}
	}

	TStdWinError CKryoFluxDevice::SamBaCommand(LPCSTR cmd,LPCSTR end) const{
		// performs a Sam-Ba command; returns Windows standard i/o error
		// https://sourceforge.net/p/lejos/wiki-nxt/SAM-BA%20Protocol
		if (const TStdWinError err=WriteFull( cmd, ::lstrlenA(cmd) ))
			return err;
		if (const int endLength=::lstrlenA(end)){
			for( PBYTE p=dataBuffer; const int nBytesFree=KF_BUFFER_CAPACITY+dataBuffer-p; )
				if (const auto n=Read( p, nBytesFree )){
					p+=n;
					if (p-dataBuffer>=endLength)
						if (!::memcmp( p-endLength, end, endLength ))
							return ERROR_SUCCESS;
				}else
					switch (const TStdWinError err=::GetLastError()){
						case ERROR_IO_PENDING:
							continue;
						default: // i/o operation anything but pending
							return err;
					}
			return ERROR_INSUFFICIENT_BUFFER;
		}else
			return ERROR_SUCCESS;
	}

	LPCTSTR CKryoFluxDevice::GetProductName() const{
		// determines and returns the product introduced under a KryoFlux device connection
		if (!*this)
			return nullptr;
		DWORD nBytesTransferred;
		switch (driver){
			case TDriver::WINUSB:{
				USB_DEVICE_DESCRIPTOR desc={ sizeof(desc), USB_DEVICE_DESCRIPTOR_TYPE };
				if (::WinUsb_GetDescriptor(
						winusb.hLibrary,
						USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, (PUCHAR)&desc, sizeof(desc),
						&nBytesTransferred
					)!=0
					&&
					desc.iProduct>0
				){
					struct{
						USB_STRING_DESCRIPTOR desc;
						WCHAR buf[MAX_PATH];
					} strW;
					if (::WinUsb_GetDescriptor(
							winusb.hLibrary,
							USB_STRING_DESCRIPTOR_TYPE, desc.iProduct, 0, (PUCHAR)&strW, sizeof(strW),
							&nBytesTransferred
						)!=0
						&&
						strW.desc.bLength>2
					){
						lastRequestResultMsg[
							::WideCharToMultiByte( CP_ACP, 0, strW.desc.bString,(strW.desc.bLength-2)/sizeof(WCHAR), lastRequestResultMsg,sizeof(lastRequestResultMsg), nullptr,nullptr )
						]='\0';
						return lastRequestResultMsg;
					}
				}
				return nullptr;
			}
			default:
				ASSERT(FALSE);
				return nullptr;
		}
	}

	TStdWinError CKryoFluxDevice::UploadFirmware(){
		// uploads firmware to a KryoFlux-based device; returns Windows standard i/o error
		// - uploading and launching a firmware
		if (::lstrcmpA( GetProductName(), KF_DEVICE_NAME_ANSI )){
			// . assuming failure
			::lstrcpyA( firmwareVersion, "Not loaded" );
			// . opening the firmware file for reading
			CFileException e;
			CFile f;
			if (!f.Open( params.firmwareFileName, CFile::modeRead|CFile::shareDenyWrite|CFile::typeBinary, &e ))
				return e.m_cause;
			// . firmware file must fit into the internal data buffer
			const auto fLength=f.GetLength();
			if (fLength>KF_BUFFER_CAPACITY)
				return ERROR_INSUFFICIENT_BUFFER;
			// . setting interactive then non-interactive mode to check for boot responses
			if (const TStdWinError err=SamBaCommand( "T#\r", ">" ))
				return err;
			if (const TStdWinError err=SamBaCommand( "N#\r", "\n\r" ))
				return err;
			// . uploading the firmware
			char cmd[32];
			::wsprintfA( cmd, "S%08lx,%08lx#\r", KF_FIRMWARE_LOAD_ADDR, fLength );
			if (const TStdWinError err=SamBaCommand( cmd, nullptr ))
				return err;
			if (f.Read( dataBuffer, fLength )!=fLength)
				return ::GetLastError();
			if (const TStdWinError err=WriteFull( dataBuffer, fLength ))
				return err;
			// . verifying the upload
			*cmd='R';
			if (const TStdWinError err=SamBaCommand( cmd, nullptr ))
				return err;
			const PBYTE p=(PBYTE)::malloc(fLength);
				const bool uploadedWrongly=	ReadFull( p, fLength )==ERROR_SUCCESS
											? ::memcmp( dataBuffer, p, fLength )!=0
											: false;
			::free(p);
			if (uploadedWrongly)
				return ERROR_NOT_READY;
			// . executing the firmware
			::wsprintfA( cmd, "G%08lx#\r", KF_FIRMWARE_EXEC_ADDR );
			if (const TStdWinError err=SamBaCommand( cmd, nullptr ))
				return err;
			// . waiting for KryoFlux to finish initialization by firmware
			while (!CKryoFluxDevice(driver,fddId))
				::Sleep(1000);
			// . reconnecting to the device may be needed
			Disconnect();
			if (!Connect())
				return ERROR_NOT_READY;
		}else{
			// . resetting the KryoFlux device
			if (const TStdWinError err=SendRequest( TRequest::RESET )) // TODO: is it necessary?
				return err;
		}
		// - retrieving firmware information
		::lstrcpyA( firmwareVersion, KF_DEVICE_NAME_ANSI );
		// - firmware successfully uploaded
		return ERROR_SUCCESS;
	}

	TStdWinError CKryoFluxDevice::SendRequest(TRequest req,WORD index,WORD value) const{
		// sends a command to a locally connected KryoFlux device; returns Windows standard i/o error
		if (!*this) // not connected
			return ERROR_NOT_READY;
		DWORD nBytesTransferred=0;
		switch (driver){
			case TDriver::WINUSB:{
				const WINUSB_SETUP_PACKET sp={
					(1<<7) | (2<<5) | (3<<0), // RequestType: IN | VENDOR | OTHER
					req, // Request
					value,
					index,
					sizeof(lastRequestResultMsg)
				};
				return	::WinUsb_ControlTransfer(
							winusb.hLibrary,
							sp, (PUCHAR)lastRequestResultMsg, sizeof(lastRequestResultMsg),
							&nBytesTransferred, nullptr
						)!=0
						? ERROR_SUCCESS
						: ::GetLastError();
			}
			default:
				ASSERT(FALSE);
				return ERROR_BAD_UNIT;
		}
	}

	DWORD CKryoFluxDevice::Read(PVOID buffer,DWORD nBytesFree) const{
		// reads a chunk of data received from a KryoFlux device; returns the number of Bytes received
		if (!*this) // not connected
			return 0;
		DWORD nBytesTransferred=0;
		switch (driver){
			case TDriver::WINUSB:
				return	::WinUsb_ReadPipe(
							winusb.hDeviceInterface,
							KF_EP_BULK_IN, (PUCHAR)buffer, nBytesFree,
							&nBytesTransferred, nullptr
						)!=0
						? nBytesTransferred
						: 0;
			default:
				ASSERT(FALSE);
				return 0;
		}
	}

	TStdWinError CKryoFluxDevice::ReadFull(PVOID buffer,DWORD nBytes) const{
		// blocks caller until all requested Bytes are read from the device; returns Windows standard i/o error
		for( PBYTE p=(PBYTE)buffer; nBytes>0; )
			if (const DWORD n=Read( buffer, nBytes )){
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

	DWORD CKryoFluxDevice::Write(LPCVOID buffer,DWORD nBytes) const{
		// writes a chunk of data to a KryoFlux device; returns the number of Bytes accepted by the device
		if (!*this) // not connected
			return 0;
		DWORD nBytesTransferred=0;
		switch (driver){
			case TDriver::WINUSB:
				return	::WinUsb_WritePipe(
							winusb.hDeviceInterface,
							KF_EP_BULK_OUT, (PUCHAR)buffer, nBytes,
							&nBytesTransferred, nullptr
						)!=0
						? nBytesTransferred
						: 0;
			default:
				ASSERT(FALSE);
				return 0;
		}
	}

	TStdWinError CKryoFluxDevice::WriteFull(LPCVOID buffer,DWORD nBytes) const{
		// blocks caller until all requested Bytes are written to the device; returns Windows standard i/o error
		for( PCBYTE p=(PCBYTE)buffer; nBytes>0; )
			if (const DWORD n=Write( buffer, nBytes )){
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

	bool CKryoFluxDevice::SetMotorOn(bool on) const{
		return	SendRequest( TRequest::MOTOR, on )==ERROR_SUCCESS;
	}

	bool CKryoFluxDevice::SetDensity(bool high) const{
		return	SendRequest( TRequest::DENSITY, high )==ERROR_SUCCESS;
	}

	bool CKryoFluxDevice::SeekTo(TCylinder cyl) const{
		return	SendRequest( TRequest::TRACK, cyl<<(BYTE)params.doubleTrackStep )==ERROR_SUCCESS;
	}

	inline
	bool CKryoFluxDevice::SeekHome() const{
		return SeekTo(0);
	}

	bool CKryoFluxDevice::SelectHead(THead head) const{
		return	SendRequest( TRequest::SIDE, head )==ERROR_SUCCESS;
	}









	BOOL CKryoFluxDevice::OnOpenDocument(LPCTSTR lpszPathName){
		// True <=> Image opened successfully, otherwise False
		// - base
		if (!__super::OnOpenDocument(lpszPathName)
			&&
			::GetLastError()!=ERROR_NOT_SUPPORTED // the CAPS library currently doesn't support reading Stream files
		)
			return FALSE;
		// - successfully mounted
		return TRUE; // failure may arise later on when attempting to access the Drive
	}

	BOOL CKryoFluxDevice::OnSaveDocument(LPCTSTR lpszPathName){
		// True <=> this Image has been successfully saved, otherwise False
		::SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	TSector CKryoFluxDevice::ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec,PSectorId bufferId,PWORD bufferLength,PLogTime startTimesNanoseconds,PBYTE pAvgGap3) const{
		// returns the number of Sectors found in given Track, and eventually populates the Buffer with their IDs (if Buffer!=Null); returns 0 if Track not formatted or not found
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return 0;
		// - if Track already scanned before, returning the result from before
		if (internalTracks[cyl][head]!=nullptr)
			return __super::ScanTrack( cyl, head, pCodec, bufferId, bufferLength, startTimesNanoseconds, pAvgGap3 );
		// - scanning (forced recovery from errors right during scanning)
		for( char nRecoveryTrials=3; true; nRecoveryTrials-- ){
			// . issuing a Request to the KryoFlux device to read fluxes in the specified Track
			if (!SeekTo(cyl) || !SelectHead(head) || !SetMotorOn())
				return 0;
			const BYTE nIndexesRequested=std::min( 4, DEVICE_REVOLUTIONS_MAX );
			SendRequest( TRequest::STREAM, MAKEWORD(1,nIndexesRequested) ); // start streaming
				PBYTE p=dataBuffer;
				while (const DWORD nBytesFree=KF_BUFFER_CAPACITY+dataBuffer-p)
					if (const auto n=Read( p, nBytesFree )){
						p+=n;
						if (p-dataBuffer>7
							&&
							!::memcmp( p-7, "\xd\xd\xd\xd\xd\xd\xd", 7 ) // the final Out-of-Stream block (see KryoFlux Stream specification for explanation)
						)
							break;
					}else if (::GetLastError()!=ERROR_IO_PENDING) // i/o operation anything but pending
						break; // possibly disconnected while in operation
				const TStdWinError err=::GetLastError();
			SendRequest( TRequest::STREAM, 0 ); // stop streaming
			if (err==ERROR_SEM_TIMEOUT) // currently, the only known way how to detect a non-existing FDD is to observe a timeout during reading
				return 0;
			// . making sure the read content is a KryoFlux Stream whose data actually make sense
			const CKfStream kfStream( dataBuffer, p-dataBuffer );
			if (!kfStream.GetError()){
				// it's a KryoFlux Stream whose data make sense
				CTrackReaderWriter trw=kfStream.ToTrack(*this);
				if (floppyType!=TMedium::UNKNOWN){ // may be unknown if Medium is still being recognized
					trw.SetMediumType(floppyType);
					if (params.normalizeReadTracks)
						trw.Normalize();
				}
				internalTracks[cyl][head]=CInternalTrack::CreateFrom( *this, trw );
				__super::ScanTrack( cyl, head, pCodec, bufferId, bufferLength, startTimesNanoseconds, pAvgGap3 );
			}
			// . if no more trials left, we are done
			if (nRecoveryTrials<=0)
				break;
			// . attempting to return good data
			if (const PCInternalTrack pit=internalTracks[cyl][head]){ // may be Null if, e.g., device manually reset, disconnected, etc.
				if (const_cast<CKryoFluxDevice *>(this)->IsTrackHealthy(cyl,head) || !pit->nSectors) // Track explicitly healthy or without Sectors
					return pit->nSectors;
				switch (params.calibrationAfterError){
					case TParams::TCalibrationAfterError::NONE:
						// calibration disabled
						return pit->nSectors;
					case TParams::TCalibrationAfterError::ONCE_PER_CYLINDER:
						// calibrating only once for the whole Cylinder
						nRecoveryTrials=0;
						//fallthrough
					case TParams::TCalibrationAfterError::FOR_EACH_SECTOR:
						SeekHome();
						break;
				}				
				delete pit; // disposing the erroneous Track ...
				internalTracks[cyl][head]=nullptr; // ... and attempting to obtain its data after head has been calibrated
			}
		}
		// - returning whatever has been read
		if (const PCInternalTrack pit=internalTracks[cyl][head]) // may be Null if, e.g., device manually reset, disconnected, etc.
			return pit->nSectors;
		else
			return 0;
	}

	bool CKryoFluxDevice::EditSettings(bool initialEditing){
		// True <=> new settings have been accepted (and adopted by this Image), otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - making sure the firmware is uploaded
		Disconnect(), Connect();
		UploadFirmware();
		// - base
		return __super::EditSettings(initialEditing);
	}

	TStdWinError CKryoFluxDevice::Reset(){
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
		// - resetting the KryoFlux device
		return ERROR_SUCCESS;
	}
