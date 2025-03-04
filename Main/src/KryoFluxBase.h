#ifndef KRYOFLUXBASE_H
#define KRYOFLUXBASE_H

	#define KF_BUFFER_CAPACITY		2000000

	#define KF_STREAM_ID			MAKE_IMAGE_ID('K','r','y','o','F','l','u','x')

	class CKryoFluxBase abstract:public CCapsBase{
	protected:
		static int WriteCreatorOob(PBYTE streamBuffer);

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
		//TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus) override;
		//TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
	};

#endif // KRYOFLUXBASE_H
