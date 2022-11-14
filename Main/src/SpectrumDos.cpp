#include "stdafx.h"

	CSpectrumDos::CSpectrumDos(PImage image,PCFormat pFormatBoot,TTrackScheme trackAccessScheme,PCProperties properties,UINT nResId,CSpectrumBaseFileManagerView *pFileManager,TGetFileSizeOptions getFileSizeDefaultOption,TSectorStatus unformatFatStatus)
		// ctor
		// - base
		: CSpectrumBase(image,pFormatBoot,trackAccessScheme,properties,nResId,pFileManager,getFileSizeDefaultOption,unformatFatStatus)
		// - loading MRU Tapes
		, mruTapes( 0, INI_SPECTRUM _T("MruTapes"), _T("T%d"), 4 ) {
		mruTapes.ReadList();
	}

	CSpectrumDos::~CSpectrumDos(){
		// dtor
		if (CTape::pSingleInstance)
			delete CTape::pSingleInstance;
		mruTapes.WriteList();
	}









	#define IMAGE	tab.image
	#define DOS		IMAGE->dos

	#define FORMAT_ADDITIONAL_COUNT	2

	#define TAPE_EXTENSION	_T(".tap")
	#define TAPE_FILTER		_T("Tape (*") TAPE_EXTENSION _T(")|*") TAPE_EXTENSION _T("|")

	CDos::TCmdResult CSpectrumDos::ProcessCommand(WORD cmd){
		// returns the Result of processing a DOS-related command
		switch (cmd){
			case ID_DOS_FILL_EMPTY_SPACE:
				// filling out empty space on disk
				__fillEmptySpace__( CFillEmptySpaceDialog(this) ); // WARNING: It's assumed that "dot" and "dotdot"-like DirectoryEntries are disabled to prevent from unfinite looping when selected to fill Empty DirectoryEntries!
				return TCmdResult::DONE_REDRAW;
			case ID_DOS_FORMAT:{
				// formatting standard Cylinders (i.e. with standard "official" Sectors)
				const TCylinder cylMin=std::min<int>( 1+GetLastOccupiedStdCylinder(), formatBoot.nCylinders );
				CFormatDialog::TStdFormat additionalFormats[]={
					{ _T("Expand to 40 cylinders"),	cylMin, formatBoot, 1, 0, FDD_350_SECTOR_GAP3, properties->stdFormats->params.nAllocationTables, properties->nRootDirectoryEntriesMax }, // Gap3 is fine even for 5.25" Spectrum floppies
					{ _T("Expand to 80 cylinders"),	cylMin, formatBoot, 1, 0, FDD_350_SECTOR_GAP3, properties->stdFormats->params.nAllocationTables, properties->nRootDirectoryEntriesMax }
				};
					additionalFormats[0].params.format.nCylinders=39;
					additionalFormats[1].params.format.nCylinders=79;
				CFormatDialog d(this, additionalFormats,
								cylMin&&formatBoot.mediumType!=Medium::UNKNOWN ? FORMAT_ADDITIONAL_COUNT : 0 // AdditionalFormats available only if Image already formatted before
							);
				return	ShowDialogAndFormatStdCylinders(d)==ERROR_SUCCESS
						? TCmdResult::DONE_REDRAW
						: TCmdResult::REFUSED;
			}
			case ID_DOS_UNFORMAT:{
				// unformatting Cylinders
				const TCylinder cylMin=1+GetLastOccupiedStdCylinder(), cylMax=image->GetCylinderCount()-1;
				const CUnformatDialog::TStdUnformat stdUnformats[]={
					{ _T("Trim to 40 cylinders"),	40, cylMax },
					{ _T("Trim to 80 cylinders"),	80, cylMax },
					{ STR_TRIM_TO_MIN_NUMBER_OF_CYLINDERS,	cylMin, cylMax }
				};
				if (const TStdWinError err=CUnformatDialog( this, stdUnformats, ARRAYSIZE(stdUnformats) ).ShowModalAndUnformatStdCylinders()){
					Utils::Information( DOS_ERR_CANNOT_UNFORMAT, err );
					return TCmdResult::REFUSED;
				}else
					return TCmdResult::DONE_REDRAW;
			}
			case ID_FILE_CLOSE:
			case ID_TAPE_CLOSE:
				// closing this Image (or a Tape, if opened)
				if (CTape::pSingleInstance)
					if (CTape::pSingleInstance->image->SaveModified()){
						CTdiCtrl::RemoveTab( TDI_HWND, &CTape::pSingleInstance->pFileManager->tab );
						return TCmdResult::DONE; // the closing command applies only to the open Tape, not to the main disk Image
					}else
						return TCmdResult::REFUSED; // rejected to close the Tape
				break; // closing the main disk Image
			case ID_TAPE_NEW:{
				// creating the underlying Tape file on local disk
				if (CTape::pSingleInstance) // closing the open Tape first
					if (ProcessCommand(ID_FILE_CLOSE)==TCmdResult::REFUSED) // if closing of the open Tape rejected ...
						return TCmdResult::DONE; // ... a new Tape cannot be created
				const CString fileName=Utils::DoPromptSingleTypeFileName( _T("newTape") TAPE_EXTENSION, TAPE_FILTER, OFN_HIDEREADONLY|OFN_DONTADDTORECENT );
				if (!fileName.IsEmpty()){
					// . ejecting current Tape (if any)
					if (CTape::pSingleInstance)
						if (ProcessCommand(ID_TAPE_CLOSE)==TCmdResult::REFUSED) // if Tape not ejected ...
							return TCmdResult::DONE; // ... we are done (successfully)
					// . inserting a blank Tape (by creating a new underlying physical file and opening it)
					CFile( fileName, CFile::modeCreate|CFile::shareDenyRead|CFile::typeBinary ).Close(); // creating the underlying file on local disk
					( CTape::pSingleInstance=new CTape(fileName,this,true) )->ToggleWriteProtection(); // new Tape is not WriteProtected
					return TCmdResult::DONE;
				}else
					return TCmdResult::DONE_REDRAW;
			}
			case ID_TAPE_OPEN:{
				// opening an existing file with Tape
				if (CTape::pSingleInstance) // closing the open Tape first
					if (ProcessCommand(ID_FILE_CLOSE)==TCmdResult::REFUSED) // if closing of the open Tape rejected ...
						return TCmdResult::DONE; // ... a new Tape cannot be open
				const CString fileName=Utils::DoPromptSingleTypeFileName( nullptr, TAPE_FILTER, OFN_FILEMUSTEXIST );
				if (!fileName.IsEmpty()){
					// . ejecting current Tape (if any)
					if (CTape::pSingleInstance)
						if (ProcessCommand(ID_TAPE_CLOSE)==TCmdResult::REFUSED) // if Tape not ejected ...
							return TCmdResult::DONE; // ... we are done (successfully)
					// . inserting a recorded Tape (by opening its underlying physical file)
					CTape::pSingleInstance=new CTape(fileName,this,true); // inserted Tape is WriteProtected by default
					return TCmdResult::DONE;
				}else
					return TCmdResult::DONE_REDRAW;
			}
			case ID_TAPE_APPEND:{
				// copying content of another tape to the end of this tape
				const CString fileName=Utils::DoPromptSingleTypeFileName( nullptr, TAPE_FILTER, OFN_FILEMUSTEXIST );
				if (!fileName.IsEmpty()){
					// . opening the Tape to append
					//const std::unique_ptr<const CTape> tape(new CTape(fileName,this,false));
					const CTape *const tape=new CTape(fileName,this,false);
						// . appending each File (or block)
						if (const auto pdt=tape->BeginDirectoryTraversal(ZX_DIR_ROOT))
							for( BYTE blockData[65536]; const PCFile file=pdt->GetNextFileOrSubdir(); ){
								const CString nameAndExt=tape->GetFileExportNameAndExt( file, false );
								PFile f;
								if (const TStdWinError err=	CTape::pSingleInstance->ImportFile(
																&CMemFile(blockData,sizeof(blockData)), 
																tape->ExportFile( file, &CMemFile(blockData,sizeof(blockData)), sizeof(blockData), nullptr ),
																nameAndExt,
																0, f
															)
								){
									CString msg;
									msg.Format( _T("Failed to append block \"%s\""), (LPCTSTR)nameAndExt );
									Utils::Information( msg, err, _T("Appended only partly.") );
									break;
								}
							}
					CTdiCtrl::RemoveTab( TDI_HWND, &tape->pFileManager->tab );
					image->UpdateAllViews(nullptr);
					return TCmdResult::DONE;
				}else
					return TCmdResult::DONE_REDRAW;
			}
			case ID_FILE_MRU_FILE10:
			case ID_FILE_MRU_FILE11:
			case ID_FILE_MRU_FILE12:
			case ID_FILE_MRU_FILE13:
				// opening one of most recently used Tapes
				// . ejecting current Tape (if any)
				if (CTape::pSingleInstance)
					if (ProcessCommand(ID_TAPE_CLOSE)==TCmdResult::REFUSED) // if Tape not ejected ...
						return TCmdResult::DONE; // ... we are done (successfully)
				// . inserting a recorded Tape (by opening its underlying physical file)
				CTape::pSingleInstance=new CTape(mruTapes[cmd-ID_FILE_MRU_FILE10],this,true); // inserted Tape is WriteProtected by default
				return TCmdResult::DONE;
			default:
				// passing a non-recognized Command to an open Tape first
				if (__isTapeFileManagerShown__() && CTape::pSingleInstance->OnCmdMsg(cmd,CN_COMMAND,nullptr,nullptr))
					return TCmdResult::DONE;
		}
		return __super::ProcessCommand(cmd);
	}

	bool CSpectrumDos::UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const{
		// True <=> given Command-specific user interface successfully updated, otherwise False
		switch (cmd){
			case ID_TAPE_APPEND:
				pCmdUI->Enable(CTape::pSingleInstance!=nullptr && !CTape::pSingleInstance->IsWriteProtected());
				return true;
			case ID_TAPE_CLOSE:
				pCmdUI->Enable(CTape::pSingleInstance!=nullptr);
				return true;
			case ID_FILE_MRU_FILE10:
				mruTapes.UpdateMenu(pCmdUI);
				return true;
			default:
				if (__isTapeFileManagerShown__() && CTape::pSingleInstance->OnCmdMsg(cmd,CN_UPDATE_COMMAND_UI,pCmdUI,nullptr))
					return true;
				break;
		}
		return __super::UpdateCommandUi(cmd,pCmdUI);
	}

	bool CSpectrumDos::__isTapeFileManagerShown__() const{
		// True <=> Tape's FileManager is currently shown in the TDI, otherwise False
		return CTape::pSingleInstance && CTape::pSingleInstance->fileManager.m_hWnd; // A&B, A = Tape inserted, B = Tape's FileManager currently switched to
	}

	bool CSpectrumDos::CanBeShutDown(CFrameWnd* pFrame) const{
		// True <=> this DOS has no dependecies which would require it to remain active, otherwise False (has some dependecies which require the DOS to remain active)
		// - first attempting to close the Tape
		if (CTape::pSingleInstance)
			if (!CTape::pSingleInstance->CanCloseFrame(pFrame))
				return FALSE;
		// - base
		return __super::CanBeShutDown(pFrame);
	}
