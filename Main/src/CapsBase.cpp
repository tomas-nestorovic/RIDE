#include "stdafx.h"
#include "CapsBase.h"
#include "MSDOS7.h"

	static TStdWinError InitCapsLibrary(CapsVersionInfo &cvi){
		// - checking version
		if (CAPS::GetVersionInfo(&cvi,0) || cvi.release<5 || cvi.release==5&&cvi.revision<1){
			static constexpr Utils::CSimpleCommandDialog::TCmdButtonInfo CmdButtons[]={
				{ IDYES, _T("Get 32-bit version from SPS website (recommended)") }
			};
			switch (
				Utils::CSimpleCommandDialog(
					_T("CAPS library outdated, 5.1 or newer required!"),
					CmdButtons, ARRAYSIZE(CmdButtons)
				).DoModal()
			){
				case IDYES:
					app.GetMainWindow()->OpenWebPage( _T("SPS"), _T("http://www.softpres.org/download") );
					return ERROR_INSTALL_USEREXIT;
				default:
					return ERROR_EVT_VERSION_TOO_OLD;
			}
		}
		// - initializing the library
		if (CAPS::Init())
			return ERROR_DLL_INIT_FAILED;
		// - initialized successfully, can now use the library
		return ERROR_SUCCESS;
	}

	CCapsBase::CCapsBase(PCProperties properties,char realDriveLetter,bool hasEditableSettings,LPCTSTR iniSectionName)
		// ctor
		// - base
		: CFloppyImage(properties,hasEditableSettings)
		// - loading the CAPS library
		, capsLibLoadingError( ::InitCapsLibrary(capsVersionInfo) )
		// - creating a CAPS device
		, capsDeviceHandle(  capsLibLoadingError ? -1 : CAPS::AddImage()  )
		// - initialization
		, precompensation(realDriveLetter)
		, lastCalibratedCylinder(0)
		, preservationQuality(true) // mustn't change Track timings (derived Images may override otherwise)
		, informedOnPoorPrecompensation(true) // Devices override in their ctors to False
		, forcedMediumType( Medium::FLOPPY_ANY )
		, params(iniSectionName)
		, lastSuccessfullCodec(Codec::MFM) {
		::ZeroMemory( &capsImageInfo, sizeof(capsImageInfo) );
		::ZeroMemory( internalTracks, sizeof(internalTracks) );
		params.fortyTrackDrive&=properties->IsRealDevice();
	}

	CCapsBase::~CCapsBase(){
		// dtor
		// - destroying all Tracks
		Reset();
		// - unloading the CAPS library (and thus destroying all data created during this session)
		if (!capsLibLoadingError)
			CAPS::Exit();
	}










	class CCapsBitReader sealed{
		const UBYTE *pBits;
		UDWORD nBits,iCurrBit;
		const UDWORD *pByteTimes;
		UDWORD nByteTimes;
	public:
		CCapsBitReader()
			// ctor (set up to always fail)
			: iCurrBit(nBits) {
		}
		CCapsBitReader(const CapsTrackInfoT2 &cti,UDWORD lockFlags)
			// ctor
			: pBits(cti.trackbuf)
			, nBits( lockFlags&DI_LOCK_TRKBIT ? cti.tracklen : cti.tracklen*CHAR_BIT )
			, pByteTimes(cti.timebuf)
			, nByteTimes(cti.timelen)
			, iCurrBit(0) {
		}

		inline operator bool() const{ return iCurrBit<nBits; }
		inline UDWORD GetCount() const{ return nBits; }

		bool ReadBit(UDWORD &outByteTime){
			ASSERT(*this);
			const div_t d=div( iCurrBit++, CHAR_BIT );
			outByteTime = d.quot<nByteTimes ? pByteTimes[d.quot] : 0;
			ASSERT(outByteTime<INT_MAX);
			return ( pBits[d.quot]&(0x80>>d.rem) )!=0;
		}
	};











	TLogTime CCapsBase::TInternalSector::GetAverageIdEndTime(const CTrackReader &tr) const{
		// returns the average time of at which the Sector ID ends
		LONGLONG sum=0; BYTE n=0;
		for( BYTE r=0; r<nRevolutions; r++ )
			if (revolutions[r].idEndTime>0) // may be zero (time information not available) or negative (time information invalid)
				sum+=revolutions[r].idEndTime-tr.GetIndexTime(r), n++;
		return sum/n;
	}











	CCapsBase::CInternalTrack::CInternalTrack(const CTrackReaderWriter &trw,PInternalSector sectors,TSector nSectors)
		// ctor
		// - base
		: CTrackReaderWriter(trw)
		// - initialization
		, modified(false)
		, sectors( nSectors, sectors ) {
		RewindToIndex(0);
	}

	CCapsBase::CInternalTrack::~CInternalTrack(){
		// dtor
		for each( const TInternalSector &ris in sectors )
			for each( auto &r in ris.revolutions )
				if (const PVOID data=r.data)
					::free(data);
	}

	CCapsBase::CInternalTrack *CCapsBase::CInternalTrack::CreateFrom(const CCapsBase &cb,const CapsTrackInfoT2 *ctiRevs,BYTE nRevs,UDWORD lockFlags){
		// creates and returns a Track decoded from underlying CAPS Track representation
		// - at least one full Revolution must be available
		if (!nRevs)
			return nullptr;
		// - reconstructing flux information over all Revolutions of the disk
		nRevs=std::min( nRevs, (BYTE)CAPS_MTRS ); // just to be sure we don't overrun the buffers
		CCapsBitReader revs[CAPS_MTRS];
		UDWORD nBitsTotally=0;
		for( BYTE r=0; r<nRevs; r++ )
			nBitsTotally+=(  revs[r]=CCapsBitReader( ctiRevs[r], lockFlags )  ).GetCount();
		CTrackReaderWriter trw( nBitsTotally*125/100, CTrackReader::KEIR_FRASER, true ); // pessimistic estimation of # of fluxes; allowing for 25% of false "ones" introduced by "FDC-like" decoders
			if (cb.floppyType!=Medium::UNKNOWN && !ctiRevs[0].timelen){
				// Medium already known and the CAPS Track does NOT contain explicit timing information
				trw.SetMediumType(cb.floppyType); // adopting the Medium
			}else{
				// Medium not yet known; estimating it by the average # of Cells per Revolution
				DWORD type=1;
				for( const UDWORD nBitsPerTrackAvg=nBitsTotally/nRevs; type!=0; type<<=1 )
					if (type&Medium::FLOPPY_ANY)
						if (Medium::GetProperties( (Medium::TType)type )->IsAcceptableCountOfCells(nBitsPerTrackAvg)){
							// likely the correct Medium type
							trw.SetMediumType( (Medium::TType)type );
							break;
						}
				if (!type){
					ASSERT(FALSE); //TODO: 8" SD medium
					return nullptr;
				}
			}
		trw.AddIndexTime(0);
		TLogTime currentTime=0, *pFluxTime=trw.GetBuffer();
		for( BYTE r=0; r<nRevs; r++ ){
			// . add fluxes
			auto rev=revs[r];
			for( UDWORD byteTime; rev; ){
				const bool bit=rev.ReadBit(byteTime);
				if (byteTime) // timing available?
					currentTime+= ::MulDiv( trw.GetCurrentProfile().iwTimeDefault, byteTime, 1000 );
				else
					currentTime+= trw.GetCurrentProfile().iwTimeDefault;
				if (bit)
					*pFluxTime++=currentTime;
			}
			// . finish Revolution with an Index
			trw.AddIndexTime( currentTime );
		}
		trw.AddTimes( trw.GetBuffer(), pFluxTime-trw.GetBuffer() );
		// - creating a Track from above reconstructed flux information
		return CreateFrom( cb, std::move(trw) );
	}

	CCapsBase::CInternalTrack *CCapsBase::CInternalTrack::CreateFrom(const CCapsBase &cb,CTrackReaderWriter &&trw,Medium::TType floppyType){
		// creates and returns a Track decoded from underlying flux representation
		if (floppyType==Medium::UNKNOWN) // if type not explicitly overridden ...
			floppyType=cb.floppyType; // ... adopt what the CapsBase contains
		if (floppyType!=Medium::UNKNOWN){ // may be unknown if Medium is still being recognized
			if (!Medium::GetProperties(floppyType)->IsAcceptableRevolutionTime( trw.GetAvgIndexDistance() ))
				return new CInternalTrack( trw, nullptr, 0 );
			trw.SetMediumType(floppyType); // keeps timing intact, just presets codec parameters (codec itself determined below)
			if (cb.dos!=nullptr) // DOS already known (aka. creating final version of the Track)
				if (!cb.preservationQuality && !cb.m_strPathName.IsEmpty()) // normalization makes sense only for existing Images - it's useless for Images just created
					cb.params.corrections.ApplyTo(trw);
			//the following commented out as it brings little to no readability improvement and leaves Tracks influenced by the MediumType
			//else if (params.corrections.indexTiming) // DOS still being recognized ...
				//trw.Normalize(); // ... hence can only improve readability by adjusting index-to-index timing
		}
		Codec::TType c=cb.lastSuccessfullCodec; // turning first to the Codec that successfully decoded the previous Track
		for( Codec::TTypeSet codecs=Codec::ANY,next=1; codecs!=0; c=(Codec::TType)next ){
			// . determining the Codec to be used in the NEXT iteration for decoding
			for( codecs&=~c; (codecs&next)==0&&(next&Codec::ANY)!=0; next<<=1 );
			// . scanning the Track and if no Sector recognized, continuing with Next Codec
			TSectorId ids[Revolution::MAX*(TSector)-1]; TLogTime idEnds[Revolution::MAX*(TSector)-1]; TProfile idProfiles[Revolution::MAX*(TSector)-1]; TFdcStatus statuses[Revolution::MAX*(TSector)-1];
			trw.SetCodec(c);
			const WORD nSectorsFound=trw.Scan( ids, idEnds, idProfiles, statuses );
			if (!nSectorsFound)
				continue;
			// . putting the found Sectors over all complete disk revolutions together (some might have not been recognized in one revolution, but might in another revolution)
			idEnds[nSectorsFound]=INT_MAX; // stop-condition
			class CLongestCommonSubstring sealed{
				const CTrackReader &tr;
				PCSectorId rowIds; // iterated over R in the LCS method
				PCLogTime rowIdEndTimes; // iterated over R in the LCS method
				struct TBacktrackValue{
					enum:BYTE{
						None, Left, Top, Diagonal
					} direction;
					TSector length;
				} backtrackTable[6144]; // big enough to contain the highest number of Sectors known to this date (Night Shift by US Gold contains almost 70!)

				inline
				TBacktrackValue &BacktrackValue(TSector r,TSector c){
					return	backtrackTable[ (r-1)*nUniqueSectors+c ];
				}

				TSector LCS(TSector r,TSector c){
					// naive LCS algorithm using dynamic programming
					if (!r || !c)
						return 0;
					TBacktrackValue &v=BacktrackValue(r,c);
					if (v.direction!=TBacktrackValue::None)
						return v.length;
					if (rowIds[r-1]==uniqueSectors[c-1].id){
						v.direction=TBacktrackValue::Diagonal;
						return v.length = LCS(r-1,c-1)+1;
					}
					const TSector a=LCS(r,c-1), b=LCS(r-1,c);
					if (a>b){
						// the new Sector (Row) is likely to appear earlier on the Track
						v.direction=TBacktrackValue::Left;
						return v.length = a;
					}else if (b>a){
						// the new Sector (Row) is likely to appear later on the Track
						v.direction=TBacktrackValue::Top;
						return v.length = b;
					}else{
						// order of Sectors couldn't be estimated using the LCS algorithm - resolving their appearance on the Track by observing their ID Times
						const TInternalSector &ris=uniqueSectors[c-1];
						BYTE rev=0;
						while (ris.revolutions[rev].idEndTime<=0) rev++;
						v.direction= rowIdEndTimes[r-1]<ris.revolutions[rev].idEndTime ? TBacktrackValue::Left : TBacktrackValue::Top; // proceeding from a Sector that appears LATER on the Track towards a Sector that appears EARLIER on the Track
						return v.length = b;
					}
				}
			public:
				TInternalSector uniqueSectors[(TSector)-1]; // iterated over C in the LCS method
				TSector nUniqueSectors;

				CLongestCommonSubstring(const CTrackReader &tr)
					: tr(tr) , nUniqueSectors(0) {
				}

				/*inline
				bool operator()(const TInternalSector &a,const TInternalSector &b) const{
					// a comparer used to sort Sectors ascending from the index pulse
					BYTE ra=0,rb=0;
					while (a.revolutions[ra].idEndTime<=0) ra++;
					while (b.revolutions[rb].idEndTime<=0) rb++;
					return	a.revolutions[ra].idEndTime-tr.GetIndexTime(ra) > b.revolutions[rb].idEndTime-tr.GetIndexTime(rb);
				}*/

				void Merge(BYTE rev,TSector nIds,PCSectorId ids,PCLogTime idEnds,const TProfile *idProfiles,PCFdcStatus idStatuses){
					// : performing a naive LCS algorithm
					rowIds=ids, rowIdEndTimes=idEnds;
					::ZeroMemory( backtrackTable, (nIds+1)*(nUniqueSectors+1)*sizeof(TBacktrackValue) );
					const TSector nCommonSectors=LCS( nIds, nUniqueSectors );
					// : merging the two series of Sector IDs
					WORD i=nUniqueSectors+nIds-nCommonSectors;
					for( TSector r=nIds,c=nUniqueSectors; r|c; )
						if (!r)
							break; // all new Sectors merged to existing ones
						else
							switch ( c>0 ? BacktrackValue(r,c).direction : TBacktrackValue::Top ){
								case TBacktrackValue::Left:
									// just making space for a Sector that was missing
									uniqueSectors[--i]=uniqueSectors[--c];
									break;
								case TBacktrackValue::Top:{
									// adding a Sector that was missing
									TInternalSector &ris=uniqueSectors[--i];
										::ZeroMemory( &ris, sizeof(TInternalSector) );
										ris.id=ids[--r];
										ris.revolutions[rev].idEndTime=idEnds[r];
										ris.revolutions[rev].idEndProfile=idProfiles[r];
										ris.revolutions[rev].fdcStatus=idStatuses[r];
									break;
								}
								case TBacktrackValue::Diagonal:{
									// a Sector common to both series
									TInternalSector &ris = uniqueSectors[--i] = uniqueSectors[--c];
										ris.revolutions[rev].idEndTime=idEnds[--r];
										ris.revolutions[rev].idEndProfile=idProfiles[r];
										ris.revolutions[rev].fdcStatus=idStatuses[r];
									break;
								}
								default:
									ASSERT(FALSE);
									break;
							}
					nUniqueSectors+=nIds-nCommonSectors;
					// : making sure the Sectors are ordered from the index pulse time ascending
					//std::sort( uniqueSectors, uniqueSectors+nUniqueSectors-1, *this );
				}
			} lcs(trw);
			WORD start,end=0;
			for( BYTE rev=0; rev<trw.GetIndexCount()-1; rev++ ){
				const TLogTime revEndTime=trw.GetIndexTime(rev+1); // revolution end Time
				for( start=end; idEnds[end]<revEndTime; end++ );
				lcs.Merge( rev, end-start, ids+start, idEnds+start, idProfiles+start, statuses+start );
			}
			for( TSector s=0; s<lcs.nUniqueSectors; s++ ){
				auto &rsi=lcs.uniqueSectors[s];
				rsi.nRevolutions=std::max( 1, trw.GetIndexCount()-1 );
				rsi.dirtyRevolution=Revolution::NONE;
			}
			return new CInternalTrack( trw, lcs.uniqueSectors, lcs.nUniqueSectors );
		}
		return new CInternalTrack( trw, nullptr, 0 );
	}

	void CCapsBase::CInternalTrack::ReadSector(TInternalSector &ris,BYTE rev){
		// buffers specified Revolution of the Sector (assumed part of this Track)
		auto &currRev=ris.revolutions[rev];
		if (currRev.idEndTime<=0)
			// Sector's ID Field not found in specified Revolution
			currRev.fdcStatus.ExtendWith( TFdcStatus::SectorNotFound );
		else if (!IsValidSectorLengthCode(ris.id.lengthCode))
			// e.g. invalid for copy-protection marks (Sector with LengthCode 167 has no data)
			currRev.fdcStatus.ExtendWith( TFdcStatus::NoDataField );
		else if (!currRev.data){ // data not yet buffered
			// at least Sector's ID Field found in specified Revolution
			if (currRev.fdcStatus.DescribesMissingDam()) // known from before that data don't exist?
				return;
			const WORD sectorOfficialLength=ris.GetOfficialSectorLength();
			BYTE buffer[16384]; // big enough to contain the longest possible Sector
			currRev.fdcStatus.ExtendWith(
				ReadData(
					ris.id,
					currRev.idEndTime, currRev.idEndProfile,
					sectorOfficialLength, buffer
				)
			);
			if (!currRev.fdcStatus.DescribesMissingDam()){ // "some" data found
				currRev.data=(PSectorData)::memcpy( ::malloc(sectorOfficialLength), buffer, sectorOfficialLength );
				currRev.dataEndTime=GetCurrentTime();
			}
		}
	}

	void CCapsBase::CInternalTrack::FlushSectorBuffers(){
		// spreads referential "dirty" data (if Sector modified) across each Revolution
		for each( const TInternalSector &ris in sectors )
			if (ris.dirtyRevolution<Revolution::MAX){
				// Sector has been modified
				const WORD sectorOfficialDataLength=ris.GetOfficialSectorLength();
				const auto &refRev=ris.revolutions[ris.dirtyRevolution];
				for( BYTE r=0; r<ris.nRevolutions; r++ ){
					const auto &rev=ris.revolutions[r];
					if (const PSectorData data=rev.data)
						WriteData( // spreading referential data across each Revolution
							rev.idEndTime, rev.idEndProfile,
							sectorOfficialDataLength,
							(PCBYTE)::memcpy( data, refRev.data, sectorOfficialDataLength ),
							refRev.fdcStatus
						);
				}
				//ris.dirtyRevolution=Revolution::NONE; // commented out - particular Revolution remains "selected" until the end of this session
			}
		//modified=false; // commented out as the Track hasn't yet been saved!
	}











	CCapsBase::CTrackTempReset::CTrackTempReset(PInternalTrack &rit,PInternalTrack newTrack)
		// ctor
		: Utils::CVarTempReset<PInternalTrack>( rit, newTrack ) {
	}

	CCapsBase::CTrackTempReset::~CTrackTempReset(){
		// dtor
		if (var)
			delete var;
	}











	TStdWinError CCapsBase::UploadFirmware(){
		// uploads firmware to a CAPS-based device (e.g. KryoFlux); returns Windows standard i/o error
		return ERROR_SUCCESS; // no firmware needed
	}

	BOOL CCapsBase::OnOpenDocument(LPCTSTR lpszPathName){
		// True <=> Image opened successfully, otherwise False
		// - determining if library initialized ok
		if (capsLibLoadingError){
			::SetLastError(capsLibLoadingError);
			return FALSE;
		}else if (capsDeviceHandle<0){
			::SetLastError(ERROR_DEVICE_NOT_AVAILABLE);
			return FALSE;
		}
		// - we are done if the CAPS library shouldn't be further involved in opening the Image
		if (lpszPathName==nullptr)
			return TRUE;
		// - "mounting" the Image file to the Device
		char fileName[MAX_PATH];
		#ifdef UNICODE
			static_assert( false, "Unicode support not implemented" );
		#else
			::lstrcpy( fileName, lpszPathName );
		#endif
		if (CAPS::LockImage( capsDeviceHandle, fileName )){
			::SetLastError(ERROR_READ_FAULT);
			return FALSE;
		}
		if (CAPS::GetImageInfo( &capsImageInfo, capsDeviceHandle )
			||
			capsImageInfo.type!=ciitFDD
			||
			capsImageInfo.maxhead>=2 // inclusive!
		){
			CAPS::UnlockImage(capsDeviceHandle);
			::SetLastError(ERROR_NOT_SUPPORTED);
			return FALSE;
		}
		capsImageInfo.maxcylinderOrg=capsImageInfo.maxcylinder;
		capsImageInfo.maxcylinder=std::min( capsImageInfo.maxcylinderOrg, (UDWORD)(FDD_CYLINDERS_MAX-1) ); // inclusive! - correct # of Cylinders
		// - confirming initial settings
		if (!EditSettings(true)){ // dialog cancelled?
			::SetLastError( ERROR_CANCELLED );
			return FALSE;
		}
		// - successfully mounted
		return TRUE;
	}

	struct TSaveParams sealed{
		const CCapsBase &cb;

		TSaveParams(const CCapsBase &cb)
			: cb(cb) {
		}
	};

	TCylinder CCapsBase::GetCylinderCount() const{
		// determines and returns the actual number of Cylinders in the Image
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return capsImageInfo.maxcylinder+1; // the last INCLUSIVE Cylinder plus one
	}

	THead CCapsBase::GetHeadCount() const{
		// determines and returns the actual number of Heads in the Image
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return capsImageInfo.maxhead+1; // the last INCLUSIVE Head plus one
	}

	BYTE CCapsBase::GetAvailableRevolutionCount(TCylinder cyl,THead head) const{
		// returns the number of data variations of one Sector that are guaranteed to be distinct
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (const PInternalTrack pit=GetInternalTrackSafe(cyl,head))
			return pit->GetIndexCount()-1; // # of full Revolutions
		return 0; // Track doesn't exist
	}

	TSector CCapsBase::ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec,PSectorId bufferId,PWORD bufferLength,PLogTime startTimesNanoseconds,PBYTE pAvgGap3) const{
		// returns the number of Sectors found in given Track, and eventually populates the Buffer with their IDs (if Buffer!=Null); returns 0 if Track not formatted or not found
		PInternalTrack &rit=internalTracks[cyl][head];
	{	EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return 0;
	}
		// - scanning (forced recovery from errors right during scanning)
		for( char nRecoveryTrials=7; !rit; nRecoveryTrials-- ){
			// . attempting reading
			ReadTrack( cyl, head );
			// . only for real Devices may we need several trials to obtain healthy data
			if (!properties->IsRealDevice())
				break;
			// . if no more trials left, we are done
			if (nRecoveryTrials<=0)
				break;
			// . attempting to return good data
			EXCLUSIVELY_LOCK_THIS_IMAGE(); // !!! see also below this->{Lock,Unlock}
			if (rit){ // may be Null if, e.g., device manually reset, disconnected, etc.
				if (GetCountOfHealthySectors(cyl,head)>0 || !rit->sectors.length // Track at least partly healthy or without known Sectors
					||
					params.calibrationAfterError==TParams::TCalibrationAfterError::NONE // calibration disabled
				)
					break;
				if (params.calibrationAfterErrorOnlyForKnownSectors && dos->IsKnown()){
					bool knownSectorBad=false; // assumption (the Track is unhealthy due to an irrelevant Unknown Sector, e.g. out of geometry)
					for( TSector s=0; s<rit->sectors.length; s++ ){
						const TInternalSector &is=rit->sectors[s];
						const TPhysicalAddress chs={ cyl, head, is.id };
						if (!dos->IsStdSector(chs))
							continue; // ignore Unknown Sector
						if ( knownSectorBad=!const_cast<CCapsBase *>(this)->GetHealthySectorData(chs,nullptr,s) )
							break;
					}
					if (!knownSectorBad)
						break;
				}
				switch (params.calibrationAfterError){
					case TParams::TCalibrationAfterError::ONCE_PER_CYLINDER:
						// calibrating only once for the whole Cylinder
						if (lastCalibratedCylinder==cyl) // already calibrated?
							break;
						nRecoveryTrials=0;
						//fallthrough
					case TParams::TCalibrationAfterError::FOR_EACH_SECTOR:
						lastCalibratedCylinder=cyl;
						this->locker.Unlock(); // don't block other threads that may want to access already scanned Cylinders
							SeekHeadsHome();
						this->locker.Lock();
						break;
				}				
				delete rit; // disposing the erroneous Track ...
				rit=nullptr; // ... and attempting to obtain its data after head has been calibrated
			}
		}
		// - scanning the Track
		if (const PCInternalTrack pit=rit){
			for each( const TInternalSector &ris in pit->sectors ){
				if (bufferId)
					*bufferId++=ris.id;
				if (bufferLength)
					*bufferLength++=GetUsableSectorLength( ris.id.lengthCode );
				if (startTimesNanoseconds)
					if (floppyType&Medium::FLOPPY_ANY)
						*startTimesNanoseconds++=ris.GetAverageIdEndTime(*pit);
					else{
						ASSERT(FALSE); // we shouldn't end up here - all floppy Types should be covered!
						*startTimesNanoseconds++=-1;
						break;
					}
			}
			if (pCodec)
				*pCodec=pit->GetCodec();
			if (pAvgGap3)
				*pAvgGap3=FDD_350_SECTOR_GAP3*2/3; // TODO
			return pit->sectors.length;
		}else
			return 0;
	}

	bool CCapsBase::IsTrackScanned(TCylinder cyl,THead head) const{
		// True <=> Track exists and has already been scanned, otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return GetInternalTrackSafe(cyl,head)!=nullptr;
	}

	TStdWinError CCapsBase::UnscanTrack(TCylinder cyl,THead head){
		// disposes internal representation of specified Track if possible; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (cyl>=ARRAYSIZE(internalTracks) || head>capsImageInfo.maxhead) // can Track actually exist?
			return ERROR_SEEK;
		if (const TStdWinError err=__super::UnscanTrack(cyl,head)) // base
			return err;
		PInternalTrack &rit=internalTracks[cyl][head];
		delete rit;
		rit=nullptr;
		return ERROR_SUCCESS;
	}

	void CCapsBase::GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses,TLogTime *outDataStarts){
		// populates output buffers with specified Sectors' data, usable lengths, and FDC statuses; ALWAYS attempts to buffer all Sectors - caller is then to sort out eventual read errors (by observing the FDC statuses); caller can call ::GetLastError to discover the error for the last Sector in the input list
		ASSERT( outBufferData!=nullptr && outBufferLengths!=nullptr && outFdcStatuses!=nullptr && outDataStarts!=nullptr );
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead) // can Track actually exist?
			goto invalidTrack;
		if (internalTracks[cyl][head]==nullptr)
			ScanTrack(cyl,head); // reading the Track (if not yet read)
		if (const PInternalTrack pit=internalTracks[cyl][head])
			while (nSectors-->0){
				// . searching for the Sector on the Track
				const TSectorId sectorId=*bufferId++;
				TInternalSector *pis=pit->sectors;
				TSector n=pit->sectors.length;
				for( BYTE nSectorsToSkip=*bufferNumbersOfSectorsToSkip++; n>0; n--,pis++ )
					if (nSectorsToSkip)
						nSectorsToSkip--;
					else if (pis->id==sectorId) // Sector IDs are equal
						break;
				// . if Sector with given ID not found in the Track, we are done
				*outBufferLengths++=GetUsableSectorLength(sectorId.lengthCode); // e.g. Sector with LengthCode 167 has no data
				if (!n){
					*outBufferData++=nullptr, *outFdcStatuses++=TFdcStatus::SectorNotFound, *outDataStarts++=0;
					continue;
				}
				// . setting initial Revolution
				BYTE nDataAttempts=1; // assumption
				if (pis->dirtyRevolution<Revolution::MAX)
					pis->currentRevolution=pis->dirtyRevolution; // modified Revolution is obligatory for any subsequent data requests
				else if (rev<pis->nRevolutions)
					pis->currentRevolution=rev; // wanted particular existing Revolution
				else if (rev<Revolution::MAX){
					*outFdcStatuses++=TFdcStatus::SectorNotFound; // wanted particular non-existent Revolution
					*outBufferData++=nullptr, *outDataStarts++=0;
					continue;
				}else
					switch (rev){
						default:
							ASSERT(FALSE); // we shouldn't end up here!
						case Revolution::CURRENT:
							break;
						case Revolution::NEXT:
							pis->currentRevolution=(pis->currentRevolution+1)%pis->nRevolutions;
							break;
						case Revolution::ANY_GOOD:
							nDataAttempts=pis->nRevolutions+1; // "+1" = given the below DO-WHILE cycle, make sure we end-up where we started if all Revolutions contain bad data
							break;
					}
				// . attempting for Sector data
				const WORD fdcStatusMask=~outFdcStatuses->w;
				const auto *currRev=pis->revolutions+pis->currentRevolution;
				const auto *optRev=currRev;
					do{
						// : attempting for Sector data in CurrentRevolution
						pit->ReadSector( *pis, pis->currentRevolution );
						// : if "better" Data read (by the count of errors), make them a candidate
						if (currRev->fdcStatus.GetSeverity(fdcStatusMask)<optRev->fdcStatus.GetSeverity(fdcStatusMask)) // better Data read?
							if (( optRev=currRev )->fdcStatus.IsWithoutError()) // healthy Data read?
								break; // return them
						// : attempting next disk Revolution to retrieve healthy Data
						if (!--nDataAttempts) // was this the last attempt?
							break;
						if (++pis->currentRevolution>=pis->nRevolutions)
							pis->currentRevolution=0;
						currRev=pis->revolutions+pis->currentRevolution;
					}while (true);
				// . returning (any) Data
				*outDataStarts++=optRev->idEndTime;
				*outFdcStatuses++=optRev->fdcStatus;
				*outBufferData++=optRev->data;
				//*outBufferLengths++=... // already set above
			}
		else
