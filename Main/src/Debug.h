#ifndef RIDEDEBUG_H
#define RIDEDEBUG_H

namespace Debug{

	class CLogFile sealed{
		TCHAR filename[MAX_PATH];
		BYTE nIndent;
	public:
		class CAction sealed{
			CLogFile &logFile;
			const LPCTSTR name;
			SYSTEMTIME start;
		public:
			CAction(CLogFile &rLogFile,LPCTSTR name);
			~CAction();
		};

		CLogFile(LPCTSTR logDescription);
		
		const CLogFile &operator<<(TCHAR c) const;
		const CLogFile &operator<<(LPCTSTR text) const;
		const CLogFile &operator<<(DWORD dw) const;
		const CLogFile &operator<<(const SYSTEMTIME &rst) const;
		const CLogFile &operator<<(const TSectorId &rsi) const;
		const CLogFile &operator<<(const TPhysicalAddress &rchs) const;

		void LogError(TStdWinError err) const;
	};

}

#endif // RIDEDEBUG_H