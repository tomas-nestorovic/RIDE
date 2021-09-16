#include "stdafx.h"

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









	CFloppyImage::TScannedTracks::TScannedTracks()
		// ctor
		: n(0)
		, dataTotalLength(0) {
		::ZeroMemory( infos, sizeof(infos) );
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

	std::unique_ptr<CImage::CSectorDataSerializer> CFloppyImage::CreateSectorDataSerializer(CHexaEditor *pParentHexaEditor){
		// abstracts all Sector data (good and bad) into a single file and returns the result
		// - defining the Serializer class
		#define EXCLUSIVELY_LOCK_SCANNED_TRACKS()	const Utils::CExclusivelyLocked<TScannedTracks> locker(GetFloppyImage().scannedTracks)
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
					// . suspending the Worker if commanded so
					if (ps->workerStatus==TScannerStatus::PAUSED)
						ps->trackWorker.Suspend(); // again resumed via SetTrackScannerStatus method
					// . checking if all Tracks on the disk have already been scanned
					const bool allTracksScanned=ps->AllTracksScanned();
					// . first, processing a request to buffer a Track (if any)
					if (ps->request.bufferEvent.Lock( allTracksScanned ? INFINITE : 2 )){
						const auto &req=ps->request;
						TSectorId ids[FDD_SECTORS_MAX];
						if (req.revolution<Revolution::MAX){
							// only particular Revolution wanted
							image->BufferTrackData(
								req.track>>1, req.track&1, req.revolution,
								ids, sectorIdAndPositionIdentity,
								ps->__scanTrack__( req.track, ids, nullptr ),
								false
							);
							const Utils::CExclusivelyLocked<TScannedTracks> locker(scannedTracks);
							scannedTracks.infos[req.track].bufferedRevs|=1<<req.revolution;
						}else{
							// all Revolutions wanted
							for( BYTE rev=std::min<BYTE>(Revolution::MAX,image->GetAvailableRevolutionCount()); rev-->0; )
								image->BufferTrackData(
									req.track>>1, req.track&1, (Revolution::TType)rev,
									ids, sectorIdAndPositionIdentity,
									ps->__scanTrack__( req.track, ids, nullptr ),
									false
								);
							const Utils::CExclusivelyLocked<TScannedTracks> locker(scannedTracks);
							scannedTracks.infos[req.track].bufferedRevs=-1;
						}
						if (ps->workerStatus!=TScannerStatus::UNAVAILABLE) // should we terminate?
							ps->pParentHexaEditor->RepaintData(true); // True = immediate repainting
					// . then, scanning the remaining Tracks (if not all yet scanned)
					}else{
						// : scanning the next remaining Track
						const Utils::CExclusivelyLocked<TScannedTracks> locker(scannedTracks);
						ps->__scanTrack__( scannedTracks.n, nullptr, nullptr );
						const int tmp = ps->trackHexaInfos[ scannedTracks.n++ ].Update(*ps);
						ps->dataTotalLength = scannedTracks.dataTotalLength = tmp; // making sure the DataTotalLength is the last thing modified in the Locked section
						if (!ps->bChsValid)
							ps->Seek(0,SeekPosition::current); // initializing state of current Sector to read from or write to
						// : the Serializer has changed its state - letting the related HexaEditor know of the change
						ps->pParentHexaEditor->SetLogicalBounds( 0, scannedTracks.dataTotalLength );
						ps->pParentHexaEditor->SetLogicalSize(scannedTracks.dataTotalLength);
					}
				} while (ps->workerStatus!=TScannerStatus::UNAVAILABLE); // should we terminate?
				return ERROR_SUCCESS;
			}

			const CBackgroundAction trackWorker;
			TScannerStatus workerStatus;
			bool bChsValid;
			struct{
				TTrack track;
				Revolution::TType revolution;
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
					auto &rNext=*(this+1);
					rNext.logicalPosition=logicalPosition, rNext.nRowsAtLogicalPosition=nRowsAtLogicalPosition;
					while (nSectors>0){
						const WORD length=lengths[--nSectors];
						rNext.logicalPosition+=length;
						rNext.nRowsAtLogicalPosition+=(length+s.lastKnownHexaRowLength-1)/s.lastKnownHexaRowLength;
					}
					return rNext.logicalPosition;
				}
			} trackHexaInfos[FDD_CYLINDERS_MAX*2+1];
			BYTE lastKnownHexaRowLength;

			TSector __scanTrack__(TTrack track,PSectorId ids,PWORD lengths) const{
				// a wrapper around CImage::ScanTrack
				const TSector nSectors=image->ScanTrack( track>>1, track&1, nullptr, ids, lengths );
				if (lengths)
					for( TSector s=0; s<nSectors; s++ )
						lengths[s]+=lengths[s]==0; // length>0 ? length : 1
				return nSectors;
			}

			bool AllTracksScanned() const{
				// True <=> all Tracks have been scanned for Sectors, otherwise False
				EXCLUSIVELY_LOCK_SCANNED_TRACKS();
				return GetFloppyImage().scannedTracks.n==2*image->GetCylinderCount();
			}
		public:
			CSerializer(CHexaEditor *pParentHexaEditor,CFloppyImage *image)
				// ctor
				// . base
				: CSectorDataSerializer( pParentHexaEditor, image, image->scannedTracks.dataTotalLength )
				// . initialization
				, trackWorker( __trackWorker_thread__, this, THREAD_PRIORITY_IDLE )
				, workerStatus(TScannerStatus::PAUSED) // set to Unavailable to terminate Worker's labor
				, bChsValid(false)
				, lastKnownHexaRowLength(1) {
				::ZeroMemory( trackHexaInfos, sizeof(trackHexaInfos) );
				// . repopulating ScannedTracks
				EXCLUSIVELY_LOCK_SCANNED_TRACKS();
				for( BYTE t=0; t<image->scannedTracks.n; t++ ){
					__scanTrack__( t, nullptr, nullptr );
					trackHexaInfos[t].Update(*this);
				}
				// . launching the TrackWorker
				request.track=-1;
				SetTrackScannerStatus( TScannerStatus::RUNNING );
				// . initializing state of current Sector to read from or write to
				//nop (in Worker)
			}
			~CSerializer(){
				// dtor
				// . terminating the Worker
				workerStatus=TScannerStatus::UNAVAILABLE;
				request.track=0; // zeroth Track highly likely already scanned, so there will be no waiting time
				request.bufferEvent.SetEvent(); // releasing the eventually blocked Worker
				::WaitForSingleObject( trackWorker, INFINITE );
			}

			bool __getPhysicalAddress__(int logPos,PTrack pOutTrack,PBYTE pOutSectorIndexOnTrack,PWORD pOutSectorOffset) const{
				// returns the ScannedTrack that contains the specified LogicalPosition
				const auto &scannedTracks=GetFloppyImage().scannedTracks;
				EXCLUSIVELY_LOCK_SCANNED_TRACKS();
				if (logPos<0 || logPos>=scannedTracks.dataTotalLength)
					return false;
				if (BYTE track=scannedTracks.n)
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
				EXCLUSIVELY_LOCK_SCANNED_TRACKS();
				if (track>=scannedTracks.n)
					return scannedTracks.dataTotalLength;
				DWORD result=trackHexaInfos[track].logicalPosition;
				TSectorId ids[FDD_SECTORS_MAX]; WORD lengths[FDD_SECTORS_MAX];
				for( TSector s=0,const nSectors=__scanTrack__(track,ids,lengths); s<nSectors; result+=lengths[s++] )
					if (nSectorsToSkip)
						nSectorsToSkip--;
					else if (ids[s]==chs.sectorId) // Sector IDs are equal
						break;
				return result;
			}
			TScannerStatus GetTrackScannerStatus() const{
				// returns Track scanner Status, if any
				return AllTracksScanned() ? TScannerStatus::UNAVAILABLE : workerStatus;
			}
			void SetTrackScannerStatus(TScannerStatus status){
				// suspends/resumes Track scanner, if any (if none, simply ignores the request)
				if (workerStatus!=status)
					switch ( workerStatus=status ){
						case TScannerStatus::RUNNING:
							trackWorker.Resume();
							break;
						case TScannerStatus::PAUSED:
							break; // Worker suspends itself upon receiving this Status
						default:
							ASSERT(FALSE); break;
					}
			}

			// CHexaEditor::IContentAdviser methods
			int LogicalPositionToRow(int logPos,BYTE nBytesInRow) override{
				// computes and returns the row containing the specified LogicalPosition
				if (logPos<0)
					return 0;
				const auto &scannedTracks=GetFloppyImage().scannedTracks;
				EXCLUSIVELY_LOCK_SCANNED_TRACKS();
				if (logPos>=scannedTracks.dataTotalLength)
					return trackHexaInfos[scannedTracks.n].nRowsAtLogicalPosition;
				if (dataTotalLength){
					// . updating the ScannedTrack structure if necessary
					if (nBytesInRow!=lastKnownHexaRowLength){
						lastKnownHexaRowLength=nBytesInRow;
						for( BYTE t=0; t<scannedTracks.n; trackHexaInfos[t++].Update(*this) );
					}
					// . returning the result
					TTrack track;
					__getPhysicalAddress__(logPos,&track,nullptr,nullptr); // guaranteed to always succeed
					int pos=trackHexaInfos[track+1].logicalPosition;
					int nRows=trackHexaInfos[track+1].nRowsAtLogicalPosition;
					WORD lengths[FDD_SECTORS_MAX];
					TSector nSectors=__scanTrack__( track, nullptr, lengths );
					do{
						const WORD length=lengths[--nSectors];
						pos-=length;
						nRows-=(length+nBytesInRow-1)/nBytesInRow;
					}while (pos>logPos);
					return nRows + (logPos-pos)/nBytesInRow;
				}
				return 0;
			}
			int RowToLogicalPosition(int row,BYTE nBytesInRow) override{
				// converts Row begin (i.e. its first Byte) to corresponding logical position in underlying File and returns the result
				if (row<0)
					return 0;
				const auto &scannedTracks=GetFloppyImage().scannedTracks;
				EXCLUSIVELY_LOCK_SCANNED_TRACKS();
				if (row>=trackHexaInfos[scannedTracks.n].nRowsAtLogicalPosition)
					return trackHexaInfos[scannedTracks.n].logicalPosition;
				if (dataTotalLength){
					// . updating the ScannedTrack structure if necessary
					if (nBytesInRow!=lastKnownHexaRowLength){
						lastKnownHexaRowLength=nBytesInRow;
						for( BYTE t=0; t<scannedTracks.n; trackHexaInfos[t++].Update(*this) );
					}
					// . returning the result
					BYTE track=scannedTracks.n;
					do{
						while (trackHexaInfos[--track].nRowsAtLogicalPosition>row);
						WORD lengths[FDD_SECTORS_MAX];
						if (TSector nSectors=__scanTrack__( track, nullptr, lengths )){
							// found an non-empty Track - guaranteed to contain the requested Row
							int logPos=trackHexaInfos[track+1].logicalPosition;
							int nRows=trackHexaInfos[track+1].nRowsAtLogicalPosition;
							do{
								const WORD length=lengths[--nSectors];
								logPos-=length;
								nRows-=(length+nBytesInRow-1)/nBytesInRow;
							}while (nRows>row);
							return logPos + (row-nRows)*nBytesInRow;
						}//else
							// empty Track - skipping it
					}while (true);
				}
				return 0;
			}
			void GetRecordInfo(int logPos,PINT pOutRecordStartLogPos,PINT pOutRecordLength,bool *pOutDataReady) override{
				// retrieves the start logical position and length of the Record pointed to by the input LogicalPosition
				TTrack track;
				if (!__getPhysicalAddress__(logPos,&track,nullptr,nullptr))
					return;
				if (pOutRecordStartLogPos || pOutRecordLength){
					WORD lengths[FDD_SECTORS_MAX];
					TSector nSectors=__scanTrack__( track, nullptr, lengths );
					int result=trackHexaInfos[track+1].logicalPosition;
					while (( result-=lengths[--nSectors] )>logPos);
					if (pOutRecordStartLogPos)
						*pOutRecordStartLogPos = result;
					if (pOutRecordLength)
						*pOutRecordLength = lengths[nSectors];
				}
				if (pOutDataReady){
					const BYTE mask=revolution<Revolution::MAX
									? 1<<revolution // only particular Revolution wanted
									: -1; // all Revolutions wanted
					const auto &scannedTracks=GetFloppyImage().scannedTracks;
					EXCLUSIVELY_LOCK_SCANNED_TRACKS();
					if (!( *pOutDataReady=(scannedTracks.infos[track].bufferedRevs&mask)==mask ))
						// Sector not yet buffered and its data probably wanted - buffering them now
						if (request.track!=track || request.revolution!=revolution){ // Request not yet issued
							request.track=track;
							request.revolution=revolution;
							request.bufferEvent.SetEvent();
						}
				}
			}
			LPCWSTR GetRecordLabelW(int logPos,PWCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const override{
				// populates the Buffer with label for the Record that STARTS at specified LogicalPosition, and returns the Buffer; returns Null if no Record starts at specified LogicalPosition
				TTrack track;
				if (!__getPhysicalAddress__(logPos,&track,nullptr,nullptr))
					return nullptr;
				TSectorId ids[FDD_SECTORS_MAX]; WORD lengths[FDD_SECTORS_MAX];
				TSector nSectors=__scanTrack__( track, ids, lengths );
				int lp=trackHexaInfos[track+1].logicalPosition;
				while (( lp-=lengths[--nSectors] )>logPos);
				if (logPos!=lp)
					return nullptr;
				const TPhysicalAddress chs={ track>>1, track&1, ids[nSectors] };
				switch (const Revolution::TType dirtyRev=image->GetDirtyRevolution(chs,nSectors)){
					case Revolution::UNKNOWN:
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
							::MultiByteToWideChar( CP_ACP, 0, ids[nSectors].ToString(),-1, idStrW,sizeof(idStrW)/sizeof(WCHAR) );
							::wnsprintfW( labelBuffer, labelBufferCharsMax, L"\x25d9Rev%d %s", dirtyRev+1, idStrW );
						#endif
						return labelBuffer;
				}
			}
		};
		// - returning a Serializer class instance
		return std::unique_ptr<CSectorDataSerializer>(new CSerializer( pParentHexaEditor, this ));
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
