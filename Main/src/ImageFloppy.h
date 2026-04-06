#ifndef IMAGEFLOPPY_H
#define IMAGEFLOPPY_H

	class CFloppyImage:public CImage{
		struct TScannerState{
			static const TScannerState Initial;

			Sector::CReaderWriter::TScannerStatus scannerStatus;
			TTrack n;
			bool allScanned;
			Yahel::TPosition dataTotalLength;
			TRev nDiscoveredRevolutions;
		};

		struct TScannedTracks sealed:public TScannerState{
			CCriticalSection locker;

			TScannedTracks();
		} scannedTracks;
	public:
		static bool IsValidSectorLengthCode(Sector::LC lengthCode);
	protected:
		Medium::TType floppyType; // DD/HD

		CFloppyImage(PCProperties properties,bool hasEditableSettings);
	public:
		Sector::L GetUsableSectorLength(Sector::LC sectorLengthCode) const;
		TFormat::TLengthCode GetMaximumSectorLengthCode() const;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		TStdWinError UnscanTrack(TCylinder cyl,THead head) override;
		Sector::CReaderWriter::CComPtr CreateDiskSerializer(CHexaEditor *pParentHexaEditor) override sealed;
		TLogTime EstimateNanosecondsPerOneByte() const override;
		virtual void EstimateTrackTiming(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,BYTE gap3,PLogTime startTimesNanoseconds) const;
	};

#endif // IMAGEFLOPPY_H
