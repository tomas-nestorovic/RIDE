#include "stdafx.h"

	static LPCTSTR Recognize(PTCHAR){
		static const char SingleDeviceName[]=_T("DSK (Rev.5)\0");
		return SingleDeviceName;
	}
	PImage CDsk5::Instantiate(LPCTSTR){
		return new CDsk5(&Properties);
	}
	const CImage::TProperties CDsk5::Properties={	MAKE_IMAGE_ID('D','s','k','_','R','e','v','5'), // a unique identifier
													Recognize,	// list of recognized device names
													Instantiate,	// instantiation function
													_T("*.dsk") IMAGE_FORMAT_SEPARATOR _T("*.edsk"), // filter
													Medium::FLOPPY_ANY, // supported Media
													Codec::FLOPPY_ANY, // supported Codecs
													1,2*6144	// Sector supported min and max length
												};

	#define INI_DSK	_T("DSK")

	#define INI_VERSION		_T("ver")
	#define INI_CREATOR		_T("crt")
	#define INI_EMPTY_TRACKS _T("pet")

	CDsk5::TParams::TParams()
		// ctor
		: rev5( app.GetProfileInt(INI_DSK,INI_VERSION,1)!=0 )
		, preserveEmptyTracks( app.GetProfileIntA(INI_DSK,INI_EMPTY_TRACKS,0)!=0 ) {
	}

	#define STD_HEADER		"MV - CPC" /* suffices to recognize "standard" DSK format */
	#define STD_HEADER_LEN	8
	#define REV5_HEADER		"EXTENDED CPC DSK File\r\nDisk-Info\r\n"

	CDsk5::TDiskInfo::TDiskInfo(const TParams &rParams){
		// ctor
		::ZeroMemory(this,sizeof(*this));
		::lstrcpyA( header, rParams.rev5?REV5_HEADER:STD_HEADER );
		::strncpy(	creator,
					app.GetProfileString( INI_DSK, INI_CREATOR, APP_ABBREVIATION _T(" ") APP_VERSION),
					sizeof(creator)
				);
		nHeads=2;
	}





	CDsk5::CDsk5(PCProperties properties)
		// ctor
		: CFloppyImage(properties,true)
		, diskInfo(params) {
		::ZeroMemory(tracks,sizeof(tracks));
		#ifdef _DEBUG
		::ZeroMemory(tracksDebug,sizeof(tracksDebug));
		#endif
	}

	CDsk5::~CDsk5(){
		// dtor
		__freeAllTracks__();
	}






	bool CDsk5::TSectorInfo::operator==(const TSectorId &rSectorId) const{
		// True <=> SectorIDs are equal, otherwise False
		return	cylinderNumber==rSectorId.cylinder
				&&
				sideNumber==rSectorId.side
				&&
				sectorNumber==rSectorId.sector
				&&
				sectorLengthCode==rSectorId.lengthCode;
	}

	bool CDsk5::TSectorInfo::__hasValidSectorLengthCode__() const{
		// True <=> SectorLengthCode complies with Simon Owen's recommendation (interval 0..7), otherwise False
		return (sectorLengthCode&0xf8)==0;
	}

	#define	TRACKINFO_HEADER "Track-Info\r\n"

	bool CDsk5::TTrackInfo::__readAndValidate__(CFile &f){
		// True <=> valid TrackInfo read from the given File, otherwise False
		if (f.Read(this,sizeof(*this))<sizeof(*this)) return false;
		if (::strncmp(header,TRACKINFO_HEADER,sizeof(header))) return false;
		if (nSectors>DSK_TRACKINFO_SECTORS_MAX) return false;
		return true;
	}









	CDsk5::PTrackInfo CDsk5::__findTrack__(TCylinder cyl,THead head) const{
		// finds and returns given Track (returns Null if Track not formatted or not found)
		if (cyl>=diskInfo.nCylinders || head>=diskInfo.nHeads) return nullptr;
		const WORD w=cyl*diskInfo.nHeads+head;
		return w<DSK_REV5_TRACKS_MAX ? tracks[w] : nullptr;
	}

	WORD CDsk5::__getSectorLength__(const TSectorInfo *si) const{
		// determines and returns Sector length given its LengthCode and/or reported SectorLength
		return	params.rev5
				? si->rev5_sectorLength+0x7f & 0xff80 // rounding up to whole multiples of 128
				: GetUsableSectorLength(si->sectorLengthCode);
	}

	WORD CDsk5::__getTrackLength256__(const TTrackInfo *ti) const{
		// determines and returns Track length rounded up to whole multiples of 256
		WORD nBytesOfTrack=255; // to round up to whole multiples of 256
		const TSectorInfo *si=ti->sectorInfo;
		for( TSector n=ti->nSectors; n--; nBytesOfTrack+=__getSectorLength__(si++) );
		return nBytesOfTrack&0xff00;
	}

	void CDsk5::__freeAllTracks__(){
		// disposes all Tracks
		const bool pet0=params.preserveEmptyTracks;
		params.preserveEmptyTracks=false;
			while (diskInfo.nCylinders>0)
				for( THead head=diskInfo.nHeads; head; UnformatTrack(diskInfo.nCylinders-1,--head) );
		params.preserveEmptyTracks=pet0;
	}








	BOOL CDsk5::OnOpenDocument(LPCTSTR lpszPathName){
		// True <=> Image opened successfully, otherwise False
		CFile f;
		if (!OpenImageForReading(lpszPathName,&f)) return FALSE;
		// - if data shorter than an empty Image, resetting to empty Image
		const WORD nDiskInfoBytesRead=f.Read(&diskInfo,sizeof(TDiskInfo));
		if (!nDiskInfoBytesRead){
			Reset(); // to correctly initialize using current Parameters
			return TRUE;
		}else if (nDiskInfoBytesRead<sizeof(TDiskInfo)){
formatError: ::SetLastError(ERROR_BAD_FORMAT);
			return FALSE;
		}
		// - reading content of the Image file and continuously validating its structure
		PTrackInfo *ppti=tracks;
		if ( params.rev5=!::strncmp(diskInfo.header,REV5_HEADER,sizeof(diskInfo.header)) ){
			// DSK Revision 5
			if (diskInfo.nCylinders*diskInfo.nHeads>DSK_REV5_TRACKS_MAX) goto formatError;
			for( BYTE nTracks=DSK_REV5_TRACKS_MAX,*pOffset256=diskInfo.rev5_trackOffsets256; nTracks--; pOffset256++,ppti++ )
				if (*pOffset256){
					// . validating the TrackInfo structure
					TTrackInfo ti;
					if (!ti.__readAndValidate__(f)) goto formatError;
					// . validating the Offset
					const WORD trackLength=__getTrackLength256__(&ti);
					if (*pOffset256!=(sizeof(ti)+trackLength>>8)) goto formatError;
					// . reading Track data
					*( *ppti=(PTrackInfo)::malloc(sizeof(ti)+trackLength) )=ti;
					if (f.Read(*ppti+1,trackLength)<trackLength) goto formatError;
				}
		}else if (!::strncmp(diskInfo.header,STD_HEADER,STD_HEADER_LEN))
			// standard DSK
			for( TCylinder cyl=diskInfo.nCylinders; cyl--; )
				for( THead head=diskInfo.nHeads; head--; ppti++ ){
					// . validating the TrackInfo structure
					TTrackInfo ti;
					if (!ti.__readAndValidate__(f)) goto formatError;
					const TSectorInfo *psi=ti.sectorInfo;
					for( TSector n=ti.nSectors; n--; psi++ )
						if (psi->__hasValidSectorLengthCode__() && psi->sectorLengthCode>ti.std_sectorMaxLengthCode) goto formatError;
					// . reading Track data
					*( *ppti=(PTrackInfo)::malloc(sizeof(ti)+diskInfo.std_trackLength) )=ti;
					if (f.Read(*ppti+1,diskInfo.std_trackLength-sizeof(ti))<diskInfo.std_trackLength-sizeof(ti)) goto formatError;
				}
		else
			// unknown DSK variant
			goto formatError;
		return TRUE;
	}

	TStdWinError CDsk5::SaveAllModifiedTracks(LPCTSTR lpszPathName,PBackgroundActionCancelable pAction){
		// saves all Modified Tracks; returns Windows standard i/o error
		#ifdef _DEBUG
			for( BYTE t=DSK_REV5_TRACKS_MAX; t--; )
				if (tracks[t] && tracksDebug[t]){ // Track exists and read
					const PTrackInfo ti=tracks[t];
					PCSectorData dataStart=(PCSectorData)(ti+1);
					const TSectorInfo *si=ti->sectorInfo;
					TSectorDebug *&td=tracksDebug[t];
					const TSectorDebug *sectorsDebug=td;
					for( BYTE i=0; i<ti->nSectors; i++,dataStart+=__getSectorLength__(si++) )
						if (!sectorsDebug[i].modified // Sector not marked Modified ...
							&&
							sectorsDebug[i].crc16!=GetCrc16Ccitt( dataStart, __getSectorLength__(si) ) // ... but its data show different CRC
						){
							TCHAR buf[200];
							const TSectorId id={ si->cylinderNumber, si->sideNumber, si->sectorNumber, si->sectorLengthCode };
							::wsprintf( buf, _T("Track %d: Sector with %s not marked as modified!"), t, (LPCTSTR)id.ToString() );
							Utils::FatalError(buf);
							return ERROR_GEN_FAILURE;
						}
					::free(td), td=nullptr;
				}
			if (!Utils::QuestionYesNo( _T("All modified sectors successfully marked as \"dirty\".\n\nSave the disk now?"), MB_DEFBUTTON2 ))
				return ERROR_CANCELLED;
		#endif
		CFile f;
		if (!OpenImageForWriting(lpszPathName,&f)) return ERROR_GEN_FAILURE;
		f.Write(&diskInfo,sizeof(TDiskInfo));
		const PTrackInfo *ppti=tracks;
		if (params.rev5){
			// DSK Revision 5
			pAction->SetProgressTarget( DSK_REV5_TRACKS_MAX );
			for( BYTE track=0,*pOffset256=diskInfo.rev5_trackOffsets256; track<DSK_REV5_TRACKS_MAX; pOffset256++,ppti++,pAction->UpdateProgress(++track) )
				if (pAction->IsCancelled())
					return ERROR_CANCELLED;
				else if (const BYTE tmp=*pOffset256)
					f.Write(*ppti,tmp<<8);
		}else{
			// standard DSK
			bool mayLeadToIncompatibilityIssues=false; // assumption (none Track contains assets that could potentially lead to unreadability in randomly selected emulator)
			for( TCylinder cyl=0; cyl<diskInfo.nCylinders; pAction->UpdateProgress(++cyl) )
				for( THead head=diskInfo.nHeads; head--; ){
					const TTrackInfo *const ti=*ppti++;
					WORD trackLength=sizeof(TTrackInfo)+__getTrackLength256__(ti);
					f.Write( ti, trackLength );
					if (trackLength<diskInfo.std_trackLength){
						for( static const BYTE Zero; trackLength++<diskInfo.std_trackLength; f.Write(&Zero,1) );
						mayLeadToIncompatibilityIssues=true;
					}
				}
			if (mayLeadToIncompatibilityIssues)
				Utils::Information(_T("ATTENTION: Some tracks violate the common length or contain various sized sectors - such image may not work in all emulators. You are recommended to save it as \"Revision 5\" DSK (use the Disk -> Dump command)."));
		}
		return ERROR_SUCCESS;
	}

	TCylinder CDsk5::GetCylinderCount() const{
		// determines and returns the actual number of Cylinders in the Image
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return diskInfo.nCylinders;
	}

	THead CDsk5::GetHeadCount() const{
		// determines and returns the actual number of Heads in the Image
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return diskInfo.nHeads;
	}

	TSector CDsk5::ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec,PSectorId bufferId,PWORD bufferLength,PLogTime startTimesNanoseconds,PBYTE pAvgGap3) const{
		// returns the number of Sectors found in given Track, and eventually populates the Buffer with their IDs (if Buffer!=Null); returns 0 if Track not formatted or not found
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (const PTrackInfo ti=__findTrack__(cyl,head)){
			const TSectorInfo *si=ti->sectorInfo;
			for( TSector n=ti->nSectors; n--; si++ ){
				if (bufferId){
					bufferId->cylinder=si->cylinderNumber, bufferId->side=si->sideNumber, bufferId->sector=si->sectorNumber, bufferId->lengthCode=si->sectorLengthCode;
					bufferId++;
				}
				if (bufferLength)
					*bufferLength++=__getSectorLength__(si);
			}
			if (startTimesNanoseconds)
				if (floppyType!=Medium::UNKNOWN || app.IsInGodMode())
					// unsupported DOSes don't set up FloppyType but "some" timing may still be shown in God Mode
					EstimateTrackTiming( cyl, head, ti->nSectors, bufferId-ti->nSectors, bufferLength-ti->nSectors, ti->gap3, startTimesNanoseconds );
				else
					// timing is not applicable for this kind of Image
					for( TSector s=ti->nSectors; s>0; startTimesNanoseconds[--s]=INT_MIN );
			if (pCodec)
				*pCodec=Codec::UNKNOWN;
			if (pAvgGap3)
				*pAvgGap3=ti->gap3;
			return ti->nSectors;
		}else
			return 0;
	}

	void CDsk5::GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses){
		// populates output buffers with specified Sectors' data, usable lengths, and FDC statuses; ALWAYS attempts to buffer all Sectors - caller is then to sort out eventual read errors (by observing the FDC statuses); caller can call ::GetLastError to discover the error for the last Sector in the input list
		ASSERT( outBufferData!=nullptr && outBufferLengths!=nullptr && outFdcStatuses!=nullptr );
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (const PTrackInfo ti=__findTrack__(cyl,head)){
			#ifdef _DEBUG
				if (tracksDebug[cyl*diskInfo.nHeads+head]==nullptr){ // Track read for the first time
					TSectorDebug *const sectorsDebug=(TSectorDebug *)::calloc(ti->nSectors,sizeof(TSectorDebug));
						PCSectorData dataStart=(PCSectorData)(ti+1);
						const TSectorInfo *si=ti->sectorInfo;
						for( BYTE i=0; i<ti->nSectors; i++,dataStart+=__getSectorLength__(si++) ){
							sectorsDebug[i].modified=false;
							sectorsDebug[i].crc16=GetCrc16Ccitt( dataStart, __getSectorLength__(si) );
						}
					tracksDebug[cyl*diskInfo.nHeads+head]=sectorsDebug;
				}
			#endif
			for( const PSectorData trackDataStart=(PSectorData)(ti+1); nSectors-->0; ){
				const TSectorId sectorId=*bufferId++;
				PSectorData result=trackDataStart;
				const TSectorInfo *si=ti->sectorInfo;
				TSector n=ti->nSectors;
				for( BYTE nSectorsToSkip=*bufferNumbersOfSectorsToSkip++; n>0; n--,result+=__getSectorLength__(si++) )
					if (nSectorsToSkip)
						nSectorsToSkip--;
					else if (*si==sectorId) // Sector IDs are equal
						break;
				if (n>0){
					// Sector with given ID found in the Track
					*outBufferData++ =	!( *outFdcStatuses++=si->fdcStatus ).DescribesMissingDam()
										? result
										: nullptr;
					*outBufferLengths++=__getSectorLength__(si);
				}else{
					// Sector with given ID not found in the Track
					*outBufferData++=nullptr, *outFdcStatuses++=TFdcStatus::SectorNotFound;
					outBufferLengths++;
				}
			}
		}else
			while (nSectors-->0)
				*outBufferData++=nullptr, *outFdcStatuses++=TFdcStatus::SectorNotFound;
		::SetLastError( *--outBufferData ? ERROR_SUCCESS : ERROR_SECTOR_NOT_FOUND );
	}

	TStdWinError CDsk5::MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus){
		// marks Sector on a given PhysicalAddress as "dirty", plus sets it the given FdcStatus; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (const PTrackInfo ti=__findTrack__(chs.cylinder,chs.head)){
			PSectorInfo si=ti->sectorInfo;
			for( TSector n=ti->nSectors; n--; si++ )
				if (!nSectorsToSkip){
					if (*si==chs.sectorId){ // Sector IDs are equal
						#ifdef _DEBUG
							tracksDebug[chs.cylinder*diskInfo.nHeads+chs.head][si-ti->sectorInfo].modified=true;
						#endif
						si->fdcStatus=*pFdcStatus;
						m_bModified=TRUE;
						return ERROR_SUCCESS;
					}
				}else
					nSectorsToSkip--;
		}
		return ERROR_SECTOR_NOT_FOUND;
	}

	TStdWinError CDsk5::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - base
		if (const TStdWinError err=__super::SetMediumTypeAndGeometry( pFormat, sideMap, firstSectorNumber ))
			return err;
		// - changes in geometry allowed only if Image is empty
		if (!diskInfo.nCylinders)
			diskInfo.nHeads=pFormat->nHeads;
		return ERROR_SUCCESS;
	}

	bool CDsk5::EditSettings(bool initialEditing){
		// True <=> new settings have been accepted (and adopted by this Image), otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - defining the Dialog
		class CTypeSelectionDialog sealed:public Utils::CRideDialog{
			void DoDataExchange(CDataExchange *pDX) override{
				// exchange of data from and to controls
				// . Type
				int i=rParams.rev5;
				DDX_Radio( pDX, ID_STANDARD, i );
				rParams.rev5=i>0;
				static const WORD Controls[]={ ID_STANDARD, ID_DRIVE, ID_PROTECTED, 0 } ;
				EnableDlgItems( Controls, !readOnly&&allowTypeBeChanged );
				// . Creator
				const BYTE nCyls=rDiskInfo.nCylinders;
				rDiskInfo.nCylinders=0; // converting the Creator field to a null-terminated string
					DDX_Text( pDX, ID_CREATOR, rDiskInfo.creator, sizeof(rDiskInfo.creator)+1 ); // "+1" = terminal Null character
						DDV_MaxChars( pDX, rDiskInfo.creator, sizeof(rDiskInfo.creator) );
					if (!pDX->m_bSaveAndValidate){
						// populating the Creator combo-box with preset names
						CComboBox cb;
						cb.Attach(GetDlgItemHwnd(ID_CREATOR));
							TCHAR buf[80];
							cb.AddString( ::lstrcpyn(buf,APP_ABBREVIATION _T(" ") APP_VERSION,sizeof(rDiskInfo.creator)+1) );
							DWORD dw=sizeof(buf)/sizeof(TCHAR);
							if (::GetUserName(buf,&dw)){
								buf[sizeof(rDiskInfo.creator)]='\0';
								cb.AddString(buf);
							}
						cb.Detach();
					}
				rDiskInfo.nCylinders=nCyls;
				EnableDlgItem( ID_CREATOR, !readOnly );
				// . preservation of empty Tracks
				i=rParams.preserveEmptyTracks;
				DDX_Check( pDX, ID_TRACK, i );
				rParams.preserveEmptyTracks=i!=BST_UNCHECKED;
				EnableDlgItem( ID_TRACK, !readOnly );
			}
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				switch (msg){
					case WM_NOTIFY:
						if (((LPNMHDR)lParam)->code==NM_CLICK){
							// . defining the Dialog
							class CHelpDialog sealed:public Utils::CCommandDialog{
								BOOL OnInitDialog() override{
									// dialog initialization
									// : base
									const BOOL result=__super::OnInitDialog();
									// : supplying available actions
									__addCommandButton__( ID_SIZE, _T("Which version of DSK image should I prefer?") );
									__addCommandButton__( ID_FORMAT, _T("How do I merge two images? What is a \"patch\"?") );
									__addCommandButton__( IDCANCEL, MSG_HELP_CANCEL );
									return result;
								}
							public:
								CHelpDialog()
									// ctor
									: Utils::CCommandDialog(_T("Suitable DSK version may spare you space on disk and burden during emulation.")) {
								}
							} d;
							// . showing the Dialog and processing its result
							TCHAR url[200];
							switch (d.DoModal()){
								case ID_SIZE:
									Utils::NavigateToUrlInDefaultBrowser( Utils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_dsk.html"),url) );
									break;
								case ID_FORMAT:
									Utils::NavigateToUrlInDefaultBrowser( Utils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_patch.html"),url) );
									break;
							}
						}
						break;
				}
				return __super::WindowProc(msg,wParam,lParam);
			}
		public:
			const bool readOnly,allowTypeBeChanged;
			TParams &rParams;
			TDiskInfo &rDiskInfo;

			CTypeSelectionDialog(bool allowTypeBeChanged,CDsk5 &dsk)
				: Utils::CRideDialog(IDR_DSK_TYPE)
				, readOnly(dsk.IsWriteProtected())
				, allowTypeBeChanged(allowTypeBeChanged)
				, rParams(dsk.params) , rDiskInfo(dsk.diskInfo) {
			}
		} d(initialEditing,*this);
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			// . saving settings
			app.WriteProfileInt( INI_DSK, INI_VERSION, params.rev5 );
			app.WriteProfileInt( INI_DSK, INI_EMPTY_TRACKS, params.preserveEmptyTracks );
			const BYTE nCyls=diskInfo.nCylinders;
			diskInfo.nCylinders=0; // converting the Creator field to a null-terminated string
				app.WriteProfileString( INI_DSK, INI_CREATOR, diskInfo.creator );
			diskInfo.nCylinders=nCyls;
			// . marking the Image as "dirty"
			SetModifiedFlag(TRUE);
			return true;
		}else
			return false;
	}

	TStdWinError CDsk5::Reset(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - disposing existing Tracks
		__freeAllTracks__();
		// - reinitializing to an empty Image
		diskInfo=TDiskInfo(params);
		return ERROR_SUCCESS;
	}

	TStdWinError CDsk5::FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte){
		// formats given Track {Cylinder,Head} to the requested NumberOfSectors, each with corresponding Length and FillerByte as initial content; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if ((codec&Properties.supportedCodecs)==0)
			return ERROR_NOT_SUPPORTED;
		if (nSectors>DSK_TRACKINFO_SECTORS_MAX)
			return Utils::ErrorByOs( ERROR_VHD_INVALID_TYPE, ERROR_NOT_SUPPORTED );
		WORD w=cyl*diskInfo.nHeads+head;
		if (w>=DSK_REV5_TRACKS_MAX)
			return Utils::ErrorByOs( ERROR_VHD_INVALID_TYPE, ERROR_NOT_SUPPORTED );
		if (!params.rev5 && cyl>diskInfo.nCylinders)
			return Utils::ErrorByOs( ERROR_VHD_INVALID_TYPE, ERROR_NOT_SUPPORTED ); // only at most one consecutive Track may be formatted at "standard" DSK
		if (!nSectors) // formatting to "no Sectors" ...
			return UnformatTrack(cyl,head); // ... is translated as unformatting the Track
		// - statistics about the new Track
		DWORD trackLength=sizeof(TTrackInfo)+255; // 255 = rouding up to whole multiples of 256
		PCSectorId pId=bufferId; BYTE maxSectorLengthCode=0; PCWORD pLength=bufferLength;
		for( TSector n=nSectors; n--; ){
			TSectorInfo si;
				si.sectorLengthCode=pId++->lengthCode, si.rev5_sectorLength=*pLength++;
			trackLength+=__getSectorLength__(&si);
			if (si.__hasValidSectorLengthCode__())
				if (si.sectorLengthCode>maxSectorLengthCode) maxSectorLengthCode=si.sectorLengthCode;
		}
		trackLength&=0xffffff00; // rounding up to whole multiples of 256
		if (trackLength>0xff80) // 0xff80 = maximum for "standard" DSK, 0xff00 = maximum for Revision 5
			return ERROR_BAD_COMMAND;
		// - if Track already formatted, unformatting it
		if (__findTrack__(cyl,head))
			UnformatTrack(cyl,head);
		// - formatting Track
		do{
			if (const PTrackInfo ti = tracks[w] = (PTrackInfo)::malloc(trackLength)){
				diskInfo.nCylinders=std::max<TCylinder>( diskInfo.nCylinders, 1+cyl ); // updating the NumberOfCylinders
				diskInfo.std_trackLength=std::max<DWORD>( diskInfo.std_trackLength, trackLength );
				diskInfo.rev5_trackOffsets256[w]=trackLength>>8; 
				// . initializing the TrackInfo structure
				::ZeroMemory(ti,sizeof(TTrackInfo));
				::lstrcpyA(ti->header,TRACKINFO_HEADER);
				ti->cylinderNumber=cyl;
				ti->headNumber=head;
				ti->std_sectorMaxLengthCode=maxSectorLengthCode;
				ti->nSectors=nSectors;
				ti->gap3=gap3;
				ti->fillerByte=fillerByte;
				PSectorInfo si=ti->sectorInfo;	pId=bufferId, pLength=bufferLength;	PCFdcStatus pFdcStatus=bufferFdcStatus;
				for( TSector n=nSectors; n--; si++,pId++ ){
					si->cylinderNumber=pId->cylinder, si->sideNumber=pId->side, si->sectorNumber=pId->sector;
					si->sectorLengthCode=pId->lengthCode, si->rev5_sectorLength=*pLength++;
					si->fdcStatus=*pFdcStatus++;
				}
				// . initializing the content with the FillerByte
				::memset( ti+1, fillerByte, trackLength-sizeof(TTrackInfo) );
			}else
				return ERROR_NOT_ENOUGH_MEMORY;
			w++;
		}while (!params.rev5 && ++head<diskInfo.nHeads); // for "standard" DSK, formatting all Tracks on the Cylinder
		m_bModified=TRUE;
		return ERROR_SUCCESS;
	}

	TStdWinError CDsk5::UnformatTrack(TCylinder cyl,THead head){
		// unformats given Track {Cylinder,Head}; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (head<diskInfo.nHeads && (params.rev5||cyl==diskInfo.nCylinders-1)){
			// A&(B|C); A = existing Head, B|C = existing Cylinder
			// . redimensioning the Image
			if (const PTrackInfo ti=__findTrack__(cyl,head)){
				diskInfo.rev5_trackOffsets256[cyl*diskInfo.nHeads+head]=0;
				::free(ti), tracks[cyl*diskInfo.nHeads+head]=nullptr;
				#ifdef _DEBUG
					if (TSectorDebug *&td=tracksDebug[cyl*diskInfo.nHeads+head])
						::free(td), td=nullptr;
				#endif
			}
			// . updating the NumberOfCylinders
			if (params.preserveEmptyTracks)
				diskInfo.nCylinders=std::max<BYTE>( 1+cyl, diskInfo.nCylinders );
			else{
				for( cyl=diskInfo.nCylinders; cyl--; )
					if (__findTrack__(cyl,0)!=__findTrack__(cyl,1)) // equal only if both Sides are Null
						break;
				diskInfo.nCylinders=1+cyl;
			}
			m_bModified=TRUE;
			return ERROR_SUCCESS;
		}else
			return ERROR_BAD_COMMAND;
	}












	#define DEVICE_COUNT		2
	#define DEVICE_NAME_PATTERN	_T("Dummy DSK drive %c")

	LPCTSTR CDsk5::CDummyDevice::Recognize(PTCHAR deviceNameList){
		// returns a null-separated list of dummy floppy drives
		// - composing a list of dummy floppy drives
		PTCHAR p=deviceNameList;
		for( BYTE fddId=0; fddId<DEVICE_COUNT; fddId++ )
			p+=::wsprintf( p, DEVICE_NAME_PATTERN, fddId+'0' )+1; // "+1" = null-terminated items
		// - no further access possibilities
		*p='\0'; // null-terminated list
		return deviceNameList;
	}

	PImage CDsk5::CDummyDevice::Instantiate(LPCTSTR deviceName){
		// creates and returns a KryoFluxDevice instance for a specified floppy drive
		TCHAR fddId;
		::sscanf( deviceName, DEVICE_NAME_PATTERN, &fddId );
		if (fddId<'0'+DEVICE_COUNT)
			return new CDsk5(&Properties);
		ASSERT(FALSE);
		::SetLastError( ERROR_BAD_DEVICE );
		return nullptr;
	}

	const CImage::TProperties CDsk5::CDummyDevice::Properties={
		MAKE_IMAGE_ID('D','s','k','D','u','m','m','y'), // a unique identifier
		Recognize,	// list of recognized device names
		Instantiate,	// instantiation function
		nullptr, // filter
		Medium::FLOPPY_ANY, // supported Media
		Codec::FLOPPY_ANY, // supported Codecs
		1,2*6144	// Sector supported min and max length
	};
