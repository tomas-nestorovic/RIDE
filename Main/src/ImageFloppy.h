#ifndef IMAGEFLOPPY_H
#define IMAGEFLOPPY_H

	class CFloppyImage:public CImage{
	public:
		typedef WORD TCrc16;

		static TCrc16 GetCrc16Ccitt(TCrc16 seed,LPCVOID bytes,WORD nBytes);
		static TCrc16 GetCrc16Ccitt(LPCVOID bytes,WORD nBytes);
		static bool IsValidSectorLengthCode(BYTE lengthCode);
	protected:
		Medium::TType floppyType; // DD/HD

		CFloppyImage(PCProperties properties,bool hasEditableSettings);

		WORD GetUsableSectorLength(BYTE sectorLengthCode) const;
		TFormat::TLengthCode GetMaximumSectorLengthCode() const;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		std::unique_ptr<CSectorDataSerializer> CreateSectorDataSerializer(CHexaEditor *pParentHexaEditor) override sealed;
		TLogTime EstimateNanosecondsPerOneByte() const override;
		virtual void EstimateTrackTiming(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,BYTE gap3,PLogTime startTimesNanoseconds) const;
	};

#endif // IMAGEFLOPPY_H
