#include "stdafx.h"
#include "Charting.h"

	CImage::CTrackReaderBase::CTrackReaderBase(PLogTime logTimes,PLogTimesInfo pLti,TDecoderMethod method)
		// ctor
		: logTimes(logTimes) , nLogTimes(0) , pLogTimesInfo(pLti)
		, indexPulses(pLti->indexPulses) , iNextIndexPulse(0) , nIndexPulses(0)
		, iNextTime(0) , currentTime(0) , lastReadBits(0)
		, profile(method) {
		itCurrMetaData=pLogTimesInfo->metaData.cbegin();
	}

	CImage::CTrackReaderBase::PCMetaDataItem CImage::CTrackReaderBase::GetCurrentTimeMetaData() const{
		// returns the MetaDataItem that contain the CurrentTime, or Null
		if (itCurrMetaData!=pLogTimesInfo->metaData.cend()){
			ASSERT( itCurrMetaData->Contains(currentTime) ); // just to be sure
			return &*itCurrMetaData;
		}else
			return nullptr;
	}

	CImage::CTrackReaderBase::PCMetaDataItem CImage::CTrackReaderBase::ApplyCurrentTimeMetaData(){
		// uses the MetaDataItem at CurrentTime for reading
		if (const PCMetaDataItem pmdi=GetCurrentTimeMetaData()){
			// application of found MetaDataItem
			return pmdi;
		}else
			return nullptr;
	}

	CImage::CTrackReaderBase::PCMetaDataItem CImage::CTrackReaderBase::IncrMetaDataIteratorAndApply(){
		// the CurrentTime has moved forward, and the good strategy is to iterate to the MetaDataItem that contains it
		while (itCurrMetaData!=pLogTimesInfo->metaData.cend())
			if (currentTime<itCurrMetaData->tEnd)
				return ApplyCurrentTimeMetaData();
			else
				itCurrMetaData++;
		return nullptr;
	}

	CImage::CTrackReaderBase::PCMetaDataItem CImage::CTrackReaderBase::FindMetaDataIteratorAndApply(){
		// the CurrentTime has changed randomly
		itCurrMetaData=pLogTimesInfo->metaData.upper_bound( TLogTimeInterval(currentTime,INT_MAX) ); // 'upper_bound' = don't search the sharp beginning but rather something bigger ...
		if (itCurrMetaData!=pLogTimesInfo->metaData.cbegin())
			itCurrMetaData--; // ... and then iterate back, because that's the usual case when "randomly" pinning in the timeline
		return ApplyCurrentTimeMetaData();
	}






	CImage::CTrackReaderBase::CMetaData::const_iterator CImage::CTrackReaderBase::CMetaData::GetMetaDataIterator(TLogTime t) const{
		// returns an iterator to a MetaDataItem that contains the specified Time, or 'cend()'
		auto it=upper_bound( TLogTimeInterval(t,INT_MAX) ); // 'upper_bound' = don't search the sharp beginning but rather something bigger ...
		if (it!=cend())
			if (it!=cbegin() && (--it)->Contains(t)) // ... and then iterate back, because that's the usual case when "randomly" pinning in the timeline
				return it;
		return cend();
	}

	CImage::CTrackReaderBase::PCMetaDataItem CImage::CTrackReaderBase::CMetaData::GetMetaDataItem(TLogTime t) const{
		// returns a MetaDataItem that contains the specified Time, or Null
		const auto it=GetMetaDataIterator(t);
		return it!=cend() ? &*it : nullptr;
	}

	CImage::CTrackReaderBase::PCMetaDataItem CImage::CTrackReaderBase::CMetaData::GetFirst() const{
		return size() ? &*cbegin() : nullptr;
	}

	CImage::CTrackReaderBase::PCMetaDataItem CImage::CTrackReaderBase::CMetaData::GetLast() const{
		if (!size())
			return nullptr;
		auto it=cend();
		return &*--it;
	}

	CImage::CTrackReaderBase::TMetaDataItem::TMetaDataItem(const TLogTimeInterval &ti)
		// ctor (for clearing MetaData in specified Interval)
		: TLogTimeInterval(ti)
		, isFuzzy(false) , nBits(0) {
	}
	
	CImage::CTrackReaderBase::TMetaDataItem::TMetaDataItem(const TLogTimeInterval &ti,bool isFuzzy,int nBits)
		// ctor
		: TLogTimeInterval(ti)
		, isFuzzy(isFuzzy) , nBits(nBits) {
	}

	TLogTime CImage::CTrackReaderBase::TMetaDataItem::GetBitTimeAvg() const{
		// computes and returns the average inspection window size
		ASSERT( nBits>0 );
		return GetLength()/nBits;
	}

	TLogTime CImage::CTrackReaderBase::TMetaDataItem::GetBitTime(int iBit) const{
		// computes and returns the LogicalTime of I-th bit (aka. the center of I-th inspection window)
		ASSERT( 0<=iBit && iBit<nBits );
		return	tStart + ::MulDiv( GetLength(), iBit, nBits );
	}

	int CImage::CTrackReaderBase::TMetaDataItem::GetBitIndex(TLogTime t) const{
		//
		ASSERT( Contains(t) );
		return ::MulDiv( t-tStart, nBits, GetLength() ); // mathematic rounding
	}

	TLogTimeInterval CImage::CTrackReaderBase::TMetaDataItem::GetIw(int iBit) const{
		// determines and returns the inspection window that has the I-th bit at its center
		const TLogTime tCenter=GetBitTime(iBit), tAvgSize=GetBitTimeAvg();
		const TLogTime tStart=tCenter-tAvgSize/2;
		return TLogTimeInterval( tStart, tStart+tAvgSize );
	}

	CImage::CTrackReaderBase::TMetaDataItem CImage::CTrackReaderBase::TMetaDataItem::Split(TLogTime tAt){
		// trims this MetaDataItem to Interval [tStart,tAt), returning the rest; this semantics is compatible with 'operator<' which uses 'tStart' - when "un-const-ed", Split(.) can be call directly on 'std::set' items (which is nasty, btw)
		const int iBit=GetBitIndex(tAt);
		ASSERT( GetBitTime(iBit)==tAt ); // up to the caller to correctly align!
		const TMetaDataItem result( TLogTimeInterval(tAt,tEnd), isFuzzy, nBits-iBit );
		nBits=iBit;
		return result;
	}

	bool CImage::CTrackReaderBase::TMetaDataItem::Equals(const TMetaDataItem &r) const{
		// True <=> the two MetaDataItems can be merged, otherwise False
		return isFuzzy==r.isFuzzy && GetBitTimeAvg()==r.GetBitTimeAvg();
	}

	CImage::CTrackReader::TLogTimesInfoData::TLogTimesInfoData(DWORD nLogTimesMax,TDecoderMethod defaultDecoder,bool resetDecoderOnIndex)
		// ctor
		: nLogTimesMax(nLogTimesMax)
		, mediumType(Medium::UNKNOWN) , codec(Codec::UNKNOWN)
		, defaultDecoder(defaultDecoder)
		, resetDecoderOnIndex(resetDecoderOnIndex) {
		*indexPulses=INT_MAX; // a virtual IndexPulse in infinity
	}

	CImage::CTrackReader::CLogTimesInfo::CLogTimesInfo(DWORD nLogTimesMax,TDecoderMethod defaultDecoder,bool resetDecoderOnIndex)
		// "ctor"
		: TLogTimesInfoData( nLogTimesMax, defaultDecoder, resetDecoderOnIndex )
		, nRefs(1) {
	}

	bool CImage::CTrackReaderBase::CLogTimesInfo::Release(){
		// "dtor"
		if (::InterlockedDecrement(&nRefs)==0){
			delete this;
			return true;
		}else
			return false;
	}






	CImage::CTrackReader::CTrackReader(PLogTime logTimes,PLogTimesInfo pLti,Codec::TType codec,TDecoderMethod method)
		// ctor
		: CTrackReaderBase( logTimes, pLti, method ) {
		SetCodec(codec); // setting values associated with the specified Codec
	}

	CImage::CTrackReader::CTrackReader(const CTrackReader &tr)
		// copy ctor
		: CTrackReaderBase(tr) {
		pLogTimesInfo->AddRef();
	}

	CImage::CTrackReader::CTrackReader(CTrackReader &&tr)
		// move ctor
		: CTrackReaderBase(tr) {
		pLogTimesInfo->AddRef();
	}

	CImage::CTrackReader::~CTrackReader(){
		// dtor
		if (pLogTimesInfo->Release())
			::free(logTimes);
	}




	constexpr TLogTime TimelyFromPrevious=INT_MIN;

	void CImage::CTrackReader::SetCurrentTime(TLogTime logTime){
		// seeks to the specified LogicalTime
		if (!nLogTimes)
			return;
		if (logTime<0)
			logTime=0;
		for( iNextIndexPulse=0; iNextIndexPulse<nIndexPulses; iNextIndexPulse++ )
			if (logTime<indexPulses[iNextIndexPulse])
				break;
		if (logTime<*logTimes){
			iNextTime=0;
			currentTime=logTime;
		}else{
			DWORD L=0, R=nLogTimes;
			do{
				const DWORD M=(L+R)/2;
				if (logTimes[L]<=logTime && logTime<logTimes[M])
					R=M;
				else
					L=M;
			}while (R-L>1);
			iNextTime=R;
			currentTime= R<nLogTimes ? logTime : logTimes[L];
		}
		lastReadBits=0;
		if (const PCMetaDataItem pmdi=FindMetaDataIteratorAndApply()){
			profile.method=TDecoderMethod::METADATA;
			profile.methodState.metaData.iCurrBit=pmdi->GetBitIndex(currentTime)-1;
		}else if (profile.method==TDecoderMethod::METADATA){
			profile.method=pLogTimesInfo->defaultDecoder;
			profile.Reset();
		}
	}

	void CImage::CTrackReader::SetCurrentTimeAndProfile(TLogTime logTime,const TProfile &profile){
		// seeks to the specified LogicalTime, setting also the specified Profile at that LogicalTime
		this->profile=profile; // eventually overridden ...
		SetCurrentTime(logTime); // ... here
	}

	CImage::CTrackReader::TProfile CImage::CTrackReader::CreateResetProfile() const{
		// creates and returns current Profile that is reset
		TProfile result=profile;
		result.Reset();
		return result;
	}

	TLogTime CImage::CTrackReader::TruncateCurrentTime(){
		// truncates CurrentTime to the nearest lower LogicalTime, and returns it
		if (!iNextTime)
			currentTime=0;
		else if (iNextTime<nLogTimes)
			currentTime=logTimes[iNextTime-1];
		else
			currentTime=logTimes[nLogTimes-1];
		FindMetaDataIteratorAndApply();
		return currentTime;
	}

	TLogTime CImage::CTrackReader::GetIndexTime(BYTE index) const{
		// returns the Time at which the specified IndexPulse occurs
		if (!nLogTimes || (nIndexPulses|index)==0)
			return 0;
		else
			return	index<nIndexPulses
					? indexPulses[index]
					: logTimes[nLogTimes-1];
	}

	TLogTime CImage::CTrackReader::GetAvgIndexDistance() const{
		// given at least two indices, computes and returns the average distance between them, otherwise 0
		if (nIndexPulses<2)
			return 0;
		LONGLONG distSum=0;
		for( BYTE i=1; i<nIndexPulses; i++ )
			distSum+= indexPulses[i]-indexPulses[i-1];
		return distSum/(nIndexPulses-1);
	}

	TLogTime CImage::CTrackReader::GetLastTime() const{
		// returns the last recorded Time
		return	nLogTimes>0 ? logTimes[nLogTimes-1] : 0;
	}

	TLogTime CImage::CTrackReader::GetTotalTime() const{
		// returns the minimum time that covers both Indices and recorded Times
		return	std::max( GetLastTime(), GetLastIndexTime() );
	}

	PCBYTE CImage::CTrackReader::GetRawDeviceData(TId dataId,DWORD &outLength) const{
		// retrieves data as they were received from a disk (e.g. used for fast copying between compatible disks)
		if (const auto *p=pLogTimesInfo->rawDeviceData.get())
			if (pLogTimesInfo->rawDeviceData.id==dataId){
				outLength=p->length;
				return p->get();
			}
		return nullptr;
	}

	TLogTime CImage::CTrackReader::ReadTime(){
		// returns the next LogicalTime (or zero if all time information already read)
		if (*this){
			currentTime=logTimes[iNextTime++];
			IncrMetaDataIteratorAndApply();
			return currentTime;
		}else
			return 0;
	}

	void CImage::CTrackReaderBase::SetCodec(Codec::TType codec){
		// changes the interpretation of recorded LogicalTimes according to the new Codec
		switch ( pLogTimesInfo->codec=codec ){
			default:
				ASSERT(FALSE); // we shouldn't end up here, this value must be set for all implemented Codecs!
				//fallthrough
			case Codec::FM:
				nConsecutiveZerosMax=1;
				break;
			case Codec::MFM:
				nConsecutiveZerosMax=3;
				break;
		}
	}

	void CImage::CTrackReaderBase::SetMediumType(Medium::TType mediumType){
		// changes the interpretation of recorded LogicalTimes according to the new MediumType
		switch ( pLogTimesInfo->mediumType=mediumType ){
			default:
				ASSERT(FALSE); // we shouldn't end-up here, all Media Types applicable for general Track description should be covered
				//fallthrough
			case Medium::FLOPPY_DD:
				static_cast<Medium::TIwProfile &>(profile)=Medium::TProperties::FLOPPY_DD.CreateIwProfile();
				break;
			case Medium::FLOPPY_DD_525:
				static_cast<Medium::TIwProfile &>(profile)=Medium::TProperties::FLOPPY_DD_525.CreateIwProfile();
				break;
			case Medium::FLOPPY_HD_350:
				static_cast<Medium::TIwProfile &>(profile)=Medium::TProperties::FLOPPY_HD_350.CreateIwProfile();
				break;
			case Medium::FLOPPY_HD_525:
				static_cast<Medium::TIwProfile &>(profile)=Medium::TProperties::FLOPPY_HD_525.CreateIwProfile();
				break;
		}
		profile.Reset();
		ApplyCurrentTimeMetaData();
	}

	Medium::PCProperties CImage::CTrackReader::GetMediumProperties() const{
		return Medium::GetProperties( pLogTimesInfo->mediumType );
	}

	LPCTSTR CImage::CTrackReader::GetDescription(TDecoderMethod dm){
		switch (dm){
			case TDecoderMethod::NONE:
				return _T("None (just archivation)");
			case TDecoderMethod::KEIR_FRASER:
				return _T("Keir Fraser's FDC-like decoder");
			case TDecoderMethod::MARK_OGDEN:
				return _T("Mark Ogden's FDC-like decoder");
			default:
				ASSERT(FALSE);
				return nullptr;
		}
	}

	bool CImage::CTrackReader::ReadBit(TLogTime &rtOutOne){
		// returns first bit not yet read
		// - if we just crossed an IndexPulse, resetting the Profile
		if (currentTime>=indexPulses[iNextIndexPulse]){
			if (pLogTimesInfo->resetDecoderOnIndex){
				profile.Reset();
				const TLogTime indexTime=indexPulses[ iNextIndexPulse++ ];
				currentTime=indexTime;// + Utils::RoundUpToMuls( currentTime-indexTime, profile.iwTimeDefault );
				FindMetaDataIteratorAndApply();
			}else
				iNextIndexPulse++;
		}
		// - reading next bit
		switch (profile.method){
			case TDecoderMethod::NONE:
				// no decoder - aka. "don't extract bits from the record"
				if (*this){
					currentTime=profile.PeekNextIwTime(currentTime);
					while (*this && logTimes[iNextTime]<=currentTime)
						iNextTime++;
				}
				return 0;
			case TDecoderMethod::KEIR_FRASER:{
				// FDC-like flux reversal decoding from Keir Fraser's Disk-Utilities/libdisk
				// . skip all fluxes in current inspection window
				auto &r=profile.methodState.fraser;
				const TLogTime iwTimeHalf=profile.iwTime/2, tCurrIwEnd=currentTime+iwTimeHalf;
				do{
					if (!*this)
						return 0;
					if (logTimes[iNextTime]<tCurrIwEnd)
						iNextTime++;
					else
						break;
				}while (true);
				// . move to the next inspection window
				currentTime=profile.PeekNextIwTime(currentTime);
				IncrMetaDataIteratorAndApply();
				// . detect zero (longer than 1/2 of an inspection window size)
				const TLogTime diff=( rtOutOne=logTimes[iNextTime] )-currentTime;
				iNextTime+=logTimes[iNextTime]<=currentTime; // eventual correction of the pointer to the next time
				lastReadBits<<=1, lastReadBits|=1; // 'valid' flag
				lastReadBits<<=1;
				if (diff>=iwTimeHalf){
					r.nConsecutiveZeros++;
					return 0;
				}
				// . adjust data frequency according to phase mismatch
				constexpr int AdjustmentPercentMax=30; // percentual "speed" in inspection window adjustment
				if (r.nConsecutiveZeros<=nConsecutiveZerosMax)
					// in sync - adjust inspection window by percentage of phase mismatch
					profile.iwTime+= diff * AdjustmentPercentMax/100;
				else
					// out of sync - adjust inspection window towards its Default size
					profile.iwTime+= (profile.iwTimeDefault-profile.iwTime) * AdjustmentPercentMax/100;
				profile.ClampIwTime(); // keep the inspection window size within limits
				// . a "1" recognized
				r.nConsecutiveZeros=0;
				lastReadBits|=1;
				return 1;
			}
			case TDecoderMethod::MARK_OGDEN:{
				// FDC-like flux reversal decoding from Mark Ogdens's DiskTools/flux2track
				// . skip all fluxes in current inspection window
				auto &r=profile.methodState.ogden;
				const TLogTime iwTimeHalf=profile.iwTime/2, tCurrIwEnd=currentTime+iwTimeHalf;
				do{
					if (!*this)
						return 0;
					if (logTimes[iNextTime]<tCurrIwEnd)
						iNextTime++;
					else
						break;
				}while (true);
				// . move to the next inspection window
				currentTime=profile.PeekNextIwTime(currentTime);
				IncrMetaDataIteratorAndApply();
				// . detect zero (longer than 1/2 of an inspection window size)
				const TLogTime diff=( rtOutOne=logTimes[iNextTime] )-currentTime;
				lastReadBits<<=1, lastReadBits|=1; // 'valid' flag
				lastReadBits<<=1;
				if (diff>=iwTimeHalf)
					return 0;
				// . estimate new data frequency
				const BYTE iSlot=((diff+iwTimeHalf)<<4)/profile.iwTime;
				BYTE cState=1; // default is IPC
				if (iSlot<7 || iSlot>8){
					if (iSlot<7&&!r.up || iSlot>8&&r.up)
						r.up=!r.up, r.pcCnt = r.fCnt = 0;
					if (++r.fCnt>=3 || iSlot<3&&++r.aifCnt>=3 || iSlot>12&&++r.adfCnt>=3){
						static const TLogTime iwDelta=profile.iwTimeDefault/100;
						if (r.up){
							if (( profile.iwTime-=iwDelta )<profile.iwTimeMin)
								profile.iwTime=profile.iwTimeMin;
						}else
							if (( profile.iwTime+=iwDelta )>profile.iwTimeMax)
								profile.iwTime=profile.iwTimeMax;
						cState = r.fCnt = r.aifCnt = r.adfCnt = r.pcCnt = 0;
					}else if (++r.pcCnt>=2)
						cState = r.pcCnt = 0;
				}
				// . estimate new data phase
				static constexpr BYTE PhaseAdjustments[2][16]={ // C1/C2, C3
					//	8	9	A	B	C	D	E	F	0	1	2	3	4	5	6	7
					 { 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20 },
					 { 13, 14, 14, 15, 15, 16, 16, 16, 16, 16, 16, 17, 17, 18, 18, 19 }
				};
				if (const TLogTime dt= (PhaseAdjustments[cState][iSlot]*profile.iwTime>>4) - profile.iwTime){
					currentTime+=dt;
					if (dt>0)
						while (iNextTime<nLogTimes && logTimes[iNextTime]<=currentTime)
							iNextTime++;
					else
						while (iNextTime>0 && currentTime<logTimes[iNextTime-1])
							iNextTime--;
				}
				// . a "1" recognized
				lastReadBits|=1;
				return 1;
			}
			case TDecoderMethod::METADATA:{
				// a hidden decoder to help extract bits from Times tagged with MetaData
				if (!*this)
					return 0;
				auto &r=profile.methodState.metaData;
				PCMetaDataItem pmdi;
				do{
					if (!( pmdi=IncrMetaDataIteratorAndApply() )){
						ASSERT(FALSE);
						return 0;
					}
					if (++r.iCurrBit<pmdi->nBits)
						break;
					currentTime=pmdi->tEnd;
					r.iCurrBit=-1;
				}while (true);
				currentTime=pmdi->GetBitTime(r.iCurrBit);
				profile.iwTime = profile.iwTimeDefault = pmdi->GetBitTimeAvg();
				const auto &&ti=pmdi->GetIw(r.iCurrBit);
				ASSERT( ti.Contains(currentTime) ); // just to be sure
				const bool result =	ti.Contains( rtOutOne=logTimes[iNextTime] ) // // is there a Time in the second half of the inspection window?
									||
									iNextTime>0 && ti.Contains( rtOutOne=logTimes[iNextTime-1] ); // is there a Time in the first half of the inspection window?
				while (*this && logTimes[iNextTime]<=ti.tEnd)
					iNextTime++;
				lastReadBits<<=1, lastReadBits|=1; // 'valid' flag
				lastReadBits<<=1, lastReadBits|=(BYTE)result;
				return result;
			}
			default:
				ASSERT(FALSE);
				return 0;
		}
	}

	bool CImage::CTrackReader::ReadBit(){
		// returns first bit not yet read
		static TLogTime tDummy;
		return ReadBit(tDummy);
	}

	char CImage::CTrackReader::ReadBits8(BYTE &rOut){
		// returns the number of bits read into the output Byte
		for( char n=0; n<8; n++,rOut=(rOut<<1)|(BYTE)ReadBit() )
			if (!*this)
				return n;
		return 8;
	}

	bool CImage::CTrackReader::ReadBits15(WORD &rOut){
		// True <=> all 15 bits successfully read, otherwise False
		return ReadBits<15>(rOut);
	}

	bool CImage::CTrackReader::ReadBits16(WORD &rOut){
		// True <=> all 16 bits successfully read, otherwise False
		return ReadBits<16>(rOut);
	}

	bool CImage::CTrackReader::ReadBits32(DWORD &rOut){
		// True <=> all 32 bits successfully read, otherwise False
		return ReadBits<32>(rOut);
	}

	WORD CImage::CTrackReader::Scan(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,CParseEventList *pOutParseEvents){
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

	WORD CImage::CTrackReader::ScanAndAnalyze(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,PLogTime pOutDataEnds,CParseEventList &rOutParseEvents,CActionProgress &ap,bool fullAnalysis){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		constexpr int StepGranularity=1000;
		const BYTE nFullRevolutions=std::max( 0, GetIndexCount()-1 );
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
			if (!ReadData( pOutFoundSectors[s], pOutIdEnds[s], pOutIdProfiles[s], CImage::GetOfficialSectorLength(pOutFoundSectors[s].lengthCode), &peSector ).DescribesMissingDam()
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
									TParseEvent( TParseEvent::NONFORMATTED, t0, t, 0 )
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
			typedef WORD TBitPattern;
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
					const TLogTime tNextStart= itNext ? itNext->second->tStart : INT_MAX;
					SetCurrentTimeAndProfile( r.time, r.profile );
					BYTE nBytesInspected=0, nBytesTypical=0;
					TDataParseEvent peData( TSectorId::Invalid, r.time );
					while (*this && nBytesInspected<nBytesInspectedMax){
						TDataParseEvent::TByteInfo &rbi=peData.byteInfos[nBytesInspected];
							rbi.tStart=currentTime;
							ReadByte( bitPattern, &rbi.value );
						if (currentTime>=tNextStart){
							currentTime=rbi.tStart; // putting unsuitable Byte back
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
							TDataParseEvent::TByteInfo &rbi=peData.byteInfos[nBytesInspected];
								rbi.tStart=currentTime;
								ReadByte( bitPattern, &rbi.value );
							const auto it=hist.Find(bitPattern);
							if (currentTime>=tNextStart
								||
								it!=hist.end() && it->first==typicalBitPattern
							){
								currentTime=rbi.tStart; // putting unsuitable Byte back
								break; // again Typical, so probably all gap data discovered
							}
							nBytesInspected++;
						}
						peData.Finalize( currentTime, nBytesInspected, TParseEvent::DATA_IN_GAP );
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
			std::unique_ptr<CBitSequence> pRevolutionBits[Revolution::MAX];
			for( BYTE i=0; i<nFullRevolutions; i++ )
				pRevolutionBits[i].reset(
					new CBitSequence( *this, GetIndexTime(i), CreateResetProfile(), GetIndexTime(i+1) )
				);
			// . forward comparison of Revolutions, from the first to the last; bits not included in the last diff script are stable across all previous Revolutions
			struct:Utils::CCallocPtr<CDiffBase::TScriptItem>{
				int nItems;
			} shortesEditScripts[Revolution::MAX];
			for( BYTE i=0; i<nFullRevolutions-1; ){
				// : comparing the two neighboring Revolutions I and J
				const CBitSequence &jRev=*pRevolutionBits[i], &iRev=*pRevolutionBits[++i];
				const auto nSesItemsMax=iRev.GetBitCount()+jRev.GetBitCount();
				auto &ses=shortesEditScripts[i];
				ses.nItems=iRev.GetShortestEditScript( jRev, ses.Realloc(nSesItemsMax), nSesItemsMax, ap.CreateSubactionProgress(StepGranularity) );
				if (ses.nItems==0){ // neighboring Revolutions bitwise identical?
					ses.reset();
					continue;
				}else if (ses.nItems<0){ // comparison failure?
					ses.reset();
					if (ap.Cancelled)
						return nSectorsFound;
					break;
				}else
					ses.Realloc(ses.nItems); // spare on space
				// : marking different Bits as Fuzzy
				iRev.ScriptToLocalDiffs( ses, ses.nItems, Utils::MakeCallocPtr<TRegion>(nSesItemsMax) );
				// : inheriting fuzzyness from previous Revolution
				iRev.InheritFlagsFrom( jRev, ses, ses.nItems );
			}
			// . backward comparison of Revolutions, from the last to the first
			for( BYTE i=nFullRevolutions; i>1; )
				if (const auto &ses=shortesEditScripts[--i]){ // neighboring Revolutions bitwise different?
					// : conversion to dual script
					for( DWORD k=ses.nItems; k>0; ses[--k].ConvertToDual() );
					// : marking different Bits as Fuzzy
					const CBitSequence &jRev=*pRevolutionBits[i], &iRev=*pRevolutionBits[i-1];
					iRev.ScriptToLocalDiffs( ses, ses.nItems, Utils::MakeCallocPtr<TRegion>(ses.nItems) );
					// : inheriting fuzzyness from next Revolution
					iRev.InheritFlagsFrom( jRev, ses, ses.nItems );
				}
			// . merging consecutive fuzzy bits into FuzzyEvents
			CActionProgress apMerge=ap.CreateSubactionProgress( StepGranularity, StepGranularity );
			auto peIt=rOutParseEvents.GetIterator();
			for( BYTE r=0; r<nFullRevolutions; apMerge.UpdateProgress(++r) ){
				const CBitSequence &rev=*pRevolutionBits[r];
				CActionProgress apRev=apMerge.CreateSubactionProgress( StepGranularity/nFullRevolutions, rev.GetBitCount() );
				CBitSequence::PCBit bit=rev.begin(), lastBit=rev.end();
				do{
					// : finding next Fuzzy interval
					while (bit<lastBit && !(bit->fuzzy||bit->cosmeticFuzzy)) // skipping Bits that are not Fuzzy
						if (ap.Cancelled)
							return nSectorsFound;
						else
							bit++;
					if (bit==lastBit) // no more Fuzzy bits?
						break;
					TParseEvent peFuzzy( TParseEvent::NONE, bit->time, 0, 0 );
					while (bit<lastBit && (bit->fuzzy||bit->cosmeticFuzzy)) // discovering consecutive Fuzzy Bits
						if (ap.Cancelled)
							return nSectorsFound;
						else
							bit++;
					peFuzzy.tEnd=bit->time;
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
				while (++itMdi!=itMdiEnd && itMdi->isFuzzy)
					ti.tEnd=itMdi->tEnd;
				rOutParseEvents.Add(
					TParseEvent(
						rOutParseEvents.GetTypeOfFuzziness( peIt, ti, tTrackEnd ),
						ti.tStart, ti.tEnd, 0
					)
				);
				if (itMdi==itMdiEnd)
					break;
			}
		// - successfully analyzed
		return nSectorsFound;
	}

	WORD CImage::CTrackReader::ScanAndAnalyze(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,PLogTime pOutDataEnds,CParseEventList &rOutParseEvents,CActionProgress &ap,bool fullAnalysis){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		CImage::CTrackReader::TProfile idProfiles[Revolution::MAX*(TSector)-1]; TFdcStatus statuses[Revolution::MAX*(TSector)-1];
		return ScanAndAnalyze( pOutFoundSectors, pOutIdEnds, idProfiles, statuses, pOutDataEnds, rOutParseEvents, ap, fullAnalysis );
	}
	
	CImage::CTrackReader::CParseEventList CImage::CTrackReader::ScanAndAnalyze(CActionProgress &ap,bool fullAnalysis){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		CImage::CTrackReader::CParseEventList peTrack;
		TSectorId ids[Revolution::MAX*(TSector)-1]; TLogTime idEnds[Revolution::MAX*(TSector)-1]; TLogTime dataEnds[Revolution::MAX*(TSector)-1];
		ScanAndAnalyze( ids, idEnds, dataEnds, peTrack, ap, fullAnalysis );
		return peTrack;
	}

	TFdcStatus CImage::CTrackReader::ReadData(const TSectorId &id,TLogTime idEndTime,const TProfile &idEndProfile,WORD nBytesToRead,CParseEventList *pOutParseEvents){
		// attempts to read specified amount of Bytes into the Buffer, starting at position pointed to by the BitReader
		SetCurrentTimeAndProfile( idEndTime, idEndProfile );
		const Utils::CVarTempReset<bool> rdoi0( pLogTimesInfo->resetDecoderOnIndex, false ); // never reset when reading data
		switch (pLogTimesInfo->codec){
			case Codec::FM:
				return	ReadDataFm( id, nBytesToRead, pOutParseEvents );
			case Codec::MFM:
				return	ReadDataMfm( id, nBytesToRead, pOutParseEvents );
			default:
				ASSERT(FALSE); // we shouldn't end up here - all Codecs should be included in the Switch statement!
				return TFdcStatus::NoDataField;
		}
	}

	TFdcStatus CImage::CTrackReader::ReadData(const TSectorId &id,TLogTime idEndTime,const TProfile &idEndProfile,WORD nBytesToRead,LPBYTE buffer){
		// attempts to read specified amount of Bytes into the Buffer, starting at position pointed to by the BitReader
		CParseEventList peSector;
		const TFdcStatus st=ReadData( id, idEndTime, idEndProfile, nBytesToRead, &peSector );
		for each( const auto &pair in peSector ){
			const TParseEventPtr pe=pair.second;
			if (pe->IsDataStd()){
				DWORD nBytes=pe.data->dw;
				for( auto *pbi=pe.data->byteInfos; nBytes--; *buffer++=pbi++->value );
				break;
			}
		}
		return st;
	}

//#ifdef _DEBUG
	void CImage::CTrackReader::SaveCsv(LPCTSTR filename) const{
		CFile f( filename, CFile::modeWrite|CFile::modeCreate );
		for( DWORD i=0; i<nLogTimes; i++ )
			Utils::WriteToFileFormatted( f, _T("%d\n"), logTimes[i] );		
	}

	void CImage::CTrackReader::SaveDeltaCsv(LPCTSTR filename) const{
		CFile f( filename, CFile::modeWrite|CFile::modeCreate );
		TLogTime tPrev=0;
		for( DWORD i=0; i<nLogTimes; tPrev=logTimes[i++] )
			Utils::WriteToFileFormatted( f, _T("%d\n"), logTimes[i]-tPrev );		
	}
//#endif











	CImage::CTrackReader::CBitSequence::CBitSequence(CTrackReader tr,TLogTime tFrom,const CTrackReader::TProfile &profileFrom, TLogTime tTo,BYTE oneOkPercent)
		// ctor
		// - initialization
		: nBits(0) {
		// - count all Bits ("tr.GetTotalTime()/profileFrom.iwTimeMin" not used to account for decoder phase adjustment, allowing for returning back in time)
		const TLogTime iwTimeDefaultHalf=profileFrom.iwTimeDefault/2;
		tr.SetCurrentTimeAndProfile( tFrom, profileFrom );
		tTo-=iwTimeDefaultHalf;
		while (tr && tr.GetCurrentTime()<tTo)
			tr.ReadBit(), nBits++;
		// - create and populate the BitBuffer
		bitBuffer.Realloc( 1+nBits+2 )->time=-1; // "1+" = one hidden Bit before Sequence (with negative Time), "+2" = auxiliary terminal Bits
		pBits=bitBuffer+1; // skip that one hidden Bit
		tr.SetCurrentTimeAndProfile( tFrom, profileFrom );
		TBit *p=pBits;
		for( TLogTime tOne; tr && tr.GetCurrentTime()<tTo; ){
			p->flags=0;
			p->value=tr.ReadBit(tOne);
			p->time=tr.GetCurrentTime();
			if (oneOkPercent && p->value){ // only windows containing "1" are evaluated as for timing
				const TLogTime iwTimeHalf=tr.GetCurrentProfile().iwTime/2;
				const TLogTime absDiff=std::abs(tOne-tr.GetCurrentTime());
				//ASSERT( absDiff <= iwTimeHalf+1 ); // "+1" = when IwTime is odd, e.g. 1665, half of which is 833, not 832
				p->bad=absDiff*100>iwTimeHalf*oneOkPercent;
			}
			p->bad|=!tr.IsLastReadBitHealthy();
			p+= p[-1].time<p->time; // may not be the case if Decoder went over Index and got reset
		}
		p->time=p[-1].time; // auxiliary terminal Bit
		p[1].time=INT_MAX;
		nBits=p-pBits; // # of Bits may in the end be lower due to dropping of Bits over Indices
	}

	CImage::CTrackReader::CBitSequence::PCBit CImage::CTrackReader::CBitSequence::Find(TLogTime tMin) const{
		// returns the Bit at specified minimum LogicalTime
		auto it=std::lower_bound(
			begin(), end(), tMin,
			[](const TBit &b,TLogTime t){ return b.time<=t; }
		);
		if (it==end())
			return nullptr;
		it-=tMin<it->time; // correct the result of Binary Seach (usually finds the next Bit)
		return it;
	}

	int CImage::CTrackReader::CBitSequence::GetShortestEditScript(const CBitSequence &theirs,CDiffBase::TScriptItem *pOutScript,int nScriptItemsMax,CActionProgress &ap) const{
		// creates the shortest edit script (SES) and returns the number of its Items (or -1 if SES couldn't have been composed, e.g. insufficient output Buffer)
		ASSERT( pOutScript!=nullptr );
		return	CDiff<const TBit>(
					begin(), GetBitCount()
				).GetShortestEditScript(
					theirs.begin(), theirs.GetBitCount(),
					pOutScript, nScriptItemsMax,
					ap
				);
	}

	void CImage::CTrackReader::CBitSequence::ScriptToLocalDiffs(const CDiffBase::TScriptItem *pScript,int nScriptItems,TRegion *pOutDiffs) const{
		// composes Regions of differences that timely match with bits observed in this BitSequence (e.g. for visual display by the caller)
		ASSERT( nScriptItems>0 );
		while (nScriptItems-->0){
			const auto &si=pScript[nScriptItems];
			auto &rDiff=pOutDiffs[nScriptItems];
			rDiff.tStart=pBits[si.iPosA].time;
			switch (si.operation){
				case CDiffBase::TScriptItem::INSERTION:
					// "theirs" contains some extra bits that "this" misses
					rDiff.color=0xb4; // tinted red
					rDiff.tEnd=pBits[std::min( si.iPosA+1, nBits )].time; // even Insertions must be represented locally!
					pBits[si.iPosA].cosmeticFuzzy=true;
					break;
				default:
					ASSERT(FALSE); // we shouldn't end up here!
					//fallthrough
				case CDiffBase::TScriptItem::DELETION:
					// "theirs" misses some bits that "this" contains
					rDiff.color=0x5555ff; // another tinted red
					int iDeletionEnd=std::min( si.iPosA+si.del.nItemsA+1, nBits ); // "+1" = see above Insertion (only for cosmetical reasons)
					rDiff.tEnd=pBits[iDeletionEnd].time;
					pBits[--iDeletionEnd].cosmeticFuzzy=true; // see above Insertion
					while (iDeletionEnd>si.iPosA)
						pBits[--iDeletionEnd].fuzzy=true;
					break;
			}
		}
	}

	DWORD CImage::CTrackReader::CBitSequence::ScriptToLocalRegions(const CDiffBase::TScriptItem *pScript,int nScriptItems,TRegion *pOutRegions,COLORREF regionColor) const{
		// composes Regions of differences that timely match with bits observed in this BitSequence (e.g. for visual display by the caller); returns the number of unique Regions
		ScriptToLocalDiffs( pScript, nScriptItems, pOutRegions );
		TLogTime tLastRegionEnd=INT_MIN;
		DWORD nRegions=0;
		for( int i=0; i<nScriptItems; i++ ){
			const auto &diff=pOutRegions[i];
			if (diff.tStart>tLastRegionEnd){
				// disjunct Diffs - creating a new Region
				auto &rRgn=pOutRegions[nRegions++];
					rRgn.color=regionColor;
					rRgn.tStart=diff.tStart;
				tLastRegionEnd = rRgn.tEnd = diff.tEnd;
			}else
				// overlapping BadRegions (Diff: something has been Deleted, something else has been Inserted)
				tLastRegionEnd = pOutRegions[nRegions-1].tEnd = std::max(tLastRegionEnd,diff.tEnd);
		}
		return nRegions;
	}

	void CImage::CTrackReader::CBitSequence::InheritFlagsFrom(const CBitSequence &theirs,const CDiffBase::TScriptItem *pScriptItem,DWORD nScriptItems) const{
		//
		int iMyBit=0, iTheirBit=0;
		do{
			// . inheriting Flags from Bits identical up to the next ScriptItem
			const int iDiffStartPos= nScriptItems>0 ? pScriptItem->iPosA : nBits;
			int nIdentical=std::min( iDiffStartPos-iMyBit, theirs.nBits-iTheirBit );
			while (nIdentical-->0){
				if (pBits[iMyBit].value!=theirs.pBits[iTheirBit].value)
					Utils::Information(_T("MRTKI!!!!"));
				#ifdef _DEBUG
					const auto &mine=pBits[iMyBit], &their=theirs.pBits[iTheirBit];
					ASSERT( mine.value==their.value ); // just to be sure; failing here may point at a bug in Diff implementation!
				#endif
				pBits[iMyBit++].flags|=theirs.pBits[iTheirBit++].flags;
			}
			// . if no more differences, then we have just processed the common tail Bits
			if (!nScriptItems)
				break;
			// . skipping Bits that are different
			switch (pScriptItem->operation){
				case CDiffBase::TScriptItem::INSERTION:
					// "theirs" contains some extra bits that "this" misses
					iTheirBit+=pScriptItem->op.nItemsB;
					break;
				default:
					ASSERT(FALSE); // we shouldn't end up here!
					//fallthrough
				case CDiffBase::TScriptItem::DELETION:
					// "theirs" misses some bits that "this" contains
					iMyBit+=pScriptItem->op.nItemsB;
					break;
			}
			pScriptItem++, nScriptItems--;
		} while(true);
	}

	void CImage::CTrackReader::CBitSequence::OffsetAll(TLogTime dt) const{
		// offsets each Bit by given constant
		for( TBit *p=pBits,*const pLast=pBits+nBits; p<pLast; p++ ) // don't modify the padding Bits at the beginning and end
			p->time+=dt;
	}

#ifdef _DEBUG
	void CImage::CTrackReader::CBitSequence::SaveCsv(LPCTSTR filename) const{
		CFile f( filename, CFile::modeWrite|CFile::modeCreate );
		for( int b=0; b<nBits; b++ )
			Utils::WriteToFileFormatted( f, "%c, %d\n", '0'+pBits[b].value, pBits[b].time );
	}
#endif











	const COLORREF CImage::CTrackReader::TParseEvent::TypeColors[LAST]={
		0,			// None
		0xa398c2,	// sync 3 Bytes
		0x82c5e7,	// mark 1 Byte
		0xccd4f2,	// preamble
		0xc4886c,	// data ok (variable length)
		0x8057c0,	// data bad (variable length)
		0x00becc,	// data in gap (variable length)
		0x91d04d,	// CRC ok
		0x6857ff,	// CRC bad
		0xabbaba,	// non-formatted area
		0xabbaba,	// ok fuzzy area
		0xabbaba,	// bad fuzzy area
		0xa79b8a	// custom (variable string)
	};

	CImage::CTrackReader::TParseEvent::TParseEvent(TType type,TLogTime tStart,TLogTime tEnd,DWORD data)
		// ctor
		: TLogTimeInterval( tStart, tEnd )
		, type(type) , size(sizeof(TParseEvent)) {
		dw=data;
	}

	CString CImage::CTrackReader::TParseEvent::GetDescription() const{
		CString desc;
		switch (type){
			case TParseEvent::SYNC_3BYTES:
				desc.Format( _T("0x%06X sync"), dw);
				break;
			case TParseEvent::MARK_1BYTE:
				desc.Format( _T("0x%02X mark"), dw );
				break;
			case TParseEvent::PREAMBLE:
				desc.Format( _T("Preamble (%d Bytes)"), dw );
				break;
			case TParseEvent::DATA_OK:
			case TParseEvent::DATA_BAD:{
				desc.Format( _T("Data %s (%d Bytes)"), type==DATA_OK?_T("ok"):_T("bad"), dw );
				const TDataParseEvent &dpe=*(PCDataParseEvent)this;
				if (dpe.sectorId!=TSectorId::Invalid)
					desc+=_T(" for ")+dpe.sectorId.ToString();
				break;
			}
			case TParseEvent::DATA_IN_GAP:
				desc.Format( _T("Gap data (circa %d Bytes)"), dw);
				break;
			case TParseEvent::CRC_OK:
				desc.Format( _T("0x%X ok CRC"), dw);
				break;
			case TParseEvent::CRC_BAD:
				desc.Format( _T("0x%X bad CRC"), dw );
				break;
			case TParseEvent::NONFORMATTED:
				desc.Format( _T("Nonformatted %d.%d µs"), div((int)GetLength(),1000) );
				break;
			case TParseEvent::FUZZY_OK:
			case TParseEvent::FUZZY_BAD:
				desc.Format( _T("Fuzzy %d.%d µs"), div((int)GetLength(),1000) );
				break;
			default:
				return lpszMetaString;
		}
		return desc;
	}

	void CImage::CTrackReader::TMetaStringParseEvent::Create(TParseEvent &buffer,TLogTime tStart,TLogTime tEnd,LPCSTR lpszMetaString){
		ASSERT( lpszMetaString!=nullptr );
		buffer=TParseEvent( TType::META_STRING, tStart, tEnd, 0 );
		buffer.size =	sizeof(TMetaStringParseEvent)
						+
						::lstrlenA(  ::lstrcpyA( buffer.lpszMetaString, lpszMetaString )  ) // caller responsible for allocating enough buffer
						+
						1 // "+1" = including terminal Null character
						-
						sizeof(buffer.lpszMetaString); // already counted into Size
	}

	CImage::CTrackReader::TDataParseEvent::TDataParseEvent(const TSectorId &sectorId,TLogTime tStart)
		: TParseEvent( NONE, tStart, 0, 0 )
		, sectorId(sectorId) {
	}

	void CImage::CTrackReader::TDataParseEvent::Finalize(TLogTime tEnd,DWORD nBytes,TType type){
		ASSERT( nBytes>0 );
		static_cast<TParseEvent &>(*this)=TParseEvent( type, tStart, tEnd, nBytes );
		size=(PCBYTE)byteInfos-(PCBYTE)this+nBytes*sizeof(TByteInfo);
	}




	CImage::CTrackReader::CParseEventList::CParseEventList(){
		// shallow-copy ctor
		::ZeroMemory( peTypeCounts, sizeof(peTypeCounts) );
	}

	CImage::CTrackReader::CParseEventList::CParseEventList(CParseEventList &r)
		// copy-ctor implemented as move-ctor
		: Utils::CCopyList<TParseEvent>(r)
		, logStarts( std::move(r.logStarts) )
		, logEnds( std::move(r.logEnds) ) {
		::memcpy( peTypeCounts, r.peTypeCounts, sizeof(peTypeCounts) );
	}

	void CImage::CTrackReader::CParseEventList::Add(const TParseEvent &pe){
		// adds copy of the specified ParseEvent into this List
		// - creating a copy of the ParseEvent
		POSITION pos=AddTail( pe, pe.size );
		TParseEvent &copy=GetPrev(pos);
		if (pe.tStart==TimelyFromPrevious)
			copy.tStart=GetAt(pos).tEnd; // the tail assumed to be the ParseEvent added previously
		// - registering the ParseEvent for quick searching by Start/End time
		logStarts.insert( std::make_pair(pe.tStart,&copy) );
		logEnds.insert( std::make_pair(pe.tEnd,&copy) );
		// - increasing counter
		peTypeCounts[pe.type]++;
	}

	void CImage::CTrackReader::CParseEventList::Add(const CParseEventList &list){
		// adds all ParseEvents to this -List
		for each( const auto &pair in list.logStarts )
			Add( *pair.second );
	}

	CImage::CTrackReader::CParseEventList::CIterator::CIterator(const CLogTiming &logTimes,const CLogTiming::const_iterator &it)
		// ctor
		: CLogTiming::const_iterator(it)
		, logTimes(logTimes) {
	}

	typedef CImage::CTrackReader::CParseEventList::CIterator CParseEventListIterator;

	CParseEventListIterator CImage::CTrackReader::CParseEventList::GetIterator() const{
		return CParseEventListIterator( logStarts, logStarts.cbegin() );
	}

	CParseEventListIterator CImage::CTrackReader::CParseEventList::GetLastByStart() const{
		CParseEventListIterator it( logStarts, logStarts.cend() );
		if (GetCount()>0)
			it--;
		return it;
	}

	CParseEventListIterator CImage::CTrackReader::CParseEventList::TBinarySearch::Find(TLogTime tMin,TParseEvent::TType typeFrom,TParseEvent::TType typeTo) const{
		for( auto it=lower_bound(tMin); it!=cend(); it++ ){
			const TParseEvent &pe=*it->second;
			if (pe.IsType(typeFrom,typeTo))
				return CParseEventListIterator( *this, it );
		}
		return CParseEventListIterator( *this, cend() );
	}

	CParseEventListIterator CImage::CTrackReader::CParseEventList::FindByStart(TLogTime tStartMin,TParseEvent::TType typeFrom,TParseEvent::TType typeTo) const{
		return	logStarts.Find( tStartMin, typeFrom, typeTo );
	}

	CParseEventListIterator CImage::CTrackReader::CParseEventList::FindByStart(TLogTime tStartMin,TParseEvent::TType type) const{
		return	FindByStart( tStartMin, type, type );
	}

	CParseEventListIterator CImage::CTrackReader::CParseEventList::FindByEnd(TLogTime tEndMin,TParseEvent::TType typeFrom,TParseEvent::TType typeTo) const{
		return	logEnds.Find( tEndMin, typeFrom, typeTo );
	}

	CParseEventListIterator CImage::CTrackReader::CParseEventList::FindByEnd(TLogTime tEndMin,TParseEvent::TType type) const{
		return	FindByEnd( tEndMin, type, type );
	}

	bool CImage::CTrackReader::CParseEventList::IntersectsWith(const TLogTimeInterval &ti) const{
		static_assert( std::is_same<decltype(ti.tStart),int>::value, "type must be integral" ); // ...
		if (const auto it=FindByEnd( ti.tStart+1 )) // ... otherwise use 'upper_bound' here
			return it->second->Intersect(ti);
		return false;
	}

	void CImage::CTrackReader::CParseEventList::RemoveConsecutiveBeforeEnd(TLogTime tEndMax){
		// removes all ParseEvents that touch or overlap just before the End time
		auto it=FindByEnd(tEndMax);
		while (it && it->second->tEnd>tEndMax) // 'while' for Event having the same EndTime
			it--;
		if (!it)
			return;
		TLogTime tStart = tEndMax = it->second->tEnd;
		while (it){
			const auto &pe=*(it--)->second;
			if (tStart<=pe.tEnd){ // touching or overlapping?
				tStart=pe.tStart;
				peTypeCounts[pe.type]--; // adjust the -Counter
			}else
				break;
		}
		logStarts.erase(
			FindByStart(tStart),
			FindByStart(tEndMax)
		);
		logEnds.erase(
			logEnds.upper_bound(tStart),
			logEnds.upper_bound(tEndMax)
		);
	}

	CImage::CTrackReader::TParseEvent::TType CImage::CTrackReader::CParseEventList::GetTypeOfFuzziness(CIterator &itContinue,const TLogTimeInterval &tiFuzzy,TLogTime tTrackEnd) const{
		// observing the existing ParseEvents, determines and returns the type of fuzziness in the specified Interval
		while (itContinue){
			const TParseEvent &pe=*itContinue->second;
			if (tiFuzzy.tEnd<=pe.tStart)
				break;
			if (pe.IsDataStd() || pe.IsCrc())
				if (pe.Intersect(tiFuzzy))
					if ((pe.type==TParseEvent::DATA_BAD||pe.type==TParseEvent::CRC_BAD) // the fuzziness is in Bad Sector data ...
						&&
						pe.tEnd<tTrackEnd // ... and the data is complete (aka, it's NOT data over Index)
					)
						return TParseEvent::FUZZY_BAD;
			itContinue++;
		}
		return TParseEvent::FUZZY_OK; // the fuzzy Interval occurs NOT in a Bad Sector
	}













	WORD CImage::CTrackReader::ScanFm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,CParseEventList *pOutParseEvents){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		ASSERT( pOutFoundSectors!=nullptr && pOutIdEnds!=nullptr );
		RewindToIndex(0);
		//TODO
		return 0;
	}

	TFdcStatus CImage::CTrackReader::ReadDataFm(const TSectorId &sectorId,WORD nBytesToRead,CParseEventList *pOutParseEvents){
		// attempts to read specified amount of Bytes into the Buffer, starting at current position; returns the amount of Bytes actually read
		ASSERT( pLogTimesInfo->codec==Codec::FM );
		//TODO
		return TFdcStatus::SectorNotFound;
	}

	WORD CImage::CTrackReaderWriter::WriteDataFm(WORD nBytesToWrite,PCBYTE buffer,TFdcStatus sr){
		// attempts to write specified amount of Bytes in the Buffer, starting at current position; returns the amount of Bytes actually written
		ASSERT( pLogTimesInfo->codec==Codec::FM );
		//TODO
		return 0;
	}










	namespace MFM{
		static constexpr CFloppyImage::TCrc16 CRC_A1A1A1=0xb4cd; // CRC of 0xa1, 0xa1, 0xa1

		static bool *EncodeByte(BYTE byte,bool *bitBuffer){
			bool prevDataBit=bitBuffer[-1];
			for( BYTE mask=0x80; mask!=0; mask>>=1 )
				if (byte&mask){ // current bit is a "1"
					*bitBuffer++=false; // clock is a "0"
					*bitBuffer++ = prevDataBit = true; // data is a "1"
				}else{ // current bit is a "0"
					*bitBuffer++=!prevDataBit; // insert "1" clock if previous data bit was a "0"
					*bitBuffer++ = prevDataBit = false; // data is a "0"
				}
			return bitBuffer;
		}
		static bool *EncodeWord(WORD w,bool *bitBuffer){
			return	EncodeByte(
						LOBYTE(w),
						EncodeByte( HIBYTE(w), bitBuffer ) // high Byte comes first
					);
		}

		static BYTE DecodeByte(WORD w){
			BYTE result=0;
			for( BYTE n=8; n-->0; w<<=1,w<<=1 )
				result=(result<<1)|((w&0x4000)!=0);
			return result;
		}
		static WORD DecodeWord(DWORD dw){
			WORD result=0;
			for( BYTE n=16; n-->0; dw<<=1,dw<<=1 )
				result=(result<<1)|((dw&0x40000000)!=0);
			return result;
		}

		static bool IsWellEncodedSequence(WORD bits){
			// see comment at 'CTrackReader::lastReadBits'
			if ((bits&3)==3) // last valid bit is "1"?
				return (bits&4)==0; // previous bit must be "0" (and we don't care if it's valid or not)
			else
				return (BYTE)bits!=0xaa; // four valid consecutive "0"s are forbidden
		}
	}

	WORD CImage::CTrackReader::ScanMfm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,CParseEventList *pOutParseEvents){
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
				pOutParseEvents->Add( TParseEvent( TParseEvent::SYNC_3BYTES, tSyncStarts[(iSyncStart+256-48)&63], currentTime, 0xa1a1a1 ) );
			sync1=0; // invalidating "buffered" synchronization, so that it isn't reused
			// . an ID Field mark should follow the synchronization
			if (!ReadBits16(w)) // Track end encountered
				break;
			const BYTE idam=MFM::DecodeByte(w);
			if ((idam&0xfe)!=0xfe) // not the expected ID Field mark; the least significant bit is always ignored by the FDC [http://info-coach.fr/atari/documents/_mydoc/Atari-Copy-Protection.pdf]
				continue;			
			struct{
				BYTE idFieldAm, cyl, side, sector, length;
			} data={ idam };
			if (pOutParseEvents)
				pOutParseEvents->Add( TParseEvent( TParseEvent::MARK_1BYTE, TimelyFromPrevious, currentTime, idam ) );
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
				TMetaStringParseEvent::Create( peId.base, TimelyFromPrevious, currentTime, rid.ToString() );
				pOutParseEvents->Add( peId.base );
			}
			// . reading and comparing ID Field's CRC
			DWORD dw;
			CFloppyImage::TCrc16 crc=0;
			const bool crcBad=!ReadBits32(dw) || Utils::CBigEndianWord(MFM::DecodeWord(dw)).GetBigEndian()!=( crc=CFloppyImage::GetCrc16Ccitt(MFM::CRC_A1A1A1,&data,sizeof(data)) ); // no or wrong IdField CRC
			*pOutIdStatuses++ =	crcBad 
								? TFdcStatus::IdFieldCrcError
								: TFdcStatus::WithoutError;
			*pOutIdEnds++=currentTime;
			*pOutIdProfiles++=profile;
			if (pOutParseEvents)
				pOutParseEvents->Add( TParseEvent( crcBad?TParseEvent::CRC_BAD:TParseEvent::CRC_OK, TimelyFromPrevious, currentTime, crc ) );
			// . sector found
			nSectors++;
		}
		// - returning the result
		return nSectors;
	}

	TFdcStatus CImage::CTrackReader::ReadDataMfm(const TSectorId &sectorId,WORD nBytesToRead,CParseEventList *pOutParseEvents){
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
			pOutParseEvents->Add( TParseEvent( TParseEvent::SYNC_3BYTES, tSyncStarts[(iSyncStart+256-48)&63], currentTime, 0xa1a1a1 ) );
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
			pOutParseEvents->Add( TParseEvent( TParseEvent::MARK_1BYTE, TimelyFromPrevious, currentTime, dam ) );
		// - reading specified amount of Bytes into the Buffer
		TDataParseEvent peData( sectorId, TimelyFromPrevious );
		TDataParseEvent::TByteInfo *p=peData.byteInfos;
		CFloppyImage::TCrc16 crc=CFloppyImage::GetCrc16Ccitt( MFM::CRC_A1A1A1, &dam, sizeof(dam) ); // computing actual CRC along the way
		for( ; nBytesToRead>0; nBytesToRead-- ){
			p->tStart=currentTime;
			if (!*this || !ReadBits16(w)){ // Track end encountered
				result.ExtendWith( TFdcStatus::DataFieldCrcError );
				break;
			}
			p->value=MFM::DecodeByte(w);
			crc=CFloppyImage::GetCrc16Ccitt( crc, &p++->value, 1 );
		}
		const auto nDataBytes=p-peData.byteInfos;
		peData.Finalize( currentTime, nDataBytes );
		if (!*this){
			if (pOutParseEvents && nDataBytes)
				pOutParseEvents->Add( peData );
			return result;
		}
		// - comparing Data Field's CRC
		DWORD dw;
		const bool crcBad=!ReadBits32(dw) || Utils::CBigEndianWord(MFM::DecodeWord(dw)).GetBigEndian()!=crc; // no or wrong Data Field CRC
		if (crcBad){
			result.ExtendWith( TFdcStatus::DataFieldCrcError );
			if (pOutParseEvents){
				pOutParseEvents->Add( peData );
				pOutParseEvents->Add( TParseEvent( TParseEvent::CRC_BAD, TimelyFromPrevious, currentTime, crc ) );
			}
		}else
			if (pOutParseEvents){
				peData.type=TParseEvent::DATA_OK;
				pOutParseEvents->Add( peData );
				pOutParseEvents->Add( TParseEvent( TParseEvent::CRC_OK, TimelyFromPrevious, currentTime, crc ) );
			}
		return result;
	}

	WORD CImage::CTrackReaderWriter::WriteDataMfm(WORD nBytesToWrite,PCBYTE buffer,TFdcStatus sr){
		// attempts to write specified amount of Bytes in the Buffer, starting at current position; returns the amount of Bytes actually written
		ASSERT( pLogTimesInfo->codec==Codec::MFM );
		// - searching for the nearest three consecutive 0xA1 distorted synchronization Bytes
		WORD w, sync1=0; DWORD sync23=0;
		while (*this){
			sync23=	(sync23<<1) | ((sync1&0x8000)!=0);
			sync1 =	(sync1<<1) | (BYTE)ReadBit();
			if ((sync1&0xffdf)==0x4489 && (sync23&0xffdfffdf)==0x44894489)
				break;
		}
		w=ReadBit(); // leaving synchronization mark behind
		if (!*this) // Track end encountered
			return 0;
		const TLogTime tDataFieldMarkStart=logTimes[iNextTime-1]; // aligning to the synchronization mark terminal "1"
		const TProfile dataFieldMarkProfile=profile;
		// - a Data Field mark should follow the synchronization
		if (!ReadBits15(w)) // Track end encountered; 15 plus 1 bit from above
			return 0;
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
				return 0; // not the expected Data mark
		}
		// - the NumberOfBytesToWrite should be the same as already written!
		for( WORD i=nBytesToWrite; i>0; i-- )
			if (!ReadBits16(w)) // Track end encountered
				return 0;
		// - data should be followed by a 16-bit CRC
		DWORD dw;
		if (!ReadBits32(dw)) // Track end encountered
			return 0;
		// - rewinding back to the end of distorted 0xA1A1A1 synchronization mark
		SetCurrentTimeAndProfile( tDataFieldMarkStart, dataFieldMarkProfile );
		bool bits[(WORD)-1],*pBit=bits;
		*pBit++=true; // the previous data bit in a distorted 0xA1 sync mark was a "1"
		// - encoding the Data Field mark
		pBit=MFM::EncodeByte( dam, pBit );
		CFloppyImage::TCrc16 crc=CFloppyImage::GetCrc16Ccitt( MFM::CRC_A1A1A1, &dam, sizeof(dam) ); // computing the CRC along the way
		// - encoding all Buffer data
		crc=CFloppyImage::GetCrc16Ccitt( crc, buffer, nBytesToWrite ); // computing the CRC along the way
		for( WORD n=nBytesToWrite; n>0; n-- )
			pBit=MFM::EncodeByte( *buffer++, pBit );
		// - encoding computed 16-bit CRC
		if (sr.DescribesDataFieldCrcError())
			crc=~crc;
		pBit=MFM::EncodeWord( Utils::CBigEndianWord(crc).GetBigEndian(), pBit ); // CRC already big-endian, converting it to little-endian
		// - writing one extra "0" bit if the CRC ends with "1" (leaving this case uncovered often leads to magnetic problems)
		if (pBit[-1]) // CRC ends with "1" ...
			*pBit++=false; // ... so the gap must begin with "0"
		// - writing the Bits
		return	WriteBits( bits+1, pBit-bits-1 ) // "1" = the auxiliary "previous" bit of distorted 0xA1 sync mark
				? nBytesToWrite
				: 0;
	}

	char CImage::CTrackReader::ReadByte(WORD &rOutBits,PBYTE pOutValue){
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

	bool CImage::CTrackReader::IsLastReadBitHealthy() const{
		// True <=> the bit read last by ReadBit* methods is well encoded, otherwise False (first bits on a Track or right after Index may be evaluated unreliably!)
		switch (pLogTimesInfo->codec){
			case Codec::FM:
				return true;
			case Codec::MFM:
				return MFM::IsWellEncodedSequence( lastReadBits );
			default:
				ASSERT(FALSE); // we shouldn't end up here - check if all Codecs are included in the Switch statement!
				return false;
		}
	}













	CImage::CTrackReaderBase::TProfile::TProfile(TDecoderMethod method)
		// ctor
		: Medium::TIwProfile(0)
		, method(method) {
		Reset();
	}

	CImage::CTrackReaderBase::TProfile::TProfile(const TMetaDataItem &mdi)
		// ctor
		: Medium::TIwProfile( mdi.GetBitTimeAvg() )
		, method(TDecoderMethod::METADATA) {
		methodState.metaData.iCurrBit=-1; // begin "before" the MetaDataItem
	}

	void CImage::CTrackReaderBase::TProfile::Reset(){
		iwTime=iwTimeDefault;
		switch (method){
			default:
				::ZeroMemory( &methodState, sizeof(methodState) );
				//fallthrough
			case TDecoderMethod::METADATA:
				break;
		}
	}












	const CImage::CTrackReaderWriter CImage::CTrackReaderWriter::Invalid( 0, CTrackReader::TDecoderMethod::NONE, false ); // TrackReader invalid right from its creation

	CImage::CTrackReaderWriter::CTrackReaderWriter(DWORD nLogTimesMax,TDecoderMethod method,bool resetDecoderOnIndex)
		// ctor
		: CTrackReader(
			(PLogTime)::calloc( nLogTimesMax+1, sizeof(TLogTime) ),
			new CLogTimesInfo( nLogTimesMax, method, resetDecoderOnIndex ),
			Codec::MFM,
			method
		){
		SetMediumType(Medium::FLOPPY_DD); // setting values associated with the specified MediumType
	}

	CImage::CTrackReaderWriter::CTrackReaderWriter(const CTrackReaderWriter &trw,bool shareTimes)
		// copy ctor
		: CTrackReader( trw ) {
		if (!shareTimes){
			CTrackReaderWriter tmp( trw.GetBufferCapacity(), trw.profile.method, trw.pLogTimesInfo->resetDecoderOnIndex );
			std::swap<CTrackReaderBase>( tmp, *this );
			::memcpy( logTimes, trw.logTimes, nLogTimes*sizeof(TLogTime) );
			*static_cast<TLogTimesInfoData *>(pLogTimesInfo)=*trw.pLogTimesInfo;
		}
	}
	
	CImage::CTrackReaderWriter::CTrackReaderWriter(CTrackReaderWriter &&rTrackReaderWriter)
		// move ctor
		: CTrackReader( std::move(rTrackReaderWriter) ) {
	}
	
	CImage::CTrackReaderWriter::CTrackReaderWriter(const CTrackReader &tr)
		// copy ctor
		: CTrackReader(tr) {
	}
	
	void CImage::CTrackReaderWriter::AddTime(TLogTime logTime){
		// appends LogicalTime at the end of the Track
		ASSERT( nLogTimes<GetBufferCapacity() );
		ASSERT( logTime>=0 );
		logTimes[nLogTimes++]=logTime;
		pLogTimesInfo->rawDeviceData.reset(); // modified Track is no longer as we received it from the Device
	}

	void CImage::CTrackReaderWriter::AddTimes(PCLogTime logTimes,DWORD nLogTimes){
		// appends given amount of LogicalTimes at the end of the Track
		ASSERT( this->nLogTimes+nLogTimes<=GetBufferCapacity() );
		if (this->logTimes+this->nLogTimes==logTimes)
			// caller wrote directly into the buffer (e.g. creation of initial content); faster than calling N-times AddTime
			this->nLogTimes+=nLogTimes;
		else{
			// caller used its own buffer to store new LogicalTimes
			::memcpy( this->logTimes+this->nLogTimes, logTimes, nLogTimes*sizeof(TLogTime) );
			this->nLogTimes+=nLogTimes;
		}
		pLogTimesInfo->rawDeviceData.reset(); // modified Track is no longer as we received it from the Device
	}

	void CImage::CTrackReaderWriter::AddIndexTime(TLogTime logTime){
		// appends LogicalTime representing the position of the index pulse on the disk
		ASSERT( nIndexPulses<=Revolution::MAX );
		ASSERT( logTime>=0 );
		indexPulses[nIndexPulses++]=logTime;
		indexPulses[nIndexPulses]=INT_MAX;
		pLogTimesInfo->rawDeviceData.reset(); // modified Track is no longer as we received it from the Device
	}

	void CImage::CTrackReaderWriter::TrimToTimesCount(DWORD nKeptLogTimes){
		// discards some tail LogicalTimes, keeping only specified amount of them
		ASSERT( nKeptLogTimes<=nLogTimes ); // can only shrink
		nLogTimes=nKeptLogTimes;
		pLogTimesInfo->rawDeviceData.reset(); // modified Track is no longer as we received it from the Device
	}

	void CImage::CTrackReaderWriter::AddMetaData(const TMetaDataItem &mdi){
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

	void CImage::CTrackReaderWriter::SetRawDeviceData(TId dataId,Utils::CCallocPtr<BYTE,DWORD> &&data){
		// remembers data as they were received from a disk (later used for fast copying between compatible disks)
		pLogTimesInfo->rawDeviceData.reset( new Utils::CCallocPtr<BYTE,DWORD>(std::move(data)) );
		pLogTimesInfo->rawDeviceData.id=dataId;
	}

	void CImage::CTrackReaderWriter::ClearMetaData(TLogTime a,TLogTime z){
		// removes (or just shortens) all MetaDataItems in specified range
		AddMetaData( TLogTimeInterval(a,z) );
		FindMetaDataIteratorAndApply();
	}

	void CImage::CTrackReaderWriter::ClearAllMetaData(){
		// removes all MetaDataItems
		auto &metaData=pLogTimesInfo->metaData;
		metaData.clear();
		itCurrMetaData=metaData.cend();
	}

	TLogTime CImage::CTrackReader::GetLastIndexTime() const{
		// returns the LogicalTime of the last added Index (or 0)
		return	nIndexPulses ? indexPulses[nIndexPulses-1] : 0;
	}

	bool CImage::CTrackReaderWriter::WriteBits(const bool *bits,DWORD nBits){
		// True <=> specified amount of Bits in the buffer has successfully overwritten "nBits" immediatelly following the CurrentTime, otherwise False
		// - determining the number of current "ones" in the immediatelly next "nBits" cells
		const TLogTime tOverwritingStart=currentTime;
		const TProfile overwritingStartProfile=profile;
		DWORD nOnesPreviously=0;
		for( DWORD n=nBits; n-->0; nOnesPreviously+=ReadBit() );
		const TLogTime tOverwritingEnd=currentTime;
		// - determining the number of new "ones" in the current Bits
		DWORD nOnesCurrently=0;
		for( DWORD n=nBits; n-->0; nOnesCurrently+=bits[n] );
		// - overwriting the "nBits" cells with new Bits
		SetCurrentTimeAndProfile( tOverwritingStart, overwritingStartProfile );
		const DWORD nNewLogTimes=nLogTimes+nOnesCurrently-nOnesPreviously;
		if (nNewLogTimes>GetBufferCapacity())
			return false;
		const TLogTime tOverwritingLength=tOverwritingEnd-tOverwritingStart;
		if (GetMetaData()) // does this Track use MetaData?
			AddMetaData(
				TMetaDataItem( TLogTimeInterval(tOverwritingStart,tOverwritingEnd), false, nBits )
			);
		const PLogTime pOverwritingStart=logTimes+iNextTime;
		::memmove(
			pOverwritingStart+nOnesCurrently,
			pOverwritingStart+nOnesPreviously,
			(nLogTimes-iNextTime-nOnesPreviously)*sizeof(TLogTime)
		);
		nLogTimes=nNewLogTimes;
		const auto newLogTimesTemp=Utils::MakeCallocPtr<TLogTime>(nOnesCurrently);
			PLogTime pt=newLogTimesTemp;
			for( DWORD i=0; i++<nBits; )
				if (*bits++)
					*pt++=tOverwritingStart+(LONGLONG)tOverwritingLength*i/nBits;
			::memcpy( pOverwritingStart, newLogTimesTemp, nOnesCurrently*sizeof(TLogTime) );
		pLogTimesInfo->rawDeviceData.reset(); // modified Track is no longer as we received it from the Device
		return true;
	}

	WORD CImage::CTrackReaderWriter::WriteData(TLogTime idEndTime,const TProfile &idEndProfile,WORD nBytesToWrite,PCBYTE buffer,TFdcStatus sr){
		// attempts to write specified amount of Bytes in the Buffer, starting after specified IdEndTime; returns the number of Bytes actually written
		SetCurrentTimeAndProfile( idEndTime, idEndProfile );
		const Utils::CVarTempReset<bool> rdoi0( pLogTimesInfo->resetDecoderOnIndex, false ); // never reset when reading data
		switch (pLogTimesInfo->codec){
			case Codec::FM:
				return	WriteDataFm( nBytesToWrite, buffer, sr );
			case Codec::MFM:
				return	WriteDataMfm( nBytesToWrite, buffer, sr );
			default:
				ASSERT(FALSE); // we shouldn't end up here - all Codecs should be included in the Switch statement!
				return 0;
		}
	}

	static DWORD InterpolateTimes(PLogTime logTimes,DWORD nLogTimes,TLogTime tSrcA,DWORD iSrcA,TLogTime tSrcZ,TLogTime tDstA,TLogTime tDstZ){
		// in-place interpolation of LogicalTimes in specified range; returns an "index-pointer" to the first unprocessed LogicalTime (outside the range)
		TLogTime &rtStop=logTimes[nLogTimes],const tStopOrg=rtStop;
		rtStop=INT_MAX; // stop-condition
			PLogTime pTime=logTimes+iSrcA;
			for( const TLogTime tSrcInterval=tSrcZ-tSrcA,tDstInterval=tDstZ-tDstA; *pTime<tSrcZ; pTime++ )
				*pTime = tDstA+(LONGLONG)(*pTime-tSrcA)*tDstInterval/tSrcInterval;
		rtStop=tStopOrg;
		return pTime-logTimes;
	}

	bool CImage::CTrackReaderWriter::Normalize(){
		// True <=> asked and successfully normalized for a known MediumType, otherwise False
		// - if the Track contains less than two Indices, we are successfully done
		if (nIndexPulses<2)
			return true;
		// - determining the RevolutionTime to the next Index
		TLogTime revolutionTime;
		if (const Medium::PCProperties mp=GetMediumProperties())
			revolutionTime=mp->revolutionTime;
		else
			return false;
		ClearAllMetaData();
		// - adjusting consecutive index-to-index distances
		RewindToIndex(0);
		for( BYTE i=0; i+1<nIndexPulses; i++ )
			iNextTime=InterpolateTimes(
				logTimes, nLogTimes,
				GetIndexTime(i), iNextTime, GetIndexTime(i+1),
				GetIndexTime(0)+i*revolutionTime, GetIndexTime(0)+(i+1)*revolutionTime
			);
		const TLogTime dt=GetIndexTime(0)+(nIndexPulses-1)*revolutionTime-GetIndexTime(nIndexPulses-1);
		while (iNextTime<nLogTimes)
			logTimes[iNextTime++]+=dt; // offsetting the remainder of the Track
		for( BYTE i=1; i<nIndexPulses; i++ )
			indexPulses[i]=indexPulses[i-1]+revolutionTime;
		// - correctly re-initializing this object
		SetCurrentTime(0);
		pLogTimesInfo->rawDeviceData.reset(); // modified Track is no longer as we received it from the Device
		return true;
	}

	TStdWinError CImage::CTrackReaderWriter::NormalizeEx(TLogTime indicesOffset,bool fitTimesIntoIwMiddles,bool correctCellCountPerRevolution,bool correctRevolutionTime){
		// True <=> all Revolutions of this Track successfully normalized using specified parameters, otherwise False
		// - if the Track contains less than two Indices, we are successfully done
		if (nIndexPulses<2)
			return ERROR_SUCCESS;
		// - MediumType must be supported
		const Medium::PCProperties mp=GetMediumProperties();
		if (!mp)
			return ERROR_UNRECOGNIZED_MEDIA;
		ClearAllMetaData();
		// - shifting Indices by shifting all Times in oposite direction
		if (indicesOffset){
			for( DWORD i=0; i<nLogTimes; logTimes[i++]-=indicesOffset ); // shift all Times in oposite direction
			for( DWORD i=0; i<nLogTimes; i++ ) // discard negative Times
				if (logTimes[i]>=0){
					::memcpy( logTimes, logTimes+i, (nLogTimes-=i)*sizeof(TLogTime) );
					break;
				}
		}
		// - ignoring what's before the first Index
		TLogTime tCurrIndexOrg=RewindToIndex(0);
		// - normalization
		pLogTimesInfo->rawDeviceData.reset(); // modified Track is no longer as we received it from the Device
		const TLogTime tLastIndex=GetLastIndexTime();
		const DWORD iModifStart=iNextTime;
		DWORD iTime=iModifStart;
		const DWORD nLogTimesMaxNew=std::max( nIndexPulses*mp->nCells, GetBufferCapacity() );
		const Utils::CCallocPtr<TLogTime> buffer( nLogTimesMaxNew, 0 ); // pessimistic estimation for FM encoding
		const PLogTime ptModified=buffer.get();
		for( BYTE nextIndex=1; nextIndex<nIndexPulses; nextIndex++ ){
			// . resetting inspection conditions
			profile.Reset();
			const TLogTime tNextIndexOrg=GetIndexTime(nextIndex);
			const DWORD iModifRevStart=iTime;
			// . alignment of LogicalTimes to inspection window centers
			DWORD nAlignedCells=0;
			if (fitTimesIntoIwMiddles){
				// alignment wanted
				for( ; *this&&logTimes[iNextTime]<tNextIndexOrg; nAlignedCells++ )
					if (ReadBit())
						if (iTime<nLogTimesMaxNew)
							ptModified[iTime++] = tCurrIndexOrg + nAlignedCells*profile.iwTimeDefault;
						else
							return ERROR_INSUFFICIENT_BUFFER; // mustn't overrun the Buffer
			}else
				// alignment not wanted - just copying the Times in current Revolution
				while (*this && logTimes[iNextTime]<tNextIndexOrg)
					ptModified[iTime++]=ReadTime();
			DWORD iModifRevEnd=iTime;
			// . shortening/prolonging this revolution to correct number of cells
			if (correctCellCountPerRevolution){
				ptModified[iModifRevEnd]=INT_MAX; // stop-condition
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
		const DWORD nNewLogTimes=nLogTimes+iTime-iNextTime;
		CTrackReaderWriter tmp( nLogTimesMaxNew, profile.method, pLogTimesInfo->resetDecoderOnIndex );
			::memcpy( tmp.logTimes, logTimes, iModifStart*sizeof(TLogTime) ); // Times before first Index
			::memcpy( tmp.logTimes+iModifStart, ptModified+iModifStart, (iTime-iModifStart)*sizeof(TLogTime) ); // Times in full Revolutions
			::memcpy( tmp.logTimes+iTime, logTimes+iNextTime, (nLogTimes-iNextTime)*sizeof(TLogTime) ); // Times after last Index
			if (const TLogTime dt=GetLastIndexTime()-tLastIndex)
				for( DWORD i=iTime; i<nNewLogTimes; tmp.logTimes[i++]+=dt );
		std::swap( tmp.logTimes, logTimes );
		nLogTimes=nNewLogTimes;
		SetCurrentTime(0); // setting valid state
		// - successfully normalized
		return ERROR_SUCCESS;
	}

	CImage::CTrackReaderWriter &CImage::CTrackReaderWriter::Reverse(){
		// reverses timing of this Track
		// - reversing Indices
		const auto tTotal=GetTotalTime();
		for( BYTE i=0; i<GetIndexCount()/2; i++ )
			std::swap( indexPulses[i], indexPulses[GetIndexCount()-1-i] );
		for( BYTE i=0; i<GetIndexCount(); i++ )
			indexPulses[i]=tTotal-indexPulses[i];
		// - reversing Times
		for( DWORD i=0; i<nLogTimes/2; i++ )
			std::swap( logTimes[i], logTimes[nLogTimes-1-i] );
		for( DWORD i=0; i<nLogTimes; i++ )
			logTimes[i]=tTotal-logTimes[i];
		// - reversing MetaData
		CMetaData metaData;
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
