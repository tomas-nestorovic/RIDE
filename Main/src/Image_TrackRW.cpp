#include "stdafx.h"

	CImage::CTrackReader::CTrackReader(PLogTime _logTimes,DWORD nLogTimes,PCLogTime indexPulses,BYTE _nIndexPulses,Medium::TType mediumType,Codec::TType codec,TDecoderMethod method,bool resetDecoderOnIndex)
		// ctor
		: logTimes(_logTimes+1) , nLogTimes(nLogTimes) // "+1" = hidden item represents reference counter
		, iNextIndexPulse(0) , nIndexPulses(  std::min<BYTE>( Revolution::MAX, _nIndexPulses )  )
		, iNextTime(0) , currentTime(0)
		, method(method) , resetDecoderOnIndex(resetDecoderOnIndex) {
		::memcpy( this->indexPulses, indexPulses, nIndexPulses*sizeof(TLogTime) );
		this->indexPulses[nIndexPulses]=INT_MAX; // a virtual IndexPulse in infinity
		logTimes[-1]=1; // initializing the reference counter
		SetCodec(codec); // setting values associated with the specified Codec
		SetMediumType(mediumType); // setting values associated with the specified MediumType
	}

	CImage::CTrackReader::CTrackReader(const CTrackReader &tr)
		// copy ctor
		: logTimes(tr.logTimes) , method(tr.method) , resetDecoderOnIndex(tr.resetDecoderOnIndex) {
		::memcpy( this, &tr, sizeof(*this) );
		static_assert( sizeof(TLogTime)==sizeof(UINT), "InterlockedIncrement" );
		::InterlockedIncrement( (PUINT)logTimes-1 ); // increasing the reference counter
	}

	CImage::CTrackReader::CTrackReader(CTrackReader &&rTrackReader)
		// move ctor
		: logTimes(rTrackReader.logTimes) , method(rTrackReader.method) , resetDecoderOnIndex(rTrackReader.resetDecoderOnIndex) {
		::memcpy( this, &rTrackReader, sizeof(*this) );
		static_assert( sizeof(TLogTime)==sizeof(UINT), "InterlockedIncrement" );
		::InterlockedIncrement( (PUINT)logTimes-1 ); // increasing the reference counter
	}

	CImage::CTrackReader::~CTrackReader(){
		// dtor
		static_assert( sizeof(TLogTime)==sizeof(UINT), "InterlockedDecrement" );
		if (!::InterlockedDecrement( (PUINT)logTimes-1 )) // decreasing the reference counter
			::free(logTimes-1);
	}




	void CImage::CTrackReader::SetCurrentTime(TLogTime logTime){
		// seeks to the specified LogicalTime
		if (!nLogTimes)
			return;
		if (logTime<0)
			logTime=0;
		for( iNextIndexPulse=0; iNextIndexPulse<nIndexPulses; iNextIndexPulse++ )
			if (logTime<=indexPulses[iNextIndexPulse])
				break;
		if (logTime<*logTimes){
			iNextTime=0;
			currentTime=logTime;
			return;
		}
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

	void CImage::CTrackReader::SetCurrentTimeAndProfile(TLogTime logTime,const TProfile &profile){
		// seeks to the specified LogicalTime, setting also the specified Profile at that LogicalTime
		SetCurrentTime(logTime);
		this->profile=profile;
	}

	CImage::CTrackReader::TProfile CImage::CTrackReader::CreateResetProfile() const{
		// creates and returns current Profile that is reset
		TProfile result=profile;
		result.Reset();
		return result;
	}

	void CImage::CTrackReader::TruncateCurrentTime(){
		// truncates CurrentTime to the nearest lower LogicalTime
		if (!iNextTime)
			currentTime=0;
		if (iNextTime<nLogTimes)
			currentTime=logTimes[iNextTime-1];
		else
			currentTime=logTimes[nLogTimes-1];
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

	TLogTime CImage::CTrackReader::GetTotalTime() const{
		// returns the last recorded Time
		return	nLogTimes>0 ? logTimes[nLogTimes-1] : 0;
	}

	TLogTime CImage::CTrackReader::ReadTime(){
		// returns the next LogicalTime (or zero if all time information already read)
		return	*this ? (currentTime=logTimes[iNextTime++]) : 0;
	}

	void CImage::CTrackReader::SetCodec(Codec::TType codec){
		// changes the interpretation of recorded LogicalTimes according to the new Codec
		switch ( this->codec=codec ){
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

	void CImage::CTrackReader::SetMediumType(Medium::TType mediumType){
		// changes the interpretation of recorded LogicalTimes according to the new MediumType
		switch ( this->mediumType=mediumType ){
			default:
				ASSERT(FALSE); // we shouldn't end-up here, all Media Types applicable for general Track description should be covered
				//fallthrough
			case Medium::FLOPPY_DD:
				profile=TProfile::DD;
				break;
			case Medium::FLOPPY_DD_525:
				profile=TProfile::DD_525;
				break;
			case Medium::FLOPPY_HD_350:
			case Medium::FLOPPY_HD_525:
				profile=TProfile::HD;
				break;
		}
		profile.Reset();
	}

	bool CImage::CTrackReader::ReadBit(){
		// returns first bit not yet read
		// - if we just crossed an IndexPulse, resetting the Profile
		if (currentTime>=indexPulses[iNextIndexPulse]){
			if (resetDecoderOnIndex)
				profile.Reset();
			const TLogTime indexTime=indexPulses[ iNextIndexPulse++ ];
			currentTime=indexTime + (currentTime-indexTime+profile.iwTimeDefault-1)/profile.iwTimeDefault*profile.iwTimeDefault;
		}
		// - reading next bit
		switch (method){
			case TDecoderMethod::NONE:
				// no decoder - aka. "don't extract bits from the record"
				if (*this){
					currentTime+=profile.iwTimeDefault;
					while (*this && logTimes[iNextTime]<=currentTime)
						iNextTime++;
				}
				return 0;
			case TDecoderMethod::FDD_KEIR_FRASER:{
				// FDC-like flux reversal decoding from Keir Fraser's Disk-Utilities/libdisk
				// - reading some more from the Track
				auto &r=profile.method.fraser;
				const TLogTime iwTimeHalf=profile.iwTime/2;
				do{
					if (!*this)
						return 0;
					if (logTimes[iNextTime]-currentTime<iwTimeHalf)
						iNextTime++;
					else
						break;
				}while (true);
				// - detecting zero (longer than 3/2 of an inspection window)
				currentTime+=profile.iwTime;
				const TLogTime diff=logTimes[iNextTime]-currentTime;
				iNextTime+=logTimes[iNextTime]<=currentTime; // eventual correction of the pointer to the next time
				if (diff>=iwTimeHalf){
					r.nConsecutiveZeros++;
					return 0;
				}
				// - adjust data frequency according to phase mismatch
				if (r.nConsecutiveZeros<=nConsecutiveZerosMax)
					// in sync - adjust inspection window by percentage of phase mismatch
					profile.iwTime+= diff * profile.adjustmentPercentMax/100;
				else
					// out of sync - adjust inspection window towards its Default size
					profile.iwTime+= (profile.iwTimeDefault-profile.iwTime) * profile.adjustmentPercentMax/100;
				// - keep the inspection window size within limits
				if (profile.iwTime<profile.iwTimeMin)
					profile.iwTime=profile.iwTimeMin;
				else if (profile.iwTime>profile.iwTimeMax)
					profile.iwTime=profile.iwTimeMax;
				// - a "1" recognized
				r.nConsecutiveZeros=0;
				return 1;
			}
			case TDecoderMethod::FDD_MARK_OGDEN:{
				// FDC-like flux reversal decoding from Mark Ogdens's DiskTools/flux2track
				// . reading some more from the Track for the next time
				auto &r=profile.method.ogden;
				const TLogTime iwTimeHalf=profile.iwTime/2;
				do{
					if (!*this)
						return 0;
					if (logTimes[iNextTime]-currentTime<iwTimeHalf)
						iNextTime++;
					else
						break;
				}while (true);
				// . detecting zero
				currentTime+=profile.iwTime;
				const TLogTime diff=logTimes[iNextTime]-currentTime;
				if (diff>=iwTimeHalf)
					return 0;
				// . estimating data frequency
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
				static const BYTE PhaseAdjustments[2][16]={ // C1/C2, C3
					//	8	9	A	B	C	D	E	F	0	1	2	3	4	5	6	7
					 { 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20 },
					 { 13, 14, 14, 15, 15, 16, 16, 16, 16, 16, 16, 17, 17, 18, 18, 19 }
				};
				// . estimating data phase
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
				return 1;
			}
			default:
				ASSERT(FALSE);
				return 0;
		}
	}

	bool CImage::CTrackReader::ReadBits15(WORD &rOut){
		// True <=> at least 16 bits have not yet been read, otherwise False
		for( BYTE n=15; n-->0; rOut=(rOut<<1)|(BYTE)ReadBit() )
			if (!*this)
				return false;
		return true;
	}

	bool CImage::CTrackReader::ReadBits16(WORD &rOut){
		// True <=> at least 16 bits have not yet been read, otherwise False
		//if (method&TMethod::FDD_METHODS){
			for( BYTE n=16; n-->0; rOut=(rOut<<1)|(BYTE)ReadBit() )
				if (!*this)
					return false;
			return true;
		//}
	}

	bool CImage::CTrackReader::ReadBits32(DWORD &rOut){
		// True <=> at least 32 bits have not yet been read, otherwise False
		//if (method&TMethod::FDD_METHODS){
			for( BYTE n=32; n-->0; rOut=(rOut<<1)|(BYTE)ReadBit() )
				if (!*this)
					return false;
			return true;
		//}
	}

	WORD CImage::CTrackReader::Scan(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,TParseEvent *pOutParseEvents){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		profile.Reset();
		WORD nSectorsFound;
		switch (codec){
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
		if (pOutParseEvents)
			*pOutParseEvents=TParseEvent::Empty; // termination
		return nSectorsFound;
	}

	WORD CImage::CTrackReader::ScanAndAnalyze(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,TParseEvent *pOutParseEvents){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		// - standard scanning using current Codec
		ASSERT( pOutParseEvents!=nullptr );
		const WORD nSectorsFound=Scan( pOutFoundSectors, pOutIdEnds, pOutIdProfiles, pOutIdStatuses, pOutParseEvents );
		// - getting ParseEvents in Sector data
		struct{
			TLogTime time;
			TProfile profile;
		} dataEnds[1536];
		WORD nDataEnds=0;
		for( WORD s=0; s<nSectorsFound; s++ ){
			TSingleSectorParseEventBuffer peData;
			if (!ReadData( pOutIdEnds[s], pOutIdProfiles[s], CImage::GetOfficialSectorLength(pOutFoundSectors[s].lengthCode), peData ).DescribesMissingDam()
				&&
				*this // not end of Track (aka. data complete)
			){
				auto &r=dataEnds[nDataEnds++];
					r.time=currentTime;
					r.profile=profile;
			}
			pOutParseEvents->AddAscendingByStart( peData );
		}
		// - search for non-formatted areas
		if (nSectorsFound>0){ // makes sense only if some Sectors found
			const BYTE nCellsMin=64;
			const TLogTime tAreaLengthMin=nCellsMin*profile.iwTimeDefault; // ignore all non-formatted areas that are shorter
			for( TLogTime t0=RewindToIndexAndResetProfile(0),t; *this; t0=t )
				if (( t=ReadTime() )-t0>=tAreaLengthMin)
					for( PCParseEvent pe=pOutParseEvents; !pe->IsEmpty(); pe=pe->GetNext() )
						if (pe->IsDataStd())
							if (  pe->tStart<=t0 && t0<pe->tEnd  ||  pe->tStart<=t && t<pe->tEnd  ){
								pOutParseEvents->AddAscendingByStart(
									TParseEvent( TParseEvent::NONFORMATTED, t0, t-profile.iwTimeDefault, 0 )
								);
								break;
							}
		}
		// - search for data in gaps
		if (nSectorsFound>0){ // makes sense only if some Sectors found
			// . composition of all ends of ID and Data fields
			for( WORD s=0; s<nSectorsFound; s++ ){
				auto &r=dataEnds[nDataEnds+s];
					r.time=pOutIdEnds[s];
					r.profile=pOutIdProfiles[s];
			}
			const WORD nGaps=nDataEnds+nSectorsFound;
			// . analyzing gap between two consecutive ParseEvents
			const BYTE nBytesInspectedMax=20; // or maximum number of Bytes inspected before deciding that there are data in the gap
			ULONGLONG bitPattern;
			std::map<ULONGLONG,WORD> tmpHist; // Key = bit pattern (data+clock), Value = number of occurences
			char bitPatternLength=-1; // number of valid bits in the BitPattern
			for( WORD i=nGaps; i>0; ){
				const auto &r=dataEnds[--i];
				const PCParseEvent peNextEnd=pOutParseEvents->GetNextEnd(r.time+1);
				if (!peNextEnd->IsEmpty() && peNextEnd->tStart<r.time)
					continue; // gap that overlaps a ParseEvent is not a gap (e.g. Sector within Sector copy protection)
				const PCParseEvent peNext=pOutParseEvents->GetNextStart(r.time);
				if (peNext->IsEmpty())
					continue; // don't collect data in Gap4 after the last Sector (but yes, search the Gap4 after the Histogram is complete)
				SetCurrentTimeAndProfile( r.time, r.profile );
				BYTE nBytesInspected=0;
				for( TLogTime t=r.time; t<peNext->tStart && nBytesInspected<nBytesInspectedMax; nBytesInspected++ ){
					BYTE byte;
					bitPatternLength=ReadByte( bitPattern, &byte );
					auto it=tmpHist.find(bitPattern);
					if (it!=tmpHist.cend())
						it->second++;
					else
						tmpHist.insert( std::make_pair(bitPattern,1) );
				}
			}
			struct TItem sealed{
				ULONGLONG bitPattern; // data and clock corresponding to a Byte
				WORD count;

				inline bool operator<(const TItem &r) const{
					return count>r.count; // ">" = order descending
				}
				bool HasSameBitPatternRotated(ULONGLONG bitPattern,char bitPatternLength) const{
					const ULONGLONG bitMask=(1<<bitPatternLength)-1; // to mask out only lower N bits
					bool result=false; // assumption (BitPatterns are unequal no matter how rotated against each other)
					for( char nRotations=bitPatternLength; nRotations>0; nRotations-- ){
						result|=bitPattern==this->bitPattern;
						bitPattern=	( (bitPattern<<1)|(bitPattern>>(bitPatternLength-1)) ) // circular rotation left
									&
									bitMask; // masking out only lower N bits
					}
					return result;
				}
			} histogram[2500];
			WORD nUniqueBitPatterns=0;
			for( auto it=tmpHist.cbegin(); it!=tmpHist.cend(); it++ ){
				TItem &r=histogram[nUniqueBitPatterns++];
					r.bitPattern=it->first;
					r.count=it->second;
			}
			std::sort( histogram, histogram+nUniqueBitPatterns );
			// . production of new ParseEvents
			if (histogram->count>0) // a gap should always consits of some Bytes, but just to be sure
				for( WORD i=nGaps; i>0; ){
					const auto &r=dataEnds[--i];
					const PCParseEvent peNextEnd=pOutParseEvents->GetNextEnd(r.time+1);
					if (!peNextEnd->IsEmpty() && peNextEnd->tStart<r.time)
						continue; // gap that overlaps a ParseEvent is not a gap (e.g. Sector within Sector copy protection)
					const PCParseEvent peNext=pOutParseEvents->GetNextStart(r.time);
					const TLogTime tNextStart= peNext->IsEmpty() ? INT_MAX : peNext->tStart;
					SetCurrentTimeAndProfile( r.time, r.profile );
					const TItem &typicalItem=*histogram; // "typically" the filler Byte of post-ID Gap2, created during formatting, and thus always well aligned
					BYTE nBytesInspected=0, nBytesTypical=0;
					const BYTE nGapBytesMax=250;
					TDataParseEvent::TByteInfo byteInfos[nGapBytesMax];
					while (*this && nBytesInspected<nBytesInspectedMax){
						TDataParseEvent::TByteInfo &rbi=byteInfos[nBytesInspected++];
							rbi.tStart=currentTime;
							ReadByte( bitPattern, &rbi.value );
						if (currentTime>=tNextStart){
							currentTime=rbi.tStart; // putting unsuitable Byte back
							break;
						}
						nBytesTypical+=typicalItem.HasSameBitPatternRotated( bitPattern, bitPatternLength );
					}
					if (nBytesInspected-nBytesTypical>4){
						// significant amount of other than TypicalBytes, beyond a random noise on Track
						while (*this && nBytesInspected<nGapBytesMax){
							TDataParseEvent::TByteInfo &rbi=byteInfos[nBytesInspected];
								rbi.tStart=currentTime;
								ReadByte( bitPattern, &rbi.value );
							if (currentTime>=tNextStart
								||
								typicalItem.HasSameBitPatternRotated( bitPattern, bitPatternLength )
							){
								currentTime=rbi.tStart; // putting unsuitable Byte back
								break; // again Typical, so probably all gap data discovered
							}else
								nBytesInspected++;
						}
						TParseEvent peData[2000/sizeof(TParseEvent)], *pe=peData;
						TDataParseEvent::Create( pe, TParseEvent::DATA_IN_GAP, r.time, currentTime, nBytesInspected, byteInfos );
						pOutParseEvents->AddAscendingByStart( *peData );
					}
				}
		}
		// - successfully analyzed
		return nSectorsFound;
	}

	TFdcStatus CImage::CTrackReader::ReadData(TLogTime idEndTime,const TProfile &idEndProfile,WORD nBytesToRead,TParseEvent *pOutParseEvents){
		// attempts to read specified amount of Bytes into the Buffer, starting at position pointed to by the BitReader
		SetCurrentTimeAndProfile( idEndTime, idEndProfile );
		TFdcStatus st=TFdcStatus::NoDataField; // assumption
		switch (codec){
			case Codec::FM:
				st=ReadDataFm( nBytesToRead, pOutParseEvents );
				break;
			case Codec::MFM:
				st=ReadDataMfm( nBytesToRead, pOutParseEvents );
				break;
			default:
				ASSERT(FALSE); // we shouldn't end up here - all Codecs should be included in the Switch statement!
				break;
		}
		*pOutParseEvents=TParseEvent::Empty; // termination
		return st;
	}

	TFdcStatus CImage::CTrackReader::ReadData(TLogTime idEndTime,const TProfile &idEndProfile,WORD nBytesToRead,LPBYTE buffer){
		// attempts to read specified amount of Bytes into the Buffer, starting at position pointed to by the BitReader
		TSingleSectorParseEventBuffer peBuffer;
		const TFdcStatus st=ReadData( idEndTime, idEndProfile, nBytesToRead, peBuffer );
		for( PCParseEvent pe=peBuffer; !pe->IsEmpty(); pe=pe->GetNext() )
			if (pe->IsDataStd()){
				auto nBytes=pe->size-sizeof(TParseEvent);
				for( auto *pbi=((PCDataParseEvent)pe)->byteInfos; nBytes; nBytes-=sizeof(*pbi) )
					*buffer++=pbi++->value;
				break;
			}
		return st;
	}











	const CImage::CTrackReader::TParseEvent CImage::CTrackReader::TParseEvent::Empty(EMPTY,INT_MAX,INT_MAX,0);

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
		0xa79b8a	// custom (variable string)
	};

	inline
	CImage::CTrackReader::TParseEvent::TParseEvent(TType type,TLogTime tStart,TLogTime tEnd,DWORD data)
		// ctor
		: type(type) , size(sizeof(TParseEvent))
		, tStart(tStart) , tEnd(tEnd) {
		dw=data;
	}

	void CImage::CTrackReader::TMetaStringParseEvent::Create(TParseEvent *&buffer,TLogTime tStart,TLogTime tEnd,LPCSTR lpszMetaString){
		ASSERT( lpszMetaString!=nullptr );
		*buffer=TParseEvent( TType::META_STRING, tStart, tEnd, 0 );
		buffer->size =	sizeof(TMetaStringParseEvent)
						+
						::lstrlenA(  ::lstrcpynA( buffer->lpszMetaString, lpszMetaString, (decltype(buffer->size))-1-sizeof(TParseEvent) )  )
						+
						1 // "+1" = including terminal Null character
						-
						sizeof(buffer->lpszMetaString); // already counted into Size
		buffer=const_cast<TParseEvent *>(buffer->GetNext());
	}

	void CImage::CTrackReader::TDataParseEvent::Create(TParseEvent *&buffer,bool dataOk,TLogTime tStart,TLogTime tEnd,DWORD nBytes,PCByteInfo pByteInfos){
		Create( buffer, dataOk?DATA_OK:DATA_BAD, tStart, tEnd, nBytes, pByteInfos );
	}

	void CImage::CTrackReader::TDataParseEvent::Create(TParseEvent *&buffer,TParseEvent::TType type,TLogTime tStart,TLogTime tEnd,DWORD nBytes,PCByteInfo pByteInfos){
		ASSERT( nBytes>0 && pByteInfos!=nullptr );
		*buffer=TParseEvent( type, tStart, tEnd, nBytes );
		if (sizeof(TDataParseEvent)+nBytes*sizeof(TByteInfo)<=(decltype(buffer->size))-1){
			::memcpy( ((TDataParseEvent *)buffer)->byteInfos, pByteInfos, nBytes*sizeof(TByteInfo) );
			buffer->size =	sizeof(TDataParseEvent)
							+
							(nBytes-1)*sizeof(TByteInfo); // "-1" = already counted into Size
		}else // if ParseEvent Size would exceed the range, can't store information on ByteEnds
			ASSERT(FALSE); // need to increase the Size capacity
		buffer=const_cast<TParseEvent *>(buffer->GetNext());
	}

	const CImage::CTrackReader::TParseEvent *CImage::CTrackReader::TParseEvent::GetNext() const{
		return	(PCParseEvent)(  (PCBYTE)this+size  );
	}

	const CImage::CTrackReader::TParseEvent *CImage::CTrackReader::TParseEvent::GetNextStart(TLogTime tMin,TType type) const{
		PCParseEvent pe=this;
		while (!pe->IsEmpty() && (pe->tStart<tMin || type!=TType::EMPTY&&pe->type!=type))
			pe=pe->GetNext();
		return pe;
	}

	const CImage::CTrackReader::TParseEvent *CImage::CTrackReader::TParseEvent::GetNextEnd(TLogTime tMin,TType type) const{
		PCParseEvent pe=this;
		while (!pe->IsEmpty() && (pe->tEnd<tMin || type!=TType::EMPTY&&pe->type!=type))
			pe=pe->GetNext();
		return pe;
	}

	const CImage::CTrackReader::TParseEvent *CImage::CTrackReader::TParseEvent::GetLast() const{
		if (IsEmpty()) // this is already the last Event
			return this;
		PCParseEvent pe=this;
		for( PCParseEvent next; !(next=pe->GetNext())->IsEmpty(); pe=next );
		return pe;
	}

	bool CImage::CTrackReader::TParseEvent::Contains(TType type) const{
		return !GetNextStart( tStart, CImage::CTrackReader::TParseEvent::DATA_IN_GAP )->IsEmpty();
	}

	void CImage::CTrackReader::TParseEvent::AddAscendingByStart(const TParseEvent &pe){
		// adds specified ParseEvents to this list so that the result is ordered by Start times
		TParseEvent *const peInsertAt=const_cast<TParseEvent *>(GetNextStart(pe.tStart));
		int nBytes=(PCBYTE)peInsertAt->GetLast()->GetNext()-(PCBYTE)peInsertAt+sizeof(TParseEvent); // "+sizeof" = including the terminal Empty ParseEvent
		::memmove( (PBYTE)peInsertAt+pe.size, peInsertAt, nBytes );
		::memcpy( peInsertAt, &pe, pe.size );
	}

	void CImage::CTrackReader::TParseEvent::AddAscendingByStart(const TParseEvent *peList){
		// adds all ParseEvents to this list, ordered by their Start times ascending (MergeSort)
		int nThisEventBytes=(PCBYTE)GetLast()->GetNext()-(PCBYTE)this+sizeof(TParseEvent); // "+sizeof" = including the terminal Empty ParseEvent
		int nListEventBytes=(PCBYTE)peList->GetLast()->GetNext()-(PCBYTE)peList;
		for( PCParseEvent pe1=this,pe2=peList; nThisEventBytes&&nListEventBytes; pe1=pe1->GetNext() )
			if (pe1->tStart<=pe2->tStart) // should never be equal, but just in case
				nThisEventBytes-=pe1->size;
			else{
				const auto pe2Size=pe2->size;
				::memmove( (PBYTE)pe1+pe2Size, pe1, nThisEventBytes );
				::memcpy( (PBYTE)pe1, pe2, pe2Size );
				nListEventBytes-=pe2Size, pe2=pe2->GetNext();
			}
	}













	WORD CImage::CTrackReader::ScanFm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,TParseEvent *&pOutParseEvents){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		ASSERT( pOutFoundSectors!=nullptr && pOutIdEnds!=nullptr );
		RewindToIndex(0);
		//TODO
		return 0;
	}

	TFdcStatus CImage::CTrackReader::ReadDataFm(WORD nBytesToRead,TParseEvent *&pOutParseEvents){
		// attempts to read specified amount of Bytes into the Buffer, starting at current position; returns the amount of Bytes actually read
		ASSERT( codec==Codec::FM );
		//TODO
		return TFdcStatus::SectorNotFound;
	}

	WORD CImage::CTrackReaderWriter::WriteDataFm(WORD nBytesToWrite,PCBYTE buffer,TFdcStatus sr){
		// attempts to write specified amount of Bytes in the Buffer, starting at current position; returns the amount of Bytes actually written
		ASSERT( codec==Codec::FM );
		//TODO
		return 0;
	}










	namespace MFM{
		static const CFloppyImage::TCrc16 CRC_A1A1A1=0xb4cd; // CRC of 0xa1, 0xa1, 0xa1

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
	}

	WORD CImage::CTrackReader::ScanMfm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,TParseEvent *&pOutParseEvents){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		ASSERT( pOutFoundSectors!=nullptr && pOutIdEnds!=nullptr );
		// - some Sectors may start just after the IndexPulse (e.g. MDOS 1.0); preparing the scanner for this eventuality by adjusting the frequency and phase shortly BEFORE the IndexPulse
		const TLogTime indexTime=GetIndexTime(0);
		for( SetCurrentTime(indexTime-10*profile.iwTimeDefault); currentTime<indexTime; ReadBit() ); // "N*" = number of 0x00 Bytes before the 0xA1 clock-distorted mark
		// - scanning
		TLogTime tEventStart;
		TLogTime tSyncStarts[64]; BYTE iSyncStart=0;
		WORD nSectors=0, w, sync1=0; DWORD sync23=0;
		for( const TLogTime revolutionEndTime=GetIndexTime(nIndexPulses+1); *this; ){
			// . searching for three consecutive 0xA1 distorted synchronization Bytes
			tSyncStarts[iSyncStart++&63]=currentTime;
			sync23=	(sync23<<1) | ((sync1&0x8000)!=0);
			sync1 =	(sync1<<1) | (BYTE)ReadBit();
			if ((sync1&0xffdf)!=0x4489 || (sync23&0xffdfffdf)!=0x44894489)
				continue;
			if (pOutParseEvents)
				*pOutParseEvents++=TParseEvent( TParseEvent::SYNC_3BYTES, tSyncStarts[(iSyncStart+256-48)&63], currentTime, 0xa1a1a1 );
			sync1=0; // invalidating "buffered" synchronization, so that it isn't reused
			// . an ID Field mark should follow the synchronization
			tEventStart=currentTime;
			if (!ReadBits16(w)) // Track end encountered
				break;
			const BYTE idam=MFM::DecodeByte(w);
			if ((idam&0xfe)!=0xfe){ // not the expected ID Field mark; the least significant bit is always ignored by the FDC [http://info-coach.fr/atari/documents/_mydoc/Atari-Copy-Protection.pdf]
				if (pOutParseEvents)
					pOutParseEvents--; // dismiss the synchronization ParseEvent
				continue;
			}
			struct{
				BYTE idFieldAm, cyl, side, sector, length;
			} data={ idam };
			if (pOutParseEvents)
				*pOutParseEvents++=TParseEvent( TParseEvent::MARK_1BYTE, tEventStart, currentTime, idam );
			// . reading SectorId
			tEventStart=currentTime;
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
			if (pOutParseEvents)
				TMetaStringParseEvent::Create( pOutParseEvents, tEventStart, currentTime, rid.ToString() );
			// . reading and comparing ID Field's CRC
			tEventStart=currentTime;
			DWORD dw;
			CFloppyImage::TCrc16 crc=0;
			const bool crcBad=!ReadBits32(dw) || Utils::CBigEndianWord(MFM::DecodeWord(dw)).GetBigEndian()!=( crc=CFloppyImage::GetCrc16Ccitt(MFM::CRC_A1A1A1,&data,sizeof(data)) ); // no or wrong IdField CRC
			*pOutIdStatuses++ =	crcBad 
								? TFdcStatus::IdFieldCrcError
								: TFdcStatus::WithoutError;
			*pOutIdEnds++=currentTime;
			*pOutIdProfiles++=profile;
			if (pOutParseEvents)
				*pOutParseEvents++=TParseEvent( crcBad?TParseEvent::CRC_BAD:TParseEvent::CRC_OK, tEventStart, currentTime, crc );
			// . sector found
			nSectors++;
		}
		// - returning the result
		return nSectors;
	}

	TFdcStatus CImage::CTrackReader::ReadDataMfm(WORD nBytesToRead,TParseEvent *&pOutParseEvents){
		// attempts to read specified amount of Bytes into the Buffer, starting at position pointed to by the BitReader; returns the amount of Bytes actually read
		ASSERT( codec==Codec::MFM );
		// - searching for the nearest three consecutive 0xA1 distorted synchronization Bytes
		TLogTime tEventStart;
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
			*pOutParseEvents++=TParseEvent( TParseEvent::SYNC_3BYTES, tSyncStarts[(iSyncStart+256-48)&63], currentTime, 0xa1a1a1 );
		// - a Data Field mark should follow the synchronization
		tEventStart=currentTime;
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
			*pOutParseEvents++=TParseEvent( TParseEvent::MARK_1BYTE, tEventStart, currentTime, dam );
		// - reading specified amount of Bytes into the Buffer
		TDataParseEvent::TByteInfo byteInfos[16384], *p=byteInfos;
		CFloppyImage::TCrc16 crc=CFloppyImage::GetCrc16Ccitt( MFM::CRC_A1A1A1, &dam, sizeof(dam) ); // computing actual CRC along the way
		for( tEventStart=currentTime; *this&&nBytesToRead>0; nBytesToRead-- ){
			p->tStart=currentTime;
			if (!ReadBits16(w)){ // Track end encountered
				result.ExtendWith( TFdcStatus::DataFieldCrcError );
				break;
			}
			p->value=MFM::DecodeByte(w);
			crc=CFloppyImage::GetCrc16Ccitt( crc, &p++->value, 1 );
		}
		TParseEvent *const peData=pOutParseEvents;
		if (pOutParseEvents)
			TDataParseEvent::Create( pOutParseEvents, !nBytesToRead, tEventStart, currentTime, p-byteInfos, byteInfos );
		if (!*this)
			return result;
		// - comparing Data Field's CRC
		tEventStart=currentTime;
		DWORD dw;
		const bool crcBad=!ReadBits32(dw) || Utils::CBigEndianWord(MFM::DecodeWord(dw)).GetBigEndian()!=crc; // no or wrong Data Field CRC
		if (crcBad){
			result.ExtendWith( TFdcStatus::DataFieldCrcError );
			if (peData){
				peData->type=TParseEvent::DATA_BAD;
				*pOutParseEvents++=TParseEvent( TParseEvent::CRC_BAD, tEventStart, currentTime, crc );
			}
		}else
			if (pOutParseEvents)
				*pOutParseEvents++=TParseEvent( TParseEvent::CRC_OK, tEventStart, currentTime, crc );
		return result;
	}

	WORD CImage::CTrackReaderWriter::WriteDataMfm(WORD nBytesToWrite,PCBYTE buffer,TFdcStatus sr){
		// attempts to write specified amount of Bytes in the Buffer, starting at current position; returns the amount of Bytes actually written
		ASSERT( codec==Codec::MFM );
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
		// - writing the Bits
		return	WriteBits( bits+1, pBit-bits-1 ) // "1" = the auxiliary "previous" bit of distorted 0xA1 sync mark
				? nBytesToWrite
				: 0;
	}

	char CImage::CTrackReader::ReadByte(ULONGLONG &rOutBits,PBYTE pOutValue){
		// reads number of bits corresponding to one Byte; if all such bits successfully read, returns their count, or -1 otherwise
		switch (codec){
			case Codec::FM:
				ASSERT(FALSE); //TODO
				return -1;
			case Codec::MFM:{
				WORD w;
				if (ReadBits16(w)){ // all bits read?
					rOutBits=w;
					if (pOutValue)
						*pOutValue=MFM::DecodeByte(w);
					return 16;
				}else
					return -1;
			}
			default:
				ASSERT(FALSE); // we shouldn't end up here - check if all Codecs are included in the Switch statement!
				return -1;
		}
	}













	const CImage::CTrackReader::TProfile CImage::CTrackReader::TProfile::HD(
		Medium::TProperties::FLOPPY_HD_350, // same for both 3.5" and 5.25" HD floppies
		4 // inspection window size tolerance
	);

	const CImage::CTrackReader::TProfile CImage::CTrackReader::TProfile::DD(
		Medium::TProperties::FLOPPY_DD,
		4 // inspection window size tolerance
	);

	const CImage::CTrackReader::TProfile CImage::CTrackReader::TProfile::DD_525(
		Medium::TProperties::FLOPPY_DD_525,
		4 // inspection window size tolerance
	);

	CImage::CTrackReader::TProfile::TProfile(const Medium::TProperties &floppyProps,BYTE iwTimeTolerancePercent)
		// ctor
		: iwTimeDefault(floppyProps.cellTime)
		, iwTime(iwTimeDefault)
		, iwTimeMin( iwTimeDefault*(100-iwTimeTolerancePercent)/100 )
		, iwTimeMax( iwTimeDefault*(100+iwTimeTolerancePercent)/100 )
		, adjustmentPercentMax(30) {
	}

	void CImage::CTrackReader::TProfile::Reset(){
		iwTime=iwTimeDefault;
		::ZeroMemory( &method, sizeof(method) );
	}












	const CImage::CTrackReaderWriter CImage::CTrackReaderWriter::Invalid( 0, CTrackReader::TDecoderMethod::NONE, false ); // TrackReader invalid right from its creation

	CImage::CTrackReaderWriter::CTrackReaderWriter(DWORD nLogTimesMax,TDecoderMethod method,bool resetDecoderOnIndex)
		// ctor
		: CTrackReader( (PLogTime)::calloc(1+nLogTimesMax+1,sizeof(TLogTime)), 0, nullptr, 0, Medium::FLOPPY_DD, Codec::MFM, method, resetDecoderOnIndex ) // "1+" = hidden item represents reference counter, "+1" = for stop-conditions and other purposes
		, nLogTimesMax(nLogTimesMax) {
	}

	CImage::CTrackReaderWriter::CTrackReaderWriter(const CTrackReaderWriter &rTrackReaderWriter,bool shareTimes)
		// copy ctor
		: CTrackReader( rTrackReaderWriter )
		, nLogTimesMax( rTrackReaderWriter.nLogTimesMax ) {
		if (!shareTimes){
			CTrackReaderWriter tmp( rTrackReaderWriter.nLogTimesMax, rTrackReaderWriter.method, rTrackReaderWriter.resetDecoderOnIndex );
			std::swap( *const_cast<PLogTime *>(&tmp.logTimes), *const_cast<PLogTime *>(&logTimes) );
			::memcpy( logTimes, rTrackReaderWriter.logTimes, nLogTimes*sizeof(TLogTime) );
		}
	}
	
	CImage::CTrackReaderWriter::CTrackReaderWriter(CTrackReaderWriter &&rTrackReaderWriter)
		// move ctor
		: CTrackReader( std::move(rTrackReaderWriter) )
		, nLogTimesMax( rTrackReaderWriter.nLogTimesMax ) {
	}
	
	CImage::CTrackReaderWriter::CTrackReaderWriter(const CTrackReader &tr)
		// copy ctor
		: CTrackReader(tr)
		, nLogTimesMax( tr.GetTimesCount() ) {
	}
	
	void CImage::CTrackReaderWriter::AddTimes(PCLogTime logTimes,DWORD nLogTimes){
		// appends given amount of LogicalTimes at the end of the Track
		ASSERT( this->nLogTimes+nLogTimes<=nLogTimesMax );
		if (this->logTimes+this->nLogTimes==logTimes)
			// caller wrote directly into the buffer (e.g. creation of initial content); faster than calling N-times AddTime
			this->nLogTimes+=nLogTimes;
		else{
			// caller used its own buffer to store new LogicalTimes
			::memcpy( this->logTimes, logTimes, nLogTimes*sizeof(TLogTime) );
			this->nLogTimes+=nLogTimes;
		}
	}

	void CImage::CTrackReaderWriter::AddIndexTime(TLogTime logTime){
		// appends LogicalTime representing the position of the index pulse on the disk
		ASSERT( nIndexPulses<Revolution::MAX );
		ASSERT( logTime>=0 );
		indexPulses[nIndexPulses++]=logTime;
		indexPulses[nIndexPulses]=INT_MAX;
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
		if (nNewLogTimes>nLogTimesMax)
			return false;
		const PLogTime pOverwritingStart=logTimes+iNextTime;
		::memmove(
			pOverwritingStart+nOnesCurrently,
			pOverwritingStart+nOnesPreviously,
			(nLogTimes-iNextTime-nOnesPreviously)*sizeof(TLogTime)
		);
		nLogTimes=nNewLogTimes;
		const PLogTime newLogTimesTemp=(PLogTime)::calloc( nOnesCurrently, sizeof(TLogTime) );
			PLogTime pt=newLogTimesTemp;
			for( DWORD i=0; i++<nBits; )
				if (*bits++)
					*pt++=tOverwritingStart+(LONGLONG)(tOverwritingEnd-tOverwritingStart)*i/nBits;
			::memcpy( pOverwritingStart, newLogTimesTemp, nOnesCurrently*sizeof(TLogTime) );
		::free(newLogTimesTemp);
		return true;
	}

	WORD CImage::CTrackReaderWriter::WriteData(TLogTime idEndTime,const TProfile &idEndProfile,WORD nBytesToWrite,PCBYTE buffer,TFdcStatus sr){
		// attempts to write specified amount of Bytes in the Buffer, starting after specified IdEndTime; returns the number of Bytes actually written
		SetCurrentTimeAndProfile( idEndTime, idEndProfile );
		switch (codec){
			case Codec::FM:
				return WriteDataFm( nBytesToWrite, buffer, sr );
			case Codec::MFM:
				return WriteDataMfm( nBytesToWrite, buffer, sr );
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
		if (const Medium::PCProperties mp=Medium::GetProperties(mediumType))
			revolutionTime=mp->revolutionTime;
		else
			return false;
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
		return true;
	}

	TStdWinError CImage::CTrackReaderWriter::NormalizeEx(TLogTime timeOffset,bool fitTimesIntoIwMiddles,bool correctCellCountPerRevolution,bool correctRevolutionTime){
		// True <=> all Revolutions of this Track successfully normalized using specified parameters, otherwise False
		// - if the Track contains less than two Indices, we are successfully done
		if (nIndexPulses<2)
			return ERROR_SUCCESS;
		// - MediumType must be supported
		const Medium::PCProperties mp=Medium::GetProperties(mediumType);
		if (!mp)
			return ERROR_UNRECOGNIZED_MEDIA;
		// - ignoring what's before the first Index
		TLogTime tCurrIndexOrg=RewindToIndex(0);
		// - normalization
		const TLogTime tLastIndex=GetIndexTime(nIndexPulses-1);
		const DWORD iModifStart=iNextTime;
		DWORD iTime=iModifStart;
		const std::unique_ptr<TLogTime,void (__cdecl *)(PVOID)> buffer(  (PLogTime)::calloc( nLogTimesMax, sizeof(TLogTime) ), ::free  );
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
						if (iTime<nLogTimesMax)
							ptModified[iTime++] = tCurrIndexOrg + nAlignedCells*profile.iwTimeDefault;
						else
							return ERROR_INSUFFICIENT_BUFFER; // mustn't overrun the Buffer
			}else
				// alignment not wanted - just copying the Times in current Revolution
				while (*this && logTimes[iNextTime]<tNextIndexOrg)
					ptModified[iTime++]=ReadTime();
			DWORD iModifRevEnd=iTime;
			// . offsetting all LogicalTimes in this Revolution
			if (timeOffset){
				timeOffset=	nAlignedCells>0 // do we have time-corrected cells from above?
							? timeOffset/profile.iwTimeDefault*profile.iwTimeDefault // rounding down to whole multiples of correctly-sized cells
							: (LONGLONG)timeOffset*(tNextIndexOrg-tCurrIndexOrg)/mp->revolutionTime;
				ptModified[iModifRevEnd]=INT_MAX-timeOffset; // stop-condition
				for( iTime=iModifRevStart; ptModified[iTime]+timeOffset<=tCurrIndexOrg; iTime++ ); // ignoring Times that would end up before this Revolution has begun
				iModifRevEnd=iModifRevStart;
				if (nAlignedCells>0){ // are we working with time-corrected cells?
					const TLogTime tLastAlignedCell= tCurrIndexOrg + nAlignedCells*profile.iwTimeDefault;
					while (( ptModified[iModifRevEnd]=ptModified[iTime++]+timeOffset )<tLastAlignedCell) // adjusting Times that remain within this Revolution
						iModifRevEnd++;
					if (iModifRevEnd>iModifRevStart) // at least one Time
						nAlignedCells=(ptModified[iModifRevEnd-1]-tCurrIndexOrg)/profile.iwTimeDefault;
				}else
					while (( ptModified[iModifRevEnd]=ptModified[iTime++]+timeOffset )<tNextIndexOrg) // adjusting Times that remain within this Revolution
						iModifRevEnd++;
			}
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
		if (nLogTimes+iTime-iNextTime>=nLogTimesMax)
			return ERROR_INSUFFICIENT_BUFFER; // mustn't overrun the Buffer
		::memmove( logTimes+iTime, logTimes+iNextTime, (nLogTimes-iNextTime)*sizeof(TLogTime) );
		nLogTimes+=iTime-iNextTime;
		if (const TLogTime dt=indexPulses[nIndexPulses-1]-tLastIndex)
			for( DWORD i=iTime; i<nLogTimes; logTimes[i++]+=dt );
		::memcpy( logTimes+iModifStart, ptModified+iModifStart, (iTime-iModifStart)*sizeof(TLogTime) );
		SetCurrentTime(0); // setting valid state
		// - successfully aligned
		return ERROR_SUCCESS;
	}
