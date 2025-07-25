#include "stdafx.h"

	#define INI_DUMP	_T("Dump")

	struct TWarnings{
		WORD scannedWithError:1;
		WORD hasNonformattedArea:1;
		WORD hasDataInGaps:1;
		WORD hasFuzzyData:1;
		WORD hasDataOverIndex:1;
		WORD hasDuplicatedIdFields:1;
		WORD isEmpty:1;
		WORD missesSomeSectors:1;
		WORD manuallyChangedCrc:1;
		WORD manuallyCreatedStdSectors:1;
		WORD trackNonwriteable:1;

		inline TWarnings(){ *(PWORD)this=0; }

		inline operator WORD() const{ return *(PWORD)this; }
	};

	struct TDumpParams sealed{
		#pragma pack(1)
		typedef const struct TSourceSectorError sealed{
			TSectorId id;
			TFdcStatus fdcStatus;
			bool excluded;
		} *PCSourceSectorError;

		const PCDos dos;
		Medium::TType mediumType;
		Codec::TTypeSet targetCodecs;
		const PImage source;
		const bool canCalibrateSourceHeads;
		const bool sourceSupportsTrackReading;
		std::unique_ptr<CImage> target;
		TCHAR targetFileName[MAX_PATH];
		bool formatJustBadTracks, requireAllStdSectorDataPresent, fullTrackAnalysis;
		bool beepOnError;
		TCylinder cylinderA,cylinderZ;
		THead nHeads;
		struct{
			BYTE value;
			bool valueValid;
		} gap3;
		BYTE fillerByte;
		#pragma pack(1)
		const struct TSourceTrackErrors sealed:TWarnings{
			const TCylinder cyl;
			const THead head;
			const Utils::CCallocPtr<TSourceSectorError,TSector> erroneousSectors;
			const TSourceTrackErrors *pNextErroneousTrack;
			TSourceTrackErrors(TCylinder cyl,THead head,TWarnings warnings,const TSourceSectorError *erroneousSectors,TSector nErroneousSectors)
				: TWarnings(warnings)
				, cyl(cyl) , head(head)
				, erroneousSectors( nErroneousSectors, erroneousSectors )
				, pNextErroneousTrack(nullptr) {
			}
		} *pOutErroneousTracks;

		TDumpParams(PCDos _dos)
			// ctor
			: dos(_dos)
			, source(dos->image)
			, canCalibrateSourceHeads( source->SeekHeadsHome()==ERROR_SUCCESS )
			, sourceSupportsTrackReading( source->WriteTrack(0,0,CImage::CTrackReaderWriter::Invalid)!=ERROR_NOT_SUPPORTED )
			, formatJustBadTracks(false)
			, requireAllStdSectorDataPresent( dos->IsKnown() )
			, fullTrackAnalysis( source->ReadTrack(0,0) ) // if the Source provides access to low-level recording, let's also do the FullTrackAnalysis
			, beepOnError( source->properties->IsRealDevice() )
			, fillerByte(dos->properties->sectorFillerByte)
			, cylinderA(0) , cylinderZ(source->GetCylinderCount()-1)
			, nHeads(source->GetNumberOfFormattedSides(0)) // may be just a subset of GetHeadCount()
			, pOutErroneousTracks(nullptr) {
			gap3.value=dos->properties->GetValidGap3ForMedium(dos->formatBoot.mediumType);
			gap3.valueValid=true;
		}

		~TDumpParams(){
			// dtor
			// . DOS mustn't be destroyed
			if (target)
				target->dos=nullptr;
			// . destroying the list of SourceTrackErrors
			while (const TSourceTrackErrors *const tmp=pOutErroneousTracks)
				pOutErroneousTracks=pOutErroneousTracks->pNextErroneousTrack, delete tmp;
		}

		static CString SettingsToHtml(const CImage::CSettings &s){
			#define STR_PROPERTIES	_T("Relevant properties: ")
			TCHAR buf[4096], *p=::lstrcpy(buf,STR_PROPERTIES)+ARRAYSIZE(STR_PROPERTIES)-1;
			if (s.GetCount()){
				CString k,v;
				for( POSITION pos=s.GetStartPosition(); pos; ){
					s.GetNextAssoc( pos, k, v );
					p+=::wsprintf( p, _T("<i>%s</i> = %s, "), k, v );
				}
				p[-2]='.'; // replace comma with full stop
			}else
				::lstrcpy( p, _T("None.") );
			return buf;
		}

		void __exportErroneousTracksToHtml__(CFile &fHtml,const Utils::CRideTime &duration,bool realtimePriority) const{
			// exports SourceTrackErrors to given HTML file
			Utils::WriteToFile( fHtml, Utils::GetCommonHtmlHeadStyleBody() );
				Medium::TType srcMediumType=Medium::UNKNOWN;
				source->GetInsertedMediumType( 0, srcMediumType );
				if (srcMediumType==Medium::UNKNOWN)
					srcMediumType=dos->formatBoot.mediumType;
				Utils::WriteToFileFormatted( fHtml, _T("<h3>Configuration</h3><table><tr><th><a href=\"%s\">") _T(APP_ABBREVIATION) _T("</a><sup>[1]</sup> version:</th><td>") _T(APP_VERSION) _T("</td></tr><tr><th>System:<sup>[2]</sup></th><td>%s</td></tr><tr><th>Source:<sup>[3]</sup></th><td>%s<br>via<br>%s</td></tr><tr><th>Target:<sup>[4]</sup></th><td>%s<br>via<br>%s</td></tr><tr><th>Cylinders:</th><td>%d &#8211; %d (%s)</td></tr><tr><th>Heads:</th><td>0 &#8211; %d (%s)</td></tr><tr><th>Full track analysis:</th><td>%s</td></tr><tr><th>Real-time priority:</th><td>%s</td></tr></table><br>"), GITHUB_REPOSITORY, dos->properties->name, Medium::GetDescription(srcMediumType), source->GetPathName().GetLength()?source->GetPathName():_T("N/A"), Medium::GetDescription(mediumType), target->GetPathName().GetLength()?target->GetPathName():_T("N/A"), cylinderA,cylinderZ,cylinderA!=cylinderZ?_T("incl."):_T("single cylinder"), nHeads-1,nHeads>1?_T("incl."):_T("single head"), fullTrackAnalysis?_T("On"):_T("Off"), realtimePriority?_T("On"):_T("Off") );
				Utils::WriteToFile(fHtml,_T("<h3>Overview</h3>"));
					Utils::WriteToFileFormatted( fHtml, _T("<p>Duration: %d.%03d seconds (%02d:%02d:%02d.%03d).</p>"), div(duration.ToMilliseconds(),1000), duration.wHour, duration.wMinute, duration.wSecond, duration.wMilliseconds );
					Utils::WriteToFileFormatted( fHtml, _T("<p>Date finished: %s.</p>"), (LPCTSTR)Utils::CRideTime().DateToStdString() );
					if (pOutErroneousTracks){
						Utils::WriteToFile(fHtml,_T("<table><tr><th>Status</th><th>Count</th></tr>"));
							for( TFdcStatus sr=1; sr.w; sr.w<<=1 ){
								if (sr.IsWithoutError())
									continue; 
								int nErrorOccurences=0;
								for( const TSourceTrackErrors *pErroneousTrack=pOutErroneousTracks; pErroneousTrack; pErroneousTrack=pErroneousTrack->pNextErroneousTrack )
									for each( const auto &es in pErroneousTrack->erroneousSectors )
										nErrorOccurences+=(es.fdcStatus&sr)!=0;
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
							for( const TSourceTrackErrors *pErroneousTrack=pOutErroneousTracks; pErroneousTrack; pErroneousTrack=pErroneousTrack->pNextErroneousTrack )
								nWarnings+=Utils::CountSetBits(*pErroneousTrack);
							Utils::WriteToFile(fHtml,_T("<tr><td>Warning</td><td align=right>"));
								Utils::WriteToFile(fHtml,nWarnings);
							Utils::WriteToFile(fHtml,_T("</td></tr>"));							
						Utils::WriteToFile(fHtml,_T("</table>"));
					}else
						Utils::WriteToFile(fHtml,_T("No errors or warnings occurred."));
				Utils::WriteToFile(fHtml,_T("<h3>Details</h3>"));
					if (pOutErroneousTracks){
						Utils::WriteToFile(fHtml,_T("<table><tr><th width=120>Track</th><th>Errors</th></tr>"));
							for( const TSourceTrackErrors *pErroneousTrack=pOutErroneousTracks; pErroneousTrack; pErroneousTrack=pErroneousTrack->pNextErroneousTrack ){
								Utils::WriteToFile(fHtml,_T("<tr><td>"));
									Utils::WriteToFileFormatted( fHtml, _T("Cyl %d, Head %d"), pErroneousTrack->cyl, pErroneousTrack->head );
								Utils::WriteToFile(fHtml,_T("</td><td><ul>"));
									if (pErroneousTrack->scannedWithError)
										Utils::WriteToFile(fHtml,_T("<li><span style=color:red><b>Warning</b>: Error scanning the track!</span></li>"));
									if (pErroneousTrack->erroneousSectors.length)
										for each( const auto &es in pErroneousTrack->erroneousSectors ){
											Utils::WriteToFile(fHtml,_T("<li>"));
												Utils::WriteToFileFormatted( fHtml, _T("%s<b>%s</b>. "), es.excluded?_T("Excluded "):_T(""), (LPCTSTR)es.id.ToString() );
												LPCTSTR bitDescriptions[10],*pDesc=bitDescriptions;
												const TPhysicalAddress chs={ pErroneousTrack->cyl, pErroneousTrack->head, es.id };
												Utils::WriteToFileFormatted( fHtml, _T("<i>FAT</i>: %s, "), dos->GetSectorStatusText(chs) );
												es.fdcStatus.GetDescriptionsOfSetBits(bitDescriptions);
												Utils::WriteToFileFormatted( fHtml, _T("<i>SR1</i> (0x%02X): "), es.fdcStatus.reg1 );
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
												Utils::WriteToFileFormatted( fHtml, _T(" <i>SR2</i> (0x%02X): "), es.fdcStatus.reg2 );
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
									else if (!pErroneousTrack->isEmpty)
										Utils::WriteToFile(fHtml,_T("<li>All sectors ok.</li>"));
									if (pErroneousTrack->isEmpty)
										Utils::WriteToFile(fHtml,_T("<li><b>Warning</b>: No recognized sectors.</li>"));
									if (pErroneousTrack->hasNonformattedArea)
										Utils::WriteToFile(fHtml,_T("<li><b>Warning</b>: Significant non-formatted area.</li>"));
									if (pErroneousTrack->hasDataInGaps)
										Utils::WriteToFile(fHtml,_T("<li><b>Warning</b>: Suspected data in gap.</li>"));
									if (pErroneousTrack->hasFuzzyData)
										Utils::WriteToFile(fHtml,_T("<li><b>Warning</b>: Always bad fuzzy data.</li>"));
									if (pErroneousTrack->hasDataOverIndex)
										Utils::WriteToFile(fHtml,_T("<li><b>Warning</b>: Data over index.</li>"));
									if (pErroneousTrack->hasDuplicatedIdFields)
										Utils::WriteToFile(fHtml,_T("<li><b>Warning</b>: Duplicated ID fields.</li>"));
									if (pErroneousTrack->missesSomeSectors)
										Utils::WriteToFile(fHtml,_T("<li><b>Warning</b>: Some standard sectors missing.</li>"));
									if (pErroneousTrack->manuallyChangedCrc)
										Utils::WriteToFile(fHtml,_T("<li><b>Warning</b>: Some CRCs manually modified.</li>"));
									if (pErroneousTrack->manuallyCreatedStdSectors)
										Utils::WriteToFile(fHtml,_T("<li><b>Warning</b>: Some standard sectors manually created.</li>"));
									if (pErroneousTrack->trackNonwriteable) // this warning should always be the last, justified by previous errors/warnings
										Utils::WriteToFile(fHtml,_T("<li><b>Warning</b>: Track reconstructed, preservation quality lost.</li>"));
								Utils::WriteToFile(fHtml,_T("</ul></td></tr>"));
							}
						Utils::WriteToFile(fHtml,_T("</table>"));
					}else
						Utils::WriteToFile(fHtml,_T("None."));
				CImage::CSettings srcSettings, trgSettings, dosSettings;
				source->EnumSettings(srcSettings), target->EnumSettings(trgSettings);
				TCHAR strFormat[80];
				::wsprintf( strFormat, _T("%i &times; %i &times; %i &times; %i"), dos->formatBoot.nCylinders, dos->formatBoot.nHeads, dos->formatBoot.nSectors, dos->formatBoot.sectorLength );
				dosSettings.SetAt( _T("recognized format"), dos->IsKnown()?strFormat:_T("None") );
				Utils::WriteToFileFormatted( fHtml, _T("<h3>Notes</h3><ol><li><p>%s</p></li><li><p>%s</p></li><li><p>%s</p></li><li><p>%s</p></li></ol>"), GITHUB_REPOSITORY, SettingsToHtml(dosSettings), SettingsToHtml(srcSettings), SettingsToHtml(trgSettings) );
			Utils::WriteToFile(fHtml,_T("</body></html>"));
		}
	};

	#define ACCEPT_ERROR_ID			IDOK

	#define RESOLVE_EXCLUDE_ID		IDIGNORE
	#define RESOLVE_EXCLUDE_UNKNOWN	IDCONTINUE

	#define NO_STATUS_ERROR	_T("- no error\r\n")

	static UINT AFX_CDECL __dump_thread__(PVOID _pCancelableAction){
		// threat to copy Tracks
		LOG_ACTION(_T("dump thread"));
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		TDumpParams &dp=*(TDumpParams *)pAction->GetParams();
		pAction->SetProgressTarget( dp.cylinderZ+1-dp.cylinderA );
		// - dumping
		BYTE defaultSectorContent[16384];
		::memset( defaultSectorContent, dp.dos->properties->sectorFillerByte, sizeof(defaultSectorContent) );
		const Utils::CVarTempReset<bool> bws0( Utils::CRideDialog::BeepWhenShowed, dp.beepOnError );
		const TDumpParams::TSourceTrackErrors **ppSrcTrackErrors=&dp.pOutErroneousTracks;
		#pragma pack(1)
		struct TParams sealed{
			bool targetSupportsTrackWriting;
			TPhysicalAddress chs;
			TSector s; // # of Sectors to skip
			TSector nSectorsExcluded;
			TTrack track;
			bool trackScanned;
			bool trackWriteable; // Track can be written at once using CImage::WriteTrack
			BYTE revolution;
			struct{
				TFdcStatus automaticallyAcceptedErrors;
				bool anyErrorsOnUnknownSectors;
				bool anyErrorsOnEmptySectors;
				bool remainingErrorsOnTrack;
			} acceptance;
			struct{
				bool current;
				bool allUnknown;
			} exclusion;
			struct{
				bool idCrc;
				bool dataCrc;
				bool dataCrcEmptyOrBadAutofix;
				bool stdSectorAdded;
			} modification;
			TFdcStatus fdcStatus;
		} p;
		::ZeroMemory(&p,sizeof(p));
		p.exclusion.allUnknown=dp.dos->IsKnown() && dynamic_cast<CImageRaw *>(dp.target.get())!=nullptr; // Unknown Sectors in recognized DOS cause preliminary termination of dumping to a RawImage, hence excluding them all automatically
		p.targetSupportsTrackWriting=dp.target->WriteTrack(0,0,CImage::CTrackReaderWriter::Invalid)!=ERROR_NOT_SUPPORTED;
		const Utils::CByteIdentity sectorIdAndPositionIdentity;
		for( p.chs.cylinder=dp.cylinderA; p.chs.cylinder<=dp.cylinderZ; pAction->UpdateProgress(++p.chs.cylinder-dp.cylinderA) )
			for( p.chs.head=0; p.chs.head<dp.nHeads; ){
				if (pAction->Cancelled) return LOG_ERROR(ERROR_CANCELLED);
				LOG_TRACK_ACTION(p.chs.cylinder,p.chs.head,_T("processing"));
				p.track=p.chs.GetTrackNumber(dp.nHeads);
				// . scanning Source Track
				TSectorId bufferId[(TSector)-1];	WORD bufferLength[(TSector)-1];
				Codec::TType sourceCodec; TSector nSectors; TStdWinError err;
{LOG_TRACK_ACTION(p.chs.cylinder,p.chs.head,_T("scanning source"));
				nSectors=dp.source->ScanTrack(p.chs.cylinder,p.chs.head,&sourceCodec,bufferId,bufferLength);
}
				TWarnings warnings;
				warnings.isEmpty=!nSectors;
				p.trackScanned=dp.source->IsTrackScanned( p.chs.cylinder, p.chs.head );
				warnings.scannedWithError=!p.trackScanned; // was there a problem scanning the Track?
				if (dp.requireAllStdSectorDataPresent && p.trackScanned){
					TSectorId stdIds[(TSector)-1];
					const TSector nStdIds=dp.dos->GetListOfStdSectors( p.chs.cylinder, p.chs.head, stdIds );
					for( TSector iStd=0; iStd<nStdIds; ){
						const auto id = p.chs.sectorId=stdIds[iStd++];
						TSector iFound=0;
						while (iFound<nSectors)
							if (bufferId[iFound]!=id)
								iFound++;
							else
								break;
						if (iFound==nSectors && dp.dos->IsStdSector(p.chs)){
							// a standard Sector missing; add it to the list to later invoke a common error dialog
							bufferId[nSectors]=id, bufferLength[nSectors]=CImage::GetOfficialSectorLength(id.lengthCode);
							nSectors++;
						}
					}
				}
				// . reading Source Track
				CImage::CTrackReader trSrc=dp.source->ReadTrack( p.chs.cylinder, p.chs.head );
				const bool trackWriteable0 = p.trackWriteable = trSrc && p.targetSupportsTrackWriting && (sourceCodec&dp.targetCodecs)!=0; // A&B&C, A&B = Source and Target must support whole Track access, C = Target must support the Codec used in Source
				// . if possible, analyzing the read Source Track
				if (trSrc && dp.fullTrackAnalysis){
					const auto peTrack=trSrc.ScanAndAnalyze( pAction->CreateSubactionProgress(0) );
					warnings.hasNonformattedArea=peTrack.Contains( CImage::CTrackReader::TParseEvent::NONFORMATTED );
					warnings.hasDataInGaps=peTrack.Contains( CImage::CTrackReader::TParseEvent::DATA_IN_GAP );
					warnings.hasFuzzyData=peTrack.Contains( CImage::CTrackReader::TParseEvent::FUZZY_BAD );
					for each( const auto &pair in peTrack ){
						const auto &pe=*pair.second;
						if (pe.IsDataStd()){
							for( BYTE i=0; i<trSrc.GetIndexCount(); i++ )
								warnings.hasDataOverIndex|=pe.Contains( trSrc.GetIndexTime(i) );
							if (warnings.hasDataOverIndex)
								break;
						}
					}
				}
				for( TSector i=0; i<nSectors; i++ )
					if ( warnings.hasDuplicatedIdFields=TSectorId::CountAppearances(bufferId,i,bufferId[i])>0 )
						break;
				if (p.chs.cylinder<dp.dos->formatBoot.nCylinders // reporing a missing official Sector makes sense only in official part of the disk
					&&
					dp.dos->IsKnown() // must understand the disk structure to decide on "official part"
				){
					TSectorId stdIds[(TSector)-1];
					const TSector nStdIds=dp.dos->GetListOfStdSectors( p.chs.cylinder, p.chs.head, stdIds );
					for( TSector i=0; i<nStdIds; i++ ){
						p.chs.sectorId=stdIds[i];
						TSector j=0;
						while (j<nSectors && bufferId[j]!=p.chs.sectorId)
							j++;
						if ( warnings.missesSomeSectors=j==nSectors ) // missing a Sector in official geometry?
							break;
					}
				}
				// . reading individual Sectors
				if (pAction->Cancelled)
					return ERROR_CANCELLED;
				#pragma pack(1)
				struct{
					TSector n;
					TDumpParams::TSourceSectorError buffer[(TSector)-1];
				} erroneousSectors;
				erroneousSectors.n=0;
				PSectorData bufferSectorData[(TSector)-1];
				TFdcStatus bufferFdcStatus[(TSector)-1];
				for( TSector i=nSectors; i>0; bufferFdcStatus[--i]=p.acceptance.automaticallyAcceptedErrors );
				PVOID dummyBuffer[(TSector)-1];
{LOG_TRACK_ACTION(p.chs.cylinder,p.chs.head,_T("reading source"));
				dp.source->GetTrackData( p.chs.cylinder, p.chs.head, Revolution::ANY_GOOD, bufferId, sectorIdAndPositionIdentity, nSectors, bufferSectorData, bufferLength, bufferFdcStatus, (PLogTime)dummyBuffer ); // reading healthy Sectors (unhealthy ones read individually below); "DummyBuffer" = throw away any outputs
				for( TSector sPrev=~(p.s=p.nSectorsExcluded=0),sIncr=1; p.s<nSectors; ){
					if (pAction->Cancelled)
						return ERROR_CANCELLED;
					p.chs.sectorId=bufferId[p.s];
					p.fdcStatus=bufferFdcStatus[p.s];
					// : reporting SourceSector Exclusion
					p.exclusion.current|= p.exclusion.allUnknown && !dp.dos->IsStdSector(p.chs);
					if (p.exclusion.current){
						nSectors--, p.nSectorsExcluded++;
						::memmove( bufferId+p.s, bufferId+p.s+1, sizeof(*bufferId)*(nSectors-p.s) );
						::memmove( bufferSectorData+p.s, bufferSectorData+p.s+1, sizeof(*bufferSectorData)*(nSectors-p.s) );
						::memmove( bufferLength+p.s, bufferLength+p.s+1, sizeof(*bufferLength)*(nSectors-p.s) );
						::memmove( bufferFdcStatus+p.s, bufferFdcStatus+p.s+1, sizeof(*bufferFdcStatus)*(nSectors-p.s) );
						p.trackWriteable=false; // once modified, can't write the Track as a whole anymore
						sPrev=~--p.s; // as below incremented
					// : reporting SourceSector Data field automatic fix
					}else if (p.modification.dataCrc|= p.modification.dataCrcEmptyOrBadAutofix && p.fdcStatus.DescribesDataFieldCrcError() && dp.dos->IsSectorStatusBadOrEmpty(p.chs) ){
						sIncr=0; // check there are no other errors with current Sector
					// : reporting SourceSector Errors if ...
					}else if (
						p.fdcStatus & ~( // do remain any errors that can't be accepted automatically?
							p.acceptance.automaticallyAcceptedErrors
							|
							~TFdcStatus::SectorNotFound*(
								p.acceptance.remainingErrorsOnTrack
								||
								p.acceptance.anyErrorsOnUnknownSectors && !dp.dos->IsStdSector(p.chs)
								||
								p.acceptance.anyErrorsOnEmptySectors && dp.dos->GetSectorStatus(p.chs)==TSectorStatus::EMPTY
							)
						)
					){
						// | reading SourceSector particular Revolution
						LOG_SECTOR_ACTION(&p.chs.sectorId,_T("reading"));
						const BYTE nRevsAvailable=dp.source->GetAvailableRevolutionCount( p.chs.cylinder, p.chs.head );
						if (sPrev!=p.s) // is this the first trial?
							p.revolution=Revolution::R0;
						else if (nRevsAvailable<=Revolution::MAX){ // is the # of Revolutions limited?
							if (++p.revolution>=nRevsAvailable)
								p.revolution=Revolution::R0;
							bufferSectorData[p.s]=dp.source->GetSectorData( p.chs, p.s, (Revolution::TType)p.revolution, bufferLength+p.s, bufferFdcStatus+p.s );
						}else{
							++p.revolution;
							bufferSectorData[p.s]=dp.source->GetSectorData( p.chs, p.s, Revolution::NEXT, bufferLength+p.s, bufferFdcStatus+p.s );
						}
						p.fdcStatus=bufferFdcStatus[p.s]; // updating the Params (just in case the Sector was opted to exclude below and its FdcStatus in the Buffer lost)
						// | Dialog definition
						class CErroneousSectorDialog sealed:public Utils::CRideDialog{
							const TDumpParams &dp;
							TParams &rp;
							const PSectorData sectorData;
							const WORD sectorDataLength;
							TFdcStatus &rFdcStatus;
							CEdit errorTextBox,sectorListTextBox;
							std::unique_ptr<CSplitterWnd> splitter,upperSplitter;
							HACCEL hAccel;

							class CSectorHexaEditor sealed:public CHexaEditor{
							public:
								const TDumpParams &dp;
								const TParams &p;

								CSectorHexaEditor(const TDumpParams &dp,const TParams &p)
									// ctor
									// > base
									: CHexaEditor((PVOID)&dp)
									// > modification of ContextMenu
									, dp(dp) , p(p) {
									contextMenu.AppendSeparator();
									contextMenu.AppendMenu(
										MF_BYCOMMAND | MF_STRING | MF_GRAYED*!dp.source->ReadTrack(0,0),
										ID_TIME, _T("Timing under cursor...")
									);
								}
								bool ProcessCustomCommand(UINT cmd) override{
									// custom command processing
									switch (cmd){
										case ID_TIME:
											dp.source->ShowModalTrackTimingAt(
												p.chs, 0,
												instance->GetCaretPosition(),
												(Revolution::TType)p.revolution
											);
											return true;
									}
									return __super::ProcessCustomCommand(cmd);
								}
							} hexaEditor;

							void PreInitDialog() override{
								// dialog initialization
								// > base
								__super::PreInitDialog();
								BeepWhenShowed=false; // no subdialogs to produce a beep when shown
								ConvertDlgCheckboxToHyperlink( ID_BEEP );
								// > creating the Splitter
								const CRect rcSplitter=MapDlgItemClientRect(ID_ALIGN);
								splitter.reset( new CSplitterWnd );
								splitter->CreateStatic( this, 2,1, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS );//WS_CLIPCHILDREN|
									upperSplitter.reset( new CSplitterWnd );
									upperSplitter->CreateStatic( splitter.get(), 1,2, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS );//WS_CLIPCHILDREN|
									upperSplitter->SetColumnInfo( 0, rcSplitter.Width()*2/3, 0 ); // Utils::LogicalUnitScaleFactor
										errorTextBox.Create(
											AFX_WS_DEFAULT_VIEW&~WS_BORDER | WS_CLIPSIBLINGS | ES_MULTILINE | ES_AUTOHSCROLL | WS_VSCROLL | ES_NOHIDESEL | ES_READONLY,
											CFrameWnd::rectDefault, upperSplitter.get(), upperSplitter->IdFromRowCol(0,0)
										);
										errorTextBox.SetFont( GetFont() );
										sectorListTextBox.Create(
											AFX_WS_DEFAULT_VIEW&~WS_BORDER | WS_CLIPSIBLINGS | ES_MULTILINE | ES_AUTOHSCROLL | WS_VSCROLL | ES_NOHIDESEL | ES_READONLY,
											CFrameWnd::rectDefault, upperSplitter.get(), upperSplitter->IdFromRowCol(0,1)
										);
										sectorListTextBox.SetFont( GetFont() );
								splitter->SetRowInfo( 0, rcSplitter.Height()/3, 0 ); // Utils::LogicalUnitScaleFactor
									if (IStream *const s=Yahel::Stream::FromBuffer( sectorData, sectorDataLength )){
										hexaEditor.Reset( s, nullptr, sectorDataLength );
										s->Release();
									}
									hexaEditor.Create(
										nullptr, nullptr,
										AFX_WS_DEFAULT_VIEW&~WS_BORDER | WS_CLIPSIBLINGS,
										CFrameWnd::rectDefault, splitter.get(), splitter->IdFromRowCol(1,0)
									);
								SetDlgItemPos( splitter->m_hWnd, rcSplitter );
								// > creating message on Errors
								LPCTSTR bitDescriptions[20],*pDesc=bitDescriptions; // 20 = surely big enough buffer
								rFdcStatus.GetDescriptionsOfSetBits(bitDescriptions);
								TCHAR buf[1024],*p=buf+::wsprintf(buf,_T("Cannot read sector with %s on source Track %d.\r\n"),(LPCTSTR)rp.chs.sectorId.ToString(),rp.track);
								const Revolution::TType dirtyRevolution=dp.source->GetDirtyRevolution(rp.chs,rp.s);
								const BYTE nRevolutions=dp.source->GetAvailableRevolutionCount( rp.chs.cylinder, rp.chs.head );
								if (nRevolutions==1)
									p+=::lstrlen( ::lstrcpy(p,_T("Single revolution.\r\n")) );
								else if (dirtyRevolution==Revolution::NONE) // not yet modified or Unknown Sector queried
									p+=::wsprintf( p, _T("Revolution #%d\r\n"), rp.revolution+1 );
								else if (dirtyRevolution<Revolution::MAX)
									p+=::wsprintf( p, _T("Locked modified Revolution #%d.\r\n"), dirtyRevolution+1 );
								else
									p+=::lstrlen( ::lstrcpy(p,_T("Locked modified revolution.\r\n")) );
								const bool onlyPartlyRecoverable=( rFdcStatus & ~(TFdcStatus::DataFieldCrcError|TFdcStatus::IdFieldCrcError) )!=0;
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
								errorTextBox.SetWindowText( buf );
								const CString sectorList=dp.source->ListSectors( rp.chs.cylinder, rp.chs.head, rp.s+rp.nSectorsExcluded, '>' );
								::wsprintf( buf, _T("All sectors on this track:\r\n%s\r\n"), sectorList );
								sectorListTextBox.SetWindowText( buf );
								// > converting the "Accept" button to a SplitButton
								ACCEL accels[16],*pLastAccel=accels;
								const bool sectorNotFound=rFdcStatus.DescribesMissingId();
								const Utils::TSplitButtonAction Actions[]={
									{ ACCEPT_ERROR_ID, _T("Accept this error"), MF_GRAYED*( sectorNotFound&&!(dp.requireAllStdSectorDataPresent&&dp.dos->IsStdSector(rp.chs)) ) }, // allow acceptance of missing standard Sector (e.g. copy-protection in "Life & Death" PC game)
									Utils::TSplitButtonAction::HorizontalLine,
									{ ID_ERROR, _T("Accept all errors of this kind"), MF_GRAYED*( sectorNotFound ) },
									{ ID_TRACK, _T("Accept all errors in this track"), MF_GRAYED*( sectorNotFound ) },
									{ ID_DRIVE, _T("Accept all errors on unknown sectors"), MF_GRAYED*( !dp.source->dos->IsKnown() || sectorNotFound ) }, // not available if DOS Unknown
									{ ID_STATE, _T("Accept all errors on empty sectors"), MF_GRAYED*( sectorNotFound ) },
									{ ID_IMAGE, _T("Accept all errors on the disk"), MF_GRAYED*( sectorNotFound ) }
								};
								ConvertDlgButtonToSplitButton( IDOK, Actions );
								EnableDlgItem( IDOK, // accepting errors is allowed only if ...
									!Actions->menuItemFlags // can accept this error?
									&&
									dynamic_cast<CImageRaw *>(dp.target.get())==nullptr // ... the Target Image can accept them
								);
								// > converting the "Resolve" button to a SplitButton
								const UINT miningMenuFlags=MF_GRAYED*!( dp.source->MineTrack(TPhysicalAddress::Invalid.cylinder,TPhysicalAddress::Invalid.head)!=ERROR_NOT_SUPPORTED );
								const Utils::TSplitButtonAction resolveActions[]={
									{ 0, onlyPartlyRecoverable?_T("Resolve partly"):_T("Resolve") }, // 0 = no default action
									{ RESOLVE_EXCLUDE_ID, _T("Exclude from track\tCtrl+X") },
									{ RESOLVE_EXCLUDE_UNKNOWN, _T("Exclude all unknown from disk"), MF_GRAYED*( !dp.source->dos->IsKnown() ) }, // not available if DOS Unknown
									{ ID_CREATOR, _T("Add missing standard sector\tCtrl+A"), MF_GRAYED*( !(rFdcStatus.DescribesMissingId()||rFdcStatus.DescribesMissingDam()) || !dp.source->dos->IsSectorStatusBadOrEmpty(rp.chs) ) },
									Utils::TSplitButtonAction::HorizontalLine,
									{ ID_DATAFIELD_CRC, _T("Fix Data CRC only\tCtrl+D"), MF_GRAYED*( rFdcStatus.DescribesMissingDam() || rFdcStatus.DescribesMissingId() || !rFdcStatus.DescribesDataFieldCrcError() ) }, // disabled if the Data CRC ok
									{ ID_COMPUTE_CHECKSUM, _T("Fix Data CRCs for all empty or bad sectors"), MF_GRAYED*( rFdcStatus.DescribesMissingDam() || rFdcStatus.DescribesMissingId() || !rFdcStatus.DescribesDataFieldCrcError() || !dp.source->dos->IsSectorStatusBadOrEmpty(rp.chs) ) }, // disabled if the Data CRC ok
									{ ID_RECOVER, _T("Fix ID or Data...\tCtrl+F"), MF_GRAYED*( rFdcStatus.DescribesMissingDam() || rFdcStatus.DescribesMissingId() || !rFdcStatus.DescribesIdFieldCrcError()&&!rFdcStatus.DescribesDataFieldCrcError() ) }, // enabled only if either ID or Data field with error
									Utils::TSplitButtonAction::HorizontalLine,
									{ ID_AUTO, _T("Mine track (auto-start last config)...\tCtrl+S"), miningMenuFlags },
									{ ID_TIME, _T("Mine track...\tCtrl+M"), miningMenuFlags }
								};
								ConvertDlgButtonToSplitButton( IDNO, resolveActions, &pLastAccel );
								//EnableDlgItem( IDNO, dynamic_cast<CImageRaw *>(dp.target.get())==nullptr ); // recovering errors is allowed only if the Target Image can accept them
								// > converting the "Retry" button to a SplitButton
								static constexpr Utils::TSplitButtonAction CanCalibrateHeadsAction={ ID_HEAD, _T("Calibrate head and retry\tCtrl+R") };
								static constexpr Utils::TSplitButtonAction CannotCalibrateHeadsAction={ ID_HEAD, _T("Can't calibrate heads for this track"), MF_GRAYED };
								const Utils::TSplitButtonAction retryActions[]={
									{ IDRETRY, _T("Retry") },
									dp.canCalibrateSourceHeads ? CanCalibrateHeadsAction : CannotCalibrateHeadsAction
								};
								ConvertDlgButtonToSplitButton( IDRETRY, retryActions, &pLastAccel );
								// > the "Retry" button enabled only if Sector not yet modified and there are several Revolutions available
								EnableDlgItem( IDRETRY, dirtyRevolution==Revolution::NONE && nRevolutions>1 );
								// > creating accelerator table
								hAccel=::CreateAcceleratorTable( accels, pLastAccel-accels );
							}
							void DoDataExchange(CDataExchange *pDX) override{
								// exchange of data from and to controls
								DDX_Check( pDX, ID_BEEP, beepWhenShowed );
							}
							bool ConfirmLowLevelTrackModifications(){
								// True <=> user confirmed an original command, otherwise False
								if (!dp.sourceSupportsTrackReading || !rp.targetSupportsTrackWriting)
									return true; // automatically confirmed if Source or Target don't support low-level Track timing
								static constexpr Utils::CSimpleCommandDialog::TCmdButtonInfo CmdButtons[]={
									{ IDYES, _T("Carry out anyway") },
									{ IDNO, _T("Accept this sector error (recommended)") }
								};
								switch (
									Utils::CSimpleCommandDialog(
										_T("This command may destroy low-level information for this track. Consider using one of the \"Accept\" options."),
										CmdButtons, ARRAYSIZE(CmdButtons), _T("Return")
									).DoModal()
								){
									case IDYES:
										return true;
									case IDNO:
										UpdateData(TRUE);
										EndDialog( ACCEPT_ERROR_ID );
										//fallthrough
									default:
										return false;
								}
							}
							BOOL PreTranslateMessage(PMSG pMsg){
								// pre-processing the Message
								if (::TranslateAccelerator( m_hWnd, hAccel, pMsg ))
									return TRUE;
								return __super::PreTranslateMessage(pMsg);
							}
							LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
								// window procedure
								switch (msg){
									case WM_COMMAND:
										switch (const WORD id=LOWORD(wParam)){
											case ID_HEAD:{
												TStdWinError err;
												if (!( err=dp.source->SeekHeadsHome() ))
													if (!( err=dp.source->UnscanTrack( rp.chs.cylinder, rp.chs.head ) ))
														rp.trackScanned=false;
												if (err)
													Utils::Information( _T("Can't calibrate"), err, _T("Retrying without calibration.") );
												//fallthrough
											}
											case IDRETRY:
												UpdateData(TRUE);
												EndDialog(IDRETRY);
												return 0;
											case ID_ERROR:
												rp.acceptance.automaticallyAcceptedErrors.ExtendWith(rFdcStatus);
												UpdateData(TRUE);
												EndDialog(ACCEPT_ERROR_ID);
												return 0;
											case ID_DRIVE:
												rp.acceptance.anyErrorsOnUnknownSectors=true;
												UpdateData(TRUE);
												EndDialog(ACCEPT_ERROR_ID);
												return 0;
											case ID_STATE:
												rp.acceptance.anyErrorsOnEmptySectors=true;
												UpdateData(TRUE);
												EndDialog(ACCEPT_ERROR_ID);
												return 0;
											case ID_IMAGE:
												rp.acceptance.automaticallyAcceptedErrors.ExtendWith(~TFdcStatus::SectorNotFound); // all but "Missing ID Field" accepted (e.g. extremely damaged disk where Sectors appear and disappear randomly in each Revolution)
												//fallthrough
											case ID_TRACK:
												rp.acceptance.remainingErrorsOnTrack=true;
												UpdateData(TRUE);
												EndDialog(ACCEPT_ERROR_ID);
												return 0;
											case ID_COMPUTE_CHECKSUM:
												// recovering Data field CRC for all Sectors reported as Empty or Bad
												//fallthrough
											case ID_DATAFIELD_CRC:
												// recovering Data field CRC
												if (!ConfirmLowLevelTrackModifications())
													return 0;
												rp.modification.dataCrc=true;
												rp.modification.dataCrcEmptyOrBadAutofix|=id==ID_COMPUTE_CHECKSUM;
												UpdateData(TRUE);
												EndDialog(ACCEPT_ERROR_ID);
												return 0;
											case ID_RECOVER:{
												// Sector recovery
												if (!ConfirmLowLevelTrackModifications())
													return 0;
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
															DDX_Text( pDX, ID_IDFIELD_VALUE, bufSectorId, ARRAYSIZE(bufSectorId) );
															if (pDX->m_bSaveAndValidate){
																int cyl,side,sect,len;
																_stscanf( bufSectorId, _T("%d.%d.%d.%d"), &cyl, &side, &sect, &len );
																idFieldSubstituteSectorId.cylinder=cyl, idFieldSubstituteSectorId.side=side, idFieldSubstituteSectorId.sector=sect, idFieldSubstituteSectorId.lengthCode=len;
															}else
																::wsprintf( bufSectorId, _T("%d.%d.%d.%d"), idFieldSubstituteSectorId.cylinder, idFieldSubstituteSectorId.side, idFieldSubstituteSectorId.sector, idFieldSubstituteSectorId.lengthCode );
															DDX_Text( pDX, ID_IDFIELD_VALUE, bufSectorId, ARRAYSIZE(bufSectorId) );
															static constexpr WORD IdFieldRecoveryOptions[]={ ID_IDFIELD, ID_IDFIELD_CRC, ID_IDFIELD_REPLACE, 0 };
															EnableDlgItems( IdFieldRecoveryOptions, rFdcStatus.DescribesIdFieldCrcError() );
															static constexpr WORD IdFieldReplaceOption[]={ ID_IDFIELD_VALUE, ID_DEFAULT1, 0 };
															EnableDlgItems( IdFieldReplaceOption, idFieldRecoveryType==2 );
														// | "Data Field" region
														DDX_Radio( pDX, ID_DATAFIELD, dataFieldRecoveryType );
															DDX_Text( pDX, ID_DATAFIELD_FILLERBYTE, dataFieldSubstituteFillerByte );
															if (!dosProps->IsKnown())
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
																		if (dosProps->IsKnown())
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
												const Utils::CVarTempReset<bool> tempDisabledBeeping( Utils::CRideDialog::BeepWhenShowed, false );
												if (d.DoModal()==IDOK){
													switch (d.idFieldRecoveryType){
														case 2:
															// replacing Sector ID
															rp.chs.sectorId=d.idFieldSubstituteSectorId;
															//fallthrough
														case 1:
															// recovering CRC
															rp.modification.idCrc=true;
															break;
													}
													switch (d.dataFieldRecoveryType){
														case 2:
															// replacing data with SubstituteByte
															::memset( sectorData, d.dataFieldSubstituteFillerByte, sectorDataLength );
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
											case ID_AUTO:
											case ID_TIME:{
												// Track mining
												const Utils::CVarTempReset<bool> tempDisabledBeeping( Utils::CRideDialog::BeepWhenShowed, false );
												if (ERROR_SUCCESS==dp.source->MineTrack( rp.chs.cylinder, rp.chs.head, id==ID_AUTO )){
													UpdateData(TRUE);
													rp.trackScanned=false;
													EndDialog(IDRETRY);
												}
												return 0;
											}
											case RESOLVE_EXCLUDE_UNKNOWN:
												// exclusion of this and all future Unknown Sectors from the disk
												if (!ConfirmLowLevelTrackModifications())
													return 0;
												rp.exclusion.allUnknown=true;
												EndDialog(RESOLVE_EXCLUDE_ID);
												return 0;
											case RESOLVE_EXCLUDE_ID:
												// exclusion of this Sector
												if (!ConfirmLowLevelTrackModifications())
													return 0;
												EndDialog(RESOLVE_EXCLUDE_ID);
												return 0;
											case ID_CREATOR:
												// add missing standard Sector
												if (!ConfirmLowLevelTrackModifications())
													return 0;
												rp.modification.stdSectorAdded=true;
												EndDialog(ID_CREATOR);
												return 0;
										}
										break;
									case WM_NOTIFY:
										switch (GetClickedHyperlinkId(lParam)){
											case ID_BEEP: // beep test
												Utils::StdBeep();
												break;
										}
										break;
								}
								return __super::WindowProc(msg,wParam,lParam);
							}
						public:
							int beepWhenShowed;

							CErroneousSectorDialog(CWnd *pParentWnd,const TDumpParams &dp,TParams &_rParams,PSectorData sectorData,WORD sectorLength,TFdcStatus &rFdcStatus)
								// ctor
								: Utils::CRideDialog( IDR_IMAGE_DUMP_ERROR, pParentWnd )
								, beepWhenShowed(BeepWhenShowed)
								, dp(dp) , rp(_rParams) , rFdcStatus(rFdcStatus)
								, sectorData( rFdcStatus.DescribesMissingDam()?nullptr:sectorData )
								, sectorDataLength( rFdcStatus.DescribesMissingDam()?0:sectorLength )
								, hexaEditor( dp, _rParams ) {
							}
							~CErroneousSectorDialog(){
								// dtor
								::DestroyAcceleratorTable(hAccel);
							}
						} d(pAction,dp,p,bufferSectorData[p.s],bufferLength[p.s],p.fdcStatus);
						// | showing the Dialog and processing its result
				{		LOG_DIALOG_DISPLAY(_T("CErroneousSectorDialog"));
						const auto result=LOG_DIALOG_RESULT(d.DoModal());
						Utils::CRideDialog::BeepWhenShowed=d.beepWhenShowed!=BST_UNCHECKED;
						switch (result){
							case IDCANCEL:
								err=ERROR_CANCELLED;
								return LOG_ERROR(pAction->TerminateWithError(err));
							case RESOLVE_EXCLUDE_ID:
								p.exclusion.current=true;
								continue;
							case IDRETRY:
								if (!p.trackScanned) // has the Track been disposed as part of the recovery from error?
									p.s=nSectors; // break this cycle
								sPrev=p.s;
								continue;
				}		}
					}
					if (!p.fdcStatus.IsWithoutError() || p.exclusion.current){
						TDumpParams::TSourceSectorError *const psse=&erroneousSectors.buffer[erroneousSectors.n++];
						psse->id=p.chs.sectorId, psse->fdcStatus=p.fdcStatus;
						psse->excluded=p.exclusion.current;
					}
					if (p.modification.idCrc || p.modification.dataCrc){
						if (p.modification.idCrc)
							p.fdcStatus.CancelIdFieldCrcError();
						else
							p.fdcStatus.CancelDataFieldCrcError();
						warnings.manuallyChangedCrc=true;
						p.trackWriteable=false; // once modified, can't write the Track as a whole anymore
						sIncr=0; // check there are no other errors with current Sector
					}
					if (p.modification.stdSectorAdded){
						bufferSectorData[p.s]=defaultSectorContent;
						p.fdcStatus=TFdcStatus::WithoutError;
						warnings.manuallyCreatedStdSectors=true;
						p.trackWriteable=false; // once modified, can't write the Track as a whole anymore
					}
					if (!p.exclusion.current)
						bufferFdcStatus[p.s]=p.fdcStatus; // propagate modifications to the Buffer
					// : next SourceSector
					p.exclusion.current = p.modification.idCrc = p.modification.dataCrc = p.modification.stdSectorAdded = false;
					p.s+=sIncr;
					sIncr=1;
				}
}
				p.acceptance.remainingErrorsOnTrack=false; // "True" valid only for Track it was set on
				if (!p.trackScanned && !warnings.isEmpty) // has a non-empty Track been disposed as part of the recovery from error? (e.g. a Track may be empty due to non-existent KryoFlux Streams)
					continue; // begin anew with this Track
				// . formatting Target Track
				if (pAction->Cancelled)
					return ERROR_CANCELLED;
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
									structuresIdentical&=count==nullptr;
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
reformatTrack:		if (!p.trackWriteable){ // formatting the Track only if can't write the Track using CImage::WriteTrack
						warnings.trackNonwriteable=trackWriteable0; // warn if the Track was writeable before
						const Codec::TType targetCodec=Codec::FirstFromMany( dp.targetCodecs ); // the first of selected Codecs (available for the DOS/Medium combination)
						if ( err=dp.target->FormatTrack(p.chs.cylinder,p.chs.head,targetCodec,nSectors,bufferId,bufferLength,bufferFdcStatus,dp.gap3.value,dp.fillerByte,pAction->Cancelled) )
							return LOG_ERROR(pAction->TerminateWithError(err));
					}
}
				// . writing to Target Track
				if (pAction->Cancelled)
					return ERROR_CANCELLED;
{LOG_TRACK_ACTION(p.chs.cylinder,p.chs.head,_T("writing to Target Track"));
				if (p.trackWriteable)
					// can use the CImage::WriteTrack to write the whole Track at once
					for( trSrc.RewindToIndex(0); err=dp.target->WriteTrack( p.chs.cylinder, p.chs.head, trSrc ); ){
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
					dp.target->BufferTrackData( p.chs.cylinder, p.chs.head, Revolution::ANY_GOOD, bufferId, sectorIdAndPositionIdentity, nSectors ); // make Sectors data ready for IMMEDIATE usage
					for( BYTE s=0; s<nSectors; ){
						if (pAction->Cancelled)
							return ERROR_CANCELLED;
						if (!bufferFdcStatus[s].DescribesMissingDam()){
							p.chs.sectorId=bufferId[s];
							LOG_SECTOR_ACTION(&p.chs.sectorId,_T("writing"));
							if (const PSectorData targetData=dp.target->GetSectorData(p.chs,s,Revolution::ANY_GOOD)){
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
				switch ( err=dp.target->SaveTrack(p.chs.cylinder,p.chs.head,pAction->Cancelled) ){
					case ERROR_SUCCESS:
						//fallthrough
					case ERROR_NOT_SUPPORTED:
						break; // writings to the Target Image will be saved later via CImage::OnSaveDocument
					default:
terminateWithError:		return LOG_ERROR(pAction->TerminateWithError(err));
				}
				// . registering Track with ErroneousSectors
//Utils::Information("registering Track with ErroneousSectors");
				if (warnings || erroneousSectors.n){
					auto *const pste=new TDumpParams::TSourceTrackErrors(
						p.chs.cylinder, p.chs.head,
						warnings,
						erroneousSectors.buffer, erroneousSectors.n
					);
					*ppSrcTrackErrors=pste, ppSrcTrackErrors=&pste->pNextErroneousTrack;
				}
}
				p.chs.head++;
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

			void PreInitDialog() override{
				// dialog initialization
				// . base
				__super::PreInitDialog();
				// . adjusting text in button next to FillerByte edit box
				if (!dos->IsKnown())
					SetDlgItemText( ID_DEFAULT1, _T("Random value") );
				// . showing devices recently dumped to in hidden menu
				static constexpr Utils::TSplitButtonAction OpenDialogAction={ ID_FILE, _T("Select image or device...") };
				Utils::TSplitButtonAction actions[10], *pAction=actions;
				*pAction++=OpenDialogAction;
				static constexpr Utils::TSplitButtonAction HelpCreateStream={ ID_HELP_USING, _T("FAQ: How do I create stream files? (online)") };
				*pAction++=HelpCreateStream;
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
				ConvertDlgButtonToSplitButtonEx( ID_FILE, actions, pAction-actions );
				ConvertDlgCheckboxToHyperlink( ID_BEEP );
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
					// : Codec must be known
					if (( dumpParams.targetCodecs=GetDlgComboBoxSelectedValue(ID_CODEC) )==Codec::UNKNOWN){
						Utils::Information( _T("Encoding not specified.") );
						pDX->Fail();
					}
				}else{
					SetDlgItemText( ID_FILE, ELLIPSIS );
					PopulateComboBoxWithCompatibleMedia( hMedium, 0, nullptr ); // if FileName not set, Medium cannot be determined
					PopulateComboBoxWithCompatibleCodecs( GetDlgItemHwnd(ID_CODEC), 0, nullptr ); // if FileName not set, Codec cannot be determined
				}
				const Medium::PCProperties mp=	targetImageProperties // ComboBox populated with compatible Media and one of them selected
												? Medium::GetProperties( dumpParams.mediumType=(Medium::TType)GetDlgComboBoxSelectedValue(ID_MEDIUM) )
												: nullptr;
				DDX_Check( pDX, ID_FORMAT, dumpParams.formatJustBadTracks );
				DDX_CheckEnable( pDX, ID_ACCURACY, dumpParams.fullTrackAnalysis, dumpParams.fullTrackAnalysis );
				DDX_CheckEnable( pDX, ID_STANDARD, dumpParams.requireAllStdSectorDataPresent, dos->IsKnown() );
				DDX_Check( pDX, ID_BEEP, dumpParams.beepOnError );
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
						dos->IsKnown() // Unknown DOS doesn't have valid Format information
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
										TCHAR buf[80];
										::wsprintf( buf, _T("Continue to Cylinder %d (incl.) reported in boot sector (recommended)"), CImage::GetActive()->dos->formatBoot.nCylinders-1 );
										AddCommandButton( IDYES, buf, true );
										::wsprintf( buf, _T("Continue to last occupied Cylinder %d (incl.)"), lastOccupiedCyl );
										AddCommandButton( IDRETRY, buf );
										AddCommandButton( IDNO, _T("Continue with current settings (cylinders beyond official format may fail!)") );
										AddCancelButton( _T("Return to dump dialog") );
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
										dumpParams.cylinderZ=dos->formatBoot.nCylinders-1;
										dumpParams.cylinderA=std::min( dumpParams.cylinderA, dumpParams.cylinderZ );
										//fallthrough
									case IDNO:
										break;
									case IDRETRY:
										dumpParams.cylinderZ=d.lastOccupiedCyl;
										dumpParams.cylinderA=std::min( dumpParams.cylinderA, dumpParams.cylinderZ );
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
								TCHAR targetFileNameOrg[MAX_PATH];
								::lstrcpy( targetFileNameOrg, dumpParams.targetFileName );
								if (const CImage::PCProperties p=dumpParams.dos->properties->typicalImage){
									if (const PTCHAR pSep=::StrChr( ::lstrcpy(dumpParams.targetFileName,p->filter), *IMAGE_FORMAT_SEPARATOR ))
										*pSep='\0';
								}else
									*dumpParams.targetFileName='\0';
								if (const CImage::PCProperties imgProps=app.DoPromptFileName( dumpParams.targetFileName, true, AFX_IDS_SAVEFILE, 0, nullptr )){
									targetImageProperties=imgProps;
setDestination:						// : compacting FileName in order to be better displayable on the button
									if (targetImageProperties->IsRealDevice())
										SetDlgItemCompactPath( ID_FILE, dumpParams.targetFileName );
									else
										SetDlgItemFormattedText( ID_FILE, _T("%s\n(%s)"), dumpParams.targetFileName, targetImageProperties->fnRecognize(nullptr) );
									// : populating ComboBox with Media supported by both DOS and Image
									BYTE nCompatibleMedia;
									if (dos->formatBoot.mediumType!=Medium::UNKNOWN
										&&
										(dos->formatBoot.mediumType&Medium::FLOPPY_ANY)!=0
									)
										// source Image is a floppy - enabling dumping to any kind of a floppy (motivation: some copy-protection schemes feature misleading information on the kind of floppy; e.g., "Teen Agent" [or "Agent mlicnak"] installation disk #2 and #3 are introduced as 2DD floppies while they really are HD!)
										nCompatibleMedia=PopulateComboBoxWithCompatibleMedia(
											GetDlgItemHwnd(ID_MEDIUM),
											dos->properties->supportedMedia&Medium::FLOPPY_ANY,
											targetImageProperties
										);
									else
										// source Image is anything else
										nCompatibleMedia=PopulateComboBoxWithCompatibleMedia(
											GetDlgItemHwnd(ID_MEDIUM),
											dos->formatBoot.mediumType,
											targetImageProperties
										);
									// : can change Medium only when Source has no explicit Track timing
									EnableDlgItem( ID_MEDIUM, dos->image->WriteTrack(0,0,CImage::CTrackReaderWriter::Invalid)==ERROR_NOT_SUPPORTED );
									// : preselection of current MediumType (if any recognized)
									Medium::TType mt=Medium::UNKNOWN; // assumption (Medium not recognized)
									dos->image->GetInsertedMediumType( 0, mt );
									if (mt!=Medium::UNKNOWN)
										SelectDlgComboBoxValue( ID_MEDIUM, mt );
									// : automatically ticking the "Real-time thread priority" check-box if either the source or the target is a real drive
									if (dos->image->properties->IsRealDevice() || targetImageProperties->IsRealDevice())
										CheckDlgItem(ID_PRIORITY);
									//fallthrough
								}else{
									::lstrcpy( dumpParams.targetFileName, targetFileNameOrg );
									break;
								}
								//fallthrough
							}
							case MAKELONG(ID_MEDIUM,CBN_SELCHANGE):{
								// : populating ComboBox with Codecs supported by both DOS and Image
								const Medium::TType mt=(Medium::TType)GetDlgComboBoxSelectedValue(ID_MEDIUM);
								const BYTE nCompatibleCodecs=PopulateComboBoxWithCompatibleCodecs(
									GetDlgItemHwnd(ID_CODEC),
									dos->properties->supportedCodecs,
									mt!=Medium::UNKNOWN ? targetImageProperties : nullptr
								);
								// : can change Codec only when Source has no explicit Track timing
								EnableDlgItem( ID_CODEC, dos->image->WriteTrack(0,0,CImage::CTrackReaderWriter::Invalid)==ERROR_NOT_SUPPORTED );
								// : enabling/disabling controls
								static constexpr WORD Controls[]={ ID_CYLINDER, ID_CYLINDER_N, ID_HEAD, ID_GAP, ID_NUMBER, ID_DEFAULT1, IDOK, 0 };
								CheckAndEnableDlgItem(
									ID_FORMAT,
									EnableDlgItems( Controls, mt!=Medium::UNKNOWN&&nCompatibleCodecs>0 )  &&  targetImageProperties->IsRealDevice()
								);
								FocusDlgItem(IDOK);
								break;
							}
							case ID_DEFAULT1:
								SetDlgItemInt( ID_NUMBER, dos->IsKnown() ? dos->properties->sectorFillerByte : ::GetTickCount()&0xff );
								break;
							case MAKELONG(ID_CYLINDER,EN_CHANGE):
							case MAKELONG(ID_CYLINDER_N,EN_CHANGE):
								Invalidate();
								break;
							case IDOK:
								if (targetImageProperties && targetImageProperties->IsRealDevice())
									mruDevices.Add( dumpParams.targetFileName, &CUnknownDos::Properties, targetImageProperties );
								break;
							case ID_HELP_USING:
								Utils::NavigateToFaqInDefaultBrowser( _T("createStreams") );
								return 0;
							default:
								if (ID_FILE_MRU_FIRST<=wParam && wParam<=ID_FILE_MRU_LAST){
									wParam-=ID_FILE_MRU_FIRST;
									targetImageProperties=mruDevices.GetMruDevice(wParam);
									if (!targetImageProperties->fnRecognize(dumpParams.targetFileName)){ // make sure Device is initialized (e.g. KryoFlux firmware loaded, etc.)
										const CString msg=Utils::SimpleFormat( _T("Can't use \"%s\" device"), mruDevices[wParam] );
										Utils::Information( msg, ERROR_DEVICE_NOT_AVAILABLE );
										break;
									}
									::lstrcpy( dumpParams.targetFileName, mruDevices[wParam] );
									wParam=ID_FILE;
									goto setDestination;
								}else
									break;
						}
						break;
					case WM_NOTIFY:
						switch (GetClickedHyperlinkId(lParam)){
						case ID_BEEP: // beep test
							Utils::StdBeep();
							break;
						case ID_HELP_INDEX:{
							// . defining the Dialog
							class CHelpDialog sealed:public Utils::CCommandDialog{
								BOOL OnInitDialog() override{
									// dialog initialization
									// : base
									const BOOL result=__super::OnInitDialog();
									// : supplying available actions
									AddHelpButton( ID_IMAGE, _T("How do I create an image of a real floppy disk?") );
									AddHelpButton( ID_HELP_USING, _T("How do I create stream files?") );
									AddHelpButton( ID_DRIVE, _T("How do I save an image back to a real floppy disk?") );
									AddHelpButton( ID_FILE, _T("How do I convert between two images? (E.g. *.IMA to *.DSK)") );
									AddHelpButton( ID_ACCURACY, _T("How do I create an exact copy of a real floppy disk?") );
									AddCancelButton( MSG_HELP_CANCEL );
									return result;
								}
							public:
								CHelpDialog()
									// ctor
									: Utils::CCommandDialog(_T("Dumping is used to convert between data containers.")) {
								}
							} d;
							// . showing the Dialog and processing its result
							switch (d.DoModal()){
								case ID_IMAGE:
									Utils::NavigateToFaqInDefaultBrowser( _T("floppy2image") );
									break;
								case ID_HELP_USING:
									Utils::NavigateToFaqInDefaultBrowser( _T("createStreams") );
									break;
								case ID_DRIVE:
									Utils::NavigateToFaqInDefaultBrowser( _T("image2floppy") );
									break;
								case ID_FILE:
									Utils::NavigateToFaqInDefaultBrowser( _T("convertImage") );
									break;
								case ID_ACCURACY:
									Utils::NavigateToFaqInDefaultBrowser( _T("copyFloppy") );
									break;
							}
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
			bool realtimeThreadPriority,showReport;

			CDumpDialog(PDos _dos)
				// ctor
				: Utils::CRideDialog(IDR_IMAGE_DUMP)
				, dos(_dos) , targetImageProperties(nullptr) , dumpParams(_dos)
				, mruDevices( CRecentFileList(0,INI_DUMP,_T("MruDev%d"),4) )
				, realtimeThreadPriority(false)
				, showReport(true) {
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
			TStdWinError err=ERROR_WRITE_PROTECT; // assumption
			if (d.dumpParams.target->IsWriteProtected()){
				d.dumpParams.target->ToggleWriteProtection();
				if (d.dumpParams.target->IsWriteProtected())
error:				return Utils::FatalError(_T("Cannot dump"),err);
			}
			if ( err=d.dumpParams.target->Reset() )
				goto error;
			if (!d.dumpParams.target->EditSettings(true)){
				err=ERROR_CANCELLED;
				goto error;
			}
			// . setting geometry to the Target Image
			const struct TDeducedSides sealed{
				bool ambigous; // multiple Side numbers on a single Track?
				TSide map[(THead)-1];
				TDeducedSides(PCImage source)
					: ambigous(false) {
					for( THead head=0; head<source->GetHeadCount(); head++ ){
						TSectorId ids[(TSector)-1];
						if (TSector nSectors=source->ScanTrack( 0, head, nullptr, ids )){
							for( map[head]=ids->side; nSectors>0; )
								if ( ambigous|=ids[--nSectors].side!=map[head] )
									break;
						}else
							ambigous=true;
					}
				}
			} deducedSides(dos->image);
			TSector nSectors=dos->image->ScanTrack(0,0);
			const TFormat targetGeometry={ d.dumpParams.mediumType, dos->formatBoot.codecType, d.dumpParams.cylinderZ+1, d.dumpParams.nHeads, nSectors, dos->formatBoot.sectorLengthCode, dos->formatBoot.sectorLength, 1 };
			const PCSide sideMap =	dos->image->GetSideMap() // if Source explicitly defines Sides ...
									? dos->image->GetSideMap() // ... adopt them
									: !deducedSides.ambigous // if unique Sides can be deduced from the first Cylinder ...
									? deducedSides.map // ... adopt them
									: dos->sideMap; // otherwise adopt Sides defined by the DOS
			if ( err=d.dumpParams.target->SetMediumTypeAndGeometry( &targetGeometry, sideMap, dos->properties->firstSectorNumber ) )
				goto error;
			d.dumpParams.target->SetPathName( d.dumpParams.targetFileName, FALSE );
			// . dumping
			CBackgroundMultiActionCancelable bmac( d.realtimeThreadPriority ? THREAD_PRIORITY_TIME_CRITICAL : THREAD_PRIORITY_NORMAL );
				bmac.AddAction( __dump_thread__, &d.dumpParams, _T("Dumping to target") );
				const TSaveThreadParams stp( d.dumpParams.target.get(), d.dumpParams.targetFileName );
				bmac.AddAction( SaveAllModifiedTracks_thread, &stp, _T("Saving target") );
			err=bmac.Perform(true);
			const_cast<PImage>(this)->UpdateAllViews(nullptr);
			if (err)
				goto error;
			// . reporting success
			if (!d.showReport)
				if (!Utils::QuestionYesNo( _T("Dumped successfully.\n\nShow report?"), MB_DEFBUTTON2 ))
					return;
			// . displaying statistics on SourceTrackErrors
			const CString tmpFileName=Utils::GenerateTemporaryFileName()+_T(".html");
			d.dumpParams.__exportErroneousTracksToHtml__( CFile(tmpFileName,CFile::modeCreate|CFile::modeWrite), bmac.GetDurationTime(), d.realtimeThreadPriority );
			app.GetMainWindow()->OpenWebPage( _T("Dump results"), tmpFileName );
		}
	}
