#include "stdafx.h"

	#define INI_DISKBROWSER	_T("DiskBrowser")

	#define DOS		tab.dos
	#define IMAGE	DOS->image


	CDiskBrowserView::CDiskBrowserView(PDos dos)
		// ctor
		// - base
		: CHexaEditor( this, Utils::CreateSubmenuByContainedCommand(IDR_DISKBROWSER,ID_EDIT_SELECT_ALL), Utils::CreateSubmenuByContainedCommand(IDR_DISKBROWSER,ID_NAVIGATE_ADDRESS) )
		// - initialization
		, tab( IDR_DISKBROWSER, IDR_HEXAEDITOR, ID_CYLINDER, dos, this )
		, iScrollY(0) , f(nullptr) {
	}

	BEGIN_MESSAGE_MAP(CDiskBrowserView,CHexaEditor)
		ON_WM_CREATE()
		ON_COMMAND(ID_IMAGE_PROTECT,__toggleWriteProtection__)
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
		SetScrollPos( SB_VERT, iScrollY );
		SetEditable( !IMAGE->IsWriteProtected() );
		return 0;
	}

	void CDiskBrowserView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		if (f)
			delete f;
		f=IMAGE->CreateSectorDataSerializer(this);
		Reset( f, f->GetLength(), f->GetLength() );
	}

	BOOL CDiskBrowserView::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		switch (nCode){
			case CN_COMMAND:{
				// command
				BYTE nBytesToCompare=sizeof(TCylinder);
				switch (nID){
					case ID_SELECT_CURRENT_TRACK:
						// selecting current Track and placing Cursor at the end of the selection
						nBytesToCompare+=sizeof(THead);
						//fallthrough
					case ID_SELECT_CURRENT_CYLINDER:{
						// selecting current Cylinder and placing Cursor at the end of the selection
						int pos=__getCursorPos__();
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
						Edit_SetSel( m_hWnd, selectionA, selectionZ );
						return 0;
					}
					case ID_NAVIGATE_PREVIOUSTRACK:
						// moving Cursor at the beginning of previous Track
						nBytesToCompare+=sizeof(THead);
						//fallthrough
					case ID_NAVIGATE_PREVIOUSCYLINDER:{
						// moving Cursor at the beginning of previous Cylinder
						int pos=__getCursorPos__();
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
						Edit_SetSel( m_hWnd, targetPos, targetPos );
						return 0;
					}
					case ID_NAVIGATE_NEXTTRACK:
						// moving Cursor at the beginning of next Track
						nBytesToCompare+=sizeof(THead);
						//fallthrough
					case ID_NAVIGATE_NEXTCYLINDER:{
						// moving Cursor at the beginning of next Cylinder
						int pos=__getCursorPos__();
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
						Edit_SetSel( m_hWnd, pos, pos );
						return 0;
					}
					case ID_NAVIGATE_SECTOR:{
						// moving Cursor at the beginning of user-selected Sector
						// . seeking at the Cursor Position to determine the PhysicalAddress
						f->Seek( __getCursorPos__(), CFile::begin );
						// . defining the Dialog
						class CGoToSectorDialog sealed:public CDialog{
							const PCImage image;

							BOOL OnInitDialog() override{
								// dialog initialization
								// : base
								__super::OnInitDialog();
								// : setting arrows indicating the flow of interaction
								Utils::SetSingleCharTextUsingFont( ::GetDlgItem(m_hWnd,ID_CYLINDER_N), 0xf0e0, FONT_WINGDINGS, 110 );
								Utils::SetSingleCharTextUsingFont( ::GetDlgItem(m_hWnd,ID_TRACK), 0xf0e0, FONT_WINGDINGS, 110 );
								// : populating the Cylinder listbox with available Cylinder numbers and pre-selecting current Cylinder
								const BYTE sectorIndexOnTrackBk=sectorIndexOnTrack; const TPhysicalAddress chsBk=chs;
								TCHAR buf[80];
								CListBox lb;
								lb.Attach( ::GetDlgItem(m_hWnd,ID_CYLINDER) );
									for( TCylinder cyl=0; cyl<image->GetCylinderCount(); lb.AddString(::_itot(cyl++,buf,10)) );
									lb.SetCurSel(chsBk.cylinder);
								lb.Detach();
								SendMessage( WM_COMMAND, MAKELONG(ID_CYLINDER,LBN_SELCHANGE) ); // populating the Head listbox
								// : pre-selecting current Head
								ListBox_SetCurSel( ::GetDlgItem(m_hWnd,ID_HEAD), chsBk.head );
								SendMessage( WM_COMMAND, MAKELONG(ID_HEAD,LBN_SELCHANGE) ); // populating the Sector listbox
								// : pre-selecting current Sector
								ListBox_SetCurSel( ::GetDlgItem(m_hWnd,ID_SECTOR), sectorIndexOnTrackBk );
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
											lb.Attach( ::GetDlgItem(m_hWnd,ID_CYLINDER) );
												chs.cylinder=lb.GetCurSel();
											lb.Detach();
											lb.Attach( ::GetDlgItem(m_hWnd,ID_HEAD) );
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
											lb.Attach( ::GetDlgItem(m_hWnd,ID_HEAD) );
												chs.head=lb.GetCurSel();
											lb.Detach();
											lb.Attach( ::GetDlgItem(m_hWnd,ID_SECTOR) );
												lb.ResetContent();
												if (chs.head<image->GetNumberOfFormattedSides(chs.cylinder)){ // Head number valid
													TSectorId ids[(TSector)-1];
													TSector s=0,const nSectors=image->ScanTrack( chs.cylinder, chs.head, ids );
													for( TCHAR buf[80]; s<nSectors; lb.AddString(ids[s++].ToString(buf)) );
													lb.SetCurSel(0);
												}
											lb.Detach();
											//fallthrough
										}
										case MAKELONG(ID_SECTOR,LBN_SELCHANGE):{
											// Sector selection has changed
											CListBox lb;
											lb.Attach( ::GetDlgItem(m_hWnd,ID_SECTOR) );
												const int iSel=lb.GetCurSel();
												if (Utils::EnableDlgControl(m_hWnd,IDOK,iSel>=0)){
													TSectorId ids[(TSector)-1];
													image->ScanTrack( chs.cylinder, chs.head, ids );
													chs.sectorId=ids[ sectorIndexOnTrack=iSel ];
												}
											lb.Detach();
											break;
										}
									}
								return __super::WindowProc(msg,wParam,lParam);
							}
						public:
							BYTE sectorIndexOnTrack;
							TPhysicalAddress chs;

							CGoToSectorDialog(PCImage image,BYTE rSectorIndexOnTrack,RCPhysicalAddress rChs)
								: CDialog(IDR_DISKBROWSER_GOTOSECTOR)
								, image(image)
								, sectorIndexOnTrack(rSectorIndexOnTrack) , chs(rChs) {
							}
						} d( IMAGE, f->GetCurrentSectorIndexOnTrack(), f->GetCurrentPhysicalAddress() );
						// . showing the Dialog and processing its result
						if (d.DoModal()==IDOK){
							const int pos=f->GetSectorStartPosition(d.chs,d.sectorIndexOnTrack);
							Edit_SetSel( m_hWnd, pos, pos );
						}
						return 0;
					}
				}
				break;
			}
		}
		return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	}

	afx_msg void CDiskBrowserView::OnDestroy(){
		// window destroyed
		// - saving Scroll position for later
		iScrollY=GetScrollPos(SB_VERT);
		// - disposing the underlying File
		delete f, f=nullptr;
		// - base
		CView::OnDestroy();
	}

	afx_msg void CDiskBrowserView::__toggleWriteProtection__(){
		// toggles Image's WriteProtection flag
		IMAGE->__toggleWriteProtection__(); // "base"
		SetEditable( !IMAGE->IsWriteProtected() );
	}

	afx_msg void CDiskBrowserView::__closeView__(){
		CTdiCtrl::RemoveCurrentTab( TDI_HWND );
	}
