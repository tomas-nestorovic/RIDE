#include "stdafx.h"

	const TSide CDos::StdSidesMap[]={ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
	CPtrList CDos::known;


	PDos CDos::GetFocused(){
		return ((CMainWindow *)app.m_pMainWnd)->pTdi->__getCurrentTab__()->dos;
	}

	void CDos::__errorCannotDoCommand__(TStdWinError cause){
		// reports on last command being not carried out due to given Cause
		Utils::FatalError(_T("Cannot carry out the command"),cause);
	}










	#define INI_SHELL_COMPLIANT_EXPORT_NAMES	_T("shcomp")
	#define INI_GETFILESIZE_OPTION				_T("gfsopt")

	CDos::CDos(PImage _image,PCFormat _pFormatBoot,TTrackScheme trackAccessScheme,PCProperties _properties,TFnCompareNames _fnCompareNames,PCSide _sideMap,UINT nResId,CFileManagerView *_pFileManager,TGetFileSizeOptions _getFileSizeDefaultOption)
		// ctor
		: image(_image) , properties(_properties) , fnCompareNames(_fnCompareNames)
		, sideMap(_sideMap) , menu(nResId) , pFileManager(_pFileManager)
		, formatBoot(*_pFormatBoot) // information on Medium Format retrieved from Boot; this information has ALWAYS priority when manipulating data on the disk; changes in this structure must be projected back to Boot Sector using FlushToBootSector (e.g. called automatically by BootView)
		, trackAccessScheme(trackAccessScheme) // single Scheme to access Tracks in Image
		, currentDir(DOS_DIR_ROOT) , currentDirId(DOS_DIR_ROOT_ID)
		, generateShellCompliantExportNames( __getProfileBool__(INI_SHELL_COMPLIANT_EXPORT_NAMES,true) ) // True <=> the GetFileExportNameAndExt function must produce names that are compliant with the FAT32 file system, otherwise False
		, getFileSizeDefaultOption( (TGetFileSizeOptions)__getProfileInt__(INI_GETFILESIZE_OPTION,_getFileSizeDefaultOption) ) {
	}

	CDos::~CDos(){
		// dtor
		// - destroying HexaPreview of File's content associated with current FileManager
		if (CHexaPreview::pSingleInstance && &CHexaPreview::pSingleInstance->rFileManager==pFileManager)
			CHexaPreview::pSingleInstance->DestroyWindow();
		// - hiding DOS Menu
		menu.__hide__();
		// - removing Tabs from TDI
		//nop (see CTdiTemplate)
	}









	bool CDos::__isValidCharInFat32LongName__(WCHAR c){
		// True <=> specified Character is valid for a FAT32 long file name, otherwise False
		static const WCHAR ForbiddenChars[]=L"%#&<>|/";
		return (WORD)c>=32 && !::wcschr(ForbiddenChars,c);
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

	bool CDos::ValidateFormatChangeAndReportProblem(bool reformatting,PCFormat f) const{
		// True <=> specified Format is acceptable, otherwise False (and informing on error)
		const DWORD nSectorsInTotal=f->GetCountOfAllSectors();
		const WORD clusterSize=f->clusterSize*( f->sectorLength - properties->dataBeginOffsetInSector - properties->dataEndOffsetInSector );
		TCHAR buf[200];
		if (nSectorsInTotal<properties->nSectorsInTotalMin){ // occurs only when fresh formatting a new Image
			::wsprintf(buf,_T("The minimum total number of sectors for a \"%s\" disk is %d (the new geometry makes up only %d)."),properties->name,properties->nSectorsInTotalMin,nSectorsInTotal);
reportError:Utils::Information(buf);
			return false;
		}else if (clusterSize>properties->clusterSizeMax){
			::wsprintf(buf,_T("The maximum cluster size for a \"%s\" disk is %d Bytes (it's %d Bytes now)."),properties->name,properties->clusterSizeMax,clusterSize);
			goto reportError;
		}else
			return true;
	}

	TCylinder CDos::__getLastOccupiedStdCylinder__() const{
		// finds and returns number of the last (at least partially) occupied Cylinder (0..N-1)
		TSectorId bufferId[(TSector)-1];
		if (formatBoot.mediumType!=TMedium::UNKNOWN) // Unknown Medium if creating a new Image
			for( TCylinder cylMin=TMedium::GetProperties(formatBoot.mediumType)->cylinderRange.iMax; cylMin--; )
				for( THead head=formatBoot.nHeads; head--; )
					if (ERROR_EMPTY!=__isTrackEmpty__( cylMin, head, __getListOfStdSectors__(cylMin,head,bufferId), bufferId ))
						return cylMin;
		return 0;
	}

	TSector CDos::__getListOfStdSectors__(TCylinder cyl,THead head,PSectorId bufferId) const{
		// populates Buffer with standard ("official") Sector IDs for given Track and returns their count (Zone Bit Recording currently NOT supported!)
		for( TSector s=properties->firstSectorNumber,nSectors=formatBoot.nSectors; nSectors--; bufferId++ )
			bufferId->cylinder=cyl, bufferId->side=sideMap[head], bufferId->sector=s++, bufferId->lengthCode=formatBoot.sectorLengthCode;
		return formatBoot.nSectors;
	}

	TStdWinError CDos::__isTrackEmpty__(TCylinder cyl,THead head,TSector nSectors,PCSectorId sectors) const{
		// return ERROR_EMPTY/ERROR_NOT_EMPTY or another Windows standard i/o error
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

	struct TEmptyCylinderParams sealed{
		const CDos *const dos;
		const TCylinder nCylinders;
		PCCylinder cylinders;

		TEmptyCylinderParams(const CDos *dos,TCylinder nCylinders,PCCylinder cylinders)
			: dos(dos) , nCylinders(nCylinders) , cylinders(cylinders) {
		}
	};
	UINT AFX_CDECL CDos::__checkCylindersAreEmpty_thread__(PVOID _pCancelableAction){
		// thread to determine if given Cylinders are empty; return ERROR_EMPTY/ERROR_NOT_EMPTY or another Windows standard i/o error
		TBackgroundActionCancelable *const pAction=(TBackgroundActionCancelable *)_pCancelableAction;
		TEmptyCylinderParams ecp=*(TEmptyCylinderParams *)pAction->fnParams;
		const TCylinder firstCylinderWithEmptySector=ecp.dos->GetFirstCylinderWithEmptySector();
		for( TCylinder n=0; n<ecp.nCylinders; pAction->UpdateProgress(++n) ){
			if (!pAction->bContinue) return ERROR_CANCELLED;
			const TCylinder cyl=*ecp.cylinders++;
			// . Cylinder not empty if located before FirstCylinderWithEmptySector
			if (cyl<firstCylinderWithEmptySector)
				return pAction->TerminateWithError(ERROR_NOT_EMPTY);
			// . checking if Cylinder empty
			TSectorId bufferId[(TSector)-1];
			for( THead head=0; head<ecp.dos->formatBoot.nHeads; head++ ){
				const TStdWinError err=ecp.dos->__isTrackEmpty__( cyl, head, ecp.dos->__getListOfStdSectors__(cyl,head,bufferId), bufferId );
				if (err!=ERROR_EMPTY)
					return pAction->TerminateWithError(err);
			}
		}
		return ERROR_EMPTY;
	}
	TStdWinError CDos::__areStdCylindersEmpty__(TTrack nCylinders,PCylinder bufCylinders) const{
		// checks if specified Cylinders are empty (i.e. none of their standard "official" Sectors is allocated, non-standard sectors ignored); returns Windows standard i/o error
		if (nCylinders)
			return	TBackgroundActionCancelable(
						__checkCylindersAreEmpty_thread__,
						&TEmptyCylinderParams( this, nCylinders, bufCylinders ),
						THREAD_PRIORITY_BELOW_NORMAL
					).CarryOut(nCylinders);
		else
			return ERROR_EMPTY;
	}


	TStdWinError CDos::__showDialogAndFormatStdCylinders__(CFormatDialog &rd,PCylinder bufCylinders,PHead bufHeads){
		// formats Cylinders using parameters obtained from the confirmed FormatDialog (CDos-derivate and FormatDialog guarantee that all parameters are valid); returns Windows standard i/o error
		if (image->__reportWriteProtection__()) return ERROR_WRITE_PROTECT;
		LOG_DIALOG_DISPLAY(_T("CFormatDialog"));
		if (LOG_DIALOG_RESULT(rd.DoModal())!=IDOK){
			::SetLastError(ERROR_CANCELLED);
			return ERROR_CANCELLED;
		}
		const TStdWinError err=__formatStdCylinders__(rd,bufCylinders,bufHeads);
		::SetLastError(err);
		return LOG_ERROR(err);
	}

	TStdWinError CDos::__formatStdCylinders__(const CFormatDialog &rd,PCylinder bufCylinders,PHead bufHeads){
		// formats Cylinders using parameters obtained from the confirmed FormatDialog (CDos-derivate and FormatDialog guarantee that all parameters are valid); returns Windows standard i/o error
		// - composing list of Tracks to format
		TTrack n=0;
		for( TCylinder c=rd.params.cylinder0; c<=rd.params.format.nCylinders; c++ )
			for( THead h=0; h<rd.params.format.nHeads; h++,n++ )
				bufCylinders[n]=c, bufHeads[n]=h;
		// - checking if formatting can proceed
		TStdWinError err;
		if (rd.params.cylinder0){
			// request to NOT format from the beginning of disk - all targeted Tracks must be empty
			const TStdWinError err=__areStdCylindersEmpty__(n,bufCylinders);
			if (err!=ERROR_EMPTY){
				Utils::Information( DOS_ERR_CANNOT_FORMAT, DOS_ERR_CYLINDERS_NOT_EMPTY, DOS_MSG_CYLINDERS_UNCHANGED );
				return err;
			}
		}else{
			// request to format from the beginning of disk - warning that all data will be destroyed
			if (!Utils::QuestionYesNo(_T("About to format the whole image and destroy all data.\n\nContinue?!"),MB_DEFBUTTON2))
				return ERROR_CANCELLED;
			err=image->Reset();
			if (err!=ERROR_SUCCESS) return err;
			err=image->SetMediumTypeAndGeometry( &rd.params.format, sideMap, properties->firstSectorNumber );
			if (err!=ERROR_SUCCESS) return err;
		}
		// - carrying out the formatting
		TSectorId bufferId[(TSector)-1];	WORD bufferLength[(TSector)-1];
		for( TSector n=-1; n--; bufferLength[n]=rd.params.format.sectorLength );
		const TSector nSectors0=formatBoot.nSectors;
			const TFormat::TLengthCode lengthCode0=formatBoot.sectorLengthCode;
				formatBoot.nSectors=rd.params.format.nSectors, formatBoot.sectorLengthCode=rd.params.format.sectorLengthCode;
				err=__formatTracks__(	n, bufCylinders, bufHeads,
										0, bufferId, bufferLength, // 0 = standard Sectors
										rd.params, rd.showReportOnFormatting==BST_CHECKED
									);
			formatBoot.sectorLengthCode=lengthCode0;
		formatBoot.nSectors=nSectors0;
		if (err!=ERROR_SUCCESS)
			return err;
		// - adding formatted Cylinders to Boot and FAT
		if (!rd.params.cylinder0){
			// formatted from beginning of disk - updating internal information on Format
			formatBoot=rd.params.format;
			formatBoot.nCylinders++; // because Cylinders numbered from zero
			InitializeEmptyMedium(&rd.params); // DOS-specific initialization of newly formatted Medium
			if (dynamic_cast<CFDD *>(image)) // if formatted on floppy Drive ...
				if (!image->OnSaveDocument(nullptr)) // ... immediately saving all Modified Sectors (Boot, FAT, Dir,...)
					return ::GetLastError();
		}else{
			// formatted only selected Cylinders
			if (rd.updateBoot){
				// requested to update Format in Boot Sector
				formatBoot.nCylinders=max( formatBoot.nCylinders, rd.params.format.nCylinders+1 ); // "+1" = because Cylinders numbered from zero
				FlushToBootSector();
			}
			if (rd.addTracksToFat)
				// requested to include newly formatted Tracks into FAT
				if (!__addStdTracksToFatAsEmpty__(n,bufCylinders,bufHeads))
					Utils::Information(FAT_SECTOR_UNMODIFIABLE, err=::GetLastError() );
		}
		image->UpdateAllViews(nullptr); // although updated already in FormatTracks, here calling too as FormatBoot might have changed since then
		::SetLastError(err);
		return ERROR_SUCCESS;
	}
	struct TFmtParams sealed{
		const CDos *const dos;
		TTrack nTracks;
		PCCylinder cylinders;
		PCHead heads;
		const TSector nSectors;
		const PSectorId bufferId; // non-const to be able to dynamically generate "Zoned Bit Recording" Sectors in the future
		const PCWORD bufferLength;
		const CFormatDialog::TParameters &rParams;
		const bool showReport;

		TFmtParams(const CDos *dos,TTrack nTracks,PCCylinder cylinders,PCHead heads,TSector nSectors,PSectorId bufferId,PCWORD bufferLength,const CFormatDialog::TParameters &rParams,bool showReport)
			: dos(dos) , nTracks(nTracks) , cylinders(cylinders) , heads(heads) , nSectors(nSectors) , bufferId(bufferId) , bufferLength(bufferLength) , rParams(rParams) , showReport(showReport) {
		}
	};
	UINT AFX_CDECL CDos::__formatTracks_thread__(PVOID _pCancelableAction){
		// thread to format selected Tracks
		TBackgroundActionCancelable *const pAction=(TBackgroundActionCancelable *)_pCancelableAction;
		TFmtParams fp=*(TFmtParams *)pAction->fnParams;
		struct{
			TTrack nTracks;
			DWORD nSectorsInTotal;
			DWORD nSectorsBad;
			DWORD availableGoodCapacityInBytes;
		} statistics;
		::ZeroMemory(&statistics,sizeof(statistics));
		for( TCylinder n=0; fp.nTracks--; pAction->UpdateProgress(++n) ){
			if (!pAction->bContinue) return ERROR_CANCELLED;
			const TCylinder cyl=*fp.cylinders++;
			const THead head=*fp.heads++;
			// . formatting Track
			TFdcStatus bufferFdcStatus[(TSector)-1];
			TSector nSectors;
			TStdWinError err;
			if (fp.nSectors)
				// custom Sectors (caller generated their IDs into Buffer)
				err=fp.dos->image->FormatTrack(	cyl, head,
												nSectors=fp.nSectors, fp.bufferId, fp.bufferLength,
												(PCFdcStatus)::memset(bufferFdcStatus,0,sizeof(TFdcStatus)*fp.nSectors),
												fp.rParams.gap3, fp.dos->properties->sectorFillerByte
											);
			else{
				// standard Sectors (their IDs generated into Buffer now)
				// : generating IDs
				nSectors=fp.dos->__getListOfStdSectors__( cyl, head, fp.bufferId );
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
				err=fp.dos->image->FormatTrack(	cyl, head,
												nSectors, stdSectors, fp.bufferLength,
												(PCFdcStatus)::memset(bufferFdcStatus,0,sizeof(TFdcStatus)*nSectors),
												fp.rParams.gap3, fp.dos->properties->sectorFillerByte
											);
			}
			if (err!=ERROR_SUCCESS)
				return pAction->TerminateWithError(err);
			statistics.nTracks++, statistics.nSectorsInTotal+=nSectors;
			// . verifying Tracks by trying to read their formatted Sectors
			if (fp.dos->image->RequiresFormattedTracksVerification()){
				// : buffering Sectors from the same Track by the underlying Image, making them ready for IMMEDIATE usage
				fp.dos->image->BufferTrackData( cyl, head, fp.bufferId, Utils::CByteIdentity(), nSectors, false );
				// : verifying formatted Sectors
				WORD w;
				for( PCSectorId pId=fp.bufferId; nSectors--; )
					if (const PCSectorData tmp=fp.dos->image->GetSectorData(cyl,head,pId++,&w))
						statistics.availableGoodCapacityInBytes+=w;
					else
						statistics.nSectorsBad++;
			}else
				for( PCWORD pLength=fp.bufferLength; nSectors--; statistics.availableGoodCapacityInBytes+=*pLength++ );
		}
		if (fp.showReport){
			TCHAR buf[512];
			float units; LPCTSTR unitsName;
			Utils::BytesToHigherUnits( statistics.availableGoodCapacityInBytes, units, unitsName );
			_stprintf( buf, _T("LOW-LEVEL FORMATTING DONE\n\n\n- formatted %d track(s)\n- with totally %d sectors\n- of which %d are bad (%.2f %%)\n- resulting in %.2f %s raw good capacity.\n\nSee the Track Map tab for more information."), statistics.nTracks, statistics.nSectorsInTotal, statistics.nSectorsBad, (float)statistics.nSectorsBad*100/statistics.nSectorsInTotal, units, unitsName );
			Utils::Information(buf);
		}
		return ERROR_SUCCESS;
	}
	TStdWinError CDos::__formatTracks__(TTrack nTracks,PCCylinder cylinders,PCHead heads,TSector nSectors,PSectorId bufferId,PCWORD bufferLength,const CFormatDialog::TParameters &rParams,bool showReport){
		// formats given Tracks, each with given NumberOfSectors, each with given Length; returns Windows standard i/o error
		const TStdWinError err=	TBackgroundActionCancelable(
									__formatTracks_thread__,
									&TFmtParams( this, nTracks, cylinders, heads, nSectors, bufferId, bufferLength, rParams, showReport ),
									THREAD_PRIORITY_BELOW_NORMAL
								).CarryOut(nTracks);
		if (err!=ERROR_SUCCESS)
			Utils::FatalError(_T("Cannot format a track"),err);
		return err;
	}

	TStdWinError CDos::__unformatStdCylinders__(CUnformatDialog &rd,PCylinder bufCylinders,PHead bufHeads){
		// unformats Cylinders using Parameters obtained from confirmed UnformatDialog (CDos-derivate and UnformatDialog guarantee that all parameters are valid); returns Windows standard i/o error
		if (image->__reportWriteProtection__()) return ERROR_WRITE_PROTECT;
		LOG_DIALOG_DISPLAY(_T("CUnformatDialog"));
		if (LOG_DIALOG_RESULT(rd.DoModal())!=IDOK) return ERROR_CANCELLED;
		// - composing the list of Tracks to unformat
		TTrack n=0;
		for( TCylinder c=rd.cylA; c<=rd.cylZ; c++ )
			for( THead h=0; h<formatBoot.nHeads; h++,n++ )
				bufCylinders[n]=c, bufHeads[n]=h;
		// - checking that all Cylinders to unformat are empty
		TStdWinError err=__areStdCylindersEmpty__(n,bufCylinders);
		if (err!=ERROR_EMPTY){
			Utils::Information( DOS_ERR_CANNOT_UNFORMAT, DOS_ERR_CYLINDERS_NOT_EMPTY, DOS_MSG_CYLINDERS_UNCHANGED );
			return LOG_ERROR(err);
		}
		// - carrying out the unformatting
		if (( err=__unformatTracks__(n,bufCylinders,bufHeads) )!=ERROR_SUCCESS)
			return LOG_ERROR(err);
		// - removing unformatted Cylinders from Boot Sector and FAT (if commanded so)
		if (rd.updateBoot){
			if (1+rd.cylZ>=formatBoot.nCylinders)
				formatBoot.nCylinders=min(formatBoot.nCylinders,rd.cylA);
			FlushToBootSector();
		}
		if (rd.removeTracksFromFat)
			if (!__removeStdTracksFromFat__(n,bufCylinders,bufHeads))
				Utils::Information(FAT_SECTOR_UNMODIFIABLE, err=::GetLastError() );
		image->UpdateAllViews(nullptr); // although updated already in UnformatTracks, here calling too as FormatBoot might have changed since then
		return ERROR_SUCCESS;
	}
	struct TUnfmtParams sealed{
		const PImage image;
		TTrack nTracks;
		PCCylinder cylinders;
		PCHead heads;

		TUnfmtParams(PImage image,TTrack nTracks,PCCylinder cylinders,PCHead heads)
			: image(image) , nTracks(nTracks) , cylinders(cylinders) , heads(heads) {
		}
	};
	UINT AFX_CDECL CDos::__unformatTracks_thread__(PVOID _pCancelableAction){
		// thread to unformat specified Tracks
		TBackgroundActionCancelable *const pAction=(TBackgroundActionCancelable *)_pCancelableAction;
		TUnfmtParams ufp=*(TUnfmtParams *)pAction->fnParams;
		ufp.cylinders+=ufp.nTracks, ufp.heads+=ufp.nTracks; // unformatting "backwards"
		for( TCylinder n=0; ufp.nTracks--; pAction->UpdateProgress(++n) ){
			if (!pAction->bContinue) return ERROR_CANCELLED;
			const TStdWinError err=ufp.image->UnformatTrack( *--ufp.cylinders, *--ufp.heads );
			if (err!=ERROR_SUCCESS)
				return pAction->TerminateWithError(err);
		}
		return ERROR_SUCCESS;
	}
	TStdWinError CDos::__unformatTracks__(TTrack nTracks,PCCylinder cylinders,PCHead heads){
		// unformats given Tracks; returns Windows standard i/o error
		const TStdWinError err=	TBackgroundActionCancelable(
									__unformatTracks_thread__,
									&TUnfmtParams( image, nTracks, cylinders, heads ),
									THREAD_PRIORITY_BELOW_NORMAL
								).CarryOut(nTracks);
		if (err!=ERROR_SUCCESS)
			Utils::FatalError(_T("Cannot unformat a track"),err);
		return err;
	}

	bool CDos::__addStdTracksToFatAsEmpty__(TTrack nTracks,PCCylinder cylinders,PCHead heads){
		// records standard "official" Sectors in given Tracks as Empty into FAT
		// - all possible Sectors are Empty
		TSectorStatus statuses[(TSector)-1];
		for( TSector n=0; n<(TSector)-1; statuses[n++]=TSectorStatus::EMPTY );
		// - adding
		bool result=true; // assumption (all Tracks successfully added to FAT)
		while (nTracks--)
			result&=ModifyTrackInFat( *cylinders++, *heads++, statuses );
		return result;
	}
	bool CDos::__removeStdTracksFromFat__(TTrack nTracks,PCCylinder cylinders,PCHead heads){
		// records standard "official" Sectors in given Tracks as Unavailable
		// - all possible Sectors are Unavailable
		TSectorStatus statuses[(TSector)-1];
		for( TSector n=0; n<(TSector)-1; statuses[n++]=TSectorStatus::UNAVAILABLE );
		// - removing
		bool result=true; // assumption (all Tracks successfully removed from FAT)
		while (nTracks--)
			result&=ModifyTrackInFat( *cylinders++, *heads++, statuses );
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
	UINT AFX_CDECL CDos::__fillEmptySpace_thread__(PVOID _pCancelableAction){
		// thread to flood selected types of empty space on disk
		LOG_ACTION(_T("Fill empty space"));
		const TBackgroundActionCancelable *const pAction=(TBackgroundActionCancelable *)_pCancelableAction;
		TFillEmptySpaceParams fesp=*(TFillEmptySpaceParams *)pAction->fnParams;
		const PImage image=fesp.dos->image;
		// - filling all Empty Sectors
		if (fesp.rd.fillEmptySectors)
			for( TCylinder cyl=0,const nCylinders=image->GetCylinderCount(); cyl<nCylinders; pAction->UpdateProgress(++cyl) )
				for( THead head=fesp.dos->formatBoot.nHeads; head--; ){
					if (!pAction->bContinue) return ERROR_CANCELLED;
					// : determining standard Empty Sectors
					TSectorId bufferId[(TSector)-1],*pId=bufferId,*pEmptyId=bufferId;
					TSector nSectors=fesp.dos->__getListOfStdSectors__(cyl,head,bufferId);
					TSectorStatus statuses[(TSector)-1],*ps=statuses;
					for( fesp.dos->GetSectorStatuses(cyl,head,nSectors,bufferId,statuses); nSectors-->0; pId++ )
						if (*ps++==TSectorStatus::EMPTY)
							*pEmptyId++=*pId;
					// : buffering Sectors from the same Track by the underlying Image, making them ready for IMMEDIATE usage
					fesp.dos->image->BufferTrackData( cyl, head, bufferId, Utils::CByteIdentity(), pEmptyId-bufferId, true );
					// : filling all Empty Sectors
					TPhysicalAddress chs={ cyl, head };
					for( WORD w; pEmptyId>bufferId; ){
						chs.sectorId=*--pEmptyId;
						if (const PSectorData data=image->GetSectorData(chs,&w)){
							::memset( data, fesp.rd.sectorFillerByte, w );
							image->MarkSectorAsDirty(chs);
						}//else
							//TODO: warning on Sector unreadability
					}
				}
		// - filling empty space in each File's last Sector (WARNING: It's assumed that "dot" and "dotdot"-like DirectoryEntries are disabled to prevent from unfinite looping!)
		if (fesp.rd.fillFileEndings){
			const WORD nDataBytesInSector=fesp.dos->formatBoot.sectorLength-fesp.dos->properties->dataBeginOffsetInSector-fesp.dos->properties->dataEndOffsetInSector;
			pAction->UpdateProgress(0);
			// . adding current Directory into DiscoverdDirectories, and backing-up current Directory (may be changed during processing)
			CFileManagerView::TFileList discoveredDirs;
			discoveredDirs.AddHead(fesp.dos->currentDir);
			// . filling empty space last Sectors
			while (discoveredDirs.GetCount()){
				const PFile dir=discoveredDirs.RemoveHead();
				if (const auto pdt=fesp.dos->BeginDirectoryTraversal(dir))
					while (const PCFile file=pdt->GetNextFileOrSubdir()){
						if (!pAction->bContinue) return ERROR_CANCELLED;
						switch (pdt->entryType){
							case TDirectoryTraversal::FILE:{
								// File
								const CFatPath fatPath(fesp.dos,file);
								CFatPath::PCItem item; DWORD n;
								if (const LPCTSTR err=fatPath.GetItems(item,n))
									fesp.dos->__showFileProcessingError__(file,err);
								else
									for( DWORD fileSize=fesp.dos->GetFileOccupiedSize(file); n--; item++ )
										if (nDataBytesInSector<fileSize)
											fileSize-=nDataBytesInSector;
										else{
											pAction->UpdateProgress(item->chs.cylinder);
											if (const PSectorData sectorData=fesp.dos->image->GetSectorData(item->chs)){
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
		}
		// - filling Empty Directory entries (WARNING: It's assumed that "dot" and "dotdot"-like DirectoryEntries are disabled to prevent from unfinite looping!)
		if (fesp.rd.fillEmptyDirectoryEntries){
			pAction->UpdateProgress(0);
			// . adding current Directory into DiscoverdDirectories, and backing-up current Directory (may be changed during processing)
			CFileManagerView::TFileList discoveredDirs;
			discoveredDirs.AddHead(fesp.dos->currentDir);
			// . filling Empty Directory entries
			while (discoveredDirs.GetCount()){
				const PFile dir=discoveredDirs.RemoveHead();
				if (const auto pdt=fesp.dos->BeginDirectoryTraversal(dir)){
					pAction->UpdateProgress(pdt->chs.cylinder);
					while (pdt->AdvanceToNextEntry()){
						if (!pAction->bContinue) return ERROR_CANCELLED;
						switch (pdt->entryType){
							case TDirectoryTraversal::EMPTY:
								// Empty entry
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
				}//else
					//TODO: warning
			}
		}
		pAction->UpdateProgress(-1); // "-1" = completed
		return ERROR_SUCCESS;
	}
	bool CDos::__fillEmptySpace__(CFillEmptySpaceDialog &rd){
		// True <=> filling of empty space on disk with specified FillerBytes was successfull, otherwise False
		if (image->__reportWriteProtection__()) return false;
		if (rd.DoModal()!=IDOK) return false;
		// - filling
		TBackgroundActionCancelable(
			__fillEmptySpace_thread__,
			&TFillEmptySpaceParams(this,rd),
			THREAD_PRIORITY_BELOW_NORMAL
		).CarryOut( 1+image->GetCylinderCount() ); // "1+" = to not terminate the action prelimiary when having processed the last Cylinder in Image
		// - updating Views
		image->UpdateAllViews(nullptr);
		return true;
	}






	DWORD CDos::GetFreeSpaceInBytes(TStdWinError &rError) const{
		// computes and returns the empty space on Image in Bytes
		DWORD result=0; rError=ERROR_SUCCESS; // assumption (no empty space, no error)
		const WORD nDataBytesInSector=formatBoot.sectorLength-properties->dataBeginOffsetInSector-properties->dataEndOffsetInSector;
		for( TCylinder cyl=GetFirstCylinderWithEmptySector(); cyl<formatBoot.nCylinders; cyl++ )
			for( THead head=0; head<formatBoot.nHeads; head++ ){
				TSectorId bufferId[(TSector)-1];
				TSector n=__getListOfStdSectors__(cyl,head,bufferId);
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

	PTCHAR CDos::GetFileNameWithAppendedExt(PCFile file,PTCHAR bufNameExt) const{
		// returns the Buffer populated with File name concatenated with File extension
		TCHAR bufExt[MAX_PATH];
		GetFileNameAndExt(file,bufNameExt,bufExt);
		if (*bufExt)
			return ::lstrcat( ::lstrcat(bufNameExt,_T(".")), bufExt );
		else
			return bufNameExt;
	}

	bool CDos::HasFileNameAndExt(PCFile file,LPCTSTR fileName,LPCTSTR fileExt) const{
		// True <=> given File has the name and extension as specified, otherwise False
		ASSERT(fileName!=nullptr && fileExt!=nullptr);
		TCHAR bufName[MAX_PATH],bufExt[MAX_PATH];
		GetFileNameAndExt( file, bufName, bufExt );
		return !fnCompareNames(fileName,bufName) && !fnCompareNames(fileExt,bufExt);
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
		if (pCreated) *pCreated=TFileDateTime::None;	// Files don't have time stamps by default
		if (pLastRead) *pLastRead=TFileDateTime::None;	// Files don't have time stamps by default
		if (pLastWritten) *pLastWritten=TFileDateTime::None;	// Files don't have time stamps by default
	}

	bool CDos::GetFileCreatedTimeStamp(PCFile file,FILETIME &rCreated) const{
		// True <=> File has a Created time stamp copied to the output field, otherwise False
		GetFileTimeStamps( file, &rCreated, nullptr, nullptr );
		return TFileDateTime::None!=rCreated;
	}

	bool CDos::GetFileLastReadTimeStamp(PCFile file,FILETIME &rLastRead) const{
		// True <=> File has a LastRead time stamp copied to the output field, otherwise False
		GetFileTimeStamps( file, nullptr, &rLastRead, nullptr );
		return TFileDateTime::None!=rLastRead;
	}

	bool CDos::GetFileLastWrittenTimeStamp(PCFile file,FILETIME &rLastWritten) const{
		// True <=> File has a LastRead time stamp copied to the output field, otherwise False
		GetFileTimeStamps( file, nullptr, nullptr, &rLastWritten );
		return TFileDateTime::None!=rLastWritten;
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
			DWORD nDataBytesToExport=GetFileSize(file,&nBytesReservedBeforeData,nullptr);
			nDataBytesToExport=min(nDataBytesToExport,nMaxDataBytesToExport);
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
				image->BufferTrackData( currCyl, currHead, bufferId, sectorIdAndPositionIdentity, nSectors, true ); // make Sectors data ready for IMMEDIATE usage
				// . reading Sectors from the same Track in the underlying Image
				WORD w;
				for( TSector s=0; s<nSectors; s++ )
					if (const PCSectorData sectorData=image->GetSectorData(currCyl,currHead,bufferId+s,0,true,&w,&TFdcStatus())){
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

	PTCHAR CDos::GetFileExportNameAndExt(PCFile file,bool shellCompliant,PTCHAR buf) const{
		// populates Buffer with specified File's export name and extension and returns the Buffer; returns Null if File cannot be exported (e.g. a "dotdot" entry in MS-DOS); caller guarantees that the Buffer is at least MAX_PATH characters big
		if (shellCompliant){
			// exporting to non-RIDE target (e.g. to the Explorer); excluding from the Buffer characters that are forbidden in FAT32 long file names
			TCHAR nameAndExt[MAX_PATH];
			for( PTCHAR p=GetFileNameWithAppendedExt(file,nameAndExt); const TCHAR c=*p; )
				if (__isValidCharInFat32LongName__(c))
					p++; // keeping valid Character
				else
					::lstrcpy(p,1+p); // skipping invalid Character
			if (*nameAndExt=='.' || *nameAndExt=='\0'){
				// invalid export name - generating an artifical one
				static WORD fileId;
				::wsprintf( buf, _T("File%04d%s"), ++fileId, nameAndExt );
			}else
				// valid export name - taking it as the result
				::lstrcpy(buf,nameAndExt);
		}else{
			// exporting to another RIDE instance; substituting non-alphanumeric characters with "URL-like" escape sequences
			TCHAR tmp[MAX_PATH],*pOutChar=buf;
			GetFileNameAndExt(file,tmp,nullptr);
			for( LPCTSTR pInChar=tmp; const TCHAR c=*pInChar++; )
				if (::isalpha((unsigned char)c))
					*pOutChar++=c;
				else
					pOutChar+=::wsprintf( pOutChar, _T("%%%02x"), (unsigned char)c );
			*pOutChar++='.';
			GetFileNameAndExt(file,nullptr,tmp);
			for( LPCTSTR pInChar=tmp; const TCHAR c=*pInChar++; )
				if (::isalpha((unsigned char)c))
					*pOutChar++=c;
				else
					pOutChar+=::wsprintf( pOutChar, _T("%%%02x"), (unsigned char)c );
			*pOutChar='\0';		
		}
		return buf;
	}

	DWORD CDos::ExportFile(PCFile file,CFile *fOut,DWORD nBytesToExportMax,LPCTSTR *pOutError) const{
		// exports data portion of specfied File (data portion size determined by GetFileDataSize); returns the export size of specified File
		if (fOut){
			const LPCTSTR errMsg=__exportFileData__(file,fOut,nBytesToExportMax);
			if (pOutError)
				*pOutError=errMsg;
		}
		return std::min<>( GetFileSize(file), nBytesToExportMax );
	}

	TStdWinError CDos::__importFileData__(CFile *f,PFile fDesc,LPCTSTR fileName,LPCTSTR fileExt,DWORD fileSize,PFile &rFile,CFatPath &rFatPath){
		// imports given File into the disk; returns Windows standard i/o error
		ASSERT(fileName!=nullptr && fileExt!=nullptr);
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
					case TDirectoryTraversal::WARNING:{
						// any Warning becomes a real error!
						const TStdWinError err=pdt->warning;
						return LOG_ERROR(err);
					}
					#ifdef _DEBUG
					default:
						Utils::Information(_T("CDos::__importFile__ - unknown pdt->entryType"));
						ASSERT(FALSE);
					#endif
				}
		//}
		// - creating a record for specified File in current Directory
		//if (const auto pdt=BeginDirectoryTraversal()){
			PFile tmp;
			TStdWinError err=ChangeFileNameAndExt( fDesc, fileName, fileExt, tmp );
			if (err==ERROR_SUCCESS)
				if (tmp!=fDesc) // a new record was created for the File (in ChangeFileNameAndExt)
					rFile=tmp;
				else{ // no record was created for the File in current Directory - recording the File into the entry found above
					if (!rFile) // no Empty entry found above ...
						rFile=pdt->AllocateNewEntry(); // ... allocating new one
					if (rFile){ // Empty entry found ...
						::memcpy( rFile, fDesc, pdt->entrySize ); // ... initializing it by supplied FileDescriptor (DOS-specific)
						__markDirectorySectorAsDirty__(rFile);
					}else // Empty entry not found and not allocated
						err=ERROR_CANNOT_MAKE;
				}
			if (err!=ERROR_SUCCESS)
				return LOG_ERROR(err);
		//}
		// - checking if there's enough empty space on the disk
		//if (const auto pdt=BeginDirectoryTraversal()){
			if (fileSize>GetFreeSpaceInBytes(err)){
				DeleteFile(rFile); // removing the above added File record from current Directory
				return LOG_ERROR(ERROR_DISK_FULL);
			}
		}else
			return LOG_ERROR(ERROR_PATH_NOT_FOUND);
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
					const TSector nSectors=__getListOfStdSectors__( item.chs.cylinder, item.chs.head, bufferId );
					// . filtering out only Empty standard Sectors
					TSectorStatus statuses[(TSector)-1],*ps=statuses;
					GetSectorStatuses( item.chs.cylinder, item.chs.head, nSectors, bufferId, statuses );
					TSector nEmptySectors=0;
					for( TSector s=0; s<nSectors; s++ )
						if (*ps++==TSectorStatus::EMPTY)
							bufferId[nEmptySectors++]=bufferId[s];
					// . buffering Sectors from the same Track by the underlying Image, making them ready for IMMEDIATE usage
					image->BufferTrackData( item.chs.cylinder, item.chs.head, bufferId, sectorIdAndPositionIdentity, nEmptySectors, true );
					// . importing the File to Empty Sectors on the current Track
					for( WORD w; nEmptySectors--; ){
						item.chs.sectorId=*pId++;
						if (const PSectorData sectorData=image->GetSectorData(item.chs,&w)){
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
								goto finished;
							}else{
								f->Read(sectorData+properties->dataBeginOffsetInSector,fileSize);
								if (w==fileSize) fileSize=0;
								//image->MarkSectorAsDirty(...); // commented out as carried out below if whole import successfull
								item.value=fileSize;
								rFatPath.AddItem(&item);
								goto finished;
							}
						}else{ // error when accessing discovered Empty Sector
							const TStdWinError err=::GetLastError();
							DeleteFile(rFile); // removing the above added File record from current Directory
							return LOG_ERROR(err);
						}
					}
				}
finished:
		CFatPath::PCItem p;	DWORD n;
		for( rFatPath.GetItems(p,n); n--; image->MarkSectorAsDirty(p++->chs) );
		return ERROR_SUCCESS;
	}

	#define ERROR_MSG_CANNOT_PROCESS	_T("Cannot process \"%s\"")

	void CDos::__showFileProcessingError__(PCFile file,LPCTSTR cause) const{
		// shows general error message on File being not processable due to occured Cause
		TCHAR buf[MAX_PATH+50];
		::wsprintf( buf, ERROR_MSG_CANNOT_PROCESS, GetFileNameWithAppendedExt(file,buf+50) );
		Utils::FatalError(buf,cause);
	}
	void CDos::__showFileProcessingError__(PCFile file,TStdWinError cause) const{
		// shows general error message on File being not processable due to occured Cause
		TCHAR buf[MAX_PATH+50];
		::wsprintf( buf, ERROR_MSG_CANNOT_PROCESS, GetFileNameWithAppendedExt(file,buf+50) );
		Utils::FatalError(buf,cause);
	}

	void CDos::__markDirectorySectorAsDirty__(LPCVOID dirEntry) const{
		// marks Directory Sector that contains specified Directory entry as "dirty"
		if (const auto pdt=BeginDirectoryTraversal())
			while (pdt->AdvanceToNextEntry())
				if (pdt->entry==dirEntry){
					image->MarkSectorAsDirty(pdt->chs);
					break;
				}
	}

	CDos::PFile CDos::__findFile__(PCFile directory,LPCTSTR fileName,LPCTSTR fileExt,PCFile ignoreThisFile) const{
		// finds and returns a File with given NameAndExtension; returns Null if such File doesn't exist
		ASSERT(fileName!=nullptr && fileExt!=nullptr);
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

	CDos::PFile CDos::__findFileInCurrDir__(LPCTSTR fileName,LPCTSTR fileExt,PCFile ignoreThisFile) const{
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
			return ERROR_IO_DEVICE;
		// - making sure all Sectors in the FatPath are readable
		WORD w;
		for( DWORD n=0; n<nItems; )
			if (!image->GetSectorData(pItem[n++].chs,&w))
				return ERROR_SECTOR_NOT_FOUND;
			else if (w!=formatBoot.sectorLength)
				return ERROR_VOLMGR_DISK_SECTOR_SIZE_INVALID;
		// - shifting
		const WORD nDataBytesInSector=w-properties->dataBeginOffsetInSector-properties->dataEndOffsetInSector;
		if (nBytesShift<0){
			// shifting to the "left"
			const WORD offset=-nBytesShift,delta=nDataBytesInSector-offset;
			do{
				const PSectorData sectorData=image->GetSectorData(pItem->chs)+properties->dataBeginOffsetInSector;
				::memcpy( sectorData, sectorData+offset, delta );
				image->MarkSectorAsDirty(pItem++->chs);
				if (!--nItems) break;
				::memcpy( sectorData+delta, image->GetSectorData(pItem->chs)+properties->dataBeginOffsetInSector, offset );
			}while (true);
		}else{
			// shifting to the "right"
			pItem+=nItems-1;
			const WORD offset=nBytesShift,delta=nDataBytesInSector-offset;
			do{
				const PSectorData sectorData=image->GetSectorData(pItem->chs)+properties->dataBeginOffsetInSector;
				::memmove( sectorData+offset, sectorData, delta );
				image->MarkSectorAsDirty(pItem--->chs);
				if (!--nItems) break;
				::memcpy( sectorData, image->GetSectorData(pItem->chs)+properties->dataBeginOffsetInSector+delta, offset );
			}while (true);
		}
		return ERROR_SUCCESS;
	}












	TStdWinError CDos::CreateUserInterface(HWND hTdi){
		// creates DOS-specific Tabs in TDI; returns Windows standard i/o error
		// - adding the Document (Image) to TdiTemplate
		CMainWindow::CTdiTemplate::pSingleInstance->AddDocument(image);
		return ERROR_SUCCESS; // always succeeds (but may fail in CDos-derivate)
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













	BYTE CDos::TProperties::GetValidGap3ForMedium(TMedium::TType medium) const{
		// infers and returns the minimum Gap3 value applicable for all available StandardFormats that regard the specified Medium
		BYTE result=FDD_SECTOR_GAP3_STD;
		CFormatDialog::PCStdFormat pStdFmt=stdFormats;
		for( BYTE n=nStdFormats; n-->0; pStdFmt++ )
			if (pStdFmt->params.format.mediumType & medium)
				result=min( result, pStdFmt->params.gap3 );
		return result;
	}








	CDos::TDirectoryTraversal::TDirectoryTraversal(PCFile directory,WORD entrySize,WORD nameCharsMax)
		// ctor
		: directory(directory) , entrySize(entrySize) , nameCharsMax(nameCharsMax)
		, entryType(TDirectoryTraversal::EMPTY) {
	}

	CDos::PFile CDos::TDirectoryTraversal::AllocateNewEntry(){
		// allocates and returns new entry at the end of current Directory and returns; returns Null if new entry cannot be allocated (e.g. because disk is full)
		return nullptr; // Null = cannot allocate new Empty entry (aka. this Directory has fixed number of entries)
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








	static const FILETIME None;

	const CDos::TFileDateTime CDos::TFileDateTime::None(::None);

	CDos::TFileDateTime::TFileDateTime(const FILETIME &r)
		// ctor
		: FILETIME(r) {
	}

	bool CDos::TFileDateTime::operator==(const FILETIME &r) const{
		// True <=> this DateTime is the same as the specified one, otherwise False
		return dwLowDateTime==r.dwLowDateTime && dwHighDateTime==r.dwHighDateTime;
	}

	bool CDos::TFileDateTime::operator!=(const FILETIME &r) const{
		// True <=> this DateTime is different from the specified one, otherwise False
		return dwLowDateTime!=r.dwLowDateTime || dwHighDateTime!=r.dwHighDateTime;
	}

	PTCHAR CDos::TFileDateTime::DateToString(PTCHAR buf) const{
		// populates the Buffer with this Date value and returns the buffer
		static const LPCTSTR MonthAbbreviations[]={ _T("Jan"), _T("Feb"), _T("Mar"), _T("Apr"), _T("May"), _T("Jun"), _T("Jul"), _T("Aug"), _T("Sep"), _T("Oct"), _T("Nov"), _T("Dec") };
		SYSTEMTIME st;
		::FileTimeToSystemTime( this, &st );
		::SystemTimeToTzSpecificLocalTime( nullptr, &st, &st );
		::wsprintf( buf, _T("%d/%s/%d"), st.wDay, MonthAbbreviations[st.wMonth-1], st.wYear );
		return buf;
	}

	PTCHAR CDos::TFileDateTime::TimeToString(PTCHAR buf) const{
		// populates the Buffer with this Time value and returns the buffer
		SYSTEMTIME st;
		::FileTimeToSystemTime( this, &st );
		::SystemTimeToTzSpecificLocalTime( nullptr, &st, &st );
		::wsprintf( buf, _T("%d:%02d:%02d"), st.wHour, st.wMinute, st.wSecond );
		return buf;
	}

	bool CDos::TFileDateTime::Edit(bool dateEditingEnabled,bool timeEditingEnabled,const SYSTEMTIME *epoch){
		// True <=> user confirmed the shown editation dialog and accepted the new value, otherwise False
		// - defining the Dialog
		class TDateTimeDialog sealed:public CDialog{
			void DoDataExchange(CDataExchange *pDX) override{
				// exchange of data from and to controls
				SYSTEMTIME st;
				if (pDX->m_bSaveAndValidate){
					// saving the date and time combined from values of both controls together, impossible to do using DDX_* functions
					SYSTEMTIME tmp;
					SendDlgItemMessage( ID_DATE, MCM_GETCURSEL, 0, (LPARAM)&st );
					SendDlgItemMessage( ID_TIME, DTM_GETSYSTEMTIME, 0, (LPARAM)&tmp );
					st.wHour=tmp.wHour, st.wMinute=tmp.wMinute, st.wSecond=tmp.wSecond, st.wMilliseconds=tmp.wMilliseconds;
					::SystemTimeToFileTime( &st, &ft );
				}else{
					// loading the date and time values
					// . adjusting interactivity
					Utils::EnableDlgControl( m_hWnd, ID_DATE, dateEditingEnabled );
					Utils::EnableDlgControl( m_hWnd, ID_TIME, timeEditingEnabled );
					// . restricting the Date control to specified Epoch only
					SendDlgItemMessage( ID_DATE, MCM_SETRANGE, GDTR_MIN|GDTR_MAX, (LPARAM)epoch );
					// . loading
					::FileTimeToSystemTime( &ft, &st );
					SendDlgItemMessage( ID_DATE, MCM_SETCURSEL, 0, (LPARAM)&st );
					SendDlgItemMessage( ID_TIME, DTM_SETSYSTEMTIME, 0, (LPARAM)&st );
				}
			}
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				if (msg==WM_NOTIFY){
					const PNMLINK pnml=(PNMLINK)lParam;
					if (pnml->hdr.code==NM_CLICK || pnml->hdr.code==NM_RETURN)
						switch (pnml->hdr.idFrom){
							case ID_AUTO:{
								// notification regarding the "Select current {date,time}" option
								SYSTEMTIME st;
								::GetLocalTime(&st);
								pnml->item.iLink++;
								if (pnml->item.iLink&1)
									SendDlgItemMessage( ID_DATE, MCM_SETCURSEL, 0, (LPARAM)&st );
								if (pnml->item.iLink&2)
									SendDlgItemMessage( ID_TIME, DTM_SETSYSTEMTIME, 0, (LPARAM)&st );
								return 0;
							}
							case ID_REMOVE:
								// notification regarding the "Remove from FAT" option
								EndDialog(ID_REMOVE);
								break;
						}
				}
				return CDialog::WindowProc(msg,wParam,lParam);
			}
		public:
			const bool dateEditingEnabled, timeEditingEnabled;
			const SYSTEMTIME *const epoch;
			FILETIME ft;

			TDateTimeDialog(const FILETIME &rDateTime,bool dateEditingEnabled,bool timeEditingEnabled,const SYSTEMTIME *epoch)
				: CDialog(IDR_DOS_DATETIME_EDIT)
				, dateEditingEnabled(dateEditingEnabled) , timeEditingEnabled(timeEditingEnabled) , epoch(epoch)
				, ft(rDateTime) {
			}
		} d( *this, dateEditingEnabled, timeEditingEnabled, epoch );
		// - showing the Dialog and processing its result
		switch (d.DoModal()){
			case ID_REMOVE:
				d.ft=None;
				//fallthrough
			case IDOK:
				*this=d.ft;
				return true;
			default:
				return false;
		}
	}








	WORD CDos::TBigEndianWord::operator=(WORD newValue){
		// "setter"
		highByte=HIBYTE(newValue), lowByte=LOBYTE(newValue);
		return newValue;
	}

	CDos::TBigEndianWord::operator WORD() const{
		// "getter"
		return MAKEWORD(lowByte,highByte);
	}








	DWORD CDos::TBigEndianDWord::operator=(DWORD newValue){
		// "setter"
		highWord=HIWORD(newValue), lowWord=LOWORD(newValue);
		return newValue;
	}

	CDos::TBigEndianDWord::operator DWORD() const{
		// "getter"
		return MAKELONG(lowWord,highWord);
	}
