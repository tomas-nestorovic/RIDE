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









	CFloppyImage::CFloppyImage(PCProperties properties,bool hasEditableSettings)
		// ctor
		: CImage(properties,hasEditableSettings)
		, floppyType(TMedium::UNKNOWN) {
	}








	WORD CFloppyImage::GetUsableSectorLength(BYTE sectorLengthCode) const{
		// determines and returns usable portion of a Sector based on supplied LenghtCode and actual FloppyType
		if (!IsValidSectorLengthCode(sectorLengthCode))
			return 0; // e.g. only copy-protection marks
		const WORD officialLength=GetOfficialSectorLength(sectorLengthCode);
		if ((floppyType&TMedium::FLOPPY_DD_ANY)!=0 || floppyType==TMedium::UNKNOWN) // Unknown = if FloppyType not set (e.g. if DOS Unknown), the floppy is by default considered as a one with the lowest capacity
			return std::min( (WORD)6144, officialLength );
		else
			return officialLength;
	}

	TFormat::TLengthCode CFloppyImage::GetMaximumSectorLengthCode() const{
		// returns the maximum LengthCode given the actual FloppyType
		switch (floppyType){
			case TMedium::FLOPPY_DD:
			case TMedium::FLOPPY_DD_525:
				return TFormat::LENGTHCODE_4096;
			case TMedium::FLOPPY_HD_525:
			case TMedium::FLOPPY_HD_350:
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
		class CSerializer sealed:public CSectorDataSerializer{
			static UINT AFX_CDECL __trackWorker_thread__(PVOID _pBackgroundAction){
				// thread to scan and buffer Tracks
				const PCBackgroundAction pAction=(PCBackgroundAction)_pBackgroundAction;
				CSerializer *const ps=(CSerializer *)pAction->GetParams();
				const PImage image=ps->image;
				do{
					// . checking if all Tracks on the disk have already been scanned
					ps->scannedTracks.locker.Lock();
						const bool allTracksScanned=ps->scannedTracks.n==2*image->GetCylinderCount();
					ps->scannedTracks.locker.Unlock();
					// . first, processing a request to buffer a Track (if any)
					if (ps->request.bufferEvent.Lock( allTracksScanned ? INFINITE : 2 )){
						const TTrack requestTrack=ps->request.track;
						TSectorId ids[FDD_SECTORS_MAX];
						image->BufferTrackData(	requestTrack>>1, requestTrack&1,
												ids, Utils::CByteIdentity(),
												ps->__scanTrack__( requestTrack, ids, nullptr ),
												false
											);
						ps->scannedTracks.infos[requestTrack].buffered=true;
						if (ps->bContinue)
							ps->pParentHexaEditor->RepaintData(true); // True = immediate repainting
					// . then, scanning the remaining Tracks (if not all yet scanned)
					}else{
						// : scanning the next remaining Track in parallel with the main thread ...
						ps->__scanTrack__( ps->scannedTracks.n, nullptr, nullptr );
						// : ... but updating the ScannedTracks structure synchronously with the main thread
						ps->scannedTracks.locker.Lock();
							const int tmp = ps->scannedTracks.infos[ ps->scannedTracks.n++ ].__update__(*ps);
							ps->dataTotalLength=tmp; // making sure the DataTotalLength is the last thing modified in the Locked section
						ps->scannedTracks.locker.Unlock();
						if (!ps->bChsValid)
							ps->Seek(0,SeekPosition::current); // initializing state of current Sector to read from or write to
						// : the Serializer has changed its state - letting the related HexaEditor know of the change
						ps->pParentHexaEditor->SetLogicalSize(ps->dataTotalLength);
						ps->pParentHexaEditor->SetLogicalBounds( 0, ps->dataTotalLength );
					}
				} while (ps->bContinue);
				return ERROR_SUCCESS;
			}

			const CBackgroundAction trackWorker;
			bool bContinue, bChsValid;
			struct{
				TTrack track;
				CEvent bufferEvent;
			} request;
			struct{
				struct{
					int logicalPosition;
					int nRowsAtLogicalPosition;
					bool buffered;

					int __update__(const CSerializer &rds){
						// updates the Track info and returns the LogicalPosition at which this Track ends
						// . retrieving the Sectors lengths via ScanTrack (though Track already scanned by the TrackWorker)
						const BYTE track=this-rds.scannedTracks.infos;
						WORD lengths[FDD_SECTORS_MAX];
						TSector nSectors=rds.__scanTrack__( track, nullptr, lengths );
						// . updating the state - the results are stored in the NEXT structure
						auto &rNext=*(this+1);
						rNext.logicalPosition=logicalPosition, rNext.nRowsAtLogicalPosition=nRowsAtLogicalPosition;
						while (nSectors>0){
							const WORD length=lengths[--nSectors];
							rNext.logicalPosition+=length;
							rNext.nRowsAtLogicalPosition+=(length+rds.lastKnownHexaRowLength-1)/rds.lastKnownHexaRowLength;
						}
						return rNext.logicalPosition;
					}
				} infos[FDD_CYLINDERS_MAX*2+1];
				BYTE n;
				CCriticalSection locker;
			} scannedTracks;
			BYTE lastKnownHexaRowLength;

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
				: CSectorDataSerializer( pParentHexaEditor, image, 0 )
				// . initialization
				, trackWorker( __trackWorker_thread__, this, THREAD_PRIORITY_IDLE )
				, bContinue(true) // set to False when wanting the Worker to terminate its labor
				, bChsValid(false)
				, lastKnownHexaRowLength(1) {
				scannedTracks.n=0;
				::ZeroMemory( scannedTracks.infos, sizeof(scannedTracks.infos) );
				// . launching the TrackWorker
				request.track=-1;
				trackWorker.Resume();
				// . initializing state of current Sector to read from or write to
				//nop (in Worker)
			}
			~CSerializer(){
				// dtor
				// . terminating the Worker
				bContinue=false;
				request.track=0; // zeroth Track highly likely already scanned, so there will be no waiting time
				request.bufferEvent.SetEvent(); // releasing the eventually blocked Worker
				::WaitForSingleObject( trackWorker, INFINITE );
			}

			bool __getPhysicalAddress__(int logPos,PTrack pOutTrack,PBYTE pOutSectorIndexOnTrack,PWORD pOutSectorOffset) const{
				// returns the ScannedTrack that contains the specified LogicalPosition
				if (logPos<0 || logPos>=dataTotalLength)
					return false;
				if (BYTE track=scannedTracks.n)
					do{
						while (scannedTracks.infos[--track].logicalPosition>logPos);
						TSectorId ids[FDD_SECTORS_MAX]; WORD lengths[FDD_SECTORS_MAX];
						if (TSector nSectors=__scanTrack__( track, ids, lengths )){
							// found an non-empty Track - guaranteed to contain the requested Position
							int pos=scannedTracks.infos[track+1].logicalPosition;
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
				const LONG result=__super::Seek(lOff,nFrom);
				bChsValid=__getPhysicalAddress__( result, &currTrack, &sector.indexOnTrack, &sector.offset );
				return result;
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
				if (track>=scannedTracks.n)
					return dataTotalLength;
				DWORD result=scannedTracks.infos[track].logicalPosition;
				TSectorId ids[FDD_SECTORS_MAX]; WORD lengths[FDD_SECTORS_MAX];
				for( TSector s=0,const nSectors=__scanTrack__(track,ids,lengths); s<nSectors; result+=lengths[s++] )
					if (nSectorsToSkip)
						nSectorsToSkip--;
					else if (ids[s]==chs.sectorId) // Sector IDs are equal
						break;
				return result;
			}

			// CHexaEditor::IContentAdviser methods
			int LogicalPositionToRow(int logPos,BYTE nBytesInRow) override{
				// computes and returns the row containing the specified LogicalPosition
				if (logPos<0)
					return 0;
				if (logPos>=dataTotalLength)
					return scannedTracks.infos[scannedTracks.n].nRowsAtLogicalPosition;
				if (dataTotalLength){
					// . updating the ScannedTrack structure if necessary
					if (nBytesInRow!=lastKnownHexaRowLength){
						scannedTracks.locker.Lock();
							lastKnownHexaRowLength=nBytesInRow;
							for( BYTE t=0; t<scannedTracks.n; scannedTracks.infos[t++].__update__(*this) );
						scannedTracks.locker.Unlock();
					}
					// . returning the result
					TTrack track;
					__getPhysicalAddress__(logPos,&track,nullptr,nullptr); // guaranteed to always succeed
					int pos=scannedTracks.infos[track+1].logicalPosition;
					int nRows=scannedTracks.infos[track+1].nRowsAtLogicalPosition;
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
				if (row>=scannedTracks.infos[scannedTracks.n].nRowsAtLogicalPosition)
					return scannedTracks.infos[scannedTracks.n].logicalPosition;
				if (dataTotalLength){
					// . updating the ScannedTrack structure if necessary
					if (nBytesInRow!=lastKnownHexaRowLength){
						scannedTracks.locker.Lock();
							lastKnownHexaRowLength=nBytesInRow;
							for( BYTE t=0; t<scannedTracks.n; scannedTracks.infos[t++].__update__(*this) );
						scannedTracks.locker.Unlock();
					}
					// . returning the result
					BYTE track=scannedTracks.n;
					do{
						while (scannedTracks.infos[--track].nRowsAtLogicalPosition>row);
						WORD lengths[FDD_SECTORS_MAX];
						if (TSector nSectors=__scanTrack__( track, nullptr, lengths )){
							// found an non-empty Track - guaranteed to contain the requested Row
							int logPos=scannedTracks.infos[track+1].logicalPosition;
							int nRows=scannedTracks.infos[track+1].nRowsAtLogicalPosition;
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
					int result=scannedTracks.infos[track+1].logicalPosition;
					while (( result-=lengths[--nSectors] )>logPos);
					if (pOutRecordStartLogPos)
						*pOutRecordStartLogPos = result;
					if (pOutRecordLength)
						*pOutRecordLength = lengths[nSectors];
				}
				if (pOutDataReady)
					if (!( *pOutDataReady=scannedTracks.infos[track].buffered ))
						// Sector not yet buffered and its data probably wanted - buffering them now
						if (request.track!=track){ // Request not yet issued
							request.track=track;
							request.bufferEvent.SetEvent();
						}
			}
			LPCTSTR GetRecordLabel(int logPos,PTCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const override{
				// populates the Buffer with label for the Record that STARTS at specified LogicalPosition, and returns the Buffer; returns Null if no Record starts at specified LogicalPosition
				TTrack track;
				if (!__getPhysicalAddress__(logPos,&track,nullptr,nullptr))
					return nullptr;
				TSectorId ids[FDD_SECTORS_MAX]; WORD lengths[FDD_SECTORS_MAX];
				TSector nSectors=__scanTrack__( track, ids, lengths );
				int lp=scannedTracks.infos[track+1].logicalPosition;
				while (( lp-=lengths[--nSectors] )>logPos);
				return	logPos==lp
						? ::lstrcpyn( labelBuffer, ids[nSectors].ToString(), labelBufferCharsMax )
						: nullptr;
			}
		};
		// - returning a Serializer class instance
		return std::unique_ptr<CSectorDataSerializer>(new CSerializer( pParentHexaEditor, this ));
	}

	TLogTime CFloppyImage::EstimateNanosecondsPerOneByte() const{
		// estimates and returns the number of Nanoseconds that represent a single Byte on the Medium
		switch (floppyType){
			case TMedium::FLOPPY_HD_350:
			case TMedium::FLOPPY_HD_525:
				return FDD_NANOSECONDS_PER_DD_BYTE/2;
			case TMedium::FLOPPY_DD:
				return FDD_NANOSECONDS_PER_DD_BYTE;
			case TMedium::FLOPPY_DD_525:
				return FDD_NANOSECONDS_PER_DD_BYTE*5/6;
			default:
				return __super::EstimateNanosecondsPerOneByte();
		}
	}

	void CFloppyImage::EstimateTrackTiming(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,BYTE gap3,PLogTime startTimesNanoseconds) const{
		// given specified Track and Sectors that it contains, estimates the positions of these Sectors
		//const BYTE gap3= floppyType==TMedium::FLOPPY_DD_525 ? FDD_525_SECTOR_GAP3 : FDD_350_SECTOR_GAP3;
		const TLogTime nNanosecondsPerByte=EstimateNanosecondsPerOneByte();
		for( TSector s=0; s<nSectors; s++ )
			if (s>0){
				startTimesNanoseconds[s] =	startTimesNanoseconds[s-1] + (bufferLength[s-1]+gap3)*nNanosecondsPerByte;
			}else
				startTimesNanoseconds[0] =	26*nNanosecondsPerByte; // 26 = IBM post-index gap for MFM encoding [Bytes]
	}
