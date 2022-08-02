#ifndef KRYOFLUXBASE_H
#define KRYOFLUXBASE_H

	#define KF_BUFFER_CAPACITY		1000000

	class CKryoFluxBase abstract:public CCapsBase{
	protected:
		struct TParamsEtc{
			// persistent (saved and loaded)
			CString firmwareFileName;
			// volatile (current session only)
			//none

			TParamsEtc();
			~TParamsEtc();
		} paramsEtc;

		const LPCTSTR firmware;

		CKryoFluxBase(PCProperties properties,char realDriveLetter,LPCTSTR firmware);

		CTrackReaderWriter StreamToTrack(LPBYTE inStreamBytes,DWORD nStreamBytes) const;
		DWORD TrackToStream(CTrackReader tr,LPBYTE outBuffer) const;
	public:
		static DWORD TimeToStdSampleCounter(TLogTime t);

		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		TStdWinError WriteTrack(TCylinder cyl,THead head,CTrackReader tr) override;
		TStdWinError FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte,const volatile bool &cancelled) override;
		TStdWinError UnformatTrack(TCylinder cyl,THead head) override;
	};

#endif // KRYOFLUXBASE_H
