#ifndef SCL_H
#define SCL_H

	class CSCL sealed:public CImageRaw{
		TStdWinError SaveAllModifiedTracks(LPCTSTR lpszPathName,PBackgroundActionCancelable pAction) override;
	public:
		static const TProperties Properties;

		CSCL();

		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
	};

#endif // SCL_H
