#include "stdafx.h"
#include "MDOS2.h"
#include "MSDOS7.h"
#include "TRDOS.h"
#include "GDOS.h"
#include "BSDOS.h"
#include "CapsBase.h"
#include "IPF.h"
#include "CtRaw.h"
#include "KryoFluxBase.h"
#include "KryoFluxStreams.h"
#include "KryoFluxDevice.h"
#include "SuperCardProBase.h"
#include "SCP.h"
#include "Greaseweazle.h"
#include "HFE.h"

	CRideApp::CRecentFileListEx::CRecentFileListEx(const CRecentFileList &rStdMru)
		// ctor
		: CRecentFileList( rStdMru.m_nStart, rStdMru.m_strSectionName, rStdMru.m_strEntryFormat, rStdMru.m_nSize, rStdMru.m_nMaxDisplayLength ) {
		ASSERT( m_nSize<ARRAYSIZE(openWith) );
		ACCEL accels[ARRAYSIZE(openWith)];
		for( int i=0; i<ARRAYSIZE(accels); i++ ){
			ACCEL &r=accels[i];
			r.fVirt=FVIRTKEY|FCONTROL, r.key='1'+i, r.cmd=ID_FILE_MRU_FIRST+i;
		}
		hAccelTable=::CreateAcceleratorTable( accels, ARRAYSIZE(accels) );
	}

	CRideApp::CRecentFileListEx::~CRecentFileListEx(){
		// dtor
		::DestroyAcceleratorTable(hAccelTable);
	}

	CDos::PCProperties CRideApp::CRecentFileListEx::GetDosMruFileOpenWith(int nIndex) const{
		// returns the Properties of the DOS most recently used to open the file under the specified Index (or the Properties of the Unknown DOS if automatic recognition should be used)
		ASSERT( 0<=nIndex && nIndex<m_nSize );
		return openWith[nIndex];
	}

	CImage::PCProperties CRideApp::CRecentFileListEx::GetMruDevice(int nIndex) const{
		// returns the Properties of the Device most recently used to access a real medium
		ASSERT( 0<=nIndex && nIndex<m_nSize );
		return m_deviceProps[nIndex];
	}

	void CRideApp::CRecentFileListEx::Add(LPCTSTR lpszPathName,CDos::PCProperties dosProps,CImage::PCProperties deviceProps){
		// add the file to the list
		ASSERT( lpszPathName );
		// - check whether MRU entry already exists
		int i=0;
		while (i<m_nSize-1)
			if (m_arrNames[i].CompareNoCase(lpszPathName))
				i++;
			else
				break;
		// - move MRU entries before this one down
		const CString fileNameCopy=lpszPathName; // creating a copy as MFC may (for some reason) corrupt the original string
		for( int j=i; j>0; j-- )
			m_arrNames[j]=m_arrNames[j-1];
		::memmove( openWith+1, openWith, i*sizeof(openWith[0]) );
		::memmove( m_deviceProps+1, m_deviceProps, i*sizeof(m_deviceProps[0]) );
		// - place this one at the beginning
		m_arrNames[0]=fileNameCopy;
		openWith[0]=dosProps;
		m_deviceProps[0]=deviceProps;
	}

	void CRideApp::CRecentFileListEx::Remove(int nIndex){
		// removes the MRU file under the specified Index
		ASSERT( 0<=nIndex && nIndex<m_nSize );
		__super::Remove(nIndex);
		::memmove( openWith+nIndex, openWith+nIndex+1, (m_nSize-nIndex-1)*sizeof(openWith[0]) );
		openWith[m_nSize-1]=nullptr;
		::memmove( m_deviceProps+nIndex, m_deviceProps+nIndex+1, (m_nSize-nIndex-1)*sizeof(m_deviceProps[0]) );
		m_deviceProps[m_nSize-1]=nullptr;
	}

	#define PREFIX_MRU_DOS		_T("Dos")
	#define PREFIX_MRU_DEVICE	_T("Dev")

	void CRideApp::CRecentFileListEx::ReadList(){
		// reads the MRU files list
		__super::ReadList();
		TCHAR entryName[200];
		for( int iMru=0; iMru<m_nSize; iMru++ ){
			// . explicitly forced DOS
			openWith[iMru]=nullptr; // assumption (automatic recognition should be used)
			::wsprintf( ::lstrcpy(entryName,PREFIX_MRU_DOS)+(ARRAYSIZE(PREFIX_MRU_DOS)-1), m_strEntryFormat, iMru+1 );
			const CDos::TId dosId=app.GetProfileInt( m_strSectionName, entryName, 0 );
			if (dosId==CUnknownDos::Properties.id)
				openWith[iMru]=&CUnknownDos::Properties;
			else
				for( POSITION pos=CDos::Known.GetHeadPosition(); pos; ){
					const CDos::PCProperties props=CDos::Known.GetNext(pos);
					if (props->id==dosId){
						openWith[iMru]=props;
						break;
					}
				}
			// . real Device
			m_deviceProps[iMru]=nullptr; // assumption (actually an Image, not a real Device)
			::wsprintf( ::lstrcpy(entryName,PREFIX_MRU_DEVICE)+(ARRAYSIZE(PREFIX_MRU_DEVICE)-1), m_strEntryFormat, iMru+1 );
			const CDos::TId devId=app.GetProfileInt( m_strSectionName, entryName, 0 );
			for( POSITION pos=CImage::Devices.GetHeadPosition(); pos; ){
				const CImage::PCProperties props=CImage::Devices.GetNext(pos);
				if (props->id==devId){
					m_deviceProps[iMru]=props;
					break;
				}
			}
		}
	}

	void CRideApp::CRecentFileListEx::WriteList(){
		// writes the MRU files list
		__super::WriteList();
		TCHAR entryName[200];
		for( int iMru=0; iMru<m_nSize; iMru++ )
			if (!m_arrNames[iMru].IsEmpty()){
				// . explicitly forced DOS
				::wsprintf( ::lstrcpy(entryName,PREFIX_MRU_DOS)+(ARRAYSIZE(PREFIX_MRU_DOS)-1), m_strEntryFormat, iMru+1 );
				if (const auto *const forcedDosProps=openWith[iMru])
					app.WriteProfileInt( m_strSectionName, entryName, forcedDosProps->id );
				else
					app.WriteProfileInt( m_strSectionName, entryName, 0 );
				// . real Device
				::wsprintf( ::lstrcpy(entryName,PREFIX_MRU_DEVICE)+(ARRAYSIZE(PREFIX_MRU_DEVICE)-1), m_strEntryFormat, iMru+1 );
				if (m_deviceProps[iMru])
					app.WriteProfileInt( m_strSectionName, entryName, m_deviceProps[iMru]->id );
			}
	}








	CRideApp app;

	#define INI_MSG_OPEN_AS		_T("msgopenas")
	#define INI_MSG_READONLY	_T("msgro")
	#define INI_MSG_FAQ			_T("1stfaq")
	#define INI_MSG_DEVICE_SHORT _T("devshrt")
	#define INI_MSG_REAL_DEVICES _T("devhint")

	#define INI_CRASHED			_T("crash")

	CLIPFORMAT CRideApp::cfDescriptor;
	CLIPFORMAT CRideApp::cfRideFileList;
	CLIPFORMAT CRideApp::cfContent;
	CLIPFORMAT CRideApp::cfPreferredDropEffect;
	CLIPFORMAT CRideApp::cfPerformedDropEffect;
	CLIPFORMAT CRideApp::cfPasteSucceeded;



	BEGIN_MESSAGE_MAP(CRideApp,CWinApp)
		ON_COMMAND(ID_FILE_NEW,CreateNewImage)
		ON_COMMAND(ID_FILE_OPEN,__openImage__)
		ON_COMMAND_RANGE(ID_FILE_MRU_FIRST,ID_FILE_MRU_LAST,OnOpenRecentFile)
		ON_COMMAND(ID_OPEN_AS,__openImageAs__)
		ON_COMMAND(ID_OPEN_UNKNOWN,OpenImageWithoutDos)
		ON_COMMAND(ID_OPEN_DEVICE,__openDevice__)
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
		static constexpr INITCOMMONCONTROLSEX Icc={ sizeof(Icc), ICC_LINK_CLASS };
		if (!::InitCommonControlsEx(&Icc))
			return FALSE; // will end-up here if running on Windows 2000 or older!
		cfDescriptor=::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW);
		cfRideFileList=::RegisterClipboardFormat( _T("Ride") CFSTR_FILEDESCRIPTORW );
		cfContent=::RegisterClipboardFormat(CFSTR_FILECONTENTS);
		cfPreferredDropEffect=::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
		cfPerformedDropEffect=::RegisterClipboardFormat(CFSTR_PERFORMEDDROPEFFECT);
		cfPasteSucceeded=::RegisterClipboardFormat(CFSTR_PASTESUCCEEDED);
		propGridWndClass=PropGrid::RegisterWindowClass( m_hInstance );
		// - registering recognizable Image types and known DOSes (in alphabetical order)
		CImage::Known.AddTail( &CCtRaw::Properties );
		CImage::Known.AddTail( &D80::Properties );
		CImage::Known.AddTail( &CDsk5::Properties );
		CImage::Known.AddTail( &CHFE::Properties );
		CImage::Known.AddTail( &CIpf::Properties );
		CImage::Known.AddTail( &CKryoFluxStreams::Properties );
		CImage::Known.AddTail( &MBD::Properties );
		CImage::Known.AddTail( &MGT::Properties );
		CImage::Known.AddTail( &CImageRaw::Properties );
		CImage::Known.AddTail( &CSCL::Properties );
		CImage::Known.AddTail( &CSCP::Properties );
		CImage::Known.AddTail( &TRD::Properties );
		CImage::Devices.AddTail( &CFDD::Properties );
		CImage::Devices.AddTail( &CGreaseweazleV4::Properties );
		CImage::Devices.AddTail( &CKryoFluxDevice::Properties );
		#ifdef _DEBUG
			CImage::Devices.AddTail( &CDsk5::CDummyDevice::Properties );
		#endif
		CDos::Known.AddTail( &CBSDOS308::Properties );
		CDos::Known.AddTail( &CGDOS::Properties );
		CDos::Known.AddTail( &CMDOS2::Properties );
		CDos::Known.AddTail( &CMSDOS7::Properties );
		CDos::Known.AddTail( &CTRDOS503::Properties );
		CDos::Known.AddTail( &CTRDOS504::Properties );
		CDos::Known.AddTail( &CTRDOS505::Properties );
		// - restoring Most Recently Used (MRU) file Images
		if (!Utils::IsVistaOrNewer()){ // for Windows XP and older ...
			delete m_pszProfileName; // ... list is stored to and read from the INI file in application's folder
			struct TTmp sealed{ TCHAR profile[MAX_PATH]; }; // encapsulating the array into a structure - because MFC4.2 doesn't know the "new TCHAR[MAX_PATH]" operator!
			PTCHAR pIniFileName=(new TTmp)->profile;
			::GetModuleFileName(0,pIniFileName,MAX_PATH);
			::lstrcpy( _tcsrchr(pIniFileName,'\\'), _T("\\") _T(APP_ABBREVIATION) _T(".INI") );
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
		dateRecencyLastChecked=app.GetProfileInt( INI_GENERAL, INI_IS_UP_TO_DATE, 0 ); // 0 = recency not yet automatically checked online
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
		for( POSITION pos=CDos::Known.GetHeadPosition(); pos; )
			if (!CDos::CRecognition().GetOrderIndex(CDos::Known.GetNext(pos))){
				// found a DOS that's not recorded in the profile - displaying the dialog to confirm its recognition
				GetMainWindow()->EditAutomaticRecognitionSequence();
				break;
			}
		// - suggesting to visit the FAQ page to learn more about the application
		if (!app.GetProfileBool(INI_GENERAL,INI_MSG_FAQ)){
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
		return __super::ExitInstance();
	}

	bool CRideApp::GetProfileBool(LPCTSTR sectionName,LPCTSTR keyName,bool bDefault){
		return GetProfileInt( sectionName, keyName, bDefault )!=0;
	}

	bool CRideApp::IsInGodMode() const{
		// True <=> the application has been launched with the "--godmode" parameter, otherwise False
		return godMode;
	}

	afx_msg void CRideApp::CreateNewImage(){
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
			const PImage image=d.fnImage(d.deviceName);
			CMainWindow::CTdiTemplate::pSingleInstance->AddDocument(image); // for the CImage::GetActive function to work
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
			dos = image->dos = CDos::CRecognition().Perform(image,&formatBoot)->fnInstantiate(image,&formatBoot);
			image->SetMediumTypeAndGeometry( &formatBoot, dos->sideMap, dos->properties->firstSectorNumber );
			// . creating the user interface for recognized DOS
			CMainWindow::CTdiTemplate::pSingleInstance->RemoveDocument(image); // added back in CreateUserInterface below
			if (const TStdWinError err=dos->CreateUserInterface(TDI_HWND)){
				delete image;
				return Utils::FatalError( _T("Cannot access the image"), err );
			}
			image->SetPathName(
				image->properties->IsRealDevice() ? d.deviceName : _T(VOLUME_LABEL_DEFAULT_ANSI_8CHARS),
				FALSE
			);
		}
	}




	CImage::PCProperties imageProps=nullptr; // Null = an Image, not a real Device
	CDos::PCProperties manuallyForceDos=nullptr; // Null = use automatic recognition

	CDocument *CRideApp::OpenDocumentFile(LPCTSTR lpszFileName){
		// opens document with specified FileName
		app.m_pMainWnd->SetFocus(); // to immediately carry out actions that depend on focus
		// - opening the Image
		std::unique_ptr<CImage> image;
		const CImage::PCProperties devProps=imageProps!=nullptr && imageProps->IsRealDevice()
											? imageProps
											: nullptr;
		imageProps=nullptr; // information consumed, it's invalid in next call of this method
		if (devProps){
			// accessing a local real Device
			OnFileNew(); // to close any previous Image
			image.reset( devProps->fnInstantiate(lpszFileName) );
			TStdWinError err=ERROR_CANCELLED;
			if (!image->EditSettings(true)
				||
				( err=image->Reset() )
			){
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
							static constexpr Utils::CSimpleCommandDialog::TCmdButtonInfo CmdButtons[]={
								{ IDYES, _T("Open at least valid part of it") },
								{ IDNO, _T("Try to open it as a raw sector image") }
							};
							// . showing the Dialog and processing its result
							const BYTE command=Utils::CSimpleCommandDialog(
								_T("The image seems to be malformatted."),
								CmdButtons, ARRAYSIZE(CmdButtons), _T("Don't open it")
							).DoModal();
							if (command==IDYES) break;
							OnFileNew();
							if (command==IDCANCEL) return nullptr;
							image.reset( CImageRaw::Properties.fnInstantiate(nullptr) ); // Null as buffer = one Image represents only one "device" whose name is known at compile-time
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
		image->SetPathName( lpszFileName, FALSE ); // at this moment, Image became application's active document and the name of its underlying file is shown in MainWindow's caption
		// - adding file Image into list of Most Recently Used (MRU) documents
		//nop (added by calling CDocument::SetPathName below)
		// - determining the DOS
		CDos::PCProperties dosProps=nullptr;
		TFormat formatBoot=TFormat::Unknown; // information on Format (# of Cylinders, etc.) obtained from Image's Boot record
		if (!manuallyForceDos)
			// automatic recognition of suitable DOS by sequentially testing each of them
			do{
				::SetLastError(ERROR_SUCCESS); // assumption (no errors)
				dosProps=CDos::CRecognition().Perform( image.get(), &formatBoot );
				if (!dosProps) // if recognition sequence cancelled ...
					return nullptr; // ... no Image or disk is accessed
				if (!dosProps->IsKnown()){
					static constexpr Utils::CSimpleCommandDialog::TCmdButtonInfo CmdButtons[]={
						{ IDCONTINUE, _T("Continue without DOS") },
						{ IDRETRY, _T("Retry (recommended)") },
						{ IDTRYAGAIN, _T("Revise recognition sequence, and retry") },
						{ IDNO, _T("Manually select DOS") }
					};
					switch (
						Utils::CSimpleCommandDialog(
							_T("CANNOT RECOGNIZE THE DOS!\nDoes it participate in recognition?"),
							CmdButtons, ARRAYSIZE(CmdButtons)
						).DoModal()
					){
						case IDCANCEL:
							return nullptr;
						case IDNO:
							manuallyForceDos=(CDos::PCProperties)INVALID_HANDLE_VALUE;
							break;
						case IDTRYAGAIN:
							CDos::CRecognition::EditSequence();
							//fallthrough
						case IDRETRY:
							for( TCylinder cyl=image->GetCylinderCount(); cyl-->0; )
								for( THead head=image->GetHeadCount(); head-->0; )
									image->UnscanTrack( cyl, head );
							continue;
					}
				}
				break;
			}while (true);
		if (manuallyForceDos){ // testing again for the flag could have been changed above
			// manual recognition of suitable DOS by user
			// . defining the Dialog
			class CDosSelectionDialog sealed:public Utils::CRideDialog{
				BOOL OnInitDialog() override{
					// initialization
					// - base
					const BOOL result=__super::OnInitDialog();
					// - populating the list of known DOSes
					CListBox lb;
					lb.Attach(GetDlgItemHwnd(ID_DOS));
						lb.SetItemDataPtr( lb.AddString(_T("[ Open without DOS ]")), (PVOID)&CUnknownDos::Properties );
						for( POSITION pos=CDos::Known.GetHeadPosition(); pos; ){
							const CDos::PCProperties p=CDos::Known.GetNext(pos);
							lb.SetItemDataPtr( lb.AddString(p->name), (PVOID)p );
						}
					lb.Detach();
					return result;
				}
				void CDosSelectionDialog::DoDataExchange(CDataExchange *pDX){
					// exchange of data from and to controls
					DDX_LBIndex( pDX, ID_DOS	,(int &)dosProps );
					if (pDX->m_bSaveAndValidate){
						CListBox lb;
						lb.Attach(GetDlgItemHwnd(ID_DOS));
							dosProps=(CDos::PCProperties)lb.GetItemDataPtr((int)dosProps);
						lb.Detach();
					}
				}
				BOOL OnCommand(WPARAM wParam,LPARAM lParam){
					// command processing
					switch (wParam){
						case MAKELONG(ID_DOS,LBN_SELCHANGE):
							EnableDlgItem( IDOK, GetDlgListBoxSelectedIndex(ID_DOS)>=0 );
							break;
						case MAKELONG(ID_DOS,LBN_DBLCLK):
							// DOS selected by double-clicking on it
							if (IsDlgItemEnabled(IDOK))
								SendMessage( WM_COMMAND, IDOK );
							break;
					}
					return __super::OnCommand(wParam,lParam);
				}
			public:
				CDos::PCProperties dosProps;

				CDosSelectionDialog()
					// ctor
					: Utils::CRideDialog(IDR_DOS_UNKNOWN) , dosProps(nullptr) {
				}
			} d;
			// . showing the Dialog and processing its result
			if (manuallyForceDos!=(CDos::PCProperties)INVALID_HANDLE_VALUE) // DOS already pre-selected? (e.g. recent file)
				d.dosProps=manuallyForceDos;
			else if (d.DoModal()!=IDOK)
				return nullptr;
			formatBoot=( dosProps=manuallyForceDos=d.dosProps )->stdFormats->params.format;
			formatBoot.nCylinders++;
			Medium::TType mt;
			switch (const TStdWinError err=image->GetInsertedMediumType(0,mt)){
				case ERROR_SUCCESS: // particular Medium inserted (e.g. KryoFlux Stream files)
					formatBoot.mediumType=mt;
					break;
				case ERROR_NO_MEDIA_IN_DRIVE: // no specific medium inserted (e.g. raw images)
					break;
				default:
					Utils::FatalError( _T("Can't recognize the medium"), err, _T("The disk can't be open.") );
					return nullptr;
			}
			if (const TStdWinError err=image->SetMediumTypeAndGeometry( &formatBoot, CDos::StdSidesMap, d.dosProps->firstSectorNumber )){
				Utils::FatalError( _T("Can't change the medium geometry"), err, _T("The disk can't be open.") );
				return nullptr;
			}
			// . informing
			if (dosProps->IsKnown()) // a damage can't occur in Unknown DOS
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
			image.release(); // leaving Image disposal upon TdiTemplate ...
			CMainWindow::CTdiTemplate::pSingleInstance->__closeDocument__(); // ... here
			return nullptr;
		}
		// - informing on what to do in case of DOS misrecognition
		Utils::InformationWithCheckableShowNoMore( _T("If the DOS has been misrecognized, adjust the recognition sequence under \"Disk -> Recognition\"."), INI_GENERAL, INI_MISRECOGNITION );
		// - returning the just open Image
		return image.release();
	}

	afx_msg void CRideApp::__openImage__(){
		// opens Image and recognizes suitable DOS automatically
		manuallyForceDos=nullptr; // use automatic recognition
		OnFileOpen();
	}

	static CImage::PCProperties ChooseLocalDevice(PTCHAR pOutDeviceName){
		const PCImage image=CImage::GetActive();
		CRealDeviceSelectionDialog d( image?image->dos->properties:&CUnknownDos::Properties );
		if (d.DoModal()==IDOK){
			::lstrcpy( pOutDeviceName, d.deviceName ); // cannot directly write to FileName in the Hook procedure as the "Open/Save File" dialog writes '\0' to the buffer under Windows 7 and higher
			return d.deviceProps;
		}else
			return nullptr;
	}

	afx_msg void CRideApp::__openDevice__(){
		// opens access to a local real device
		//Utils::InformationWithCheckableShowNoMore( _T("This is a shorthand to access local devices. The longer way via \"Open\" and \"Open as\" commands may offer more possibilities."), INI_GENERAL, INI_MSG_DEVICE_SHORT );
		manuallyForceDos=nullptr; // use automatic recognition
		TCHAR deviceName[MAX_PATH];
		*deviceName='\0';
		if (imageProps=ChooseLocalDevice( deviceName )){
			if (const CDocument *const doc=CImage::GetActive())
				if (doc->GetPathName()==deviceName) // if attempting to open an already opened Image ...
					return; // ... doing nothing
			if (OpenDocumentFile(deviceName))
				return;
		}else if (*deviceName){
			const TStdWinError err=::GetLastError();
			Utils::FatalError( _T("Can't access device"), err?err:ERROR_DEVICE_NOT_AVAILABLE );
		}
	}

	afx_msg void CRideApp::__openImageAs__(){
		// opens Image and lets user to determine suitable DOS
		manuallyForceDos=(CDos::PCProperties)INVALID_HANDLE_VALUE; // show dialog to manually pick a DOS
		if (CMainWindow::CTdiTemplate::pSingleInstance->__closeDocument__()) // to close any previous Image
			OnFileOpen();
	}

	afx_msg void CRideApp::OpenImageWithoutDos(){
		// opens Image using Unknown ("no") DOS
		manuallyForceDos=&CUnknownDos::Properties; // use this DOS for opening
		if (CMainWindow::CTdiTemplate::pSingleInstance->__closeDocument__()) // to close any previous Image
			OnFileOpen();
	}

	afx_msg void CRideApp::__showAbout__(){
		// about
		TCHAR buf[80];
		::wsprintf( buf, _T("Version ") _T(APP_VERSION) _T("\n\ntomascz, 2015-%d"), Utils::CRideTime().wYear );
		Utils::Information(buf);
	}




	

	// CAPITALS to close the hooked dialog by IDOK (mean it Save or Open), otherwise by IDCANCEL
	#define SHORTCUT_DEVICES			_T("dev")
	#define SHORTCUT_KRYOFLUX_STREAMS	_T("KFS")

	static HHOOK ofn_hHook;
	static HHOOK ofn_hMsgHook;
	static WCHAR ofn_shortcut[4]; // long enough to accommodate any of the above 3-char Shortcuts
	static CString ofn_shortcutHint;

	#define WM_SHOW_HINT	WM_USER+1

	static LRESULT CALLBACK __dlgOpen_hook__(int code,WPARAM wParam,LPARAM lParam){
		// hooking the "Open/Save File" dialogs
		const LPCWPSTRUCT pcws=(LPCWPSTRUCT)lParam;
		if (pcws->message==WM_INITDIALOG){
			::SetDlgItemText( pcws->hwnd, ID_DRIVEA, ofn_shortcutHint );
			if (const HWND hComment=::GetDlgItem( pcws->hwnd, ID_COMMENT )){
				// initializing the customized part of the dialog
				Utils::CRideDialog::SetDlgItemSingleCharUsingFont(
					pcws->hwnd, ID_COMMENT,
					L'\xf0e8', (HFONT)Utils::CRideFont(FONT_WINGDINGS,190,false,true).Detach() // a thick arrow right
				);
				::PostMessage( hComment, WM_SHOW_HINT, 0, (LPARAM)INI_MSG_REAL_DEVICES );
			}
		}else if (pcws->message==WM_NOTIFY)
			switch (Utils::CRideDialog::GetClickedHyperlinkId(pcws->lParam)){
				case ID_DRIVEA:{
					const HWND hDlg=::GetParent(pcws->hwnd);
					if (const HWND hFileNameComboBox=::GetDlgItem( hDlg, 0x47c )){ // ID obtained via Spy++
						::SetWindowTextW( // manually set the file name
							hFileNameComboBox,
							::lstrcpyW( ofn_shortcut, ((PNMLINK)pcws->lParam)->item.szID )
						);
						return ::SendMessageW( hDlg, WM_COMMAND,
							::IsCharLowerW(*ofn_shortcut) ? IDCANCEL : IDOK, // manually close the dialog
							0
						);
					}
					return 0;
				}
			}
		return ::CallNextHookEx(ofn_hHook,code,wParam,lParam);
	}

	static LRESULT CALLBACK __dlgOpen_msgHook__(int code,WPARAM wParam,LPARAM lParam){
		// hooking the "Open/Save File" dialogs
		PMSG pMsg=(PMSG)lParam;
		if (pMsg->message==WM_SHOW_HINT)
			if (pMsg->lParam==(LPARAM)INI_MSG_REAL_DEVICES)
				if (::IsWindowVisible(pMsg->hwnd)) // if the hyperlink already shown ...
					Utils::InformationWithCheckableShowNoMore( // ... displaying the message ...
						_T("Always see the bottom of this dialog for access to real devices."),
						INI_GENERAL, INI_MSG_REAL_DEVICES
					);
				else
					::PostMessage( pMsg->hwnd, WM_SHOW_HINT, 0, (LPARAM)INI_MSG_REAL_DEVICES ); // ... otherwise waiting until the hyperlink is shown
		return ::CallNextHookEx(ofn_hMsgHook,code,wParam,lParam);
	}

	CImage::PCProperties CRideApp::DoPromptFileName(PTCHAR fileName,bool deviceAccessAllowed,UINT stdStringId,DWORD flags,CImage::PCProperties singleAllowedImage){
		// reimplementation of CDocManager::DoPromptFileName
		ofn_shortcutHint.Empty();
		if (deviceAccessAllowed && !singleAllowedImage)
			ofn_shortcutHint+=_T("<a id=\"") SHORTCUT_DEVICES _T("\">Access real devices on this computer</a> (additional drivers may be required). ");
		if (stdStringId==AFX_IDS_SAVEFILE && (!singleAllowedImage||singleAllowedImage==&CKryoFluxStreams::Properties))
			ofn_shortcutHint+=_T("Continue with <a id=\"") SHORTCUT_KRYOFLUX_STREAMS _T("\">KryoFlux streams</a>. ");
		// - creating the list of Filters
		TCHAR buf[2048],*a=buf; // an "always big enough" buffer
		DWORD nFilters=0;
		if (singleAllowedImage)
			// list of Filters consists of only one item
			::wsprintf( buf, _T("%s (%s)|%s|"), singleAllowedImage->fnRecognize(nullptr), singleAllowedImage->filter, singleAllowedImage->filter ); // Null as buffer = one Image represents only one "device" whose name is known at compile-time
		else{
			// list of Filters consists of all recognizable Images
			// . all known Images
			a+=_stprintf( a, _T("All known images|") );
			for( POSITION pos=CImage::Known.GetHeadPosition(); pos; )
				a+=_stprintf( a, _T("%s;"), CImage::Known.GetNext(pos)->filter );
			*(a-1)='|'; // replacing semicolon with pipe '|'
			// . individual Images by extension
			for( POSITION pos=CImage::Known.GetHeadPosition(); pos; nFilters++ ){
				const CImage::PCProperties p=CImage::Known.GetNext(pos);
				a+=_stprintf( a, _T("%s (%s)|%s|"), p->fnRecognize(nullptr), p->filter, p->filter ); // Null as buffer = one Image represents only one "device" whose name is known at compile-time
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
		if (!ofn_shortcutHint.IsEmpty()){
			// . extending the standard Dialog with a control to access local devices
			d.m_ofn.Flags|= OFN_ENABLETEMPLATE | OFN_EXPLORER;
			d.m_ofn.lpTemplateName=MAKEINTRESOURCE(IDR_OPEN_CUSTOM);
			// . hooking, showing the Dialog, processing its Result, unhooking, and returning the Result
			*ofn_shortcut=L'\0';
			ofn_hHook=::SetWindowsHookEx( WH_CALLWNDPROC, __dlgOpen_hook__, 0, ::GetCurrentThreadId() );
				ofn_hMsgHook=::SetWindowsHookEx( WH_GETMESSAGE, __dlgOpen_msgHook__, 0, ::GetCurrentThreadId() );
					dialogConfirmed=d.DoModal()==IDOK || *ofn_shortcut;
				::UnhookWindowsHookEx(ofn_hMsgHook);
			::UnhookWindowsHookEx(ofn_hHook);
			const CDos::CPathString shortcut(ofn_shortcut);
			if (!::lstrcmp(shortcut,SHORTCUT_DEVICES)) // want access real device?
				return ChooseLocalDevice(fileName);
			if (!::lstrcmp(shortcut,SHORTCUT_KRYOFLUX_STREAMS)) // want save as KryoFlux Streams?
				if (const LPTSTR lastBackslash=_tcsrchr(fileName,'\\')){
					::lstrcpy( lastBackslash+1, _T("track00.0.raw") );
					return &CKryoFluxStreams::Properties;
				}
		}else
			dialogConfirmed=d.DoModal()==IDOK;
		if (dialogConfirmed){
			// selected an Image
			if (singleAllowedImage){
				if (CImage::DetermineType(fileName)!=singleAllowedImage) // not the expected Image type?
					if (const LPCTSTR separator=::StrChr(singleAllowedImage->filter,*IMAGE_FORMAT_SEPARATOR)) // are there more than one possible extension
						::StrNCat( fileName, 1+singleAllowedImage->filter, separator-singleAllowedImage->filter ); // 1 = asterisk
					else
						::lstrcat( fileName, 1+singleAllowedImage->filter ); // 1 = asterisk
				return singleAllowedImage;
			}else
				return CImage::DetermineType(fileName); // Null <=> unknown container
		}else
			// Dialog cancelled
			return nullptr;
	}
	void CRideApp::OnFileOpen(){
		// public wrapper
		//__super::OnFileOpen();
		TCHAR fileName[MAX_PATH];
		*fileName='\0';
		if (imageProps=DoPromptFileName( fileName, true, AFX_IDS_OPENFILE, OFN_FILEMUSTEXIST, nullptr )){
			if (const CDocument *const doc=CImage::GetActive())
				if (doc->GetPathName()==fileName) // if attempting to open an already opened Image ...
					return; // ... doing nothing
			if (OpenDocumentFile(fileName))
				return;
		}else if (*fileName){
			const TStdWinError err=::GetLastError();
			Utils::FatalError( _T("Can't open the file"), err?err:MK_E_INVALIDEXTENSION );
		}
	}

	CRideApp::CRecentFileListEx *CRideApp::GetRecentFileList() const{
		// public getter of RecentFileList
		return (CRecentFileListEx *)m_pRecentFileList;
	}

	HWND CRideApp::GetEnabledActiveWindow() const{
		// returns currently focused window, regardless of which thread created it
		GUITHREADINFO gti={ sizeof(gti) };
		if (::GetGUIThreadInfo(::GetCurrentThreadId(),&gti) && gti.hwndActive && ::IsWindowEnabled(gti.hwndActive))
			// current thread has created a GUI; disabled windows ignored (as mustn't parent any pop-up windows)
			return gti.hwndActive;
		else if (m_pMainWnd!=nullptr && ::GetGUIThreadInfo(::GetWindowThreadProcessId(*m_pMainWnd,nullptr),&gti) && gti.hwndActive && ::IsWindowEnabled(gti.hwndActive))
			// the main thread has (still/already) some GUI; disabled windows ignored (as mustn't parent any pop-up windows)
			return gti.hwndActive;
		else
			// no known GUI exists
			return ::GetActiveWindow();
	}

	void WINAPI AfxThrowInvalidArgException(){
		// without this function, we wouldn't be able to build the "MFC 4.2" version!
	}


#ifdef RELEASE_MFC42
	void __cdecl operator delete(PVOID ptr, UINT sz) noexcept {
		operator delete(ptr);
	}

	void __cdecl _invalid_parameter_noinfo_noreturn(void) {
	}
#endif
