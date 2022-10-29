#ifndef GREASEWEAZLE_H
#define GREASEWEAZLE_H

	class CGreaseweazleV4 sealed:public CCapsBase{
		enum TRequest:BYTE{
			GET_INFO		=0,
			UPDATE			=1,
			SEEK_ABS		=2,
			HEAD			=3,
			SET_PARAMS		=4,
			GET_PARAMS		=5,
			MOTOR			=6,
			READ_FLUX		=7,
			WRITE_FLUX		=8,
			GET_FLUX_STATUS	=9,
			GET_INDEX_TIMES	=10,
			SWITCH_FW_MODE	=11,
			SELECT_DRIVE	=12,
			DESELECT_DRIVE	=13,
			SET_BUS_TYPE	=14,
			SET_PIN			=15,
			SOFT_RESET		=16,
			ERASE_FLUX		=17,
			SOURCE_BYTES	=18,
			SINK_BYTES		=19,
			GET_PIN			=20,
			TEST_MODE		=21,
			NO_CLICK_STEP	=22
		};

		enum TResponse:BYTE{
			OKAY			=0,
			BAD_COMMAND		=1,
			NO_INDEX		=2,
			NO_TRK0			=3,
			FLUX_OVERFLOW	=4,
			FLUX_UNDERFLOW	=5,
			WRPROT			=6,
			NO_UNIT			=7,
			NO_BUS			=8,
			BAD_UNIT		=9,
			BAD_PIN			=10,
			BAD_CYLINDER	=11,
			OUT_OF_SRAM		=12,
			OUT_OF_FLASH	=13
		};

		struct TDriveInfo sealed{
			DWORD cylSeekedValid:1;
			DWORD motorOn:1;
			DWORD isFlippy:1;
			union{
				TCylinder cylSeeked;
				int reserved1;
			};
			BYTE reserved2[24];
		};

		const enum TDriver{
			UNSUPPORTED,
			USBSER
		} driver;
		const BYTE fddId;
		const Utils::CCallocPtr<BYTE> dataBuffer;
		#pragma pack(1);
		struct{
			BYTE major, minor, isMainFirmware, maxCmd;
			DWORD sampleFrequency;
			BYTE hardwareModel, hardwareSubmodel, usbSpeed, mcuId;
			short mcuMhz, mcuRamKb;
			BYTE reserved[16];
		} firmwareInfo;
		struct{
			mutable CMutex locker;
			HANDLE handle;
			union{
				// Driver-related params here
			};
		} device;
		Utils::TRationalNumber sampleClock;
		
		static LPCTSTR Recognize(PTCHAR deviceNameList);
		static PImage Instantiate(LPCTSTR deviceName);

		CGreaseweazleV4(TDriver driver,BYTE fddId);

		operator bool() const;

		TStdWinError Connect();
		void Disconnect();
		DWORD Read(PVOID buffer,DWORD nBytesFree) const;
		TStdWinError ReadFull(PVOID buffer,DWORD nBytes) const;
		DWORD Write(LPCVOID buffer,DWORD nBytes) const;
		TStdWinError WriteFull(LPCVOID buffer,DWORD nBytes) const;
		TStdWinError SendRequest(TRequest req,LPCVOID params,BYTE paramsLength) const;
		CTrackReaderWriter GwV4StreamToTrack(PCBYTE p,DWORD length) const;
		DWORD TrackToGwV4Stream(CTrackReader tr,PBYTE pOutStream) const;
		TStdWinError UploadTrack(TCylinder cyl,THead head,CTrackReader tr) const override;
		inline TStdWinError GetLastFluxOperationError() const{ return SendRequest( TRequest::GET_FLUX_STATUS, nullptr, 0 ); }
		inline TStdWinError SelectDrive() const{ return SendRequest( TRequest::SELECT_DRIVE, &fddId, sizeof(BYTE) ); }
		TStdWinError GetDriveInfo(TDriveInfo &rOutDi) const;
		TStdWinError GetPin(BYTE pin,bool &outValue) const;
		TStdWinError SetMotorOn(bool on=true) const;
		TStdWinError SeekTo(TCylinder cyl) const;
		TStdWinError SelectHead(THead head) const;
	public:
		static const TProperties Properties;

		~CGreaseweazleV4();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		TStdWinError SeekHeadsHome() const override;
		bool EditSettings(bool initialEditing) override;
		TStdWinError Reset() override;
		CTrackReader ReadTrack(TCylinder cyl,THead head) const override;
		void SetPathName(LPCTSTR lpszPathName,BOOL bAddToMRU=TRUE) override;
	};

#endif // GREASEWEAZLE_H
