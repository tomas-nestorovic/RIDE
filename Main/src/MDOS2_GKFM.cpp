#include "stdafx.h"
#include "MDOS2.h"

	#define INI_GKFM_PREVIEW		_T("gkfmpreviewmsg")
	#define INI_GKFM_IMPORT_LATER	_T("gkfmimplater")

	#define GKFM_WINDOW_MARGIN	2
	#define GKFM_ICON_WIDTH		32
	#define GKFM_ICON_HEIGHT	24
	#define GKFM_ICON_FM_ZOOM_FACTOR	2
	#define GKFM_ICON_PG_ZOOM_FACTOR	1


	HIMAGELIST CMDOS2::TBootSector::TGKFileManager::GetListOfDefaultIcons(){
		// creates and returns the list of GKFM's standard icons
		const HIMAGELIST result=ImageList_Create( GKFM_ICON_WIDTH*GKFM_ICON_FM_ZOOM_FACTOR, GKFM_ICON_HEIGHT*GKFM_ICON_FM_ZOOM_FACTOR, ILC_COLOR24, 0, 1 );
			static constexpr BYTE DefaultIcon[]={ // smiling face in glases
				0,0,0,0,0,0,0,0,0,15,224,0,0,48,24,0,0,64,4,0,0,184,58,0,1,0,1,0,2,124,124,128,48,48,48,48,3,255,255,128,4,254,254,64,4,254,254,64,4,254,254,64,4,125,124,64,4,1,0,64,4,3,128,64,4,0,0,64,48,48,48,48,2,0,0,128,2,7,192,128,1,12,97,0,0,144,18,0,0,64,4,0,0,48,24,0,0,15,224,0,0,0,0,0,48,48,48,48
			};
			static constexpr BYTE SnapshotIcon[]={ // skull with crossbones
				0,0,0,0,0,7,224,0,0,26,176,0,0,53,8,0,0,96,12,0,0,76,100,0,0,158,246,0,0,158,242,0,112,112,112,112,12,158,242,112,18,205,102,136,18,67,4,136,33,99,140,132,33,51,153,196,35,216,55,204,28,250,191,120,0,58,188,0,112,112,112,112,7,31,240,224,9,250,189,16,8,234,175,16,8,64,3,16,4,64,2,32,4,64,2,32,3,128,1,192,0,0,0,0,112,112,112,112
			};
			AddIconToList(result,DefaultIcon);	// = available under index GKFM_ICON_DEFAULT
			AddIconToList(result,SnapshotIcon);	// = available under index GKFM_ICON_SNAPSHOT
		return result;
	}

	BYTE CMDOS2::TBootSector::TGKFileManager::AddIconToList(HIMAGELIST icons,PCBYTE iconZxData){
		// adds a new icon from IconZxData to the list, and returns the index of the added icon
		const CClientDC screen(nullptr);
		const HDC dc=::CreateCompatibleDC(screen);
			const HBITMAP hBmp=::CreateCompatibleBitmap( screen, GKFM_ICON_WIDTH*GKFM_ICON_FM_ZOOM_FACTOR, GKFM_ICON_HEIGHT*GKFM_ICON_FM_ZOOM_FACTOR );
				const HGDIOBJ hBmp0=::SelectObject(dc,hBmp);
					DrawIcon( dc, iconZxData, GKFM_ICON_FM_ZOOM_FACTOR );
				::SelectObject(dc,hBmp0);
				const int i=ImageList_Add( icons, hBmp, nullptr );
			::DeleteObject(hBmp);
		::DeleteDC(dc);
		return i;
	}

	void CMDOS2::TBootSector::TGKFileManager::DrawIcon(HDC dc,PCBYTE iconZxData,BYTE zoomFactor){
		// draws icon described by its ZxData scaled using the ZoomFactor into the DC
		// - if there are no ZxData, no icon can be drawn (may happen if corresponding Advanced value is wrongly set (see GetAboutIconData)
		if (!iconZxData)
			return;
		// - translating ZxData into a Bitmap
		struct{
			BITMAPINFO bmi;
			RGBQUAD colors[16];
		} info={ sizeof(info.bmi), GKFM_ICON_WIDTH, GKFM_ICON_HEIGHT, 1, 8, BI_RGB, 0,0,0, 16, 16 };
		::memcpy( &info.colors, &Colors, sizeof(Colors) ); // color table immediately follows the BmiHeader
		PBYTE dibPixel;
		const HBITMAP hBmp=::CreateDIBSection( dc, &info.bmi, DIB_RGB_COLORS, (PVOID *)&dibPixel, 0,0 );
		for( BYTE nThirds=3; nThirds--; ){
			for( BYTE nMicroRows=8,*iconZxAttribs=(PBYTE)iconZxData+32; nMicroRows--; )
				for( BYTE z=4,*pZxAttr=iconZxAttribs; z--; iconZxData++,pZxAttr++ ){
					// converting current MicroRow (four ZX characters; flashing ignored!)
					const BYTE zxAttr=*pZxAttr, paper=(zxAttr&120)>>3, bright=paper&8, ink=(zxAttr&7)|bright;
					for( BYTE b=8,zxByte=*iconZxData; b--; zxByte<<=1 ) // converting one ZxByte to eight DibPixels
						*dibPixel++ = zxByte&128 ? ink : paper;
				}
			iconZxData+=4; // each Third followed by four already preprocessed attribute Bytes
		}
		// - factoring in the Zoom and turning the Bitmap upside down (as the Y axis points up in DIBs)
		const HDC dcsrc=::CreateCompatibleDC(dc);
			const HGDIOBJ hBmpSrc0=::SelectObject(dcsrc,hBmp);
				::StretchBlt(	dc, 0,0, GKFM_ICON_WIDTH*zoomFactor, GKFM_ICON_HEIGHT*zoomFactor,
								dcsrc, 0,GKFM_ICON_HEIGHT-1, GKFM_ICON_WIDTH, -GKFM_ICON_HEIGHT,
								SRCCOPY
							);
			::DeleteObject( ::SelectObject(dcsrc,hBmpSrc0) );
		::DeleteDC(dcsrc);
	}






	#define GKFM_BASE	34000

	static constexpr BYTE DefaultIcon[]={ // default floppy icon
		255,255,255,0,255,255,255,0,192,248,3,0,192,255,255,0,192,248,3,0,192,255,255,0,255,248,3,0,255,255,254,0,88,48,48,56,255,255,254,0,255,255,255,0,255,231,255,0,255,195,255,0,255,194,255,0,255,231,255,0,255,255,255,0,255,255,255,0,56,56,56,56,255,231,255,0,255,231,255,0,255,231,255,0,255,231,255,0,255,231,255,0,255,231,255,0,255,255,255,0,255,255,255,0,56,40,56,56
	};
	constexpr WORD DefaultIconAddress=0x7926;

	static bool IsValidBootAddress(WORD w){
		return GKFM_BASE<w && w<GKFM_BASE+MDOS2_SECTOR_LENGTH_STD;
	}

	static bool IsValidCustomIconAddress(WORD w){
		return GKFM_BASE<w && w<GKFM_BASE+MDOS2_SECTOR_LENGTH_STD-sizeof(DefaultIcon);
	}

	PCBYTE CMDOS2::TBootSector::TGKFileManager::GetAboutIconData() const{
		// returns the beginning of GKFM's icon in the Boot Sector
		if (IsValidCustomIconAddress(aIcon))
			return (PCBYTE)this+aIcon-GKFM_BASE;
		else if (aIcon==DefaultIconAddress)
			return DefaultIcon;
		else
			return nullptr;
	}

	#define DESKTOP_NL	13	/* Proxima Desktop's "new line" character */
	#define DESKTOP_CR	15	/* Proxima Desktop's "carriage return" character, line break */

	#define GKFM_TEXT_MAX	255 /* max length of text description stored in the Boot Sector */

	void CMDOS2::TBootSector::TGKFileManager::GetAboutText(PTCHAR bufT) const{
		// populates Buffer with Text retrieved from the Boot Sector and converted to ASCII char-set; caller guarantees that the Buffer can contain at least GKFM_TEXT_MAX characters
		if (IsValidBootAddress(aText)){
			// . getting Text from Boot Sector
			BYTE bufD[GKFM_TEXT_MAX]; // D = Desktop
			::memcpy( bufD, (PCBYTE)this+aText-GKFM_BASE, std::min(GKFM_TEXT_MAX,MDOS2_SECTOR_LENGTH_STD-(aText-GKFM_BASE)) );
			bufD[GKFM_TEXT_MAX-1]=DESKTOP_NL;
			// . Desktop->ASCII char-set conversion
			for( BYTE n=GKFM_TEXT_MAX,*a=bufD; n--; a++ )
				switch (*a){
					case DESKTOP_CR: *bufT++='\r'; *bufT++='\n'; break;
					case DESKTOP_NL: *bufT++='\0'; return;
					default: *bufT++=*a; break;
				}
		}else
			*bufT='\0';
	}

	BYTE CMDOS2::TBootSector::TGKFileManager::GetPropGridItemHeight() const{
		// returns the height of GKFM property in PropertyGrid
		return GKFM_ICON_PG_ZOOM_FACTOR*(2*(GKFM_WINDOW_MARGIN+8)+GKFM_ICON_HEIGHT); // 8 = window standard padding (8 ZX pixels)
	}

	bool WINAPI CMDOS2::TBootSector::TGKFileManager::WarnOnEditingAdvancedValue(PropGrid::PCustomParam,int){
		// shows a warning on about to change an "advanced" parameter of GK's File Manager
		if (Utils::QuestionYesNo(_T("Advanced properties are best left alone if not knowing their purpose (consult George K's \"Boot Maker\").\n\nContinue anyway?!"),MB_DEFBUTTON2))
			return CBootView::__bootSectorModified__( nullptr, 0 );
		else
			return false;
	}

	#define GKFM_NAME			_T("GK's File Manager")
	#define GKFM_IMPORT_NAME	_T("run.P ZXP500001L2200T8")
	#define GKFM_ONLINE_NAME	_T("MDOS2/GKFM/") GKFM_IMPORT_NAME

	static bool WINAPI UpdateOnline(PropGrid::PCustomParam,int hyperlinkId,LPCTSTR hyperlinkName){
		// True <=> PropertyGrid's Editor can be destroyed after this function has terminated, otherwise False
		BYTE gkfmDataBuffer[16384]; // sufficiently big buffer
		DWORD gkfmDataLength;
		TCHAR gkfmUrl[200];
		TStdWinError err =	Utils::DownloadSingleFile( // also displays the error message in case of problems
								Utils::GetApplicationOnlineFileUrl( GKFM_ONLINE_NAME, gkfmUrl ),
								gkfmDataBuffer, sizeof(gkfmDataBuffer), &gkfmDataLength,
								MDOS2_RUNP_NOT_MODIFIED
							);
		if (err==ERROR_SUCCESS){
			CDos::PFile tmp;
			DWORD conflictResolution=CFileManagerView::TConflictResolution::UNDETERMINED;
			err=CDos::GetFocused()->pFileManager->ImportFileAndResolveConflicts( &CMemFile(gkfmDataBuffer,sizeof(gkfmDataBuffer)), gkfmDataLength, GKFM_IMPORT_NAME, 0, FILETIME(), FILETIME(), FILETIME(), tmp, conflictResolution );
			if (err!=ERROR_SUCCESS)
				Utils::FatalError( _T("Cannot import ") GKFM_NAME, err, MDOS2_RUNP_NOT_MODIFIED );
		}
		return true; // True = destroy PropertyGrid's Editor
	}

	void WINAPI CMDOS2::TBootSector::TGKFileManager::DrawIconBytes(PropGrid::PCustomParam,PropGrid::PCValue value,PropGrid::TSize valueSize,PDRAWITEMSTRUCT pdis){
		PCBootSector boot=(PCBootSector)value;
		if (IsValidCustomIconAddress(boot->gkfm.aIcon))
			CDos::CHexaValuePropGridEditor::DrawValue( nullptr, boot->gkfm.GetAboutIconData(), sizeof(DefaultIcon), pdis );
		else if (boot->gkfm.aIcon==DefaultIconAddress)
			::DrawText( pdis->hDC, _T(" (Default)"),-1, &pdis->rcItem, DT_SINGLELINE|DT_VCENTER|DT_LEFT );
		else{
			::SetTextColor( pdis->hDC, COLOR_RED );
			::DrawText( pdis->hDC, _T(" Invalid icon address"),-1, &pdis->rcItem, DT_SINGLELINE|DT_VCENTER|DT_LEFT );
		}
	}

	bool WINAPI CMDOS2::TBootSector::TGKFileManager::EditIconBytes(PropGrid::PCustomParam,PropGrid::PValue value,PropGrid::TSize valueSize){
		PBootSector boot=(PBootSector)value;
		if (IsValidCustomIconAddress(boot->gkfm.aIcon))
			return CDos::CHexaValuePropGridEditor::EditValue( nullptr, (PBYTE)boot->gkfm.GetAboutIconData(), sizeof(DefaultIcon) );
		if (boot->gkfm.aIcon==DefaultIconAddress){
			if (Utils::QuestionYesNo( _T("Copy icon to boot sector at default address?"), MB_DEFBUTTON2 )){
				boot->gkfm.aIcon=34300;
				::memcpy( (PBYTE)boot->gkfm.GetAboutIconData(), DefaultIcon, sizeof(DefaultIcon) );
				return true;
			}
		}else
			if (Utils::QuestionYesNo( _T("Set default icon?"), MB_DEFBUTTON2 )){
				boot->gkfm.aIcon=Default.aIcon;
				return true;
			}
		return false;
	}

	void CMDOS2::TBootSector::TGKFileManager::AddToPropertyGrid(HWND hPropGrid){
		// adds a property showing the presence of GK's File Manager on the disk into PropertyGrid
		const HANDLE hGkfm=PropGrid::AddCategory(hPropGrid,nullptr,GKFM_NAME);
		const bool recognized=id==Default.id; // textual representation of "FM" string
		PropGrid::AddDisabledProperty(	hPropGrid, hGkfm, _T("Status"),
										recognized?"Recognized":"Not recognized",
										PropGrid::String::DefineFixedLengthEditorA(0)
									);
		if (recognized){
			// . basic preview
			PropGrid::AddProperty(	hPropGrid, hGkfm, _T("Basic"),
									this,
									PropGrid::Custom::DefineEditor( GetPropGridItemHeight(), sizeof(TBootSector), DrawPropGridItem, nullptr, EditPropGridItem )
								);
			// . advanced properties
			const HANDLE hAdvanced=PropGrid::AddCategory( hPropGrid, hGkfm, BOOT_SECTOR_ADVANCED, false );
				const PropGrid::PCEditor advEditor=PropGrid::Integer::DefineWordEditor(WarnOnEditingAdvancedValue);
				PropGrid::AddProperty(	hPropGrid, hAdvanced, _T("Text address"),
										&aText, advEditor
									);
				PropGrid::AddProperty(	hPropGrid, hAdvanced, _T("Window address"),
										&aWnd, advEditor
									);
				PropGrid::AddProperty(	hPropGrid, hAdvanced, _T("Icon address"),
										&aIcon, advEditor
									);
				PropGrid::AddProperty(	hPropGrid, hAdvanced, _T("Icon data"), this,
										PropGrid::Custom::DefineEditor(
											0, // default height
											sizeof(DefaultIcon),
											DrawIconBytes,
											nullptr, EditIconBytes, // no main control, just ellipsis button
											nullptr,
											CBootView::__bootSectorModified__
										),
										this
									);
				PropGrid::AddProperty(	hPropGrid, hAdvanced, _T("VideoRAM address"),
										&aVRam, advEditor
									);
			// . offering to update the GKFM on the disk from an on-line resource
			PropGrid::AddProperty(	hPropGrid, hGkfm, MDOS2_RUNP,
									BOOT_SECTOR_UPDATE_ONLINE_HYPERLINK,
									PropGrid::Hyperlink::DefineEditorA(UpdateOnline)
								);
		}else
			PropGrid::AddProperty(	hPropGrid, hGkfm, _T(""),
									"<a>Create</a>",
									PropGrid::Hyperlink::DefineEditorA( CreateNew, CBootView::__updateCriticalSectorView__ )
								);
	}

	void WINAPI CMDOS2::TBootSector::TGKFileManager::DrawPropGridItem(PropGrid::PCustomParam,LPCVOID bootSector,short,PDRAWITEMSTRUCT pdis){
		// draws a summary on GK's File Manager status into PropertyGrid
		const HDC dc=pdis->hDC;
		Utils::ScaleLogicalUnit(dc);
		const TGKFileManager &gkfm=*(TGKFileManager *)bootSector;
		POINT org;
		::GetViewportOrgEx(dc,&org);
		org.x+=Utils::LogicalUnitScaleFactor*GKFM_ICON_PG_ZOOM_FACTOR*GKFM_WINDOW_MARGIN, org.y+=Utils::LogicalUnitScaleFactor*GKFM_ICON_PG_ZOOM_FACTOR*GKFM_WINDOW_MARGIN;
		::SetViewportOrgEx( dc, org.x, org.y, nullptr );
		// - drawing the background
		const BYTE color=gkfm.color;
		const RGBQUAD paper=Colors[(color&120)>>3], ink=Colors[(color&7)|((color&64)>>3)]; // RGBQUAD inversed order of R-G-B components than COLORREF
		const CBrush paperBrush(RGB(paper.rgbRed,paper.rgbGreen,paper.rgbBlue)), inkBrush(RGB(ink.rgbRed,ink.rgbGreen,ink.rgbBlue));
		RECT r=pdis->rcItem;
		r.right=r.left+GKFM_ICON_PG_ZOOM_FACTOR*gkfm.w, r.bottom=r.top+GKFM_ICON_PG_ZOOM_FACTOR*gkfm.h;
		::FillRect(dc,&r,paperBrush);
		::FrameRect(dc,&r,inkBrush);
		// - drawing icons
		union{
			struct{ BYTE L,H; };
			WORD w;
		} vram;
		vram.w=gkfm.aVRam;
		::SetViewportOrgEx(	dc,
							org.x+Utils::LogicalUnitScaleFactor*GKFM_ICON_PG_ZOOM_FACTOR*( (BYTE)(vram.L<<3) - gkfm.x ), // A-B, A = converted ZX->PC pixel, B = PC pixel
							org.y+Utils::LogicalUnitScaleFactor*GKFM_ICON_PG_ZOOM_FACTOR*( (BYTE)(vram.H<<3&192 | vram.L>>2&56 | vram.H&7) - gkfm.y ), // A|B|C, A = third, B = row, C = microrow
							nullptr
						);
		DrawIcon( dc, gkfm.GetAboutIconData(), GKFM_ICON_PG_ZOOM_FACTOR );
		// - drawing text
		const Utils::CRideFont font(FONT_VERDANA,60);
		const HGDIOBJ hFont0=::SelectObject(dc,font);
			TCHAR buf[GKFM_TEXT_MAX];
			gkfm.GetAboutText(buf);
			::SetViewportOrgEx( dc, org.x+Utils::LogicalUnitScaleFactor*GKFM_ICON_PG_ZOOM_FACTOR*gkfm.dx, org.y+Utils::LogicalUnitScaleFactor*GKFM_ICON_PG_ZOOM_FACTOR*gkfm.dy, nullptr );
			::SetTextColor(dc,RGB(ink.rgbRed,ink.rgbGreen,ink.rgbBlue));
			::DrawText(	dc, buf,-1, &pdis->rcItem, DT_LEFT|DT_TOP );
		::SelectObject(dc,hFont0);
	}

	#define COLOR_TRANSPARENT	255

	bool WINAPI CMDOS2::TBootSector::TGKFileManager::EditPropGridItem(PropGrid::PCustomParam,PVOID bootSector,short){
		// True <=> edited values of GK's File Manager in PropertyGrid confirmed, otherwise False
		// - defining the Dialog
		class CEditDialog sealed:public Utils::CRideDialog{
		public:
			TBootSector boot;
			TGKFileManager &rGkfm;
			int colorSelection,ink,paper,bright,aligning;
		private:
			static void DDV_DivisibleBy8(CDataExchange *pDX,BYTE value){
				// validates divisibility of Value by eight, reports a problem
				if (pDX->m_bSaveAndValidate && value&7){
					Utils::Information(_T("All window dimensions must be multiples of eight!"));
					pDX->Fail();
				}
			}
			void DoDataExchange(CDataExchange *pDX) override{
				// exchange of data from and to controls
				// - window parameters
				DDX_Text(	pDX, ID_X		,rGkfm.x);
					DDV_DivisibleBy8(pDX,rGkfm.x);
				DDX_Text(	pDX, ID_Y		,rGkfm.y);
					DDV_DivisibleBy8(pDX,rGkfm.y);
					DDV_MinMaxByte( pDX, rGkfm.h, 0, 192 );
				DDX_Text(	pDX, ID_W		,rGkfm.w);
					DDV_DivisibleBy8(pDX,rGkfm.w);
					DDV_MinMaxByte( pDX, rGkfm.w, 0, 256-rGkfm.x );
				DDX_Text(	pDX, ID_H		,rGkfm.h);
					DDV_DivisibleBy8(pDX,rGkfm.h);
					DDV_MinMaxByte( pDX, rGkfm.h, 0, 192-rGkfm.y );
				DDX_Radio(	pDX, ID_DEFAULT1	,colorSelection);
				DDX_Text(	pDX, ID_INK		,ink);
					if (colorSelection==0) DDV_MinMaxInt( pDX, ink, 0, 7 );
				DDX_Text(	pDX, ID_PAPER	,paper);
					if (colorSelection==0) DDV_MinMaxInt( pDX, paper, 0, 7 );
				DDX_Check(	pDX, ID_BRIGHT	,bright);
				rGkfm.color= colorSelection==0 ? 64*bright+paper*8+ink : COLOR_TRANSPARENT;
				// - icon (icons drawn in WM_PAINT)
				DDX_Radio(	pDX, ID_ALIGN,aligning);
				if (pDX->m_bSaveAndValidate){
					const BYTE y=rGkfm.y+8;
					union{
						struct{ BYTE L,H; };
						WORD w;
					} vram;
					vram.H= 0x40 | y>>3&24 /*| y&7*/; // A|B|C, A = screen base, B = third, C = microrow; commented out as Y always divisible by 8, and thus always Y&7==0
					vram.L= y<<2&224 | rGkfm.x>>3; // A|B, A = row, B = column
					switch (aligning){
						case 0: rGkfm.aVRam=vram.w+1; break;
						case 1: rGkfm.aVRam=vram.w+(rGkfm.w-GKFM_ICON_WIDTH>>3)-1; break;
					}
				}
				// - text
				//DDX_Text(	pDX, ID_DATA	,bufT,sizeof(bufT)/sizeof(TCHAR)); // commented out as carried out below on custom basis
				DDX_Text(	pDX, ID_DX		,rGkfm.dx);
					DDV_MinMaxInt( pDX, rGkfm.dx, 0, rGkfm.w );
				DDX_Text(	pDX, ID_DY		,rGkfm.dy);
					DDV_MinMaxInt( pDX, rGkfm.dy, 0, rGkfm.h );
				TCHAR bufT[GKFM_TEXT_MAX+100]; // "+100" = just to be sure
				if (pDX->m_bSaveAndValidate){
					// . determining the max length of Text (mustn't collide with important regions in MDOS Boot Sector)
					const WORD w=rGkfm.aText-GKFM_BASE;
					BYTE nCharsMaxD,*d=(PBYTE)&boot+w,nCharsD=1; // D = Desktop, 1 = terminating DESKTOP_NL character
					if (sizeof(TGKFileManager)<=w && w<sizeof(boot.reserved1)) // GKFM_VRAM+2 = GK's File Manager data length in Boot Sector
						nCharsMaxD=sizeof(boot.reserved1)-w;
					else if (w>=MDOS2_SECTOR_LENGTH_STD-sizeof(UReserved3) && w<MDOS2_SECTOR_LENGTH_STD)
						nCharsMaxD=std::min<>( GKFM_TEXT_MAX, MDOS2_SECTOR_LENGTH_STD-w );
					else{
errorText:				TCHAR buf[400];
						::wsprintf( buf, _T("Text location collides with critical section in the boot.\n\nTo resolve, try to\n(a) shorten the text to max.%d characters (incl. all Desktop formatting characters), or\n(b) change its beginning in the ") BOOT_SECTOR_ADVANCED _T(" setting subcategory."), GKFM_TEXT_MAX );
						Utils::Information(buf);
						pDX->PrepareEditCtrl(ID_DATA);
						pDX->Fail();
					}
					// . converting char-sets PC->Desktop
					GetDlgItemText( ID_DATA, bufT, sizeof(bufT)/sizeof(TCHAR) );
					for( PTCHAR a=bufT; const TCHAR z=*a++; nCharsD++ ){
						switch (z){
							case '\r':
								*d++=DESKTOP_CR;
								// fallthrough
							case '\n':
								break;
							default:
								*d++=z;
						}
						if (nCharsD==nCharsMaxD)
							goto errorText;
					}
					*d=DESKTOP_NL;
				}else{
					// reading text into dedicated control
					boot.gkfm.GetAboutText(bufT);
					SetDlgItemText( ID_DATA, bufT );
				}
			}
			LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override{
				// window procedure
				if (message==WM_PAINT){
					// drawing icons
					const POINT iconPosition=MapDlgItemClientOrigin(ID_IMAGE);
					const CClientDC dc(this);
					const int iDc0=::SaveDC(dc);
						::SetViewportOrgEx(dc,iconPosition.x,iconPosition.y,nullptr);
						Utils::ScaleLogicalUnit(dc);
						DrawIcon( dc, rGkfm.GetAboutIconData(), 2 );
					::RestoreDC(dc,iDc0);
				}else if (message==WM_COMMAND)
					switch (wParam){
						case MAKELONG(ID_INK,EN_SETFOCUS):
						case MAKELONG(ID_PAPER,EN_SETFOCUS):
						case MAKELONG(ID_BRIGHT,BN_CLICKED):
							CheckRadioButton( ID_DEFAULT1, ID_DEFAULT2, ID_DEFAULT1 );
							break;
					}
				return __super::WindowProc(message,wParam,lParam);
			}
		public:
			CEditDialog(PBootSector _boot)
				// ctor
				: Utils::CRideDialog(IDR_MDOS_GKFM_EDITOR)
				, boot(*_boot) , rGkfm(boot.gkfm) {
				// - window parameters
				const BYTE color=rGkfm.color;
				colorSelection= color==COLOR_TRANSPARENT;
				ink=color&7;
				paper=(color>>3)&7;
				bright=color&64 ? BST_CHECKED : BST_UNCHECKED;
				// - icon
				union{
					struct{ BYTE L,H; };
					WORD w;
				} vram;
				vram.w=rGkfm.aVRam;
				const BYTE ikonaX=vram.L<<3, ikonaY=vram.H<<3&192 | vram.L>>2&56 | vram.H&7; // A|B|C, A = third, B = row, C = microrow
				const BYTE wndX=rGkfm.x, wndY=rGkfm.y;
				if (wndX+8==ikonaX && wndY+8==ikonaY)
					aligning=0;
				else if (wndX+rGkfm.w-GKFM_ICON_WIDTH-8==ikonaX && wndY+8==ikonaY)
					aligning=1;
				else
					aligning=2;
				// - text
				//nop (see DoDataExchange)
			}
		} d((PBootSector)bootSector);
		// - showing Dialog and processing its result
		__informationWithCheckableShowNoMore__(_T("Layout on real hardware may differ from the preview.\n\nClick OK to proceed to the editor."),INI_GKFM_PREVIEW);
		if (d.DoModal()==IDOK){
			*(PBootSector)bootSector=d.boot;
			return CBootView::__bootSectorModified__(nullptr,0);
		}else
			return false;
	}

	constexpr CMDOS2::TBootSector::TGKFileManager CMDOS2::TBootSector::TGKFileManager::Default={
		0x4d46, // textual representation of "FM" string
		8, 112, 120, 40, // y, x, w, h [in pixels]
		56, // color (black text on white background)
		5, 40,	// [dy,dx] offset of the text from window's upper left corner
		34210,	// address of the text in memory
		34209,	// address of the window
		0,		// always zero
		31014,	// address of icon in memory
		16463	// address of window in Spectrum's VideoRAM
	};
	
	bool WINAPI CMDOS2::TBootSector::TGKFileManager::CreateNew(PropGrid::PCustomParam param,int hyperlinkId,LPCTSTR hyperlinkName){
		// True <=> PropertyGrid's Editor can be destroyed after this function has terminated, otherwise False
		const PMDOS2 mdos=(PMDOS2)CDos::GetFocused();
		const PImage image=mdos->image;
		const PBootSector pBootSector=(PBootSector)image->GetHealthySectorData(CHS);
		TBootSector tmpBootSector=*pBootSector;
			tmpBootSector.gkfm=TGKFileManager::Default;
			static constexpr BYTE DefGkfmText[]={	0,255,
												'P','R','O','X','I','M','A',',',' ','v','.','o','.','s','.',15,
												'S','o','f','t','w','a','r','e',15,
												'n','o','v',128,' ','d','i','m','e','n','z','e',13
											};
			::memcpy( tmpBootSector.reserved1+208, DefGkfmText, sizeof(DefGkfmText) );
		if (EditPropGridItem(nullptr,&tmpBootSector,0)){
			// creation of GKFM confirmed
			// . accepting the GKFM record in the Boot Sector
			*pBootSector=tmpBootSector; // adopting confimed values
			image->UpdateAllViews(nullptr);
			// . downloading the GKFM binary from the Internet and importing it to the disk
			UpdateOnline(param,0,nullptr);
		}
		return true; // True = destroy PropertyGrid's Editor
	}
