#include "stdafx.h"
#include "MDOS2.h"
#include "MSDOS7.h"
#include "TRDOS.h"
#include "GDOS.h"
#include "BSDOS.h"

	CRideApp::CRecentFileListEx::CRecentFileListEx(const CRecentFileList &rStdMru)
		// ctor
		: CRecentFileList( rStdMru.m_nStart, rStdMru.m_strSectionName, rStdMru.m_strEntryFormat, rStdMru.m_nSize, rStdMru.m_nMaxDisplayLength ) {
		ASSERT( m_nSize<sizeof(openWith)/sizeof(openWith[0]) );
	}

	CDos::PCProperties CRideApp::CRecentFileListEx::GetDosMruFileOpenWith(int nIndex) const{
		// returns the Properties of the DOS most recently used to open the file under the specified Index (or the Properties of the Unknown DOS if automatic recognition should be used)
		ASSERT( 0<=nIndex && nIndex<m_nSize );
		return openWith[nIndex];
	}

	void CRideApp::CRecentFileListEx::Add(LPCTSTR lpszPathName,CDos::PCProperties dosProps){
		// add the file to the list
		ASSERT( lpszPathName );
		ASSERT( dosProps!=nullptr ); // use Properties of the Unknown DOS if automatic recognition should be used
		int i=0;
		while (i<m_nSize)
			if (m_arrNames[i].CompareNoCase(lpszPathName))
				i++;
			else
				break;
		TCHAR fileName[MAX_PATH];
		__super::Add( ::lstrcpy(fileName,lpszPathName) ); // handing over a copy as MFC may (for some reason) corrupt the original string
		::memmove( openWith+1, openWith, i*sizeof(openWith[0]) );
		openWith[0]=dosProps;
	}

	void CRideApp::CRecentFileListEx::Remove(int nIndex){
		// removes the MRU file under the specified Index
		ASSERT( 0<=nIndex && nIndex<m_nSize );
		__super::Remove(nIndex);
		::memmove( openWith+nIndex, openWith+nIndex+1, (m_nSize-nIndex-1)*sizeof(openWith[0]) );
		openWith[m_nSize-1]=&CUnknownDos::Properties;
	}

	#define PREFIX_MRU_DOS _T("Dos")

	void CRideApp::CRecentFileListEx::ReadList(){
		// reads the MRU files list
		__super::ReadList();
		TCHAR entryName[200];
		::lstrcpy( entryName, PREFIX_MRU_DOS );
		for( int iMru=0; iMru<m_nSize; iMru++ ){
			openWith[iMru]=&CUnknownDos::Properties; // assumption (automatic recognition should be used)
			::wsprintf( entryName+(sizeof(PREFIX_MRU_DOS)/sizeof(TCHAR)-1), m_strEntryFormat, iMru+1 );
			const CDos::TId dosId=app.GetProfileInt( m_strSectionName, entryName, 0 );
			for( POSITION pos=CDos::known.GetHeadPosition(); pos; ){
				const CDos::PCProperties props=(CDos::PCProperties)CDos::known.GetNext(pos);
				if (props->id==dosId){
					openWith[iMru]=props;
					break;
				}
			}
		}
	}

	void CRideApp::CRecentFileListEx::WriteList(){
		// writes the MRU files list
		__super::WriteList();
		TCHAR entryName[200];
		::lstrcpy( entryName, PREFIX_MRU_DOS );
		for( int iMru=0; iMru<m_nSize; iMru++ ){
			::wsprintf( entryName+(sizeof(PREFIX_MRU_DOS)/sizeof(TCHAR)-1), m_strEntryFormat, iMru+1 );
			if (!m_arrNames[iMru].IsEmpty())
				app.WriteProfileInt( m_strSectionName, entryName, openWith[iMru]->id );
		}
	}








	CRideApp app;

	#define INI_MSG_OPEN_AS		_T("msgopenas")
	#define INI_MSG_READONLY	_T("msgro")
	#define INI_MSG_FAQ			_T("1stfaq")

	#define INI_CRASHED			_T("crash")

	CLIPFORMAT CRideApp::cfDescriptor;
	CLIPFORMAT CRideApp::cfRideFileList;
	CLIPFORMAT CRideApp::cfContent;
	CLIPFORMAT CRideApp::cfPreferredDropEffect;
	CLIPFORMAT CRideApp::cfPerformedDropEffect;
	CLIPFORMAT CRideApp::cfPasteSucceeded;



	BEGIN_MESSAGE_MAP(CRideApp,CWinApp)
		ON_COMMAND(ID_FILE_NEW,__createNewImage__)
		ON_COMMAND(ID_FILE_OPEN,__openImage__)
		ON_COMMAND_RANGE(ID_FILE_MRU_FIRST,ID_FILE_MRU_LAST,OnOpenRecentFile)
		ON_COMMAND(ID_OPEN_AS,__openImageAs__)
		ON_COMMAND(ID_APP_ABOUT,__showAbout__ )
	END_MESSAGE_MAP()








	static BOOL __stdcall compareWindowClassNameWithRide(HWND hWnd,LPARAM pnAppsRunning){
		TCHAR buf[200];
		::GetClassName( hWnd, buf, sizeof(buf) );
		*(PINT)pnAppsRunning+=::lstrcmpi(buf,APP_CLASSNAME)==0;
		return TRUE;
	}

	BOOL CRideApp::InitInstance(){
		// application initialization
		if (!__super::InitInstance())
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
		// - registering recognizable Image types and known DOSes (in alphabetical order)
		CImage::known.AddTail( (PVOID)&D80::Properties );
		CImage::known.AddTail( (PVOID)&CDsk5::Properties );
		CImage::known.AddTail( (PVOID)&MBD::Properties );
		CImage::known.AddTail( (PVOID)&CMGT::Properties );
		CImage::known.AddTail( (PVOID)&CImageRaw::Properties );
		CImage::known.AddTail( (PVOID)&CSCL::Properties );
		CImage::known.AddTail( (PVOID)&TRD::Properties );
		CDos::known.AddTail( (PVOID)&CBSDOS308::Properties );
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
			::lstrcpy( _tcsrchr(pIniFileName,'\\'), _T("\\") APP_ABBREVIATION _T(".INI") );
			m_pszProfileName=pIniFileName;
		}else // for Windows Vista and newer ...
			SetRegistryKey(APP_FULLNAME); // ... list is stored to and read from system register (as INI files need explicit administrator rights)
		LoadStdProfileSettings();
		if (const CRecentFileList *const pStdMru=m_pRecentFileList){
			// replacing the standard MRU files list with an extended one
			(  m_pRecentFileList=new CRecentFileListEx(*pStdMru)  )->ReadList();
			delete pStdMru;
		}
		// - registering the only document template available in this application
		AddDocTemplate(
			new CMainWindow::CTdiTemplate()
		);
		// - calling "static constructors"
		//nop
		// - displaying the document-view MainWindow
		m_pMainWnd->ShowWindow(SW_SHOW);
		m_pMainWnd->UpdateWindow();
		TDI_INSTANCE->SetFocus(); // explicitly focusing on the TDI View to activate the IntroductoryGuidePost
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
		// - checking whether the app crashed last time
		#ifndef _DEBUG
			const int nAppsReportedRunning=app.GetProfileInt(INI_GENERAL,INI_CRASHED,0);
			int nAppsRunning=0;
			::EnumTaskWindows( 0, compareWindowClassNameWithRide, (LPARAM)&nAppsRunning );
			if (nAppsReportedRunning>=nAppsRunning)
				Utils::Information(_T("If the app CRASHED last time, please reproduce the conditions and report the problem (see Help menu).\n\nThank you and sorry for the inconvenience."));
			app.WriteProfileInt( INI_GENERAL, INI_CRASHED, nAppsRunning ); // assumption (the app has crashed)
		#endif
		// - parsing the command line
		godMode=!::lstrcmpi( __targv[__argc-1], _T("--godmode") );
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
		// - the app hasn't crashed
		app.WriteProfileInt( INI_GENERAL, INI_CRASHED, app.GetProfileInt(INI_GENERAL,INI_CRASHED,1)-1 );
		// - base
		return CWinApp::ExitInstance();
	}

	bool CRideApp::IsInGodMode() const{
		// True <=> the application has been launched with the "--godmode" parameter, otherwise False
		return godMode;
	}

	afx_msg void CRideApp::__createNewImage__(){
		// initiates creation of new Image
		OnFileNew(); // to close any previous Image
		//SaveModified()
		if (!app.m_pMainWnd->IsWindowVisible()) return; // ignoring the initial command sent by MFC immediately after the application has started
		if (!CImage::GetActive()){
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
			if (const TStdWinError err=dos->CreateUserInterface(TDI_HWND)){
				delete image; // ... as it may change here!
				return Utils::FatalError( _T("Cannot access the image"), err );
			}
			image->SetPathName(VOLUME_LABEL_DEFAULT_ANSI_8CHARS,FALSE);
		}
	}




	CDos::PCProperties manuallyForceDos=nullptr; // Null = use automatic recognition

	#define ENTERING_LIMITED_MODE	_T("\n\nContinuing to view the image in limited mode.")

	CDocument *CRideApp::OpenDocumentFile(LPCTSTR lpszFileName){
		// opens document with specified FileName
		app.m_pMainWnd->SetFocus(); // to immediately carry out actions that depend on focus
		// - opening the Image
		std::unique_ptr<CImage> image;
		if (!::lstrcmp(lpszFileName,FDD_A_LABEL)){
			// accessing the floppy in Drive A:
			OnFileNew(); // to close any previous Image
			image.reset( new CFDD );
			if (const TStdWinError err=image->Reset()){
				Utils::FatalError(_T("Cannot access the floppy drive"),err);
				return nullptr;
				//AfxThrowFileException( CFileException::OsErrorToException(err), err, FDD_A_LABEL );
			}
		}else if (image=std::unique_ptr<CImage>((PImage)__super::OpenDocumentFile(lpszFileName))){
			// Image opened successfully
openImage:	if (image->OnOpenDocument(lpszFileName)){ // if opened successfully ...
				if (!image->CanBeModified()) // ... inform on eventual "read-only" mode (forced if Image on the disk is read-only, e.g. because opened from a CD-R)
					Utils::InformationWithCheckableShowNoMore(_T("The image has the Read-only flag set - editing will be disabled."),INI_GENERAL,INI_MSG_READONLY);
			}else
				switch (const TStdWinError err=::GetLastError()){
					case ERROR_BAD_FORMAT:
						if (!dynamic_cast<CImageRaw *>(image.get())){
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
							OnFileNew();
							if (command==IDCANCEL) return nullptr;
							image.reset( CImageRaw::Properties.fnInstantiate() );
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
		//nop (added by calling CDocument::SetPathName below)
		// - determining the DOS
		CDos::PCProperties dosProps=nullptr;
		TFormat formatBoot; // information on Format (# of Cylinders, etc.) obtained from Image's Boot record
		if (!manuallyForceDos){
			// automatic recognition of suitable DOS by sequentially testing each of them
			::SetLastError(ERROR_SUCCESS); // assumption (no errors)
			dosProps=CDos::CRecognition().__perform__( image.get(), &formatBoot );
			if (!dosProps) // if recognition sequence cancelled ...
				return nullptr; // ... no Image or disk is accessed
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
			if (manuallyForceDos!=&CUnknownDos::Properties)
				d.dosProps=manuallyForceDos;
			else if (d.DoModal()!=IDOK)
				return nullptr;
			formatBoot=( dosProps=d.dosProps )->stdFormats->params.format;
			formatBoot.nCylinders++;
			if (const TStdWinError err=image->SetMediumTypeAndGeometry( &formatBoot, CDos::StdSidesMap, d.dosProps->firstSectorNumber )){
				Utils::FatalError( _T("Can't change the medium geometry"), err, _T("The container can't be open.") );
				return nullptr;
			}
			// . informing
			Utils::InformationWithCheckableShowNoMore( _T("The image will be opened using the default format of the selected DOS (see the \"") BOOT_SECTOR_TAB_LABEL _T("\" tab if available).\n\nRISK OF DATA CORRUPTION if the selected DOS and/or format is not suitable!"), INI_GENERAL, INI_MSG_OPEN_AS );
		}
		// - instantiating recognized/selected DOS
		const PDos dos = image->dos = dosProps->fnInstantiate( image.get(), &formatBoot );
		if (!dos)
			return nullptr;
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
		return image.release();
	}

	afx_msg void CRideApp::__openImage__(){
		// opens Image and recognizes suitable DOS automatically
		manuallyForceDos=nullptr; // use automatic recognition
		OnFileOpen();
	}

	afx_msg void CRideApp::__openImageAs__(){
		// opens Image and lets user to determine suitable DOS
		manuallyForceDos=&CUnknownDos::Properties; // show dialog to manually pick a DOS
		if (CMainWindow::CTdiTemplate::pSingleInstance->__closeDocument__()) // to close any previous Image
			OnFileOpen();
	}

	afx_msg void CRideApp::__showAbout__(){
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

	CImage::PCProperties CRideApp::DoPromptFileName(PTCHAR fileName,bool fddAccessAllowed,UINT stdStringId,DWORD flags,CImage::PCProperties singleAllowedImage){
		// reimplementation of CDocManager::DoPromptFileName
		// - creating the list of Filters
		TCHAR buf[500],*a=buf; // an "always big enough" buffer
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
		bool dialogConfirmed;
		if (fddAccessAllowed){
			// . extending the standard Dialog with a control to access local floppy Drive
			d.m_ofn.Flags|= OFN_ENABLETEMPLATE | OFN_EXPLORER;
			d.m_ofn.lpTemplateName=MAKEINTRESOURCE(IDR_OPEN_CUSTOM);
			// . hooking, showing the Dialog, processing its Result, unhooking, and returning the Result
			ofn_fileName=fileName;
			ofn_hHook=::SetWindowsHookEx( WH_CALLWNDPROC, __dlgOpen_hook__, 0, ::GetCurrentThreadId() );
				dialogConfirmed=d.DoModal()==IDOK;
			::UnhookWindowsHookEx(ofn_hHook);
			if (ofn_fileName==FDD_ACCESS)
				::lstrcpy( fileName, FDD_A_LABEL ); // cannot directly write to FileName in the Hook procedure as the "Open/Save File" dialog writes '\0' to the buffer under Windows 7 and higher
				return &CFDD::Properties;
			}
		}else
			dialogConfirmed=d.DoModal()==IDOK;
		if (dialogConfirmed){
			// selected an Image
			const LPCTSTR ext=_tcsrchr(fileName,'.');
			if (singleAllowedImage){
				if (CImage::__determineTypeByExtension__(ext)!=singleAllowedImage) // no or wrong file Extension?
					::strncat( fileName, 1+singleAllowedImage->filter, 4 ); // 1 = asterisk, 4 = dot and three-character extension (e.g. "*.d40")
				return singleAllowedImage;
			}else
				return CImage::__determineTypeByExtension__(ext); // Null <=> unknown container
		}else
			// Dialog cancelled
			return nullptr;
	}
	void CRideApp::OnFileOpen(){
		// public wrapper
		//CWinApp::OnFileOpen();
		TCHAR fileName[MAX_PATH];
		*fileName='\0';
		if (DoPromptFileName( fileName, true, AFX_IDS_OPENFILE, OFN_FILEMUSTEXIST, nullptr )){
			if (const CDocument *const doc=CImage::GetActive())
				if (doc->GetPathName()==fileName) // if attempting to open an already opened Image ...
					return; // ... doing nothing
			OpenDocumentFile(fileName);
		}
	}

	CRideApp::CRecentFileListEx *CRideApp::GetRecentFileList() const{
		// public getter of RecentFileList
		return (CRecentFileListEx *)m_pRecentFileList;
	}

	void WINAPI AfxThrowInvalidArgException(){
		// without this function, we wouldn't be able to build the "MFC 4.2" version!
	}
