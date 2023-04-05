#include "stdafx.h"
using namespace Yahel;

	#define INI_DISKBROWSER	_T("DiskBrowser")

	#define IMAGE	tab.image
	#define DOS		IMAGE->dos

	typedef CImage::CSectorDataSerializer::TScannerStatus TScannerStatus;


	CDiskBrowserView::CDiskBrowserView(PImage image,RCPhysicalAddress chsToSeekTo,BYTE nSectorsToSkip)
		// ctor
		// - base
		: CHexaEditor( this )
		// - initialization
		, tab( IDR_DISKBROWSER, IDR_HEXAEDITOR, ID_CYLINDER, image, this )
		, revolution(Revolution::ANY_GOOD)
		, iScrollY(0) {
		seekTo.chs=chsToSeekTo, seekTo.nSectorsToSkip=nSectorsToSkip;
		// - modification of default HexaEditor's ContextMenu
		const Utils::CRideContextMenu mainMenu( *tab.menu.GetSubMenu(0) );
		contextMenu.ModifySubmenu(
			contextMenu.GetPosByContainedSubcommand(ID_YAHEL_SELECT_ALL),
			*mainMenu.GetSubMenu( mainMenu.GetPosByContainedSubcommand(ID_YAHEL_SELECT_ALL) )
		);
		contextMenu.ModifySubmenu(
			contextMenu.GetPosByContainedSubcommand(ID_YAHEL_GOTO_ADDRESS),
			*mainMenu.GetSubMenu( mainMenu.GetPosByContainedSubcommand(ID_YAHEL_GOTO_ADDRESS) )
		);
		contextMenu.AppendSeparator();
		contextMenu.AppendMenu( MF_BYCOMMAND|MF_STRING, ID_TIME, mainMenu.GetMenuStringByCmd(ID_TIME) );
	}
	
	static const auto WM_REPORT_SCANNER_PROGRESS=::RegisterWindowMessage(INI_DISKBROWSER);

	BEGIN_MESSAGE_MAP(CDiskBrowserView,CHexaEditor)
		ON_WM_CREATE()
		ON_COMMAND(ID_IMAGE_PROTECT,ToggleWriteProtection)
		ON_COMMAND(ID_FILE_CLOSE,__closeView__)
		ON_REGISTERED_MESSAGE(WM_REPORT_SCANNER_PROGRESS,ReportScanningProgress)
		ON_WM_DESTROY()
	END_MESSAGE_MAP()






	CDiskBrowserView &CDiskBrowserView::CreateAndSwitchToTab(PImage image,RCPhysicalAddress chsToSeekTo,BYTE nSectorsToSkip){
		// creates new instance of CDiskBrowserView, adds its Tab to the TDI, and returns the instance
		CDiskBrowserView *const dbView=new CDiskBrowserView( image, chsToSeekTo, nSectorsToSkip );
		if (chsToSeekTo==TPhysicalAddress::Invalid)
			CTdiCtrl::AddTabLast( TDI_HWND, _T("Sectors hexa-browser"), &dbView->tab, true, TDI_TAB_CANCLOSE_ALWAYS, CMainWindow::CTdiView::TTab::OnOptionalTabClosing );
		else{
			TCHAR caption[80];
			::wsprintf( caption, _T("Sector %s (%d)"), (LPCTSTR)chsToSeekTo.sectorId.ToString(), nSectorsToSkip );
			CTdiCtrl::AddTabLast( TDI_HWND, caption, &dbView->tab, true, TDI_TAB_CANCLOSE_ALWAYS, CMainWindow::CTdiView::TTab::OnOptionalTabClosing );
		}
		return *dbView;
	}

	void CDiskBrowserView::UpdateStatusBar(){
		// repopulates each pane in the StatusBar (if any)
		CStatusBar &rStatusBar=app.GetMainWindow()->statusBar;
		if (rStatusBar.m_hWnd) // may not exist if the app is closing
			switch (revolution){
				case Revolution::ANY_GOOD:
					rStatusBar.SetPaneText( 1, _T("Any good") );
					break;
				case Revolution::ALL_INTERSECTED:
					rStatusBar.SetPaneText( 1, _T("All intersected") );
					break;
				default:
					TCHAR buf[8];
					::wsprintf( buf, _T("Rev %c"), '1'+revolution );
					rStatusBar.SetPaneText( 1, buf );
					break;
			}
		ReportScanningProgress(0,0); // to make sure scanner status is immediatelly up-to-date
	}

	afx_msg int CDiskBrowserView::OnCreate(LPCREATESTRUCT lpcs){
		// window created
		// - base
		if (__super::OnCreate(lpcs)==-1)
			return -1;
		// - displaying the content
		OnUpdate(nullptr,0,nullptr);
		// - recovering the Scroll position and repainting the view (by setting its editability)
		//SetScrollPos( SB_VERT, iScrollY ); //TODO: Uncomment when scroll position is represented as absolute position in content, not as a row
		SetEditable( !IMAGE->IsWriteProtected() );
		// - reinitializing the StatusBar
		CStatusBar &rStatusBar=app.GetMainWindow()->statusBar;
		if (rStatusBar.m_hWnd){ // may not exist if the app is closing
			static constexpr UINT Indicators[]={ ID_SEPARATOR, ID_SEPARATOR };
			rStatusBar.SetIndicators( Indicators, ARRAYSIZE(Indicators) );
			rStatusBar.SetPaneInfo( 1, ID_SEPARATOR, SBPS_NORMAL, 90 );
		}
		UpdateStatusBar();
		SendMessage( WM_COMMAND, ID_CREATOR ); // navigate to requested Sector, if any
		return 0;
	}

	void CDiskBrowserView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		f.Attach( IMAGE->CreateSectorDataSerializer(this) );
		Update( f, f, f->GetLength() );
		const auto lastKnownScannerStatus=f->GetTrackScannerStatus(); // getting last known explicit status (e.g. by the user) ...
		if (lastKnownScannerStatus!=CImage::CSectorDataSerializer::TScannerStatus::UNAVAILABLE)
			f->SetTrackScannerStatus(lastKnownScannerStatus); // ... and resetting any internal status of the scanner
	}

	int CDiskBrowserView::GetCustomCommandMenuFlags(WORD cmd) const{
		// custom command GUI update
		switch (cmd){
			case ID_AUTO:
				return MF_CHECKED*( revolution==Revolution::ANY_GOOD );
			case ID_INTERLEAVE:
				return MF_CHECKED*( revolution==Revolution::ALL_INTERSECTED );
			case ID_DEFAULT1:
			case ID_DEFAULT2:
			case ID_DEFAULT3:
			case ID_DEFAULT4:
			case ID_DEFAULT5:
			case ID_DEFAULT6:
			case ID_DEFAULT7:
			case ID_DEFAULT8:
				return MF_CHECKED*( revolution==cmd-ID_DEFAULT1 );
			case ID_BUFFER:
				return	MF_CHECKED*( f->GetTrackScannerStatus()==TScannerStatus::PAUSED )
						+
						MF_GRAYED*!( f->GetTrackScannerStatus()!=TScannerStatus::UNAVAILABLE );
			case ID_SELECT_CURRENT_TRACK:
			case ID_SELECT_CURRENT_CYLINDER:
			case ID_NAVIGATE_PREVIOUSTRACK:
			case ID_NAVIGATE_PREVIOUSCYLINDER:
			case ID_NAVIGATE_NEXTTRACK:
			case ID_NAVIGATE_NEXTCYLINDER:
			case ID_NAVIGATE_SECTOR:
				return 0;
			case ID_TIME:
				return MF_GRAYED*!( IMAGE->ReadTrack(0,0) );
		}
		return __super::GetCustomCommandMenuFlags(cmd);
	}

	bool CDiskBrowserView::ProcessCustomCommand(UINT cmd){
		// custom command processing
		BYTE nBytesToCompare=sizeof(TCylinder);
		switch (cmd){
			case ID_AUTO:
				// selecting any Revolution with healthy data
				f->SetCurrentRevolution( revolution=Revolution::ANY_GOOD );
				UpdateStatusBar();
				return true;
			case ID_INTERLEAVE:
				f->SetCurrentRevolution( revolution=Revolution::ALL_INTERSECTED );
				UpdateStatusBar();
				return true;
			case ID_DEFAULT1:
			case ID_DEFAULT2:
			case ID_DEFAULT3:
			case ID_DEFAULT4:
			case ID_DEFAULT5:
			case ID_DEFAULT6:
			case ID_DEFAULT7:
			case ID_DEFAULT8:
				// selecting particular disk Revolution
				if (cmd-ID_DEFAULT1<f->nDiscoveredRevolutions) // do we have such Revolution?
					f->SetCurrentRevolution( revolution=(Revolution::TType)(cmd-ID_DEFAULT1) );
				UpdateStatusBar();
				return true;
			case ID_BUFFER:
				// pause/resume scanning
				f->SetTrackScannerStatus( 
					f->GetTrackScannerStatus()==TScannerStatus::PAUSED
					? TScannerStatus::RUNNING
					: TScannerStatus::PAUSED
				);
				return true;
			case ID_SELECT_CURRENT_TRACK:
				// selecting current Track and placing Cursor at the end of the selection
				nBytesToCompare+=sizeof(THead);
				//fallthrough
			case ID_SELECT_CURRENT_CYLINDER:{
				// selecting current Cylinder and placing Cursor at the end of the selection
				auto pos=GetCaretPosition();
				f->Seek(pos,CFile::begin);
				const TPhysicalAddress currChs=f->GetCurrentPhysicalAddress();
				TPosition selectionA;
				do{
					selectionA=pos;
					if (!pos) // if first Sector in the Image reached ...
						break; // ... then this is the beginning of current Track or Cylinder
					f->GetRecordInfo( --pos, &pos, nullptr, nullptr );
					f->Seek(pos,CFile::begin);
				}while (!::memcmp( &currChs, &f->GetCurrentPhysicalAddress(), nBytesToCompare ));
				auto selectionZ=selectionA;
				do{
					if (selectionZ>=f->GetLength()) // if we would be beyond the last Sector in the Image ...
						break; // ... then this is the end of the current Track or Cylinder
					TPosition sectorLength;
					f->GetRecordInfo( selectionZ, nullptr, &sectorLength, nullptr );
					selectionZ+=sectorLength;
					f->Seek(selectionZ,CFile::begin);
				}while (!::memcmp( &currChs, &f->GetCurrentPhysicalAddress(), nBytesToCompare ));					
				SetSelection( selectionA, selectionZ );
				return true;
			}
			case ID_NAVIGATE_PREVIOUSTRACK:
				// moving Cursor at the beginning of previous Track
				nBytesToCompare+=sizeof(THead);
				//fallthrough
			case ID_NAVIGATE_PREVIOUSCYLINDER:{
				// moving Cursor at the beginning of previous Cylinder
				auto pos=GetCaretPosition();
				f->Seek(pos-1,CFile::begin);
				const TPhysicalAddress currChs=f->GetCurrentPhysicalAddress();
				TPosition targetPos;
				do{
					targetPos=pos;
					if (!pos) // if first Sector in the Image reached ...
						break; // ... then this is the beginning of current Track or Cylinder
					f->GetRecordInfo( --pos, &pos, nullptr, nullptr );
					f->Seek(pos,CFile::begin);
				}while (!::memcmp( &currChs, &f->GetCurrentPhysicalAddress(), nBytesToCompare ));
				SetSelection( targetPos, targetPos );
				return true;
			}
			case ID_NAVIGATE_NEXTTRACK:
				// moving Cursor at the beginning of next Track
				nBytesToCompare+=sizeof(THead);
				//fallthrough
			case ID_NAVIGATE_NEXTCYLINDER:{
				// moving Cursor at the beginning of next Cylinder
				auto pos=GetCaretPosition();
				f->Seek(pos,CFile::begin);
				const TPhysicalAddress currChs=f->GetCurrentPhysicalAddress();
				do{
					if (pos>=f->GetLength()) // if we would be beyond the last Sector in the Image ...
						break; // ... then this is the end of the current Track or Cylinder
					TPosition sectorLength;
					f->GetRecordInfo( pos, &pos, &sectorLength, nullptr );
					pos+=sectorLength;
					f->Seek(pos,CFile::begin);
				}while (!::memcmp( &currChs, &f->GetCurrentPhysicalAddress(), nBytesToCompare ));
				SetSelection( pos, pos );
				return true;
			}
			case ID_NAVIGATE_SECTOR:{
				// moving Cursor at the beginning of user-selected Sector
				// . seeking at the Cursor Position to determine the PhysicalAddress
				f->Seek( GetCaretPosition(), CFile::begin );
				// . defining the Dialog
				class CGoToSectorDialog sealed:public Utils::CRideDialog{
					const Utils::CRideFont symbolFont;
					const PCImage image;
					bool sectorDoubleClicked;

					BOOL OnInitDialog() override{
						// dialog initialization
						// : base
						__super::OnInitDialog();
						// : setting arrows indicating the flow of interaction
						SetDlgItemSingleCharUsingFont( ID_CYLINDER_N, 0xf0e0, symbolFont );
						SetDlgItemSingleCharUsingFont( ID_TRACK, 0xf0e0, symbolFont );
						// : populating the Cylinder listbox with available Cylinder numbers and pre-selecting current Cylinder
						const BYTE sectorIndexOnTrackBk=sectorIndexOnTrack; const TPhysicalAddress chsBk=chs;
						TCHAR buf[80];
						CListBox lb;
						lb.Attach( GetDlgItemHwnd(ID_CYLINDER) );
							for( TCylinder cyl=0; cyl<image->GetCylinderCount(); lb.AddString(::_itot(cyl++,buf,10)) );
							lb.SetCurSel(chsBk.cylinder);
						lb.Detach();
						SendMessage( WM_COMMAND, MAKELONG(ID_CYLINDER,LBN_SELCHANGE) ); // populating the Head listbox
						// : pre-selecting current Head
						ListBox_SetCurSel( GetDlgItemHwnd(ID_HEAD), chsBk.head );
						SendMessage( WM_COMMAND, MAKELONG(ID_HEAD,LBN_SELCHANGE) ); // populating the Sector listbox
						// : pre-selecting current Sector
						ListBox_SetCurSel( GetDlgItemHwnd(ID_SECTOR), sectorIndexOnTrackBk );
						sectorIndexOnTrack=sectorIndexOnTrackBk;
						return TRUE;
					}
					LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
						// window procedure
						if (msg==WM_COMMAND)
							switch (wParam){
								case MAKELONG(ID_CYLINDER,LBN_SELCHANGE):{
									// Cylinder selection has changed
									chs.cylinder=GetDlgListBoxSelectedIndex(ID_CYLINDER);
									CListBox lb;
									lb.Attach( GetDlgItemHwnd(ID_HEAD) );
										lb.ResetContent();
										if (chs.cylinder<image->GetCylinderCount()){ // Cylinder number valid
											TCHAR buf[80];
											for( THead h=0,const nHeads=image->GetNumberOfFormattedSides(chs.cylinder); h<nHeads; lb.AddString(::_itot(h++,buf,10)) );
											lb.SetCurSel(0);
										}
									lb.Detach();
									//fallthrough
								}
								case MAKELONG(ID_HEAD,LBN_SELCHANGE):{
									// Head selection has changed
									chs.head=GetDlgListBoxSelectedIndex(ID_HEAD);
									CListBox lb;
									lb.Attach( GetDlgItemHwnd(ID_SECTOR) );
										lb.ResetContent();
										if (chs.head<image->GetNumberOfFormattedSides(chs.cylinder)){ // Head number valid
											TSectorId ids[(TSector)-1];
											TSector s=0,const nSectors=image->ScanTrack( chs.cylinder, chs.head, nullptr, ids );
											while (s<nSectors) lb.AddString(ids[s++].ToString());
											lb.SetCurSel(0);
										}
									lb.Detach();
									//fallthrough
								}
								case MAKELONG(ID_SECTOR,LBN_SELCHANGE):{
									// Sector selection has changed
									const int iSel=GetDlgListBoxSelectedIndex(ID_SECTOR);
									if (EnableDlgItem(IDOK,iSel>=0)){
										TSectorId ids[(TSector)-1];
										image->ScanTrack( chs.cylinder, chs.head, nullptr, ids );
										chs.sectorId=ids[ sectorIndexOnTrack=iSel ];
									}
									break;
								}
								case MAKELONG(ID_SECTOR,LBN_DBLCLK):
									// Image selected by double-clicking on it
									sectorDoubleClicked=IsDlgItemEnabled(IDOK); // setting flag ...
									SetCapture(); // ... waiting until mouse button released ...
									break;
							}
						else if (msg==WM_LBUTTONUP)
							if (sectorDoubleClicked)
								return SendMessage( WM_COMMAND, IDOK ); // ... and only after that confirming the dialog
						return __super::WindowProc(msg,wParam,lParam);
					}
				public:
					BYTE sectorIndexOnTrack;
					TPhysicalAddress chs;

					CGoToSectorDialog(PCImage image,BYTE rSectorIndexOnTrack,RCPhysicalAddress rChs)
						: Utils::CRideDialog(IDR_DISKBROWSER_GOTOSECTOR)
						, symbolFont( FONT_WINGDINGS, 125, false, true )
						, image(image)
						, sectorDoubleClicked(false)
						, sectorIndexOnTrack(rSectorIndexOnTrack) , chs(rChs) {
					}
				} d( IMAGE, f->GetCurrentSectorIndexOnTrack(), f->GetCurrentPhysicalAddress() );
				// . showing the Dialog and processing its result
				if (d.DoModal()==IDOK){
					const auto pos=f->GetSectorStartPosition(d.chs,d.sectorIndexOnTrack);
					SetSelection( pos, pos );
				}
				return true;
			}
			case ID_TIME:
				// display of low-level Track timing
				f->Seek( GetCaretPosition(), CFile::begin );
				IMAGE->ShowModalTrackTimingAt(
					f->GetCurrentPhysicalAddress(),
					f->GetCurrentSectorIndexOnTrack(),
					f->GetPositionInCurrentSector(),
					revolution<Revolution::MAX ? revolution : Revolution::ANY_GOOD
				);
				return true;
			case ID_CREATOR:
				// command sent by CSectorDataSerializer to inform that new Tracks have been scanned
				// . seeking to particular PhysicalAddress
				if (seekTo.chs!=TPhysicalAddress::Invalid
					&&
					true//newLogicalSize!=ls0 // has anything changed?
				){
					const auto posSector=f->GetSectorStartPosition( seekTo.chs, seekTo.nSectorsToSkip );
					const auto length=f->GetLength();
					if (posSector<length){
						// Sector already discovered on the disk
						SetSelection( posSector, posSector );
						seekTo.chs=TPhysicalAddress::Invalid; // seeking finished
					}else
						// disk still scanned for the Sector
						SetSelection( length, length );
				}
				// . reporting on scanning progress in the status bar
				if (m_hWnd)
					PostMessage( WM_REPORT_SCANNER_PROGRESS );
				return true;
		}
		return __super::ProcessCustomCommand(cmd);
	}

	afx_msg void CDiskBrowserView::OnDestroy(){
		// window destroyed
		// - saving Scroll position for later
		iScrollY=GetVertScrollPos();
		// - base
		__super::OnDestroy();
		// - disposing the underlying File
		f.Release();
		Update( nullptr, nullptr );
		// - clearing status bar
		app.GetMainWindow()->__resetStatusBar__();
	}

	afx_msg void CDiskBrowserView::ToggleWriteProtection(){
		// toggles Image's WriteProtection flag
		IMAGE->ToggleWriteProtection(); // "base"
		SetEditable( !IMAGE->IsWriteProtected() );
	}

	afx_msg LRESULT CDiskBrowserView::ReportScanningProgress(WPARAM,LPARAM){
		// reports the disk scanning progress in StatusBar
		// - report in StatusBar
		TCylinder nScannedCyls;
		switch (f->GetTrackScannerStatus(&nScannedCyls)){
			case CImage::CSectorDataSerializer::TScannerStatus::RUNNING:{
				TCHAR buf[32];
				::wsprintf( buf, _T("%d %% of disk scanned"), 100*nScannedCyls/IMAGE->GetCylinderCount() );
				CMainWindow::__setStatusBarText__(buf);
				break;
			}
			case CImage::CSectorDataSerializer::TScannerStatus::PAUSED:
				CMainWindow::__setStatusBarText__( _T("SCANNER PAUSED!") );
				break;
			default:
				CMainWindow::SetStatusBarTextReady();
				break;
		}
		// - update available Revolutions Submenu
		Utils::CRideContextMenu mainMenu( *app.GetMainWindow()->GetMenu() );
		for( TCHAR rev=Revolution::R1,cmdStr[16]; rev<f->nDiscoveredRevolutions; rev++ ){
			::wsprintf( cmdStr, _T("%c\tCtrl+%c"), '1'+rev, '1'+rev );
			mainMenu.InsertAfter( ID_DEFAULT1+rev-1, MF_BYCOMMAND, ID_DEFAULT1+rev, cmdStr );
		}
		return 0;
	}

	afx_msg void CDiskBrowserView::__closeView__(){
		CTdiCtrl::RemoveCurrentTab( TDI_HWND );
	}
