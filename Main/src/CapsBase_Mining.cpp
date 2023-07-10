#include "stdafx.h"
#include "CapsBase.h"
#include "Charting.h"

	#define INI_MINING		_T("MineTrk")
	#define INI_TARGET		_T("trg")
	#define INI_METHOD		_T("mtd")
	#define INI_CALIBRATION	_T("calib")

	#define MSG_CANT_MINE_TRACK	_T("« This track cannot be mined »")

	TStdWinError CCapsBase::MineTrack(TCylinder cyl,THead head){
		// begins mining of specified Track; returns Windows standard i/o error
		// - has a valid Track been specified?
{		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (cyl>=GetCylinderCount() || head>=GetHeadCount())
			return ERROR_INVALID_PARAMETER; // yes, the mining is supported but the parameters are invalid
}		// - defining the Dialog
		static constexpr WORD MiningParamIds[]={ ID_ACCURACY, ID_CREATOR, ID_HEAD, 0 };
		#define MINED_TRACK_TIMES_COUNT_MAX	800
		class CTrackMiningDialog sealed:public Utils::CRideDialog{
			CCapsBase &cb;
			const TCylinder cyl;
			const THead head;
			const Utils::CRidePen minedTimingPen;
			const Utils::CRidePen minedIndexPen;
			const Utils::CCallocPtr<POINT> minedTrackDeltaTiming;
			POINT minedTrackIndices[Revolution::MAX+1];
			struct TGraphics sealed{
				CChartView::PCGraphics list[2];

				inline TGraphics(){ ::ZeroMemory( this, sizeof(*this) ); }
			} graphics;
			CChartView::CXyDisplayInfo di;
			std::unique_ptr<CChartView::CXyPointSeries> minedTrackDeltaTimingSeries;
			std::unique_ptr<CChartView::CXyOrderedBarSeries> minedTrackIndexSeries;
			CChartView scatterPlotView;
			volatile bool miningRunning;
			CBackgroundAction miningWorker;

			enum TMiningTarget{
				TARGET_NONE,
				TARGET_ALL_STD_SECTORS_PRESENT		=2,
				TARGET_ALL_STD_SECTORS_HEALTHY,
				TARGET_ALL_CURRENT_SECTORS_PRESENT	=4,
				TARGET_ALL_CURRENT_SECTORS_HEALTHY,
			} miningTarget;

			enum TMiningMethod{
				METHOD_NONE,
				METHOD_READING_FROM_DRIVE
			};

			enum THeadCalibration{
				HEAD_DONT_CALIBRATE					=INT_MAX,
				HEAD_CALIBRATE_ON_START				=1,
				HEAD_CALIBRATE_EACH_10_REVOLUTIONS	=10
			} headCalibration;

			void PreInitDialog() override{
				// dialog initialization
				// . base
				__super::PreInitDialog();
				// . dialog title
				TCHAR buf[80];
				const TPhysicalAddress chs={ cyl, head };
				::wsprintf( buf, _T("Mining Track %d (Cyl %d, Head %d)"), chs.GetTrackNumber(cb.GetHeadCount()), cyl, head );
				SetWindowText(buf);
				// . populating the "Mining target" combo-box
				const PCInternalTrack pit=cb.internalTracks[cyl][head];
				CComboBox cbx;
				cbx.Attach( GetDlgItemHwnd(ID_ACCURACY) );
					if (cb.dos->properties!=&CUnknownDos::Properties){
						cbx.SetItemData(
							cbx.AddString(_T("All standard sectors healthy")),
							TARGET_ALL_STD_SECTORS_HEALTHY
						);
						cbx.SetItemData(
							cbx.AddString(_T("All standard sectors present (ID healthy, data present)")),
							TARGET_ALL_STD_SECTORS_PRESENT
						);
					}
					if (pit && pit->nSectors){
						cbx.SetItemData(
							cbx.AddString(_T("All current sectors healthy")),
							TARGET_ALL_CURRENT_SECTORS_HEALTHY
						);
						cbx.SetItemData(
							cbx.AddString(_T("All current sectors present (ID healthy, data present)")),
							TARGET_ALL_CURRENT_SECTORS_PRESENT
						);
					}
					if (!EnableDlgItem( ID_ACCURACY, cbx.GetCount()>0 )){
						EnableDlgItem( IDOK, false );
						cbx.AddString(MSG_CANT_MINE_TRACK);
					}
					cbx.SetCurSel(0);
				cbx.Detach();
				if (const auto savedValue=app.GetProfileInt( INI_MINING, INI_TARGET, TARGET_NONE ))
					SelectDlgComboBoxValue( ID_ACCURACY, savedValue, false );
				// . populating the "Mining approach" combo-box
				cbx.Attach( GetDlgItemHwnd(ID_CREATOR) );
					if (cb.properties->IsRealDevice())
						cbx.SetItemData(
							cbx.AddString(_T("Repeated reading from drive")),
							METHOD_READING_FROM_DRIVE
						);
					if (!EnableDlgItem( ID_CREATOR, cbx.GetCount()>0 )){
						EnableDlgItem( IDOK, false );
						cbx.AddString(MSG_CANT_MINE_TRACK);
					}
					cbx.SetCurSel(0);
				cbx.Detach();
				if (const auto savedValue=app.GetProfileInt( INI_MINING, INI_METHOD, METHOD_NONE ))
					SelectDlgComboBoxValue( ID_CREATOR, savedValue, false );
				// . creating a Scatter Plot at position of the placeholder
				scatterPlotView.Create(
					nullptr, nullptr,
					AFX_WS_DEFAULT_VIEW&~WS_BORDER|WS_CLIPSIBLINGS,
					MapDlgItemClientRect(ID_CHART), this, AFX_IDW_PANE_FIRST
				);
				if (pit)
					ShowScatterPlotOfTrack(*pit);
				// . Head calibration options
				cbx.Attach( GetDlgItemHwnd(ID_HEAD) );
					cbx.SetItemData(
						cbx.AddString(_T("Don't calibrate head")),
						HEAD_DONT_CALIBRATE
					);
					cbx.SetItemData(
						cbx.AddString(_T("Calibrate head once on start")),
						HEAD_CALIBRATE_ON_START
					);
					for( char nRevs=HEAD_CALIBRATE_EACH_10_REVOLUTIONS,buf[80]; nRevs<=30; nRevs+=10 ){
						::wsprintfA( buf, _T("Calibrate head each %d revolutions"), nRevs );
						cbx.SetItemData( cbx.AddString(buf), nRevs );
					}
				cbx.Detach();
				SelectDlgComboBoxValue( ID_HEAD, headCalibration );
			}

			void ShowScatterPlotOfTrack(const CImage::CTrackReaderWriter &trw){
				// populates the ScatterPlotView with Track's simplified timing
				// . indices
				for( BYTE i=0; i<trw.GetIndexCount(); i++ ){
					POINT &r=minedTrackIndices[i];
						r.x=trw.GetIndexTime(i);
						r.y=TIME_MICRO(200); // should suffice for any Medium
				}
				std::swap( *minedTrackIndices, minedTrackIndices[trw.GetIndexCount()-1] ); // swapping first and last index pulse to disable application of percentiles, unapplicable to Index pulses
				minedTrackIndexSeries.reset(
					new CChartView::CXyOrderedBarSeries( trw.GetIndexCount(), minedTrackIndices, minedIndexPen )
				);
				graphics.list[0]=minedTrackIndexSeries.get();
				// . timing
				const auto iTimeStride=(trw.GetTimesCount()+MINED_TRACK_TIMES_COUNT_MAX-1)/MINED_TRACK_TIMES_COUNT_MAX; // round up so that we never overrun the buffer
				const PCLogTime trackTiming=trw.GetBuffer();
				LPPOINT pxy=minedTrackDeltaTiming;
				for( DWORD i=1; i<trw.GetTimesCount(); i+=iTimeStride,pxy++ ){
					pxy->x=trackTiming[i];
					pxy->y=trackTiming[i]-trackTiming[i-1];
				}
				minedTrackDeltaTimingSeries.reset(
					new CChartView::CXyPointSeries( pxy-minedTrackDeltaTiming.get(), minedTrackDeltaTiming, minedTimingPen )
				);
				graphics.list[1]=minedTrackDeltaTimingSeries.get();
				// . displaying the simplified Track
				di.RefreshDrawingLimits( trw.GetTotalTime() );
				scatterPlotView.Invalidate();
			}

			void TerminateMining(){
				if (miningRunning){
					miningRunning=false;
					::WaitForSingleObject( miningWorker, INFINITE );
					EnableDlgItems( MiningParamIds, true );
					SetDlgItemText( IDOK, _T("Start") );
				}
			}

			void OnOK() override{
				// dialog confirmed
				if (miningRunning)
					// terminating the mining
					TerminateMining();
				else{
					// launching the mining
					miningRunning=true;
					miningTarget=(TMiningTarget)GetDlgComboBoxSelectedValue(ID_ACCURACY);
					headCalibration=(THeadCalibration)GetDlgComboBoxSelectedValue(ID_HEAD);
					app.WriteProfileInt( INI_MINING, INI_TARGET, miningTarget ); // save for next time
					app.WriteProfileInt( INI_MINING, INI_CALIBRATION, headCalibration ); // save for next time
					if (headCalibration==HEAD_CALIBRATE_ON_START){ // initial (and the only) Head calibration
						if (const TStdWinError err=cb.SeekHeadsHome()){
							PostMiningErrorMessage(err);
							return;
						}
						headCalibration=HEAD_DONT_CALIBRATE;
					}
					const TMiningMethod miningMethod=(TMiningMethod)GetDlgComboBoxSelectedValue(ID_CREATOR);
					app.WriteProfileInt( INI_MINING, INI_METHOD, miningMethod ); // save for next time
					switch (miningMethod){
						case METHOD_READING_FROM_DRIVE:
							miningWorker.BeginAnother( RepeatedReadingFromDrive_thread, this, THREAD_PRIORITY_NORMAL );
							miningWorker.Resume();
							break;
						default:
							ASSERT(FALSE);
							break;
					}
					EnableDlgItems( MiningParamIds, false );
					SetDlgItemText( IDOK, _T("Stop") );
				}
			}

			void OnCancel() override{
				// dialog cancelled
				TerminateMining();
				__super::OnCancel();
			}

			BOOL OnCommand(WPARAM wParam,LPARAM lParam) override{
				// command processing
				switch (LOWORD(wParam)){
					case ID_ERROR:
						// mining error message
						TerminateMining();
						if (lParam)
							// an error
							Utils::FatalError( _T("Can't continue mining"), lParam );
						else{
							// a notification on successfull mining
							UpdateData(TRUE);
							EndDialog(IDOK);
						}
						return TRUE;
				}
				return __super::OnCommand(wParam,lParam);
			}

			TStdWinError PostMiningErrorMessage(TStdWinError err) const{
				::PostMessage( *this, WM_COMMAND, ID_ERROR, err );
				return err;
			}

			static UINT AFX_CDECL RepeatedReadingFromDrive_thread(PVOID _pBackgroundAction){
				// thread to mine specified Track using repeated reading from drive
				const CBackgroundAction &ba=*(PCBackgroundAction)_pBackgroundAction;
				CTrackMiningDialog &d=*(CTrackMiningDialog *)ba.GetParams();
				EXCLUSIVELY_LOCK_IMAGE(d.cb);
				struct{
					TSector n;
					TSectorId list[(TSector)-1];
				} searchedSectors;
				if (d.miningTarget&TARGET_ALL_STD_SECTORS_PRESENT)
					searchedSectors.n=d.cb.dos->GetListOfStdSectors( d.cyl, d.head, searchedSectors.list );
				else if (d.miningTarget&TARGET_ALL_CURRENT_SECTORS_PRESENT)
					searchedSectors.n=d.cb.ScanTrack( d.cyl, d.head, nullptr, searchedSectors.list );
				else{
					ASSERT(FALSE); // we shouldn't end up here!
					return d.PostMiningErrorMessage(ERROR_INVALID_OPERATION);
				}
				int nRevsToCalibration=d.headCalibration;
				PInternalTrack &rit=d.cb.internalTracks[d.cyl][d.head];
				while (d.miningRunning){
					// . calibrate Head
					if (!--nRevsToCalibration){
						if (const TStdWinError err=d.cb.SeekHeadsHome())
							return d.PostMiningErrorMessage(err);
						nRevsToCalibration=d.headCalibration;
					}
					// . putting existing Track aside
					std::unique_ptr<CInternalTrack> pitMined;
			{		const Utils::CVarTempReset<PInternalTrack> pitOrg( rit, nullptr );
					// . rescanning the Track
					TSectorId foundSectors[(TSector)-1];
					const TSector nFoundSectors=d.cb.ScanTrack( d.cyl, d.head, nullptr, foundSectors );
					pitMined.reset(rit);
					// . displaying the Track
					d.ShowScatterPlotOfTrack( *rit );
					// . evaluating the Track against the MiningTarget
					TSector i=0;
					switch (d.miningTarget){
						case TARGET_ALL_STD_SECTORS_HEALTHY:
						case TARGET_ALL_CURRENT_SECTORS_HEALTHY:
							for( WORD w; i<searchedSectors.n; i++ )
								if (!d.cb.GetHealthySectorData( d.cyl, d.head, searchedSectors.list+i, &w ))
									break;
							break;
						case TARGET_ALL_STD_SECTORS_PRESENT:
						case TARGET_ALL_CURRENT_SECTORS_PRESENT:
							for( WORD w; i<searchedSectors.n; i++ )
								if (!d.cb.GetSectorData( d.cyl, d.head, Revolution::ANY_GOOD, searchedSectors.list+i, i, &w ))
									break;
							break;
						default:
							ASSERT(FALSE); // we shouldn't end up here!
							return d.PostMiningErrorMessage(ERROR_INVALID_OPERATION);
					}
					if (i<searchedSectors.n) // some Sectors not healthy or missing?
						continue;
			}		// . Track mined successfully
					if (const TStdWinError err=d.cb.UnscanTrack( d.cyl, d.head )) // "forget" the existing Track
						return d.PostMiningErrorMessage(err);
					rit=pitMined.release();
					return d.PostMiningErrorMessage(ERROR_SUCCESS);
				}
				return d.PostMiningErrorMessage(ERROR_CANCELLED);
			}

		public:
			CTrackMiningDialog(CCapsBase &cb,TCylinder cyl,THead head)
				// ctor
				: Utils::CRideDialog(IDR_CAPS_MINING)
				, cb(cb) , cyl(cyl) , head(head)
				, miningTarget(TARGET_NONE)
				, headCalibration( (THeadCalibration)app.GetProfileInt( INI_MINING, INI_CALIBRATION, HEAD_DONT_CALIBRATE ) )
				, minedTimingPen( 2, COLOR_RED )
				, minedIndexPen( 2, COLOR_BLUE )
				, minedTrackDeltaTiming( Utils::MakeCallocPtr<POINT,int>(MINED_TRACK_TIMES_COUNT_MAX) )
				, di(
					CChartView::TMargin::Default, graphics.list, ARRAYSIZE(graphics.list), Utils::CRideFont::StdBold,
					's', INT_MIN, Utils::CTimeline::TimePrefixes,
					's', INT_MIN, Utils::CTimeline::TimePrefixes
					)
				, scatterPlotView(di)
				, miningRunning(false) {
			}
		} d( *this, cyl, head );
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			return ERROR_SUCCESS;
		}else
			return ERROR_CANCELLED;
	}
