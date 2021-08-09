#include "stdafx.h"

	#define INI_DUMP	_T("Dump")

	struct TDumpParams sealed{
		#pragma pack(1)
		typedef const struct TSourceSectorError sealed{
			TSectorId id;
			TFdcStatus fdcStatus;
			bool excluded;
		} *PCSourceSectorError;

		const CDos *const dos;
		Medium::TType mediumType;
		const PImage source;
		std::unique_ptr<CImage> target;
		TCHAR targetFileName[MAX_PATH];
		bool formatJustBadTracks, fullTrackAnalysis;
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
			bool hasNonformattedArea, hasDataInGaps, hasFuzzyData;
			const TSourceTrackErrors *pNextErroneousTrack;
			TSector nErroneousSectors;
			TSourceSectorError erroneousSectors[1];
		} *pOutErroneousTracks;

		TDumpParams(PDos _dos)
			// ctor
			: dos(_dos)
			, source(dos->image)
			, formatJustBadTracks(false)
			, fullTrackAnalysis( source->ReadTrack(0,0) ) // if the Source provides access to low-level recording, let's also do the FullTrackAnalysis
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
						Utils::WriteToFile(fHtml,_T("<table><tr><td class=caption>Status</td><td class=caption>Count</td></tr>"));
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
							int nWarnings=0;
							for( const TSourceTrackErrors *pErroneousTrack=pOutErroneousTracks; pErroneousTrack; pErroneousTrack=pErroneousTrack->pNextErroneousTrack ){
								nWarnings+=pErroneousTrack->hasNonformattedArea;
								nWarnings+=pErroneousTrack->hasDataInGaps;
								nWarnings+=pErroneousTrack->hasFuzzyData;
							}
							Utils::WriteToFile(fHtml,_T("<tr><td>Warning</td><td align=right>"));
								Utils::WriteToFile(fHtml,nWarnings);
							Utils::WriteToFile(fHtml,_T("</td></tr>"));							
						Utils::WriteToFile(fHtml,_T("</table>"));
					}else
						Utils::WriteToFile(fHtml,_T("No errors occurred."));
				Utils::WriteToFile(fHtml,_T("<h3>Details</h3>"));
					if (pOutErroneousTracks){
						Utils::WriteToFile(fHtml,_T("<table><tr><td class=caption width=120>Track</td><td class=caption>Errors</td></tr>"));
							for( const TSourceTrackErrors *pErroneousTrack=pOutErroneousTracks; pErroneousTrack; pErroneousTrack=pErroneousTrack->pNextErroneousTrack ){
								Utils::WriteToFile(fHtml,_T("<tr><td>"));
									Utils::WriteToFileFormatted( fHtml, _T("Cyl %d, Head %d"), pErroneousTrack->cyl, pErroneousTrack->head );
								Utils::WriteToFile(fHtml,_T("</td><td><ul>"));
									if (BYTE n=pErroneousTrack->nErroneousSectors)
										for( PCSourceSectorError psse=pErroneousTrack->erroneousSectors; n; n--,psse++ ){
											Utils::WriteToFile(fHtml,_T("<li>"));
												Utils::WriteToFileFormatted( fHtml, _T("%s<b>%s</b>. "), psse->excluded?_T("Excluded "):_T(""), (LPCTSTR)psse->id.ToString() );
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
									else
										Utils::WriteToFile(fHtml,_T("<li>All sectors ok.</li>"));
									if (pErroneousTrack->hasNonformattedArea)
										Utils::WriteToFile(fHtml,_T("<li><b>Warning</b>: Significant non-formatted area.</li>"));
									if (pErroneousTrack->hasDataInGaps)
										Utils::WriteToFile(fHtml,_T("<li><b>Warning</b>: Suspected data in gap.</li>"));
									if (pErroneousTrack->hasFuzzyData)
										Utils::WriteToFile(fHtml,_T("<li><b>Warning</b>: Always bad fuzzy data.</li>"));
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

	#define RESOLVE_OPTIONS_COUNT	5
	#define RESOLVE_EXCLUDE_ID		IDIGNORE
	#define RESOLVE_EXCLUDE_UNKNOWN	IDCONTINUE

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
		dp.target->SetPathName( dp.targetFileName, FALSE );
		// - dumping
		const TDumpParams::TSourceTrackErrors **ppSrcTrackErrors=&dp.pOutErroneousTracks;
		#pragma pack(1)
		struct TParams sealed{
			TPhysicalAddress chs;
			TTrack track;
			bool trackWriteable; // Track can be written at once using CImage::WriteTrack
			BYTE nTrials;
			struct{
				WORD automaticallyAcceptedErrors;
				bool remainingErrorsOnTrack;
			} acceptance;
			struct{
				bool current;
				bool allUnknown;
			} exclusion;
		} p;
		::ZeroMemory(&p,sizeof(p));
		const bool targetSupportsTrackWriting=dp.target->WriteTrack(0,0,CImage::CTrackReaderWriter::Invalid)!=ERROR_NOT_SUPPORTED;
		const Utils::CByteIdentity sectorIdAndPositionIdentity;
		TPhysicalAddress chsPrev=TPhysicalAddress::Invalid;
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
				CImage::CTrackReader trSrc=dp.source->ReadTrack( p.chs.cylinder, p.chs.head );
				const CImage::CTrackReader &tr= targetSupportsTrackWriting ? trSrc : CImage::CTrackReaderWriter::Invalid;
				p.trackWriteable=tr;
				// . if possible, analyzing the read Source Track
				bool hasNonformattedArea=false, hasDataInGaps=false, hasFuzzyData=false;
				if (trSrc && dp.fullTrackAnalysis){
					TSectorId ids[Revolution::MAX*(TSector)-1]; TLogTime idEnds[Revolution::MAX*(TSector)-1]; CImage::CTrackReader::TProfile idProfiles[Revolution::MAX*(TSector)-1]; TFdcStatus idStatuses[Revolution::MAX*(TSector)-1];
					CImage::CTrackReader::CParseEventList peTrack;
					trSrc.ScanAndAnalyze( ids, idEnds, idProfiles, idStatuses, peTrack );
					hasNonformattedArea=peTrack.Contains( CImage::CTrackReader::TParseEvent::NONFORMATTED );
					hasDataInGaps=peTrack.Contains( CImage::CTrackReader::TParseEvent::DATA_IN_GAP );
					hasFuzzyData=peTrack.Contains( CImage::CTrackReader::TParseEvent::FUZZY_BAD );
				}
				// . reading individual Sectors
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
					if (p.chs==chsPrev && dp.source->GetAvailableRevolutionCount()<=Revolution::MAX)
						p.nTrials++;
					else
						p.nTrials=1;
					chsPrev=p.chs;
					LOG_SECTOR_ACTION(&p.chs.sectorId,_T("reading"));
					bufferSectorData[s]=dp.source->GetSectorData( p.chs, s, true, bufferLength+s, bufferFdcStatus+s );
					// : reporting SourceSector Exclusion
					p.exclusion.current|= p.exclusion.allUnknown && dp.dos->GetSectorStatus(p.chs)==CDos::TSectorStatus::UNKNOWN;
					if (p.exclusion.current){
						nSectors--;
						::memmove( bufferId+s, bufferId+s+1, sizeof(*bufferId)*(nSectors-s) );
						::memmove( bufferLength+s, bufferLength+s+1, sizeof(*bufferLength)*(nSectors-s) );
						p.trackWriteable=false; // once modified, can't write the Track as a whole anymore
						s--; // as below incremented
					// : reporting SourceSector Errors if A&B, A = automatically not accepted Errors exist, B = Error reporting for current Track is enabled
					}else if (bufferFdcStatus[s].ToWord()&~p.acceptance.automaticallyAcceptedErrors && !p.acceptance.remainingErrorsOnTrack){
						// | Dialog definition
						class CErroneousSectorDialog sealed:public Utils::CRideDialog{
							const TDumpParams &dp;
							TParams &rp;
							const PSectorData sectorData;
							const WORD sectorLength;
							TFdcStatus &rFdcStatus;
							Utils::TSplitButtonAction resolveActions[RESOLVE_OPTIONS_COUNT];

							void PreInitDialog() override{
								// dialog initialization
								// > base
								__super::PreInitDialog();
								// > creating message on Errors
								LPCTSTR bitDescriptions[20],*pDesc=bitDescriptions; // 20 = surely big enough buffer
								rFdcStatus.GetDescriptionsOfSetBits(bitDescriptions);
								TCHAR buf[512],*p=buf+::wsprintf(buf,_T("Cannot read sector with %s on source Track %d.\r\n"),(LPCTSTR)rp.chs.sectorId.ToString(),rp.track);
								const bool onlyPartlyRecoverable=( rFdcStatus.ToWord() & ~(TFdcStatus::DataFieldCrcError.ToWord()|TFdcStatus::IdFieldCrcError.ToWord()) )!=0;
								if (onlyPartlyRecoverable)
									p+=::lstrlen( ::lstrcpy(p,_T("SOME ERRORS CAN'T BE FIXED!\r\n")) );
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
								static constexpr Utils::TSplitButtonAction Actions[ACCEPT_OPTIONS_COUNT]={
									{ ACCEPT_ERROR_ID, _T("Accept error") },
									{ ID_ERROR, _T("Accept all errors of this kind") },
									{ ID_TRACK, _T("Accept all errors in this track") },
									{ ID_IMAGE, _T("Accept all errors on the disk") }
								};
								ConvertDlgButtonToSplitButton( IDOK, Actions, ACCEPT_OPTIONS_COUNT );
								EnableDlgItem( IDOK, dynamic_cast<CImageRaw *>(dp.target.get())==nullptr ); // accepting errors is allowed only if the Target Image can accept them
								// > converting the "Resolve" button to a SplitButton
								static constexpr Utils::TSplitButtonAction ResolveActions[RESOLVE_OPTIONS_COUNT]={
									{ 0, _T("Resolve") }, // 0 = no default action
									{ RESOLVE_EXCLUDE_ID, _T("Exclude from track") },
									{ RESOLVE_EXCLUDE_UNKNOWN, _T("Exclude all unknown from disk") },
									{ ID_DATAFIELD_CRC, _T("Fix Data CRC only") },
									{ ID_RECOVER, _T("Fix ID or Data...") }
								};
								::memcpy( resolveActions, ResolveActions, sizeof(ResolveActions) );
									if (onlyPartlyRecoverable)
										resolveActions->commandCaption=_T("Resolve partly");
									resolveActions[3].menuItemFlags=MF_GRAYED*( rFdcStatus.DescribesMissingDam() || !rFdcStatus.DescribesDataFieldCrcError() ); // disabled if the Data CRC ok
									resolveActions[4].menuItemFlags=MF_GRAYED*( rFdcStatus.DescribesMissingDam() || !rFdcStatus.DescribesIdFieldCrcError()&&!rFdcStatus.DescribesDataFieldCrcError() ); // enabled only if either ID or Data field with error
								ConvertDlgButtonToSplitButton( IDNO, resolveActions, RESOLVE_OPTIONS_COUNT );
								EnableDlgItem( IDNO, dynamic_cast<CImageRaw *>(dp.target.get())==nullptr ); // recovering errors is allowed only if the Target Image can accept them
								// > the "Retry" button enabled only if not all Revolutions yet exhausted
								EnableDlgItem( IDRETRY, rp.nTrials<dp.source->GetAvailableRevolutionCount() );
							}
							LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
								// window procedure
								switch (msg){
									case WM_COMMAND:
										switch (wParam){
											case IDRETRY:
												UpdateData(TRUE);
												EndDialog(wParam);
												return 0;
											case ID_ERROR:
												rp.acceptance.automaticallyAcceptedErrors|=rFdcStatus.ToWord();
												UpdateData(TRUE);
												EndDialog(ACCEPT_ERROR_ID);
												return 0;
											case ID_IMAGE:
												rp.acceptance.automaticallyAcceptedErrors=-1;
												//fallthrough
											case ID_TRACK:
												rp.acceptance.remainingErrorsOnTrack=true;
												UpdateData(TRUE);
												EndDialog(ACCEPT_ERROR_ID);
												return 0;
											case ID_DATAFIELD_CRC:
												// recovering CRC
												rFdcStatus.CancelDataFieldCrcError();
												rp.trackWriteable=false; // once modified, can't write the Track as a whole anymore
												UpdateData(TRUE);
												EndDialog(ACCEPT_ERROR_ID);
												return 0;
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
															static constexpr WORD IdFieldRecoveryOptions[]={ ID_IDFIELD, ID_IDFIELD_CRC, ID_IDFIELD_REPLACE, 0 };
															EnableDlgItems( IdFieldRecoveryOptions, rFdcStatus.DescribesIdFieldCrcError() );
															static constexpr WORD IdFieldReplaceOption[]={ ID_IDFIELD_VALUE, ID_DEFAULT1, 0 };
															EnableDlgItems( IdFieldReplaceOption, idFieldRecoveryType==2 );
														// | "Data Field" region
														DDX_Radio( pDX, ID_DATAFIELD, dataFieldRecoveryType );
															DDX_Text( pDX, ID_DATAFIELD_FILLERBYTE, dataFieldSubstituteFillerByte );
															if (dosProps==&CUnknownDos::Properties)
																SetDlgItemText( ID_DEFAULT2, _T("Random value") );
															static constexpr WORD DataFieldRecoveryOptions[]={ ID_DATAFIELD, ID_DATAFIELD_CRC, ID_DATAFIELD_REPLACE, 0 };
															EnableDlgItems( DataFieldRecoveryOptions, rFdcStatus.DescribesDataFieldCrcError() );
															static constexpr WORD DataFieldReplaceOption[]={ ID_DATAFIELD_FILLERBYTE, ID_DEFAULT2, 0 };
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
													CSectorRecoveryDialog(CWnd *pParentWnd,const CDos *dos,const TParams &rParams,const TFdcStatus &rFdcStatus)
														: Utils::CRideDialog( IDR_IMAGE_DUMP_ERROR_RESOLUTION, pParentWnd )
														, dosProps(dos->properties) , rParams(rParams) , rFdcStatus(rFdcStatus)
														, idFieldRecoveryType(0) , idFieldSubstituteSectorId(rParams.chs.sectorId)
														, dataFieldRecoveryType(0) , dataFieldSubstituteFillerByte(dos->properties->sectorFillerByte) {
													}
												} d(this,dp.dos,rp,rFdcStatus);
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
															rp.trackWriteable=false; // once modified, can't write the Track as a whole anymore
															break;
													}
													switch (d.dataFieldRecoveryType){
														case 2:
															// replacing data with SubstituteByte
															::memset( sectorData, d.dataFieldSubstituteFillerByte, sectorLength );
															//fallthrough
														case 1:
															// recovering CRC
															SendMessage( WM_COMMAND, ID_DATAFIELD_CRC );
															break;
													}
													EndDialog(ACCEPT_ERROR_ID);
													return 0;
												}
												break;
											}
											case RESOLVE_EXCLUDE_UNKNOWN:
												// exclusion of this and all future Unknown Sectors from the disk
												rp.exclusion.allUnknown=true;
												//fallthrough
											case RESOLVE_EXCLUDE_ID:
												// exclusion of this Sector
												EndDialog(RESOLVE_EXCLUDE_ID);
												return 0;
										}
										break;
								}
								return __super::WindowProc(msg,wParam,lParam);
							}
						public:
							CErroneousSectorDialog(CWnd *pParentWnd,const TDumpParams &dp,TParams &_rParams,PSectorData sectorData,WORD sectorLength,TFdcStatus &rFdcStatus)
								// ctor
								: Utils::CRideDialog( IDR_IMAGE_DUMP_ERROR, pParentWnd )
								, dp(dp) , rp(_rParams) , sectorData(sectorData) , sectorLength(sectorLength) , rFdcStatus(rFdcStatus) {
							}
						} d(pAction,dp,p,bufferSectorData[s],bufferLength[s],bufferFdcStatus[s]);
						// | showing the Dialog and processing its result
						LOG_DIALOG_DISPLAY(_T("CErroneousSectorDialog"));
						switch (LOG_DIALOG_RESULT(d.DoModal())){
							case IDCANCEL:
								err=ERROR_CANCELLED;
								goto terminateWithError;
							case RESOLVE_EXCLUDE_ID:
								p.exclusion.current=true;
								//fallthrough
							case IDRETRY:
								continue;
						}
					}
					if (!bufferFdcStatus[s].IsWithoutError()){
						TDumpParams::TSourceSectorError *const psse=&erroneousSectors.buffer[erroneousSectors.n++];
						psse->id=p.chs.sectorId, psse->fdcStatus=bufferFdcStatus[s];
						psse->excluded=p.exclusion.current;
					}
					// : next SourceSector
					p.exclusion.current=false;
					s++;
				}
}
				p.acceptance.remainingErrorsOnTrack=false; // "True" valid only for Track it was set on
				// . formatting Target Track
{LOG_TRACK_ACTION(p.chs.cylinder,p.chs.head,_T("formatting target"));
				if (dp.formatJustBadTracks && dp.source->IsTrackHealthy(p.chs.cylinder,p.chs.head)){
					if (!dp.gap3.valueValid){ // "real" Gap3 Value (i.e. the one that was used when previously formatting the disk) not yet determined
						BYTE tmpGap3;
						TSectorId targetSectorIds[(TSector)-1];
						if (TSector nTargetSectors=dp.target->ScanTrack( p.chs.cylinder, p.chs.head, nullptr, targetSectorIds, nullptr, nullptr, &tmpGap3 )) // if there are some Sectors on the Target Track ...
							if (nSectors==nTargetSectors && dp.target->IsTrackHealthy(p.chs.cylinder,p.chs.head)){ // ... and all of them are well readable ...
								// : composing a record of Source Track structure
								CMapStringToPtr sectorIdCounts; // key = Sector ID, value = count
								for( TSector i=0; i<nSectors; i++ ){
									const CString strId=bufferId[i].ToString();
									PVOID count=nullptr;
									sectorIdCounts.Lookup( strId, count );
									sectorIdCounts.SetAt( strId, (PVOID)((LONG_PTR)count+1) );
								}
								// : comparing the record with already formatted Target Track structure
								while (nTargetSectors>0){
									const CString strId=targetSectorIds[--nTargetSectors].ToString();
									PVOID count=nullptr;
									sectorIdCounts.Lookup( strId, count );
									sectorIdCounts.SetAt( strId, (PVOID)((LONG_PTR)count-1) );
								}
								bool structuresIdentical=true; // assumption (both Source and Target Track structures are identical)
								for( POSITION pos=sectorIdCounts.GetStartPosition(); pos; ){
									PVOID count;
									sectorIdCounts.GetNextAssoc( pos, CString(), count );
									structuresIdentical=count==nullptr;
								}
								// : adopting recognized Gap3
								if (structuresIdentical){
									#ifdef LOGGING_ENABLED
										LOG_TRACK_ACTION(p.chs.cylinder,p.chs.head,_T("Avg target image Gap3"));
										TCHAR buf[80];
										::wsprintf(buf,_T("dp.gap3.value=%d"),dp.gap3.value);
										LOG_MESSAGE(buf);
									#endif
									dp.gap3.value=tmpGap3;
									dp.gap3.valueValid=true; // ... then the Gap3 Value found valid and can be used as a reference value for working with the Target Image
								}
							}
					}
					if (dp.target->PresumeHealthyTrackStructure(p.chs.cylinder,p.chs.head,nSectors,bufferId,dp.gap3.value,dp.fillerByte)!=ERROR_SUCCESS)
						goto reformatTrack;
				}else
reformatTrack:		if (!p.trackWriteable) // formatting the Track only if can't write the Track using CImage::WriteTrack
						if ( err=dp.target->FormatTrack(p.chs.cylinder,p.chs.head,codec!=Codec::UNKNOWN?codec:dp.dos->formatBoot.codecType,nSectors,bufferId,bufferLength,bufferFdcStatus,dp.gap3.value,dp.fillerByte) )
							goto terminateWithError;
}
				// . writing to Target Track
{LOG_TRACK_ACTION(p.chs.cylinder,p.chs.head,_T("writing to Target Track"));
				if (p.trackWriteable)
					// can use the CImage::WriteTrack to write the whole Track at once
					while (err=dp.target->WriteTrack( p.chs.cylinder, p.chs.head, tr )){
						TCHAR buf[80];
						::wsprintf( buf, _T("Can't write target Track %d"), p.track );
						switch (Utils::AbortRetryIgnore(buf,err,MB_DEFBUTTON2)){
							default:		goto terminateWithError;
							case IDRETRY:	continue;
							case IDIGNORE:	break;
						}
						break;
					}
				else{
					// must write to each Sector individually
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
errorDuringWriting:				TCHAR buf[80];
								::wsprintf(buf,_T("Cannot write to sector with %s on target Track %d"),(LPCTSTR)p.chs.sectorId.ToString(),p.track);
								switch (Utils::AbortRetryIgnore(buf,err,MB_DEFBUTTON2)){
									default:		goto terminateWithError;
									case IDRETRY:	continue;
									case IDIGNORE:	break;
								}
							}
						}
						s++; // cannot include in the FOR clause - see Continue statement in the cycle
					}
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
				if (hasNonformattedArea || hasDataInGaps || hasFuzzyData || erroneousSectors.n){
					TDumpParams::TSourceTrackErrors *psse=(TDumpParams::TSourceTrackErrors *)::malloc(sizeof(TDumpParams::TSourceTrackErrors)+std::max(0,erroneousSectors.n-1)*sizeof(TDumpParams::TSourceSectorError));
						psse->cyl=p.chs.cylinder, psse->head=p.chs.head;
						psse->hasNonformattedArea=hasNonformattedArea;
						psse->hasDataInGaps=hasDataInGaps;
						psse->hasFuzzyData=hasFuzzyData;
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
			CRideApp::CRecentFileListEx mruDevices;
			Utils::TSplitButtonAction actions[10];

			void PreInitDialog() override{
				// dialog initialization
				// . base
				__super::PreInitDialog();
				// . adjusting text in button next to FillerByte edit box
				if (dos->properties==&CUnknownDos::Properties)
					SetDlgItemText( ID_DEFAULT1, _T("Random value") );
				// . showing devices recently dumped to in hidden menu
				static constexpr Utils::TSplitButtonAction OpenDialogAction={ ID_FILE, _T("Select image or device...") };
				Utils::TSplitButtonAction *pAction=actions;
				*pAction++=OpenDialogAction;
				*pAction++=Utils::TSplitButtonAction::HorizontalLine;
				if (!mruDevices[0].IsEmpty())
					for( int i=0; !mruDevices[i].IsEmpty(); i++ ){
						const Utils::TSplitButtonAction item={ ID_FILE_MRU_FIRST+i, mruDevices[i] };
						*pAction++=item;
					}
				else{
					static constexpr Utils::TSplitButtonAction NoMruDevices={ ID_FILE, _T("No recent target devices"), MF_GRAYED };
					*pAction++=NoMruDevices;
				}
				ConvertDlgButtonToSplitButton( ID_FILE, actions, pAction-actions );
			}
			void DoDataExchange(CDataExchange *pDX) override{
				// transferring data to and from controls
				const HWND hMedium=GetDlgItemHwnd(ID_MEDIUM);
				if (pDX->m_bSaveAndValidate){
					// : FileName must be known
					pDX->PrepareEditCtrl(ID_FILE);
					if (!::lstrcmp(dumpParams.targetFileName,ELLIPSIS)){
						Utils::Information( _T("Target not specified.") );
						pDX->Fail();
					}else if (!dos->image->GetPathName().Compare(dumpParams.targetFileName)){
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
				i=dumpParams.fullTrackAnalysis;
					DDX_Check( pDX, ID_ACCURACY, i );
					EnableDlgItem( ID_ACCURACY, dumpParams.fullTrackAnalysis );
				dumpParams.fullTrackAnalysis=i!=0;
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
						dumpParams.target=std::unique_ptr<CImage>( targetImageProperties->fnInstantiate(dumpParams.targetFileName) );
					}else{
						Utils::FatalError(_T("Unknown destination to dump to."));
						return pDX->Fail();
					}
					// : suggesting to dump only Cylinders within the officially reported Format (where suitable to)
					if (
						dos->properties!=&CUnknownDos::Properties // Unknown DOS doesn't have valid Format information
					)
							if (dumpParams.cylinderA || dumpParams.cylinderZ+1!=dos->formatBoot.nCylinders){ // "+1" = Cylinders are numbered from 0
								// : defining the Dialog
								TCHAR caption[200];
								class CSuggestionDialog sealed:public Utils::CCommandDialog{
									BOOL OnInitDialog() override{
										// dialog initialization
										// : base
										const BOOL result=__super::OnInitDialog();
										// : supplying available actions
										__addCommandButton__( IDYES, _T("Continue with format adopted from boot sector (recommended)") );
										TCHAR buf[80];
										::wsprintf( buf, _T("Continue to last occupied Cylinder %d (incl.)"), lastOccupiedCyl );
										__addCommandButton__( IDRETRY, buf );
										__addCommandButton__( IDNO, _T("Continue with current settings (cylinders beyond official format may fail!)") );
										__addCommandButton__( IDCANCEL, _T("Return to dump dialog") );
										return result;
									}
								public:
									const TCylinder lastOccupiedCyl;
						
									CSuggestionDialog(LPCTSTR caption)
										// ctor
										: Utils::CCommandDialog(caption)
										, lastOccupiedCyl( CImage::GetActive()->dos->GetLastOccupiedStdCylinder() ) {
									}
								} d(
									::lstrcat(
										::lstrcpy( caption, _T("Dumped and reported cylinder ranges don't match.") ),
										dumpParams.cylinderZ>=dos->formatBoot.nCylinders && dynamic_cast<CImageRaw *>(dumpParams.target.get())!=nullptr
											? _T(" Dumping cylinders outside official format to a raw image may fail!")
											: _T("")
									)
								);
								// : showing the Dialog and processing its result
								switch (d.DoModal()){
									case IDYES:
										dumpParams.cylinderA=0, dumpParams.cylinderZ=dos->formatBoot.nCylinders-1;
										//fallthrough
									case IDNO:
										break;
									case IDRETRY:
										dumpParams.cylinderA=0, dumpParams.cylinderZ=d.lastOccupiedCyl;
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
								const TCHAR c=*dumpParams.targetFileName;
								*dumpParams.targetFileName='\0';
								if (targetImageProperties=app.DoPromptFileName( dumpParams.targetFileName, true, AFX_IDS_SAVEFILE, 0, nullptr )){
setDestination:						// : compacting FileName in order to be better displayable on the button
									CWnd *const pBtnFile=GetDlgItem(ID_FILE);
									RECT r;
									pBtnFile->GetClientRect(&r);
									TCHAR buf[MAX_PATH+100];
									::PathCompactPath( CClientDC(pBtnFile), ::lstrcpy(buf,dumpParams.targetFileName), r.right-r.left );
									if (!targetImageProperties->IsRealDevice())
										::wsprintf( buf+::lstrlen(buf), _T("\n(%s)"), targetImageProperties->fnRecognize(nullptr) );
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
										// > preselection of current MediumType (if any recognized)
										Medium::TType mt=Medium::UNKNOWN; // assumption (Medium not recognized)
										dos->image->GetInsertedMediumType( 0, mt );
										if (mt!=Medium::UNKNOWN)
											SelectDlgComboBoxValue( ID_MEDIUM, mt );
										// > enabling/disabling controls
										static constexpr WORD Controls[]={ ID_CYLINDER, ID_CYLINDER_N, ID_HEAD, ID_GAP, ID_NUMBER, ID_DEFAULT1, IDOK, 0 };
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
									*dumpParams.targetFileName=c;
								break;
							}
							case ID_DEFAULT1:
								SetDlgItemInt( ID_NUMBER, dos->properties!=&CUnknownDos::Properties ? dos->properties->sectorFillerByte : ::GetTickCount()&0xff );
								break;
							case MAKELONG(ID_CYLINDER,EN_CHANGE):
							case MAKELONG(ID_CYLINDER_N,EN_CHANGE):
								Invalidate();
								break;
							case IDOK:
								if (targetImageProperties && targetImageProperties->IsRealDevice())
									mruDevices.Add( dumpParams.targetFileName, &CUnknownDos::Properties, targetImageProperties );
								break;
							default:
								if (ID_FILE_MRU_FIRST<=wParam && wParam<=ID_FILE_MRU_LAST){
									wParam-=ID_FILE_MRU_FIRST;
									targetImageProperties=mruDevices.GetMruDevice(wParam);
									::lstrcpy( dumpParams.targetFileName, mruDevices[wParam] );
									wParam=ID_FILE;
									goto setDestination;
								}else
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
			CImage::PCProperties targetImageProperties;
			TDumpParams dumpParams;
			int realtimeThreadPriority,showReport;

			CDumpDialog(PDos _dos)
				// ctor
				: Utils::CRideDialog(IDR_IMAGE_DUMP)
				, dos(_dos) , targetImageProperties(nullptr) , dumpParams(_dos)
				, mruDevices( CRecentFileList(0,INI_DUMP,_T("MruDev%d"),4) )
				, realtimeThreadPriority(BST_UNCHECKED)
				, showReport(BST_CHECKED) {
				::lstrcpy( dumpParams.targetFileName, ELLIPSIS );
				mruDevices.ReadList();
			}

			~CDumpDialog(){
				// dtor
				mruDevices.WriteList();
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
			{CBackgroundMultiActionCancelable bmac( d.realtimeThreadPriority ? THREAD_PRIORITY_TIME_CRITICAL : THREAD_PRIORITY_NORMAL );
				bmac.AddAction( __dump_thread__, &d.dumpParams, _T("Dumping to target") );
				const TSaveThreadParams stp={ d.dumpParams.target.get(), d.dumpParams.targetFileName };
				bmac.AddAction( SaveAllModifiedTracks_thread, &stp, _T("Saving target") );
			err=bmac.Perform();}
			if (err==ERROR_SUCCESS){
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
error:			Utils::FatalError(_T("Cannot dump"),err);
			// . destroying the list of SourceTrackErrors
			while (const TDumpParams::TSourceTrackErrors *tmp=d.dumpParams.pOutErroneousTracks)
				d.dumpParams.pOutErroneousTracks=d.dumpParams.pOutErroneousTracks->pNextErroneousTrack, ::free((PVOID)tmp);
		}
	}
