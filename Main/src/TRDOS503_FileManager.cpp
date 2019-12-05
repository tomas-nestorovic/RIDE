#include "stdafx.h"
#include "TRDOS.h"

	#define INFORMATION_COUNT		8
	#define INFORMATION_NAME		1 /* column to sort by */
	#define INFORMATION_EXTENSION	2 /* column to sort by */
	#define INFORMATION_SIZE		3 /* column to sort by */
	#define INFORMATION_SECTORS_COUNT 4 /* column to sort by */
	#define INFORMATION_FIRST_SECTOR  5 /* column to sort by */
	#define INFORMATION_STD_PARAM_1	6 /* column to sort by */
	#define INFORMATION_STD_PARAM_2	7 /* column to sort by */

	const CFileManagerView::TFileInfo CTRDOS503::CTrdosFileManagerView::InformationList[INFORMATION_COUNT]={
		{ nullptr,			8,		TFileInfo::AlignLeft }, // auxiliary column to indent the first information from left edge of window
		{ _T("Name"),		180,	TFileInfo::AlignLeft|TFileInfo::FileName },
		{ _T("Extension"),	70,		TFileInfo::AlignRight },
		{ _T("Size"),		55,		TFileInfo::AlignRight },
		{ _T("Sectors"),	55,		TFileInfo::AlignRight },
		{ _T("First sector"),100,	TFileInfo::AlignRight },
		{ ZX_PARAMETER_1,	90,		TFileInfo::AlignRight },
		{ ZX_PARAMETER_2,	80,		TFileInfo::AlignRight }
	};

	CTRDOS503::CTrdosFileManagerView::CTrdosFileManagerView(PTRDOS503 trdos)
		// ctor
		: CSpectrumFileManagerView( trdos, trdos->zxRom, REPORT, LVS_REPORT, INFORMATION_COUNT, InformationList, TRDOS503_FILE_NAME_LENGTH_MAX ) {
	}









	#define DOS	tab.dos

	LRESULT CTRDOS503::CTrdosFileManagerView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_CREATE:{
				static const UINT Indicators[]={ ID_SEPARATOR, ID_SEPARATOR };
				CStatusBar &rStatusBar=( (CMainWindow *)app.m_pMainWnd )->statusBar;
				if (rStatusBar.m_hWnd){ // may not exist if the app is closing
					rStatusBar.SetIndicators(Indicators,2);
					rStatusBar.SetPaneInfo(1,ID_SEPARATOR,SBPS_NORMAL,72);
					rStatusBar.SetPaneText(1,DOS->properties->name);
				}
				break;
			}
		}
		return CSpectrumFileManagerView::WindowProc(msg,wParam,lParam);
	}

	void CTRDOS503::CTrdosFileManagerView::DrawReportModeCell(PCFileInfo pFileInfo,LPDRAWITEMSTRUCT pdis) const{
		// draws Information on File
		RECT r=pdis->rcItem;
		const HDC dc=pdis->hDC;
		const PCDirectoryEntry de=(PCDirectoryEntry)pdis->itemData;
		// . color distinction of Files based on their Extension
		if (!pdis->itemState&ODS_SELECTED)
			switch (de->extension){
				case TDirectoryEntry::BASIC_PRG	: ::SetTextColor(dc,FILE_MANAGER_COLOR_EXECUTABLE); break;
				case TDirectoryEntry::DATA_FIELD: ::SetTextColor(dc,0xff00ff); break;
				//case TDirectoryEntry::BLOCK	: break;
				case TDirectoryEntry::PRINT		: ::SetTextColor(dc,0x999999); break;
			}
		// . drawing Information
		TCHAR bufT[MAX_PATH];
		switch (pFileInfo-InformationList){
			case INFORMATION_NAME:
				// File Name
				varLengthFileNameEditor.DrawReportModeCell( de->name, TRDOS503_FILE_NAME_LENGTH_MAX, pdis );
				break;
			case INFORMATION_EXTENSION:
				// File Extension
				singleCharExtEditor.DrawReportModeCell( de->extension, pdis );
				break;
			case INFORMATION_SIZE:
				// File Size
				integerEditor.DrawReportModeCell( de->__getOfficialFileSize__(nullptr), pdis );
				break;
			case INFORMATION_SECTORS_COUNT:
				// # of File Sectors
				integerEditor.DrawReportModeCell( de->nSectors, pdis );
				break;
			case INFORMATION_FIRST_SECTOR:
				// first File Sector
				::wsprintf( bufT, _T("Tr%d/Sec%d"), de->first.track, de->first.sector );
				::DrawText( dc, bufT,-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
				break;
			case INFORMATION_STD_PARAM_1:{
				// start address / Basic start line
				WORD param;
				::DrawText(	dc,
							((PTRDOS503)DOS)->__getStdParameter1__(de,param) ? _itot(param,bufT,10) : _T("N/A"),
							-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT
						);
				break;
			}
			case INFORMATION_STD_PARAM_2:{
				// length of Basic Program without variables
				WORD param;
				::DrawText(	dc,
							((PTRDOS503)DOS)->__getStdParameter2__(de,param) ? _itot(param,bufT,10) : _T("N/A"),
							-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT
						);
				break;
			}
		}
	}

	int CTRDOS503::CTrdosFileManagerView::CompareFiles(PCFile file1,PCFile file2,BYTE information) const{
		// determines the order of given Files by the specified Information
		const PCDirectoryEntry f1=(PCDirectoryEntry)file1, f2=(PCDirectoryEntry)file2;
		switch (information){
			case INFORMATION_NAME:
				return ::strncmp(f1->name,f2->name,TRDOS503_FILE_NAME_LENGTH_MAX);
			case INFORMATION_EXTENSION:
				return f1->extension-f2->extension;
			case INFORMATION_SIZE:
				return f1->__getOfficialFileSize__(nullptr)-f2->__getOfficialFileSize__(nullptr);
			case INFORMATION_SECTORS_COUNT:
				return f1->nSectors-f2->nSectors;
			case INFORMATION_FIRST_SECTOR:
				return (f1->first.track-f2->first.track)*TRDOS503_TRACK_SECTORS_COUNT+f1->first.sector-f2->first.sector;
			case INFORMATION_STD_PARAM_1:{
				WORD w1=0,w2=0;
				((PTRDOS503)DOS)->__getStdParameter1__(f1,w1), ((PTRDOS503)DOS)->__getStdParameter1__(f2,w2);
				return w1-w2;
			}
			case INFORMATION_STD_PARAM_2:{
				WORD w1=0,w2=0;
				((PTRDOS503)DOS)->__getStdParameter2__(f1,w1), ((PTRDOS503)DOS)->__getStdParameter2__(f2,w2);
				return w1-w2;
			}
		}
		return 0;
	}

	bool WINAPI CTRDOS503::CTrdosFileManagerView::__onStdParam1Changed__(PVOID file,int newWord){
		return ((PTRDOS503)CDos::GetFocused())->__setStdParameter1__((PDirectoryEntry)file,newWord);
	}
	bool WINAPI CTRDOS503::CTrdosFileManagerView::__onStdParam2Changed__(PVOID file,int newWord){
		return ((PTRDOS503)CDos::GetFocused())->__setStdParameter2__((PDirectoryEntry)file,newWord);
	}
	CFileManagerView::PEditorBase CTRDOS503::CTrdosFileManagerView::CreateFileInformationEditor(PFile file,BYTE infoId) const{
		// creates and returns Editor of File's selected Information; returns Null if Information cannot be edited
		const PDirectoryEntry de=(PDirectoryEntry)file;
		switch (infoId){
			case INFORMATION_NAME:
				return varLengthFileNameEditor.Create( de, TRDOS503_FILE_NAME_LENGTH_MAX, ' ' );
			case INFORMATION_EXTENSION:
				return singleCharExtEditor.Create(de);
			case INFORMATION_STD_PARAM_1:
				if (((PTRDOS503)DOS)->__getStdParameter1__(de,stdParameter))
					return integerEditor.Create( de, &stdParameter, __onStdParam1Changed__ );
				else
					return nullptr;
			case INFORMATION_STD_PARAM_2:
				if (((PTRDOS503)DOS)->__getStdParameter2__(de,stdParameter))
					return integerEditor.Create( de, &stdParameter, __onStdParam2Changed__ );
				else
					return nullptr;
			default:
				return nullptr;
		}
	}
