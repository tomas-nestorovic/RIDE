#include "stdafx.h"

namespace Debug{

#ifdef LOGGING_ENABLED

	#include <intrin.h>

	CLogFile CLogFile::Default(_T("default"),true);

	CLogFile::CLogFile(LPCTSTR logDescription,bool permanentlyOpen)
		// ctor
		// - initialization
		: permanentlyOpen(permanentlyOpen) , nIndent(0) {
		// - creating a standard "Save File" Dialog with suggested Filename
		#ifdef _DEBUG
			const TUtils::CLocalTime st;
			::wsprintf( filename, _T("%d_%d_%d_%s.txt"), st.GetHours(), st.GetMinutes(), st.GetSeconds(), logDescription );
		#else
			CFileDialog d( false, _T(""), NULL, OFN_OVERWRITEPROMPT );
				d.m_ofn.lStructSize=sizeof(OPENFILENAME); // to show the "Places bar"
				d.m_ofn.nFilterIndex=1;
				d.m_ofn.lpstrTitle=_T("Enter new log file name");
				const TUtils::CLocalTime st;
				::wsprintf( filename, _T("c:\\%d_%d_%d_%s.txt"), st.GetHours(), st.GetMinutes(), st.GetSeconds(), logDescription );
				d.m_ofn.lpstrFile=filename;
			if (d.DoModal()!=IDOK){
				::PostQuitMessage(ERROR_CANCELLED);
				*filename='\0'; // the file won't be created and no attempt to write to it will be made
				return;
			}
		#endif
		// - opening the file for writing if commanded to have it open permanently
		if (permanentlyOpen)
			Open( filename, CFile::modeWrite|CFile::modeCreate|CFile::modeNoTruncate );
		// - logging some machine information, potentially useful for correctly interpreting the final Log file
		{	LOG_ACTION(_T("Machine info"));
			TCHAR cpuBrandName[200];
			HKEY hCpuKey;
			if (TStdWinError err=::RegOpenKeyEx( HKEY_LOCAL_MACHINE, _T("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), 0, KEY_READ, &hCpuKey ))
				LOG_MESSAGE(_T("Can't detect CPU"));
			else{
				DWORD dw=sizeof(cpuBrandName);
				if (err=::RegQueryValueEx( hCpuKey, _T("ProcessorNameString"), 0, NULL, (LPBYTE)cpuBrandName, &dw )){
					// extended CPU information NOT available - retrieving just basic information, available always
					TCHAR vendor[30]; dw=sizeof(vendor);
					::RegQueryValueEx( hCpuKey, _T("VendorIdentifier"), 0, NULL, (LPBYTE)vendor, &dw ); // e.g. "GenuineIntel"
					TCHAR identifier[80]; dw=sizeof(identifier);
					::RegQueryValueEx( hCpuKey, _T("Identifier"), 0, NULL, (LPBYTE)identifier, &dw ); // e.g. "x86 Family 6 Model 15 Stepping 6"
					DWORD mhz; dw=sizeof(mhz);
					::RegQueryValueEx( hCpuKey, _T("~MHz"), 0, NULL, (LPBYTE)&mhz, &dw ); // e.g. "199"
					::wsprintf( cpuBrandName, _T("%s, %s, %d MHz"), vendor, identifier, mhz );
				}else
					// yes, extended CPU information available, e.g. "Intel(R) Celeron(R) M CPU        520  @ 1.60GHz"
					//nop
				::RegCloseKey(hCpuKey);
				LOG_MESSAGE(cpuBrandName);
			}
			TCHAR buffer[80];
			MEMORYSTATUSEX mse={ sizeof(MEMORYSTATUSEX) };
			::GlobalMemoryStatusEx(&mse);
			float nUnits; LPCTSTR unitName;
			TUtils::BytesToHigherUnits( mse.ullTotalPhys, nUnits, unitName );
			_stprintf( buffer, _T("%.2f %s RAM"), nUnits, unitName );
			LOG_MESSAGE(buffer);
			OSVERSIONINFOEX osvi={ sizeof(OSVERSIONINFOEX) };
			::GetVersionEx((POSVERSIONINFO)&osvi);
			::wsprintf( buffer, _T("Windows %d.%d, Build %d, %s"), osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber, osvi.szCSDVersion );
			LOG_MESSAGE(buffer);
		}
	}

	CLogFile::~CLogFile(){
		// dtor
		if (!*filename) return;
		// - outputting how long each of Actions took
		*this << '\r' << '\n' << '\r' << '\n' << '\r' << '\n'; // separate new-line characters as the operator<<(LPCTSTR) operator removes new-line characters, so can't use _T("\r\n")
		for( POSITION pos=actionMinTimes.GetStartPosition(); pos; ){
			CString key;
			PVOID minTime,maxTime;
			actionMinTimes.GetNextAssoc(pos,key,minTime);
			actionMaxTimes.Lookup(key,maxTime);
			*this << _T("- ") << key << '\r' << '\n' << '\t' << _T("< ") << (DWORD)minTime << _T("ms, ") << (DWORD)maxTime << _T("ms >") << '\r' << '\n'; // separate new-line characters as the operator<<(LPCTSTR) operator removes new-line characters, so can't use _T("\r\n")
		}
	}

	CLogFile &CLogFile::operator<<(TCHAR c){
		// writes given Character to the LogFile
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

	CLogFile &CLogFile::operator<<(const TUtils::CLocalTime &rlt){
		// writes given SystemTime to the LogFile
		TCHAR buf[64];
		::wsprintf( buf, _T("%02d:%02d:%02d:%03d"), rlt.GetHours(), rlt.GetMinutes(), rlt.GetSeconds(), rlt.GetMilliseconds() );
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

	LPCTSTR CLogFile::LogMessage(LPCTSTR text){
		// logs given text message to the LogFile
		*this << CString('\t',nIndent) << TUtils::CLocalTime() << _T(" Message: ") << text << '\r' << '\n'; // separate new-line characters as the operator<<(LPCTSTR) operator removes new-line characters, so can't use _T("\r\n")
		return text;
	}

	TStdWinError CLogFile::LogError(TStdWinError err){
		// logs given Error to the LogFile and returns the logged Error
		TCHAR buf[220];
		::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, 0,
						buf, sizeof(buf)/sizeof(TCHAR),
						NULL
					);
		*this << CString('\t',nIndent) << TUtils::CLocalTime() << _T(" Error ") << (DWORD)err << _T(": ") << buf << '\r' << '\n'; // separate new-line characters as the operator<<(LPCTSTR) operator removes new-line characters, so can't use _T("\r\n")
		return err;
	}

	bool CLogFile::LogBool(bool b){
		// logs given Boolean value to the LogFile and returns the logged Boolean value
		Default << CString('\t',nIndent) << TUtils::CLocalTime() << _T(" Boolean = ") << (b?_T("True"):_T("False")) << '\r' << '\n'; // separate new-line characters as the operator<<(LPCTSTR) operator removes new-line characters, so can't use _T("\r\n")
		return b;
	}

	PSectorData CLogFile::LogSectorDataPointer(PSectorData pSectorData){
		// logs given PSectorData pointer to the LogFile and returns the logged PSectorData pointer
		TCHAR buf[24];
		::wsprintf( buf, _T("0%08x"), pSectorData );
		Default << CString('\t',nIndent) << TUtils::CLocalTime() << _T(" PSectorData = ") << (pSectorData!=NULL?buf:_T("Null")) << '\r' << '\n'; // separate new-line characters as the operator<<(LPCTSTR) operator removes new-line characters, so can't use _T("\r\n")
		return pSectorData;
	}

	DWORD CLogFile::LogDialogResult(DWORD result){
		// logs given dialog Result to the LogFile and returns the logged Result
		Default << CString('\t',nIndent) << TUtils::CLocalTime() << _T(" Dialog result = ") << result << '\r' << '\n'; // separate new-line characters as the operator<<(LPCTSTR) operator removes new-line characters, so can't use _T("\r\n")
		return result;
	}









	CLogFile::CAction::CAction(LPCTSTR name,CLogFile &rLogFile)
		// ctor
		: logFile(rLogFile)
		, name(name) {
		logFile << CString('\t',rLogFile.nIndent++) << start << _T(" BEGIN ") << name << '\r' << '\n'; // separate new-line characters as the operator<<(LPCTSTR) operator removes new-line characters, so can't use _T("\r\n")
	}

	CLogFile::CAction::~CAction(){
		// dtor
		// - writing to the LogFile
		const TUtils::CLocalTime end;
		const UINT_PTR nMilliseconds=( end-start ).ToMilliseconds();
		logFile << CString('\t',--logFile.nIndent) << end << _T(" END, t=") << (DWORD)nMilliseconds << _T("ms") << '\r' << '\n'; // separate new-line characters as the operator<<(LPCTSTR) operator removes new-line characters, so can't use _T("\r\n")
		// - recording the NumberOfMilliseconds as the minimum time
		PVOID value=(PVOID)-1;
		logFile.actionMinTimes.Lookup(name,value);
		if (nMilliseconds<(UINT_PTR)value)
			logFile.actionMinTimes.SetAt(name,(PVOID)nMilliseconds);
		// - recording the NumberOfMilliseconds as the maximum time
		value=NULL;
		logFile.actionMaxTimes.Lookup(name,value);
		if (nMilliseconds>(UINT_PTR)value)
			logFile.actionMaxTimes.SetAt(name,(PVOID)nMilliseconds);
	}

#endif

}
