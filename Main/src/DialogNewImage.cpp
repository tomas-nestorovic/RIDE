#include "stdafx.h"

	CNewImageDialog::CNewImageDialog()
		// ctor
		: Utils::CRideDialog(IDR_IMAGE_NEW)
		, fnImage(nullptr) , dosProps(nullptr) {
	}








	BOOL CNewImageDialog::OnInitDialog(){
		// dialog initialization
		// - base
		__super::OnInitDialog();
		// - positioning and scaling the Error box to match the Image list-box
		SetDlgItemPos( ID_ERROR, MapDlgItemClientRect(ID_IMAGE) );
		// - populating the list of available DOSes
		CListBox lb;
		lb.Attach(GetDlgItemHwnd(ID_DOS));
			lb.SetItemDataPtr( lb.AddString(_T("-- Select --")), (PVOID)&CUnknownDos::Properties );
			for( POSITION pos=CDos::Known.GetHeadPosition(); pos; ){
				const CDos::PCProperties p=CDos::Known.GetNext(pos);
				lb.SetItemDataPtr( lb.AddString(p->name), (PVOID)p );
			}
			lb.SetCurSel(0);
			GotoDlgCtrl(&lb); // focusing the ComboBox with DOSes
		lb.Detach();
		__refreshListOfContainers__();
		// - done
		return FALSE; // False = focus already set manually
	}

	static void __checkCompatibilityAndAddToOptions__(CDos::PCProperties dosProps,CListBox &rLBImage,CImage::PCProperties imageProps){
		// determines if Properties of given DOS and Image are compatible, and in case that yes, adds given Image to options
		const WORD dosStdSectorLength=dosProps->stdFormats->params.format.sectorLength;
		if (imageProps->supportedMedia&dosProps->supportedMedia) // DOS and Image support common Media
			if (imageProps->supportedCodecs&dosProps->supportedCodecs) // DOS and Image support common Codecs
				if (imageProps->sectorLengthMin<=dosStdSectorLength && dosStdSectorLength<=imageProps->sectorLengthMax && !imageProps->isReadOnly)
					rLBImage.SetItemDataPtr( rLBImage.AddString(imageProps->fnRecognize(nullptr)), (PVOID)imageProps ); // Null as buffer = one Image represents only one "device" whose name is known at compile-time
	}

	#define REAL_DEVICE_OPTION_STRING	_T("[ Compatible physical device ]")

	void CNewImageDialog::__refreshListOfContainers__(){
		// forces refreshing of the list of containers available for the selected DOS; eventually displays an error message
		EnableDlgItem( IDOK, false ); // need yet to select a container
		CWnd *const pImageListBox=GetDlgItem(ID_IMAGE);
		// - checking if the selected DOS is among those which participate in the automatic recognition sequence
		if (dosProps){
			// some DOS selected
			const CDos::CRecognition recognition;
			for( POSITION pos=recognition.GetFirstRecognizedDosPosition(); pos; )
				if (recognition.GetNextRecognizedDos(pos)==dosProps){
					// yes, DOS participates in the automatic recognition sequence
					CListBox lb;
					lb.Attach(pImageListBox->m_hWnd);
						lb.ShowWindow(SW_SHOW), ShowDlgItem(ID_ERROR,false);
						lb.ResetContent();
						for( POSITION pos=CImage::Known.GetHeadPosition(); pos; )
							__checkCompatibilityAndAddToOptions__( dosProps, lb, CImage::Known.GetNext(pos) );
						lb.SetItemDataPtr( lb.AddString(REAL_DEVICE_OPTION_STRING), (PVOID)&CFDD::Properties ); // "some" real device
						lb.SetCurSel( lb.FindString(0,dosProps->typicalImage->fnRecognize(nullptr)) ); // Null as buffer = one Image represents only one "device" whose name is known at compile-time
					lb.Detach();
					SendMessage( WM_COMMAND, MAKELONG(ID_IMAGE,LBN_SELCHANGE), (LPARAM)pImageListBox->m_hWnd );
					return;
				}
		}
		// - the selected DOS doesn't participate in the automatic recognition sequence
		ShowDlgItem(ID_ERROR), pImageListBox->ShowWindow(SW_HIDE);
		if (dosProps)
			SetDlgItemFormattedText( ID_ERROR, _T("Can't create a disk of \"%s\" as it doesn't participate in automatic recognition.\n\n<a>Change the recognition sequence</a>"), dosProps->name );
		else
			SetDlgItemText( ID_ERROR, _T("Select a DOS first") );
	}

	BOOL CNewImageDialog::OnCommand(WPARAM wParam,LPARAM lParam){
		// command processing
		switch (wParam){
			case MAKELONG(ID_DOS,LBN_SELCHANGE):{ // DOS selection changed
				// another DOS selected - refreshing the list of Images that comply with DOS requirements
				CListBox lb;
				lb.Attach((HWND)lParam);
					const int iSelected=lb.GetCurSel();
					dosProps= iSelected>0 ? (CDos::PCProperties)lb.GetItemData(iSelected) : nullptr;
				lb.Detach();
				__refreshListOfContainers__();
				return TRUE;
			}
			case MAKELONG(ID_IMAGE,LBN_SELCHANGE):{ // Image selection changed
				// another Image selected - enabling the OK button
				CListBox lb;
				lb.Attach((HWND)lParam);
					const int iSelected=lb.GetCurSel();
					if (EnableDlgItem( IDOK, iSelected>=0 )){
						fnImage=( (CImage::PCProperties)lb.GetItemData(iSelected) )->fnInstantiate;
						lb.GetText( iSelected, deviceName );
					}
				lb.Detach();
				return TRUE;
			}
			case MAKELONG(ID_IMAGE,LBN_DBLCLK):
				// Image selected by double-clicking on it
				if (IsDlgItemEnabled(IDOK))
					SendMessage( WM_COMMAND, IDOK );
				break;
		}
		return __super::OnCommand(wParam,lParam);
	}

	BOOL CNewImageDialog::OnNotify(WPARAM wParam,LPARAM lParam,LRESULT *pResult){
		// processes notification
		switch (GetClickedHyperlinkId(lParam)){
			case ID_HELP_INDEX:{
					// . defining the Dialog
					class CHelpDialog sealed:public Utils::CCommandDialog{
						BOOL OnInitDialog() override{
							// dialog initialization
							// : base
							const BOOL result=__super::OnInitDialog();
							// : supplying available actions
							AddHelpButton( ID_DRIVE, _T("Does the application co-work with real floppy drives?") );
							AddHelpButton( ID_FORMAT, _T("How do I format a real floppy using this application?") );
							AddHelpButton( ID_SYSTEM, _T("How do I open a real floppy using this application?") );
							AddCancelButton( MSG_HELP_CANCEL );
							return result;
						}
					public:
						CHelpDialog()
							// ctor
							: Utils::CCommandDialog(_T("This might interest you:")) {
						}
					} d;
					// . showing the Dialog and processing its result
					TCHAR url[200];
					switch (d.DoModal()){
						case ID_DRIVE:
							Utils::NavigateToUrlInDefaultBrowser( Utils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_realFdd.html"),url) );
							break;
						case ID_FORMAT:
							Utils::NavigateToUrlInDefaultBrowser( Utils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_formatFloppy.html"),url) );
							break;
						case ID_SYSTEM:
							Utils::NavigateToUrlInDefaultBrowser( Utils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_accessFloppy.html"),url) );
							break;
					}
					*pResult=0;
					return TRUE;
			}
			case ID_ERROR:
					app.GetMainWindow()->__changeAutomaticDiskRecognitionOrder__();
					__refreshListOfContainers__();
					*pResult=0;
					return TRUE;
		}
		return __super::OnNotify(wParam,lParam,pResult);
	}

	void CNewImageDialog::OnOK(){
		// Dialog confirmed
		if (::lstrcmp(deviceName,REAL_DEVICE_OPTION_STRING))
			// selected one of Image containers
			__super::OnOK();
		else{
			// selected a real Device option, displaying a dialog to select a real local Device
			ShowWindow(SW_HIDE); // hiding the "New image" dialog to not distract attention from the following subdialog
			CRealDeviceSelectionDialog d(dosProps);
			if (d.DoModal()==IDOK){
				// real Device selected
				fnImage=d.deviceProps->fnInstantiate;
				::lstrcpy( deviceName, d.deviceName );
				__super::OnOK();
			}else
				// real Device selection cancelled
				EndDialog(IDCANCEL);
		}
	}
