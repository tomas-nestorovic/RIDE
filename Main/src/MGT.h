#ifndef MGT_H
#define MGT_H

	class CMGT sealed:public CImageRaw{
	public:
		static const TProperties Properties;

		CMGT();

		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
	};

#endif // MGT_H
