#include "stdafx.h"
#include "MDOS2.h"

	#define INI_MSG_GKFM	_T("gkfmview")

	#define INFORMATION_COUNT		6
	#define INFORMATION_EXTENSION	0 /* column to sort by */
	#define INFORMATION_NAME		1 /* column to sort by */
	#define INFORMATION_SIZE		2 /* column to sort by */
	#define INFORMATION_ATTRIBUTES	3 /* column to sort by */
	#define INFORMATION_PARAM_1		4 /* column to sort by */
	#define INFORMATION_PARAM_2		5 /* column to sort by */

	const CFileManagerView::TFileInfo CMDOS2::CMdos2FileManagerView::InformationList[INFORMATION_COUNT]={
		{ _T("Extension"),	70,		TFileInfo::AlignRight },
		{ _T("Name"),		180,	TFileInfo::AlignLeft|TFileInfo::FileName },
		{ _T("Size"),		60,		TFileInfo::AlignRight },
		{ _T("Attributes"), 80,		TFileInfo::AlignRight },
		{ ZX_PARAMETER_1,	90,		TFileInfo::AlignRight },
		{ ZX_PARAMETER_2,	80,		TFileInfo::AlignRight }
	};

	CMDOS2::CMdos2FileManagerView::CMdos2FileManagerView(PMDOS2 mdos)
		// ctor
		: CSpectrumFileManagerView( mdos, mdos->zxRom, BIG_ICONS|REPORT, LVS_REPORT, INFORMATION_COUNT, InformationList, MDOS2_FILE_NAME_LENGTH_MAX ) {
	}








	#define DOS	tab.dos

	#define GKFM_ICON_BYTES_COUNT	108

	#define GKFM_ICON_DEFAULT	0
	#define GKFM_ICON_SNAPSHOT	1

	void CMDOS2::CMdos2FileManagerView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		// - refreshing the indication of MDOS Version
		( (CMainWindow *)app.m_pMainWnd )->statusBar.SetPaneText( 1, DOS->sideMap[1]==TVersion::VERSION_2?_T("MDOS 2.0"):_T("MDOS 1.0") );		
		// - refreshing the appearance
		if (displayMode==LVS_ICON){
			// GK's File Manager
			// . base (populating the FileManager with Files)
			const CClientDC dc(this);
			const HIMAGELIST icons=TBootSector::UReserved1::TGKFileManager::__getListOfDefaultIcons__(dc);
			CSpectrumFileManagerView::OnUpdate( pSender, LVSIL_NORMAL, (CObject *)icons );
			// . assigning Icons to individual Files
			CListCtrl &lv=GetListCtrl();
			LVITEM lvi={ LVIF_PARAM|LVIF_IMAGE, lv.GetItemCount() };
			while (lvi.iItem--){
				lv.GetItem(&lvi);
				switch ( ((PCDirectoryEntry)lvi.lParam)->extension ){
					case TDirectoryEntry::SNAPSHOT:
						// snapshot
						lvi.iImage=GKFM_ICON_SNAPSHOT;
						lv.SetItem(&lvi);
						break;
					case TDirectoryEntry::PROGRAM:{
						// program
						const LPCSTR name=((PDirectoryEntry)lvi.lParam)->name;
						for( TMdos2DirectoryTraversal dt((PMDOS2)DOS); dt.__existsNextEntry__(); )
							if (dt.entryType==TDirectoryTraversal::FILE){
								const PCDirectoryEntry de=(PCDirectoryEntry)dt.entry;
								if (de->extension==TDirectoryEntry::BLOCK // "Bytes" File
									&&
									de->attributes&(TDirectoryEntry::HIDDEN|TDirectoryEntry::SYSTEM) // the File is Hidden and System
									&&
									!strncmp(name,de->name,MDOS2_FILE_NAME_LENGTH_MAX) // "Bytes" File name equal to Program name
									&&
									DOS->GetFileOfficialSize(de)==GKFM_ICON_BYTES_COUNT // correct size
								){
									// found a File with icon intended for particular Program
									lvi.iImage=TBootSector::UReserved1::TGKFileManager::__addIconToList__( icons, ((PMDOS2)DOS)->__getHealthyLogicalSectorData__(de->firstLogicalSector), dc );
									lv.SetItem(&lvi);
									break;
								}
							}
						break;
					}
				}
			}
			CMDOS2::__informationWithCheckableShowNoMore__(_T("This view imitates what the files would look like in George K's File Manager.\n\nAlthough possible, it's recommended to NOT edit the file names here and use the Report view instead.\nReason - not all characters can be properly shown and/or typed in!"),INI_MSG_GKFM);
		}else
			// Report view
			CSpectrumFileManagerView::OnUpdate(pSender,lHint,pHint); // base (populating the FileManager with Files)
	}
	
	LRESULT CMDOS2::CMdos2FileManagerView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_CREATE:{
				static const UINT Indicators[]={ ID_SEPARATOR, ID_SEPARATOR };
				CStatusBar &rStatusBar=( (CMainWindow *)app.m_pMainWnd )->statusBar;
					rStatusBar.SetIndicators(Indicators,2);
					rStatusBar.SetPaneInfo(1,ID_SEPARATOR,SBPS_NORMAL,60);
				break;
			}
		}
		return CSpectrumFileManagerView::WindowProc(msg,wParam,lParam);
	}

	void CMDOS2::CMdos2FileManagerView::DrawReportModeCell(PCFileInfo pFileInfo,LPDRAWITEMSTRUCT pdis) const{
		// draws Information on File
		RECT &r=pdis->rcItem;
		const HDC dc=pdis->hDC;
		const PCDirectoryEntry de=(PCDirectoryEntry)pdis->itemData;
		// . color distinction of Files based on their Extension
		if (!pdis->itemState&ODS_SELECTED)
			switch (de->extension){
				case TDirectoryEntry::PROGRAM		: ::SetTextColor(dc,FILE_MANAGER_COLOR_EXECUTABLE); break;
				case TDirectoryEntry::CHAR_ARRAY	: ::SetTextColor(dc,0xff00ff); break;
				case TDirectoryEntry::NUMBER_ARRAY	: ::SetTextColor(dc,0xff00); break;
				//case TDirectoryEntry::BLOCK		: break;
				case TDirectoryEntry::SNAPSHOT		: ::SetTextColor(dc,FILE_MANAGER_COLOR_EXECUTABLE); break;
				case TDirectoryEntry::SEQUENTIAL	: ::SetTextColor(dc,0x999999); break;
			}
		// . drawing Information
		switch (pFileInfo-InformationList){
			case INFORMATION_EXTENSION:
				// File Extension
				r.right-=5;
				singleCharExtEditor.DrawReportModeCell( de->extension, pdis );
				break;
			case INFORMATION_NAME:
				// File Name
				varLengthFileNameEditor.DrawReportModeCell( de->name, MDOS2_FILE_NAME_LENGTH_MAX, pdis );
				break;
			case INFORMATION_SIZE:
				// File Size
				integerEditor.DrawReportModeCell( MAKELONG(de->lengthLow,de->lengthHigh), pdis );
				break;
			case INFORMATION_ATTRIBUTES:{
				// File Attributes
				TCHAR buf[16];
				::DrawText( dc, de->__attributes2text__(buf,true),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
				break;
			}
			case INFORMATION_PARAM_1:
				// start address / Basic start line
				integerEditor.DrawReportModeCell( de->params.param1, pdis );
				break;
			case INFORMATION_PARAM_2:
				// length of Basic Program without variables
				integerEditor.DrawReportModeCell( de->params.param2, pdis );
				break;
		}
	}

	int CMDOS2::CMdos2FileManagerView::CompareFiles(PCFile file1,PCFile file2,BYTE information) const{
		// determines the order of given Files by the specified Information
		const PCDirectoryEntry f1=(PCDirectoryEntry)file1, f2=(PCDirectoryEntry)file2;
		switch (information){
			case INFORMATION_EXTENSION:
				return *(LPCSTR)f1-*(LPCSTR)f2;
			case INFORMATION_NAME:
				return ::strncmp(f1->name,f2->name,MDOS2_FILE_NAME_LENGTH_MAX);
			case INFORMATION_SIZE:
				return DOS->GetFileOfficialSize(f1)-DOS->GetFileOfficialSize(f2);
			case INFORMATION_ATTRIBUTES:
				return f1->attributes-f2->attributes;
			case INFORMATION_PARAM_1:
				return f1->params.param1-f2->params.param1;
			case INFORMATION_PARAM_2:
				return f1->params.param2-f2->params.param2;
		}
		return 0;
	}

	CFileManagerView::PEditorBase CMDOS2::CMdos2FileManagerView::CreateFileInformationEditor(CDos::PFile file,BYTE infoId) const{
		// creates and returns Editor of File's selected Information; returns Null if Information cannot be edited
		const PDirectoryEntry de=(PDirectoryEntry)file;
		switch (infoId){
			case INFORMATION_EXTENSION:
				return singleCharExtEditor.Create(de);
			case INFORMATION_NAME:
				return varLengthFileNameEditor.Create( de, MDOS2_FILE_NAME_LENGTH_MAX, '\0' );
			case INFORMATION_ATTRIBUTES:
				return __createStdEditorWithEllipsis__( de, __editFileAttributes__ );
			case INFORMATION_PARAM_1:
				return integerEditor.Create( de, &de->params.param1, __markDirectorySectorAsDirty__ );
			case INFORMATION_PARAM_2:
				return integerEditor.Create( de, &de->params.param2, __markDirectorySectorAsDirty__ );
			default:
				return nullptr;
		}
	}












	bool WINAPI CMDOS2::CMdos2FileManagerView::__editFileAttributes__(PVOID file,PVOID,short){
		// True <=> modified File Attributes confirmed, otherwise False
		return ((PDirectoryEntry)file)->__editAttributesViaDialog__();
	}
