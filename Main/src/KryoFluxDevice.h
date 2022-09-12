#ifndef KRYOFLUXDEVICE_H
#define KRYOFLUXDEVICE_H

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


	class CKryoFluxDevice sealed:public CKryoFluxBase{
		enum TRequest{
			STATUS = 0x00,				// status
			INFO = 0x01,				// info (index 1 or 2)
			RESULT = 0x02,
			DATA = 0x03,
			INDEX = 0x04,				// index positions from last read?
			RESET = 0x05,				// soft reset
			DEVICE = 0x06,				// select device
			MOTOR = 0x07,				// motor state
			DENSITY = 0x08,				// select density; indication that 5.25" DD floppy inserted in 360 RPM HD drive; don't set - the app handles this case its way!
			SIDE = 0x09,				// select side
			TRACK = 0x0a,				// seek
			STREAM = 0x0b,				// stream on/off, MSB=revs
			MIN_TRACK = 0x0c,			// set min track (default=0)
			MAX_TRACK = 0x0d,			// set max track (default=81)
			RESULT_WRITE = 0x82,
			INDEX_WRITE = 0x84,

			T_SET_LINE = 0x0e,			// (default=4)
			T_DENSITY_SELECT = 0x0f,	// (default=500000)
			T_DRIVE_SELECT = 0x10,		// (default=60000)
			T_SIDE_SELECT = 0x11,		// (default=1000)
			T_DIRECTION_SELECT = 0x12,	// (default=12)
			T_SPIN_UP = 0x13,			// (default=800000)
			T_STEP_AFTER_MOTOR = 0x14,	// (default=200000)
			T_STEP_SIGNAL = 0x15,		// (default=4)
			T_STEP = 0x16,				// (default=8000)
			T_TRACK0_SIGNAL = 0x17,		// (default=1000)
			T_DIRECTION_CHANGE = 0x18,	// (default=38000)
			T_HEAD_SETTLING = 0x19,		// (default=40000)
			T_WRITE_GATE_OFF = 0x20,	// (default=1200)
			T_WRITE_GATE_ON = 0x21,		// (default=8)
			T_BYPASS_OFF = 0x22,		// (default=8)
			T_BYPASS_ON = 0x23			// (default=8)
		};

		const enum TDriver{
			UNSUPPORTED,
			WINUSB
		} driver;
		const BYTE fddId;
		const Utils::CCallocPtr<BYTE> dataBuffer;
		struct{
			mutable CMutex locker;
			HANDLE handle;
			union{
				struct{
					WINUSB_INTERFACE_HANDLE hLibrary;
					WINUSB_INTERFACE_HANDLE hDeviceInterface;
				} winusb;
			};
			mutable char lastRequestResultMsg[240];
			TCHAR firmwareVersion[100];
		} device;
		bool fddFound;
		mutable TCylinder lastCalibratedCylinder;
		
		static LPCTSTR GetDevicePath(TDriver driver,PTCHAR devicePathBuf);
		static LPCTSTR Recognize(PTCHAR deviceNameList);
		static PImage Instantiate(LPCTSTR deviceName);

		CKryoFluxDevice(TDriver driver,BYTE fddId);

		operator bool() const;

		bool Connect();
		void Disconnect();
		LPCSTR GetProductName() const;
		TStdWinError SamBaCommand(LPCSTR cmd,LPCSTR end) const;
		TStdWinError UploadFirmware() override;
		DWORD TrackToKfw1(CTrackReader tr) const;
		TStdWinError SendRequest(TRequest req,WORD index=0,WORD value=0) const;
	    int GetLastRequestResult() const;
		DWORD Read(PVOID buffer,DWORD nBytesFree) const;
		TStdWinError ReadFull(PVOID buffer,DWORD nBytes) const;
		DWORD Write(LPCVOID buffer,DWORD nBytes) const;
		TStdWinError WriteFull(LPCVOID buffer,DWORD nBytes) const;
		bool SetMotorOn(bool on=true) const;
		bool SeekTo(TCylinder cyl) const;
		bool SeekHome() const;
		bool SelectHead(THead head) const;
		TStdWinError SaveAndVerifyTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const;
	public:
		static const TProperties Properties;

		~CKryoFluxDevice();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		//BOOL OnSaveDocument(LPCTSTR lpszPathName) override;
		//TCylinder GetCylinderCount() const override;
		//THead GetNumberOfFormattedSides(TCylinder cyl) const override;
		TSector ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec=nullptr,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		//void GetTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override;
		//TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus) override;
		//TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		TStdWinError Reset() override;
		TStdWinError SaveTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const override;
		void SetPathName(LPCTSTR lpszPathName,BOOL bAddToMRU=TRUE) override;
	};

#endif // KRYOFLUXDEVICE_H
