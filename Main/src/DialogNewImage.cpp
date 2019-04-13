#include "stdafx.h"

	CNewImageDialog::CNewImageDialog()
		// ctor
		: CDialog(IDR_IMAGE_NEW)
		, fnImage(NULL) , dosProps(NULL) {
	}








	BOOL CNewImageDialog::OnInitDialog(){
		// dialog initialization
		// - base
		CDialog::OnInitDialog();
		// - positioning and scaling the Error box to match the Image list-box
		RECT rc;
		GetDlgItem(ID_IMAGE)->GetWindowRect(&rc);
		ScreenToClient(&rc);
		GetDlgItem(ID_ERROR)->SetWindowPos( NULL, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER );
		// - populating the list of available DOSes
		CListBox lb;
		lb.Attach(GetDlgItem(ID_DOS)->m_hWnd);
			lb.SetItemDataPtr( lb.AddString(_T("-- Select --")), (PVOID)&CUnknownDos::Properties );
			for( POSITION pos=CDos::known.GetHeadPosition(); pos; ){
				const CDos::PCProperties p=(CDos::PCProperties)CDos::known.GetNext(pos);
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
			if (imageProps->sectorLengthMin<=dosStdSectorLength && dosStdSectorLength<=imageProps->sectorLengthMax)
				rLBImage.SetItemDataPtr( rLBImage.AddString(imageProps->name), (PVOID)imageProps );
	}

	void CNewImageDialog::__refreshListOfContainers__(){
		// forces refreshing of the list of containers available for the selected DOS; eventually displays an error message
		GetDlgItem(IDOK)->EnableWindow(FALSE); // need yet to select a container
		CWnd *const pImageListBox=GetDlgItem(ID_IMAGE), *const pErrorBox=GetDlgItem(ID_ERROR);
		// - checking if the selected DOS is among those which participate in the automatic recognition sequence
		if (dosProps){
			// some DOS selected
			const CDos::CRecognition recognition;
			for( POSITION pos=recognition.__getFirstRecognizedDosPosition__(); pos; )
				if (recognition.__getNextRecognizedDos__(pos)==dosProps){
					// yes, DOS participates in the automatic recognition sequence
					CListBox lb;
					lb.Attach(pImageListBox->m_hWnd);
						lb.ShowWindow(SW_SHOW), pErrorBox->ShowWindow(SW_HIDE);
						lb.ResetContent();
						for( POSITION pos=CImage::known.GetHeadPosition(); pos; )
							__checkCompatibilityAndAddToOptions__( dosProps, lb, (CImage::PCProperties)CImage::known.GetNext(pos) );
						__checkCompatibilityAndAddToOptions__( dosProps, lb, &CFDD::Properties );
					lb.Detach();
					return;
				}
		}
		// - the selected DOS doesn't participate in the automatic recognition sequence
		pErrorBox->ShowWindow(SW_SHOW), pImageListBox->ShowWindow(SW_HIDE);
		TCHAR errMsg[200];
		if (dosProps)
			::wsprintf( errMsg, _T("Can't create a disk of \"%s\" as it doesn't participate in automatic recognition.\n\n<a>Change the recognition sequence</a>"), dosProps->name );
		else
			::wsprintf( errMsg, _T("Select a DOS first") );
		SetDlgItemText( ID_ERROR, errMsg );
		dosProps=NULL;
	}

	BOOL CNewImageDialog::OnCommand(WPARAM wParam,LPARAM lParam){
		// command processing
		switch (wParam){
			case MAKELONG(ID_DOS,LBN_SELCHANGE):{ // DOS selection changed
				// another DOS selected - refreshing the list of Images that comply with DOS requirements
				CListBox lb;
				lb.Attach((HWND)lParam);
					const int iSelected=lb.GetCurSel();
					dosProps= iSelected>0 ? (CDos::PCProperties)lb.GetItemData(iSelected) : NULL;
				lb.Detach();
				__refreshListOfContainers__();
				return TRUE;
			}
			case MAKELONG(ID_IMAGE,LBN_SELCHANGE):{ // Image selection changed
				// another Image selected - enabling the OK button
				CListBox lb;
				lb.Attach((HWND)lParam);
					const int iSelected=lb.GetCurSel();
					static const WORD Controls[]={ IDOK, 0 };
					if (Utils::EnableDlgControls( m_hWnd, Controls, iSelected>=0 ))
						fnImage=( (CImage::PCProperties)lb.GetItemData(iSelected) )->fnInstantiate;
				lb.Detach();
				return TRUE;
			}
		}
		return CDialog::OnCommand(wParam,lParam);
	}

	BOOL CNewImageDialog::OnNotify(WPARAM wParam,LPARAM lParam,LRESULT *pResult){
		// processes notification
		const LPCWPSTRUCT pcws=(LPCWPSTRUCT)lParam;
		if (pcws->wParam==ID_HELP_INDEX)
			switch (pcws->message){
				case NM_CLICK:
				case NM_RETURN:{
					// . defining the Dialog
					class CHelpDialog sealed:public Utils::CCommandDialog{
						void PreInitDialog() override{
							// dialog initialization
							// : base
							Utils::CCommandDialog::PreInitDialog();
							// : supplying available actions
							__addCommandButton__( ID_DRIVE, _T("Does the application co-work with real floppy drives?") );
							__addCommandButton__( ID_FORMAT, _T("How do I format a real floppy using this application?") );
							__addCommandButton__( ID_SYSTEM, _T("How do I open a real floppy using this application?") );
							__addCommandButton__( IDCANCEL, MSG_HELP_CANCEL );
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
			}
		else if (pcws->wParam==ID_ERROR)
			switch (pcws->message){
				case NM_CLICK:
				case NM_RETURN:{
					((CMainWindow *)app.m_pMainWnd)->__changeAutomaticDiskRecognitionOrder__();
					__refreshListOfContainers__();
					*pResult=0;
					return TRUE;
				}
			}
		return CDialog::OnNotify(wParam,lParam,pResult);
	}
