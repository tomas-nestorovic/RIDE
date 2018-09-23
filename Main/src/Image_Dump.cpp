#include "stdafx.h"

	#define INI_DUMP	_T("Dump")

	struct TDumpParams sealed{
		#pragma pack(1)
		typedef const struct TSourceSectorError sealed{
			TSectorId id;
			TFdcStatus fdcStatus;
		} *PCSourceSectorError;

		const CDos *const dos;
		TMedium::TType mediumType;
		const PImage source;
		PImage target;
		bool formatJustBadTracks;
		TCylinder cylinderA,cylinderZ;
		THead nHeads;
		BYTE gap3,fillerByte;
		#pragma pack(1)
		const struct TSourceTrackErrors sealed{
			TCylinder cyl;
			THead head;
			const TSourceTrackErrors *pNextErroneousTrack;
			TSector nErroneousSectors;
			TSourceSectorError erroneousSectors[1];
		} *pOutErroneousTracks;

		TDumpParams(PDos _dos)
			// ctor
			: dos(_dos)
			, source(dos->image) , target(NULL)
			, formatJustBadTracks(false)
			, gap3(FDD_SECTOR_GAP3_STD) , fillerByte(dos->properties->sectorFillerByte)
			, cylinderA(0) , cylinderZ(source->GetCylinderCount()-1)
			, nHeads(source->GetNumberOfFormattedSides(0))
			, pOutErroneousTracks(NULL) {
		}

		~TDumpParams(){
			// dtor
			if (target){
				target->dos=NULL; // to not also destroy the DOS
				delete target;
			}
		}

		void __exportErroneousTracksToHtml__(CFile &fHtml) const{
			// exports SourceTrackErrors to given HTML file
			TCHAR buffer[128];
			TUtils::WriteToFile(fHtml,_T("<html><head><style>body,td{font-size:13pt;margin:24pt}table{border:1pt solid black;spacing:10pt}td{vertical-align:top}td.caption{font-size:14pt;background:silver}</style></head><body>"));
				TUtils::WriteToFile(fHtml,_T("<h3>Overview</h3>"));
					if (pOutErroneousTracks){
						TUtils::WriteToFile(fHtml,_T("<table><tr><td class=caption>Error</td><td class=caption>Count</td></tr>"));
							union{
								BYTE bRegisters[2];
								WORD wRegisters;
							} fdc;
							for( fdc.wRegisters=1; fdc.wRegisters; fdc.wRegisters<<=1 ){
								const TFdcStatus sr(fdc.bRegisters[0],fdc.bRegisters[1]);
								if (sr.IsWithoutError())
									continue; 
								int nErrorOccurences=0;
								for( const TSourceTrackErrors *pErroneousTrack=pOutErroneousTracks; pErroneousTrack; pErroneousTrack=pErroneousTrack->pNextErroneousTrack ){
									PCSourceSectorError psse=pErroneousTrack->erroneousSectors;
									for( BYTE n=pErroneousTrack->nErroneousSectors; n; n-- )
										nErrorOccurences+=(psse++->fdcStatus.ToWord()&fdc.wRegisters)!=0;
								}
								if (nErrorOccurences){
									LPCTSTR bitDescriptions[3]; // 3 = prave jedna Chyba a dvakrat Null
									sr.GetDescriptionsOfSetBits(bitDescriptions);
									TUtils::WriteToFile(fHtml,_T("<tr><td>"));
										TUtils::WriteToFile(fHtml,(LPCTSTR)((UINT)bitDescriptions[0]|(UINT)bitDescriptions[1]|(UINT)bitDescriptions[2]));
									TUtils::WriteToFile(fHtml,_T("</td><td>"));
										TUtils::WriteToFile(fHtml,nErrorOccurences);
									TUtils::WriteToFile(fHtml,_T("</td></tr>"));
								}
							}
						TUtils::WriteToFile(fHtml,_T("</table>"));
					}else
						TUtils::WriteToFile(fHtml,_T("No errors occurred."));
				TUtils::WriteToFile(fHtml,_T("<h3>Details</h3>"));
					if (pOutErroneousTracks){
						TUtils::WriteToFile(fHtml,_T("<table><tr><td class=caption width=120>Track</td><td class=caption>Erroneous Sectors</td></tr>"));
							for( const TSourceTrackErrors *pErroneousTrack=pOutErroneousTracks; pErroneousTrack; pErroneousTrack=pErroneousTrack->pNextErroneousTrack ){
								TUtils::WriteToFile(fHtml,_T("<tr><td>"));
									::wsprintf( buffer, _T("Cyl %d, Head %d"), pErroneousTrack->cyl, pErroneousTrack->head );
									TUtils::WriteToFile(fHtml,buffer);
								TUtils::WriteToFile(fHtml,_T("</td><td><ul>"));
									PCSourceSectorError psse=pErroneousTrack->erroneousSectors;
									for( BYTE n=pErroneousTrack->nErroneousSectors; n; n--,psse++ ){
										TUtils::WriteToFile(fHtml,_T("<li>"));
											::wsprintf( buffer, _T("<b>%s</b>. "), psse->id.ToString(buffer+40) );
											TUtils::WriteToFile(fHtml,buffer);
											LPCTSTR bitDescriptions[10],*pDesc=bitDescriptions;
											psse->fdcStatus.GetDescriptionsOfSetBits(bitDescriptions);
											::wsprintf( buffer, _T("<i>SR1</i> (0x%02X): "), psse->fdcStatus.reg1 );
											TUtils::WriteToFile(fHtml,buffer);
											if (*pDesc){
												TUtils::WriteToFile(fHtml,*pDesc++);
												while (*pDesc){
													TUtils::WriteToFile(fHtml,_T(", "));
													TUtils::WriteToFile(fHtml,*pDesc++);
												}
											}else
												TUtils::WriteToFile(fHtml,_T("No error"));
											TUtils::WriteToFile(fHtml,_T("."));
											pDesc++; // skipping Null that terminates the list of bits set in Register 1
											::wsprintf( buffer, _T(" <i>SR2</i> (0x%02X): "), psse->fdcStatus.reg2 );
											TUtils::WriteToFile(fHtml,buffer);
											if (*pDesc){
												TUtils::WriteToFile(fHtml,*pDesc++);
												while (*pDesc){
													TUtils::WriteToFile(fHtml,_T(", "));
													TUtils::WriteToFile(fHtml,*pDesc++);
												}
											}else
												TUtils::WriteToFile(fHtml,_T("No error"));
											TUtils::WriteToFile(fHtml,_T("."));
										TUtils::WriteToFile(fHtml,_T("</li>"));
									}
								TUtils::WriteToFile(fHtml,_T("</ul></td></tr>"));
							}
						TUtils::WriteToFile(fHtml,_T("</table>"));
					}else
						TUtils::WriteToFile(fHtml,_T("None."));
			TUtils::WriteToFile(fHtml,_T("</body></html>"));
		}
	};

	#define ACCEPT_OPTIONS_COUNT	4
	#define ACCEPT_ERROR_ID			IDOK

	#define NO_ERROR	_T("- no error\n")

	static UINT AFX_CDECL __dump_thread__(PVOID _pCancelableAction){
		// vlakno kopirovani Stop
		TBackgroundActionCancelable *const pAction=(TBackgroundActionCancelable *)_pCancelableAction;
		TDumpParams &dp=*(TDumpParams *)pAction->fnParams;
		// - setting geometry to the TargetImage
		TSectorId bufferId[(TSector)-1];	WORD bufferLength[(TSector)-1];
		TSector nSectors=dp.source->ScanTrack(0,0,bufferId,bufferLength);
		const TFormat targetGeometry={ dp.mediumType, dp.cylinderZ+1, dp.nHeads, nSectors, dp.dos->formatBoot.sectorLengthCode, *bufferLength, 1 };
		TStdWinError err=dp.target->SetMediumTypeAndGeometry( &targetGeometry, dp.dos->sideMap, dp.dos->properties->firstSectorNumber );
		if (err!=ERROR_SUCCESS)
terminateWithError:
			return pAction->TerminateWithError(err);
		// - dumping
		const TDumpParams::TSourceTrackErrors **ppSrcTrackErrors=&dp.pOutErroneousTracks;
		#pragma pack(1)
		struct TParams sealed{
			TPhysicalAddress chs;
			TTrack track;
			struct{
				WORD automaticallyAcceptedErrors;
				bool remainingErrorsOnTrack;
			} acceptance;
		} p;
		::ZeroMemory(&p,sizeof(p));
		for( p.chs.cylinder=dp.cylinderA; p.chs.cylinder<=dp.cylinderZ; pAction->UpdateProgress(++p.chs.cylinder-dp.cylinderA) )
			for( p.chs.head=0; p.chs.head<dp.nHeads; p.chs.head++ ){
				if (!pAction->bContinue) return ERROR_CANCELLED;
				p.track=p.chs.GetTrackNumber(dp.nHeads);
				// . scanning Source Track
//TUtils::Information("scanning Source Track");
				nSectors=dp.source->ScanTrack(p.chs.cylinder,p.chs.head,bufferId,bufferLength);
				// . reading Source Track
//TUtils::Information("reading Source Track");
				#pragma pack(1)
				struct{
					TSector n;
					TDumpParams::TSourceSectorError buffer[(TSector)-1];
				} erroneousSectors;
				erroneousSectors.n=0;
				#pragma pack(1)
				struct TSourceSector sealed{
					PSectorData data;
					WORD dataLength;
				} sourceSectors[(TSector)-1],*pSrcSector=sourceSectors;
				TFdcStatus bufferFdcStatus[(TSector)-1],*pFdcStatus=bufferFdcStatus;
				for( TSector s=0; s<nSectors; ){
					// : reading SourceSector
					p.chs.sectorId=bufferId[s];
					pSrcSector->data=dp.source->GetSectorData( p.chs, s, true, &pSrcSector->dataLength, pFdcStatus );
					// : reporting SourceSector Errors if A&B, A = automatically not accepted Errors exist, B = Error reporting for current Track is enabled
					if (pFdcStatus->ToWord()&~p.acceptance.automaticallyAcceptedErrors && !p.acceptance.remainingErrorsOnTrack){
						// | Dialog definition
						class CErroneousSectorDialog sealed:public CDialog{
							const TDumpParams &dp;
							TParams &rp;
							const TSourceSector &rss;
							TFdcStatus &rFdcStatus;

							void PreInitDialog() override{
								// dialog initialization
								// > base
								CDialog::PreInitDialog();
								// > creating message on Errors
								LPCTSTR bitDescriptions[20],*pDesc=bitDescriptions; // 20 = surely big enough buffer
								rFdcStatus.GetDescriptionsOfSetBits(bitDescriptions);
								TCHAR buf[512],tmp[30],*p=buf+::wsprintf(buf,_T("Cannot read sector with %s on source Track %d.\n\n"),rp.chs.sectorId.ToString(tmp),rp.track);
								p+=::wsprintf( p, _T("\"Status register 1\" reports (0x%02X)\n"), rFdcStatus.reg1 );
								if (*pDesc)
									while (*pDesc)
										p+=::wsprintf( p, _T("- %s\n"), *pDesc++ );
								else
									p+=::lstrlen(::lstrcpy(p,NO_ERROR));
								pDesc++; // skipping Null that terminates list of bits set in Register 1
								p+=::wsprintf( p, _T("\n\"Status register 2\" reports (0x%02X)\n"), rFdcStatus.reg2 );
								if (*pDesc)
									while (*pDesc)
										p+=::wsprintf( p, _T("- %s\n"), *pDesc++ );
								else
									p+=::lstrlen(::lstrcpy(p,NO_ERROR));
								SetDlgItemText( ID_ERROR, buf );
								// > converting the "Accept" button to a SplitButton
								static const TUtils::TSplitButtonAction Actions[ACCEPT_OPTIONS_COUNT]={
									{ ACCEPT_ERROR_ID, _T("Accept error") },
									{ ID_ERROR, _T("Accept all errors of this kind") },
									{ ID_TRACK, _T("Accept all errors in this track") },
									{ ID_IMAGE, _T("Accept all errors on the disk") }
								};
								CWnd *const pBtnAccept=GetDlgItem(IDOK);
								TUtils::ConvertToSplitButton( pBtnAccept->m_hWnd, Actions, ACCEPT_OPTIONS_COUNT );
								pBtnAccept->EnableWindow( dynamic_cast<CImageRaw *>(dp.target)==NULL ); // accepting errors is allowed only if the Target Image can accept them
								// > enabling/disabling the "Recover" button
								GetDlgItem(ID_RECOVER)->EnableWindow( rFdcStatus.DescribesIdFieldCrcError() || rFdcStatus.DescribesDataFieldCrcError() );
							}
							LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
								// window procedure
								switch (msg){
									case WM_COMMAND:
										switch (wParam){
											case IDRETRY:
												UpdateData(TRUE);
												EndDialog(wParam);
												break;
											case ID_ERROR:
												rp.acceptance.automaticallyAcceptedErrors|=rFdcStatus.ToWord();
												UpdateData(TRUE);
												EndDialog(ACCEPT_ERROR_ID);
												break;
											case ID_IMAGE:
												rp.acceptance.automaticallyAcceptedErrors=-1;
												//fallthrough
											case ID_TRACK:
												rp.acceptance.remainingErrorsOnTrack=true;
												UpdateData(TRUE);
												EndDialog(ACCEPT_ERROR_ID);
												break;
											case ID_RECOVER:{
												// Sector recovery
												// : Dialog definition
												class CSectorRecoveryDialog sealed:public CDialog{
												public:
													int idFieldRecoveryType, dataFieldRecoveryType;
													TSectorId idFieldSubstituteSectorId;
													BYTE dataFieldSubstituteFillerByte;
												private:
													const CDos::PCProperties dosProps;
													const TParams &rParams;
													const TFdcStatus &rFdcStatus;

													void DoDataExchange(CDataExchange *pDX) override{
														// exchange of data from and to controls
														// | "ID Field" region
														DDX_Radio( pDX, ID_IDFIELD, idFieldRecoveryType );
															TCHAR bufSectorId[32];
															DDX_Text( pDX, ID_IDFIELD_VALUE, bufSectorId, sizeof(bufSectorId)/sizeof(TCHAR) );
															if (pDX->m_bSaveAndValidate){
																int cyl,side,sect,len;
																_stscanf( bufSectorId, _T("%d.%d.%d.%d"), &cyl, &side, &sect, &len );
																idFieldSubstituteSectorId.cylinder=cyl, idFieldSubstituteSectorId.side=side, idFieldSubstituteSectorId.sector=sect, idFieldSubstituteSectorId.lengthCode=len;
															}else
																::wsprintf( bufSectorId, _T("%d.%d.%d.%d"), idFieldSubstituteSectorId.cylinder, idFieldSubstituteSectorId.side, idFieldSubstituteSectorId.sector, idFieldSubstituteSectorId.lengthCode );
															DDX_Text( pDX, ID_IDFIELD_VALUE, bufSectorId, sizeof(bufSectorId)/sizeof(TCHAR) );
															static const WORD IdFieldRecoveryOptions[]={ ID_IDFIELD, ID_IDFIELD_CRC, ID_IDFIELD_REPLACE, 0 };
															TUtils::EnableDlgControls( m_hWnd, IdFieldRecoveryOptions, rFdcStatus.DescribesIdFieldCrcError() );
															static const WORD IdFieldReplaceOption[]={ ID_IDFIELD_VALUE, ID_DEFAULT1, 0 };
															TUtils::EnableDlgControls( m_hWnd, IdFieldReplaceOption, idFieldRecoveryType==2 );
														// | "Data Field" region
														DDX_Radio( pDX, ID_DATAFIELD, dataFieldRecoveryType );
															DDX_Text( pDX, ID_DATAFIELD_FILLERBYTE, dataFieldSubstituteFillerByte );
															if (dosProps==&CUnknownDos::Properties)
																GetDlgItem(ID_DEFAULT2)->SetWindowText(_T("Random value"));
															static const WORD DataFieldRecoveryOptions[]={ ID_DATAFIELD, ID_DATAFIELD_CRC, ID_DATAFIELD_REPLACE, 0 };
															TUtils::EnableDlgControls( m_hWnd, DataFieldRecoveryOptions, rFdcStatus.DescribesDataFieldCrcError() );
															static const WORD DataFieldReplaceOption[]={ ID_DATAFIELD_FILLERBYTE, ID_DEFAULT2, 0 };
															TUtils::EnableDlgControls( m_hWnd, DataFieldReplaceOption, dataFieldRecoveryType==2 );
														// | interactivity
														GetDlgItem(IDOK)->EnableWindow(idFieldRecoveryType|dataFieldRecoveryType);
													}
													LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
														switch (msg){
															case WM_COMMAND:
																// processing a command
																// | processing a command
																switch (wParam){
																	case ID_DEFAULT1:
																		idFieldSubstituteSectorId=rParams.chs.sectorId;
																		DoDataExchange( &CDataExchange(this,FALSE) ); // updating visuals
																		break;
																	case ID_DEFAULT2:
																		dataFieldSubstituteFillerByte= dosProps!=&CUnknownDos::Properties ? dosProps->sectorFillerByte : ::GetTickCount() ;
																		DoDataExchange( &CDataExchange(this,FALSE) ); // updating visuals
																		break;
																}
																// | updating interactivity
																if (HIWORD(wParam)==BN_CLICKED)
																	DoDataExchange( &CDataExchange(this,TRUE) );
																break;
														}
														return __super::WindowProc(msg,wParam,lParam);
													}
												public:
													CSectorRecoveryDialog(const CDos *dos,const TParams &rParams,const TFdcStatus &rFdcStatus)
														: CDialog(IDR_IMAGE_DUMP_ERROR_RESOLUTION)
														, dosProps(dos->properties) , rParams(rParams) , rFdcStatus(rFdcStatus)
														, idFieldRecoveryType(0) , idFieldSubstituteSectorId(rParams.chs.sectorId)
														, dataFieldRecoveryType(0) , dataFieldSubstituteFillerByte(dos->properties->sectorFillerByte) {
													}
												} d(dp.dos,rp,rFdcStatus);
												// : showing the Dialog and processing its result
												if (d.DoModal()==IDOK){
													switch (d.idFieldRecoveryType){
														case 2:
															// replacing Sector ID
															rp.chs.sectorId=d.idFieldSubstituteSectorId;
															//fallthrough
														case 1:
															// recovering CRC
															rFdcStatus.CancelIdFieldCrcError();
															break;
													}
													switch (d.dataFieldRecoveryType){
														case 2:
															// replacing data with SubstituteByte
															::memset( rss.data, d.dataFieldSubstituteFillerByte, rss.dataLength );
															//fallthrough
														case 1:
															// recovering CRC
															rFdcStatus.CancelDataFieldCrcError();
															break;
													}
													EndDialog(ACCEPT_ERROR_ID);
												}
												break;
											}
										}
										break;
								}
								return CDialog::WindowProc(msg,wParam,lParam);
							}
						public:
							CErroneousSectorDialog(const TDumpParams &dp,TParams &_rParams,const TSourceSector &_rss,TFdcStatus &_rFdcStatus)
								// ctor
								: CDialog(IDR_IMAGE_DUMP_ERROR)
								, dp(dp) , rp(_rParams) , rss(_rss) , rFdcStatus(_rFdcStatus) {
							}
						} d(dp,p,*pSrcSector,*pFdcStatus);
						// | showing the Dialog and processing its result
						switch (d.DoModal()){
							case IDCANCEL:
								err=ERROR_CANCELLED;
								goto terminateWithError;
							case IDRETRY:
								continue;
						}
					}
					if (!pFdcStatus->IsWithoutError()){
						TDumpParams::TSourceSectorError *const psse=&erroneousSectors.buffer[erroneousSectors.n++];
						psse->id=p.chs.sectorId, psse->fdcStatus=*pFdcStatus;
					}
					// : next SourceSector
					s++, pSrcSector++, pFdcStatus++;
				}
				p.acceptance.remainingErrorsOnTrack=false; // "True" valid only for Track it was set on
				// . formatting Target Track
