#include "stdafx.h"

	CFillEmptySpaceDialog::CFillEmptySpaceDialog(const CDos *dos)
		// ctor
		// - base
		: Utils::CRideDialog(IDR_DOS_FILL_EMPTY_SPACE)
		// - initialization
		, dos(dos)
		, dosProps(dos->properties)
		, sectorFillerByte(dosProps->sectorFillerByte) , directoryFillerByte(dosProps->directoryFillerByte)
		, nOptionsChecked(0)
		, fillEmptySectors(false)
		, fillFileEndings(false) , fillSubdirectoryFileEndings(false)
		, fillEmptyDirectoryEntries(false) , fillEmptySubdirectoryEntries(false) {
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
		nOptionsChecked+=-1+2*IsDlgItemChecked(id);
		EnableDlgItem(  ID_RECURRENCY,  IsDlgItemChecked(ID_FILE) && dos->pFileManager->pDirectoryStructureManagement!=nullptr  );
		EnableDlgItem( ID_SUBDIRECTORY,  IsDlgItemChecked(ID_DIRECTORY) && dos->pFileManager->pDirectoryStructureManagement!=nullptr  );
		EnableDlgItem( IDOK, nOptionsChecked>0 );
	}

	afx_msg void CFillEmptySpaceDialog::__setDefaultFillerByteForGeneralSectors__(){
		// adopts default SectorFillerByte from DOS properties
		SetDlgItemInt( ID_NUMBER, dosProps->sectorFillerByte, FALSE );
	}
	afx_msg void CFillEmptySpaceDialog::__setDefaultFillerByteForDirectorySectors__(){
		// adopts default DirectoryFillerByte from DOS properties
		SetDlgItemInt( ID_NUMBER2, dosProps->directoryFillerByte, FALSE );
	}
