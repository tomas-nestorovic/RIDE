#include "stdafx.h"

	CVerifyVolumeDialog::TParams::TParams(CDos *dos,const TVerificationFunctions &rvf)
		// ctor
		: dos(dos)
		, verifyBootSector(BST_UNCHECKED) , verifyFat(BST_UNCHECKED) , verifyFilesystem(BST_UNCHECKED)
		, verifyVolumeSurface(BST_UNCHECKED)
		, repairStyle(0)
		, verificationFunctions(rvf) {
	}

	BYTE CVerifyVolumeDialog::TParams::ConfirmFix(LPCTSTR problemDesc,LPCTSTR problemSolutionSuggestion) const{
		// presents the occured problem along with suggestion how to solve it, and returns user's reaction
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
		}
	}









	CVerifyVolumeDialog::CVerifyVolumeDialog(CDos *dos,const TVerificationFunctions &rvf)
		// ctor
		// - base
		: CDialog(IDR_DOS_VERIFY)
		// - initialization
		, params(dos,rvf)
		, nOptionsChecked(0) {
	}









	void CVerifyVolumeDialog::DoDataExchange(CDataExchange *pDX){
		// exchange of data from and to controls
		DDX_Text( pDX, ID_DOS, CString(params.dos->properties->name) );
		DDX_Check( pDX, ID_BOOT,	params.verifyBootSector );
			Utils::EnableDlgControl( m_hWnd, ID_BOOT, params.verificationFunctions.fnBootSector );
		DDX_Check( pDX, ID_FAT,		params.verifyFat );
			Utils::EnableDlgControl( m_hWnd, ID_FAT, params.verificationFunctions.fnFatValues||params.verificationFunctions.fnFatCrossedFiles||params.verificationFunctions.fnFatLostAllocUnits );
		DDX_Check( pDX, ID_FILE1,	params.verifyFilesystem );
			Utils::EnableDlgControl( m_hWnd, ID_FILE1, params.verificationFunctions.fnFilesystem );
		DDX_Check( pDX, ID_IMAGE,	params.verifyVolumeSurface );
			Utils::EnableDlgControl( m_hWnd, ID_IMAGE, params.verificationFunctions.fnVolumeSurface );
		DDX_CBIndex( pDX, ID_REPAIR, params.repairStyle );
	}

	LRESULT CVerifyVolumeDialog::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_NOTIFY:
				// processing notification from child control
				if (((LPNMHDR)lParam)->code==NM_CLICK){
					CheckDlgButton( ID_BOOT, BST_CHECKED&&Utils::IsDlgControlEnabled(m_hWnd,ID_BOOT) );
					CheckDlgButton( ID_FAT, BST_CHECKED&&Utils::IsDlgControlEnabled(m_hWnd,ID_FAT) );
					CheckDlgButton( ID_FILE1, BST_CHECKED&&Utils::IsDlgControlEnabled(m_hWnd,ID_FILE1) );
					wParam=ID_BOOT; // for the below fallthrough
					//fallthrough
				}else
					break;
			case WM_COMMAND:
				// processing a command
				if (ID_BOOT<=wParam && wParam<=ID_IMAGE){
					// one of check-boxes (un)ticked
					nOptionsChecked=(IsDlgButtonChecked(ID_BOOT)==BST_CHECKED)
									+
									(IsDlgButtonChecked(ID_FAT)==BST_CHECKED)
									+
									(IsDlgButtonChecked(ID_FILE1)==BST_CHECKED)
									+
									(IsDlgButtonChecked(ID_IMAGE)==BST_CHECKED);
					GetDlgItem(IDOK)->EnableWindow( nOptionsChecked>0 );
				}
				break;
			case WM_PAINT:{
				// drawing
				// . base
				const LRESULT result=__super::WindowProc(msg,wParam,lParam);
				// . wrapping full integrity check options in a curly bracket
				Utils::WrapControlsByClosingCurlyBracketWithText( this, GetDlgItem(ID_BOOT), GetDlgItem(ID_FILE1), nullptr, 0 );
				return result;
			}
		}
		return __super::WindowProc(msg,wParam,lParam);
	}












	#define WARNING_CROSSED_FILES	_T("Kept cross-linked, changes in one will affect the other")

	UINT AFX_CDECL TVerificationFunctions::FloppyCrossLinkedFilesVerification_thread(PVOID pCancelableAction){
		// thread to find and separate cross-linked Files on current volume
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const CVerifyVolumeDialog::TParams &vp=*(CVerifyVolumeDialog::TParams *)pAction->GetParams();
		// - checking preconditions (usable only for floppies)
		if (vp.dos->formatBoot.nCylinders>FDD_CYLINDERS_MAX
			||
			vp.dos->formatBoot.nHeads>2
			||
			vp.dos->formatBoot.clusterSize!=1
		)
			return pAction->TerminateWithError(ERROR_NOT_SUPPORTED);
		// - verifying the volume
		const PImage image=vp.dos->image;
		pAction->SetProgressTarget( vp.dos->formatBoot.nCylinders );
		CMapWordToPtr sectorOccupation[FDD_CYLINDERS_MAX*2];
		CFileManagerView::TFileList bfsFiles; // breadth first search, searching through Directories in breadth
		CFileManagerView::TFileList visitedDirectories;
		for( bfsFiles.AddTail((CDos::PFile)DOS_DIR_ROOT); bfsFiles.GetCount()>0; ){
			if (pAction->IsCancelled()) return ERROR_CANCELLED;
			const CDos::PFile file=bfsFiles.RemoveHead();
			// . checking that the File's FatPath is unique
			const CDos::CFatPath fatPath( vp.dos, file );
			CDos::CFatPath::PItem pItem; DWORD nItems;
			if (const LPCTSTR err=fatPath.GetItems(pItem,nItems))
				return pAction->TerminateWithError(ERROR_OPEN_FAILED); // problems in FatPath must be taken care for elsewhere
			if (!nItems)
				continue; // makes no sense to test a File that occupies no space on the disk
			const TCylinder firstFileCylinder=pItem->chs.cylinder;
			bool fatModified=false; // assumption (the File is not cross-linked)
			for( bool askedToAutoFixProblem=false; nItems--; pItem++ ){
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
					return pAction->TerminateWithError(ERROR_NOT_SUPPORTED);
				// : verifying that the Sector is not used by another File
				CDos::PFile sectorOccupiedByFile;
				if (sectorOccupation[chs.GetTrackNumber()].Lookup( chs.sectorId.sector, sectorOccupiedByFile )){
					// | asking whether or not to fix this problem
					if (!askedToAutoFixProblem){
						CString msg;
						msg.Format( _T("%s \"%s\" is cross-linked with %s \"%s\""), vp.dos->IsDirectory(file)?_T("Directory"):_T("File"), (LPCTSTR)vp.dos->GetFilePresentationNameAndExt(file), vp.dos->IsDirectory(sectorOccupiedByFile)?_T("directory"):_T("file"), (LPCTSTR)vp.dos->GetFilePresentationNameAndExt(sectorOccupiedByFile) );
						switch (vp.ConfirmFix( msg, WARNING_CROSSED_FILES )){
							case IDCANCEL:
								return pAction->TerminateWithError(ERROR_CANCELLED);
							case IDNO:
								goto nextFile;
						}
						askedToAutoFixProblem=true; // don't ask for the rest of this File's Sectors
					}
					// | reading the cross-linked Sector
					PCSectorData crossLinkedSectorData;
					while (!( crossLinkedSectorData=image->GetHealthySectorData(chs) )){
						CString msg;
						msg.Format( _T("Sector %s in \"%s\" %s is unreadable"), (LPCTSTR)chs.sectorId.ToString(), (LPCTSTR)vp.dos->GetFilePresentationNameAndExt(file), vp.dos->IsDirectory(file)?_T("directory"):_T("file") );
						switch (Utils::CancelRetryContinue( msg, ::GetLastError(), MB_DEFBUTTON2, WARNING_CROSSED_FILES )){
							case IDCANCEL:
								return pAction->TerminateWithError(ERROR_CANCELLED);
							case IDCONTINUE:
								goto nextFile;
						}
					}
					// | finding an empty healthy Sector in the volume
					while (const TStdWinError err=vp.dos->GetFirstEmptyHealthySector(true,pItem->chs)){
						CString msg;
						msg.Format( _T("\"%s\" cannot be fixed"), (LPCTSTR)vp.dos->GetFilePresentationNameAndExt(file) );
						switch (Utils::CancelRetryContinue( msg, err, MB_DEFBUTTON1 )){
							case IDCANCEL:
								return pAction->TerminateWithError(ERROR_CANCELLED);
							case IDCONTINUE:
								goto nextFile;
						}
					}
					// | reserving the found empty healthy Sector by marking it Bad so that it's excluded from available empty Sectors
					if (!vp.dos->ModifyStdSectorStatus( pItem->chs, CDos::TSectorStatus::BAD ))
						return pAction->TerminateWithError(ERROR_NOT_SUPPORTED); // DOS unable to reserve the above Sector
					// | copying data to the found empty healthy Sector
					::memcpy(	image->GetHealthySectorData(pItem->chs),
								crossLinkedSectorData,
								vp.dos->formatBoot.sectorLength
							);
					image->MarkSectorAsDirty(pItem->chs);
					fatModified=true;
				}
				// : recording that given Sector is used by current File
				sectorOccupation[chs.GetTrackNumber()].SetAt( chs.sectorId.sector, file ); // PhysicalAddress might have been fixed above, hence using actual values refered by Chs
			}
			// . writing File's Modified FatPath back to FAT
			if (fatModified)
				vp.dos->ModifyFileFatPath( file, fatPath ); // all FAT Sectors by previous actions guaranteed to be readable
nextFile:	// . if the File is actually a Directory, processing it recurrently
			if (vp.dos->IsDirectory(file)){
				if (const auto pdt=vp.dos->BeginDirectoryTraversal(file))
					while (const CDos::PFile subfile=pdt->GetNextFileOrSubdir())
						switch (pdt->entryType){
							case CDos::TDirectoryTraversal::SUBDIR:
								if (visitedDirectories.Find(subfile)!=nullptr) // the Subdirectory has already been processed
									continue; // not processing it again
								//fallthrough
							case CDos::TDirectoryTraversal::FILE:
								bfsFiles.AddTail(subfile);
								break;
							case CDos::TDirectoryTraversal::WARNING:
								return pAction->TerminateWithError(pdt->warning);
						}
				visitedDirectories.AddTail(file);
			}
			// . informing on progress
			pAction->UpdateProgress( firstFileCylinder );
		}
		pAction->UpdateProgressFinished();
		return ERROR_SUCCESS;
	}

	UINT AFX_CDECL TVerificationFunctions::WholeDiskSurfaceVerification_thread(PVOID pCancelableAction){
		// thread to verify if unreadable Empty Sectors on all volume Cylinders are marked in allocation table as Bad
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const CVerifyVolumeDialog::TParams &vp=*(CVerifyVolumeDialog::TParams *)pAction->GetParams();
		const PImage image=vp.dos->image;
		pAction->SetProgressTarget( vp.dos->formatBoot.nCylinders );
		const auto sectorIdAndPositionIdentity=Utils::CByteIdentity();
		TPhysicalAddress chs;
		for( chs.cylinder=vp.dos->GetFirstCylinderWithEmptySector(); chs.cylinder<vp.dos->formatBoot.nCylinders; chs.cylinder++ )
			for( chs.head=0; chs.head<vp.dos->formatBoot.nHeads; chs.head++ ){
				if (pAction->IsCancelled()) return ERROR_CANCELLED;
				// . getting the list of standard Sectors
				TSectorId bufferId[(TSector)-1];
				const TSector nSectors=vp.dos->GetListOfStdSectors( chs.cylinder, chs.head, bufferId );
				// . determining whether the Track contains some Empty Sectors
				CDos::TSectorStatus statuses[(TSector)-1];
				vp.dos->GetSectorStatuses( chs.cylinder, chs.head, nSectors, bufferId, statuses );
				bool trackContainsEmptySectors=false; // assumption
				for( TSector s=0; s<nSectors; s++ )
					trackContainsEmptySectors|=statuses[s]==CDos::TSectorStatus::EMPTY;
				// . if the Track contains no Empty Sectors, proceeding with the next Track
				if (!trackContainsEmptySectors)
					continue;
				// . buffering Sectors from the same Track by the underlying Image, making them ready for IMMEDIATE usage
				image->BufferTrackData( chs.cylinder, chs.head, bufferId, sectorIdAndPositionIdentity, nSectors, true );
				// . determining healthiness of Empty Sectors
				for( TSector s=0; s<nSectors; s++ )
					if (statuses[s]==CDos::TSectorStatus::EMPTY){
						chs.sectorId=bufferId[s];
						if (!image->GetHealthySectorData(chs)){
							TCHAR buf[120];
							::wsprintf( buf, _T("On Track %d, empty sector with %s is bad but is not marked so in the FAT."), chs.GetTrackNumber(vp.dos->formatBoot.nHeads), (LPCTSTR)chs.sectorId.ToString() );
							switch (vp.ConfirmFix(buf,_T("Future data loss at stake if not marked so."))){
								case IDCANCEL:
									return pAction->TerminateWithError(ERROR_CANCELLED);
								case IDNO:
									continue;
							}
							vp.dos->ModifyStdSectorStatus( chs, CDos::TSectorStatus::BAD );
						}
					}
				pAction->UpdateProgress(chs.cylinder);
			}
		pAction->UpdateProgressFinished();
		return ERROR_SUCCESS;
	}
