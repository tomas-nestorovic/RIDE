#include "stdafx.h"

	const TFormat TFormat::Unknown={ TMedium::UNKNOWN, -1,-1,-1, TFormat::LENGTHCODE_128,-1, 1 };

	bool TFormat::operator==(const TFormat &fmt2) const{
		// True <=> Formats{1,2} are equal, otherwise False
		return	supportedMedia&fmt2.supportedMedia
				&&
				nCylinders==fmt2.nCylinders
				&&
				nHeads==fmt2.nHeads
				&&
				nSectors==fmt2.nSectors
				&&
				sectorLength==fmt2.sectorLength
				&&
				clusterSize==fmt2.clusterSize;
	}
	DWORD TFormat::GetCountOfAllSectors() const{
		// determines and returns the count of all Sectors
		return nCylinders*nHeads*nSectors;
	}





	bool TSectorId::operator==(const TSectorId &id2) const{
		// True <=> Sector IDs are equal, otherwise False
		return	cylinder==id2.cylinder
				&&
				side==id2.side
				&&
				sector==id2.sector
				&&
				lengthCode==id2.lengthCode;
	}
	bool TSectorId::operator!=(const TSectorId &id2) const{
		// True <=> Identifiers are not equal, otherwise False
		return !operator==(id2);
	}

	TSectorId &TSectorId::operator=(const FD_ID_HEADER &rih){
		// assigns Simon Owen's definition of ID to this ID and returns it
		cylinder=rih.cyl, side=rih.head, sector=rih.sector, lengthCode=rih.size;
		return *this;
	}

	PTCHAR TSectorId::ToString(PTCHAR buffer) const{
		// fills the Buffer with string describing the Sector ID and returns the Buffer (caller guarantees that the Buffer is big enough)
		::wsprintf(buffer,_T("ID={%d,%d,%d,%d}"),cylinder,side,sector,lengthCode);
		return buffer;
	}

	bool TPhysicalAddress::operator==(const TPhysicalAddress &chs2) const{
		// True <=> PhysicalAddresses are equal, otherwise False
		return	cylinder==chs2.cylinder
				&&
				head==chs2.head
				&&
				sectorId==chs2.sectorId;
	}
	TTrack TPhysicalAddress::GetTrackNumber() const{
		// determines and returns the Track number based on DOS's current Format
		return GetTrackNumber( CImage::__getActive__()->dos->formatBoot.nHeads );
	}
	TTrack TPhysicalAddress::GetTrackNumber(THead nHeads) const{
		// determines and returns the Track number based on the specified NumberOfHeads
		return cylinder*nHeads+head;
	}





	const TFdcStatus TFdcStatus::WithoutError;

	TFdcStatus::TFdcStatus()
		// ctor
		: reg1(0) , reg2(0) {
	}

	TFdcStatus::TFdcStatus(BYTE _reg1,BYTE _reg2)
		// ctor
		: reg1(_reg1 & (FDC_ST1_END_OF_CYLINDER|FDC_ST1_DATA_ERROR|FDC_ST1_NO_DATA|FDC_ST1_NO_ADDRESS_MARK))
		, reg2(_reg2 & (FDC_ST2_DELETED_DAM|FDC_ST2_CRC_ERROR_IN_DATA|FDC_ST2_NOT_DAM)) {
	}

	WORD TFdcStatus::ToWord() const{
		// returns Registers{1,2} as a combination in a single Word
		return MAKEWORD(reg1,reg2);
	}

	void TFdcStatus::GetDescriptionsOfSetBits(LPCTSTR *pDescriptions) const{
		// generates Descriptions of currently set bits (caller guarantees that the buffer is large enough)
		if (reg1 & FDC_ST1_END_OF_CYLINDER)		*pDescriptions++=_T("end of cylinder");
		if (reg1 & FDC_ST1_DATA_ERROR)			*pDescriptions++=_T("error in ID or Data field");
		if (reg1 & FDC_ST1_NO_DATA)				*pDescriptions++=_T("ID field with error");
		if (reg1 & FDC_ST1_NO_ADDRESS_MARK)		*pDescriptions++=_T("missing address mark");
		*pDescriptions++=NULL; // end of Descriptions of set bits in Register1
		if (reg2 & FDC_ST2_DELETED_DAM)			*pDescriptions++=_T("Data field deletion inconsistence");
		if (reg2 & FDC_ST2_CRC_ERROR_IN_DATA)	*pDescriptions++=_T("Data field CRC error");
		if (reg2 & FDC_ST2_NOT_DAM)				*pDescriptions++=_T("no Data field found");
		*pDescriptions++=NULL; // end of Descriptions of set bits in Register2
	}

	bool TFdcStatus::IsWithoutError() const{
		// True <=> Registers don't describe any error, otherwise False
		return (reg1|reg2)==0;
	}

	bool TFdcStatus::DescribesIdFieldCrcError() const{
		// True <=> Registers describe that ID Field cannot be read without error, otherwise False
		return reg1&FDC_ST1_NO_DATA;
	}

	void TFdcStatus::CancelIdFieldCrcError(){
		// resets ID Field CRC error bits
		reg1&=~FDC_ST1_NO_DATA;
		if (!DescribesDataFieldCrcError()) // no indication that Data Field contains a CRC error
			reg1&=~FDC_ST1_DATA_ERROR;
	}

	bool TFdcStatus::DescribesDataFieldCrcError() const{
		// True <=> Registers describe that ID Field or Data contain CRC error, otherwise False
		return reg2&FDC_ST2_CRC_ERROR_IN_DATA;
	}

	void TFdcStatus::CancelDataFieldCrcError(){
		// resets Data Field CRC error bits
		reg2&=~FDC_ST2_CRC_ERROR_IN_DATA;
		if (!DescribesIdFieldCrcError()) // no indication that ID Field contains a CRC error
			reg1&=~FDC_ST1_DATA_ERROR;
	}

	bool TFdcStatus::DescribesDeletedDam() const{
		// True <=> Registers describe that using normal data reading command, deleted data have been read instead, otherwise False
		return reg2&FDC_ST2_DELETED_DAM;
	}

	bool TFdcStatus::DescribesMissingDam() const{
		// True <=> Registers describe that the data portion of a Sector has not been found, otherwise False
		return reg1&FDC_ST1_NO_ADDRESS_MARK || reg2&FDC_ST2_NOT_DAM;
	}










	LPCTSTR TMedium::GetDescription(TType mediumType){
		// returns the string description of a given MediumType
		switch (mediumType){
			case FLOPPY_DD	: return _T("2DD floppy");
			case FLOPPY_HD	: return _T("HD floppy");
			case HDD_RAW	: return _T("Hard disk (without MBR support)");
			default:
				ASSERT(FALSE);
				return NULL;
		}
	}
	TMedium::PCProperties TMedium::GetProperties(TType mediumType){
		// returns properties of a given MediumType
		switch (mediumType){
			case FLOPPY_DD:
			case FLOPPY_HD:{
				static const TProperties P={{ 1, FDD_CYLINDERS_MAX }, // supported range of Cylinders (min and max)
											{ 1, 2 },	// supported range of Heads (min and max)
											{ 1, FDD_SECTORS_MAX } // supported range of Sectors (min and max)
										};
				return &P;
			}
			case HDD_RAW:{
				static const TProperties P={{ 1, HDD_CYLINDERS_MAX },// supported range of Cylinders (min and max)
											{ 1, HDD_HEADS_MAX },	// supported range of Heads (min and max)
											{ 1, (TSector)-1 }	// supported range of Sectors (min and max)
										};
				return &P;
			}
			default:
				ASSERT(FALSE);
				return NULL;
		}
	}












	PImage CImage::__getActive__(){
		// returns active document
		return (PImage)CMainWindow::CTdiTemplate::pSingleInstance->__getDocument__();
	}

	bool CImage::__openImageForReading__(LPCTSTR fileName,CFile *f){
		// True <=> File successfully opened for reading, otherwise False
		return f->Open( fileName, CFile::modeRead|CFile::typeBinary|CFile::shareDenyWrite )!=FALSE;
	}

	bool CImage::__openImageForWriting__(LPCTSTR fileName,CFile *f){
		// True <=> File successfully opened for writing, otherwise False
		if (f->Open( fileName, CFile::modeCreate|CFile::modeWrite|CFile::typeBinary|CFile::shareExclusive )!=FALSE){
			::SetLastError(ERROR_SUCCESS); // because the last error might have been 183 (File cannot be created because it already exists)
			return true;
		}else{
			TCHAR buf[MAX_PATH+30];
			::wsprintf( buf, _T("Cannot save to \"%s\""), fileName );
			TUtils::FatalError(buf,::GetLastError());
			return false;
		}
	}

	#define LENGTH_CODE_BASE	0x80

	BYTE CImage::__getSectorLengthCode__(WORD sectorLength){
		// returns the code of the SectorLength
		BYTE lengthCode=0;
		while (sectorLength>LENGTH_CODE_BASE) sectorLength>>=1, lengthCode++;
		return lengthCode;
	}

	WORD CImage::__getOfficialSectorLength__(BYTE sectorLengthCode){
		// returns the official size in Bytes of a Sector with the given LengthCode
		return LENGTH_CODE_BASE<<sectorLengthCode;
	}

	CPtrList CImage::known;

	CImage::PCProperties CImage::__determineTypeByExtension__(LPCTSTR extension){
		// determines and returns Properties for an Image with a given Extension; returns Null if extension unknown
		if (extension){
			TCHAR buf[MAX_PATH];
			::lstrcat( ::CharLower(::lstrcpy(buf,extension)), IMAGE_FORMAT_SEPARATOR );
			for( POSITION pos=known.GetHeadPosition(); pos; ){
				const CImage::PCProperties p=(CImage::PCProperties)CImage::known.GetNext(pos);
				TCHAR tmp[40];
				::lstrcat( ::CharLower(::lstrcpy(tmp,p->filter)), IMAGE_FORMAT_SEPARATOR );
				if (_tcsstr(tmp,buf))
					return p;
			}
		}
		return NULL;
	}

	BYTE CImage::__populateComboBoxWithCompatibleMedia__(HWND hComboBox,WORD dosSupportedMedia,PCProperties imageProperties){
		// populates ComboBox with Media supported both by DOS and Image, and returns their number (or zero if there is no intersection)
		CComboBox cb;
		cb.Attach(hComboBox);
			cb.ResetContent();
			const WORD mediaSupportedByImage= imageProperties ? imageProperties->supportedMedia : 0;
			BYTE result=0;
			for( WORD commonMedia=dosSupportedMedia&mediaSupportedByImage,type=1,n=8*sizeof(commonMedia); n--; type<<=1 )
				if (commonMedia&type){
					cb.SetItemDataPtr( cb.AddString(TMedium::GetDescription((TMedium::TType)type)), (PVOID)type );
					result++;
				}
			if (!result)
				cb.AddString(_T("No compatible medium"));
			cb.EnableWindow(result);
			cb.SetCurSel(0);
		cb.Detach();
		return result;
	}









	CImage::CImage(PCProperties _properties,bool hasEditableSettings)
		// ctor
		// - initialization
		: properties(_properties) , dos(NULL)
		, hasEditableSettings(hasEditableSettings) , writeProtected(true) , canBeModified(true)
		// - creating Toolbar (its displaying in CTdiView::ShowContent)
		, toolbar(IDR_IMAGE,ID_IMAGE) { // ID_IMAGE = "some" unique ID
		// - when destroying all Views, the document must exist further (e.g. when switching Tabs in TDI)
		m_bAutoDelete=FALSE;
	}

	CImage::~CImage(){
		// dtor
		// - disposing DOS
		if (dos)
			delete dos;
	}







	void CImage::OnCloseDocument(){
		// document is being closed
		//nop (CDocument::OnCloseDocument destroys parent FrameWnd (MainWindow) - this must exist even after the document was closed)
	}

	void CImage::__toggleWriteProtection__(){
		// toggles Image's WriteProtection flag
		// - cannot toggle if not allowed to write to the Image (e.g. because the Image has been opened from a CD-ROM)
		if (!canBeModified)
			return;
		// - if this Image is in existing file, verifying that the file can be modified
		const CString &path=GetPathName();
		if (!path.IsEmpty() && path!=FDD_A_LABEL){
			const DWORD attr=::GetFileAttributes(path);
			if (attr!=(DWORD)INVALID_HANDLE_VALUE && attr&FILE_ATTRIBUTE_READONLY)
				return TUtils::FatalError(_T("Cannot toggle the write protection"),_T("The file is read-only."));
		}
		// - toggling WriteProtection
		writeProtected=!writeProtected;
	}

	bool CImage::__reportWriteProtection__() const{
		// True <=> Image is WriteProtected and a warning window has been shown, otherwise False
		if (writeProtected){
			TUtils::Information(_T("This operation requires the image to be accessible for writing.\n\nRemove the write protection and try again."));
			return true;
		}else
			return false;
	}

	#define INI_MSG_SAVE_AS		_T("msgsaveas")

	BOOL CImage::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:
				// update
				if (dos)
					if (dos->UpdateCommandUi(nID,(CCmdUI *)pExtra))
						return TRUE;
				switch (nID){
					case ID_FILE_SAVE:
						((CCmdUI *)pExtra)->Enable(m_bModified);
						return TRUE;
					case ID_IMAGE_PROTECT:
						((CCmdUI *)pExtra)->SetCheck(writeProtected);
						((CCmdUI *)pExtra)->Enable(canBeModified);
						return TRUE;
					case ID_IMAGE_SETTINGS:
						((CCmdUI *)pExtra)->Enable(hasEditableSettings);
						return TRUE;
					case ID_IMAGE_DUMP:
						((CCmdUI *)pExtra)->Enable(TRUE);
						return TRUE;
					case ID_IMAGE_PATCH:
						((CCmdUI *)pExtra)->Enable( dos->formatBoot.mediumType&TMedium::FLOPPY_ANY );
						return TRUE;
				}
				break;
			case CN_COMMAND:
				// command
				if (dos)
					switch (dos->ProcessCommand(nID)){
						case CDos::TCmdResult::DONE_REDRAW:
							UpdateAllViews(NULL,0,NULL);
							// fallthrouh
						case CDos::TCmdResult::DONE:
							return TRUE;
					}
				switch (nID){
					case ID_FILE_SAVE_AS:
						TUtils::InformationWithCheckableShowNoMore( _T("Conversion between image types (e.g. DSK to IMA) must be approached by dumping, not by \"Saving as\"."), INI_GENERAL, INI_MSG_SAVE_AS );
						OnFileSaveAs();
						return TRUE;
					case ID_IMAGE_PROTECT:
						// . toggling WriteProtection
						__toggleWriteProtection__();
						// . refreshing known windows that depend on Image's WriteProtection flag
						if (CDos::CHexaPreview::pSingleInstance) 
							CDos::CHexaPreview::pSingleInstance->hexaEditor.SetEditable(!writeProtected);
						return TRUE;
					case ID_IMAGE_SETTINGS:
						EditSettings();
						return TRUE;
					case ID_IMAGE_DUMP:
						__dump__();
						return TRUE;
					case ID_IMAGE_PATCH:
						if (!__reportWriteProtection__()){
							__patch__();
							UpdateAllViews(NULL,0,NULL);
						}
						return TRUE;
					case ID_FILE_CLOSE:
						return FALSE; // for CDocument to be excluded from processing, and processing was forwarded right to MainWindow
				}
				break;
		}
		return CDocument::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	}

	BOOL CImage::DoSave(LPCTSTR lpszPathName,BOOL bReplace){
		// True <=> Image successfully saved, otherwise False
		app.m_pMainWnd->SetFocus(); // to immediately carry out actions that depend on focus
		TCHAR bufFileName[MAX_PATH];
		if (!lpszPathName){
			// FileName not determined yet or the file is read-only - determining FileName now
			if (/*bReplace &&*/ m_strPathName.IsEmpty()){ // A&B; A = the "Save as" command, B = fully qualified FileName not determined yet; commented out as "Save file copy" command not used (when it holds bReplace==False)
				// . validating that there are no Forbidden characters
				if (const PTCHAR forbidden=::strpbrk( ::lstrcpy(bufFileName,m_strTitle), _T("#%;/\\<>:") ))
					*forbidden='\0';
				// . adding Extension
				::strncat( bufFileName, 1+properties->filter, 4 ); // 1 = asterisk, 4 = dot and three-character extension (e.g. "*.d40")
			}else
				::lstrcpy(bufFileName,m_strPathName);
			if (!CRideApp::__doPromptFileName__( bufFileName, false, AFX_IDS_SAVEFILE, OFN_HIDEREADONLY|OFN_PATHMUSTEXIST, properties ))
				return FALSE;
		}else
			::lstrcpy(bufFileName,lpszPathName);
		return CDocument::DoSave( bufFileName, bReplace );
	}












	bool CImage::IsWriteProtected() const{
		// True <=> Image is WriteProtected, otherwise False
		return writeProtected;
	}

	bool CImage::CanBeModified() const{
		// True <=> content of this Image CanBeModified (can't be if, for instance, opened from CD-ROM), otherwise False and the Disk remains WriteProtected
		return canBeModified;
	}

	TTrack CImage::GetTrackCount() const{
		// returns the number of all Tracks in the Image
		return GetCylinderCount()*GetNumberOfFormattedSides(0);
	}

	bool CImage::IsTrackHealthy(TCylinder cyl,THead head){
		// True <=> specified Track is not empty and contains only well readable Sectors, otherwise False
		// - if Track is empty, assuming the Track surface is damaged, so the Track is NOT healthy
		TSectorId bufferId[(BYTE)-1]; WORD bufferLength[(BYTE)-1];
		const TSector nSectors=ScanTrack(cyl,head,bufferId,bufferLength);
		if (!nSectors)
			return false;
		// - if any of the Sectors cannot be read without error, the Track is NOT healthy
		for( TSector s=0; s<nSectors; s++ ){
			const TPhysicalAddress chs={ cyl, head, bufferId[s] };
			WORD w; TFdcStatus st;
			if (!GetSectorData(chs,s,false,&w,&st))
				return false;
			if (!st.IsWithoutError())
				return false;
		}
		// - the Track is healthy
		return true;
	}

	PSectorData CImage::GetSectorData(RCPhysicalAddress chs,PWORD sectorLength){
		// returns Data of a Sector on a given PhysicalAddress; returns Null if Sector not found or Track not formatted
		TFdcStatus st;
		if (const PSectorData data=GetSectorData(chs,0,true,sectorLength,&st))
			return st.IsWithoutError() ? data : NULL; // Data must be either without error, or none
		else
			return NULL; // Sector not found
	}

	PSectorData CImage::GetSectorData(RCPhysicalAddress chs){
		// returns Data of a Sector on a given PhysicalAddress; returns Null if Sector not found or Track not formatted
		WORD w;
		return GetSectorData(chs,&w);
	}

	PSectorData CImage::GetSectorDataOfUnknownLength(TPhysicalAddress &rChs,PWORD sectorLength){
		// returns Data of a Sector of unknown length (i.e. LengthCode is not used to find Sector with a given ID)
		// - scanning given Track to find out Sectors a their Lengths
		TSectorId bufferId[(TSector)-1];	WORD bufferLength[(TSector)-1];
		TSector nSectorsOnTrack=ScanTrack(rChs.cylinder,rChs.head,bufferId,bufferLength);
		// - searching for first matching ID among found Sectors (LengthCode ignored)
		for( PCSectorId pId=bufferId; nSectorsOnTrack; nSectorsOnTrack-- ){
			rChs.sectorId.lengthCode=pId->lengthCode; // accepting whatever LengthCode
			if (rChs.sectorId==*pId++)
				break;
		}
		if (!nSectorsOnTrack) // Sector with a given ID not found (LengthCode ignored)
			return NULL;
		// - retrieving Data
		return GetSectorData(rChs,sectorLength);
	}

	void CImage::MarkSectorAsDirty(RCPhysicalAddress chs){
		// marks Sector on a given PhysicalAddress as "dirty", plus sets it the given FdcStatus
		MarkSectorAsDirty(chs,0,&TFdcStatus::WithoutError);
	}

	TStdWinError CImage::SetMediumTypeAndGeometry(PCFormat,PCSide,TSector){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		return ERROR_SUCCESS;
	}

	bool CImage::RequiresFormattedTracksVerification() const{
		// True <=> the Image requires its newly formatted Tracks be verified, otherwise False (and caller doesn't have to carry out verification)
		return false; // verification NOT required by default (but Images abstracting physical drives can override this setting)
	}

	TStdWinError CImage::PresumeHealthyTrackStructure(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId){
		// without formatting it, presumes that given Track contains Sectors with specified parameters; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED; // each Track by default must be explicitly formatted to be sure about its structure (but Images abstracting physical drives can override this setting)
	}

	BOOL CImage::CanCloseFrame(CFrameWnd* pFrame){
		// True <=> the MainWindow can be closed (and thus the application), otherwise False
		// - first asking the DOS that handles this Image
		if (!dos->CanBeShutDown(pFrame))
			return FALSE;
		// - then attempting to close this Image
		return CDocument::CanCloseFrame(pFrame);
	}
