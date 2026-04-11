#include "stdafx.h"

namespace Time
{
	const TInterval TInterval::Invalid( Infinity, INT_MIN );



	const TCHAR Prefixes[]=_T("nnnµµµmmm   "); // nano, micro, milli, no-prefix

	CTimeline::CTimeline(T logTimeLength,T logTimePerUnit,BYTE initZoomFactor)
		// ctor
		: Utils::CAxis( logTimeLength, logTimePerUnit, 's', Prefixes, initZoomFactor ) {
	}




	TMetaDataItem::TMetaDataItem(const TInterval &ti)
		// ctor (to clear MetaData in specified Interval)
		: TInterval(ti)
		, isFuzzy(false) , nBits(0) {
	}
	
	TMetaDataItem::TMetaDataItem(const TInterval &ti,bool isFuzzy,Bit::N nBits)
		// ctor
		: TInterval(ti)
		, isFuzzy(isFuzzy) , nBits(nBits) {
	}
	
	T TMetaDataItem::GetBitTimeAvg() const{
		// computes and returns the average InspectionWindow size
		ASSERT( nBits>0 );
		return GetLength()/nBits;
	}

	T TMetaDataItem::GetBitTime(Bit::N iBit) const{
		// computes and returns the LogicalTime of I-th bit (aka. the center of I-th InspectionWindow)
		ASSERT( 0<=iBit && iBit<nBits );
		return	tStart + ::MulDiv( GetLength(), iBit, nBits ); // mathematic rounding
	}

	Bit::N TMetaDataItem::GetBitIndex(T t) const{
		//
		ASSERT( Contains(t) );
		return ::MulDiv( t-tStart, nBits, GetLength() ); // mathematic rounding
	}

	TInterval TMetaDataItem::GetIw(Bit::N iBit) const{
		// determines and returns the InspectionWindow that has the I-th bit at its center
		const T tCenter=GetBitTime(iBit), tAvgSize=GetBitTimeAvg();
		const T tStart=tCenter-tAvgSize/2;
		return TInterval( tStart, tStart+tAvgSize );
	}

	TMetaDataItem TMetaDataItem::Split(T tAt){
		// trims this MetaDataItem to Interval [tStart,tAt), returning the rest; this semantics is compatible with 'operator<' which uses 'tStart' - when "un-const-ed", Split(.) can be call directly on 'std::set' items (which is nasty, btw)
		const Bit::N iBit=GetBitIndex(tAt);
		ASSERT( GetBitTime(iBit)==tAt ); // up to the caller to correctly align!
		const TMetaDataItem result( TInterval(tAt,tEnd), isFuzzy, nBits-iBit );
		nBits=iBit;
		return result;
	}

	bool TMetaDataItem::Equals(const TMetaDataItem &r) const{
		// True <=> the two MetaData have identical properties, otherwise False
		return isFuzzy==r.isFuzzy && GetBitTimeAvg()==r.GetBitTimeAvg();
	}




	CMetaData::const_iterator CMetaData::GetMetaDataIterator(T t) const{
		// returns an iterator to a MetaDataItem that contains the specified Time, or 'cend()'
		auto &&it=upper_bound( TInterval(t,Infinity) ); // 'upper_bound' = don't search the sharp beginning but rather something bigger ...
		if (it!=cend())
			if (it!=cbegin() && (--it)->Contains(t)) // ... and then iterate back, because that's the usual case when "randomly" pinning in the timeline
				return it;
		return cend();
	}

	PCMetaDataItem CMetaData::GetMetaDataItem(T t) const{
		// returns a MetaDataItem that contains the specified Time, or Null
		const auto it=GetMetaDataIterator(t);
		return it!=cend() ? &*it : nullptr;
	}

	PCMetaDataItem CMetaData::GetFirst() const{
		return size() ? &*cbegin() : nullptr;
	}

	PCMetaDataItem CMetaData::GetLast() const{
		if (!size())
			return nullptr;
		auto it=cend();
		return &*--it;
	}




