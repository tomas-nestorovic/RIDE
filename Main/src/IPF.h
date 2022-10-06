#ifndef IPF_H
#define IPF_H

	class CIpf sealed:public CCapsBase{
	public:
		static const TProperties Properties;

		CIpf();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
	};

#endif // IPF_H
