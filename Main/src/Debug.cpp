#include "stdafx.h"

namespace Debug{

#ifdef LOGGING_ENABLED

	CLogFile CLogFile::Default(_T("default"),true);

	CLogFile::CLogFile(LPCTSTR logDescription,bool permanentlyOpen)
		// ctor
		// - initialization
		: permanentlyOpen(permanentlyOpen) , nIndent(0) {
		// - creating a standard "Save File" Dialog with suggested Filename
		#ifdef _DEBUG
			SYSTEMTIME st;
			::GetSystemTime(&st);
			::wsprintf( filename, _T("%d_%d_%d_%d_%s.txt"), st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, logDescription );
		#else
			CFileDialog d( false, _T(""), NULL, OFN_OVERWRITEPROMPT );
				d.m_ofn.lStructSize=sizeof(OPENFILENAME); // to show the "Places bar"
				d.m_ofn.nFilterIndex=1;
				d.m_ofn.lpstrTitle=_T("Enter new log file name");
				SYSTEMTIME st;
				::GetSystemTime(&st);
				::wsprintf( filename, _T("c:\\%d_%d_%d_%d_%s.txt"), st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, logDescription );
				d.m_ofn.lpstrFile=filename;
			if (d.DoModal()!=IDOK)
				*filename='\0';
		#endif
		// - opening the file for writing if commanded to have it open permanently
		if (*filename && permanentlyOpen)
			Open( filename, CFile::modeWrite|CFile::modeCreate|CFile::modeNoTruncate );
	}

	CLogFile &CLogFile::operator<<(TCHAR c){
		// writes given Character to the LogFile
		if (*filename)
			if (permanentlyOpen)
				Write(&c,sizeof(c));
			else{
				Open( filename, CFile::modeWrite|CFile::modeCreate|CFile::modeNoTruncate );
				SeekToEnd();
				Write(&c,sizeof(c));
				Close();
			}
		return *this;
	}

	CLogFile &CLogFile::operator<<(LPCTSTR text){
		// writes given Text to the LogFile, removing all new-line characters
		TCHAR buf[2048];
		for( PTCHAR p=::lstrcpy(buf,text); const TCHAR c=*p; p++ )
			if (c=='\r' || c=='\n')
				*p=' ';
		if (*filename)
			if (permanentlyOpen)
				Write( text, ::lstrlen(text) );
			else{
				Open( filename, CFile::modeWrite|CFile::modeCreate|CFile::modeNoTruncate );
				SeekToEnd();
				Write( text, ::lstrlen(text) );
				Close();
			}
		return *this;
	}

	CLogFile &CLogFile::operator<<(DWORD dw){
		// writes given number to the LogFile
		TCHAR buf[16];
		return operator<<( _itot(dw,buf,10) );
	}

	CLogFile &CLogFile::operator<<(const SYSTEMTIME &rst){
		// writes given SystemTime to the LogFile
		TCHAR buf[64];
		::wsprintf( buf, _T("%02d:%02d:%02d:%03d"), rst.wHour, rst.wMinute, rst.wSecond, rst.wMilliseconds );
		return operator<<(buf);
	}

	CLogFile &CLogFile::operator<<(const TSectorId &rsi){
		// writes given SectorId to the LogFile
		TCHAR buf[32];
		return operator<<( rsi.ToString(buf) );
	}

	CLogFile &CLogFile::operator<<(const TPhysicalAddress &rchs){
		// writes given PhysicalAddress to the LogFile
		TCHAR buf[64],si[32];
		::wsprintf( buf, _T("[%d,%d,%s]"), rchs.cylinder, rchs.head, rchs.sectorId.ToString(si) );
		return operator<<(buf);
	}

	TStdWinError CLogFile::LogError(TStdWinError err){
		// logs given Error to the LogFile and returns the logged Error
		SYSTEMTIME time;
		::GetSystemTime(&time);
		TCHAR buf[220];
		::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, 0,
						buf, sizeof(buf)/sizeof(TCHAR),
						NULL
					);
		*this << CString('\t',nIndent) << time << _T(" Error ") << (DWORD)err << _T(": ") << buf << '\r' << '\n'; // separate new-line characters as the operator<<(LPCTSTR) operator removes new-line characters, so can't use _T("\r\n")
		return err;
	}

	DWORD CLogFile::LogDialogResult(DWORD result){
		// logs given dialog Result to the LogFile and returns the logged Result
		SYSTEMTIME time;
		::GetSystemTime(&time);
		Default << CString('\t',nIndent) << time << _T(" Dialog result = ") << result << '\r' << '\n'; // separate new-line characters as the operator<<(LPCTSTR) operator removes new-line characters, so can't use _T("\r\n")
		return result;
	}









	CLogFile::CAction::CAction(LPCTSTR name,CLogFile &rLogFile)
		// ctor
		: logFile(rLogFile)
		, name(name) {
		::GetSystemTime(&start);
		logFile << CString('\t',rLogFile.nIndent++) << start << _T(" BEGIN ") << name << '\r' << '\n'; // separate new-line characters as the operator<<(LPCTSTR) operator removes new-line characters, so can't use _T("\r\n")
	}

	CLogFile::CAction::~CAction(){
		// dtor
		SYSTEMTIME end;
		::GetSystemTime(&end);
		const DWORD nMilliseconds=	((end.wHour*60+end.wMinute)*60+end.wSecond)*1000+end.wMilliseconds
									-
									( ((start.wHour*60+start.wMinute)*60+start.wSecond)*1000+start.wMilliseconds );
		logFile << CString('\t',--logFile.nIndent) << end << _T(" END, t=") << nMilliseconds << _T("ms") << '\r' << '\n'; // separate new-line characters as the operator<<(LPCTSTR) operator removes new-line characters, so can't use _T("\r\n")
	}

#endif

}
