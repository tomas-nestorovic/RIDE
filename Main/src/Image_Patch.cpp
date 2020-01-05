#include "stdafx.h"

	struct TPatchParams sealed{
		const CDos *const dos;
		std::unique_ptr<CImage> source;
		const PImage target;
		TCylinder cylinderA,cylinderZ;
		THead nHeads;
		BYTE gap3;
		int skipEmptySourceTracks;

		TPatchParams(PDos dos)
			// ctor
			: dos(dos) , target(dos->image)
			, cylinderA(0) , cylinderZ(0) , nHeads(1)
			, gap3( dos->properties->GetValidGap3ForMedium(dos->formatBoot.mediumType) )
			, skipEmptySourceTracks(BST_CHECKED) {
		}
	};

	static UINT AFX_CDECL __patch_thread__(PVOID _pCancelableAction){
		// thread to copy Tracks
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		const TPatchParams &pp=*(TPatchParams *)pAction->GetParams();
		pAction->SetProgressTarget( pp.cylinderZ+1-pp.cylinderA );
		const Utils::CByteIdentity sectorIdAndPositionIdentity;
		TPhysicalAddress chs;
		for( chs.cylinder=pp.cylinderA; chs.cylinder<=pp.cylinderZ; pAction->UpdateProgress(++chs.cylinder-pp.cylinderA) )
			for( chs.head=0; chs.head<pp.nHeads; chs.head++ ){
				if (pAction->IsCancelled()) return ERROR_CANCELLED;
				// . scanning Source Track
				TSectorId bufferId[(TSector)-1];	WORD bufferLength[(TSector)-1];
				const TSector nSectors=pp.source->ScanTrack(chs.cylinder,chs.head,bufferId,bufferLength);
				// . if Source Track empty, skipping it if commanded so
				if (!nSectors && pp.skipEmptySourceTracks==BST_CHECKED)
					continue; // skipping empty Source Track
				// . reading Source Track
				PSectorData bufferSectorData[(TSector)-1];
				TFdcStatus bufferFdcStatus[(TSector)-1];
				pp.source->GetTrackData( chs.cylinder, chs.head, bufferId, sectorIdAndPositionIdentity, nSectors, true, bufferSectorData, bufferLength, bufferFdcStatus );
				for( TSector s=0; s<nSectors; s++ ){
					chs.sectorId=bufferId[s];
					bufferSectorData[s]=pp.source->GetSectorData( chs, s, false, bufferLength+s, bufferFdcStatus+s );
				}
				// . formatting Target Track
				TStdWinError err=pp.target->FormatTrack(chs.cylinder,chs.head,nSectors,bufferId,bufferLength,bufferFdcStatus,pp.gap3,0x00);
				if (err!=ERROR_SUCCESS)
terminateWithError:	return pAction->TerminateWithError(err);
				// . writing to Target Track
				PVOID dummyBuffer[(TSector)-1];
				pp.target->GetTrackData( chs.cylinder, chs.head, bufferId, sectorIdAndPositionIdentity, nSectors, true, (PSectorData *)dummyBuffer, (PWORD)dummyBuffer, (TFdcStatus *)dummyBuffer ); // "DummyBuffer" = throw away any outputs
				for( BYTE s=0; s<nSectors; ){
					if (!bufferFdcStatus[s].DescribesMissingDam()){
						chs.sectorId=bufferId[s]; WORD w;
						if (const PSectorData targetData=pp.target->GetSectorData(chs,s,true,&w,&TFdcStatus())){
							::memcpy( targetData, bufferSectorData[s], bufferLength[s] );
							if (( err=pp.target->MarkSectorAsDirty(chs,s,bufferFdcStatus+s) )!=ERROR_SUCCESS)
								goto errorDuringWriting;
						}else{
							err=::GetLastError();
errorDuringWriting:			TCHAR buf[80],tmp[30];
							::wsprintf(buf,_T("Cannot write to sector with %s on target Track %d"),chs.sectorId.ToString(tmp),chs.GetTrackNumber(pp.target->GetNumberOfFormattedSides(0)));
							switch (Utils::AbortRetryIgnore(buf,err,MB_DEFBUTTON2)){
								case IDABORT:	goto terminateWithError;
								case IDRETRY:	continue;
								case IDIGNORE:	break;
							}
						}
					}
					s++; // cannot include in the FOR clause - see Continue statement in the cycle
				}
			}
		return ERROR_SUCCESS;
	}

	void CImage::__patch__(){
		// patches this Image
		// - defining the Dialog
		class CPatchDialog sealed:public CDialog{
			void DoDataExchange(CDataExchange *pDX) override{
				// transferring data to and from controls
				if (pDX->m_bSaveAndValidate){
					// . FileName must be known
					pDX->PrepareEditCtrl(ID_FILE);
					if (!::lstrcmp(fileName,ELLIPSIS)){
						Utils::Information( _T("Target not specified.") );
						pDX->Fail();
					}else if (!patchParams.dos->image->GetPathName().Compare(fileName)){
						Utils::Information( _T("Target must not be the same as source.") );
						pDX->Fail();
					}
				}else
					GetDlgItem(ID_FILE)->SetWindowText(ELLIPSIS);
				DDX_Text( pDX,	ID_CYLINDER,	(RCylinder)patchParams.cylinderA );
					if (patchParams.source)
						DDV_MinMaxUInt( pDX,patchParams.cylinderA, 0, patchParams.source->GetCylinderCount()-1 );
				DDX_Text( pDX,	ID_CYLINDER_N,	(RCylinder)patchParams.cylinderZ );
					if (patchParams.source)
						DDV_MinMaxUInt( pDX,patchParams.cylinderZ, patchParams.cylinderA, patchParams.source->GetCylinderCount()-1 );
				DDX_Text( pDX,	ID_HEAD,		patchParams.nHeads );
					if (patchParams.source)
						DDV_MinMaxUInt( pDX,patchParams.nHeads, 1, patchParams.source->GetNumberOfFormattedSides(patchParams.cylinderA) );
				DDX_Text( pDX,	ID_GAP,			patchParams.gap3 );
				DDX_Check(pDX,	ID_TRACK,		patchParams.skipEmptySourceTracks );
				DDX_Check(pDX,	ID_PRIORITY,	realtimeThreadPriority );
			}
			afx_msg void OnPaint(){
				// painting
				// - base
				CDialog::OnPaint();
				// - painting curly brackets
				TCHAR buf[32];
				::wsprintf(buf,_T("%d cylinder(s)"),GetDlgItemInt(ID_CYLINDER_N)+1-GetDlgItemInt(ID_CYLINDER));
				Utils::WrapControlsByClosingCurlyBracketWithText( this, GetDlgItem(ID_CYLINDER), GetDlgItem(ID_CYLINDER_N), buf, 0 );
			}
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				switch (msg){
					case WM_PAINT:
						OnPaint();
						return 0;
					case WM_COMMAND:
						switch (wParam){
							case ID_FILE:{
								const TCHAR c=*fileName;
								*fileName='\0';
								if (app.__doPromptFileName__( fileName, false, AFX_IDS_OPENFILE, 0, &CDsk5::Properties )){
									// . determining SourceImageProperties
									sourceImageProperties=__determineTypeByExtension__(_tcsrchr(fileName,'.')); // Null <=> unknown container
									// . adjusting interactivity and updating values
									CWnd *const pBtnFile=GetDlgItem(ID_FILE);
									static const WORD Controls[]={ ID_CYLINDER, ID_CYLINDER_N, ID_HEAD, ID_GAP, IDOK, 0 };
									if (Utils::EnableDlgControls( m_hWnd, Controls, sourceImageProperties==&CDsk5::Properties )){
										// : interactivity
										patchParams.source.reset( sourceImageProperties->fnInstantiate() );
										patchParams.source->OnOpenDocument(fileName);
										patchParams.cylinderA=0, patchParams.cylinderZ=patchParams.source->GetCylinderCount()-1;
										patchParams.nHeads=std::min<>( patchParams.source->GetNumberOfFormattedSides(0), patchParams.target->GetNumberOfFormattedSides(0) );
										DoDataExchange( &CDataExchange(this,FALSE) );
										// : compacting FileName in order to be displayable on the button
										RECT r;
										pBtnFile->GetClientRect(&r);
										TCHAR buf[MAX_PATH];
										::PathCompactPath( CClientDC(pBtnFile), ::lstrcpy(buf,fileName), r.right-r.left );
										pBtnFile->SetWindowText(buf);
									}else
										pBtnFile->SetWindowText( ::lstrcpy(fileName,ELLIPSIS) );
									GetDlgItem(IDOK)->SetFocus();
								}else
									*fileName=c;
								break;
							}
							case MAKELONG(ID_CYLINDER,EN_CHANGE):
							case MAKELONG(ID_CYLINDER_N,EN_CHANGE):
								Invalidate();
								break;
						}
						break;
					case WM_NOTIFY:
						if (((LPNMHDR)lParam)->code==NM_CLICK){
							TCHAR url[200];
							Utils::NavigateToUrlInDefaultBrowser( Utils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_patch.html"),url) );
						}
						break;

				}
				return CDialog::WindowProc(msg,wParam,lParam);
			}
		public:
			TCHAR fileName[MAX_PATH];
			TPatchParams patchParams;
			CImage::PCProperties sourceImageProperties;
			int realtimeThreadPriority;

			CPatchDialog(PDos dos)
				// ctor
				: CDialog(IDR_IMAGE_PATCH)
				, patchParams(dos) , sourceImageProperties(nullptr)
				, realtimeThreadPriority( dos->image->properties==&CFDD::Properties ) {
				::lstrcpy( fileName, ELLIPSIS );
			}
		} d(GetActive()->dos);
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK)
			if (const TStdWinError err=	CBackgroundActionCancelable(
											__patch_thread__,
											&d.patchParams,
											d.realtimeThreadPriority ? THREAD_PRIORITY_TIME_CRITICAL : THREAD_PRIORITY_NORMAL
										).Perform()
			)
				Utils::FatalError(_T("Cannot patch"),err);
			else
				Utils::Information(_T("Patched successfully."));
	}
