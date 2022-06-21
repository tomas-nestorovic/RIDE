#include "stdafx.h"

	#define INI_HEXAEDITOR	_T("HexaEdit")
	#define INI_MSG_PADDING	_T("msgpad")

	void CHexaEditor::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_HEXAEDITOR, messageId );
	}





	CHexaEditor::TCaret::TCaret(int position)
		// ctor
		: ascii(false) , hexaLow(true)
		, selectionA(position) , selectionZ(position) { // nothing selected
	}
	CHexaEditor::TCaret &CHexaEditor::TCaret::operator=(const TCaret &r){
		// copy assignment operator
		return *(TCaret *)(  ::memcpy( this, &r, sizeof(*this) )  );
	}
	void CHexaEditor::TCaret::__detectNewSelection__(){
		// detects and sets the beginning of new Selection
		if (::GetAsyncKeyState(VK_SHIFT)>=0) // if Shift NOT pressed ...
			selectionA=position;	// ... we have detected a new Selection
	}






	#define BOOKMARK_POSITION_INFINITY	INT_MAX

	void CHexaEditor::CBookmarks::__addBookmark__(int logPos){
		// adds a new Bookmark at specified LogicalPosition (if not already added there)
		if (__getNearestNextBookmarkPosition__(logPos)==logPos) // if Bookmark already present ...
			return; // ... we are done
		auto i=GetSize();
		while (i>0)
			if ((*this)[i-1]>logPos)
				i--;
			else
				break;
		InsertAt( i, logPos ); // adding a new Bookmark so that the Bookmarks are ordered ascending
	}
	void CHexaEditor::CBookmarks::__removeBookmark__(int logPos){
		// removes existing Bookmark from specified LogicalPosition (if not already removed before)
		for( auto i=GetSize(); i>0; )
			if ((*this)[--i]==logPos){
				RemoveAt(i);
				break;
			}
	}
	void CHexaEditor::CBookmarks::__removeAllBookmarks__(){
		// removes all Bookmarks
		RemoveAll();
	}
	int CHexaEditor::CBookmarks::__getNearestNextBookmarkPosition__(int logPos) const{
		// finds and returns the Bookmark at LogicalPosition or the nearest next Bookmark
		auto i=GetSize();
		while (i>0)
			if ((*this)[i-1]>=logPos)
				i--;
			else
				break;
		return	i<GetSize()
				? (*this)[i] // nearest next Bookmark found
				: BOOKMARK_POSITION_INFINITY; // no nearest next Bookmark found - considering it in infinity
	}







	CHexaEditor::CSearch::CSearch()
		// ctor
		: f(nullptr)
		, type(ASCII_ANY_CASE) , patternLength(0) {
	}

	static bool EqualBytes(BYTE b1,BYTE b2){
		return b1==b2;
	}

	static bool EqualChars(BYTE b1,BYTE b2){
		return ::strnicmp( (LPCSTR)&b1, (LPCSTR)&b2, 1 )==0;
	}

	static bool InequalBytes(BYTE b1,BYTE b2){
		return b1!=b2;
	}

	#define F	search.f

	UINT AFX_CDECL CHexaEditor::CSearch::SearchForward_thread(PVOID pCancelableAction){
		// thread to perform forward search of specified Pattern
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		CSearch &search=*(CSearch *)pAction->GetParams();
		// - determining Comparer function
		typedef bool (* FnComparer)(BYTE b1,BYTE b2);
		FnComparer cmp;
		switch (search.type){
			case CSearch::HEXA:
			case CSearch::ASCII_MATCH_CASE:
				cmp=EqualBytes;
				break;
			case CSearch::ASCII_ANY_CASE:
				cmp=EqualChars;
				break;
			case CSearch::NOT_BYTE:
				cmp=InequalBytes;
				break;
			default:
				ASSERT(FALSE);
				return pAction->TerminateWithError( ERROR_NOT_SUPPORTED );
		}
		// - preparation for KMP search (Knuth-Morris-Pratt)
		BYTE pie[sizeof(search.pattern.bytes)];
		*pie=0;
		for( BYTE i=1,k=0; i<search.patternLength; i++ ){
			while (k>0 && search.pattern.bytes[k]!=search.pattern.bytes[i])
				k=pie[k-1];
			k+=search.pattern.bytes[k]==search.pattern.bytes[i];
			pie[i]=k;
		}
		// - search for next match of the Pattern
		pAction->SetProgressTarget( F->GetLength()+1 ); // "+1" = to not preliminary end the search thread
		auto fEnd=F->GetLength();
		do{
			F->Seek( search.logPosFound, CFile::begin );
			for( BYTE posMatched=0,b; F->GetPosition()<fEnd; pAction->UpdateProgress(F->GetPosition()) )
				if (pAction->Cancelled)
					return ERROR_CANCELLED;
				else if (!F->Read( &b, 1 )){
					F->Seek( 1, CFile::current ); // skipping irrecoverable portion of data ...
					posMatched=0; // ... and beginning with Pattern comparison from scratch
				}else{
					while (posMatched>0 && !cmp(b,search.pattern.bytes[posMatched]) )
						posMatched=pie[posMatched-1];
					posMatched+=cmp( b, search.pattern.bytes[posMatched] );
					if (posMatched==search.patternLength){
						search.logPosFound=F->GetPosition()-search.patternLength;
						return pAction->TerminateWithSuccess();
					}
				}
			if (!search.logPosFound) // searched through the whole content?
				break;
			if (!Utils::QuestionYesNo( _T("No match found yet.\nContinue from the beginning?"), MB_DEFBUTTON1 ))
				break;
			fEnd=search.logPosFound;
			search.logPosFound=0;
		}while (true);
		// - no match found
		return pAction->TerminateWithError( ERROR_NOT_FOUND );
	}

	TStdWinError CHexaEditor::CSearch::FindNextPositionModal(){
		// finds and returns Position of the next occurence of Pattern, starting From specified Position
		return	CBackgroundActionCancelable(
					SearchForward_thread,
					this,
					THREAD_PRIORITY_BELOW_NORMAL
				).Perform();
	}







	static struct TDefaultContentAdviser sealed:public CHexaEditor::IContentAdviser{
		void GetRecordInfo(int logPos,PINT pOutRecordStartLogPos,PINT pOutRecordLength,bool *pOutDataReady) override{
			// retrieves the start logical position and length of the Record pointed to by the input LogicalPosition
			if (pOutRecordStartLogPos)
				*pOutRecordStartLogPos=0;
			if (pOutRecordLength)
				*pOutRecordLength=HEXAEDITOR_RECORD_SIZE_INFINITE;
			if (pOutDataReady)
				*pOutDataReady=true;
		}
		int LogicalPositionToRow(int logPos,BYTE nBytesInRow) override{
			// computes and returns the row containing the specified LogicalPosition
			return logPos/nBytesInRow;
		}
		int RowToLogicalPosition(int row,BYTE nBytesInRow) override{
			// converts Row begin (i.e. its first Byte) to corresponding logical position in underlying File and returns the result
			return row*nBytesInRow;
		}
		LPCWSTR GetRecordLabelW(int logPos,PWCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const override{
			// populates the Buffer with label for the Record that STARTS at specified LogicalPosition, and returns the Buffer; returns Null if no Record starts at specified LogicalPosition
			return nullptr;
		}
	} DefaultContentAdviser;

	#define WM_HEXA_PAINTSCROLLBARS	WM_USER+1

	#define ADDRESS_FORMAT			_T(" %04X-%04X")
	#define ADDRESS_FORMAT_LENGTH	10
	#define ADDRESS_SPACE_LENGTH	1

	const CHexaEditor::TEmphasis CHexaEditor::TEmphasis::Terminator={ INT_MAX, INT_MAX };

	CHexaEditor::CHexaEditor(PVOID param,HMENU customSelectSubmenu,HMENU customResetSubmenu,HMENU customGotoSubmenu)
		// ctor
		// - initialization
		: font(FONT_COURIER_NEW,105,false,true)
		, customSelectSubmenu(customSelectSubmenu) , customResetSubmenu(customResetSubmenu) , customGotoSubmenu(customGotoSubmenu)
		, hDefaultAccelerators(::LoadAccelerators(app.m_hInstance,MAKEINTRESOURCE(IDR_HEXAEDITOR)))
		, caret(0) , param(param) , hPreviouslyFocusedWnd(0)
		, logPosScrolledTo(0)
		, pContentAdviser(&DefaultContentAdviser)
		, nBytesInRow(16) , editable(true) , addrLength(ADDRESS_FORMAT_LENGTH)
		, emphases((PEmphasis)&TEmphasis::Terminator) {
		// - comparing requested configuration with HexaEditor's skills
		/*ASSERT(	recordSize>=128 // making sure that entire Rows are either (1) well readable, (2) readable with error, or (3) non-readable; 128 = min Sector length
				? recordSize%128==0 // case: Record spans over entire Sectors
				: 128%recordSize==0 // case: Sector contains integral multiple of Records
			);*/
	}

	CHexaEditor::~CHexaEditor(){
		// dtor
		// - destroying the Accelerator table
		::DestroyAcceleratorTable(hDefaultAccelerators);
	}







	static bool mouseInNcArea;

	void CHexaEditor::SetEditable(bool _editable){
		// enables/disables possibility to edit the content of the File (see the Reset function)
		editable=_editable;
		if (::IsWindow(m_hWnd)){ // may be window-less if the owner is window-less
			if (::GetFocus()==m_hWnd){
				__refreshCaretDisplay__();
				SetFocus();
				ShowCaret();
			}
			Invalidate(FALSE);
		}
	}

	bool CHexaEditor::IsEditable() const{
		// True <=> content can be edited, otherwise False
		return editable;
	}

	int CHexaEditor::ShowAddressBand(bool _show){
		// shows/hides the Address bar; returns the width of the Address bar
		addrLength= _show ? ADDRESS_FORMAT_LENGTH : 0;
		if (::IsWindow(m_hWnd)) // may be window-less if the owner is window-less
			Invalidate(FALSE);
		return ADDRESS_FORMAT_LENGTH*font.charAvgWidth;
	}

	void CHexaEditor::Update(CFile *f){
		// updates the underlying File content
		locker.Lock();
			F=nullptr; // anything previously set is now invalid
			if (!( pContentAdviser=dynamic_cast<PContentAdviser>(  F=f  ) ))
				pContentAdviser=&DefaultContentAdviser;
			SetLogicalSize(F->GetLength());
		locker.Unlock();
	}

	void CHexaEditor::Update(CFile *f,int minFileSize,int maxFileSize){
		// updates the underlying File content
		locker.Lock();
			F=nullptr; // anything previously set is now invalid
			SetLogicalBounds( minFileSize, maxFileSize );
			Update( f );
			if (::IsWindow(m_hWnd)){ // may be window-less if the owner is window-less
				__refreshVertically__();
				Invalidate(FALSE);
			}
		locker.Unlock();
	}

	void CHexaEditor::Reset(CFile *f,int minFileSize,int maxFileSize){
		// resets the HexaEditor and supplies it new File content
		locker.Lock();
			caret=TCaret(0); // resetting the Caret and Selection
			logPosScrolledTo=0;
			Update( f, minFileSize, maxFileSize );
		locker.Unlock();
	}

	void CHexaEditor::SetLogicalBounds(int _minFileSize,int _maxFileSize){
		// changes the min and max File size
		locker.Lock();
			if (mouseInNcArea){
				// when in the non-client area (e.g. over a scrollbar), putting updated values aside
				update.minFileSize=_minFileSize;
				update.maxFileSize=_maxFileSize;
			}else{
				// otherwise, updating the values normally
				minFileSize = update.minFileSize = _minFileSize; // setting also Update just in case the cursor is in non-client area
				maxFileSize = update.maxFileSize = _maxFileSize;
				if (::IsWindow(m_hWnd)) // may be window-less if the owner is window-less
					if (F)
						__refreshVertically__();
			}
		locker.Unlock();
	}

	int CHexaEditor::GetLogicalSize() const{
		// returns the LogicalSize of File content
		EXCLUSIVELY_LOCK(*this);
		return	mouseInNcArea
				? update.logicalSize
				: logicalSize;
	}

	void CHexaEditor::SetLogicalSize(int _logicalSize){
		// changes the LogicalSize of File content (originally set when Resetting the HexaEditor)
		locker.Lock();
			caret.selectionA=std::min( caret.selectionA, _logicalSize );
			caret.position=std::min( caret.position, _logicalSize );
			if (mouseInNcArea)
				// when in the non-client area (e.g. over a scrollbar), putting updated values aside
				update.logicalSize=_logicalSize;
			else{
				// otherwise, updating the values normally
				logicalSize = update.logicalSize = _logicalSize; // setting also Update just in case the cursor is in non-client area
				if (::IsWindow(m_hWnd)) // may be window-less if the owner is window-less
					__refreshVertically__();
			}
		locker.Unlock();
	}

	void CHexaEditor::GetVisiblePart(int &rLogicalBegin,int &rLogicalEnd) const{
		// gets the beginning and end of visible portion of the File content
		const int i=GetScrollPos(SB_VERT);
		rLogicalBegin=__firstByteInRowToLogicalPosition__(i);
		rLogicalEnd=__firstByteInRowToLogicalPosition__(i+nRowsDisplayed);
	}

	void CHexaEditor::ScrollToCaretAsync(){
		// makes sure the Caret is visible
		if (::GetCurrentThreadId()==::GetWindowThreadProcessId(*this,nullptr)) // do we owe the HexaEditor control?
			SendMessage( WM_KEYDOWN, VK_KANJI );
		else
			PostMessage( WM_KEYDOWN, VK_KANJI );
	}

	int CHexaEditor::GetLogicalSelection(PINT pOutSelA,PINT pOutSelZ) const{
		// gets current Selection and returns current Caret Position
		if (pOutSelA)
			*pOutSelA=caret.selectionA;
		if (pOutSelZ)
			*pOutSelZ=caret.position;
		return caret.position;
	}

	void CHexaEditor::SetLogicalSelection(int selA,int selZ){
		// sets current Selection, moving Caret to the end of the Selection
		if (selA>selZ)
			std::swap( selA, selZ );
		caret.selectionA=std::max(0,selA);
		caret.position=std::min(selZ,maxFileSize);
		RepaintData();
		ScrollToCaretAsync();
	}

	void CHexaEditor::ScrollTo(int logicalPos,bool moveAlsoCaret){
		// independently from Caret, displays specified LogicalPosition
		if (moveAlsoCaret)
			caret.selectionA = caret.position = logicalPos;
		__refreshVertically__();
		__scrollToRow__( __logicalPositionToRow__(logicalPos) );
	}

	void CHexaEditor::ScrollToRow(int iRow,bool moveAlsoCaret){
		// independently from Caret, displays specified LogicalPosition
		ScrollTo( __firstByteInRowToLogicalPosition__(iRow), moveAlsoCaret );
	}

	void CHexaEditor::AddEmphasis(int a,int z){
		// adds a new Emphasis into the list and orders the list by beginnings A (and thus also by endings Z; insertsort)
		PEmphasis *p=&emphases;
		while (a>(*p)->a) p=&(*p)->pNext;
		const PEmphasis pNext=*p;
		if (z<pNext->a){
			// new Emphasis doesn't concatenate with an existing one, e.g. <2,15) comes before Emphasis <16,30)
			const PEmphasis newEmp=new TEmphasis;
			newEmp->a=a, newEmp->z=z, newEmp->pNext=*p;
			*p=newEmp;
		}else
			// new Emphasis concatenates with an existing one, e.g. <2,16) can be merged with existing <16,30)
			pNext->a=a;
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
		return pContentAdviser->RowToLogicalPosition(row,nBytesInRow);
	}

	int CHexaEditor::__logicalPositionToRow__(int logPos) const{
		// computes and returns at which Row is the specified LogicalPosition
		return pContentAdviser->LogicalPositionToRow(logPos,nBytesInRow);
	}

	#define HEADER_LINES_COUNT	1
	#define HEADER_HEIGHT		HEADER_LINES_COUNT*font.charHeight

	int CHexaEditor::__scrollToRow__(int row){
		// scrolls the HexaEditor so that the specified Row is shown as the first one from top; returns the Row number which it has been really scrolled to
		locker.Lock();
			row=std::min( std::max(row,0), std::max(nLogicalRows-nRowsOnPage,0) );
		locker.Unlock();
		if (const int dr=GetScrollPos(SB_VERT)-row){
			RECT rcScroll;
				GetClientRect(&rcScroll);
				rcScroll.bottom=( rcScroll.top=HEADER_HEIGHT )+nRowsDisplayed*font.charHeight;
			ScrollWindow( 0, dr*font.charHeight, nullptr, &rcScroll );
			SetScrollPos(SB_VERT,row,TRUE); // True = redrawing the scroll-bar, not HexaEditor's canvas!
			SendMessage( WM_VSCROLL, SB_THUMBPOSITION ); // letting descendants of HexaEditor know that a scrolling occured
			::DestroyCaret();
		}
		return GetScrollPos(SB_VERT);
	}

	void CHexaEditor::__refreshVertically__(){
		// refreshes all parameters that relate to vertical axis
		// - determining the total number of Rows
		locker.Lock();
			nLogicalRows=__logicalPositionToRow__( std::max<int>(F->GetLength(),logicalSize) );
		locker.Unlock();
		// - setting the scrolling dimensions
		RECT r;
		GetClientRect(&r);
		nRowsDisplayed=std::max( 0L, (r.bottom-r.top)/font.charHeight-HEADER_LINES_COUNT );
		nRowsOnPage=std::max( 0, nRowsDisplayed-1 );
		if (::GetCurrentThreadId()==::GetWindowThreadProcessId(*this,nullptr)) // do I owe the HexaEditor control?
			SendMessage( WM_HEXA_PAINTSCROLLBARS );
		else
			PostMessage( WM_HEXA_PAINTSCROLLBARS );
	}

	void CHexaEditor::RepaintData() const{
		// invalidates the "data" (the content below the Header)
		if (m_hWnd){
			locker.Lock();
				if (!mouseInNcArea) // when NOT in the non-client area (e.g. over a scrollbar), repainting normally
					const_cast<CHexaEditor *>(this)->__refreshVertically__();
			locker.Unlock();
			RECT rc;
			GetClientRect(&rc);
			rc.top=HEADER_HEIGHT;
			::InvalidateRect( m_hWnd, &rc, TRUE );
		}
	}

	#define BYTES_MAX		64

	#define HEXA_FORMAT			_T("%02X ")
	#define HEXA_FORMAT_LENGTH	3
	#define HEXA_SPACE_LENGTH	2

	void CHexaEditor::__refreshCaretDisplay__() const{
		// shows Caret on screen at position that corresponds with Caret's actual Position in the underlying File content (e.g. the 12345-th Byte of the File)
		#define CARET_DISABLED_HEIGHT 2
		::CreateCaret(	m_hWnd, nullptr, (2-caret.ascii)*font.charAvgWidth,
						!editable*CARET_DISABLED_HEIGHT + editable*font.charHeight // either N (if not Editable) or CharHeight (if Editable)
					);
		int currRecordStart, currRecordLength=1;
		pContentAdviser->GetRecordInfo( caret.position, &currRecordStart, &currRecordLength, nullptr );
		const div_t d=div(caret.position-currRecordStart,currRecordLength);
		const int iScrollY=GetScrollPos(SB_VERT);
		//if (d.quot>=iScrollY){ // commented out as always guaranteed
			// Caret "under" the header
			POINT pos={	d.rem % nBytesInRow, // translated below to a particular pixel position
						(HEADER_LINES_COUNT + __logicalPositionToRow__(caret.position) - iScrollY)*font.charHeight // already a particular Y pixel position
					};
			if (caret.ascii) // Caret in the Ascii area
				pos.x=(addrLength+ADDRESS_SPACE_LENGTH+HEXA_FORMAT_LENGTH*nBytesInRow+HEXA_SPACE_LENGTH+pos.x)*font.charAvgWidth;
			else // Caret in the Hexa area
				pos.x=(addrLength+ADDRESS_SPACE_LENGTH+HEXA_FORMAT_LENGTH*pos.x)*font.charAvgWidth;
			pos.y+=!editable*(font.charHeight-CARET_DISABLED_HEIGHT);
			SetCaretPos(pos);
		/*}else{ // commented out as it never occurs
			// Caret "above" the header
			static constexpr POINT Pos={ -100, -100 };
			SetCaretPos(Pos);
		}*/
	}

	int CHexaEditor::__logicalPositionFromPoint__(const POINT &rPoint,bool *pOutAsciiArea) const{
		// determines and returns the LogicalPosition pointed to by the input Point (or -1 if not pointing at a particular Byte in both the Hexa and Ascii areas)
		const int x=rPoint.x-(addrLength+ADDRESS_SPACE_LENGTH)*font.charAvgWidth;
		const int r=rPoint.y/font.charHeight-HEADER_LINES_COUNT+GetScrollPos(SB_VERT);
		const int byteW=HEXA_FORMAT_LENGTH*font.charAvgWidth, hexaW=nBytesInRow*byteW;
		const int asciiX=hexaW+HEXA_SPACE_LENGTH*font.charAvgWidth;
		const int currLineStart=__firstByteInRowToLogicalPosition__(r), currLineBytesMinusOne=__firstByteInRowToLogicalPosition__(r+1)-currLineStart-1;
		if (x>0 && x<=hexaW){ // "x>0" - cannot be just "x" because x can be negative
			// Hexa area
			if (pOutAsciiArea) *pOutAsciiArea=false;
			return currLineStart+std::min<>( x/byteW, currLineBytesMinusOne );
		}else if (x>asciiX && x<=asciiX+nBytesInRow*font.charAvgWidth){
			// Ascii area
			if (pOutAsciiArea) *pOutAsciiArea=true;
			return currLineStart+std::min<>((x-asciiX)/font.charAvgWidth,currLineBytesMinusOne);
		}else
			// outside any area
			return -1;
	}

	void CHexaEditor::__showMessage__(LPCTSTR msg) const{
		// shows Message and passes focus back to the HexaEditor
		Utils::Information(msg);
		::PostMessage( m_hWnd, WM_LBUTTONDOWN, 0, -1 ); // recovering the focus; "-1" = [x,y] = nonsense value; can't use mere SetFocus because this alone doesn't work
	}

	void CHexaEditor::SendEditNotification(WORD en) const{
		::SendMessage( ::GetParent(m_hWnd), WM_COMMAND, MAKELONG(GetWindowLong(m_hWnd,GWL_ID),en), 0 );
	}

	#define MESSAGE_LIMIT_UPPER	_T("The content has reached its upper limit.")

	static bool mouseDragged;

	#define CLIPFORMAT_BINARY	_T("RideHexaEditBinary")

	static UINT cfBinary;

	#define SEARCH_PARAMS	(&mouseDragged)
	#define SEARCH_ENABLED	(param!=&mouseDragged)

	LRESULT CHexaEditor::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		static DWORD cursorPos0;
		int i;
		const int caretPos0=caret.position;
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
				ScrollTo( logPosScrolledTo );
				return 0;
			case WM_KEYDOWN:{
				// key pressed
				const bool ctrl=::GetKeyState(VK_CONTROL)<0;
				switch (wParam){
					case VK_LEFT:
						caret.position--;
caretCorrectlyMoveTo:	// . adjusting the Caret's Position
						caret.hexaLow=true; // the next keystroke will modify the lower four bits of current hexa-value
						if (caret.position<0) caret.position=0;
						else if (caret.position>F->GetLength()) caret.position=F->GetLength();
						// . adjusting an existing Selection if Shift pressed
						if (!mouseDragged){ // if mouse is being -Dragged, the beginning of a Selection has already been detected
							if (caret.selectionA!=caretPos0) // if there has been a Selection before ...
								RepaintData(); // ... invalidating the content as the Selection may no longer be valid (e.g. may be deselected)
							caret.__detectNewSelection__();
						}
						//fallthrough
					case VK_KANJI:{
caretRefresh:			// . refreshing the Caret
						HideCaret();
							// : scrolling if Caret has been moved to an invisible part of the File content
							const int iRow=__logicalPositionToRow__(caret.position), iScrollY=GetScrollPos(SB_VERT);
							if (iRow<iScrollY) __scrollToRow__(iRow);
							else if (iRow>=iScrollY+nRowsOnPage) __scrollToRow__(iRow-nRowsOnPage+1);
							// : invalidating the content if Selection has (or is being) changed
							if (caret.position!=caretPos0) // yes, the Caret has moved
								if (mouseDragged || ::GetAsyncKeyState(VK_SHIFT)<0) // yes, Selection is being edited (by dragging the mouse or having the Shift key pressed)
									RepaintData();
							// : displaying the Caret
							__refreshCaretDisplay__();
						ShowCaret();
						return 0;
					}
					case VK_RIGHT:
						caret.position++;
						goto caretCorrectlyMoveTo;
					case VK_UP:{
						i=1; // move Caret one row up
moveCaretUp:			const int iRow=__logicalPositionToRow__(caret.position);
						if (ctrl){
							const int iScrollY=__scrollToRow__(GetScrollPos(SB_VERT)-i);
							if (iRow<iScrollY+nRowsOnPage) goto caretRefresh;
						}
						const int currRowStart=__firstByteInRowToLogicalPosition__(iRow);
						const int targetRowStart=__firstByteInRowToLogicalPosition__(iRow-i);
						caret.position -=	std::max<>(
												caret.position-__firstByteInRowToLogicalPosition__(iRow-i+1)+1,
													// ..........			Target row
													// .................
													// ..........
													// .............C...	Current row with Caret
												currRowStart-targetRowStart
													// .................	..........			Target row
													// ..........			................
													// .................	..........
													// .......C..			.......C........	Current row with Caret
											);
						goto caretCorrectlyMoveTo;
					}
					case VK_DOWN:{
						i=1; // move Caret one row down
moveCaretDown:			const int iRow=__logicalPositionToRow__(caret.position);
						if (ctrl){
							const int iScrollY=__scrollToRow__(GetScrollPos(SB_VERT)+i);
							if (iRow>=iScrollY) goto caretRefresh;
						}
						const int currRowStart=__firstByteInRowToLogicalPosition__(iRow);
						const int targetRowStart=__firstByteInRowToLogicalPosition__(iRow+i);
						caret.position +=	std::min<>(
												targetRowStart-currRowStart,
													// .......C..			.......C........	Current row with Caret
													// .................	..........
													// ..........			................
													// .................	..........			Target row
												__firstByteInRowToLogicalPosition__(iRow+i+1)-caret.position-1
													// .............C...	Current row with Caret
													// ..........
													// .................
													// ..........			Target row
											);
						goto caretCorrectlyMoveTo;
					}
					case VK_PRIOR:	// page up
						i=nRowsOnPage-ctrl; // move Caret N rows up
						goto moveCaretUp;
					case VK_NEXT:	// page down
						i=nRowsOnPage-ctrl; // move Caret N rows down
						goto moveCaretDown;
					case VK_HOME:
						caret.position=( ctrl ? 0 : __firstByteInRowToLogicalPosition__(__logicalPositionToRow__(caret.position)) );
						goto caretCorrectlyMoveTo;
					case VK_END:
						caret.position=( ctrl ? F->GetLength() : __firstByteInRowToLogicalPosition__(__logicalPositionToRow__(caret.position)+1)-1 );
						goto caretCorrectlyMoveTo;
					case VK_TAB:{
						const HWND hDlg=::GetTopWindow(m_hWnd);
						const bool shiftPressed=::GetKeyState(VK_SHIFT)<0;
						if (shiftPressed ^ caret.ascii){
							// leaving the HexaEditor control if (a) Tab alone pressed while in Ascii part, or (b) Shift+Tab pressed while in the hexa part
							::SetFocus(  ::GetNextDlgTabItem( hDlg, m_hWnd, shiftPressed )  );
							break;
						}else{
							// switching between the Ascii and hexa parts
							caret.ascii=!caret.ascii;
							goto caretRefresh;
						}
					}
					case VK_DELETE:{
editDelete:				// deleting the Byte after Caret, or deleting the Selection
						if (!editable) return 0; // can't edit content of a disabled window
						// . if Selection not set, setting it as the Byte immediately after Caret
						if (caret.selectionA==caret.position)
							if (caret.position<F->GetLength()) caret.selectionA=caret.position++;
							else return 0;
deleteSelection:		int posSrc=std::max(caret.selectionA,caret.position), posDst=std::min(caret.selectionA,caret.position);
						// . checking if there are any Bookmarks selected
						if (bookmarks.__getNearestNextBookmarkPosition__(posDst)<posSrc){
							if (Utils::QuestionYesNo(_T("Sure to delete selected bookmarks?"),MB_DEFBUTTON2)){
								for( int pos; (pos=bookmarks.__getNearestNextBookmarkPosition__(posDst))<posSrc; bookmarks.__removeBookmark__(pos) );
								caret.selectionA=caret.position; // cancelling any Selection
								RepaintData();
							}
							SetFocus();
							return 0;
						}
						// . moving the content "after" Selection "to" the position of the Selection
						caret.selectionA = caret.position = posDst; // moving the Caret and cancelling any Selection
						int nBytesToMove=F->GetLength()-posSrc;
						for( BYTE buf[65536]; const int nBytesRequested=std::min<int>(nBytesToMove,sizeof(buf)); ){
							F->Seek(posSrc,CFile::begin);
							int nBytesBuffered=0;
							while (const int nBytesRead=F->Read( buf+nBytesBuffered, nBytesRequested-nBytesBuffered ))
								nBytesBuffered+=nBytesRead;
							F->Seek(posDst,CFile::begin);
							F->Write(buf,nBytesBuffered);
							const int nBytesWritten=F->GetPosition()-posDst;
							const int nBytesSuccessfullyMoved=std::min( std::min(nBytesRequested,nBytesBuffered), nBytesWritten );
							for( int pos=posSrc; (pos=bookmarks.__getNearestNextBookmarkPosition__(pos))<posSrc+nBytesSuccessfullyMoved; ){
								bookmarks.__removeBookmark__(pos);
								bookmarks.__addBookmark__(posDst+pos-posSrc);
							}
							nBytesToMove-=nBytesSuccessfullyMoved, posSrc+=nBytesSuccessfullyMoved, posDst+=nBytesSuccessfullyMoved;
							if (nBytesSuccessfullyMoved!=nBytesRequested)
								break;
						}
						// . the "source-destination" difference filled up with zeros
						if (!nBytesToMove) // successfully moved all Bytes?
							posSrc = logicalSize = std::max( logicalSize+posDst-posSrc, minFileSize );
						if (posDst<posSrc){
							for( static constexpr BYTE Zero=0; posDst++<posSrc; F->Write(&Zero,1) );
							if (!nBytesToMove) // successfully moved all Bytes?
								__informationWithCheckableShowNoMore__( _T("To preserve the minimum size, the content has been padded with zeros."), INI_MSG_PADDING );
						}
						// . refreshing the scrollbar
						F->SetLength( logicalSize );
						SetLogicalSize( logicalSize );
						SendEditNotification( EN_CHANGE );
						RepaintData();
						goto caretRefresh;
					}
					case VK_BACK:
						// deleting the Byte before Caret, or deleting the Selection
						if (!editable) return 0; // can't edit content of a disabled window
						// . if Selection not set, setting it as the Byte immediately before Caret
						if (caret.selectionA==caret.position)
							if (caret.position) caret.selectionA=caret.position-1;
							else return 0;
						// . moving the content "after" Selection "to" the position of the Selection
						goto deleteSelection;
					case VK_RETURN:
						// refocusing the window that has previously lost the focus in favor of this HexaEditor
						::SetFocus(hPreviouslyFocusedWnd);
						break;
					case VK_F5:
						// redrawing
						caret.hexaLow=true; // the next keystroke will modify the lower four bits of current hexa-value
						Invalidate();
						break;
					default:
						if (!editable) return 0; // can't edit content of a disabled window
						if (ctrl){
							// a shortcut other than Caret positioning
							return 0;
						}else if (!caret.ascii){ // here Hexa mode; Ascii mode handled in WM_CHAR
							// Hexa modification
							if (wParam>='0' && wParam<='9')
								wParam-='0';
							else if (wParam>=VK_NUMPAD0 && wParam<=VK_NUMPAD9)
								wParam-=VK_NUMPAD0;
							else if (wParam>='A' && wParam<='F')
								wParam-='A'-10;
							else
								break;
							if (caret.position<maxFileSize){
								BYTE b=0;
								F->Seek(caret.position,CFile::begin);
								F->Read(&b,1);
								b= b<<4 | wParam;
								if (caret.position<F->GetLength()) F->Seek(-1,CFile::current);
								F->Write(&b,1);
								if ( caret.hexaLow=!caret.hexaLow )
									caret.position++;
								caret.selectionA=caret.position; // cancelling any Selection
								SetLogicalSize(  std::max<int>( logicalSize, F->GetLength() )  );
								SendEditNotification( EN_CHANGE );
								RepaintData();
								goto caretRefresh;
							}else
								__showMessage__(MESSAGE_LIMIT_UPPER);
						}
						break;
				}
				break;
			}
			case WM_CHAR:
				// character
				if (!editable) return 0; // can't edit content of a disabled window
				if (caret.ascii) // here Ascii mode; Hexa mode handled in WM_KEYDOWN
					// Ascii modification
					if (::GetAsyncKeyState(VK_CONTROL)>=0 && ::isprint(wParam)) // Ctrl not pressed, thus character printable
						if (caret.position<maxFileSize){
							F->Seek( caret.position, CFile::begin );
							F->Write(&wParam,1);
							caret.selectionA = ++caret.position; // moving the Caret and cancelling any Selection
							SetLogicalSize(  std::max( logicalSize, caret.position )  );
							SendEditNotification( EN_CHANGE );
							RepaintData();
							goto caretRefresh;
						}else
							__showMessage__(MESSAGE_LIMIT_UPPER);
				return 0;
			case WM_CONTEXTMENU:{
				// context menu invocation
				Utils::CRideContextMenu mnu( IDR_HEXAEDITOR, this );
				BYTE iSelectSubmenu, iResetSubmenu, iGotoSubmenu;
				if (customSelectSubmenu){ // custom "Select" submenu
					const HMENU hSubmenu=Utils::GetSubmenuByContainedCommand( mnu, ID_EDIT_SELECT_ALL, &iSelectSubmenu );
					mnu.ModifyMenu(
						iSelectSubmenu, MF_BYPOSITION|MF_POPUP, (UINT_PTR)customSelectSubmenu,
						mnu.GetMenuStringByPos( iSelectSubmenu )
					);
				}
				if (customResetSubmenu){ // custom "Fill" submenu
					const HMENU hSubmenu=Utils::GetSubmenuByContainedCommand( mnu, ID_ZERO, &iResetSubmenu );
					mnu.ModifyMenu(
						iResetSubmenu, MF_BYPOSITION|MF_POPUP, (UINT_PTR)customResetSubmenu,
						mnu.GetMenuStringByPos( iResetSubmenu )
					);
				}
				if (customGotoSubmenu){ // custom "Go to" submenu
					const HMENU hSubmenu=Utils::GetSubmenuByContainedCommand( mnu, ID_NAVIGATE_ADDRESS, &iGotoSubmenu );
					mnu.ModifyMenu(
						iGotoSubmenu, MF_BYPOSITION|MF_POPUP, (UINT_PTR)customGotoSubmenu,
						mnu.GetMenuStringByPos( iGotoSubmenu )
					);
				}
				mnu.UpdateUi( this );
				int x=GET_X_LPARAM(lParam), y=GET_Y_LPARAM(lParam);
				if (x==-1){ // occurs if the context menu invoked using Shift+F10
					POINT caretPos=GetCaretPos();
					ClientToScreen(&caretPos);
					x=caretPos.x+(1+!caret.ascii)*font.charAvgWidth, y=caretPos.y+font.charHeight;
				}
				if ( wParam=mnu.TrackPopupMenu( TPM_RETURNCMD, x,y, this ) )
					OnCmdMsg( wParam, CN_COMMAND, nullptr, nullptr );
				if (customGotoSubmenu) // custom "Go to" submenu
					mnu.RemoveMenu( iGotoSubmenu, MF_BYPOSITION ); // "detaching" the Submenu from the parent
				if (customResetSubmenu) // custom "Reset" submenu
					mnu.RemoveMenu( iResetSubmenu, MF_BYPOSITION ); // "detaching" the Submenu from the parent
				if (customSelectSubmenu) // custom "Select" submenu
					mnu.RemoveMenu( iSelectSubmenu, MF_BYPOSITION ); // "detaching" the Submenu from the parent
				return 0;
			}
			case WM_COMMAND:
				// processing a command
				switch (LOWORD(wParam)){
					case ID_BOOKMARK_TOGGLE:
						// toggling a Bookmark at Caret's Position
						if (bookmarks.__getNearestNextBookmarkPosition__(caret.position)==caret.position)
							bookmarks.__removeBookmark__(caret.position);
						else
							bookmarks.__addBookmark__(caret.position);
						RepaintData();
						goto caretRefresh;
					case ID_BOOKMARK_PREV:
						// navigating the Caret to the previous Bookmark
						if (bookmarks.__getNearestNextBookmarkPosition__(0)<caret.position){
							int prevBookmarkPos=0;
							for( int pos=0; pos<caret.position; pos=bookmarks.__getNearestNextBookmarkPosition__(pos+1) )
								prevBookmarkPos=pos;
							caret.selectionA = caret.position = prevBookmarkPos; // moving the Caret and cancelling any Selection
							RepaintData();
							goto caretRefresh;
						}else
							break;
					case ID_BOOKMARK_NEXT:{
						// navigating the Caret to the next Bookmark
						const int nextBookmarkPos=bookmarks.__getNearestNextBookmarkPosition__(caret.position+1);
						if (nextBookmarkPos<BOOKMARK_POSITION_INFINITY){
							caret.selectionA = caret.position = nextBookmarkPos; // moving the Caret and cancelling any Selection
							RepaintData();
							goto caretRefresh;
						}else
							break;
					}
					case ID_BOOKMARK_DELETEALL:
						// deleting all Bookmarks
						if (Utils::QuestionYesNo(_T("Sure to delete all bookmarks?"),MB_DEFBUTTON2)){
							bookmarks.__removeAllBookmarks__();
							RepaintData();
							goto caretRefresh;
						}else
							break;
					case ID_EDIT_SELECT_ALL:
						// Selecting everything
						caret.selectionA=0, caret.position=F->GetLength();
						RepaintData();
						goto caretRefresh;
					case ID_EDIT_SELECT_NONE:
						// removing current selection
						caret.selectionA=caret.position; // cancelling any Selection
						RepaintData();
						goto caretRefresh;
					case ID_EDIT_SELECT_CURRENT:{
						// selecting the whole Record under the Caret
						int recordLength=0;
						pContentAdviser->GetRecordInfo( caret.position, &caret.selectionA, &recordLength, nullptr );
						caret.position=caret.selectionA+recordLength;
						RepaintData();
						goto caretRefresh;
					}
					case ID_FILE_SAVE_COPY_AS:{
						// saving Selection as
						const CString fileName=Utils::DoPromptSingleTypeFileName( _T("selection.bin"), nullptr );
						if (!fileName.IsEmpty()){
							CFileException e;
							CFile fDest;
							if (fDest.Open( fileName, CFile::modeWrite|CFile::modeCreate|CFile::shareDenyWrite|CFile::typeBinary, &e ))
								for( DWORD nBytesToSave=std::max(caret.selectionA,caret.position)-F->Seek(std::min(caret.selectionA,caret.position),CFile::begin),n; nBytesToSave; nBytesToSave-=n ){
									BYTE buf[65536];
									n=F->Read(  buf,  std::min<UINT>( nBytesToSave, sizeof(buf) )  );
									fDest.Write( buf, n );
									if (::GetLastError()==ERROR_READ_FAULT){
										Utils::Information( _T("Selection saved only partially"), ERROR_READ_FAULT );
										break;
									}
								}
							else
								Utils::FatalError( _T("Can't save selection"), e.m_cause );
						}
						SetFocus(); // restoring focus lost by displaying the "Save as" dialog
						return 0;
					}
					case ID_EDIT_PASTE_SPECIAL:{
						// pasting content of a file at current Caret Position
						const CString fileName=Utils::DoPromptSingleTypeFileName( nullptr, nullptr, OFN_FILEMUSTEXIST );
						if (!fileName.IsEmpty()){
							CFileException e;
							CFile fSrc;
							if (fSrc.Open( fileName, CFile::modeRead|CFile::shareDenyWrite|CFile::typeBinary, &e )){
								for( DWORD nBytesToRead=std::min<DWORD>( fSrc.GetLength(), maxFileSize-F->Seek(caret.position,CFile::begin) ),n; nBytesToRead>0; nBytesToRead-=n ){
									BYTE buf[65536];
									n=fSrc.Read(  buf,  std::min<UINT>( nBytesToRead, sizeof(buf) )  );
									F->Write( buf, n );
									if (::GetLastError()==ERROR_WRITE_FAULT){
										Utils::Information( _T("Pasted only partially"), ERROR_WRITE_FAULT );
										break;
									}
								}
								SendEditNotification( EN_CHANGE );
								RepaintData();
							}else
								Utils::FatalError( _T("Can't paste file content"), e.m_cause );
						}
						SetFocus(); // restoring focus lost by displaying the "Open" dialog
						return 0;
					}
					case ID_ZERO:
						// resetting Selection with zeros
						if (!editable) return 0; // can't edit content of a disabled window
						i=0;
resetSelectionWithValue:BYTE buf[65535];
						if (i>=0) // some constant value
							::memset( buf, i, sizeof(buf) );
						for( DWORD nBytesToReset=std::max(caret.selectionA,caret.position)-F->Seek(std::min(caret.selectionA,caret.position),CFile::begin),n; nBytesToReset; nBytesToReset-=n ){
							n=std::min<UINT>( nBytesToReset, sizeof(buf) );
							if (i<0) // Gaussian noise
								Utils::RandomizeData( buf, sizeof(buf) );
							F->Write( buf, n );
							if (::GetLastError()==ERROR_WRITE_FAULT){
								Utils::Information( _T("Selection only partially reset"), ERROR_WRITE_FAULT );
								break;
							}
						}
						SendEditNotification( EN_CHANGE );
						RepaintData();
						goto caretCorrectlyMoveTo;
					case ID_NUMBER:{
						// resetting Selection with user-defined value
						if (!editable) return 0; // can't edit content of a disabled window
						// . defining the Dialog
						class CResetDialog sealed:public Utils::CRideDialog{
							BOOL OnInitDialog() override{
								TCHAR buf[80];
								::wsprintf( buf+GetDlgItemText(ID_DIRECTORY,buf,sizeof(buf)/sizeof(TCHAR)), _T(" (0x%02X)"), directoryDefaultByte );
								SetDlgItemText( ID_DIRECTORY, buf );
								::wsprintf( buf+GetDlgItemText(ID_DATA,buf,sizeof(buf)/sizeof(TCHAR)), _T(" (0x%02X)"), dataDefaultByte );
								SetDlgItemText( ID_DATA, buf );
								return __super::OnInitDialog();
							}
							void DoDataExchange(CDataExchange *pDX) override{
								DDX_Radio( pDX, ID_DIRECTORY, iRadioSel );
								if (!pDX->m_bSaveAndValidate || iRadioSel==3)
									DDX_Text( pDX, ID_NUMBER, value );
							}
							LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
								if (msg==WM_COMMAND)
									EnableDlgItem( ID_NUMBER, IsDlgButtonChecked(ID_NUMBER2)==BST_CHECKED );
								return __super::WindowProc(msg,wParam,lParam);
							}
						public:
							const BYTE directoryDefaultByte, dataDefaultByte;
							int iRadioSel;
							BYTE value;

							CResetDialog(CWnd *pParentWnd)
								: Utils::CRideDialog( IDR_HEXAEDITOR_RESETSELECTION, pParentWnd )
								, directoryDefaultByte( CDos::GetFocused()->properties->directoryFillerByte )
								, dataDefaultByte( CDos::GetFocused()->properties->sectorFillerByte )
								, iRadioSel(3) , value(0) {
							}
						} d(this);
						// . showing the Dialog and processing its result
						const Utils::CVarBackup<TCaret> caret0=caret;
						if (d.DoModal()==IDOK){
							switch (d.iRadioSel){
								case 0: i=d.directoryDefaultByte; break;
								case 1: i=d.dataDefaultByte; break;
								case 2: i=-1; break;
								case 3: i=d.value; break;
								default:
									ASSERT(FALSE);
							}
							goto resetSelectionWithValue;
						}else
							return 0;
					}
					case ID_EDIT_COPY:
						// copying the Selection into clipboard
						if (caret.selectionA!=caret.position)
							( new COleBinaryDataSource(	F,
														std::min<>(caret.selectionA,caret.position),
														std::max<>(caret.position,caret.selectionA)
							) )->SetClipboard();
						return 0;
					case ID_EDIT_PASTE:{
						// pasting binary data from clipboard at the Position of Caret
						if (!editable) return 0; // can't edit content of a disabled window
						COleDataObject odo;
						odo.AttachClipboard();
						if (const HGLOBAL hg=odo.GetGlobalData(cfBinary)){
							const DWORD *p=(PDWORD)::GlobalLock(hg), length=*p; // binary data are prefixed by their length
								F->Seek(caret.position,CFile::begin);
								const DWORD lengthLimit=maxFileSize-caret.position;
								if (length<=lengthLimit){
									F->Write( ++p, length );
									caret.selectionA = caret.position+=length; // moving the Caret and cancelling any Selection
								}else{
									F->Write( ++p, lengthLimit );
									caret.position+=lengthLimit;
									__showMessage__(MESSAGE_LIMIT_UPPER);
								}
							::GlobalUnlock(hg);
							::GlobalFree(hg);
						}
						SendEditNotification( EN_CHANGE );
						RepaintData();
						goto caretRefresh;
					}
					case ID_EDIT_DELETE:
						// deleting content of the current selection
						goto editDelete;
					case ID_EDIT_FIND_NEXT:
						// find next occurence of Pattern in the Content
						if (search.patternLength==0) // no Pattern specified yet
							wParam=ID_EDIT_FIND;
						//fallthrough
					case ID_EDIT_FIND:{
						// find a Pattern in the Content
						// . ignoring disabled command
						if (!SEARCH_ENABLED)
							return 0;
						// . defining the Dialog
						class CSearchDialog sealed:public Utils::CRideDialog{
							CMemFile f;
							CHexaEditor hexaEditor;
							bool acceptNotification;

							BOOL OnInitDialog() override{
								const BOOL r=__super::OnInitDialog();
								hexaEditor.Create( nullptr, nullptr, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, MapDlgItemClientRect(ID_FILE), this, ID_FILE2 );
								return r;
							}

							void DoDataExchange(CDataExchange *pDX) override{
								DDX_Radio( pDX, ID_DEFAULT1, (int &)search.type );
								if (pDX->m_bSaveAndValidate)
									switch (search.type){
										case CSearch::ASCII_ANY_CASE:
											search.patternLength=::GetDlgItemTextA( m_hWnd, ID_TEXT, search.pattern.chars, sizeof(search.pattern.chars) );
											if (IsDlgButtonChecked(ID_ACCURACY))
												search.type=CSearch::ASCII_MATCH_CASE;
											break;
										case CSearch::HEXA:
											search.patternLength=f.GetLength();
											break;
										case CSearch::NOT_BYTE:
											DDX_Text( pDX, ID_NUMBER, search.pattern.bytes[0] );
											search.patternLength=1;
											break;
										default:
											ASSERT(FALSE);
											break;
									}
							}

							LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
								static constexpr WORD SearchButtons[]={ ID_EDIT_FIND_PREV, ID_EDIT_FIND_NEXT, 0 };
								switch (msg){
									case WM_COMMAND:
										switch (wParam){
											case ID_EDIT_FIND_PREV:
											case ID_EDIT_FIND_NEXT:
												if (UpdateData(TRUE))
													EndDialog(wParam);
												return 0;
										// ASCII search events
											case MAKELONG(ID_DEFAULT1,BN_CLICKED):
											case MAKELONG(ID_TEXT,EN_SETFOCUS):
											case MAKELONG(ID_ACCURACY,BN_CLICKED):
												CheckRadioButton( ID_DEFAULT1, ID_DEFAULT4, ID_DEFAULT1 );
												//fallthrough
											case MAKELONG(ID_TEXT,EN_CHANGE):
												if (acceptNotification){
													acceptNotification=false; // preventing from recurrent processing
														hexaEditor.SetLogicalSize(
															search.patternLength=GetDlgItemTextA( ID_TEXT, search.pattern.chars, sizeof(search.pattern.chars) )
														);
														f.SetLength( search.patternLength );
														hexaEditor.Invalidate();
													acceptNotification=true;
													EnableDlgItems( SearchButtons, search.patternLength>0 );
												}
												break;
										// Hexa search events
											case MAKELONG(ID_DEFAULT2,BN_CLICKED):
											case MAKELONG(ID_FILE2,EN_SETFOCUS):
												CheckRadioButton( ID_DEFAULT1, ID_DEFAULT4, ID_DEFAULT2 );
												//fallthrough
											case MAKELONG(ID_FILE2,EN_CHANGE):
												if (acceptNotification){
													acceptNotification=false; // preventing from recurrent processing
														search.pattern.chars[ search.patternLength=f.GetLength() ]='\0';
														SetDlgItemText( ID_TEXT, search.pattern.chars );
													acceptNotification=true;
													EnableDlgItems( SearchButtons, search.patternLength>0 );
												}
												break;
										// "Not-Byte" search events
											case MAKELONG(ID_DEFAULT3,BN_CLICKED):
											case MAKELONG(ID_NUMBER,EN_SETFOCUS):
												CheckRadioButton( ID_DEFAULT1, ID_DEFAULT4, ID_DEFAULT3 );
												//fallthrough
											case MAKELONG(ID_NUMBER,EN_CHANGE):
												if (acceptNotification)
													EnableDlgItems( SearchButtons, GetDlgItemTextA(ID_NUMBER,search.pattern.chars,sizeof(search.pattern.chars))>0 );
												break;
										}
										break;
								}
								return __super::WindowProc( msg, wParam, lParam );
							}
						public:
							CSearch search;

							CSearchDialog(CWnd *pParentWnd,const CSearch &rSearch)
								: Utils::CRideDialog( IDR_HEXAEDITOR_FIND, pParentWnd )
								, search(rSearch)
								, acceptNotification(true)
								, f( search.pattern.bytes, sizeof(search.pattern.bytes) )
								, hexaEditor(SEARCH_PARAMS) {
								f.SetLength(0);
								f.SeekToBegin();
								hexaEditor.ShowAddressBand(false);
								hexaEditor.Reset( &f, 0, sizeof(search.pattern.bytes) );
							}
						} d( this, search );
						// . showing the Dialog and processing its result
						d.search.logPosFound=caret.position;
						TStdWinError err=ERROR_NOT_FOUND; // assumption (Pattern not found)
						wParam=LOWORD(wParam);
						switch ( wParam==ID_EDIT_FIND ? d.DoModal() : wParam ){
							case ID_EDIT_FIND_PREV:
								ASSERT(FALSE); //TODO
								break;
							case ID_EDIT_FIND_NEXT:
								err=( search=d.search ).FindNextPositionModal();
								break;
							case IDCANCEL:
								return 0;
						}
						if (err){
							__showMessage__(  Utils::ComposeErrorMessage( _T("Search failed"), err )  );
							return 0;
						}else{
							SetFocus();
							caret.position=( caret.selectionA=search.logPosFound )+search.patternLength;
							RepaintData();
							goto caretRefresh;
						}
					}
					case ID_NEXT:{
						// navigating to the next Record
						int currRecordLength=0;
						pContentAdviser->GetRecordInfo( caret.position, &caret.position, &currRecordLength, nullptr );
						caret.position+=currRecordLength;
						goto caretCorrectlyMoveTo;
					}
					case ID_PREV:
						// navigating to the previous Record (or the beginning of current Record, if Caret not already there)
						pContentAdviser->GetRecordInfo( --caret.position, &caret.position, nullptr, nullptr );
						goto caretCorrectlyMoveTo;
					case ID_NAVIGATE_ADDRESS:{
						// navigating to an address input by the user
						const PropGrid::Integer::TUpDownLimits addrRange={ 0, F->GetLength() };
						if (const Utils::CSingleNumberDialog &&d=Utils::CSingleNumberDialog( _T("Go to"), _T("Address"), addrRange, caret.position, true, this )){
							caret.position=d.Value;
							goto caretCorrectlyMoveTo;
						}else
							return 0;
					}
				}
				return 0; // to suppress CEdit's standard context menu
			case WM_LBUTTONDOWN:
				// left mouse button pressed
				mouseDragged=false;
				goto leftMouseDragged; // "as if it's already been Dragged"
			case WM_RBUTTONDOWN:{
				// right mouse button pressed
				mouseDragged=false;
				const int logPos=__logicalPositionFromPoint__(CPoint(lParam),nullptr);
				if (logPos>=0){
					// in either Hexa or Ascii areas
					if (std::min<>(caret.selectionA,caret.position)<=logPos && logPos<std::max<>(caret.selectionA,caret.position))
						// right-clicked on the Selection - showing context menu at the place of Caret
						break;
					else
						// right-clicked outside the Selection - unselecting everything and moving the Caret
						goto leftMouseDragged; // "as if it's already been Dragged"
				}else
					// outside any area - ignoring the right-click
					return 0;
			}
			case WM_LBUTTONUP:
				// left mouse button released
				mouseDragged=false;
				break;
			case WM_MOUSEMOVE:{
				// mouse moved
				if (!( mouseDragged=::GetKeyState(VK_LBUTTON)<0 )) return 0; // if mouse button not pressed, current Selection cannot be modified
				if (cursorPos0==lParam) return 0; // actually no Cursor movement occured
leftMouseDragged:
				cursorPos0=lParam;
				const int logPos=__logicalPositionFromPoint__(CPoint(lParam),&caret.ascii);
				if (logPos>=0)
					// in either Hexa or Ascii areas
					caret.position=logPos;
				else{
					// outside any area
					if (!mouseDragged){ // if right now mouse button pressed ...
						caret.selectionA=caret.position; // ... cancelling any Selection ...
						RepaintData(); // ... and painting the result
					}
					break;
				}
				__super::WindowProc(msg,wParam,lParam); // to set focus and accept WM_KEY* messages
				ShowCaret();
				goto caretCorrectlyMoveTo;
			}
			case WM_LBUTTONDBLCLK:{
				// left mouse button double-clicked
				const int logPos=__logicalPositionFromPoint__(CPoint(lParam),&caret.ascii);
				if (logPos>=0){
					// in either Hexa or Ascii areas
					int recordStart,recordLength;
					pContentAdviser->GetRecordInfo( logPos, &recordStart, &recordLength, nullptr );
					SetLogicalSelection( recordStart, recordStart+recordLength );
				}
				break;
			}
			case WM_SETFOCUS:
				// window has received focus
				hPreviouslyFocusedWnd=(HWND)wParam; // the window that is losing the focus (may be refocused later when Enter is pressed)
				SendEditNotification( EN_SETFOCUS );
				ShowCaret();
				goto caretRefresh;
			case WM_KILLFOCUS:
				// window has lost focus
				locker.Lock();
					mouseInNcArea=false;
				locker.Unlock();
				::DestroyCaret();
				if (CWnd *const pParentWnd=GetParent()) pParentWnd->Invalidate(FALSE);
				hPreviouslyFocusedWnd=0;
				break;
			case WM_MOUSEWHEEL:{
				// mouse wheel was rotated
				int nLinesToScroll=1;
				::SystemParametersInfo( SPI_GETWHEELSCROLLLINES, 0, &nLinesToScroll, 0 );
				const short zDelta=(short)HIWORD(wParam);
				if (nLinesToScroll==WHEEL_PAGESCROLL)
					SendMessage( WM_VSCROLL, zDelta>0?SB_PAGEUP:SB_PAGEDOWN, 0 );
				else
					__scrollToRow__( GetScrollPos(SB_VERT)-zDelta*nLinesToScroll/WHEEL_DELTA );
				return TRUE;
			}
			case WM_VSCROLL:{
				// scrolling vertically
				// . determining the Row to scroll to
				SCROLLINFO si={ sizeof(si) };
				GetScrollInfo( SB_VERT, &si, SIF_POS|SIF_RANGE|SIF_TRACKPOS ); // getting 32-bit position
				int row=si.nPos;
				switch (LOWORD(wParam)){
					case SB_PAGEUP:		// clicked into the gap above "thumb"
						row-=nRowsOnPage;	break;
					case SB_PAGEDOWN:	// clicked into the gap below "thumb"
						row+=nRowsOnPage; break;
					case SB_LINEUP:		// clicked on arrow up
						row--; break;
					case SB_LINEDOWN:	// clicked on arrow down
						row++; break;
					case SB_THUMBPOSITION:	// "thumb" released
						break;
					case SB_THUMBTRACK:		// "thumb" dragged
						row = si.nTrackPos;	break;
				}
				// . redrawing HexaEditor's client and non-client areas
				__scrollToRow__(row);
				//fallthrough (the "thumb" might have been released outside the scrollbar area)
			}
			case WM_NCMOUSEMOVE:{
				// mouse moved in non-client area
				locker.Lock();
					mouseInNcArea=true;
				locker.Unlock();
				TRACKMOUSEEVENT tme={ sizeof(tme), TME_NONCLIENT|TME_LEAVE, m_hWnd, 0 };
				::TrackMouseEvent(&tme);
				break;
			}
			case WM_NCMOUSELEAVE:
				// mouse left non-client area
				locker.Lock();
					if (*static_cast<TState *>(this)!=update){
						*static_cast<TState *>(this)=update;
						RepaintData();
					}
					mouseInNcArea=false;
				locker.Unlock();
				break;
			case EM_GETSEL:
				// gets current Selection
				return GetLogicalSelection( (PINT)wParam, (PINT)lParam ); // returns the Caret Position, in contrast to the convenient value for an Edit control
			case EM_SETSEL:
				// sets current Selection, moving Caret to the end of the Selection
				if (wParam>lParam){
					const int tmp=wParam; wParam=lParam; lParam=tmp;
				}
				caret.selectionA=std::max<int>(0,wParam);
				caret.position=std::min<int>(lParam,maxFileSize);
				RepaintData();
				goto caretRefresh;
			case WM_ERASEBKGND:
				// drawing the background
				return TRUE; // nop (always drawing over existing content)
			case WM_SIZE:			
				// window size has changed
				__refreshVertically__(); // to guarantee that the actual view is always drawn
				break;
			case WM_PAINT:{
				// drawing
				class CHexaPaintDC sealed:public CPaintDC{
					const bool hexaEditorEditable;
					BYTE currContentFlags;
					COLORREF currEmphasisColor;
				public:
					enum TContentFlags:BYTE{
						Normal		=0,
						Selected	=1,
						Erroneous	=2,
						Unknown		=4 // = currently fetching data
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
						}else if (newContentFlags&Unknown){
							// content not yet known (e.g. floppy drive head is currently seeking to requested cylinder, etc.)
							// : TextColor is (some tone of) Yellow
							if (!(currContentFlags&Unknown)) // "if previously not Unknown"
								SetTextColor( !hexaEditorEditable*0x1122+0x66cc99 );
							// : BackgroundColor is the EmphasisColor
							goto blendEmphasisAndSelection;
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
						::FillRect( dc, &rcHeader, Utils::CRideBrush::BtnFace );
						TCHAR buf[16];
						dc.SetContentPrintState( CHexaPaintDC::Normal, ::GetSysColor(COLOR_BTNFACE) );
						rcHeader.left=(addrLength+ADDRESS_SPACE_LENGTH)*font.charAvgWidth;
						for( BYTE n=0; n<nBytesInRow&&rcHeader.left<rcHeader.right; rcHeader.left+=HEXA_FORMAT_LENGTH*font.charAvgWidth )
							dc.DrawText( buf, ::wsprintf(buf,HEXA_FORMAT,n++), &rcHeader, DT_LEFT|DT_TOP );
					}
					// . determining the visible part of the File content
					const int iRowFirstToPaint=std::max( (std::max(rcClip.top,dc.m_ps.rcPaint.top)-HEADER_HEIGHT)/font.charHeight, 0L );
					int iRowA= GetScrollPos(SB_VERT) + iRowFirstToPaint;
					const int nPhysicalRows=__logicalPositionToRow__( std::min<int>(F->GetLength(),logicalSize) );
					const int iRowLastToPaint= GetScrollPos(SB_VERT) + std::min(rcClip.bottom,dc.m_ps.rcPaint.bottom)/font.charHeight + 1;
					const int iRowZ=std::min<>( std::min<>(nPhysicalRows,iRowLastToPaint), iRowA+nRowsOnPage );
					// . filling the gaps between Addresses/Hexa/Ascii, and Label space to erase any previous Label
					const int xHexaStart=(addrLength+ADDRESS_SPACE_LENGTH)*font.charAvgWidth, xHexaEnd=xHexaStart+HEXA_FORMAT_LENGTH*nBytesInRow*font.charAvgWidth;
					const int xAsciiStart=xHexaEnd+HEXA_SPACE_LENGTH*font.charAvgWidth, xAsciiEnd=xAsciiStart+nBytesInRow*font.charAvgWidth;
					RECT r={ std::min<LONG>(addrLength*font.charAvgWidth,rcClip.right), std::max<LONG>(rcClip.top,HEADER_HEIGHT), std::min<LONG>(xHexaStart,rcClip.right), rcClip.bottom }; // Addresses-Hexa space; min(.) = to not paint over the scrollbar
					::FillRect( dc, &r, Utils::CRideBrush::White );
					r.left=std::min<LONG>(xHexaEnd,rcClip.right), r.right=std::min<LONG>(xAsciiStart,rcClip.right); // Hexa-Ascii space; min(.) = to not paint over the scrollbar
					::FillRect( dc, &r, Utils::CRideBrush::White );
					r.left=std::min<LONG>(xAsciiEnd,rcClip.right), r.right=std::min<>(rcClip.right,rcClip.right); // Label space; min(.) = to not paint over the scrollbar
					::FillRect( dc, &r, Utils::CRideBrush::White );
					// . drawing Addresses and data (both Ascii and Hexa parts)
					const COLORREF labelColor=Utils::GetSaturatedColor(::GetSysColor(COLOR_GRAYTEXT),1.7f);
					const Utils::CRidePen recordDelimitingHairline( 0, labelColor );
					const HGDIOBJ hPen0=::SelectObject( dc, recordDelimitingHairline );
						int address=__firstByteInRowToLogicalPosition__(iRowA), y=HEADER_HEIGHT+iRowFirstToPaint*font.charHeight;
						const int _selectionA=std::min<>(caret.selectionA,caret.position), _selectionZ=std::max<>(caret.position,caret.selectionA);
						PEmphasis pEmp=emphases;
						while (pEmp->z<address) pEmp=pEmp->pNext; // choosing the first visible Emphasis
						int nearestNextBookmarkPos=bookmarks.__getNearestNextBookmarkPosition__(address);
						F->Seek( address, CFile::begin );
						for( TCHAR buf[16]; iRowA<=iRowZ; iRowA++,y+=font.charHeight ){
							RECT rcHexa={ /*xHexaStart*/0, y, std::min<LONG>(xHexaEnd,rcClip.right), std::min<LONG>(y+font.charHeight,rcClip.bottom) }; // commented out as this rectangle also used to paint the Address
							RECT rcAscii={ std::min<LONG>(xAsciiStart,rcClip.right), y, std::min<LONG>(xAsciiEnd,rcClip.right), rcHexa.bottom };
							// : address
							if (addrLength){
								dc.SetContentPrintState( CHexaPaintDC::Normal, ::GetSysColor(COLOR_BTNFACE) );
								dc.DrawText( buf, ::wsprintf(buf,ADDRESS_FORMAT,HIWORD(address),LOWORD(address)), &rcHexa, DT_LEFT|DT_TOP );
							}
							rcHexa.left=xHexaStart;
							// : File content
							const bool isEof=F->GetPosition()==F->GetLength();
							BYTE nBytesExpected=std::min<int>( __firstByteInRowToLogicalPosition__(iRowA+1), F->GetLength() )-address;
							bool dataReady=false; // assumption
							pContentAdviser->GetRecordInfo( address, nullptr, nullptr, &dataReady );
							if (dataReady){
								// Record's data are known (there are some, some with error, or none)
								BYTE bytes[BYTES_MAX], nBytesRead=0;
								enum:BYTE{ Good, Bad, Fuzzy } byteStates[BYTES_MAX];
								while (const BYTE nMissing=nBytesExpected-nBytesRead){
									::SetLastError(ERROR_SUCCESS); // clearing any previous error
									const BYTE nNewBytesRead=F->Read( bytes+nBytesRead, nMissing );
									const TStdWinError err=::GetLastError();
									if (err==ERROR_READ_FAULT) // no Bytes are available
										break;
									if (nNewBytesRead>0){ // some more data read - Good or Bad
										::memset( byteStates+nBytesRead, err!=ERROR_SUCCESS, nNewBytesRead );
										nBytesRead+=nNewBytesRead;
									}else if (F->GetPosition()<F->GetLength()){ // no data read - probably because none could have been determined (e.g. fuzzy bits)
										byteStates[nBytesRead++]=Fuzzy;
										F->Seek( 1, CFile::current );
									}else{
										nBytesExpected=nBytesRead;
										break;
									}
								}
								if (nBytesRead==nBytesExpected)
									// entire Row available
									for( const BYTE *p=bytes; nBytesRead--; address++ ){
										// | choosing colors
										BYTE printFlags= byteStates[p-bytes]==Good ? CHexaPaintDC::Normal : CHexaPaintDC::Erroneous;
										if (_selectionA<=address && address<_selectionZ)
											printFlags|=CHexaPaintDC::Selected;
										else
											printFlags&=~CHexaPaintDC::Selected;
										COLORREF emphasisColor;
										if (pEmp->a<=address && address<pEmp->z)
											emphasisColor=COLOR_YELLOW;
										else{
											emphasisColor=COLOR_WHITE;
											if (address==pEmp->z) pEmp=pEmp->pNext;
										}
										dc.SetContentPrintState( printFlags, emphasisColor );
										// | Hexa
										const bool isFuzzy=byteStates[p-bytes]==Fuzzy;
										const int iByte=*p++;
										if (!isFuzzy)
											dc.DrawText( buf, ::wsprintf(buf,HEXA_FORMAT,iByte), &rcHexa, DT_LEFT|DT_TOP );
										else
											::DrawTextW( dc, L"\x2592\x2592 ", 3, &rcHexa, DT_LEFT|DT_TOP );
										if (address==nearestNextBookmarkPos){
											const RECT rcBookmark={ rcHexa.left, rcHexa.top, rcHexa.left+2*font.charAvgWidth, rcHexa.top+font.charHeight };
											::FrameRect( dc, &rcBookmark, Utils::CRideBrush::Black );
										}
										rcHexa.left+=HEXA_FORMAT_LENGTH*font.charAvgWidth;
										// | Ascii
										if (!isFuzzy)
											::DrawTextW(dc,
														::isprint(iByte) ? (LPCWSTR)&iByte : L"\x2219", 1, // if original character not printable, displaying a substitute one
														&rcAscii, DT_LEFT|DT_TOP|DT_NOPREFIX
													);
										else
											::DrawTextW( dc, L"\x2592", 1, &rcAscii, DT_LEFT|DT_TOP|DT_NOPREFIX );
										if (address==nearestNextBookmarkPos){
											const RECT rcBookmark={ rcAscii.left, rcAscii.top, rcAscii.left+font.charAvgWidth, rcAscii.top+font.charHeight };
											::FrameRect( dc, &rcBookmark, Utils::CRideBrush::Black );
											nearestNextBookmarkPos=bookmarks.__getNearestNextBookmarkPosition__(address+1);
										}
										rcAscii.left+=font.charAvgWidth;
									}
								else if (!isEof){
									// content not available (e.g. irrecoverable Sector read error)
									F->Seek( address+=nBytesExpected, CFile::begin );
									#define ERR_MSG	_T("» No data «")
									dc.SetContentPrintState( CHexaPaintDC::Erroneous, COLOR_WHITE );
									dc.DrawText( ERR_MSG, -1, &rcHexa, DT_LEFT|DT_TOP );
									rcHexa.left+=(sizeof(ERR_MSG)-sizeof(TCHAR))*font.charAvgWidth;
								}
							}else if (!isEof){
								// Record's data are not yet known - caller will refresh the HexaEditor when data for this Record are known
								F->Seek( address+=nBytesExpected, CFile::begin );
								#define STATUS_MSG	_T("» Fetching data ... «")
								dc.SetContentPrintState( CHexaPaintDC::Unknown, COLOR_WHITE );
								dc.DrawText( STATUS_MSG, -1, &rcHexa, DT_LEFT|DT_TOP );
								rcHexa.left+=(sizeof(STATUS_MSG)-sizeof(TCHAR))*font.charAvgWidth;
							}
							// : filling the rest of the Row with background color (e.g. the last Row in a Record may not span up to the end)
							if (rcHexa.left<rcHexa.right) // to not paint over the scrollbar
								::FillRect( dc, &rcHexa, Utils::CRideBrush::White );
							if (rcAscii.left<rcAscii.right) // to not paint over the scrollbar
								::FillRect( dc, &rcAscii, Utils::CRideBrush::White );
							// : drawing the Record label if the just drawn Row is the Record's first Row
							if (!isEof){ // yes, a new Record can potentially start at the Row
								WCHAR buf[80];
								if (const LPCWSTR recordLabel=pContentAdviser->GetRecordLabelW( __firstByteInRowToLogicalPosition__(iRowA), buf, sizeof(buf)/sizeof(WCHAR), param )){
									RECT rc={ rcAscii.right+2*font.charAvgWidth, y, rcClip.right, rcClip.bottom };
									const COLORREF textColor0=dc.SetTextColor(labelColor), bgColor0=dc.SetBkColor(COLOR_WHITE);
										::DrawTextW( dc, recordLabel, -1, &rc, DT_LEFT|DT_TOP );
										dc.MoveTo( addrLength*font.charAvgWidth, y );
										dc.LineTo( rcClip.right, y );
									dc.SetTextColor(textColor0), dc.SetBkColor(bgColor0);
								}
							}
						}
					::SelectObject(dc,hPen0);
					// . filling the rest of HexaEditor with background color
					rcClip.top=y;
					::FillRect( dc, &rcClip, Utils::CRideBrush::White );
				::SelectObject(dc,hFont0);
				return 0;
			}
			case WM_HEXA_PAINTSCROLLBARS:{
				// repainting the scrollbars
				locker.Lock();
					SCROLLINFO si={ sizeof(si), SIF_RANGE|SIF_PAGE, 0,nLogicalRows-1, nRowsOnPage };
					SetScrollInfo( SB_VERT, &si, TRUE );
					if (mouseInNcArea){
						const BOOL scrollbarNecessary=nRowsOnPage<nLogicalRows;
						ShowScrollBar(SB_VERT,scrollbarNecessary);
					}
				locker.Unlock();
				return 0;
			}
			case WM_DESTROY:{
				// window destroyed
				int i;
				GetVisiblePart( logPosScrolledTo, i );
				break;
			}
		}
		return __super::WindowProc(msg,wParam,lParam);
	}

	BOOL CHexaEditor::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:
				// update
				switch (nID){
					case ID_EDIT_COPY:
					case ID_FILE_SAVE_COPY_AS:
						((CCmdUI *)pExtra)->Enable( caret.selectionA!=caret.position );
						return TRUE;
					case ID_EDIT_PASTE:{
						COleDataObject odo;
						odo.AttachClipboard();
						((CCmdUI *)pExtra)->Enable( editable && odo.IsDataAvailable(cfBinary) );
						return TRUE;
					}
					case ID_BOOKMARK_TOGGLE:
						((CCmdUI *)pExtra)->Enable(
							caret.selectionA==caret.position // no selection
							&&
							caret.position<F->GetLength() // can't set a Bookmark beyond the content
						);
						((CCmdUI *)pExtra)->SetCheck( bookmarks.__getNearestNextBookmarkPosition__(caret.position)==caret.position );
						return TRUE;
					case ID_BOOKMARK_DELETEALL:
					case ID_EDIT_SELECT_ALL:
					case ID_EDIT_SELECT_NONE:
					case ID_EDIT_SELECT_CURRENT:
					case ID_NEXT:
					case ID_PREV:
					case ID_NAVIGATE_ADDRESS:
						((CCmdUI *)pExtra)->Enable(true);
						return TRUE;
					case ID_EDIT_FIND:
					case ID_EDIT_FIND_NEXT:
						((CCmdUI *)pExtra)->Enable(SEARCH_ENABLED);
						return TRUE;
					case ID_EDIT_PASTE_SPECIAL:
					case ID_EDIT_DELETE:
					case ID_ZERO:
					case ID_NUMBER:
						((CCmdUI *)pExtra)->Enable(editable);
						return TRUE;
					case ID_BOOKMARK_PREV:
						((CCmdUI *)pExtra)->Enable( bookmarks.__getNearestNextBookmarkPosition__(0)<caret.position );
						return TRUE;
					case ID_BOOKMARK_NEXT:
						((CCmdUI *)pExtra)->Enable( bookmarks.__getNearestNextBookmarkPosition__(caret.position+1)<BOOKMARK_POSITION_INFINITY );
						return TRUE;
					case ID_IMAGE_PROTECT:
						break; // leaving up to a higher logic to decide if write-protection can be removed
				}
				break;
			case CN_COMMAND:
				// command
				switch (nID){
					case ID_EDIT_SELECT_ALL:
					case ID_EDIT_SELECT_NONE:
					case ID_EDIT_SELECT_CURRENT:
					case ID_FILE_SAVE_COPY_AS:
					case ID_EDIT_COPY:
					case ID_EDIT_PASTE:
					case ID_EDIT_PASTE_SPECIAL:
					case ID_EDIT_DELETE:
					case ID_EDIT_FIND:
					case ID_EDIT_FIND_NEXT:
					case ID_BOOKMARK_TOGGLE:
					case ID_BOOKMARK_PREV:
					case ID_BOOKMARK_NEXT:
					case ID_BOOKMARK_DELETEALL:
					case ID_ZERO:
					case ID_NUMBER:
					case ID_NEXT:
					case ID_PREV:
					case ID_NAVIGATE_ADDRESS:
						WindowProc( WM_COMMAND, nID, 0 );
						return TRUE;
				}
				break;
		}
		return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	}

	BOOL CHexaEditor::PreTranslateMessage(PMSG pMsg){
		// pre-processing the Message
		return	::GetFocus()==m_hWnd
				?	::TranslateAccelerator(m_hWnd,hDefaultAccelerators,pMsg)
				: FALSE;
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
			return __super::OnRenderFileData(lpFormatEtc,pFile);
	}*/

