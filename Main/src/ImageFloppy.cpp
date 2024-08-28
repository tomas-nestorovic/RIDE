#include "stdafx.h"
using namespace Yahel;

	#define CRC16_POLYNOM 0x1021

	CFloppyImage::TCrc16 CFloppyImage::GetCrc16Ccitt(TCrc16 seed,LPCVOID bytes,WORD nBytes){
		// computes and returns CRC-CCITT (0xFFFF) of data with a given Length in Buffer
		TCrc16 result= (LOBYTE(seed)<<8) + HIBYTE(seed);
		for( PCBYTE buffer=(PCBYTE)bytes; nBytes--; ){
			BYTE x = result>>8 ^ *buffer++;
			x ^= x>>4;
			result = (result<<8) ^ (WORD)(x<<12) ^ (WORD)(x<<5) ^ (WORD)x;
		}
		return (LOBYTE(result)<<8) + HIBYTE(result);
	}

	CFloppyImage::TCrc16 CFloppyImage::GetCrc16Ccitt(LPCVOID bytes,WORD nBytes){
		// computes and returns CRC-CCITT (0xFFFF) of data with a given Length in Buffer
		return GetCrc16Ccitt( 0xffff, bytes, nBytes );
	}

	bool CFloppyImage::IsValidSectorLengthCode(BYTE lengthCode){
		// True <=> SectorLengthCode complies with Simon Owen's recommendation (interval 0..7), otherwise False
		return (lengthCode&0xf8)==0;
	}









	const CFloppyImage::TScannerState CFloppyImage::TScannerState::Initial={
		CSectorDataSerializer::TScannerStatus::RUNNING,
		0, // # of Tracks scanned thus far
		false, // not all Tracks scanned yet
		0, // total length of data discovered thus far
		1 // at least one Revolution will be discovered
	};

	CFloppyImage::TScannedTracks::TScannedTracks()
		// ctor
		: TScannerState( TScannerState::Initial ) {
	}










	CFloppyImage::CFloppyImage(PCProperties properties,bool hasEditableSettings)
		// ctor
		: CImage(properties,hasEditableSettings)
		, floppyType(Medium::UNKNOWN) {
	}








	WORD CFloppyImage::GetUsableSectorLength(BYTE sectorLengthCode) const{
		// determines and returns usable portion of a Sector based on supplied LenghtCode and actual FloppyType
		if (!IsValidSectorLengthCode(sectorLengthCode))
			return 0; // e.g. only copy-protection marks
		const WORD officialLength=GetOfficialSectorLength(sectorLengthCode);
		if ((floppyType&Medium::FLOPPY_DD_ANY)!=0 || floppyType==Medium::UNKNOWN) // Unknown = if FloppyType not set (e.g. if DOS Unknown), the floppy is by default considered as a one with the lowest capacity
			return std::min( (WORD)6144, officialLength );
		else
			return officialLength;
	}

	TFormat::TLengthCode CFloppyImage::GetMaximumSectorLengthCode() const{
		// returns the maximum LengthCode given the actual FloppyType
		switch (floppyType){
			case Medium::FLOPPY_DD:
			case Medium::FLOPPY_DD_525:
				return TFormat::LENGTHCODE_4096;
			case Medium::FLOPPY_HD_525:
			case Medium::FLOPPY_HD_350:
				return TFormat::LENGTHCODE_8192;
			default:
				ASSERT(FALSE);
				return TFormat::LENGTHCODE_128;
		}
	}

	TStdWinError CFloppyImage::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		floppyType=pFormat->mediumType;
		return __super::SetMediumTypeAndGeometry( pFormat, sideMap, firstSectorNumber );
	}

	TStdWinError CFloppyImage::UnscanTrack(TCylinder cyl,THead head){
		// unformats given Track {Cylinder,Head}; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
{		EXCLUSIVELY_LOCK(scannedTracks);
		*static_cast<TScannerState *>(&scannedTracks)=TScannerState::Initial;
		return ERROR_SUCCESS;
}	}

	CImage::CSectorDataSerializer *CFloppyImage::CreateSectorDataSerializer(CHexaEditor *pParentHexaEditor){
		// abstracts all Sector data (good and bad) into a single file and returns the result
		// - defining the Serializer class
		#define EXCLUSIVELY_LOCK_SCANNED_TRACKS()	EXCLUSIVELY_LOCK(GetFloppyImage().scannedTracks)
		class CSerializer sealed:public CSectorDataSerializer{
			inline CFloppyImage &GetFloppyImage() const{
				return *(CFloppyImage *)image;
			}

			static UINT AFX_CDECL __trackWorker_thread__(PVOID _pBackgroundAction){
				// thread to scan and buffer Tracks
				const PCBackgroundAction pAction=(PCBackgroundAction)_pBackgroundAction;
				CSerializer *const ps=(CSerializer *)pAction->GetParams();
				CFloppyImage *const image=&ps->GetFloppyImage();
				auto &scannedTracks=image->scannedTracks;
				const Utils::CByteIdentity sectorIdAndPositionIdentity;
				do{
					// . first, processing a request to buffer a Track (if any)
					if (ps->request.bufferEvent.Lock( scannedTracks.allScanned||ps->workerStatus==TScannerStatus::PAUSED ? INFINITE : 2 )){ // paused Worker processes only requests to retrieve data
						ps->request.locker.Lock();
							const TRequestParams req=ps->request;
						ps->request.locker.Unlock();
						if (req.track==-1) // a dummy data-retrieval request to "wake up" a Paused Worker?
							continue;
						TSectorId ids[FDD_SECTORS_MAX];
						if (req.revolution!=Revolution::ALL_INTERSECTED)
							// only particular Revolution wanted
							image->BufferTrackData(
								req.track>>1, req.track&1, req.revolution,
								ids, sectorIdAndPositionIdentity,
								ps->__scanTrack__( req.track, ids, nullptr )
							);
						else
							// all Revolutions wanted
							for( BYTE rev=ps->GetAvailableRevolutionCount(req.track>>1,req.track&1); rev-->0; )
								image->BufferTrackData(
									req.track>>1, req.track&1, (Revolution::TType)rev,
									ids, sectorIdAndPositionIdentity,
									ps->__scanTrack__( req.track, ids, nullptr )
								);
						EXCLUSIVELY_LOCK(ps->request); // synchronizing with dtor
						if (ps->workerStatus!=TScannerStatus::UNAVAILABLE) // should we terminate?
							ps->pParentHexaEditor->RepaintData();
					// . then, scanning the remaining Tracks (if not all yet scanned)
					}else{
						// : scanning the next remaining Track
						do{
							//EXCLUSIVELY_LOCK(scannedTracks); // postponed until below for smoother operation; may thus work with outdated values in ScannedTracks but that's ok!
							const bool hasTrackBeenScannedBefore=image->IsTrackScanned( scannedTracks.n>>1, scannedTracks.n&1 );
							const int tmp = ps->trackHexaInfos[scannedTracks.n].Update(*ps); // calls CImage::ScanTrack
							EXCLUSIVELY_LOCK(scannedTracks);
							ps->dataTotalLength = scannedTracks.dataTotalLength = tmp; // making sure the DataTotalLength is the last thing modified in the Locked section
							scannedTracks.nDiscoveredRevolutions=std::max( scannedTracks.nDiscoveredRevolutions, ps->GetAvailableRevolutionCount(scannedTracks.n>>1,scannedTracks.n&1) );
							scannedTracks.allScanned=++scannedTracks.n>=2*image->GetCylinderCount();
							if (!hasTrackBeenScannedBefore)
								break; // a single time-expensive access to real Device is enough, let other parts of the app have a crack on the Device
						}while (!scannedTracks.allScanned);
						if (!ps->bChsValid)
							ps->Seek(0,SeekPosition::current); // initializing state of current Sector to read from or write to
						// : the Serializer has changed its state - letting the related HexaEditor know of the change
						//DEADLOCK: No need to have the ScannedTracks locked - the only place where the values can change is above, so what we read below IS in sync!
						EXCLUSIVELY_LOCK(ps->request); // synchronizing with dtor
						if (ps->workerStatus!=TScannerStatus::UNAVAILABLE){ // should we terminate?
							ps->pParentHexaEditor->SetLogicalSizeLimits( scannedTracks.dataTotalLength );
							ps->pParentHexaEditor->SetLogicalSize(scannedTracks.dataTotalLength);
							ps->pParentHexaEditor->ProcessCustomCommand(ID_CREATOR);
						}
					}
				} while (ps->workerStatus!=TScannerStatus::UNAVAILABLE); // should we terminate?
				return ERROR_SUCCESS;
			}

			const CBackgroundAction trackWorker;
			TScannerStatus workerStatus;
			bool bChsValid;
			struct TRequestParams{
				TTrack track;
				Revolution::TType revolution;
			};
			struct:public TRequestParams{
				CCriticalSection locker;
				CEvent bufferEvent;
			} request;
			struct{
				int logicalPosition;
				int nRowsAtLogicalPosition; // how many rows from the beginning of the disk are at the end of this Track

				int Update(const CSerializer &s){
					// updates the Track info and returns the LogicalPosition at which this Track ends
					// . retrieving the Sectors lengths via ScanTrack (though Track already scanned by the TrackWorker)
					const BYTE track=this-s.trackHexaInfos;
					WORD lengths[FDD_SECTORS_MAX];
					TSector nSectors=s.__scanTrack__( track, nullptr, lengths );
					// . updating the state - the results are stored in the NEXT structure
					auto &scannedTracks=s.GetFloppyImage().scannedTracks;
					EXCLUSIVELY_LOCK(scannedTracks);
					auto &rNext=*(this+1);
					rNext=*this;
					while (nSectors>0){
						const WORD length=lengths[--nSectors];
						rNext.logicalPosition+=length;
						rNext.nRowsAtLogicalPosition+=Utils::RoundDivUp( length, s.lastKnownHexaRowLength );
					}
					return rNext.logicalPosition;
				}
			} trackHexaInfos[FDD_CYLINDERS_MAX*2+1];
			WORD lastKnownHexaRowLength;

			TSector __scanTrack__(TTrack track,PSectorId ids,PWORD lengths) const{
				// a wrapper around CImage::ScanTrack
				const TSector nSectors=image->ScanTrack( track>>1, track&1, nullptr, ids, lengths );
				if (lengths)
					for( TSector s=0; s<nSectors; s++ )
						lengths[s]+=lengths[s]==0; // length>0 ? length : 1
				return nSectors;
			}
		public:
			CSerializer(CHexaEditor *pParentHexaEditor,CFloppyImage *image)
				// ctor
				// . base
				: CSectorDataSerializer( pParentHexaEditor, image, image->scannedTracks.dataTotalLength, image->scannedTracks.nDiscoveredRevolutions )
				// . initialization
				, trackWorker( __trackWorker_thread__, this, THREAD_PRIORITY_IDLE )
				, workerStatus(TScannerStatus::PAUSED) // set to Unavailable to terminate Worker's labor
				, bChsValid(false)
				, lastKnownHexaRowLength(pParentHexaEditor->GetBytesPerRow()) {
				::ZeroMemory( trackHexaInfos, sizeof(trackHexaInfos) );
				// . repopulating ScannedTracks
				EXCLUSIVELY_LOCK_SCANNED_TRACKS();
				for( BYTE t=0; t<image->scannedTracks.n; t++ ){
					__scanTrack__( t, nullptr, nullptr );
					dataTotalLength=trackHexaInfos[t].Update(*this);
				}
				image->scannedTracks.dataTotalLength=dataTotalLength;
				// . launching the TrackWorker
				request.track=-1;
				trackWorker.Resume(); // initially Paused, gets locked waiting for the first data retrieval request
				//SetTrackScannerStatus( TScannerStatus::RUNNING ); // commented out as up to caller to launch the scanner
				// . initializing state of current Sector to read from or write to
				//nop (in Worker)
			}
			~CSerializer(){
				// dtor
				// . terminating the Worker
		{		EXCLUSIVELY_LOCK(request);
				if (workerStatus!=TScannerStatus::RUNNING) // e.g. Paused
					return;
				workerStatus=TScannerStatus::UNAVAILABLE;
				request.track=0, request.revolution=Revolution::R0; // zeroth Track highly likely already scanned, so there will be no waiting time
		}		request.bufferEvent.SetEvent(); // releasing the eventually blocked Worker
				::WaitForSingleObject( trackWorker, INFINITE );
			}

			bool __getPhysicalAddress__(int logPos,PTrack pOutTrack,PBYTE pOutSectorIndexOnTrack,PWORD pOutSectorOffset) const{
				// returns the ScannedTrack that contains the specified LogicalPosition
				const auto &scannedTracks=GetFloppyImage().scannedTracks;
				BYTE track;
		{		EXCLUSIVELY_LOCK_SCANNED_TRACKS();
				if (logPos<0 || logPos>=scannedTracks.dataTotalLength)
					return false;
				track=scannedTracks.n;
		}		if (track)
					do{
						while (trackHexaInfos[--track].logicalPosition>logPos);
						TSectorId ids[FDD_SECTORS_MAX]; WORD lengths[FDD_SECTORS_MAX];
						if (TSector nSectors=__scanTrack__( track, ids, lengths )){
							// found an non-empty Track - guaranteed to contain the requested Position
							int pos=trackHexaInfos[track+1].logicalPosition;
							while (( pos-=lengths[--nSectors] )>logPos);
							if (pOutSectorOffset)
								*pOutSectorOffset=logPos-pos;
							if (pOutSectorIndexOnTrack)
								*pOutSectorIndexOnTrack=nSectors;
							if (pOutTrack)
								*pOutTrack=track;
							return true;
						}//else
							// empty Track - skipping it
					}while (true);
				return false;
			}

			// CSectorDataSerializer methods
			#if _MFC_VER>=0x0A00
			ULONGLONG Seek(LONGLONG lOff,UINT nFrom) override{
			#else
			LONG Seek(LONG lOff,UINT nFrom) override{
			#endif
				// sets the actual Position in the Serializer
				const auto result=__super::Seek(lOff,nFrom);
				bChsValid=__getPhysicalAddress__( result, &currTrack, &sector.indexOnTrack, &sector.offset );
				return result;
			}
			void SetCurrentRevolution(Revolution::TType rev) override{
				// selects Revolution from which to retrieve Sector data
				const bool revDifferent=rev!=revolution;
				revolution=rev;
				if (revDifferent)
					pParentHexaEditor->RepaintData();
			}
			TPhysicalAddress GetCurrentPhysicalAddress() const override{
				// returns the current Sector's PhysicalAddress
				TSectorId ids[FDD_SECTORS_MAX];
				__scanTrack__(currTrack,ids,nullptr);
				const TPhysicalAddress result={ currTrack>>1, currTrack&1, ids[sector.indexOnTrack] };
				return result;
			}
			DWORD GetSectorStartPosition(RCPhysicalAddress chs,BYTE nSectorsToSkip) const override{
				// computes and returns the position of the first Byte of the Sector at the PhysicalAddress
				const BYTE track=chs.cylinder*2+chs.head;
				const auto &scannedTracks=GetFloppyImage().scannedTracks;
		{		EXCLUSIVELY_LOCK_SCANNED_TRACKS();
				if (track>=scannedTracks.n)
					return scannedTracks.dataTotalLength;
		}		DWORD result=trackHexaInfos[track].logicalPosition;
				TSectorId ids[FDD_SECTORS_MAX]; WORD lengths[FDD_SECTORS_MAX];
				for( TSector s=0,const nSectors=__scanTrack__(track,ids,lengths); s<nSectors; result+=lengths[s++] )
					if (nSectorsToSkip)
						nSectorsToSkip--;
					else if (ids[s]==chs.sectorId) // Sector IDs are equal
						break;
				return result;
			}
			TScannerStatus GetTrackScannerStatus(PCylinder pnOutScannedCyls) const override{
				// returns Track scanner Status, if any
				EXCLUSIVELY_LOCK_SCANNED_TRACKS();
				if (pnOutScannedCyls)
					*pnOutScannedCyls=GetFloppyImage().scannedTracks.n>>1;
				const auto &scannedTracks=GetFloppyImage().scannedTracks;
				return	scannedTracks.allScanned
						? TScannerStatus::UNAVAILABLE
						: scannedTracks.scannerStatus; // returning what has been explicitly set via SetTrackScannerStatus (for the internal state may not yet reflect the explicit command)
			}
			void SetTrackScannerStatus(TScannerStatus status) override{
				// suspends/resumes Track scanner, if any (if none, simply ignores the request)
				EXCLUSIVELY_LOCK_SCANNED_TRACKS();
				if (workerStatus!=status)
					switch ( GetFloppyImage().scannedTracks.scannerStatus= workerStatus = status ){
						case TScannerStatus::RUNNING:
							request.bufferEvent.SetEvent(); // a previously Paused scanner has been waiting for a data retrieval request
							break;
						case TScannerStatus::PAUSED:
							break; // Worker suspends itself upon receiving this Status
						default:
							ASSERT(FALSE); break;
					}
			}

			// Yahel::Stream::IAdvisor methods
			TRow LogicalPositionToRow(TPosition logPos,WORD nBytesInRow) override{
				// computes and returns the row containing the specified LogicalPosition
				if (logPos<0)
					return 0;
				const auto &scannedTracks=GetFloppyImage().scannedTracks;
		{		EXCLUSIVELY_LOCK_SCANNED_TRACKS();
				// . updating the ScannedTrack structure if necessary
				if (nBytesInRow!=lastKnownHexaRowLength){
					lastKnownHexaRowLength=nBytesInRow;
					for( BYTE t=0; t<scannedTracks.n; trackHexaInfos[t++].Update(*this) );
				}
				// . returning the result
				if (logPos>=scannedTracks.dataTotalLength)
					return trackHexaInfos[scannedTracks.n].nRowsAtLogicalPosition;
				if (!dataTotalLength)
					return 0;
		}		TTrack track;
				__getPhysicalAddress__(logPos,&track,nullptr,nullptr); // guaranteed to always succeed
				auto pos=trackHexaInfos[track+1].logicalPosition;
				auto nRows=trackHexaInfos[track+1].nRowsAtLogicalPosition;
				WORD lengths[FDD_SECTORS_MAX];
				TSector nSectors=__scanTrack__( track, nullptr, lengths );
				do{
					const WORD length=lengths[--nSectors];
					pos-=length;
					nRows-=Utils::RoundDivUp( length, nBytesInRow );
				}while (pos>logPos);
				return nRows + (logPos-pos)/nBytesInRow;
			}
			TPosition RowToLogicalPosition(TRow row,WORD nBytesInRow) override{
				// converts Row begin (i.e. its first Byte) to corresponding logical position in underlying File and returns the result
				if (row<0)
					return 0;
				const auto &scannedTracks=GetFloppyImage().scannedTracks;
				BYTE track;
		{		EXCLUSIVELY_LOCK_SCANNED_TRACKS();
				// . updating the ScannedTrack structure if necessary
				if (nBytesInRow!=lastKnownHexaRowLength){
					lastKnownHexaRowLength=nBytesInRow;
					for( BYTE t=0; t<scannedTracks.n; trackHexaInfos[t++].Update(*this) );
				}
				// . returning the result
				if (row>=trackHexaInfos[scannedTracks.n].nRowsAtLogicalPosition)
					return trackHexaInfos[scannedTracks.n].logicalPosition;
				if (!dataTotalLength)
					return 0;
				track=scannedTracks.n;
		}		if (track)
					do{
						while (trackHexaInfos[--track].nRowsAtLogicalPosition>row);
						WORD lengths[FDD_SECTORS_MAX];
						if (TSector nSectors=__scanTrack__( track, nullptr, lengths )){
							// found an non-empty Track - guaranteed to contain the requested Row
							auto logPos=trackHexaInfos[track+1].logicalPosition;
							auto nRows=trackHexaInfos[track+1].nRowsAtLogicalPosition;
							do{
								const WORD length=lengths[--nSectors];
								logPos-=length;
								nRows-=Utils::RoundDivUp( length, nBytesInRow );
							}while (nRows>row);
							return logPos + (row-nRows)*nBytesInRow;
						}//else
							// empty Track - skipping it
					}while (true);
				return 0;
			}
			void GetRecordInfo(TPosition logPos,PPosition pOutRecordStartLogPos,PPosition pOutRecordLength,bool *pOutDataReady) override{
				// retrieves the start logical position and length of the Record pointed to by the input LogicalPosition
				TTrack track; BYTE iSector;
				if (!__getPhysicalAddress__(logPos,&track,&iSector,nullptr))
					return;
				if (pOutRecordStartLogPos || pOutRecordLength){
					WORD lengths[FDD_SECTORS_MAX];
					TSector nSectors=__scanTrack__( track, nullptr, lengths );
					auto result=trackHexaInfos[track+1].logicalPosition;
					while (( result-=lengths[--nSectors] )>logPos);
					if (pOutRecordStartLogPos)
						*pOutRecordStartLogPos = result;
					if (pOutRecordLength)
						*pOutRecordLength = lengths[nSectors];
				}
				if (pOutDataReady){
					TSectorId ids[(TSector)-1];
					__scanTrack__( track, ids, nullptr );
					switch (revolution){
						case Revolution::ANY_GOOD:
							*pOutDataReady=true; // assumption (all Revolutions already attempted, none holding healthy data)
							for( BYTE r=0; r<GetAvailableRevolutionCount(track>>1,track&1); r++ )
								if (const TDataStatus ds=image->IsSectorDataReady( track>>1, track&1, ids[iSector], iSector, (Revolution::TType)r )){
									if (ds==TDataStatus::READY_HEALTHY){
										*pOutDataReady=true;
										break;
									}
								}else{
									// this Revolution hasn't yet been queried for data, so there is a chance it contains healthy data
									*pOutDataReady=false;
									EXCLUSIVELY_LOCK(request);
									request.revolution=Revolution::NONE; // to issue a duplicate request below
								}
							break;
						case Revolution::ALL_INTERSECTED:
							*pOutDataReady=true; // assumption (all Revolutions already attempted)
							for( BYTE r=0; r<GetAvailableRevolutionCount(track>>1,track&1); r++ )
								*pOutDataReady&=image->IsSectorDataReady( track>>1, track&1, ids[iSector], iSector, (Revolution::TType)r )!=TDataStatus::NOT_READY;
							break;
						default:
							*pOutDataReady=TDataStatus::NOT_READY!=image->IsSectorDataReady( track>>1, track&1, ids[iSector], iSector, revolution );
							break;
					}
					if (!*pOutDataReady){
						// Sector not yet buffered and its data probably wanted - buffering them now
						EXCLUSIVELY_LOCK(request);
						if (request.track!=track || request.revolution!=revolution){ // Request not yet issued
							request.track=track;
							request.revolution=revolution;
							request.bufferEvent.SetEvent();
						}
					}
				}
			}
			LPCWSTR GetRecordLabelW(TPosition logPos,PWCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const override{
				// populates the Buffer with label for the Record that STARTS at specified LogicalPosition, and returns the Buffer; returns Null if no Record starts at specified LogicalPosition
				TTrack track;
				if (!__getPhysicalAddress__(logPos,&track,nullptr,nullptr))
					return nullptr;
				TSectorId ids[FDD_SECTORS_MAX]; WORD lengths[FDD_SECTORS_MAX];
				TSector nSectors=__scanTrack__( track, ids, lengths );
				auto lp=trackHexaInfos[track+1].logicalPosition;
				while (( lp-=lengths[--nSectors] )>logPos);
				if (logPos!=lp)
					return nullptr;
				const TPhysicalAddress chs={ track>>1, track&1, ids[nSectors] };
				switch (const Revolution::TType dirtyRev=image->GetDirtyRevolution(chs,nSectors)){
					case Revolution::NONE:
						#ifdef UNICODE
							return ::lstrcpyn( labelBuffer, ids[nSectors].ToString(), labelBufferCharsMax );
						#else
							::MultiByteToWideChar( CP_ACP, 0, ids[nSectors].ToString(),-1, labelBuffer,labelBufferCharsMax );
							return labelBuffer;
						#endif
					default:
						#ifdef UNICODE
							::wnsprintf( labelBuffer, labelBufferCharsMax, L"\x25d9Rev%d %s", dirtyRev+1, (LPCTSTR)ids[nSectors].ToString() );
						#else
							WCHAR idStrW[80];
							::MultiByteToWideChar( CP_ACP, 0, ids[nSectors].ToString(),-1, idStrW,ARRAYSIZE(idStrW) );
							::wnsprintfW( labelBuffer, labelBufferCharsMax, L"\x25d9Rev%d %s", dirtyRev+1, idStrW );
						#endif
						return labelBuffer;
				}
			}
		};
		// - returning a Serializer class instance
		return new CSerializer(pParentHexaEditor,this);
	}

	TLogTime CFloppyImage::EstimateNanosecondsPerOneByte() const{
		// estimates and returns the number of Nanoseconds that represent a single Byte on the Medium
		switch (floppyType){
			case Medium::FLOPPY_HD_350:
			case Medium::FLOPPY_HD_525:
				return FDD_NANOSECONDS_PER_DD_BYTE/2;
			case Medium::FLOPPY_DD:
				return FDD_NANOSECONDS_PER_DD_BYTE;
			case Medium::FLOPPY_DD_525:
				return FDD_NANOSECONDS_PER_DD_BYTE*5/6;
			default:
				return __super::EstimateNanosecondsPerOneByte();
		}
	}

	void CFloppyImage::EstimateTrackTiming(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,BYTE gap3,PLogTime startTimesNanoseconds) const{
		// given specified Track and Sectors that it contains, estimates the positions of these Sectors
		//const BYTE gap3= floppyType==Medium::FLOPPY_DD_525 ? FDD_525_SECTOR_GAP3 : FDD_350_SECTOR_GAP3;
		const TLogTime nNanosecondsPerByte=EstimateNanosecondsPerOneByte();
		for( TSector s=0; s<nSectors; s++ )
			if (s>0){
				startTimesNanoseconds[s] =	startTimesNanoseconds[s-1] + (bufferLength[s-1]+gap3)*nNanosecondsPerByte;
			}else
				startTimesNanoseconds[0] =	26*nNanosecondsPerByte; // 26 = IBM post-index gap for MFM encoding [Bytes]
	}
