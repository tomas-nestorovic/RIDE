#include "stdafx.h"

	CFillEmptySpaceDialog::CFillEmptySpaceDialog(const CDos *dos)
		// ctor
		// - base
		: CDialog(IDR_DOS_FILL_EMPTY_SPACE)
		// - initialization
		, dos(dos)
		, dosProps(dos->properties)
		, sectorFillerByte(dosProps->sectorFillerByte) , directoryFillerByte(dosProps->directoryFillerByte)
		, nOptionsChecked(0)
		, fillEmptySectors(BST_UNCHECKED)
		, fillFileEndings(BST_UNCHECKED) , fillSubdirectoryFileEndings(BST_UNCHECKED)
		, fillEmptyDirectoryEntries(BST_UNCHECKED) , fillEmptySubdirectoryEntries(BST_UNCHECKED) {
	}

	BEGIN_MESSAGE_MAP(CFillEmptySpaceDialog,CDialog)
		ON_COMMAND_RANGE(ID_FILE,ID_DIRECTORY,__enableOkButton__)
		ON_BN_CLICKED(ID_DEFAULT1,__setDefaultFillerByteForGeneralSectors__)
		ON_BN_CLICKED(ID_DEFAULT2,__setDefaultFillerByteForDirectorySectors__)
	END_MESSAGE_MAP()










	void CFillEmptySpaceDialog::DoDataExchange(CDataExchange *pDX){
		// exchange of data from and to controls
		DDX_Check( pDX, ID_SECTOR,		fillEmptySectors );
		DDX_Check( pDX, ID_FILE,		fillFileEndings );
		DDX_Check( pDX, ID_RECURRENCY,	fillSubdirectoryFileEndings );
		DDX_Check( pDX, ID_DIRECTORY,	fillEmptyDirectoryEntries );
		DDX_Check( pDX, ID_SUBDIRECTORY,fillEmptySubdirectoryEntries );
		DDX_Text( pDX, ID_NUMBER, sectorFillerByte );
		DDX_Text( pDX, ID_NUMBER2, directoryFillerByte );
	}

	afx_msg void CFillEmptySpaceDialog::__enableOkButton__(UINT id){
		// projecting feasibility into UI
		nOptionsChecked+=-1+2*(Button_GetState(GetDlgItem(id)->m_hWnd)&BST_CHECKED);
		GetDlgItem(ID_RECURRENCY)->EnableWindow(
			(Button_GetState(GetDlgItem(ID_FILE)->m_hWnd)&BST_CHECKED) && dos->pFileManager->pDirectoryStructureManagement!=NULL
		);
		GetDlgItem(ID_SUBDIRECTORY)->EnableWindow(
			(Button_GetState(GetDlgItem(ID_DIRECTORY)->m_hWnd)&BST_CHECKED) && dos->pFileManager->pDirectoryStructureManagement!=NULL
		);
		GetDlgItem(IDOK)->EnableWindow( nOptionsChecked );
	}

	afx_msg void CFillEmptySpaceDialog::__setDefaultFillerByteForGeneralSectors__(){
		// adopts default SectorFillerByte from DOS properties
		SetDlgItemInt( ID_NUMBER, dosProps->sectorFillerByte, FALSE );
	}
	afx_msg void CFillEmptySpaceDialog::__setDefaultFillerByteForDirectorySectors__(){
		// adopts default DirectoryFillerByte from DOS properties
		SetDlgItemInt( ID_NUMBER2, dosProps->directoryFillerByte, FALSE );
	}
