#ifndef IPF_H
#define IPF_H

	class CIpf sealed:public CCapsBase{
	public:
		static const TProperties Properties;

		CIpf();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		//TCylinder GetCylinderCount() const override;
		//THead GetNumberOfFormattedSides(TCylinder cyl) const override;
		BYTE GetAvailableRevolutionCount() const override;
		//TSector ScanTrack(TCylinder cyl,THead head,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PINT startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		//void GetTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override;
		TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus) override;
		//TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		//void EditSettings() override;
		//TStdWinError Reset() override;
		TStdWinError FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte) override;
		TStdWinError UnformatTrack(TCylinder cyl,THead head) override;
	};

#endif // IPF_H
