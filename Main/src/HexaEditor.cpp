#include "stdafx.h"

	#define INI_HEXAEDITOR	_T("HexaEdit")
	#define INI_MSG_PADDING	_T("msgpad")

	void CHexaEditor::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		TUtils::InformationWithCheckableShowNoMore( text, INI_HEXAEDITOR, messageId );
	}





	CHexaEditor::TCursor::TCursor(int position)
		// ctor
		: ascii(false) , hexaLow(true) , selectionA(-1) , position(position) {
	}
	void CHexaEditor::TCursor::__detectNewSelection__(){
		// detects and sets the beginning of new Selection
		if (::GetAsyncKeyState(VK_SHIFT)>=0) // if Shift NOT pressed ...
			selectionA=position;	// ... we have detected a new Selection
	}






	#define ADDRESS_FORMAT			_T(" %04X-%04X")
	#define ADDRESS_FORMAT_LENGTH	10
	#define ADDRESS_SPACE_LENGTH	1

	const CHexaEditor::TEmphasis CHexaEditor::TEmphasis::Terminator={ -1, -1 };

	CHexaEditor::CHexaEditor(PVOID _param)
		// ctor
		: font(_T("Courier New"),105,false,true)
		, cursor(0) , param(_param) , hPreviouslyFocusedWnd(0)
		, nBytesInRow(16) , editable(true) , addrLength(ADDRESS_FORMAT_LENGTH)
		, emphases((PEmphasis)&TEmphasis::Terminator) {
	}








	#define SELECTION_CANCEL()	cursor.selectionA=cursor.selectionZ=-1; // cancelling the Selection

	void CHexaEditor::SetEditable(bool _editable){
		// enables/disables possibility to edit the content of the File (see the Reset function)
		editable=_editable;
		cursor=TCursor( GetScrollPos(SB_VERT)*nBytesInRow ); // resetting the Cursor and thus Selection
		if (::IsWindow(m_hWnd)){ // may be window-less if the owner is window-less
			app.m_pMainWnd->SetFocus(); // for the Cursor to disappear
			Invalidate(FALSE);
		}
	}

	int CHexaEditor::ShowAddressBand(bool _show){
		// shows/hides the Address bar; returns the width of the Address bar
		addrLength= _show ? ADDRESS_FORMAT_LENGTH : 0;
		if (::IsWindow(m_hWnd)) // may be window-less if the owner is window-less
			Invalidate(FALSE);
		return ADDRESS_FORMAT_LENGTH*font.charAvgWidth;
	}

	void CHexaEditor::Reset(CFile *_f,DWORD _minFileSize,DWORD _maxFileSize){
		// resets the HexaEditor and supplies it new File content
		f=_f, minFileSize=_minFileSize, maxFileSize=_maxFileSize, logicalSize=0;
		cursor=TCursor(0); // resetting the Cursor and Selection
		if (::IsWindow(m_hWnd)){ // may be window-less if the owner is window-less
			__refreshVertically__();
			Invalidate(FALSE);
		}
	}

	void CHexaEditor::SetLogicalSize(DWORD _logicalSize){
		// changes the LogicalSize of File content (originally set when Resetting the HexaEditor)
		logicalSize=_logicalSize;
		if (::IsWindow(m_hWnd)){ // may be window-less if the owner is window-less
			__refreshVertically__();
			Invalidate(FALSE);
		}
	}

	void CHexaEditor::GetVisiblePart(DWORD &rLogicalBegin,DWORD &rLogicalEnd) const{
		// gets the beginning and end of visible portion of the File content
		const DWORD dw=GetScrollPos(SB_VERT);
		rLogicalBegin=dw*nBytesInRow, rLogicalEnd=(dw+nRowsDisplayed)*nBytesInRow;
	}

	void CHexaEditor::AddEmphasis(DWORD a,DWORD z){
		// adds a new Emphasis into the list and orders the list by beginnings A (and thus also by endings Z; insertsort)
		PEmphasis *p=&emphases;
		while (a>(*p)->a) p=&(*p)->pNext;
		const PEmphasis newEmp=new TEmphasis;
		newEmp->a=a, newEmp->z=z, newEmp->pNext=*p;
		*p=newEmp;
	}

	void CHexaEditor::CancelAllEmphases(){
		// destroys the list of Emphases
		while (emphases!=&TEmphasis::Terminator){
			const PEmphasis p=emphases;
			emphases=p->pNext;
			delete p;
		}
	}

	#define ROWS_MAX	max(logicalSize/nBytesInRow,nRowsInTotal)

	int CHexaEditor::__scrollToRow__(int row){
		// scrolls the HexaEditor so that the specified Row is shown as the first one from top; returns the Row number to which it has been really scrolled to
		// - Row must be in expected limits
		const int scrollMax=ROWS_MAX-nRowsOnPage;
		if (row<0) row=0;
		else if (row>scrollMax) row=scrollMax;
		// - displaying where it's been scrolled to
		SetScrollPos(SB_VERT,row,TRUE); // True = redrawing the scroll-bar, not HexaEditor's canvas!
		// - redrawing HexaEditor's client and non-client areas
		Invalidate(FALSE);
		return row;
	}

	#define HEADER_LINES_COUNT	1

	void CHexaEditor::__refreshVertically__(){
		// refreshes all parameters that relate to vertical axis
		// - determining the total number of Rows
		nRowsInTotal=f->GetLength()/nBytesInRow;
		// - setting the scrolling dimensions
		RECT r;
		GetClientRect(&r);
		nRowsDisplayed=max( 0, (r.bottom-r.top)/font.charHeight-HEADER_LINES_COUNT );
		nRowsOnPage=max( 0, nRowsDisplayed-1 );
		const DWORD rowMax=ROWS_MAX;
		const BOOL scrollbarNecessary=nRowsOnPage<rowMax;
		ShowScrollBar(SB_VERT,scrollbarNecessary);
		//if (scrollbarNecessary){
			SCROLLINFO si={ sizeof(si), SIF_RANGE|SIF_PAGE, 0,rowMax-1, nRowsOnPage };
			SetScrollInfo( SB_VERT, &si, TRUE );
		//}
	}


	#define BYTES_MAX		64

	#define HEXA_FORMAT			_T("%02X ")
	#define HEXA_FORMAT_LENGTH	3
	#define HEXA_SPACE_LENGTH	2

	void CHexaEditor::__refreshCursorDisplay__() const{
		// shows Cursor at position on screen that corresponds with Cursor's actual "logical" Position (e.g. the Cursor is "logically" at 12345-th Byte in the underlying File content)
		const div_t d=div(cursor.position,nBytesInRow);
		const int iScrollY=GetScrollPos(SB_VERT);
		//if (d.quot>=iScrollY){ // commented out as always guaranteed
			// Cursor "under" the header
			POINT pos={ 0, (HEADER_LINES_COUNT+d.quot-iScrollY)*font.charHeight };
			if (cursor.ascii) // Cursor in the Ascii area
				pos.x=(addrLength+ADDRESS_SPACE_LENGTH+HEXA_FORMAT_LENGTH*nBytesInRow+HEXA_SPACE_LENGTH+d.rem)*font.charAvgWidth;
			else // Cursor in the Hexa area
				pos.x=(addrLength+ADDRESS_SPACE_LENGTH+HEXA_FORMAT_LENGTH*d.rem)*font.charAvgWidth;
			SetCaretPos(pos);
		/*}else{ // commented out as it never occurs
			// Cursor "above" the header
			static const POINT Pos={ -100, -100 };
			SetCaretPos(Pos);
		}*/
	}

	void CHexaEditor::__showMessage__(LPCTSTR msg) const{
		// shows Message and passes focus back to the HexaEditor
		TUtils::Information(msg);
		::PostMessage( m_hWnd, WM_LBUTTONDOWN, 0, -1 ); // recovering the focus; "-1" = [x,y] = nonsense value; can't use mere SetFocus because this alone doesn't work
	}

	#define MESSAGE_LIMIT_UPPER	_T("The content has reached its upper limit.")

	static bool mouseDragged;

	#define CLIPFORMAT_BINARY	_T("RideHexaEditBinary")

	static UINT cfBinary;

	void CHexaEditor::__setNormalPrinting__(HDC dc){
		::SetTextColor( dc, ::GetSysColor( editable?COLOR_WINDOWTEXT:COLOR_GRAYTEXT) );
		::SetBkColor( dc, 0xffffff );
	}
	static void __setEmphasizedPrinting__(HDC dc){
		::SetTextColor( dc, ::GetSysColor(COLOR_HIGHLIGHTTEXT) );
		::SetBkColor( dc, ::GetSysColor(COLOR_ACTIVECAPTION) );
	}
	static void __setSelectionPrinting__(HDC dc){
		::SetTextColor( dc, RGB(235,100,150) );
		::SetBkColor( dc, 0xffffff );
	}
	LRESULT CHexaEditor::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_MOUSEACTIVATE:
				// preventing the focus from being stolen by the parent
				return MA_ACTIVATE;
			case WM_CREATE:
				// window created
				// . base
				if (CEdit::WindowProc(msg,wParam,lParam)<0) return -1;
				// . creating the ClipFormat
				cfBinary=::RegisterClipboardFormat(CLIPFORMAT_BINARY);
				// . recovering the scroll position
				//SetScrollPos( SB_VERT, iScrollY, FALSE );
				return 0;
			case WM_KEYDOWN:{
				// key pressed
				if (!editable) return 0; // if window disabled, focus cannot be acquired
				const bool ctrl=::GetKeyState(VK_CONTROL)<0;
				switch (wParam){
					case VK_LEFT:{
						cursor.position--;
cursorCorrectlyMoveTo:	// . adjusting the Cursor's Position
						cursor.hexaLow=true;
						if (cursor.position<0) cursor.position=0;
						else if (cursor.position>maxFileSize) cursor.position=maxFileSize;
						// . adjusting an existing Selection if Shift pressed
						if (!mouseDragged) // if mouse is being -Dragged, the beginning of a Selection has already been detected
							cursor.__detectNewSelection__();
						cursor.selectionZ=cursor.position;
cursorRefresh:			// . refreshing the Cursor
						HideCaret();
							// : scrolling if Cursor has been moved to an invisible part of the File content
							const int iRow=cursor.position/nBytesInRow, iScrollY=GetScrollPos(SB_VERT);
							if (iRow<iScrollY) __scrollToRow__(iRow);
							else if (iRow>=iScrollY+nRowsOnPage) __scrollToRow__(iRow-nRowsOnPage+1);
							// : displaying the Cursor
							__refreshCursorDisplay__();
							Invalidate(FALSE);
						ShowCaret();
						return 0;
					}
					case VK_RIGHT:
						cursor.position++; goto cursorCorrectlyMoveTo;
					case VK_UP:
						if (ctrl){
							const int iRow=cursor.position/nBytesInRow, iScrollY=__scrollToRow__(GetScrollPos(SB_VERT)-1);
							if (iRow<iScrollY+nRowsOnPage) goto cursorRefresh;
						}
						cursor.position-=nBytesInRow; goto cursorCorrectlyMoveTo;
					case VK_DOWN:
						if (ctrl){
							const int iRow=cursor.position/nBytesInRow, iScrollY=__scrollToRow__(GetScrollPos(SB_VERT)+1);
							if (iRow>=iScrollY) goto cursorRefresh;
						}
						cursor.position+=nBytesInRow; goto cursorCorrectlyMoveTo;
					case VK_PRIOR:	// page up
						cursor.position-=( ctrl ? nRowsOnPage-1 : nRowsOnPage )*nBytesInRow; goto cursorCorrectlyMoveTo;
					case VK_NEXT:	// page down
						cursor.position+=( ctrl ? nRowsOnPage-1 : nRowsOnPage )*nBytesInRow; goto cursorCorrectlyMoveTo;
					case VK_HOME:
						cursor.position=( ctrl ? 0 : cursor.position/nBytesInRow*nBytesInRow ); goto cursorCorrectlyMoveTo;
					case VK_END:
						cursor.position=( ctrl ? f->GetLength() : cursor.position/nBytesInRow*nBytesInRow+nBytesInRow-1 ); goto cursorCorrectlyMoveTo;
					case VK_DELETE:{
editDelete:				// deleting the Byte after Cursor, or deleting the Selection
						// . if Selection not set, setting it as the Byte immediately after Cursor
						if (cursor.selectionA==cursor.selectionZ)
							if (cursor.position<f->GetLength()) cursor.selectionA=cursor.position, cursor.selectionZ=cursor.position+1;
							else return 0;
deleteSelection:		// . moving the content "after" Selection "to" the position of the Selection
						UINT posSrc=max(cursor.selectionA,cursor.selectionZ), posDst=min(cursor.selectionA,cursor.selectionZ);
						cursor.position=posDst; // moving the Cursor
						SELECTION_CANCEL()
						for( DWORD nBytesToMove=f->GetLength()-posSrc; nBytesToMove; ){
							BYTE buf[65536];
							f->Seek(posSrc,CFile::begin);
							const DWORD nBytesBuffered=f->Read(buf, min(nBytesToMove,sizeof(buf)) );
							f->Seek(posDst,CFile::begin);
							f->Write(buf,nBytesBuffered);
							nBytesToMove-=nBytesBuffered, posSrc+=nBytesBuffered, posDst+=nBytesBuffered;
						}
						// . the "source-destination" difference filled up with zeros
						if (posDst<minFileSize){
							static const BYTE Zero=0;
							for( f->Seek(posDst,CFile::begin); posDst++<minFileSize; f->Write(&Zero,1) );
							__informationWithCheckableShowNoMore__( _T("To preserve the minimum size of the content, it has been padded with zeros."), INI_MSG_PADDING );
						}
						// . refreshing the scrollbar
						__refreshVertically__();
						goto cursorRefresh;
					}
					case VK_BACK:
						// deleting the Byte before Cursor, or deleting the Selection
						// . if Selection not set, setting it as the Byte immediately before Cursor
						if (cursor.selectionA==cursor.selectionZ)
							if (cursor.position) cursor.selectionA=cursor.position-1, cursor.selectionZ=cursor.position;
							else return 0;
						// . moving the content "after" Selection "to" the position of the Selection
						goto deleteSelection;
					case VK_RETURN:
						// refocusing the window that has previously lost the focus in favor of this HexaEditor
						::SetFocus(hPreviouslyFocusedWnd);
						break;
					default:
						if (ctrl){
							// a shortcut other than Cursor positioning
							switch (wParam){
								case 'A':
editSelectAll:						// Selecting all
									cursor.selectionA=0, cursor.selectionZ=cursor.position=f->GetLength();
									goto cursorRefresh;
								case 'C':
editCopy:							// copying the Selection into clipboard
									if (cursor.selectionA!=cursor.selectionZ)
										( new COleBinaryDataSource(	f,
																	min(cursor.selectionA,cursor.selectionZ),
																	max(cursor.selectionZ,cursor.selectionA)
										) )->SetClipboard();
									return 0;
								case 'V':{
editPaste:							// pasting binary data from clipboard at the Position of Cursor
									COleDataObject odo;
									odo.AttachClipboard();
									if (const HGLOBAL hg=odo.GetGlobalData(cfBinary)){
										const DWORD *p=(PDWORD)::GlobalLock(hg), length=*p; // binary data are prefixed by their length
											f->Seek(cursor.position,CFile::begin);
											const DWORD lengthLimit=maxFileSize-cursor.position;
											if (length<=lengthLimit){
												SELECTION_CANCEL()
												f->Write( ++p, length );
												cursor.position+=length;
											}else{
												f->Write( ++p, lengthLimit );
												cursor.position+=lengthLimit;
												__showMessage__(MESSAGE_LIMIT_UPPER);
											}
										::GlobalUnlock(hg);
										::GlobalFree(hg);
									}
									goto cursorRefresh;
								}
							}
							return 0;
						}else if (!cursor.ascii) // here Hexa mode; Ascii mode handled in WM_CHAR
							// Hexa modification
							if (wParam>='0' && wParam<='9'){
								wParam-='0';
changeHalfbyte:					if (cursor.position<maxFileSize){
									SELECTION_CANCEL()
									BYTE b=0;
									f->Seek(cursor.position,CFile::begin);
									f->Read(&b,1);
									b= b<<4 | wParam;
									if (cursor.position<f->GetLength()) f->Seek(-1,CFile::current);
									f->Write(&b,1);
									if ( cursor.hexaLow=!cursor.hexaLow )
										cursor.position++;
									goto cursorRefresh;
								}else
									__showMessage__(MESSAGE_LIMIT_UPPER);
							}else if (wParam>='A' && wParam<='F'){
								wParam-='A'-10;
								goto changeHalfbyte;
							}
						break;
				}
				break;
			}
			case WM_CHAR:
				// character
				if (cursor.ascii) // here Ascii mode; Hexa mode handled in WM_KEYDOWN
					// Ascii modification
					if (::GetAsyncKeyState(VK_CONTROL)>=0 && ::isprint(wParam)) // Ctrl not pressed, thus character printable
						if (cursor.position<maxFileSize){
							SELECTION_CANCEL()
							f->Seek( cursor.position++ ,CFile::begin);
							f->Write(&wParam,1);
							goto cursorRefresh;
						}else
							__showMessage__(MESSAGE_LIMIT_UPPER);
				return 0;
			case WM_CONTEXTMENU:{
				// context menu invocation
				CMenu mnu;
				mnu.LoadMenu(IDR_HEXAEDITOR);
				register union{
					struct{ short x,y; };
					int i;
				};
				i=lParam;
				if (x==-1) x=y=0; // occurs if the context menu invoked using Shift+F10
				switch (mnu.GetSubMenu(0)->TrackPopupMenu( TPM_RETURNCMD, x,y, this )){
					case ID_EDIT_SELECT_ALL:
						goto editSelectAll;
					case ID_EDIT_COPY:
						goto editCopy;
					case ID_EDIT_PASTE:
						goto editPaste;
					case ID_DELETE:
						goto editDelete;
				}
				return 0; // to suppress CEdit's standard context menu
			}
			case WM_LBUTTONDOWN:
				// left mouse button pressed
				mouseDragged=false;
				goto leftMouseDragged; // "as if it's already been Dragged"
			case WM_LBUTTONUP:
				// left mouse button released
				mouseDragged=false;
				break;
			case WM_MOUSEMOVE:{
				// mouse moved
				if (!( mouseDragged=::GetAsyncKeyState(VK_LBUTTON)<0 )) return 0; // if mouse button not pressed, current Selection cannot be modified
leftMouseDragged:
				if (!editable) return 0; // if window disabled, focus cannot be acquired
				const int x=GET_X_LPARAM(lParam)-(addrLength+ADDRESS_SPACE_LENGTH)*font.charAvgWidth;
				const int r=GET_Y_LPARAM(lParam)/font.charHeight-HEADER_LINES_COUNT+GetScrollPos(SB_VERT);
				const int byteW=HEXA_FORMAT_LENGTH*font.charAvgWidth, hexaW=nBytesInRow*byteW;
				const int asciiX=hexaW+HEXA_SPACE_LENGTH*font.charAvgWidth;
				if (x>0 && x<=hexaW) // "x>0" - cannot be just "x" because x can be negative
					// Hexa area
					cursor.ascii=false, cursor.position=r*nBytesInRow+x/byteW;
				else if (x>asciiX && x<=asciiX+nBytesInRow*font.charAvgWidth)
					// Ascii area
					cursor.ascii=true, cursor.position=r*nBytesInRow+(x-asciiX)/font.charAvgWidth;
				else
					// outside any area
					break;
				CEdit::WindowProc(msg,wParam,lParam); // to set focus and accept WM_KEY* messages
				wParam=(WPARAM)hPreviouslyFocusedWnd; // due to the fallthrough
				//fallthrough
			}
			case WM_SETFOCUS:
				// window has received focus
				hPreviouslyFocusedWnd=(HWND)wParam; // the window that is losing the focus (may be refocused later when Enter is pressed)
				CreateSolidCaret( (2-cursor.ascii)*font.charAvgWidth, font.charHeight );
				if (editable) ShowCaret();
				goto cursorCorrectlyMoveTo;
			case WM_KILLFOCUS:
				// window has lost focus
				::DestroyCaret();
				if (CWnd *const pParentWnd=GetParent()) pParentWnd->Invalidate(FALSE);
				hPreviouslyFocusedWnd=0;
				break;
			case WM_VSCROLL:{
				// scrolling vertically
				// . determining the Row to scroll to
				SCROLLINFO si;
				GetScrollInfo( SB_VERT, &si, SIF_POS|SIF_TRACKPOS ); // getting 32-bit position
				switch (LOWORD(wParam)){
					case SB_PAGEUP:		// clicked into the gap above "thumb"
						si.nPos-=nRowsOnPage;	break;
					case SB_PAGEDOWN:	// clicked into the gap below "thumb"
						si.nPos+=nRowsOnPage; break;
					case SB_LINEUP:		// clicked on arrow up
						si.nPos--; break;
					case SB_LINEDOWN:	// clicked on arrow down
						si.nPos++; break;
					case SB_THUMBPOSITION:	// "thumb" released
					case SB_THUMBTRACK:		// "thumb" dragged
						si.nPos=si.nTrackPos;	break;
				}
				// . scrolling
				__scrollToRow__(si.nPos);
				::DestroyCaret();
				return 0;
			}
			case WM_ERASEBKGND:{
				// drawing the background
				// . header
				RECT r;
				GetClientRect(&r);
				r.bottom=font.charHeight;
				::FillRect((HDC)wParam,&r,CRideBrush::BtnFace);
				// . data
				GetClientRect(&r);
				r.top=font.charHeight;
				::FillRect((HDC)wParam,&r,CRideBrush::White);
				return r.bottom; // = window height
			}
			case WM_PAINT:{
				// drawing
				__refreshVertically__(); // to guarantee that the actual view is always drawn
				const CPaintDC dc(this);
				const HGDIOBJ hFont0=::SelectObject( dc, font );
					// . drawing background
					//nop (see WM_ERASEBKGND)
					// . drawing header text (header background drawn in WM_ERASEBKGND)
					RECT r;
					GetClientRect(&r);
					r.left=(addrLength+ADDRESS_SPACE_LENGTH)*font.charAvgWidth;
					TCHAR buf[16];
					__setNormalPrinting__(dc);
					::SetBkColor( dc, ::GetSysColor(COLOR_BTNFACE) );
					for( DWORD n=0; n<nBytesInRow; r.left+=HEXA_FORMAT_LENGTH*font.charAvgWidth )
						::DrawText( dc, buf, _stprintf(buf,HEXA_FORMAT,n++), &r, DT_LEFT );
					// . determining the visible part of the File content
					const DWORD iRowA=GetScrollPos(SB_VERT), iRowZ=min( nRowsInTotal, iRowA+nRowsOnPage );
					// . drawing Addresses and data (both Ascii and Hexa parts)
					DWORD iRow=iRowA, address=iRowA*nBytesInRow;
					f->Seek(address,CFile::begin);
					r.bottom=font.charHeight;
					const int _selectionA=min(cursor.selectionA,cursor.selectionZ), _selectionZ=max(cursor.selectionZ,cursor.selectionA);
					PEmphasis pEmp=emphases;
					while (pEmp->z<address) pEmp=pEmp->pNext; // choosing the first visible Emphasis
					for( const int xHexa=(addrLength+ADDRESS_SPACE_LENGTH)*font.charAvgWidth,xAscii=xHexa+(HEXA_FORMAT_LENGTH*nBytesInRow+HEXA_SPACE_LENGTH)*font.charAvgWidth; iRow++<=iRowZ; ){
						r.left=0, r.top=r.bottom, r.bottom+=font.charHeight;
						// : if the last Row is being drawn, erasing it along with everything that's "below" it (i.e. a sort of EraseBackground)
						if (address>=nRowsInTotal*nBytesInRow)
							::FillRect( dc, &r, CRideBrush::White );
						// : address
						if (addrLength){
							__setNormalPrinting__(dc);
							::SetBkColor( dc, ::GetSysColor(COLOR_BTNFACE) );
							::DrawText( dc, buf, _stprintf(buf,ADDRESS_FORMAT,HIWORD(address),LOWORD(address)), &r, DT_LEFT );
						}
						// : File content
						if (_selectionA<=address && address<_selectionZ) __setEmphasizedPrinting__(dc);
						else if (pEmp->a<=address && address<pEmp->z) __setSelectionPrinting__(dc);
						else __setNormalPrinting__(dc);
						RECT rcHexa=r, rcAscii=r;	rcHexa.left=xHexa, rcAscii.left=xAscii;
						for( BYTE bytes[BYTES_MAX],n=f->Read(bytes,nBytesInRow),*p=bytes; n--; address++ ){
							// | choosing colors
							if (_selectionA<=address && address<_selectionZ) __setEmphasizedPrinting__(dc);
							else if (pEmp->a<=address && address<pEmp->z) __setSelectionPrinting__(dc);
							else __setNormalPrinting__(dc);
							if (address==pEmp->z) pEmp=pEmp->pNext;
							// | Hexa
							int iByte=*p++, c=_stprintf(buf,HEXA_FORMAT,iByte);
							::DrawText( dc, buf,c, &rcHexa, DT_LEFT|DT_SINGLELINE|DT_VCENTER );
							rcHexa.left+=c*font.charAvgWidth;
							// | Ascii
							if (!::isprint(iByte)) iByte='.'; // if original character not printable, displaying a substitute one
							::DrawText( dc, (LPCTSTR)&iByte,1, &rcAscii, DT_LEFT|DT_SINGLELINE|DT_VCENTER );
							rcAscii.left+=font.charAvgWidth;
						}
					}
					// . the rest of client area is white
					const int y=r.bottom;
					GetClientRect(&r), r.top=y;
					::FillRect( dc, &r, CRideBrush::White );
				::SelectObject(dc,hFont0);
				return 0;
			}
		}
		return CEdit::WindowProc(msg,wParam,lParam);
	}











	CHexaEditor::COleBinaryDataSource::COleBinaryDataSource(CFile *_f,DWORD _dataBegin,DWORD dataEnd)
		// ctor
		// - initialization
		: f(_f) , dataBegin(_dataBegin) , dataLength(dataEnd-_dataBegin) {
		// - data target can inform BinaryDataSource on events
		DelaySetData( CRideApp::cfPasteSucceeded );
		// - rendering the data
		const HGLOBAL hg=::GlobalAlloc( GMEM_FIXED, sizeof(dataLength)+dataLength );
		const PDWORD p=(PDWORD)::GlobalLock(hg);
			*p=dataLength; // File content prefixed by its length
			f->Seek(dataBegin,CFile::begin);
			f->Read(1+p,dataLength); // actual content
		::GlobalUnlock(hg);
		CacheGlobalData( cfBinary, hg );
		// - delayed rendering of data
		//DelayRenderFileData( cfBinary );	// data to be supplied later
	}




/*	BOOL CHexaEditor::COleBinaryDataSource::OnRenderFileData(LPFORMATETC lpFormatEtc,CFile *pFile){
		// rendering File content from Image to COM
		if (lpFormatEtc->cfFormat==cfBinary){
			// exporting binary data "as if they were a file"
			DWORD nBytesToExport=dataLength, nBytesExported=0;
			pFile->Write(&nBytesExported,sizeof(nBytesExported)); // binary data prefixed by their length (set below)
				f->Seek(dataBegin,CFile::begin);
				BYTE buf[65536];
				for( WORD n=1+dataLength/sizeof(buf); n--; ){
					const DWORD nBytesBuffered=f->Read(buf, min(nBytesToExport,sizeof(buf)) );
					pFile->Write(buf,nBytesBuffered);
					nBytesToExport-=nBytesBuffered, nBytesExported+=nBytesBuffered;
				}
			pFile->Seek(0,CFile::begin);
			pFile->Write(&nBytesExported,sizeof(nBytesExported));
			pFile->Seek(0,CFile::end);
			return TRUE;
		}else
			// other form of rendering than the "File" one (i.e. other than CFSTR_FILECONTENTS)
			return COleDataSource::OnRenderFileData(lpFormatEtc,pFile);
	}*/
