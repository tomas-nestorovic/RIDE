#include "stdafx.h"
#include "CapsBase.h"
#include "KryoFluxBase.h"


	CKryoFluxBase::CKryoFluxBase(PCProperties properties,LPCTSTR firmware)
		// ctor
		// - base
		: CCapsBase( properties, true )
		// - initialization
		, firmware(firmware) {
		canBeModified=false; // modifications not possible at the moment
	}







	#define INI_KRYOFLUX	_T("Kflux")

	#define INI_FIRMWARE_FILE			_T("fw")
	#define INI_FLUX_DECODER			_T("decod")
	#define INI_CALIBRATE_SECTOR_ERROR	_T("clberr")
	#define INI_CALIBRATE_FORMATTING	_T("clbfmt")
	#define INI_NORMALIZE_READ_TRACKS	_T("nrt")
	#define INI_VERIFY_FORMATTING		_T("vrftr")
	#define INI_VERIFY_WRITTEN_DATA		_T("vrfdt")


	CKryoFluxBase::TParams::TParams()
		// ctor
		// - persistent (saved and loaded)
		: firmwareFileName( app.GetProfileString(INI_KRYOFLUX,INI_FIRMWARE_FILE) )
		, fluxDecoder( (TFluxDecoder)app.GetProfileInt(INI_KRYOFLUX,INI_FLUX_DECODER,TFluxDecoder::KEIR_FRASIER) )
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
		app.WriteProfileInt( INI_KRYOFLUX, INI_FLUX_DECODER, fluxDecoder );
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
				TMedium::TType mt;
				static const WORD Interactivity[]={ ID_LATENCY, ID_NUMBER2, ID_GAP };
				if (!EnableDlgItems( Interactivity, rkfb.GetInsertedMediumType(0,mt)==ERROR_SUCCESS ))
					SetDlgItemText( ID_MEDIUM, _T("Not inserted") );
				// . attempting to recognize any previous format on the floppy
				else
					switch (mt){
						case TMedium::FLOPPY_DD_525:
							SetDlgItemText( ID_MEDIUM, _T("5.25\" DD formatted, 360 RPM drive") );
							if (EnableDlgItem( ID_40D80, initialEditing )){
								const bool doubleTrackStep0=rkfb.params.doubleTrackStep;
									rkfb.params.doubleTrackStep=false;
									if (rkfb.GetInsertedMediumType(0,mt)==ERROR_SUCCESS)
										CheckDlgButton( ID_40D80, mt==TMedium::UNKNOWN ); // first Track is empty, so likely each odd Track is empty
								rkfb.params.doubleTrackStep=doubleTrackStep0;
								rkfb.GetInsertedMediumType(0,mt); // a workaround to make floppy Drive head seek home
							}
							break;
						case TMedium::FLOPPY_DD:
							SetDlgItemText( ID_MEDIUM, _T("3.5\"/5.25\" DD formatted, 300 RPM drive") );
							CheckDlgButton( ID_40D80,  EnableDlgItem( ID_40D80, false )  );
							break;
						case TMedium::FLOPPY_HD_525:
						case TMedium::FLOPPY_HD_350:
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
				static const WORD InitialSettingIds[]={ ID_ACCURACY, ID_TRACK, 0 };
				EnableDlgItems( InitialSettingIds, initialEditing );
				// . displaying inserted Medium information
				RefreshMediumInformation();
			}

			void DoDataExchange(CDataExchange* pDX) override{
				// exchange of data from and to controls
				// . FluxDecoder
				int tmp=params.fluxDecoder;
				DDX_CBIndex( pDX, ID_ACCURACY,	tmp );
				params.fluxDecoder=(TParams::TFluxDecoder)tmp;
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








	TStdWinError CKryoFluxBase::MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus){
		// marks Sector on a given PhysicalAddress as "dirty", plus sets it the given FdcStatus; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}

	TStdWinError CKryoFluxBase::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - "re-normalizing" already read Tracks according to the new Medium
		if (params.normalizeReadTracks)
			if (pFormat->mediumType!=TMedium::UNKNOWN) // a particular Medium specified ...
				if (floppyType!=pFormat->mediumType) // ... and it's different
					for( TCylinder cyl=0; cyl<FDD_CYLINDERS_MAX; cyl++ )
						for( THead head=0; head<2; head++ )
							if (auto pit=internalTracks[cyl][head]){
								pit->SetMediumType(pFormat->mediumType);
								pit->Normalize();
							}
		// - base
		return __super::SetMediumTypeAndGeometry( pFormat, sideMap, firstSectorNumber );
	}

	TStdWinError CKryoFluxBase::FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte){
		// formats given Track {Cylinder,Head} to the requested NumberOfSectors, each with corresponding Length and FillerByte as initial content; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}

	TStdWinError CKryoFluxBase::UnformatTrack(TCylinder cyl,THead head){
		// unformats given Track {Cylinder,Head}; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}









	#define MASTER_CLOCK_DEFAULT	(18432000*73/14/2)
	#define SAMPLE_CLOCK_DEFAULT	(MASTER_CLOCK_DEFAULT/2)
	#define INDEX_CLOCK_DEFAULT		(MASTER_CLOCK_DEFAULT/16)

	CKryoFluxBase::CKfStream::CKfStream(LPBYTE rawBytes,DWORD nBytes)
		// ctor
		// - initialization
		: nIndexPulses(0)
		, errorState(ERROR_SUCCESS)
		, mck(0) , sck(0) , ick(0) // all defaults, allowing flux computation with minimal precision loss
		// - parsing the input raw Bytes obtained from the KryoFlux device (eventually producing an error)
		, inStreamData(rawBytes) , inStreamDataLength(0)
		, nFluxes(0) {
		bool isKryofluxStream=false; // assumption (despite its extension, this file is actually NOT a KryoFlux Stream)
		LPBYTE pis=rawBytes; // "in-stream-data" only
		for( const PCBYTE pLastRawByte=rawBytes+nBytes; rawBytes<pLastRawByte; ){
			const BYTE header=*rawBytes++;
			if (rawBytes+3>pLastRawByte){ // "+3" = we should finish with an Out-of-Stream mark whose Header has just been read, leaving 3 unread Bytes
badFormat:		errorState=ERROR_BAD_FORMAT;
				return;
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
									if (nIndexPulses==DEVICE_REVOLUTIONS_MAX){
										errorState=ERROR_INVALID_INDEX;
										return;
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
											errorState=ERROR_BUFFER_OVERFLOW;
											return;
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
		inStreamDataLength=pis-inStreamData;
		if (!isKryofluxStream) // not explicitly confirmed that this is a KryoFlux Stream
			errorState=ERROR_BAD_FILE_TYPE;
	}

	CImage::CTrackReaderWriter CKryoFluxBase::CKfStream::ToTrack(const CKryoFluxBase &kfb) const{
		// creates and returns a Track representation of the Stream data
		CTrackReader::TDecoderMethod decoderMethod;
		switch (kfb.params.fluxDecoder){
			case TParams::TFluxDecoder::KEIR_FRASIER:
				decoderMethod=CTrackReader::TDecoderMethod::FDD_KEIR_FRASIER; break;
			default:
				ASSERT(FALSE); break;
		}
		CTrackReaderWriter result( nFluxes, decoderMethod );
		DWORD sampleCounter=0;
		TLogTime prevTime=0,*buffer=result.GetBuffer(),*pLogTime=buffer;
		BYTE nearestIndexPulse=0;
		DWORD nearestIndexPulsePos= nIndexPulses>0 ? indexPulses[0].posInStreamData : -1;
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
				if (sck==0){ // default Sample-Clock, allowing for relatively precise computation of absolute timing
					const LONGLONG tmp=(LONGLONG)TIME_SECOND(1)*indexPulses[nearestIndexPulse].sampleCounter; // temporary 64-bit precision even on 32-bit machines
					result.AddIndexTime( prevTime + tmp/SAMPLE_CLOCK_DEFAULT );
				}else{ // custom Sample-Clock, involving floating-point number computation
					const double tmp=(double)TIME_SECOND(1)*sampleCounter; // temporary 64-bit precision even on 32-bit machines
					result.AddIndexTime( prevTime + (TLogTime)(tmp/sck) );
				}
				nearestIndexPulsePos= ++nearestIndexPulse<nIndexPulses ? indexPulses[nearestIndexPulse].posInStreamData : -1;
			}
			// . adding the flux into the Buffer
			if (sck==0){ // default Sample-Clock, allowing for relatively precise computation of absolute timing
				const LONGLONG tmp=(LONGLONG)TIME_SECOND(1)*sampleCounter; // temporary 64-bit precision even on 32-bit machines
				*pLogTime++= prevTime += tmp/SAMPLE_CLOCK_DEFAULT;
			}else{ // custom Sample-Clock, involving floating-point number computation
				const double tmp=(double)TIME_SECOND(1)*sampleCounter; // temporary 64-bit precision even on 32-bit machines
				*pLogTime++= prevTime += (TLogTime)(tmp/sck);
			}
			sampleCounter=0;
		}
		result.AddTimes( buffer, pLogTime-buffer );
		return result;
	}
