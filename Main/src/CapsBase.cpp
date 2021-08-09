#include "stdafx.h"
#include "CapsBase.h"
#include "MSDOS7.h"

	static TStdWinError InitCapsLibrary(CapsVersionInfo &cvi){
		// - checking version
		if (CAPS::GetVersionInfo(&cvi,0) || cvi.release<5 || cvi.release==5&&cvi.revision<1){
			Utils::FatalError(_T("CAPS library outdated, 5.1 or newer required!"));
			return ERROR_EVT_VERSION_TOO_OLD;
		}
		// - initializing the library
		if (CAPS::Init())
			return ERROR_DLL_INIT_FAILED;
		// - initialized successfully, can now use the library
		return ERROR_SUCCESS;
	}

	CCapsBase::CCapsBase(PCProperties properties,char realDriveLetter,bool hasEditableSettings)
		// ctor
		// - base
		: CFloppyImage(properties,hasEditableSettings)
		// - loading the CAPS library
		, capsLibLoadingError( ::InitCapsLibrary(capsVersionInfo) )
		// - creating a CAPS device
		, capsDeviceHandle(  capsLibLoadingError ? -1 : CAPS::AddImage()  )
		// - initialization
		, precompensation(realDriveLetter)
		, forcedMediumType( Medium::FLOPPY_ANY )
		, lastSuccessfullCodec(Codec::MFM) {
		::ZeroMemory( &capsImageInfo, sizeof(capsImageInfo) );
		::ZeroMemory( internalTracks, sizeof(internalTracks) );
	}

	CCapsBase::~CCapsBase(){
		// dtor
		// - destroying all Tracks
		Reset();
		// - unloading the CAPS library (and thus destroying all data created during this session)
		if (!capsLibLoadingError)
			CAPS::Exit();
	}










	CCapsBase::CBitReader::CBitReader(const CapsTrackInfoT2 &cti,UDWORD lockFlags)
		// ctor to iterate over bits on all available disk revolutions
		: pCurrByte(cti.trackbuf-1) , currBitMask(0) // pointing "before" the first valid bit
		, nRemainingBits( lockFlags&DI_LOCK_TRKBIT ? cti.tracklen : cti.tracklen*8 )
		, Count(nRemainingBits) {
	}

	CCapsBase::CBitReader::CBitReader(const CBitReader &rBitReader,UDWORD position)
		// copy ctor
		: pCurrByte(rBitReader.pCurrByte) , currBitMask(rBitReader.currBitMask)
		, nRemainingBits(rBitReader.nRemainingBits)
		, Count(rBitReader.Count) {
		SeekTo(position);
	}

	inline
	CCapsBase::CBitReader::operator bool() const{
		// True <=> not all bits yet read, otherwise False
		return nRemainingBits>0;
	}

	bool CCapsBase::CBitReader::ReadBit(){
		// returns first bit not yet read
		#ifdef _DEBUG
			if (!*this)
				ASSERT(FALSE); // this method mustn't be called when there's nothing actually to be read!
		#endif
		if ((currBitMask>>=1)==0)
			pCurrByte++, currBitMask=0x80;
		nRemainingBits--;
		return (*pCurrByte&currBitMask)!=0;
	}

	bool CCapsBase::CBitReader::ReadBits16(WORD &rOut){
		// True <=> at least 16 bits have not yet been read, otherwise False
		if (nRemainingBits<16)
			return false;
		for( BYTE n=16; n-->0; rOut=(rOut<<1)|(BYTE)ReadBit() );
		return true;
	}

	bool CCapsBase::CBitReader::ReadBits32(DWORD &rOut){
		// True <=> at least 32 bits have not yet been read, otherwise False
		if (nRemainingBits<32)
			return false;
		for( BYTE n=32; n-->0; rOut=(rOut<<1)|(BYTE)ReadBit() );
		return true;
	}

	inline
	UDWORD CCapsBase::CBitReader::GetPosition() const{
		// returns the number of bits already read, applicable only to THIS BitReader!
		return Count-nRemainingBits;
	}

	void CCapsBase::CBitReader::SeekTo(UDWORD pos){
		// sets current position returned earlier from the GetPosition method
		int diff=pos-GetPosition();
		if (diff>0){
			for( BYTE nBits=diff&7; nBits-->0; ReadBit() );
			pCurrByte+=diff>>3;
		}else if (diff<0){
			diff=-diff;
			for( BYTE nBits=diff&7; nBits-->0; )
				if ((currBitMask<<=1)==0)
					pCurrByte--, currBitMask=0x01;
			pCurrByte-=diff>>3;
		}
		nRemainingBits=Count-pos;
	}

	inline
	void CCapsBase::CBitReader::SeekToBegin(){
		// sets current position to the beginning of the bit stream
		SeekTo(0);
	}











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
		, nSectors(nSectors) , sectors( nSectors, sectors ) {
	}

	CCapsBase::CInternalTrack::~CInternalTrack(){
		// dtor
		for( TSector i=0; i<nSectors; i++ )
			for( BYTE r=0; r<Revolution::MAX; r++ )
				if (const PVOID data=sectors[i].revolutions[r].data)
					::free(data);
	}

	CCapsBase::CInternalTrack *CCapsBase::CInternalTrack::CreateFrom(const CCapsBase &cb,const CapsTrackInfoT2 *ctiRevs,BYTE nRevs,UDWORD lockFlags){
		// creates and returns a Track decoded from underlying CAPS Track representation
		// - at least one full Revolution must be available
		if (!nRevs)
			return nullptr;
		// - reconstructing flux information over all Revolutions of the disk
		nRevs=std::min( nRevs, (BYTE)CAPS_MTRS ); // just to be sure we don't overrun the buffers
		UDWORD nBitsPerTrack[CAPS_MTRS], nBitsPerTrackOfficial, nBitsTotally=0;
		for( BYTE rev=0; rev<nRevs; rev++ )
			nBitsTotally += nBitsPerTrack[rev] = CBitReader(ctiRevs[rev],lockFlags).Count;
		CTrackReaderWriter trw( nBitsTotally*125/100, CTrackReader::FDD_KEIR_FRASER, true ); // pessimistic estimation of # of fluxes; allowing for 25% of false "ones" introduced by "FDC-like" decoders
			if (cb.floppyType!=Medium::UNKNOWN && !ctiRevs[0].timelen){
				// Medium already known and the CAPS Track does NOT contain explicit timing information
				nBitsPerTrackOfficial=Medium::GetProperties(cb.floppyType)->nCells;
				trw.SetMediumType(cb.floppyType); // adopting the Medium
			}else if (*nBitsPerTrack>( nBitsPerTrackOfficial=Medium::TProperties::FLOPPY_HD_350.nCells )*95/100) // 5% tolerance
				// likely a 3.5" HD medium
				trw.SetMediumType( Medium::FLOPPY_HD_350 );
			else if (*nBitsPerTrack>( nBitsPerTrackOfficial=Medium::TProperties::FLOPPY_HD_525.nCells )*95/100) // 5% tolerance
				// likely a 5.25" HD medium
				trw.SetMediumType( Medium::FLOPPY_HD_525 );
			else if (*nBitsPerTrack>( nBitsPerTrackOfficial=Medium::TProperties::FLOPPY_DD.nCells )*95/100) // 5% tolerance
				// likely a 3.5" DD or 5.25" medium in 300 RPM drive
				trw.SetMediumType( Medium::FLOPPY_DD );
			else if (*nBitsPerTrack>( nBitsPerTrackOfficial=Medium::TProperties::FLOPPY_DD_525.nCells )*95/100) // 5% tolerance
				// likely a 5.25" DD medium in 360 RPM drive
				trw.SetMediumType( Medium::FLOPPY_DD_525 );
			else{
				ASSERT(FALSE); //TODO: 8" SD medium
				return nullptr;
			}
		trw.AddIndexTime(0);
		const TLogTime fullRevolutionTime=nBitsPerTrackOfficial*trw.GetCurrentProfile().iwTimeDefault;
		TLogTime currentTime=0, *pFluxTimeBuffer=trw.GetBuffer(), *pFluxTime=pFluxTimeBuffer;
		TLogTime nextIndexTime=fullRevolutionTime;
		for( BYTE rev=0; rev<nRevs; ){
			const CapsTrackInfoT2 &cti=ctiRevs[rev++];
			for( CBitReader br(cti,lockFlags); br; ){
				// . adding new index
				if (currentTime>=nextIndexTime){
					trw.AddIndexTime( nextIndexTime );
					if (rev<nRevs){ // if more Revolutions to follow ...
						currentTime=nextIndexTime;
						nextIndexTime+=fullRevolutionTime;
						break; // ... mustn't overlap into them
					}else
						nextIndexTime+=fullRevolutionTime;
				}
				// . adding new flux
				const UDWORD i=br.GetPosition()>>3;
				if (i<cti.timelen)
					currentTime+= trw.GetCurrentProfile().iwTimeDefault * cti.timebuf[i]/1000;
				else
					currentTime+= trw.GetCurrentProfile().iwTimeDefault;
				if (br.ReadBit())
					*pFluxTime++=currentTime;
			}
		}
		trw.AddTimes( pFluxTimeBuffer, pFluxTime-pFluxTimeBuffer );
		if (trw.GetIndexCount()<=nRevs) // an IPF image may end up here
			trw.AddIndexTime(currentTime);
		// - creating a Track from above reconstructed flux information
		return CreateFrom( cb, trw );
	}

	CCapsBase::CInternalTrack *CCapsBase::CInternalTrack::CreateFrom(const CCapsBase &cb,const CTrackReaderWriter &trw){
		// creates and returns a Track decoded from underlying flux representation
		CTrackReader tr=trw;
		Codec::TType c=cb.lastSuccessfullCodec; // turning first to the Codec that successfully decoded the previous Track
		for( Codec::TTypeSet codecs=Codec::ANY,next=1; codecs!=0; c=(Codec::TType)next ){
			// . determining the Codec to be used in the NEXT iteration for decoding
			for( codecs&=~c; (codecs&next)==0&&(next&Codec::ANY)!=0; next<<=1 );
			// . scanning the Track and if no Sector recognized, continuing with Next Codec
			TSectorId ids[Revolution::MAX*(TSector)-1]; TLogTime idEnds[Revolution::MAX*(TSector)-1]; TProfile idProfiles[Revolution::MAX*(TSector)-1]; TFdcStatus statuses[Revolution::MAX*(TSector)-1];
			tr.SetCodec(c);
			const WORD nSectorsFound=tr.Scan( ids, idEnds, idProfiles, statuses );
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
			for( BYTE rev=0; rev<tr.GetIndexCount()-1; rev++ ){
				const TLogTime revEndTime=tr.GetIndexTime(rev+1); // revolution end Time
				for( start=end; idEnds[end]<revEndTime; end++ );
				lcs.Merge( rev, end-start, ids+start, idEnds+start, idProfiles+start, statuses+start );
			}
			for( TSector s=0; s<lcs.nUniqueSectors; s++ ){
				auto &rsi=lcs.uniqueSectors[s];
				rsi.nRevolutions=std::max( 1, tr.GetIndexCount()-1 );
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
		else if (!currRev.data){ // data not yet buffered
			// at least Sector's ID Field found in specified Revolution
			const WORD sectorOfficialLength=ris.GetOfficialSectorLength();
			BYTE buffer[16384]; // big enough to contain the longest possible Sector
			currRev.fdcStatus.ExtendWith(
				ReadData(
					currRev.idEndTime, currRev.idEndProfile,
					sectorOfficialLength, buffer
				)
			);
			if (!currRev.fdcStatus.DescribesMissingDam()) // "some" data found
				currRev.data=(PSectorData)::memcpy( ::malloc(sectorOfficialLength), buffer, sectorOfficialLength );
		}
	}

	void CCapsBase::CInternalTrack::FlushSectorBuffers(){
		// spreads referential "dirty" data (if Sector modified) across each Revolution
		for( TSector s=0; s<nSectors; s++ ){
			const TInternalSector &ris=sectors[s];
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
		}
		//modified=false; // commented out as the Track hasn't yet been saved!
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
		// - "mounting" the Image file to the Device
		char fileName[MAX_PATH];
		#ifdef UNICODE
			x//TODO
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
		// - confirming initial settings
		if (!EditSettings(true)){ // dialog cancelled?
			::SetLastError( ERROR_CANCELLED );
			return FALSE;
		}
		// - warning
		if (capsImageInfo.maxcylinder>=FDD_CYLINDERS_MAX){ // inclusive!
			TCHAR msg[200];
			::wsprintf( msg, _T("The image contains %d cylinders, ") APP_ABBREVIATION _T(" shows just first %d of them."), capsImageInfo.maxcylinder+1, FDD_CYLINDERS_MAX );
			Utils::Warning(msg);
			capsImageInfo.maxcylinder=FDD_CYLINDERS_MAX-1; // inclusive!
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

	BYTE CCapsBase::GetAvailableRevolutionCount() const{
		// returns the number of data variations of one Sector that are guaranteed to be distinct
		return 4;
	}

	TSector CCapsBase::ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec,PSectorId bufferId,PWORD bufferLength,PLogTime startTimesNanoseconds,PBYTE pAvgGap3) const{
		// returns the number of Sectors found in given Track, and eventually populates the Buffer with their IDs (if Buffer!=Null); returns 0 if Track not formatted or not found
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return 0;
		// - if Track doesn't exists yet (e.g. not created by a derived class), reading it by the CAPS library
		if (internalTracks[cyl][head]==nullptr){
			static constexpr CapsTrackInfoT2 CtiEmpty={2};
			const UDWORD lockFlags= capsVersionInfo.flag&( DI_LOCK_INDEX | DI_LOCK_DENVAR | DI_LOCK_DENAUTO | DI_LOCK_DENNOISE | DI_LOCK_NOISE | DI_LOCK_TYPE | DI_LOCK_OVLBIT | DI_LOCK_TRKBIT | DI_LOCK_UPDATEFD );
			CapsTrackInfoT2 cti[CAPS_MTRS];
			*cti=CtiEmpty;
			if (CAPS::LockTrack( cti, capsDeviceHandle, cyl, head, lockFlags )!=imgeOk
				||
				(cti->type&CTIT_MASK_TYPE)==ctitNA // error during Track retrieval
			)
				return 0;
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
			if (const PInternalTrack tmp = CInternalTrack::CreateFrom( *this, cti, nRevs, lockFlags )){
				CTrackReaderWriter trw=*tmp; // extracting raw flux data ...
					trw.SetMediumType(floppyType);
				delete tmp;
				internalTracks[cyl][head] = CInternalTrack::CreateFrom( *this, trw ); // ... and rescanning the Track using current FloppyType Profile
			}
			while (--nRevs>0){
				const CapsTrackInfoT2 &r=cti[nRevs];
				::free(r.trackbuf), ::free(r.timebuf);
			}
			CAPS::UnlockTrack( capsDeviceHandle, cyl, head );
		}
		// - scanning the Track
		if (const PCInternalTrack pit=internalTracks[cyl][head]){
			for( TSector s=0; s<pit->nSectors; s++ ){
				const TInternalSector &ris=pit->sectors[s];
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
			return pit->nSectors;
		}else
			return 0;
	}

	void CCapsBase::GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses){
		// populates output buffers with specified Sectors' data, usable lengths, and FDC statuses; ALWAYS attempts to buffer all Sectors - caller is then to sort out eventual read errors (by observing the FDC statuses); caller can call ::GetLastError to discover the error for the last Sector in the input list
		ASSERT( outBufferData!=nullptr && outBufferLengths!=nullptr && outFdcStatuses!=nullptr );
		silentlyRecoverFromErrors&=rev>=Revolution::MAX; // can't recover if wanted particular Revolution
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (internalTracks[cyl][head]==nullptr)
			ScanTrack(cyl,head); // reading the Track (if not yet read)
		if (const PInternalTrack pit=internalTracks[cyl][head])
			while (nSectors-->0){
				// . searching for the Sector on the Track
				const TSectorId sectorId=*bufferId++;
				TInternalSector *pis=pit->sectors;
				TSector n=pit->nSectors;
				for( BYTE nSectorsToSkip=*bufferNumbersOfSectorsToSkip++; n>0; n--,pis++ )
					if (nSectorsToSkip)
						nSectorsToSkip--;
					else if (pis->id==sectorId) // Sector IDs are equal
						break;
				// . if Sector with given ID not found in the Track, we are done
				if (!n){
					*outBufferData++=nullptr, *outFdcStatuses++=TFdcStatus::SectorNotFound;
					outBufferLengths++;
					continue;
				}
				// . if Data already read WithoutError, returning them
				if (pis->dirtyRevolution<Revolution::MAX){
					rev = pis->dirtyRevolution; // modified Revolution is obligatory for any subsequent data requests
					silentlyRecoverFromErrors=false; // can't recover if one particular Revolution already modified
				}
				const WORD usableSectorLength=GetUsableSectorLength(sectorId.lengthCode);
				auto *currRev=pis->revolutions+( rev<Revolution::MAX ? pis->currentRevolution=rev : pis->currentRevolution );
				if (currRev->data || currRev->fdcStatus.DescribesMissingDam()) // A|B, A = some data exist, B = reattempting to read the DAM-less Sector only if automatic recovery desired
					if (currRev->fdcStatus.IsWithoutError() || !silentlyRecoverFromErrors){ // A|B, A = returning error-free data, B = settling with any data if automatic recovery not desired
returnData:				*outFdcStatuses++=currRev->fdcStatus;
						*outBufferData++=currRev->data;
						*outBufferLengths++=usableSectorLength;
						continue;
					}
				// . attempting next disk Revolution to retrieve healthy Data
				if (usableSectorLength!=0){ // e.g. Sector with LengthCode 167 has no data
					::free(currRev->data), currRev->data=nullptr;
					if (rev<pis->nRevolutions) // wanted particular EXISTING Revolution
						pit->ReadSector( *pis, pis->nRevolutions>1?rev:0 );
					else if (rev>=Revolution::MAX){ // wanted any Revolution
						do{
							if (++pis->currentRevolution>=pis->nRevolutions)
								pis->currentRevolution=0;
							currRev=pis->revolutions+pis->currentRevolution;
						}while (currRev->idEndTime<=0);
						pit->ReadSector( *pis, pis->currentRevolution );
					}
				}
				// . returning (any) Data
				goto returnData;
			}
		else
			while (nSectors-->0)
				*outBufferData++=nullptr, *outFdcStatuses++=TFdcStatus::SectorNotFound;
		::SetLastError( *--outBufferData ? ERROR_SUCCESS : ERROR_SECTOR_NOT_FOUND );
	}

	Revolution::TType CCapsBase::GetDirtyRevolution(RCPhysicalAddress chs,BYTE nSectorsToSkip) const{
		// returns the Revolution that has been marked as "dirty"
		if (const PCInternalTrack pit=internalTracks[chs.cylinder][chs.head]){
			while (nSectorsToSkip<pit->nSectors){
				const auto &ris=pit->sectors[nSectorsToSkip++];
				if (ris.id==chs.sectorId)
					return ris.dirtyRevolution;
			}
			return Revolution::UNKNOWN; // unknown Sector queried
		}else
			return Revolution::NONE; // not modified yet
	}

	TStdWinError CCapsBase::GetInsertedMediumType(TCylinder cyl,Medium::TType &rOutMediumType) const{
		// True <=> Medium inserted in the Drive and recognized, otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - retrieving currently inserted Medium zeroth Track
		PInternalTrack pit=internalTracks[cyl][0];
			internalTracks[cyl][0]=nullptr; // forcing a new scanning
			ScanTrack(cyl,0);
		std::swap( internalTracks[cyl][0], pit );
		if (pit==nullptr)
			return ERROR_NO_MEDIA_IN_DRIVE;
		// - enumerating possible floppy Types and attempting to recognize some Sectors
		CTrackReaderWriter trw=*pit;
		delete pit;
		WORD nHealthySectorsMax=0; // arbitering the MediumType by the # of healthy Sectors
		Medium::TType bestMediumType=Medium::UNKNOWN;
		for( DWORD type=1; type!=0; type<<=1 )
			if (type&Medium::FLOPPY_ANY){
				trw.SetMediumType( rOutMediumType=(Medium::TType)type );
				if ( pit=CInternalTrack::CreateFrom( *this, trw ) ){
					std::swap( internalTracks[cyl][0], pit );
						const TSector nHealthySectors=GetCountOfHealthySectors(cyl,0);
					std::swap( internalTracks[cyl][0], pit );
					delete pit;
					if (nHealthySectors>nHealthySectorsMax)
						nHealthySectorsMax=nHealthySectors, bestMediumType=rOutMediumType;
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
			WORD nHealthySectorsMax=0; // arbitering the MediumType by the # of healthy Sectors
			Medium::TType bestMediumType=Medium::UNKNOWN;
			TFormat tmp=*pFormat;
			for( DWORD type=1; type!=0; type<<=1 )
				if (type&forcedMediumType){
					WORD nHealthySectorsCurr=0;
					tmp.mediumType=(Medium::TType)type;
					const PDos dos0=dos;
					dos=nullptr;
						SetMediumTypeAndGeometry( &tmp, sideMap, firstSectorNumber );
					dos=dos0;
					for( TCylinder cyl=0; cyl<SCANNED_CYLINDERS; cyl++ ) // counting the # of healthy Sectors
						for( THead head=2; head>0; nHealthySectorsCurr+=GetCountOfHealthySectors(cyl,--head) );
					if (nHealthySectorsCurr>nHealthySectorsMax)
						nHealthySectorsMax=nHealthySectorsCurr, bestMediumType=tmp.mediumType;
				}
			if (nHealthySectorsMax>0){
				tmp.mediumType=bestMediumType;
				return SetMediumTypeAndGeometry( &tmp, sideMap, firstSectorNumber );
			}
		}else{
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
			// . reinterpreting the fluxes
			if (newMediumTypeDifferent && pFormat->mediumType!=Medium::UNKNOWN)
				for( TCylinder cyl=0; cyl<FDD_CYLINDERS_MAX; cyl++ )
					for( THead head=0; head<2; head++ )
						if (auto &rit=internalTracks[cyl][head]){
							CTrackReaderWriter trw=*rit;
								trw.SetMediumType( pFormat->mediumType );
							delete rit;
							rit=CInternalTrack::CreateFrom( *this, trw );
						}
			// . seeing if some Sectors can be recognized in any of Tracks
			for( TCylinder cyl=0; cyl<SCANNED_CYLINDERS; cyl++ ) // examining just first N Cylinders
				for( THead head=2; head>0; )
					if (ScanTrack(cyl,--head)!=0){
						if (!IsTrackHealthy(cyl,head)){ // if Track read with errors ...
							auto &rit=internalTracks[cyl][head];
							delete rit, rit=nullptr; // ... disposing it and letting DOS later read it once again
						}
						return ERROR_SUCCESS;
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
				constexpr WORD Controls[]={ ID_MEDIUM, IDOK, 0 };
				EnableDlgItems( Controls, initialEditing );
				SetDlgItemFormattedText( ID_SYSTEM, _T("Version %d.%d"), cb.capsVersionInfo.release, cb.capsVersionInfo.revision );
				SetDlgItemFormattedText( ID_ARCHIVE, _T("%u (0x%08X)"), cb.capsImageInfo.release, cb.capsImageInfo.release );
				SetDlgItemInt( ID_ACCURACY, cb.capsImageInfo.revision, FALSE );
				SYSTEMTIME st={ cb.capsImageInfo.crdt.year, cb.capsImageInfo.crdt.month, 0, cb.capsImageInfo.crdt.day, cb.capsImageInfo.crdt.hour, cb.capsImageInfo.crdt.min, cb.capsImageInfo.crdt.sec };
					FILETIME ft;
					::SystemTimeToFileTime( &st, &ft );
				TCHAR buf[256];
				SetDlgItemText( ID_DATE, CMSDOS7::TDateTime(ft).ToString(buf) );
				*buf='\0';
				for( BYTE i=0; i<CAPS_MAXPLATFORM; i++ )
					if (cb.capsImageInfo.platform[i]!=ciipNA)
						::lstrcat(  ::lstrcat(buf, _T(", ") ),  CAPS::GetPlatformName(cb.capsImageInfo.platform[i])  );
					else if (!i) // no Platforms specified for the file
						::lstrcpy( buf+2, _T("N/A") );
				SetDlgItemText( ID_DOS, buf+2 );
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

	void CCapsBase::DestroyAllTracks(){
		// disposes all InternalTracks created thus far
		for( TCylinder cyl=0; cyl<FDD_CYLINDERS_MAX; cyl++ )
			for( THead head=0; head<2; head++ )
				if (auto &rit=internalTracks[cyl][head])
					delete rit, rit=nullptr;
		CAPS::UnlockAllTracks( capsDeviceHandle );
	}

	TStdWinError CCapsBase::Reset(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		DestroyAllTracks();
		return ERROR_SUCCESS;
	}

	TStdWinError CCapsBase::VerifyTrack(TCylinder cyl,THead head,const CTrackReaderWriter &trwWritten,bool showDiff) const{
		// verifies specified Track that is assumed to be just written; returns Windows standard i/o error
		// - Medium must be known
		const Medium::PCProperties mp=Medium::GetProperties(floppyType);
		if (!mp)
			return ERROR_UNRECOGNIZED_MEDIA;
		// - verification
		TStdWinError err=ERROR_SUCCESS; // assumption (verification was successful)
		const PInternalTrack pit0=internalTracks[cyl][head];
		internalTracks[cyl][head]=nullptr; // forcing rescan
			ScanTrack( cyl, head );
			if (const PInternalTrack pitVerif=internalTracks[cyl][head]){
				if (pitVerif->nSectors>0){
					const PInternalTrack pitWritten=CInternalTrack::CreateFrom( *this, trwWritten );
						// . comparing common cells between first two Indices
						const auto &revWrittenFirstSector=pitWritten->sectors[0].revolutions[0];
						pitWritten->SetCurrentTimeAndProfile( revWrittenFirstSector.idEndTime, revWrittenFirstSector.idEndProfile );
						const auto &revVerifFirstSector=pitVerif->sectors[0].revolutions[0];
						pitVerif->SetCurrentTimeAndProfile( revVerifFirstSector.idEndTime, revVerifFirstSector.idEndProfile );
						while (
							*pitWritten && pitWritten->GetCurrentTime()<pitWritten->GetIndexTime(1)
							&&
							*pitVerif && pitVerif->GetCurrentTime()<pitVerif->GetIndexTime(1)
						)
							if (pitWritten->ReadBit()!=pitVerif->ReadBit()){ // the bits differ
								if (pitWritten->GetCurrentTime()<mp->revolutionTime/100*95) // TODO: better way than ignoring the last 5% of Track
									// a significant difference
									err=ERROR_DS_COMPARE_FALSE;
								break;
							}
						// . if written and read Tracks differ significantly, showing problematic parts
						if (err==ERROR_DS_COMPARE_FALSE && showDiff){
							// : composition of Written- and ReadBits
							const CTrackReader::CBitSequence writtenBits( *pitWritten, revWrittenFirstSector.idEndTime, revWrittenFirstSector.idEndProfile, pitWritten->GetIndexTime(1) );
							const CTrackReader::CBitSequence readBits( *pitVerif, revVerifFirstSector.idEndTime, revVerifFirstSector.idEndProfile, pitVerif->GetIndexTime(1) );
							// : composition and display of shortest edit script (SES)
							const DWORD nSesItemsMax=writtenBits.GetBitCount()+readBits.GetBitCount();
							if (const auto pSes=Utils::MakeCallocPtr<CDiffBase::TScriptItem>(nSesItemsMax)){
								const int nSesItems=writtenBits.GetShortestEditScript( readBits, pSes, nSesItemsMax );
								if (nSesItems<=0)
									err=ERROR_FUNCTION_FAILED;
								else if (const auto pBadRegions=Utils::MakeCallocPtr<CTrackReader::TRegion>(nSesItems)){
									// composition and display of non-overlapping erroneously written regions of the Track
									const DWORD nBadRegions=writtenBits.ScriptToLocalRegions( pSes, nSesItems, pBadRegions, nSesItems, COLOR_RED );
									switch (pitWritten->ShowModal( pBadRegions, nBadRegions, MB_ABORTRETRYIGNORE, true, _T("Track %02d.%c verification failed: Review RED-MARKED errors and decide how to proceed!"), cyl, '0'+head )){
										case IDOK: // ignore
											err=ERROR_CONTINUE;
											break;
										case IDCANCEL:
											err=ERROR_CANCELLED;
											break;
										case IDRETRY:
											err=ERROR_RETRY;
											break;
										default:
											err=ERROR_FUNCTION_FAILED;
											break;
									}
								}else
									err=ERROR_NOT_ENOUGH_MEMORY;
							}else
								err=ERROR_NOT_ENOUGH_MEMORY;
						}
					delete pitWritten;
				}else
					err=ERROR_SECTOR_NOT_FOUND;
				delete pitVerif;
			}else
				err=ERROR_GEN_FAILURE;
		internalTracks[cyl][head]=pit0;
		return err;
	}

	TStdWinError CCapsBase::DetermineMagneticReliabilityByWriting(Medium::TType floppyType,TCylinder cyl,THead head) const{
		// determines if specified Track on real floppy can be trusted (ERROR_SUCCESS) or not (ERROR_DISK_CORRUPT); returns Windows standard i/o error
		// - determining Medium
		const Medium::PCProperties mp=Medium::GetProperties(floppyType);
		if (!mp)
			return ERROR_UNRECOGNIZED_MEDIA;
		// - composition of test Track
		CTrackReaderWriter trw( mp->nCells, CTrackReader::FDD_KEIR_FRASER, false );
			trw.SetMediumType(floppyType);
			trw.AddIndexTime(0);
			trw.AddIndexTime(mp->revolutionTime);
		const TLogTime doubleCellTime=2*mp->cellTime;
		for( TLogTime t=0; t<mp->revolutionTime; trw.AddTime(t+=doubleCellTime) );
		// - evaluating Track magnetic reliability
		for( BYTE nTrials=3; nTrials>0; nTrials-- ){
			// . saving the test Track
			PInternalTrack pit=CInternalTrack::CreateFrom( *this, trw );
				pit->modified=true; // to pass the save conditions
			std::swap( pit, internalTracks[cyl][head] );
				const TStdWinError err=SaveTrack( cyl, head );
			std::swap( pit, internalTracks[cyl][head] );
			delete pit;
			if (err!=ERROR_SUCCESS)
				return err;
			// . reading the test Track back
			pit=internalTracks[cyl][head];
				internalTracks[cyl][head]=nullptr; // forcing a new scan
				ScanTrack( cyl, head );
			std::swap( pit, internalTracks[cyl][head] );
			if (pit==nullptr)
				return ERROR_FUNCTION_FAILED;
			//pit->SetMediumType( floppyType ); // commented out as unnecessary (no decoder used here)
			// . evaluating what we read
			CTrackReader tr=*pit;
			delete pit;
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
		// - making sure the Track is buffered
		ScanTrack( cyl, head );
		// - returning the description
		if (const PInternalTrack pit=internalTracks[cyl][head]){
			pit->FlushSectorBuffers(); // convert all modifications into flux transitions
			pit->SetCurrentTime(0); // just to be sure the internal TrackReader is returned in valid state (as invalid state indicates this functionality is not supported)
			return *pit;
		}else
			return __super::ReadTrack(cyl,head);
	}












	CCapsBase::TCorrections::TCorrections(LPCTSTR iniSection,LPCTSTR iniName)
		// ctor
		// - the defaults
		: valid(true)
		, use(true)
		, indexTiming(true)
		, cellCountPerTrack(true)
		, fitFluxesIntoIwMiddles(true)
		, firstSectorTime(false)
		, firstSectorMicroseconds(1500) {
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
				tmp=corr.firstSectorTime;
					DDX_Check( pDX, ID_ADDRESS, tmp );
				corr.firstSectorTime=tmp!=BST_UNCHECKED;
				tmp=corr.firstSectorMicroseconds;
					DDX_Text( pDX, ID_TIME, tmp );
						DDV_MinMaxInt( pDX, tmp, SHRT_MIN, SHRT_MAX );
				corr.firstSectorMicroseconds=tmp;
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
						firstSectorTime ? firstSectorMicroseconds*1000 : 0, // micro- to nanoseconds
						fitFluxesIntoIwMiddles,
						cellCountPerTrack,
						indexTiming
					);
		else
			return ERROR_SUCCESS;
	}
