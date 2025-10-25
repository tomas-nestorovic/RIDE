#ifndef KRYOFLUXSTREAMS_H
#define KRYOFLUXSTREAMS_H

	class CKryoFluxStreams sealed:public CKryoFluxBase{
		CString nameBase;

		TStdWinError SaveAllModifiedTracks(LPCTSTR lpszPathName,CActionProgress &ap) override;
		CString GetStreamFileName(LPCTSTR nameBase,TCylinder cyl,THead head) const;
		inline CString GetStreamFileName(TCylinder cyl,THead head) const{ return GetStreamFileName(nameBase,cyl,head); }
		bool SetNameBase(LPCTSTR fullName);
	public:
		static const TProperties Properties;

		CKryoFluxStreams();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		//TCylinder GetCylinderCount() const override;
		//THead GetNumberOfFormattedSides(TCylinder cyl) const override;
		//TSector ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec=nullptr,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		//void GetTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override;
		//TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus) override;
		//TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		///void EditSettings() override;
		//TStdWinError Reset() override;
		const CTrackReader &ReadTrack(TCylinder cyl,THead head) const override;
		TStdWinError SaveTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const override;
		//TStdWinError FormatTrack(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte) override;
		//TStdWinError UnformatTrack(TCylinder cyl,THead head) override;
		void SetPathName(LPCTSTR lpszPathName,BOOL bAddToMRU=TRUE) override;
	};

#endif // KRYOFLUXSTREAMS_H
