#include "stdafx.h"

	struct TPatchParams sealed{
		const CDos *const dos;
		std::unique_ptr<CImage> source;
		const PImage target;
		TCylinder cylinderA,cylinderZ;
		THead nHeads;
		BYTE gap3;
		bool skipEmptySourceTracks;

		TPatchParams(PDos dos)
			// ctor
			: dos(dos) , target(dos->image)
			, cylinderA(0) , cylinderZ(0) , nHeads(1)
			, gap3( dos->properties->GetValidGap3ForMedium(dos->formatBoot.mediumType) )
			, skipEmptySourceTracks(true) {
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
				if (pAction->Cancelled) return ERROR_CANCELLED;
				// . scanning Source Track
				TSectorId bufferId[(TSector)-1];	WORD bufferLength[(TSector)-1];
				Codec::TType codec;
				const TSector nSectors=pp.source->ScanTrack(chs.cylinder,chs.head,&codec,bufferId,bufferLength);
				// . if Source Track empty, skipping it if commanded so
				if (!nSectors && pp.skipEmptySourceTracks)
					continue; // skipping empty Source Track
				// . reading Source Track
				PSectorData bufferSectorData[(TSector)-1];
				TFdcStatus bufferFdcStatus[(TSector)-1];
				PVOID dummyBuffer[(TSector)-1];
				pp.source->GetTrackData( chs.cylinder, chs.head, Revolution::ANY_GOOD, bufferId, sectorIdAndPositionIdentity, nSectors, bufferSectorData, bufferLength, bufferFdcStatus, (PLogTime)dummyBuffer ); // "DummyBuffer" = throw away any outputs
				for( TSector s=0; s<nSectors; s++ ){
					chs.sectorId=bufferId[s];
					bufferSectorData[s]=pp.source->GetSectorData( chs, s, Revolution::CURRENT, bufferLength+s, bufferFdcStatus+s );
				}
				// . formatting Target Track
				TStdWinError err=pp.target->FormatTrack( chs.cylinder, chs.head, pp.dos->formatBoot.codecType, nSectors, bufferId, bufferLength, bufferFdcStatus, pp.gap3, 0x00, pAction->Cancelled );
				if (err!=ERROR_SUCCESS)
terminateWithError:	return pAction->TerminateWithError(err);
				// . writing to Target Track
				pp.target->GetTrackData( chs.cylinder, chs.head, Revolution::ANY_GOOD, bufferId, sectorIdAndPositionIdentity, nSectors, (PSectorData *)dummyBuffer, (PWORD)dummyBuffer, (TFdcStatus *)dummyBuffer, (PLogTime)dummyBuffer ); // "DummyBuffer" = throw away any outputs
				for( BYTE s=0; s<nSectors; ){
					if (!bufferFdcStatus[s].DescribesMissingDam()){
						chs.sectorId=bufferId[s];
						if (const PSectorData targetData=pp.target->GetSectorData(chs,s,Revolution::ANY_GOOD)){
							::memcpy( targetData, bufferSectorData[s], bufferLength[s] );
							if (( err=pp.target->MarkSectorAsDirty(chs,s,bufferFdcStatus+s) )!=ERROR_SUCCESS)
								goto errorDuringWriting;
						}else{
							err=::GetLastError();
errorDuringWriting:			const CString msg=Utils::SimpleFormat( _T("Can't write to sector with %s on target %s"), chs.sectorId.ToString(), chs.GetTrackIdDesc(pp.target->GetHeadCount()) );
							switch (Utils::AbortRetryIgnore(msg,err,MB_DEFBUTTON2)){
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

	void CImage::Patch(){
		// patches this Image
		// - defining the Dialog
		class CPatchDialog sealed:public Utils::CRideDialog{
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
					SetDlgItemText( ID_FILE, ELLIPSIS );
				DDX_Text( pDX,	ID_CYLINDER,	(RCylinder)patchParams.cylinderA );
					if (patchParams.source)
						DDV_MinMaxUInt( pDX,patchParams.cylinderA, 0, patchParams.source->GetCylinderCount()-1 );
				DDX_Text( pDX,	ID_CYLINDER_N,	(RCylinder)patchParams.cylinderZ );
					if (patchParams.source)
						DDV_MinMaxUInt( pDX,patchParams.cylinderZ, patchParams.cylinderA, patchParams.source->GetCylinderCount()-1 );
				DDX_Text( pDX,	ID_HEAD,		patchParams.nHeads );
					if (patchParams.source)
						DDV_MinMaxUInt( pDX,patchParams.nHeads, 1, patchParams.source->GetHeadCount() );
				DDX_Text( pDX,	ID_GAP,			patchParams.gap3 );
				DDX_Check(pDX,	ID_TRACK,		patchParams.skipEmptySourceTracks );
				DDX_Check(pDX,	ID_PRIORITY,	realtimeThreadPriority );
			}
			afx_msg void OnPaint(){
				// painting
				// - base
				__super::OnPaint();
				// - painting curly brackets
				TCHAR buf[32];
				::wsprintf(buf,_T("%d cylinder(s)"),GetDlgItemInt(ID_CYLINDER_N)+1-GetDlgItemInt(ID_CYLINDER));
				WrapDlgItemsByClosingCurlyBracketWithText( ID_CYLINDER, ID_CYLINDER_N, buf, 0 );
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
								if (sourceImageProperties=app.DoPromptFileName( fileName, false, AFX_IDS_OPENFILE, 0, &CDsk5::Properties )){
									// . adjusting interactivity and updating values
									static constexpr WORD Controls[]={ ID_CYLINDER, ID_CYLINDER_N, ID_HEAD, ID_GAP, IDOK, 0 };
									if (EnableDlgItems( Controls, sourceImageProperties==&CDsk5::Properties )){
										// : interactivity
										patchParams.source.reset( sourceImageProperties->fnInstantiate(nullptr) ); // Null as buffer = one Image represents only one "device" whose name is known at compile-time
										patchParams.source->OnOpenDocument(fileName);
										patchParams.cylinderA=0, patchParams.cylinderZ=patchParams.source->GetCylinderCount()-1;
										patchParams.nHeads=std::min( patchParams.source->GetHeadCount(), patchParams.target->GetHeadCount() );
										DoDataExchange( &CDataExchange(this,FALSE) );
										// : compacting FileName in order to be displayable on the button
										SetDlgItemCompactPath( ID_FILE, fileName );
									}else
										SetDlgItemText( ID_FILE, ::lstrcpy(fileName,ELLIPSIS) );
									FocusDlgItem(IDOK);
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
						if (GetClickedHyperlinkId(lParam))
							Utils::NavigateToFaqInDefaultBrowser( _T("patch") );
						break;

				}
				return __super::WindowProc(msg,wParam,lParam);
			}
		public:
			TCHAR fileName[MAX_PATH];
			TPatchParams patchParams;
			CImage::PCProperties sourceImageProperties;
			bool realtimeThreadPriority;

			CPatchDialog(PDos dos)
				// ctor
				: Utils::CRideDialog(IDR_IMAGE_PATCH)
				, patchParams(dos) , sourceImageProperties(nullptr)
				, realtimeThreadPriority( dos->image->properties->IsRealDevice() ) {
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
