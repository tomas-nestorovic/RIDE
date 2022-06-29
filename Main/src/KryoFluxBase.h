#ifndef KRYOFLUXBASE_H
#define KRYOFLUXBASE_H

	#define KF_BUFFER_CAPACITY		1000000

	class CKryoFluxBase abstract:public CCapsBase{
	protected:
		struct TParamsEtc{
			// persistent (saved and loaded)
			CString firmwareFileName;
			// volatile (current session only)
			//none

			TParamsEtc();
			~TParamsEtc();
		} paramsEtc;

		const LPCTSTR firmware;

		CKryoFluxBase(PCProperties properties,char realDriveLetter,LPCTSTR firmware);

		virtual TStdWinError UploadFirmware()=0;
		CTrackReaderWriter StreamToTrack(LPBYTE inStreamBytes,DWORD nStreamBytes) const;
		DWORD TrackToStream(CTrackReader tr,LPBYTE outBuffer) const;
	public:
		static DWORD TimeToStdSampleCounter(TLogTime t);

		//BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		//BOOL OnSaveDocument(LPCTSTR lpszPathName) override;
		//TCylinder GetCylinderCount() const override;
		//THead GetNumberOfFormattedSides(TCylinder cyl) const override;
		//BYTE GetAvailableRevolutionCount(TCylinder cyl,THead head) const override;
		//TSector ScanTrack(TCylinder cyl,THead head,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		//void GetTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override;
		TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus) override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		//TStdWinError Reset() override;
		TStdWinError WriteTrack(TCylinder cyl,THead head,CTrackReader tr) override;
		TStdWinError FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte,const volatile bool &cancelled) override;
		bool RequiresFormattedTracksVerification() const override;
		TStdWinError UnformatTrack(TCylinder cyl,THead head) override;
	};

#endif // KRYOFLUXBASE_H
