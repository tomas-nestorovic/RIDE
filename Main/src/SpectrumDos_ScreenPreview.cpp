#include "stdafx.h"

	#define SCREEN_BYTES_MAX	6912
	#define SCREEN_WIDTH		256
	#define SCREEN_HEIGHT		192

	#define INI_PREVIEW	_T("ZXScreen")

	#define INI_MSG	_T("msg")

	#define ID_FLASH	0

	static CDos::CFilePreview *pSingleInstance; // only single File can be previewed at a time

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
			case WM_COMMAND:
				// command processing
				switch (LOWORD(wParam)){
					case ID_PREV:
						offset=USHRT_MAX; // resetting the Offset before navigating to the previous File
						break;
					case ID_NEXT:
						offset=USHRT_MAX; // resetting the Offset before navigating to the next File
						break;
				}
				break;
			case WM_KEYDOWN:
				// character
				switch (wParam){
					case 'P':
						return SendMessage( WM_COMMAND, ID_DATA );
					case 'A':
						return SendMessage( WM_COMMAND, ID_ATTRIBUTE );
					case 'B':
						return SendMessage( WM_COMMAND, ID_ACCURACY );
					case 'D':
						return SendMessage( WM_COMMAND, ID_ALIGN );
					case 'F':
						return SendMessage( WM_COMMAND, ID_FILE_SHIFT_UP );
					case 'S':
						return SendMessage( WM_COMMAND, ID_FILE_SHIFT_DOWN );
				}
				break;
			case WM_PAINT:{
				// drawing
				RECT r;
				GetClientRect(&r);
				const CPaintDC dc(this);
				if (pdt->entry){
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
									0,0,
									SCREEN_WIDTH,SCREEN_HEIGHT,
									dib.data,
									&dib.bmi, DIB_RGB_COLORS, SRCCOPY
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
		// - ignore this flash request if flashing disabled
		auto *const psi=static_cast<CScreenPreview *>(pSingleInstance);
		if (!psi->showFlashing)
			return;
		// - swapping Colors
		RGBQUAD *bk=psi->dib.flashCombinations, *colors=psi->dib.colors;
		if ( psi->paperFlash=!psi->paperFlash )
			for( BYTE b=0; b<128; b++ )
				*bk++=colors[b>>3];
		else
			for( BYTE b=0; b<128; b++ )
				*bk++=colors[ b&64 ? 8+(b&7) : b&7 ];
		// - redrawing
		::InvalidateRect(hPreview,nullptr,FALSE);
	}







	#define IMAGE	fileManager.tab.image
	#define DOS		IMAGE->dos

	#define LABEL	_T("Screen$")

	CSpectrumBase::PCFilePreviewOffsetByFileType CSpectrumBase::CScreenPreview::pOffsetsByFileType;

	CSpectrumBase::CScreenPreview::CScreenPreview(const CFileManagerView &fileManager)
		// ctor
		// - base
		: CFilePreview( nullptr, LABEL, INI_PREVIEW, fileManager, SCREEN_WIDTH, SCREEN_HEIGHT, true, IDR_SPECTRUM_PREVIEW_SCREEN, &pSingleInstance )
		// - initialization
		, offset(USHRT_MAX)
		, showPixels(true) , showAttributes(true) , showFlashing(true)
		, paperFlash(false) {
		// - creating the Device Independent Bitmap (DIB)
		dib.bmi.bmiHeader.biSize=sizeof(dib.bmi);
		dib.bmi.bmiHeader.biWidth=SCREEN_WIDTH, dib.bmi.bmiHeader.biHeight=-SCREEN_HEIGHT; // negative height = a top-down DIB (otherwise bottom-up)
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
		pOffsetsByFileType=nullptr; // client's responsibility to free the array
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
		CDos::CFileReaderWriter frw( DOS, file );
		if (offset==USHRT_MAX){ // initial Offset? (i.e. NOT set manually)
			offset=0;
			DOS->GetFileSize( file, (PBYTE)&offset, nullptr );
			offset=pOffsetsByFileType->FindOffset(
				DOS->GetFileExt(file)
			);
		}
		frw.Seek( offset, CFile::begin );
		frw.Read( &buf, 6144+showAttributes*768 );
		if (const TStdWinError err=::GetLastError())
			return DOS->ShowFileProcessingError( file, err );
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
			for( BYTE z=SCREEN_WIDTH/8,*pZxByte=buf+zxOffset.w,*pZxAttr=zxAttributes; z--; pZxByte++,pZxAttr++ ){
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
					case ID_ACCURACY:
						((CCmdUI *)pExtra)->SetCheck(showFlashing);
						return TRUE;
					case ID_ALIGN:
						((CCmdUI *)pExtra)->Enable( DOS->GetFileSize(pdt->entry)>0 );
						return TRUE;
					case ID_FILE_SHIFT_UP:
						((CCmdUI *)pExtra)->Enable( offset<DOS->GetFileSize(pdt->entry) );
						return TRUE;
					case ID_FILE_SHIFT_DOWN:
						((CCmdUI *)pExtra)->Enable( offset>0 );
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
					case ID_ACCURACY:
						showFlashing=!showFlashing;
						RefreshPreview();
						return TRUE;
					case ID_ALIGN:{
						const PropGrid::Integer::TUpDownLimits limits={ 0, DOS->GetFileSize(pdt->entry)-1 };
						if (const Utils::CSingleNumberDialog &&d=Utils::CSingleNumberDialog( _T("Offset"), _T("Screen data begin at (0=default)"), limits, offset, false, this )){
							offset=d.Value;
							RefreshPreview();
						}
						return TRUE;
					}
					case ID_FILE_SHIFT_UP:
						offset++;
						RefreshPreview();
						return TRUE;
					case ID_FILE_SHIFT_DOWN:
						offset--;
						RefreshPreview();
						return TRUE;
					case ID_FILE_SAVE_AS:{
						const CString fileName=Utils::DoPromptSingleTypeFileName( _T(""), _T("Bitmap (*.bmp)|*.bmp|"), 0 );
						if (fileName.IsEmpty())
							return TRUE;
						struct{
							LOGPALETTE header;
							PALETTEENTRY palEntries[16];
						} logPalette={ {0x300,16} };
						for( BYTE c=0; c<16; c++ ){
							const RGBQUAD &quad=dib.bmi.bmiColors[c];
							PALETTEENTRY &r=logPalette.header.palPalEntry[c];
							r.peRed=quad.rgbRed, r.peGreen=quad.rgbGreen, r.peBlue=quad.rgbBlue;
						}
						CPalette palette;
						#define SCREEN_SAVE_ERROR_MSG	_T("Can't save screen")
						if (!palette.CreatePalette(&logPalette.header)){
							Utils::FatalError( SCREEN_SAVE_ERROR_MSG, ::GetLastError() );
							return TRUE;
						}
						HRESULT hr;
						PICTDESC pd={ sizeof(pd), PICTYPE_BITMAP };
							pd.bmp.hbitmap=dib.handle;
							pd.bmp.hpal=palette;
						CComPtr<IPicture> picture;
						if (SUCCEEDED( hr=::OleCreatePictureIndirect( &pd, __uuidof(IPicture), FALSE, (LPVOID*)&picture ) )){
							CComPtr<IStream> stream;
							if (SUCCEEDED( hr=::CreateStreamOnHGlobal( nullptr, TRUE, &stream ) ))
								if (SUCCEEDED( hr=picture->SaveAsFile( stream, TRUE, nullptr ) )){
									CComPtr<IPictureDisp> disp;
									if (SUCCEEDED( hr=picture->QueryInterface(&disp) ))
										hr=::OleSavePictureFile( disp, CComBSTR(fileName));
								}
						}
						if (FAILED(hr))
							Utils::FatalError( SCREEN_SAVE_ERROR_MSG, hr );
						return TRUE;
					}
				}
				break;
		}
		return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	}
