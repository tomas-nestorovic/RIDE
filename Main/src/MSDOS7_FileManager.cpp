#include "stdafx.h"
#include "MSDOS7.h"

	#define INFORMATION_COUNT		6
	#define INFORMATION_NAME_A_EXT	0 /* column to sort by */
	#define INFORMATION_SIZE		1 /* column to sort by */
	#define INFORMATION_ATTRIBUTES	2 /* column to sort by */
	#define INFORMATION_CREATED		3 /* column to sort by */
	#define INFORMATION_READ		4 /* column to sort by */
	#define INFORMATION_MODIFIED	5 /* column to sort by */

	const CFileManagerView::TFileInfo CMSDOS7::CMsdos7FileManagerView::InformationList[INFORMATION_COUNT]={
		{ _T("Name"),		250,	TFileInfo::AlignLeft|TFileInfo::FileName },
		{ _T("Size"),		70,		TFileInfo::AlignRight },
		{ _T("Attributes"), 80,		TFileInfo::AlignRight },
		{ _T("Created"),	190,	TFileInfo::AlignRight },
		{ _T("Last read"),	110,	TFileInfo::AlignRight },
		{ _T("Last modified"),190,	TFileInfo::AlignRight }
	};

	const CFileManagerView::TDirectoryStructureManagement CMSDOS7::CMsdos7FileManagerView::dirManagement={
		(CDos::TFnCreateSubdirectory)&CMSDOS7::CreateSubdirectory,
		(CDos::TFnChangeCurrentDirectory)&CMSDOS7::SwitchToDirectory,
		(CDos::TFnMoveFileToCurrDir)&CMSDOS7::MoveFileToCurrDir
	};

	CMSDOS7::CMsdos7FileManagerView::CMsdos7FileManagerView(PMSDOS7 msdos)
		// ctor
		// - base
		: CFileManagerView( msdos, REPORT, LVS_REPORT, font, 3, INFORMATION_COUNT, InformationList, &dirManagement )
		// - loading libraries
		, hShell32(::LoadLibrary(DLL_SHELL32))
		// - creating presentation Font
		, font(FONT_LUCIDA_CONSOLE,108,false,true,58)
		// - initialization of editors
		//nop
		{
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
		4	,nullptr, // folder closed
		5	,nullptr, // folder open
		1	,nullptr, // file general
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
		CStatusBar &rStatusBar=( (CMainWindow *)app.m_pMainWnd )->statusBar;
		if (rStatusBar.m_hWnd) // may not exist if the app is closing
			rStatusBar.SetPaneText( 1, buf );
		// - base
		__super::OnUpdate(pSender,lHint,pHint);
	}

	LRESULT CMSDOS7::CMsdos7FileManagerView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_CREATE:{
				// window created
				// . reinitializing the StatusBar
				static const UINT Indicators[]={ ID_SEPARATOR, ID_SEPARATOR };
				CStatusBar &rStatusBar=( (CMainWindow *)app.m_pMainWnd )->statusBar;
				if (rStatusBar.m_hWnd){ // may not exist if the app is closing
					rStatusBar.SetIndicators(Indicators,2);
					rStatusBar.SetPaneInfo(1,ID_SEPARATOR,SBPS_NORMAL,40);
				}
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
		return __super::WindowProc(msg,wParam,lParam);
	}

	HICON CMSDOS7::CMsdos7FileManagerView::__getIcon__(PCDirectoryEntry de) const{
		// determines and returns the Icon based on given File's type and extensions
		if (DOS->IsDirectory(de))
			return icons[ICON_FOLDER_OPEN];
		else{
			CPathString ext;
			DOS->GetFileNameOrExt(de,nullptr,&ext);
			if (ext.GetLength()){
				ext.LowerCase()+=',';
				for( BYTE n=MSDOS7_FILE_ICONS_COUNT; --n>ICON_FILE_GENERAL; )
					if (::strstr(ICON_INFOS[n].extensions,ext))
						return icons[n];
			}
			return icons[ICON_FILE_GENERAL];
		}
	}

	#define EXTENSION_EXE	0x657865
	#define EXTENSION_BAT	0x746162
	#define EXTENSION_COM	0x6d6f63

	#define ATTRIBUTES_COUNT	6

	void CMSDOS7::CMsdos7FileManagerView::DrawReportModeCell(PCFileInfo pFileInfo,LPDRAWITEMSTRUCT pdis) const{
		// draws Information on File
		RECT &r=pdis->rcItem;
		const HDC dc=pdis->hDC;
		const PCDirectoryEntry de=(PCDirectoryEntry)pdis->itemData;
		// . color distinction of Files based on their Extension; commented out as it doesn't look well
/*		if ((pdis->itemState&ODS_SELECTED)==0){
			DWORD extension=*(PDWORD)de->shortNameEntry.extension & 0xffffff;
			::CharLower((PCHAR)&extension);
			switch (extension){
				case EXTENSION_EXE: ::SetTextColor(dc,FILE_MANAGER_COLOR_EXECUTABLE); break;
				case EXTENSION_BAT: ::SetTextColor(dc,0xff00ff); break;
				case EXTENSION_COM: ::SetTextColor(dc,0xff00); break;
				//default: break;
			}
		}*/
		// . drawing Information
		switch (pFileInfo-InformationList){
			case INFORMATION_NAME_A_EXT:{
				// icon, Name a Extension
				const auto &dpiScaleFactor=Utils::LogicalUnitScaleFactor;
				::DrawIconEx( dc, r.left,r.top, __getIcon__(de), 16*dpiScaleFactor,16*dpiScaleFactor, 0, nullptr, DI_NORMAL|DI_COMPAT );
				r.left+=20*dpiScaleFactor;
				::DrawText( dc, DOS->GetFilePresentationNameAndExt(de),-1, &r, DT_SINGLELINE|DT_VCENTER );
				break;
			}
			case INFORMATION_SIZE:
				// File Size
				integerEditor.DrawReportModeCell( DOS->GetFileOfficialSize(de), pdis );
				break;
			case INFORMATION_ATTRIBUTES:{
				// Attributes
				BYTE attr=de->shortNameEntry.attributes;
				TCHAR buf[8], *t=::lstrcpy(buf,_T("ADVSHR"));
				for( BYTE a=ATTRIBUTES_COUNT; a--; attr<<=1,t++ )
					if (!(attr&32)) *t='-';
				::DrawText( dc, buf,-1, &r, DT_SINGLELINE|DT_VCENTER|DT_RIGHT );
				break;
			}
			case INFORMATION_CREATED:
				// date/time created
				dateTimeEditor.DrawReportModeCell( de->shortNameEntry.timeAndDateCreated, pdis );
				break;
			case INFORMATION_READ:
				// date last read
				dateTimeEditor.DrawReportModeCell( de->shortNameEntry.dateLastAccessed, pdis );
				break;
			case INFORMATION_MODIFIED:
				// date/time last modified
				dateTimeEditor.DrawReportModeCell( de->shortNameEntry.timeAndDateLastModified, pdis );
				break;
		}
	}

	int CMSDOS7::CMsdos7FileManagerView::CompareFiles(PCFile file1,PCFile file2,BYTE information) const{
		// determines the order of given Files by the specified Information
		const PCDirectoryEntry f1=(PCDirectoryEntry)file1, f2=(PCDirectoryEntry)file2;
		switch (information){
			case INFORMATION_NAME_A_EXT:
				if (const int d=(f1->shortNameEntry.attributes&FILE_ATTRIBUTE_DIRECTORY)-(f2->shortNameEntry.attributes&FILE_ATTRIBUTE_DIRECTORY))
					return -d; // Directories first
				else
					return ::lstrcmpi( DOS->GetFilePresentationNameAndExt(f1), DOS->GetFilePresentationNameAndExt(f2) );
			case INFORMATION_SIZE:
				return DOS->GetFileOfficialSize(f1)-DOS->GetFileOfficialSize(f2);
			case INFORMATION_ATTRIBUTES:
				return f1->shortNameEntry.attributes-f2->shortNameEntry.attributes;
			case INFORMATION_CREATED:
				return f1->shortNameEntry.timeAndDateCreated-f2->shortNameEntry.timeAndDateCreated;
			case INFORMATION_READ:
				return f1->shortNameEntry.dateLastAccessed-f2->shortNameEntry.dateLastAccessed;
			case INFORMATION_MODIFIED:
				return f1->shortNameEntry.timeAndDateLastModified-f2->shortNameEntry.timeAndDateLastModified;
		}
		return 0;
	}

	bool WINAPI CMSDOS7::CMsdos7FileManagerView::__onNameAndExtConfirmed__(PVOID file,LPCTSTR newNameAndExt,short nCharsOfNewNameAndExt){
		// True <=> NewNameAndExtension confirmed, otherwise False
		const PMSDOS7 msdos=(PMSDOS7)CDos::GetFocused();
		TCHAR tmpName[MAX_PATH];
		PTCHAR pExt=_tcsrchr( ::lstrcpy(tmpName,(LPCTSTR)newNameAndExt), '.' );
		if (pExt)
			*pExt++='\0';
		else
			pExt=_T("");
		CDos::PFile renamedFile;
		if (const TStdWinError err=msdos->ChangeFileNameAndExt(file,tmpName,pExt,renamedFile)){
			// at least two Files with the same Name+Extension combination exist
			Utils::Information(FILE_MANAGER_ERROR_RENAMING,err);
			return false;
		}else{
			// the new Name+Extension combination is unique
			if (file!=renamedFile)
				msdos->fileManager.__replaceFileDisplay__(file,renamedFile);
			return true;
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
					for( BYTE i=0; i<ATTRIBUTES_COUNT; attributes=(attributes<<1)|(BYTE)IsDlgButtonChecked(Controls[i++]) );
				else
					for( BYTE i=ATTRIBUTES_COUNT; i--; CheckDlgButton(Controls[i],attributes&1),attributes>>=1 );
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
			CDos::GetFocused()->MarkDirectorySectorAsDirty(de);
			return true;
		}else
			return false;
	}

	CFileManagerView::PEditorBase CMSDOS7::CMsdos7FileManagerView::CreateFileInformationEditor(CDos::PFile file,BYTE infoId) const{
		// creates and returns Editor of File's selected Information; returns Null if Information cannot be edited
		switch (infoId){
			case INFORMATION_NAME_A_EXT:
				return	CValueEditorBase::CreateStdEditor(
							file,
							const_cast<PTCHAR>((LPCTSTR)DOS->GetFilePresentationNameAndExt(file)),
							#ifdef UNICODE
								PropGrid::String::DefineDynamicLengthEditorW( __onNameAndExtConfirmed__ )
							#else
								PropGrid::String::DefineDynamicLengthEditorA( __onNameAndExtConfirmed__ )
							#endif
						);
			case INFORMATION_ATTRIBUTES:
				return CValueEditorBase::CreateStdEditorWithEllipsis( file, __editFileAttributes__ );
			case INFORMATION_CREATED:
				return dateTimeEditor.Create( file, &((PDirectoryEntry)file)->shortNameEntry.timeAndDateCreated );
			case INFORMATION_READ:
				return dateTimeEditor.Create( file, &((PDirectoryEntry)file)->shortNameEntry.dateLastAccessed );
			case INFORMATION_MODIFIED:
				return dateTimeEditor.Create( file, &((PDirectoryEntry)file)->shortNameEntry.timeAndDateLastModified );
			default:
				return nullptr;
		}
	}

	PTCHAR CMSDOS7::CMsdos7FileManagerView::GenerateExportNameAndExtOfNextFileCopy(CDos::PCFile file,bool shellCompliant,PTCHAR pOutBuffer) const{
		// returns the Buffer populated with the export name and extension of the next File's copy in current Directory; returns Null if no further name and extension can be generated
		// - getting copied File's Name and Extension
		CPathString fileName, fileExt;
		DOS->GetFileNameOrExt( file, &fileName, &fileExt );
		// - composing a unique Name+Extension combination for the next File copy
		for( BYTE copyNumber=0; ++copyNumber; ){
			// . composing the Name for the File copy
			CPathString fileCopyName;
			if (((CMSDOS7 *)DOS)->dontShowLongFileNames)
				// using only short "8.3" names
				( fileCopyName.Format(_T("%d~"),copyNumber)+=fileName ).TrimToLength(MSDOS7_FILE_NAME_LENGTH_MAX);
			else
				// using long names
				fileCopyName.Format(_T("Copy %d - "),copyNumber)+=fileName;
			// . finding if a file with given Name+Ext combination already exists
			if (!DOS->FindFileInCurrentDir(fileCopyName,fileExt,nullptr))
				// generated a unique Name for the next File copy - returning the final export name and extension
				return __getFileExportNameAndExt__( fileCopyName, fileExt, shellCompliant, pOutBuffer );
		}
		return nullptr; // the Name for the next File copy cannot be generated
	}










	const SYSTEMTIME CMSDOS7::TDateTime::Epoch[]={ {1980,1,2,1}, {2107,12,4,31} };

	static void WINAPI __pg_dateTime_draw__(PropGrid::PCustomParam,PropGrid::PCValue value,PropGrid::TSize size,PDRAWITEMSTRUCT pdis){
		if (size==sizeof(DWORD)) // both date and time
			CMSDOS7::TDateTime(*(const DWORD *)value).DrawInPropGrid( pdis->hDC, pdis->rcItem, false, DT_LEFT );
		else // only date
			CMSDOS7::TDateTime(*(PCWORD)value).DrawInPropGrid( pdis->hDC, pdis->rcItem, true, DT_LEFT );
	}

	static bool WINAPI __editFileDateTime__(PVOID file,PVOID value,short valueSize){
		// True <=> date&time editing confirmed, otherwise False
		CMSDOS7::TDateTime dt( valueSize==sizeof(DWORD) ? *(PDWORD)value : *(PWORD)value);
		if (dt.Edit( true, valueSize==sizeof(DWORD), CMSDOS7::TDateTime::Epoch )){
			if (valueSize==sizeof(DWORD))
				dt.ToDWord( (PDWORD)value );
			else{
				DWORD tmp;
				dt.ToDWord(&tmp);
				*(PWORD)value=HIWORD(tmp);
			}
			CDos::GetFocused()->MarkDirectorySectorAsDirty(file);
			return true;
		}else
			return false;
	}

	PropGrid::PCEditor CMSDOS7::TDateTime::DefinePropGridDateTimeEditor(PropGrid::TOnValueChanged onValueChanged){
		// creates and returns a PropertyGrid Editor with specified parameters
		return	PropGrid::Custom::DefineEditor(
					0, // default height
					sizeof(DWORD), // both date and time
					__pg_dateTime_draw__,
					nullptr,
					__editFileDateTime__,
					nullptr,
					onValueChanged
				);
	}

	CMSDOS7::TDateTime::TDateTime(WORD msdosDate)
		// ctor
		: TFileDateTime(TFileDateTime::None) {
		::DosDateTimeToFileTime( msdosDate, 0, this );
	}

	CMSDOS7::TDateTime::TDateTime(DWORD msdosTimeAndDate)
		// ctor
		: TFileDateTime(TFileDateTime::None) {
		::DosDateTimeToFileTime( HIWORD(msdosTimeAndDate), LOWORD(msdosTimeAndDate), this );
	}

	CMSDOS7::TDateTime::TDateTime(const FILETIME &r)
		// ctor
		: TFileDateTime(r) {
	}

	LPCTSTR CMSDOS7::TDateTime::ToString(PTCHAR buf) const{
		// populates the Buffer with this DateTime value and returns the buffer
		if (DateToString(buf)){ // is valid in MS-DOS format?
			TimeToString(  buf+::lstrlen( ::lstrcat(buf,_T(", ")) )  );
			return buf;
		}else
			return nullptr;
	}

	PTCHAR CMSDOS7::TDateTime::DateToString(PTCHAR buf) const{
		// populates the Buffer with this Date value and returns the buffer
		return	ToDWord(nullptr) // is valid in MS-DOS format?
				? __super::DateToString(buf)
				: nullptr;
	}

	bool CMSDOS7::TDateTime::ToDWord(PDWORD pOutResult) const{
		// True <=> successfully packed this DateTime to a DWORD whose higher and lower Words contains MS-DOS compliant date and time, respectively, otherwise False
		// - attempting to compose and return the result
		WORD msdosDate, msdosTime;
		if (::FileTimeToDosDateTime( this, &msdosDate, &msdosTime )){
			if (pOutResult) *pOutResult=MAKELONG(msdosTime,msdosDate);
			return true;
		}
		// - the SystemTime contains values that CANNOT be represented in MS-DOS format
		SYSTEMTIME st;
		::FileTimeToSystemTime( this, &st );
		if (pOutResult) *pOutResult=0; // 0 = begin of MS-DOS epoch
		return !st.wYear; // zeroth Year reserved for clearing date-time entries in FAT (or whereever else used), see the Edit method
	}

	void CMSDOS7::TDateTime::DrawInPropGrid(HDC dc,RECT rc,bool onlyDate,BYTE horizonalAlignment) const{
		// draws the MS-DOS file date&time information
		rc.left+=PROPGRID_CELL_MARGIN_LEFT;
		TCHAR buf[80];
		if (const LPCTSTR desc=onlyDate?DateToString(buf):ToString(buf)) // is valid in MS-DOS format?
			::DrawText(	dc, desc, -1, &rc,
						DT_SINGLELINE | horizonalAlignment | DT_VCENTER
					);
		else{
			const int color0=::SetTextColor( dc, 0xee );
				::DrawText(	dc, _T("N/A"), -1, &rc,
							DT_SINGLELINE | horizonalAlignment | DT_VCENTER
						);
			::SetTextColor(dc,color0);
		}
	}



	CFileManagerView::PEditorBase CMSDOS7::TDateTime::CEditor::Create(PFile file,PDWORD pMsdosTimeAndDate){
		// creates and returns an Editor of File DateTime stamp
		return CreateStdEditorWithEllipsis( file, pMsdosTimeAndDate, sizeof(DWORD), __editFileDateTime__ );
	}

	CFileManagerView::PEditorBase CMSDOS7::TDateTime::CEditor::Create(PFile file,PWORD pMsdosDate){
		// creates and returns an Editor of File Date stamp
		return CreateStdEditorWithEllipsis( file, pMsdosDate, sizeof(WORD), __editFileDateTime__ );
	}

	void CMSDOS7::TDateTime::CEditor::DrawReportModeCell(DWORD msdosTimeAndDate,LPDRAWITEMSTRUCT pdis,BYTE horizonalAlignment){
		// draws the MS-DOS File date&time information
		TDateTime(msdosTimeAndDate).DrawInPropGrid( pdis->hDC, pdis->rcItem, false, horizonalAlignment );
	}

	void CMSDOS7::TDateTime::CEditor::DrawReportModeCell(WORD msdosDate,LPDRAWITEMSTRUCT pdis,BYTE horizonalAlignment){
		// draws the MS-DOS File date information
		TDateTime(msdosDate).DrawInPropGrid( pdis->hDC, pdis->rcItem, true, horizonalAlignment );
	}
