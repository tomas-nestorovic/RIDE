#ifndef IMAGERAW_H
#define IMAGERAW_H
	
	class CImageRaw:public CImage{
		TCylinder nCylinders;
		PVOID *bufferOfCylinders;
		THead nHeads;
		TSector nSectors,firstSectorNumber;
		BYTE sectorLengthCode;	WORD sectorLength;

		PSectorData __getBufferedSectorData__(TCylinder cyl,THead head,PCSectorId sectorId) const;
		void __saveTrackToCurrentPositionInFile__(CFile *pfOtherThanCurrentFile,TPhysicalAddress chs);
	protected:
		TTrackScheme trackAccessScheme;
		PCSide sideMap;
		DWORD sizeWithoutGeometry;
		CFile f;

		bool __openImageForReadingAndWriting__(LPCTSTR fileName);
		TStdWinError __setMediumTypeAndGeometry__(PCFormat pFormat,PCSide _sideMap,TSector _firstSectorNumber);
		TStdWinError __extendToNumberOfCylinders__(TCylinder nCyl,BYTE fillerByte);
		void __freeCylinder__(TCylinder cyl);
		void __freeBufferOfCylinders__();
		TStdWinError SaveAllModifiedTracks(LPCTSTR lpszPathName,PBackgroundActionCancelable pAction) override;
	public:
		static const TProperties Properties;

		CImageRaw(PCProperties properties,bool hasEditableSettings);
		~CImageRaw();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		TCylinder GetCylinderCount() const override;
		THead GetHeadCount() const override;
		TSector ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec=nullptr,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		void GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override;
		TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE,PCFdcStatus pFdcStatus) override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		TStdWinError Reset() override;
		TStdWinError FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector _nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte) override;
		TStdWinError UnformatTrack(TCylinder cyl,THead) override;
		std::unique_ptr<CSectorDataSerializer> CreateSectorDataSerializer(CHexaEditor *pParentHexaEditor) override;
	};

#endif // IMAGERAW_H