	namespace Decoder
	{
		LPCTSTR GetDescription(TMethod m){
			switch (m){
				case TMethod::NONE:
					return _T("None (just archivation)");
				case TMethod::KEIR_FRASER:
					return _T("Keir Fraser's FDC-like decoder");
				case TMethod::MARK_OGDEN:
					return _T("Mark Ogden's FDC-like decoder");
				default:
					ASSERT(FALSE);
					return nullptr;
			}
		}


	
	
		TLimits::TLimits(T iwTimeDefault,BYTE iwTimeTolerancePercent)
			// ctor
			: iwTimeDefault(iwTimeDefault)
			, iwTime(iwTimeDefault)
			, iwTimeMin( iwTimeDefault*(100-iwTimeTolerancePercent)/100 )
			, iwTimeMax( iwTimeDefault*(100+iwTimeTolerancePercent)/100 ) {
		}

		void TLimits::ClampIwTime(){
			// keep the InspectionWindow size within limits
			if (iwTime<iwTimeMin)
				iwTime=iwTimeMin;
			else if (iwTime>iwTimeMax)
				iwTime=iwTimeMax;
		}




		TProfile::TProfile(TMethod method)
			// ctor
			: TLimits(0)
			, method(method) {
			Reset();
		}

		TProfile::TProfile(const TMetaDataItem &mdi)
			// ctor
			: TLimits( mdi.GetBitTimeAvg() )
			, method(TMethod::METADATA) {
			methodState.metaData.iCurrBit=-1; // begin "before" the MetaDataItem
		}

		void TProfile::Reset(){
			switch (method){
				default:
					iwTime=iwTimeDefault;
					::ZeroMemory( &methodState, sizeof(methodState) );
					//fallthrough
				case TMethod::METADATA:
					break;
			}
		}




		CBase::CBase(TMethod defaultMethod,const CSharedArray &logTimes,N nLogTimes,const CMetaData &metaData)
			// ctor
			: defaultMethod(defaultMethod) , profile(defaultMethod)
			, logTimes(logTimes) , nLogTimes(nLogTimes)
			, iNextTime(0) , currentTime(0) , lastReadBits(0)
			, pMetaData(&metaData) , itCurrMetaData(metaData.cbegin()) {
		}

		PCMetaDataItem CBase::GetCurrentTimeMetaData() const{
			// returns the MetaDataItem that contain the CurrentTime, or Null
			if (itCurrMetaData!=pMetaData->cend()){
				ASSERT( itCurrMetaData->Contains(currentTime) ); // just to be sure
				return &*itCurrMetaData;
			}else
				return nullptr;
		}

		PCMetaDataItem CBase::ApplyCurrentTimeMetaData(){
			// uses the MetaDataItem at CurrentTime for reading
			if (const PCMetaDataItem pmdi=GetCurrentTimeMetaData()){
				// application of found MetaDataItem
				return pmdi;
			}else
				return nullptr;
		}

		PCMetaDataItem CBase::IncrMetaDataIteratorAndApply(){
			// the CurrentTime has moved forward, and the good strategy is to iterate to the MetaDataItem that contains it
			while (itCurrMetaData!=pMetaData->cend())
				if (currentTime<itCurrMetaData->tEnd)
					return ApplyCurrentTimeMetaData();
				else
					itCurrMetaData++;
			return nullptr;
		}

		PCMetaDataItem CBase::DecrMetaDataIteratorAndApply(){
			// the CurrentTime has moved backward, and the good strategy is to iterate to the MetaDataItem that contains it
			while (itCurrMetaData!=pMetaData->cbegin())
				if (itCurrMetaData->tEnd<=currentTime) // this is already too much ...
					return IncrMetaDataIteratorAndApply(); // ... hence go one MetaData forward
				else
					itCurrMetaData--;
			return nullptr;
		}

