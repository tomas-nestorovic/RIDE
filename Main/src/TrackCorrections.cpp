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
		, cellCountPerRevolution(true)
		, fitTimesIntoIwMiddles(true)
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
				tmp=c.cellCountPerRevolution;
					DDX_Check( pDX, ID_NUMBER, tmp );
				c.cellCountPerRevolution=tmp!=BST_UNCHECKED;
				tmp=c.fitTimesIntoIwMiddles;
					DDX_Check( pDX, ID_ACCURACY, tmp );
				c.fitTimesIntoIwMiddles=tmp!=BST_UNCHECKED;
				tmp=c.offsetIndices;
					DDX_Check( pDX, ID_ADDRESS, tmp );
				c.offsetIndices=tmp!=BST_UNCHECKED;
				tmp=c.indexOffsetMicroseconds;
					DDX_Text( pDX, ID_TIME, tmp );
						DDV_MinMaxInt( pDX, tmp, SHRT_MIN, SHRT_MAX );
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





	TStdWinError CReaderWriter::Apply(const TCorrections &c){
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
		if (c.offsetIndices){
			TLogTime dt=TIME_MICRO(c.indexOffsetMicroseconds);
			if (dt<0)
				dt=std::max( *indexPulses+dt, 0 )-*indexPulses; // mustn't run into negative timing
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
			if (c.fitTimesIntoIwMiddles){
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
			if (c.cellCountPerRevolution){
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
			if (c.indexTiming) // index-to-index time correction enabled?
				indexPulses[nextIndex]=indexPulses[nextIndex-1]+mp->revolutionTime;
			const TLogTime tNextIndexWork =	nAlignedCells>0 // are we working with time-corrected cells?
											? tCurrIndexOrg+nAlignedCells*profile.iwTimeDefault
											: tNextIndexOrg;
			if (tCurrIndexOrg!=indexPulses[nextIndex-1] || tNextIndexWork!=indexPulses[nextIndex])
				Time::Interpolate(
					ptModified+iModifRevStart, iModifRevEnd-iModifRevStart,
					tCurrIndexOrg, tNextIndexWork,
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
		#ifdef _DEBUG
			VerifyChronology();
		#endif
		return ERROR_SUCCESS;
	}

}
