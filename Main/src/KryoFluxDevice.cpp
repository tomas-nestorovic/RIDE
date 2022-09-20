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
	static_assert( sizeof(KF_DEVICE_NAME_PATTERN)/sizeof(TCHAR)+8<=DEVICE_NAME_CHARS_MAX, "Identifier too long" );

	#define KF_ACCESS_DRIVER_WINUSB	_T("winusb")

	#define KF_DRIVES_MAX			2

	DEFINE_GUID( GUID_DEVINTERFACE_KRYOFLUX, 0x9E09C9CD, 0x5068, 0x4b31, 0x82, 0x89, 0xE3, 0x63, 0xE4, 0xE0, 0x62, 0xAC );

	#define KF_FIRMWARE_LOAD_ADDR	0x202000
    #define KF_FIRMWARE_EXEC_ADDR	KF_FIRMWARE_LOAD_ADDR

	LPCTSTR CKryoFluxDevice::Recognize(PTCHAR deviceNameList){
		// returns a null-separated list of floppy drives connected via a local KryoFlux device
		// - evaluating possibilities how to access KryoFlux
		ASSERT( deviceNameList!=nullptr );
		if (SetupDi::GetDevicePath( GUID_DEVINTERFACE_KRYOFLUX ).IsEmpty())
			return nullptr; // KryoFlux inaccessible
		// - checking if firmware loaded
		if (CKryoFluxDevice &&tmp=CKryoFluxDevice( TDriver::WINUSB, 0 )) // connected ...
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
						tmp.paramsEtc.firmwareFileName=fileName;
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

	#define EXCLUSIVELY_LOCK_DEVICE()	const Utils::CExclusivelyLocked deviceLocker(device.locker)
	// always lock first the Image and THEN the Device, so that the locking is compatible with base classes!

	#define hDevice	device.handle
	#define winusb	device.winusb

	CKryoFluxDevice::CKryoFluxDevice(TDriver driver,BYTE fddId)
		// ctor
		// - base
		: CKryoFluxBase( &Properties, fddId+'0', device.firmwareVersion )
		// - initialization
		, driver(driver) , fddId(fddId)
		, dataBuffer( KF_BUFFER_CAPACITY )
		, fddFound(false)
		, lastCalibratedCylinder(0) {
		informedOnPoorPrecompensation=false;
		// - connecting to a local KryoFlux device
		hDevice=INVALID_HANDLE_VALUE;
		switch (driver){
			case TDriver::WINUSB:
				winusb.Clear();
				break;
			default:
				ASSERT(FALSE);
				break;
		}
		Connect();
		DestroyAllTracks(); // because Connect scans zeroth Track
	}

	CKryoFluxDevice::~CKryoFluxDevice(){
		// dtor
		EXCLUSIVELY_LOCK_THIS_IMAGE(); // mustn't destroy this instance while it's being used!
		Disconnect();
		DestroyAllTracks(); // see Reset()
	}








	bool CKryoFluxDevice::Connect(){
		// True <=> successfully connected to a local KryoFlux device, otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		EXCLUSIVELY_LOCK_DEVICE();
		// - connecting to the device
		ASSERT( hDevice==INVALID_HANDLE_VALUE );
		hDevice=::CreateFile(
					SetupDi::GetDevicePath( GUID_DEVINTERFACE_KRYOFLUX ),
					GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_WRITE | FILE_SHARE_READ,
					nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr
				);
		if (hDevice==INVALID_HANDLE_VALUE)
			return false;
		// - setting transfer properties
		switch (driver){
			case TDriver::WINUSB:
				if (winusb.ConnectToInterface( hDevice, KF_INTERFACE-1 )){
					winusb.SetPipePolicy( KF_EP_BULK_IN, true, 1500 );
					winusb.SetPipePolicy( KF_EP_BULK_OUT, true, 1500 );
				}
				break;
			default:
				ASSERT(FALSE);
				return false;
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
		EXCLUSIVELY_LOCK_DEVICE();
		fddFound=false;
		switch (driver){
			case TDriver::WINUSB:
				winusb.DisconnectFromInterface();
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
			for( PBYTE p=dataBuffer; const int nBytesFree=dataBuffer+KF_BUFFER_CAPACITY-p; )
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
		switch (driver){
			case TDriver::WINUSB:
				return	winusb.GetProductName( device.lastRequestResultMsg, sizeof(device.lastRequestResultMsg) );
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
			::lstrcpyA( device.firmwareVersion, "Not loaded" );
			// . opening the firmware file for reading
			if (paramsEtc.firmwareFileName.IsEmpty()) // catching an empty string as it may succeed as filename on Win10!
				return ERROR_FILE_NOT_FOUND;
			CFileException e;
			CFile f;
			if (!f.Open( paramsEtc.firmwareFileName, CFile::modeRead|CFile::shareDenyWrite|CFile::typeBinary, &e ))
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
			const auto p=Utils::MakeCallocPtr<BYTE>(fLength);
			if (ReadFull(p,fLength)!=ERROR_SUCCESS || ::memcmp(dataBuffer,p,fLength)!=0) // uloaded wrongly?
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
		::lstrcpyA( device.firmwareVersion, KF_DEVICE_NAME_ANSI );
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
					sizeof(device.lastRequestResultMsg)
				};
				return	WinUsb::Lib::ControlTransfer(
							winusb.hLibrary,
							sp, (PUCHAR)device.lastRequestResultMsg, sizeof(device.lastRequestResultMsg),
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
				return	WinUsb::Lib::ReadPipe(
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
				return	WinUsb::Lib::WritePipe(
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
		static constexpr WORD UNIQUE_FLUXES_COUNT_MAX=10000;
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
				#ifdef _DEBUG
					tr.ShowModal( _T("Tachyon flux at %d"), currTime );
					return 0;
				#endif
				continue;
			}
			totalSampleCounter+=sampleCounter;
			if (sampleCounter>0xffff)
				continue; // long fluxes below replaced with sequence of quick fluxes to indicate non-formatted area
			histogram.Add(sampleCounter);
		}
		// - writing Signature
		static constexpr BYTE Signature[]={ 'K', 'F', 'W', '\x1' };
		pb=(PBYTE)::memcpy( pb, Signature, sizeof(Signature) )+sizeof(Signature);
		// - writing header
		struct TUniqueFlux sealed{
			BYTE three;
			BYTE index;
			Utils::CBigEndianWord sampleCounter;
		};
		static_assert( sizeof(TUniqueFlux)==sizeof(DWORD), "Incorrect size" );
		static constexpr BYTE Data1[]={ 0xF4, 0x01, 0x00, 0x00, 0x88, 0x13, 0x00, 0x00 }; // TODO: find out the meaning
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
		static constexpr BYTE FluxTablePostamble[]={ 0x0B, 0x05, 0x09, 0x00, 0x01, 0x05, 0x07, 0x0A, 0x05, 0x06, 0x01 }; // TODO: find out the meaning
		pb=(PBYTE)::memcpy( pb, FluxTablePostamble, sizeof(FluxTablePostamble) )+sizeof(FluxTablePostamble);
		const int nHeaderBytes=(pb-dataBuffer+63)/64*64; // rounding header to whole multiples of 64 Bytes
		::ZeroMemory(pb,64);
		pb=dataBuffer+nHeaderBytes;
		// - converting UniqueFluxesUsed to an auxiliary Track (with LogicalTime set to SampleCounter) so that nearest neighbors can be used to approximate fluxes excluded from the Histogram
		CTrackReaderWriter trwFluxes( nUniqueFluxesUsed, CTrackReader::TDecoderMethod::NONE, false );
		for( BYTE i=0; i<nUniqueFluxesUsed; i++ )
			trwFluxes.AddTime( histogram[i] );
		std::sort( trwFluxes.GetBuffer(), trwFluxes.GetBuffer()+nUniqueFluxesUsed );
		// - writing fluxes
		const PBYTE fluxesStart=pb;
		static constexpr BYTE FluxesPreamble[]={ 0x00, 0x12, 0x00, 0x00 };
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
		const int nContentBytes=(pb-dataBuffer+63)/64*64; // rounding to whole multiples of 64 Bytes
		::ZeroMemory( pb, 64 );
		pb=dataBuffer+nContentBytes;
		// - successfully processed
		::SetLastError(ERROR_SUCCESS);
		return pb-dataBuffer;
	}

	#define ERROR_SAVE_MESSAGE_TEMPLATE	_T("Track %02d.%c %s failed")

	TStdWinError CKryoFluxDevice::SaveAndVerifyTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const{
		// saves the specified Track to the inserted Medium; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - Track must already exist from before
		const PInternalTrack &pit=internalTracks[cyl][head]; // the Track may be changed during verification, so don't remove the reference!
		if (!pit)
			return ERROR_SUCCESS;
		if (!pit->modified)
			return ERROR_SUCCESS;
		//CTrackReader::CBitSequence( *pit, 0, pit->CreateResetProfile(), pit->GetIndexTime(1) ).SaveCsv("r:\\format.txt");
		pit->FlushSectorBuffers(); // convert all modifications into flux transitions
		// - extracting the "best" Revolution into a temporary Track
		CTrackReaderWriter trw( pit->GetTimesCount(), CTrackReader::TDecoderMethod::FDD_KEIR_FRASER, false );
		trw.AddIndexTime(0);
			// . finding the Revolution with the most of healthy Sectors
			struct{
				BYTE i;
				TSector nHealthySectors;
				bool hasDataOverIndex;
			} bestRev={};
			for( BYTE i=1; i<pit->GetIndexCount(); i++ ){
				const BYTE r=i-1;
				TSector nHealthySectors=0; bool hasDataOverIndex=false; // assumptions
				for( TSector s=0; s<pit->nSectors; s++ ){
					pit->ReadSector( pit->sectors[s], r );
					const auto &rev=pit->sectors[s].revolutions[r];
					if (pit->GetIndexTime(i)<rev.dataEndTime){ // data over index?
						hasDataOverIndex=true;
						if (i+1==pit->GetIndexCount()){
							nHealthySectors=0; // can't use the last Revolution if it has data over index
							break;
						}
					}
					nHealthySectors +=	rev.fdcStatus==TFdcStatus::WithoutError // healthy data
										||
										rev.fdcStatus==TFdcStatus::DeletedDam; // Deleted but still healthy data
				}
				if (bestRev.nHealthySectors<nHealthySectors) // better Revolution found?
					bestRev.i=r, bestRev.nHealthySectors=nHealthySectors, bestRev.hasDataOverIndex=hasDataOverIndex;
				if (bestRev.nHealthySectors==pit->nSectors) // best possible Revolution found?
					break;
			}
			// . extracting the minimum number of fluxes into the temporary Track
			const TLogTime tIndex0=pit->RewindToIndex(bestRev.i), tIndex1=pit->GetIndexTime(bestRev.i+1);
			TLogTime tWritingEnd=tIndex1;
			if (bestRev.hasDataOverIndex){
				const auto &firstSector=pit->sectors[0];
				TLogTime tOverhang=INT_MAX;
				for( BYTE r=0; r<firstSector.nRevolutions; r++ ){
					TLogTime tIdEnd=firstSector.revolutions[r].idEndTime;
					if (tIdEnd>0){ // Sector found in the Revolution?
						tIdEnd-=pit->GetIndexTime(r); // relative to the Revolution beginning
						if (tIdEnd<tOverhang)
							tOverhang=tIdEnd;
					}
				}
				tWritingEnd+= tOverhang - 500*pit->GetCurrentProfile().iwTimeMax; // "-N" = 12x sync 0x00, 3x distorted 0xA1, 1x mark Byte, 4x ID Bytes, 2x CRC Bytes, making 22 Bytes in total, or 176 data bits, or 352 cells on the disk, allowing for some reserve
			}
			while (*pit && pit->GetCurrentTime()<tWritingEnd)
				trw.AddTime( pit->ReadTime()-tIndex0 );
		trw.AddIndexTime( tIndex1-tIndex0 );
		if (floppyType!=Medium::UNKNOWN){
			trw.SetMediumType( floppyType );
			trw.Normalize();
		}
		// - pre-compensation of the temporary Track
		const CTrackReaderWriter trwCompensated( trw, false );
		TStdWinError err;
		if (precompensation.methodVersion!=CPrecompensation::None)
			if ( err=precompensation.ApplyTo(*this,cyl,head,trwCompensated) )
				return err;
		// - Drive's head calibration
		EXCLUSIVELY_LOCK_DEVICE();
		if (params.calibrationStepDuringFormatting)
			if (std::abs(cyl-lastCalibratedCylinder)>>(BYTE)params.doubleTrackStep>=params.calibrationStepDuringFormatting){
				lastCalibratedCylinder=cyl;
				SeekHome();
			}
		// - writing (and optional verification)
		char nSilentRetrials=4;
		do{
			if (cancelled)
				return ERROR_CANCELLED;
			// . consuming one SilentRetrial
			nSilentRetrials--;
			// . converting the temporary Track to "KFW" data, below streamed directly to KryoFlux
			const DWORD nBytesToWrite=TrackToKfw1( trwCompensated );
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
			winusb.ClearIoPipes( KF_EP_BULK_IN, KF_EP_BULK_OUT );
			// . streaming the "KFW" data to KryoFlux
			SendRequest( TRequest::INDEX_WRITE, 2 ); // waiting for an index?
			if (!SetMotorOn() || !SelectHead(head) || !SeekTo(cyl)) // some Drives require motor to be on before seeking Heads
				return ERROR_NOT_READY;
			SendRequest( TRequest::STREAM, 2 ); // start streaming
				err=WriteFull( dataBuffer, nBytesToWrite );
				if (err==ERROR_SUCCESS)
					do{
						if (err=SendRequest( TRequest::RESULT_WRITE ))
							break;
					}while (::strrchr(device.lastRequestResultMsg,'=')[1]=='9'); // TODO: explain why sometimes instead of '0' a return code is '3' but the Track has been written; is it a timeout? if yes, how to solve it?
			SendRequest( TRequest::STREAM, 0 ); // stop streaming
			TCHAR msgSavingFailed[80];
			::wsprintf( msgSavingFailed, ERROR_SAVE_MESSAGE_TEMPLATE, cyl, '0'+head, _T("saving") );
			if (err) // writing to the device failed
				switch (
					nSilentRetrials>0 || cancelled
					? IDRETRY
					: Utils::AbortRetryIgnore(msgSavingFailed,err,MB_DEFBUTTON2)
				){
					case IDIGNORE:	// ignoring the Error
						err=ERROR_CONTINUE;
						break;
					case IDABORT:	// aborting the saving
						return err;
					default:		// attempting to save the Track once more (or returning ERROR_CANCELLED if operation externally cancelled)
						continue;
				}
			// . writing verification
			if (!err && params.verifyWrittenTracks && pit->nSectors>0){ // can verify the Track only if A&B&C, A = writing successfull, B&C = at least one Sector is recognized in it
				const Utils::CVarTempReset<TParams::TCalibrationAfterError> cae0( params.calibrationAfterError, TParams::TCalibrationAfterError::NONE ); // already calibrated before writing
				const Utils::CVarTempReset<TParams::TPrecision> p0( params.precision, TParams::TPrecision::SINGLE );
				std::unique_ptr<CTrackReaderWriter> pVerifiedTrack;
				switch (err=VerifyTrack( cyl, head, trw, nSilentRetrials<0, &pVerifiedTrack, cancelled )){
					case ERROR_CONTINUE:// validation failed but ignore the failure and continue
						delete internalTracks[cyl][head];
						internalTracks[cyl][head]=CInternalTrack::CreateFrom( *this, *pVerifiedTrack );
						//fallthrough
					case ERROR_SUCCESS:	// validation was successfull
						break;
					case ERROR_RETRY:	// attempting to save the Track once more
					case ERROR_DS_COMPARE_FALSE: // silent automatic retrial
						continue;
					default:	// another error during verification, including Cancellation of saving
						return err;
				}
			}
		}while (err!=ERROR_SUCCESS && err!=ERROR_CONTINUE);
		// - (successfully) saved - see TODOs
		pit->modified=false;
		return err;
	}

	TStdWinError CKryoFluxDevice::SaveTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const{
		// saves the specified Track to the inserted Medium; returns Windows standard i/o error
		switch (const TStdWinError err=SaveAndVerifyTrack(cyl,head,cancelled)){
			case ERROR_SUCCESS:
			case ERROR_CONTINUE: // writing errors ignored
				return ERROR_SUCCESS;
			default:
				return err;
		}
	}

	TSector CKryoFluxDevice::ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec,PSectorId bufferId,PWORD bufferLength,PLogTime startTimesNanoseconds,PBYTE pAvgGap3) const{
		// returns the number of Sectors found in given Track, and eventually populates the Buffer with their IDs (if Buffer!=Null); returns 0 if Track not formatted or not found
	{	EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return 0;
		// - if Track already scanned before, returning the result from before
		if (internalTracks[cyl][head]!=nullptr)
			return __super::ScanTrack( cyl, head, pCodec, bufferId, bufferLength, startTimesNanoseconds, pAvgGap3 );
	}
		// - scanning (forced recovery from errors right during scanning)
		const Utils::CCallocPtr<BYTE> tmpDataBuffer(KF_BUFFER_CAPACITY);
		for( char nRecoveryTrials=7; true; nRecoveryTrials-- ){
			// . issuing a Request to the KryoFlux device to read fluxes in the specified Track
			PBYTE p=tmpDataBuffer;
	{		EXCLUSIVELY_LOCK_DEVICE();
			if (!SetMotorOn() || !SelectHead(head) || !SeekTo(cyl)) // some Drives require motor to be on before seeking Heads
				return 0;
			const BYTE nIndicesRequested=std::min<BYTE>( params.PrecisionToFullRevolutionCount()+1, Revolution::MAX ); // N+1 indices = N full revolutions
			SendRequest( TRequest::STREAM, MAKEWORD(1,nIndicesRequested) ); // start streaming
				while (const DWORD nBytesFree=tmpDataBuffer+KF_BUFFER_CAPACITY-p)
					if (const auto n=Read( p, nBytesFree )){
						p+=n;
						if (p-tmpDataBuffer>7
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
	}
			// . making sure the read content is a KryoFlux Stream whose data actually make sense
			PInternalTrack &rit=internalTracks[cyl][head];
			if (CTrackReaderWriter trw=StreamToTrack( tmpDataBuffer, p-tmpDataBuffer )){
				// it's a KryoFlux Stream whose data make sense
				if (floppyType!=Medium::UNKNOWN){ // may be unknown if Medium is still being recognized
					trw.SetMediumType(floppyType);
					if (dos!=nullptr) // DOS already known
						params.corrections.ApplyTo(trw);
					//the following commented out as it brings little to no readability improvement and leaves Tracks influenced by the MediumType
					//else if (params.corrections.indexTiming) // DOS still being recognized ...
						//trw.Normalize(); // ... hence can only improve readability by adjusting index-to-index timing
				}
				const PInternalTrack pit=CInternalTrack::CreateFrom( *this, trw ); // this is time-consuming, so it's out of locked section
				EXCLUSIVELY_LOCK_THIS_IMAGE();
				ASSERT(!rit);
				if (rit) // deleting whatever Track that emerged between the Image and Device locks
					delete rit;
				rit=pit;
				__super::ScanTrack( cyl, head, pCodec, bufferId, bufferLength, startTimesNanoseconds, pAvgGap3 );
			}
			// . if no more trials left, we are done
			if (nRecoveryTrials<=0)
				break;
			// . attempting to return good data
			EXCLUSIVELY_LOCK_THIS_IMAGE(); // !!! see also below this->{Lock,Unlock}
			if (rit){ // may be Null if, e.g., device manually reset, disconnected, etc.
				if (GetCountOfHealthySectors(cyl,head)>0 || !rit->nSectors // Track at least partly healthy or without known Sectors
					||
					params.calibrationAfterError==TParams::TCalibrationAfterError::NONE // calibration disabled
				)
					return rit->nSectors;
				if (params.calibrationAfterErrorOnlyForKnownSectors && dos && dos->properties!=&CUnknownDos::Properties){
					bool knownSectorBad=false; // assumption (the Track is unhealthy due to an irrelevant Unknown Sector, e.g. out of geometry)
					for( TSector s=0; s<rit->nSectors; s++ ){
						const TInternalSector &is=rit->sectors[s];
						const TPhysicalAddress chs={ cyl, head, is.id };
						if (dos->GetSectorStatus(chs)==CDos::TSectorStatus::UNKNOWN)
							continue; // ignore Unknown Sector
						WORD w; TFdcStatus st=TFdcStatus::WithoutError;
						const_cast<CKryoFluxDevice *>(this)->GetSectorData( chs, s, Revolution::ANY_GOOD, &w, &st );
						if ( knownSectorBad=!st.IsWithoutError() )
							break;
					}
					if (!knownSectorBad)
						return rit->nSectors;
				}
				switch (params.calibrationAfterError){
					case TParams::TCalibrationAfterError::ONCE_PER_CYLINDER:
						// calibrating only once for the whole Cylinder
						if (lastCalibratedCylinder==cyl) // already calibrated?
							break;
						nRecoveryTrials=0;
						//fallthrough
					case TParams::TCalibrationAfterError::FOR_EACH_SECTOR:
						lastCalibratedCylinder=cyl;
						this->locker.Unlock();
					{		EXCLUSIVELY_LOCK_DEVICE();
							SeekHome();
					}	this->locker.Lock();
						break;
				}				
				delete rit; // disposing the erroneous Track ...
				rit=nullptr; // ... and attempting to obtain its data after head has been calibrated
			}
		}
		// - returning whatever has been read
		EXCLUSIVELY_LOCK_THIS_IMAGE();
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
		// - TODO: the following regards writing to disk and needs to be explained
		EXCLUSIVELY_LOCK_DEVICE();
		do{
			if (!SetMotorOn())
				return ERROR_DRIVE_NOT_INSTALLED;
			if (const TStdWinError err=SendRequest( TRequest::INDEX_WRITE, 8 ))
				return err;
		}while (::strrchr(device.lastRequestResultMsg,'=')[1]!='8');
		// - resetting the KryoFlux device
		return ERROR_SUCCESS;
	}

	BOOL CCapsBase::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		if (nCode==CN_COMMAND) // a command
			if (nID==ID_IMAGE_PROTECT){ // toggling WriteProtection
				__super::OnCmdMsg( nID, nCode, pExtra, pHandlerInfo ); // base
				if (!IsWriteProtected())
					// write-protection turned off - informing on poorly determined (or none) pre-compensation
					if (!informedOnPoorPrecompensation)
						if (const TStdWinError err=precompensation.DetermineUsingLatestMethod(*this,0)){
							static constexpr TCHAR Msg[]=_T("WARNING: Writing likely erroneous");
							switch (err){
								case ERROR_INVALID_DATA:
									Utils::Information( Msg, _T("Precompensation not yet determined for this drive/disk") );
									break;
								case ERROR_EVT_VERSION_TOO_OLD:
									Utils::Information( Msg, _T("Precompensation outdated for this drive/disk") );
									break;
								default:
									Utils::Information( Msg, err );
									break;
							}
							informedOnPoorPrecompensation=true;
						}
				return TRUE;
			}
		return __super::OnCmdMsg( nID, nCode, pExtra, pHandlerInfo ); // base
	}

	void CKryoFluxDevice::SetPathName(LPCTSTR lpszPathName,BOOL bAddToMRU){
		__super::SetPathName( lpszPathName, bAddToMRU );
		m_strPathName=lpszPathName;
	}
