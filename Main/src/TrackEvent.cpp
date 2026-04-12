#include "stdafx.h"

namespace Track{
namespace Event
{
	const COLORREF TypeColors[LAST]={
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




	T::T(TType type,TLogTime tStart,TLogTime tEnd,DWORD data)
		// ctor
		: Time::TInterval( tStart, tEnd )
		, type(type) , size(sizeof(T)) {
		dw=data;
	}

	CString T::GetDescription(bool preferDecimalValues) const{
		CString desc;
		switch (type){
			case SYNC_3BYTES:
				desc.Format( _T("0x%06X sync"), dw);
				break;
			case MARK_1BYTE:
				desc.Format( _T("0x%02X mark"), dw );
				break;
			case PREAMBLE:
				desc.Format( _T("Preamble (%d Bytes)"), dw );
				break;
			case DATA_OK:
			case DATA_BAD:{
				desc.Format( _T("Data %s (%d Bytes)"), type==DATA_OK?_T("ok"):_T("bad"), dw );
				const TData &d=*(TData *)this;
				if (d.sectorId)
					desc+=_T(" for ")+d.sectorId.ToString();
				break;
			}
			case DATA_IN_GAP:
				desc.Format( _T("Gap data (circa %d Bytes)"), dw);
				break;
			case CRC_OK:
			case CRC_BAD:{
				TCHAR format[]=_T("0x%_ %s CRC");
				format[3]= preferDecimalValues ? 'd' : 'X';
				desc.Format( format+2*preferDecimalValues, dw, type==CRC_OK?_T("ok"):_T("bad") );
				break;
			}
			case NONFORMATTED:
				desc.Format( _T("Nonformatted %d.%d µs"), div((int)GetLength(),1000) );
				break;
			case FUZZY_OK:
			case FUZZY_BAD:
				desc.Format( _T("Fuzzy %d.%d µs"), div((int)GetLength(),1000) );
				break;
			default:
				return lpszMetaString;
		}
		return desc;
	}




	void TMetaString::Create(T &buffer,TLogTime tStart,TLogTime tEnd,LPCSTR lpszMetaString){
		ASSERT( lpszMetaString!=nullptr );
		buffer=T( META_STRING, tStart, tEnd, 0 );
		buffer.size =	sizeof(TMetaString)
						+
						::lstrlenA(  ::lstrcpyA( buffer.lpszMetaString, lpszMetaString )  ) // caller responsible for allocating enough buffer
						+
						1 // "+1" = including terminal Null character
						-
						sizeof(buffer.lpszMetaString); // already counted into Size
	}




	TData::TData(const TSectorId &sectorId,TLogTime tStart)
		: T( NONE, tStart, 0, ARRAYSIZE(dummy) )
		, sectorId(sectorId) {
	}

	void TData::Finalize(TLogTime tEnd,const Time::Decoder::TProfile &profileEnd,Sector::L nBytes,TType type){
		ASSERT( nBytes>0 );
		::memcpy( bytes+nBytes, GetByteInfos(), nBytes*sizeof(TByteInfo) );
		static_cast<T &>(*this)=T( type, tStart, tEnd, nBytes );
		GetByteInfos()[nBytes].dtStart=tEnd-tStart; // auxiliary ByteInfo to store the end of the last Byte
		size=(PCBYTE)(dummy+nBytes+1) - (PCBYTE)this;
		this->profileEnd=profileEnd;
	}




	CList::CList(){
		// ctor
		::ZeroMemory( peTypeCounts, sizeof(peTypeCounts) );
	}

	CList::CList(CList &&r)
		// move-ctor
		: Utils::CPodList<T>( std::move(r) )
		, logStarts( std::move(r.logStarts) )
		, logEnds( std::move(r.logEnds) ) {
		::memcpy( peTypeCounts, r.peTypeCounts, sizeof(peTypeCounts) );
		::ZeroMemory( r.peTypeCounts, sizeof(r.peTypeCounts) );
		r.RemoveAll();
	}

	void CList::Add(const CSharedPtr &ptr){
		// - creating a copy of the ParseEvent
		T &pe=*ptr;
		ASSERT( pe.tStart<pe.tEnd );
		POSITION pos=static_cast<CStringList *>(this)->AddTail(ptr);
		if (pe.tStart==TimelyFromPrevious){
			GetPrev(pos);
			pe.tStart=GetAt(pos).tEnd; // the tail assumed to be the ParseEvent added previously
		}
		// - registering the ParseEvent for quick searching by Start/End time
		logStarts.insert( std::make_pair(pe.tStart,&pe) );
		logEnds.insert( std::make_pair(pe.tEnd,&pe) );
		// - increasing counter
		peTypeCounts[pe.type]++;
	}

	void CList::Add(const T &pe){
		// adds copy of the specified ParseEvent into this List
		Add( CSharedPtr(pe,pe.size) );
	}

	void CList::Add(const CList &list){
		// adds all ParseEvents to this -List
		for each( const auto &pair in list.logStarts )
			Add( *pair.second );
	}

	CList::CIterator::CIterator(const CLogTiming &logTimes,const CLogTiming::const_iterator &it)
		// ctor
		: CLogTiming::const_iterator(it)
		, logTimes(logTimes) {
	}

	CList::CIterator CList::GetIterator() const{
		return CList::CIterator( logStarts, logStarts.cbegin() );
	}

	CList::CIterator CList::GetLastByStart() const{
		CList::CIterator it( logStarts, logStarts.cend() );
		if (GetCount()>0)
			it--;
		return it;
	}

	CList::CIterator CList::TBinarySearch::Find(TLogTime tMin,TType typeFrom,TType typeTo) const{
		for( auto it=lower_bound(tMin); it!=cend(); it++ ){
			const T &pe=*it->second;
			if (pe.IsType(typeFrom,typeTo))
				return CList::CIterator( *this, it );
		}
		return CList::CIterator( *this, cend() );
	}

	CList::CIterator CList::FindByStart(TLogTime tStartMin,TType type) const{
		return	FindByStart( tStartMin, type, type );
	}

	CList::CIterator CList::FindByEnd(TLogTime tEndMin,TType type) const{
		return	FindByEnd( tEndMin, type, type );
	}

	bool CList::IntersectsWith(const Time::TInterval &ti) const{
		static_assert( std::is_same<decltype(ti.tStart),int>::value, "type must be integral" ); // ...
		if (const auto it=FindByEnd( ti.tStart+1 )) // ... otherwise use 'upper_bound' here
			return it->second->Intersect(ti);
		return false;
	}

	void CList::RemoveConsecutiveBeforeEnd(TLogTime tEndMax){
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

	TType CList::GetTypeOfFuzziness(CIterator &itContinue,const Time::TInterval &tiFuzzy,TLogTime tTrackEnd) const{
		// observing the existing ParseEvents, determines and returns the type of fuzziness in the specified Interval
		while (itContinue){
			const T &pe=*itContinue->second;
			if (tiFuzzy.tEnd<=pe.tStart)
				break;
			if (pe.IsDataStd() || pe.IsCrc())
				if (pe.Intersect(tiFuzzy))
					if ((pe.type==DATA_BAD||pe.type==CRC_BAD) // the fuzziness is in Bad Sector data ...
						&&
						pe.tEnd<tTrackEnd // ... and the data is complete (aka, it's NOT data over Index)
					)
						return FUZZY_BAD;
			itContinue++;
		}
		return FUZZY_OK; // the fuzzy Interval occurs NOT in a Bad Sector
	}

}}