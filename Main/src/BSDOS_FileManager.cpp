#include "stdafx.h"
#include "BSDOS.h"

	#define INFORMATION_COUNT		11
	#define INFORMATION_DIR_NUMBER	0 /* column to sort by */
	#define INFORMATION_TYPE		1 /* column to sort by */
	#define INFORMATION_NAME		2 /* column to sort by */
	#define INFORMATION_SIZE		3 /* column to sort by */
	#define INFORMATION_SIZE_REPORTED	4 /* column to sort by */
	#define INFORMATION_STD_PARAM_1	5 /* column to sort by */
	#define INFORMATION_STD_PARAM_2	6 /* column to sort by */
	#define INFORMATION_CREATED		7 /* column to sort by */
	#define INFORMATION_FLAG		8 /* column to sort by */
	#define INFORMATION_CHECKSUM	9 /* column to sort by */
	#define INFORMATION_COMMENT		10 /* column to sort by */

	const CFileManagerView::TFileInfo CBSDOS308::CBsdos308FileManagerView::InformationList[INFORMATION_COUNT]={
		{ _T("Dir #"),		48,		TFileInfo::AlignRight },
		{ _T("Type"),		90,		TFileInfo::AlignRight },
		{ _T("Name"),		180,	TFileInfo::AlignLeft|TFileInfo::FileName },
		{ _T("Size"),		60,		TFileInfo::AlignRight },
		{ _T("Reported size"), 90,	TFileInfo::AlignRight },
		{ ZX_PARAMETER_1,	75,		TFileInfo::AlignRight },
		{ ZX_PARAMETER_2,	75,		TFileInfo::AlignRight },
		{ _T("Created"),	180,	TFileInfo::AlignRight },
		//{ _T("Special"),	75,		TFileInfo::AlignRight }, // TODO?
		{ _T("Block flag"),	75,		TFileInfo::AlignRight },
		{ _T("Checksum"),	75,		TFileInfo::AlignRight },
		{ _T("Comment"),	320,	TFileInfo::AlignLeft }
	};

	const CFileManagerView::TDirectoryStructureManagement CBSDOS308::CBsdos308FileManagerView::DirManagement={
		(CDos::TFnCreateSubdirectory)&CBSDOS308::CreateSubdirectory,
		(CDos::TFnChangeCurrentDirectory)&CBSDOS308::SwitchToDirectory,
		(CDos::TFnMoveFileToCurrDir)&CBSDOS308::MoveFileToCurrentDir
	};

	CBSDOS308::CBsdos308FileManagerView::CBsdos308FileManagerView(CBSDOS308 *bsdos)
		// ctor
		: CSpectrumFileManagerView( bsdos, bsdos->zxRom, REPORT, LVS_REPORT, INFORMATION_COUNT, InformationList, ZX_TAPE_FILE_NAME_LENGTH_MAX, &DirManagement )
		, dateTimeEditor(this) {
	}









	#define DOS		tab.dos
	#define BSDOS	((CBSDOS308 *)DOS)

	void CBSDOS308::CBsdos308FileManagerView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		reportModeDisplayedInfos =	BSDOS->currentDir==ZX_DIR_ROOT
									?	(1<<INFORMATION_DIR_NUMBER)+
										(1<<INFORMATION_NAME)+
										(1<<INFORMATION_CHECKSUM)+
										(1<<INFORMATION_CREATED)+
										(1<<INFORMATION_COMMENT)

									:	(1<<INFORMATION_TYPE)+
										(1<<INFORMATION_NAME)+
										(1<<INFORMATION_SIZE)+
										(1<<INFORMATION_SIZE_REPORTED)+
										(1<<INFORMATION_STD_PARAM_1)+
										(1<<INFORMATION_STD_PARAM_2)+
										(1<<INFORMATION_FLAG)+
										(1<<INFORMATION_CREATED);
		__super::OnUpdate(pSender,lHint,pHint);
	}

	void CBSDOS308::CBsdos308FileManagerView::DrawReportModeCell(PCFileInfo pFileInfo,LPDRAWITEMSTRUCT pdis) const{
		// draws Information on File
		RECT &r=pdis->rcItem;
		const HDC dc=pdis->hDC;
		if (DOS->IsDirectory((PCFile)pdis->itemData)){
			// root Directory
			const CDirsSector::PCSlot slot=(CDirsSector::PCSlot)pdis->itemData;
			const PCDirectoryEntry de=BSDOS->dirsSector.TryGetDirectoryEntry(slot);
			switch (pFileInfo-InformationList){
				case INFORMATION_DIR_NUMBER:
					// Directory number
					r.right-=10;
					integerEditor.DrawReportModeCell( slot-BSDOS->dirsSector.GetSlots(), pdis );
					break;
				case INFORMATION_NAME:
					// Directory Name
					if (de!=nullptr)
						varLengthCommandLineEditor.DrawReportModeCell( de->dir.name, ZX_TAPE_FILE_NAME_LENGTH_MAX, ' ', pdis );
					else{
						const int color0=::SetTextColor( dc, COLOR_RED );
							::DrawText( dc, BSDOS_DIR_CORRUPTED, -1, &r, DT_SINGLELINE|DT_VCENTER );
						::SetTextColor(dc,color0);
					}
					break;
				case INFORMATION_CHECKSUM:
					// Directory Name Checksum
					if (de!=nullptr){
						const BYTE actualChecksum=slot->nameChecksum, correctChecksum=de->GetDirNameChecksum();
						integerEditor.DrawReportModeCellWithCheckmark( actualChecksum, actualChecksum==correctChecksum, pdis );
					}
					break;
				case INFORMATION_CREATED:
					// Directory daytime created
					if (de!=nullptr)
						dateTimeEditor.DrawReportModeCell( de->dateTimeCreated, pdis );
					break;
				case INFORMATION_COMMENT:
					// Directory personalized Comment
					if (de!=nullptr)
						zxRom.PrintAt( dc, de->dir.comment, sizeof(de->dir.comment), r, DT_SINGLELINE|DT_VCENTER );
					break;
			}
		}else{
			// File
			// . color distinction of Files based on their Type
			const PCDirectoryEntry de=(PCDirectoryEntry)pdis->itemData;
			if ((pdis->itemState&ODS_SELECTED)==0)
				if (de->fileHasStdHeader)
					// File with Header
					switch (de->file.stdHeader.type){
						case TZxRom::PROGRAM		: ::SetTextColor(dc,FILE_MANAGER_COLOR_EXECUTABLE); break;
						case TZxRom::NUMBER_ARRAY	: ::SetTextColor(dc,0xff00); break;
						case TZxRom::CHAR_ARRAY		: ::SetTextColor(dc,0xff00ff); break;
						//case TZxRom::CODE			: break;
					}
				else
					// Headerless File
					::SetTextColor(dc,0x999999);
			// . drawing Information
			switch (pFileInfo-InformationList){
				case INFORMATION_TYPE:
					// standard header Type
					r.right-=5;
					stdTapeHeaderTypeEditor.DrawReportModeCell(
						de->fileHasStdHeader ? de->file.stdHeader.type : TZxRom::TFileType::HEADERLESS,
						pdis
					);
					break;
				case INFORMATION_NAME:
					// File Name
					if (de->fileHasStdHeader)
						varLengthCommandLineEditor.DrawReportModeCell( de->file.stdHeader.name, ZX_TAPE_FILE_NAME_LENGTH_MAX, ' ', pdis );
					break;
				case INFORMATION_SIZE:
					// File Size
					integerEditor.DrawReportModeCell( de->file.dataLength, pdis );
					break;
				case INFORMATION_SIZE_REPORTED:
					// File reported Size
					if (de->fileHasStdHeader)
						integerEditor.DrawReportModeCell( de->file.stdHeader.length, pdis );
					break;
				case INFORMATION_STD_PARAM_1:
					// start address / Basic start line
					if (de->fileHasStdHeader)
						integerEditor.DrawReportModeCell( de->file.stdHeader.params.param1, pdis );
					break;
				case INFORMATION_STD_PARAM_2:
					// length of Basic Program without variables
					if (de->fileHasStdHeader)
						integerEditor.DrawReportModeCell( de->file.stdHeader.params.param2, pdis );
					break;
				case INFORMATION_CREATED:
					// File daytime created
					dateTimeEditor.DrawReportModeCell( de->dateTimeCreated, pdis );
					break;
				case INFORMATION_FLAG:
					// File data block Flag
					integerEditor.DrawReportModeCell( de->file.dataFlag, pdis );
					break;
			}
		}
	}

	int CBSDOS308::CBsdos308FileManagerView::CompareFiles(PCFile file1,PCFile file2,BYTE information) const{
		// determines the order of given Files by the specified Information
		if (BSDOS->currentDir==ZX_DIR_ROOT){
			// root Directory
			const CDirsSector::PCSlot s1=(CDirsSector::PCSlot)file1, s2=(CDirsSector::PCSlot)file2;
			const PCDirectoryEntry de1=BSDOS->dirsSector.TryGetDirectoryEntry(s1), de2=BSDOS->dirsSector.TryGetDirectoryEntry(s2);
			switch (information){
				case INFORMATION_DIR_NUMBER:
					return s1-s2;
				case INFORMATION_NAME:
					return	::strncmp(
								de1!=nullptr ? de1->dir.name : BSDOS_DIR_CORRUPTED,
								de2!=nullptr ? de2->dir.name : BSDOS_DIR_CORRUPTED,
								ZX_TAPE_FILE_NAME_LENGTH_MAX
							);
				case INFORMATION_CHECKSUM:
					return s1->nameChecksum-s2->nameChecksum;
				case INFORMATION_CREATED:
					return	( de1!=nullptr ? de1->dateTimeCreated : 0 )
							-
							( de2!=nullptr ? de2->dateTimeCreated : 0 );
				case INFORMATION_COMMENT:
					return	::strncmp(
								de1!=nullptr ? de1->dir.comment : _T(""),
								de2!=nullptr ? de2->dir.comment : _T(""),
								sizeof(de1->dir.comment)
							);
			}
		}else{
			// File
			const PCDirectoryEntry de1=(PCDirectoryEntry)file1, de2=(PCDirectoryEntry)file2;
			switch (information){
				case INFORMATION_TYPE:
					return	( de1->fileHasStdHeader ? de1->file.stdHeader.type : TZxRom::TFileType::HEADERLESS )
							-
							( de2->fileHasStdHeader ? de2->file.stdHeader.type : TZxRom::TFileType::HEADERLESS );
				case INFORMATION_NAME:
					return	::strncmp(
								de1->fileHasStdHeader ? de1->file.stdHeader.name : _T(""),
								de2->fileHasStdHeader ? de2->file.stdHeader.name : _T(""),
								ZX_TAPE_FILE_NAME_LENGTH_MAX
							);
				case INFORMATION_SIZE:
					return de1->file.dataLength-de2->file.dataLength;
				case INFORMATION_STD_PARAM_1:
					return	( de1->fileHasStdHeader ? de1->file.stdHeader.params.param1 : TStdParameters::Default.param1 )
							-
							( de2->fileHasStdHeader ? de2->file.stdHeader.params.param1 : TStdParameters::Default.param1 );
				case INFORMATION_STD_PARAM_2:
					return	( de1->fileHasStdHeader ? de1->file.stdHeader.params.param2 : TStdParameters::Default.param2 )
							-
							( de2->fileHasStdHeader ? de2->file.stdHeader.params.param2 : TStdParameters::Default.param2 );
				case INFORMATION_CREATED:
					return de1->dateTimeCreated-de2->dateTimeCreated;
				case INFORMATION_FLAG:
					return de1->file.dataFlag-de2->file.dataFlag;
			}
		}
		return 0;
	}

	void WINAPI CBSDOS308::CBsdos308FileManagerView::__onFirstDirectoryEntryChanged__(PropGrid::PCustomParam slot){
		const CBSDOS308 *const bsdos=(CBSDOS308 *)CDos::GetFocused();
		bsdos->dirsSector.MarkDirectoryEntryAsDirty( (CDirsSector::PCSlot)slot );
	}

	void WINAPI CBSDOS308::CBsdos308FileManagerView::__onSubdirectoryNameChanged__(PropGrid::PCustomParam slot){
		const CBSDOS308 *const bsdos=(CBSDOS308 *)CDos::GetFocused();
		const CDirsSector::PSlot pSlot=(CDirsSector::PSlot)slot;
		pSlot->nameChecksum=bsdos->dirsSector.TryGetDirectoryEntry(pSlot)->GetDirNameChecksum();
		bsdos->dirsSector.MarkAsDirty();
		__onFirstDirectoryEntryChanged__(slot);
	}

	bool WINAPI CBSDOS308::CBsdos308FileManagerView::__fileTypeModified__(PVOID file,PropGrid::Enum::UValue newType){
		// changes the Type of File
		const PDos dos=CDos::GetFocused();
		const CSpectrumBaseFileManagerView *const pZxFileManager=(CSpectrumBaseFileManagerView *)dos->pFileManager;
		const PDirectoryEntry de=(PDirectoryEntry)file;
		if (de->fileHasStdHeader)
			// File with Header
			switch ((TZxRom::TFileType)newType.charValue){
				case TZxRom::HEADERLESS:
					de->fileHasStdHeader=!Utils::QuestionYesNo(_T("Sure to dispose the header?"),MB_DEFBUTTON2);
					break;
				default:
					de->file.stdHeader.type=(TZxRom::TFileType)newType.charValue;
					break;
			}
		else
			// Headerless File or Fragment
			switch ((TZxRom::TFileType)newType.charValue){
				case TZxRom::HEADERLESS:
					de->fileHasStdHeader=false;
					break;
				default:{
					de->fileHasStdHeader=true;
					de->file.stdHeader.length=std::min<DWORD>( de->file.dataLength, (WORD)-1 );
					de->file.stdHeader.params=TStdParameters::Default;
					de->file.stdHeader.SetName(_T("Unnamed"));
					de->file.stdHeader.type=(TZxRom::TFileType)newType.charValue;
					break;
				}
			}
		dos->MarkDirectorySectorAsDirty(de);
		return true;
	}

	CFileManagerView::PEditorBase CBSDOS308::CBsdos308FileManagerView::CreateFileInformationEditor(PFile file,BYTE information) const{
		// creates and returns Editor of File's selected Information; returns Null if Information cannot be edited
		if (BSDOS->currentDir==ZX_DIR_ROOT){
			// root Directory
			const CDirsSector::PSlot slot=(CDirsSector::PSlot)file;
			if (const PDirectoryEntry de=BSDOS->dirsSector.TryGetDirectoryEntry(slot))
				switch (information){
					case INFORMATION_NAME:
						return varLengthCommandLineEditor.CreateForFileName( slot, ZX_TAPE_FILE_NAME_LENGTH_MAX, ' ', __onSubdirectoryNameChanged__ );
					case INFORMATION_CHECKSUM:
						return integerEditor.Create( slot, &slot->nameChecksum );
					case INFORMATION_CREATED:
						return dateTimeEditor.Create( slot, &de->dateTimeCreated );
					case INFORMATION_COMMENT:
						return	varLengthCommandLineEditor.Create( slot, de->dir.comment, sizeof(de->dir.comment), ' ' );
				}
		}else{
			// File
			const PDirectoryEntry de=(PDirectoryEntry)file;
			switch (information){
				case INFORMATION_TYPE:
					return stdTapeHeaderTypeEditor.Create(	de, de->file.stdHeader.type,
															CStdTapeHeaderBlockTypeEditor::STD_AND_HEADERLESS,
															__fileTypeModified__
														);
				case INFORMATION_NAME:
					return	de->fileHasStdHeader
							? varLengthCommandLineEditor.CreateForFileName( de, ZX_TAPE_FILE_NAME_LENGTH_MAX, ' ' )
							: nullptr;
				case INFORMATION_SIZE_REPORTED:
					return	de->fileHasStdHeader
							? integerEditor.Create( file, &de->file.stdHeader.length )
							: nullptr;
				case INFORMATION_STD_PARAM_1:
					return	de->fileHasStdHeader
							? integerEditor.Create( de, &de->file.stdHeader.params.param1 )
							: nullptr;
				case INFORMATION_STD_PARAM_2:
					return	de->fileHasStdHeader
							? integerEditor.Create( de, &de->file.stdHeader.params.param2 )
							: nullptr;
				case INFORMATION_FLAG:
					return integerEditor.Create( de, &de->file.dataFlag );
				case INFORMATION_CREATED:
					return dateTimeEditor.Create( de, &de->dateTimeCreated );
			}
		}
		return nullptr;
	}
