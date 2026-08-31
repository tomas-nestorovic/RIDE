#ifndef SCL_H
#define SCL_H

	class CSCL sealed:public CImageRaw{
		TStdWinError SaveAllModifiedTracks(LPCTSTR lpszPathName,CActionProgress &ap) override;
	public:
		static const TProperties Properties;

		CSCL();

		TStdWinError SetMediumTypeAndGeometry(RCFormat format) override;
	};

#endif // SCL_H
