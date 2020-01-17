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












	UINT AFX_CDECL TVerificationFunctions::WholeDiskSurfaceVerification_thread(PVOID pCancelableAction){
		// thread to verify if unreadable Empty Sectors on all volume Cylinders are marked in allocation table as Bad
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const CVerifyVolumeDialog::TParams &vp=*(CVerifyVolumeDialog::TParams *)pAction->GetParams();
		const PImage image=vp.dos->image;
		pAction->SetProgressTarget( image->GetCylinderCount() );
		const auto sectorIdAndPositionIdentity=Utils::CByteIdentity();
		TPhysicalAddress chs;
		for( chs.cylinder=0; chs.cylinder<image->GetCylinderCount(); chs.cylinder++ )
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
							TCHAR buf[120], bufId[50];
							::wsprintf( buf, _T("On Track %d, empty sector with %s is bad but is not marked so in the FAT."), chs.GetTrackNumber(vp.dos->formatBoot.nHeads), chs.sectorId.ToString(bufId) );
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
