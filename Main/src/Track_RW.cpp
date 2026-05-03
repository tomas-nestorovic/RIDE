#include "stdafx.h"
#include "Charting.h"
namespace MFM=Codec::Impl::MFM;

namespace Track
{
	CReaderBuffers::CReaderBuffers(const CDecoder &decoder,PLogTimesInfo pLti)
		// ctor
		: CDecoder(decoder)
		, pLogTimesInfo(pLti)
		, indexPulses(pLti->indexPulses) {
	}






	CReaderBuffers::TLogTimesInfoData::TLogTimesInfoData(bool resetDecoderOnIndex)
		// ctor
		: mediumProps(nullptr) , codec(Codec::UNKNOWN)
		, resetDecoderOnIndex(resetDecoderOnIndex) {
		*indexPulses=Time::Infinity; // a virtual IndexPulse in infinity
	}

	CReaderBuffers::CLogTimesInfo::CLogTimesInfo(bool resetDecoderOnIndex)
		// "ctor"
		: TLogTimesInfoData( resetDecoderOnIndex )
		, nRefs(1) {
	}

	bool CReaderBuffers::CLogTimesInfo::Release(){
		// "dtor"
		if (::InterlockedDecrement(&nRefs)==0){
			delete this;
			return true;
		}else
			return false;
	}






	CReader::CReader(Time::N nLogTimesMax,TDecoderMethod method,PLogTimesInfo pLti,Codec::TType codec)
		// ctor
		: CReaderBuffers(
			CDecoder( method, nLogTimesMax, 0, pLti->metaData ),
			pLti
		)
		, iNextIndexPulse(0) , nIndexPulses(0) {
		SetMediumType(Medium::FLOPPY_DD); // init values associated with the specified Medium
		SetCodec(codec); // init values associated with the specified Codec
	}

	CReader::CReader(const CReader &tr)
		// copy ctor
		: CReaderBuffers(tr)
		, iNextIndexPulse(tr.iNextIndexPulse) , nIndexPulses(tr.nIndexPulses) {
		pLogTimesInfo->AddRef();
	}

	CReader::CReader(CReader &&tr)
		// move ctor
		: CReaderBuffers(tr)
		, iNextIndexPulse(tr.iNextIndexPulse) , nIndexPulses(tr.nIndexPulses) {
		pLogTimesInfo->AddRef();
	}

	CReader::~CReader(){
		// dtor
		pLogTimesInfo->Release();
	}




	constexpr TLogTime TimelyFromPrevious=Event::TimelyFromPrevious;

	void CReader::SetCurrentTime(TLogTime logTime){
		// seeks to the specified LogicalTime
		__super::SetCurrentTime(logTime);
		FindMetaDataIteratorAndApply();
		//TODO: indexPulses.FindNextGreater( time, arrayLength=0 )
		//TODO: indexPulses.FindNextGreaterIndex( time, arrayLength=0 )
		for( iNextIndexPulse=0; iNextIndexPulse<nIndexPulses; iNextIndexPulse++ )
			if (logTime<indexPulses[iNextIndexPulse])
				break;
	}

	void CReader::SetCurrentTimeAndProfile(TLogTime logTime,const TProfile &profile){
		// seeks to the specified LogicalTime, setting also the specified Profile at that LogicalTime
		this->profile=profile; // eventually overridden ...
		SetCurrentTime(logTime); // ... here
	}

	TLogTime CReader::RewindToIndex(TRev index){
		// navigates back to the first Flux found just after the index pulse
		SetCurrentTime( GetIndexTime(index) );
		return GetCurrentTime();
	}

	TLogTime CReader::RewindToIndexAndResetProfile(TRev index){
		// navigates back to the first Flux found just after the index pulse
		profile.Reset();
		return RewindToIndex( index );
	}

	TLogTime CReader::GetIndexTime(TRev index) const{
		// returns the Time at which the specified IndexPulse occurs
		if (!nLogTimes || (nIndexPulses|index)==0)
			return 0;
		else
			return	index<nIndexPulses
					? indexPulses[index]
					: logTimes[nLogTimes-1];
	}

	TLogTime CReader::GetAvgIndexDistance() const{
		// given at least two indices, computes and returns the average distance between them, otherwise 0
		if (nIndexPulses<2)
			return 0;
		const TRev nFullRevs=nIndexPulses-1;
		return (indexPulses[nFullRevs]-*indexPulses)/nFullRevs;
	}

	TLogTime CReader::GetTotalTime() const{
		// returns the minimum time that covers both Indices and recorded Times
		return	std::max( GetLastTime(), GetLastIndexTime() );
	}

	const Utils::CSharedBytes &CReader::GetRawDeviceData(TTypeId dataId) const{
		// retrieves data as they were received from a disk (e.g. used for fast copying between compatible disks)
		if (const auto &r=pLogTimesInfo->rawDeviceData)
			if (r.id==dataId)
				return r;
		return Utils::CSharedBytes::GetEmpty();
	}

	void CReader::SetCodec(Codec::TType codec){
		// changes the interpretation of recorded LogicalTimes according to the new Codec
		if (const Codec::PCProperties p=Codec::GetProperties(codec)){
			pLogTimesInfo->codec=codec;
			nConsecutiveZerosMax=p->RLL.k;
		}else
			ASSERT(FALSE); // we shouldn't end up here!
	}

	void CReader::SetMediumType(Medium::TType mediumType){
		// changes the interpretation of recorded LogicalTimes according to the new MediumType
		if ( pLogTimesInfo->mediumProps=Medium::GetProperties(mediumType) )
			static_cast<Time::Decoder::TLimits &>(profile)=pLogTimesInfo->mediumProps->CreateTimeDecoderLimits();
		else
			ASSERT(FALSE); // we shouldn't end-up here, all Media Types applicable for general Track description should be covered
		profile.Reset();
		ApplyCurrentTimeMetaData();
	}

	bool CReader::ReadBit(TLogTime &rtOutOne){
		// returns first bit not yet read
		const bool value=__super::ReadBit(rtOutOne);
		if (profile.method!=TDecoderMethod::METADATA)
			if (currentTime>=indexPulses[iNextIndexPulse]){
				if (pLogTimesInfo->resetDecoderOnIndex)
					RewindToIndexAndResetProfile(iNextIndexPulse);
				iNextIndexPulse++;
			}
		return value;
	}

	WORD CReader::Scan(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,CParseEventList *pOutParseEvents){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		profile.Reset();
		WORD nSectorsFound;
		switch (pLogTimesInfo->codec){
			case Codec::FM:
				nSectorsFound=ScanFm( pOutFoundSectors, pOutIdEnds, pOutIdProfiles, pOutIdStatuses, pOutParseEvents );
				break;
			case Codec::MFM:
				nSectorsFound=ScanMfm( pOutFoundSectors, pOutIdEnds, pOutIdProfiles, pOutIdStatuses, pOutParseEvents );
				break;
			default:
				ASSERT(FALSE); // we shouldn't end up here - check if all Codecs are included in the Switch statement!
				return 0;
		}
		return nSectorsFound;
	}

