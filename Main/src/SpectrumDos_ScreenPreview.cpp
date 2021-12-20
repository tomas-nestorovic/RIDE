#include "stdafx.h"

	#define SCREEN_BYTES_MAX	6912
	#define SCREEN_WIDTH		256
	#define SCREEN_HEIGHT		192

	#define INI_PREVIEW	_T("ZXScreen")

	#define INI_MSG	_T("msg")

	#define ID_FLASH	0

	LRESULT CSpectrumBase::CScreenPreview::WindowProc(UINT uMsg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (uMsg){
			case WM_MOUSEWHEEL:
				// mouse wheel was rotated
				if (::GetKeyState(VK_CONTROL)<0){
					// if Ctrl key pressed, resizing the window
					RECT rc;
					GetWindowRect(&rc);
					const short zDelta=(short)HIWORD(wParam)/WHEEL_DELTA;
					rc.right+=Utils::LogicalUnitScaleFactor*zDelta*24, rc.bottom+=Utils::LogicalUnitScaleFactor*zDelta*24;
					SendMessage( WM_SIZING, WMSZ_TOPLEFT, (LPARAM)&rc ); // for the window to keep its aspect ratio
					SetWindowPos( nullptr, 0,0, rc.right-rc.left, rc.bottom-rc.top, SWP_NOMOVE|SWP_NOZORDER );
					return TRUE;
				}else
					// if Ctrl key NOT pressed, simply handing the message over to the parent class
					break;
			case WM_KEYDOWN:
				// character
				switch (wParam){
					case 'P':
						return SendMessage( WM_COMMAND, ID_DATA );
					case 'A':
						return SendMessage( WM_COMMAND, ID_ATTRIBUTE );
				}
				break;
			case WM_PAINT:{
				// drawing
				RECT r;
				GetClientRect(&r);
				const CPaintDC dc(this);
				if (pSingleInstance->pdt->entry){
					// drawing the Spectrum screen stored in a Device Independed Bitmap (DIB)
					// . DIB drawn across the whole Preview canvas; commented out as substituted by suitable StretchDIBits parameters
					/*::SetMapMode( dc, MM_ISOTROPIC );	// custom conversion of logical coordinates to device coordinates (Y axis points down)
					::SetWindowExtEx( dc, SCREEN_WIDTH-1, -SCREEN_HEIGHT+1, nullptr );	// "logical" window size is [Width,-Height] ("minus" in order for the Y axis to point up)
					::SetViewportExtEx( dc, ps.rcPaint.right, ps.rcPaint.bottom, nullptr );
					::SetViewportOrgEx( dc, 0, ps.rcPaint.bottom, nullptr );	// placing origin to the lower left corner
					*/
					// . drawing DIB
					::StretchDIBits(dc,
									0,0, r.right,r.bottom,
									0,SCREEN_HEIGHT+1,
									SCREEN_WIDTH,-SCREEN_HEIGHT,
									pSingleInstance->dib.data,
									&pSingleInstance->dib.bmi, DIB_RGB_COLORS, SRCCOPY
								);
				}else{
					// no screen File to draw
					//::FillRect( dc, &r, Utils::CRideBrush::BtnFace );
					::SetBkMode(dc,TRANSPARENT);
					::DrawText( dc, _T("Error: No file to display"),-1, &r, DT_SINGLELINE|DT_CENTER|DT_VCENTER );
					::LineTo( dc, r.right, r.bottom );
					::MoveToEx( dc, r.right, 0, nullptr );
					::LineTo( dc, 0, r.bottom );
				}
				return 0;
			}
		}
		return __super::WindowProc(uMsg,wParam,lParam);
	}

	void CALLBACK CSpectrumBase::CScreenPreview::__flash__(HWND hPreview,UINT nMsg,UINT nTimerID,DWORD dwTime){
		// swapping Colors of all FlashCombinations (Ink vs Paper) and redrawing the Preview
		// - swapping Colors
		RGBQUAD *bk=pSingleInstance->dib.flashCombinations, *colors=pSingleInstance->dib.colors;
		if ( pSingleInstance->paperFlash=!pSingleInstance->paperFlash )
			for( BYTE b=0; b<128; b++ )
				*bk++=colors[b>>3];
		else
			for( BYTE b=0; b<128; b++ )
				*bk++=colors[ b&64 ? 8+(b&7) : b&7 ];
		// - redrawing
		::InvalidateRect(hPreview,nullptr,FALSE);
	}







	#define IMAGE	rFileManager.tab.image
	#define DOS		IMAGE->dos

	#define LABEL	_T("Screen$")

	CSpectrumBase::CScreenPreview *CSpectrumBase::CScreenPreview::pSingleInstance;

	CSpectrumBase::CScreenPreview::CScreenPreview(const CFileManagerView &rFileManager)
		// ctor
		// - base
		: CFilePreview( nullptr, INI_PREVIEW, rFileManager, SCREEN_WIDTH, SCREEN_HEIGHT, true, IDR_SPECTRUM_PREVIEW_SCREEN )
		// - initialization
		, showPixels(true) , showAttributes(true)
		, paperFlash(false) {
		pSingleInstance=this;
		// - creating the Device Independent Bitmap (DIB)
		dib.bmi.bmiHeader.biSize=sizeof(dib.bmi);
		dib.bmi.bmiHeader.biWidth=SCREEN_WIDTH, dib.bmi.bmiHeader.biHeight=SCREEN_HEIGHT;
		dib.bmi.bmiHeader.biPlanes=1;
		dib.bmi.bmiHeader.biBitCount=8;
		dib.bmi.bmiHeader.biCompression=BI_RGB;
		dib.bmi.bmiHeader.biClrUsed=144; // 144 = 16 non-flashing Colors + 128 FlashCombinations
		dib.bmi.bmiHeader.biClrImportant=16;
		::memcpy( &dib.colors, &Colors, sizeof(Colors) ); // BmiHeader is immediately followed by Color table
		dib.handle=::CreateDIBSection( CClientDC(this), &dib.bmi, DIB_RGB_COLORS, (PVOID *)&dib.data, 0,0 );
		// - showing the first File
		__showNextFile__();
		// - launching the Flashing
		hFlashTimer=(HANDLE)::SetTimer( m_hWnd, ID_FLASH, 500, __flash__ );
		__flash__(m_hWnd,WM_TIMER,ID_FLASH,0);
		// - information
		Utils::InformationWithCheckableShowNoMore(_T("The \"") LABEL _T(" preview\" displays a file as if it was a picture.\nTwo usages are possible:\n(1) browsing only files that are selected in the \"") FILE_MANAGER_TAB_LABEL _T("\" (eventually showing just one picture if only one file is selected), or\n(2) browsing all files (if no particular files are selected)."),INI_PREVIEW,INI_MSG);
	}

	CSpectrumBase::CScreenPreview::~CScreenPreview(){
		// dtor
		// - freeing resources
		::KillTimer(m_hWnd,ID_FLASH);
		::DeleteObject(dib.handle);
		pSingleInstance=nullptr;
	}









	void CSpectrumBase::CScreenPreview::RefreshPreview(){
		// refreshes the Preview window
		const PCFile file=pdt->entry;
		if (!file) return;
		// - initializing the Buffer
		BYTE buf[SCREEN_BYTES_MAX];
		::memset(buf,0,6144);		// pixel part
		::memset(buf+6144,56,768);	// attribute part (56 = Paper 7 + Ink 0)
		// - loading the File into Buffer
		LPCTSTR errMsg=nullptr;
		DOS->ExportFile( file, &CMemFile(buf,sizeof(buf)), sizeof(buf), &errMsg );
		if (errMsg)
			return DOS->ShowFileProcessingError(file,errMsg);
		// - resetting the parts not desired for display
		if (!showPixels) // don't want pixels
			::memset(buf,0,6144);
		if (!showAttributes) // don't want attributes
			::memset(buf+6144,56,768);
		// - converting Spectrum data in Buffer to DIB pixel data
		union{
			struct{ BYTE L,H; };
			WORD w;
		} zxOffset; // ZX Byte offset on the screen
		zxOffset.w=0;
		for( BYTE microRow=SCREEN_HEIGHT,*dibPixel=dib.data,*zxAttributes=buf+6144; microRow--; ){
			// . converting the MicroRow (32 ZX characters)
			for( BYTE z=32,*pZxByte=buf+zxOffset.w,*pZxAttr=zxAttributes; z--; pZxByte++,pZxAttr++ ){
				// : converting one ZX Byte to eight DIB pixels
				BYTE b=8, zxByte=*pZxByte, zxAttr=*pZxAttr;
				if (zxAttr&128) // Attributes with flashing on
					for( const BYTE bright=zxAttr&64,ink=zxAttr&7,paper=(zxAttr>>3)&7; b--; zxByte<<=1 )
						*dibPixel++ = 16+( ( zxByte&128 ? (paper<<3)|ink : (ink<<3)|paper )|bright ); // "16+" = skipping non-flashing Colors in the table
				else	// Attributes with flashing off
					for( const BYTE paper=(zxAttr&120)>>3,bright=paper&8,ink=(zxAttr&7)|bright; b--; zxByte<<=1 )
						*dibPixel++ = zxByte&128 ? ink : paper;
			}
			// . next MicroRow (see ZXM 4/92, pp.17)
			if ((++zxOffset.H)&7) continue;	// next MicroRow in current character row
			zxAttributes+=32;
			if (!(zxOffset.L+=32)) continue;// next third
			zxOffset.H-=8;					// next character row
		}
		// - drawing the converted image
		InvalidateRect(nullptr,FALSE);
		// - updaring window's caption
		CString caption;
		caption.Format( LABEL " (%s)", (LPCTSTR)DOS->GetFilePresentationNameAndExt(file) );
		SetWindowText(caption);
	}

	BOOL CSpectrumBase::CScreenPreview::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:
				// update
				switch (nID){
					case ID_DATA:
						((CCmdUI *)pExtra)->SetCheck(showPixels);
						return TRUE;
					case ID_ATTRIBUTE:
						((CCmdUI *)pExtra)->SetCheck(showAttributes);
						return TRUE;
				}
				break;
			case CN_COMMAND:
				// command
				switch (nID){
					case ID_DATA:
						showPixels=!showPixels;
						RefreshPreview();
						return TRUE;
					case ID_ATTRIBUTE:
						showAttributes=!showAttributes;
						RefreshPreview();
						return TRUE;
				}
				break;
		}
		return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	}