invalidTrack:
			while (nSectors-->0)
				*outBufferData++=nullptr, *outFdcStatuses++=TFdcStatus::SectorNotFound, *outDataStarts++=0;
		::SetLastError( *--outBufferData ? ERROR_SUCCESS : ERROR_SECTOR_NOT_FOUND );
	}

	TDataStatus CCapsBase::IsSectorDataReady(TCylinder cyl,THead head,RCSectorId id,BYTE nSectorsToSkip,Revolution::TType rev) const{
		// True <=> specified Sector's data variation (Revolution) has been buffered, otherwise False
		ASSERT( rev<Revolution::MAX );
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (const PCInternalTrack pit=GetInternalTrackSafe(cyl,head)) // is Track scanned?
			while (nSectorsToSkip<pit->sectors.length){
				const auto &ris=pit->sectors[nSectorsToSkip++];
				if (ris.id==id)
					if (rev>=ris.nRevolutions)
						return TDataStatus::READY; // can't create another sample of the data (unlike in CFDD class where we have infinite # of trials), so here declaring "no data ready"
					else if (ris.revolutions[rev].HasGoodDataReady())
						return TDataStatus::READY_HEALTHY;
					else if (ris.revolutions[rev].HasDataReady())
						return TDataStatus::READY;
					else
						break;
			}
		return TDataStatus::NOT_READY;
	}

	Revolution::TType CCapsBase::GetDirtyRevolution(RCPhysicalAddress chs,BYTE nSectorsToSkip) const{
		// returns the Revolution that has been marked as "dirty"
		if (const PCInternalTrack pit=GetInternalTrackSafe(chs.cylinder,chs.head))
			while (nSectorsToSkip<pit->sectors.length){
				const auto &ris=pit->sectors[nSectorsToSkip++];
				if (ris.id==chs.sectorId)
					return ris.dirtyRevolution;
			}
		return Revolution::NONE; // unknown Track or Sector is never "dirty"
	}

	TStdWinError CCapsBase::GetInsertedMediumType(TCylinder cyl,Medium::TType &rOutMediumType) const{
		// True <=> Medium inserted in the Drive and recognized, otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (params.userForcedMedium){
			rOutMediumType=floppyType;
			return ERROR_SUCCESS;
		}
		// - retrieving currently inserted Medium zeroth Track
		const CTrackTempReset ritInserted( internalTracks[cyl][0] ); // forcing a new scanning
		const CTrackTempReset resetHead1( internalTracks[cyl][1] ); // dispose eventually changed Head 1 Track (e.g. in *.HFE that loads whole cylinders at once)
		ScanTrack(cyl,0);
		if (ritInserted==nullptr)
			if (properties->IsRealDevice())
				return ERROR_NO_MEDIA_IN_DRIVE;
			else{
				rOutMediumType=Medium::UNKNOWN;
				return ERROR_SUCCESS; // e.g. a KryoFlux Stream file has been manually deleted, thus Unknown Medium has been "successfully" recognized
			}
		// - enumerating possible floppy Types and attempting to recognize some Sectors
		ritInserted->ClearAllMetaData(); // don't influence recognition with MetaData
		WORD highestScore=0; // arbitering the MediumType by the HighestScore and indices distance
		Medium::TType bestMediumType=Medium::UNKNOWN;
		for( DWORD type=1; type!=0; type<<=1 )
			if (type&Medium::FLOPPY_ANY)
				if (const CTrackTempReset &&rit=CTrackTempReset(
						internalTracks[cyl][0],
						CInternalTrack::CreateFrom( *this, std::move(CTrackReaderWriter(*ritInserted,false)), rOutMediumType=(Medium::TType)type )
					)
				){
					const TSector nRecognizedSectors=rit->sectors.length;
					if (const WORD score= nRecognizedSectors + 32*GetCountOfHealthySectors(cyl,0)){
						if (score>highestScore)
							highestScore=score, bestMediumType=rOutMediumType;
					}
				}
		// - Medium (possibly) recognized
		rOutMediumType=bestMediumType; // may be Medium::UNKNOWN
		return ERROR_SUCCESS;
	}

	#define SCANNED_CYLINDERS	3

	TStdWinError CCapsBase::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		// - determining if library initialized ok
		if (capsLibLoadingError)
			return capsLibLoadingError;
		// - Medium set correctly if some Sectors can be extracted from one of Tracks
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (pFormat->mediumType==Medium::UNKNOWN){
			// no particular Medium specified - enumerating all supported floppy Types
			WORD scoreMax=0; // arbitering the MediumType by scores
			Medium::TType bestMediumType=Medium::UNKNOWN;
			TFormat tmp=*pFormat;
			for( DWORD type=1; type!=0; type<<=1 )
				if (type&forcedMediumType){
					WORD score=0;
					tmp.mediumType=(Medium::TType)type;
					const Utils::CVarTempReset<PDos> dos0( dos, nullptr );
					const TStdWinError err=SetMediumTypeAndGeometry( &tmp, sideMap, firstSectorNumber );
					if (params.userForcedMedium && tmp.mediumType==floppyType)
						return ERROR_SUCCESS;
					if (err)
						continue;
					for( TCylinder cyl=0; cyl<SCANNED_CYLINDERS; cyl++ ) // counting the # of healthy Sectors
						for( THead head=0; head<2; head++ )
							score+=	ScanTrack(cyl,head)
									+
									8*GetCountOfHealthySectors(cyl,head);
					if (score>scoreMax)
						scoreMax=score, bestMediumType=tmp.mediumType;
				}
			if (scoreMax>0){
				tmp.mediumType=bestMediumType;
				return SetMediumTypeAndGeometry( &tmp, sideMap, firstSectorNumber );
			}
		}else if (!params.userForcedMedium || params.userForcedMedium&&pFormat->mediumType==floppyType){
			// a particular Medium specified
			// . determining if this is yet a non-formatted disk
			bool blankMedium=true;
			for( TCylinder cyl=0; cyl<FDD_CYLINDERS_MAX; cyl++ )
				blankMedium&=internalTracks[cyl][0]==internalTracks[cyl][1]; // equal only if both Null
			// . loading pre-compensation parameters for the specified FloppyType
			precompensation.Load( pFormat->mediumType );
			// . if a fresh formatted new disk, we are done - as the rest is VERY time-consuming when applied for the whole disk, it's forbidden to change MediumType at this state
			const bool newMediumTypeDifferent=floppyType!=pFormat->mediumType;
			if (m_strPathName.IsEmpty() && !blankMedium)
				return	newMediumTypeDifferent ? ERROR_NOT_SUPPORTED : ERROR_SUCCESS;
			// . base
			if (const TStdWinError err=__super::SetMediumTypeAndGeometry( pFormat, sideMap, firstSectorNumber ))
				return err;
			// . if blank (not yet formatted) new disk, we are done
			if (m_strPathName.IsEmpty() && blankMedium)
				return ERROR_SUCCESS;
			// . reinterpreting the fluxes with updated MediumType (now/not recognizing Sectors)
			if (newMediumTypeDifferent || dos!=nullptr) // A|B, A = Medium different, B = setting final MediumType
				for( TCylinder cyl=0; cyl<FDD_CYLINDERS_MAX; cyl++ )
					for( THead head=0; head<2; head++ )
						if (auto &rit=internalTracks[cyl][head]){
							CTrackReaderWriter trw=*rit; // extract fluxes
							delete rit;
							rit=CInternalTrack::CreateFrom( *this, std::move(trw), pFormat->mediumType );
						}
			// . seeing if some Sectors can be recognized in any of Tracks that usually contain the Boot Sector of implemented DOSes
			if (params.userForcedMedium)
				return ERROR_SUCCESS;
			CMapWordToPtr bootCylinders; // unique Cylinders where usually the Boot Sector (or its backup) is found
			for( POSITION pos=CDos::Known.GetHeadPosition(); pos; )
				bootCylinders.SetAt( CDos::Known.GetNext(pos)->stdBootCylinder, nullptr );
			for( POSITION pos=bootCylinders.GetStartPosition(); pos; ){
				WORD cyl; LPVOID tmp;
				bootCylinders.GetNextAssoc( pos, cyl, tmp );
				for( const TCylinder cylZ=cyl+SCANNED_CYLINDERS; cyl<cylZ; cyl++ ) // examining just first N Cylinders
					for( THead head=2; head>0; )
						if (ScanTrack(cyl,--head)!=0){
							if (!IsTrackHealthy(cyl,head)){ // if Track read with errors ...
								auto &rit=internalTracks[cyl][head];
								delete rit, rit=nullptr; // ... disposing it and letting DOS later read it once again
							}
							return ERROR_SUCCESS;
						}
			}
		}
		// - no data could be recognized on any of Tracks
		return ERROR_NOT_SUPPORTED;
	}

	bool CCapsBase::EditSettings(bool initialEditing){
		// True <=> new settings have been accepted (and adopted by this Image), otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - defining the Dialog
		class CCapsInformation sealed:public Utils::CRideDialog{
			const CCapsBase &cb;
			const bool initialEditing;

			void PreInitDialog() override{
				__super::PreInitDialog();
				const HWND hMedium=GetDlgItemHwnd(ID_MEDIUM);
				CImage::PopulateComboBoxWithCompatibleMedia( hMedium, forcedMediumType, cb.properties );
				ComboBox_SetItemData(
					hMedium,
					ComboBox_AddString( hMedium, _T("Automatically") ),
					forcedMediumType
				);
				static constexpr WORD Controls[]={ ID_MEDIUM, IDOK, 0 };
				EnableDlgItems( Controls, initialEditing );
				SetDlgItemFormattedText( ID_SYSTEM, _T("Version %d.%d"), cb.capsVersionInfo.release, cb.capsVersionInfo.revision );
				SetDlgItemFormattedText( ID_ARCHIVE, _T("%u (0x%08X)"), cb.capsImageInfo.release, cb.capsImageInfo.release );
				SetDlgItemInt( ID_ACCURACY, cb.capsImageInfo.revision );
				const SYSTEMTIME st={ cb.capsImageInfo.crdt.year, cb.capsImageInfo.crdt.month, 0, cb.capsImageInfo.crdt.day, cb.capsImageInfo.crdt.hour, cb.capsImageInfo.crdt.min, cb.capsImageInfo.crdt.sec };
					FILETIME ft;
					::SystemTimeToFileTime( &st, &ft );
		{		TCHAR buf[256];
				SetDlgItemText( ID_DATE, CMSDOS7::TDateTime(ft).ToString(buf) );
		}		char buf[256]; *buf='\0';
				for( BYTE i=0; i<CAPS_MAXPLATFORM; i++ )
					if (cb.capsImageInfo.platform[i]!=ciipNA)
						::lstrcatA(  ::lstrcatA(buf, ", " ),  CAPS::GetPlatformName(cb.capsImageInfo.platform[i])  );
					else if (!i) // no Platforms specified for the file
						::lstrcpyA( buf+2, "N/A" );
				::SetDlgItemTextA( *this, ID_DOS, buf+2 );
			}

			void DoDataExchange(CDataExchange *pDX) override{
				CComboBox cb;
				cb.Attach( GetDlgItemHwnd(ID_MEDIUM) );
					if (pDX->m_bSaveAndValidate)
						forcedMediumType=(Medium::TType)cb.GetItemData( cb.GetCurSel() );
					else{
						int iSel=0;
						for( const int nMedia=cb.GetCount()-1; iSel<nMedia; iSel++ ) // "-1" = to later select the "Automatic" option
							if (cb.GetItemData(iSel)==this->cb.floppyType)
								break;
						cb.SetCurSel(iSel);
					}
				cb.Detach();
			}
		public:
			Medium::TType forcedMediumType;

			CCapsInformation(const CCapsBase &cb,bool initialEditing)
				: Utils::CRideDialog(IDR_CAPS)
				, forcedMediumType(cb.forcedMediumType)
				, cb(cb) , initialEditing(initialEditing) {
			}
		} d( *this, initialEditing );
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			forcedMediumType=d.forcedMediumType;
			return true;
		}else
			return false;
	}

	void CCapsBase::EnumSettings(CSettings &rOut) const{
		// returns a collection of relevant settings for this Image
		__super::EnumSettings(rOut);
		rOut.AddLibrary( _T("CAPS"), capsVersionInfo.release, capsVersionInfo.revision );
		rOut.AddId(capsImageInfo.release);
		rOut.AddRevision(capsImageInfo.revision);
		rOut.AddMediumIsForced( forcedMediumType!=Medium::FLOPPY_ANY );
		rOut.AddCylinderCount(capsImageInfo.maxcylinder+1);
		rOut.AddHeadCount(capsImageInfo.maxhead+1);
	}

	CString CCapsBase::ListUnsupportedFeatures() const{
		// returns a list of all features currently not properly implemented
		return	unsupportedFeaturesMessageBar.CreateListItemIfUnsupported( capsImageInfo.maxcylinderOrg+1 )
				+
				__super::ListUnsupportedFeatures();
	}

	CCapsBase::PInternalTrack CCapsBase::GetModifiedTrackSafe(TCylinder cyl,THead head) const{
		if (const PInternalTrack pit=GetInternalTrackSafe(cyl,head)) // Track buffered?
			if (pit->modified)
				return pit;
		return nullptr;
	}

	bool CCapsBase::AnyTrackModified(TCylinder cyl) const{
		// True <=> Track under any of Heads has been Modified, otherwise False
		return GetModifiedTrackSafe(cyl,0)!=GetModifiedTrackSafe(cyl,1); // both Null if none of the Modified
	}

	void CCapsBase::DestroyAllTracks(){
		// disposes all InternalTracks created thus far
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		for( TCylinder cyl=0; cyl<FDD_CYLINDERS_MAX; cyl++ )
			for( THead head=0; head<2; head++ )
				if (auto &rit=internalTracks[cyl][head])
					delete rit, rit=nullptr;
		CAPS::UnlockAllTracks( capsDeviceHandle );
	}

	TStdWinError CCapsBase::Reset(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (!properties->IsRealDevice()) // for real Devices already sampled Tracks are unnecessary to be destroyed (caller to explicitly call 'DestroyAllTracks' method, e.g. in dtor)
			DestroyAllTracks();
		return ERROR_SUCCESS;
	}

	TStdWinError CCapsBase::UploadTrack(TCylinder cyl,THead head,CTrackReader tr) const{
		// uploads specified Track to a CAPS-based device (e.g. KryoFlux); returns Windows standard i/o error
		ASSERT(FALSE); // override in descendant to send Track to a Device
		return ERROR_NOT_SUPPORTED;
	}

	TStdWinError CCapsBase::VerifyTrack(TCylinder cyl,THead head,CTrackReaderWriter trwWritten,bool showDiff,std::unique_ptr<CTrackReaderWriter> *ppOutReadTrack,const volatile bool &cancelled) const{
		// verifies specified Track that is assumed to be just written; returns Windows standard i/o error
		// - Medium must be known
		const Medium::PCProperties mp=Medium::GetProperties(floppyType);
		if (!mp)
			return ERROR_UNRECOGNIZED_MEDIA;
		// - scanning what's been written
		const Utils::CVarTempReset<PInternalTrack> pit0( internalTracks[cyl][head], nullptr ); // forcing rescan
			ScanTrack( cyl, head );
		std::unique_ptr<CInternalTrack> tmp( internalTracks[cyl][head] );
		if (cancelled)
			return ERROR_CANCELLED;
		const PCInternalTrack pitRead=tmp.get();
		if (!pitRead)
			return ERROR_GEN_FAILURE;
		if (ppOutReadTrack) // caller wants to take over ownership?
			ppOutReadTrack->reset( tmp.release() );
		// - verification
		const auto &&writtenBits=trwWritten.CreateBitSequence( Revolution::R0 );
		const auto &&readBits=pitRead->CreateBitSequence( Revolution::R0 );
		const auto &&ses=writtenBits.GetShortestEditScript( // shortest edit script
			readBits, CActionProgress(cancelled)
		);
		if (cancelled)
			return ERROR_CANCELLED;
		if (!ses)
			return ERROR_FUNCTION_FAILED;
		if (!ses.length) // the unlikely case of absolutely no differences
			return ERROR_SUCCESS;
		const TLogTimeInterval significant( // TODO: better way than ignoring the first and last N% of the Revolution
			mp->revolutionTime/100*2, mp->revolutionTime/100*98
		);
		const auto *const psi=ses.LowerBound(
			significant.tStart,
			[&writtenBits](const CDiffBase::TScriptItem &si,TLogTime t){ return writtenBits[si.iPosA].time<=t; }
		);
		if (psi==ses.end() || writtenBits[psi->iPosA].time>significant.tEnd)
			return ERROR_SUCCESS; // only insignificant differences at the beginning and end of Revolution
		// - composition and display of non-overlapping erroneously written regions of the Track
		const Utils::CCallocPtr<CTrackReader::TRegion> badRegions(ses.length);
		if (!badRegions)
			return ERROR_NOT_ENOUGH_MEMORY;
		const DWORD nBadRegions=writtenBits.ScriptToLocalRegions( ses, ses.length, badRegions, COLOR_RED );
		CTrackReader::CParseEventList peTrack;
		TSectorId ids[Revolution::MAX*(TSector)-1]; TLogTime idEnds[Revolution::MAX*(TSector)-1]; TLogTime dataEnds[Revolution::MAX*(TSector)-1];
		const auto nSectors=trwWritten.ScanAndAnalyze( ids, idEnds, dataEnds, peTrack, CActionProgress::None, false ); // False = only linear, time-inexpensive analysis (thus no need for doing this in parallel)
		if (!params.verifyBadSectors) // remove Events that relate to Bad Sectors
			for( auto i=nSectors; i>0; ){
				const TPhysicalAddress chs={ cyl, head, ids[--i] };
				if (dos->GetSectorStatus(chs)==TSectorStatus::BAD){
					peTrack.RemoveConsecutiveBeforeEnd( idEnds[i] );
					peTrack.RemoveConsecutiveBeforeEnd( dataEnds[i] );
				}
			}
		for each( const auto &br in badRegions )
			if (peTrack.IntersectsWith(br)) // an intersection with ID or Data fields
				switch (trwWritten.ShowModal( badRegions, nBadRegions, MB_ABORTRETRYIGNORE, true, br.tStart, _T("Track %02d.%c verification failed: Review RED-MARKED errors (use J and L keys) and decide how to proceed!"), cyl, '0'+head )){
					case IDOK: // ignore
						return ERROR_CONTINUE;
					case IDCANCEL:
						return ERROR_CANCELLED;
					case IDRETRY:
						return ERROR_RETRY;
					default:
						return ERROR_FUNCTION_FAILED;
				}
		// - none of the BadRegions impairs written data
		return ERROR_SUCCESS;
	}

	TStdWinError CCapsBase::DetermineMagneticReliabilityByWriting(Medium::TType floppyType,TCylinder cyl,THead head,const volatile bool &cancelled) const{
		// determines if specified Track on real floppy can be trusted (ERROR_SUCCESS) or not (ERROR_DISK_CORRUPT); returns Windows standard i/o error
		// - determining Medium
		const Medium::PCProperties mp=Medium::GetProperties(floppyType);
		if (!mp)
			return ERROR_UNRECOGNIZED_MEDIA;
		// - composition of test Track
		CTrackReaderWriter trw( mp->nCells/2, floppyType );
		const TLogTime doubleCellTime=2*mp->cellTime;
		// - evaluating Track magnetic reliability
		const std::unique_ptr<CInternalTrack> pit(
			CInternalTrack::CreateFrom( *this, std::move(trw), floppyType )
		);
		pit->modified=true; // to pass the save conditions
		for( BYTE nTrials=3; nTrials>0; nTrials-- ){
			// . saving the test Track
	{		const Utils::CVarTempReset<PInternalTrack> pit0( internalTracks[cyl][head], pit.get() );
			if (const TStdWinError err=SaveTrack( cyl, head, cancelled ))
				return err;
	}		// . reading the test Track back
			const CTrackTempReset rit( internalTracks[cyl][head] ); // forcing a new scan
			ScanTrack( cyl, head );
			if (rit==nullptr)
				return ERROR_FUNCTION_FAILED;
			//pit->SetMediumType( floppyType ); // commented out as unnecessary (no decoder used here)
			// . evaluating what we read
			CTrackReader tr=*rit;
			TLogTime t=tr.GetIndexTime(0)+60*doubleCellTime; // "+N" = ignoring the region immediatelly after index - may be invalid due to Write Gate signal still on
			tr.SetCurrentTime(t);
			for( const TLogTime tOkA=doubleCellTime*80/100,tOkZ=doubleCellTime*120/100; t<mp->revolutionTime; ){ // allowing for 20% deviation from nominal Flux transition
				const TLogTime t0=t;
				const TLogTime flux=( t=tr.ReadTime() )-t0;
				if (flux<tOkA || tOkZ<flux)
					break;
			}
			if (t>=mp->revolutionTime)
				return ERROR_SUCCESS; // yes, the Track can be magnetically trusted
		}
		return ERROR_DISK_CORRUPT;
	}

	CImage::CTrackReader CCapsBase::ReadTrack(TCylinder cyl,THead head) const{
		// creates and returns a general description of the specified Track, represented using neutral LogicalTimes
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - if Track already read before, returning the result from before
		if (const auto &tr=ReadExistingTrackUnsafe(cyl,head))
			return tr;
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return CTrackReaderWriter::Invalid;
		// - creating the description
		static constexpr CapsTrackInfoT2 CtiEmpty={2};
		const UDWORD lockFlags= capsVersionInfo.flag&( DI_LOCK_INDEX | DI_LOCK_DENVAR | DI_LOCK_DENAUTO | DI_LOCK_DENNOISE | DI_LOCK_NOISE | DI_LOCK_TYPE | DI_LOCK_OVLBIT | DI_LOCK_TRKBIT | DI_LOCK_UPDATEFD );
		CapsTrackInfoT2 cti[CAPS_MTRS];
		*cti=CtiEmpty;
		if (CAPS::LockTrack( cti, capsDeviceHandle, cyl, head, lockFlags )!=imgeOk
			||
			(cti->type&CTIT_MASK_TYPE)==ctitNA // error during Track retrieval
		)
			return CTrackReaderWriter::Invalid;
		BYTE nRevs=1;
		if (cti->weakcnt!=0) // Track contains some areas with fuzzy bits
			while (nRevs<CAPS_MTRS){
				CapsTrackInfoT2 &r = cti[nRevs] = *cti;
				r.trackbuf=(PUBYTE)::memcpy( ::malloc(r.timelen), r.trackbuf, r.timelen ); // timelen = # of Time information and also # of Bytes that individual bits are stored in
				r.timebuf=(PUDWORD)::memcpy( ::malloc(r.timelen*sizeof(UDWORD)), r.timebuf, r.timelen*sizeof(UDWORD) );
				*cti=CtiEmpty;
				if (CAPS::LockTrack( cti, capsDeviceHandle, cyl, head, capsVersionInfo.flag&(lockFlags|DI_LOCK_SETWSEED) )!=imgeOk
					||
					(r.type&CTIT_MASK_TYPE)==ctitNA // error during Track retrieval
				)
					break;
				nRevs++;
			}
		PInternalTrack &rit=internalTracks[cyl][head];
		if (const PInternalTrack tmp=CInternalTrack::CreateFrom( *this, cti, nRevs, lockFlags )){
			CTrackReaderWriter trw=*tmp; // extracting raw flux data ...
			delete tmp;
			rit=CInternalTrack::CreateFrom( *this, std::move(trw) ); // ... and rescanning the Track using current FloppyType Profile
		}
		while (--nRevs>0){
			const CapsTrackInfoT2 &r=cti[nRevs];
			::free(r.trackbuf), ::free(r.timebuf);
		}
		CAPS::UnlockTrack( capsDeviceHandle, cyl, head );
		return *rit;
	}

	const CImage::CTrackReader &CCapsBase::ReadExistingTrackUnsafe(TCylinder cyl,THead head) const{
		// creates and returns a general description of the specified Track, represented using neutral LogicalTimes
		//EXCLUSIVELY_LOCK_THIS_IMAGE(); // the reason why "unsafe" - up to the caller to lock the Image
		if (const PInternalTrack pit=GetInternalTrackSafe(cyl,head)){
			pit->FlushSectorBuffers(); // convert all modifications into flux transitions
			pit->SetCurrentTime(0); // just to be sure the internal TrackReader is returned in valid state (as invalid state indicates this functionality is not supported)
			return *pit;
		}else
			return CTrackReaderWriter::Invalid;
	}












	#define INI_FLUX_DECODER			_T("deco2")
	#define INI_FLUX_DECODER_RESET		_T("drst")
	#define INI_PRECISION				_T("prec2")
	#define INI_40_TRACK_DRIVE			_T("sd40")
	#define INI_CALIBRATE_SECTOR_ERROR	_T("clberr")
	#define INI_CALIBRATE_SECTOR_ERROR_KNOWN _T("clbknw")
	#define INI_CALIBRATE_FORMATTING	_T("clbfmt")
	#define INI_VERIFY_WRITTEN_TRACKS	_T("vwt")
	#define INI_VERIFY_BAD_SECTORS		_T("vwtbs")

	typedef CImage::CTrackReader::TDecoderMethod TFluxDecoder;

	CCapsBase::TParams::TParams(LPCTSTR iniSectionName)
		// ctor
		// - persistent (saved and loaded)
		: iniSectionName( iniSectionName )
		, precision( app.GetProfileEnum(iniSectionName,INI_PRECISION,TPrecision::BASIC) )
		, fluxDecoder( app.GetProfileEnum(iniSectionName,INI_FLUX_DECODER,TFluxDecoder::KEIR_FRASER) )
		, resetFluxDecoderOnIndex( app.GetProfileBool(iniSectionName,INI_FLUX_DECODER_RESET,true) )
		, fortyTrackDrive( app.GetProfileBool(iniSectionName,INI_40_TRACK_DRIVE) )
		, calibrationAfterError( app.GetProfileEnum(iniSectionName,INI_CALIBRATE_SECTOR_ERROR,TCalibrationAfterError::ONCE_PER_CYLINDER) )
		, calibrationAfterErrorOnlyForKnownSectors( app.GetProfileBool(iniSectionName,INI_CALIBRATE_SECTOR_ERROR_KNOWN) )
		, calibrationStepDuringFormatting( app.GetProfileInt(iniSectionName,INI_CALIBRATE_FORMATTING,0) )
		, corrections( iniSectionName )
		, verifyWrittenTracks( app.GetProfileBool(iniSectionName,INI_VERIFY_WRITTEN_TRACKS,true) )
		, verifyBadSectors( app.GetProfileBool(iniSectionName,INI_VERIFY_BAD_SECTORS,true) )
		// - volatile (current session only)
		, userForcedMedium(false)
		, flippyDisk(false) , userForcedFlippyDisk(false)
		, doubleTrackStep(false) , userForcedDoubleTrackStep(false) { // True once the ID_40D80 button in Settings dialog is pressed
	}

	CCapsBase::TParams::~TParams(){
		// dtor
		app.WriteProfileInt( iniSectionName, INI_PRECISION, precision );
		app.WriteProfileInt( iniSectionName, INI_FLUX_DECODER, fluxDecoder );
		app.WriteProfileInt( iniSectionName, INI_FLUX_DECODER_RESET, resetFluxDecoderOnIndex );
		app.WriteProfileInt( iniSectionName, INI_40_TRACK_DRIVE, fortyTrackDrive );
		app.WriteProfileInt( iniSectionName, INI_CALIBRATE_SECTOR_ERROR, calibrationAfterError );
		app.WriteProfileInt( iniSectionName, INI_CALIBRATE_SECTOR_ERROR_KNOWN, calibrationAfterErrorOnlyForKnownSectors );
		app.WriteProfileInt( iniSectionName, INI_CALIBRATE_FORMATTING, calibrationStepDuringFormatting );
		corrections.Save( iniSectionName );
		app.WriteProfileInt( iniSectionName, INI_VERIFY_WRITTEN_TRACKS, verifyWrittenTracks );
		app.WriteProfileInt( iniSectionName, INI_VERIFY_BAD_SECTORS, verifyBadSectors );
	}

	bool CCapsBase::TParams::EditInModalDialog(CCapsBase &rcb,LPCTSTR firmware,bool initialEditing){
		// True <=> new settings have been accepted (and adopted by this Image), otherwise False
		static constexpr WORD InitialSettingIds[]={ ID_ROTATION, ID_ACCURACY, ID_DEFAULT1, ID_VARIABLE, ID_MEDIUM, ID_SIDE, ID_DRIVE, ID_40D80, ID_TRACK, ID_TIME, 0 };
		static constexpr WORD AutoRecognizedMediumIds[]={ ID_MEDIUM, ID_RECOVER, 0 };
		// - defining the Dialog
		class CParamsDialog sealed:public Utils::CRideDialog{
			const LPCTSTR firmware;
			const bool initialEditing;
			const Utils::CRideFont &warningFont;
			CCapsBase &rcb;
			Medium::TType currentMediumType;
			CPrecompensation tmpPrecomp;
			TCHAR flippyDiskTextOrg[40],doubleTrackDistanceTextOrg[80];

			bool IsFlippyDiskForcedByUser() const{
				// True <=> user has manually overrode FlippyDisk setting, otherwise False
				return ::lstrlen(flippyDiskTextOrg)!=GetDlgItemTextLength(ID_SIDE);
			}

			bool IsDoubleTrackDistanceForcedByUser() const{
				// True <=> user has manually overrode DoubleTrackDistance setting, otherwise False
				return ::lstrlen(doubleTrackDistanceTextOrg)!=GetDlgItemTextLength(ID_40D80);
			}

			void RefreshMediumInformation(){
				// detects a floppy in the Drive and attempts to recognize its Type
				// . making sure that a floppy is in the Drive
				ShowDlgItem( ID_INFORMATION, false );
				static constexpr WORD Interactivity[]={ IDOK, ID_LATENCY, ID_NUMBER2, ID_GAP, 0 };
				const bool fortyTrackDrive=IsDlgItemChecked(ID_DRIVE);
				if (!EnableDlgItems( Interactivity+!initialEditing, rcb.GetInsertedMediumType(0,currentMediumType)==ERROR_SUCCESS )){
					SetDlgItemText( ID_MEDIUM, _T("Not inserted") );
					EnableDlgItem( IDOK, false ); // Drives that have a hardware error (e.g. Track 0 sensor) may have long response time; to avoid the application to appear "frozen" later on, we don't allow continuation
				// . attempting to recognize any previous format on the floppy
				}else
					switch (currentMediumType){
						case Medium::FLOPPY_DD_525:
							if (EnableDlgItem( ID_40D80, initialEditing&&!fortyTrackDrive )){
								const Utils::CVarTempReset<bool> dts0( rcb.params.doubleTrackStep, false );
								const Utils::CVarTempReset<Medium::TType> ft0( rcb.floppyType, currentMediumType );
								Medium::TType mt;
								if (rcb.GetInsertedMediumType(1,mt)==ERROR_SUCCESS){
									const CTrackTempReset rit( rcb.internalTracks[2][0] );
									TSectorId ids[(TSector)-1];
									ShowDlgItem( ID_INFORMATION,
										!CheckDlgItem( ID_40D80,
											mt==Medium::UNKNOWN // first Track is empty, so likely each odd Track is empty
											||
											CountSectorsBelongingToCylinder( 1, ids, rcb.ScanTrack(2,0,nullptr,ids) )>=5 // ">=N" = at least N Sectors (empirical value) from Cylinder 2 actually belong to Cylinder 1
										)
										&&
										CountSectorsBelongingToCylinder( 1, ids, rcb.ScanTrack(1,0,nullptr,ids) )<5 // "<N" = less than N Sectors (empirical value) from Cylinder 1 actually belong to Cylinder 1
									);
								}
								rcb.SeekHeadsHome();
							}
							//fallthrough
						case Medium::FLOPPY_DD:
							SetDlgItemText( ID_MEDIUM, Medium::GetDescription(currentMediumType) );
							if (initialEditing){
								if (currentMediumType==Medium::FLOPPY_DD)
									CheckAndEnableDlgItem( ID_40D80, false, !fortyTrackDrive );
								const CTrackTempReset rit( rcb.internalTracks[0][1] ); // forcing a new scanning
								const Utils::CVarTempReset<bool> fd0( rcb.params.flippyDisk, true ); // assumption (this is a FlippyDisk)
								CheckDlgItem( ID_SIDE, rcb.ScanTrack(0,1)>0 );
							}
							break;
						case Medium::FLOPPY_HD_525:
						case Medium::FLOPPY_HD_350:
							SetDlgItemText( ID_MEDIUM, Medium::GetDescription(currentMediumType) );
							if (initialEditing){
								CheckAndEnableDlgItem( ID_SIDE, false );
								CheckAndEnableDlgItem( ID_40D80, false, !fortyTrackDrive );
							}
							break;
						default:
							SetDlgItemFormattedText( ID_MEDIUM, _T("Not formatted or faulty%c(<a>set manually</a>)"), initialEditing?' ':'\0' );
							if (initialEditing){
								CheckAndEnableDlgItem( ID_SIDE, false );
								CheckAndEnableDlgItem( ID_40D80, false, !fortyTrackDrive );
							}
							break;
					}
				// . forcing redrawing (as the new text may be shorter than the original text, leaving the original partly visible)
				InvalidateDlgItem(ID_MEDIUM);
				Invalidate(FALSE);
				// . refreshing the status of Precompensation
				tmpPrecomp.Load(currentMediumType);
				RefreshPrecompensationStatus();
			}

			void RefreshPrecompensationStatus(){
				// retrieves and displays current write pre-compensation status
				// . assuming pre-compensation determination error
				const RECT rcWarning=MapDlgItemClientRect(ID_INSTRUCTION);
				ShowDlgItem( ID_INSTRUCTION, true );
				RECT rcMessage=MapDlgItemClientRect(ID_ALIGN);
				rcMessage.left=rcWarning.right;
				SetDlgItemPos( ID_ALIGN, rcMessage );
				// . displaying current pre-compensation status
				TCHAR msg[235];
				switch (const TStdWinError err=tmpPrecomp.DetermineUsingLatestMethod(rcb,0)){
					case ERROR_SUCCESS:
						ShowDlgItem( ID_INSTRUCTION, false );
						rcMessage.left=rcWarning.left;
						SetDlgItemPos( ID_ALIGN, rcMessage );
						::wsprintf( msg, _T("Determined for medium in Drive %c using latest <a id=\"details\">Method %d</a>. No action needed now."), tmpPrecomp.driveLetter, tmpPrecomp.methodVersion );
						break;
					case ERROR_INVALID_DATA:
						::wsprintf( msg, _T("Not determined for medium in Drive %c!\n<a id=\"compute\">Determine now using latest Method %d</a>"), tmpPrecomp.driveLetter, CPrecompensation::MethodLatest );
						break;
					case ERROR_EVT_VERSION_TOO_OLD:
						::wsprintf( msg, _T("Determined for medium using <a id=\"details\">Method %d</a>. <a id=\"compute\">Redetermine using latest Method %d</a>"), tmpPrecomp.methodVersion, CPrecompensation::MethodLatest );
						break;
					case ERROR_UNRECOGNIZED_MEDIA:
						::wsprintf( msg, _T("Unknown medium in Drive %c.\n<a id=\"details\">Determine even so using latest Method %d</a>"), tmpPrecomp.driveLetter, CPrecompensation::MethodLatest );
						break;
					default:
						::FormatMessage(
							FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, 0,
							msg+::lstrlen(::lstrcpy(msg,_T("Couldn't determine status because\n"))), sizeof(msg)-35,
							nullptr
						);
						break;
				}
				SetDlgItemText( ID_ALIGN, msg );
			}

			void PreInitDialog() override{
				// dialog initialization
				// . base
				__super::PreInitDialog();
				// . displaying Firmware information
				SetDlgItemText( ID_SYSTEM, firmware );
				// . populating combo-box with available DecoderMethods
				for( BYTE dm=1; dm; dm<<=1 )
					if (dm&TFluxDecoder::FDD_METHODS)
						AppendDlgComboBoxValue( ID_ACCURACY, dm, CTrackReader::GetDescription((TFluxDecoder)dm) );
				// . some settings are changeable only during InitialEditing
				PopulateComboBoxWithCompatibleMedia(
					GetDlgItemHwnd(ID_VARIABLE),
					Medium::ANY,
					rcb.properties
				);
				EnableDlgItems( InitialSettingIds, initialEditing );
				EnableDlgItem( ID_READABLE, params.calibrationAfterError!=TParams::TCalibrationAfterError::NONE );
				SetDlgItemSingleCharUsingFont( // a warning that No decoder selected (archivation)
					ID_HIDDEN, L'\xf0ea', warningFont
				);
				// . if DoubleTrackStep changed manually, adjusting the text of the ID_40D80 checkbox
				SetDlgItemSingleCharUsingFont( ID_RECOVER, 0xf071, Utils::CRideFont::Webdings120 );
				ConvertDlgCheckboxToHyperlink( ID_SIDE );
				GetDlgItemText( ID_SIDE,   flippyDiskTextOrg );
				if (CheckDlgItem(ID_SIDE,rcb.params.flippyDisk) && rcb.params.userForcedFlippyDisk)
					SendMessage( WM_COMMAND, ID_SIDE );
				GetDlgItemText( ID_40D80,  doubleTrackDistanceTextOrg );
				if (rcb.params.userForcedDoubleTrackStep)
					SendMessage( WM_COMMAND, ID_40D80 );
				CheckAndEnableDlgItem( ID_40D80,
					rcb.params.doubleTrackStep,
					!CheckDlgItem( ID_DRIVE, rcb.params.fortyTrackDrive )
				);
				// . displaying inserted Medium information
				SetDlgItemSingleCharUsingFont( // a warning that a 40-track disk might have been misrecognized
					ID_INFORMATION, L'\xf0ea', warningFont
				);
				ConvertDlgCheckboxToHyperlink( ID_40D80 );
				CRect rc;
				rc.UnionRect( &MapDlgItemClientRect(ID_MEDIUM), &MapDlgItemClientRect(ID_RECOVER) );
				SetDlgItemPos( ID_VARIABLE, rc );
				ShowDlgItems( AutoRecognizedMediumIds, !ShowDlgItem(ID_VARIABLE,params.userForcedMedium) );
				if (params.userForcedMedium)
					SelectDlgComboBoxValue( ID_VARIABLE, rcb.floppyType );
				// . updating write pre-compensation status
				SetDlgItemSingleCharUsingFont( // a warning that pre-compensation not up-to-date
					ID_INSTRUCTION, L'\xf0ea', warningFont
				);
				// . adjusting calibration possibilities
				extern CDos::PCProperties manuallyForceDos;
				if (EnableDlgItem( ID_READABLE,
						!rcb.dos && manuallyForceDos->IsKnown() // DOS now yet known: either automatic DOS recognition, or manual selection of DOS but Unknown
						||
						rcb.dos->IsKnown() // DOS already known: it's NOT the Unknown DOS
					)
				)
					CheckDlgButton( ID_READABLE, false ); // this option is never ticked for Unknown DOS
				ConvertDlgCheckboxToHyperlink( ID_TRACK );
			}

			void DoDataExchange(CDataExchange* pDX) override{
				// exchange of data from and to controls
				static constexpr WORD DeviceOnlyIds[]={
					ID_ROTATION, // precision
					ID_NONE, ID_CYLINDER, ID_SECTOR, ID_READABLE, // calibration upon read failure
					ID_ZERO, ID_CYLINDER_N, ID_ADD, // calibration upon write failure
					ID_VERIFY_TRACK, ID_VERIFY_SECTOR,
					0
				};
				const bool isRealDevice=rcb.properties->IsRealDevice();
				// . Precision
				int tmp=params.precision/2-1;
				DDX_CBIndex( pDX, ID_ROTATION,	tmp );
				params.precision=(TParams::TPrecision)((tmp+1)*2);
				if (!EnableDlgItems( DeviceOnlyIds, isRealDevice )){
					CComboBox cb;
					cb.Attach( GetDlgItemHwnd(ID_ROTATION) );
						const int i=cb.GetCurSel();
						cb.DeleteString( i );
						cb.InsertString( i, _T("Automatically") );
						cb.SetCurSel( i );
					cb.Detach();
				}
				// . FluxDecoder
				DDX_CBValue( pDX, ID_ACCURACY, params.fluxDecoder );
				DDX_Check( pDX, ID_DEFAULT1,	params.resetFluxDecoderOnIndex );
				// . manually set Medium
				if (params.userForcedMedium)
					DDX_CBValue( pDX, ID_VARIABLE, rcb.floppyType );
				// . CalibrationAfterError
				tmp= isRealDevice ? params.calibrationAfterError : TParams::TCalibrationAfterError::NONE;
				DDX_Radio( pDX,	ID_NONE,		tmp );
				params.calibrationAfterError=(TParams::TCalibrationAfterError)tmp;
				DDX_Check( pDX, ID_READABLE,	params.calibrationAfterErrorOnlyForKnownSectors );
				// . CalibrationStepDuringFormatting
				EnableDlgItem( ID_NUMBER, tmp=params.calibrationStepDuringFormatting!=0 );
				DDX_Radio( pDX,	ID_ZERO,		tmp );
				if (tmp)
					DDX_Text( pDX,	ID_NUMBER,	tmp=params.calibrationStepDuringFormatting );
				else
					SetDlgItemInt(ID_NUMBER,4);
				params.calibrationStepDuringFormatting=tmp;
				// . NormalizeReadTracks
				tmp=params.corrections.use;
				DDX_Check( pDX,	ID_TRACK,		tmp );
				params.corrections.use=tmp!=0;
				EnableDlgItem( ID_TRACK, params.fluxDecoder!=TFluxDecoder::NONE );
				// . WrittenTracksVerification
				DDX_Check( pDX,	ID_VERIFY_TRACK, params.verifyWrittenTracks&=isRealDevice );
				DDX_CheckEnable( pDX, ID_VERIFY_SECTOR, params.verifyBadSectors&=isRealDevice, params.verifyWrittenTracks );
				// . finish dialog initialization
				if (!pDX->m_bSaveAndValidate){ // still initializing?
					SendMessage( WM_COMMAND, MAKELONG(ID_ACCURACY,CBN_SELCHANGE) );
					if (!initialEditing)
						EnableDlgItems( InitialSettingIds, false ); // disable what should be disabled (just to be sure)
				}
			}

			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				switch (msg){
					case WM_PAINT:
						// drawing
						__super::OnPaint();
						if (IsDlgItemShown(ID_INFORMATION))
							WrapDlgItemsByOpeningCurlyBracket( ID_DRIVE, ID_40D80 );
						WrapDlgItemsByClosingCurlyBracketWithText( ID_NONE, ID_READABLE, _T("on read error"), 0 );
						WrapDlgItemsByClosingCurlyBracketWithText( ID_ZERO, ID_CYLINDER_N, _T("when writing"), 0 );
						return 0;
					case WM_COMMAND:
						switch (wParam){
							case MAKELONG(ID_ACCURACY,CBN_SELCHANGE):{
								// FluxDecoder changed
								const Utils::CVarTempReset<TFluxDecoder> fd0( rcb.params.fluxDecoder, (TFluxDecoder)GetDlgComboBoxSelectedValue(ID_ACCURACY) );
								if (ShowDlgItem(  ID_HIDDEN,  !EnableDlgItem( ID_TRACK, rcb.params.fluxDecoder!=TFluxDecoder::NONE )  ))
									CheckDlgButton( ID_TRACK, BST_UNCHECKED ); // when archiving, any corrections must be turned off
								SendMessage( WM_COMMAND, ID_RECOVER ); // refresh information on inserted Medium
								break;
							}
							case MAKELONG(ID_VARIABLE,CBN_SELCHANGE):
								currentMediumType=(Medium::TType)GetDlgComboBoxSelectedValue(ID_VARIABLE);
								RefreshMediumInformation();
								break;
							case ID_RECOVER:
								// refreshing information on (inserted) floppy
								if (initialEditing){ // if no Tracks are yet formatted, then resetting the flag that user has overridden these settings
									CheckDlgItem( ID_DRIVE, CheckDlgItem(ID_SIDE,false) );
									SetDlgItemText( ID_SIDE, flippyDiskTextOrg );
									SetDlgItemText( ID_40D80, doubleTrackDistanceTextOrg );
									if (rcb.properties->IsRealDevice())
										rcb.Reset(); // reset connection (e.g. COM port) to real Devices only, given 'Reset' doesn't implicitly dispose all Tracks for them
								}
								RefreshMediumInformation();
								break;
							case ID_SIDE:
								// flippy disk setting changed manually
								SetDlgItemFormattedText( ID_SIDE, _T("%s (user forced)"), flippyDiskTextOrg );
								break;
							case ID_DRIVE:
								// drive physical track range changed manually
								CheckAndEnableDlgItem( ID_40D80, false, !IsDlgItemChecked(ID_DRIVE) );
								//fallthrough
							case ID_40D80:
								// track distance changed manually
								if (wParam==ID_40D80){
									TCHAR tmp[80];
									::lstrcpy(
										_tcsrchr( ::lstrcpy(tmp,doubleTrackDistanceTextOrg), '(' ),
										_T("(user forced)")
									);
									SetDlgItemText( ID_40D80, tmp );
								}
								ShowDlgItem( ID_INFORMATION, false ); // user manually revised # of Tracks either on Medium's or Drive's side, so no need to continue displaying the warning
								Invalidate(); // get also rid of the curly bracket
								break;
							case ID_NONE:
							case ID_CYLINDER:
							case ID_SECTOR:
								// adjusting possibility to edit controls that depend on CalibrationAfterError
								EnableDlgItem( ID_READABLE, wParam!=ID_NONE );
								break;
							case ID_ZERO:
							case ID_CYLINDER_N:
								// adjusting possibility to edit the CalibrationStep according to selected option
								EnableDlgItem( ID_NUMBER, wParam!=ID_ZERO );
								break;
							case MAKELONG(ID_VERIFY_TRACK,BN_CLICKED):
								// whole Track verification
								EnableDlgItem( ID_VERIFY_SECTOR, IsDlgItemChecked(ID_VERIFY_TRACK) );
								break;
							case IDOK:
								// attempting to confirm the Dialog
								params.flippyDisk=IsDlgItemChecked(ID_SIDE);
								params.userForcedFlippyDisk=IsFlippyDiskForcedByUser();
								params.fortyTrackDrive=IsDlgItemChecked(ID_DRIVE);
								params.doubleTrackStep=IsDlgItemChecked(ID_40D80);
								params.userForcedDoubleTrackStep=IsDoubleTrackDistanceForcedByUser();
								break;
						}
						break;
					case WM_NOTIFY:
						switch (GetClickedHyperlinkId(lParam)){
							case ID_MEDIUM:
								ShowDlgItems(AutoRecognizedMediumIds,false);
								params.userForcedMedium=ShowDlgItem(ID_VARIABLE);
								break;
							case ID_ALIGN:
								rcb.locker.Unlock(); // giving way to parallel thread
						{			const Utils::CVarTempReset<bool> vwt0( params.verifyWrittenTracks, false );
									const LPCWSTR strId=((PNMLINK)lParam)->item.szID;
									if (!::lstrcmpW(strId,L"details"))
										tmpPrecomp.ShowOrDetermineModal(rcb);
									else if (!::lstrcmpW(strId,L"compute"))
										if (const TStdWinError err=tmpPrecomp.DetermineUsingLatestMethod(rcb))
											Utils::FatalError( _T("Can't determine precompensation"), err );
										else
											tmpPrecomp.Save();
						}		rcb.locker.Lock();
								RefreshMediumInformation();
								break;
							case ID_SIDE:
								Utils::NavigateToFaqInDefaultBrowser( _T("flippyDisk") );
								break;
							case ID_40D80:{
								const Utils::CVarTempReset<bool> dts0( rcb.params.doubleTrackStep, false );
								const Utils::CVarTempReset<Medium::TType> ft0( rcb.floppyType, currentMediumType );
								const CTrackTempReset rit( rcb.internalTracks[1][0] );
								rcb.ScanTrack( 1, 0 );
								Utils::Information(
									CString(_T("Sectors on physical Track 1 (Cyl=1, Head=0) of assumed 80-track drive:\n\n")) + rcb.ListSectors(1,0)
								);
								break;
							}
							case ID_TRACK:
								params.corrections.ShowModal(this);
								break;
						}
						break;
				}
				return __super::WindowProc(msg,wParam,lParam);
			}
		public:
			TParams params;

			CParamsDialog(CCapsBase &rcb,LPCTSTR firmware,bool initialEditing)
				// ctor
				: Utils::CRideDialog(IDR_CAPS_DEVICE_ACCESS)
				, warningFont( Utils::CRideFont::Webdings175 )
				, rcb(rcb) , params(rcb.params) , initialEditing(initialEditing) , firmware(firmware)
				, currentMediumType(rcb.floppyType)
				, tmpPrecomp(rcb.precompensation) {
			}
		} d( rcb, firmware, initialEditing );
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			*this=d.params;
			rcb.capsImageInfo.maxcylinder=( FDD_CYLINDERS_HD>>(BYTE)(doubleTrackStep||fortyTrackDrive) )+FDD_CYLINDERS_EXTRA - 1; // "-1" = inclusive!
			return true;
		}else
			return false;
	}

	void CCapsBase::TParams::EnumSettings(CSettings &rOut,bool isRealDevice) const{
		// returns a collection of relevant settings for this Image
		rOut.Add( _T("decoder"), CTrackReader::GetDescription(fluxDecoder) );
		rOut.Add( _T("reset on index"), resetFluxDecoderOnIndex );
		if (isRealDevice)
			rOut.AddRevolutionCount( PrecisionToFullRevolutionCount() );
		rOut.AddMediumIsForced(userForcedMedium);
		rOut.AddMediumIsFlippy( flippyDisk, userForcedFlippyDisk );
		rOut.Add40TrackDrive(fortyTrackDrive);
		rOut.AddDoubleTrackStep( doubleTrackStep, userForcedDoubleTrackStep );
		//TODO: calibrationAfterError (calibrationAfterErrorOnlyForKnownSectors)
		corrections.EnumSettings(rOut);
	}












	CCapsBase::TCorrections::TCorrections()
		// ctor of "no" Corrections
		: valid(true)
		, use(false) {
	}
	
	CCapsBase::TCorrections::TCorrections(LPCTSTR iniSection,LPCTSTR iniName)
		// ctor
		// - the defaults
		: valid(true)
		, use(false)
		, indexTiming(true)
		, cellCountPerTrack(true)
		, fitFluxesIntoIwMiddles(true)
		, offsetIndices(false)
		, indexOffsetMicroseconds(1500) {
		// - attempting to load existing values from last session
		if (const DWORD settings=app.GetProfileInt(iniSection,iniName,0)) // do Valid settings exist?
			*(PDWORD)this=settings;
	}

	void CCapsBase::TCorrections::Save(LPCTSTR iniSection,LPCTSTR iniName) const{
		// dtor
		app.WriteProfileInt( iniSection, iniName, *(PDWORD)this );
	}

	bool CCapsBase::TCorrections::ShowModal(CWnd *pParentWnd){
		// shows a dialog with exposed settings
		// - defining the Dialog
		class CCorrectionsDialog sealed:public Utils::CRideDialog{
			void DoDataExchange(CDataExchange *pDX) override{
				__super::DoDataExchange(pDX);
				int tmp=corr.indexTiming;
					DDX_Check( pDX, ID_ALIGN,	tmp );
				corr.indexTiming=tmp!=BST_UNCHECKED;
				tmp=corr.cellCountPerTrack;
					DDX_Check( pDX, ID_NUMBER, tmp );
				corr.cellCountPerTrack=tmp!=BST_UNCHECKED;
				tmp=corr.fitFluxesIntoIwMiddles;
					DDX_Check( pDX, ID_ACCURACY, tmp );
				corr.fitFluxesIntoIwMiddles=tmp!=BST_UNCHECKED;
				tmp=corr.offsetIndices;
					DDX_Check( pDX, ID_ADDRESS, tmp );
				corr.offsetIndices=tmp!=BST_UNCHECKED;
				tmp=corr.indexOffsetMicroseconds;
					DDX_Text( pDX, ID_TIME, tmp );
						DDV_MinMaxInt( pDX, tmp, SHRT_MIN, SHRT_MAX );
				corr.indexOffsetMicroseconds=tmp;
			}
		public:
			TCorrections corr;

			CCorrectionsDialog(const TCorrections &c,CWnd *pParentWnd)
				: Utils::CRideDialog( IDR_CAPS_CORRECTIONS, pParentWnd )
				, corr(c) {
			}
		} d( *this, pParentWnd );
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			*this=d.corr;
			return true;
		}else
			return false;
	}

	TStdWinError CCapsBase::TCorrections::ApplyTo(CTrackReaderWriter &trw) const{
		// attempts to apply current Correction settings to the specified Track; returns Windows standard i/o error
		ASSERT( valid );
		if (use)
			return	trw.NormalizeEx(
						offsetIndices ? TIME_MICRO(indexOffsetMicroseconds) : 0, // micro- to nanoseconds
						fitFluxesIntoIwMiddles,
						cellCountPerTrack,
						indexTiming
					);
		else
			return ERROR_SUCCESS;
	}

	void CCapsBase::TCorrections::EnumSettings(CSettings &rOut) const{
		// returns a collection of relevant settings for this Image
		TCHAR buf[256], *p=buf;
		*p++='{';
		if (use){
			if (indexTiming)
				p+=::wsprintf( p, _T("revolution time, ") );
			if (cellCountPerTrack)
				p+=::wsprintf( p, _T("bit count, ") );
			if (fitFluxesIntoIwMiddles)
				p+=::wsprintf( p, _T("bit positions, ") );
			if (offsetIndices)
				p+=::wsprintf( p, _T("indices offset %d us, "), indexOffsetMicroseconds );
			if (p>buf+1) // more than just the opening '{' bracket?
				p-=2; // drop tail comma
		}
		::lstrcpy( p, _T("}") );
		rOut.SetAt( _T("corrections"), buf );
	}
