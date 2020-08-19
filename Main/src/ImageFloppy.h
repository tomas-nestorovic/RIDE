#ifndef IMAGEFLOPPY_H
#define IMAGEFLOPPY_H

	class CFloppyImage:public CImage{
	protected:
		typedef WORD TCrc16;

		static TCrc16 GetCrc16Ccitt(PCSectorData buffer,WORD length);

		TMedium::TType floppyType; // DD/HD

		CFloppyImage(PCProperties properties,bool hasEditableSettings);

		WORD __getUsableSectorLength__(BYTE sectorLengthCode) const;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		std::unique_ptr<CSectorDataSerializer> CreateSectorDataSerializer(CHexaEditor *pParentHexaEditor) override sealed;
		int EstimateNanosecondsPerOneByte() const override;
		virtual void EstimateTrackTiming(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PINT startTimesNanoseconds) const;
	};

#endif // IMAGEFLOPPY_H