	WORD CReader::ScanAndAnalyze(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,PLogTime pOutDataEnds,CParseEventList &rOutParseEvents,CActionProgress &ap,bool fullAnalysis,CBits *pOutBits){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		constexpr int StepGranularity=1000;
		const TRev nFullRevolutions=std::max( 0, GetIndexCount()-1 );
		ap.SetProgressTarget( (4+2*nFullRevolutions+1)*StepGranularity ); // (N)*X, N = analysis steps
		// - Step 1: standard scanning using current Codec
		const WORD nSectorsFound=Scan( pOutFoundSectors, pOutIdEnds, pOutIdProfiles, pOutIdStatuses, &rOutParseEvents );
		ap.UpdateProgress( 1*StepGranularity );
		// - Step 2: getting ParseEvents in Sector data
		struct{
			TLogTime time;
			TProfile profile;
		} dataEnds[1536];
		WORD nDataEnds=0;
		for( WORD s=0; s<nSectorsFound; s++ ){
			CParseEventList peSector;
			if (!ReadData( pOutFoundSectors[s], pOutIdEnds[s], pOutIdProfiles[s], Sector::GetLength(pOutFoundSectors[s].lengthCode), nullptr, &peSector ).DescribesMissingDam()
				&&
				*this // not end of Track (aka. data complete)
			){
				auto &r=dataEnds[nDataEnds++];
					r.time=currentTime;
					r.profile=profile;
				pOutDataEnds[s]=currentTime;
			}else
				pOutDataEnds[s]=0;
			rOutParseEvents.Add( peSector );
		}
		if (!fullAnalysis)
			return nSectorsFound;
		ap.UpdateProgress( 2*StepGranularity );
		// - Step 3: search for non-formatted areas
		if (nSectorsFound>0){ // makes sense only if some Sectors found
			constexpr BYTE nCellsMin=64;
			const TLogTime tAreaLengthMin=nCellsMin*profile.iwTimeDefault; // ignore all non-formatted areas that are shorter
			for( TLogTime t0=RewindToIndexAndResetProfile(0),t; *this; t0=t )
				if (( t=ReadTime() )-t0>=tAreaLengthMin) // report a non-formatted area ...
					for each( const auto &pair in rOutParseEvents ){
						const TParseEvent &pe=*pair.second;
						if (pe.IsDataStd()) // ... only within a Sector data
							if (pe.Contains(t0) || pe.Contains(t)){
								rOutParseEvents.Add(
									TParseEvent( Event::NONFORMATTED, t0, t, 0 )
								);
								break;
							}
					}
		}
		ap.UpdateProgress( 3*StepGranularity );
		// - Step 4: search for data in gaps
		if (nSectorsFound>0){ // makes sense only if some Sectors found
			// . composition of all ends of ID and Data fields
			for( WORD s=0; s<nSectorsFound; s++ ){
				auto &r=dataEnds[nDataEnds+s];
					r.time=pOutIdEnds[s];
					r.profile=pOutIdProfiles[s];
			}
			const WORD nGaps=nDataEnds+nSectorsFound;
			// . analyzing gap between two consecutive ParseEvents
			typedef Bit::TPattern TBitPattern;
			struct:Charting::CHistogram{ // Key = bit pattern (data+clock), Value = number of occurences
				iterator Find(TBitPattern bp){
					static_assert( sizeof(TBitPattern)==2, "" ); // the following is valid only for "8 data bits + 8 clock bits"
					for( char n=sizeof(bp)*CHAR_BIT; n-->0; ){ // try all rotations
						const auto it=find(bp);
						if (it!=end())
							return it;
						bp= (bp<<1) | (bp>>(sizeof(bp)*CHAR_BIT-1));
					}
					return end(); // unknown combination of data+clock bits
				}
				void Add(TBitPattern bp){
					static_assert( sizeof(bp)<=sizeof(key_type), "" ); // must fit in
					const auto it=Find(bp);
					if (it!=end())
						it->second++;
					else
						__super::Add(bp); 
				}
			} hist;
			constexpr BYTE nBytesInspectedMax=20; // or maximum number of Bytes inspected before deciding that there are data in the gap
			TBitPattern bitPattern;
			for( WORD i=nGaps; i>0; ){
				const auto &r=dataEnds[--i];
				if (const auto it=rOutParseEvents.FindByEnd(r.time+1))
					if (it->second->tStart<r.time) // gap that overlaps a ParseEvent (e.g. Sector within Sector copy protection) ...
						continue; // ... is not a gap
				const auto itNext=rOutParseEvents.FindByStart(r.time);
				if (!itNext)
					continue; // don't collect data in Gap4 after the last Sector (but yes, search the Gap4 after the Histogram is complete)
				const TParseEvent &peNext=*itNext->second;
				SetCurrentTimeAndProfile( r.time, r.profile );
				BYTE nBytesInspected=0;
				ReadByte(bitPattern); // skip this Byte written just after the Data CRC; the Head is turned off after that, creating magnetic noise
				ReadByte(bitPattern); // skip this Byte for the magnetic noise still overlaps into it
				for( TLogTime t=GetCurrentTime(); t<peNext.tStart && nBytesInspected<nBytesInspectedMax; nBytesInspected++ ){
					BYTE byte;
					ReadByte( bitPattern, &byte );
					hist.Add(bitPattern);
				}
			}
			// . production of new ParseEvents
			if (hist.size()>0){ // a gap should always consits of some Bytes, but just to be sure
				typedef const Charting::CHistogram::CPair &RCPair;
				const TBitPattern typicalBitPattern=std::max_element( // "typical" BitPattern, likely the default gap filler Byte created during formatting
					hist.cbegin(), hist.cend(),
					[](RCPair p1,RCPair p2)->bool{ return p1.second<p2.second; }
				)->first; 
				for( WORD i=nGaps; i>0; ){
					const auto &r=dataEnds[--i];
					if (const auto it=rOutParseEvents.FindByEnd(r.time+1))
						if (it->second->tStart<r.time) // gap that overlaps a ParseEvent (e.g. Sector within Sector copy protection) ...
							continue; // ... is not a gap
					const auto itNext=rOutParseEvents.FindByStart(r.time);
					const TLogTime tNextStart= itNext ? itNext->second->tStart : Time::Infinity;
					SetCurrentTimeAndProfile( r.time, r.profile );
					BYTE nBytesInspected=0, nBytesTypical=0;
					TDataParseEvent peData( TSectorId::Invalid, r.time );
					auto *const pbi=peData.GetByteInfos();
					while (*this && nBytesInspected<nBytesInspectedMax){
						auto &rbi=pbi[nBytesInspected];
							rbi.dtStart=currentTime-peData.tStart;
							ReadByte( bitPattern, peData.bytes+nBytesInspected );
						if (currentTime>=tNextStart){
							currentTime=rbi.dtStart+peData.tStart; // putting unsuitable Byte back
							break;
						}
						const auto it=hist.Find(bitPattern);
						nBytesTypical+= it!=hist.end() && it->first==typicalBitPattern;
						nBytesInspected++;
					}
					if (nBytesInspected-nBytesTypical>6){
						// significant amount of other than TypicalBitPatterns, beyond a random noise on Track
						constexpr BYTE nGapBytesMax=60;
						while (*this && nBytesInspected<nGapBytesMax){
							auto &rbi=pbi[nBytesInspected];
								rbi.dtStart=currentTime-peData.tStart;
								ReadByte( bitPattern, peData.bytes+nBytesInspected );
							const auto it=hist.Find(bitPattern);
							if (currentTime>=tNextStart
								||
								it!=hist.end() && it->first==typicalBitPattern
							){
								currentTime=rbi.dtStart+peData.tStart; // putting unsuitable Byte back
								break; // again Typical, so probably all gap data discovered
							}
							nBytesInspected++;
						}
						peData.Finalize( currentTime, profile, nBytesInspected, Event::DATA_IN_GAP );
						rOutParseEvents.Add( peData );
					}
				}
			}
		}
		ap.UpdateProgress( 4*StepGranularity );
		// - Step 5,6,...: search for fuzzy regions in Sectors
		const TLogTime tTrackEnd=GetIndexTime(nFullRevolutions)-profile.iwTimeMax;
		if (nSectorsFound>0 && nFullRevolutions>=2){ // makes sense only if some Sectors found over several Revolutions
			// . extraction of bits from each full Revolution
			const auto &&bits=CreateFullRevBitSequences();
			if (pOutBits)
				*pOutBits=bits;
			// . forward comparison of Revolutions, from the first to the last; bits not included in the last diff script are stable across all previous Revolutions
			Bit::CSharedDiffScript shortesEditScripts[Revolution::MAX];
			for( TRev i=0; i<nFullRevolutions-1; ){
				// : comparing the two neighboring Revolutions I and J
				const Bit::CSequence &jRev=bits.revs[i], &iRev=bits.revs[++i];
				auto &ses=shortesEditScripts[i];
				ses=iRev.GetShortestEditScript( jRev, ap.CreateSubactionProgress(StepGranularity) );
				if (ap.Cancelled)
					return nSectorsFound;
				// : marking different Bits neighboring Revolutions as Fuzzy
				iRev.ScriptToLocalDiffs( ses );
				// : inheriting fuzzyness from previous Revolution
				iRev.InheritFlagsFrom( jRev, ses );
			}
			// . backward comparison of Revolutions, from the last to the first
			for( TRev i=nFullRevolutions; i>1; ){
				// : conversion to dual script
				const auto &ses=shortesEditScripts[--i];
				for( auto k=ses.length; k>0; ses[--k].ConvertToDual() );
				// : marking different Bits as Fuzzy
				const Bit::CSequence &jRev=bits.revs[i], &iRev=bits.revs[i-1];
				iRev.ScriptToLocalDiffs( ses );
				// : inheriting fuzzyness from next Revolution
				iRev.InheritFlagsFrom( jRev, ses );
			}
			// . merging consecutive fuzzy bits into FuzzyEvents
			CActionProgress apMerge=ap.CreateSubactionProgress( StepGranularity, StepGranularity );
			auto peIt=rOutParseEvents.GetIterator();
			for( TRev r=0; r<nFullRevolutions; apMerge.UpdateProgress(++r) ){
				const Bit::CSequence &rev=bits.revs[r];
				CActionProgress apRev=apMerge.CreateSubactionProgress( StepGranularity/nFullRevolutions, rev.GetBitCount() );
				Bit::CSequence::PCBit bit=rev.begin(), lastBit=rev.end();
				do{
					// : finding next Fuzzy interval
					while (bit<lastBit && !bit->IsFuzzy()) // skipping Bits that are not Fuzzy
						if (ap.Cancelled)
							return nSectorsFound;
						else
							bit++;
					if (bit==lastBit) // no more Fuzzy bits?
						break;
					const TLogTime tPrevBit= bit==rev.begin()&&pLogTimesInfo->resetDecoderOnIndex ? bit->time-profile.iwTimeDefault : bit[-1].time;
					TParseEvent peFuzzy( Event::NONE, tPrevBit, 0, 0 ); // "tPrevBit" = all ParseEvents must be one InspectionWindow behind to comply with rest of the app
					while (bit<lastBit && bit->IsFuzzy()) // discovering consecutive Fuzzy Bits
						if (ap.Cancelled)
							return nSectorsFound;
						else
							bit++;
					peFuzzy.tEnd=bit[-1].time; // "[-1]" = all ParseEvents must be one InspectionWindow behind to comply with rest of the app
					// : determining the type of fuzziness
					peFuzzy.type=rOutParseEvents.GetTypeOfFuzziness( peIt, peFuzzy, tTrackEnd );
					// : creating new FuzzyEvent
					rOutParseEvents.Add( peFuzzy );
					apRev.UpdateProgress( bit-rev.begin() );
				} while (bit<lastBit);
			}
		}
		auto peIt=rOutParseEvents.GetIterator();
		for( auto itMdi=GetMetaData().cbegin(),itMdiEnd=GetMetaData().cend(); itMdi!=itMdiEnd; itMdi++ )
			if (ap.Cancelled)
				return nSectorsFound;
			else if (itMdi->isFuzzy){
				// merge all consecutive "fuzzy" MetaDataItems
				TLogTimeInterval ti=*itMdi;
				const TLogTime tIw=itMdi->GetBitTimeAvg();
				while (++itMdi!=itMdiEnd && itMdi->isFuzzy)
					ti.tEnd=itMdi->tEnd;
				rOutParseEvents.Add(
					TParseEvent(
						rOutParseEvents.GetTypeOfFuzziness( peIt, ti, tTrackEnd ),
						ti.tStart-tIw, ti.tEnd-tIw, 0 // "-tIw" = all ParseEvents must be one InspectionWindow behind to comply with rest of the app
					)
				);
				if (itMdi==itMdiEnd)
					break;
			}
		// - successfully analyzed
		return nSectorsFound;
	}

