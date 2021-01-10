#include "stdafx.h"

	#define INI_DUMP	_T("Dump")

	struct TDumpParams sealed{
		#pragma pack(1)
		typedef const struct TSourceSectorError sealed{
			TSectorId id;
			TFdcStatus fdcStatus;
		} *PCSourceSectorError;

		const CDos *const dos;
		Medium::TType mediumType;
		const PImage source;
		std::unique_ptr<CImage> target;
		bool formatJustBadTracks;
		TCylinder cylinderA,cylinderZ;
		THead nHeads;
		struct{
			BYTE value;
			bool valueValid;
		} gap3;
		BYTE fillerByte;
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
			, source(dos->image)
			, formatJustBadTracks(false)
			, fillerByte(dos->properties->sectorFillerByte)
			, cylinderA(0) , cylinderZ(source->GetCylinderCount()-1)
			, nHeads(source->GetNumberOfFormattedSides(0))
			, pOutErroneousTracks(nullptr) {
			gap3.value=dos->properties->GetValidGap3ForMedium(dos->formatBoot.mediumType);
			gap3.valueValid=true;
		}

		~TDumpParams(){
			// dtor
			if (target)
				target->dos=nullptr; // to not also destroy the DOS
		}

		void __exportErroneousTracksToHtml__(CFile &fHtml) const{
			// exports SourceTrackErrors to given HTML file
			Utils::WriteToFile(fHtml,_T("<html><head><style>body,td{font-size:13pt;margin:24pt}table{border:1pt solid black;spacing:10pt}td{vertical-align:top}td.caption{font-size:14pt;background:silver}</style></head><body>"));
				Utils::WriteToFile(fHtml,_T("<h3>Overview</h3>"));
					if (pOutErroneousTracks){
						Utils::WriteToFile(fHtml,_T("<table><tr><td class=caption>Error</td><td class=caption>Count</td></tr>"));
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
									Utils::WriteToFile(fHtml,_T("<tr><td>"));
										Utils::WriteToFile(fHtml,(LPCTSTR)((UINT)bitDescriptions[0]|(UINT)bitDescriptions[1]|(UINT)bitDescriptions[2]));
									Utils::WriteToFile(fHtml,_T("</td><td align=right>"));
										Utils::WriteToFile(fHtml,nErrorOccurences);
									Utils::WriteToFile(fHtml,_T("</td></tr>"));
								}
							}
						Utils::WriteToFile(fHtml,_T("</table>"));
					}else
						Utils::WriteToFile(fHtml,_T("No errors occurred."));
				Utils::WriteToFile(fHtml,_T("<h3>Details</h3>"));
					if (pOutErroneousTracks){
						Utils::WriteToFile(fHtml,_T("<table><tr><td class=caption width=120>Track</td><td class=caption>Erroneous Sectors</td></tr>"));
							for( const TSourceTrackErrors *pErroneousTrack=pOutErroneousTracks; pErroneousTrack; pErroneousTrack=pErroneousTrack->pNextErroneousTrack ){
								Utils::WriteToFile(fHtml,_T("<tr><td>"));
									Utils::WriteToFileFormatted( fHtml, _T("Cyl %d, Head %d"), pErroneousTrack->cyl, pErroneousTrack->head );
								Utils::WriteToFile(fHtml,_T("</td><td><ul>"));
									PCSourceSectorError psse=pErroneousTrack->erroneousSectors;
									for( BYTE n=pErroneousTrack->nErroneousSectors; n; n--,psse++ ){
										Utils::WriteToFile(fHtml,_T("<li>"));
											Utils::WriteToFileFormatted( fHtml, _T("<b>%s</b>. "), (LPCTSTR)psse->id.ToString() );
											LPCTSTR bitDescriptions[10],*pDesc=bitDescriptions;
											const TPhysicalAddress chs={ pErroneousTrack->cyl, pErroneousTrack->head, psse->id };
											Utils::WriteToFileFormatted( fHtml, _T("<i>FAT</i>: %s, "), dos->GetSectorStatusText(chs) );
											psse->fdcStatus.GetDescriptionsOfSetBits(bitDescriptions);
											Utils::WriteToFileFormatted( fHtml, _T("<i>SR1</i> (0x%02X): "), psse->fdcStatus.reg1 );
											if (*pDesc){
												Utils::WriteToFile(fHtml,*pDesc++);
												while (*pDesc){
													Utils::WriteToFile(fHtml,_T(", "));
													Utils::WriteToFile(fHtml,*pDesc++);
												}
											}else
												Utils::WriteToFile(fHtml,_T("No error"));
											Utils::WriteToFile(fHtml,_T("."));
											pDesc++; // skipping Null that terminates the list of bits set in Register 1
											Utils::WriteToFileFormatted( fHtml, _T(" <i>SR2</i> (0x%02X): "), psse->fdcStatus.reg2 );
											if (*pDesc){
												Utils::WriteToFile(fHtml,*pDesc++);
												while (*pDesc){
													Utils::WriteToFile(fHtml,_T(", "));
													Utils::WriteToFile(fHtml,*pDesc++);
												}
											}else
												Utils::WriteToFile(fHtml,_T("No error"));
											Utils::WriteToFile(fHtml,_T("."));
										Utils::WriteToFile(fHtml,_T("</li>"));
									}
								Utils::WriteToFile(fHtml,_T("</ul></td></tr>"));
							}
						Utils::WriteToFile(fHtml,_T("</table>"));
					}else
						Utils::WriteToFile(fHtml,_T("None."));
			Utils::WriteToFile(fHtml,_T("</body></html>"));
		}
	};

	#define ACCEPT_OPTIONS_COUNT	4
	#define ACCEPT_ERROR_ID			IDOK

	#define RESOLVE_OPTIONS_COUNT	3
	#define RESOLVE_EXCLUDE_ID		IDIGNORE

	#define NO_STATUS_ERROR	_T("- no error\r\n")

	static UINT AFX_CDECL __dump_thread__(PVOID _pCancelableAction){
		// threat to copy Tracks
		LOG_ACTION(_T("dump thread"));
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		TDumpParams &dp=*(TDumpParams *)pAction->GetParams();
		pAction->SetProgressTarget( dp.cylinderZ+1-dp.cylinderA );
		// - setting geometry to the TargetImage
		TSector nSectors=dp.source->ScanTrack(0,0);
		const TFormat targetGeometry={ dp.mediumType, dp.dos->formatBoot.codecType, dp.cylinderZ+1, dp.nHeads, nSectors, dp.dos->formatBoot.sectorLengthCode, dp.dos->formatBoot.sectorLength, 1 };
		TStdWinError err=dp.target->SetMediumTypeAndGeometry( &targetGeometry, dp.dos->sideMap, dp.dos->properties->firstSectorNumber );
		if (err!=ERROR_SUCCESS)
terminateWithError:
			return LOG_ERROR(pAction->TerminateWithError(err));
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
		const Utils::CByteIdentity sectorIdAndPositionIdentity;
		for( p.chs.cylinder=dp.cylinderA; p.chs.cylinder<=dp.cylinderZ; pAction->UpdateProgress(++p.chs.cylinder-dp.cylinderA) )
			for( p.chs.head=0; p.chs.head<dp.nHeads; p.chs.head++ ){
				if (pAction->IsCancelled()) return LOG_ERROR(ERROR_CANCELLED);
				LOG_TRACK_ACTION(p.chs.cylinder,p.chs.head,_T("processing"));
				p.track=p.chs.GetTrackNumber(dp.nHeads);
				// . scanning Source Track
				TSectorId bufferId[(TSector)-1];	WORD bufferLength[(TSector)-1];
				Codec::TType codec;
{LOG_TRACK_ACTION(p.chs.cylinder,p.chs.head,_T("scanning source"));
				nSectors=dp.source->ScanTrack(p.chs.cylinder,p.chs.head,&codec,bufferId,bufferLength);
}
				// . reading Source Track
				#pragma pack(1)
				struct{
					TSector n;
					TDumpParams::TSourceSectorError buffer[(TSector)-1];
				} erroneousSectors;
				erroneousSectors.n=0;
				PSectorData bufferSectorData[(TSector)-1];
				TFdcStatus bufferFdcStatus[(TSector)-1];
{LOG_TRACK_ACTION(p.chs.cylinder,p.chs.head,_T("reading source"));
				dp.source->GetTrackData( p.chs.cylinder, p.chs.head, Revolution::ANY_GOOD, bufferId, sectorIdAndPositionIdentity, nSectors, true, bufferSectorData, bufferLength, bufferFdcStatus ); // reading healthy Sectors (unhealthy ones read individually below)
				for( TSector s=0; s<nSectors; ){
					// : reading SourceSector
					p.chs.sectorId=bufferId[s];
					LOG_SECTOR_ACTION(&p.chs.sectorId,_T("reading"));
					bufferSectorData[s]=dp.source->GetSectorData( p.chs, s, true, bufferLength+s, bufferFdcStatus+s );
					// : reporting SourceSector Errors if A&B, A = automatically not accepted Errors exist, B = Error reporting for current Track is enabled
					if (bufferFdcStatus[s].ToWord()&~p.acceptance.automaticallyAcceptedErrors && !p.acceptance.remainingErrorsOnTrack){
						// | Dialog definition
						class CErroneousSectorDialog sealed:public Utils::CRideDialog{
							const TDumpParams &dp;
							TParams &rp;
							const PSectorData sectorData;
							const WORD sectorLength;
							TFdcStatus &rFdcStatus;

							void PreInitDialog() override{
								// dialog initialization
								// > base
								__super::PreInitDialog();
								// > creating message on Errors
								LPCTSTR bitDescriptions[20],*pDesc=bitDescriptions; // 20 = surely big enough buffer
								rFdcStatus.GetDescriptionsOfSetBits(bitDescriptions);
								TCHAR buf[512],*p=buf+::wsprintf(buf,_T("Cannot read sector with %s on source Track %d.\r\n"),(LPCTSTR)rp.chs.sectorId.ToString(),rp.track);
								p+=::wsprintf( p, _T("\r\nFAT reports this sector \"%s\".\r\n"), dp.dos->GetSectorStatusText(rp.chs) );
								p+=::wsprintf( p, _T("\r\n\"Status register 1\" reports (0x%02X)\r\n"), rFdcStatus.reg1 );
								if (*pDesc)
									while (*pDesc)
										p+=::wsprintf( p, _T("- %s\r\n"), *pDesc++ );
								else
									p+=::lstrlen(::lstrcpy(p,NO_STATUS_ERROR));
								pDesc++; // skipping Null that terminates list of bits set in Register 1
								p+=::wsprintf( p, _T("\r\n\"Status register 2\" reports (0x%02X)\r\n"), rFdcStatus.reg2 );
								if (*pDesc)
									while (*pDesc)
										p+=::wsprintf( p, _T("- %s\r\n"), *pDesc++ );
								else
									p+=::lstrlen(::lstrcpy(p,NO_STATUS_ERROR));
								SetDlgItemText( ID_ERROR, buf );
								// > converting the "Accept" button to a SplitButton
								static const Utils::TSplitButtonAction Actions[ACCEPT_OPTIONS_COUNT]={
									{ ACCEPT_ERROR_ID, _T("Accept error") },
									{ ID_ERROR, _T("Accept all errors of this kind") },
									{ ID_TRACK, _T("Accept all errors in this track") },
									{ ID_IMAGE, _T("Accept all errors on the disk") }
								};
								ConvertDlgButtonToSplitButton( IDOK, Actions, ACCEPT_OPTIONS_COUNT );
								EnableDlgItem( IDOK, dynamic_cast<CImageRaw *>(dp.target.get())==nullptr ); // accepting errors is allowed only if the Target Image can accept them
								// > converting the "Resolve" button to a SplitButton
								static const Utils::TSplitButtonAction ResolveActions[RESOLVE_OPTIONS_COUNT]={
									{ 0, _T("Resolve") }, // 0 = no default action
									{ RESOLVE_EXCLUDE_ID, _T("Exclude from track") },
									{ ID_RECOVER, _T("Recover ID or Data...") },
								};
								ConvertDlgButtonToSplitButton( IDNO, ResolveActions, RESOLVE_OPTIONS_COUNT-1+(rFdcStatus.DescribesIdFieldCrcError()||rFdcStatus.DescribesDataFieldCrcError()) );
								EnableDlgItem( IDNO, dynamic_cast<CImageRaw *>(dp.target.get())==nullptr ); // recovering errors is allowed only if the Target Image can accept them
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
												class CSectorRecoveryDialog sealed:public Utils::CRideDialog{
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
															EnableDlgItems( IdFieldRecoveryOptions, rFdcStatus.DescribesIdFieldCrcError() );
															static const WORD IdFieldReplaceOption[]={ ID_IDFIELD_VALUE, ID_DEFAULT1, 0 };
															EnableDlgItems( IdFieldReplaceOption, idFieldRecoveryType==2 );
														// | "Data Field" region
														DDX_Radio( pDX, ID_DATAFIELD, dataFieldRecoveryType );
															DDX_Text( pDX, ID_DATAFIELD_FILLERBYTE, dataFieldSubstituteFillerByte );
															if (dosProps==&CUnknownDos::Properties)
																SetDlgItemText( ID_DEFAULT2, _T("Random value") );
															static const WORD DataFieldRecoveryOptions[]={ ID_DATAFIELD, ID_DATAFIELD_CRC, ID_DATAFIELD_REPLACE, 0 };
															EnableDlgItems( DataFieldRecoveryOptions, rFdcStatus.DescribesDataFieldCrcError() );
															static const WORD DataFieldReplaceOption[]={ ID_DATAFIELD_FILLERBYTE, ID_DEFAULT2, 0 };
															EnableDlgItems( DataFieldReplaceOption, dataFieldRecoveryType==2 );
														// | interactivity
														EnableDlgItem( IDOK, (idFieldRecoveryType|dataFieldRecoveryType)!=0 );
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
																		if (dosProps!=&CUnknownDos::Properties)
																			dataFieldSubstituteFillerByte=dosProps->sectorFillerByte;
																		else
																			Utils::RandomizeData( &dataFieldSubstituteFillerByte, sizeof(dataFieldSubstituteFillerByte) );
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
														: Utils::CRideDialog(IDR_IMAGE_DUMP_ERROR_RESOLUTION)
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
															::memset( sectorData, d.dataFieldSubstituteFillerByte, sectorLength );
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
											case RESOLVE_EXCLUDE_ID:{
												// Sector exclusion
												EndDialog(RESOLVE_EXCLUDE_ID);
												break;
											}
										}
										break;
								}
								return __super::WindowProc(msg,wParam,lParam);
							}
						public:
							CErroneousSectorDialog(const TDumpParams &dp,TParams &_rParams,PSectorData sectorData,WORD sectorLength,TFdcStatus &rFdcStatus)
								// ctor
								: Utils::CRideDialog(IDR_IMAGE_DUMP_ERROR)
								, dp(dp) , rp(_rParams) , sectorData(sectorData) , sectorLength(sectorLength) , rFdcStatus(rFdcStatus) {
							}
						} d(dp,p,bufferSectorData[s],bufferLength[s],bufferFdcStatus[s]);
						// | showing the Dialog and processing its result
						LOG_DIALOG_DISPLAY(_T("CErroneousSectorDialog"));
						switch (LOG_DIALOG_RESULT(d.DoModal())){
							case IDCANCEL:
								err=ERROR_CANCELLED;
								goto terminateWithError;
							case RESOLVE_EXCLUDE_ID:
								nSectors--;
								::memmove( bufferId+s, bufferId+s+1, sizeof(*bufferId)*(nSectors-s) );
								::memmove( bufferLength+s, bufferLength+s+1, sizeof(*bufferLength)*(nSectors-s) );
								//fallthrough
							case IDRETRY:
								continue;
						}
					}
					if (!bufferFdcStatus[s].IsWithoutError()){
						TDumpParams::TSourceSectorError *const psse=&erroneousSectors.buffer[erroneousSectors.n++];
						psse->id=p.chs.sectorId, psse->fdcStatus=bufferFdcStatus[s];
					}
					// : next SourceSector
					s++;
				}
}
				p.acceptance.remainingErrorsOnTrack=false; // "True" valid only for Track it was set on
				// . formatting Target Track
{LOG_TRACK_ACTION(p.chs.cylinder,p.chs.head,_T("formatting target"));
				if (dp.formatJustBadTracks && dp.source->IsTrackHealthy(p.chs.cylinder,p.chs.head)){
					if (!dp.gap3.valueValid){ // "real" Gap3 Value (i.e. the one that was used when previously formatting the disk) not yet determined
						if (dp.target->ScanTrack( p.chs.cylinder, p.chs.head, nullptr, nullptr, nullptr, nullptr, &dp.gap3.value )) // if there are some Sectors on the Target Track ...
							if (dp.target->IsTrackHealthy(p.chs.cylinder,p.chs.head)){ // ... and all of them are well readable ...
								#ifdef LOGGING_ENABLED
									LOG_TRACK_ACTION(p.chs.cylinder,p.chs.head,_T("Avg target image Gap3"));
									TCHAR buf[80];
									::wsprintf(buf,_T("dp.gap3.value=%d"),dp.gap3.value);
									LOG_MESSAGE(buf);
								#endif
								dp.gap3.valueValid=true; // ... then the Gap3 Value found valid and can be used as a reference value for working with the Target Image
							}
					}
					if (dp.target->PresumeHealthyTrackStructure(p.chs.cylinder,p.chs.head,nSectors,bufferId,dp.gap3.value,dp.fillerByte)!=ERROR_SUCCESS)
						goto reformatTrack;
				}else
reformatTrack:		if ( err=dp.target->FormatTrack(p.chs.cylinder,p.chs.head,codec!=Codec::UNKNOWN?codec:dp.dos->formatBoot.codecType,nSectors,bufferId,bufferLength,bufferFdcStatus,dp.gap3.value,dp.fillerByte) )
						goto terminateWithError;
}
				// . writing to Target Track
{LOG_TRACK_ACTION(p.chs.cylinder,p.chs.head,_T("writing to Target Track"));
				dp.target->BufferTrackData( p.chs.cylinder, p.chs.head, bufferId, sectorIdAndPositionIdentity, nSectors, true ); // make Sectors data ready for IMMEDIATE usage
				for( BYTE s=0; s<nSectors; ){
					if (!bufferFdcStatus[s].DescribesMissingDam()){
						p.chs.sectorId=bufferId[s]; WORD w;
						LOG_SECTOR_ACTION(&p.chs.sectorId,_T("writing"));
						if (const PSectorData targetData=dp.target->GetSectorData(p.chs,s,true,&w,&TFdcStatus())){
							::memcpy( targetData, bufferSectorData[s], bufferLength[s] );
							if (( err=dp.target->MarkSectorAsDirty(p.chs,s,bufferFdcStatus+s) )!=ERROR_SUCCESS)
								goto errorDuringWriting;
						}else{
							err=::GetLastError();
errorDuringWriting:			TCHAR buf[80];
							::wsprintf(buf,_T("Cannot write to sector with %s on target Track %d"),(LPCTSTR)p.chs.sectorId.ToString(),p.track);
							switch (Utils::AbortRetryIgnore(buf,err,MB_DEFBUTTON2)){
								case IDABORT:	goto terminateWithError;
								case IDRETRY:	continue;
								case IDIGNORE:	break;
							}
						}
					}
					s++; // cannot include in the FOR clause - see Continue statement in the cycle
				}
				// . saving the writing to the Target Track (if the Target Image supports it)
				switch ( err=dp.target->SaveTrack(p.chs.cylinder,p.chs.head) ){
					case ERROR_SUCCESS:
						//fallthrough
					case ERROR_NOT_SUPPORTED:
						break; // writings to the Target Image will be saved later via CImage::OnSaveDocument
					default:
						goto terminateWithError;
				}
				// . registering Track with ErroneousSectors
//Utils::Information("registering Track with ErroneousSectors");
				if (erroneousSectors.n){
					TDumpParams::TSourceTrackErrors *psse=(TDumpParams::TSourceTrackErrors *)::malloc(sizeof(TDumpParams::TSourceTrackErrors)+(erroneousSectors.n-1)*sizeof(TDumpParams::TSourceSectorError));
						psse->cyl=p.chs.cylinder, psse->head=p.chs.head;
						psse->pNextErroneousTrack=nullptr;
						::memcpy( psse->erroneousSectors, erroneousSectors.buffer, ( psse->nErroneousSectors=erroneousSectors.n )*sizeof(TDumpParams::TSourceSectorError) );
					*ppSrcTrackErrors=psse, ppSrcTrackErrors=&psse->pNextErroneousTrack;
				}
}
			}
		return ERROR_SUCCESS;
	}

	void CImage::Dump() const{
		// dumps this Image to a chosen target
		const PDos dos=GetActive()->dos;
		// - defining the Dialog
		class CDumpDialog sealed:public Utils::CRideDialog{
			const PDos dos;

			void PreInitDialog() override{
				// dialog initialization
				// . base
				__super::PreInitDialog();
				// . adjusting text in button next to FillerByte edit box
				if (dos->properties==&CUnknownDos::Properties)
					SetDlgItemText( ID_DEFAULT1, _T("Random value") );
			}
			void DoDataExchange(CDataExchange *pDX) override{
				// transferring data to and from controls
				const HWND hMedium=GetDlgItemHwnd(ID_MEDIUM);
				if (pDX->m_bSaveAndValidate){
					// : FileName must be known
					pDX->PrepareEditCtrl(ID_FILE);
					if (!::lstrcmp(fileName,ELLIPSIS)){
						Utils::Information( _T("Target not specified.") );
						pDX->Fail();
					}else if (!dos->image->GetPathName().Compare(fileName)){
						Utils::Information( _T("Target must not be the same as source.") );
						pDX->Fail();
					}
				}else{
					SetDlgItemText( ID_FILE, ELLIPSIS );
					PopulateComboBoxWithCompatibleMedia( hMedium, 0, nullptr ); // if FileName not set, Medium cannot be determined
				}
				const Medium::PCProperties mp=	targetImageProperties // ComboBox populated with compatible Media and one of them selected
												? Medium::GetProperties( dumpParams.mediumType=(Medium::TType)GetDlgComboBoxSelectedValue(ID_MEDIUM) )
												: nullptr;
				int i=dumpParams.formatJustBadTracks;
				DDX_Check( pDX, ID_FORMAT, i );
				dumpParams.formatJustBadTracks=i!=0;
				DDX_Text( pDX,	ID_CYLINDER,	(RCylinder)dumpParams.cylinderA );
					if (mp)
						DDV_MinMaxUInt( pDX,dumpParams.cylinderA, 0, mp->cylinderRange.iMax-1 );
				DDX_Text( pDX,	ID_CYLINDER_N,	(RCylinder)dumpParams.cylinderZ );
					if (mp)
						DDV_MinMaxUInt( pDX,dumpParams.cylinderZ, dumpParams.cylinderA, mp->cylinderRange.iMax-1 );
				DDX_Text( pDX,	ID_HEAD,		dumpParams.nHeads );
					if (mp)
						DDV_MinMaxUInt( pDX,dumpParams.nHeads, 1, mp->headRange.iMax );
				DDX_Text( pDX,	ID_GAP,			dumpParams.gap3.value );
				DDX_Text( pDX,	ID_NUMBER,		dumpParams.fillerByte );
				DDX_Check(pDX,	ID_PRIORITY,	realtimeThreadPriority );
				DDX_Check(pDX,	ID_REPORT,		showReport );
				if (pDX->m_bSaveAndValidate){
					// : instantiating Target Image
					if (targetImageProperties){
						LOG_ACTION(_T("Creating target image"));
						dumpParams.target=std::unique_ptr<CImage>( targetImageProperties->fnInstantiate(fileName) );
					}else{
						Utils::FatalError(_T("Unknown destination to dump to."));
						return pDX->Fail();
					}
					// : suggesting to dump only Cylinders within the officially reported Format (where suitable to)
					if (
						#ifndef _DEBUG // checking if the Source is a floppy only in Release mode
							dumpParams.source->properties->IsRealDevice()
							&&
						#endif
						dynamic_cast<CImageRaw *>(dumpParams.target.get())!=nullptr
					)
						// suggestion is helpfull only if dumping a floppy to an image with invariant structure (as Cylinders outside the official Format may have different layout, causing an "Device doesn't recognize this command" error during formatting the raw Image)
						if (dos->properties!=&CUnknownDos::Properties) // Unknown DOS doesn't have valid Format information
							if (dumpParams.cylinderA || dumpParams.cylinderZ>=dos->formatBoot.nCylinders){ // ">=" = Cylinders are numbered from 0
								// : defining the Dialog
								class CSuggestionDialog sealed:public Utils::CCommandDialog{
									BOOL OnInitDialog() override{
										// dialog initialization
										// : base
										const BOOL result=__super::OnInitDialog();
										// : supplying available actions
										__addCommandButton__( IDYES, _T("Continue with format adopted from boot sector (recommended)") );
										__addCommandButton__( IDNO, _T("Continue with current settings (cylinders beyond official format may fail!)") );
										__addCommandButton__( IDCANCEL, _T("Return to dump dialog") );
										return result;
									}
								public:
									CSuggestionDialog()
										// ctor
										: Utils::CCommandDialog(_T("Dumping cylinders outside official format to a raw image may fail!")) {
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
					// : Gap3 Value is Valid only if requesting to format each Track (that is, not just the bad ones)
					dumpParams.gap3.valueValid=!dumpParams.formatJustBadTracks;
				}
			}
			afx_msg void OnPaint(){
				// painting
				// . base
				__super::OnPaint();
				// . painting curly brackets
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
								if (targetImageProperties=app.DoPromptFileName( fileName, true, AFX_IDS_SAVEFILE, 0, nullptr )){
									// : compacting FileName in order to be better displayable on the button
									CWnd *const pBtnFile=GetDlgItem(ID_FILE);
									RECT r;
									pBtnFile->GetClientRect(&r);
									TCHAR buf[MAX_PATH];
									::PathCompactPath( CClientDC(pBtnFile), ::lstrcpy(buf,fileName), r.right-r.left );
									pBtnFile->SetWindowText(buf);
									// : adjusting interactivity
										// > populating ComboBox with Media supported by both DOS and Image
										BYTE nCompatibleMedia;
										if (dos->formatBoot.mediumType&Medium::FLOPPY_ANY)
											// source Image is a floppy - enabling dumping to any kind of a floppy (motivation: some copy-protection schemes feature misleading information on the kind of floppy; e.g., "Teen Agent" [or "Agent mlicnak"] installation disk #2 and #3 are introduced as 2DD floppies while they really are HD!)
											nCompatibleMedia=PopulateComboBoxWithCompatibleMedia(
												GetDlgItemHwnd(ID_MEDIUM),
												dos->properties->supportedMedia&Medium::FLOPPY_ANY,
												targetImageProperties
											);
										else
											// source Image is a hard-disk
											nCompatibleMedia=PopulateComboBoxWithCompatibleMedia(
												GetDlgItemHwnd(ID_MEDIUM),
												dos->formatBoot.mediumType,
												targetImageProperties
											);
										// > enabling/disabling controls
										static const WORD Controls[]={ ID_CYLINDER, ID_CYLINDER_N, ID_HEAD, ID_GAP, ID_NUMBER, ID_DEFAULT1, IDOK, 0 };
										CheckDlgButton(
											ID_FORMAT,
											EnableDlgItem(
												ID_FORMAT,
												EnableDlgItems( Controls, nCompatibleMedia>0 )  &&  targetImageProperties->IsRealDevice()
											)
										);
										FocusDlgItem(IDOK);
										// > automatically ticking the "Real-time thread priority" check-box if either the source or the target is a real drive
										if (dos->image->properties->IsRealDevice() || targetImageProperties->IsRealDevice())
											SendDlgItemMessage( ID_PRIORITY, BM_SETCHECK, BST_CHECKED );
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
							class CHelpDialog sealed:public Utils::CCommandDialog{
								BOOL OnInitDialog() override{
									// dialog initialization
									// : base
									const BOOL result=__super::OnInitDialog();
									// : supplying available actions
									__addCommandButton__( ID_IMAGE, _T("How do I create an image of a real floppy disk?") );
									__addCommandButton__( ID_DRIVE, _T("How do I save an image back to a real floppy disk?") );
									__addCommandButton__( ID_FILE, _T("How do I convert between two images? (E.g. *.IMA to *.DSK)") );
									__addCommandButton__( ID_ACCURACY, _T("How do I create an exact copy of a real floppy disk?") );
									__addCommandButton__( IDCANCEL, MSG_HELP_CANCEL );
									return result;
								}
							public:
								CHelpDialog()
									// ctor
									: Utils::CCommandDialog(_T("Dumping is used to convert between data containers.")) {
								}
							} d;
							// . showing the Dialog and processing its result
							TCHAR url[200];
							switch (d.DoModal()){
								case ID_IMAGE:
									Utils::NavigateToUrlInDefaultBrowser( Utils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_floppy2image.html"),url) );
									break;
								case ID_DRIVE:
									Utils::NavigateToUrlInDefaultBrowser( Utils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_image2floppy.html"),url) );
									break;
								case ID_FILE:
									Utils::NavigateToUrlInDefaultBrowser( Utils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_convertImage.html"),url) );
									break;
								case ID_ACCURACY:
									Utils::NavigateToUrlInDefaultBrowser( Utils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_copyFloppy.html"),url) );
									break;
							}
						}
						break;
				}
				return __super::WindowProc(msg,wParam,lParam);
			}
		public:
			TCHAR fileName[MAX_PATH];
			CImage::PCProperties targetImageProperties;
			TDumpParams dumpParams;
			int realtimeThreadPriority,showReport;

			CDumpDialog(PDos _dos)
				// ctor
				: Utils::CRideDialog(IDR_IMAGE_DUMP)
				, dos(_dos) , targetImageProperties(nullptr) , dumpParams(_dos)
				, realtimeThreadPriority(BST_UNCHECKED)
				, showReport(BST_CHECKED) {
				::lstrcpy( fileName, ELLIPSIS );
			}
		} d(dos);
		// - showing Dialog and processing its result
		if (d.DoModal()==IDOK){
			// . resetting Target Image
			d.dumpParams.target->dos=dos;
			TStdWinError err=ERROR_CANCELLED;
			if (d.dumpParams.target->IsWriteProtected()){
				d.dumpParams.target->ToggleWriteProtection();
				if (d.dumpParams.target->IsWriteProtected()){
					err=ERROR_WRITE_PROTECT;
					goto error;
				}
			}
			if (!d.dumpParams.target->EditSettings(true))
				goto error;
			err=d.dumpParams.target->Reset();
			if (err!=ERROR_SUCCESS)
				goto error;
			// . dumping
			err=CBackgroundActionCancelable(
					__dump_thread__,
					&d.dumpParams,
					d.realtimeThreadPriority ? THREAD_PRIORITY_TIME_CRITICAL : THREAD_PRIORITY_NORMAL
				).Perform();
			if (err==ERROR_SUCCESS){
				if (d.dumpParams.target->OnSaveDocument(d.fileName)){
					// : displaying statistics on SourceTrackErrors
					if (d.showReport==BST_CHECKED){
						// | saving to temporary file
						TCHAR tmpFileName[MAX_PATH];
						::GetTempPath(MAX_PATH,tmpFileName);
						::GetTempFileName( tmpFileName, nullptr, FALSE, tmpFileName );
						d.dumpParams.__exportErroneousTracksToHtml__( CFile(::lstrcat(tmpFileName,_T(".html")),CFile::modeCreate|CFile::modeWrite) );
						// | displaying
						((CMainWindow *)app.m_pMainWnd)->OpenWebPage( _T("Dump results"), tmpFileName );
					}
					// : reporting success
					Utils::Information(_T("Dumped successfully."));
				}else
					Utils::FatalError(_T("Cannot save to the target"),::GetLastError());
			}else
error:			Utils::FatalError(_T("Cannot dump"),err);
			// . destroying the list of SourceTrackErrors
			while (const TDumpParams::TSourceTrackErrors *tmp=d.dumpParams.pOutErroneousTracks)
				d.dumpParams.pOutErroneousTracks=d.dumpParams.pOutErroneousTracks->pNextErroneousTrack, ::free((PVOID)tmp);
		}
	}
