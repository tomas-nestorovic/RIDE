#include "stdafx.h"

	#define INI_DIRENTRIES	_T("DirEnt")

	#define DOS		tab.dos
	#define IMAGE	DOS->image

	static WORD __getDirectoryEntrySize__(PCDos dos,CDos::PFile directory){
		// determines and returns the DirectoryEntry size in Bytes (e.g. returns 32 for MDOS entries)
		if (const CDos::PDirectoryTraversal pdt=dos->BeginDirectoryTraversal(directory)){
			const WORD result=pdt->entrySize;
			dos->EndDirectoryTraversal(pdt);
			return result;
		}else
			return HEXAEDITOR_RECORD_SIZE_INFINITE;
	}

	static LPCTSTR __getRecordLabel__(int recordIndex,PTCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param){
		// determines and returns the DirectoryEntry description; returns NULL if description for the DirectoryEntry doesn't exist
		LPCTSTR result=NULL; // assumption (no description exists for the DirectoryEntry)
		const CDirEntriesView *const pdev=(CDirEntriesView *)param;
		if (const CDos::PDirectoryTraversal pdt=pdev->DOS->BeginDirectoryTraversal(pdev->directory)){
			while (pdt->AdvanceToNextEntry() && recordIndex>0)
				recordIndex--;
			if (!recordIndex) // successfully navigated to the DirectoryEntry
				switch (pdt->entryType){
					case CDos::TDirectoryTraversal::SUBDIR:
					case CDos::TDirectoryTraversal::FILE:
						result=pdev->DOS->GetFileNameWithAppendedExt( pdt->entry, labelBuffer );
						break;
					case CDos::TDirectoryTraversal::EMPTY:
						result=_T("[ empty ]");
						break;
				}
			pdev->DOS->EndDirectoryTraversal(pdt);
		}
		return result;
	}

	CDirEntriesView::CDirEntriesView(PDos dos,CDos::PFile directory)
		// ctor
		// - base
		: CHexaEditor( this, __getDirectoryEntrySize__(dos,directory), __getRecordLabel__ )
		// - initialization
		, tab(0,0,dos,this)
		, iScrollY(0)
		, directory(directory) {
	}

	BEGIN_MESSAGE_MAP(CDirEntriesView,CHexaEditor)
		ON_WM_CREATE()
		ON_COMMAND(ID_IMAGE_PROTECT,__toggleWriteProtection__)
		ON_COMMAND(ID_FILE_CLOSE,__closeView__)
		ON_WM_DESTROY()
	END_MESSAGE_MAP()

	CDirEntriesView::~CDirEntriesView(){
		// dtor
		// - discarding this View from the FileManager owner
		CPtrList &rList=DOS->pFileManager->ownedDirEntryViews;
		for( POSITION pos=rList.GetHeadPosition(); pos; ){
			const POSITION pos0=pos;
			const CDirEntriesView *const pdev=(CDirEntriesView *)rList.GetNext(pos);
			if (pdev==this){
				rList.RemoveAt(pos0);
				break;
			}
		}
	}






	afx_msg int CDirEntriesView::OnCreate(LPCREATESTRUCT lpcs){
		// window created
		// - base
		if (__super::OnCreate(lpcs)==-1)
			return -1;
		// - displaying the content
		f=new CDos::CFileReaderWriter(DOS,directory,true);
		OnUpdate(NULL,0,NULL);
		// - recovering the Scroll position and repainting the view (by setting its editability)
		SetScrollPos( SB_VERT, iScrollY );
		SetEditable( !IMAGE->IsWriteProtected() );
		return 0;
	}

	void CDirEntriesView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		Reset( f, f->GetLength(), f->GetLength() );
	}

	afx_msg void CDirEntriesView::OnDestroy(){
		// window destroyed
		// - saving Scroll position for later
		iScrollY=GetScrollPos(SB_VERT);
		// - disposing the underlying File
		delete f;
		// - base
		CView::OnDestroy();
	}

	afx_msg void CDirEntriesView::__toggleWriteProtection__(){
		// toggles Image's WriteProtection flag
		IMAGE->__toggleWriteProtection__(); // "base"
		SetEditable( !IMAGE->IsWriteProtected() );
	}

	afx_msg void CDirEntriesView::__closeView__(){
		CTdiCtrl::RemoveCurrentTab( TDI_HWND );
	}
