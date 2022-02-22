#include "stdafx.h"
#include "CapsBase.h"

	#define INI_SECTION_PATTERN	_T("precomp%c%d")

	CCapsBase::CPrecompensation::CPrecompensation(char driveLetter)
		// ctor
		: driveLetter(driveLetter) , floppyType(Medium::UNKNOWN)
		, methodVersion(TMethodVersion::Identity) {
	}










	#define INI_METHOD_VERSION	_T("mver")

	void CCapsBase::CPrecompensation::Load(Medium::TType floppyType){
		// loads existing or default values for specified FloppyType
		this->floppyType=floppyType;
		TCHAR iniSection[32];
		::wsprintf( iniSection, INI_SECTION_PATTERN, driveLetter, floppyType );
		switch (methodVersion=(TMethodVersion)app.GetProfileInt(iniSection,INI_METHOD_VERSION,None)){
			case MethodVersion1:{
				double *pd=&v1.coeffs[0][0];
				for( TCHAR i=0,iniValue[]=_T("coefA"); i<sizeof(v1.coeffs)/sizeof(v1.coeffs[0][0]); i++,iniValue[4]++ )
					*pd++=(int)app.GetProfileInt( iniSection, iniValue, 0 )/1e8;
				break;
			}
			case MethodVersion2:{
				double *pd=&v2.coeffs[0][0][0];
				for( TCHAR i=0,iniValue[]=_T("coefA"); i<sizeof(v2.coeffs)/sizeof(v2.coeffs[0][0][0]); i++,iniValue[4]++ )
					*pd++=(int)app.GetProfileInt( iniSection, iniValue, 0 )/1e8;
				break;
			}
		}
	}

	void CCapsBase::CPrecompensation::Save() const{
		// saves current values for specified FloppyType
		if (floppyType==Medium::UNKNOWN)
			return;
		TCHAR iniSection[32];
		::wsprintf( iniSection, INI_SECTION_PATTERN, driveLetter, floppyType );
		app.WriteProfileInt( iniSection, INI_METHOD_VERSION, methodVersion );
		switch (methodVersion){
			case MethodVersion1:{
				const double *pd=&v1.coeffs[0][0];
				for( TCHAR i=0,iniValue[]=_T("coefA"); i<sizeof(v1.coeffs)/sizeof(v1.coeffs[0][0]); i++,iniValue[4]++ )
					app.WriteProfileInt( iniSection, iniValue, *pd++*1e8 );
				break;
			}
			case MethodVersion2:{
				const double *pd=&v2.coeffs[0][0][0];
				for( TCHAR i=0,iniValue[]=_T("coefA"); i<sizeof(v2.coeffs)/sizeof(v2.coeffs[0][0][0]); i++,iniValue[4]++ )
					app.WriteProfileInt( iniSection, iniValue, *pd++*1e8 );
				break;
			}
		}
	}

	struct TPrecompThreadParams sealed{
		const CCapsBase &cb;
		const TCylinder cyl;
		const THead head;
		const BYTE nTrials;
		const PVOID pPrecomp; // (CCapsBase::CPrecompensation *)

		TPrecompThreadParams(const CCapsBase &cb,TCylinder cyl,THead head,BYTE nTrials,PVOID pPrecomp)
			: cb(cb)
			, cyl(cyl) , head(head) , nTrials(nTrials) , pPrecomp(pPrecomp) {
		}
	};

	UINT AFX_CDECL CCapsBase::CPrecompensation::PrecompensationDetermination_thread(PVOID pCancelableAction){
		// thread to determine Precompensation parameters using the latest Method
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const TPrecompThreadParams ptp=*(TPrecompThreadParams *)pAction->GetParams();
		CCapsBase::CPrecompensation &rPrecomp=*(CCapsBase::CPrecompensation *)ptp.pPrecomp;
		// - determination of new write pre-compensation parameters
		constexpr BYTE nNeighboringFluxes=2; // must be an EVEN number!
		constexpr BYTE nEvaluationFluxes=2+1+2; // "N+1+N", N = fluxes considered before/after the current flux
		decltype(rPrecomp.latest) trialResults[9];
		const BYTE nTrials=std::min<BYTE>( ptp.nTrials, sizeof(trialResults)/sizeof(*trialResults) );
		const Medium::PCProperties pMediumProps=Medium::GetProperties(rPrecomp.floppyType);
		if (!pMediumProps)
			return pAction->TerminateWithError(ERROR_UNRECOGNIZED_MEDIA);
		const auto &mediumProps=*pMediumProps;
		EXCLUSIVELY_LOCK_IMAGE(ptp.cb); // locking the access so that no one can disturb during the testing
		pAction->SetProgressTarget( nTrials+1 );
		TCylinder cyl=ptp.cyl;
		while (--cyl>0) // finding a Cylinder with magnetically reliable Track
			if (pAction->Cancelled)
				return ERROR_CANCELLED;
			else if (const TStdWinError err=ptp.cb.DetermineMagneticReliabilityByWriting( rPrecomp.floppyType, cyl, ptp.head, pAction->Cancelled )){
				if (err!=ERROR_DISK_CORRUPT)
					return pAction->TerminateWithError(err);
			}else
				break;
		BYTE distances[24576];
		constexpr WORD nRows=sizeof(distances)/2-nEvaluationFluxes;
		struct TMatrixRow sealed{
			BYTE correctDistances[nEvaluationFluxes];
		};
		const auto A=Utils::MakeCallocPtr<TMatrixRow>(nRows);
		for( BYTE nFailures=0,trial=0; trial<nTrials; ){
			if (pAction->Cancelled)
				return ERROR_CANCELLED;
			// . composition of test Track
			CTrackReaderWriter trw( mediumProps.nCells, CTrackReader::FDD_KEIR_FRASER, false );
				trw.SetMediumType(rPrecomp.floppyType);
				trw.AddIndexTime(0);
				trw.AddIndexTime(mediumProps.revolutionTime);
			TLogTime t=0, const doubleCellTime=2*mediumProps.cellTime;
			for( BYTE n=200; n>0; n-- ) // MFM-like inspection window stabilisation
				trw.AddTime( t+=doubleCellTime );
			for( BYTE n=10; n>0; n-- ) // indication of beginning of test data
				trw.AddTime( t+=5*mediumProps.cellTime );
			for( WORD n=0; n<sizeof(distances); n++ ){ // generating test data
				distances[n]=std::min<BYTE>( 4, 2+(::rand()&3) ); // "2+" = between two 1's must always be at least one 0 (otherwise pre-compensation is useless, e.g. FM encoding)
				trw.AddTime( t+=distances[n]*mediumProps.cellTime );
			}
			while (t<mediumProps.revolutionTime) // filling the remainder of the Track
				trw.AddTime( t+=doubleCellTime );
			trw.AddTime( t+=doubleCellTime ); // one extra flux
			// . saving the test Track
			PInternalTrack pit=CInternalTrack::CreateFrom( ptp.cb, trw );
			pit->modified=true; // to pass the save conditions
			std::swap( pit, ptp.cb.internalTracks[cyl][ptp.head] );
				const auto precompMethod0=ptp.cb.precompensation.methodVersion;
				ptp.cb.precompensation.methodVersion=CPrecompensation::Identity;
					const Medium::TType mt0=ptp.cb.floppyType;
					Medium::TType &mt=*const_cast<Medium::TType *>(&ptp.cb.floppyType);
					mt=rPrecomp.floppyType;
						const TStdWinError err=ptp.cb.SaveTrack( cyl, ptp.head, pAction->Cancelled );
					mt=mt0;
				ptp.cb.precompensation.methodVersion=precompMethod0;
			std::swap( pit, ptp.cb.internalTracks[cyl][ptp.head] );
			delete pit;
			if (err!=ERROR_SUCCESS)
				return pAction->TerminateWithError(err);
			// . reading the test Track back
			pit=ptp.cb.internalTracks[cyl][ptp.head];
				ptp.cb.internalTracks[cyl][ptp.head]=nullptr; // forcing a new scan
				ptp.cb.ScanTrack(cyl,ptp.head);
			std::swap( ptp.cb.internalTracks[cyl][ptp.head], pit );
			if (pit==nullptr)
				return pAction->TerminateWithError(ERROR_FUNCTION_FAILED);
			// . evaluating what we read
			CTrackReader tr=*pit;
			delete pit;
			TLogTime t0=tr.GetIndexTime(0)+120*mediumProps.cellTime; // "+N" = ignoring the region immediatelly after index - may be invalid due to Write Gate signal still on
			tr.SetCurrentTime(t0);
			for( const TLogTime threshold=mediumProps.cellTime*3; ( t=tr.ReadTime() )-t0<threshold; t0=t ); // skipping initial stabilisation
			for( BYTE n=0; ++n<10; t=tr.ReadTime() ); // skipping indication of test data begin
			const TLogTime testBeginTime=tr.GetCurrentTime(); // this is where the test data begin
			for( WORD n=0; n<sizeof(distances); n++ ){ // checking that the TestBeginTime has been determined correctly
				t0=t, t=tr.ReadTime();
				const TLogTime dt=t-t0;
				if (dt<mediumProps.cellTime*distances[n]*7/10 || mediumProps.cellTime*distances[n]*13/10<dt) // allowing 30% tolerance
					if (nFailures++==3) // found unexpected flux - TestBeginTime determined wronly or the Drive writes too badly for precompensation to be computed reliably
						return pAction->TerminateWithError(ERROR_FUNCTION_FAILED);
					else{
						#ifdef _DEBUG
							tr.ShowModal(_T("Sanity check failed!"));
						#endif
						goto nextTrial;
					}
			}
			// . computation of precompensation parameters using the latest Method
				for( BYTE p=0; p<2; p++ ){ // even (0) and odd (1) fluxes
					// : composing matrix "A" whose rows consist of one pivot flux and its left/right neighbors, and a vector "dt" of pivot flux differences introduced during writing
					tr.SetCurrentTime( t0=testBeginTime );
					for( BYTE n=nNeighboringFluxes; n-->0; t0=tr.ReadTime() );
					if (p)
						t0=tr.ReadTime();
					TLogTime dt[nRows]; // time misrecognition of fluxes
					for( WORD i=0; i<nRows; i++ ){
						PBYTE row=A[i].correctDistances;
						::memcpy( row, distances+p+i*2, nEvaluationFluxes );
						//t0=tr.ReadTime();
						t=tr.ReadTime();
						dt[i]= row[nNeighboringFluxes]*mediumProps.cellTime - (t-t0); // the pivot flux in the middle
						t0=tr.ReadTime();
					}
					// : using the Ordinary Least Squares method to transform an overdetermined system of equations to a normal system with matrix M and vector B
					double M[nEvaluationFluxes][nEvaluationFluxes];
					for( BYTE r=0; r<nEvaluationFluxes; r++ ) // row in M
						for( BYTE c=0; c<nEvaluationFluxes; c++ ){ // column in M
							int tmp=0;
							for( WORD i=0; i<nRows; i++ )
								tmp+=A[i].correctDistances[r]*A[i].correctDistances[c];
							M[r][c]=TIME_MICRO(tmp); // Distance*Distance*Microseconds
						}
					double b[nEvaluationFluxes];
					for( BYTE r=0; r<nEvaluationFluxes; r++ ){ // row in B
						int tmp=0;
						for( WORD i=0; i<nRows; i++ )
							tmp+=A[i].correctDistances[r]*dt[i];
						b[r]=tmp; // Distance*RealDistance*Microseconds
					}
					// . solving the normal system of equations using Gaussian method
					for( BYTE c=0; c<nEvaluationFluxes-1; c++ )
						for( BYTE r=c; ++r<nEvaluationFluxes; ){
							const double k1=M[r][c], k2=M[c][c];
							for( BYTE i=c-1; ++i<nEvaluationFluxes; M[r][i]=k1*M[c][i]-k2*M[r][i] );
							#ifdef _DEBUG
								M[r][c]=0;
							#endif
							b[r]=k1*b[c]-k2*b[r];
						}
					for( BYTE r=nEvaluationFluxes; r-->0; ){
						double sum=b[r];
						for( BYTE i=r; ++i<nEvaluationFluxes; sum-=M[r][i]*b[i] );
						trialResults[trial].coeffs[ptp.head][p][r] = b[r] = sum/M[r][r];
					}
				}
			// . next trial
			pAction->UpdateProgress( ++trial );
nextTrial:	;
		}
		// - putting partial results into final one
		::ZeroMemory( rPrecomp.latest.coeffs[ptp.head], sizeof(rPrecomp.latest.coeffs[ptp.head]) );
		for( BYTE p=0; p<2; p++ ) // even (0) and odd (1) fluxes
			for( BYTE r=0; r<nEvaluationFluxes; r++ ){
				for( BYTE trial=0; trial<nTrials; trial++ )
					rPrecomp.latest.coeffs[ptp.head][p][r]+=trialResults[trial].coeffs[ptp.head][p][r];
				rPrecomp.latest.coeffs[ptp.head][p][r]/=(nTrials*2); // 2 = a flux has two ends (start and end), both of which are compensated independently, hence each must contribute just a half to the final flux length
			}
		// - write pre-compensation parameters successfully determined using the latest Method
		rPrecomp.methodVersion=MethodLatest;
		return pAction->TerminateWithSuccess();
	}

	TStdWinError CCapsBase::CPrecompensation::DetermineUsingLatestMethod(const CCapsBase &cb,BYTE nTrials){
		// determines parameters for write pre-compensation (zero Trials to get current status); returns Windows standard i/o error
		// - handling a request to retrieve precompensation status for specified FloppyType
		if (!driveLetter)
			return ERROR_NOT_SUPPORTED; // flux precompensation is needed only for physical floppy drives
		if (floppyType==Medium::UNKNOWN)
			return ERROR_UNRECOGNIZED_MEDIA; // unknown FloppyType to determine precompensation for
		if (!nTrials)
			if (methodVersion==None)
				return ERROR_INVALID_DATA; // precompensation not yet determined for this Drive and FloppyType
			else if (methodVersion<MethodLatest)
				return ERROR_EVT_VERSION_TOO_OLD; // a new determination Method is available
			else
				return ERROR_SUCCESS; // precompensation determined using the latest Method available
		// - requesting insertion of a blank floppy
		if (!Utils::InformationOkCancel(_T("Insert an empty disk that you don't mind writing to, and hit OK.")))
			return ERROR_CANCELLED;
		// - determination of new write pre-compensation parameters
		CBackgroundMultiActionCancelable bmac(THREAD_PRIORITY_TIME_CRITICAL);
			const TPrecompThreadParams paramsHead0( cb, 40, 0, nTrials, this );
			bmac.AddAction( PrecompensationDetermination_thread, &paramsHead0, _T("Head 0") );
			const TPrecompThreadParams paramsHead1( cb, 40, 1, nTrials, this );
			bmac.AddAction( PrecompensationDetermination_thread, &paramsHead1, _T("Head 1") );
		if (const TStdWinError err=bmac.Perform())
			return err;
		// - write pre-compensation parameters successfully determined using the latest Method
		return ERROR_SUCCESS;
	}



	void CCapsBase::CPrecompensation::ShowOrDetermineModal(const CCapsBase &cb){
		// displays summary of precompensation Method and its parameters
		EXCLUSIVELY_LOCK_IMAGE(cb); // no one must disturb us
		// - defining the Dialog
		class CPrecompDialog sealed:public Utils::CRideDialog{
			const CCapsBase &cb;
			CPrecompensation precomp;

			void ShowReportForLoadedFloppyType(){
				// displays a report on precompensation for FloppyType selected in dedicated combo-box
				if (precomp.floppyType==Medium::UNKNOWN)
					SetDlgItemText( ID_INFORMATION, _T("Select floppy type first.") );
				else{
					TCHAR report[2048],*p=report;
					switch (const TStdWinError err=precomp.DetermineUsingLatestMethod(cb,0)){
						case ERROR_EVT_VERSION_TOO_OLD:
						case ERROR_SUCCESS:
							break;
						default:
							SetDlgItemText( ID_INFORMATION, _T("No report available.") );
							return;
					}
					p+=::wsprintf( p, _T("Drive letter: %c (0x%02X)\r\nFloppy type: %s\r\nMethod version: %d\r\n\r\n"), precomp.driveLetter, precomp.driveLetter, Medium::GetDescription(precomp.floppyType), precomp.methodVersion );
					switch (precomp.methodVersion){
						case MethodVersion1:
							p+=::lstrlen( ::lstrcpy(p,_T("\r\nEven flux coefficients:\r\n")) );
							for( BYTE i=0; i<sizeof(precomp.v1.coeffs[0])/sizeof(precomp.v1.coeffs[0][0]); i++ )
								p+=_stprintf( p, _T("- %d: %f\r\n"), i, precomp.v1.coeffs[0][i] );
							p+=::lstrlen( ::lstrcpy(p,_T("\r\nOdd flux coefficients:\r\n")) );
							for( BYTE i=0; i<sizeof(precomp.v1.coeffs[1])/sizeof(precomp.v1.coeffs[1][0]); i++ )
								p+=_stprintf( p, _T("- %d: %f\r\n"), i, precomp.v1.coeffs[1][i] );
							break;
						case MethodVersion2:
							for( TCHAR heading[]=_T("\r\nHEAD 0\r\n"); heading[7]<'2'; heading[7]++ ){
								auto &coeffs=precomp.v2.coeffs[heading[7]-'0'];
								p+=::lstrlen( ::lstrcpy(p,heading) );
								p+=::lstrlen( ::lstrcpy(p,_T("\r\nEven flux coefficients:\r\n")) );
								for( BYTE i=0; i<sizeof(coeffs[0])/sizeof(coeffs[0][0]); i++ )
									p+=_stprintf( p, _T("- %d: %f\r\n"), i, coeffs[0][i] );
								p+=::lstrlen( ::lstrcpy(p,_T("\r\nOdd flux coefficients:\r\n")) );
								for( BYTE i=0; i<sizeof(coeffs[1])/sizeof(coeffs[1][0]); i++ )
									p+=_stprintf( p, _T("- %d: %f\r\n"), i, coeffs[1][i] );
							}
							break;
						default:
							ASSERT(FALSE);
							break;
					}
					SetDlgItemText( ID_INFORMATION, report );
				}
			}

			void PreInitDialog() override{
				// dialog initialization
				// - base
				__super::PreInitDialog();
				// - populating dedicated combo-box with supported FloppyTypes
				const HWND hMedium=GetDlgItemHwnd(ID_MEDIUM);
				CImage::PopulateComboBoxWithCompatibleMedia( hMedium, Medium::FLOPPY_ANY, cb.properties );
				SelectDlgComboBoxValue( ID_MEDIUM, precomp.floppyType );
				// - displaying precompensation report
				ShowReportForLoadedFloppyType();
				// - setting caption of the button to determine precompensation params
				TCHAR caption[32];
				::wsprintf( caption, _T("Determine using Method %d"), MethodLatest );
				SetDlgItemText( IDRETRY, caption );
			}

			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				switch (msg){
					case WM_COMMAND:
						switch (wParam){
							case MAKELONG(ID_MEDIUM,CBN_SELCHANGE):
								precomp.Load( (Medium::TType)GetDlgComboBoxSelectedValue(ID_MEDIUM) );
								ShowReportForLoadedFloppyType();
								break;
							case IDRETRY:
								cb.locker.Unlock(); // giving way to parallel thread
									if (const TStdWinError err=precomp.DetermineUsingLatestMethod(cb))
										Utils::FatalError( _T("Couldn't determine precompensation"), err );
									else{
										precomp.Save();
										ShowReportForLoadedFloppyType();
									}
								cb.locker.Lock();
								break;
						}
						break;
				}
				return __super::WindowProc(msg,wParam,lParam);
			}

		public:
			CPrecompDialog(const CCapsBase &cb,const CPrecompensation &precomp)
				// ctor
				: Utils::CRideDialog( IDR_CAPS_PRECOMPENSATION )
				, cb(cb)
				, precomp(precomp) {
			}
		} d( cb, *this );
		// - showing the Dialog and processing its result
		d.DoModal();
	}

	TStdWinError CCapsBase::CPrecompensation::ApplyTo(const CCapsBase &cb,TCylinder cyl,THead head,CTrackReaderWriter trw) const{
		// applies current Precompensation parameters (if any) to the Track - it's up to the caller to Load them!; returns Windows standard i/o error
		// - precompensation must be determined (it's up to the caller to Load the parameters!)
		switch (const TStdWinError err=const_cast<CPrecompensation *>(this)->DetermineUsingLatestMethod(cb,0)){
			case ERROR_SUCCESS:
			case ERROR_UNRECOGNIZED_MEDIA: // unformatted Medium is fine to be unrecognized
				break;
			default:
				return err;
		}
		// - zeroth index must be at zero Time
		if (trw.RewindToIndex(0))
			return ERROR_INVALID_DATA;
		// - extracting first N non-compensated fluxes from the Track
		DWORD nTimes=trw.GetTimesCount();
		const BYTE nOrigTimes=8;
		if (nTimes<nOrigTimes) // a Track must consist of at least N fluxes
			return ERROR_INVALID_DATA;
		TLogTime origFluxes[nOrigTimes]; // non-compensated flux timing
		PLogTime pt=trw.GetBuffer();
		*origFluxes=*pt;
		for( BYTE i=1; i<nOrigTimes; i++ )
			origFluxes[i]=pt[i]-pt[i-1];
		// - application
		for( DWORD i=0; i<nTimes; i++,pt++ )
			switch (methodVersion){
				case Identity:
					return ERROR_SUCCESS;
				case MethodVersion1:{
					constexpr BYTE COEFFS_COUNT=sizeof(v1.coeffs[0])/sizeof(*v1.coeffs[0]);
					constexpr BYTE PIVOT_INDEX=COEFFS_COUNT/2;
					const auto &coeffs=v1.coeffs[(i+PIVOT_INDEX)&1];
					if (i<nTimes-COEFFS_COUNT){ // applicable range
						double compensation=0;
						for( BYTE c=0; c<COEFFS_COUNT; c++ )
							compensation+=coeffs[c] * 2*trw.GetCurrentProfile().iwTimeDefault;//origFluxes[c];
						pt[PIVOT_INDEX]+=compensation+.5;
					}
					::memmove( origFluxes, origFluxes+1, (COEFFS_COUNT-1)*sizeof(TLogTime) );
					origFluxes[COEFFS_COUNT-1]=pt[COEFFS_COUNT]-pt[COEFFS_COUNT-1];
					break;
				}
				case MethodVersion2:{
					const auto &headCoeffs=v2.coeffs[head];
					constexpr BYTE COEFFS_COUNT=sizeof(headCoeffs[0])/sizeof(*headCoeffs[0]);
					constexpr BYTE PIVOT_INDEX=COEFFS_COUNT/2;
					const auto &coeffs=headCoeffs[(i+PIVOT_INDEX)&1];
					if (i<nTimes-COEFFS_COUNT){ // applicable range
						double compensation=0;
						for( BYTE c=0; c<COEFFS_COUNT; c++ )
							compensation+=coeffs[c] * 2*trw.GetCurrentProfile().iwTimeDefault;//origFluxes[c];
						pt[PIVOT_INDEX]+=compensation+.5;
					}
					::memmove( origFluxes, origFluxes+1, (COEFFS_COUNT-1)*sizeof(TLogTime) );
					origFluxes[COEFFS_COUNT-1]=pt[COEFFS_COUNT]-pt[COEFFS_COUNT-1];
					break;
				}
				default:
					ASSERT(FALSE);
					return ERROR_UNKNOWN_COMPONENT;
			}
		// - assuring that all fluxes are after pre-compensation still chronologically ordered
		pt=trw.GetBuffer();
		for( DWORD tPrev=*pt,i=1,j=1; i<nTimes; j++ ) // i = "target" position, j = "source" position
			if (pt[j]<tPrev){
				// no - the J-th flux should already been processed, meaning the Previous flux was a "tachyon" one!
				ASSERT(FALSE);
				nTimes--; // keeping the Previous "tachyon" flux, discarding current flux
			}else
				// yes, the two consecutive fluxes are chronologically ordered
				tPrev = pt[i++] = pt[j];
		trw.TrimToTimesCount( nTimes );
		// - precompensated successfully
		return ERROR_SUCCESS;
	}
