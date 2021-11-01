#ifndef KRYOFLUXBASE_H
#define KRYOFLUXBASE_H

	#define KF_BUFFER_CAPACITY		1000000

	class CKryoFluxBase abstract:public CCapsBase{
	protected:
		struct TParams{
			// persistent (saved and loaded)
			CString firmwareFileName;
			int precision; // 0 = two full revolutions, 1 = four full revolutions
			enum TFluxDecoder{ // in order of appearance in corresponding combo-box in IDR_KRYOFLUX_ACCESS dialog
				NO_FLUX_DECODER,
				KEIR_FRASER,
				MARK_OGDEN
			} fluxDecoder;
			bool resetFluxDecoderOnIndex;
			enum TCalibrationAfterError{
				NONE				=0,
				ONCE_PER_CYLINDER	=1,
				FOR_EACH_SECTOR		=2
			} mutable calibrationAfterError;
			BYTE calibrationStepDuringFormatting;
			TCorrections corrections;
			bool verifyWrittenTracks;
			// volatile (current session only)
			bool doubleTrackStep, userForcedDoubleTrackStep;

			TParams();
			~TParams();
		} params;

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
		BYTE GetAvailableRevolutionCount() const override;
		//TSector ScanTrack(TCylinder cyl,THead head,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		//void GetTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override;
		TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus) override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		//TStdWinError Reset() override;
		TStdWinError WriteTrack(TCylinder cyl,THead head,CTrackReader tr) override;
		TStdWinError FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte) override;
		TStdWinError UnformatTrack(TCylinder cyl,THead head) override;
	};

#endif // KRYOFLUXBASE_H
