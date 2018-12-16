#include "stdafx.h"
#include "MSDOS7.h"

	#define INFORMATION_COUNT		3
	#define INFORMATION_NAME_A_EXT	0 /* column to sort by */
	#define INFORMATION_SIZE		1 /* column to sort by */
	#define INFORMATION_ATTRIBUTES	2 /* column to sort by */

	const CFileManagerView::TFileInfo CMSDOS7::CMsdos7FileManagerView::InformationList[INFORMATION_COUNT]={
		{ _T("Name"),		LVCFMT_LEFT,	250 },
		{ _T("Size"),		LVCFMT_RIGHT,	70 },
		{ _T("Attributes"), LVCFMT_RIGHT,	80 }
	};

	const CFileManagerView::TDirectoryStructureManagement CMSDOS7::CMsdos7FileManagerView::dirManagement={
		(CDos::TFnGetCurrentDirectory)&CMSDOS7::__getCurrentDirectory__,
		(CDos::TFnGetCurrentDirectoryId)&CMSDOS7::__getCurrentDirectoryId__,
		(CDos::TFnCreateSubdirectory)&CMSDOS7::__createSubdirectory__,
		(CDos::TFnChangeCurrentDirectory)&CMSDOS7::__switchToDirectory__,
		(CDos::TFnMoveFileToCurrDir)&CMSDOS7::__moveFileToCurrDir__
	};

	CMSDOS7::CMsdos7FileManagerView::CMsdos7FileManagerView(PMSDOS7 msdos)
		// ctor
		// - base
		: CFileManagerView( msdos, REPORT, LVS_REPORT, font, 3, INFORMATION_COUNT, InformationList, INFORMATION_NAME_A_EXT, &dirManagement )
		// - loading libraries
		, hShell32(::LoadLibrary(DLL_SHELL32))
		// - creating presentation Font
		, font(FONT_LUCIDA_CONSOLE,108,false,true,58) {
	}

	CMSDOS7::CMsdos7FileManagerView::~CMsdos7FileManagerView(){
		// dtor
		::FreeLibrary(hShell32);
	}








	#define DOS	tab.dos

	#define ICON_FOLDER_OPEN	0
	#define ICON_FOLDER_CLOSED	1
	#define ICON_FILE_GENERAL	2

	#pragma pack(1)
	static const struct{
		WORD iconId;
		LPCTSTR extensions; // deliminated by comma
	} ICON_INFOS[MSDOS7_FILE_ICONS_COUNT]={
		4	,NULL, // folder closed
		5	,NULL, // folder open
		1	,NULL, // file general
		152	,_T("txt,"),
		2	,_T("rtf,doc,pdf,"),
		153	,_T("bat,"),
		239	,_T("bmp,jpg,jpeg,gif,png,"),
		225	,_T("mid,wav,mp3,"),
		3	,_T("com,exe,"),
		154	,_T("dll,vxd,"),
		151	,_T("ini,inf,")
	};

	void CMSDOS7::CMsdos7FileManagerView::OnUpdate(CView *pSender,LPARAM lHint,CObject *pHint){
		// request to refresh the display of content
		// - updating the FAT indication in StatusBar
		TCHAR buf[8];
		::wsprintf( buf, _T("FAT%d"), ((PMSDOS7)DOS)->fat.type*4 );
		( (CMainWindow *)app.m_pMainWnd )->statusBar.SetPaneText( 1, buf );
		// - base
		CFileManagerView::OnUpdate(pSender,lHint,pHint);
	}

	LRESULT CMSDOS7::CMsdos7FileManagerView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_CREATE:{
				// window created
				// . reinitializing the StatusBar
				static const UINT Indicators[]={ ID_SEPARATOR, ID_SEPARATOR };
				CStatusBar &rStatusBar=( (CMainWindow *)app.m_pMainWnd )->statusBar;
					rStatusBar.SetIndicators(Indicators,2);
					rStatusBar.SetPaneInfo(1,ID_SEPARATOR,SBPS_NORMAL,40);
				// . creating the Icons
				for( BYTE n=MSDOS7_FILE_ICONS_COUNT; n--; icons[n]=(HICON)::LoadImage( hShell32, (LPCTSTR)ICON_INFOS[n].iconId, IMAGE_ICON, 16,16, LR_DEFAULTCOLOR ) );
				break;
			}
			case WM_DESTROY:
				// window destroyed
				// . freeing the Icons
				for( BYTE n=MSDOS7_FILE_ICONS_COUNT; n; ::DestroyIcon(icons[--n]) );
				break;
		}
		return CFileManagerView::WindowProc(msg,wParam,lParam);
	}

	HICON CMSDOS7::CMsdos7FileManagerView::__getIcon__(PCDirectoryEntry de) const{
		// determines and returns the Icon based on given File's type and extensions
		if (DOS->IsDirectory(de))
			return icons[ICON_FOLDER_OPEN];
		else{
			TCHAR bufExt[MAX_PATH];
			DOS->GetFileNameAndExt(de,NULL,bufExt);
			if (*bufExt){
				::lstrcat( ::CharLower(bufExt), _T(",") );
				for( BYTE n=MSDOS7_FILE_ICONS_COUNT; --n>ICON_FILE_GENERAL; )
					if (::strstr(ICON_INFOS[n].extensions,bufExt))
						return icons[n];
			}
			return icons[ICON_FILE_GENERAL];
		}
	}

	#define EXTENSION_EXE	0x657865
	#define EXTENSION_BAT	0x746162
	#define EXTENSION_COM	0x6d6f63

	#define ATTRIBUTES_COUNT	6

	void CMSDOS7::CMsdos7FileManagerView::DrawFileInfo(LPDRAWITEMSTRUCT pdis,const int *tabs) const{
		// draws Information on File
		RECT r=pdis->rcItem;
		const HDC dc=pdis->hDC;
		const PCDirectoryEntry de=(PCDirectoryEntry)pdis->itemData;
		// . color distinction of Files based on their Extension; commented out as it doesn't look well
/*		if (!pdis->itemState&ODS_SELECTED){
			DWORD extension=*(PDWORD)de->shortNameEntry.extension & 0xffffff;
			::CharLower((PCHAR)&extension);
			switch (extension){
				case EXTENSION_EXE: ::SetTextColor(dc,FILE_MANAGER_COLOR_EXECUTABLE); break;
				case EXTENSION_BAT: ::SetTextColor(dc,0xff00ff); break;
				case EXTENSION_COM: ::SetTextColor(dc,0xff00); break;
				//default: break;
			}
		}*/
		// . COLUMN: icon, name a extension
		const float dpiScaleFactor=Utils::LogicalUnitScaleFactor;
		BYTE attr=de->shortNameEntry.attributes;
		::DrawIconEx( dc, r.left,r.top, __getIcon__(de), 16*dpiScaleFactor,16*dpiScaleFactor, 0, NULL, DI_NORMAL|DI_COMPAT );
		r.left+=20*dpiScaleFactor;
		TCHAR buf[MAX_PATH];
		r.right=*tabs++;
			::DrawText( dc, DOS->GetFileNameWithAppendedExt(de,buf),-1, &r, DT_SINGLELINE|DT_VCENTER );
		r.left=r.right;
		// . COLUMN: size
		r.right=*tabs++;
			::DrawText( dc, _itot(DOS->GetFileDataSize(de),buf,10),-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
		r.left=r.right;
		// . COLUMN: attributes
		r.right=*tabs++;
			PTCHAR t=::lstrcpy(buf,_T("ADVSHR"));
			for( BYTE a=ATTRIBUTES_COUNT; a--; attr<<=1,t++ )
				if (!(attr&32)) *t='-';
			::DrawText( dc, buf,-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
		r.left=r.right;
	}

	int CMSDOS7::CMsdos7FileManagerView::CompareFiles(PCFile file1,PCFile file2,BYTE information) const{
		// determines the order of given Files by the specified Information
		const PCDirectoryEntry f1=(PCDirectoryEntry)file1, f2=(PCDirectoryEntry)file2;
		switch (information){
			case INFORMATION_NAME_A_EXT:
				if (const int d=(f1->shortNameEntry.attributes&FILE_ATTRIBUTE_DIRECTORY)-(f2->shortNameEntry.attributes&FILE_ATTRIBUTE_DIRECTORY))
					return -d; // Directories first
				else{
					TCHAR n1[MAX_PATH],n2[MAX_PATH];
					return ::lstrcmpi( DOS->GetFileNameWithAppendedExt(f1,n1), DOS->GetFileNameWithAppendedExt(f2,n2) );
				}
			case INFORMATION_SIZE:
				return DOS->GetFileDataSize(f1)-DOS->GetFileDataSize(f2);
			case INFORMATION_ATTRIBUTES:
				return f1->shortNameEntry.attributes-f2->shortNameEntry.attributes;
		}
		return 0;
	}

	bool WINAPI CMSDOS7::CMsdos7FileManagerView::__onNameAndExtConfirmed__(PVOID file,LPCTSTR newNameAndExt,short nCharsOfNewNameAndExt){
		// True <=> NewNameAndExtension confirmed, otherwise False
		const PMSDOS7 msdos=(PMSDOS7)CDos::__getFocused__();
		TCHAR tmpName[MAX_PATH];
		PTCHAR pExt=_tcsrchr( ::lstrcpy(tmpName,(LPCTSTR)newNameAndExt), '.' );
		if (pExt)
			*pExt++='\0';
		else
			pExt=_T("");
		CDos::PFile renamedFile;
		const TStdWinError err=msdos->ChangeFileNameAndExt(file,tmpName,pExt,renamedFile);
		if (err==ERROR_SUCCESS){ // the new Name+Extension combination is unique
			if (file!=renamedFile)
				msdos->fileManager.__replaceFileDisplay__(file,renamedFile);
			return true;
		}else{	// at least two Files with the same Name+Extension combination exist
			Utils::Information(FILE_MANAGER_ERROR_RENAMING,err);
			return false;
		}
	}
	bool WINAPI CMSDOS7::CMsdos7FileManagerView::__editFileAttributes__(PVOID file,PVOID,short){
		// True <=> Attributes editing confirmed, otherwise False
		const PDirectoryEntry de=(PDirectoryEntry)file;
		// - defining the Dialog
		class CAttributesDialog sealed:public CDialog{
			void DoDataExchange(CDataExchange *pDX) override{
				static const WORD Controls[]={ ID_ARCHIVE, ID_DIRECTORY, ID_VOLUME, ID_SYSTEM, ID_HIDDEN, ID_READONLY };
				if (pDX->m_bSaveAndValidate)
					for( BYTE i=0; i<ATTRIBUTES_COUNT; attributes=(attributes<<1)|(BYTE)Button_GetCheck(::GetDlgItem(m_hWnd,Controls[i++])) );
				else
					for( BYTE i=ATTRIBUTES_COUNT; i--; Button_SetCheck(::GetDlgItem(m_hWnd,Controls[i]),attributes&1),attributes>>=1 );
			}
		public:
			BYTE attributes;
			CAttributesDialog(BYTE _attributes)
				: CDialog(IDR_MSDOS_FILE_ATTRIBUTES_EDITOR)
				, attributes(_attributes) {
			}
		} d(de->shortNameEntry.attributes);
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			de->shortNameEntry.attributes=d.attributes;
			((PMSDOS7)CDos::__getFocused__())->__markDirectorySectorAsDirty__(de);
			return true;
		}else
			return false;
	}
	CFileManagerView::PEditorBase CMSDOS7::CMsdos7FileManagerView::CreateFileInformationEditor(CDos::PFile file,BYTE infoId) const{
		// creates and returns Editor of File's selected Information; returns Null if Information cannot be edited
		switch (infoId){
			case INFORMATION_NAME_A_EXT:{
				TCHAR buf[MAX_PATH];
				return __createStdEditor__(	file,
											DOS->GetFileNameWithAppendedExt(file,buf), MAX_PATH,
											#ifdef UNICODE
												CPropGridCtrl::TString::DefineDynamicLengthEditorW(__onNameAndExtConfirmed__)
											#else
												CPropGridCtrl::TString::DefineDynamicLengthEditorA(__onNameAndExtConfirmed__)
											#endif
										);
			}
			case INFORMATION_ATTRIBUTES:
				return __createStdEditorWithEllipsis__( file, __editFileAttributes__ );
			default:
				return NULL;
		}
	}

	PTCHAR CMSDOS7::CMsdos7FileManagerView::GenerateExportNameAndExtOfNextFileCopy(CDos::PCFile file,bool shellCompliant,PTCHAR pOutBuffer) const{
		// returns the Buffer populated with the export name and extension of the next File's copy in current Directory; returns Null if no further name and extension can be generated
		for( BYTE copyNumber=0; ++copyNumber; ){
			// . composing the Name for the File copy
			TCHAR bufNameCopy[20+MAX_PATH], bufExt[MAX_PATH];
			if (((CMSDOS7 *)DOS)->dontShowLongFileNames){
				// using only short "8.3" names
				DOS->GetFileNameAndExt(	file,
										bufNameCopy+::wsprintf(bufNameCopy,_T("%d~"),copyNumber),
										bufExt
									);
				bufNameCopy[MSDOS7_FILE_NAME_LENGTH_MAX]='\0'; // trimming to maximum number of characters
			}else{
				// using long names
				DOS->GetFileNameAndExt(	file,
										bufNameCopy+::wsprintf(bufNameCopy,_T("Copy %d - "),copyNumber),
										bufExt
									);
				bufNameCopy[MAX_PATH]='\0'; // trimming to maximum number of characters
			}
			// . finding if a file with given name already exists
			if (!((CMSDOS7 *)DOS)->__findFile__(bufNameCopy,bufExt,NULL))
				// generated a unique Name for the next File copy - returning the final export name and extension
				return ((CMSDOS7 *)DOS)->__getFileExportNameAndExt__( bufNameCopy, bufExt, shellCompliant, pOutBuffer );
		}
		return NULL; // the Name for the next File copy cannot be generated
	}
