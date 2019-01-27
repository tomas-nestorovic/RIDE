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
		{ NULL,				LVCFMT_LEFT,	8 }, // auxiliary column to indent the first information from left edge of window
		{ _T("Name"),		LVCFMT_LEFT,	180 },
		{ _T("Extension"),	LVCFMT_RIGHT,	70 },
		{ _T("Size"),		LVCFMT_RIGHT,	55 },
		{ _T("Sectors"),	LVCFMT_RIGHT,	55 },
		{ _T("First sector"),LVCFMT_RIGHT,	100 },
		{ ZX_PARAMETER_1,	LVCFMT_RIGHT,	90 },
		{ ZX_PARAMETER_2,	LVCFMT_RIGHT,	80 }
	};

	CTRDOS503::CTrdosFileManagerView::CTrdosFileManagerView(PTRDOS503 trdos)
		// ctor
		: CSpectrumFileManagerView( trdos, trdos->zxRom, REPORT, LVS_REPORT, INFORMATION_COUNT, InformationList ) {
	}









	#define DOS	tab.dos

	LRESULT CTRDOS503::CTrdosFileManagerView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_CREATE:{
				static const UINT Indicators[]={ ID_SEPARATOR, ID_SEPARATOR };
				CStatusBar &rStatusBar=( (CMainWindow *)app.m_pMainWnd )->statusBar;
					rStatusBar.SetIndicators(Indicators,2);
					rStatusBar.SetPaneInfo(1,ID_SEPARATOR,SBPS_NORMAL,72);
					rStatusBar.SetPaneText(1,DOS->properties->name);
				break;
			}
		}
		return CSpectrumFileManagerView::WindowProc(msg,wParam,lParam);
	}

	void CTRDOS503::CTrdosFileManagerView::DrawFileInfo(LPDRAWITEMSTRUCT pdis,const int *tabs) const{
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
		TCHAR bufT[MAX_PATH];
		// . COLUMN: <indent from left edge>
		r.left=*tabs++;
		// . COLUMN: Name
		r.right=*tabs++;
			zxRom.PrintAt( dc, TZxRom::ZxToAscii(de->name,TRDOS503_FILE_NAME_LENGTH_MAX,bufT), r, DT_SINGLELINE|DT_VCENTER );
		r.left=r.right;
		// . COLUMN: Extension
		r.right=*tabs++;
			zxRom.PrintAt( dc, TZxRom::ZxToAscii((LPCSTR)&de->extension,1,bufT), r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
		r.left=r.right;
		// . COLUMN: Size
		r.right=*tabs++;
			::DrawText( dc, _itot(de->__getOfficialFileSize__(NULL),bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
		r.left=r.right;
		// . COLUMN: # of Sectors
		r.right=*tabs++;
			::DrawText( dc, _itot(de->nSectors,bufT,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
		r.left=r.right;
		// . COLUMN: first Sector
		r.right=*tabs++;
			::wsprintf( bufT, _T("Tr%d/Sec%d"), de->firstTrack, de->firstSector );
			::DrawText( dc, bufT,-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
		r.left=r.right;
		// . COLUMN: start address / Basic start line
		r.right=*tabs++;
			WORD param;
			::DrawText(	dc,
						((PTRDOS503)DOS)->__getStdParameter1__(de,param) ? _itot(param,bufT,10) : _T("N/A"),
						-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT
					);
		r.left=r.right;
		// . COLUMN: length of Basic Program without variables
		r.right=*tabs++;
			::DrawText(	dc,
						((PTRDOS503)DOS)->__getStdParameter2__(de,param) ? _itot(param,bufT,10) : _T("N/A"),
						-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT
					);
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
				return f1->__getOfficialFileSize__(NULL)-f2->__getOfficialFileSize__(NULL);
			case INFORMATION_SECTORS_COUNT:
				return f1->nSectors-f2->nSectors;
			case INFORMATION_FIRST_SECTOR:
				return (f1->firstTrack-f2->firstTrack)*TRDOS503_TRACK_SECTORS_COUNT+f1->firstSector-f2->firstSector;
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
		return ((PTRDOS503)CDos::__getFocused__())->__setStdParameter1__((PDirectoryEntry)file,newWord);
	}
	bool WINAPI CTRDOS503::CTrdosFileManagerView::__onStdParam2Changed__(PVOID file,int newWord){
		return ((PTRDOS503)CDos::__getFocused__())->__setStdParameter2__((PDirectoryEntry)file,newWord);
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
					return stdParamEditor.Create( de, &stdParameter, __onStdParam1Changed__ );
				else
					return NULL;
			case INFORMATION_STD_PARAM_2:
				if (((PTRDOS503)DOS)->__getStdParameter2__(de,stdParameter))
					return stdParamEditor.Create( de, &stdParameter, __onStdParam2Changed__ );
				else
					return NULL;
			default:
				return NULL;
		}
	}
