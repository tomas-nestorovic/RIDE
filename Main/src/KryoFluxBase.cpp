#include "stdafx.h"
#include "CapsBase.h"
#include "KryoFluxBase.h"


	#define INI_KRYOFLUX	_T("Kflux")

	CKryoFluxBase::CKryoFluxBase(PCProperties properties,char realDriveLetter,LPCTSTR firmware)
		// ctor
		// - base
		: CCapsBase( properties, realDriveLetter, true, INI_KRYOFLUX )
		// - initialization
		, firmware(firmware) {
		preservationQuality=false; // no descendant intended for preservation
		// - setting a classical 5.25" floppy geometry
		capsImageInfo.maxcylinder=FDD_CYLINDERS_HD/2+FDD_CYLINDERS_EXTRA - 1; // "-1" = inclusive!
		capsImageInfo.maxhead=2-1; // inclusive!
	}







	#define INI_FIRMWARE_FILE			_T("fw")

	CKryoFluxBase::TParamsEtc::TParamsEtc()
		// ctor
		// - persistent (saved and loaded)
		: firmwareFileName( app.GetProfileString(INI_KRYOFLUX,INI_FIRMWARE_FILE) )
		// - volatile (current session only)
		//none
		 { // True once the ID_40D80 button in Settings dialog is pressed
	}

	CKryoFluxBase::TParamsEtc::~TParamsEtc(){
		// dtor
		app.WriteProfileString( INI_KRYOFLUX, INI_FIRMWARE_FILE, firmwareFileName );
	}

	bool CKryoFluxBase::EditSettings(bool initialEditing){
		// True <=> new settings have been accepted (and adopted by this Image), otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return params.EditInModalDialog( *this, firmware, initialEditing );
	}

	void CKryoFluxBase::EnumSettings(CSettings &rOut) const{
		// returns a collection of relevant settings for this Image
		__super::EnumSettings(rOut);
		params.EnumSettings( rOut, properties->IsRealDevice() );
	}








	TStdWinError CCapsBase::MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus,bool flush){
		// marks Sector on a given PhysicalAddress as "dirty", plus sets it the given FdcStatus; returns Windows standard i/o error
		ASSERT( !IsWriteProtected() );
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (chs.cylinder>capsImageInfo.maxcylinder || chs.head>capsImageInfo.maxhead)
			return ERROR_INVALID_PARAMETER;
		if (const PInternalTrack pit=internalTracks[chs.cylinder][chs.head]){
			while (nSectorsToSkip<pit->sectors.length){
				auto &ris=pit->sectors[nSectorsToSkip++];
				if (ris.id==chs.sectorId){
					ASSERT( ris.dirtyRevolution>=Revolution::MAX||ris.dirtyRevolution==ris.currentRevolution ); // no Revolution yet marked as "dirty" or marking "dirty" the same Revolution
					for( TRev r=0; r<ris.nRevolutions; pit->ReadSector(ris,r++) ); // making sure all Revolutions of the Sector are buffered
					ris.dirtyRevolution=(Revolution::TType)ris.currentRevolution;
					if (pFdcStatus)
						ris.revolutions[ris.dirtyRevolution].fdcStatus=*pFdcStatus;
					SetModifiedFlag( pit->modified=true );
					if (flush)
						pit->FlushSectorBuffer(ris);
					return ERROR_SUCCESS;
				}
			}
			return ERROR_SECTOR_NOT_FOUND; // unknown Sector queried
		}else
			return ERROR_BAD_ARGUMENTS; // Track must be scanned first!
	}

	TStdWinError CCapsBase::WriteTrack(TCylinder cyl,THead head,CTrackReader tr){
		// converts general description of the specified Track into Image-specific representation; caller may provide Invalid TrackReader to check support of this feature; returns Windows standard i/o error
		// - TrackReader must be valid
		if (!tr) // caller is likely checking the support of this feature
			return ERROR_INVALID_DATA; // yes, it's supported but the TrackReader is invalid
		// - checking that specified Track actually CAN exist
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return ERROR_INVALID_PARAMETER;
		// - disposing previous Track, if any
		PInternalTrack &rit=internalTracks[cyl][head];
		if (rit!=nullptr){
			if (rit->modified)
				return ERROR_WRITE_FAULT; // cannot overwrite Track that has already been Modified
			if (const TStdWinError err=UnscanTrack( cyl, head ))
				return err;
		}
		// - creation of new content
		if ( rit = CInternalTrack::CreateFrom(*this,CTrackReaderWriter(tr)) ){
			SetModifiedFlag( rit->modified=true );
			return ERROR_SUCCESS;
		}else
			return ERROR_NOT_ENOUGH_MEMORY;
	}

	TStdWinError CCapsBase::FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte,const volatile bool &cancelled){
		// formats given Track {Cylinder,Head} to the requested NumberOfSectors, each with corresponding Length and FillerByte as initial content; returns Windows standard i/o error
		// - must support the Codec specified
		if ((codec&properties->supportedCodecs)==0)
			return ERROR_NOT_SUPPORTED;
		// - checking that specified Track actually CAN exist
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return ERROR_INVALID_PARAMETER;
		// - disposal of previous content
		if (const TStdWinError err=UnscanTrack( cyl, head ))
			return err;
		// - defining the new Track layout
		UBYTE bitBuffer[32768];
		CapsFormatTrack cft;
			CapsFormatBlock cfb[(TSector)-1];
				const CapsFormatBlock cfbDefault={
					12,	0x00,	// gap length/value before first am
					22,	0x4e,	// gap length/value after first am count
					12,	0x00,	// gap length/value before second am count
					gap3, 0x4e,	// gap length/value after second am count
					cfrmbtData,	// type of block
					0,0,0,0,	// sector ID
					nullptr,	// source data buffer
					fillerByte	// source data value if buffer is NULL
				};
				for( TSector s=0; s<nSectors; s++ ){
					const TSectorId &rid=bufferId[s];
					CapsFormatBlock &r = cfb[s] = cfbDefault;
					r.track=rid.cylinder;
					r.side=rid.side;
					r.sector=rid.sector;
					r.sectorlen=bufferLength[s]; //GetOfficialSectorLength(rid.lengthCode);
				}
			::ZeroMemory( &cft, sizeof(cft) );
			cft.gapacnt= nSectors<11 ? 60 : 48;
			cft.gapavalue=0x4e;
			cft.gapbvalue=0x4e;
			cft.trackbuf=bitBuffer;
			if (const Medium::PCProperties p=Medium::GetProperties(floppyType))
				cft.tracklen=Utils::RoundDivUp( p->nCells, (Bit::N)8 ); // # of bits round up to whole # of Bytes
			else
				return ERROR_INVALID_MEDIA;
			cft.buflen=sizeof(bitBuffer);
			cft.blockcnt=nSectors;
			cft.block=cfb;
		// - formatting the Track
		switch (codec){
			case Codec::MFM:{
				if (CAPS::FormatDataToMFM( &cft, DI_LOCK_TYPE ))
					return ERROR_FUNCTION_FAILED;
				::memset( bitBuffer+cft.bufreq, 170, cft.tracklen-cft.bufreq); // 170 = zero in MFM
				break;
			}
			default:
				ASSERT(FALSE); // we shouldn't end up here - all Codecs claimed to be supported must be covered!
				return ERROR_NOT_SUPPORTED;
		}
		// - instantiating the new Track
		const CapsTrackInfoT2 cti={ 2, cyl, head, nSectors, 0, bitBuffer, cft.tracklen };
		if ( PInternalTrack &rit=internalTracks[cyl][head]=CInternalTrack::CreateFrom( *this, &cti, 1, 0 ) ){
			SetModifiedFlag( rit->modified=true );
			return ERROR_SUCCESS;
		}else
			return ERROR_GEN_FAILURE;
	}

	bool CCapsBase::RequiresFormattedTracksVerification() const{
		// True <=> the Image requires its newly formatted Tracks be verified, otherwise False (and caller doesn't have to carry out verification)
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return params.verifyWrittenTracks;
	}

	TStdWinError CCapsBase::UnformatTrack(TCylinder cyl,THead head){
		// unformats given Track {Cylinder,Head}; returns Windows standard i/o error
		if (const Medium::PCProperties mp=Medium::GetProperties(floppyType)){
			// . checking that specified Track actually CAN exist
			EXCLUSIVELY_LOCK_THIS_IMAGE();
			if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
				return ERROR_INVALID_PARAMETER;
			// . disposal of previous content
			if (const TStdWinError err=UnscanTrack( cyl, head ))
				return err;
			// . creation of new empty Track
			const Utils::CVarTempReset<Track::TCorrections> cor0( params.corrections, Track::TCorrections() ); // disable Corrections
			CTrackReaderWriter trw( mp->nCells, floppyType );
			if ( const PInternalTrack pit = internalTracks[cyl][head] = CInternalTrack::CreateFrom(*this,std::move(trw)) ){
				SetModifiedFlag( pit->modified=true );
				return ERROR_SUCCESS;
			}else
				return ERROR_NOT_ENOUGH_MEMORY;
		}else
			return ERROR_MEDIA_INCOMPATIBLE;
	}









	static const Utils::TRationalNumber MasterClockDefault( 18432000*73, 14*2 );
	static const Utils::TRationalNumber SampleClockDefault=(MasterClockDefault/2).Simplify();
	static const Utils::TRationalNumber IndexClockDefault=(MasterClockDefault/16).Simplify();

	DWORD CKryoFluxBase::TimeToStdSampleCounter(TLogTime t){
		return SampleClockDefault*t/TIME_SECOND(1);
	}

	struct TIndexPulse sealed{
		int posInStreamData;
		DWORD sampleCounter;
		DWORD indexCounter;

		void AppendIndexTime(CTrackReaderWriter &trw,DWORD totalSampleCounter,double sck) const{
			const DWORD indexSampleCounter=totalSampleCounter+sampleCounter;
			if (sck==0) // default Sample-Clock, allowing for relatively precise computation of absolute timing
				trw.AppendIndexTime( (LONGLONG)TIME_SECOND(1)*indexSampleCounter/SampleClockDefault ); // temporary 64-bit precision even on 32-bit machines
			else // custom Sample-Clock, involving floating-point number computation
				trw.AppendIndexTime( (double)TIME_SECOND(1)*indexSampleCounter/sck ); // temporary 64-bit precision even on 32-bit machines
		}
	};

	CTrackReaderWriter CKryoFluxBase::StreamToTrack(const Memory::CSharedBytes &bytes) const{
		// creates and returns a Track representation of the Stream data
		// - parsing the input raw Bytes obtained from the KryoFlux device (eventually producing an error)
		Memory::CSharedBytes inStreamData( bytes.length, bytes ); // extracted "in-stream-data" only
		bool isKryofluxStream=false; // assumption (actually NOT a KryoFlux Stream)
		LPBYTE pis=inStreamData, pb=bytes;
		DWORD nFluxes=0;
		TIndexPulse indexPulses[Revolution::MAX+1]; // N+1 indices = N full revolutions
		TRev nIndexPulses=0;
		double mck=0,sck=0,ick=0; // all defaults, allowing flux computation with minimal precision loss
		for( const PBYTE pLastRawByte=bytes.end(); pb<pLastRawByte; ){
			const BYTE header=*pb++;
			if (pb+3>pLastRawByte){ // "+3" = we should finish with an Out-of-Stream mark whose Header has just been read, leaving 3 unread Bytes
badFormat:		::SetLastError(ERROR_BAD_FORMAT);
				return Track::Invalid;
			}
			if (header<=0x07){
				// Flux2 (see KryoFlux Stream specification for explanation)
				nFluxes++;
				*pis++=header;
				*pis++=*pb++;
			}else if (header>=0x0e){
				// Flux1
				nFluxes++;
				*pis++=header;
			}else
				switch (header){
					case 0x0c:
						// Flux3
						nFluxes++;
						*pis++=header;
						*pis++=*pb++;
						*pis++=*pb++;
						break;
					case 0x08:
						// Nop1
						*pis++=header;
						break;
					case 0x09:
						// Nop2
						*pis++=header;
						pis++, pb++;
						break;
					case 0x0a:
						// Nop3
						*pis++=header;
						pis+=2, pb+=2;
						break;
					case 0x0b:
						// Ovl16
						*pis++=header;
						break;
					case 0x0d:{
						// Out-of-Stream information
						const BYTE type=*pb++;
						const WORD size=*(PCWORD)pb; pb+=sizeof(WORD);
						if (type!=0x0d){ // still not EOF
							if (pb+size>pLastRawByte)
								goto badFormat;
							switch (type){
								default:
								case 0x00:
									// Invalid
									goto badFormat;
								case 0x01:
									// StreamInfo
									if (size!=8)
										goto badFormat;
									if (*(LPDWORD)pb!=pis-inStreamData)
										goto badFormat;
									break;
								case 0x02:
									// index
									if (size!=12)
										goto badFormat;
									if (nIndexPulses>Revolution::MAX){
										::SetLastError(ERROR_INVALID_INDEX);
										ASSERT(FALSE); //TODO: KF Streams created using Greaseweazle may contain even 20 full Revolutions!
										pb=pLastRawByte;
										break;
									}
									indexPulses[nIndexPulses++]=*(TIndexPulse *)pb;
									isKryofluxStream=true; // a Stream generated by Greaseweazle may lack the initial header and begin right with an index pulse
									break;
								case 0x03:
									// StreamEnd
									if (size!=8)
										goto badFormat;
									switch (((PDWORD)pb)[1]){
										case 0x00:
											// streaming successfull (doesn't imply that data makes sense!)
											break;
										case 0x01:
											// buffering problem
											::SetLastError(ERROR_BUFFER_OVERFLOW);
											return Track::Invalid;
										case 0x02:
											// no index signal detected
											break;
										default:
											goto badFormat;
									}
									break;
								case 0x04:{
									// KryoFlux device info
									char buf[1024];
									if (size>sizeof(buf))
										goto badFormat;
									for( LPCSTR param=::strtok( ::lstrcpynA(buf,(LPCSTR)pb,size), "," ); param!=nullptr; param=::strtok(nullptr,",") ){
										while (::IsCharSpaceA(*param))
											param++;
										if (!::strncmp(param,"name=",5))
											isKryofluxStream|=::strstr( param, "KryoFlux" )!=nullptr;
										else if (!::strncmp(param,"sck=",4)){
											const double tmp=::atof(param+4); // a custom Sample-Clock value in defined ...
											if (std::floor(tmp)!=(int)SampleClockDefault) // ... and it is different from the Default
												sck=tmp;
										}else if (!::strncmp(param,"ick=",4)){
											const double tmp=::atof(param+4); // a custom Index-Clock value in defined ...
											if (std::floor(tmp)!=(int)IndexClockDefault) // ... and it is different from the Default
												ick=tmp;
										}
									}
									break;
								}
							}
						}
						pb+=size;
						break;
					}
				}
		}
		if (!isKryofluxStream){ // not explicitly confirmed that this is a KryoFlux Stream
			::SetLastError(ERROR_BAD_FILE_TYPE);
			return Track::Invalid;
		}
		inStreamData.length=pis-inStreamData;
		// - creating and returning a Track representation of the Stream
		CTrackReaderWriter result( nFluxes*125/100, params.fluxDecoder, params.resetFluxDecoderOnIndex ); // allowing for 25% of false "ones" introduced by "FDC-like" decoders
		DWORD sampleCounter=0, totalSampleCounter=0; // delta and absolute sample counters
		PLogTime buffer=result.GetBuffer(),pLogTime=buffer;
		TRev nearestIndexPulse=0;
		int nearestIndexPulsePos= nIndexPulses>0 ? indexPulses[0].posInStreamData : INT_MAX;
		for( PCBYTE pis=inStreamData,pLastInStreamData=inStreamData.end(); pis<pLastInStreamData; ){
			const BYTE header=*pis++;
			// . extracting flux from the KryoFlux "in-Stream" data (pre-processed in ctor)
			if (header<=0x07)
				// Flux2 (see KryoFlux Stream specification for explanation)
				sampleCounter+= (header<<8) + *pis++;
			else if (header>=0x0e)
				// Flux1
				sampleCounter+=header;
			else
				switch (header){
					case 0x0c:
						// Flux3
						sampleCounter+= (*pis++<<8) + *pis++;
						ASSERT( 0x800<=sampleCounter );
						break;
					case 0x0b:
						// Ovl16
						sampleCounter+=0x10000;
						continue;
					default:
						// Nop1, Nop2, Nop3 - no other values are expected at this moment!
						ASSERT( 0x08<=header && header<=0x0a );
						pis+=header-0x08;
						continue;
				}
			// . adding an index pulse if its time has already been reached
			if (pis-inStreamData>=nearestIndexPulsePos){
				indexPulses[nearestIndexPulse].AppendIndexTime( result, totalSampleCounter, sck );
				nearestIndexPulsePos= ++nearestIndexPulse<nIndexPulses ? indexPulses[nearestIndexPulse].posInStreamData : INT_MAX;
			}
			// . adding the flux into the Buffer
			totalSampleCounter+=sampleCounter;
			if (sck==0) // default Sample-Clock, allowing for relatively precise computation of absolute timing
				*pLogTime++= (LONGLONG)TIME_SECOND(1)*totalSampleCounter/SampleClockDefault; // temporary 64-bit precision even on 32-bit machines, and mathematical rounding
			else // custom Sample-Clock, involving floating-point number computation
				*pLogTime++= (double)TIME_SECOND(1)*totalSampleCounter/sck; // temporary 64-bit precision even on 32-bit machines
			sampleCounter=0;
		}
		result.AppendTimes( buffer, pLogTime-buffer );
		// - final index pulse might not have been added if the Track ends with an unformatted area
		if (nearestIndexPulse<nIndexPulses)
			indexPulses[nearestIndexPulse].AppendIndexTime( result, totalSampleCounter, sck );
		// - preserve the input RawBytes for fast copying between compatible disks
		result.SetRawDeviceData( KF_STREAM_ID, bytes );
		return result;
	}

	static void WriteIndexBlock(Memory::CSharedBytesGrowing &buffer,TLogTime firstIndexTime,DWORD totalSampleCounter,DWORD inStreamDataLength,TLogTime indexTime){
		const struct{
			BYTE header,type;
			WORD size;
			TIndexPulse data;
		} indexBlock={
			0x0d, 0x02, sizeof(TIndexPulse),
			inStreamDataLength,
			CKryoFluxBase::TimeToStdSampleCounter(indexTime)-totalSampleCounter, // temporary 64-bit precision even on 32-bit machines
			IndexClockDefault*(indexTime-firstIndexTime)/TIME_SECOND(1) // temporary 64-bit precision even on 32-bit machines
		};
		buffer.Append( &indexBlock, sizeof(indexBlock) );
	}

	Memory::CSharedBytes CKryoFluxBase::TrackToStream(CTrackReader tr) const{
		// converts specified Track representation into Stream data and returns the length of the Stream
		Memory::CSharedBytesGrowing buffer(KF_BUFFER_CAPACITY);
		// - writing app signature
		WriteCreatorOob(buffer);
		// - writing hardware information
		#define HW_INFO_1 "host_date=2019.09.24, host_time=20:57:32, hc=0"
		buffer.AppendFormatted( "\xd\x4%c%c" HW_INFO_1, sizeof(HW_INFO_1), 0 );
		#define HW_INFO_2 "name=KryoFlux DiskSystem, version=3.00s, date=Mar 27 2018, time=18:25:55, hwid=1, hwrv=1, hs=1, sck=24027428.5714285, ick=3003428.5714285625"
		buffer.AppendFormatted( "\xd\x4%c%c" HW_INFO_2, sizeof(HW_INFO_2), 0 );
		// - writing each Revolution on the Track
		DWORD totalSampleCounter=0;
		struct{
			BYTE header,type;
			WORD size;
			DWORD dataLength, zero;
		} streamInfoBlock={
			0x0d,
			0x03, // StreamEnd
			2*sizeof(DWORD) // 8 Bytes long "out-of-Stream" data, containing StreamInfo
		};
		TRev index=0;
		TLogTime nextIndexPulseTime= tr.GetIndexCount()>0 ? tr.GetIndexTime(0) : Time::Infinity;
		for( tr.SetCurrentTime(0); tr; ){
			// . writing an index pulse if its time has already been reached
			const TLogTime currTime=tr.ReadTime();
			if (currTime>nextIndexPulseTime){
				WriteIndexBlock(
					buffer, tr.GetIndexTime(0), totalSampleCounter,
					streamInfoBlock.dataLength,
					tr.GetIndexTime(index)
				);
				nextIndexPulseTime= ++index<tr.GetIndexCount() ? tr.GetIndexTime(index) : Time::Infinity;
			}
			// . writing the flux
			int sampleCounter= TimeToStdSampleCounter(currTime)-totalSampleCounter; // temporary 64-bit precision even on 32-bit machines
			if (sampleCounter<=0){ // just to be sure
				ASSERT(FALSE); // we shouldn't end up here!
				continue;
			}
			totalSampleCounter+=sampleCounter;
			streamInfoBlock.dataLength+=buffer.AppendRepeated( '\xb', HIWORD(sampleCounter) ); // # of overflows, Ovl16
			sampleCounter=LOWORD(sampleCounter);
			if (sampleCounter<=0x0d) // Flux2
				streamInfoBlock.dataLength+=buffer.AppendFormatted( "%c%c", 0, sampleCounter );
			else if (sampleCounter<=0xff) // Flux1
				streamInfoBlock.dataLength+=buffer.Append( &sampleCounter, sizeof(BYTE) );
			else if (sampleCounter<=0x7ff) // Flux2
				streamInfoBlock.dataLength+=buffer.AppendFormatted( "%c%c", sampleCounter>>8, sampleCounter&0xff );
			else // Flux3
				streamInfoBlock.dataLength+=buffer.AppendFormatted( "\xc%c%c", sampleCounter>>8, sampleCounter&0xff );
		}
		// - writing last index pulse, if not already written
		if (index<tr.GetIndexCount())
			WriteIndexBlock(
				buffer, tr.GetIndexTime(0), totalSampleCounter,
				streamInfoBlock.dataLength,
				tr.GetIndexTime(index)
			);
		// - there are no more flux-related data in the Stream
		buffer.Append( &streamInfoBlock, sizeof(streamInfoBlock) );
		// - end of Stream
		buffer.AppendRepeated( '\xd', 7 );
		return buffer;
	}

	void CKryoFluxBase::WriteCreatorOob(Memory::CSharedBytesGrowing &buffer){
		// writes "creator" out-of-stream-buffer block into the Buffer
		#define APP_SIGNATURE "creator=" APP_ABBREVIATION " " APP_VERSION ", " GITHUB_REPOSITORY
		buffer.AppendFormatted( "\xd\x4%c%c" APP_SIGNATURE, sizeof(APP_SIGNATURE), 0 );
	}
