#include "stdafx.h"
using namespace Yahel;

	#define INI_COMPARISON	_T("FileCmp")

	#define INI_MSG			_T("msg")
	#define INI_POSITION	_T("pos")
	#define LABEL_HEIGHT	20

	CFileManagerView::CFileComparisonDialog *CFileManagerView::CFileComparisonDialog::pSingleInstance;





	CFileManagerView::CFileComparisonDialog::CFileComparisonDialog(const CFileManagerView &fm)
		// ctor
		: file1(*this) , file2(*this) , fm(fm) {
		// - creating and positioning the window (restoring position saved earlier in OnCancel)
		Create(IDR_FILEMANAGER_COMPARE_FILES);
		const CString s=app.GetProfileString(INI_COMPARISON,INI_POSITION,_T(""));
		if (!s.IsEmpty()){
			RECT r; int windowState=SW_NORMAL;
			_stscanf(s,_T("%d,%d,%d,%d,%d"),&r.left,&r.top,&r.right,&r.bottom,&windowState);
			::SetWindowPos( m_hWnd, 0, r.left,r.top, r.right-r.left,r.bottom-r.top, SWP_NOZORDER );
			ShowWindow(windowState); // minimized/maximized/normal
		}
		// - initialization
		file1.Init(GetDlgItem(ID_FILE_MRU_FILE1),GetDlgItem(ID_FILE1)),
		file2.Init(GetDlgItem(ID_FILE_MRU_FILE2),GetDlgItem(ID_FILE2));
		padding.x=padding.y=0;
		::MapWindowPoints(file1.hLabel,m_hWnd,&padding,1);
		RECT r;
		::GetClientRect( hCompareButton=GetDlgItemHwnd(IDOK) ,&r);
		buttonWidth=r.right, buttonHeight=r.bottom, addressColumnWidth=file1.GetAddressColumnWidth();
		file2.ShowColumns( IInstance::TColumn::MINIMAL );
		GetClientRect(&r);
		// - updating control layout
		SendMessage(WM_SIZE,0,MAKELONG(r.right,r.bottom));
		// - informing
		Utils::InformationWithCheckableShowNoMore(_T("Drop files to compare over their respective hexa-editors and press the Compare button."),INI_COMPARISON,INI_MSG);
	}




	void CFileManagerView::CFileComparisonDialog::SetComparison(CDos::PCFile f1){
		// opens specified File stored in currently open Image, and shows its content in HexaEditor
		file1.Open(f1);
	}

	void CFileManagerView::CFileComparisonDialog::SetComparison(CDos::PCFile f1,CDos::PCFile f2){
		// opens specified Files stored in currently open Image, and shows their contents in HexaEditors
		SetComparison(f1);
		file2.Open(f2);
	}

	void CFileManagerView::CFileComparisonDialog::OnOK(){
		// compares Files
		// - testing equality of names
		//nop (unimportant)
		// - testing equality of File Sizes
		//nop (see more general test of equality of File contents)
		// - testing equality of File contents
		file1.f->SeekToBegin(), file2.f->SeekToBegin(); // because HexaEditor showed their contents and thus influenced the File Positions
		const auto fileSize1=file1.f->GetLength(), fileSize2=file2.f->GetLength();
		auto length1=fileSize1, length2=fileSize2;
		for( DWORD nBytesRead; length1&&length2; length1-=nBytesRead,length2-=nBytesRead ){
			BYTE buf1[65536],buf2[65536];
			nBytesRead=std::min( file1.f->Read(buf1,sizeof(buf1)), file2.f->Read(buf2,sizeof(buf2)) );
			if (::memcmp(buf1,buf2,nBytesRead)){ // doing fast assembly language comparison first ...
				DWORD i=0;
				for( ; i<nBytesRead; i++ ) // ... and only if there's actually a difference, finding the position of that difference using slow FOR-loop
					if (buf1[i]!=buf2[i])
						break;
				length1-=i, length2-=i;
				break;
			}
		}
		file1.RepaintData(), file2.RepaintData();
		if (length1||length2){
			file1.ScrollTo( fileSize1-length1, true );
			file2.ScrollTo( fileSize2-length1, true );
			//Utils::Information(_T("No, the files differ in content! (File names are ignored.)")); // commented out as unnecessary - feedback already given by scrolling to first difference
		}else{
			file1.ScrollTo( 0, true );
			file2.ScrollTo( 0, true );
			Utils::Information(_T("Yes, the file contents are identical! (File names are ignored.)"));
		}
		file1.f->SeekToBegin(), file2.f->SeekToBegin();
	}
	
	void CFileManagerView::CFileComparisonDialog::OnCancel(){
		// closes the FileComparisonDialog
		// - saving window's current position for next time
		WINDOWPLACEMENT wp;
		GetWindowPlacement(&wp);
		TCHAR buf[50];
		::wsprintf(buf,_T("%d,%d,%d,%d,%d"),wp.rcNormalPosition,wp.showCmd);
		app.WriteProfileString(INI_COMPARISON,INI_POSITION,buf);
		// - closing
		file1.Revoke(), file2.Revoke();
		DestroyWindow();
	}

	#define WINDOW_PADDING			4
	#define BUTTON_ELLIPSIS_WIDTH	30

	LRESULT CFileManagerView::CFileComparisonDialog::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_SIZE:{
				// window size changed
				const int wndW=GET_X_LPARAM(lParam), wndH=GET_Y_LPARAM(lParam);
				// . laying out Labels
				const int hexaH=wndH-3*padding.y-LABEL_HEIGHT-buttonHeight;
				const int hexaY=padding.y+LABEL_HEIGHT;
				const int hexa2W=(wndW-2*padding.x-WINDOW_PADDING-addressColumnWidth)/2;
				const int hexa1X=padding.x+addressColumnWidth;
				const int hexa2X=hexa1X+hexa2W+WINDOW_PADDING;
				::SetWindowPos( file1.hLabel, 0, hexa1X,padding.y, hexa2W-BUTTON_ELLIPSIS_WIDTH,LABEL_HEIGHT, SWP_NOZORDER );
				::SetWindowPos( file1.hEllipsisButton, 0, hexa1X+hexa2W-BUTTON_ELLIPSIS_WIDTH,padding.y, BUTTON_ELLIPSIS_WIDTH,LABEL_HEIGHT, SWP_NOZORDER );
				::SetWindowPos( file2.hLabel, 0, hexa2X,padding.y, hexa2W-BUTTON_ELLIPSIS_WIDTH,LABEL_HEIGHT, SWP_NOZORDER );
				::SetWindowPos( file2.hEllipsisButton, 0, hexa2X+hexa2W-BUTTON_ELLIPSIS_WIDTH,padding.y, BUTTON_ELLIPSIS_WIDTH,LABEL_HEIGHT, SWP_NOZORDER );
				// . laying out HexaEditors
				::SetWindowPos( file1, 0, hexa1X-addressColumnWidth,hexaY, addressColumnWidth+hexa2W,hexaH, SWP_NOZORDER|SWP_SHOWWINDOW );
				::SetWindowPos( file2, 0, hexa2X,hexaY, hexa2W,hexaH, SWP_NOZORDER|SWP_SHOWWINDOW );
				// . laying out buttons (can't use "GetDlgItem(.)->..." because buttons don't exist immediately after the Dialog has been created)
				const int buttonX=wndW-padding.x-buttonWidth+1, buttonY=wndH-padding.y-buttonHeight;
				SetDlgItemPos( IDCANCEL, buttonX, buttonY );
				SetDlgItemPos( IDOK, buttonX-padding.x-buttonWidth, buttonY );
				break;
			}
			case WM_COMMAND:
				// command processing
				if (wParam==MAKELONG(ID_FILE1,BN_CLICKED))
					file1.ChooseAndOpenPhysicalFile();
				else if (wParam==MAKELONG(ID_FILE2,BN_CLICKED))
					file2.ChooseAndOpenPhysicalFile();
				break;
			case WM_NCDESTROY:
				// destroying the window's non-client area
				__super::WindowProc(msg,wParam,lParam);
				delete pSingleInstance, pSingleInstance=nullptr;
				return 0;
		}
		return __super::WindowProc(msg,wParam,lParam);
	}









	CFileManagerView::CFileComparisonDialog::COleComparisonDropTarget::COleComparisonDropTarget(CFileComparisonDialog &rDialog)
		// ctor
		: CHexaEditor(nullptr)
		, rDialog(rDialog) {
		SetLabelColumnParams( 0 );
	}





	void CFileManagerView::CFileComparisonDialog::COleComparisonDropTarget::Init(CWnd *pLabel,CWnd *pButton){
		// initialization
		hLabel=pLabel->m_hWnd, hEllipsisButton=pButton->m_hWnd;
		Create( nullptr, nullptr, WS_CHILD|WS_CLIPSIBLINGS /*|WS_VISIBLE*/, CFrameWnd::rectDefault, pLabel->GetParent(), 0 ); // commented out because see WM_SIZE
		Register(this); // making HexaEditor a target of drag&drop
		DragAcceptFiles(); // to not pass the WM_DROPFILES message to the MainWindow (which would attempt to open the dropped File as an Image)
	}

	void CFileManagerView::CFileComparisonDialog::COleComparisonDropTarget::ChooseAndOpenPhysicalFile(){
		// shows the "Open File" dialog, opens chosen physical File upon confirmation and shows its content in HexaEditor
		TCHAR buf[MAX_PATH];
		CFileDialog d(TRUE);
			d.m_ofn.lStructSize=sizeof(OPENFILENAME); // to show the "Places bar"
			d.m_ofn.lpstrFile=buf;
		*buf='\0';
		if (d.DoModal()==IDOK){
			#ifdef UNICODE
				OpenPhysicalFile(buf);
			#else
				WCHAR fileNameW[MAX_PATH];
				::MultiByteToWideChar( CP_ACP,0, buf,-1, fileNameW,ARRAYSIZE(fileNameW) );
				OpenPhysicalFile(fileNameW);
			#endif
		}
	}

	void CFileManagerView::CFileComparisonDialog::COleComparisonDropTarget::OpenPhysicalFile(LPCWSTR fileName){
		// opens chosen physical File upon confirmation and shows its content in HexaEditor
		if (IStream *const s=Stream::FromFileForSharedReading(fileName)){
			Open( s, fileName );
			s->Release();
		}else
			Utils::FatalError( _T("Can't open"), ::GetLastError() );
	}

	void CFileManagerView::CFileComparisonDialog::COleComparisonDropTarget::Open(CDos::PCFile f){
		// opens specified File stored in currently open Image, and shows its content in HexaEditor
		const PCDos dos=rDialog.fm.tab.image->dos;
		CComPtr<IStream> s;
		s.p=new CDos::CFileReaderWriter(dos,f);
		Open( s, dos->GetFilePresentationNameAndExt(f).GetUnicode() );
	}

	void CFileManagerView::CFileComparisonDialog::COleComparisonDropTarget::Open(IStream *s,LPCWSTR fileName){
		// opens specified File and shows its content in HexaEditor
		// - freeing any previous File
		if (f)
			f.reset();
		// - showing the File in HexaEditor
		f.reset( new COleStreamFile(s) );
		s->AddRef();
		Update( s, nullptr, f->GetLength() );
		::SetWindowTextW(hLabel,fileName);
		rDialog.InvalidateDlgItem(hLabel);
		// - updating the LogicalSizes of both HexaEditors to BiggerSize of the two Files
		const auto L1= rDialog.file1.f ? rDialog.file1.f->GetLength() : 0;
		const auto L2= rDialog.file2.f ? rDialog.file2.f->GetLength() : 0;
		const auto biggerSize=std::max(L1,L2);
			rDialog.file1.SetLogicalSize(biggerSize);
			rDialog.file2.SetLogicalSize(biggerSize);
		// - Drop always succeeds
		if (rDialog.EnableDlgItem( IDOK, rDialog.file1.f&&rDialog.file2.f ))
			rDialog.OnOK(); // if both Files specified, automatically triggering the comparison
	}

	DROPEFFECT CFileManagerView::CFileComparisonDialog::COleComparisonDropTarget::OnDragEnter(CWnd *,COleDataObject *pDataObject,DWORD dwKeyState,CPoint point){
		// cursor entered the window's client area
		// - accepted are only objects of Shell or (any) instance of FileManager (even across instances of applications)
		return	pDataObject->GetGlobalData(CF_HDROP) || pDataObject->GetGlobalData(CRideApp::cfRideFileList) || pDataObject->GetGlobalData(CRideApp::cfDescriptor)
				? DROPEFFECT_COPY
				: DROPEFFECT_NONE;
	}
	DROPEFFECT CFileManagerView::CFileComparisonDialog::COleComparisonDropTarget::OnDragOver(CWnd *pWnd,COleDataObject *pDataObject,DWORD dwKeyState,CPoint point){
		// dragged cursor moved above this window
		return OnDragEnter(pWnd,pDataObject,dwKeyState,point);
	}

	BOOL CFileManagerView::CFileComparisonDialog::COleComparisonDropTarget::OnDrop(CWnd *,COleDataObject *pDataObject,DROPEFFECT dropEffect,CPoint point){
		// dragged cursor released above window
		// - accepting first dragged File (ignoring others)
		if (const HGLOBAL hg=pDataObject->GetGlobalData(CF_HDROP)){
			// physical Files (dragged over from Explorer)
			if (const HDROP hDrop=(HDROP)::GlobalLock(hg)){
				WCHAR buf[MAX_PATH];
				::DragQueryFileW(hDrop,0,buf,ARRAYSIZE(buf)); // 0 = only first File, others ignored
				OpenPhysicalFile(buf);
				::DragFinish(hDrop);
				::GlobalUnlock(hg);
			}
		}else if (const HGLOBAL hg=pDataObject->GetGlobalData(CRideApp::cfDescriptor)){
			// virtual Files (dragged over from an instance of FileManager, even from another instance of this application)
			if (const LPFILEGROUPDESCRIPTORW pfgd=(LPFILEGROUPDESCRIPTORW)::GlobalLock(hg)){
				FORMATETC etcFileContents={ CRideApp::cfContent, nullptr, DVASPECT_CONTENT, 0, TYMED_ISTREAM }; // 0 = only first File, others ignored
				if (COleStreamFile *const osf=dynamic_cast<COleStreamFile *>(pDataObject->GetFileData(CRideApp::cfContent,&etcFileContents))){ // abstracting virtual data into a File
					// File is readable (e.g. doesn't contain no "Sector no found" errors)
					const FILEDESCRIPTORW *const pfd=pfgd->fgd;
					osf->SetLength(pfd->nFileSizeLow);
					Open( osf->m_lpStream, pfd->cFileName );
				}
				::GlobalUnlock(hg);
			}
			::GlobalFree(hg);
		}
		// - Drop always succeeds
		return TRUE;
	}

	






	afx_msg void CFileManagerView::__compareFiles__(){
		// shows the FileComparison window
		// - making sure the Dialog is shown
		if (CFileComparisonDialog::pSingleInstance)
			CFileComparisonDialog::pSingleInstance->BringWindowToTop();
		else
			CFileComparisonDialog::pSingleInstance=new CFileComparisonDialog(*this);
		// - loading Files selected in currently open disk
		POSITION pos=GetFirstSelectedFilePosition();
		switch (GetCountOfSelectedFiles()){
			case 0: // no File selected
				break;
			case 1: // single File selected - openingit in the left pane of the Dialog
				CFileComparisonDialog::pSingleInstance->SetComparison(
					GetNextSelectedFile(pos)
				);
				break;
			default: // two or more Files selected - opening the first two in both panes, respectively
				CFileComparisonDialog::pSingleInstance->SetComparison(
					GetNextSelectedFile(pos),
					GetNextSelectedFile(pos)
				);
				break;
		}
	}








	

	LRESULT CFileManagerView::CFileComparisonDialog::COleComparisonDropTarget::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_HSCROLL:{
				// wanted to scroll horizontally
				// . base (doing the scrolling)
				__super::WindowProc(msg,wParam,lParam);
				// . synchronously scrolling the OtherHexaEditor
				auto &rOtherHexaEditor=	this==&rDialog.file1
										? rDialog.file2
										: rDialog.file1;
				rOtherHexaEditor.ScrollToColumn( GetHorzScrollPos() );
				return 0;
			}
			case WM_VSCROLL:{
				// wanted to scroll vertically
				// . base (doing the scrolling)
				__super::WindowProc(msg,wParam,lParam);
				// . synchronously scrolling the OtherHexaEditor
				auto &rOtherHexaEditor=	this==&rDialog.file1
										? rDialog.file2
										: rDialog.file1;
				rOtherHexaEditor.ScrollToRow( GetVertScrollPos() );
				return 0;
			}
			case WM_PAINT:{
				// drawing
				static bool currentlyRepainting; // to prevent from recurrent calls initiated by COM at various places
				if (currentlyRepainting)
					return 0;
				const Utils::CVarTempReset<bool> cr0( currentlyRepainting, true );
				RemoveAllHighlights();
				CFile *thisFile,*otherFile;
				if (this==&rDialog.file1)
					thisFile=rDialog.file1.f.get(), otherFile=rDialog.file2.f.get();
				else
					thisFile=rDialog.file2.f.get(), otherFile=rDialog.file1.f.get();
				if (!thisFile || !otherFile) break; // if one of Files doesn't exist, we can't test their equality
				// . determining visible portion of ThisFile
				auto L=thisFile->GetLength();
				const auto &&visible=GetVisiblePart();
				auto a=visible.a, z=visible.z;
				if (a>=L) break; // it's being scrolled "behind" ThisFile (e.g. because the OtherFile is longer)
				if (z>L) z=L;
				// . creating a list of differences in visible portion of ThisFile (it's useless to create differences for invisible part/s of it)
				L=otherFile->GetLength();
				if (a>=L){ AddHighlight(a,z); break; } // ThisFile is longer than the OtherFile
				if (z>L){ AddHighlight(L,z); z=L; } // ThisFile is longer than the OtherFile
				auto diffBegin=z; // Z = there's no difference between the Files
				for( thisFile->Seek(a,CFile::begin),otherFile->Seek(a,CFile::begin); a<z; a++ ){
					BYTE b1,b2;
					thisFile->Read(&b1,1), otherFile->Read(&b2,1);
					if (b1==b2 && diffBegin<a){
						// end of difference iff A&B: A = there's no difference at current position, B = there is a difference at previous position
						AddHighlight(diffBegin,a); diffBegin=z;
					}else if (b1!=b2 && diffBegin==z)
						// begin of difference iff A&B: A = there's a difference at current position, B = there is no difference at previous position
						diffBegin=a;
				}
				if (diffBegin<z) AddHighlight(diffBegin,z);
				// . drawing
				break;
			}
			case WM_DESTROY:
				// window destroyed
				RemoveAllHighlights();
				break;
		}
		return __super::WindowProc(msg,wParam,lParam);
	}
