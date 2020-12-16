#ifndef KRYOFLUXSTREAMS_H
#define KRYOFLUXSTREAMS_H

	class CKryoFluxStreams sealed:public CKryoFluxBase{
		TCHAR nameBase[MAX_PATH];

		TStdWinError UploadFirmware() override;
	public:
		static const TProperties Properties;

		CKryoFluxStreams();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		BOOL OnSaveDocument(LPCTSTR lpszPathName) override;
		//TCylinder GetCylinderCount() const override;
		//THead GetNumberOfFormattedSides(TCylinder cyl) const override;
		TSector ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec=nullptr,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		//void GetTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override;
		//TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus) override;
		//TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		///void EditSettings() override;
		//TStdWinError Reset() override;
		//TStdWinError FormatTrack(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte) override;
		//TStdWinError UnformatTrack(TCylinder cyl,THead head) override;
	};

#endif // KRYOFLUXSTREAMS_H
