#include "stdafx.h"

	#define INI_HEXAEDITOR	_T("HexaEdit")
	#define INI_MSG_PADDING	_T("msgpad")

	void CHexaEditor::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_HEXAEDITOR, messageId );
	}





	CHexaEditor::TCursor::TCursor(int position)
		// ctor
		: ascii(false) , hexaLow(true)
		, selectionA(-1) , selectionZ(-1) // nothing selected
		, position(position) {
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

	CHexaEditor::CHexaEditor(PVOID param,DWORD recordSize,TFnQueryRecordLabel fnQueryRecordLabel)
		// ctor
		// - initialization
		: font(_T("Courier New"),105,false,true)
		, recordSize(recordSize) , fnQueryRecordLabel(fnQueryRecordLabel) , nRowsPerRecord(1)
		, cursor(0) , param(param) , hPreviouslyFocusedWnd(0)
		, nBytesInRow(16) , editable(true) , addrLength(ADDRESS_FORMAT_LENGTH)
		, emphases((PEmphasis)&TEmphasis::Terminator) {
		// - comparing requested configuration with HexaEditor's skills
		ASSERT(	recordSize>=128 // making sure that entire Rows are either (1) well readable, (2) readable with error, or (3) non-readable; 128 = min Sector length
				? recordSize%128==0 // case: Record spans over entire Sectors
				: 128%recordSize==0 // case: Sector contains integral multiple of Records
			);
	}








	#define SELECTION_CANCEL()	cursor.selectionA=cursor.selectionZ=-1; // cancelling the Selection

	void CHexaEditor::SetEditable(bool _editable){
		// enables/disables possibility to edit the content of the File (see the Reset function)
		editable=_editable;
		if (::IsWindow(m_hWnd)){ // may be window-less if the owner is window-less
			cursor=TCursor( __firstByteInRowToLogicalPosition__(GetScrollPos(SB_VERT)) ); // resetting the Cursor and thus the Selection
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
		SetLogicalSize(f->GetLength());
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
			Invalidate(TRUE);
		}
	}

	void CHexaEditor::GetVisiblePart(DWORD &rLogicalBegin,DWORD &rLogicalEnd) const{
		// gets the beginning and end of visible portion of the File content
		const int i=GetScrollPos(SB_VERT);
		rLogicalBegin=__firstByteInRowToLogicalPosition__(i);
		rLogicalEnd=__firstByteInRowToLogicalPosition__(i+nRowsDisplayed);
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

	int CHexaEditor::__firstByteInRowToLogicalPosition__(int row) const{
		// converts Row begin (i.e. its first Byte) to corresponding position in underlying File and returns the result
		const div_t d=div(row,nRowsPerRecord);
		return d.quot*recordSize + d.rem*nBytesInRow;
	}

	int CHexaEditor::__logicalPositionToRow__(int logPos) const{
		// computes and returns at which Row is the specified LogicalPosition
		const div_t d=div(logPos,recordSize);
		return d.quot*nRowsPerRecord + d.rem/nBytesInRow;;// + (d.rem+nBytesInRow-1)/nBytesInRow;
	}

	int CHexaEditor::__getRecordIndexThatStartsAtRow__(int row) const{
		// computes and returns the zero-based record index that starts at specified Row; returns -1 if no record starts at given Row
		const div_t d=div(row,nRowsPerRecord);
		return d.rem ? -1 : d.quot;
	}

	#define HEADER_LINES_COUNT	1
	#define HEADER_HEIGHT		HEADER_LINES_COUNT*font.charHeight

	int CHexaEditor::__scrollToRow__(int row){
		// scrolls the HexaEditor so that the specified Row is shown as the first one from top; returns the Row number to which it has been really scrolled to
		// - Row must be in expected limits
		const int scrollMax=nLogicalRows-nRowsOnPage;
		if (row<0) row=0;
		else if (row>scrollMax) row=scrollMax;
		// - redrawing HexaEditor's client and non-client areas
		RECT rcScroll;
			GetClientRect(&rcScroll);
			rcScroll.bottom=( rcScroll.top=HEADER_HEIGHT )+nRowsDisplayed*font.charHeight;
		ScrollWindow( 0, (GetScrollPos(SB_VERT)-row)*font.charHeight, &rcScroll, &rcScroll );
		// - displaying where it's been scrolled to
		SetScrollPos(SB_VERT,row,TRUE); // True = redrawing the scroll-bar, not HexaEditor's canvas!
		return row;
	}

	void CHexaEditor::__refreshVertically__(){
		// refreshes all parameters that relate to vertical axis
		// - determining the total number of Rows
		nRowsPerRecord=(recordSize+nBytesInRow-1)/nBytesInRow;
		nLogicalRows=__logicalPositionToRow__( max(f->GetLength(),logicalSize) );
		// - setting the scrolling dimensions
		RECT r;
		GetClientRect(&r);
		nRowsDisplayed=max( 0, (r.bottom-r.top)/font.charHeight-HEADER_LINES_COUNT );
		nRowsOnPage=max( 0, nRowsDisplayed-1 );
		const BOOL scrollbarNecessary=nRowsOnPage<nLogicalRows;
		ShowScrollBar(SB_VERT,scrollbarNecessary);
		//if (scrollbarNecessary){
			SCROLLINFO si={ sizeof(si), SIF_RANGE|SIF_PAGE, 0,nLogicalRows-1, nRowsOnPage };
			SetScrollInfo( SB_VERT, &si, TRUE );
		//}
	}

	void CHexaEditor::__invalidateData__() const{
		// invalidates the "data" (the content below the Header)
		RECT rc;
		GetClientRect(&rc);
		rc.top=HEADER_HEIGHT;
		::InvalidateRect( m_hWnd, &rc, FALSE );
	}

	#define BYTES_MAX		64

	#define HEXA_FORMAT			_T("%02X ")
	#define HEXA_FORMAT_LENGTH	3
	#define HEXA_SPACE_LENGTH	2

	void CHexaEditor::__refreshCursorDisplay__() const{
		// shows Cursor on screen at position that corresponds with Cursor's actual Position in the underlying File content (e.g. the 12345-th Byte of the File)
		const div_t d=div(cursor.position,recordSize);
		const int iScrollY=GetScrollPos(SB_VERT);
		//if (d.quot>=iScrollY){ // commented out as always guaranteed
			// Cursor "under" the header
			POINT pos={	d.rem % nBytesInRow, // translated below to a particular pixel position
						(HEADER_LINES_COUNT + __logicalPositionToRow__(cursor.position) - iScrollY)*font.charHeight // already a particular Y pixel position
					};
			if (cursor.ascii) // Cursor in the Ascii area
				pos.x=(addrLength+ADDRESS_SPACE_LENGTH+HEXA_FORMAT_LENGTH*nBytesInRow+HEXA_SPACE_LENGTH+pos.x)*font.charAvgWidth;
			else // Cursor in the Hexa area
				pos.x=(addrLength+ADDRESS_SPACE_LENGTH+HEXA_FORMAT_LENGTH*pos.x)*font.charAvgWidth;
			SetCaretPos(pos);
		/*}else{ // commented out as it never occurs
			// Cursor "above" the header
			static const POINT Pos={ -100, -100 };
			SetCaretPos(Pos);
		}*/
	}

	void CHexaEditor::__showMessage__(LPCTSTR msg) const{
		// shows Message and passes focus back to the HexaEditor
		Utils::Information(msg);
		::PostMessage( m_hWnd, WM_LBUTTONDOWN, 0, -1 ); // recovering the focus; "-1" = [x,y] = nonsense value; can't use mere SetFocus because this alone doesn't work
	}

	#define MESSAGE_LIMIT_UPPER	_T("The content has reached its upper limit.")

	static bool mouseDragged;

	#define CLIPFORMAT_BINARY	_T("RideHexaEditBinary")

	static UINT cfBinary;

	LRESULT CHexaEditor::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		int i;
		const int cursorPos0=cursor.position;
		switch (msg){
			case WM_MOUSEACTIVATE:
				// preventing the focus from being stolen by the parent
				return MA_ACTIVATE;
			case WM_CREATE:
				// window created
				// . base
				if (__super::WindowProc(msg,wParam,lParam)<0) return -1;
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
						if (ctrl) // Ctrl+Left shortcut navigates to the previous Record (or the beginning of current Record, if Cursor not already there)
							cursor.position= cursor.position/recordSize * recordSize;
cursorCorrectlyMoveTo:	// . adjusting the Cursor's Position
						cursor.hexaLow=true; // the next keystroke will modify the lower four bits of current hexa-value
						if (cursor.position<0) cursor.position=0;
						else if (cursor.position>maxFileSize) cursor.position=maxFileSize;
						// . adjusting an existing Selection if Shift pressed
						if (!mouseDragged){ // if mouse is being -Dragged, the beginning of a Selection has already been detected
							if (cursor.selectionA!=cursor.selectionZ) // if there was a Selection before ...
								__invalidateData__(); // ... invalidating the content as the Selection may no longer be valid (e.g. may be deselected)
							cursor.__detectNewSelection__();
						}
						cursor.selectionZ=cursor.position;
cursorRefresh:			// . refreshing the Cursor
						HideCaret();
							// : scrolling if Cursor has been moved to an invisible part of the File content
							const int iRow=__logicalPositionToRow__(cursor.position), iScrollY=GetScrollPos(SB_VERT);
							if (iRow<iScrollY) __scrollToRow__(iRow);
							else if (iRow>=iScrollY+nRowsOnPage) __scrollToRow__(iRow-nRowsOnPage+1);
							// : invalidating the content if Selection has (or is being) changed
							if (cursor.position!=cursorPos0) // yes, the Cursor has moved
								if (mouseDragged || ::GetAsyncKeyState(VK_SHIFT)<0) // yes, Selection is being edited (by dragging the mouse or having the Shift key pressed)
									__invalidateData__();
							// : displaying the Cursor
							__refreshCursorDisplay__();
						ShowCaret();
						return 0;
					}
					case VK_RIGHT:
						if (ctrl) // Ctrl+Right short navigates to the next Record
							cursor.position= (cursor.position/recordSize+1) * recordSize;
						else
							cursor.position++;
						goto cursorCorrectlyMoveTo;
					case VK_UP:{
						i=1; // move Cursor one row up
moveCursorUp:			const int iRow=__logicalPositionToRow__(cursor.position);
						if (ctrl){
							const int iScrollY=__scrollToRow__(GetScrollPos(SB_VERT)-i);
							if (iRow<iScrollY+nRowsOnPage) goto cursorRefresh;
						}
						const int currRowStart=__firstByteInRowToLogicalPosition__(iRow);
						cursor.position -=	currRowStart-__firstByteInRowToLogicalPosition__(iRow-i+1) // # of Bytes between current and "target" row to place the Cursor to (if I=1, this difference is zero)
											+
											max(cursor.position-currRowStart+1,
												currRowStart-__firstByteInRowToLogicalPosition__(iRow-i)
											);
						goto cursorCorrectlyMoveTo;
					}
					case VK_DOWN:{
						i=1; // move Cursor one row down
moveCursorDown:			const int iRow=__logicalPositionToRow__(cursor.position);
						if (ctrl){
							const int iScrollY=__scrollToRow__(GetScrollPos(SB_VERT)+i);
							if (iRow>=iScrollY) goto cursorRefresh;
						}
						const int currRowStart=__firstByteInRowToLogicalPosition__(iRow), targetRowStart=__firstByteInRowToLogicalPosition__(iRow+i);
						cursor.position +=	targetRowStart-__firstByteInRowToLogicalPosition__(iRow+1) // # of Bytes between current and "target" row to place the Cursor to (if I=1, this difference is zero)
											+
											min(targetRowStart-currRowStart,
												__firstByteInRowToLogicalPosition__(iRow+i+1)-cursor.position-1
											);
						goto cursorCorrectlyMoveTo;
					}
					case VK_PRIOR:	// page up
						i=nRowsOnPage-ctrl; // move Cursor N rows up
						goto moveCursorUp;
					case VK_NEXT:	// page down
						i=nRowsOnPage-ctrl; // move Cursor N rows down
						goto moveCursorDown;
					case VK_HOME:
						cursor.position=( ctrl ? 0 : __firstByteInRowToLogicalPosition__(__logicalPositionToRow__(cursor.position)) );
						goto cursorCorrectlyMoveTo;
					case VK_END:
						cursor.position=( ctrl ? f->GetLength() : __firstByteInRowToLogicalPosition__(__logicalPositionToRow__(cursor.position)+1)-1 );
						goto cursorCorrectlyMoveTo;
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
							if (!nBytesBuffered) break; // no Bytes buffered if, for instance, Sector not found
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
						__invalidateData__();
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
									__invalidateData__();
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
									__invalidateData__();
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
									__invalidateData__();
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
							__invalidateData__();
							goto cursorRefresh;
						}else
							__showMessage__(MESSAGE_LIMIT_UPPER);
				return 0;
			case WM_CONTEXTMENU:{
				// context menu invocation
				if (!editable) return 0; // if window disabled, no context actions can be performed
				CMenu mnu;
				mnu.LoadMenu(IDR_HEXAEDITOR);
				register union{
					struct{ short x,y; };
					int i;
				};
				i=lParam;
				if (x==-1){ // occurs if the context menu invoked using Shift+F10
					POINT caretPos=GetCaretPos();
					ClientToScreen(&caretPos);
					x=caretPos.x+(1+!cursor.ascii)*font.charAvgWidth, y=caretPos.y+font.charHeight;
				}
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
				if (!editable) break; // if window NotEditable, ignoring any mouse events and just focusing the window to receive MouseWheel messages
				const int x=GET_X_LPARAM(lParam)-(addrLength+ADDRESS_SPACE_LENGTH)*font.charAvgWidth;
				const int r=GET_Y_LPARAM(lParam)/font.charHeight-HEADER_LINES_COUNT+GetScrollPos(SB_VERT);
				const int byteW=HEXA_FORMAT_LENGTH*font.charAvgWidth, hexaW=nBytesInRow*byteW;
				const int asciiX=hexaW+HEXA_SPACE_LENGTH*font.charAvgWidth;
				const int currLineStart=__firstByteInRowToLogicalPosition__(r), currLineBytesMinusOne=__firstByteInRowToLogicalPosition__(r+1)-currLineStart-1;
				if (x>0 && x<=hexaW) // "x>0" - cannot be just "x" because x can be negative
					// Hexa area
					cursor.ascii=false, cursor.position=currLineStart+min(x/byteW,currLineBytesMinusOne);
				else if (x>asciiX && x<=asciiX+nBytesInRow*font.charAvgWidth)
					// Ascii area
					cursor.ascii=true, cursor.position=currLineStart+min((x-asciiX)/font.charAvgWidth,currLineBytesMinusOne);
				else{
					// outside any area
					if (!mouseDragged){ // if right now mouse button pressed ...
						SELECTION_CANCEL(); // ... unselecting everything
						Invalidate(FALSE); // ... and painting the result
					}
					break;
				}
				__super::WindowProc(msg,wParam,lParam); // to set focus and accept WM_KEY* messages
				wParam=(WPARAM)hPreviouslyFocusedWnd; // due to the fallthrough
				//fallthrough
			}
			case WM_SETFOCUS:
				// window has received focus
				hPreviouslyFocusedWnd=(HWND)wParam; // the window that is losing the focus (may be refocused later when Enter is pressed)
				if (!editable) return 0;
				CreateSolidCaret( (2-cursor.ascii)*font.charAvgWidth, font.charHeight );
				ShowCaret();
				goto cursorCorrectlyMoveTo;
			case WM_KILLFOCUS:
				// window has lost focus
				::DestroyCaret();
				if (CWnd *const pParentWnd=GetParent()) pParentWnd->Invalidate(FALSE);
				hPreviouslyFocusedWnd=0;
				break;
			case WM_MOUSEWHEEL:{
				// mouse wheel was rotated
				UINT nLinesToScroll;
				::SystemParametersInfo( SPI_GETWHEELSCROLLLINES, 0, &nLinesToScroll, 0 );
				const short zDelta=(short)HIWORD(wParam);
				if (nLinesToScroll==WHEEL_PAGESCROLL)
					SendMessage( WM_VSCROLL, zDelta>0?SB_PAGEUP:SB_PAGEDOWN, 0 );
				else
					for( WORD nLines=abs(zDelta)*nLinesToScroll/WHEEL_DELTA; nLines--; SendMessage(WM_VSCROLL,zDelta>0?SB_LINEUP:SB_LINEDOWN) );
				return TRUE;
			}
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
			case WM_ERASEBKGND:
				// drawing the background
				return TRUE; // nop (always drawing over existing content)
			case WM_PAINT:{
				// drawing
				__refreshVertically__(); // to guarantee that the actual view is always drawn
				class CHexaPaintDC sealed:public CPaintDC{
					const bool hexaEditorEditable;
					BYTE currContentFlags;
					COLORREF currEmphasisColor;
				public:
					enum TContentFlags:BYTE{
						Normal		=0,
						Selected	=1,
						Erroneous	=2
					};

					const COLORREF SelectionColor;

					CHexaPaintDC(CHexaEditor *pHexaEditor)
						: CPaintDC(pHexaEditor)
						, hexaEditorEditable(pHexaEditor->editable)
						, SelectionColor(::GetSysColor(COLOR_HIGHLIGHT))
						, currContentFlags(-1) , currEmphasisColor(GetBkColor()) {
					}

					void SetContentPrintState(BYTE newContentFlags,COLORREF newEmphasisColor){
						if (newContentFlags&Erroneous){
							// Erroneous content
							// : TextColor is (some tone of) Red
							if (!(currContentFlags&Erroneous)) // "if previously not Erroneous"
								SetTextColor( !hexaEditorEditable*0x777700+0xff );
							// : BackgroundColor is the EmphasisColor overlayed with SelectionColor
blendEmphasisAndSelection:	if (newEmphasisColor!=currEmphasisColor || newContentFlags!=currContentFlags) // "if EmphasisColor or the application of SelectionColor have changed"
								SetBkColor(	newContentFlags&Selected
											? Utils::GetBlendedColor(newEmphasisColor,SelectionColor,.6f) // need to overlay EmphasisColor with SelectionColor
											: newEmphasisColor // EmphasisColor only
										);
						}else if (!newContentFlags){
							// Normal (not Selected) content
							// : TextColor is (some tone of) Black
							if (currContentFlags) // "if previously not Normal"
								SetTextColor( ::GetSysColor(hexaEditorEditable?COLOR_WINDOWTEXT:COLOR_GRAYTEXT) );
							// : BackgroundColor is the EmphasisColor
							goto blendEmphasisAndSelection;
						}else
							// Selected content
							if (newEmphasisColor!=currEmphasisColor || newContentFlags!=currContentFlags){ // "if EmphasisColor or the application of SelectionColor have changed"
								// : TextColor is either Black or White, whichever is in higher contrast to CurrentBackgroundColor
								const COLORREF bgColor=Utils::GetBlendedColor(newEmphasisColor,SelectionColor,.6f); // need to overlay EmphasisColor with SelectionColor
								const WORD rgbSum = *(PCBYTE)&bgColor + ((PCBYTE)&bgColor)[1] + ((PCBYTE)&bgColor)[2];
								SetTextColor( rgbSum>0x180 ? COLOR_BLACK : COLOR_WHITE );
								// : BackgroundColor is the EmphasisColor overlayed with SelectionColor
								SetBkColor(bgColor);
							}
						currContentFlags=newContentFlags, currEmphasisColor=newEmphasisColor;
					}
				} dc(this);
				const HGDIOBJ hFont0=::SelectObject( dc, font );
					// . drawing Header
					RECT rcClip;
						GetClientRect(&rcClip);
						rcClip.top=dc.m_ps.rcPaint.top, rcClip.bottom=dc.m_ps.rcPaint.bottom;
					if (rcClip.top<HEADER_HEIGHT){ // Header drawn only if its region invalid
						RECT rcHeader={ 0, 0, rcClip.right, HEADER_HEIGHT };
						::FillRect( dc, &rcHeader, CRideBrush::BtnFace );
						TCHAR buf[16];
						dc.SetContentPrintState( CHexaPaintDC::Normal, ::GetSysColor(COLOR_BTNFACE) );
						rcHeader.left=(addrLength+ADDRESS_SPACE_LENGTH)*font.charAvgWidth;
						for( BYTE n=0; n<nBytesInRow&&rcHeader.left<rcHeader.right; rcHeader.left+=HEXA_FORMAT_LENGTH*font.charAvgWidth )
							dc.DrawText( buf, ::wsprintf(buf,HEXA_FORMAT,n++), &rcHeader, DT_LEFT|DT_TOP );
					}
					// . determining the visible part of the File content
					const int iRowFirstToPaint=max( (rcClip.top-HEADER_HEIGHT)/font.charHeight, 0 );
					int iRowA= GetScrollPos(SB_VERT) + iRowFirstToPaint;
					const int nPhysicalRows=__logicalPositionToRow__( min(f->GetLength(),logicalSize) );
					const int iRowLastToPaint= GetScrollPos(SB_VERT) + (rcClip.bottom-HEADER_HEIGHT)/font.charHeight + 1;
					const int iRowZ=min( min(nPhysicalRows,iRowLastToPaint), iRowA+nRowsOnPage );
					// . filling the gaps between Addresses/Hexa/Ascii, and Label space to erase any previous Label
					const int xHexaStart=(addrLength+ADDRESS_SPACE_LENGTH)*font.charAvgWidth, xHexaEnd=xHexaStart+HEXA_FORMAT_LENGTH*nBytesInRow*font.charAvgWidth;
					const int xAsciiStart=xHexaEnd+HEXA_SPACE_LENGTH*font.charAvgWidth, xAsciiEnd=xAsciiStart+nBytesInRow*font.charAvgWidth;
					RECT r={ min(addrLength*font.charAvgWidth,rcClip.right), max(rcClip.top,HEADER_HEIGHT), min(xHexaStart,rcClip.right), rcClip.bottom }; // Addresses-Hexa space; min(.) = to not paint over the scrollbar
					::FillRect( dc, &r, CRideBrush::White );
					r.left=min(xHexaEnd,rcClip.right), r.right=min(xAsciiStart,rcClip.right); // Hexa-Ascii space; min(.) = to not paint over the scrollbar
					::FillRect( dc, &r, CRideBrush::White );
					r.left=min(xAsciiEnd,rcClip.right), r.right=min(rcClip.right,rcClip.right); // Label space; min(.) = to not paint over the scrollbar
					::FillRect( dc, &r, CRideBrush::White );
					// . drawing Addresses and data (both Ascii and Hexa parts)
					const COLORREF labelColor=Utils::GetSaturatedColor(::GetSysColor(COLOR_GRAYTEXT),1.7f+.1f*!editable);
					const CRidePen recordDelimitingHairline( 0, labelColor );
					const HGDIOBJ hPen0=::SelectObject( dc, recordDelimitingHairline );
						int address=__firstByteInRowToLogicalPosition__(iRowA), y=HEADER_HEIGHT+iRowFirstToPaint*font.charHeight;
						const int _selectionA=min(cursor.selectionA,cursor.selectionZ), _selectionZ=max(cursor.selectionZ,cursor.selectionA);
						PEmphasis pEmp=emphases;
						while (pEmp->z<address) pEmp=pEmp->pNext; // choosing the first visible Emphasis
						f->Seek( address, CFile::begin );
						for( TCHAR buf[16]; iRowA<=iRowZ; iRowA++,y+=font.charHeight ){
							RECT rcHexa={ /*xHexaStart*/0, y, min(xHexaEnd,rcClip.right), min(y+font.charHeight,rcClip.bottom) }; // commented out as this rectangle also used to paint the Address
							RECT rcAscii={ min(xAsciiStart,rcClip.right), y, min(xAsciiEnd,rcClip.right), rcHexa.bottom };
							// : address
							if (addrLength){
								dc.SetContentPrintState( CHexaPaintDC::Normal, ::GetSysColor(COLOR_BTNFACE) );
								dc.DrawText( buf, ::wsprintf(buf,ADDRESS_FORMAT,HIWORD(address),LOWORD(address)), &rcHexa, DT_LEFT|DT_TOP );
							}
							rcHexa.left=xHexaStart;
							// : File content
							const bool isEof=f->GetPosition()==f->GetLength();
							BYTE bytes[BYTES_MAX],const nBytesExpected=__firstByteInRowToLogicalPosition__(iRowA+1)-address;
							::SetLastError(ERROR_SUCCESS); // clearing any previous error
							BYTE nBytesRead=f->Read(bytes,nBytesExpected);
							BYTE printFlags=::GetLastError()==ERROR_SUCCESS // File content readable without error
											? CHexaPaintDC::Normal
											: CHexaPaintDC::Erroneous;
							if (nBytesRead)
								// entire Row available (guaranteed thanks to checks in the ctor)
								for( const BYTE *p=bytes; nBytesRead--; address++ ){
									// | choosing colors
									COLORREF emphasisColor;
									if (_selectionA<=address && address<_selectionZ)
										printFlags|=CHexaPaintDC::Selected;
									else
										printFlags&=~CHexaPaintDC::Selected;
									if (pEmp->a<=address && address<pEmp->z)
										emphasisColor=COLOR_YELLOW;
									else{
										emphasisColor=COLOR_WHITE;
										if (address==pEmp->z) pEmp=pEmp->pNext;
									}
									dc.SetContentPrintState( printFlags, emphasisColor );
									// | Hexa
									const int iByte=*p++;
									dc.DrawText( buf, ::wsprintf(buf,HEXA_FORMAT,iByte), &rcHexa, DT_LEFT|DT_TOP );
									rcHexa.left+=HEXA_FORMAT_LENGTH*font.charAvgWidth;
									// | Ascii
									::DrawTextW(dc,
												::isprint(iByte) ? (LPCWSTR)&iByte : L"\x2219", 1, // if original character not printable, displaying a substitute one
												&rcAscii, DT_LEFT|DT_TOP|DT_NOPREFIX
											);
									rcAscii.left+=font.charAvgWidth;
								}
							else if (!isEof){
								// content not available (e.g. irrecoverable Sector read error)
								f->Seek( address+=nBytesExpected, CFile::begin );
								#define ERR_MSG	_T("» No data «")
								dc.SetContentPrintState( printFlags, COLOR_WHITE );
								dc.DrawText( ERR_MSG, -1, &rcHexa, DT_LEFT|DT_TOP );
								rcHexa.left+=(sizeof(ERR_MSG)-sizeof(TCHAR))*font.charAvgWidth;
							}
							// : filling the rest of the Row with background color (e.g. the last Row in a Record may not span up to the end)
							if (rcHexa.left<rcHexa.right) // to not paint over the scrollbar
								::FillRect( dc, &rcHexa, CRideBrush::White );
							if (rcAscii.left<rcAscii.right) // to not paint over the scrollbar
								::FillRect( dc, &rcAscii, CRideBrush::White );
							// : drawing the Record label if the just drawn Row is the Record's first Row
							if (fnQueryRecordLabel && !isEof){ // yes, a new Record can potentially start at the Row
								const int recordIndex=__getRecordIndexThatStartsAtRow__(iRowA);
								if (recordIndex>=0){ // yes, a new Record starts at the Row
									TCHAR buf[80];
									if (const LPCTSTR recordLabel=fnQueryRecordLabel(recordIndex,buf,sizeof(buf)/sizeof(TCHAR),param)){
										RECT rc={ rcAscii.right+2*font.charAvgWidth, y, rcClip.right, rcClip.bottom };
										const COLORREF textColor0=dc.SetTextColor(labelColor), bgColor0=dc.SetBkColor(COLOR_WHITE);
											dc.DrawText( recordLabel, -1, &rc, DT_LEFT|DT_TOP );
											dc.MoveTo( addrLength*font.charAvgWidth, y );
											dc.LineTo( rcClip.right, y );
										dc.SetTextColor(textColor0), dc.SetBkColor(bgColor0);
									}
								}
							}
						}
					::SelectObject(dc,hPen0);
					// . filling the rest of HexaEditor with background color
					rcClip.top=y;
					::FillRect( dc, &rcClip, CRideBrush::White );
				::SelectObject(dc,hFont0);
				return 0;
			}
		}
		return __super::WindowProc(msg,wParam,lParam);
	}

	void CHexaEditor::PostNcDestroy(){
		// self-destruction
		//nop (View destroyed by its owner)
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
			f->Seek(dataBegin,CFile::begin);
			*p=f->Read(1+p,dataLength); // File content is prefixed by its length
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
