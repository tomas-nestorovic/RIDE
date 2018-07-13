#if     _MSC_VER > 1000
#pragma once
#endif

#ifndef _INC_CRTNEW_MFC42_X86
#define _INC_CRTNEW_MFC42_X86

#if     !defined(_WIN32)
#error ERROR: Only Win32 target supported!
#endif

#define CRTNEW_RESOLVE_CRTDBGREPORT_LINK_ERROR() _CrtDbgReport(0,0,0,0,0)

#ifdef  __cplusplus
extern "C" {
#endif  /* __cplusplus */

	//Set crtnew.lib's working mode. 
	//If bIsDebugMode=TRUE(not zero), then it will call the __CxxFrameHandler in msvcrtd.dll
	//If bIsDebugMode=FALSE(zero), then it will call the __CxxFrameHandler in msvcrt.dll
	//The Default Value is FALSE .
	void __cdecl SetDbgModeOfCRTNewLib(int bIsDebugMode);

	#ifndef _DEBUG
	//MFC4.2������û��_CrtDbgReportW�����ģ�ֻ��MBCS�汾��_CrtDbgReport������������ֻ��DEBUGģʽ�µ�msvcrtd�����У�Releaseģʽ��û�С�
	//����Releaseģʽ�£��и�chkespģ���õ���_CrtDbgReport������������Ҫ�Լ�ʵ��һ��������Releaseģʽ����Ҫ��������һ�£�_CrtDbgReport(0,0,0,0,0); ���������Ӵ���
	//There is no _CrtDbgReportW in MFC4.2 DLL Library. And only the msvcrtd has a _CrtDbgReport function.
	//But in this static lib , there is a chkesp module required the _CrtDbgReport function. So we have to implement a dummy one to solve linker errors.
	//It's only needed to be called in a "Release" mode project, just like this : _CrtDbgReport(0,0,0,0,0); or use macro: CRTNEW_RESOLVE_CRTDBGREPORT_LINK_ERROR();
	//If you still need it to report some information, you could implement it by your self.
	inline int __cdecl _CrtDbgReport(int _ReportType,const char * _Filename,int _LineNumber,const char * _ModuleName,const char * _Format,...)
	{
		return 0;
	}
	#endif
#ifdef  __cplusplus
}
#endif  /* __cplusplus */

#endif  /* _INC_CRTNEW_MFC42_X86 */