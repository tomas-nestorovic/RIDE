#include "stdafx.h"

namespace Track
{
	TCorrections::TCorrections()
		// ctor of "no" Corrections
		: w(0) , indexOffsetMicroseconds(0) {
	}
	
	TCorrections::TCorrections(LPCTSTR iniSection,LPCTSTR iniName)
		// ctor
		// - the defaults
		: use(false)
		, indexTiming(true)
		, nominalIwSize(true)
		, jitter(true)
		, offsetIndices(false)
		, indexOffsetMicroseconds(1500) {
		// - attempting to load existing values from last session
		static_assert( sizeof(*this)==sizeof(int), "" );
		if (const int settings=app.GetProfileInt(iniSection,iniName,0)) // do Valid settings exist?
			*(PINT)this=settings;
	}

	void TCorrections::Save(LPCTSTR iniSection,LPCTSTR iniName) const{
		// dtor
		static_assert( sizeof(*this)==sizeof(int), "" );
		app.WriteProfileInt( iniSection, iniName, *(PINT)this );
	}

	bool TCorrections::ShowModal(CWnd *pParentWnd){
		// shows a dialog with exposed settings
		// - defining the Dialog
		class CCorrectionsDialog sealed:public Utils::CRideDialog{
			void DoDataExchange(CDataExchange *pDX) override{
				__super::DoDataExchange(pDX);
				int tmp=c.indexTiming;
					DDX_Check( pDX, ID_ALIGN,	tmp );
				c.indexTiming=tmp!=BST_UNCHECKED;
				tmp=c.nominalIwSize;
					DDX_Check( pDX, ID_NUMBER, tmp );
				c.nominalIwSize=tmp!=BST_UNCHECKED;
				tmp=c.jitter;
					DDX_Check( pDX, ID_ACCURACY, tmp );
				c.jitter=tmp!=BST_UNCHECKED;
				tmp=c.offsetIndices;
					DDX_Check( pDX, ID_ADDRESS, tmp );
				c.offsetIndices=tmp!=BST_UNCHECKED;
				tmp=c.indexOffsetMicroseconds;
					DDX_Text( pDX, ID_TIME, tmp );
						DDV_MinMaxInt( pDX, tmp, Time::Invalid16, Time::Infinity16 );
				c.indexOffsetMicroseconds=tmp;
			}
		public:
			TCorrections c;

			CCorrectionsDialog(const TCorrections &c,CWnd *pParentWnd)
				: Utils::CRideDialog( IDR_TRACK_CORRECTIONS, pParentWnd )
				, c(c) {
			}
		} d( *this, pParentWnd );
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			*this=d.c;
			return true;
		}else
			return false;
	}





	TStdWinError CReaderWriter::Apply(const Medium::TProperties &mp,TCorrections c){
		// True <=> all Revolutions of this Track successfully normalized using specified parameters, otherwise False
return ERROR_SUCCESS; // temporarily suspended
		SetMedium(mp); // assert everything correctly set up
		// - do nothing if Corrections disabled
		if (!c.use)
			return ERROR_SUCCESS;
		// - mustn't apply corrections twice
		if (corrected)
			return ERROR_SUCCESS;
		//ASSERT( pLogTimesInfo.GetData()->nRefs==1 ); // normalization of a TrackReaderWriter that is used more than once always needs an attention
		// - if the Track contains less than two Indices, we are successfully done
		if (indexPulses.length<2)
			return ERROR_SUCCESS;
		ClearAllMetaData();
		rawDeviceData.reset(); // modified Track is no longer as we received it from the Device
		corrected=true;
		// - shifting Indices
		const TLogTime tLastIndexOrg=indexPulses.Last();
		if (c.offsetIndices){
			const TLogTime dt=TIME_MICRO(c.indexOffsetMicroseconds);
			const TLogTime dtMin= -*indexPulses; // notice the minus sign!
			indexPulses.Offset(
				std::max( dt, dtMin ) // avoid running into negative Times
			);
		}
		// - ignoring what's before the first Index
		PLogTime ptCorrected=logTimes
			.Fork() // guaranteed to suffice (for it sufficed before and the # of Times shall be equal or smaller), thus can avoid calling 'Append' by directly modifying the content
			.LowerBound( *indexPulses, std::less<Time::T>() );
		// - normalization
		const Time::CSharedArray indexPulsesOrg=indexPulses;
		indexPulses.Fork();
		const auto &&bits=CreateFullRevBitSequences();
		Time::T tRightIndexDistance=mp.revolutionTime, *ptEnd=logTimes.end();
		for( TRev iRev=0; iRev<bits.revs.nFull; iRev++ ){
			const auto &rev=bits.revs[iRev];
			const PLogTime ptCorrectedA=ptCorrected;
			const Time::T tCurrIndex=indexPulses[iRev], dtIndex=tCurrIndex-indexPulsesOrg[iRev];
			// . jitter suppression
			if (c.jitter){
				if (c.nominalIwSize){ // want resize Bits to their Medium nominal ?
					auto *pBit=rev.begin();
					auto *const pLast= c.indexTiming // want correct Index distance ?
						? pBit+std::min( mp.nCells, rev.GetBitCount() )
						: rev.end();
					Time::T t=tCurrIndex;
					while (pBit<pLast){ // suppress jitter
						if (pBit++->value)
							*ptCorrected++=t;
						t+=mp.cellTime;
					}
					indexPulses[iRev+1]= c.indexTiming // force/correct Index distance
						? tCurrIndex+mp.revolutionTime
						: t;
					tRightIndexDistance=0; // next Index position just set, don't do it twice
				}else // suppress jitter with regard to current inspection
					for each( const auto &bit in rev ) // suppress jitter
						if (bit.value)
							*ptCorrected++=bit.time+dtIndex;
			}else{ // want preserve actual Timing
				ptCorrected=logTimes.LowerBound( ptCorrected, ptEnd, indexPulsesOrg[iRev+1], std::less<Time::T>() );
				Time::Offset( ptCorrectedA, ptCorrected, dtIndex );
			}
			// . Index distance
			if (c.indexTiming)
				if (tRightIndexDistance) // Index distance NOT YET corrected above ?
					Time::Interpolate( // correct Index distance
						ptCorrectedA, ptCorrected,
						tCurrIndex, indexPulsesOrg[iRev+1]+dtIndex,
						tCurrIndex,
							indexPulses[iRev+1]=tCurrIndex+tRightIndexDistance // correct next Index position
					);
		}
		// - ignoring what's past the last Index
		const PCLogTime ptPast=logTimes.LowerBound( ptCorrected, ptEnd, indexPulsesOrg[bits.revs.nFull], std::less<Time::T>() );
		const auto nBytes=(INT_PTR)ptEnd-(INT_PTR)ptPast;
		Time::Offset(
			ptCorrected,
			ptEnd = PLogTime( (PBYTE)::memcpy(ptCorrected,ptPast,nBytes)+nBytes ),
			indexPulses[bits.revs.nFull]-indexPulsesOrg[bits.revs.nFull]
		);
		logTimes.length=ptEnd-logTimes.begin();
		// - successfully normalized
		#ifdef _DEBUG
			VerifyChronology();
		#endif
		SetCurrentTime(0); // setting valid state
		return ERROR_SUCCESS;
	}

}