	WORD CReader::ScanAndAnalyze(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,PLogTime pOutDataEnds,CParseEventList &rOutParseEvents,CActionProgress &ap,bool fullAnalysis,CBits *pOutBits){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		TProfile idProfiles[Revolution::MAX*(TSector)-1]; TFdcStatus statuses[Revolution::MAX*(TSector)-1];
		return ScanAndAnalyze( pOutFoundSectors, pOutIdEnds, idProfiles, statuses, pOutDataEnds, rOutParseEvents, ap, fullAnalysis, pOutBits );
	}
	
	CParseEventList CReader::ScanAndAnalyze(CActionProgress &ap,bool fullAnalysis,CBits *pOutBits){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		CParseEventList peTrack;
		TSectorId ids[Revolution::MAX*(TSector)-1]; TLogTime idEnds[Revolution::MAX*(TSector)-1]; TLogTime dataEnds[Revolution::MAX*(TSector)-1];
		ScanAndAnalyze( ids, idEnds, dataEnds, peTrack, ap, fullAnalysis, pOutBits );
		return peTrack;
	}

	TFdcStatus CReader::ReadData(const TSectorId &id,TLogTime idEndTime,const TProfile &idEndProfile,WORD nBytesToRead,CSharedParseEventPtr *pOutDataPe,CParseEventList *pOutParseEvents){
		// attempts to read specified amount of Bytes into the Buffer, starting at position pointed to by the BitReader
		SetCurrentTimeAndProfile( idEndTime, idEndProfile );
		const Utils::CVarTempReset<bool> rdoi0( pLogTimesInfo->resetDecoderOnIndex, false ); // never reset when reading data
		switch (pLogTimesInfo->codec){
			case Codec::FM:
				return	ReadDataFm( id, nBytesToRead, pOutDataPe, pOutParseEvents );
			case Codec::MFM:
				return	ReadDataMfm( id, nBytesToRead, pOutDataPe, pOutParseEvents );
			default:
				ASSERT(FALSE); // we shouldn't end up here - all Codecs should be included in the Switch statement!
				return TFdcStatus::NoDataField;
		}
	}











