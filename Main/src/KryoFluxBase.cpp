#include "stdafx.h"
#include "CapsBase.h"
#include "KryoFluxBase.h"


	CKryoFluxBase::CKryoFluxBase(PCProperties properties,LPCTSTR firmware)
		// ctor
		// - base
		: CCapsBase( properties, true )
		// - initialization
		, firmware(firmware) {
		// - setting a classical 5.25" floppy geometry
		capsImageInfo.maxcylinder=FDD_CYLINDERS_MAX/2-1; // inclusive!
		capsImageInfo.maxhead=2-1; // inclusive!
	}







	#define INI_KRYOFLUX	_T("Kflux")

	#define INI_FIRMWARE_FILE			_T("fw")
	#define INI_FLUX_DECODER			_T("decod")
	#define INI_FLUX_DECODER_RESET		_T("drst")
	#define INI_PRECISION				_T("prec")
	#define INI_CALIBRATE_SECTOR_ERROR	_T("clberr")
	#define INI_CALIBRATE_FORMATTING	_T("clbfmt")
	#define INI_NORMALIZE_READ_TRACKS	_T("nrt")
	#define INI_VERIFY_FORMATTING		_T("vrftr")
	#define INI_VERIFY_WRITTEN_DATA		_T("vrfdt")


	CKryoFluxBase::TParams::TParams()
		// ctor
		// - persistent (saved and loaded)
		: firmwareFileName( app.GetProfileString(INI_KRYOFLUX,INI_FIRMWARE_FILE) )
		, precision( app.GetProfileInt(INI_KRYOFLUX,INI_PRECISION,0) )
		, fluxDecoder( (TFluxDecoder)app.GetProfileInt(INI_KRYOFLUX,INI_FLUX_DECODER,TFluxDecoder::KEIR_FRASIER) )
		, resetFluxDecoderOnIndex( (TFluxDecoder)app.GetProfileInt(INI_KRYOFLUX,INI_FLUX_DECODER_RESET,true)!=0 )
		, calibrationAfterError( (TCalibrationAfterError)app.GetProfileInt(INI_KRYOFLUX,INI_CALIBRATE_SECTOR_ERROR,TCalibrationAfterError::ONCE_PER_CYLINDER) )
		, calibrationStepDuringFormatting( app.GetProfileInt(INI_KRYOFLUX,INI_CALIBRATE_FORMATTING,0) )
		, normalizeReadTracks( app.GetProfileInt(INI_KRYOFLUX,INI_NORMALIZE_READ_TRACKS,true)!=0 )
		, verifyFormattedTracks( app.GetProfileInt(INI_KRYOFLUX,INI_VERIFY_FORMATTING,true)!=0 )
		, verifyWrittenData( app.GetProfileInt(INI_KRYOFLUX,INI_VERIFY_WRITTEN_DATA,false)!=0 )
		// - volatile (current session only)
		, doubleTrackStep(false)
		, userForcedDoubleTrackStep(false) { // True once the ID_40D80 button in Settings dialog is pressed
	}


	CKryoFluxBase::TParams::~TParams(){
		// dtor
		app.WriteProfileString( INI_KRYOFLUX, INI_FIRMWARE_FILE, firmwareFileName );
		app.WriteProfileInt( INI_KRYOFLUX, INI_PRECISION, precision );
		app.WriteProfileInt( INI_KRYOFLUX, INI_FLUX_DECODER, fluxDecoder );
		app.WriteProfileInt( INI_KRYOFLUX, INI_FLUX_DECODER_RESET, resetFluxDecoderOnIndex );
		app.WriteProfileInt( INI_KRYOFLUX, INI_CALIBRATE_SECTOR_ERROR, calibrationAfterError );
		app.WriteProfileInt( INI_KRYOFLUX, INI_CALIBRATE_FORMATTING, calibrationStepDuringFormatting );
		app.WriteProfileInt( INI_KRYOFLUX, INI_NORMALIZE_READ_TRACKS, normalizeReadTracks );
		app.WriteProfileInt( INI_KRYOFLUX, INI_VERIFY_FORMATTING, verifyFormattedTracks );
		app.WriteProfileInt( INI_KRYOFLUX, INI_VERIFY_WRITTEN_DATA, verifyWrittenData );
	}

	bool CKryoFluxBase::EditSettings(bool initialEditing){
		// True <=> new settings have been accepted (and adopted by this Image), otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - defining the Dialog
		class CParamsDialog sealed:public Utils::CRideDialog{
			const bool initialEditing;
			CKryoFluxBase &rkfb;
			TCHAR doubleTrackDistanceTextOrg[80];

			bool IsDoubleTrackDistanceForcedByUser() const{
				// True <=> user has manually overridden DoubleTrackDistance setting, otherwise False
				return ::lstrlen(doubleTrackDistanceTextOrg)!=::GetWindowTextLength( GetDlgItemHwnd(ID_40D80) );
			}

			void RefreshMediumInformation(){
				// detects a floppy in the Drive and attempts to recognize its Type
				// . making sure that a floppy is in the Drive
				Medium::TType mt;
				static const WORD Interactivity[]={ ID_LATENCY, ID_NUMBER2, ID_GAP };
				if (!EnableDlgItems( Interactivity, rkfb.GetInsertedMediumType(0,mt)==ERROR_SUCCESS ))
					SetDlgItemText( ID_MEDIUM, _T("Not inserted") );
				// . attempting to recognize any previous format on the floppy
				else
					switch (mt){
						case Medium::FLOPPY_DD_525:
							SetDlgItemText( ID_MEDIUM, _T("5.25\" DD formatted, 360 RPM drive") );
							if (EnableDlgItem( ID_40D80, initialEditing )){
								const bool doubleTrackStep0=rkfb.params.doubleTrackStep;
									rkfb.params.doubleTrackStep=false;
									if (rkfb.GetInsertedMediumType(0,mt)==ERROR_SUCCESS)
										CheckDlgButton( ID_40D80, mt==Medium::UNKNOWN ); // first Track is empty, so likely each odd Track is empty
								rkfb.params.doubleTrackStep=doubleTrackStep0;
								rkfb.GetInsertedMediumType(0,mt); // a workaround to make floppy Drive head seek home
							}
							break;
						case Medium::FLOPPY_DD:
							SetDlgItemText( ID_MEDIUM, _T("3.5\"/5.25\" DD formatted, 300 RPM drive") );
							CheckDlgButton( ID_40D80,  EnableDlgItem( ID_40D80, false )  );
							break;
						case Medium::FLOPPY_HD_525:
						case Medium::FLOPPY_HD_350:
							SetDlgItemText( ID_MEDIUM, _T("3.5\"/5.25\" HD formatted") );
							CheckDlgButton( ID_40D80, false );
							EnableDlgItem( ID_40D80, initialEditing );
							break;
						default:
							SetDlgItemText( ID_MEDIUM, _T("Not formatted or faulty") );
							CheckDlgButton( ID_40D80, false );
							EnableDlgItem( ID_40D80, initialEditing );
							break;
					}
				// . forcing redrawing (as the new text may be shorter than the original text, leaving the original partly visible)
				GetDlgItem(ID_MEDIUM)->Invalidate();
			}

			void PreInitDialog() override{
				// dialog initialization
				// . base
				__super::PreInitDialog();
				// . displaying Firmware information
				SetDlgItemText( ID_SYSTEM, rkfb.firmware );
				// . if DoubleTrackStep changed manually, adjusting the text of the ID_40D80 checkbox
				GetDlgItemText( ID_40D80,  doubleTrackDistanceTextOrg, sizeof(doubleTrackDistanceTextOrg)/sizeof(TCHAR) );
				if (rkfb.params.userForcedDoubleTrackStep)
					WindowProc( WM_COMMAND, ID_40D80, 0 );
				CheckDlgButton( ID_40D80, rkfb.params.doubleTrackStep );
				// . some settings are changeable only during InitialEditing
				static const WORD InitialSettingIds[]={ ID_ROTATION, ID_ACCURACY, ID_DEFAULT1, ID_TRACK, 0 };
				EnableDlgItems( InitialSettingIds, initialEditing );
				// . displaying inserted Medium information
				RefreshMediumInformation();
			}

			void DoDataExchange(CDataExchange* pDX) override{
				// exchange of data from and to controls
				// . Precision
				DDX_CBIndex( pDX, ID_ROTATION,	params.precision );
				// . FluxDecoder
				int tmp=params.fluxDecoder;
				DDX_CBIndex( pDX, ID_ACCURACY,	tmp );
				params.fluxDecoder=(TParams::TFluxDecoder)tmp;
				tmp=params.resetFluxDecoderOnIndex;
				DDX_Check( pDX, ID_DEFAULT1,	tmp );
				params.resetFluxDecoderOnIndex=tmp!=0;
				// . CalibrationAfterError
				tmp=params.calibrationAfterError;
				DDX_Radio( pDX,	ID_NONE,		tmp );
				params.calibrationAfterError=(TParams::TCalibrationAfterError)tmp;
				// . CalibrationStepDuringFormatting
				EnableDlgItem( ID_NUMBER, tmp=params.calibrationStepDuringFormatting!=0 );
				DDX_Radio( pDX,	ID_ZERO,		tmp );
				if (tmp)
					DDX_Text( pDX,	ID_NUMBER,	tmp=params.calibrationStepDuringFormatting );
				else
					SetDlgItemInt(ID_NUMBER,4,FALSE);
				params.calibrationStepDuringFormatting=tmp;
				// . NormalizeReadTracks
				tmp=params.normalizeReadTracks;
				DDX_Check( pDX,	ID_TRACK,		tmp );
				params.normalizeReadTracks=tmp!=0;
				// . FormattedTracksVerification
				tmp=params.verifyFormattedTracks;
				DDX_Check( pDX,	ID_VERIFY_TRACK,	tmp );
				params.verifyFormattedTracks=tmp!=0;
				// . WrittenDataVerification
				tmp=params.verifyWrittenData;
				DDX_Check( pDX,	ID_VERIFY_SECTOR,	tmp );
				params.verifyWrittenData=tmp!=0;
			}

			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				switch (msg){
					case WM_PAINT:
						// drawing
						__super::OnPaint();
						WrapDlgItemsByClosingCurlyBracketWithText( ID_NONE, ID_SECTOR, _T("if error encountered"), 0 );
						WrapDlgItemsByClosingCurlyBracketWithText( ID_ZERO, ID_CYLINDER_N, _T("when formatting"), 0 );
						return 0;
					case WM_COMMAND:
						switch (wParam){
							case ID_RECOVER:
								// refreshing information on (inserted) floppy
								if (initialEditing) // if no Tracks are yet formatted ...
									SetDlgItemText( ID_40D80, doubleTrackDistanceTextOrg ); // ... then resetting the flag that user has overridden DoubleTrackDistance
								RefreshMediumInformation();
								break;
							case ID_40D80:{
								// track distance changed manually
								TCHAR buf[sizeof(doubleTrackDistanceTextOrg)/sizeof(TCHAR)+20];
								SetDlgItemText( ID_40D80, ::lstrcat(::lstrcpy(buf,doubleTrackDistanceTextOrg),_T(" (user forced)")) );
								break;
							}
							case ID_ZERO:
							case ID_CYLINDER_N:
								// adjusting possibility to edit the CalibrationStep according to selected option
								EnableDlgItem( ID_NUMBER, wParam!=ID_ZERO );
								break;
							case IDOK:
								// attempting to confirm the Dialog
								params.doubleTrackStep=IsDlgButtonChecked( ID_40D80 )!=BST_UNCHECKED;
								params.userForcedDoubleTrackStep=IsDoubleTrackDistanceForcedByUser();
								break;
						}
						break;
				}
				return __super::WindowProc(msg,wParam,lParam);
			}
		public:
			TParams params;

			CParamsDialog(CKryoFluxBase &rkfb,bool initialEditing)
				// ctor
				: Utils::CRideDialog(IDR_KRYOFLUX_ACCESS)
				, rkfb(rkfb) , params(rkfb.params) , initialEditing(initialEditing) {
			}
		} d( *this, initialEditing );
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			params=d.params;
			capsImageInfo.maxcylinder=( FDD_CYLINDERS_MAX>>(BYTE)params.doubleTrackStep )-1; // inclusive!
			return true;
		}else
			return false;
	}








	BYTE CKryoFluxBase::GetAvailableRevolutionCount() const{
		// returns the number of data variations of one Sector that are guaranteed to be distinct
		return 2+params.precision*2;
	}

	TStdWinError CKryoFluxBase::MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus){
		// marks Sector on a given PhysicalAddress as "dirty", plus sets it the given FdcStatus; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}

	TStdWinError CKryoFluxBase::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - "re-normalizing" already read Tracks according to the new Medium
		if (params.normalizeReadTracks && !m_strPathName.IsEmpty()) // normalization makes sense only for existing Images - it's useless for Images just created
			if (pFormat->mediumType!=Medium::UNKNOWN) // a particular Medium specified ...
				if (floppyType!=pFormat->mediumType) // ... and it's different
					for( TCylinder cyl=0; cyl<FDD_CYLINDERS_MAX; cyl++ )
						for( THead head=0; head<2; head++ )
							if (auto pit=internalTracks[cyl][head]){
								pit->SetMediumType(pFormat->mediumType);
								if (!pit->Normalize())
									return ERROR_UNRECOGNIZED_MEDIA;
							}
		// - base
		return __super::SetMediumTypeAndGeometry( pFormat, sideMap, firstSectorNumber );
	}

	TStdWinError CKryoFluxBase::FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte){
		// formats given Track {Cylinder,Head} to the requested NumberOfSectors, each with corresponding Length and FillerByte as initial content; returns Windows standard i/o error
		// - must support the Codec specified
		if ((codec&properties->supportedCodecs)==0)
			return ERROR_NOT_SUPPORTED;
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return ERROR_INVALID_PARAMETER;
		// - disposing previous Track, if any
		PInternalTrack &rit=internalTracks[cyl][head];
		if (rit!=nullptr)
			delete rit, rit=nullptr;
		// - defining the new Track layout
		UBYTE bitBuffer[32768];
		CapsFormatTrack cft;
			CapsFormatBlock cfb[(TSector)-1];
				::ZeroMemory( cfb, sizeof(*cfb)*nSectors );
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
					r.sectorlen=GetOfficialSectorLength(rid.lengthCode);
				}
			::ZeroMemory( &cft, sizeof(cft) );
			cft.gapacnt= nSectors<11 ? 60 : 2;
			cft.gapavalue=0x4e;
			cft.gapbvalue=0x4e;
			cft.trackbuf=bitBuffer;
			if (const Medium::PCProperties p=Medium::GetProperties(floppyType))
				cft.tracklen=(p->nCells+7)/8; // # of bits to # of whole Bytes
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
		if ( rit=CInternalTrack::CreateFrom( *this, &cti, 1, 0 ) ){
			rit->modified=true;
			SetModifiedFlag();
			return ERROR_SUCCESS;
		}else
			return ERROR_GEN_FAILURE;
	}

	TStdWinError CKryoFluxBase::UnformatTrack(TCylinder cyl,THead head){
		// unformats given Track {Cylinder,Head}; returns Windows standard i/o error
		if (const Medium::PCProperties mp=Medium::GetProperties(floppyType)){
			// . checking that specified Track actually CAN exist
			if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
				return ERROR_INVALID_PARAMETER;
			// . preparing Track content
			const DWORD nLogTimes=mp->nCells*2;
			CTrackReaderWriter trw( nLogTimes, CTrackReader::TDecoderMethod::FDD_KEIR_FRASIER, true );
				trw.AddIndexTime(0);
					for( TLogTime t=0; t<nLogTimes; trw.AddTime(++t) );
				trw.AddIndexTime( nLogTimes );
				trw.SetMediumType(floppyType);
				if (!trw.Normalize())
					return ERROR_MEDIA_INCOMPATIBLE;
			// . disposal of previous content
			if (const PInternalTrack pit=internalTracks[cyl][head])
				delete pit;
			// . creation of new content
			if ( const PInternalTrack pit = internalTracks[cyl][head] = CInternalTrack::CreateFrom(*this,trw) ){
				pit->modified=true;
				return ERROR_SUCCESS;
			}else
				return ERROR_NOT_ENOUGH_MEMORY;
		}else
			return ERROR_MEDIA_INCOMPATIBLE;
	}









	struct TIndexPulse sealed{
		DWORD posInStreamData;
		DWORD sampleCounter;
		DWORD indexCounter;
	};

	#define MASTER_CLOCK_DEFAULT	(18432000*73/14/2)
	#define SAMPLE_CLOCK_DEFAULT	(MASTER_CLOCK_DEFAULT/2)
	#define INDEX_CLOCK_DEFAULT		(MASTER_CLOCK_DEFAULT/16)

	CImage::CTrackReaderWriter CKryoFluxBase::StreamToTrack(LPBYTE rawBytes,DWORD nBytes) const{
		// creates and returns a Track representation of the Stream data
		const PCBYTE inStreamData=rawBytes; // "in-stream-data" only
		// - parsing the input raw Bytes obtained from the KryoFlux device (eventually producing an error)
		bool isKryofluxStream=false; // assumption (actually NOT a KryoFlux Stream)
		LPBYTE pis=rawBytes; // "in-stream-data" only
		DWORD nFluxes=0;
		TIndexPulse indexPulses[Revolution::MAX];
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
									if (nIndexPulses==Revolution::MAX){
										::SetLastError(ERROR_INVALID_INDEX);
										return CTrackReaderWriter::Invalid;
									}
									indexPulses[nIndexPulses++]=*(TIndexPulse *)rawBytes;
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
									for( LPCSTR param=::strtok( ::lstrcpyn(buf,(LPCSTR)rawBytes,size), _T(",") ); param!=nullptr; param=::strtok(nullptr,_T(",")) ){
										while (::isspace(*param))
											param++;
										if (!::strncmp(param,"name=",5))
											isKryofluxStream|=::strstr( param, "KryoFlux" )!=nullptr;
										else if (!::strncmp(param,"sck=",4)){
											const double tmp=::atof(param+4); // a custom Sample-Clock value in defined ...
											if ((int)tmp!=SAMPLE_CLOCK_DEFAULT) // ... and it is different from the Default
												sck=tmp;
										}else if (!::strncmp(param,"ick=",4)){
											const double tmp=::atof(param+4); // a custom Index-Clock value in defined ...
											if ((int)tmp!=INDEX_CLOCK_DEFAULT) // ... and it is different from the Default
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
		CTrackReader::TDecoderMethod decoderMethod;
		switch (params.fluxDecoder){
			default:
				ASSERT(FALSE);
				//fallthrough
			case TParams::TFluxDecoder::KEIR_FRASIER:
				decoderMethod=CTrackReader::TDecoderMethod::FDD_KEIR_FRASIER; break;
			case TParams::TFluxDecoder::MARK_OGDEN:
				decoderMethod=CTrackReader::TDecoderMethod::FDD_MARK_OGDEN; break;
		}
		CTrackReaderWriter result( nFluxes, decoderMethod, params.resetFluxDecoderOnIndex );
		DWORD sampleCounter=0, totalSampleCounter=0; // delta and absolute sample counters
		PLogTime buffer=result.GetBuffer(),pLogTime=buffer;
		BYTE nearestIndexPulse=0;
		DWORD nearestIndexPulsePos= nIndexPulses>0 ? indexPulses[0].posInStreamData : INT_MAX;
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
						ASSERT( 0x800<=sampleCounter && sampleCounter<=0xffff );
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
				const DWORD indexSampleCounter=totalSampleCounter+indexPulses[nearestIndexPulse].sampleCounter;
				if (sck==0) // default Sample-Clock, allowing for relatively precise computation of absolute timing
					result.AddIndexTime( (LONGLONG)TIME_SECOND(1)*indexSampleCounter/SAMPLE_CLOCK_DEFAULT ); // temporary 64-bit precision even on 32-bit machines
				else // custom Sample-Clock, involving floating-point number computation
					result.AddIndexTime( (LONGLONG)TIME_SECOND(1)*indexSampleCounter/sck ); // temporary 64-bit precision even on 32-bit machines
				nearestIndexPulsePos= ++nearestIndexPulse<nIndexPulses ? indexPulses[nearestIndexPulse].posInStreamData : -1;
			}
			// . adding the flux into the Buffer
			totalSampleCounter+=sampleCounter;
			if (sck==0) // default Sample-Clock, allowing for relatively precise computation of absolute timing
				*pLogTime++= (LONGLONG)TIME_SECOND(1)*totalSampleCounter/SAMPLE_CLOCK_DEFAULT; // temporary 64-bit precision even on 32-bit machines
			else // custom Sample-Clock, involving floating-point number computation
				*pLogTime++= (LONGLONG)TIME_SECOND(1)*totalSampleCounter/sck; // temporary 64-bit precision even on 32-bit machines
			sampleCounter=0;
		}
		result.AddTimes( buffer, pLogTime-buffer );
		return result;
	}
