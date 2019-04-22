#include "stdafx.h"
#include "MDOS2.h"
#include "MSDOS7.h"
#include "TRDOS.h"
#include "GDOS.h"

	CRidePen::CRidePen(BYTE thickness,COLORREF color)
		// ctor
		: CPen(PS_SOLID,thickness,color) {
	}

	const CRidePen CRidePen::BlackHairline(0,0);
	const CRidePen CRidePen::WhiteHairline(0,0xffffff);
	const CRidePen CRidePen::RedHairline(0,0xff);




	CRideBrush::CRideBrush(int stockObjectId){
		// ctor
		CreateStockObject(stockObjectId);
	}

	CRideBrush::CRideBrush(bool sysColor,int sysColorId){
		// ctor
		CreateSysColorBrush(sysColorId);
	}

	const CRideBrush CRideBrush::None=NULL_BRUSH;
	const CRideBrush CRideBrush::Black=BLACK_BRUSH;
	const CRideBrush CRideBrush::White=WHITE_BRUSH;
	const CRideBrush CRideBrush::BtnFace(true,COLOR_BTNFACE);
	const CRideBrush CRideBrush::Selection(true,COLOR_ACTIVECAPTION);




	CRideFont::CRideFont(LPCTSTR face,int pointHeight,bool bold,bool dpiScaled,int pointWidth){
		// ctor
		// - creating the Font
		//CreatePointFont(pointHeight,face);
		float fontHeight=10.f*-pointHeight/72.f, fontWidth=10.f*-pointWidth/72.f;
		if (dpiScaled){
			const float factor=Utils::LogicalUnitScaleFactor;
			fontHeight*=factor, fontWidth*=factor;
		}
		CreateFont( fontHeight, fontWidth, 0, 0,
					bold*FW_BOLD,
					FALSE, FALSE, FALSE,
					DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
					ANTIALIASED_QUALITY,
					FF_DONTCARE,
					face
				);
/*		if (bold){
			LOGFONT logFont;
			GetObject(sizeof(logFont),&logFont);
				logFont.lfWeight=FW_BOLD;
			DeleteObject();
			CreateFontIndirect(&logFont);
		}*/
		// - determining the AvgWidth and Height of Font characters
		CClientDC dc(app.m_pMainWnd);
		const HGDIOBJ hFont0=::SelectObject( dc, m_hObject );
			TEXTMETRIC tm;
			dc.GetTextMetrics(&tm);
			charAvgWidth=tm.tmAveCharWidth;
			charHeight=tm.tmHeight;
		::SelectObject(dc,hFont0);
	}

	const CRideFont CRideFont::Small(FONT_MS_SANS_SERIF,70);
	const CRideFont CRideFont::Std(FONT_MS_SANS_SERIF,90);
	const CRideFont CRideFont::StdBold(FONT_MS_SANS_SERIF,90,true);




	CRideApp app;

	#define INI_MSG_OPEN_AS		_T("msgopenas")
	#define INI_MSG_READONLY	_T("msgro")
	#define INI_MSG_FAQ	_T("1stfaq")


	CLIPFORMAT CRideApp::cfDescriptor;
	CLIPFORMAT CRideApp::cfRideFileList;
	CLIPFORMAT CRideApp::cfContent;
	CLIPFORMAT CRideApp::cfPreferredDropEffect;
	CLIPFORMAT CRideApp::cfPerformedDropEffect;
	CLIPFORMAT CRideApp::cfPasteSucceeded;



	BEGIN_MESSAGE_MAP(CRideApp,CWinApp)
		ON_COMMAND(ID_FILE_NEW,__createNewImage__)
		ON_COMMAND(ID_FILE_OPEN,__openImage__)
		ON_COMMAND(ID_OPEN_AS,__openImageAs__)
		ON_COMMAND(ID_APP_ABOUT,__showAbout__ )
	END_MESSAGE_MAP()








	BOOL CRideApp::InitInstance(){
		// application initialization
		if (!CWinApp::InitInstance())
			return FALSE;
		// - creating/allocating resources
		//nop
		// - initializing OLE and Common Controls
		::OleInitialize(nullptr);
		static const INITCOMMONCONTROLSEX Icc={ sizeof(Icc), ICC_LINK_CLASS };
		if (!::InitCommonControlsEx(&Icc))
			return FALSE; // will end-up here if running on Windows 2000 or older!
		cfDescriptor=::RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);
		cfRideFileList=::RegisterClipboardFormat( _T("Ride") CFSTR_FILEDESCRIPTOR );
		cfContent=::RegisterClipboardFormat(CFSTR_FILECONTENTS);
		cfPreferredDropEffect=::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
		cfPerformedDropEffect=::RegisterClipboardFormat(CFSTR_PERFORMEDDROPEFFECT);
		cfPasteSucceeded=::RegisterClipboardFormat(CFSTR_PASTESUCCEEDED);
		// - registering the only document template available in this application
		AddDocTemplate(
			new CMainWindow::CTdiTemplate()
		);
		// - calling "static constructors"
		//nop
		// - registering recognizable Image types and known DOSes (in alphabetical order)
		CImage::known.AddTail( (PVOID)&D80::Properties );
		CImage::known.AddTail( (PVOID)&CDsk5::Properties );
		CImage::known.AddTail( (PVOID)&CMGT::Properties );
		CImage::known.AddTail( (PVOID)&CImageRaw::Properties );
		CImage::known.AddTail( (PVOID)&CSCL::Properties );
		CImage::known.AddTail( (PVOID)&TRD::Properties );
		CDos::known.AddTail( (PVOID)&CGDOS::Properties );
		CDos::known.AddTail( (PVOID)&CMDOS2::Properties );
		CDos::known.AddTail( (PVOID)&CMSDOS7::Properties );
		CDos::known.AddTail( (PVOID)&CTRDOS503::Properties );
		CDos::known.AddTail( (PVOID)&CTRDOS504::Properties );
		CDos::known.AddTail( (PVOID)&CTRDOS505::Properties );
		// - restoring Most Recently Used (MRU) file Images
		if ((::GetVersion()&0xff)<=5){ // for Windows XP and older ...
			delete m_pszProfileName; // ... list is stored to and read from the INI file in application's folder
			struct TTmp sealed{ TCHAR profile[MAX_PATH]; }; // encapsulating the array into a structure - because MFC4.2 doesn't know the "new TCHAR[MAX_PATH]" operator!
			PTCHAR pIniFileName=(new TTmp)->profile;
			::GetModuleFileName(0,pIniFileName,MAX_PATH);
			::lstrcpy( _tcsrchr(pIniFileName,'\\'), _T("\\RIDE.INI") );
			m_pszProfileName=pIniFileName;
		}else // for Windows Vista and newer ...
			SetRegistryKey(_T("Real and Imaginary Disk Editor")); // ... list is stored to and read from system register (as INI files need explicit administrator rights)
		LoadStdProfileSettings();
		// - displaying the document-view MainWindow
		m_pMainWnd->ShowWindow(SW_SHOW);
		m_pMainWnd->UpdateWindow();
		// - searching for newly added DOSes
		for( POSITION pos=CDos::known.GetHeadPosition(); pos; )
			if (!CDos::CRecognition().__getOrderIndex__((CDos::PCProperties)CDos::known.GetNext(pos))){
				// found a DOS that's not recorded in the profile - displaying the dialog to confirm its recognition
				((CMainWindow *)m_pMainWnd)->__changeAutomaticDiskRecognitionOrder__();
				break;
			}
		// - suggesting to visit the FAQ page to learn more about the application
		if (app.GetProfileInt(INI_GENERAL,INI_MSG_FAQ,FALSE)==0){
			if (Utils::QuestionYesNo(_T("Looks like you've launched this app for the first time. Do you want to visit the \"Frequently Asked Questions\" (FAQ) page to see what it can do?"),MB_DEFBUTTON1))
				m_pMainWnd->SendMessage(WM_COMMAND,ID_HELP_FAQ);
			else
				Utils::Information(_T("Okay! You can visit the FAQ page later by clicking Help -> FAQ."));
			app.WriteProfileInt( INI_GENERAL, INI_MSG_FAQ, TRUE );
		}
		// - parsing the command line
		CCommandLineInfo cmdInfo;
		ParseCommandLine(cmdInfo);
		if (cmdInfo.m_nShellCommand==CCommandLineInfo::FileNew) // instead of displaying the "New image" dialog ...
			cmdInfo.m_nShellCommand=CCommandLineInfo::FileNothing; // ... simply do nothing
		return ProcessShellCommand(cmdInfo);
	}
	int CRideApp::ExitInstance(){
		// application uninitialization
		// - saving the list of Most Recently Used (MRU) file Images
		SaveStdProfileSettings();
		// - calling "static destructors"
		//nop
		// - setting automatic destruction of TdiTemplate (see also TdiTemplate's ctor)
		CMainWindow::CTdiTemplate::pSingleInstance->m_bAutoDelete=TRUE;
		// - uninitializing OLE
		//::OleUninitialize(); // commented out as must be initialized until the app has really terminated (then unititialized automatically by Windows)
		// - releasing resources
		//nop
		return CWinApp::ExitInstance();
	}


	afx_msg void CRideApp::__createNewImage__(){
		// initiates creation of new Image
		OnFileNew(); // to close any previous Image
		//SaveModified()
		if (!app.m_pMainWnd->IsWindowVisible()) return; // ignoring the initial command sent by MFC immediately after the application has started
		PVOID tab;
		if (!CImage::__getActive__()){
			// no Image open, i.e. any existing Image successfully closed in above OnFileNew
			// . displaying the "New Image" Dialog
			CNewImageDialog d;
			if (d.DoModal()!=IDOK) return;
			// . creating selected Image
			const PImage image=d.fnImage();
			// . formatting Image under selected DOS
			PDos dos = image->dos = d.dosProps->fnInstantiate(image,&TFormat::Unknown);
				image->writeProtected=false; // just to be sure
				if (dos->ProcessCommand(ID_DOS_FORMAT)==CDos::TCmdResult::REFUSED || !image->GetCylinderCount()){
					// A|B, A = formatting cancelled by the user, B = formatting failed; the conditions cannot be switched (because of short-circuit evaluation)
					const TStdWinError err=::GetLastError(); // extracting the cause of error here ...
					delete image; // ... as it may change here!
					return Utils::FatalError( _T("Cannot create a new image"), err );
				}
			delete dos;
			// . automatically recognizing suitable DOS (e.g. because a floppy might not have been formatted correctly)
			TFormat formatBoot;
			dos = image->dos = CDos::CRecognition().__perform__(image,&formatBoot)->fnInstantiate(image,&formatBoot);
			image->SetMediumTypeAndGeometry( &formatBoot, dos->sideMap, dos->properties->firstSectorNumber );
			// . creating the user interface for recognized DOS
			dos->CreateUserInterface( TDI_HWND ); // assumed always ERROR_SUCCESS
			image->SetPathName(VOLUME_LABEL_DEFAULT_ANSI_8CHARS,FALSE);
		}
	}




	static bool recognizeDosAutomatically=true;

	#define ENTERING_LIMITED_MODE	_T("\n\nContinuing to view the image in limited mode.")

	CDocument *CRideApp::OpenDocumentFile(LPCTSTR lpszFileName){
		// opens document with specified FileName
		app.m_pMainWnd->SetFocus(); // to immediately carry out actions that depend on focus
		// - opening the Image
		PImage image;
		if (!::lstrcmp(lpszFileName,FDD_A_LABEL)){
			// accessing the floppy in Drive A:
			OnFileNew(); // to close any previous Image
			image=new CFDD;
			const TStdWinError err=image->Reset();
			if (err!=ERROR_SUCCESS){
				delete image;
				Utils::FatalError(_T("Cannot access the floppy drive"),err);
				return nullptr;
				//AfxThrowFileException( CFileException::OsErrorToException(err), err, FDD_A_LABEL );
			}
		}else if (CDocument *const doc=CWinApp::OpenDocumentFile(lpszFileName)){
			// Image opened successfully
			if (doc==CImage::__getActive__()) // if attempting to open an already opened Image ...
				return doc; // ... returning the already opened instance
			const LPCTSTR extension=_tcsrchr(lpszFileName,'.');
			const CImage::PCProperties p=	extension // recognizing file Image by its extension
											? CImage::__determineTypeByExtension__(extension)
											: nullptr;
			if (!p){
				Utils::FatalError(_T("Unknown container to load."));
				return nullptr;
			}
			image=p->fnInstantiate(); // instantiating recognized file Image
openImage:	if (image->OnOpenDocument(lpszFileName)){ // if opened successfully ...
				if (!image->CanBeModified()) // ... inform on eventual "read-only" mode (forced if Image on the disk is read-only, e.g. because opened from a CD-R)
					Utils::InformationWithCheckableShowNoMore(_T("The image has the Read-only flag set - editing will be disabled."),INI_GENERAL,INI_MSG_READONLY);
			}else
				switch (const TStdWinError err=::GetLastError()){
					case ERROR_BAD_FORMAT:
						if (!dynamic_cast<CImageRaw *>(image)){
							// . defining the Dialog
							class CWrongInnerFormatDialog sealed:public Utils::CCommandDialog{
								void PreInitDialog() override{
									// dialog initialization
									// : base
									Utils::CCommandDialog::PreInitDialog();
									// : supplying available actions
									__addCommandButton__( IDYES, _T("Open at least valid part of it") );
									__addCommandButton__( IDNO, _T("Try to open it as a raw sector image") );
									__addCommandButton__( IDCANCEL, _T("Don't open it") );
								}
							public:
								CWrongInnerFormatDialog()
									// ctor
									: Utils::CCommandDialog(_T("The image seems to be malformatted.")) {
								}
							} d;
							// . showing the Dialog and processing its result
							const BYTE command=d.DoModal();
							if (command==IDYES) break;
							delete image;
							OnFileNew();
							if (command==IDCANCEL) return nullptr;
							image=CImageRaw::Properties.fnInstantiate();
							goto openImage;
						}
						//fallthrough
					default:
						Utils::FatalError(_T("Cannot open the file"),err);
						return nullptr;
					case ERROR_SUCCESS:
						break;
				}
		}else
			// failed to open Image
			return nullptr;
		// - adding file Image into list of Most Recently Used (MRU) documents
		if (!dynamic_cast<CFDD *>(image))
			app.AddToRecentFileList(lpszFileName);
		// - determining the DOS
		CDos::PCProperties dosProps=nullptr;
		TFormat formatBoot; // information on Format (# of Cylinders, etc.) obtained from Image's Boot record
		if (recognizeDosAutomatically){
			// automatic recognition of suitable DOS by sequentially testing each of them
			::SetLastError(ERROR_SUCCESS); // assumption (no errors)
			dosProps=CDos::CRecognition().__perform__(image,&formatBoot);
			if (!dosProps){ // if recognition sequence cancelled ...
				delete image;
				return nullptr; // ... no Image or disk is accessed
			}
			if (dosProps==&CUnknownDos::Properties)
				Utils::Information(_T("CANNOT RECOGNIZE THE DOS!\nDoes it participate in recognition?") ENTERING_LIMITED_MODE );
		}else{
			// manual recognition of suitable DOS by user
			// . defining the Dialog
			class CDosSelectionDialog sealed:public CDialog{
				BOOL OnInitDialog() override{
					// initialization
					// - populating the list of known DOSes
					CListBox lb;
					lb.Attach(GetDlgItem(ID_DOS)->m_hWnd);
						for( POSITION pos=CDos::known.GetHeadPosition(); pos; ){
							const CDos::PCProperties p=(CDos::PCProperties)CDos::known.GetNext(pos);
							lb.SetItemDataPtr( lb.AddString(p->name), (PVOID)p );
						}
					lb.Detach();
					// - base
					return CDialog::OnInitDialog();
				}
				void CDosSelectionDialog::DoDataExchange(CDataExchange *pDX){
					// exchange of data from and to controls
					DDX_LBIndex( pDX, ID_DOS	,(int &)dosProps );
					if (pDX->m_bSaveAndValidate){
						CListBox lb;
						lb.Attach(GetDlgItem(ID_DOS)->m_hWnd);
							dosProps=(CDos::PCProperties)lb.GetItemDataPtr((int)dosProps);
						lb.Detach();
					}
				}
			public:
				CDos::PCProperties dosProps;

				CDosSelectionDialog()
					// ctor
					: CDialog(IDR_DOS_UNKNOWN) , dosProps(nullptr) {
				}
			} d;
			// . showing the Dialog and processing its result
			if (d.DoModal()!=IDOK){
				delete image;
				return nullptr;
			}else{
				formatBoot=( dosProps=d.dosProps )->stdFormats->params.format;
				formatBoot.nCylinders++;
			}
			// . informing
			Utils::InformationWithCheckableShowNoMore( _T("The image will be opened using the default format of the selected DOS (see the \"") BOOT_SECTOR_TAB_LABEL _T("\" tab if available).\n\nRISK OF DATA CORRUPTION if the selected DOS and/or format is not suitable!"), INI_GENERAL, INI_MSG_OPEN_AS );
		}
		// - instantiating recognized/selected DOS
		const PDos dos = image->dos = dosProps->fnInstantiate(image,&formatBoot);
		if (!dos){
			delete image;
			return nullptr;
		}
		image->SetMediumTypeAndGeometry( &formatBoot, dos->sideMap, dos->properties->firstSectorNumber );
		// - creating the user interface for recognized/selected DOS
		image->SetPathName( lpszFileName, TRUE ); // at this moment, Image became application's active document and the name of its underlying file is shown in MainWindow's caption
		image->SetModifiedFlag(FALSE); // just to be sure
		if (const TStdWinError err=dos->CreateUserInterface(TDI_HWND)){
			TCHAR errMsg[100];
			::wsprintf( errMsg, _T("Cannot use \"%s\" to access the medium"), dosProps->name );
			Utils::FatalError(errMsg,err);
			CMainWindow::CTdiTemplate::pSingleInstance->__closeDocument__();
			return nullptr;
		}
		// - informing on what to do in case of DOS misrecognition
		Utils::InformationWithCheckableShowNoMore( _T("If the DOS has been misrecognized, adjust the recognition sequence under \"Image -> Recognition\"."), INI_GENERAL, INI_MISRECOGNITION );
		// - returning the just open Image
		return image;
	}

	afx_msg void CRideApp::__openImage__(){
		// opens Image and recognizes suitable DOS automatically
		recognizeDosAutomatically=true;
		OnFileOpen();
	}

	afx_msg void CRideApp::__openImageAs__(){
		// opens Image and lets user to determine suitable DOS
		recognizeDosAutomatically=false;
		CMainWindow::CTdiTemplate::pSingleInstance->__closeDocument__(); // to close any previous Image
		OnFileOpen();
	}

	afx_msg void CRideApp::__showAbout__() const{
		// about
		SYSTEMTIME st;
		::GetLocalTime(&st);
		TCHAR buf[80];
		::wsprintf( buf, _T("Version ") APP_VERSION _T("\n\ntomascz, 2015—%d"), st.wYear );
		Utils::Information(buf);
	}




	

	#define FDD_ACCESS	nullptr

	static HHOOK ofn_hHook;
	static PTCHAR ofn_fileName;

	static LRESULT CALLBACK __dlgOpen_hook__(int kod,WPARAM wParam,LPARAM lParam){
		// hooking the "Open/Save File" dialogs
		const LPCWPSTRUCT pcws=(LPCWPSTRUCT)lParam;
		if (pcws->message==WM_NOTIFY && pcws->wParam==ID_DRIVEA) // notification regarding Drive A:
			switch ( ((LPNMHDR)pcws->lParam)->code ){
				case NM_CLICK:
				case NM_RETURN:{
					const PLITEM pItem=&( (PNMLINK)pcws->lParam )->item;
					if (!pItem->iLink){
						::EndDialog( ::GetParent(pcws->hwnd), IDOK );
						ofn_fileName=FDD_ACCESS;
					}else{
						TCHAR bufT[200];
						::WideCharToMultiByte(CP_ACP,0,pItem->szUrl,-1,bufT,sizeof(bufT)/sizeof(TCHAR),nullptr,nullptr);
						Utils::NavigateToUrlInDefaultBrowser(bufT);
					}
					return 0;
				}
			}
		return ::CallNextHookEx(ofn_hHook,kod,wParam,lParam);
	}
	bool CRideApp::__doPromptFileName__(PTCHAR fileName,bool fddAccessAllowed,UINT stdStringId,DWORD flags,LPCVOID singleAllowedImageProperties){
		// reimplementation of CDocManager::DoPromptFileName
		// - creating the list of Filters
		const CImage::PCProperties singleAllowedImage=(CImage::PCProperties)singleAllowedImageProperties;
		TCHAR buf[300],*a=buf; // an "always big enough" buffer
		DWORD nFilters=0;
		if (singleAllowedImage)
			// list of Filters consists of only one item
			::wsprintf( buf, _T("%s (%s)|%s|"), singleAllowedImage->name, singleAllowedImage->filter, singleAllowedImage->filter );
		else{
			// list of Filters consists of all recognizable Images
			// . all known Images
			a+=_stprintf( a, _T("All known images|") );
			for( POSITION pos=CImage::known.GetHeadPosition(); pos; )
				a+=_stprintf( a, _T("%s;"), ((CImage::PCProperties)CImage::known.GetNext(pos))->filter );
			*(a-1)='|'; // replacing semicolon with pipe '|'
			// . individual Images by extension
			for( POSITION pos=CImage::known.GetHeadPosition(); pos; nFilters++ ){
				const CImage::PCProperties p=(CImage::PCProperties)CImage::known.GetNext(pos);
				a+=_stprintf( a, _T("%s (%s)|%s|"), p->name, p->filter, p->filter );
			}
		}
		nFilters++;
		// - creating a standard "Open/Save File" Dialog
		CString title;
			title.LoadString(stdStringId);
		CFileDialog d( stdStringId==AFX_IDS_OPENFILE, _T(""), nullptr, flags|OFN_OVERWRITEPROMPT, buf );
			d.m_ofn.lStructSize=sizeof(OPENFILENAME); // to show the "Places bar"
			d.m_ofn.nFilterIndex=1;
			d.m_ofn.lpstrTitle=title;
			d.m_ofn.lpstrFile=fileName;
		if (fddAccessAllowed){
			// . extending the standard Dialog with a control to access local floppy Drive
			d.m_ofn.Flags|= OFN_ENABLETEMPLATE | OFN_EXPLORER;
			d.m_ofn.lpTemplateName=MAKEINTRESOURCE(IDR_OPEN_CUSTOM);
			// . hooking, showing the Dialog, processing its Result, unhooking, and returning the Result
			ofn_fileName=fileName;
			ofn_hHook=::SetWindowsHookEx( WH_CALLWNDPROC, __dlgOpen_hook__, 0, ::GetCurrentThreadId() );
				const bool result=d.DoModal()==IDOK;
			::UnhookWindowsHookEx(ofn_hHook);
			if (ofn_fileName==FDD_ACCESS)
				::lstrcpy( fileName, FDD_A_LABEL ); // cannot directly write to FileName in the Hook procedure as the "Open/Save File" dialog writes '\0' to the buffer under Windows 7 and higher
			return result;
		}else
			return d.DoModal()==IDOK;
	}
	void CRideApp::OnFileOpen(){
		// public wrapper
		//CWinApp::OnFileOpen();
		TCHAR fileName[MAX_PATH];
		*fileName='\0';
		if (__doPromptFileName__( fileName, true, AFX_IDS_OPENFILE, OFN_FILEMUSTEXIST, nullptr ))
			OpenDocumentFile(fileName);
	}

	void WINAPI AfxThrowInvalidArgException(){
		// without this function, we wouldn't be able to build the "MFC 4.2" version!
	}
