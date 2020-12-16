#include "stdafx.h"
#include "CapsBase.h"
#include "MSDOS7.h"

	static TStdWinError InitCapsLibrary(CapsVersionInfo &cvi){
		// - checking version
		if (::CAPSGetVersionInfo(&cvi,0) || cvi.release<5 || cvi.release==5&&cvi.revision<1){
			Utils::FatalError(_T("CAPS library outdated, 5.1 or newer required!"));
			return ERROR_EVT_VERSION_TOO_OLD;
		}
		// - initializing the library
		if (::CAPSInit())
			return ERROR_DLL_INIT_FAILED;
		// - initialized successfully, can now use the library
		return ERROR_SUCCESS;
	}

	CCapsBase::CCapsBase(PCProperties properties,bool hasEditableSettings)
		// ctor
		// - base
		: CFloppyImage(properties,hasEditableSettings)
		// - loading the CAPS library
		, capsLibLoadingError( ::InitCapsLibrary(capsVersionInfo) )
		// - creating a CAPS device
		, capsDeviceHandle(  capsLibLoadingError ? -1 : ::CAPSAddImage()  )
		// - initialization
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
			::CAPSExit();
	}










	CCapsBase::CBitReader::CBitReader(const CapsTrackInfo &cti,UDWORD lockFlags)
		// ctor to iterate over bits on all available disk revolutions
		: pCurrByte(cti.trackbuf-1) , currBitMask(0) // pointing "before" the first valid bit
		, nRemainingBits( lockFlags&DI_LOCK_TRKBIT ? cti.tracklen : cti.tracklen*8 )
		, Count(nRemainingBits) {
	}

	CCapsBase::CBitReader::CBitReader(const CapsTrackInfo &cti,UDWORD revolution,UDWORD lockFlags)
		// ctor to iterate over bits on particular disk revolution only
		: pCurrByte(cti.trackdata[revolution]-1) , currBitMask(0) // pointing "before" the first valid bit
		, nRemainingBits(cti.tracksize[revolution]*8)
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
		for( BYTE n=16; n-->0; rOut=(rOut<<1)|ReadBit() );
		return true;
	}

	bool CCapsBase::CBitReader::ReadBits32(DWORD &rOut){
		// True <=> at least 32 bits have not yet been read, otherwise False
		if (nRemainingBits<32)
			return false;
		for( BYTE n=32; n-->0; rOut=(rOut<<1)|ReadBit() );
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
		, sectors(  (TInternalSector *)::memcpy( ::calloc(nSectors,sizeof(TInternalSector)), sectors, nSectors*sizeof(TInternalSector) )  )
		, nSectors(nSectors) {
	}

	CCapsBase::CInternalTrack::~CInternalTrack(){
		// dtor
		for( TSector i=0; i<nSectors; i++ )
			for( BYTE r=0; r<DEVICE_REVOLUTIONS_MAX; r++ )
				if (const PVOID data=sectors[i].revolutions[r].data)
					::free(data);
		::free(sectors);
	}

	CCapsBase::CInternalTrack *CCapsBase::CInternalTrack::CreateFrom(const CCapsBase &cb,const CapsTrackInfo &cti,UDWORD lockFlags){
		// creates and returns a Track decoded from underlying CAPS Track representation
		// - at least one full revolution must be available
		if (!cti.trackcnt)
			return nullptr;
		// - only flux timing is supported at the moment (the CBitReader class is deprecated and should not be used in new code)
		if (!cti.timelen)
			return nullptr;
		// - reconstructing flux information over all revolutions of the disk
		UDWORD nBitsPerTrack[CAPS_MTRS], nBitsPerTrackOfficial, nBitsTotally=0;
		for( UDWORD rev=0; rev<cti.trackcnt; rev++ )
			nBitsTotally += nBitsPerTrack[rev] = CBitReader(cti,rev,lockFlags).Count;
		CTrackReaderWriter trw( nBitsTotally, CTrackReader::FDD_KEIR_FRASIER ); // pessimistic estimation of # of fluxes
			if (*nBitsPerTrack>( nBitsPerTrackOfficial=TIME_MILLI(200)/CTrackReader::TProfile::HD.iwTimeDefault )*95/100) // 5% tolerance
				// likely a 3.5" HD medium (10% tollerance)
				trw.SetMediumType( TMedium::FLOPPY_HD_350 );
			else if (*nBitsPerTrack>( nBitsPerTrackOfficial=TIME_SECOND(1)/(6*CTrackReader::TProfile::HD.iwTimeDefault) )*95/100) // 5% tolerance
				// likely a 5.25" HD medium (10% tollerance)
				trw.SetMediumType( TMedium::FLOPPY_HD_350 );
			else if (*nBitsPerTrack>( nBitsPerTrackOfficial=TIME_MILLI(200)/CTrackReader::TProfile::DD.iwTimeDefault )*95/100) // 5% tolerance
				// likely a 3.5" DD or 5.25" medium in 300 RPM drive (10% tollerance)
				trw.SetMediumType( TMedium::FLOPPY_DD );
			else if (*nBitsPerTrack>( nBitsPerTrackOfficial=TIME_SECOND(1)/(6*CTrackReader::TProfile::DD_525.iwTimeDefault) )*95/100) // 5% tolerance
				// likely a 5.25" DD medium in 360 RPM drive (10% tollerance)
				trw.SetMediumType( TMedium::FLOPPY_DD_525 );
			else{
				ASSERT(FALSE); //TODO: 8" SD medium
				return nullptr;
			}
		trw.AddIndexTime(0);
		TLogTime currentTime=0, *pFluxTimeBuffer=trw.GetBuffer(), *pFluxTime=pFluxTimeBuffer;
		UDWORD nextIndexBits=*nBitsPerTrack;
		BYTE rev=0;
		for( CBitReader br(cti,lockFlags); br; ){
			// . adding new index
			if (br.GetPosition()==nextIndexBits){
				trw.AddIndexTime( currentTime );
				nextIndexBits+=nBitsPerTrack[++rev];
			}
			// . adding new flux
			const UDWORD i=br.GetPosition()>>3;
			if (i<cti.timelen)
				currentTime+= trw.profile.iwTimeDefault * cti.timebuf[i]/1000;
			else
				currentTime+= trw.profile.iwTimeDefault;
			if (br.ReadBit())
				*pFluxTime++=currentTime;
		}
		if (trw.GetIndexCount()<=cti.trackcnt){ // an IPF image may end up here
			const TLogTime tIndex=trw.GetIndexTime(cti.trackcnt-1)+nBitsPerTrackOfficial*trw.profile.iwTimeDefault;
			trw.AddIndexTime(tIndex);
			if (trw.GetTotalTime()<tIndex) // adding an auxiliary flux at the Index position to prolong flux information
				*pFluxTime++=tIndex;
		}
		trw.AddTimes( pFluxTimeBuffer, pFluxTime-pFluxTimeBuffer );
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
			TSectorId ids[DEVICE_REVOLUTIONS_MAX*(TSector)-1]; TLogTime idEnds[DEVICE_REVOLUTIONS_MAX*(TSector)-1]; TProfile idProfiles[DEVICE_REVOLUTIONS_MAX*(TSector)-1]; TFdcStatus statuses[DEVICE_REVOLUTIONS_MAX*(TSector)-1];
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
			for( TSector s=0; s<lcs.nUniqueSectors; lcs.uniqueSectors[s++].nRevolutions=std::max(1,tr.GetIndexCount()-1) );
			return new CInternalTrack( trw, lcs.uniqueSectors, lcs.nUniqueSectors );
		}
		return new CInternalTrack( trw, nullptr, 0 );
	}

	void CCapsBase::CInternalTrack::ReadSector(TInternalSector &ris){
		// buffers specified Revolution of the Sector (assumed part of this Track)
		auto &currRev=ris.revolutions[ris.currentRevolution];
		const WORD sectorOfficialLength=GetOfficialSectorLength( ris.id.lengthCode );
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
		if (::CAPSLockImage( capsDeviceHandle, fileName )){
			::SetLastError(ERROR_READ_FAULT);
			return FALSE;
		}
		if (::CAPSGetImageInfo( &capsImageInfo, capsDeviceHandle )
			||
			capsImageInfo.type!=ciitFDD
			||
			capsImageInfo.maxhead>=2 // inclusive!
		){
			::CAPSUnlockImage(capsDeviceHandle);
			::SetLastError(ERROR_NOT_SUPPORTED);
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

	TCylinder CCapsBase::GetCylinderCount() const{
		// determines and returns the actual number of Cylinders in the Image
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return capsImageInfo.maxcylinder+1; // the last INCLUSIVE Cylinder plus one
	}

	THead CCapsBase::GetNumberOfFormattedSides(TCylinder cyl) const{
		// determines and returns the number of Sides formatted on given Cylinder; returns 0 iff Cylinder not formatted
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return capsImageInfo.maxhead+1; // the last INCLUSIVE Head plus one
	}

	TSector CCapsBase::ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec,PSectorId bufferId,PWORD bufferLength,PLogTime startTimesNanoseconds,PBYTE pAvgGap3) const{
		// returns the number of Sectors found in given Track, and eventually populates the Buffer with their IDs (if Buffer!=Null); returns 0 if Track not formatted or not found
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return 0;
		// - if Track doesn't exists yet (e.g. not created by a derived class), reading it by the CAPS library
		if (internalTracks[cyl][head]==nullptr){
			CapsTrackInfo cti={};
			const UDWORD lockFlags= capsVersionInfo.flag&( DI_LOCK_INDEX | DI_LOCK_DENVAR | DI_LOCK_DENAUTO | DI_LOCK_DENNOISE | DI_LOCK_NOISE | DI_LOCK_UPDATEFD | DI_LOCK_TYPE | DI_LOCK_OVLBIT | DI_LOCK_TRKBIT );
			if (::CAPSLockTrack( &cti, capsDeviceHandle, cyl, head, lockFlags )
				||
				(cti.type&CTIT_MASK_TYPE)==ctitNA // error during Track retrieval
			)
				return 0;
			if (const PInternalTrack tmp = CInternalTrack::CreateFrom( *this, cti, lockFlags )){
				CTrackReaderWriter trw=*tmp; // extracting raw flux data ...
					trw.SetMediumType(floppyType);
				delete tmp;
				internalTracks[cyl][head] = CInternalTrack::CreateFrom( *this, trw ); // ... and rescanning the Track using current FloppyType Profile
			}
			::CAPSUnlockTrack( capsDeviceHandle, cyl, head );
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
					if (floppyType&TMedium::FLOPPY_ANY)
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
				*pAvgGap3=FDD_525_SECTOR_GAP3; // TODO
			return pit->nSectors;
		}else
			return 0;
	}

	void CCapsBase::GetTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses){
		// populates output buffers with specified Sectors' data, usable lengths, and FDC statuses; ALWAYS attempts to buffer all Sectors - caller is then to sort out eventual read errors (by observing the FDC statuses); caller can call ::GetLastError to discover the error for the last Sector in the input list
		ASSERT( outBufferData!=nullptr && outBufferLengths!=nullptr && outFdcStatuses!=nullptr );
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
				const WORD usableSectorLength=GetUsableSectorLength(sectorId.lengthCode);
				auto *currRev=pis->revolutions+pis->currentRevolution;
				if (currRev->data || currRev->fdcStatus.DescribesMissingDam()) // A|B, A = some data exist, B = reattempting to read the DAM-less Sector only if automatic recovery desired
					if (currRev->fdcStatus.IsWithoutError() || !silentlyRecoverFromErrors){ // A|B, A = returning error-free data, B = settling with any data if automatic recovery not desired
returnData:				*outFdcStatuses++=currRev->fdcStatus;
						*outBufferData++=currRev->data;
						*outBufferLengths++=usableSectorLength;
						continue;
					}
				// . attempting next disk Revolution to retrieve healthy Data
				if (usableSectorLength!=0){ // e.g. Sector with LengthCode 167 has no data
					do{
						if (++pis->currentRevolution>=pis->nRevolutions)
							pis->currentRevolution=0;
						currRev=pis->revolutions+pis->currentRevolution;
					}while (currRev->idEndTime<=0);
					::free(currRev->data), currRev->data=nullptr;
					pit->ReadSector( *pis );
				}
				// . returning (any) Data
				goto returnData;
			}
		else
			while (nSectors-->0)
				*outBufferData++=nullptr, *outFdcStatuses++=TFdcStatus::SectorNotFound;
		::SetLastError( *--outBufferData ? ERROR_SUCCESS : ERROR_SECTOR_NOT_FOUND );
	}

	TStdWinError CCapsBase::GetInsertedMediumType(TCylinder cyl,TMedium::TType &rOutMediumType) const{
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
		TMedium::TType bestMediumType=TMedium::UNKNOWN;
		for( DWORD type=1; type!=0; type<<=1 )
			if (type&TMedium::FLOPPY_ANY){
				trw.SetMediumType( rOutMediumType=(TMedium::TType)type );
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
		rOutMediumType=bestMediumType; // may be TMedium::UNKNOWN
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
		if (pFormat->mediumType==TMedium::UNKNOWN){
			// no particular Medium specified - enumerating all supported floppy Types
			WORD nHealthySectorsMax=0; // arbitering the MediumType by the # of healthy Sectors
			TMedium::TType bestMediumType=TMedium::UNKNOWN;
			TFormat tmp=*pFormat;
			for( DWORD type=1; type!=0; type<<=1 )
				if (type&TMedium::FLOPPY_ANY){
					WORD nHealthySectorsCurr=0;
					tmp.mediumType=(TMedium::TType)type;
					SetMediumTypeAndGeometry( &tmp, sideMap, firstSectorNumber );
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
			// . base
			const bool newMediumTypeDifferent=floppyType!=pFormat->mediumType;
			if (const TStdWinError err=__super::SetMediumTypeAndGeometry( pFormat, sideMap, firstSectorNumber ))
				return err;
			// . reinterpreting the fluxes
			if (newMediumTypeDifferent && pFormat->mediumType!=TMedium::UNKNOWN)
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
		// - there are no settings that must be reviewed when first opening an Image
		if (initialEditing)
			return true;
		// - defining the Dialog
		class CCapsInformation sealed:public Utils::CRideDialog{
			const CCapsBase &cb;

			void PreInitDialog() override{
				__super::PreInitDialog();
				SetDlgItemFormattedText( ID_SYSTEM, _T("Version %d.%d"), cb.capsVersionInfo.release, cb.capsVersionInfo.revision );
				SetDlgItemText( ID_MEDIUM, TMedium::GetDescription(cb.floppyType) );
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
						::lstrcat(  ::lstrcat(buf, _T(", ") ),  ::CAPSGetPlatformName(cb.capsImageInfo.platform[i])  );
				if (*buf!='\0') // some Platforms specified in the file
					SetDlgItemText( ID_DOS, buf+2 );
			}
		public:
			CCapsInformation(const CCapsBase &cb)
				: Utils::CRideDialog(IDR_CAPS)
				, cb(cb) {
			}
		} d(*this);
		// - showing the Dialog and processing its result
		d.DoModal();
		return true;
	}

	void CCapsBase::DestroyAllTracks(){
		// disposes all InternalTracks created thus far
		for( TCylinder cyl=0; cyl<FDD_CYLINDERS_MAX; cyl++ )
			for( THead head=0; head<2; head++ )
				if (auto &rit=internalTracks[cyl][head])
					delete rit, rit=nullptr;
		::CAPSUnlockAllTracks( capsDeviceHandle );
	}

	TStdWinError CCapsBase::Reset(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		DestroyAllTracks();
		return ERROR_SUCCESS;
	}

	CImage::CTrackReader CCapsBase::ReadTrack(TCylinder cyl,THead head) const{
		// creates and returns a general description of the specified Track, represented using neutral LogicalTimes
		// - making sure the Track is buffered
		ScanTrack( cyl, head );
		// - returning the description
		if (const PInternalTrack pit=internalTracks[cyl][head]){
			pit->SetCurrentTime(0); // just to be sure the internal TrackReader is returned in valid state (as invalid state indicates this functionality is not supported)
			return *pit;
		}else
			return __super::ReadTrack(cyl,head);
	}