	Bit::CSequence CReader::CreateBitSequence(TLogTime tFrom,const CReader::TProfile &profileFrom, TLogTime tTo,BYTE oneOkPercent) const{
		// - count all Bits ("tr.GetTotalTime()/profileFrom.iwTimeMin" not used to account for decoder phase adjustment, allowing for returning back in time)
		const TLogTime iwTimeDefaultHalf=profileFrom.iwTimeDefault/2;
		CReader tr=*this;
		tr.SetCurrentTimeAndProfile( tFrom, profileFrom );
		tTo=std::min( tTo-iwTimeDefaultHalf, tr.GetTotalTime() );
		Bit::N nBits=0;
		while (tr.GetCurrentTime()<tTo)
			tr.ReadBit(), nBits++;
		// - create the Sequence
		tr.SetCurrentTimeAndProfile( tFrom, profileFrom );
		return Bit::CSequence( tr, nBits, oneOkPercent );
	}











	WORD CReader::ScanFm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,CParseEventList *pOutParseEvents){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		ASSERT( pOutFoundSectors!=nullptr && pOutIdEnds!=nullptr );
		RewindToIndex(0);
		//TODO
		return 0;
	}

	TFdcStatus CReader::ReadDataFm(const TSectorId &sectorId,WORD nBytesToRead,CSharedParseEventPtr *pOutDataPe,CParseEventList *pOutParseEvents){
		// attempts to read specified amount of Bytes into the Buffer, starting at current position; returns the amount of Bytes actually read
		ASSERT( pLogTimesInfo->codec==Codec::FM );
		//TODO
		return TFdcStatus::SectorNotFound;
	}

	bool CReaderWriter::WriteDataFm(TDataParseEvent &peData,TFdcStatus sr){
		// True <=> the whole DataParseEvent was written to the Track, starting at CurrentTime, otherwise False
		ASSERT( pLogTimesInfo->codec==Codec::FM );
		//TODO
		return false;
	}










		static bool IsWellEncodedMfmSequence(BYTE bits){
			// see comment at 'CReader::lastReadBits'
			if ((bits&3)==3) // last valid bit is "1"?
				return (bits&4)==0; // previous bit must be "0" (and we don't care if it's valid or not)
			else
				return bits!=0xaa; // four valid consecutive "0"s are forbidden
		}

	WORD CReader::ScanMfm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,CParseEventList *pOutParseEvents){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		ASSERT( pOutFoundSectors!=nullptr && pOutIdEnds!=nullptr );
		// - some Sectors may start just after the IndexPulse (e.g. MDOS 1.0); preparing the scanner for this eventuality by adjusting the frequency and phase shortly BEFORE the IndexPulse
		const TLogTime indexTime=GetIndexTime(0);
		for( SetCurrentTime(indexTime-10*profile.iwTimeDefault); currentTime<indexTime; ReadBit() ); // "N*" = number of 0x00 Bytes before the 0xA1 clock-distorted mark
		// - scanning
		TLogTime tSyncStarts[64]; BYTE iSyncStart=0;
		WORD nSectors=0, w, sync1=0; DWORD sync23=0;
		while (*this){
			// . searching for three consecutive 0xA1 distorted synchronization Bytes
			tSyncStarts[iSyncStart++&63]=currentTime;
			sync23=	(sync23<<1) | ((sync1&0x8000)!=0);
			sync1 =	(sync1<<1) | (BYTE)ReadBit();
			if ((sync1&0xffdf)!=0x4489 || (sync23&0xffdfffdf)!=0x44894489)
				continue;
			if (pOutParseEvents)
				pOutParseEvents->Add( TParseEvent( Event::SYNC_3BYTES, tSyncStarts[(iSyncStart+256-48)&63], currentTime, 0xa1a1a1 ) );
			sync1=0; // invalidating "buffered" synchronization, so that it isn't reused
			// . an ID Field mark should follow the synchronization
			if (!ReadBits16(w)) // Track end encountered
				break;
			struct{
				BYTE idam, cyl, side, sector, length;
			} data={ MFM::DecodeByte(w) };
			if ((data.idam&0xfe)!=0xfe) // not the expected ID Field mark; the least significant bit is always ignored by the FDC [http://info-coach.fr/atari/documents/_mydoc/Atari-Copy-Protection.pdf]
				continue;			
			if (pOutParseEvents)
				pOutParseEvents->Add( TParseEvent( Event::MARK_1BYTE, TimelyFromPrevious, currentTime, data.idam ) );
			// . reading SectorId
			TSectorId &rid=*pOutFoundSectors++;
			if (!ReadBits16(w)) // Track end encountered
				break;
			rid.cylinder = data.cyl = MFM::DecodeByte(w);
			if (!ReadBits16(w)) // Track end encountered
				break;
			rid.side = data.side = MFM::DecodeByte(w);
			if (!ReadBits16(w)) // Track end encountered
				break;
			rid.sector = data.sector = MFM::DecodeByte(w);
			if (!ReadBits16(w)) // Track end encountered
				break;
			rid.lengthCode = data.length = MFM::DecodeByte(w);
			if (pOutParseEvents){
				struct{
					TParseEvent base;
					BYTE etc[80];
				} peId;
				Event::TMetaString::Create( peId.base, TimelyFromPrevious, currentTime, rid.ToString() );
				pOutParseEvents->Add( peId.base );
			}
			// . reading and comparing ID Field's CRC
			DWORD dw;
			Checksum::W crc=0;
			const bool crcBad=!ReadBits32(dw) || MFM::DecodeWord(dw)!=( crc=Checksum::GetCrcIbm3740(MFM::CRC_A1A1A1,&data,sizeof(data)) ); // no or wrong IdField CRC
			*pOutIdStatuses++ =	crcBad 
								? TFdcStatus::IdFieldCrcError
								: TFdcStatus::WithoutError;
			*pOutIdEnds++=currentTime;
			*pOutIdProfiles++=profile;
			if (pOutParseEvents)
				pOutParseEvents->Add( TParseEvent( crcBad?Event::CRC_BAD:Event::CRC_OK, TimelyFromPrevious, currentTime, crc ) );
			// . sector found
			nSectors++;
		}
		// - returning the result
		return nSectors;
	}

	TFdcStatus CReader::ReadDataMfm(const TSectorId &sectorId,WORD nBytesToRead,CSharedParseEventPtr *pOutDataPe,CParseEventList *pOutParseEvents){
		// attempts to read specified amount of Bytes into the Buffer, starting at position pointed to by the BitReader; returns the amount of Bytes actually read
		ASSERT( pLogTimesInfo->codec==Codec::MFM );
		// - searching for the nearest three consecutive 0xA1 distorted synchronization Bytes
		TLogTime tSyncStarts[64]; BYTE iSyncStart=0;
		WORD w, sync1=0; DWORD sync23=0;
		while (*this){
			tSyncStarts[iSyncStart++&63]=currentTime;
			sync23=	(sync23<<1) | ((sync1&0x8000)!=0);
			sync1 =	(sync1<<1) | (BYTE)ReadBit();
			if ((sync1&0xffdf)==0x4489 && (sync23&0xffdfffdf)==0x44894489)
				break;
		}
		if (!*this) // Track end encountered
			return TFdcStatus::NoDataField;
		if (pOutParseEvents)
			pOutParseEvents->Add( TParseEvent( Event::SYNC_3BYTES, tSyncStarts[(iSyncStart+256-48)&63], currentTime, 0xa1a1a1 ) );
		// - a Data Field mark should follow the synchronization
		if (!ReadBits16(w)) // Track end encountered
			return TFdcStatus::NoDataField;
		const BYTE dam=MFM::DecodeByte(w);
		TFdcStatus result;
		switch (dam&0xfe){ // branching on observed data address mark; the least significant bit is always ignored by the FDC [http://info-coach.fr/atari/documents/_mydoc/Atari-Copy-Protection.pdf]
			case 0xfa:
				result=TFdcStatus::WithoutError;
				break;
			case 0xf8:
				result=TFdcStatus::DeletedDam;
				break;
			default:
				return TFdcStatus::NoDataField; // not the expected Data mark
		}
		if (pOutParseEvents)
			pOutParseEvents->Add( TParseEvent( Event::MARK_1BYTE, TimelyFromPrevious, currentTime, dam ) );
		// - reading specified amount of Bytes into the Buffer
		TDataParseEvent peData( sectorId, currentTime );
		auto *const pbi=peData.GetByteInfos();
		const Bit::CSequence bits( *this, nBytesToRead*MFM::CodedByteWidth );
		WORD nDataBytes=0;
		for( TLogTime dt=0; nDataBytes<nBytesToRead; )
			if (const auto &&d=bits.GetWord(nDataBytes)){
				auto &rbi=pbi[nDataBytes];
				rbi.dtStart=dt, dt=d.time-peData.tStart;
				rbi.flags=d.flags;
				peData.bytes[ nDataBytes++ ] = rbi.org.value = MFM::DecodeByte(
					rbi.org.encoded = d.value
				);
			}else{ // Track end encountered
				result.ExtendWith( TFdcStatus::DataFieldCrcError );
				break;
			}
		const Checksum::W crc=Checksum::GetCrcIbm3740(
			Checksum::GetCrcIbm3740( MFM::CRC_A1A1A1, &dam, sizeof(dam) ),
			peData.bytes, nDataBytes
		);
		peData.Finalize( currentTime, profile, nDataBytes );
		const CSharedParseEventPtr peDataPtr( peData, peData.size );
		if (nDataBytes){
			if (pOutDataPe)
				*pOutDataPe=peDataPtr;
			if (pOutParseEvents)
				pOutParseEvents->Add(peDataPtr);
		}
		if (!*this)
			return result;
		// - comparing Data Field's CRC
		DWORD dw;
		const Checksum::W crcReported= ReadBits32(dw) ? MFM::DecodeWord(dw) : 0;
		if (crcReported!=crc){ // no or wrong Data Field CRC
			result.ExtendWith( TFdcStatus::DataFieldCrcError );
			if (pOutParseEvents)
				pOutParseEvents->Add( TParseEvent( Event::CRC_BAD, TimelyFromPrevious, currentTime, crcReported ) );
		}else{
			peDataPtr->type=Event::DATA_OK;
			if (pOutParseEvents)
				pOutParseEvents->Add( TParseEvent( Event::CRC_OK, TimelyFromPrevious, currentTime, crcReported ) );
		}
		return result;
	}

	bool CReaderWriter::WriteDataMfm(TDataParseEvent &peData,TFdcStatus sr){
		// True <=> the whole DataParseEvent was written to the Track, starting at CurrentTime, otherwise False
		ASSERT( pLogTimesInfo->codec==Codec::MFM );
		// - searching for the nearest three consecutive 0xA1 distorted synchronization Bytes
		WORD w, sync1=0; DWORD sync23=0;
		while (*this){
			sync23=	(sync23<<1) | ((sync1&0x8000)!=0);
			sync1 =	(sync1<<1) | (BYTE)ReadBit();
			if ((sync1&0xffdf)==0x4489 && (sync23&0xffdfffdf)==0x44894489)
				break;
		}
		const TLogTime tIwSynced=profile.iwTime;
		TLogTimeInterval tiClear( currentTime+tIwSynced/2, Time::Infinity ); // portion of this Track to "unformat"
		// - a Data Field mark should follow the synchronization
		TLogTimeInterval ti( currentTime, peData.tStart ); // Time slot for each Byte to write (yet to correctly offset below)
		if (!ReadBits16(w)) // Track end encountered
			return false;
		BYTE dam=MFM::DecodeByte(w);
		switch (dam&0xfe){ // branching on observed data address mark; the least significant bit is always ignored by the FDC [http://info-coach.fr/atari/documents/_mydoc/Atari-Copy-Protection.pdf]
			case 0xfa: // normal data
				if (sr.DescribesDeletedDam()) // want a deleted data DAM instead?
					dam=0xf8;
				break;
			case 0xf8: // deleted data
				if (!sr.DescribesDeletedDam()) // want a normal data DAM instead?
					dam=0xfa;
				break;
			default:
				return false; // not the expected Data mark
		}
		// - write new Data Field mark to temporary storage
		CReaderWriter tmp( peData.GetByteCount()*MFM::CodedByteWidth, TDecoderMethod::KEIR_FRASER, true ); // temporary storage for new fluxes
		MFM::g_prevDataBit=true; // the previous data bit in a distorted 0xA1 sync mark is a "1"
		tmp.AddWord( ti, MFM::EncodeByte(dam) );
		// - write new Bytes to temporary storage
		auto *const pbi=peData.GetByteInfos();
		ASSERT( currentTime-profile.iwTime<peData.tStart && peData.tStart<currentTime+profile.iwTime ); // sanity check (we shouldn't be much off the original Start)
		for( WORD i=0; i<peData.GetByteCount(); i++ ){
			auto &rbi=pbi[i];
			auto &org=rbi.org;
			ti.tEnd=peData.GetByteTime(i+1);
			if (peData.bytes[i]==org.value) // Byte not changed
				tmp.AddWord( ti, org.encoded ); // use however it is encoded (even wrongly, e.g. non-formatted area)
			else{
				tmp.AddWord( ti,
					org.encoded = MFM::EncodeByte(
						org.value = peData.bytes[i]
					)
				);
				auto &next=pbi[i+1].org;
				if (org.encoded.w&1 && next.encoded.w>=0x8000) // transition mustn't consist of two 1's as they tend to magnetically join together
					next.value=~next.value; // updated clock bits
				rbi.MarkHealthy();
			}
		}
		// - write new CRC16 to temporary storage
		SetCurrentTimeAndProfile( peData.tEnd, peData.profileEnd );
		DWORD dw;
		if (!ReadBits32(dw)) // Track end encountered (the CRC doesn't fit in the Track)
			return false;
		Checksum::W crc=Checksum::GetCrcIbm3740(
			Checksum::GetCrcIbm3740( MFM::CRC_A1A1A1, &dam, sizeof(dam) ),
			peData.bytes, peData.GetByteCount()
		);
		if (sr.DescribesDataFieldCrcError())
			crc=~crc;
		ti.tEnd=currentTime;
		tmp.AddDWord( ti, MFM::EncodeWord(crc) );
		// - write an extra "0" bit if the CRC ends with "1" (leaving this case uncovered often leads to magnetic problems)
		if (MFM::g_prevDataBit) // CRC ends with "1" ...
			ReadBit(); // ... so the gap must begin with "0" (this read bit will be reset)
		// - dump the temporary storage to this Track
		tmp.Offset(tIwSynced); // Timing thus far offset backwards by one InspectionWindow to comply with DataParseEvent (and Decoders), hence correcting it forwards
		tiClear.tEnd=tIwSynced+currentTime+profile.iwTime/2;
		return	ReplaceTimes( tiClear, tmp );
	}

	char CReader::ReadByte(Bit::TPattern &rOutBits,PBYTE pOutValue){
		// reads number of bits corresponding to one Byte; if all such bits successfully read, returns their count, or -1 otherwise
		switch (pLogTimesInfo->codec){
			case Codec::FM:
				ASSERT(FALSE); //TODO
				return -1;
			case Codec::MFM:
				if (ReadBits16(rOutBits)){ // all bits read?
					if (pOutValue)
						*pOutValue=MFM::DecodeByte(rOutBits);
					return 16;
				}else
					return -1;
			default:
				ASSERT(FALSE); // we shouldn't end up here - check if all Codecs are included in the Switch statement!
				return -1;
		}
	}

	bool CReader::IsLastReadBitHealthy() const{
		// True <=> the bit read last by ReadBit* methods is well encoded, otherwise False (first bits on a Track or right after Index may be evaluated unreliably!)
		switch (pLogTimesInfo->codec){
			case Codec::FM:
				return true;
			case Codec::MFM:
				return IsWellEncodedMfmSequence( lastReadBits );
			default:
				ASSERT(FALSE); // we shouldn't end up here - check if all Codecs are included in the Switch statement!
				return false;
		}
	}

	Bit::CSequence CReader::CreateBitSequence(const TLogTimeInterval &ti,BYTE oneOkPercent) const{
		// records Decoder processing between the specified TimeInterval into a BitSequence
		return CreateBitSequence( ti.tStart, CreateResetProfile(), ti.tEnd, oneOkPercent );
	}

	Bit::CSequence CReader::CreateBitSequence(Revolution::TType rev,BYTE oneOkPercent) const{
		// records Decoder processing of specified full Revolution into a BitSequence
		return CreateBitSequence( GetFullRevolutionTimeInterval(rev), oneOkPercent );
	}

	Bit::CSequence CReader::CreateBitSequence(BYTE oneOkPercent) const{
		// records Decoder processing for the whole Track into a BitSequence
		return CreateBitSequence( 0, CreateResetProfile(), GetTotalTime(), oneOkPercent );
	}

	CBits CReader::CreateFullRevBitSequences(BYTE oneOkPercent) const{
		CBits result;
		static_cast<Bit::CSequence &>(result)=CreateBitSequence(oneOkPercent);
		for( TRev i=1; i<nIndexPulses; i++ )
			result.revs[i-1]=Bit::CSequence( result, GetFullRevolutionTimeInterval(i-1) );
		return result;
	}













	typedef Time::TMetaDataItem TMetaDataItem;

	const CReaderWriter CReaderWriter::Invalid( 0, Time::Decoder::NONE, false ); // TrackReader invalid right from its creation

	CReaderWriter::CReaderWriter(Time::N nLogTimesMax,TDecoderMethod method,bool resetDecoderOnIndex)
		// ctor
		: CReader(
			nLogTimesMax+LogTimesCountExtra, method,
			new CLogTimesInfo( resetDecoderOnIndex ),
			Codec::MFM
		){
	}

	CReaderWriter::CReaderWriter(const CReaderWriter &trw,bool shareTimes)
		// copy ctor
		: CReader( trw ) {
		if (!shareTimes){
			CReaderWriter tmp( trw.GetBufferCapacity(), trw.profile.method, trw.pLogTimesInfo->resetDecoderOnIndex );
			std::swap<CReaderBuffers>( tmp, *this );
			AddExternalTimes( trw.logTimes, trw.nLogTimes );
			*static_cast<TLogTimesInfoData *>(pLogTimesInfo)=*trw.pLogTimesInfo;
		}
	}

	CReaderWriter::CReaderWriter(Time::N nLogTimes,Medium::TType mediumType)
		// ctor ('nLogTimes' uniformly distributed across a single-Revolution Track)
		: CReader(
			nLogTimes+LogTimesCountExtra, TDecoderMethod::KEIR_FRASER,
			new CLogTimesInfo( true ),
			Codec::MFM
		){
		SetMediumType(mediumType);
		AddIndexTime(0);
			for( TLogTime t=0; t<nLogTimes; AddTime(++t) );
		AddIndexTime( nLogTimes );
		Normalize();
	}
	
	CReaderWriter::CReaderWriter(CReaderWriter &&rTrackReaderWriter)
		// move ctor
		: CReader( std::move(rTrackReaderWriter) ) {
	}
	
	CReaderWriter::CReaderWriter(const CReader &tr)
		// copy ctor
		: CReader(tr) {
	}

	void CReaderWriter::AddTime(TLogTime logTime){
		// appends LogicalTime at the end of the Track
		ASSERT( nLogTimes<GetBufferCapacity() );
		ASSERT( logTime>=0 );
		logTimes[nLogTimes++]=logTime;
		pLogTimesInfo->rawDeviceData.reset(); // modified Track is no longer as we received it from the Device
	}

	void CReaderWriter::AddExternalTimes(PCLogTime logTimes,Time::N nLogTimes){
		// appends given amount of LogicalTimes at the end of the Track
		::memcpy( this->logTimes+this->nLogTimes, logTimes, nLogTimes*sizeof(TLogTime) );
		this->nLogTimes+=nLogTimes;
	}

	void CReaderWriter::AddTimes(PCLogTime logTimes,Time::N nLogTimes){
		// appends given amount of LogicalTimes at the end of the Track
		ASSERT( this->nLogTimes+nLogTimes<=GetBufferCapacity() );
		if (this->logTimes+this->nLogTimes==logTimes)
			// caller wrote directly into the buffer (e.g. creation of initial content); faster than calling N-times AddTime
			this->nLogTimes+=nLogTimes;
		else{
			// caller used its own buffer to store new LogicalTimes
			AddExternalTimes( logTimes, nLogTimes );
		}
		pLogTimesInfo->rawDeviceData.reset(); // modified Track is no longer as we received it from the Device
	}

	void CReaderWriter::AddByte(TLogTimeInterval &at,BYTE b){
		// appends given Byte (most significant bit first) at the end of the Track; returns the least significant bit written
		ASSERT( GetTotalTime()<at.tStart );
		TLogTime tmpLogTimes[CHAR_BIT],L=at.GetLength(); BYTE nTmpLogTimes=0;
		for( BYTE i=0; b!=0; b<<=1,i++ )
			if ((char)b<0)
				tmpLogTimes[nTmpLogTimes++]=at.tStart+i*L/CHAR_BIT;
		AddTimes( tmpLogTimes, nTmpLogTimes );
		at.tStart=at.tEnd; // advance Time in favor of the caller
	}

	void CReaderWriter::AddWord(TLogTimeInterval &at,WORD w){
		// appends given Word (most significant bit first) at the end of the Track; returns the least significant bit written
		const TLogTime tCenter=at.tStart+at.GetLength()/2; // avoid overflow
		AddByte( TLogTimeInterval(at.tStart,tCenter), HIBYTE(w) );
		at.tStart=tCenter; // advance Time in favor of the caller
		AddByte( at, LOBYTE(w) );
	}

	void CReaderWriter::AddDWord(TLogTimeInterval &at,DWORD dw){
		// appends given DWord (most significant bit first) at the end of the Track; returns the least significant bit written
		const TLogTime tCenter=at.tStart+at.GetLength()/2; // avoid overflow
		AddWord( TLogTimeInterval(at.tStart,tCenter), HIWORD(dw) );
		at.tStart=tCenter; // advance Time in favor of the caller
		AddWord( at, LOWORD(dw) );
	}

	void CReaderWriter::AddIndexTime(TLogTime logTime){
		// appends LogicalTime representing the position of the index pulse on the disk
		ASSERT( nIndexPulses<=Revolution::MAX );
		ASSERT( logTime>=0 );
		indexPulses[nIndexPulses++]=logTime;
		indexPulses[nIndexPulses]=Time::Infinity;
		pLogTimesInfo->rawDeviceData.reset(); // modified Track is no longer as we received it from the Device
	}

	void CReaderWriter::TrimToTimesCount(Time::N nKeptLogTimes){
		// discards some tail LogicalTimes, keeping only specified amount of them
		ASSERT( nKeptLogTimes<=nLogTimes ); // can only shrink
		nLogTimes=nKeptLogTimes;
		pLogTimesInfo->rawDeviceData.reset(); // modified Track is no longer as we received it from the Device
	}

	void CReaderWriter::AddMetaData(const TMetaDataItem &mdi){
		// inserts the MetaDataItem, eventually overwritting some existing MetaDataItems
		if (!mdi) // empty or invalid?
			return;
		auto &metaData=pLogTimesInfo->metaData;
		auto it=metaData.lower_bound(mdi);
		// - do we FULLY clear any later MetaDataItem?
		while (it!=metaData.end())
			if (it->tEnd<=mdi.tEnd){ // yes, we do
				const auto itErase=it++;
				metaData.erase(itErase); // remove such Emphasis
			}else
				break;
		// - do we PARTLY clear the nearest later MetaDataItem?
		if (it!=metaData.end())
			if (it->tStart<mdi.tEnd){ // yes, we do
				TMetaDataItem tmp=*it;
				metaData.erase(it), it=metaData.insert(tmp.Split(mdi.tEnd)).first; // replace such MetaDataItem
			}
		// - do we anyhow overwrite the nearest previous MetaDataItem?
		if (it!=metaData.begin()){
			it--;
			if (mdi.tEnd<it->tEnd){ // yes, we split the previous MetaDataItem into two pieces
				TMetaDataItem tmp=const_cast<TMetaDataItem &>(*it).Split(mdi.tStart); // first part
				it=metaData.insert( tmp.Split(mdi.tEnd) ).first; // second part
			}else if (mdi.tStart<it->tEnd) // yes, only partly the end
				const_cast<TMetaDataItem &>(*it++).Split(mdi.tStart);
			else
				it++;
		}
		// - was this a deletion call?
		if (mdi.IsDefault())
			return; // deletion has just been done
		// - can the new MetaDataItem be merged with nearest next one?
		bool merged=false;
		if (it!=metaData.end() && it->tStart==mdi.tEnd && it->Equals(mdi)){ // yes, it can
			TMetaDataItem tmp=*it;
				tmp.tStart=mdi.tStart;
				tmp.nBits+=mdi.nBits;
			metaData.erase(it), it=metaData.insert(tmp).first; // merge them
			merged=true;
		}
		// - can the new MetaDataItem be merged with nearest previous one?
		if (it!=metaData.begin())
			if ((--it)->tEnd==mdi.tStart && it->Equals(mdi)){ // yes, it can
				const_cast<TLogTime &>(it->tEnd)=mdi.tEnd; // merge them; doing this is OK because the End doesn't serve as the key for the MetaDataItem std::set
				const_cast<int &>(it->nBits)+=mdi.nBits;
				if (merged) // already Merged above?
					metaData.erase(++it);
				else
					merged=true;
			}
		// - adding the new MetaDataItem
		if (!merged)
			metaData.insert(mdi);
	}

	void CReaderWriter::SetRawDeviceData(TTypeId dataId,const Utils::CSharedBytes &data){
		// remembers data as they were received from a disk (later used for fast copying between compatible disks)
		static_cast<Utils::CSharedBytes &>(pLogTimesInfo->rawDeviceData)=data;
		pLogTimesInfo->rawDeviceData.id=dataId;
	}

	void CReaderWriter::ClearMetaData(TLogTime a,TLogTime z){
		// removes (or just shortens) all MetaDataItems in specified range
		AddMetaData( TLogTimeInterval(a,z) );
		FindMetaDataIteratorAndApply();
	}

	void CReaderWriter::ClearAllMetaData(){
		// removes all MetaDataItems
		auto &metaData=pLogTimesInfo->metaData;
		metaData.clear();
		FindMetaDataIteratorAndApply();
	}

	TLogTime CReader::GetLastIndexTime() const{
		// returns the LogicalTime of the last added Index (or 0)
		return	nIndexPulses ? indexPulses[nIndexPulses-1] : 0;
	}

	bool CReaderWriter::ReplaceTimes(const TLogTimeInterval &clearTimes,const CReader &writeTimes){
		// True <=> new LogicalTimes written to the cleared interval, otherwise False
		ASSERT(writeTimes.GetTimesCount()>0);
		ASSERT( clearTimes.tStart<=*writeTimes.GetBuffer() && writeTimes.GetLastTime()<clearTimes.tEnd ); // must only write into region that has been cleared
		// - determining the number of LogicalTimes in the interval to clear
		SetCurrentTime(clearTimes.tStart);
		const auto iLogTimeToClearA=iNextTime;
		SetCurrentTime(clearTimes.tEnd);
		const auto nLogTimesToClear=iNextTime-iLogTimeToClearA;
		// - replacing the LogicalTimes
		const Time::N nNewLogTimes=nLogTimes+writeTimes.GetTimesCount()-nLogTimesToClear;
		if (nNewLogTimes>GetBufferCapacity())
			return false;
		::memmove(
			logTimes+iLogTimeToClearA+writeTimes.GetTimesCount(),
			logTimes+iNextTime,
			(nLogTimes-iNextTime)*sizeof(TLogTime)
		);
		nLogTimes=nNewLogTimes;
		::memcpy(
			logTimes+iLogTimeToClearA,
			writeTimes.GetBuffer(),
			writeTimes.GetTimesCount()*sizeof(TLogTime)
		);
		pLogTimesInfo->rawDeviceData.reset(); // modified Track is no longer as we received it from the Device
		return true;
	}

	bool CReaderWriter::WriteData(TLogTime idEndTime,const TProfile &idEndProfile,TDataParseEvent &peData,TFdcStatus sr){
		// True <=> the whole DataParseEvent was written to the Track, starting after specified IdEndTime, otherwise False
		SetCurrentTimeAndProfile( idEndTime, idEndProfile );
		const Utils::CVarTempReset<bool> rdoi0( pLogTimesInfo->resetDecoderOnIndex, false ); // never reset when reading data
		switch (pLogTimesInfo->codec){
			case Codec::FM:
				return	WriteDataFm( peData, sr );
			case Codec::MFM:
				return	WriteDataMfm( peData, sr );
			default:
				ASSERT(FALSE); // we shouldn't end up here - all Codecs should be included in the Switch statement!
				return 0;
		}
	}

	static Time::N InterpolateTimes(PLogTime logTimes,Time::N nLogTimes,TLogTime tSrcA,Time::N iSrcA,TLogTime tSrcZ,TLogTime tDstA,TLogTime tDstZ){
		// in-place interpolation of LogicalTimes in specified range; returns an "index-pointer" to the first unprocessed LogicalTime (outside the range)
		const Utils::CVarTempReset<TLogTime> tStopOrg( logTimes[nLogTimes], Time::Infinity ); // stop-condition
			PLogTime pTime=logTimes+iSrcA;
			for( const TLogTime tSrcInterval=tSrcZ-tSrcA,tDstInterval=tDstZ-tDstA; *pTime<tSrcZ; pTime++ )
				*pTime = tDstA+(LONGLONG)(*pTime-tSrcA)*tDstInterval/tSrcInterval;
		return pTime-logTimes;
	}

	TStdWinError CReaderWriter::Normalize(){
		// True <=> asked and successfully normalized for a known MediumType, otherwise False
		return NormalizeEx( 0, false, false, true );
	}

	TStdWinError CReaderWriter::NormalizeEx(TLogTime indicesOffset,bool fitTimesIntoIwMiddles,bool correctCellCountPerRevolution,bool correctRevolutionTime){
		// True <=> all Revolutions of this Track successfully normalized using specified parameters, otherwise False
		ASSERT( pLogTimesInfo->GetRefCount()==1 ); // normalization of a TrackReaderWriter that is used more than once always needs an attention
		// - if the Track contains less than two Indices, we are successfully done
		if (nIndexPulses<2)
			return ERROR_SUCCESS;
		// - MediumType must be supported
		const Medium::PCProperties mp=pLogTimesInfo->mediumProps;
		if (!mp)
			return ERROR_UNRECOGNIZED_MEDIA;
		ClearAllMetaData();
		pLogTimesInfo->rawDeviceData.reset(); // modified Track is no longer as we received it from the Device
		// - shifting Indices by shifting all Times in oposite direction
		const TLogTime tLastIndexOrg=GetLastIndexTime();
		if (indicesOffset){
			const TLogTime dt= indicesOffset<0
				? std::max(*indexPulses+indicesOffset,0)-*indexPulses // mustn't run into negative timing
				: indicesOffset;
			for( TRev i=nIndexPulses; i; indexPulses[--i]+=dt );
		}
		// - ignoring what's before the first Index
		TLogTime tCurrIndexOrg=RewindToIndex(0);
		// - normalization
		const Time::N iModifStart=iNextTime;
		Time::N iTime=iModifStart;
		const Time::CSharedArray buffer( GetBufferCapacity() );
		const PLogTime ptModified=buffer;
		for( TRev nextIndex=1; nextIndex<nIndexPulses; nextIndex++ ){
			// . resetting inspection conditions
			profile.Reset();
			const TLogTime tNextIndexOrg=GetIndexTime(nextIndex);
			const Time::N iModifRevStart=iTime;
			// . alignment of LogicalTimes to inspection window centers
			Time::N nAlignedCells=0;
			if (fitTimesIntoIwMiddles){
				// alignment wanted
				for( ; *this&&logTimes[iNextTime]<tNextIndexOrg; nAlignedCells++ )
					if (ReadBit())
						if (iTime<buffer.length)
							ptModified[iTime++] = tCurrIndexOrg + nAlignedCells*profile.iwTimeDefault;
						else
							return ERROR_INSUFFICIENT_BUFFER; // mustn't overrun the Buffer
			}else
				// alignment not wanted - just copying the Times in current Revolution
				while (*this && logTimes[iNextTime]<tNextIndexOrg)
					ptModified[iTime++]=ReadTime();
			Time::N iModifRevEnd=iTime;
			// . shortening/prolonging this revolution to correct number of cells
			if (correctCellCountPerRevolution){
				ptModified[iModifRevEnd]=Time::Infinity; // stop-condition
				if (nAlignedCells>0){ // are we working with time-corrected cells?
					iModifRevEnd=iModifRevStart;
					const TLogTime tRevEnd=tCurrIndexOrg+mp->revolutionTime;
					while (ptModified[iModifRevEnd]<tRevEnd)
						iModifRevEnd++;
					nAlignedCells=mp->nCells;
				}//else
					//nop (not applicable)
			}
			// . correction of index-to-index time distance
			if (correctRevolutionTime) // index-to-index time correction enabled?
				indexPulses[nextIndex]=indexPulses[nextIndex-1]+mp->revolutionTime;
			const TLogTime tNextIndexWork =	nAlignedCells>0 // are we working with time-corrected cells?
											? tCurrIndexOrg+nAlignedCells*profile.iwTimeDefault
											: tNextIndexOrg;
			if (tCurrIndexOrg!=indexPulses[nextIndex-1] || tNextIndexWork!=indexPulses[nextIndex])
				InterpolateTimes(
					ptModified, iModifRevEnd,
					tCurrIndexOrg, iModifRevStart, tNextIndexWork,
					indexPulses[nextIndex-1], indexPulses[nextIndex]
				);
			// . next Revolution
			tCurrIndexOrg=tNextIndexOrg;
			iTime=iModifRevEnd;
		}
		// - copying Modified LogicalTimes to the Track
		const TLogTime dtLast=GetLastIndexTime()-tLastIndexOrg;
		for( auto i=iNextTime; i<nLogTimes; logTimes[i++]+=dtLast );
		::memmove( logTimes+iTime, logTimes+iNextTime, (nLogTimes-iNextTime)*sizeof(TLogTime) ); // Times after last Index
		::memcpy( logTimes+iModifStart, ptModified+iModifStart, (iTime-iModifStart)*sizeof(TLogTime) ); // Times in full Revolutions
		nLogTimes+=iTime-iNextTime;
		SetCurrentTime(0); // setting valid state
		// - successfully normalized
		return ERROR_SUCCESS;
	}

	CReaderWriter &CReaderWriter::Reverse(){
		// reverses timing of this Track
		// - reversing Indices
		const auto tTotal=GetTotalTime();
		for( TRev i=0; i<GetIndexCount()/2; i++ )
			std::swap( indexPulses[i], indexPulses[GetIndexCount()-1-i] );
		for( TRev i=0; i<GetIndexCount(); i++ )
			indexPulses[i]=tTotal-indexPulses[i];
		// - reversing Times
		for( Time::N i=0; i<nLogTimes/2; i++ )
			std::swap( logTimes[i], logTimes[nLogTimes-1-i] );
		for( Time::N i=0; i<nLogTimes; i++ )
			logTimes[i]=tTotal-logTimes[i];
		// - reversing MetaData
		Time::CMetaData metaData;
		for each( auto mdi in GetMetaData() ){
			std::swap( mdi.tStart, mdi.tEnd );
			mdi.tStart=tTotal-mdi.tStart;
			mdi.tEnd=tTotal-mdi.tEnd;
			metaData.insert(mdi);
		}
		pLogTimesInfo->metaData=metaData;
		//pLogTimesInfo->rawDeviceData.reset(); // commented out as reversal occurs only for purposes of this application
		return *this;
	}

	CReaderWriter &CReaderWriter::Offset(TLogTime dt){
		// offsets timing in this Track
		for( auto i=nLogTimes; i>0; logTimes[--i]+=dt );
		return *this;
	}
}
