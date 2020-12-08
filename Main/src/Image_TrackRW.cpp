#include "stdafx.h"

	CImage::CTrackReader::CTrackReader(PLogTime _logTimes,DWORD nLogTimes,PCLogTime indexPulses,BYTE _nIndexPulses,TMedium::TType mediumType,TCodec codec,TDecoderMethod method)
		// ctor
		: logTimes(_logTimes+1) , nLogTimes(nLogTimes) // "+1" = hidden item represents reference counter
		, iNextIndexPulse(0) , nIndexPulses(  std::min<BYTE>( DEVICE_REVOLUTIONS_MAX, _nIndexPulses )  )
		, iNextTime(0) , currentTime(0)
		, method(method)
		, nConsecutiveZeros(0) {
		::memcpy( this->indexPulses, indexPulses, nIndexPulses*sizeof(TLogTime) );
		this->indexPulses[nIndexPulses]=INT_MAX; // a virtual IndexPulse in infinity
		logTimes[-1]=1; // initializing the reference counter
		SetCodec(codec); // setting values associated with the specified Codec
		SetMediumType(mediumType); // setting values associated with the specified MediumType
	}

	CImage::CTrackReader::CTrackReader(const CTrackReader &tr)
		// copy ctor
		: logTimes(tr.logTimes) , method(tr.method) {
		::memcpy( this, &tr, sizeof(*this) );
		::InterlockedIncrement( (PUINT)logTimes-1 ); // increasing the reference counter
	}

	CImage::CTrackReader::CTrackReader(CTrackReader &&rTrackReader)
		// move ctor
		: logTimes(rTrackReader.logTimes) , method(rTrackReader.method) {
		::memcpy( this, &rTrackReader, sizeof(*this) );
		::InterlockedIncrement( (PUINT)logTimes-1 ); // increasing the reference counter
	}

	CImage::CTrackReader::~CTrackReader(){
		// dtor
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
		return	*this ? logTimes[iNextTime++] : 0;
	}

	void CImage::CTrackReader::SetCodec(TCodec codec){
		// changes the interpretation of recorded LogicalTimes according to the new Codec
		switch ( this->codec=codec ){
			default:
				ASSERT(FALSE); // we shouldn't end up here, this value must be set for all implemented Codecs!
				//fallthrough
			case TCodec::FM:
				nConsecutiveZerosMax=1;
				break;
			case TCodec::MFM:
				nConsecutiveZerosMax=3;
				break;
		}
	}

	const CImage::CTrackReader::TProfile CImage::CTrackReader::TProfile::HD_350={
		TIME_MICRO(1), // iwTimeDefault = 1 second / 500kb = 2 us -> 1 us for MFM encoding
		TIME_MICRO(1), // iwTime
		TIME_NANO(930), TIME_NANO(1070), // iwTimeMin, iwTimeMax
		30 // adjustmentPercentMax
	};

	const CImage::CTrackReader::TProfile CImage::CTrackReader::TProfile::DD_350={
		TIME_MICRO(2), // iwTimeDefault = 1 second / 250kb = 4 us -> 2 us for MFM encoding
		TIME_MICRO(2), // iwTime
		TIME_NANO(1830), TIME_NANO(2170), // iwTimeMin, iwTimeMax
		30 // adjustmentPercentMax
	};

	const CImage::CTrackReader::TProfile CImage::CTrackReader::TProfile::DD_525={
		TIME_NANO(1666), // iwTimeDefault = 1 second / 300kb = 3.333 us -> 1.666 us for MFM encoding
		TIME_NANO(1666), // iwTime
		TIME_NANO(1506), TIME_NANO(1826), // iwTimeMin, iwTimeMax
		30 // adjustmentPercentMax
	};

	void CImage::CTrackReader::SetMediumType(TMedium::TType mediumType){
		// changes the interpretation of recorded LogicalTimes according to the new MediumType
		switch ( this->mediumType=mediumType ){
			default:
				ASSERT(FALSE); // we shouldn't end-up here, all Media Types applicable for general Track description should be covered
				//fallthrough
			case TMedium::FLOPPY_DD_350:
				profile=TProfile::DD_350;
				break;
			case TMedium::FLOPPY_DD_525:
				profile=TProfile::DD_525;
				break;
			case TMedium::FLOPPY_HD_350:
				profile=TProfile::HD_350;
				break;
		}
	}

	bool CImage::CTrackReader::ReadBit(){
		// returns first bit not yet read
		// - if we just crossed an IndexPulse, resetting the Profile
		if (currentTime>=indexPulses[iNextIndexPulse]){
			profile.Reset();
			iNextIndexPulse++;
		}
		// - reading next bit
		#ifdef _DEBUG
			if (!*this)
				ASSERT(FALSE); // this method mustn't be called when there's nothing actually to be read!
		#endif
		switch (method){
			case TDecoderMethod::FDD_KEIR_FRASIER:{
				// FDC-like flux reversal decoding from Keir Frasier's Disk-Utilities/libdisk
				// - reading some more from the Track
				const TLogTime iwTimeHalf=profile.iwTime/2;
				while (logTimes[iNextTime]-currentTime<iwTimeHalf)
					if (*this)
						iNextTime++;
					else
						return 0;
				// - detecting zero (longer than 3/2 of an inspection window)
				currentTime+=profile.iwTime;
				const TLogTime diff=logTimes[iNextTime]-currentTime;
				while (diff>=iwTimeHalf){
					nConsecutiveZeros++;
					return 0;
				}
				// - adjust data frequency according to phase mismatch
				if (nConsecutiveZeros<=nConsecutiveZerosMax)
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
				nConsecutiveZeros=0;
				return 1;
			}
			case TDecoderMethod::FDD_KEIR_FRASIER_MODIFIED:{
				// modified FDC-like flux reversal decoding from Keir Frasier's Disk-Utilities/libdisk
				// - detecting zero
				const TLogTime iwTimeHalf=profile.iwTime/2;
				const TLogTime diff=logTimes[iNextTime]-currentTime;
				currentTime+=profile.iwTime;
				if (diff>=profile.iwTime){
					nConsecutiveZeros++;
					return 0;
				}
				// - reading some more from the Track for the next time
				while (logTimes[iNextTime]-currentTime<iwTimeHalf)
					if (*this)
						iNextTime++;
					else
						return 0;
				// - adjust data frequency according to phase mismatch
				if (nConsecutiveZeros<=nConsecutiveZerosMax)
					// in sync - adjust inspection window by percentage of phase mismatch
					profile.iwTime+= (diff-iwTimeHalf) * profile.adjustmentPercentMax/100;
				else
					// out of sync - adjust inspection window towards its Default size
					profile.iwTime+= (profile.iwTimeDefault-profile.iwTime) * profile.adjustmentPercentMax/100;
				// - keep the inspection window size within limits
				if (profile.iwTime<profile.iwTimeMin)
					profile.iwTime=profile.iwTimeMin;
				else if (profile.iwTime>profile.iwTimeMax)
					profile.iwTime=profile.iwTimeMax;
				// - a "1" recognized
				nConsecutiveZeros=0;
				return 1;
			}
			default:
				ASSERT(FALSE);
				return 0;
		}
	}

	bool CImage::CTrackReader::ReadBits16(WORD &rOut){
		// True <=> at least 16 bits have not yet been read, otherwise False
		//if (method&TMethod::FDD_METHODS){
			for( BYTE n=16; n-->0; rOut=(rOut<<1)|ReadBit() )
				if (!*this)
					return false;
			return true;
		//}
	}

	bool CImage::CTrackReader::ReadBits32(DWORD &rOut){
		// True <=> at least 32 bits have not yet been read, otherwise False
		//if (method&TMethod::FDD_METHODS){
			for( BYTE n=32; n-->0; rOut=(rOut<<1)|ReadBit() )
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
			case TCodec::FM:
				nSectorsFound=ScanFm( pOutFoundSectors, pOutIdEnds, pOutIdProfiles, pOutIdStatuses, pOutParseEvents );
				break;
			case TCodec::MFM:
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

	TFdcStatus CImage::CTrackReader::ReadData(TLogTime idEndTime,const TProfile &idEndProfile,WORD nBytesToRead,LPBYTE buffer,TParseEvent *pOutParseEvents){
		// attempts to read specified amount of Bytes into the Buffer, starting at position pointed to by the BitReader; returns the amount of Bytes actually read
		SetCurrentTime( idEndTime );
		profile=idEndProfile;
		TFdcStatus st;
		switch (codec){
			case TCodec::FM:
				st=ReadDataFm( nBytesToRead, buffer, pOutParseEvents );
				break;
			case TCodec::MFM:
				st=ReadDataMfm( nBytesToRead, buffer, pOutParseEvents );
				break;
			default:
				ASSERT(FALSE); // we shouldn't end up here - all Codecs should be included in the Switch statement!
				return TFdcStatus::NoDataField;
		}
		if (pOutParseEvents)
			*pOutParseEvents=TParseEvent::Empty; // termination
		return st;
	}











	const CImage::CTrackReader::TParseEvent CImage::CTrackReader::TParseEvent::Empty(EMPTY,0,0,0);

	const COLORREF CImage::CTrackReader::TParseEvent::TypeColors[LAST]={
		0,			// None
		0xa398c2,	// sync 3 Bytes
		0x82c5e7,	// mark 1 Byte
		0xccd4f2,	// preamble
		0xc4886c,	// data (variable length)
		0x91d04d,	// CRC ok
		0x6857ff,	// CRC bad
		0xa79b8a	// custom (variable string)
	};

	inline
	CImage::CTrackReader::TParseEvent::TParseEvent(TType type,TLogTime tStart,TLogTime tEnd,DWORD data)
		// ctor
		: type(type)
		, tStart(tStart) , tEnd(tEnd) {
		dw=data;
	}

	void CImage::CTrackReader::TParseEvent::WriteCustom(TParseEvent *&buffer,TLogTime tStart,TLogTime tEnd,LPCSTR lpszCustom){
		*buffer=TParseEvent( (TType)(sizeof(TParseEvent)+::lstrlenA(lpszCustom)), tStart, tEnd, 0 );
		::lstrcpyA(buffer->lpszCustom,lpszCustom);
		buffer=const_cast<TParseEvent *>(buffer->GetNext());
	}

	const CImage::CTrackReader::TParseEvent *CImage::CTrackReader::TParseEvent::GetNext() const{
		return	(PCParseEvent)(  (PCBYTE)this+std::max<BYTE>( type, sizeof(TParseEvent) )  );
	}

	const CImage::CTrackReader::TParseEvent *CImage::CTrackReader::TParseEvent::GetLast() const{
		if (IsEmpty()) // this is already the last Event
			return this;
		PCParseEvent pe=this;
		for( PCParseEvent next; !(next=pe->GetNext())->IsEmpty(); pe=next );
		return pe;
	}













	WORD CImage::CTrackReader::ScanFm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,TParseEvent *&pOutParseEvents){
		// returns the number of Sectors recognized and decoded from underlying Track bits over all complete revolutions
		ASSERT( pOutFoundSectors!=nullptr && pOutIdEnds!=nullptr );
		RewindToIndex(0);
		//TODO
		return 0;
	}

	TFdcStatus CImage::CTrackReader::ReadDataFm(WORD nBytesToRead,LPBYTE buffer,TParseEvent *&pOutParseEvents){
		// attempts to read specified amount of Bytes into the Buffer, starting at current position; returns the amount of Bytes actually read
		ASSERT( codec==TCodec::FM );
		//TODO
		return TFdcStatus::SectorNotFound;
	}










	namespace MFM{
		static BYTE DecodeByte(WORD w){
			BYTE result=0;
			for( BYTE n=8; n-->0; w<<=1,w<<=1 )
				result=(result<<1)|((w&0x4000)!=0);
			return result;
		}
		static WORD DecodeBigEndianWord(DWORD dw){
			WORD result=0;
			for( BYTE n=16; n-->0; dw<<=1,dw<<=1 )
				result=(result<<1)|((dw&0x40000000)!=0);
			return (LOBYTE(result)<<8) + HIBYTE(result);
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
			sync1 =	(sync1<<1) | ReadBit();
			if ((sync1&0xffdf)!=0x4489 || (sync23&0xffdfffdf)!=0x44894489)
				continue;
			if (pOutParseEvents)
				*pOutParseEvents++=TParseEvent( TParseEvent::SYNC_3BYTES, tSyncStarts[(iSyncStart+256-48)&63], currentTime, 0xa1a1a1 );
			// . an ID Field mark should follow the synchronization
			tEventStart=currentTime;
			if (!ReadBits16(w)) // Track end encountered
				break;
			if (MFM::DecodeByte(w)!=0xfe){ // not the expected ID Field mark
				if (pOutParseEvents)
					pOutParseEvents--; // dismiss the synchronization ParseEvent
				continue;
			}
			struct{
				BYTE sync[3], idFieldAm, cyl, side, sector, length;
			} data={ 0xa1, 0xa1, 0xa1, 0xfe };
			if (pOutParseEvents)
				*pOutParseEvents++=TParseEvent( TParseEvent::MARK_1BYTE, tEventStart, currentTime, data.idFieldAm );
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
				TParseEvent::WriteCustom( pOutParseEvents, tEventStart, currentTime, rid.ToString() );
			// . reading and comparing ID Field's CRC
			tEventStart=currentTime;
			DWORD dw;
			const bool crcBad=!ReadBits32(dw) || MFM::DecodeBigEndianWord(dw)!=CFloppyImage::GetCrc16Ccitt(&data,sizeof(data)); // no or wrong IdField CRC
			*pOutIdStatuses++ =	crcBad 
								? TFdcStatus::IdFieldCrcError
								: TFdcStatus::WithoutError;
			*pOutIdEnds++=currentTime;
			*pOutIdProfiles++=profile;
			if (pOutParseEvents)
				*pOutParseEvents++=TParseEvent( crcBad?TParseEvent::CRC_BAD:TParseEvent::CRC_OK, tEventStart, currentTime, MFM::DecodeBigEndianWord(dw) );
			// . sector found
			nSectors++;
			sync1=0; // invalidating "buffered" synchronization
		}
		// - returning the result
		return nSectors;
	}

	TFdcStatus CImage::CTrackReader::ReadDataMfm(WORD nBytesToRead,LPBYTE buffer,TParseEvent *&pOutParseEvents){
		// attempts to read specified amount of Bytes into the Buffer, starting at position pointed to by the BitReader; returns the amount of Bytes actually read
		ASSERT( codec==TCodec::MFM );
		// - searching for the nearest three consecutive 0xA1 distorted synchronization Bytes
		TLogTime tEventStart;
		TLogTime tSyncStarts[64]; BYTE iSyncStart=0;
		WORD w, sync1=0; DWORD sync23=0;
		while (*this){
			tSyncStarts[iSyncStart++&63]=currentTime;
			sync23=	(sync23<<1) | ((sync1&0x8000)!=0);
			sync1 =	(sync1<<1) | ReadBit();
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
		const BYTE preamble[]={ 0xa1, 0xa1, 0xa1, MFM::DecodeByte(w) };
		TFdcStatus result;
		switch (preamble[3]){ // branching on observed data address mark
			case 0xfb: case 0xfa: // normal data (+alt)
				result=TFdcStatus::WithoutError;
				break;
			case 0xf8: case 0xf9: // deleted data (+alt)
				result=TFdcStatus::DeletedDam;
				break;
			default:
				return TFdcStatus::NoDataField; // not the expected Data mark
		}
		if (pOutParseEvents)
			*pOutParseEvents++=TParseEvent( TParseEvent::MARK_1BYTE, tEventStart, currentTime, preamble[3] );
		// - reading specified amount of Bytes into the Buffer
		tEventStart=currentTime;
		PBYTE p=buffer;
		while (*this && nBytesToRead-->0){
			if (!ReadBits16(w)){ // Track end encountered
				result.ExtendWith( TFdcStatus::DataFieldCrcError );
				break;
			}
			*p++=MFM::DecodeByte(w);
		}
		if (!*this)
			return result;
		if (pOutParseEvents)
			*pOutParseEvents++=TParseEvent( TParseEvent::DATA, tEventStart, currentTime, p-buffer );
		// - reading and comparing Data Field's CRC
		tEventStart=currentTime;
		DWORD dw;
		CFloppyImage::TCrc16 crc=0;
		const bool crcBad=!ReadBits32(dw) || MFM::DecodeBigEndianWord(dw)!=( crc=CFloppyImage::GetCrc16Ccitt(CFloppyImage::GetCrc16Ccitt(&preamble,sizeof(preamble)),buffer,p-buffer) ); // no or wrong Data Field CRC
		if (crcBad)
			result.ExtendWith( TFdcStatus::DataFieldCrcError );
		if (pOutParseEvents)
			*pOutParseEvents++=TParseEvent( crcBad?TParseEvent::CRC_BAD:TParseEvent::CRC_OK, tEventStart, currentTime, crc );
		return result;
	}













	void CImage::CTrackReader::TProfile::Reset(){
		iwTime=iwTimeDefault;
	}












	CImage::CTrackReaderWriter::CTrackReaderWriter(DWORD nLogTimesMax,TDecoderMethod method)
		// ctor
		: CTrackReader( (PLogTime)::calloc(nLogTimesMax+1,sizeof(TLogTime)), 0, nullptr, 0, TMedium::FLOPPY_DD_350, TCodec::MFM, method ) // "+1" = hidden item represents reference counter
		, nLogTimesMax(nLogTimesMax) {
	}

	CImage::CTrackReaderWriter::CTrackReaderWriter(const CTrackReaderWriter &rTrackReaderWriter)
		// copy ctor
		: CTrackReader( rTrackReaderWriter )
		, nLogTimesMax( rTrackReaderWriter.nLogTimesMax ) {
	}
	
	CImage::CTrackReaderWriter::CTrackReaderWriter(CTrackReaderWriter &&rTrackReaderWriter)
		// move ctor
		: CTrackReader( std::move(rTrackReaderWriter) )
		, nLogTimesMax( rTrackReaderWriter.nLogTimesMax ) {
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

	void CImage::CTrackReaderWriter::Normalize(TLogTime correctIndexDistance){
		// places neighboring Indices exactly specified Distance away, interpolating the Track timing in between
		// - in-place interpolation
		const PCLogTime pLast=logTimes+nLogTimes;
		if (correctIndexDistance>0){
			RewindToIndex(0);
			PLogTime pTime=logTimes+iNextTime;
			TLogTime orgRevStart=currentTime; // revolution start before modification
			for( BYTE i=1; i<nIndexPulses; i++ ){
				const TLogTime orgRevEnd=GetIndexTime(i); // revolution end before modification
					const TLogTime orgIndexDistance=orgRevEnd-orgRevStart;
					while (pTime<pLast && *pTime<=orgRevEnd){
						*pTime = currentTime+(LONGLONG)(*pTime-orgRevStart)*correctIndexDistance/orgIndexDistance;
						pTime++;
					}
				orgRevStart=orgRevEnd;
				currentTime = indexPulses[i] = indexPulses[i-1]+correctIndexDistance;
			}
			for( const TLogTime dt=currentTime-orgRevStart; pTime<pLast; *pTime+++=dt ); // offsetting the remainder of the Track
		}
		// - correctly re-initializing this object's state
		SetCurrentTime(0);
	}
