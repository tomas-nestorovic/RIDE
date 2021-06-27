#include "stdafx.h"
#include "CapsBase.h"
#include "KryoFluxBase.h"


	CKryoFluxBase::CKryoFluxBase(PCProperties properties,char realDriveLetter,LPCTSTR firmware)
		// ctor
		// - base
		: CCapsBase( properties, realDriveLetter, true )
		// - initialization
		, firmware(firmware) {
		// - setting a classical 5.25" floppy geometry
		capsImageInfo.maxcylinder=FDD_CYLINDERS_HD/2+FDD_CYLINDERS_EXTRA - 1; // "-1" = inclusive!
		capsImageInfo.maxhead=2-1; // inclusive!
	}







	#define INI_KRYOFLUX	_T("Kflux")

	#define INI_FIRMWARE_FILE			_T("fw")
	#define INI_FLUX_DECODER			_T("decod")
	#define INI_FLUX_DECODER_RESET		_T("drst")
	#define INI_PRECISION				_T("prec")
	#define INI_CALIBRATE_SECTOR_ERROR	_T("clberr")
	#define INI_CALIBRATE_FORMATTING	_T("clbfmt")
	#define INI_VERIFY_WRITTEN_TRACKS	_T("vwt")


	CKryoFluxBase::TParams::TParams()
		// ctor
		// - persistent (saved and loaded)
		: firmwareFileName( app.GetProfileString(INI_KRYOFLUX,INI_FIRMWARE_FILE) )
		, precision( app.GetProfileInt(INI_KRYOFLUX,INI_PRECISION,0) )
		, fluxDecoder( (TFluxDecoder)app.GetProfileInt(INI_KRYOFLUX,INI_FLUX_DECODER,TFluxDecoder::KEIR_FRASER) )
		, resetFluxDecoderOnIndex( (TFluxDecoder)app.GetProfileInt(INI_KRYOFLUX,INI_FLUX_DECODER_RESET,true)!=0 )
		, calibrationAfterError( (TCalibrationAfterError)app.GetProfileInt(INI_KRYOFLUX,INI_CALIBRATE_SECTOR_ERROR,TCalibrationAfterError::ONCE_PER_CYLINDER) )
		, calibrationStepDuringFormatting( app.GetProfileInt(INI_KRYOFLUX,INI_CALIBRATE_FORMATTING,0) )
		, corrections( INI_KRYOFLUX )
		, verifyWrittenTracks( app.GetProfileInt(INI_KRYOFLUX,INI_VERIFY_WRITTEN_TRACKS,true)!=0 )
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
		corrections.Save( INI_KRYOFLUX );
		app.WriteProfileInt( INI_KRYOFLUX, INI_VERIFY_WRITTEN_TRACKS, verifyWrittenTracks );
	}

	bool CKryoFluxBase::EditSettings(bool initialEditing){
		// True <=> new settings have been accepted (and adopted by this Image), otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - defining the Dialog
		class CParamsDialog sealed:public Utils::CRideDialog{
			const bool initialEditing;
			CKryoFluxBase &rkfb;
			CPrecompensation tmpPrecomp;
			TCHAR doubleTrackDistanceTextOrg[80];

			bool IsDoubleTrackDistanceForcedByUser() const{
				// True <=> user has manually overridden DoubleTrackDistance setting, otherwise False
				return ::lstrlen(doubleTrackDistanceTextOrg)!=::GetWindowTextLength( GetDlgItemHwnd(ID_40D80) );
			}

			void RefreshMediumInformation(){
				// detects a floppy in the Drive and attempts to recognize its Type
				// . making sure that a floppy is in the Drive
				ShowDlgItem( ID_INFORMATION, false );
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
									if (rkfb.GetInsertedMediumType(1,mt)==ERROR_SUCCESS)
										CheckDlgButton( ID_40D80, !ShowDlgItem(ID_INFORMATION,mt!=Medium::UNKNOWN) ); // first Track is empty, so likely each odd Track is empty
								rkfb.params.doubleTrackStep=doubleTrackStep0;
								rkfb.GetInsertedMediumType(0,mt); // a workaround to make floppy Drive head seek home
							}
							break;
						case Medium::FLOPPY_DD:
							SetDlgItemText( ID_MEDIUM, _T("3.5\"/5.25\" DD formatted, 300 RPM drive") );
							CheckDlgButton( ID_40D80, false );
							EnableDlgItem( ID_40D80, initialEditing );
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
				// . refreshing the status of Precompensation
				tmpPrecomp.Load(mt);
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
				switch (const TStdWinError err=tmpPrecomp.DetermineUsingLatestMethod(rkfb,0)){
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
				SetDlgItemText( ID_SYSTEM, rkfb.firmware );
				// . if DoubleTrackStep changed manually, adjusting the text of the ID_40D80 checkbox
				GetDlgItemText( ID_40D80,  doubleTrackDistanceTextOrg, sizeof(doubleTrackDistanceTextOrg)/sizeof(TCHAR) );
				if (rkfb.params.userForcedDoubleTrackStep)
					WindowProc( WM_COMMAND, ID_40D80, 0 );
				CheckDlgButton( ID_40D80, rkfb.params.doubleTrackStep );
				// . some settings are changeable only during InitialEditing
				static const WORD InitialSettingIds[]={ ID_ROTATION, ID_ACCURACY, ID_DEFAULT1, ID_TRACK, ID_TIME, 0 };
				EnableDlgItems( InitialSettingIds, initialEditing );
				// . displaying inserted Medium information
				SetDlgItemSingleCharUsingFont( // a warning that a 40-track disk might have been misrecognized
					ID_INFORMATION,
					L'\xf0ea', (HFONT)Utils::CRideFont(FONT_WEBDINGS,175,false,true).Detach()
				);
				RefreshMediumInformation();
				// . updating write pre-compensation status
				SetDlgItemSingleCharUsingFont( // a warning that pre-compensation not up-to-date
					ID_INSTRUCTION,
					L'\xf0ea', (HFONT)Utils::CRideFont(FONT_WEBDINGS,175,false,true).Detach()
				);
				RefreshPrecompensationStatus();
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
				tmp=params.corrections.use;
				DDX_Check( pDX,	ID_TRACK,		tmp );
				params.corrections.use=tmp!=0;
				EnableDlgItem( ID_TRACK, params.fluxDecoder!=TParams::TFluxDecoder::NO_FLUX_DECODER&&initialEditing );
				// . WrittenTracksVerification
				tmp=params.verifyWrittenTracks;
				DDX_Check( pDX,	ID_VERIFY_TRACK,	tmp );
				params.verifyWrittenTracks=tmp!=0;
			}

			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				switch (msg){
					case WM_PAINT:
						// drawing
						__super::OnPaint();
						WrapDlgItemsByClosingCurlyBracketWithText( ID_NONE, ID_SECTOR, _T("if error encountered"), 0 );
						WrapDlgItemsByClosingCurlyBracketWithText( ID_ZERO, ID_CYLINDER_N, _T("when writing"), 0 );
						return 0;
					case WM_COMMAND:
						switch (wParam){
							case MAKELONG(ID_ACCURACY,CBN_SELCHANGE):{
								// FluxDecoder changed
								const TParams::TFluxDecoder fd0=rkfb.params.fluxDecoder;
									rkfb.params.fluxDecoder=(TParams::TFluxDecoder)ComboBox_GetCurSel( GetDlgItemHwnd(ID_ACCURACY) );
									if (!EnableDlgItem( ID_TRACK, rkfb.params.fluxDecoder!=TParams::TFluxDecoder::NO_FLUX_DECODER ))
										CheckDlgButton( ID_TRACK, BST_UNCHECKED ); // when archiving, any corrections must be turned off
									SendMessage( WM_COMMAND, ID_RECOVER ); // refresh information on inserted Medium
								rkfb.params.fluxDecoder=fd0;
								break;
							}
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
								ShowDlgItem( ID_INFORMATION, false ); // user manually revised the Track distance, so no need to continue display the warning
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
					case WM_NOTIFY:
						switch (((LPNMHDR)lParam)->code){
							case NM_CLICK:
							case NM_RETURN:{
								PNMLINK pLink=(PNMLINK)lParam;
								const LITEM &item=pLink->item;
								if (pLink->hdr.idFrom==ID_ALIGN){
									rkfb.locker.Unlock(); // giving way to parallel thread
										const bool vwt0=params.verifyWrittenTracks;
										params.verifyWrittenTracks=false;
											if (!::lstrcmpW(item.szID,L"details"))
												tmpPrecomp.ShowOrDetermineModal(rkfb);
											else if (!::lstrcmpW(item.szID,L"compute"))
												if (const TStdWinError err=tmpPrecomp.DetermineUsingLatestMethod(rkfb))
													Utils::FatalError( _T("Can't determine precompensation"), err );
												else
													tmpPrecomp.Save();
										params.verifyWrittenTracks=vwt0;
									rkfb.locker.Lock();
									RefreshMediumInformation();
								}else if (pLink->hdr.idFrom==ID_TIME)
									params.corrections.ShowModal(this);
								break;
							}
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
				, rkfb(rkfb) , params(rkfb.params) , initialEditing(initialEditing) , tmpPrecomp(rkfb.precompensation) {
			}
		} d( *this, initialEditing );
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			params=d.params;
			capsImageInfo.maxcylinder=( FDD_CYLINDERS_HD>>(BYTE)params.doubleTrackStep )+FDD_CYLINDERS_EXTRA - 1; // "-1" = inclusive!
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

	TStdWinError CKryoFluxBase::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - "re-normalizing" already read Tracks according to the new Medium
		if (params.corrections.use && !m_strPathName.IsEmpty()) // normalization makes sense only for existing Images - it's useless for Images just created
			if (pFormat->mediumType!=Medium::UNKNOWN) // a particular Medium specified ...
				if (floppyType!=pFormat->mediumType || dos!=nullptr) // ... and A|B, A = it's different, B = setting final MediumType
					for( TCylinder cyl=0; cyl<FDD_CYLINDERS_MAX; cyl++ )
						for( THead head=0; head<2; head++ )
							if (auto pit=internalTracks[cyl][head]){
								pit->SetMediumType(pFormat->mediumType);
								if (dos!=nullptr){ // DOS already known
									CTrackReaderWriter trw=*pit;
									if (const TStdWinError err=params.corrections.ApplyTo(trw))
										return err;
									delete pit;
									internalTracks[cyl][head]=CInternalTrack::CreateFrom( *this, trw );
								}//the following commented out as it brings little to no readability improvement and leaves Tracks influenced by the MediumType
								//else if (params.corrections.indexTiming) // DOS still being recognized ...
									//if (!pit->Normalize()) // ... hence can only improve readability by adjusting index-to-index timing
										//return ERROR_INVALID_MEDIA;

							}
		// - base
		return __super::SetMediumTypeAndGeometry( pFormat, sideMap, firstSectorNumber );
	}

	TStdWinError CKryoFluxBase::WriteTrack(TCylinder cyl,THead head,CTrackReader tr){
		// converts general description of the specified Track into Image-specific representation; caller may provide Invalid TrackReader to check support of this feature; returns Windows standard i/o error
		// - TrackReader must be valid
		if (!tr) // caller is likely checking the support of this feature
			return ERROR_INVALID_DATA; // yes, it's supported but the TrackReader is invalid
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return ERROR_INVALID_PARAMETER;
		// - disposing previous Track, if any
		PInternalTrack &rit=internalTracks[cyl][head];
		if (rit!=nullptr)
			delete rit, rit=nullptr;
		// - creation of new content
		if ( rit = CInternalTrack::CreateFrom(*this,CTrackReaderWriter(tr)) ){
			rit->modified=true;
			return ERROR_SUCCESS;
		}else
			return ERROR_NOT_ENOUGH_MEMORY;
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
					r.sectorlen=bufferLength[s]; //GetOfficialSectorLength(rid.lengthCode);
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
			CTrackReaderWriter trw( nLogTimes, CTrackReader::TDecoderMethod::FDD_KEIR_FRASER, true );
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

	DWORD CKryoFluxBase::TimeToStdSampleCounter(TLogTime t){
		return (LONGLONG)t*SAMPLE_CLOCK_DEFAULT/TIME_SECOND(1);
	}

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
			case TParams::TFluxDecoder::NO_FLUX_DECODER:
				decoderMethod=CTrackReader::TDecoderMethod::NONE; break;
			case TParams::TFluxDecoder::KEIR_FRASER:
				decoderMethod=CTrackReader::TDecoderMethod::FDD_KEIR_FRASER; break;
			case TParams::TFluxDecoder::MARK_OGDEN:
				decoderMethod=CTrackReader::TDecoderMethod::FDD_MARK_OGDEN; break;
		}
		CTrackReaderWriter result( nFluxes*125/100, decoderMethod, params.resetFluxDecoderOnIndex ); // allowing for 25% of false "ones" introduced by "FDC-like" decoders
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
				const DWORD indexSampleCounter=totalSampleCounter+indexPulses[nearestIndexPulse].sampleCounter;
				if (sck==0) // default Sample-Clock, allowing for relatively precise computation of absolute timing
					result.AddIndexTime( (LONGLONG)TIME_SECOND(1)*indexSampleCounter/SAMPLE_CLOCK_DEFAULT ); // temporary 64-bit precision even on 32-bit machines
				else // custom Sample-Clock, involving floating-point number computation
					result.AddIndexTime( (double)TIME_SECOND(1)*indexSampleCounter/sck ); // temporary 64-bit precision even on 32-bit machines
				nearestIndexPulsePos= ++nearestIndexPulse<nIndexPulses ? indexPulses[nearestIndexPulse].posInStreamData : -1;
			}
			// . adding the flux into the Buffer
			totalSampleCounter+=sampleCounter;
			if (sck==0) // default Sample-Clock, allowing for relatively precise computation of absolute timing
				*pLogTime++= (LONGLONG)TIME_SECOND(1)*totalSampleCounter/SAMPLE_CLOCK_DEFAULT; // temporary 64-bit precision even on 32-bit machines
			else // custom Sample-Clock, involving floating-point number computation
				*pLogTime++= (double)TIME_SECOND(1)*totalSampleCounter/sck; // temporary 64-bit precision even on 32-bit machines
			sampleCounter=0;
		}
		result.AddTimes( buffer, pLogTime-buffer );
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
			(LONGLONG)(indexTime-firstIndexTime)*INDEX_CLOCK_DEFAULT/TIME_SECOND(1) // temporary 64-bit precision even on 32-bit machines
		};
		::memcpy( outBuffer, &indexBlock, sizeof(indexBlock) );
		return sizeof(indexBlock);
	}

	DWORD CKryoFluxBase::TrackToStream(CTrackReader tr,LPBYTE outBuffer) const{
		// converts specified Track representation into Stream data and returns the length of the Stream
		PCHAR p=(PCHAR)outBuffer;
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
		static const char STREAM_END[]="\xd\xd\xd\xd\xd\xd\xd";
		return (PBYTE)::lstrcpyA(p,STREAM_END) + sizeof(STREAM_END)-1 - outBuffer;
	}
