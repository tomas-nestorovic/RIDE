#include "stdafx.h"
#include "XmlDocs.h"

	CVerifyVolumeDialog::TParams::TParams(CDos *dos,const TVerificationFunctions &rvf)
		// ctor
		: dos(dos)
		, action(THREAD_PRIORITY_BELOW_NORMAL)
		, verifyBootSector(false) , verifyFat(false) , verifyFilesystem(false)
		, verifyVolumeSurface(false)
		, repairStyle(0)
		, verificationFunctions(rvf) {
	}

	template<typename T>
	TStdWinError CVerifyVolumeDialog::TParams::TerminateAndGoToNextAction(T error) const{
		// terminates with specified Error the action which called this method
		fReport.CloseSection( Utils::ComposeErrorMessage(_T("Can't finish this step"),error,_T("Some additional errors may remain, verification should be run once more!")) );
		return action.TerminateWithSuccess(); // proceed with the next planned action
	}

	template TStdWinError CVerifyVolumeDialog::TParams::TerminateAndGoToNextAction<TStdWinError>(TStdWinError err) const;
	template TStdWinError CVerifyVolumeDialog::TParams::TerminateAndGoToNextAction<LPCTSTR>(LPCTSTR err) const;

	TStdWinError CVerifyVolumeDialog::TParams::TerminateAll(TStdWinError error) const{
		// terminates all planned actions with specified Error
		fReport.CloseSection( Utils::ComposeErrorMessage(_T("Can't go on with verification"),error) );
		return action.TerminateWithError(error);
	}

	TStdWinError CVerifyVolumeDialog::TParams::CancelAll() const{
		// cancels all planned actions with specified Error
		return TerminateAll(ERROR_CANCELLED);
	}

	BYTE CVerifyVolumeDialog::TParams::ConfirmFix(LPCTSTR problemDesc,LPCTSTR problemSolutionSuggestion) const{
		// presents the occured problem along with suggestion how to solve it, and returns user's reaction
		fReport.OpenProblem(problemDesc);
		switch (repairStyle){
			default:
				ASSERT(FALSE);
			case 0:
				// automatic fixing of each Problem
				return IDYES;
			case 1:{
				// fixing only manually confirmed Problems
				TCHAR buf[2000];
				::wsprintf( buf, _T("%s.\n\nFix this problem? %s"), problemDesc, problemSolutionSuggestion );
				return Utils::QuestionYesNoCancel( buf, MB_DEFBUTTON1 );
			}
			case 2:
				// automatic rejection of fix to any Problem
				return IDNO;
		}
	}

	BYTE CVerifyVolumeDialog::TParams::ConfirmUnsignedValueFix(LPCTSTR locationName,LPCTSTR valueName,WORD valueOffset,DWORD value,DWORD rangeA,DWORD rangeZ) const{
		// confirms an unsigned numeric value at specified Offset has a Value from Range={A,...,Z}; if not, presents the problem using standard formulation and returns user's reaction
		if (rangeA<=value && value<=rangeZ)
			return IDNO; // ok, Value in Range, carry on verification
		int nHexaDigits;
		if (rangeZ<=(BYTE)-1)
			nHexaDigits=sizeof(BYTE);
		else if (rangeZ<=(WORD)-1)
			nHexaDigits=sizeof(WORD);
		else
			nHexaDigits=sizeof(DWORD);
		nHexaDigits<<=1;
		CString err, quotedValueName;
		if (valueName)
			quotedValueName.Format( _T(" \"%s\""), valueName );
		err.Format( _T("Value%s at offset 0x%04X %s%s has invalid value %d (0x%0*X)"),
					quotedValueName,
					valueOffset,
					locationName ? _T("in the ") : _T(""),
					locationName ? locationName : _T(""),
					value,
					nHexaDigits, value
				);
		CString sol;
		if (rangeA!=rangeZ)
			sol.Format(	_T("The expected range is from %d (0x%0*X) to %d (0x%0*X)."),
						rangeA,
						nHexaDigits, rangeA,
						rangeZ,
						nHexaDigits, rangeZ
					);
		else
			sol.Format(	_T("The correct value is %d (0x%0*X)."),
						rangeA,
						nHexaDigits, rangeA
					);
		return ConfirmFix( err, sol );
	}

	bool CVerifyVolumeDialog::TParams::WarnIfUnsignedValueOutOfRange(LPCTSTR locationName,LPCTSTR valueName,WORD valueOffset,DWORD value,DWORD rangeA,DWORD rangeZ) const{
		// issues a warning if unsigned numeric value at specified Offset has a Value out of Range={A,...,Z}
		if (rangeA<=value && value<=rangeZ)
			return false; // ok, Value in Range, carry on verification
		int nHexaDigits;
		if (rangeZ<=(BYTE)-1)
			nHexaDigits=sizeof(BYTE);
		else if (rangeZ<=(WORD)-1)
			nHexaDigits=sizeof(WORD);
		else
			nHexaDigits=sizeof(DWORD);
		nHexaDigits<<=1;
		CString err, quotedValueName;
		if (valueName)
			quotedValueName.Format( _T(" \"%s\""), valueName );
		err.Format( _T("Value%s at offset 0x%04X %s%s has invalid value %d (0x%0*X)"),
					quotedValueName,
					valueOffset,
					locationName ? _T("in the ") : _T(""),
					locationName ? locationName : _T(""),
					value,
					nHexaDigits, value
				);
		fReport.LogWarning(err);
		switch (repairStyle){
			default:
				ASSERT(FALSE);
			case 0:
				// automatic fixing of each Problem
				break;
			case 1:
				// fixing only manually confirmed Problems
				Utils::Information(err);
				//fallthrough
			case 2:
				// automatic rejection of fix to any Problem
				break;
		}
		return true;
	}






	CVerifyVolumeDialog::TParams::CReportFile::CReportFile()
		// ctor
		: itemListBegun(false) , problemOpen(false) , inProblemSolvingSection(false) {
	}

	void CVerifyVolumeDialog::TParams::CReportFile::OpenSection(LPCTSTR name,bool problemSolving){
		// begins in the Report a new section with specified Name
		CloseSection();
		Utils::WriteToFileFormatted( *this, _T("<h3>%s</h3>"), name );
		inProblemSolvingSection=problemSolving;
	}

	void CVerifyVolumeDialog::TParams::CReportFile::CloseSection(LPCTSTR errMsg){
		// closes current section
		if (errMsg){
			Utils::WriteToFileFormatted( *this, _T("<p style=\"border:1pt solid black;padding:8pt\"><b>%s</b></p>"), (LPCTSTR)CXmlDocument::EncodeXml(errMsg) );
			problemOpen=false; // problem implicitly solved by writing the ErrorMessage
		}else if (problemOpen)
			CloseProblem(false);
		if (itemListBegun){
			Utils::WriteToFile(*this,_T("</ul>"));
			itemListBegun=false;
		}else if (inProblemSolvingSection)
			Utils::WriteToFile(*this,_T("No problems found."));
		inProblemSolvingSection=false;
	}

	void AFX_CDECL CVerifyVolumeDialog::TParams::CReportFile::LogWarning(LPCTSTR format,...){
		// writes the Warning message to the Report
		if (problemOpen)
			CloseProblem(false);
		if (!itemListBegun){
			Utils::WriteToFile(*this,_T("<ul>"));
			itemListBegun=true;
		}
		va_list argList;
		va_start( argList, format );
			TCHAR buf[16384];
			::wvsprintf( buf, format, argList );
		va_end(argList);
		Utils::WriteToFileFormatted( *this, _T("<li>WARNING: %s.</li>"), (LPCTSTR)CXmlDocument::EncodeXml(buf) );
	}

	void CVerifyVolumeDialog::TParams::CReportFile::OpenProblem(LPCTSTR problemDesc){
		// begins an item on a repair of specified Problem
		if (problemOpen)
			CloseProblem(false);
		if (!itemListBegun){
			Utils::WriteToFile(*this,_T("<ul>"));
			itemListBegun=true;
		}
		Utils::WriteToFileFormatted( *this, _T("<li>%s."), (LPCTSTR)CXmlDocument::EncodeXml(problemDesc) );
		problemOpen=true;
	}

	void CVerifyVolumeDialog::TParams::CReportFile::CloseProblem(bool solved){
		// ends an item on a repair of specified Problem by writing the specified Status
		if (problemOpen){ // ignoring this call if no Problem actually open
			Utils::WriteToFileFormatted( *this, _T("<br><b>%s.</b></li>"), solved?_T("SOLVED"):_T("Skipped") );
			problemOpen=false;
		}
	}

	void CVerifyVolumeDialog::TParams::CReportFile::Close(){
		// closes the Report
		CloseSection();
		Utils::WriteToFile(*this,_T("</body></html>"));
		__super::Close();
	}







	CVerifyVolumeDialog::CVerifyVolumeDialog(TParams &rvp)
		// ctor
		// - base
		: Utils::CRideDialog(IDR_DOS_VERIFY)
		// - initialization
		, params(rvp)
		, nOptionsChecked(0) {
	}









	void CVerifyVolumeDialog::DoDataExchange(CDataExchange *pDX){
		// exchange of data from and to controls
		DDX_Text( pDX, ID_DOS, CString(params.dos->properties->name) );
		DDX_CheckEnable( pDX, ID_BOOT,	params.verifyBootSector, params.verificationFunctions.fnBootSector!=nullptr );
		DDX_CheckEnable( pDX, ID_FAT,	params.verifyFat, params.verificationFunctions.fnFatFullyReadable||params.verificationFunctions.fnFatFilePathsOk||params.verificationFunctions.fnFatCrossedFiles||params.verificationFunctions.fnFatLostAllocUnits );
		DDX_CheckEnable( pDX, ID_FILE1,	params.verifyFilesystem, params.verificationFunctions.fnFilesystem!=nullptr );
		DDX_CheckEnable( pDX, ID_IMAGE,	params.verifyVolumeSurface, params.verificationFunctions.fnVolumeSurface!=nullptr );
		DDX_CBIndex( pDX, ID_REPAIR, params.repairStyle );
	}

	LRESULT CVerifyVolumeDialog::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_NOTIFY:
				// processing notification from child control
				if (GetClickedHyperlinkId(lParam)){
					CheckDlgItem( ID_BOOT, IsDlgItemEnabled(ID_BOOT) );
					CheckDlgItem( ID_FAT, IsDlgItemEnabled(ID_FAT) );
					CheckDlgItem( ID_FILE1, IsDlgItemEnabled(ID_FILE1) );
					wParam=ID_BOOT; // for the below fallthrough
					//fallthrough
				}else
					break;
			case WM_COMMAND:
				// processing a command
				if (ID_BOOT<=wParam && wParam<=ID_IMAGE){
					// one of check-boxes (un)ticked
					nOptionsChecked=IsDlgItemChecked(ID_BOOT)
									+
									IsDlgItemChecked(ID_FAT)
									+
									IsDlgItemChecked(ID_FILE1)
									+
									IsDlgItemChecked(ID_IMAGE);
					EnableDlgItem( IDOK, nOptionsChecked>0 );
				}
				break;
			case WM_PAINT:{
				// drawing
				// . base
				const LRESULT result=__super::WindowProc(msg,wParam,lParam);
				// . wrapping full integrity check options in a curly bracket
				WrapDlgItemsByClosingCurlyBracketWithText( ID_BOOT, ID_FILE1, nullptr, 0 );
				return result;
			}
		}
		return __super::WindowProc(msg,wParam,lParam);
	}












	UINT AFX_CDECL TVerificationFunctions::ReportOnFilesWithBadFatPath_thread(PVOID pCancelableAction){
		// thread to list Files with erroneous FatPath (just list them, without attempting for their recovery)
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const CVerifyVolumeDialog::TParams &vp=*(CVerifyVolumeDialog::TParams *)pAction->GetParams();
		vp.fReport.OpenSection(_T("File sector linkage in FAT"));
		const PImage image=vp.dos->image;
		const auto nSectorsOnDisk=vp.dos->formatBoot.GetCountOfAllSectors();
		DWORD nSectorsVisited=0;
		pAction->SetProgressTarget( nSectorsOnDisk+1 );
		CFileManagerView::CFileList bfsFiles; // breadth first search, searching through Directories in breadth
		CFileManagerView::CFileList visitedDirectories;
		DWORD nItemsVisited=0; // Files and Directories
		for( bfsFiles.AddTail((CDos::PFile)DOS_DIR_ROOT); bfsFiles.GetCount()>0; ){
			if (pAction->Cancelled) return vp.CancelAll();
			// . retrieving File's FatPath
			const CDos::PFile file=bfsFiles.RemoveHead();
			const CDos::CFatPath fatPath( vp.dos, file );
			CDos::CFatPath::PItem pItem; DWORD nItems;
			if (const LPCTSTR err=fatPath.GetItems(pItem,nItems))
				vp.fReport.LogWarning( _T("The linkage of the \"%s\" %s is erroneous: %s"), (LPCTSTR)vp.dos->GetFilePresentationNameAndExt(file), vp.dos->IsDirectory(file)?_T("directory"):_T("file"), err );
			if (!nItems)
				continue; // makes no sense to test a File that occupies no space on the disk
			// . if the File is actually a Directory, processing it recurrently
			if (vp.dos->IsDirectory(file)){
				visitedDirectories.AddTail(file);
				if (const auto pdt=vp.dos->BeginDirectoryTraversal(file))
					while (const CDos::PFile subfile=pdt->GetNextFileOrSubdir())
						switch (pdt->entryType){
							case CDos::TDirectoryTraversal::SUBDIR:
								if (visitedDirectories.Contains( (CDos::PFile)vp.dos->GetDirectoryUid(subfile) )) // the Subdirectory has already been processed
									continue; // not processing it again
								//fallthrough
							case CDos::TDirectoryTraversal::FILE:
								bfsFiles.AddTail(subfile);
								if (++nItemsVisited==nSectorsOnDisk)
									if (Utils::QuestionYesNo(VERIFICATION_WARNING_SIGNIFICANT_PROBLEM,MB_DEFBUTTON1))
										return vp.TerminateAndGoToNextAction(ERROR_CANCELLED);
								break;
							case CDos::TDirectoryTraversal::WARNING:{
								const CString s=Utils::SimpleFormat( _T("The \"%s\" directory can't be processed entirely"), vp.dos->GetFilePresentationNameAndExt(file) );
								vp.fReport.LogWarning( (LPCTSTR)Utils::ComposeErrorMessage(s,pdt->warning) );
								continue;
							}
						}
			}
			// . informing on progress
			if (!fatPath.error)
				pAction->UpdateProgress(  std::min( nSectorsVisited+=fatPath.GetNumberOfItems(), nSectorsOnDisk )  );
		}
		return pAction->TerminateWithSuccess();
	}

	UINT AFX_CDECL TVerificationFunctions::FloppyCrossLinkedFilesVerification_thread(PVOID pCancelableAction){
		// thread to find and separate cross-linked Files on current volume
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const CVerifyVolumeDialog::TParams &vp=*(CVerifyVolumeDialog::TParams *)pAction->GetParams();
		// - checking preconditions (usable only for floppies)
		if (vp.dos->formatBoot.nCylinders>FDD_CYLINDERS_MAX
			||
			vp.dos->formatBoot.nHeads>2
			||
			vp.dos->formatBoot.clusterSize!=1 // because of call to CDos::GetFirstEmptyHealthySector
		)
			return vp.TerminateAndGoToNextAction(ERROR_NOT_SUPPORTED);
		// - verifying the volume
		vp.fReport.OpenSection(FAT_VERIFICATION_CROSSLINKED _T(" (floppy)"));
		const PImage image=vp.dos->image;
		const auto nSectorsOnDisk=vp.dos->formatBoot.GetCountOfAllSectors();
		DWORD nSectorsVisited=0;
		pAction->SetProgressTarget( nSectorsOnDisk+1 );
		CMapWordToPtr sectorOccupation[FDD_CYLINDERS_MAX*2];
		CFileManagerView::CFileList bfsFiles; // breadth first search, searching through Directories in breadth
		CFileManagerView::CFileList visitedDirectories;
		DWORD nItemsVisited=0; // Files and Directories
		for( bfsFiles.AddTail((CDos::PFile)DOS_DIR_ROOT); bfsFiles.GetCount()>0; ){
			if (pAction->Cancelled) return vp.CancelAll();
			const CDos::PFile file=bfsFiles.RemoveHead();
			// . retrieving File's FatPath
			const CDos::CFatPath fatPath( vp.dos, file );
			CDos::CFatPath::PItem pItem; DWORD nItems;
			if (const LPCTSTR err=fatPath.GetItems(pItem,nItems))
				continue; // silently ignoring problems in FatPath - warnings should be taken care of elsewhere
			if (!nItems)
				continue; // makes no sense to test a File that occupies no space on the disk
			// . the changes in FAT must be transactional - all or nothing!
			const struct TFatTransaction sealed{
				const PDos dos;
				const CDos::CFatPath &rFatPath;
				TFatTransaction(PDos dos,const CDos::CFatPath &rFatPath)
					: dos(dos) , rFatPath(rFatPath) {
					CDos::CFatPath::PItem pItem; DWORD nItems;
					for( rFatPath.GetItems(pItem,nItems); nItems--; pItem++->value=0 ); // "0" = Sector isn't cross-linked
				}
				~TFatTransaction(){
					CDos::CFatPath::PItem pItem; DWORD nItems;
					for( rFatPath.GetItems(pItem,nItems); nItems--; pItem++ )
						if (pItem->value) // if Sector cross-linked ...
							dos->ModifyStdSectorStatus( pItem->chs, TSectorStatus::EMPTY ); // ... reverting its reservation, setting it Empty again
				}
			} fatTransaction( vp.dos, fatPath );
			// . checking that the File's FatPath is unique
			bool fatModified=false; // assumption (the File is not cross-linked)
			CString msg;
			for( bool askedToAutoFixProblem=false; nItems--; pItem++ ){
				if (pAction->Cancelled) return vp.CancelAll();
				// : checking preconditions
				RCPhysicalAddress chs=pItem->chs;
				if (chs.sectorId.cylinder!=chs.cylinder
					||
					chs.sectorId.side!=vp.dos->sideMap[chs.head]
					||
					chs.sectorId.sector<vp.dos->properties->firstSectorNumber || vp.dos->properties->firstSectorNumber+vp.dos->formatBoot.nSectors<=chs.sectorId.sector
					||
					chs.sectorId.lengthCode!=vp.dos->formatBoot.sectorLengthCode
				)
					return vp.TerminateAll(ERROR_NOT_SUPPORTED); // we shouldn't end-up here; if we do, it's a critical problem - either DOS not properly implemented or revision of this function needed
				// : verifying that the Sector is not used by another File
				CDos::PFile sectorOccupiedByFile;
				if (sectorOccupation[chs.GetTrackNumber()].Lookup( chs.sectorId.sector, sectorOccupiedByFile )){
					// | asking whether or not to fix this problem
					if (!askedToAutoFixProblem){
						msg.Format( _T("%s \"%s\" is cross-linked with %s \"%s\""), vp.dos->IsDirectory(file)?_T("Directory"):_T("File"), (LPCTSTR)vp.dos->GetFilePresentationNameAndExt(file), vp.dos->IsDirectory(sectorOccupiedByFile)?_T("directory"):_T("file"), (LPCTSTR)vp.dos->GetFilePresentationNameAndExt(sectorOccupiedByFile) );
						switch (vp.ConfirmFix( msg, VERIF_MSG_FILE_UNCROSS )){
							case IDCANCEL:
								return vp.CancelAll();
							case IDNO:
								goto nextFile;
						}
						askedToAutoFixProblem=true; // don't ask for the rest of this File's Sectors
					}
					// | reading the cross-linked Sector
					PCSectorData crossLinkedSectorData;
					while (!( crossLinkedSectorData=image->GetHealthySectorData(chs) )){
						msg.Format( _T("Sector %s in \"%s\" %s is unreadable"), (LPCTSTR)chs.sectorId.ToString(), (LPCTSTR)vp.dos->GetFilePresentationNameAndExt(file), vp.dos->IsDirectory(file)?_T("directory"):_T("file") );
						switch (Utils::CancelRetryContinue( msg, ::GetLastError(), MB_DEFBUTTON2, VERIF_MSG_FILE_UNCROSS )){
							case IDCANCEL:
								return vp.CancelAll();
							case IDCONTINUE:
								goto nextFile;
						}
					}
					// | finding an empty healthy Sector in the volume
					while (const TStdWinError err=vp.dos->GetFirstEmptyHealthySector(true,pItem->chs)){
						msg.Format( _T("\"%s\" cannot be fixed"), (LPCTSTR)vp.dos->GetFilePresentationNameAndExt(file) );
						switch (Utils::CancelRetryContinue( msg, err, MB_DEFBUTTON1 )){
							case IDCANCEL:
								return vp.CancelAll();
							case IDCONTINUE:
								goto nextFile;
						}
					}
					// | reserving the found empty healthy Sector by marking it Bad so that it's excluded from available empty Sectors
					if (vp.dos->ModifyStdSectorStatus( pItem->chs, TSectorStatus::BAD ))
						pItem->value=1; // Sector succesfully reserved
					else
						return vp.TerminateAll(ERROR_NOT_SUPPORTED); // DOS unable to reserve the above Sector
					// | copying data to the found empty healthy Sector
					::memcpy(	image->GetHealthySectorData(pItem->chs),
								crossLinkedSectorData,
								vp.dos->formatBoot.sectorLength
							);
					image->MarkSectorAsDirty(pItem->chs);
					fatModified=true;
				}
				// : recording that given Sector is used by current File
				sectorOccupation[chs.GetTrackNumber()].SetAt( chs.sectorId.sector, file ); // PhysicalAddress might have been fixed above, hence using actual values refered by pItem
			}
			// . writing File's Modified FatPath back to FAT
			if (fatModified){
				vp.dos->ModifyFileFatPath( file, fatPath ); // all FAT Sectors by previous actions guaranteed to be readable
				for( fatPath.GetItems(pItem,nItems); nItems--; pItem++->value=0 ); // "0" = Sector isn't cross-linked
				vp.fReport.CloseProblem(true);
			}
nextFile:	// . if the File is actually a Directory, processing it recurrently
			if (vp.dos->IsDirectory(file)){
				visitedDirectories.AddTail(file);
				if (const auto pdt=vp.dos->BeginDirectoryTraversal(file))
					while (const CDos::PFile subfile=pdt->GetNextFileOrSubdir())
						switch (pdt->entryType){
							case CDos::TDirectoryTraversal::SUBDIR:
								if (visitedDirectories.Contains( (CDos::PFile)vp.dos->GetDirectoryUid(subfile) )) // the Subdirectory has already been processed
									continue; // not processing it again
								//fallthrough
							case CDos::TDirectoryTraversal::FILE:
								bfsFiles.AddTail(subfile);
								if (++nItemsVisited==nSectorsOnDisk)
									if (Utils::QuestionYesNo(VERIFICATION_WARNING_SIGNIFICANT_PROBLEM,MB_DEFBUTTON1))
										return vp.TerminateAndGoToNextAction(ERROR_CANCELLED);
								break;
							case CDos::TDirectoryTraversal::WARNING:
								continue; // silently ignore FAT unreadability - warnings should be taken care of elsewhere
						}
			}
			// . informing on progress
			pAction->UpdateProgress(  std::min( nSectorsVisited+=fatPath.GetNumberOfItems(), nSectorsOnDisk )  );
		}
		return pAction->TerminateWithSuccess();
	}

	UINT AFX_CDECL TVerificationFunctions::FloppyLostSectorsVerification_thread(PVOID pCancelableAction){
		// thread to find Sectors that are reported Occupied or Reserved but are actually not affiliated to any File
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const CVerifyVolumeDialog::TParams &vp=*(CVerifyVolumeDialog::TParams *)pAction->GetParams();
		// - checking preconditions (usable only for floppies)
		if (vp.dos->formatBoot.nCylinders>FDD_CYLINDERS_MAX
			||
			vp.dos->formatBoot.nHeads>2
			||
			vp.dos->formatBoot.clusterSize!=1
		)
			return vp.TerminateAndGoToNextAction(ERROR_NOT_SUPPORTED);
		vp.fReport.OpenSection(FAT_VERIFICATION_LOSTSECTORS _T(" (floppy)"));
		const PImage image=vp.dos->image;
		const auto nSectorsOnDisk=vp.dos->formatBoot.GetCountOfAllSectors();
		pAction->SetProgressTarget( 2*nSectorsOnDisk+1 ); // "2" = this verification consists of two parts
		CMapWordToPtr sectorAffiliation[FDD_CYLINDERS_MAX*2];
		// - (1) composing a picture of which Sectors are actually affiliated to which Files
{		CActionProgress ap=pAction->CreateSubactionProgress( nSectorsOnDisk, nSectorsOnDisk );
		CFileManagerView::CFileList bfsFiles; // in-breath search of Files
		CFileManagerView::CFileList visitedDirectories;
		DWORD nSectorsVisited=0;
		DWORD nItemsVisited=0; // Files and Directories
		for( bfsFiles.AddTail((CDos::PFile)DOS_DIR_ROOT); bfsFiles.GetCount()>0; ){
			if (ap.Cancelled) return vp.CancelAll();
			const CDos::PFile file=bfsFiles.RemoveHead();
			// . retrieving File's FatPath
			const CDos::CFatPath fatPath( vp.dos, file );
			CDos::CFatPath::PItem pItem; DWORD nItems;
			fatPath.GetItems(pItem,nItems);
			if (!nItems)
				continue; // makes no sense to test a File that occupies no space on the disk
			// . recording Sectors affiliated to current File
			while (nItems--){
				if (ap.Cancelled) return vp.CancelAll();
				// : checking preconditions
				RCPhysicalAddress chs=pItem++->chs;
				if (chs.sectorId.cylinder!=chs.cylinder
					||
					chs.sectorId.side!=vp.dos->sideMap[chs.head]
					||
					chs.sectorId.sector<vp.dos->properties->firstSectorNumber || vp.dos->properties->firstSectorNumber+vp.dos->formatBoot.nSectors<=chs.sectorId.sector
					||
					chs.sectorId.lengthCode!=vp.dos->formatBoot.sectorLengthCode
				)
					return vp.TerminateAll(ERROR_NOT_SUPPORTED); // we shouldn't end-up here; if we do, it's a critical problem - either DOS not properly implemented or revision of this function needed
				// : recording affiliation
				sectorAffiliation[chs.GetTrackNumber()].SetAt( chs.sectorId.sector, file );
			}
			// . if the File is actually a Directory, processing it recurrently
			if (vp.dos->IsDirectory(file)){
				visitedDirectories.AddTail(file);
				if (const auto pdt=vp.dos->BeginDirectoryTraversal(file))
					while (const CDos::PFile subfile=pdt->GetNextFileOrSubdir())
						switch (pdt->entryType){
							case CDos::TDirectoryTraversal::SUBDIR:
								if (visitedDirectories.Contains( (CDos::PFile)vp.dos->GetDirectoryUid(subfile) )) // the Subdirectory has already been processed
									continue; // not processing it again
								//fallthrough
							case CDos::TDirectoryTraversal::FILE:
								bfsFiles.AddTail(subfile);
								if (++nItemsVisited==vp.dos->formatBoot.GetCountOfAllSectors())
									if (Utils::QuestionYesNo(VERIFICATION_WARNING_SIGNIFICANT_PROBLEM,MB_DEFBUTTON1))
										return vp.TerminateAndGoToNextAction(ERROR_CANCELLED);
								break;
							case CDos::TDirectoryTraversal::WARNING:
								continue; // silently ignoring FAT unreadability - warnings should be taken care of elsewhere
						}
			}
			// . informing on progress
			ap.UpdateProgress(  std::min( nSectorsVisited+=fatPath.GetNumberOfItems(), nSectorsOnDisk )  );
		}
}		// - (2) verifying the volume
{		CActionProgress ap=pAction->CreateSubactionProgress( nSectorsOnDisk, vp.dos->formatBoot.nCylinders );
		struct TDir sealed{
			CDos::PFile handle;
			bool createdAndSwitchedTo;
			WORD nLastKnownOccupiedSectors; // how many Sectors the Directory occupied last time we checked for its length
			TDir()
				: handle(nullptr) , createdAndSwitchedTo(false) , nLastKnownOccupiedSectors(0) {
			}
		} dir; // Directory to store Files addressing lost Sectors
		WORD fileId=0;
		const Utils::CByteIdentity sectorIdAndPositionIdentity;
		TPhysicalAddress chs;
		for( chs.cylinder=0; chs.cylinder<vp.dos->formatBoot.nCylinders; ap.UpdateProgress(chs.cylinder++) )
			for( chs.head=0; chs.head<vp.dos->formatBoot.nHeads; chs.head++ ){
				if (ap.Cancelled)
					return vp.CancelAll();
				// . getting the list of standard Sectors
				TSectorId bufferId[(TSector)-1];
				const TSector nSectors=vp.dos->GetListOfStdSectors( chs.cylinder, chs.head, bufferId );
				// . verifying whether officially Occupied or Reserved Sectors are actually affiliated to any File
				TSectorStatus statuses[(TSector)-1];
				vp.dos->GetSectorStatuses( chs.cylinder, chs.head, nSectors, bufferId, statuses );
				for( TSector s=0; s<nSectors; s++ )
					if (statuses[s]==TSectorStatus::OCCUPIED || statuses[s]==TSectorStatus::RESERVED){
						// : checking preconditions
						chs.sectorId=bufferId[s];
						if (chs.sectorId.cylinder!=chs.cylinder
							||
							chs.sectorId.side!=vp.dos->sideMap[chs.head]
							||
							chs.sectorId.sector<vp.dos->properties->firstSectorNumber || vp.dos->properties->firstSectorNumber+vp.dos->formatBoot.nSectors<=chs.sectorId.sector
							||
							chs.sectorId.lengthCode!=vp.dos->formatBoot.sectorLengthCode
						)
							return vp.TerminateAll(ERROR_NOT_SUPPORTED); // we shouldn't end-up here; if we do, it's a critical problem - either DOS not properly implemented or revision of this function needed
						// : verifying real affiliation
						CDos::PFile file;
						if (sectorAffiliation[chs.GetTrackNumber()].Lookup( chs.sectorId.sector, file ))
							continue; // ok - Sector is reported Occupied or Reserved and is really affiliated
						// : resolving the problem
						const CString msg=Utils::SimpleFormat( _T("Sector with %s is reported occupied but is unaffiliated to any file or directory"), chs.sectorId.ToString() );
						switch (vp.ConfirmFix( msg, _T("Sector will be represented as a file.") )){
							case IDCANCEL:
								return vp.CancelAll();
							case IDNO:
								continue;
						}
						// : if possible, creating a "LOSTnnnn" Directory to store temporary Files in
						CDos::CFatPath::PItem pItem; DWORD nItems;
						if (!dir.createdAndSwitchedTo){
							if (const auto dsm=vp.dos->pFileManager->pDirectoryStructureManagement){ // yes, the Dos supports Directories
								// > switching to the Root Directory
								if (const TStdWinError err=(vp.dos->*dsm->fnChangeCurrentDir)(DOS_DIR_ROOT))
									return vp.TerminateAll(err);
								// > creating a "LOSTnnnn" Directory in Root
								for( WORD dirId=1; dirId<10000; dirId++ ){
									const TStdWinError err=(vp.dos->*dsm->fnCreateSubdir)( CDos::CPathString().FormatLostItem8(dirId), FILE_ATTRIBUTE_DIRECTORY, dir.handle );
									if (err==ERROR_SUCCESS)
										break;
									else if (err!=ERROR_FILE_EXISTS)
										return vp.TerminateAll(err);
								}
								if (!dir.handle)
									return vp.TerminateAll(ERROR_CANNOT_MAKE);
								// > switching to the "LOSTnnnn" Directory
								vp.dos->pFileManager->SwitchToDirectory(dir.handle);
							}
							dir.createdAndSwitchedTo=true;
						}
						// : importing a Sector-long File, thus creating a valid directory entry
						file=nullptr; // don't affiliate the lost Sector with a temporary File, mark it as Empty in FAT straight away
						for( BYTE data[16384]; ++fileId<10000; ){
							const TStdWinError err=vp.dos->ImportFile( &CMemFile(data,sizeof(data)), vp.dos->formatBoot.sectorLength, CDos::CPathString().FormatLostItem8(fileId), 0, file );
							if (err==ERROR_SUCCESS)
								break;
							if (err!=ERROR_FILE_EXISTS){
								const CString msg=Utils::SimpleFormat( _T("Data of sector with %s cannot be put into a temporary file"), chs.sectorId.ToString() );
								const BYTE result=Utils::QuestionYesNoCancel( msg, MB_DEFBUTTON1, err, _T("Mark the sector as empty?") );
								if (result==IDCANCEL)
									return vp.CancelAll();
								if (result==IDYES){
									vp.dos->ModifyStdSectorStatus( chs, TSectorStatus::EMPTY );
									vp.fReport.CloseProblem(true);
								}
								break;
							}
						}
						// : registering affiliation of Sectors to the "LOSTxxxx" Directory
						if (dir.createdAndSwitchedTo){
							const CDos::CFatPath fatPath( vp.dos, dir.handle );
							if (const LPCTSTR err=fatPath.GetItems(pItem,nItems))
								return vp.TerminateAll(ERROR_OPEN_FAILED); // errors shouldn't occur at this moment, but just to be sure
							while (dir.nLastKnownOccupiedSectors<nItems){
								RCPhysicalAddress &chs=pItem[dir.nLastKnownOccupiedSectors++].chs;
								sectorAffiliation[chs.GetTrackNumber()].SetAt( chs.sectorId.sector, dir.handle );
							}
						}
						// : if temporary File couldn't be created, proceeding with the next Sector
						if (!file)
							continue;
						// : freeing up Sectors allocated to the temporary File
						const CDos::CFatPath fatPath( vp.dos, file );
						if (const LPCTSTR err=fatPath.GetItems(pItem,nItems))
							return vp.TerminateAll(ERROR_OPEN_FAILED); // errors shouldn't occur at this moment, but just to be sure
						if (nItems!=1)
							return vp.TerminateAll(ERROR_OPEN_FAILED); // errors shouldn't occur at this moment, but just to be sure
						vp.dos->ModifyStdSectorStatus( pItem->chs, TSectorStatus::EMPTY );
						// : associating the lost Sector with the temporary File
						pItem->chs=chs;
						vp.dos->ModifyFileFatPath( file, fatPath );
						sectorAffiliation[chs.GetTrackNumber()].SetAt( chs.sectorId.sector, file );
						vp.fReport.CloseProblem(true);
					}
			}
}		// - successfully verified
		return pAction->TerminateWithSuccess();
	}

	UINT AFX_CDECL TVerificationFunctions::WholeDiskSurfaceVerification_thread(PVOID pCancelableAction){
		// thread to verify if unreadable Empty Sectors on all volume Cylinders are marked in allocation table as Bad
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const CVerifyVolumeDialog::TParams &vp=*(CVerifyVolumeDialog::TParams *)pAction->GetParams();
		const PImage image=vp.dos->image;
		vp.fReport.OpenSection(SURFACE_VERIFICATION _T(" (empty parts of whole disk)"));
		pAction->SetProgressTarget( vp.dos->formatBoot.nCylinders );
		const Utils::CByteIdentity sectorIdAndPositionIdentity;
		TPhysicalAddress chs;
		for( chs.cylinder=vp.dos->GetFirstCylinderWithEmptySector(); chs.cylinder<vp.dos->formatBoot.nCylinders; pAction->UpdateProgress(chs.cylinder++) )
			for( chs.head=0; chs.head<vp.dos->formatBoot.nHeads; chs.head++ ){
				if (pAction->Cancelled) return vp.CancelAll();
				// . getting the list of standard Sectors
				TSectorId bufferId[(TSector)-1];
				const TSector nSectors=vp.dos->GetListOfStdSectors( chs.cylinder, chs.head, bufferId );
				// . determining whether the Track contains some Empty Sectors
				TSectorStatus statuses[(TSector)-1];
				vp.dos->GetSectorStatuses( chs.cylinder, chs.head, nSectors, bufferId, statuses );
				bool trackContainsEmptySectors=false; // assumption
				for( TSector s=0; s<nSectors; s++ )
					trackContainsEmptySectors|=statuses[s]==TSectorStatus::EMPTY;
				// . if the Track contains no Empty Sectors, proceeding with the next Track
				if (!trackContainsEmptySectors)
					continue;
				// . buffering Sectors from the same Track by the underlying Image, making them ready for IMMEDIATE usage
				image->BufferTrackData( chs.cylinder, chs.head, Revolution::ANY_GOOD, bufferId, sectorIdAndPositionIdentity, nSectors );
				// . determining healthiness of Empty Sectors
				for( TSector s=0; s<nSectors; s++ )
					if (statuses[s]==TSectorStatus::EMPTY){
						chs.sectorId=bufferId[s];
						if (!image->GetHealthySectorData(chs)){
							const CString msg=Utils::SimpleFormat( _T("On %s, empty sector with %s is bad but is not marked so in the FAT."), chs.GetTrackIdDesc(vp.dos->formatBoot.nHeads), chs.sectorId.ToString() );
							switch (vp.ConfirmFix(msg,_T("Future data loss at stake if not marked so."))){
								case IDCANCEL:
									return vp.CancelAll();
								case IDNO:
									continue;
							}
							vp.dos->ModifyStdSectorStatus( chs, TSectorStatus::BAD );
							vp.fReport.CloseProblem(true);
						}
					}
			}
		return pAction->TerminateWithSuccess();
	}
