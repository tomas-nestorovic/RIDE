#ifndef IMAGERAW_H
#define IMAGERAW_H
	
	class CImageRaw:public CImage{
		TCylinder nCylinders;
		Utils::CCallocPtr<PVOID,TCylinder> bufferOfCylinders;
		THead nHeads;
		TSector nSectors,firstSectorNumber;
		BYTE sectorLengthCode;	WORD sectorLength;

		bool IsKnownSector(TCylinder cyl,THead head,RCSectorId id) const;
		PSectorData __getBufferedSectorData__(TCylinder cyl,THead head,PCSectorId sectorId) const;
		void __saveTrackToCurrentPositionInFile__(CFile *pfOtherThanCurrentFile,TPhysicalAddress chs);
	protected:
		TTrackScheme trackAccessScheme;
		Utils::CCallocPtr<TSide,THead> explicitSides; // non-Null = Side numbers explicitly provided by user
		DWORD sizeWithoutGeometry;
		CFile f;

		bool __openImageForReadingAndWriting__(LPCTSTR fileName);
		TStdWinError __setMediumTypeAndGeometry__(PCFormat pFormat,PCSide _sideMap,TSector _firstSectorNumber);
		TStdWinError ExtendToNumberOfCylinders(TCylinder nCyl,BYTE fillerByte,const volatile bool &cancelled);
		void __freeCylinder__(TCylinder cyl);
		void __freeBufferOfCylinders__();
		TStdWinError SaveAllModifiedTracks(LPCTSTR lpszPathName,PBackgroundActionCancelable pAction) override;
	public:
		static const TProperties Properties;

		CImageRaw(PCProperties properties,bool hasEditableSettings);
		~CImageRaw();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		TCylinder GetCylinderCount() const override sealed;
		THead GetHeadCount() const override sealed;
		TSector ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec=nullptr,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		bool IsTrackScanned(TCylinder cyl,THead head) const override sealed;
		void GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override;
		TDataStatus IsSectorDataReady(TCylinder cyl,THead head,RCSectorId id,BYTE nSectorsToSkip,Revolution::TType rev) const override;
		TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE,PCFdcStatus pFdcStatus) override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		TStdWinError Reset() override;
		TStdWinError FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector _nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte,const volatile bool &cancelled) override;
		TStdWinError UnformatTrack(TCylinder cyl,THead head) override;
		std::unique_ptr<CSectorDataSerializer> CreateSectorDataSerializer(CHexaEditor *pParentHexaEditor) override;
	};

#endif // IMAGERAW_H
