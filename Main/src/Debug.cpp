#include "stdafx.h"

namespace Debug{

	CLogFile::CLogFile(LPCTSTR logDescription)
		// ctor
		: nIndent(0) {
		SYSTEMTIME st;
		::GetSystemTime(&st);
		::wsprintf( filename, _T("c:\\%d_%d_%d_%d_%s.txt"), st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, logDescription );
	}

	const CLogFile &CLogFile::operator<<(TCHAR c) const{
		// writes given Character to the LogFile
		CFile f( filename, CFile::modeWrite|CFile::modeCreate|CFile::modeNoTruncate );
		f.Write(&c,sizeof(c));
		return *this;
	}

	const CLogFile &CLogFile::operator<<(LPCTSTR text) const{
		// writes given Text to the LogFile
		CFile f( filename, CFile::modeWrite|CFile::modeCreate|CFile::modeNoTruncate );
		f.Write( text, ::lstrlen(text) );
		return *this;
	}

	const CLogFile &CLogFile::operator<<(DWORD dw) const{
		// writes given number to the LogFile
		TCHAR buf[16];
		return operator<<( _itot(dw,buf,10) );
	}

	const CLogFile &CLogFile::operator<<(const SYSTEMTIME &rst) const{
		// writes given SystemTime to the LogFile
		TCHAR buf[64];
		::wsprintf( buf, _T("%02d:%02d:%02d:%03d"), rst.wHour, rst.wMinute, rst.wSecond, rst.wMilliseconds );
		return operator<<(buf);
	}

	const CLogFile &CLogFile::operator<<(const TSectorId &rsi) const{
		// writes given SectorId to the LogFile
		TCHAR buf[32];
		return operator<<( rsi.ToString(buf) );
	}

	const CLogFile &CLogFile::operator<<(const TPhysicalAddress &rchs) const{
		// writes given PhysicalAddress to the LogFile
		TCHAR buf[64],si[32];
		::wsprintf( buf, _T("[%d,%d,%s]"), rchs.cylinder, rchs.head, rchs.sectorId.ToString(si) );
		return operator<<(buf);
	}

	void CLogFile::LogError(TStdWinError err) const{
		// logs given Error to the LogFile
		SYSTEMTIME time;
		::GetSystemTime(&time);
		TCHAR buf[220];
		*this << time << CString('\t',nIndent) << _T("Error ") << (DWORD)err << _T(": ") << TUtils::__formatErrorCode__(buf,err) << '\n';
	}








	CLogFile::CAction::CAction(CLogFile &rLogFile,LPCTSTR name)
		// ctor
		: logFile(rLogFile)
		, name(name) {
		::GetSystemTime(&start);
		logFile << start << CString('\t',++rLogFile.nIndent) << name << _T(" BEGIN") << '\n';
	}

	CLogFile::CAction::~CAction(){
		// dtor
		SYSTEMTIME end;
		::GetSystemTime(&end);
		const DWORD nMicroseconds=	((end.wHour*60+end.wMinute)*60+end.wSecond)*1000+end.wMilliseconds
									-
									((start.wHour*60+start.wMinute)*60+start.wSecond)*1000+start.wMilliseconds;
		logFile << end << CString('\t',logFile.nIndent--) << name << _T(" END, t=") << nMicroseconds << _T("µs\n");
	}

}
