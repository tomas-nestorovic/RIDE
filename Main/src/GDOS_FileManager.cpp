#include "stdafx.h"
#include "GDOS.h"

	#define INFORMATION_COUNT		9
	#define INFORMATION_NAME		1 /* column to sort by */
	#define INFORMATION_TYPE		2 /* column to sort by */
	#define INFORMATION_SIZE		3 /* column to sort by */
	#define INFORMATION_SECTOR_COUNT 4 /* column to sort by */
	#define INFORMATION_SECTOR_FIRST 5 /* column to sort by */
	#define INFORMATION_PARAM_1		6 /* column to sort by */
	#define INFORMATION_PARAM_2		7 /* column to sort by */
	#define INFORMATION_ETC			8 /* column to sort by */

	const CFileManagerView::TFileInfo CGDOS::CGdosFileManagerView::InformationList[INFORMATION_COUNT]={
		{ nullptr,				LVCFMT_LEFT,	8 }, // auxiliary column to indent the first information from left edge of window
		{ _T("Name"),		LVCFMT_LEFT,	180 },
		{ _T("Type"),		LVCFMT_RIGHT,	110 },
		{ _T("Size"),		LVCFMT_RIGHT,	60 },
		{ _T("Sectors"),	LVCFMT_RIGHT,	55 },
		{ _T("First sector"),LVCFMT_RIGHT,	100 },
		{ ZX_PARAMETER_1,	LVCFMT_RIGHT,	90 },
		{ ZX_PARAMETER_2,	LVCFMT_RIGHT,	80 },
		{ _T("Etc."),		LVCFMT_LEFT,	80 }
	};

	CGDOS::CGdosFileManagerView::CGdosFileManagerView(PGDOS gdos)
		// ctor
		: CSpectrumFileManagerView( gdos, gdos->zxRom, REPORT, LVS_REPORT, INFORMATION_COUNT, InformationList ) {
	}








	#define DOS	tab.dos

	void CGDOS::CGdosFileManagerView::DrawFileInfo(LPDRAWITEMSTRUCT pdis,const int *tabs) const{
		// draws Information on File
		RECT r=pdis->rcItem;
		const HDC dc=pdis->hDC;
		const PDirectoryEntry de=(PDirectoryEntry)pdis->itemData;
		// . color distinction of Files based on their Type
		if (!pdis->itemState&ODS_SELECTED)
			switch (de->fileType){
				case TDirectoryEntry::BASIC			: ::SetTextColor(dc,FILE_MANAGER_COLOR_EXECUTABLE); break;
				case TDirectoryEntry::CHAR_ARRAY	: ::SetTextColor(dc,0xff00ff); break;
				case TDirectoryEntry::NUMBER_ARRAY	: ::SetTextColor(dc,0xff00); break;
				//case TDirectoryEntry::BLOCK		: break;
				case TDirectoryEntry::SNAPSHOT_48K	:
				case TDirectoryEntry::SNAPSHOT_128K: ::SetTextColor(dc,FILE_MANAGER_COLOR_EXECUTABLE); break;
				case TDirectoryEntry::OPENTYPE		: ::SetTextColor(dc,0x999999); break;
			}
		TCHAR bufT[MAX_PATH];
		// . COLUMN: <indent from left edge>
		r.left=*tabs++;
		// . COLUMN: Name
		r.right=*tabs++;
			zxRom.PrintAt( dc, TZxRom::ZxToAscii(de->name,GDOS_FILE_NAME_LENGTH_MAX,bufT), r, DT_SINGLELINE|DT_VCENTER );
		r.left=r.right;
		// . COLUMN: Type
		r.right=*tabs++;
			::DrawText( dc, de->__getFileTypeDesc__(bufT),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
		r.left=r.right;
		// . COLUMN: size
		r.right=*tabs++;
			::DrawText( dc, _itot(DOS->GetFileOfficialSize(de),bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
		r.left=r.right;
		// . COLUMN: # of Sectors
		r.right=*tabs++;
			::DrawText( dc, _itot(de->nSectors,bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
		r.left=r.right;
		// . COLUMN: first Sector
		r.right=*tabs++;
			::wsprintf( bufT, _T("Tr%d/Sec%d"), de->firstSector.__getChs__().GetTrackNumber(), de->firstSector.sector );
			::DrawText( dc, bufT,-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
		r.left=r.right;
		// . COLUMN: start address / Basic start line
		r.right=*tabs++;
			if (const PCWORD pw=de->__getStdParameter1__())
				::DrawText( dc, _itot(*pw,bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
			else
				::DrawText( dc, _T("N/A"),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
		r.left=r.right;
		// . COLUMN: length of Basic Program without variables
		r.right=*tabs++;
			if (const PCWORD pw=de->__getStdParameter2__())
				::DrawText( dc, _itot(*pw,bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
			else
				::DrawText( dc, _T("N/A"),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
		r.left=r.right;
		// . COLUMN: etc.
		r.right=*tabs++;
			PTCHAR p=bufT;
			for( BYTE i=0; i<sizeof(TDirectoryEntry::UEtc); p+=::wsprintf(p,_T(" %02X"),*((PCBYTE)&de->etc+i++)) );
			::DrawText( dc, bufT,-1, &r, DT_SINGLELINE|DT_VCENTER|DT_LEFT );
		//r.left=r.right;
	}

	int CGDOS::CGdosFileManagerView::CompareFiles(PCFile file1,PCFile file2,BYTE information) const{
		// determines the order of given Files by the specified Information
		const PDirectoryEntry f1=(PDirectoryEntry)file1, f2=(PDirectoryEntry)file2;
		switch (information){
			case INFORMATION_NAME:
				return ::strncmp(f1->name,f2->name,GDOS_FILE_NAME_LENGTH_MAX);
			case INFORMATION_TYPE:{
				TCHAR buf1[32],buf2[32];
				return ::lstrcmp( f1->__getFileTypeDesc__(buf1), f2->__getFileTypeDesc__(buf2) );
			}
			case INFORMATION_SIZE:
				return DOS->GetFileOfficialSize(f1)-DOS->GetFileOfficialSize(f2);
			case INFORMATION_SECTOR_COUNT:
				return f1->nSectors-f2->nSectors;
			case INFORMATION_SECTOR_FIRST:
				return f1->firstSector<f2->firstSector;
			case INFORMATION_PARAM_1:{
				WORD w1=0,w2=0;
				if (const PCWORD pw=f1->__getStdParameter1__()) w1=*pw;
				if (const PCWORD pw=f2->__getStdParameter1__()) w2=*pw;
				return w1-w2;
			}
			case INFORMATION_PARAM_2:{
				WORD w1=0,w2=0;
				if (const PCWORD pw=f1->__getStdParameter2__()) w1=*pw;
				if (const PCWORD pw=f2->__getStdParameter2__()) w2=*pw;
				return w1-w2;
			}
			case INFORMATION_ETC:
				return ::memcmp(&f1->etc,&f2->etc,sizeof(TDirectoryEntry::UEtc));
		}
		return 0;
	}

	bool WINAPI CGDOS::CGdosFileManagerView::__onStdParam1Changed__(PVOID file,int newWordValue){
		const PDirectoryEntry de=(PDirectoryEntry)file;
		de->__setStdParameter1__(newWordValue);
		if (const PGdosSectorData pData=(PGdosSectorData)CImage::GetActive()->GetHealthySectorData(de->firstSector.__getChs__()))
			pData->stdZxType=de->etc.stdZxType;
		__markDirectorySectorAsDirty__(file,0);
		return true;
	}
	bool WINAPI CGDOS::CGdosFileManagerView::__onStdParam2Changed__(PVOID file,int newWordValue){
		const PDirectoryEntry de=(PDirectoryEntry)file;
		de->__setStdParameter2__(newWordValue);
		if (const PGdosSectorData pData=(PGdosSectorData)CImage::GetActive()->GetHealthySectorData(de->firstSector.__getChs__()))
			pData->stdZxType=de->etc.stdZxType;
		__markDirectorySectorAsDirty__(file,0);
		return true;
	}
	CFileManagerView::PEditorBase CGDOS::CGdosFileManagerView::CreateFileInformationEditor(PFile file,BYTE infoId) const{
		// creates and returns Editor of File's selected Information; returns Null if Information cannot be edited
		const PDirectoryEntry de=(PDirectoryEntry)file;
		switch (infoId){
			case INFORMATION_NAME:
				return varLengthFileNameEditor.Create( de, GDOS_FILE_NAME_LENGTH_MAX, ' ');
			case INFORMATION_TYPE:
				return extensionEditor.Create(de);
			case INFORMATION_PARAM_1:
				if (const PWORD pw=de->__getStdParameter1__())
					return stdParamEditor.Create( de, pw, __onStdParam1Changed__ );
				else
					return nullptr;
			case INFORMATION_PARAM_2:
				if (const PWORD pw=de->__getStdParameter2__())
					return stdParamEditor.Create( de, pw, __onStdParam2Changed__ );
				else
					return nullptr;
			case INFORMATION_ETC:
				return etcEditor.Create(de);
			default:
				return nullptr;
		}
	}









	#define EXTENSION_MIN	1 /* because 0 = Empty DirectoryEntry */
	#define EXTENSION_MAX 255

	bool WINAPI CGDOS::CGdosFileManagerView::CExtensionEditor::__onChanged__(PVOID file,CPropGridCtrl::TEnum::UValue newExt){
		// changes the "extension" of selected File
		const PGDOS gdos=(PGDOS)CDos::GetFocused();
		// - validating File's new Name and Extension
		TDirectoryEntry *const de=(PDirectoryEntry)file;
		TCHAR bufOldName[GDOS_FILE_NAME_LENGTH_MAX+1];
		de->__getNameAndExt__(bufOldName,nullptr);
		const TCHAR bufNewExt[]={ newExt.charValue, '\0' };
		TDirectoryEntry tmp=*de; // backing-up the original DirectoryEntry
		TStdWinError err=gdos->ChangeFileNameAndExt(file,bufOldName,bufNewExt,file);
		if (err!=ERROR_SUCCESS){ // at least two Files with the same name
			Utils::Information(FILE_MANAGER_ERROR_RENAMING,err);
			return false;
		// - adjusting the File content to reflect the new FileType
		}else{ // the new name is unique
			// . getting information on File's original DataSize
			BYTE oldOffset,newOffset;
			const DWORD oldDataSize=tmp.__getDataSize__(&oldOffset), newDataSize=de->__getDataSize__(&newOffset);
			// . querying on eventual trimming of the File
			if (newDataSize<oldDataSize && !Utils::QuestionYesNo(_T("The file content needs to be trimmed.\n\nContinue?"),MB_DEFBUTTON2)){
				*de=tmp; // recovering the original DirectoryEntry
				return true;
			}
			// . getting the File FatPath with enough Sectors to accommodate the NewOffset before shifting the content
			const WORD nSectorsAfterRetyping=(newDataSize+newOffset+GDOS_SECTOR_LENGTH_STD-sizeof(TSectorInfo)-1)/(GDOS_SECTOR_LENGTH_STD-sizeof(TSectorInfo));
			CFatPath fatPath(gdos,&tmp);
			CFatPath::PCItem pItem; DWORD n;
			if (nSectorsAfterRetyping>de->nSectors){
				// adding one more Sector to the end of the File to accommodate the NewOffset (approached by importing a single-Byte File to the disk)
				BYTE buf; PFile p;
				CFatPath emptySector(gdos,sizeof(buf));
				err=gdos->__importFileData__( &CMemFile(&buf,sizeof(buf)), &tmp, _T(""), _T(""), sizeof(buf), p, emptySector );
				if (err!=ERROR_SUCCESS)
					goto error;
				gdos->DeleteFile(p); // deleting the imported auxiliary single-Byte File
				emptySector.GetItems(pItem,n);
				fatPath.AddItem(pItem); // interconnecting with existing Sectors of the File below
			}
			// . shifting the File content
			if (const LPCTSTR errMsg=fatPath.GetItems(pItem,n)){
				*de=tmp; // recovering the original DirectoryEntry
				gdos->__showFileProcessingError__(file,errMsg);
				return false;
			}else if (( err=gdos->__shiftFileContent__(fatPath,(char)newOffset-(char)oldOffset) )!=ERROR_SUCCESS){
error:			*de=tmp; // recovering the original DirectoryEntry
				gdos->__showFileProcessingError__(file,err);
				return false;
			}else if (de->__isStandardRomFile__())
				if (const PGdosSectorData pData=(PGdosSectorData)gdos->image->GetHealthySectorData(pItem->chs))
					pData->stdZxType=de->etc.stdZxType;
			// . adjusting connections between consecutive Sectors of the File, eventually freeing them up
			if (nSectorsAfterRetyping<de->nSectors){
				// File has become shorter (by trimming or shifting its data "to the left")
				if (nSectorsAfterRetyping){
					const TPhysicalAddress &rChs=(pItem+nSectorsAfterRetyping-1)->chs;
					if (const PGdosSectorData pData=(PGdosSectorData)gdos->image->GetHealthySectorData(rChs)){
						pData->nextSector.__setEof__();
						gdos->image->MarkSectorAsDirty(rChs);
					}else{
						err=ERROR_SECTOR_NOT_FOUND;
						goto error;
					}
				}else
					de->firstSector.__setEof__();
				for( WORD w=nSectorsAfterRetyping; w<de->nSectors; de->sectorAllocationBitmap.SetSectorAllocation((pItem+w++)->chs,false) );
			}else if (nSectorsAfterRetyping>de->nSectors){
				// File has become longer (by shifting its data "to the right")
				ASSERT(nSectorsAfterRetyping==1+de->nSectors); // File can be longer only by a single Sector
				if (de->nSectors){
					const TPhysicalAddress &rChs=(pItem+de->nSectors-1)->chs;
					if (const PGdosSectorData pData=(PGdosSectorData)gdos->image->GetHealthySectorData(rChs)){
						pData->nextSector.__setChs__((pItem+de->nSectors)->chs);
						gdos->image->MarkSectorAsDirty(rChs);
					}else{
						err=ERROR_SECTOR_NOT_FOUND;
						goto error;
					}
				}else
					de->firstSector.__setChs__(pItem->chs);
				const TPhysicalAddress &rChs=(pItem+de->nSectors)->chs;
				( (PGdosSectorData)gdos->image->GetHealthySectorData(rChs) )->nextSector.__setEof__(); // guaranteed that Sector always readable
				gdos->image->MarkSectorAsDirty(rChs);
				de->sectorAllocationBitmap.SetSectorAllocation(rChs,true);
			}
			de->nSectors=nSectorsAfterRetyping;
		}
		// - successfully changed FileType
		return true;
	}
	static CPropGridCtrl::TEnum::PCValueList WINAPI __createValues__(PVOID file,WORD &rnValues){
		// creates and returns the List of possible File "extensions"
		PBYTE list=(PBYTE)::malloc( rnValues=EXTENSION_MAX+1-EXTENSION_MIN ), a=list;
		for( WORD p=EXTENSION_MIN; p<=EXTENSION_MAX; *a++=p++ );
		return (CPropGridCtrl::TEnum::PCValueList)list;
	}
	static void WINAPI __freeValues__(PVOID,CPropGridCtrl::TEnum::PCValueList values){
		// disposes the List of possible File "extensions"
		::free((PVOID)values);
	}
	LPCTSTR WINAPI CGDOS::CGdosFileManagerView::CExtensionEditor::__getDescription__(PVOID file,CPropGridCtrl::TEnum::UValue extension,PTCHAR buf,short bufCapacity){
		// populates the Buffer with File "extension" description and returns the Buffer
		TDirectoryEntry tmp;
			tmp.fileType=(TDirectoryEntry::TFileType)extension.charValue;
		return tmp.__getFileTypeDesc__(buf);
	}
	CFileManagerView::PEditorBase CGDOS::CGdosFileManagerView::CExtensionEditor::Create(PDirectoryEntry de){
		// creates and returns the Editor of File "extension"
		const PDos dos=CDos::GetFocused();
		const CSpectrumFileManagerView *const pZxFileManager=(CSpectrumFileManagerView *)dos->pFileManager;
		const PEditorBase result=pZxFileManager->__createStdEditor__(
			de, &( data=de->fileType ), sizeof(data),
			CPropGridCtrl::TEnum::DefineConstStringListEditorA( __createValues__, __getDescription__, __freeValues__, __onChanged__ )
		);
		::SendMessage( result->hEditor, WM_SETFONT, (WPARAM)pZxFileManager->rFont.m_hObject, 0 );
		return result;
	}







/*
	CGDOS::TDirectoryEntry::UEtc::CEditor::CEditor(UEtc &rEtc)
		// ctor
		// - base
		: CHexaEditor(nullptr,nullptr) , CMemFile((PBYTE)&rEtc,sizeof(UEtc)) {
		// - initializing the HexaEditor
		__reset__(this,sizeof(UEtc),sizeof(UEtc));
		CreateEx( WS_EX_TOPMOST, HEXAEDITOR_BASE_CLASS, _T("Etc."), WS_CAPTION|WS_SYSMENU|WS_THICKFRAME, 0,0,640,240, nullptr, 0, nullptr );
		ShowWindow(SW_SHOW);
		// - making the MainWindow inactive
		//app.m_pMainWnd->BeginModalState();
	}

	LRESULT CGDOS::TDirectoryEntry::UEtc::CEditor::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		// - base
		const LRESULT res=CHexaEditor::WindowProc(msg,wParam,lParam);
		// - custom post-processing
		switch (msg){
			case WM_CLOSE:
				DestroyWindow();
				break;
		}
		return res;
	}*/

	bool WINAPI CGDOS::CGdosFileManagerView::CEtcEditor::__onEllipsisClick__(PVOID file,PVOID,short){
		// True <=> editing of File's Etc field confirmed, otherwise False
		const PDirectoryEntry de=(PDirectoryEntry)file;
		// - defining the Dialog
		class CEtcDialog sealed:public CDialog{
			CHexaEditor hexaEditor;
			CMemFile f;

			void PreInitDialog() override{
				// dialog initialization
				// - base
				CDialog::PreInitDialog();
				// - creating and showing the HexaEditor at the position of the placeholder (see Dialog's resource)
				RECT r;
				const CWnd *const pPlaceholder=GetDlgItem(ID_FILE);
				pPlaceholder->GetClientRect(&r);
				pPlaceholder->MapWindowPoints(this,&r);
				hexaEditor.Create( nullptr, nullptr, WS_CHILD|WS_VISIBLE, r, this, 0 );
			}
		public:
			TDirectoryEntry::UEtc etc;

			CEtcDialog(TDirectoryEntry::UEtc &rEtc)
				// ctor
				: CDialog(IDR_GDOS_FILE_ETC)
				, etc(rEtc) , hexaEditor(nullptr) , f((PBYTE)&etc,sizeof(etc)) {
				hexaEditor.Reset(&f,sizeof(etc),sizeof(etc));
			}
		} d(de->etc);
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			de->etc=d.etc;
			((PGDOS)CDos::GetFocused())->__markDirectorySectorAsDirty__(de);
			return true;
		}else
			return false;
	}

	CFileManagerView::PEditorBase CGDOS::CGdosFileManagerView::CEtcEditor::Create(PDirectoryEntry de){
		// creates and returns an Editor of File's Etc information
		return CDos::GetFocused()->pFileManager->__createStdEditorWithEllipsis__( de, __onEllipsisClick__ );
	}
