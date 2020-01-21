#ifndef RIDEDEBUG_H
#define RIDEDEBUG_H

//#define LOGGING_ENABLED // uncomment to enable logging, otherwise logging and its classes unavailable

namespace Debug{

#ifdef LOGGING_ENABLED

	class CLogFile sealed:public CFile{
		const bool permanentlyOpen;
		TCHAR filename[MAX_PATH];
		BYTE nIndent;
	public:
		static CLogFile Default;

		CMapStringToPtr actionMinTimes,actionMaxTimes; // minimum and maximum times that each of Actions took (in milliseconds)

		class CAction sealed{
			CLogFile &logFile;
			const LPCTSTR name;
			const Utils::CLocalTime start;
		public:
			CAction(LPCTSTR name,CLogFile &rLogFile=Default);
			~CAction();
		};

		CLogFile(LPCTSTR logDescription,bool permanentlyOpen);
		~CLogFile();
		
		CLogFile &operator<<(TCHAR c);
		CLogFile &operator<<(LPCTSTR text);
		CLogFile &operator<<(DWORD dw);
		CLogFile &operator<<(const Utils::CLocalTime &rlt);
		CLogFile &operator<<(const TSectorId &rsi);
		CLogFile &operator<<(const TPhysicalAddress &rchs);

		LPCTSTR LogMessage(LPCTSTR text);
		TStdWinError LogError(TStdWinError err);
		bool LogBool(bool b);
		PSectorData LogSectorDataPointer(PSectorData pSectorData);
		DWORD LogDialogResult(DWORD result);
	};

	#define LOG_ACTION(name)\
		const Debug::CLogFile::CAction a(name)

	#define LOG_MESSAGE(text)\
		Debug::CLogFile::Default.LogMessage(text)

	#define LOG_ERROR(error)\
		Debug::CLogFile::Default.LogError(error)

	#define LOG_BOOL(boolValue)\
		Debug::CLogFile::Default.LogBool(boolValue)

	#define LOG_PSECTORDATA(pSectorData)\
		Debug::CLogFile::Default.LogSectorDataPointer(pSectorData)

	#define LOG_CYLINDER_ACTION(cyl,name)\
		TCHAR __cylinderActionName[200];\
		::wsprintf(__cylinderActionName,_T("Cyl %d %s"),cyl,name);\
		LOG_ACTION(__cylinderActionName)

	#define LOG_TRACK_ACTION(cyl,head,name)\
		TCHAR __trackActionName[200];\
		::wsprintf(__trackActionName,_T("Track [Cyl=%d,Head=%d] %s"),cyl,head,name);\
		LOG_ACTION(__trackActionName)

	#define LOG_SECTOR_ACTION(pSectorId,name)\
		TCHAR __sectorActionName[200];\
		::wsprintf(__sectorActionName,_T("Sector %s %s"),(pSectorId)->ToString(__sectorActionName+160),name);\
		LOG_ACTION(__sectorActionName)

	#define LOG_DIALOG_DISPLAY(name)\
		LOG_ACTION(name)

	#define LOG_DIALOG_RESULT(result)\
		Debug::CLogFile::Default.LogDialogResult(result)

	#define LOG_FILE_ACTION(dos,file,name)\
		TCHAR __fileActionName[MAX_PATH+200];\
		::wsprintf(__fileActionName,_T("File \"%s\" %s"),(LPCTSTR)dos->GetFilePresentationNameAndExt(file),name);\
		LOG_ACTION(__fileActionName)

#else
	#define LOG_ACTION(name)

	#define LOG_MESSAGE(text)\
		text

	#define LOG_ERROR(error)\
		error

	#define LOG_BOOL(boolValue)\
		boolValue

	#define LOG_PSECTORDATA(pSectorData)\
		pSectorData

	#define LOG_CYLINDER_ACTION(cyl,name)

	#define LOG_TRACK_ACTION(cyl,head,name)

	#define LOG_SECTOR_ACTION(pSectorId,name)

	#define LOG_DIALOG_DISPLAY(name)

	#define LOG_DIALOG_RESULT(result)\
		result

	#define LOG_FILE_ACTION(dos,file,name)

#endif

}

#endif // RIDEDEBUG_H