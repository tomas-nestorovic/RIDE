#ifndef GREASEWEAZLE_H
#define GREASEWEAZLE_H

	class CGreaseweazleV4 sealed:public CSuperCardProBase{
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

		const enum TDriver{
			UNSUPPORTED,
			USBSER
		} driver;
		const BYTE fddId;
		const Utils::CCallocPtr<BYTE> dataBuffer;
		struct{
			mutable CMutex locker;
			HANDLE handle;
			union{
				// Driver-related params here
			};
		} device;
		bool fddFound;
		Utils::TRationalNumber sampleClock;
		
		static LPCTSTR Recognize(PTCHAR deviceNameList);
		static PImage Instantiate(LPCTSTR deviceName);

		CGreaseweazleV4(TDriver driver,BYTE fddId);

		operator bool() const;

		bool Connect();
		void Disconnect();
		DWORD Read(PVOID buffer,DWORD nBytesFree) const;
		TStdWinError ReadFull(PVOID buffer,DWORD nBytes) const;
		DWORD Write(LPCVOID buffer,DWORD nBytes) const;
		TStdWinError WriteFull(LPCVOID buffer,DWORD nBytes) const;
		TStdWinError SendRequest(TRequest req,LPCVOID params,BYTE paramsLength) const;
		CTrackReaderWriter GwV4StreamToTrack(PCBYTE p,DWORD length) const;
		bool SetMotorOn(bool on=true) const;
		bool SeekTo(TCylinder cyl) const;
		bool SelectHead(THead head) const;
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
