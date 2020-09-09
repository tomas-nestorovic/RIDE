#include "stdafx.h"

	CRealDeviceSelectionDialog::CRealDeviceSelectionDialog(CDos::PCProperties dosProps)
		// ctor
		: Utils::CRideDialog(IDR_DRIVE_ACCESS)
		, deviceProps(nullptr) , dosProps(dosProps) {
	}








	BOOL CRealDeviceSelectionDialog::OnInitDialog(){
		// dialog initialization
		// - base
		__super::OnInitDialog();
		// - positioning and scaling the Error box to match the Image list-box
		SetDlgItemPos( ID_ERROR, MapDlgItemClientRect(ID_IMAGE) );
		// - populating the list with devices compatible with specified DOS
		refreshListOfDevices();
		// - done
		return FALSE; // False = focus already set manually in RefreshListOfDevices
	}

	static void __checkCompatibilityAndAddToOptions__(CDos::PCProperties dosProps,CListBox &rLBImage,CImage::PCProperties imageProps){
		// determines if Properties of given DOS and Image are compatible, and in case that yes, adds given Image to options
		const WORD dosStdSectorLength=dosProps->stdFormats->params.format.sectorLength;
		if (imageProps->supportedMedia&dosProps->supportedMedia) // DOS and Image support common Media
			if (imageProps->sectorLengthMin<=dosStdSectorLength && dosStdSectorLength<=imageProps->sectorLengthMax)
				rLBImage.SetItemDataPtr( rLBImage.AddString(imageProps->fnRecognize(nullptr)), (PVOID)imageProps ); // Null as buffer = one Image represents only one "device" whose name is known at compile-time
	}

	void CRealDeviceSelectionDialog::refreshListOfDevices(){
		// forces refreshing of the list of containers available for the selected DOS; eventually displays an error message
		EnableDlgItem( IDOK, false ); // need yet to select a container
		CWnd *const pDeviceListBox=GetDlgItem(ID_IMAGE);
		// - populating the list with devices compatible with specified DOS
		CListBox lb;
		lb.Attach(pDeviceListBox->m_hWnd);
			lb.ResetContent();
			lb.ShowWindow(SW_SHOW), ShowDlgItem(ID_ERROR,false);
			lb.SetItemDataPtr( lb.AddString(_T("-- Select --")), (PVOID)&CUnknownDos::Properties );
			lb.SetCurSel(0);
			const WORD dosStdSectorLength=dosProps->stdFormats->params.format.sectorLength;
			for( POSITION pos=CImage::devices.GetHeadPosition(); pos; ){
				const CImage::PCProperties devProps=(CImage::PCProperties)CImage::devices.GetNext(pos);
				if (dosProps==&CUnknownDos::Properties // no preconditions on a Device
					||
					devProps->supportedMedia&dosProps->supportedMedia // DOS and Image support common Media
					&&
					devProps->sectorLengthMin<=dosStdSectorLength && dosStdSectorLength<=devProps->sectorLengthMax
				){
					TCHAR deviceNames[4*DEVICE_NAME_CHARS_MAX+1]; // up to 4 Devices, "+1" = terminating null-character
					for( LPCTSTR p=devProps->fnRecognize(deviceNames); *p; p+=::lstrlen(p)+1 )
						lb.SetItemDataPtr( lb.AddString(p), (PVOID)devProps );
				}
			}
			const int nCompatibleDevices=lb.GetCount();
		lb.Detach();
		// - if some Devices compatible with specified DOS found, focusing on the list
		if (nCompatibleDevices>1) // 1 = the "-- Select--" item
			pDeviceListBox->SetFocus();
		// - otherwise, displaying an error message
		else{
			ShowDlgItem(ID_ERROR), pDeviceListBox->ShowWindow(SW_HIDE);
			TCHAR errMsg[200], quotedDosName[50];
			::wsprintf( quotedDosName, _T("\"%s\""), dosProps->name );
			::wsprintf( errMsg, _T("No local physical device compatible with %s found."), dosProps!=&CUnknownDos::Properties?quotedDosName:_T("any DOS") );
			SetDlgItemText( ID_ERROR, errMsg );
		}
	}

	BOOL CRealDeviceSelectionDialog::OnCommand(WPARAM wParam,LPARAM lParam){
		// command processing
		switch (wParam){
			case MAKELONG(ID_IMAGE,LBN_SELCHANGE):{
				// Device selection changed
				CListBox lb;
				lb.Attach((HWND)lParam);
					const int iSelected=lb.GetCurSel();
					if (EnableDlgItem( IDOK, iSelected>0 )){
						deviceProps=(CImage::PCProperties)lb.GetItemData(iSelected);
						lb.GetText( iSelected, deviceName );
					}
				lb.Detach();
				return TRUE;
			}
		}
		return __super::OnCommand(wParam,lParam);
	}

	BOOL CRealDeviceSelectionDialog::OnNotify(WPARAM wParam,LPARAM lParam,LRESULT *pResult){
		// processes notification
		const LPCWPSTRUCT pcws=(LPCWPSTRUCT)lParam;
		if (pcws->wParam==ID_HELP_INDEX)
			switch (pcws->message){
				case NM_CLICK:
				case NM_RETURN:{
					refreshListOfDevices();
					*pResult=0;
					return TRUE;
				}
			}
		return __super::OnNotify(wParam,lParam,pResult);
	}
