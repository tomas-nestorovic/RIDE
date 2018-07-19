#ifndef SCL_H
#define SCL_H

	class CSCL sealed:public CImageRaw{
	public:
		static const TProperties Properties;

		CSCL();

		BOOL OnSaveDocument(LPCTSTR lpszPathName) override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
	};

#endif // SCL_H
