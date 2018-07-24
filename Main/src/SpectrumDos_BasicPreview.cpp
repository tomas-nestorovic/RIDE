#include "stdafx.h"

	CSpectrumDos::CBasicPreview *CSpectrumDos::CBasicPreview::pSingleInstance;

	#define INI_PREVIEW	_T("ZxBasic")

	#define PREVIEW_WIDTH_DEFAULT	400
	#define PREVIEW_HEIGHT_DEFAULT	300

	CSpectrumDos::CBasicPreview::CBasicPreview(const CFileManagerView &rFileManager)
		// ctor
		// - base
		: CFilePreview(	&listingView, INI_PREVIEW, rFileManager, PREVIEW_WIDTH_DEFAULT, PREVIEW_HEIGHT_DEFAULT )
		, listingView(_T("")) {
/*
		CreateEx(	0, _T("RICHEDIT50W"), NULL,
					WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_BORDER|WS_VISIBLE|ES_MULTILINE,
					0,0, PREVIEW_WIDTH_DEFAULT,PREVIEW_HEIGHT_DEFAULT,
					NULL, 0, NULL
				);
		//*/
/*
		const HWND hRichEdit=::CreateWindowEx(	0, _T("RICHEDIT50W"), //_T("RichEdit20A"),
												NULL,
												WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_BORDER|WS_VISIBLE|ES_MULTILINE,
												100,100,500,500,
												NULL, 0, app.m_hInstance,
												NULL
											);
		this->Attach(hRichEdit);
		//*/
		// - initialization
		pSingleInstance=this;
		// - creating the TemporaryFile to store HTML-formatted BASIC Listing
		::GetTempPath(MAX_PATH,tmpFileName);
		::GetTempFileName( tmpFileName, NULL, TRUE, tmpFileName );
		// - creating the ListingView
/*		
		::InitCommonControls();
		//::CoInitialize(NULL);
		::LoadLibrary("riched20.dll");		// _T("RichEdit20A")
		::LoadLibrary(_T("MSFTEDIT.DLL"));	// _T("RICHEDIT50W"), but must also load "riched20.dll" !!
		//*/
		listingView.Create( NULL, NULL, WS_CHILD|WS_VISIBLE, rectDefault, this, AFX_IDW_PANE_FIRST, NULL );
		listingView.OnInitialUpdate();
		// - showing the first File
		__showNextFile__();
	}

	CSpectrumDos::CBasicPreview::~CBasicPreview(){
		// dtor
		// - uninitialization
		::DeleteFile(tmpFileName);
		pSingleInstance=NULL;
	}







	#define DOS		rFileManager.tab.dos
	#define IMAGE	DOS->image

	#define PREVIEW_LABEL	"BASIC listing"

	void CSpectrumDos::CBasicPreview::RefreshPreview(){
		// refreshes the Preview (e.g. when switched to another File)
		if (const PCFile file=pdt->entry){
			CFileReaderWriter frw(DOS,pdt->entry);
			// . generating HTML-formatted content
			CFile fTmp( tmpFileName, CFile::modeWrite|CFile::modeCreate );
				TUtils::WriteToFile(fTmp,_T("<html><body style=\"font-family:'Courier New'\">"));
					// : BASIC Listing
					TUtils::WriteToFile(fTmp,_T("<table cellpadding=3 cellspacing=0>"));
						bool error=false;
						do{
							UBigEndianWord lineNumber;
							if (error=frw.Read(&lineNumber,sizeof(WORD))!=sizeof(WORD)) // error
								break;
							if (lineNumber>0x39ff) // invalid LineNumber is a correct end of BASIC program
								break;
							WORD nBytesOfLine;
							if (error=frw.Read(&nBytesOfLine,sizeof(WORD))!=sizeof(WORD)) // error
								break;
							if (!nBytesOfLine)
								break;
							BYTE lineBytes[65536];
							if (error=frw.Read(lineBytes,nBytesOfLine)!=nBytesOfLine) // error
								break;
							TUtils::WriteToFile(fTmp,_T("<tr>"));
								// | adding a new cell to the "Line Number" column
								TUtils::WriteToFile(fTmp,_T("<td width=90pt align=right valign=top style=\"padding-right:5pt;background:silver\">"));
									TUtils::WriteToFile(fTmp,lineNumber);
								//TUtils::WriteToFile(fTmp,_T("</td>")); // commented out as written in the following command
								// | adding a new cell to the "Listing" column
								TUtils::WriteToFile(fTmp,_T("</td><td style=\"padding-left:5pt\">"));
									PCBYTE pLineByte=lineBytes;
									while (--nBytesOfLine>0){ // skipping the terminating Enter character (0x0d)
										const BYTE b=*pLineByte++;
										if (TZxRom::IsStdUdgSymbol(b)){
											//TODO
										}else if (b==14)
											// skipping the five-Byte internal form of a number
											pLineByte+=5, nBytesOfLine-=5;
										else{
											TCHAR buf[16];
											TUtils::WriteToFile( fTmp, TZxRom::ZxToAscii((LPCSTR)&b,1,buf) );
										}
									}
								//TUtils::WriteToFile(fTmp,_T("</td>")); // commented out as written in the following command	
							TUtils::WriteToFile(fTmp,_T("</td></tr>"));
						} while (frw.GetPosition()<frw.GetLength());
					TUtils::WriteToFile(fTmp,_T("</table>"));
					// : Error in BASIC Listing
					if (error){
						TUtils::WriteToFile(fTmp,_T("<p>Error!</p>"));
						goto htmlDocEnd;
					}
htmlDocEnd:		TUtils::WriteToFile(fTmp,_T("</body></html>"));
			fTmp.Close();
			// . opening the HTML-formatted content
			listingView.Navigate2(tmpFileName);
			// . updating the window caption
			TCHAR buf[MAX_PATH],bufCaption[20+MAX_PATH];
			::wsprintf(	bufCaption,
						PREVIEW_LABEL " (%s)",
						TZxRom::ZxToAscii( DOS->GetFileNameWithAppendedExt(file,buf),-1, bufCaption+20 )
					);
			SetWindowText(bufCaption);
		}else
			SetWindowText(PREVIEW_LABEL);
		SetWindowPos( NULL, 0,0, 0,0, SWP_NOZORDER|SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED );
	}

	//void CSpectrumDos::CBasicPreview::CListingView::__loadBasicAndVariablesFromFile__(PCFile file){
		// loads Basic program and/or Basic variables from specified File
		//SetRedraw(FALSE);
			// - resetting the content
//			SetWindowText(_T(""));			
			// - resetting the paragraph style
/*			PARAFORMAT pf=GetParaFormatSelection();
				pf.dxOffset=500;
				pf.dxStartIndent=500;
				pf.dxRightIndent=500;
			SetParaFormat(pf);*/
			// - loading the Basic program (if any)
			//SetWindowText(_T("ajhd askjdhk jdkdjahdkd hka hdkadh k hdkjhd akjdh akjd hakjd hakjdh kdh kdh kadh k dhkjdh akjdh kjdh ak"));
		//SetRedraw(TRUE);
		//Invalidate();
	//}
