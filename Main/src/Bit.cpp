#include "stdafx.h"

namespace Bit
{
	CSequence::CSequence()
		// ctor
		: nBits(0) {
	}

	CSequence::CSequence(CTrackReader &tr,N nBitsFromCurrTime,BYTE oneOkPercent)
		// ctor
		// - initialization
		: nBits(0) {
		// - create and populate the BitBuffer
		bitBuffer.Realloc( 1+nBitsFromCurrTime+2 )->time=tr.GetCurrentTime(); // "1+" = one hidden Bit before Sequence (with negative Time), "+2" = auxiliary terminal Bits
		pBits=bitBuffer+1; // skip that one hidden Bit
		TBit *p=pBits;
		for( TLogTime tOne; nBits<nBitsFromCurrTime; nBits++ ){
			p->flags=0;
			p->value=tr.ReadBit(tOne);
			p->time=tr.GetCurrentTime();
			if (oneOkPercent && p->value){ // only windows containing "1" are evaluated as for timing
				const TLogTime iwTimeHalf=tr.GetCurrentProfile().iwTime/2;
				const TLogTime absDiff=std::abs(tOne-tr.GetCurrentTime());
				//ASSERT( absDiff <= iwTimeHalf+1 ); // "+1" = when IwTime is odd, e.g. 1665, half of which is 833, not 832
				p->badTiming=absDiff*100>iwTimeHalf*oneOkPercent;
			}
			p->badEncoding=!tr.IsLastReadBitHealthy();
			p+= p[-1].time<p->time; // may not be the case if Decoder went over Index and got reset
		}
		p->time=p[-1].time; // auxiliary terminal Bit
		p[1].time=Time::Infinity;
		nBits=p-pBits; // # of Bits may in the end be lower due to dropping of Bits over Indices
	#ifdef _DEBUG
		TLogTime tPrev=Time::Invalid;
		for each( const auto &bit in *this ){
			ASSERT( tPrev<bit.time );
			tPrev=bit.time;
		}
	#endif
	}

	CSequence::CSequence(const CSequence &base,const TLogTimeInterval &ti)
		// ctor (subsequence)
		: bitBuffer(base.bitBuffer)
		, pBits( const_cast<TBit *>(base.Find(ti.tStart)) )
		, nBits( base.Find(ti.tEnd)-pBits ) {
		if (pBits<base.pBits){ // mustn't leave the the range of Base Sequence (e.g. mustn't include hidden Bits)
			nBits-=base.pBits-pBits;
			pBits=base.pBits;
		}
	}

	CSequence::TData<WORD> CSequence::GetWord(int i) const{
		// 
		TData<WORD> result;
		i*=sizeof(result.value)*CHAR_BIT;
		const N iByteEnd=i+sizeof(result.value)*CHAR_BIT;
		if (nBits<iByteEnd) // insufficient # of Bits ?
			return result;
		while (i<iByteEnd){
			const auto &bit=pBits[i++];
			result.value<<=1, result.value|=bit.value;
			result.flags|=bit.flags;
		}
		result.time=pBits[i-1].time;
		result.Validate();
		return result;
	}

	CSequence::PCBit CSequence::Find(TLogTime t) const{
		// returns the Bit containing the specified LogicalTime
		auto it=std::lower_bound(
			begin(), end(), t,
			[](const TBit &b,TLogTime t){ return b.time<t; }
		);
		it-=t<it->time; // unless LogicalTime perfectly aligned with one of Bits, 'lower_bound' finds the immediatelly next Bit - hence going back to the Bit that contains the LogicalTime
		return it;
	}

	CSequence::PCBit CSequence::FindOrNull(TLogTime t) const{
		// returns the Bit containing the specified LogicalTime
		const auto it=Find(t);
		return	it!=end() ? it : nullptr;
	}

	CSharedDiffScript CSequence::GetShortestEditScript(const CSequence &theirs,CActionProgress &ap) const{
		// creates and returns the shortest edit script (SES)
		CSharedDiffScript ses( GetBitCount()+theirs.GetBitCount() );
		//if (!ses) // commented out as MFC CString doesn't check memory allocation failures, hence we would have already crashed anyway
			//return ses;
		const int i=CDiff<const TBit>(
			begin(), GetBitCount()
		).GetShortestEditScript(
			theirs.begin(), theirs.GetBitCount(),
			ses, ses.length,
			ap
		);
		if (i<0) // e.g. Action cancelled
			ses.reset();
		else
			ses.length=i;
		return ses;
	}