//TUtils::Information("formatting Target Track");
				if (dp.formatJustBadTracks && dp.source->IsTrackHealthy(p.chs.cylinder,p.chs.head)){
					if (dp.target->PresumeHealthyTrackStructure(p.chs.cylinder,p.chs.head,nSectors,bufferId)!=ERROR_SUCCESS)
						goto reformatTrack;
				}else
reformatTrack:		if ( err=dp.target->FormatTrack(p.chs.cylinder,p.chs.head,nSectors,bufferId,bufferLength,bufferFdcStatus,dp.gap3,dp.fillerByte) )
						goto terminateWithError;
				// . writing to Target Track
//TUtils::Information("writing to Target Track");
				pSrcSector=sourceSectors, pFdcStatus=bufferFdcStatus;
				for( BYTE s=0; s<nSectors; ){
					if (!pFdcStatus->DescribesMissingDam()){
						p.chs.sectorId=bufferId[s]; WORD w; TFdcStatus sr;
						if (const PSectorData targetData=dp.target->GetSectorData(p.chs,s,true,&w,&sr)){
							::memcpy( targetData, pSrcSector->data, bufferLength[s] );
							if (( err=dp.target->MarkSectorAsDirty(p.chs,s,pFdcStatus) )!=ERROR_SUCCESS)
								goto errorDuringWriting;
						}else{
							err=::GetLastError();
errorDuringWriting:			TCHAR buf[80],tmp[30];
							::wsprintf(buf,_T("Cannot write to sector with %s on target Track %d"),p.chs.sectorId.ToString(tmp),p.track);
							switch (TUtils::AbortRetryIgnore(buf,err,MB_DEFBUTTON2)){
								case IDABORT:	goto terminateWithError;
								case IDRETRY:	continue;
								case IDIGNORE:	break;
							}
						}
					}
					s++, pSrcSector++, pFdcStatus++; // cannot include in the FOR clause - see Continue statement in the cycle
				}
				// . registering Track with ErroneousSectors
