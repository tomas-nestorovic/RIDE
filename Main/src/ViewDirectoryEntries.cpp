#include "stdafx.h"

	#define INI_DIRENTRIES	_T("DirEnt")

	#define IMAGE	tab.image
	#define DOS		IMAGE->dos

	class CDirectoryEntriesReaderWriter sealed:public CDos::CFileReaderWriter{
	public:
		CDirectoryEntriesReaderWriter(const CDos *dos,CDos::PCFile directory)
			// ctor
			// - base
			: CDos::CFileReaderWriter(dos,directory,true) {
			// - determining the DirectoryEntry size in Bytes
			if (const auto pdt=dos->BeginDirectoryTraversal(directory))
				recordLength=pdt->entrySize;
			else
				recordLength=HEXAEDITOR_RECORD_SIZE_INFINITE;
		}

		LPCWSTR GetRecordLabelW(int logPos,PWCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const override{
			// populates the Buffer with label for the Record that STARTS at specified LogicalPosition, and returns the Buffer; returns Null if no Record starts at specified LogicalPosition
			div_t d=div( logPos, recordLength );
			if (!d.rem){
				const CDirEntriesView *const pdev=(CDirEntriesView *)param;
				if (const auto pdt=pdev->DOS->BeginDirectoryTraversal(pdev->directory)){
					while (pdt->AdvanceToNextEntry() && d.quot>0)
						d.quot--;
					if (!d.quot) // successfully navigated to the DirectoryEntry
						switch (pdt->entryType){
							case CDos::TDirectoryTraversal::SUBDIR:
							case CDos::TDirectoryTraversal::FILE:
								#ifdef UNICODE
									return ::lstrcpyn( labelBuffer, pdev->DOS->GetFilePresentationNameAndExt(pdt->entry), labelBufferCharsMax );
								#else
									::MultiByteToWideChar( CP_ACP, 0, pdev->DOS->GetFilePresentationNameAndExt(pdt->entry),-1, labelBuffer,labelBufferCharsMax );
									return labelBuffer;
								#endif
							case CDos::TDirectoryTraversal::EMPTY:
								return L"[ empty ]";
						}
				}
			}
			return nullptr; // no description exists for the DirectoryEntry
		}
	};



	CDirEntriesView::CDirEntriesView(PDos dos,CDos::PFile directory,CDos::PCFile fileToSeekTo)
		// ctor
		// - base
		: CHexaEditor( this, nullptr, Utils::CreateSubmenuByContainedCommand(IDR_DIRECTORYBROWSER,ID_DEFAULT1) )
		// - initialization
		, tab( IDR_DIRECTORYBROWSER, IDR_HEXAEDITOR, ID_CYLINDER, dos->image, this )
		, fileToSeekTo(fileToSeekTo)
		, sectorLength(dos->formatBoot.sectorLength)
		, directory(directory) {
	}

	BEGIN_MESSAGE_MAP(CDirEntriesView,CHexaEditor)
		ON_WM_CREATE()
		ON_COMMAND(ID_IMAGE_PROTECT,ToggleWriteProtection)
		ON_COMMAND(ID_FILE_CLOSE,__closeView__)
		ON_WM_DESTROY()
	END_MESSAGE_MAP()







	afx_msg int CDirEntriesView::OnCreate(LPCREATESTRUCT lpcs){
		// window created
		// - base
		if (__super::OnCreate(lpcs)==-1)
			return -1;
		// - displaying the content
{		const Utils::CVarTempReset<WORD> sl0( DOS->formatBoot.sectorLength, sectorLength );
		OnUpdate(nullptr,0,nullptr);
}		// - recovering the Scroll position and repainting the view (by setting its editability)
		SetEditable( !IMAGE->IsWriteProtected() );
		// - navigating to a particular Directory entry
		if (fileToSeekTo)
			if (POSITION pos=DOS->pFileManager->GetFirstSelectedFilePosition())
				if (const auto pdt=DOS->BeginDirectoryTraversal())
					for( int iPos=0; pdt->AdvanceToNextEntry(); iPos+=pdt->entrySize )
						if (pdt->entry==fileToSeekTo){
							ScrollTo( iPos, true );
							fileToSeekTo=nullptr; // just seeked, do nothing when switched to this View next time
							break;
						}
		return 0;
	}

	void CDirEntriesView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		f.reset( new CDirectoryEntriesReaderWriter(DOS,directory) );
		const auto dirLength=f->GetLength();
		Update( f.get(), dirLength, dirLength );
	}

	afx_msg void CDirEntriesView::OnDestroy(){
		// window destroyed
		// - disposing the underlying File
		f.reset();
		// - base
		__super::OnDestroy();
	}

	afx_msg void CDirEntriesView::ToggleWriteProtection(){
		// toggles Image's WriteProtection flag
		IMAGE->ToggleWriteProtection(); // "base"
		SetEditable( !IMAGE->IsWriteProtected() );
	}

	afx_msg void CDirEntriesView::__closeView__(){
		CTdiCtrl::RemoveCurrentTab( TDI_HWND );
	}

	BOOL CDirEntriesView::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:
				// update
				switch (nID){
					case ID_DEFAULT1:
						((CCmdUI *)pExtra)->Enable( IsEditable() );
						return TRUE;
				}
				break;
			case CN_COMMAND:
				// command
				switch (nID){
					case ID_DEFAULT1:{
						// resetting selected DirectoryEntries to their default content
						// . getting the selection range
						int selA,selZ;
						GetLogicalSelection( &selA, &selZ );
						if (selA>selZ)
							std::swap(selA,selZ);
						// . navigating to the first (at least partially) selected DirectoryEntry
						const auto pdt=DOS->BeginDirectoryTraversal(directory);
						for( DWORD n=selA/pdt->entrySize; n-->0; pdt->AdvanceToNextEntry() );
						// . resetting the selected portion of DirectoryEntries
						for( int dirEntryStart; selA<selZ; ){
							pdt->AdvanceToNextEntry();
							f->GetRecordInfo( selA, &dirEntryStart, nullptr, nullptr );
							if (selA==dirEntryStart && dirEntryStart+pdt->entrySize<=selZ){
								// whole DirectoryEntry requested to reset
								pdt->ResetCurrentEntry(DOS->properties->directoryFillerByte);
								IMAGE->MarkSectorAsDirty(pdt->chs);
								f->Seek( selA+=pdt->entrySize, CFile::begin );
							}else{
								// just a part of the DirectoryEntry requested to reset
								BYTE orgDirEntry[4096]; // should suffice to accommodate DirectoryEntry of *any* DOS
								f->Seek( dirEntryStart, CFile::begin );
								f->Read( orgDirEntry, pdt->entrySize );
								pdt->ResetCurrentEntry(DOS->properties->directoryFillerByte);
								f->Seek( dirEntryStart, CFile::begin );
								BYTE rstDirEntry[4096]; // should suffice to accommodate DirectoryEntry of *any* DOS
								f->Read( rstDirEntry, pdt->entrySize );
								::memcpy( orgDirEntry+selA-dirEntryStart, rstDirEntry+selA-dirEntryStart, std::min<size_t>(selZ-selA,pdt->entrySize) );
								f->Seek( dirEntryStart, CFile::begin );
								f->Write( orgDirEntry, pdt->entrySize );
								selA=std::min<>( dirEntryStart+pdt->entrySize, selZ );
							}
						}
						RepaintData();
						return TRUE;
					}
				}
				break;
		}
		return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	}
