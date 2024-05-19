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








	TStdWinError CCapsBase::MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus){
		// marks Sector on a given PhysicalAddress as "dirty", plus sets it the given FdcStatus; returns Windows standard i/o error
		ASSERT( !IsWriteProtected() );
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (chs.cylinder>capsImageInfo.maxcylinder || chs.head>capsImageInfo.maxhead)
			return ERROR_INVALID_PARAMETER;
		if (const PInternalTrack pit=internalTracks[chs.cylinder][chs.head]){
			while (nSectorsToSkip<pit->nSectors){
				auto &ris=pit->sectors[nSectorsToSkip++];
				if (ris.id==chs.sectorId){
					ASSERT( ris.dirtyRevolution>=Revolution::MAX||ris.dirtyRevolution==ris.currentRevolution ); // no Revolution yet marked as "dirty" or marking "dirty" the same Revolution
					for( BYTE r=0; r<ris.nRevolutions; pit->ReadSector(ris,r++) ); // making sure all Revolutions of the Sector are buffered
					ris.dirtyRevolution=(Revolution::TType)ris.currentRevolution;
					if (pFdcStatus)
						ris.revolutions[ris.dirtyRevolution].fdcStatus=*pFdcStatus;
					SetModifiedFlag( pit->modified=true );
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
			delete rit, rit=nullptr;
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
				cft.tracklen=Utils::RoundDivUp( p->nCells, (DWORD)8 ); // # of bits round up to whole # of Bytes
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
			// . preparing Track content
			const int nLogTimes=mp->nCells*2;
			CTrackReaderWriter trw( nLogTimes, CTrackReader::TDecoderMethod::FDD_KEIR_FRASER, true );
				trw.AddIndexTime(0);
					for( TLogTime t=0; t<nLogTimes; trw.AddTime(++t) );
				trw.AddIndexTime( nLogTimes );
				trw.SetMediumType(floppyType);
				if (!trw.Normalize())
					return ERROR_MEDIA_INCOMPATIBLE;
			// . disposal of previous content
			if (const TStdWinError err=UnscanTrack( cyl, head ))
				return err;
			// . creation of new content
			if ( const PInternalTrack pit = internalTracks[cyl][head] = CInternalTrack::CreateFrom(*this,trw) ){
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

		void AddIndexTime(CImage::CTrackReaderWriter &trw,DWORD totalSampleCounter,double sck) const{
			const DWORD indexSampleCounter=totalSampleCounter+sampleCounter;
			if (sck==0) // default Sample-Clock, allowing for relatively precise computation of absolute timing
				trw.AddIndexTime( (LONGLONG)TIME_SECOND(1)*indexSampleCounter/SampleClockDefault ); // temporary 64-bit precision even on 32-bit machines
			else // custom Sample-Clock, involving floating-point number computation
				trw.AddIndexTime( (double)TIME_SECOND(1)*indexSampleCounter/sck ); // temporary 64-bit precision even on 32-bit machines
		}
	};

	CImage::CTrackReaderWriter CKryoFluxBase::StreamToTrack(LPBYTE rawBytes,DWORD nBytes) const{
		// creates and returns a Track representation of the Stream data
		const PCBYTE inStreamData=rawBytes; // "in-stream-data" only
		// - parsing the input raw Bytes obtained from the KryoFlux device (eventually producing an error)
		bool isKryofluxStream=false; // assumption (actually NOT a KryoFlux Stream)
		LPBYTE pis=rawBytes; // "in-stream-data" only
		DWORD nFluxes=0;
		TIndexPulse indexPulses[Revolution::MAX+1]; // N+1 indices = N full revolutions
		BYTE nIndexPulses=0;
		double mck=0,sck=0,ick=0; // all defaults, allowing flux computation with minimal precision loss
		for( const PCBYTE pLastRawByte=rawBytes+nBytes; rawBytes<pLastRawByte; ){
			const BYTE header=*rawBytes++;
			if (rawBytes+3>pLastRawByte){ // "+3" = we should finish with an Out-of-Stream mark whose Header has just been read, leaving 3 unread Bytes
badFormat:		::SetLastError(ERROR_BAD_FORMAT);
				return CTrackReaderWriter::Invalid;
			}
			if (header<=0x07){
				// Flux2 (see KryoFlux Stream specification for explanation)
				nFluxes++;
				*pis++=header;
				*pis++=*rawBytes++;
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
						*pis++=*rawBytes++;
						*pis++=*rawBytes++;
						break;
					case 0x08:
						// Nop1
						*pis++=header;
						break;
					case 0x09:
						// Nop2
						*pis++=header;
						pis++, rawBytes++;
						break;
					case 0x0a:
						// Nop3
						*pis++=header;
						pis+=2, rawBytes+=2;
						break;
					case 0x0b:
						// Ovl16
						*pis++=header;
						break;
					case 0x0d:{
						// Out-of-Stream information
						const BYTE type=*rawBytes++;
						const WORD size=*(PCWORD)rawBytes; rawBytes+=sizeof(WORD);
						if (type!=0x0d){ // still not EOF
							if (rawBytes+size>pLastRawByte)
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
									if (*(LPDWORD)rawBytes!=pis-inStreamData)
										goto badFormat;
									break;
								case 0x02:
									// index
									if (size!=12)
										goto badFormat;
									if (nIndexPulses>Revolution::MAX){
										::SetLastError(ERROR_INVALID_INDEX);
										return CTrackReaderWriter::Invalid;
									}
									indexPulses[nIndexPulses++]=*(TIndexPulse *)rawBytes;
									isKryofluxStream=true; // a Stream generated by Greaseweazle may lack the initial header and begin right with an index pulse
									break;
								case 0x03:
									// StreamEnd
									if (size!=8)
										goto badFormat;
									switch (((PDWORD)rawBytes)[1]){
										case 0x00:
											// streaming successfull (doesn't imply that data makes sense!)
											break;
										case 0x01:
											// buffering problem
											::SetLastError(ERROR_BUFFER_OVERFLOW);
											return CTrackReaderWriter::Invalid;
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
									for( LPCSTR param=::strtok( ::lstrcpynA(buf,(LPCSTR)rawBytes,size), "," ); param!=nullptr; param=::strtok(nullptr,",") ){
										while (::isspace(*param))
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
						rawBytes+=size;
						break;
					}
				}
		}
		if (!isKryofluxStream){ // not explicitly confirmed that this is a KryoFlux Stream
			::SetLastError(ERROR_BAD_FILE_TYPE);
			return CTrackReaderWriter::Invalid;
		}
		const DWORD inStreamDataLength=pis-inStreamData;
		// - creating and returning a Track representation of the Stream
		CTrackReaderWriter result( nFluxes*125/100, params.GetGlobalFluxDecoder(), params.resetFluxDecoderOnIndex ); // allowing for 25% of false "ones" introduced by "FDC-like" decoders
		DWORD sampleCounter=0, totalSampleCounter=0; // delta and absolute sample counters
		PLogTime buffer=result.GetBuffer(),pLogTime=buffer;
		BYTE nearestIndexPulse=0;
		int nearestIndexPulsePos= nIndexPulses>0 ? indexPulses[0].posInStreamData : INT_MAX;
		for( PCBYTE pis=inStreamData,pLastInStreamData=pis+inStreamDataLength; pis<pLastInStreamData; ){
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
						pis+=0x0a-0x08;
						continue;
				}
			// . adding an index pulse if its time has already been reached
			if (pis-inStreamData>=nearestIndexPulsePos){
				indexPulses[nearestIndexPulse].AddIndexTime( result, totalSampleCounter, sck );
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
		result.AddTimes( buffer, pLogTime-buffer );
		// - final index pulse might not have been added if the Track ends with an unformatted area
		if (nearestIndexPulse<nIndexPulses)
			indexPulses[nearestIndexPulse].AddIndexTime( result, totalSampleCounter, sck );
		return result;
	}

	static BYTE WriteIndexBlock(PVOID outBuffer,TLogTime firstIndexTime,DWORD totalSampleCounter,DWORD inStreamDataLength,TLogTime indexTime){
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
		::memcpy( outBuffer, &indexBlock, sizeof(indexBlock) );
		return sizeof(indexBlock);
	}

	DWORD CKryoFluxBase::TrackToStream(CTrackReader tr,LPBYTE outBuffer) const{
		// converts specified Track representation into Stream data and returns the length of the Stream
		PCHAR p=(PCHAR)outBuffer;
		// - writing app signature
		#define APP_SIGNATURE "creator=" APP_ABBREVIATION " " APP_VERSION
		p+=::wsprintfA( p, "\xd\x4%c%c" APP_SIGNATURE, sizeof(APP_SIGNATURE), 0 )+1; // "+1" = terminal zero character
		// - writing hardware information
		#define HW_INFO_1 "host_date=2019.09.24, host_time=20:57:32, hc=0"
		p+=::wsprintfA( p, "\xd\x4%c%c" HW_INFO_1, sizeof(HW_INFO_1), 0 )+1; // "+1" = terminal zero character
		#define HW_INFO_2 "name=KryoFlux DiskSystem, version=3.00s, date=Mar 27 2018, time=18:25:55, hwid=1, hwrv=1, hs=1, sck=24027428.5714285, ick=3003428.5714285625"
		p+=::wsprintfA( p, "\xd\x4%c%c" HW_INFO_2, sizeof(HW_INFO_2), 0 )+1; // "+1" = terminal zero character
		// - writing each Revolution on the Track
		DWORD totalSampleCounter=0;
		struct{
			BYTE header,type;
			WORD size;
			DWORD dataLength, zero;
		} streamInfoBlock={ 0x0d, 0x01, 2*sizeof(DWORD) }; // 8 Bytes long "out-of-Stream" data, containing StreamInfo
		BYTE index=0;
		TLogTime nextIndexPulseTime= tr.GetIndexCount()>0 ? tr.GetIndexTime(0) : INT_MAX;
		for( tr.SetCurrentTime(0); tr; ){
			// . writing an index pulse if its time has already been reached
			const TLogTime currTime=tr.ReadTime();
			if (currTime>nextIndexPulseTime){
				p+=WriteIndexBlock(
					p, tr.GetIndexTime(0), totalSampleCounter,
					streamInfoBlock.dataLength,
					tr.GetIndexTime(index)
				);
				nextIndexPulseTime= ++index<tr.GetIndexCount() ? tr.GetIndexTime(index) : INT_MAX;
			}
			// . writing the flux
			int sampleCounter= TimeToStdSampleCounter(currTime)-totalSampleCounter; // temporary 64-bit precision even on 32-bit machines
			if (sampleCounter<=0){ // just to be sure
				ASSERT(FALSE); // we shouldn't end up here!
				continue;
			}
			totalSampleCounter+=sampleCounter;
			const WORD nOverflows=HIWORD(sampleCounter);
			::memset( p, '\xb', nOverflows ); // Ovl16
			p+=nOverflows, streamInfoBlock.dataLength+=nOverflows;
			sampleCounter=LOWORD(sampleCounter);
			BYTE nInStreamBytes;
			if (sampleCounter<=0x0d) // Flux2
				nInStreamBytes=::wsprintfA( p, "%c%c", 0, sampleCounter );
			else if (sampleCounter<=0xff) // Flux1
				nInStreamBytes=1, *p=sampleCounter;
			else if (sampleCounter<=0x7ff) // Flux2
				nInStreamBytes=::wsprintfA( p, "%c%c", sampleCounter>>8, sampleCounter&0xff );
			else // Flux3
				nInStreamBytes=::wsprintfA( p, "\xc%c%c", sampleCounter>>8, sampleCounter&0xff );
			streamInfoBlock.dataLength+=nInStreamBytes, p+=nInStreamBytes;
		}
		// - writing last index pulse, if not already written
		if (index<tr.GetIndexCount())
			p+=WriteIndexBlock(
				p, tr.GetIndexTime(0), totalSampleCounter,
				streamInfoBlock.dataLength,
				tr.GetIndexTime(index)
			);
		// - there are no more flux-related data in the Stream
		streamInfoBlock.type=0x03; // StreamEnd
		::memcpy( p, &streamInfoBlock, sizeof(streamInfoBlock) );
		p+=sizeof(streamInfoBlock);
		// - end of Stream
		static constexpr char STREAM_END[]="\xd\xd\xd\xd\xd\xd\xd";
		return (PBYTE)::lstrcpyA(p,STREAM_END) + sizeof(STREAM_END)-1 - outBuffer;
	}
