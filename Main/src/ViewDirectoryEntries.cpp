#include "stdafx.h"
using namespace Yahel;

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
				recordLength=Stream::MaximumRecordLength;
		}

		HRESULT STDMETHODCALLTYPE Clone(IStream **ppstm) override{
			// creates an exact copy of this object
			if (ppstm){
				*ppstm=new CDirectoryEntriesReaderWriter(*this);
				return S_OK;
			}else
				return E_INVALIDARG;
		}

		LPCWSTR GetRecordLabelW(TPosition logPos,PWCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const override{
			// populates the Buffer with label for the Record that STARTS at specified LogicalPosition, and returns the Buffer; returns Null if no Record starts at specified LogicalPosition
			auto d=div( logPos, (TPosition)recordLength );
			if (!d.rem){
				const CDirEntriesView *const pdev=(CDirEntriesView *)param;
				if (const auto pdt=pdev->DOS->BeginDirectoryTraversal(pdev->directory)){
					while (pdt->AdvanceToNextEntry() && d.quot>0)
						d.quot--;
					if (!d.quot) // successfully navigated to the DirectoryEntry
						switch (pdt->entryType){
							case CDos::TDirectoryTraversal::SUBDIR:
							case CDos::TDirectoryTraversal::FILE:
								return ::lstrcpynW( labelBuffer, pdev->DOS->GetFilePresentationNameAndExt(pdt->entry).GetUnicode(), labelBufferCharsMax );
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
		: CHexaEditor( this )
		// - initialization
		, tab( IDR_DIRECTORYBROWSER, IDR_HEXAEDITOR, ID_CYLINDER, dos->image, this )
		, fileToSeekTo(fileToSeekTo)
		, sectorLength(dos->formatBoot.sectorLength)
		, directory(directory) {
		// - modification of default HexaEditor's ContextMenu
		const Utils::CRideContextMenu mainMenu( *tab.menu.GetSubMenu(0) );
		contextMenu.ModifySubmenu(
			contextMenu.GetPosByContainedSubcommand(ID_YAHEL_EDIT_RESET_ZERO),
			*mainMenu.GetSubMenu( mainMenu.GetPosByContainedSubcommand(ID_DEFAULT1) )
		);
		contextMenu.AppendSeparator();
		contextMenu.AppendMenu( MF_BYCOMMAND|MF_STRING, ID_TIME, mainMenu.GetMenuStringByCmd(ID_TIME) );
	}

	BEGIN_MESSAGE_MAP(CDirEntriesView,CHexaEditor)
		ON_WM_CREATE()
		ON_COMMAND(ID_IMAGE_PROTECT,ToggleWriteProtection)
		ON_COMMAND(ID_FILE_CLOSE,__closeView__)
	END_MESSAGE_MAP()







	afx_msg int CDirEntriesView::OnCreate(LPCREATESTRUCT lpcs){
		// window created
		// - base
		if (__super::OnCreate(lpcs)==-1)
			return -1;
		// - displaying the content
		OnUpdate(nullptr,0,nullptr);
		instance->Attach(*this);
		// - recovering the Scroll position and repainting the view (by setting its editability)
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
		const Utils::CVarTempReset<WORD> sl0( DOS->formatBoot.sectorLength, sectorLength );
		f.Attach( new CDirectoryEntriesReaderWriter(DOS,directory) );
		Update( f, f, f->GetLength() );
	}

	afx_msg void CDirEntriesView::ToggleWriteProtection(){
		// toggles Image's WriteProtection flag
		IMAGE->ToggleWriteProtection(); // "base"
		SetEditable( !IMAGE->IsWriteProtected() );
	}

	afx_msg void CDirEntriesView::__closeView__(){
		CTdiCtrl::RemoveCurrentTab( TDI_HWND );
	}

	int CDirEntriesView::GetCustomCommandMenuFlags(WORD cmd) const{
		// custom command GUI update
		switch (cmd){
			case ID_DEFAULT1:
				return MF_GRAYED*!( IsEditable() );
			case ID_TIME:
				return MF_GRAYED*!( IMAGE->ReadTrack(0,0) );
		}
		return __super::GetCustomCommandMenuFlags(cmd);
	}

	bool CDirEntriesView::ProcessCustomCommand(UINT cmd){
		// custom command processing
		switch (cmd){
			case ID_DEFAULT1:{
				// resetting selected DirectoryEntries to their default content
				// . getting the selection range
				auto sel=GetSelectionAsc();
				// . navigating to the first (at least partially) selected DirectoryEntry
				const auto pdt=DOS->BeginDirectoryTraversal(directory);
				for( DWORD n=sel.a/pdt->entrySize; n-->0; pdt->AdvanceToNextEntry() );
				// . resetting the selected portion of DirectoryEntries
				for( TPosition dirEntryStart; sel; ){
					pdt->AdvanceToNextEntry();
					f->GetRecordInfo( sel.a, &dirEntryStart, nullptr, nullptr );
					if (sel.a==dirEntryStart && dirEntryStart+pdt->entrySize<=sel.z){
						// whole DirectoryEntry requested to reset
						pdt->ResetCurrentEntry(DOS->properties->directoryFillerByte);
						IMAGE->MarkSectorAsDirty(pdt->chs);
						f->Seek( sel.a+=pdt->entrySize, CFile::begin );
					}else{
						// just a part of the DirectoryEntry requested to reset
						BYTE orgDirEntry[4096]; // should suffice to accommodate DirectoryEntry of *any* DOS
						f->Seek( dirEntryStart, CFile::begin );
						f->Read( orgDirEntry, pdt->entrySize );
						pdt->ResetCurrentEntry(DOS->properties->directoryFillerByte);
						f->Seek( dirEntryStart, CFile::begin );
						BYTE rstDirEntry[4096]; // should suffice to accommodate DirectoryEntry of *any* DOS
						f->Read( rstDirEntry, pdt->entrySize );
						::memcpy( orgDirEntry+sel.a-dirEntryStart, rstDirEntry+sel.a-dirEntryStart, std::min(sel.GetLength(),(TPosition)pdt->entrySize) );
						f->Seek( dirEntryStart, CFile::begin );
						f->Write( orgDirEntry, pdt->entrySize );
						sel.a=std::min( dirEntryStart+pdt->entrySize, sel.z );
					}
				}
				RepaintData();
				return true;
			}
			case ID_TIME:
				// display of low-level Track timing
				f->Seek( GetCaretPosition(), CFile::begin );
				IMAGE->ShowModalTrackTimingAt( f->GetCurrentPhysicalAddress(), 0, f->GetPositionInCurrentSector(), Revolution::ANY_GOOD );
				return true;
		}
		return __super::ProcessCustomCommand(cmd);
	}
