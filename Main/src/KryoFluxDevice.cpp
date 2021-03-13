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
				const HDEVINFO hDevInfo=SetupDi::GetClassDevs( &GUID_KRYOFLUX, nullptr, nullptr, DIGCF_PRESENT|DIGCF_DEVICEINTERFACE );
				if (hDevInfo==INVALID_HANDLE_VALUE) // never connected to this computer
					return nullptr;
				SP_DEVINFO_DATA devInfoData={ sizeof(devInfoData) };
				SP_DEVICE_INTERFACE_DATA devIntfData={ sizeof(devIntfData) };
				if (SetupDi::EnumDeviceInterfaces( hDevInfo, nullptr, &GUID_KRYOFLUX, 0, &devIntfData )!=0){
					// yes, currently connected to this computer
					struct{
						SP_DEVICE_INTERFACE_DETAIL_DATA detail;
						TCHAR buffer[200];
					} str;
					DWORD dwSize=0;
					str.detail.cbSize=sizeof(str.detail);
					if (SetupDi::GetDeviceInterfaceDetail( hDevInfo, &devIntfData, &str.detail, sizeof(str), &dwSize, nullptr )!=0)
						::lstrcpy( devicePathBuf, str.detail.DevicePath );
				}
				SetupDi::DestroyDeviceInfoList(hDevInfo);
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
		Medium::FLOPPY_ANY, // supported Media
		Codec::FLOPPY_ANY, // supported Codecs
		1,2*6144	// Sector supported min and max length
	};







	#define KF_INTERFACE		1
	#define KF_EP_BULK_OUT		0x01
	#define KF_EP_BULK_IN		0x82

	CKryoFluxDevice::CKryoFluxDevice(TDriver driver,BYTE fddId)
		// ctor
		// - base
		: CKryoFluxBase( &Properties, fddId+'0', firmwareVersion )
		// - initialization
		, driver(driver) , fddId(fddId)
		, dataBuffer( (PBYTE)::malloc(KF_BUFFER_CAPACITY) )
		, fddFound(false)
		, lastCalibratedCylinder(0)
		// - connecting to a local KryoFlux device
		, hDevice(INVALID_HANDLE_VALUE) {
		winusb.hLibrary = winusb.hDeviceInterface = INVALID_HANDLE_VALUE;
		Connect();
		DestroyAllTracks(); // because Connect scans zeroth Track
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
					if (WinUsb::Initialize( hDevice, &winusb.hLibrary )!=0
						&&
						WinUsb::GetAssociatedInterface( winusb.hLibrary, KF_INTERFACE-1, &winusb.hDeviceInterface )!=0
					){
						WinUsb::SetPipePolicy( winusb.hDeviceInterface, KF_EP_BULK_IN, SHORT_PACKET_TERMINATE, sizeof(enable), &enable );
						WinUsb::SetPipePolicy( winusb.hDeviceInterface, KF_EP_BULK_IN, AUTO_CLEAR_STALL, sizeof(enable), &enable );
						WinUsb::SetPipePolicy( winusb.hDeviceInterface, KF_EP_BULK_IN, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout );

						WinUsb::SetPipePolicy( winusb.hDeviceInterface, KF_EP_BULK_OUT, SHORT_PACKET_TERMINATE, sizeof(enable), &enable );
						WinUsb::SetPipePolicy( winusb.hDeviceInterface, KF_EP_BULK_OUT, AUTO_CLEAR_STALL, sizeof(enable), &enable );
						WinUsb::SetPipePolicy( winusb.hDeviceInterface, KF_EP_BULK_OUT, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout );
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
					WinUsb::Free( winusb.hDeviceInterface );
				if (winusb.hLibrary!=INVALID_HANDLE_VALUE)
					WinUsb::Free( winusb.hLibrary );
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
				if (WinUsb::GetDescriptor(
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
					if (WinUsb::GetDescriptor(
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
			while (!CKryoFluxDevice(driver,precompensation.driveLetter))
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
				return	WinUsb::ControlTransfer(
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
				return	WinUsb::ReadPipe(
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
				return	WinUsb::WritePipe(
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

	DWORD CKryoFluxDevice::TrackToKfw1(CTrackReader tr) const{
		// converts specified Track representation into "KFW" format and returns the length of the representation
		union{
			PBYTE pb;
			PWORD pw;
			PDWORD pdw;
		};
		pb=dataBuffer;
		// - composing the Histogram of unique flux lengths
		static const WORD UNIQUE_FLUXES_COUNT_MAX=10000;
		class CHistogram sealed{
			WORD nUniqueFluxes;
			struct TUniqueFluxInfo sealed{
				WORD sampleCounter;
				WORD orderIndex;
				DWORD nOccurences;
			} uniqueFluxes[UNIQUE_FLUXES_COUNT_MAX];
			WORD descendingByOccurence[UNIQUE_FLUXES_COUNT_MAX]; // keys (SampleCounters) into UniqueFluxes
			CMapWordToPtr sampleCounterToFluxInfo;
		public:
			inline CHistogram()
				// ctor
				: nUniqueFluxes(0) {
			}

			void Add(WORD sampleCounter){
				// registers a new SampleCounter in this Histogram, eventually moving it "up" in the rank of most occurring SampleCounters
				PVOID value;
				if (sampleCounterToFluxInfo.Lookup(sampleCounter,value)){
					TUniqueFluxInfo &r=*(TUniqueFluxInfo *)value;
					for( r.nOccurences++; r.orderIndex>0; r.orderIndex-- ){
						const WORD moreOccuringSampleCounter=descendingByOccurence[r.orderIndex-1];
						sampleCounterToFluxInfo.Lookup( moreOccuringSampleCounter, value );
						TUniqueFluxInfo &rTmp=*(TUniqueFluxInfo *)value;
						if (rTmp.nOccurences>=r.nOccurences)
							break;
						descendingByOccurence[ rTmp.orderIndex=r.orderIndex ]=moreOccuringSampleCounter;
					}
					descendingByOccurence[r.orderIndex]=sampleCounter;
				}else if (nUniqueFluxes<UNIQUE_FLUXES_COUNT_MAX){
					TUniqueFluxInfo &r=uniqueFluxes[nUniqueFluxes];
						r.sampleCounter = descendingByOccurence[r.orderIndex=nUniqueFluxes] = sampleCounter;
						r.nOccurences=1;
					sampleCounterToFluxInfo.SetAt( sampleCounter, &r );
					nUniqueFluxes++;
				}else
					ASSERT(FALSE); // UNIQUE_FLUXES_COUNT_MAX should be enough, however the value can anytime be increased
			}

			inline WORD GetUniqueFluxesCount() const{
				return nUniqueFluxes;
			}

			inline WORD GetIndex(WORD sampleCounter) const{
				PVOID value;
				sampleCounterToFluxInfo.Lookup( sampleCounter, value );
				return ((TUniqueFluxInfo *)value)->orderIndex;
			}

			inline WORD operator[](int i) const{
				return descendingByOccurence[i];
			}
		} histogram;
		DWORD totalSampleCounter=0;
		for( tr.SetCurrentTime(0); tr; ){
			const TLogTime currTime=tr.ReadTime();
			int sampleCounter= TimeToStdSampleCounter(currTime)-totalSampleCounter; // temporary 64-bit precision even on 32-bit machines
			if (sampleCounter<=0){ // just to be sure
				ASSERT(FALSE); // we shouldn't end up here!
				continue;
			}
			totalSampleCounter+=sampleCounter;
			if (sampleCounter>0xffff)
				continue; // long fluxes below replaced with sequence of quick fluxes to indicate non-formatted area
			histogram.Add(sampleCounter);
		}
		// - writing Signature
		static const BYTE Signature[]={ 'K', 'F', 'W', '\x1' };
		pb=(PBYTE)::memcpy( pb, Signature, sizeof(Signature) )+sizeof(Signature);
		// - writing header
		struct TUniqueFlux sealed{
			BYTE three;
			BYTE index;
			Utils::CBigEndianWord sampleCounter;
		};
		ASSERT( sizeof(TUniqueFlux)==sizeof(DWORD) );
		static const BYTE Data1[]={ 0xF4, 0x01, 0x00, 0x00, 0x88, 0x13, 0x00, 0x00 }; // TODO: find out the meaning
		pb=(PBYTE)::memcpy( pb, Data1, sizeof(Data1) )+sizeof(Data1);
		const BYTE nUniqueFluxesUsed=std::min( (WORD)255, histogram.GetUniqueFluxesCount() );
		const DWORD nUsedFluxesTableBytes = *pdw++ = nUniqueFluxesUsed*sizeof(TUniqueFlux)+0x0E; // TODO: find out why 0x0E
		DWORD &rnFluxDataBytes=*pdw++; // set below
		DWORD &rnTrackDataBytes=*pdw++; // set below
		pb=(PBYTE)::ZeroMemory(pb,40)+40; // TODO: are these 40 Bytes reserved and thus always zero?
		*pb++=4; // TODO: find out why 4
		*pb++=2; // TODO: find out why 2
		*pb++=0; // TODO: find out why 0
		for( BYTE i=0; i<nUniqueFluxesUsed; i++ ){
			const TUniqueFlux uf={ 3, i+1, histogram[i] }; // TODO: find out why 3
			*pdw++=*(PDWORD)&uf;
		}
		static const BYTE FluxTablePostamble[]={ 0x0B, 0x05, 0x09, 0x00, 0x01, 0x05, 0x07, 0x0A, 0x05, 0x06, 0x01 }; // TODO: find out the meaning
		pb=(PBYTE)::memcpy( pb, FluxTablePostamble, sizeof(FluxTablePostamble) )+sizeof(FluxTablePostamble);
		const WORD nHeaderBytes=(pb-dataBuffer+63)/64*64; // rounding header to whole multiples of 64 Bytes
		pb=(PBYTE)::ZeroMemory(pb,64)+nHeaderBytes-pb+dataBuffer;
		// - converting UniqueFluxesUsed to an auxiliary Track (with LogicalTime set to SampleCounter) so that nearest neighbors can be used to approximate fluxes excluded from the Histogram
		CTrackReaderWriter trwFluxes( nUniqueFluxesUsed, CTrackReader::TDecoderMethod::NONE, false );
		for( BYTE i=0; i<nUniqueFluxesUsed; i++ )
			trwFluxes.AddTime( histogram[i] );
		std::sort( trwFluxes.GetBuffer(), trwFluxes.GetBuffer()+nUniqueFluxesUsed );
		// - writing fluxes
		const PBYTE fluxesStart=pb;
		static const BYTE FluxesPreamble[]={ 0x00, 0x12, 0x00, 0x00 };
		pb=(PBYTE)::memcpy( pb, FluxesPreamble, sizeof(FluxesPreamble) )+sizeof(FluxesPreamble);
		totalSampleCounter=0;
		for( tr.SetCurrentTime(0); tr; ){
			const TLogTime currTime=tr.ReadTime();
			const int sampleCounter= TimeToStdSampleCounter(currTime)-totalSampleCounter; // temporary 64-bit precision even on 32-bit machines
			if (sampleCounter<=0){ // just to be sure
				ASSERT(FALSE); // we shouldn't end up here!
				continue;
			}
			totalSampleCounter+=sampleCounter;
			if (sampleCounter>0xffff){
				ASSERT(FALSE); // TODO: replacing long fluxes with quick sequence of short fluxes to indicate non-formatted area
				continue;
			}
			if (((pb-fluxesStart)&0x7fff)!=0x7ffc){
				// normal representation of flux as the index into the table of fluxes
				trwFluxes.SetCurrentTime( sampleCounter );
				trwFluxes.TruncateCurrentTime();
				const TLogTime smallerSampleCounter=trwFluxes.GetCurrentTime();
				const TLogTime biggerSampleCounter=trwFluxes.ReadTime();
				if (sampleCounter-smallerSampleCounter<biggerSampleCounter-sampleCounter || biggerSampleCounter<=0)
					*pb++=1+histogram.GetIndex( smallerSampleCounter ); // closer to SmallerSampleCounter or no BiggerSampleCounter
				else
					*pb++=1+histogram.GetIndex( biggerSampleCounter ); // closer to BiggerSampleCounter
				ASSERT( 1<=pb[-1] && pb[-1]<=nUniqueFluxesUsed );
			}else{
				// each 32768 Bytes of flux data is a "check?" mark 0xb00 followed by a big endian sample counter of the next flux instead of an index
				*pw++=0xb00;
				::memcpy( pw++, &Utils::CBigEndianWord(sampleCounter), sizeof(WORD) );
			}
		}
		rnFluxDataBytes=pb-fluxesStart+8; // TODO: find out why 8
		rnTrackDataBytes=nUsedFluxesTableBytes+rnFluxDataBytes+0x18; // TODO: find out why 0x18
		// - padding the content to a whole multiple of 64 Bytes
		*pw++=0x2000; // TODO: find out the meaning
		const DWORD nContentBytes=(pb-dataBuffer+63)/64*64;
		pb=(PBYTE)::ZeroMemory( pb, 64 )+nContentBytes-pb+dataBuffer;
		// - successfully processed
		::SetLastError(ERROR_SUCCESS);
		return pb-dataBuffer;
	}

	#define ERROR_SAVE_MESSAGE_TEMPLATE	_T("Track %02d.%c saving%s failed")

	TStdWinError CKryoFluxDevice::SaveTrack(TCylinder cyl,THead head) const{
		// saves the specified Track to the inserted Medium; returns Windows standard i/o error
		// - Track must already exist from before
		const PInternalTrack pit=internalTracks[cyl][head];
		if (!pit)
			return ERROR_GEN_FAILURE;
		if (!pit->modified)
			return ERROR_SUCCESS;
		// - extracting the "best" Revolution into a temporary Track
		//TODO better
		pit->FlushSectorBuffers(); // convert all modifications into flux transitions
		CTrackReaderWriter trw( pit->GetTimesCount(), CTrackReader::TDecoderMethod::FDD_KEIR_FRASER, false );
		trw.AddIndexTime(0);
			const TLogTime tIndex0=pit->GetIndexTime(0), tIndex1=pit->GetIndexTime(1);
			pit->RewindToIndex(0);
			while (*pit && pit->GetCurrentTime()<tIndex1)
				trw.AddTime( pit->ReadTime()-tIndex0 );
		trw.AddIndexTime( tIndex1-tIndex0 );
		if (floppyType!=Medium::UNKNOWN){
			trw.SetMediumType( floppyType );
			trw.Normalize();
		}
		// - pre-compensation of the temporary Track
		TStdWinError err;
		if ( err=precompensation.ApplyTo(*this,trw) )
			return err;
		// - Drive's head calibration
		if (params.calibrationStepDuringFormatting)
			if (std::abs(cyl-lastCalibratedCylinder)>=params.calibrationStepDuringFormatting){
				lastCalibratedCylinder=cyl;
				SeekHome();
			}
		// - writing (and optional verification)
		char nSilentRetrials=3;
		do{
			// . converting the temporary Track to "KFW" data, below streamed directly to KryoFlux
			const DWORD nBytesToWrite=TrackToKfw1( trw );
			#ifdef _DEBUG
				if (false){
					CFile f;
					::CreateDirectory( _T("r:\\kfw"), nullptr );
					TCHAR kfwName[80];
					::wsprintf( kfwName, _T("r:\\kfw\\track%02d-%c.bin"), cyl, '0'+head );
					f.Open( kfwName, CFile::modeCreate|CFile::modeWrite|CFile::typeBinary|CFile::shareExclusive );
						f.Write( dataBuffer, nBytesToWrite );
					f.Close();
				}
			#endif
			// . clearing i/o pipes
			while (!WinUsb::AbortPipe( winusb.hDeviceInterface, KF_EP_BULK_OUT ));
			WinUsb::ResetPipe( winusb.hDeviceInterface, KF_EP_BULK_OUT );
			while (!WinUsb::AbortPipe( winusb.hDeviceInterface, KF_EP_BULK_IN ));
			WinUsb::ResetPipe( winusb.hDeviceInterface, KF_EP_BULK_IN );
			// . streaming the "KFW" data to KryoFlux
			SendRequest( TRequest::INDEX_WRITE, 2 ); // waiting for an index?
			if (!SetMotorOn() || !SelectHead(head) || !SeekTo(cyl))
				return ERROR_NOT_READY;
			SendRequest( TRequest::STREAM, 2 ); // start streaming
				err=WriteFull( dataBuffer, nBytesToWrite );
				if (err==ERROR_SUCCESS)
					do{
						if (err=SendRequest( TRequest::RESULT_WRITE ))
							break;
					}while (::strrchr(lastRequestResultMsg,'=')[1]=='9'); // TODO: explain why sometimes instead of '0' a return code is '3' but the Track has been written; is it a timeout? if yes, how to solve it?
			SendRequest( TRequest::STREAM, 0 ); // stop streaming
			TCHAR msgSavingFailed[80];
			::wsprintf( msgSavingFailed, ERROR_SAVE_MESSAGE_TEMPLATE, cyl, '0'+head, params.verifyWrittenTracks?_T(" and verification"):_T("") );
			if (err) // writing to the device failed
				switch (
					nSilentRetrials-->0
					? IDRETRY
					: Utils::AbortRetryIgnore(msgSavingFailed,err,MB_DEFBUTTON2)
				){
					case IDIGNORE:	// ignoring the Error
						break;
					case IDABORT:	// aborting the saving
						return err;
					default:		// attempting to save the Track once more
						continue;
				}
			// . write verification
			if (!err && params.verifyWrittenTracks && pit->nSectors>0){ // can verify the Track only if A&B&C, A = writing successfull, B&C = at least one Sector is recognized in it
				internalTracks[cyl][head]=nullptr; // forcing rescan
					ScanTrack( cyl, head );
					if (const PInternalTrack pitVerif=internalTracks[cyl][head]){
						if (pitVerif->nSectors>0){
							const PInternalTrack pitWritten=CInternalTrack::CreateFrom( *this, trw );
								const auto &revWrittenFirstSector=pitWritten->sectors[0].revolutions[0];
								pitWritten->SetCurrentTimeAndProfile( revWrittenFirstSector.idEndTime, revWrittenFirstSector.idEndProfile );
								const auto &revVerifFirstSector=pitVerif->sectors[0].revolutions[0];
								pitVerif->SetCurrentTimeAndProfile( revVerifFirstSector.idEndTime, revVerifFirstSector.idEndProfile );
								while ( // comparing common cells until the next Index pulse is reached
									*pitWritten && pitWritten->GetCurrentTime()<pitWritten->GetIndexTime(1)
									&&
									*pitVerif && pitVerif->GetCurrentTime()<pitVerif->GetIndexTime(1)
								)
									if (pitWritten->ReadBit()!=pitVerif->ReadBit()){
										err=ERROR_DS_COMPARE_FALSE;
										break;
									}
							delete pitWritten;
						}else
							err=ERROR_SECTOR_NOT_FOUND;
						delete pitVerif;
					}else
						err=ERROR_GEN_FAILURE;
				internalTracks[cyl][head]=pit;
				if (err) // verification failed
					switch (
						nSilentRetrials-->0
						? IDRETRY
						: Utils::AbortRetryIgnore(msgSavingFailed,err,MB_DEFBUTTON2)
					){
						case IDIGNORE:	// ignoring the Error
							break;
						case IDABORT:	// aborting the saving
							return err;
						default:		// attempting to save the Track once more
							continue;
					}
			}
			err=ERROR_SUCCESS;
		}while (err!=ERROR_SUCCESS);
		// - (successfully) saved - see TODOs
		pit->modified=false;
		return ERROR_SUCCESS;
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
			const BYTE nIndicesRequested=std::min<BYTE>( GetAvailableRevolutionCount()+1, Revolution::MAX ); // N+1 indices = N full revolutions
			SendRequest( TRequest::STREAM, MAKEWORD(1,nIndicesRequested) ); // start streaming
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
			if (CTrackReaderWriter trw=StreamToTrack( dataBuffer, p-dataBuffer )){
				// it's a KryoFlux Stream whose data make sense
				if (floppyType!=Medium::UNKNOWN){ // may be unknown if Medium is still being recognized
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
				if (IsTrackHealthy(cyl,head) || !pit->nSectors) // Track explicitly healthy or without Sectors
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
						lastCalibratedCylinder=cyl;
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
		//EXCLUSIVELY_LOCK_THIS_IMAGE(); // commented out as the following Dialog creates a parallel thread that in turn would attempt to lock this Image, yielding a deadlock
		// - making sure the firmware is uploaded
		Disconnect(), Connect();
		UploadFirmware();
		// - base
		const bool result=__super::EditSettings(initialEditing);
		// - if this the InitialEditing, making sure the internal representation is empty
		if (initialEditing)
			DestroyAllTracks();
		return result;
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
		// - TODO: the following regard writing to disk and needs to be explained
		do{
			SetMotorOn();
			SendRequest( TRequest::INDEX_WRITE, 8 );
		}while (::strrchr(lastRequestResultMsg,'=')[1]!='8');
		// - resetting the KryoFlux device
		return ERROR_SUCCESS;
	}
