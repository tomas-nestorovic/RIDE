#ifndef IMAGEFLOPPY_H
#define IMAGEFLOPPY_H

	class CFloppyImage:public CImage{
		struct TScannerState{
			static const TScannerState Initial;

			CSectorDataSerializer::TScannerStatus scannerStatus;
			BYTE n;
			bool allScanned;
			#if _MFC_VER>=0x0A00
			LONGLONG dataTotalLength;
			#else
			LONG dataTotalLength;
			#endif
			BYTE nDiscoveredRevolutions;
		};

		struct TScannedTracks sealed:public TScannerState{
			CCriticalSection locker;

			TScannedTracks();
		} scannedTracks;
	public:
		typedef WORD TCrc16;

		static TCrc16 GetCrc16Ccitt(TCrc16 seed,LPCVOID bytes,WORD nBytes);
		static TCrc16 GetCrc16Ccitt(LPCVOID bytes,WORD nBytes);
		static bool IsValidSectorLengthCode(BYTE lengthCode);
	protected:
		Medium::TType floppyType; // DD/HD

		CFloppyImage(PCProperties properties,bool hasEditableSettings);
	public:
		WORD GetUsableSectorLength(BYTE sectorLengthCode) const;
		TFormat::TLengthCode GetMaximumSectorLengthCode() const;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		TStdWinError UnscanTrack(TCylinder cyl,THead head) override;
		CSectorDataSerializer *CreateSectorDataSerializer(CHexaEditor *pParentHexaEditor) override sealed;
		TLogTime EstimateNanosecondsPerOneByte() const override;
		virtual void EstimateTrackTiming(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,BYTE gap3,PLogTime startTimesNanoseconds) const;
	};

#endif // IMAGEFLOPPY_H
