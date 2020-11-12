#ifndef KRYOFLUXBASE_H
#define KRYOFLUXBASE_H

	class CKryoFluxBase abstract:public CCapsBase{
	protected:
		class CKfStream{
			const LPBYTE inStreamData;
			DWORD inStreamDataLength;
			DWORD nFluxes;
			struct TIndexPulse sealed{
				DWORD posInStreamData;
				DWORD sampleCounter;
				DWORD indexCounter;
			} indexPulses[DEVICE_REVOLUTIONS_MAX];
			BYTE nIndexPulses;
			TStdWinError errorState;
			double mck,sck,ick;
		public:
			CKfStream(LPBYTE rawBytes,DWORD nBytes);

			FORCEINLINE
			TStdWinError GetError() const{
				return errorState;
			}

			CTrackReaderWriter ToTrack(const CKryoFluxBase &kfb) const;
		};

		struct TParams{
			// persistent (saved and loaded)
			enum TFluxDecoder{
				KEIR_FRASIER,
				KEIR_FRASIER_MODIFIED
			} fluxDecoder;
			enum TCalibrationAfterError{
				NONE				=0,
				ONCE_PER_CYLINDER	=1,
				FOR_EACH_SECTOR		=2
			} calibrationAfterError;
			BYTE calibrationStepDuringFormatting, nSecondsToTurnMotorOff;
			bool verifyFormattedTracks, verifyWrittenData;
			// volatile (current session only)
			bool doubleTrackStep, userForcedDoubleTrackStep;

			TParams();
			~TParams();
		} params;

		const LPCTSTR firmware;

		CKryoFluxBase(PCProperties properties,LPCTSTR firmware);

		//BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		//BOOL OnSaveDocument(LPCTSTR lpszPathName) override;
		//TCylinder GetCylinderCount() const override;
		//THead GetNumberOfFormattedSides(TCylinder cyl) const override;
		//TSector ScanTrack(TCylinder cyl,THead head,PSectorId bufferId=nullptr,PWORD bufferLength=nullptr,PLogTime startTimesNanoseconds=nullptr,PBYTE pAvgGap3=nullptr) const override;
		//void GetTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses) override;
		TStdWinError MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus) override;
		//TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		//TStdWinError Reset() override;
		TStdWinError FormatTrack(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte) override;
		TStdWinError UnformatTrack(TCylinder cyl,THead head) override;
	};

#endif // KRYOFLUXBASE_H
