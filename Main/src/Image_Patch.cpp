#include "stdafx.h"

	struct TPatchParams sealed{
		const CDos *const dos;
		PImage source;
		const PImage target;
		TCylinder cylinderA,cylinderZ;
		THead nHeads;
		BYTE gap3;
		int skipEmptySourceTracks;

		TPatchParams(PDos dos)
			// ctor
			: dos(dos) , source(NULL) , target(dos->image)
			, cylinderA(0) , cylinderZ(0) , nHeads(1) , gap3(FDD_SECTOR_GAP3_STD)
			, skipEmptySourceTracks(BST_CHECKED) {
		}
		~TPatchParams(){
			// dtor
			if (source)
				delete source;
		}
	};

	static UINT AFX_CDECL __patch_thread__(PVOID _pCancelableAction){
		// thread to copy Tracks
		TBackgroundActionCancelable *pAction=(TBackgroundActionCancelable *)_pCancelableAction;
		TPatchParams &pp=*(TPatchParams *)pAction->fnParams;
		TPhysicalAddress chs;
		for( chs.cylinder=pp.cylinderA; chs.cylinder<=pp.cylinderZ; pAction->UpdateProgress(++chs.cylinder-pp.cylinderA) )
			for( chs.head=0; chs.head<pp.nHeads; chs.head++ ){
				if (!pAction->bContinue) return ERROR_CANCELLED;
				// . scanning Source Track
				TSectorId bufferId[(TSector)-1];	WORD bufferLength[(TSector)-1];
				const TSector nSectors=pp.source->ScanTrack(chs.cylinder,chs.head,bufferId,bufferLength);
				// . if Source Track empty, skipping it if commanded so
				if (!nSectors && pp.skipEmptySourceTracks==BST_CHECKED)
					continue; // skipping empty Source Track
				// . reading Source Track
				#pragma pack(1)
				struct TSourceSector sealed{
					PSectorData data;
					WORD dataLength;
				} sourceSectors[(TSector)-1],*pSrcSector=sourceSectors;
				TFdcStatus bufferFdcStatus[(TSector)-1],*pFdcStatus=bufferFdcStatus;
				for( TSector s=0; s<nSectors; s++,pSrcSector++,pFdcStatus++ ){
					chs.sectorId=bufferId[s];
					pSrcSector->data=pp.source->GetSectorData( chs, s, false, &pSrcSector->dataLength, pFdcStatus );
				}
				// . formatting Target Track
				TStdWinError err=pp.target->FormatTrack(chs.cylinder,chs.head,nSectors,bufferId,bufferLength,bufferFdcStatus,pp.gap3,0x00);
				if (err!=ERROR_SUCCESS)
terminateWithError:	return pAction->TerminateWithError(err);
				// . writing to Target Track
				pSrcSector=sourceSectors, pFdcStatus=bufferFdcStatus;
				for( BYTE s=0; s<nSectors; ){
					if (!pFdcStatus->DescribesMissingDam()){
						chs.sectorId=bufferId[s]; WORD w;
						if (const PSectorData targetData=pp.target->GetSectorData(chs,s,true,&w,&TFdcStatus())){
							::memcpy( targetData, pSrcSector->data, bufferLength[s] );
							if (( err=pp.target->MarkSectorAsDirty(chs,s,pFdcStatus) )!=ERROR_SUCCESS)
								goto errorDuringWriting;
						}else{
							err=::GetLastError();
errorDuringWriting:			TCHAR buf[80],tmp[30];
							::wsprintf(buf,_T("Cannot write to sector with %s on target Track %d"),chs.sectorId.ToString(tmp),chs.GetTrackNumber(pp.target->GetNumberOfFormattedSides(0)));
							switch (TUtils::AbortRetryIgnore(buf,err,MB_DEFBUTTON2)){
								case IDABORT:	goto terminateWithError;
								case IDRETRY:	continue;
								case IDIGNORE:	break;
							}
						}
					}
					s++, pSrcSector++, pFdcStatus++; // cannot include in the FOR clause - see Continue statement in the cycle
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
						TUtils::Information( _T("Target not specified.") );
						pDX->Fail();
					}else if (!patchParams.dos->image->GetPathName().Compare(fileName)){
						TUtils::Information( _T("Target must not be the same as source.") );
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
			}
			afx_msg void OnPaint(){
				// painting
				// - base
				CDialog::OnPaint();
				// - painting curly brackets
				TCHAR buf[32];
				::wsprintf(buf,_T("%d cylinder(s)"),GetDlgItemInt(ID_CYLINDER_N)+1-GetDlgItemInt(ID_CYLINDER));
				TUtils::WrapControlsByClosingCurlyBracketWithText( this, GetDlgItem(ID_CYLINDER), GetDlgItem(ID_CYLINDER_N), buf, 0 );
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
									// . adjusting interactivity
									const bool controlsEnabled=sourceImageProperties==&CDsk5::Properties;
									GetDlgItem(ID_CYLINDER)->EnableWindow(controlsEnabled), GetDlgItem(ID_CYLINDER_N)->EnableWindow(controlsEnabled);
									GetDlgItem(ID_HEAD)->EnableWindow(controlsEnabled);
									GetDlgItem(ID_GAP)->EnableWindow(controlsEnabled);
									CWnd *const pBtnOk=GetDlgItem(IDOK);
									pBtnOk->EnableWindow(controlsEnabled), pBtnOk->SetFocus();
									// . updating values
									CWnd *const pBtnFile=GetDlgItem(ID_FILE);
									if (controlsEnabled){
										// : interactivity
										if (patchParams.source)
											delete patchParams.source;
										( patchParams.source=sourceImageProperties->fnInstantiate() )->OnOpenDocument(fileName);
										patchParams.cylinderA=0, patchParams.cylinderZ=patchParams.source->GetCylinderCount()-1;
										patchParams.nHeads=min(patchParams.source->GetNumberOfFormattedSides(0),patchParams.target->GetNumberOfFormattedSides(0));
										DoDataExchange( &CDataExchange(this,FALSE) );
										// : compacting FileName in order to be displayable on the button
										RECT r;
										pBtnFile->GetClientRect(&r);
										TCHAR buf[MAX_PATH];
										::PathCompactPath( CClientDC(pBtnFile), ::lstrcpy(buf,fileName), r.right-r.left );
										pBtnFile->SetWindowText(buf);
									}else
										pBtnFile->SetWindowText( ::lstrcpy(fileName,ELLIPSIS) );
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
							TUtils::NavigateToUrlInDefaultBrowser( TUtils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_patch.html"),url) );
						}
						break;

				}
				return CDialog::WindowProc(msg,wParam,lParam);
			}
		public:
			TCHAR fileName[MAX_PATH];
			TPatchParams patchParams;
			CImage::PCProperties sourceImageProperties;

			CPatchDialog(PDos dos)
				// ctor
				: CDialog(IDR_IMAGE_PATCH)
				, patchParams(dos) , sourceImageProperties(NULL) {
				::lstrcpy( fileName, ELLIPSIS );
			}
		} d(__getActive__()->dos);
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			const TStdWinError err=	TBackgroundActionCancelable(
										__patch_thread__,
										&d.patchParams
									).CarryOut(d.patchParams.cylinderZ+1-d.patchParams.cylinderA);
			if (err==ERROR_SUCCESS)
				TUtils::Information(_T("Patched successfully."));
			else
				TUtils::FatalError(_T("Cannot patch"),err);
		}
	}
