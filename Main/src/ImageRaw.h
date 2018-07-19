#ifndef IMAGERAW_H
#define IMAGERAW_H
	
	class CImageRaw:public CImage{
		TCylinder nCylinders;
		PVOID *bufferOfCylinders;
		THead nHeads;
		TSector nSectors,firstSectorNumber;
		BYTE sectorLengthCode;	WORD sectorLength;

		PSectorData __getBufferedSectorData__(RCPhysicalAddress chs) const;
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
	public:
		static const TProperties Properties;

		CImageRaw(PCProperties properties);
		~CImageRaw();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		BOOL OnSaveDocument(LPCTSTR lpszPathName) override;
		TCylinder GetCylinderCount() const override;
		THead GetNumberOfFormattedSides(TCylinder cyl) const override;
		TSector ScanTrack(TCylinder cyl,THead head,PSectorId bufferId,PWORD bufferLength) const override;
		PSectorData GetSectorData(RCPhysicalAddress chs,BYTE,bool,PWORD pSectorLength,TFdcStatus *pFdcStatus) override;
		TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE,PCFdcStatus pFdcStatus) override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		TStdWinError Reset() override;
		TStdWinError FormatTrack(TCylinder cyl,THead head,TSector _nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte) override;
		TStdWinError UnformatTrack(TCylinder cyl,THead) override;
	};

#endif // IMAGERAW_H
