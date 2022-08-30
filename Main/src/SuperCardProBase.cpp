#include "stdafx.h"
#include "CapsBase.h"
#include "SuperCardProBase.h"

	#define INDEX_CLOCK_TIME	TIME_NANO(25)

	CSuperCardProBase::CSuperCardProBase(PCProperties properties,char realDriveLetter,LPCTSTR iniSection,LPCTSTR firmware)
		// ctor
		// - base
		: CCapsBase( properties, realDriveLetter, true, iniSection )
		// - initialization
		, paramsEtc(iniSection)
		, firmware(firmware) {
		// - setting a classical 5.25" floppy geometry
		capsImageInfo.maxcylinder=FDD_CYLINDERS_HD/2+FDD_CYLINDERS_EXTRA - 1; // "-1" = inclusive!
		capsImageInfo.maxhead=2-1; // inclusive!
	}







	#define INI_FIRMWARE_FILE			_T("fw")

	CSuperCardProBase::TParamsEtc::TParamsEtc(LPCTSTR iniSection)
		// ctor
		: iniSection(iniSection)
		// - persistent (saved and loaded)
		, firmwareFileName( app.GetProfileString(iniSection,INI_FIRMWARE_FILE) )
		// - volatile (current session only)
		//none
		 {
	}

	CSuperCardProBase::TParamsEtc::~TParamsEtc(){
		// dtor
		app.WriteProfileString( iniSection, INI_FIRMWARE_FILE, firmwareFileName );
	}

	bool CSuperCardProBase::EditSettings(bool initialEditing){
		// True <=> new settings have been accepted (and adopted by this Image), otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return params.EditInModalDialog( *this, firmware, initialEditing );
	}







	#define REVISION		0x24
	#define SIGNATURE_REV	( 'S' | 'C'<<8 | 'P'<<16 | REVISION<<24 )

	CSuperCardProBase::THeader::THeader(){
		// ctor
		::ZeroMemory( this, sizeof(*this) );
		signatureAndRevision=SIGNATURE_REV;
		flags.modifiable=true, nAvailableRevolutions=1;
		flags.indexAligned=true; // all new Images shall use real hardware index pulses
		flags.tpi96=true; // assume modern 80-track drive on this PC
	}

	bool CSuperCardProBase::THeader::IsValid() const{
		return	( (signatureAndRevision^SIGNATURE_REV)&0xffffff )==0; // the "SCP" signature?
	}

	bool CSuperCardProBase::THeader::IsSupported() const{
		return	revision<=REVISION // same or older version?
				&&
				1<=nAvailableRevolutions && nAvailableRevolutions<=Revolution::MAX
				&&
				firstTrack<=168 && lastTrack<=168
				&&
				( !nFluxCellBits || nFluxCellBits==8 || nFluxCellBits==16 )
				&&
				heads<=THeader::HEAD_1_ONLY
				&&
				flags.indexAligned; // given how SuperCard Pro board handles Revolutions if this flag cleared, such Images are NOT supported at the moment
	}






	#define TRK_SIGNATURE	( 'T' | 'R'<<8 | 'K'<<16 )

	CSuperCardProBase::TTrackDataHeader::TTrackDataHeader(BYTE trackNumber){
		// ctor
		*(PDWORD)signature = TRK_SIGNATURE | trackNumber<<24;
	}

	bool CSuperCardProBase::TTrackDataHeader::IsTrack(BYTE trackNumber) const{
		// True <=> described is the specified TrackNumber, otherwise False
		return	*(PDWORD)signature==( TRK_SIGNATURE | trackNumber<<24 );
	}

	bool CSuperCardProBase::TTrackDataHeader::Read(CFile &fSeeked,TCylinder cyl,THead head,BYTE nAvailableRevolutions){
		// True <=> successfully read this TrackDataHeader structure from the File, otherwise False
		if (fSeeked.Read( this, sizeof(DWORD) )!=sizeof(DWORD)) // assumed File already seeked to the position containing the TrackDataHeader structure
			return false; // eof = Track doesn't exist
		if (!IsTrack( cyl*2+head ))
			return false; // bad position in File = Track doesn't exist
		if (fSeeked.Read( revolutions, nAvailableRevolutions*sizeof(TRevolution) )!=nAvailableRevolutions*sizeof(TRevolution))
			return false; // eof = Track doesn't exist
		return true;
	}

	void CSuperCardProBase::TTrackDataHeader::Write(CFile &fSeeked,BYTE nAvailableRevolutions) const{
		// writes this TrackDataHeader structure to the File and returns the actual number of Bytes written (zero if error)
		fSeeked.Write( this, sizeof(DWORD) ); // assumed File already seeked to the position to write the TrackDataHeader structure to
		fSeeked.Write( revolutions, nAvailableRevolutions*sizeof(TRevolution) );
	}

	const CSuperCardProBase::TRevolution &CSuperCardProBase::TTrackDataHeader::GetLastRevolution(BYTE nAvailableRevolutions) const{
		// determines and returns the last Revolution (which may not necessarily be the last Revolution in the list!)
		const TRevolution *pLastRev=revolutions;
		for( BYTE r=1; r<nAvailableRevolutions; r++ ){
			const TRevolution &ri=revolutions[r];
			if (ri.iFluxDataBegin>pLastRev->iFluxDataBegin)
				pLastRev=&ri;
		}
		return *pLastRev;
	}

	BYTE CSuperCardProBase::TTrackDataHeader::GetDistinctRevolutionCount(BYTE nAvailableRevolutions,PBYTE pOutUniqueRevs) const{
		// counts and returns how many UNIQUE revolutions are described (
		CMapPtrToWord distinctRevolutions; // key = Revolution, value = unused; a Revolution may be duplicated if, for instance, RIDE (or any other app) previously wrote a single Revolution multiple times into a read-only SCP (tweaking) to comply with the # of Revolutions specified in the Header
		for( BYTE r=0; r<nAvailableRevolutions; r++ ){
			const PVOID key=(PVOID)revolutions[r].iFluxDataBegin;
			WORD value;
			if (!distinctRevolutions.Lookup( key, value )){
				distinctRevolutions[key]=0;
				if (pOutUniqueRevs)
					*pOutUniqueRevs++=r;
			}
		}
		return distinctRevolutions.GetCount();
	}

	DWORD CSuperCardProBase::TTrackDataHeader::GetFullTrackLengthInBytes(const THeader &header) const{
		// determines and returns the # of Bytes that this Track occupies in the underlying file
		const TRevolution &lastRev=GetLastRevolution( header.nAvailableRevolutions );
		switch (header.nFluxCellBits){
			case 8:
				return lastRev.iFluxDataBegin+sizeof(BYTE)*lastRev.nFluxes;
			case 0:
			case 16:
				return lastRev.iFluxDataBegin+sizeof(WORD)*lastRev.nFluxes;
			default:
				ASSERT(FALSE);
				return 0;
		}
	}

	DWORD CSuperCardProBase::TTrackDataHeader::GetFullTrackCapacityInBytes(Medium::PCProperties mp,const THeader &header) const{
		// determines and returns the capacity of a buffer to contain an average Track with natural/sensible data
		const DWORD fluxCapacityPerRevolution= header.flags.modifiable&&mp ? mp->nCells/100*50 : 0; // buffer consisting of 50% of nominal # of cells per Revolution (experimentally determined using natural/sensible data with MFM encoding)
		const DWORD fluxTotalCapacity=GetDistinctRevolutionCount(header.nAvailableRevolutions)*fluxCapacityPerRevolution;
		if (const DWORD trackLength=GetFullTrackLengthInBytes(header))
			switch (header.nFluxCellBits){
				case 8:
					return	std::max( sizeof(*this)+sizeof(BYTE)*fluxTotalCapacity, trackLength );
				case 0:
				case 16:
					return	std::max( sizeof(*this)+sizeof(WORD)*fluxTotalCapacity, trackLength );
			}
		ASSERT(FALSE); // we shouldn't end up here!
		return 0;
	}







	CImage::CTrackReaderWriter CSuperCardProBase::StreamToTrack(CFile &f,TCylinder cyl,THead head) const{
		// creates and returns a Track representation of the input TrackDataHeader structure and fluxes
		const auto fBasePos=f.GetPosition();
		// - extracting the specified TrackDataHeader from the SCP file
		TTrackDataHeader tdh(0);
		if (!tdh.Read( f, cyl, head, header.nAvailableRevolutions ))
			return CTrackReaderWriter::Invalid; // read invalid TrackDataHeader structure
		BYTE iUniqueRevolutions[Revolution::MAX];
		const BYTE nUniqueRevolutions=tdh.GetDistinctRevolutionCount( header.nAvailableRevolutions, iUniqueRevolutions );
		// - construction of internal representation of the Fluxes
		const TLogTime sampleClockTime=header.GetSampleClockTime();
		DWORD nFluxesTotally=0;
		for( BYTE u=nUniqueRevolutions; u>0; nFluxesTotally+=tdh.revolutions[iUniqueRevolutions[--u]].nFluxes );
		CTrackReaderWriter result( nFluxesTotally*125/100, params.GetGlobalFluxDecoder(), params.resetFluxDecoderOnIndex ); // allowing for 25% of false "ones" introduced by "FDC-like" decoders
		if (header.flags.indexAligned) // if NOT index-aligned, there is no information on original index pulses as the disk was revolving (based on the drive RPM information, the SCP device just slices whatever fluxes have been read into 200/166ms intervals, thus merely "imitating" indices) - placing here any Indices thus makes no sense
			result.AddIndexTime(0);
		for( BYTE u=0; u<nUniqueRevolutions; u++ ){
			// . fluxes
			const auto &ri=tdh.revolutions[iUniqueRevolutions[u]];
			f.Seek( fBasePos+ri.iFluxDataBegin, CFile::begin );
			switch (header.nFluxCellBits){
				case 8:
					if (const auto fluxes=Utils::MakeCallocPtr<BYTE>(ri.nFluxes))
						if (f.Read( fluxes, ri.nFluxes*sizeof(BYTE) )==ri.nFluxes*sizeof(BYTE)){
							TLogTime t=result.GetLastIndexTime();
							for( DWORD i=0; i<ri.nFluxes; i++ )
								if (const auto &sampleCount=fluxes[i])
									result.AddTime( t+=sampleCount*sampleClockTime );
								else // sample counter overrun (e.g. "unformatted area" copy-protection)
									t+=256*sampleClockTime;
							break;
						}
					return CTrackReaderWriter::Invalid;
				case 0:
				case 16:
					if (const auto fluxes=Utils::MakeCallocPtr<Utils::CBigEndianWord>(ri.nFluxes))
						if (f.Read( fluxes, ri.nFluxes*sizeof(WORD) )==ri.nFluxes*sizeof(WORD)){
							TLogTime t=result.GetLastIndexTime();
							for( DWORD i=0; i<ri.nFluxes; i++ )
								if (const auto &sampleCount=fluxes[i])
									result.AddTime( t+=sampleCount*sampleClockTime );
								else // sample counter overrun (e.g. "unformatted area" copy-protection)
									t+=65536*sampleClockTime;
							break;
						}
					return CTrackReaderWriter::Invalid;
				default:
					ASSERT(FALSE);
					return CTrackReaderWriter::Invalid;
			}
			// . index
			if (header.flags.indexAligned) // if NOT index-aligned, there is no information on original index pulses as the disk was revolving (based on the drive RPM information, the SCP device just slices whatever fluxes have been read into 200/166ms intervals, thus merely "imitating" indices) - placing here any Indices thus makes no sense
				result.AddIndexTime(
					std::max<TLogTime>( // early Greaseweazle versions may report that the full Revolution time is actually smaller than the sum of all flux transitions in the Revolution!
						result.GetLastIndexTime()+ri.durationCounter*INDEX_CLOCK_TIME,
						result.GetTotalTime()
					)
				);
		}
		return result;
	}

	DWORD CSuperCardProBase::TrackToStream(CTrackReader tr,CFile &f,TCylinder cyl,THead head,bool &rOutAdjusted) const{
		// converts specified Track representation into Stream data and returns the length of the Stream
		rOutAdjusted=false; // assumption (no adjustments needed to overcome SuperCard Pro limitations)
		if (tr.GetIndexCount()<2*header.flags.indexAligned)
			return 0;
		const auto fBasePos=f.GetPosition();
		// - composition of a TrackDataHeader structure (to be continued below...)
		TTrackDataHeader tdh( cyl*2+head );
		tdh.Write( f, header.nAvailableRevolutions ); // just to correctly advance the position in the File
		// - writing Fluxes
		const TLogTime sampleClockTime=header.GetSampleClockTime();
		BYTE r=0; // TRevolutionInfo index
		TLogTime tBase= header.flags.indexAligned ? tr.GetIndexTime(0) : 0;
		for( BYTE u=header.flags.indexAligned,const iLastIndex=std::min<BYTE>(header.nAvailableRevolutions+u,tr.GetIndexCount()); u<iLastIndex; u++ ){
			// . writing Fluxes of current Revolution
			tr.SetCurrentTime( tBase );
			const TLogTime tNext=tr.GetIndexTime(u); // base Time for the next Revolution
			const auto fRevBasePos=f.GetPosition();
			DWORD prevSampleClockCounter=tBase/sampleClockTime;
			while (tr && tr.GetCurrentTime()<tNext){
				const TLogTime t=tr.ReadTime();
				const DWORD sampleClockCounter=t/sampleClockTime;
				if (sampleClockCounter<=prevSampleClockCounter){ // just to be sure (Time shouldn't go backwards)
					ASSERT(FALSE); // we shouldn't end up here!
					continue;
				}
				DWORD nDeltaSamples=sampleClockCounter-prevSampleClockCounter;
				static constexpr DWORD SampleCounterOverflow=0;
				switch (header.nFluxCellBits){
					case 8:
						for( ; nDeltaSamples>=256; nDeltaSamples-256 )
							f.Write( &SampleCounterOverflow, sizeof(BYTE) );
						if (nDeltaSamples)
							f.Write( &nDeltaSamples, sizeof(BYTE) );
						else{
							f.Write( &(nDeltaSamples=1), sizeof(BYTE) ); // mustn't generate another overflow indicator!
							rOutAdjusted=true;
						}
						break;
					case 0:
					case 16:
						for( ; nDeltaSamples>=65536; nDeltaSamples-65536 )
							f.Write( &SampleCounterOverflow, sizeof(WORD) );
						if (nDeltaSamples)
							f.Write( &Utils::CBigEndianWord(nDeltaSamples), sizeof(WORD) );
						else{
							f.Write( &Utils::CBigEndianWord(1), sizeof(WORD) ); // mustn't generate another overflow indicator!
							rOutAdjusted=true;
						}
						break;
					default:
						ASSERT(FALSE);
						return CTrackReaderWriter::Invalid;
				}
				prevSampleClockCounter=sampleClockCounter;
			}
			// . registering the Revolution in the TrackDataHeader structure
			auto &ri=tdh.revolutions[r++];
				ri.durationCounter= tNext/INDEX_CLOCK_TIME - tBase/INDEX_CLOCK_TIME; // more precise than "(tNext-tBase)/INDEX_CLOCK_TIME"
				switch (header.nFluxCellBits){
					case 8:
						ri.nFluxes=( f.GetPosition()-fRevBasePos )/sizeof(BYTE);
						break;
					case 0:
					case 16:
						ri.nFluxes=( f.GetPosition()-fRevBasePos )/sizeof(WORD);
						break;
					default:
						ASSERT(FALSE);
						return CTrackReaderWriter::Invalid;
				}
				ri.iFluxDataBegin=fRevBasePos-fBasePos;
			// . next Revolution
			tBase=tNext;
		}
		// - repetition of *FULL* Revolutions (e.g. tweaking read-only images)
		for( BYTE u=!header.flags.indexAligned,const nUniqueRevs=r; r<header.nAvailableRevolutions; r++ ){
			tdh.revolutions[r]=tdh.revolutions[u];
			if (++u==nUniqueRevs)
				u=!header.flags.indexAligned;
		}
		// - composition of a TrackDataHeader structure (continued)
		const auto fEndPos=f.GetPosition();
		f.Seek( fBasePos, CFile::begin );
		tdh.Write( f, header.nAvailableRevolutions );
		return f.Seek(fEndPos,CFile::begin) - fBasePos;
	}
