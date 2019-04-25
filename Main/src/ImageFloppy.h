#ifndef IMAGEFLOPPY_H
#define IMAGEFLOPPY_H

	class CFloppyImage:public CImage{
	protected:
		static WORD __getCrcCcitt__(PCSectorData buffer,WORD length);

		TMedium::TType floppyType; // DD/HD

		CFloppyImage(PCProperties properties,bool hasEditableSettings);

		WORD __getUsableSectorLength__(BYTE sectorLengthCode) const;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		CSectorDataSerializer *CreateSectorDataSerializer() override sealed;
	};

#endif // IMAGEFLOPPY_H