		PCMetaDataItem CBase::FindMetaDataIteratorAndApply(){
			// the CurrentTime has changed randomly
			itCurrMetaData=pMetaData->upper_bound( TInterval(currentTime,Infinity) ); // 'upper_bound' = don't search the sharp beginning but rather something bigger ...
			if (itCurrMetaData!=pMetaData->cbegin())
				itCurrMetaData--; // ... and then iterate back, because that's the usual case when "randomly" pinning in the timeline
			return ApplyCurrentTimeMetaData();
		}

		void CBase::SetCurrentTime(T logTime){
			// seeks to the specified LogicalTime
			if (!nLogTimes)
				return;
			if (logTime<0)
				logTime=0;
			if (logTime<*logTimes.begin()){
				iNextTime=0;
				currentTime=logTime;
			}else{
				//TODO: logTimes.FindNextGreater( time, arrayLength=0 )
				//TODO: logTimes.FindNextGreaterIndex( time, arrayLength=0 )
				N L=0, R=nLogTimes;
				do{
					const N M=(L+R)/2;
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
				profile.method=TMethod::METADATA;
				profile.methodState.metaData.iCurrBit=pmdi->GetBitIndex(currentTime)-1;
			}else if (profile.method==TMethod::METADATA){
				profile.method=defaultMethod;
				profile.Reset();
			}
		}

		TProfile CBase::CreateResetProfile() const{
			// creates and returns current Profile that is reset
			TProfile result=profile;
			result.Reset();
			return result;
		}

		T CBase::TruncateCurrentTime(){
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

		T CBase::GetLastTime() const{
			// returns the last recorded Time
			return	nLogTimes>0 ? logTimes[nLogTimes-1] : 0;
		}

		T CBase::ReadTime(){
			// returns the next LogicalTime (or zero if all time information already read)
			if (*this){
				currentTime=logTimes[iNextTime++];
				IncrMetaDataIteratorAndApply();
				return currentTime;
			}else
				return 0;
		}

		bool CBase::ReadBit(T &rtOutOne){
			// returns first bit not yet read
			// - methods violating the established framework come here
			switch (profile.method){
				case TMethod::METADATA:{
					// a hidden Decoder to help extract bits from Times tagged with MetaData
					if (!*this){
						currentTime=profile.PeekNextIwTime(currentTime);
						return 0;
					}
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
					const bool result =	ti.Contains( rtOutOne=logTimes[iNextTime] ) // // is there a Time in the second half of the InspectionWindow?
										||
										iNextTime>0 && ti.Contains( rtOutOne=logTimes[iNextTime-1] ); // is there a Time in the first half of the InspectionWindow?
					while (*this && logTimes[iNextTime]<=ti.tEnd)
						iNextTime++;
					lastReadBits<<=1, lastReadBits|=1; // 'valid' flag
					lastReadBits<<=1, lastReadBits|=(BYTE)result;
					return result;
				}
			}
			// - skip all remaining Times in current InspectionWindow
			const T iwTimeHalf=profile.iwTime/2, tCurrIwEnd=currentTime+iwTimeHalf;
			do{
				if (!*this){
					currentTime=profile.PeekNextIwTime(currentTime);
					return 0;
				}else if (logTimes[iNextTime]<tCurrIwEnd)
					iNextTime++;
				else
					break;
			}while (true);
			// - move to the next InspectionWindow
			currentTime=profile.PeekNextIwTime(currentTime);
			IncrMetaDataIteratorAndApply();
			// - decode bit
			switch (profile.method){
				case TMethod::NONE:
					// no decoder - aka. "don't extract bits from the recording"
					return 0;
				case TMethod::KEIR_FRASER:{
					// FDC-like flux reversal decoding from Keir Fraser's GreaseWeazle
					auto &r=profile.methodState.fraser;
					// . detect zero (longer than 1/2 of an InspectionWindow size)
					const T diff=( rtOutOne=logTimes[iNextTime] )-currentTime;
					//iNextTime+=logTimes[iNextTime]<=currentTime; // eventual correction of the pointer to the next time
					lastReadBits<<=1, lastReadBits|=1; // 'valid' flag
					lastReadBits<<=1;
					if (diff>=iwTimeHalf){
						r.nConsecutiveZeros++;
						return 0;
					}
					// . adjust data frequency according to phase mismatch
					constexpr char AdjustmentPercentMax=30; // percentual "speed" in InspectionWindow adjustment
					if (r.nConsecutiveZeros<=nConsecutiveZerosMax)
						// in sync - adjust InspectionWindow by percentage of phase mismatch
						profile.iwTime+= diff * AdjustmentPercentMax/100;
					else
						// out of sync - adjust InspectionWindow towards its Default size
						profile.iwTime+= (profile.iwTimeDefault-profile.iwTime) * AdjustmentPercentMax/100;
					profile.ClampIwTime(); // keep the InspectionWindow size within limits
					// . a "1" recognized
					r.nConsecutiveZeros=0;
					lastReadBits|=1;
					return 1;
				}
				case TMethod::MARK_OGDEN:{
					// FDC-like flux reversal decoding from Mark Ogdens's DiskTools/flux2track
					auto &r=profile.methodState.ogden;
					// . detect zero (longer than 1/2 of an InspectionWindow size)
					const T diff=( rtOutOne=logTimes[iNextTime] )-currentTime;
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
							const T iwDelta=profile.iwTimeDefault/100;
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
					static constexpr char PhaseAdjustments[2][16]={ // C1/C2, C3
						//	8	9	A	B	C	D	E	F	0	1	2	3	4	5	6	7
						 { 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20 },
						 { 13, 14, 14, 15, 15, 16, 16, 16, 16, 16, 16, 17, 17, 18, 18, 19 }
					};
					if (const T dt= (PhaseAdjustments[cState][iSlot]*profile.iwTime>>4) - profile.iwTime){
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
			}
			// - a Method implementation is missing!
			ASSERT(FALSE);
			return 0;
		}

		char CBase::ReadBits8(BYTE &rOut){
			// returns the number of bits read into the output Byte
			for( char n=0; n<8; n++,rOut=(rOut<<1)|(BYTE)ReadBit() )
				if (!*this)
					return n;
			return 8;
		}

		bool CBase::ReadBits15(WORD &rOut){
			// True <=> all 15 bits successfully read, otherwise False
			return ReadBits<15>(rOut);
		}

		bool CBase::ReadBits16(WORD &rOut){
			// True <=> all 16 bits successfully read, otherwise False
			return ReadBits<16>(rOut);
		}

		bool CBase::ReadBits32(DWORD &rOut){
			// True <=> all 32 bits successfully read, otherwise False
			return ReadBits<32>(rOut);
		}

		void CBase::SaveCsv(LPCTSTR filename) const{
			CFile f( filename, CFile::modeWrite|CFile::modeCreate );
			static_assert( sizeof(N)<=sizeof(int), "" );
			for( N i=0; i<nLogTimes; i++ )
				Utils::WriteToFileFormatted( f, _T("%d\n"), logTimes[i] );		
		}

		void CBase::SaveDeltaCsv(LPCTSTR filename) const{
			CFile f( filename, CFile::modeWrite|CFile::modeCreate );
			T tPrev=0;
			static_assert( sizeof(N)<=sizeof(int), "" );
			for( N i=0; i<nLogTimes; tPrev=logTimes[i++] )
				Utils::WriteToFileFormatted( f, _T("%d\n"), logTimes[i]-tPrev );		
		}

	#ifdef _DEBUG
		void CBase::VerifyChronology() const{
			T tPrev=INT_MIN;
			for( N i=0; i<nLogTimes; i++ )
				if (logTimes[i]<0)
					Utils::Information("negative");
				else if (logTimes[i]<=tPrev)
					Utils::Information("tachyon");
				else
					tPrev=logTimes[i];
		}
	#endif

	}

	T Ignore;

}
