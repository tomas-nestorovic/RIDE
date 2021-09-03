#include "stdafx.h"

	void WINAPI CDiskBrowserView::OnDiskBrowserViewClosing(CTdiCtrl::TTab::PContent tab){
		delete ((CMainWindow::CTdiView::PTab)tab)->view;
	}

	#define INI_DISKBROWSER	_T("DiskBrowser")

	#define DOS		tab.dos
	#define IMAGE	DOS->image

	typedef CImage::CSectorDataSerializer::TScannerStatus TScannerStatus;


	CDiskBrowserView::CDiskBrowserView(PDos dos,RCPhysicalAddress chsToSeekTo,BYTE nSectorsToSkip)
		// ctor
		// - base
		: CHexaEditor( this, Utils::CreateSubmenuByContainedCommand(IDR_DISKBROWSER,ID_EDIT_SELECT_ALL), nullptr, Utils::CreateSubmenuByContainedCommand(IDR_DISKBROWSER,ID_NAVIGATE_ADDRESS) )
		// - initialization
		, tab( IDR_DISKBROWSER, IDR_HEXAEDITOR, ID_CYLINDER, dos, this )
		, revolution(Revolution::ANY_GOOD)
		, iScrollY(0) {
		seekTo.chs=chsToSeekTo, seekTo.nSectorsToSkip=nSectorsToSkip;
	}

	BEGIN_MESSAGE_MAP(CDiskBrowserView,CHexaEditor)
		ON_WM_CREATE()
		ON_COMMAND(ID_IMAGE_PROTECT,ToggleWriteProtection)
		ON_COMMAND(ID_FILE_CLOSE,__closeView__)
		ON_WM_DESTROY()
	END_MESSAGE_MAP()

	CDiskBrowserView::~CDiskBrowserView(){
		// dtor
		// - destroying the custom "Select" and "Go to" submenus
		::DestroyMenu(customSelectSubmenu);
		::DestroyMenu(customGotoSubmenu);
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
		return 0;
	}

	void CDiskBrowserView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		f=IMAGE->CreateSectorDataSerializer(this);
		const auto fLength=f->GetLength();
		Reset( f.get(), fLength, fLength );
	}

	BOOL CDiskBrowserView::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:{
				// update
				CCmdUI *const pCmdUi=(CCmdUI *)pExtra;
				switch (nID){
					case ID_AUTO:
						pCmdUi->SetRadio( revolution==Revolution::ANY_GOOD );
						return TRUE;
					case ID_INTERLEAVE:
						pCmdUi->SetRadio( revolution==Revolution::ALL_INTERSECTED );
						return TRUE;
					case ID_DEFAULT1:
					case ID_DEFAULT2:
					case ID_DEFAULT3:
					case ID_DEFAULT4:
					case ID_DEFAULT5:
					case ID_DEFAULT6:
					case ID_DEFAULT7:
					case ID_DEFAULT8:
						if (nID-ID_DEFAULT1<std::min<BYTE>(Revolution::MAX,IMAGE->GetAvailableRevolutionCount())){
							pCmdUi->SetRadio( revolution==nID-ID_DEFAULT1 );
						}else
							pCmdUi->m_pMenu->RemoveMenu( nID, MF_BYCOMMAND );
						return TRUE;
					case ID_BUFFER:
						pCmdUi->SetCheck( f->GetTrackScannerStatus()==TScannerStatus::PAUSED );
						pCmdUi->Enable( f->GetTrackScannerStatus()!=TScannerStatus::UNAVAILABLE );
						return TRUE;						
					case ID_SELECT_CURRENT_TRACK:
					case ID_SELECT_CURRENT_CYLINDER:
					case ID_NAVIGATE_PREVIOUSTRACK:
					case ID_NAVIGATE_PREVIOUSCYLINDER:
					case ID_NAVIGATE_NEXTTRACK:
					case ID_NAVIGATE_NEXTCYLINDER:
					case ID_NAVIGATE_SECTOR:
						pCmdUi->Enable( TRUE );
						return TRUE;
				}
				break;
			}
			case CN_COMMAND:{
				// command
				BYTE nBytesToCompare=sizeof(TCylinder);
				switch (nID){
					case ID_AUTO:
						// selecting any Revolution with healthy data
						f->SetCurrentRevolution( revolution=Revolution::ANY_GOOD );
						return TRUE;
					case ID_INTERLEAVE:
						f->SetCurrentRevolution( revolution=Revolution::ALL_INTERSECTED );
						return TRUE;
					case ID_DEFAULT1:
					case ID_DEFAULT2:
					case ID_DEFAULT3:
					case ID_DEFAULT4:
					case ID_DEFAULT5:
					case ID_DEFAULT6:
					case ID_DEFAULT7:
					case ID_DEFAULT8:
						// selecting particular disk Revolution
						f->SetCurrentRevolution( revolution=(Revolution::TType)(nID-ID_DEFAULT1) );
						return TRUE;
					case ID_BUFFER:
						// pause/resume scanning
						f->SetTrackScannerStatus( 
							f->GetTrackScannerStatus()==TScannerStatus::PAUSED
							? TScannerStatus::RUNNING
							: TScannerStatus::PAUSED
						);
						return TRUE;
					case ID_SELECT_CURRENT_TRACK:
						// selecting current Track and placing Cursor at the end of the selection
						nBytesToCompare+=sizeof(THead);
						//fallthrough
					case ID_SELECT_CURRENT_CYLINDER:{
						// selecting current Cylinder and placing Cursor at the end of the selection
						int pos=HexaEditor_GetCaretPos(m_hWnd);
						f->Seek(pos,CFile::begin);
						const TPhysicalAddress currChs=f->GetCurrentPhysicalAddress();
						int selectionA;
						do{
							selectionA=pos;
							if (!pos) // if first Sector in the Image reached ...
								break; // ... then this is the beginning of current Track or Cylinder
							f->GetRecordInfo( --pos, &pos, nullptr, nullptr );
							f->Seek(pos,CFile::begin);
						}while (!::memcmp( &currChs, &f->GetCurrentPhysicalAddress(), nBytesToCompare ));
						int selectionZ=selectionA;
						do{
							if (selectionZ>=f->GetLength()) // if we would be beyond the last Sector in the Image ...
								break; // ... then this is the end of the current Track or Cylinder
							int sectorLength;
							f->GetRecordInfo( selectionZ, nullptr, &sectorLength, nullptr );
							selectionZ+=sectorLength;
							f->Seek(selectionZ,CFile::begin);
						}while (!::memcmp( &currChs, &f->GetCurrentPhysicalAddress(), nBytesToCompare ));					
						HexaEditor_SetSelection( m_hWnd, selectionA, selectionZ );
						return 0;
					}
					case ID_NAVIGATE_PREVIOUSTRACK:
						// moving Cursor at the beginning of previous Track
						nBytesToCompare+=sizeof(THead);
						//fallthrough
					case ID_NAVIGATE_PREVIOUSCYLINDER:{
						// moving Cursor at the beginning of previous Cylinder
						int pos=HexaEditor_GetCaretPos(m_hWnd);
						f->Seek(pos-1,CFile::begin);
						const TPhysicalAddress currChs=f->GetCurrentPhysicalAddress();
						int targetPos;
						do{
							targetPos=pos;
							if (!pos) // if first Sector in the Image reached ...
								break; // ... then this is the beginning of current Track or Cylinder
							f->GetRecordInfo( --pos, &pos, nullptr, nullptr );
							f->Seek(pos,CFile::begin);
						}while (!::memcmp( &currChs, &f->GetCurrentPhysicalAddress(), nBytesToCompare ));
						HexaEditor_SetSelection( m_hWnd, targetPos, targetPos );
						return 0;
					}
					case ID_NAVIGATE_NEXTTRACK:
						// moving Cursor at the beginning of next Track
						nBytesToCompare+=sizeof(THead);
						//fallthrough
					case ID_NAVIGATE_NEXTCYLINDER:{
						// moving Cursor at the beginning of next Cylinder
						int pos=HexaEditor_GetCaretPos(m_hWnd);
						f->Seek(pos,CFile::begin);
						const TPhysicalAddress currChs=f->GetCurrentPhysicalAddress();
						do{
							if (pos>=f->GetLength()) // if we would be beyond the last Sector in the Image ...
								break; // ... then this is the end of the current Track or Cylinder
							int sectorLength;
							f->GetRecordInfo( pos, &pos, &sectorLength, nullptr );
							pos+=sectorLength;
							f->Seek(pos,CFile::begin);
						}while (!::memcmp( &currChs, &f->GetCurrentPhysicalAddress(), nBytesToCompare ));
						HexaEditor_SetSelection( m_hWnd, pos, pos );
						return 0;
					}
					case ID_NAVIGATE_SECTOR:{
						// moving Cursor at the beginning of user-selected Sector
						// . seeking at the Cursor Position to determine the PhysicalAddress
						f->Seek( HexaEditor_GetCaretPos(m_hWnd), CFile::begin );
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
											CListBox lb;
											lb.Attach( GetDlgItemHwnd(ID_CYLINDER) );
												chs.cylinder=lb.GetCurSel();
											lb.Detach();
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
											CListBox lb;
											lb.Attach( GetDlgItemHwnd(ID_HEAD) );
												chs.head=lb.GetCurSel();
											lb.Detach();
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
											CListBox lb;
											lb.Attach( GetDlgItemHwnd(ID_SECTOR) );
												const int iSel=lb.GetCurSel();
												if (EnableDlgItem(IDOK,iSel>=0)){
													TSectorId ids[(TSector)-1];
													image->ScanTrack( chs.cylinder, chs.head, nullptr, ids );
													chs.sectorId=ids[ sectorIndexOnTrack=iSel ];
												}
											lb.Detach();
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
							const int pos=f->GetSectorStartPosition(d.chs,d.sectorIndexOnTrack);
							HexaEditor_SetSelection( m_hWnd, pos, pos );
						}
						return 0;
					}
				}
				break;
			}
		}
		return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	}

	void CDiskBrowserView::SetLogicalSize(int newLogicalSize){
		// changes the LogicalSize of File content (originally set when Resetting the HexaEditor)
		// - base
		__super::SetLogicalSize(newLogicalSize);
		// - seeking to particular PhysicalAddress
		if (seekTo.chs!=TPhysicalAddress::Invalid){
			const int posSector=f->GetSectorStartPosition( seekTo.chs, seekTo.nSectorsToSkip );
			if (posSector<newLogicalSize){
				// Sector already discovered on the disk
				HexaEditor_SetSelection( m_hWnd, posSector, posSector );
				seekTo.chs=TPhysicalAddress::Invalid; // seeking finished
			}else
				// disk still scanned for the Sector
				HexaEditor_SetSelection( m_hWnd, newLogicalSize, newLogicalSize );
		}
		// - content cannot be shorter or longer than the actual number of Bytes in all Sectors discovered thus far
		SetLogicalBounds( newLogicalSize, newLogicalSize );
	}

	afx_msg void CDiskBrowserView::OnDestroy(){
		// window destroyed
		// - saving Scroll position for later
		iScrollY=GetScrollPos(SB_VERT);
		// - disposing the underlying File
		f.reset();
		// - base
		__super::OnDestroy();
	}

	afx_msg void CDiskBrowserView::ToggleWriteProtection(){
		// toggles Image's WriteProtection flag
		IMAGE->ToggleWriteProtection(); // "base"
		SetEditable( !IMAGE->IsWriteProtected() );
	}

	afx_msg void CDiskBrowserView::__closeView__(){
		CTdiCtrl::RemoveCurrentTab( TDI_HWND );
	}