//TUtils::Information("registering Track with ErroneousSectors");
				if (erroneousSectors.n){
					TDumpParams::TSourceTrackErrors *psse=(TDumpParams::TSourceTrackErrors *)::malloc(sizeof(TDumpParams::TSourceTrackErrors)+(erroneousSectors.n-1)*sizeof(TDumpParams::TSourceSectorError));
						psse->cyl=p.chs.cylinder, psse->head=p.chs.head;
						psse->pNextErroneousTrack=NULL;
						::memcpy( psse->erroneousSectors, erroneousSectors.buffer, ( psse->nErroneousSectors=erroneousSectors.n )*sizeof(TDumpParams::TSourceSectorError) );
					*ppSrcTrackErrors=psse, ppSrcTrackErrors=&psse->pNextErroneousTrack;
				}
			}
		return ERROR_SUCCESS;
	}

	void CImage::__dump__() const{
		// dumps this Image to a chosen target
		const PDos dos=__getActive__()->dos;
		// - defining the Dialog
		class CDumpDialog sealed:public CDialog{
			const PDos dos;

			void PreInitDialog() override{
				// dialog initialization
				// . base
				CDialog::PreInitDialog();
				// . adjusting text in button next to FillerByte edit box
				if (dos->properties==&CUnknownDos::Properties)
					SetDlgItemText( ID_DEFAULT1, _T("Random value") );
			}
			void DoDataExchange(CDataExchange *pDX) override{
				// transferring data to and from controls
				const HWND hMedium=GetDlgItem(ID_MEDIUM)->m_hWnd;
				if (pDX->m_bSaveAndValidate){
					// : FileName must be known
					pDX->PrepareEditCtrl(ID_FILE);
					if (!::lstrcmp(fileName,ELLIPSIS)){
						TUtils::Information( _T("Target not specified.") );
						pDX->Fail();
					}else if (!dos->image->GetPathName().Compare(fileName)){
						TUtils::Information( _T("Target must not be the same as source.") );
						pDX->Fail();
					}
					// : Medium must be supported by both DOS and Image
					pDX->PrepareEditCtrl(ID_MEDIUM);
					HWND hComboBox=GetDlgItem(ID_MEDIUM)->m_hWnd;
					TMedium::TType mt=(TMedium::TType)ComboBox_GetItemData( hComboBox, ComboBox_GetCurSel(hComboBox) );
				}else{
					GetDlgItem(ID_FILE)->SetWindowText(ELLIPSIS);
					__populateComboBoxWithCompatibleMedia__( hMedium, 0, NULL ); // if FileName not set, Medium cannot be determined
				}
				CComboBox cbMedium;
				cbMedium.Attach(hMedium);
					const TMedium::PCProperties mp=	targetImageProperties // ComboBox populated with compatible Media and one of them selected
													? TMedium::GetProperties( dumpParams.mediumType=(TMedium::TType)cbMedium.GetItemData(cbMedium.GetCurSel()) )
													: NULL;
				cbMedium.Detach();
				int i=dumpParams.formatJustBadTracks;
				DDX_Check( pDX, ID_FORMAT, i );
				dumpParams.formatJustBadTracks=i;
				DDX_Text( pDX,	ID_CYLINDER,	(RCylinder)dumpParams.cylinderA );
					if (mp)
						DDV_MinMaxUInt( pDX,dumpParams.cylinderA, 0, mp->cylinderRange.iMax-1 );
				DDX_Text( pDX,	ID_CYLINDER_N,	(RCylinder)dumpParams.cylinderZ );
					if (mp)
						DDV_MinMaxUInt( pDX,dumpParams.cylinderZ, dumpParams.cylinderA, mp->cylinderRange.iMax-1 );
				DDX_Text( pDX,	ID_HEAD,		dumpParams.nHeads );
					if (mp)
						DDV_MinMaxUInt( pDX,dumpParams.nHeads, 1, mp->headRange.iMax );
				DDX_Text( pDX,	ID_GAP,			dumpParams.gap3 );
				DDX_Text( pDX,	ID_NUMBER,		dumpParams.fillerByte );
				DDX_Check(pDX,	ID_PRIORITY,	realtimeThreadPriority );
				DDX_Check(pDX,	ID_REPORT,		showReport );
				if (pDX->m_bSaveAndValidate){
					// : destroying any previously instantiated Target Image
					if (dumpParams.target)
						delete dumpParams.target, dumpParams.target=NULL;
					// : instantiating Target Image
					if (targetImageProperties)
						dumpParams.target=targetImageProperties->fnInstantiate();
					else{
						TUtils::FatalError(_T("Unknown destination to dump to."));
						return pDX->Fail();
					}
					// : suggesting to dump only Cylinders within the officially reported Format (where suitable to)
					if (
						#ifndef _DEBUG // checking if the Source is a floppy only in Release mode
							dumpParams.source->properties==&CFDD::Properties
							&&
						#endif
						dynamic_cast<CImageRaw *>(dumpParams.target)!=NULL
					)
						// suggestion is helpfull only if dumping a floppy to an image with invariant structure (as Cylinders outside the official Format may have different layout, causing an "Device doesn't recognize this command" error during formatting the raw Image)
						if (dos->properties!=&CUnknownDos::Properties) // Unknown DOS doesn't have valid Format information
							if (dumpParams.cylinderA || dumpParams.cylinderZ>=dos->formatBoot.nCylinders){ // ">=" = Cylinders are numbered from 0
								// : defining the Dialog
								class CSuggestionDialog sealed:public TUtils::CCommandDialog{
									void PreInitDialog() override{
										// dialog initialization
										// : base
										TUtils::CCommandDialog::PreInitDialog();
										// : supplying available actions
										__addCommandButton__( IDYES, _T("Continue with format adopted from boot sector (recommended)") );
										__addCommandButton__( IDNO, _T("Continue with current settings (cylinders beyond official format may fail!)") );
										__addCommandButton__( IDCANCEL, _T("Return to dump dialog") );
									}
								public:
									CSuggestionDialog()
										// ctor
										: TUtils::CCommandDialog(_T("Dumping cylinders outside official format to a raw image may fail!")) {
									}
								} d;
								// : showing the Dialog and processing its result
								switch (d.DoModal()){
									case IDYES:
										dumpParams.cylinderA=0, dumpParams.cylinderZ=dos->formatBoot.nCylinders-1;
										//fallthrough
									case IDNO:
										break;
									default:
										pDX->Fail();
										break;
								}
							}
				}
			}
			afx_msg void OnPaint(){
				// painting
				// . base
				CDialog::OnPaint();
				// . painting curly brackets
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
								if (app.__doPromptFileName__( fileName, true, AFX_IDS_SAVEFILE, 0, NULL )){
									// : compacting FileName in order to be better displayable on the button
									CWnd *const pBtnFile=GetDlgItem(ID_FILE);
									RECT r;
									pBtnFile->GetClientRect(&r);
									TCHAR buf[MAX_PATH];
									::PathCompactPath( CClientDC(pBtnFile), ::lstrcpy(buf,fileName), r.right-r.left );
									pBtnFile->SetWindowText(buf);
									// : determining TargetImageProperties
									targetImageProperties=	!::lstrcmp(fileName,FDD_A_LABEL)
															? &CFDD::Properties
															: __determineTypeByExtension__(_tcsrchr(fileName,'.')); // Null <=> unknown container
									// : adjusting interactivity
									if (targetImageProperties){
										// > populating ComboBox with Media supported by both DOS and Image
										BYTE nCompatibleMedia;
										switch (dos->formatBoot.mediumType){
											case TMedium::FLOPPY_DD:
											case TMedium::FLOPPY_HD:
												// source Image is a floppy - enabling dumping to any kind of a floppy (motivation: some copy-protection schemes feature misleading information on the kind of floppy; e.g., "Teen Agent" [or "Agent mlicnak"] installation disk #2 and #3 are introduced as 2DD floppies while they really are HD!)
												nCompatibleMedia=__populateComboBoxWithCompatibleMedia__(	GetDlgItem(ID_MEDIUM)->m_hWnd,
																											dos->properties->supportedMedia&TMedium::FLOPPY_ANY,
																											targetImageProperties
																										);
												break;
											default:
												// source Image is a hard-disk
												nCompatibleMedia=__populateComboBoxWithCompatibleMedia__(	GetDlgItem(ID_MEDIUM)->m_hWnd,
																											dos->formatBoot.mediumType,
																											targetImageProperties
																										);
												break;
										}
										// > enabling/disabling controls
										static const WORD Controls[]={ ID_CYLINDER, ID_CYLINDER_N, ID_HEAD, ID_GAP, ID_NUMBER, ID_DEFAULT1, IDOK, 0 };
										TUtils::EnableDlgControls( m_hWnd, Controls, nCompatibleMedia>0 );
										GetDlgItem(ID_FORMAT)->EnableWindow(nCompatibleMedia && targetImageProperties==&CFDD::Properties);
											CheckDlgButton( ID_FORMAT, targetImageProperties==&CFDD::Properties );
										GetDlgItem(IDOK)->SetFocus();
										// > automatically ticking the "Real-time thread priority" check-box if either the source or the target is a floppy drive
										if (dos->image->properties==&CFDD::Properties || targetImageProperties==&CFDD::Properties)
											SendDlgItemMessage( ID_PRIORITY, BM_SETCHECK, BST_CHECKED );
									}
								}else
									*fileName=c;
								break;
							}
							case ID_DEFAULT1:
								SetDlgItemInt( ID_NUMBER, dos->properties!=&CUnknownDos::Properties ? dos->properties->sectorFillerByte : ::GetTickCount()&0xff );
								break;
							case MAKELONG(ID_CYLINDER,EN_CHANGE):
							case MAKELONG(ID_CYLINDER_N,EN_CHANGE):
								Invalidate();
								break;
						}
						break;
					case WM_NOTIFY:
						if (((LPNMHDR)lParam)->code==NM_CLICK){
							// . defining the Dialog
							class CHelpDialog sealed:public TUtils::CCommandDialog{
								void PreInitDialog() override{
									// dialog initialization
									// : base
									TUtils::CCommandDialog::PreInitDialog();
									// : supplying available actions
									__addCommandButton__( ID_IMAGE, _T("How do I create an image of a real floppy disk?") );
									__addCommandButton__( ID_DRIVE, _T("How do I save an image back to a real floppy disk?") );
									__addCommandButton__( ID_FILE, _T("How do I convert between two images? (E.g. *.IMA to *.DSK)") );
									__addCommandButton__( ID_ACCURACY, _T("How do I create an exact copy of a real floppy disk?") );
									__addCommandButton__( IDCANCEL, MSG_HELP_CANCEL );
								}
							public:
								CHelpDialog()
									// ctor
									: TUtils::CCommandDialog(_T("Dumping is used to convert between data containers.")) {
								}
							} d;
							// . showing the Dialog and processing its result
							TCHAR url[200];
							switch (d.DoModal()){
								case ID_IMAGE:
									TUtils::NavigateToUrlInDefaultBrowser( TUtils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_floppy2image.html"),url) );
									break;
								case ID_DRIVE:
									TUtils::NavigateToUrlInDefaultBrowser( TUtils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_image2floppy.html"),url) );
									break;
								case ID_FILE:
									TUtils::NavigateToUrlInDefaultBrowser( TUtils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_convertImage.html"),url) );
									break;
								case ID_ACCURACY:
									TUtils::NavigateToUrlInDefaultBrowser( TUtils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_copyFloppy.html"),url) );
									break;
							}
						}
						break;
				}
				return CDialog::WindowProc(msg,wParam,lParam);
			}
		public:
			TCHAR fileName[MAX_PATH];
			CImage::PCProperties targetImageProperties;
			TDumpParams dumpParams;
			int realtimeThreadPriority,showReport;

			CDumpDialog(PDos _dos)
				// ctor
				: CDialog(IDR_IMAGE_DUMP)
				, dos(_dos) , targetImageProperties(NULL) , dumpParams(_dos)
				, realtimeThreadPriority(BST_UNCHECKED)
				, showReport(BST_CHECKED) {
				::lstrcpy( fileName, ELLIPSIS );
			}
		} d(dos);
		// - showing Dialog and processing its result
		if (d.DoModal()==IDOK){
			// . resetting Target Image
			d.dumpParams.target->dos=dos;
			TStdWinError err=d.dumpParams.target->Reset();
			if (err!=ERROR_SUCCESS)
				goto error;
			// . dumping
			err=TBackgroundActionCancelable(
					__dump_thread__,
					&d.dumpParams,
					d.realtimeThreadPriority ? THREAD_PRIORITY_TIME_CRITICAL : THREAD_PRIORITY_NORMAL
				).CarryOut( d.dumpParams.cylinderZ+1-d.dumpParams.cylinderA );
			if (err==ERROR_SUCCESS){
				if (d.dumpParams.target->OnSaveDocument(d.fileName)){
					// : displaying statistics on SourceTrackErrors
					if (d.showReport==BST_CHECKED){
						// | saving to temporary file
						TCHAR tmpFileName[MAX_PATH];
						::GetTempPath(MAX_PATH,tmpFileName);
						::GetTempFileName( tmpFileName, NULL, TRUE, tmpFileName );
						d.dumpParams.__exportErroneousTracksToHtml__( CFile(::lstrcat(tmpFileName,_T(".html")),CFile::modeCreate|CFile::modeWrite) );
						// | displaying
						((CMainWindow *)app.m_pMainWnd)->OpenWebPage( _T("Dump results"), tmpFileName );
					}
					// : reporting success
					TUtils::Information(_T("Dumped successfully."));
				}else
					TUtils::FatalError(_T("Cannot save to the target"),::GetLastError());
			}else
error:			TUtils::FatalError(_T("Cannot dump"),err);
			// . destroying the list of SourceTrackErrors
			while (const TDumpParams::TSourceTrackErrors *tmp=d.dumpParams.pOutErroneousTracks)
				d.dumpParams.pOutErroneousTracks=d.dumpParams.pOutErroneousTracks->pNextErroneousTrack, ::free((PVOID)tmp);
		}
	}