	Time::CSharedColorIntervalArray CSequence::ScriptToLocalDiffs(const CSharedDiffScript &script) const{
		// composes Regions of differences that timely match with bits observed in this BitSequence (e.g. for visual display by the caller)
		const Time::CSharedColorIntervalArray result(script.length);
		for( auto i=script.length; i-->0; ){
			const auto &si=script[i];
			auto &rDiff=result[i];
			rDiff.tStart=pBits[si.iPosA].time;
			switch (si.operation){
				case CDiffBase::TScriptItem::INSERTION:
					// "theirs" contains some extra bits that "this" misses
					rDiff.color=0xb4; // tinted red
					rDiff.tEnd=pBits[std::min( si.iPosA+1, nBits )].time; // "+1" = even Insertions must be represented locally!
					pBits[si.iPosA].cosmeticFuzzy=true;
					break;
				default:
					ASSERT(FALSE); // we shouldn't end up here!
					return result.GetEmpty();
				case CDiffBase::TScriptItem::DELETION:
					// "theirs" misses some bits that "this" contains
					rDiff.color=0x5555ff; // another tinted red
					N iDeletionEnd=si.iPosA+si.del.nItemsA;
					if (iDeletionEnd+1<nBits){ // "+1" = see above Insertion (only for cosmetical reasons)
						rDiff.tEnd=pBits[iDeletionEnd+1].time;
						pBits[iDeletionEnd].cosmeticFuzzy=true;
					}else
						rDiff.tEnd=pBits[iDeletionEnd].time;
					while (iDeletionEnd>si.iPosA)
						pBits[--iDeletionEnd].fuzzy=true;
					break;
			}
		}
		return result;
	}

	Time::CSharedColorIntervalArray CSequence::ScriptToLocalRegions(const CSharedDiffScript &script,COLORREF regionColor) const{
		// composes Regions of differences that timely match with bits observed in this BitSequence (e.g. for visual display by the caller); returns the number of unique Regions
		auto &&diffs=ScriptToLocalDiffs(script);
		TLogTime tLastRegionEnd=Time::Invalid;
		CSharedDiffScript::N nRegions=0;
		for each( const auto &d in diffs ){
			if (d.tStart>tLastRegionEnd){
				// disjunct Diffs - create a new Region
				auto &rRgn=diffs[nRegions++];
					rRgn.color=regionColor;
					rRgn.tStart=d.tStart;
				tLastRegionEnd = rRgn.tEnd = d.tEnd;
			}else
				// overlapping BadRegions (Diff: something has been Deleted, something else has been Inserted)
				tLastRegionEnd = diffs[nRegions-1].tEnd = std::max(tLastRegionEnd,d.tEnd);
		}
		diffs.length=nRegions;
		return diffs;
	}

	void CSequence::InheritFlagsFrom(const CSequence &theirs,const CSharedDiffScript &script) const{
		//
		typedef CSharedDiffScript::N N;
		N iMyBit=0, iTheirBit=0;
		N iScriptItem=0;
		do{
			// . inheriting Flags from Bits identical up to the next ScriptItem
			const auto &si=script[iScriptItem]; // below never touched when index invalid ...
			const N iDiffStartPos= iScriptItem<script.length ? si.iPosA : nBits; // ... like here
			N nIdentical=std::min( iDiffStartPos-iMyBit, theirs.nBits-iTheirBit );
			while (nIdentical-->0){
				#ifdef _DEBUG
					const auto &mine=pBits[iMyBit], &their=theirs.pBits[iTheirBit];
					ASSERT( mine.value==their.value ); // just to be sure; failing here may point at a bug in Diff implementation!
				#endif
				pBits[iMyBit++].flags|=theirs.pBits[iTheirBit++].flags;
			}
			// . if no more differences, then we have just processed the common tail Bits
			if (iScriptItem==script.length)
				break;
			// . skipping Bits that are different
			switch (si.operation){
				case CDiffBase::TScriptItem::INSERTION:
					// "theirs" contains some extra bits that "this" misses
					iTheirBit+=si.op.nItemsB;
					break;
				case CDiffBase::TScriptItem::DELETION:
					// "theirs" misses some bits that "this" contains
					iMyBit+=si.op.nItemsB;
					break;
				default:
					ASSERT(FALSE); // we shouldn't end up here!
					return;
			}
			iScriptItem++;
		} while(true);
	}

	void CSequence::OffsetAll(TLogTime dt) const{
		// offsets each Bit by given constant
		for each( TBit &p in bitBuffer ) // offset also the padding Bits at the beginning and end
			p.time+=dt;
	}

#ifdef _DEBUG
	void CSequence::SaveCsv(LPCTSTR filename) const{
		CFile f( filename, CFile::modeWrite|CFile::modeCreate );
		for( N b=0; b<nBits; b++ )
			Utils::WriteToFileFormatted( f, "%c, %d\n", '0'+pBits[b].value, pBits[b].time );
	}
#endif

}
