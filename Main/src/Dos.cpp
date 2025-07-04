#include "stdafx.h"

	const TSide CDos::StdSidesMap[]={ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
	Utils::CPtrList<CDos::PCProperties> CDos::Known;


	PDos CDos::GetFocused(){
		return TDI_INSTANCE->GetCurrentTab()->image->dos;
	}

	void CDos::__errorCannotDoCommand__(TStdWinError cause){
		// reports on last command being not carried out due to given Cause
		Utils::FatalError(_T("Cannot carry out the command"),cause);
	}










	#define INI_SHELL_COMPLIANT_EXPORT_NAMES	_T("shcomp")
	#define INI_GETFILESIZE_OPTION				_T("gfsopt")

	CDos::CDos(PImage _image,PCFormat _pFormatBoot,TTrackScheme trackAccessScheme,PCProperties _properties,TFnCompareNames _fnCompareNames,PCSide _sideMap,UINT nResId,CFileManagerView *_pFileManager,TGetFileSizeOptions _getFileSizeDefaultOption,TSectorStatus unformatFatStatus)
		// ctor
		: image(_image) , properties(_properties) , fnCompareNames(_fnCompareNames)
		, sideMap(_sideMap) , menu(nResId) , pFileManager(_pFileManager)
		, formatBoot(*_pFormatBoot) // information on Medium Format retrieved from Boot; this information has ALWAYS priority when manipulating data on the disk; changes in this structure must be projected back to Boot Sector using FlushToBootSector (e.g. called automatically by BootView)
		, trackAccessScheme(trackAccessScheme) // single Scheme to access Tracks in Image
		, currentDir(DOS_DIR_ROOT)
		, generateShellCompliantExportNames( __getProfileBool__(INI_SHELL_COMPLIANT_EXPORT_NAMES,true) ) // True <=> the GetFileExportNameAndExt function must produce names that are compliant with the FAT32 file system, otherwise False
		, getFileSizeDefaultOption( (TGetFileSizeOptions)__getProfileInt__(INI_GETFILESIZE_OPTION,_getFileSizeDefaultOption) )
		, unformatFatStatus(unformatFatStatus) {
	}

	CDos::~CDos(){
		// dtor
		// - destroying HexaPreview of File's content associated with current FileManager
		if (CHexaPreview::pSingleInstance && &CHexaPreview::pSingleInstance->rFileManager==pFileManager)
			CHexaPreview::pSingleInstance->DestroyWindow();
		// - hiding DOS Menu
		menu.Hide();
		// - removing Tabs from TDI
		//nop (see CTdiTemplate)
	}









	static void __warnOnChangingCriticaSetting__(LPCTSTR lastSettingOperation){
		// warns that operation has been successfully performed on last setting but that the setting is critical for correct operation of the DOS
		#ifndef _DEBUG
			TCHAR buf[200];
			::wsprintf( buf, _T("Setting %s but YOU KNOW WHAT YOU ARE DOING."), lastSettingOperation );
			Utils::Information(buf);
		#endif
	}

	void CDos::__warnOnEnteringCriticalConfiguration__(bool b){
		TCHAR verb[16];
		__warnOnChangingCriticaSetting__(  ::lstrcat( ::lstrcpy(verb,_T("turned ")), b?_T("on"):_T("off") )  );
	}

	BYTE CDos::XorChecksum(LPCVOID bytes,WORD nBytes){
		// computes and returns the result of Bytes xor-ed
		return Yahel::Checksum::ComputeXor( bytes, nBytes );
	}

	int CDos::__getProfileInt__(LPCTSTR entryName,int defaultValue) const{
		// returns the value of specified Entry in this DOS'es profile; returns the DefaultValue if Entry isn't found
		TCHAR sectionName[80];
		return app.GetProfileInt( _itot(properties->id,sectionName,16), entryName, defaultValue );
	}

	void CDos::__writeProfileInt__(LPCTSTR entryName,int value) const{
		// writes the Value of specified Entry in this DOS'es profile
		TCHAR sectionName[80];
		app.WriteProfileInt( _itot(properties->id,sectionName,16), entryName, value );
	}

	bool CDos::__getProfileBool__(LPCTSTR entryName,bool defaultValue) const{
		// returns the value of specified Entry in this DOS'es profile; returns the DefaultValue if Entry isn't found
		return __getProfileInt__(entryName,defaultValue)!=0;
	}

	void CDos::__writeProfileBool__(LPCTSTR entryName,bool value) const{
		// writes the Value of specified Entry in this DOS'es profile
		__writeProfileInt__( entryName, value );
	}

	CString CDos::ValidateFormat(bool considerBoot,bool considerFat,RCFormat f) const{
		// returns reason why specified new Format cannot be accepted, or empty string if Format acceptable
		CString err;
		// - mustn't overflow geometry
		const DWORD nSectorsInTotal=f.GetCountOfAllSectors();
		const WORD clusterSize=f.clusterSize*( f.sectorLength - properties->dataBeginOffsetInSector - properties->dataEndOffsetInSector );
		if (nSectorsInTotal<properties->nSectorsInTotalMin){ // occurs only when fresh formatting a new Image
			err.Format(
				_T("The minimum number of sectors for a \"%s\" disk is %d (the new geometry yields only %d)"),
				properties->name, properties->nSectorsInTotalMin, nSectorsInTotal
			);
			return err;
		}
		if (clusterSize>properties->clusterSizeMax){
			err.Format(
				_T("The maximum cluster size for a \"%s\" disk is %d Bytes (it's %d Bytes now)"),
				properties->name, properties->clusterSizeMax, clusterSize
			);
			return err;
		}
		// - making sure excluded Cylinders are Empty
		if (considerFat){
			const TCylinder cylA=std::min(formatBoot.nCylinders,f.nCylinders), cylZ=std::max(formatBoot.nCylinders,f.nCylinders);
			const TCylinder cylLastOccupied=GetLastOccupiedStdCylinder();
			if (cylA<=cylLastOccupied)
				if (AreStdCylindersEmpty(cylA,cylZ-1)!=ERROR_EMPTY){
					err.Format(
						_T("Given disk occupation, the minimum number of cylinders is %d"),
						cylLastOccupied+1
					);
					return err;
				}
		}
		return err;
	}

	bool CDos::ValidateFormatAndReportProblem(bool considerBoot,bool considerFat,RCFormat f,LPCTSTR suggestion) const{
		// True <=> specified Format is acceptable, otherwise False (and informing on error)
		const CString &&err=ValidateFormat( considerBoot, considerFat, f );
		if (err.IsEmpty())
			return true;
		Utils::Information( _T("Invalid disk format"), err, suggestion );
		return false;
	}

	bool CDos::ChangeFormat(bool considerBoot,bool considerFat,RCFormat f){
		// True <=> specified Format is acceptable, otherwise False (and informing on error)
		return ValidateFormatAndReportProblem( considerBoot, considerFat, f );
	}

	TCylinder CDos::GetLastOccupiedStdCylinder() const{
		// finds and returns number of the last (at least partially) occupied Cylinder (0..N-1)
		TSectorId bufferId[(TSector)-1];
		if (formatBoot.mediumType!=Medium::UNKNOWN) // Unknown Medium if creating a new Image
			for( TCylinder cylMin=Medium::GetProperties(formatBoot.mediumType)->cylinderRange.iMax; cylMin--; )
				for( THead head=formatBoot.nHeads; head--; )
					if (ERROR_EMPTY!=IsTrackEmpty( cylMin, head, GetListOfStdSectors(cylMin,head,bufferId), bufferId ))
						return cylMin;
		return 0;
	}

	TSector CDos::GetListOfStdSectors(TCylinder cyl,THead head,PSectorId bufferId) const{
		// populates Buffer with standard ("official") Sector IDs for given Track and returns their count (Zone Bit Recording currently NOT supported!)
		const PCSide sides= image->GetSideMap() ? image->GetSideMap() : sideMap; // prefer Sides defined by Image, e.g. defined by user
		for( TSector s=properties->firstSectorNumber,nSectors=formatBoot.nSectors; nSectors--; bufferId++ )
			bufferId->cylinder=cyl, bufferId->side=sides[head], bufferId->sector=s++, bufferId->lengthCode=formatBoot.sectorLengthCode;
		return formatBoot.nSectors;
	}

	bool CDos::IsStdSector(RCPhysicalAddress chs) const{
		// True <=> specified Sector is recognized ("official") by the DOS, otherwise False
		return GetSectorStatus(chs)!=TSectorStatus::UNKNOWN;
	}

	CString CDos::ListStdSectors(TCylinder cyl,THead head) const{
		// creates and returns a List of standard DOS Sector IDs
		TSectorId ids[(TSector)-1];
		return TSectorId::List( ids, GetListOfStdSectors(cyl,head,ids) );
	}

	TStdWinError CDos::IsTrackEmpty(TCylinder cyl,THead head,TSector nSectors,PCSectorId sectors) const{
		// returns ERROR_EMPTY/ERROR_NOT_EMPTY or another Windows standard i/o error
		TSectorStatus statuses[(TSector)-1],*ps=statuses;
		if (!GetSectorStatuses(cyl,head,nSectors,sectors,statuses))
			return ERROR_SECTOR_NOT_FOUND;
		while (nSectors--)
			switch (*ps++){
				case TSectorStatus::SYSTEM:
				case TSectorStatus::OCCUPIED:
				case TSectorStatus::RESERVED:
				case TSectorStatus::SKIPPED:
				//case TSectorStatus::UNAVAILABLE:
					return ERROR_NOT_EMPTY;
			}
		return ERROR_EMPTY;
	}

	TStdWinError CDos::IsStdTrackEmpty(TCylinder cyl,THead head) const{
		// returns ERROR_EMPTY/ERROR_NOT_EMPTY or another Windows standard i/o error
		TSectorId bufferId[(TSector)-1];
		return IsTrackEmpty( cyl, head, GetListOfStdSectors(cyl,head,bufferId), bufferId );
	}

	CDos::TEmptyCylinderParams::TEmptyCylinderParams(const CDos *dos,TCylinder cylA,TCylinder cylZInclusive)
		: dos(dos)
		, cylA(cylA) , cylZInclusive(cylZInclusive) {
	}

	void CDos::TEmptyCylinderParams::AddAction(CBackgroundMultiActionCancelable &bmac) const{
		bmac.AddAction( Thread, this, _T("Checking region empty") );
	}

	UINT AFX_CDECL CDos::TEmptyCylinderParams::Thread(PVOID pCancelableAction){
		// thread to determine if given Cylinders are empty; return ERROR_SUCCESS/ERROR_NOT_EMPTY or another Windows standard i/o error
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const TEmptyCylinderParams &ecp=*(TEmptyCylinderParams *)pAction->GetParams();
		pAction->SetProgressTarget( ecp.cylZInclusive+1-ecp.cylA );
		// - range of Cylinders not empty if beginning located before FirstCylinderWithEmptySector
		if (ecp.cylA<ecp.dos->GetFirstCylinderWithEmptySector())
			return pAction->TerminateWithError(ERROR_NOT_EMPTY);
		// - checking if range of Cylinders empty
		for( TCylinder cyl=ecp.cylA; cyl<=ecp.cylZInclusive; pAction->UpdateProgress(++cyl-ecp.cylA) ){
			if (pAction->Cancelled) return ERROR_CANCELLED;
			for( THead head=0; head<ecp.dos->formatBoot.nHeads; head++ ){
				const TStdWinError err=ecp.dos->IsStdTrackEmpty( cyl, head );
				if (err!=ERROR_EMPTY)
					return pAction->TerminateWithError(err);
			}
		}
		return pAction->TerminateWithSuccess();
	}
	TStdWinError CDos::AreStdCylindersEmpty(TCylinder cylA,TCylinder cylZInclusive) const{
		// checks if specified Cylinders are empty (i.e. none of their standard "official" Sectors is allocated, non-standard sectors ignored); returns Windows standard i/o error
		if (cylA<=cylZInclusive)
			if (const TStdWinError err=CBackgroundActionCancelable( // unlike this method, the Thread returns ERROR_SUCCESS if cyls empty!
					TEmptyCylinderParams::Thread,
					&TEmptyCylinderParams( this, cylA, cylZInclusive ),
					THREAD_PRIORITY_BELOW_NORMAL
				).Perform()
			)
				return err;
		return ERROR_EMPTY;
	}


	struct TFmtParams sealed{
		CDos *const dos;
		const CFormatDialog::TParameters &rParams;
		const PCHead head;
		const TSector nSectors;
		const PSectorId bufferId; // non-const to be able to dynamically generate "Zoned Bit Recording" Sectors in the future
		const PCWORD bufferLength;
		const bool showReport;

		TFmtParams(CDos *dos,const CFormatDialog::TParameters &params,PCHead head,TSector nSectors,PSectorId bufferId,PCWORD bufferLength,bool showReport)
			: dos(dos)
			, rParams(params) , head(head)
			, nSectors(nSectors) , bufferId(bufferId) , bufferLength(bufferLength)
			, showReport(showReport) {
		}
	};

	static UINT AFX_CDECL InitializeEmptyMedium_thread(PVOID pCancelableAction){
		// thread to initialize a fresh formatted Medium, using TFmtParams
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const TFmtParams &fp=*(TFmtParams *)pAction->GetParams();
		fp.dos->formatBoot=fp.rParams.format;
		fp.dos->formatBoot.nCylinders++; // because Cylinders numbered from zero
		fp.dos->InitializeEmptyMedium(&fp.rParams,*pAction); // DOS-specific initialization of newly formatted Medium
		return pAction->TerminateWithSuccess();
	}

	static UINT AFX_CDECL RegisterAddedCylinders_thread(PVOID pCancelableAction){
		// thread to optionally register newly formatted Cylinders into disk structure (e.g. Boot Sector, FAT, etc.)
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const CFormatDialog &d=*(CFormatDialog *)pAction->GetParams();
		pAction->SetProgressTarget(200);
		if (d.updateBoot){
			// requested to update Format in Boot Sector
			d.dos->formatBoot.nCylinders=std::max( (int)d.dos->formatBoot.nCylinders, d.params.format.nCylinders+1 ); // "+1" = because Cylinders numbered from zero
			d.dos->FlushToBootSector();
		}
		pAction->IncrementProgress(100);
		if (d.addTracksToFat)
			// requested to include newly formatted Tracks into FAT
			if (!d.dos->AddStdCylindersToFatAsEmpty( d.params.cylinder0, d.params.format.nCylinders, pAction->CreateSubactionProgress(100) ))
				Utils::Information( FAT_SECTOR_UNMODIFIABLE, ::GetLastError() );
			else if (d.dos->image->RequiresFormattedTracksVerification()){ // mark bad Sectors in the FAT?
				TSectorId ids[(TSector)-1];
				TPhysicalAddress chs;
				for( chs.cylinder=d.params.cylinder0; chs.cylinder<=d.params.format.nCylinders; chs.cylinder++ )
					for( chs.head=0; chs.head<d.dos->formatBoot.nHeads; chs.head++ )
						for( TSector n=d.dos->GetListOfStdSectors(chs.cylinder,chs.head,ids); n>0; ){
							chs.sectorId=ids[--n];
							d.dos->ModifyStdSectorStatus(
								chs,
								d.dos->image->GetHealthySectorData(chs) ? TSectorStatus::EMPTY : TSectorStatus::BAD
							);
						}

			}
		pAction->IncrementProgress(100);
		return pAction->TerminateWithSuccess();
	}

	TStdWinError CDos::ShowDialogAndFormatStdCylinders(CFormatDialog &rd){
		// formats Cylinders using parameters obtained from the confirmed FormatDialog (CDos-derivate and FormatDialog guarantee that all parameters are valid); returns Windows standard i/o error
		if (image->ReportWriteProtection()) return ERROR_WRITE_PROTECT;
		do{
			LOG_DIALOG_DISPLAY(_T("CFormatDialog"));
			if (LOG_DIALOG_RESULT(rd.DoModal())!=IDOK){
				::SetLastError(ERROR_CANCELLED);
				return ERROR_CANCELLED;
			}
			CBackgroundMultiActionCancelable bmac(THREAD_PRIORITY_TIME_CRITICAL); // transparently doing all changes to the disk in waterfall threads
			// . checking if formatting can proceed
			const TEmptyCylinderParams ecp( this, rd.params.cylinder0, rd.params.format.nCylinders );
			if (rd.params.cylinder0){
				// request to NOT format from the beginning of disk - all targeted Tracks must be empty
				ecp.AddAction(bmac);
			}else{
				// request to format from the beginning of disk - warning that all data will be destroyed
				if (!Utils::QuestionYesNo(_T("About to format the whole image and destroy all data.\n\nContinue?!"),MB_DEFBUTTON2))
					return ERROR_CANCELLED;
				if (pFileManager->GetSafeHwnd())
					pFileManager->GetListCtrl().DeleteAllItems();
				if (const TStdWinError err=image->Reset())
					return err;
				if (!image->EditSettings(true))
					return ERROR_CANCELLED;
				if (const TStdWinError err=image->SetMediumTypeAndGeometry( &rd.params.format, sideMap, properties->firstSectorNumber ))
					return err;
			}
			// . carrying out the formatting
			TSectorId bufferId[(TSector)-1];	WORD bufferLength[(TSector)-1];
			for( TSector n=-1; n--; bufferLength[n]=rd.params.format.sectorLength );
			const TFmtParams fp(
				this, rd.params,
				nullptr, // all Heads
				0, bufferId, bufferLength, // 0 = standard Sectors
				rd.showReportOnFormatting
			);
			bmac.AddAction( FormatTracks_thread, &fp, _T("Formatting cylinders") );
			// . adding formatted Cylinders to Boot and FAT
			const CImage::TSaveThreadParams spDevice( image, nullptr );
			if (!rd.params.cylinder0){
				// formatted from beginning of disk - updating internal information on Format
				bmac.AddAction( InitializeEmptyMedium_thread, &fp, _T("Initializing disk") );
				if (image->properties->IsRealDevice()) // if formatted a real device ...
					bmac.AddAction( CImage::SaveAllModifiedTracks_thread, &spDevice, _T("Saving initialization") ); // ... immediately saving all Modified Sectors (Boot, FAT, Dir,...)
			}else{
				// formatted only selected Cylinders
				if (rd.updateBoot|rd.addTracksToFat)
					bmac.AddAction( RegisterAddedCylinders_thread, &rd, _T("Updating disk") );
			}
			// . carrying out the batch
			if (const TStdWinError err=bmac.Perform(true))
				if (bmac.GetCurrentFunction()==TEmptyCylinderParams::Thread){ // error in checking if disk region empty?
					Utils::Information( DOS_ERR_CANNOT_FORMAT, DOS_ERR_CYLINDERS_NOT_EMPTY, DOS_MSG_CYLINDERS_UNCHANGED );
					continue; // show this Dialog once again so the user can amend
				}else{
					::SetLastError(err);
					return LOG_ERROR(err);
				}
			// . formatted successfully
			break;
		}while (true);
		image->UpdateAllViews(nullptr); // although updated already in FormatTracks, here calling too as FormatBoot might have changed since then
		return ERROR_SUCCESS;
	}

	UINT AFX_CDECL CDos::FormatTracks_thread(PVOID pCancelableAction){
		// thread to format selected Tracks
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const TFmtParams &fp=*(TFmtParams *)pAction->GetParams();
		const Utils::CVarTempReset<TSector> nSectors0( fp.dos->formatBoot.nSectors, fp.rParams.format.nSectors );
		const Utils::CVarTempReset<TFormat::TLengthCode> lengthCode0( fp.dos->formatBoot.sectorLengthCode, fp.rParams.format.sectorLengthCode );
		const TStdWinError err=FormatTracksEx_thread( pCancelableAction );
		if (err!=ERROR_SUCCESS)
			::SetLastError(err);
		return err;
	}

	UINT AFX_CDECL CDos::FormatTracksEx_thread(PVOID pCancelableAction){
		// thread to format selected Tracks
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const TFmtParams &fp=*(TFmtParams *)pAction->GetParams();
		struct TStatistics sealed{
			const TTrack nTracks;
			DWORD nSectorsInTotal;
			DWORD nSectorsBad;
			DWORD availableGoodCapacityInBytes;

			TStatistics(TTrack nTracks)
				: nTracks(nTracks)
				, nSectorsInTotal(0) , nSectorsBad(0) , availableGoodCapacityInBytes(0) {
			}
		} statistics( (fp.rParams.format.nCylinders+1-fp.rParams.cylinder0)*(fp.head!=nullptr?1:fp.rParams.format.nHeads) );
		pAction->SetProgressTarget( statistics.nTracks );
		TCylinder cyl=fp.rParams.cylinder0;
		THead head= fp.head!=nullptr ? *fp.head : 0; // one particular or all Heads?
		for( TTrack t=0; t<statistics.nTracks; pAction->UpdateProgress(++t) ){
			if (pAction->Cancelled) return ERROR_CANCELLED;
			// . formatting Track
			TFdcStatus bufferFdcStatus[(TSector)-1];
			TSector nSectors;
			TStdWinError err;
			if (fp.nSectors)
				// custom Sectors (caller generated their IDs into Buffer)
				err=fp.dos->image->FormatTrack(
					cyl, head,
					fp.rParams.format.codecType,
					nSectors=fp.nSectors, fp.bufferId, fp.bufferLength, bufferFdcStatus,
					fp.rParams.gap3, fp.dos->properties->sectorFillerByte,
					pAction->Cancelled
				);
			else{
				// standard Sectors (their IDs generated into Buffer now)
				// : generating IDs
				nSectors=fp.dos->GetListOfStdSectors( cyl, head, fp.bufferId );
				// : permutating them by specified Parameters (e.g. skew, etc.)
				TSectorId stdSectors[(TSector)-1];
				bool permutated[(TSector)-1];
				::ZeroMemory(permutated,sizeof(permutated));
				BYTE i=(cyl*fp.rParams.format.nHeads+head)*fp.rParams.skew%nSectors;
				for( TSector s=0; s<nSectors; s++ ){
					while (permutated[i])
						i=(i+1)%nSectors;
					stdSectors[i]=fp.bufferId[s], permutated[i]=true;
					i=(i+fp.rParams.interleaving)%nSectors;
				}
				// : formatting
				err=fp.dos->image->FormatTrack(
					cyl, head,
					fp.rParams.format.codecType,
					nSectors, stdSectors, fp.bufferLength, bufferFdcStatus,
					fp.rParams.gap3, fp.dos->properties->sectorFillerByte,
					pAction->Cancelled
				);
			}
			if (err!=ERROR_SUCCESS)
				return pAction->TerminateWithError(err);
			statistics.nSectorsInTotal+=nSectors;
			// . writing the Track (if supported)
			switch ( err=fp.dos->image->SaveTrack( cyl, head, pAction->Cancelled ) ){
				case ERROR_SUCCESS:
				case ERROR_NOT_SUPPORTED: // saved later by the caller
					break;
				default:
					return pAction->TerminateWithError(err);
			}
			// . verifying Tracks by trying to read their formatted Sectors
			if (fp.dos->image->RequiresFormattedTracksVerification()){
				// : buffering Sectors from the same Track by the underlying Image, making them ready for IMMEDIATE usage
				fp.dos->image->BufferTrackData( cyl, head, Revolution::CURRENT, fp.bufferId, Utils::CByteIdentity(), nSectors );
				// : verifying formatted Sectors
				WORD w;
				for( PCSectorId pId=fp.bufferId; nSectors--; )
					if (const PCSectorData tmp=fp.dos->image->GetHealthySectorData(cyl,head,pId++,&w))
						statistics.availableGoodCapacityInBytes+=w;
					else
						statistics.nSectorsBad++;
			}else
				for( PCWORD pLength=fp.bufferLength; nSectors--; statistics.availableGoodCapacityInBytes+=*pLength++ );
			// . next Track
			if (fp.head!=nullptr) // one particular Head
				cyl++;
			else // all Heads
				if (++head==fp.rParams.format.nHeads){
					cyl++;
					head=0;
				}
		}
		if (fp.showReport){
			TCHAR buf[512];
			_stprintf( buf, _T("LOW-LEVEL FORMATTING DONE\n\n\n- formatted %d track(s)\n- with totally %d sectors\n- of which %d are bad (%.2f %%)\n- resulting in %s of raw good capacity.\n\nSee the Track Map tab for more information."), statistics.nTracks, statistics.nSectorsInTotal, statistics.nSectorsBad, (float)statistics.nSectorsBad*100/statistics.nSectorsInTotal, Utils::BytesToHigherUnits(statistics.availableGoodCapacityInBytes) );
			Utils::Information(buf);
		}
		return ERROR_SUCCESS;
	}

	bool CDos::AddStdCylindersToFatAsEmpty(TCylinder cylA,TCylinder cylZInclusive,CActionProgress &ap) const{
		// records standard "official" Sectors in given Cylinder range as Empty into FAT
		bool result=true; // assumption (all Tracks successfully added to FAT)
		TSectorId ids[(TSector)-1];
		TPhysicalAddress chs;
		ap.SetProgressTarget( cylZInclusive+1-cylA );
		for( chs.cylinder=cylA; chs.cylinder<=cylZInclusive; ap.UpdateProgress(++chs.cylinder-cylA) )
			for( chs.head=0; chs.head<formatBoot.nHeads; chs.head++ )
				for( TSector n=GetListOfStdSectors(chs.cylinder,chs.head,ids); n>0; ){
					chs.sectorId=ids[--n];
					result&=ModifyStdSectorStatus( chs, TSectorStatus::EMPTY );
				}
		return result;
	}

	bool CDos::RemoveStdCylindersFromFat(TCylinder cylA,TCylinder cylZInclusive,CActionProgress &ap) const{
		// records standard "official" Sectors in given Cylinders as Unavailable
		bool result=true; // assumption (all Tracks successfully removed from FAT)
		TSectorId ids[(TSector)-1];
		TPhysicalAddress chs;
		ap.SetProgressTarget( cylZInclusive+1-cylA );
		for( chs.cylinder=cylA; chs.cylinder<=cylZInclusive; ap.UpdateProgress(++chs.cylinder-cylA) )
			for( chs.head=0; chs.head<formatBoot.nHeads; chs.head++ )
				for( TSector n=GetListOfStdSectors(chs.cylinder,chs.head,ids); n>0; ){
					chs.sectorId=ids[--n];
					result&=ModifyStdSectorStatus( chs, unformatFatStatus );
				}
		return result;
	}

	





	#define DIR_ROOT_SECTOR_NOT_FOUND	_T("Root directory sector not found.\n\nProceeding with remaining sectors.")

	struct TFillEmptySpaceParams sealed{
		const PDos dos;
		const CFillEmptySpaceDialog &rd;

		TFillEmptySpaceParams(PDos dos,const CFillEmptySpaceDialog &rd)
			: dos(dos) , rd(rd) {
		}
	};
	UINT AFX_CDECL CDos::__fillEmptySectors_thread__(PVOID _pCancelableAction){
		// thread to flood empty Sectors on the disk
		LOG_ACTION(_T("Fill empty space (sectors)"));
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		const TFillEmptySpaceParams fesp=*(TFillEmptySpaceParams *)pAction->GetParams();
		const PImage image=fesp.dos->image;
		pAction->SetProgressTarget( image->GetCylinderCount() );
		for( TCylinder cyl=0,const nCylinders=image->GetCylinderCount(); cyl<nCylinders; cyl++ )
			for( THead head=fesp.dos->formatBoot.nHeads; head--; ){
				if (pAction->Cancelled) return ERROR_CANCELLED;
				// : determining standard Empty Sectors
				TSectorId bufferId[(TSector)-1],*pId=bufferId,*pEmptyId=bufferId;
				TSector nSectors=fesp.dos->GetListOfStdSectors(cyl,head,bufferId);
				TSectorStatus statuses[(TSector)-1],*ps=statuses;
				for( fesp.dos->GetSectorStatuses(cyl,head,nSectors,bufferId,statuses); nSectors-->0; pId++ )
					if (*ps++==TSectorStatus::EMPTY)
						*pEmptyId++=*pId;
				if (pEmptyId==bufferId) // Track contains no Empty Sectors
					continue;
				// : buffering Sectors from the same Track by the underlying Image, making them ready for IMMEDIATE usage
				fesp.dos->image->BufferTrackData( cyl, head, Revolution::ANY_GOOD, bufferId, Utils::CByteIdentity(), pEmptyId-bufferId );
				// : filling all Empty Sectors
				TPhysicalAddress chs={ cyl, head };
				for( WORD w; pEmptyId>bufferId; ){
					chs.sectorId=*--pEmptyId;
					if (const PSectorData data=image->GetHealthySectorData(chs,&w)){
						::memset( data, fesp.rd.sectorFillerByte, w );
						image->MarkSectorAsDirty(chs);
					}//else
						//TODO: warning on Sector unreadability
				}
				pAction->UpdateProgress(cyl);
			}
		return pAction->TerminateWithSuccess();
	}
	UINT AFX_CDECL CDos::__fillEmptyLastSectors_thread__(PVOID _pCancelableAction){
		// thread to flood empty space in each File's last Sector
		//
		// WARNING: It's assumed that "dot" and "dotdot"-like DirectoryEntries are disabled to prevent from unfinite looping!
		//
		LOG_ACTION(_T("Fill empty space (LAST sectors)"));
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		const TFillEmptySpaceParams fesp=*(TFillEmptySpaceParams *)pAction->GetParams();
		const PImage image=fesp.dos->image;
		pAction->SetProgressTarget( 1+image->GetCylinderCount() ); // "1+" = to not preliminary terminating the action
		const WORD nDataBytesInSector=fesp.dos->formatBoot.sectorLength-fesp.dos->properties->dataBeginOffsetInSector-fesp.dos->properties->dataEndOffsetInSector;
		// . adding current Directory into DiscoverdDirectories, and backing-up current Directory (may be changed during processing)
		CFileManagerView::CFileList discoveredDirs;
		discoveredDirs.AddHead(fesp.dos->currentDir);
		// . filling empty space in last Sectors
		while (discoveredDirs.GetCount()){
			const PFile dir=discoveredDirs.RemoveHead();
			if (const auto pdt=fesp.dos->BeginDirectoryTraversal(dir))
				while (const PCFile file=pdt->GetNextFileOrSubdir()){
					if (pAction->Cancelled) return ERROR_CANCELLED;
					switch (pdt->entryType){
						case TDirectoryTraversal::FILE:{
							// File
							const CFatPath fatPath(fesp.dos,file);
							CFatPath::PCItem item; DWORD n;
							if (const LPCTSTR err=fatPath.GetItems(item,n))
								fesp.dos->ShowFileProcessingError(file,err);
							else
								for( DWORD fileSize=fesp.dos->GetFileOccupiedSize(file); n--; item++ )
									if (nDataBytesInSector<fileSize)
										fileSize-=nDataBytesInSector;
									else{
										pAction->UpdateProgress(item->chs.cylinder);
										if (const PSectorData sectorData=fesp.dos->image->GetHealthySectorData(item->chs)){
											::memset( sectorData+fesp.dos->properties->dataBeginOffsetInSector+fileSize, fesp.rd.sectorFillerByte, nDataBytesInSector-fileSize );
											image->MarkSectorAsDirty(item->chs);
										}//else
											//TODO: Warning (Bad Sector)
										break;
									}
							break;
						}
						case TDirectoryTraversal::SUBDIR:
							// Subdirectory (WARNING: It's assumed that "dot" and "dotdot"-like DirectoryEntries are disabled to prevent from unfinite looping!)
							if (fesp.rd.fillSubdirectoryFileEndings)
								discoveredDirs.AddTail(pdt->entry);
							break;
						case TDirectoryTraversal::WARNING:
							// warning
							//TODO: Utils::Warning(0,pdt->error);
							break;
					}
				}
			//else
				//TODO: warning
		}
		return pAction->TerminateWithSuccess();
	}
	UINT AFX_CDECL CDos::__fillEmptyDirEntries_thread__(PVOID _pCancelableAction){
		// thread to flood Empty Directory entries
		//
		// WARNING: It's assumed that "dot" and "dotdot"-like DirectoryEntries are disabled to prevent from unfinite looping!
		//
		LOG_ACTION(_T("Fill empty space (dir entries)"));
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		const TFillEmptySpaceParams fesp=*(TFillEmptySpaceParams *)pAction->GetParams();
		const PImage image=fesp.dos->image;
		pAction->SetProgressTarget( image->GetCylinderCount() ); // "1+" = to not preliminary terminating the action
		// . adding current Directory into DiscoverdDirectories, and backing-up current Directory (may be changed during processing)
		CFileManagerView::CFileList discoveredDirs;
		discoveredDirs.AddHead(fesp.dos->currentDir);
		// . filling Empty Directory entries
		while (discoveredDirs.GetCount()){
			const PFile dir=discoveredDirs.RemoveHead();
			if (const auto pdt=fesp.dos->BeginDirectoryTraversal(dir))
				while (pdt->AdvanceToNextEntry()){
					if (pAction->Cancelled) return ERROR_CANCELLED;
					switch (pdt->entryType){
						case TDirectoryTraversal::EMPTY:
							// Empty entry
							pAction->UpdateProgress(pdt->chs.cylinder);
							pdt->ResetCurrentEntry(fesp.rd.directoryFillerByte);
							image->MarkSectorAsDirty(pdt->chs);
							break;
						case TDirectoryTraversal::SUBDIR:
							// Subdirectory (WARNING: It's assumed that "dot" and "dotdot"-like DirectoryEntries are disabled to prevent from unfinite looping!)
							if (fesp.rd.fillEmptySubdirectoryEntries)
								discoveredDirs.AddTail(pdt->entry);
							break;
						case TDirectoryTraversal::WARNING:
							// warning - Directory Sector not found
							//TODO: Utils::Warning(0,DIR_ROOT_SECTOR_NOT_FOUND);
							break;
					}
				}
			//else
				//TODO: warning
		}
		return pAction->TerminateWithSuccess();
	}
	bool CDos::__fillEmptySpace__(CFillEmptySpaceDialog &rd){
		// True <=> filling of empty space on disk with specified FillerBytes was successfull, otherwise False
		if (image->ReportWriteProtection()) return false;
		if (rd.DoModal()!=IDOK) return false;
		// - filling
		CBackgroundMultiActionCancelable bmac(THREAD_PRIORITY_BELOW_NORMAL);
			const TFillEmptySpaceParams params(this,rd);
			if (rd.fillEmptySectors)
				bmac.AddAction( __fillEmptySectors_thread__, &params, _T("Filling empty sectors") );
			TCHAR labelLastSectors[128];
			if (rd.fillFileEndings){
				::wsprintf( labelLastSectors, _T("Filling last sectors of files in current directory%s"), rd.fillEmptySubdirectoryEntries?_T(" and its subdirectories"):_T("") );
				bmac.AddAction( __fillEmptyLastSectors_thread__, &params, labelLastSectors );
			}
			TCHAR labelDirEntries[128];
			if (rd.fillEmptyDirectoryEntries){
				::wsprintf( labelDirEntries, _T("Filling empty entries in current directory%s"), rd.fillEmptySubdirectoryEntries?_T(" and its subdirectories"):_T("") );
				bmac.AddAction( __fillEmptyDirEntries_thread__, &params, labelDirEntries );
			}
		bmac.Perform();
		// - updating Views
		image->UpdateAllViews(nullptr);
		return true;
	}

	bool CDos::VerifyVolume(CVerifyVolumeDialog &rd){
		// True <=> volume verification was performed (even unsuccessfull), otherwise False (e.g. user cancelled)
		// - displaying the Dialog
		if (image->ReportWriteProtection()) return false;
		if (rd.DoModal()!=IDOK) return false;
		// - opening the HTML Report for writing
		const CString tmpFileName=Utils::GenerateTemporaryFileName()+_T(".html");
		if (!rd.params.fReport.Open( tmpFileName, CFile::modeCreate|CFile::modeWrite ))
			return false;
		Utils::WriteToFile( rd.params.fReport, Utils::GetCommonHtmlHeadStyleBody() );
			rd.params.fReport.OpenSection( _T("Overview"), false );
			Utils::WriteToFileFormatted( rd.params.fReport, _T("<table><tr><td>") _T(APP_ABBREVIATION) _T(" version</td><td><b>") _T(APP_VERSION) _T("</b></td></tr><tr><td>Location:</td><td><b>%s</b></td></tr><tr><td>System:</td><td><b>%s</b></td></tr></table>"), image->GetPathName().GetLength()?image->GetPathName():_T("N/A"), properties->name );
		// - verification
		CBackgroundMultiActionCancelable &bmac=rd.params.action;
			if (rd.params.verifyBootSector)
				bmac.AddAction( rd.params.verificationFunctions.fnBootSector, &rd.params, _T("Verifying boot sector") );
			if (rd.params.verifyFat){
				bmac.AddAction( rd.params.verificationFunctions.fnFatFullyReadable, &rd.params, _T("Checking if FAT readable") );
				bmac.AddAction( rd.params.verificationFunctions.fnFatFilePathsOk, &rd.params, _T("Verifying file records in FAT") );
				// continued after verification of the File system
			}
			if (rd.params.verifyFilesystem)
				bmac.AddAction( rd.params.verificationFunctions.fnFilesystem, &rd.params, _T("Verifying filesystem") );
			if (rd.params.verifyFat){
				// continued verification of FAT
				bmac.AddAction( rd.params.verificationFunctions.fnFatCrossedFiles, &rd.params, _T("Searching for cross-linked files") );
				bmac.AddAction( rd.params.verificationFunctions.fnFatLostAllocUnits, &rd.params, _T("Searching for lost allocation units") );
			}
			if (rd.params.verifyVolumeSurface)
				bmac.AddAction( rd.params.verificationFunctions.fnVolumeSurface, &rd.params, _T("Scanning volume surface for bad sectors") );
		bmac.Perform();
		// - closing the HTML Report
		rd.params.fReport.Close();
		// - displaying the HTML Report
		app.GetMainWindow()->OpenWebPage( _T("Verification results"), tmpFileName );
		// - updating Views
		image->UpdateAllViews(nullptr);
		return true;
	}





	TSectorStatus CDos::GetSectorStatus(RCPhysicalAddress chs) const{
		// determines and returns the Status of the Sector on the specified PhysicalAddress
		TSectorStatus result;
		GetSectorStatuses( chs.cylinder, chs.head, 1, &chs.sectorId, &result );
		return result;
	}

	LPCTSTR CDos::GetSectorStatusText(RCPhysicalAddress chs) const{
		// determines and returns the Status of the Sector on the specified PhysicalAddress
		switch (GetSectorStatus(chs)){
			case TSectorStatus::SYSTEM:	return _T("System");
			case TSectorStatus::UNAVAILABLE: return _T("Unavailable");
			case TSectorStatus::SKIPPED:	return _T("Skipped");
			case TSectorStatus::BAD:		return _T("Bad");
			case TSectorStatus::OCCUPIED:	return _T("Occupied");
			case TSectorStatus::RESERVED:	return _T("Reserved");
			case TSectorStatus::EMPTY:	return _T("Empty");
			default:							return _T("Unknown");
		}
	}

	bool CDos::IsSectorStatusBadOrEmpty(RCPhysicalAddress chs) const{
		const TSectorStatus fatStatus=GetSectorStatus(chs);
		return fatStatus==TSectorStatus::BAD || fatStatus==TSectorStatus::EMPTY;
	}

	DWORD CDos::GetFreeSpaceInBytes(TStdWinError &rError) const{
		// computes and returns the empty space on Image in Bytes
		DWORD result=0; rError=ERROR_SUCCESS; // assumption (no empty space, no error)
		const WORD nDataBytesInSector=formatBoot.sectorLength-properties->dataBeginOffsetInSector-properties->dataEndOffsetInSector;
		for( TCylinder cyl=GetFirstCylinderWithEmptySector(); cyl<formatBoot.nCylinders; cyl++ )
			for( THead head=0; head<formatBoot.nHeads; head++ ){
				TSectorId bufferId[(TSector)-1];
				TSector n=GetListOfStdSectors(cyl,head,bufferId);
				TSectorStatus statuses[(TSector)-1],*ps=statuses;
				if (!GetSectorStatuses(cyl,head,n,bufferId,statuses) && rError==ERROR_SUCCESS) // first error of FAT
					rError=ERROR_SECTOR_NOT_FOUND;
				while (n--)
					if (*ps++==TSectorStatus::EMPTY)
						result+=nDataBytesInSector;
			}
		return result;
	}

	TCylinder CDos::GetFirstCylinderWithEmptySector() const{
		// determines and returns the first Cylinder which contains at least one Empty Sector
		return 0; // caller should start looking for Empty Sectors from the beginning of disk
	}

	CDos::CPathString CDos::GetFilePresentationNameAndExt(PCFile file) const{
		// returns File name concatenated with File extension for presentation of the File to the user
		CPathString name,ext;
		GetFileNameOrExt( file, &name, &ext );
		return name.AppendDotExtensionIfAny( ext );
	}

	int CDos::CompareFileNames(RCPathString filename1,RCPathString filename2) const{
		// returns an integer indicating if FileName1 preceedes, equals to, or succeeds FileName2
		return filename1.Compare( filename2, fnCompareNames );
	}

	bool CDos::EqualFileNames(RCPathString filename1,RCPathString filename2) const{
		// True <=> FileName1 and FileName2 are equal, otherwise False
		return !CompareFileNames( filename1, filename2 );
	}

	bool CDos::HasFileNameAndExt(PCFile file,RCPathString fileName,RCPathString fileExt) const{
		// True <=> given File has the name and extension as specified, otherwise False
		CPathString name,ext;
		return	GetFileNameOrExt( file, &name, &ext )
				? EqualFileNames(name,fileName) && EqualFileNames(ext,fileExt) // name relevant
				: false; // name irrelevant
	}

	DWORD CDos::GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData) const{
		// a wrapper around the virtual GetFileSize with DefaultOption
		return GetFileSize(file,pnBytesReservedBeforeData,pnBytesReservedAfterData,getFileSizeDefaultOption);
	}

	DWORD CDos::GetFileSize(PCFile file) const{
		// a wrapper around the virtual GetFileSize with DefaultOption
		return GetFileSize(file,nullptr,nullptr);
	}

	DWORD CDos::GetFileOfficialSize(PCFile file) const{
		// returns the number of Bytes in data portion of specified File (e.g. TR-DOS yet stores some extra information "after" official data - these are NOT counted in here!)
		return GetFileSize(file,nullptr,nullptr,TGetFileSizeOptions::OfficialDataLength);
	}

	DWORD CDos::GetFileOccupiedSize(PCFile file) const{
		// returns the number of Bytes that the whole File contains (e.g. TR-DOS yet stores some extra information "after" official data - EVEN THESE are counted in here!)
		BYTE nBytesReservedBeforeData,nBytesReservedAfterData;
		const DWORD nDataBytes=GetFileSize(file,&nBytesReservedBeforeData,&nBytesReservedAfterData);
		return nBytesReservedBeforeData+nDataBytes+nBytesReservedAfterData;
	}

	DWORD CDos::GetFileSizeOnDisk(PCFile file) const{
		// determines and returns how many Bytes the specified File actually occupies on disk
		return GetFileSize(file,nullptr,nullptr,TGetFileSizeOptions::SizeOnDisk);
	}

	void CDos::GetFileTimeStamps(PCFile file,LPFILETIME pCreated,LPFILETIME pLastRead,LPFILETIME pLastWritten) const{
		// given specific File, populates the Created, LastRead, and LastWritten outputs
		if (pCreated) *pCreated=Utils::CRideTime::None;		// Files don't have time stamps by default
		if (pLastRead) *pLastRead=Utils::CRideTime::None;	// Files don't have time stamps by default
		if (pLastWritten) *pLastWritten=Utils::CRideTime::None;	// Files don't have time stamps by default
	}

	bool CDos::GetFileCreatedTimeStamp(PCFile file,FILETIME &rCreated) const{
		// True <=> File has a Created time stamp copied to the output field, otherwise False
		GetFileTimeStamps( file, &rCreated, nullptr, nullptr );
		return Utils::CRideTime::None!=rCreated;
	}

	bool CDos::GetFileLastReadTimeStamp(PCFile file,FILETIME &rLastRead) const{
		// True <=> File has a LastRead time stamp copied to the output field, otherwise False
		GetFileTimeStamps( file, nullptr, &rLastRead, nullptr );
		return Utils::CRideTime::None!=rLastRead;
	}

	bool CDos::GetFileLastWrittenTimeStamp(PCFile file,FILETIME &rLastWritten) const{
		// True <=> File has a LastRead time stamp copied to the output field, otherwise False
		GetFileTimeStamps( file, nullptr, nullptr, &rLastWritten );
		return Utils::CRideTime::None!=rLastWritten;
	}

	void CDos::SetFileTimeStamps(PFile file,const FILETIME *pCreated,const FILETIME *pLastRead,const FILETIME *pLastWritten){
		// translates the Created, LastRead, and LastWritten intputs into this DOS File time stamps
		//nop (Files don't have time stamps by default)
	}

	bool CDos::IsDirectory(PCFile file) const{
		// True <=> given File is actually a Directory, otherwise False
		return (GetAttributes(file)&FILE_ATTRIBUTE_DIRECTORY)!=0;
	}

	LPCTSTR CDos::__exportFileData__(PCFile file,CFile *fOut,DWORD nMaxDataBytesToExport) const{
		// exports data portion of specfied File (data portion size determined by GetFileSize); returns textual description of occured error
		LOG_FILE_ACTION(this,file,_T("export"));
		const CFatPath fatPath(this,file);
		CFatPath::PCItem item; DWORD n;
		if (const LPCTSTR err=fatPath.GetItems(item,n))
			return LOG_MESSAGE(err);
		else{
			BYTE nBytesReservedBeforeData;
			DWORD nDataBytesToExport=std::min( GetFileSize(file,&nBytesReservedBeforeData,nullptr), nMaxDataBytesToExport );
			div_t d=div((int)nBytesReservedBeforeData,(int)formatBoot.sectorLength-properties->dataBeginOffsetInSector-properties->dataEndOffsetInSector);
			item+=d.quot, n-=d.quot; // skipping Sectors from which not read thanks to the NumberOfBytesReservedBeforeData
			for( const Utils::CByteIdentity sectorIdAndPositionIdentity; n; ){
				// . determining which of nearest Sectors are on the same Track
				TSectorId bufferId[(TSector)-1];
				TSector nSectors=0;
				const TCylinder currCyl=item->chs.cylinder;	const THead currHead=item->chs.head;
				while (n && item->chs.cylinder==currCyl && item->chs.head==currHead)
					bufferId[nSectors++]=item->chs.sectorId, item++, n--;
				// . buffering Sectors from the same Track by the underlying Image, making them ready for IMMEDIATE usage
				image->BufferTrackData( currCyl, currHead, Revolution::ANY_GOOD, bufferId, sectorIdAndPositionIdentity, nSectors ); // make Sectors data ready for IMMEDIATE usage
				// . reading Sectors from the same Track in the underlying Image
				WORD w;
				for( TSector s=0; s<nSectors; s++ )
					if (const PCSectorData sectorData=image->GetHealthySectorData(currCyl,currHead,bufferId+s,&w)){
						w-=d.rem+properties->dataBeginOffsetInSector+properties->dataEndOffsetInSector;
						if (w<nDataBytesToExport){
							fOut->Write(sectorData+properties->dataBeginOffsetInSector+d.rem,w);
							nDataBytesToExport-=w, d.rem=0;
						}else{
							fOut->Write(sectorData+properties->dataBeginOffsetInSector+d.rem,nDataBytesToExport);
							return nullptr;
						}
					}else
						return LOG_MESSAGE(_T("Data sector not found or read with CRC error."));
			}
		}
		return nullptr;
	}

	CDos::CPathString CDos::GetFileExportNameAndExt(PCFile file,bool shellCompliant) const{
		// returns File name concatenated with File extension for export of the File to another Windows application (e.g. Explorer)
		CPathString fileName,fileExt;
		GetFileNameOrExt( file, &fileName, &fileExt );
		if (shellCompliant){
			// exporting to non-RIDE target (e.g. to the Explorer); excluding from the Buffer characters that are forbidden in FAT32 long file names
			fileExt.ExcludeFat32LongNameInvalidChars();
			if (fileName.ExcludeFat32LongNameInvalidChars().GetLengthW())
				// valid export name - taking it as the result
				return fileName.AppendDotExtensionIfAny(fileExt);
			else
				// invalid export name - generating an artifical one
				return CPathString().FormatCounter8().AppendDotExtensionIfAny(fileExt);
		}else
			// exporting to another RIDE instance; substituting non-alphanumeric characters with "URL-like" escape sequences
			return fileName.Escape(true).AppendDotExtensionIfAny( fileExt.Escape(true) ); // let the result contain mostly one dot '.' that delimits file name from its extension
	}

	DWORD CDos::ExportFile(PCFile file,CFile *fOut,DWORD nBytesToExportMax,LPCTSTR *pOutError) const{
		// exports data portion of specfied File (data portion size determined by GetFileDataSize); returns the export size of specified File
		if (fOut){
			const LPCTSTR errMsg=__exportFileData__(file,fOut,nBytesToExportMax);
			if (pOutError)
				*pOutError=errMsg;
		}
		return std::min( GetFileSize(file), nBytesToExportMax );
	}

	TStdWinError CDos::__importFileData__(CFile *f,PFile fDesc,RCPathString fileName,RCPathString fileExt,DWORD fileSize,bool skipBadSectors,PFile &rFile,CFatPath &rFatPath){
		// imports given File into the disk; returns Windows standard i/o error
		// - making sure that the File with given NameAndExtension doesn't exist in current Directory
		LOG_FILE_ACTION(this,fDesc,_T("import"));
		rFile=nullptr; // assumption (cannot import the File)
		if (const auto pdt=BeginDirectoryTraversal()){
			while (pdt->AdvanceToNextEntry())
				switch (pdt->entryType){
					case TDirectoryTraversal::EMPTY:
						// found Empty Directory entry
						if (!rFile) rFile=pdt->entry; // storing the first found Empty Directory entry
						//fallthrough
					case TDirectoryTraversal::CUSTOM:
						// ignoring any Custom entries (as only CDos-derivate understands their purpose)
						//fallthrough
					case TDirectoryTraversal::SUBDIR:
						// found a Subdirectory in current Directory
						break;
					case TDirectoryTraversal::FILE:
						// found a File in current Directory
						if (HasFileNameAndExt(pdt->entry,fileName,fileExt)){
							rFile=pdt->entry;
							return LOG_ERROR(ERROR_FILE_EXISTS);
						}else
							break;
					case TDirectoryTraversal::WARNING:
						// any Warning becomes a real error!
						if (pdt->warning!=ERROR_SECTOR_NOT_FOUND) // bad Sectors at the moment don't prevent from the import
							return LOG_ERROR(pdt->warning);
						break;
					#ifdef _DEBUG
					default:
						Utils::Information(_T("CDos::__importFile__ - unknown pdt->entryType"));
						ASSERT(FALSE);
					#endif
				}
		//}
		// - creating a record for specified File in current Directory
		//if (const auto pdt=BeginDirectoryTraversal()){
			PFile tmp=fDesc;
			TStdWinError err=ChangeFileNameAndExt( fDesc, fileName, fileExt, tmp );
			if (err==ERROR_SUCCESS)
				if (tmp!=fDesc) // a new record was created for the File (in ChangeFileNameAndExt)
					rFile=tmp;
				else{ // no record was created for the File in current Directory - recording the File into the entry found above
					if (!rFile) // no Empty entry found above ...
						rFile=pdt->AllocateNewEntry(); // ... allocating new one
					if (rFile){ // Empty entry found ...
						::memcpy( rFile, fDesc, pdt->entrySize ); // ... initializing it by supplied FileDescriptor (DOS-specific)
						MarkDirectorySectorAsDirty(rFile);
					}else // Empty entry not found and not allocated
						err=ERROR_CANNOT_MAKE;
				}
			if (err!=ERROR_SUCCESS)
				return LOG_ERROR(err);
		}else
			return LOG_ERROR(ERROR_PATH_NOT_FOUND);
		// - importing the File to disk and recording its FatPath
		if (const TStdWinError err=__importData__( f, fileSize, skipBadSectors, rFatPath )){
			DeleteFile(rFile); // removing the above added File record from current Directory
			return LOG_ERROR(err);
		}
		rFatPath.MarkAllSectorsModified(image);
		return ERROR_SUCCESS;
	}

	TStdWinError CDos::__importData__(CFile *f,DWORD fileSize,bool skipBadSectors,CFatPath &rFatPath) const{
		// imports given File to the disk; returns Windows standard i/o error
		LOG_ACTION(_T("data import"));
		// - checking if there's enough empty space on the disk
		TStdWinError err;
		if (fileSize>GetFreeSpaceInBytes(err))
			return LOG_ERROR(ERROR_DISK_FULL);
		// - determining the initial range of Heads in which to search for free Sectors within each Cylinder
		THead headZ; // last Head number
		switch (trackAccessScheme){
			case TTrackScheme::BY_CYLINDERS:
				// all Heads will be considered when searching for free Sectors in the "per-Cylinder" basis
				headZ=formatBoot.nHeads-1;
				break;
			case TTrackScheme::BY_SIDES:
				// only one Head at a time will be used when searching for free Sectors in the "per-Side" basis, then going to the next Head, etc.
				headZ=0;
				break;
			default:
				ASSERT(FALSE);
		}
		// - importing the File to disk and recording its FatPath
		const Utils::CByteIdentity sectorIdAndPositionIdentity;
		CFatPath::TItem item;
		//item.value=TSectorStatus::OCCUPIED; // commented out as all Sectors in the FatPath are Occupied except for the last Sector
		for( THead headA=0; headZ<formatBoot.nHeads; headA++,headZ++ )
			for( item.chs.cylinder=GetFirstCylinderWithEmptySector(); item.chs.cylinder<formatBoot.nCylinders; item.chs.cylinder++ )
				for( item.chs.head=headA; item.chs.head<=headZ; item.chs.head++ ){
					// . getting the list of standard Sectors
					TSectorId bufferId[(TSector)-1],*pId=bufferId;
					const TSector nSectors=GetListOfStdSectors( item.chs.cylinder, item.chs.head, bufferId );
					// . filtering out only Empty standard Sectors
					TSectorStatus statuses[(TSector)-1],*ps=statuses;
					GetSectorStatuses( item.chs.cylinder, item.chs.head, nSectors, bufferId, statuses );
					TSector nEmptySectors=0;
					for( TSector s=0; s<nSectors; s++ )
						if (*ps++==TSectorStatus::EMPTY)
							bufferId[nEmptySectors++]=bufferId[s];
					if (!nEmptySectors)
						continue;
					// . buffering Sectors from the same Track by the underlying Image, making them ready for IMMEDIATE usage
					image->BufferTrackData( item.chs.cylinder, item.chs.head, Revolution::ANY_GOOD, bufferId, sectorIdAndPositionIdentity, nEmptySectors );
					// . importing the File to Empty Sectors on the current Track
					for( WORD w; nEmptySectors--; ){
						item.chs.sectorId=*pId++;
						if (const PSectorData sectorData=image->GetHealthySectorData(item.chs,&w)){
							w-=properties->dataBeginOffsetInSector+properties->dataEndOffsetInSector;
							if (w<fileSize){
								f->Read(sectorData+properties->dataBeginOffsetInSector,w);
								fileSize-=w;
								//image->MarkSectorAsDirty(...); // commented out as carried out below if whole import successfull
								//item.value=TSectorStatus::OCCUPIED; // commented out as set already above
								rFatPath.AddItem(&item);
							}else if (!fileSize){ // zero-length File
								item.value=TSectorStatus::RESERVED;
								rFatPath.AddItem(&item);
								return ERROR_SUCCESS;
							}else{
								f->Read(sectorData+properties->dataBeginOffsetInSector,fileSize);
								if (w==fileSize) fileSize=0;
								//image->MarkSectorAsDirty(...); // commented out as carried out below if whole import successfull
								item.value=fileSize;
								rFatPath.AddItem(&item);
								return ERROR_SUCCESS;
							}
						}else{ // error when accessing discovered Empty Sector
							TStdWinError err;
							if (skipBadSectors){
								ModifyStdSectorStatus( item.chs, TSectorStatus::BAD ); // marking the Sector as Bad ...
								if (fileSize>GetFreeSpaceInBytes(err))
									err=ERROR_DISK_FULL;
								else
									continue; // ... and proceeding with the next Sector
							}else
								err=::GetLastError();
							return LOG_ERROR(err);
						}
					}
				}
		return LOG_ERROR(ERROR_WRITE_FAULT);
	}

	TStdWinError CDos::GetFirstEmptyHealthySector(bool skipBadSectors,TPhysicalAddress &rOutChs) const{
		// outputs a well readable Sector that is reported Empty (the problem of finding an empty Sector is a approached by importing a single Byte to the disk); returns Windows standard i/o error
		BYTE buf;
		CFatPath emptySector(this,sizeof(buf));
		if (const TStdWinError err=__importData__( 
				&CMemFile(&buf,sizeof(buf)), sizeof(buf), skipBadSectors,
				emptySector
			)
		)
			return err;
		CFatPath::PCItem pItem; DWORD n;
		emptySector.GetItems(pItem,n);
		rOutChs=pItem->chs;
		return ERROR_SUCCESS;
	}

	#define ERROR_MSG_CANNOT_PROCESS	_T("Cannot process \"%s\"")

	void CDos::ShowFileProcessingError(PCFile file,LPCTSTR cause) const{
		// shows general error message on File being not processable due to occured Cause
		Utils::FatalError(
			Utils::SimpleFormat( ERROR_MSG_CANNOT_PROCESS, GetFilePresentationNameAndExt(file) ),
			cause
		);
	}

	CDos::CPathString CDos::GetFileName(PCFile file) const{
		// return File's Name alone (without extension)
		CPathString name;
		GetFileNameOrExt( file, &name, nullptr );
		return name;
	}

	CDos::CPathString CDos::GetFileExt(PCFile file) const{
		// return File's Extension (without prefixed dot '.')
		CPathString ext;
		GetFileNameOrExt( file, nullptr, &ext );
		return ext;
	}

	void CDos::ShowFileProcessingError(PCFile file,TStdWinError cause) const{
		// shows general error message on File being not processable due to occured Cause
		Utils::FatalError(
			Utils::SimpleFormat( ERROR_MSG_CANNOT_PROCESS, GetFilePresentationNameAndExt(file) ),
			cause
		);
	}

	CDos::PFile CDos::__findFile__(PCFile directory,RCPathString fileName,RCPathString fileExt,PCFile ignoreThisFile) const{
		// finds and returns a File with given NameAndExtension; returns Null if such File doesn't exist
		PFile result=nullptr; // assumption (File with given NameAndExtension not found)
		if (const auto pdt=BeginDirectoryTraversal(directory))
			while (pdt->AdvanceToNextEntry())
				if (pdt->entryType==TDirectoryTraversal::FILE || pdt->entryType==TDirectoryTraversal::SUBDIR)
					if (pdt->entry!=ignoreThisFile)
						if (HasFileNameAndExt(pdt->entry,fileName,fileExt)){
							result=pdt->entry;
							break;
						}
		return result;
	}

	CDos::PFile CDos::FindFileInCurrentDir(RCPathString fileName,RCPathString fileExt,PCFile ignoreThisFile) const{
		// finds and returns a File with given NameAndExtension; returns Null if such File doesn't exist
		return __findFile__( currentDir, fileName, fileExt, ignoreThisFile );
	}

	TStdWinError CDos::__shiftFileContent__(const CFatPath &rFatPath,char nBytesShift) const{
		// shifts the content of File (defined by its FatPath) by specified NumbersOfBytes to the "left" or "right"; returns Windows standard i/o error
		// - if wanted to shift by zero Bytes, we are successfully done
		if (!nBytesShift)
			return ERROR_SUCCESS;
		// - retrieving the FatPath Items
		CFatPath::PCItem pItem; DWORD nItems;
		if (rFatPath.GetItems(pItem,nItems))
			return ERROR_GEN_FAILURE;
		// - making sure all Sectors in the FatPath are readable
		WORD w;
		for( DWORD n=0; n<nItems; )
			if (!image->GetHealthySectorData(pItem[n++].chs,&w))
				return ::GetLastError();
			else if (w!=formatBoot.sectorLength)
				return Utils::ErrorByOs( ERROR_VOLMGR_DISK_SECTOR_SIZE_INVALID, ERROR_NOT_SUPPORTED );
		// - shifting
		const WORD nDataBytesInSector=w-properties->dataBeginOffsetInSector-properties->dataEndOffsetInSector;
		if (nBytesShift<0){
			// shifting to the "left"
			const WORD offset=-nBytesShift,delta=nDataBytesInSector-offset;
			do{
				const PSectorData sectorData=image->GetHealthySectorData(pItem->chs)+properties->dataBeginOffsetInSector;
				::memcpy( sectorData, sectorData+offset, delta );
				image->MarkSectorAsDirty(pItem++->chs);
				if (!--nItems) break;
				::memcpy( sectorData+delta, image->GetHealthySectorData(pItem->chs)+properties->dataBeginOffsetInSector, offset );
			}while (true);
		}else{
			// shifting to the "right"
			pItem+=nItems-1;
			const WORD offset=nBytesShift,delta=nDataBytesInSector-offset;
			do{
				const PSectorData sectorData=image->GetHealthySectorData(pItem->chs)+properties->dataBeginOffsetInSector;
				::memmove( sectorData+offset, sectorData, delta );
				image->MarkSectorAsDirty(pItem--->chs);
				if (!--nItems) break;
				::memcpy( sectorData, image->GetHealthySectorData(pItem->chs)+properties->dataBeginOffsetInSector+delta, offset );
			}while (true);
		}
		return ERROR_SUCCESS;
	}












	bool CDos::IsKnown() const{
		return	this!=nullptr && properties->IsKnown();
	}

	TStdWinError CDos::CreateUserInterface(HWND hTdi){
		// creates DOS-specific Tabs in TDI; returns Windows standard i/o error
		return image->CreateUserInterface(hTdi); // creating disk-specific Tabs in TDI
	}

	CDos::TCmdResult CDos::ProcessCommand(WORD cmd){
		// returns the Result of processing a DOS-related command
		switch (cmd){
			case ID_DOS_SHELLCOMPLIANTNAMES:
				// toggles the requirement to produce FAT32-compliant names for exported Files
				__writeProfileBool__( INI_SHELL_COMPLIANT_EXPORT_NAMES, generateShellCompliantExportNames=!generateShellCompliantExportNames );
				return TCmdResult::DONE;
			case ID_DOS_FILE_LENGTH_FROM_DIRENTRY:
				// export File size given by informatin in DirectoryEntry
				__writeProfileInt__( INI_GETFILESIZE_OPTION, getFileSizeDefaultOption=TGetFileSizeOptions::OfficialDataLength );
				return TCmdResult::DONE;
			case ID_DOS_FILE_LENGTH_OCCUPIED_SECTORS:
				// export File size given by number of Sectors in FatPath
				__writeProfileInt__( INI_GETFILESIZE_OPTION, getFileSizeDefaultOption=TGetFileSizeOptions::SizeOnDisk );
				return TCmdResult::DONE;
			case ID_DOS_PREVIEWASBINARY:
				// previewing File in hexa mode
				if (CHexaPreview::pSingleInstance)
					delete CHexaPreview::pSingleInstance;
				new CHexaPreview(*pFileManager);
				return TCmdResult::DONE;
		}
		return TCmdResult::REFUSED;
	}

	bool CDos::UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const{
		// True <=> given Command-specific user interface successfully updated, otherwise False
		switch (cmd){
			case ID_DOS_SHELLCOMPLIANTNAMES:
				// projects the requirement to produce FAT32-compliant names for exported Files into the UI
				pCmdUI->SetCheck(generateShellCompliantExportNames);
				return true;
			case ID_DOS_FILE_LENGTH_FROM_DIRENTRY:
				// projects the default GetFileSizeOption into the UI
				pCmdUI->SetRadio(getFileSizeDefaultOption==TGetFileSizeOptions::OfficialDataLength);
				return true;
			case ID_DOS_FILE_LENGTH_OCCUPIED_SECTORS:
				// projects the default GetFileSizeOption into the UI
				pCmdUI->SetRadio(getFileSizeDefaultOption==TGetFileSizeOptions::SizeOnDisk);
				return true;
		}
		return false;
	}

	bool CDos::CanBeShutDown(CFrameWnd* pFrame) const{
		// True <=> this DOS has no dependecies which would require it to remain active, otherwise False (has some dependecies which require the DOS to remain active)
		return true; // True = no dependecies require this DOS to remain active
	}













	bool CDos::TProperties::IsKnown() const{
		return this!=&CUnknownDos::Properties;
	}

	BYTE CDos::TProperties::GetValidGap3ForMedium(Medium::TType medium) const{
		// infers and returns the minimum Gap3 value applicable for all available StandardFormats that regard the specified Medium
		BYTE result=FDD_350_SECTOR_GAP3;
		CFormatDialog::PCStdFormat pStdFmt=stdFormats;
		for( BYTE n=nStdFormats; n-->0; pStdFmt++ )
			if (pStdFmt->params.format.mediumType & medium)
				result=std::min( result, pStdFmt->params.gap3 );
		return result;
	}








	CDos::TDirectoryTraversal::TDirectoryTraversal(PCFile directory,WORD entrySize)
		// ctor
		: directory(directory) , entrySize(entrySize)
		, entry(nullptr) , entryType(TDirectoryTraversal::UNKNOWN) {
	}

	CDos::PFile CDos::TDirectoryTraversal::AllocateNewEntry(){
		// allocates and returns new entry at the end of current Directory and returns; returns Null if new entry cannot be allocated (e.g. because disk is full)
		return nullptr; // Null = cannot allocate new Empty entry (aka. this Directory has fixed number of entries)
	}

	CDos::PFile CDos::TDirectoryTraversal::GetOrAllocateEmptyEntries(BYTE,PFile *){
		// finds or allocates specified Count of empty entries in current Directory; returns pointer to the first entry, or Null if not all entries could be found/allocated (e.g. because disk full)
		return nullptr;
	}

	CDos::PFile CDos::TDirectoryTraversal::GetNextFileOrSubdir(){
		// finds and returns the next File or Subdirectory in current Directory; returns Null if not found
		while (AdvanceToNextEntry())
			switch (entryType){
				case TDirectoryTraversal::SUBDIR:
				case TDirectoryTraversal::FILE:
					return entry;
				case TDirectoryTraversal::WARNING:
					return (PFile)-1; // "some" non-zero value to indicate that whole Directory has not yet been traversed
			}
		return nullptr;
	}

	std::unique_ptr<CDos::TDirectoryTraversal> CDos::BeginDirectoryTraversal() const{
		// initiates exploration of current Directory through a DOS-specific DirectoryTraversal
		return BeginDirectoryTraversal(currentDir);
	}

	DWORD CDos::GetDirectoryUid(PCFile dir) const{
		// determines and returns the unique identifier of the Directory specified
		if (dir==DOS_DIR_ROOT)
			return DOS_DIR_ROOT_ID;
		else{
			ASSERT(FALSE);
			return -1;
		}
	}

	void CDos::MarkDirectorySectorAsDirty(PCFile file) const{
		// marks Directory Sector that contains specified File as "dirty"
		if (const auto pdt=BeginDirectoryTraversal())
			while (pdt->AdvanceToNextEntry())
				if (pdt->entry==file){
					image->MarkSectorAsDirty(pdt->chs);
					break;
				}
	}

	DWORD CDos::GetCountOfItemsInCurrentDir(TStdWinError &rError) const{
		// counts and returns the number of all Files and Directories in current Directory
		rError=ERROR_SUCCESS; // assumption (Directory OK)
		DWORD result=0;
		if (const auto pdt=BeginDirectoryTraversal())
			while (pdt->GetNextFileOrSubdir())
				switch (pdt->entryType){
					case TDirectoryTraversal::SUBDIR:
					case TDirectoryTraversal::FILE:
						result++;
						break;
					case TDirectoryTraversal::WARNING:
						if (rError==ERROR_SUCCESS) // storing the first encountered error
							rError=pdt->warning;
						break;
				}
		return result;
	}









	CDos::CHexaValuePropGridEditor::CHexaValuePropGridEditor(PropGrid::PValue value,PropGrid::TSize valueSize)
		// ctor
		// - base
		: Utils::CRideDialog(IDR_DOS_PROPGRID_HEXAEDITOR)
		// - initialization
		, hexaEditor(nullptr) {
		hexaEditor.SetLabelColumnParams( 0 );
		hexaEditor.SetEditable( !CImage::GetActive()->IsWriteProtected() );
		if (IStream *const s=Yahel::Stream::FromBuffer( ::memcpy(newValueBuffer,value,valueSize), valueSize )){
			hexaEditor.Reset( s, nullptr, valueSize );
			s->Release();
		}
	}

	void CDos::CHexaValuePropGridEditor::PreInitDialog(){
		// dialog initialization
		// - base
		__super::PreInitDialog();
		// - creating and showing the HexaEditor at the position of the placeholder (see Dialog's resource)
		hexaEditor.Create( nullptr, nullptr, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, MapDlgItemClientRect(ID_FILE), this, 0 );
	}

	void WINAPI CDos::CHexaValuePropGridEditor::DrawValue(PropGrid::PCustomParam,PropGrid::PCValue value,PropGrid::TSize valueSize,PDRAWITEMSTRUCT pdis){
		TCHAR buf[1024],*p=buf;
		for( PCBYTE v=(PCBYTE)value,const vMax=(PCBYTE)value+std::min<PropGrid::TSize>(valueSize,ARRAYSIZE(buf)/3); v<vMax; p+=::wsprintf(p,_T(" %02X"),*v++) );
		::DrawText( pdis->hDC, buf,-1, &pdis->rcItem, DT_SINGLELINE|DT_VCENTER|DT_LEFT );
	}

	bool WINAPI CDos::CHexaValuePropGridEditor::EditValue(PropGrid::PCustomParam,PropGrid::PValue value,PropGrid::TSize valueSize){
		CHexaValuePropGridEditor d(value,valueSize);
		if (d.DoModal()==IDOK){
			::memcpy( value, d.newValueBuffer, valueSize );
			return true;
		}else
			return false;
	}

	PropGrid::PCEditor CDos::CHexaValuePropGridEditor::Define(PropGrid::PCustomParam,PropGrid::TSize valueSize,PropGrid::TOnValueChanged onValueChanged){
		// creates and returns a hexa-editor usable to edit values in PropertyGrid
		return	PropGrid::Custom::DefineEditor(
					0, // default height
					valueSize,
					DrawValue,
					nullptr, EditValue, // no main control, just ellipsis button
					nullptr,
					onValueChanged
				);
	}
