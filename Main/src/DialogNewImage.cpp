#include "stdafx.h"

	CNewImageDialog::CNewImageDialog()
		// ctor
		: CDialog(IDR_IMAGE_NEW)
		, fnImage(NULL) , fnDos(NULL) {
	}








	BOOL CNewImageDialog::OnInitDialog(){
		// dialog initialization
		// - base
		CDialog::OnInitDialog();
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
	BOOL CNewImageDialog::OnCommand(WPARAM wParam,LPARAM lParam){
		// command processing
		switch (wParam){
			case MAKELONG(ID_DOS,LBN_SELCHANGE):{ // DOS selection changed
				// another DOS selected - refreshing the list of Images that comply with DOS requirements
				CListBox lb;
				lb.Attach((HWND)lParam);
					const int iSelected=lb.GetCurSel();
					if (iSelected>=0){
						const CDos::PCProperties dosProps=(CDos::PCProperties)lb.GetItemData(iSelected);
						fnDos=dosProps->fnInstantiate;
						lb.Detach();
						lb.Attach(GetDlgItem(ID_IMAGE)->m_hWnd);
						lb.ResetContent();
						GetDlgItem(IDOK)->EnableWindow(FALSE);
						if (iSelected>0){
							for( POSITION pos=CImage::known.GetHeadPosition(); pos; )
								__checkCompatibilityAndAddToOptions__( dosProps, lb, (CImage::PCProperties)CImage::known.GetNext(pos) );
							__checkCompatibilityAndAddToOptions__( dosProps, lb, &CFDD::Properties );
						}
					}
				lb.Detach();
				return TRUE;
			}
			case MAKELONG(ID_IMAGE,LBN_SELCHANGE):{ // Image selection changed
				// another Image selected - enabling the OK button
				CListBox lb;
				lb.Attach((HWND)lParam);
					const int iSelected=lb.GetCurSel();
					if (iSelected>=0){
						fnImage=( (CImage::PCProperties)lb.GetItemData(iSelected) )->fnInstantiate;
						GetDlgItem(IDOK)->EnableWindow(TRUE);
					}else
						GetDlgItem(IDOK)->EnableWindow(FALSE);
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
					class CHelpDialog sealed:public TUtils::CCommandDialog{
						void PreInitDialog() override{
							// dialog initialization
							// : base
							TUtils::CCommandDialog::PreInitDialog();
							// : supplying available actions
							__addCommandButton__( ID_DRIVE, _T("Does the application co-work with real floppy drives?") );
							__addCommandButton__( ID_FORMAT, _T("How do I format a real floppy using this application?") );
							__addCommandButton__( ID_SYSTEM, _T("How do I open a real floppy using this application?") );
							__addCommandButton__( IDCANCEL, MSG_HELP_CANCEL );
						}
					public:
						CHelpDialog()
							// ctor
							: TUtils::CCommandDialog(_T("This might interest you:")) {
						}
					} d;
					// . showing the Dialog and processing its result
					TCHAR url[200];
					switch (d.DoModal()){
						case ID_DRIVE:
							TUtils::NavigateToUrlInDefaultBrowser( TUtils::GetApplicationOnlineDocumentUrl(_T("faq_realFdd.html"),url) );
							break;
						case ID_FORMAT:
							TUtils::NavigateToUrlInDefaultBrowser( TUtils::GetApplicationOnlineDocumentUrl(_T("faq_formatFloppy.html"),url) );
							break;
						case ID_SYSTEM:
							TUtils::NavigateToUrlInDefaultBrowser( TUtils::GetApplicationOnlineDocumentUrl(_T("faq_accessFloppy.html"),url) );
							break;
					}
					*pResult=0;
					return TRUE;
				}
			}
		return CDialog::OnNotify(wParam,lParam,pResult);
	}
