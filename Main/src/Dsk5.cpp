#include "stdafx.h"

	static PImage __instantiate__(){
		return new CDsk5;
	}
	const CImage::TProperties CDsk5::Properties={	_T("DSK (Rev.5)"),	// name
													__instantiate__,	// instantiation function
													_T("*.dsk"), // filter
													TMedium::FLOPPY_ANY, // supported Media
													1,2*6144	// Sector supported min and max length
												};

	#define INI_DSK	_T("DSK")

	#define INI_VERSION		_T("ver")
	#define INI_CREATOR		_T("crt")

	CDsk5::TParams::TParams()
		// ctor
		: rev5( app.GetProfileInt(INI_DSK,INI_VERSION,1) ) {
	}

	#define STD_HEADER		"MV - CPC" /* suffices to recognize "standard" DSK format */
	#define STD_HEADER_LEN	8
	#define REV5_HEADER		"EXTENDED CPC DSK File\r\nDisk-Info\r\n"

	CDsk5::TDiskInfo::TDiskInfo(const TParams &rParams){
		// ctor
		::ZeroMemory(this,sizeof(*this));
		::lstrcpyA( header, rParams.rev5?REV5_HEADER:STD_HEADER );
		::strncpy(	creator,
					app.GetProfileString( INI_DSK, INI_CREATOR, _T("RIDE ") APP_VERSION),
					sizeof(creator)
				);
		nHeads=2;
	}





	CDsk5::CDsk5()
		// ctor
		: CFloppyImage(&Properties)
		, diskInfo(params) {
		::ZeroMemory(tracks,sizeof(tracks));
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
		if (cyl>=diskInfo.nCylinders || head>=diskInfo.nHeads) return NULL;
		const WORD w=cyl*diskInfo.nHeads+head;
		return w<DSK_REV5_TRACKS_MAX ? tracks[w] : NULL;
	}

	WORD CDsk5::__getSectorLength__(const TSectorInfo *si) const{
		// determines and returns Sector length given its LengthCode and/or reported SectorLength
		if (params.rev5)
			return si->rev5_sectorLength+0x7f & 0xff80; // rounding up to whole multiples of 128
		else
			return	si->__hasValidSectorLengthCode__() // if Code valid ...
					? __getUsableSectorLength__(si->sectorLengthCode) // ... it's possible to determine UsableLength (e.g. for 2DD floppy 0..6144 Bytes)
					: 0; // ... otherwise zero length
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
		while (diskInfo.nCylinders--)
			for( THead head=diskInfo.nHeads; head; UnformatTrack(diskInfo.nCylinders,--head) );
	}








	BOOL CDsk5::OnOpenDocument(LPCTSTR lpszPathName){
		// True <=> Image opened successfully, otherwise False
		CFile f;
		if (!__openImageForReading__(lpszPathName,&f)) return FALSE;
		// - if data shorter than an empty Image, resetting to empty Image
		const WORD nDiskInfoBytesRead=f.Read(&diskInfo,sizeof(TDiskInfo));
		if (!nDiskInfoBytesRead){
			__reset__(); // to correctly initialize using current Parameters
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

	BOOL CDsk5::OnSaveDocument(LPCTSTR lpszPathName){
		// True <=> this Image has been successfully saved, otherwise False
		CFile f;
		if (!__openImageForWriting__(lpszPathName,&f)) return FALSE;
		f.Write(&diskInfo,sizeof(TDiskInfo));
		const PTrackInfo *ppti=tracks;
		if (params.rev5){
			// DSK Revision 5
			for( BYTE nTracks=DSK_REV5_TRACKS_MAX,*pOffset256=diskInfo.rev5_trackOffsets256; nTracks--; pOffset256++,ppti++ )
				if (const BYTE tmp=*pOffset256)
					f.Write(*ppti,tmp<<8);
		}else{
			// standard DSK
			bool mayLeadToIncompatibilityIssues=false; // assumption (none Track contains assets that could potentially lead to unreadability in randomly selected emulator)
			for( TCylinder cyl=diskInfo.nCylinders; cyl--; )
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
				TUtils::Information(_T("ATTENTION: Some tracks violate the common length or contain various sized sectors - such image may not work in all emulators. You are recommended to save it as \"Revision 5\" DSK (use the Image -> Dump command)."));
		}
		m_bModified=FALSE;
		return ::GetLastError()==ERROR_SUCCESS;
	}

	TCylinder CDsk5::GetCylinderCount() const{
		// determines and returns the actual number of Cylinders in the Image
		return diskInfo.nCylinders;
	}

	THead CDsk5::GetNumberOfFormattedSides(TCylinder cyl) const{
		// determines and returns the number of Sides formatted on given Cylinder; returns 0 iff Cylinder not formatted
		return diskInfo.nHeads;
	}

	TSector CDsk5::ScanTrack(TCylinder cyl,THead head,PSectorId bufferId,PWORD bufferLength) const{
		// returns the number of Sectors found in given Track, and eventually populates the Buffer with their IDs (if Buffer!=Null); returns 0 if Track not formatted or not found
		if (const PTrackInfo ti=__findTrack__(cyl,head)){
			if (bufferId){
				const TSectorInfo *si=ti->sectorInfo;
				for( TSector n=ti->nSectors; n--; bufferId++,*bufferLength++=__getSectorLength__(si++) )
					bufferId->cylinder=si->cylinderNumber, bufferId->side=si->sideNumber, bufferId->sector=si->sectorNumber, bufferId->lengthCode=si->sectorLengthCode;
			}
			return ti->nSectors;
		}else
			return 0;
	}

	PSectorData CDsk5::GetSectorData(RCPhysicalAddress chs,BYTE nSectorsToSkip,bool,PWORD sectorLength,TFdcStatus *pFdcStatus){
		// returns Data of a Sector on a given PhysicalAddress; returns Null iff Sector not found or Track not formatted
		if (const PTrackInfo ti=__findTrack__(chs.cylinder,chs.head)){
			PSectorData result=(PSectorData)(ti+1);
			const TSectorInfo *si=ti->sectorInfo;
			for( TSector n=ti->nSectors; n--; result+=__getSectorLength__(si++) )
				if (!nSectorsToSkip){ 
					if (*si==chs.sectorId){ // Sector IDs are equal
						*sectorLength=__getSectorLength__(si);
						return	!( *pFdcStatus=si->fdcStatus ).DescribesMissingDam()
								? result
								: NULL;
					}
				}else
					nSectorsToSkip--;
		}
		::SetLastError(ERROR_SECTOR_NOT_FOUND);
		return NULL;
	}

	TStdWinError CDsk5::MarkSectorAsDirty(RCPhysicalAddress chs,BYTE nSectorsToSkip,PCFdcStatus pFdcStatus){
		// marks Sector on a given PhysicalAddress as "dirty", plus sets it the given FdcStatus; returns Windows standard i/o error
		if (const PTrackInfo ti=__findTrack__(chs.cylinder,chs.head)){
			PSectorInfo si=ti->sectorInfo;
			for( TSector n=ti->nSectors; n--; si++ )
				if (!nSectorsToSkip){
					if (*si==chs.sectorId){ // Sector IDs are equal
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
		// - base
		const TStdWinError err=CFloppyImage::SetMediumTypeAndGeometry(pFormat,sideMap,firstSectorNumber);
		if (err!=ERROR_SUCCESS)
			return err;
		// - changes in geometry allowed only if Image is empty
		if (!diskInfo.nCylinders)
			diskInfo.nHeads=pFormat->nHeads;
		return ERROR_SUCCESS;
	}

	TStdWinError CDsk5::__reset__(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		// - disposing existing Tracks
		__freeAllTracks__();
		// - reinitializing to an empty Image
		diskInfo=TDiskInfo(params);
		return ERROR_SUCCESS;
	}

	bool CDsk5::__showOptions__(bool allowTypeBeChanged){
		// True <=> Parameters and/or DiskInfo have changed, otherwise False
		// - defining the Dialog
		class CTypeSelectionDialog sealed:public CDialog{
			void DoDataExchange(CDataExchange *pDX) override{
				// exchange of data from and to controls
				// . Type
				int i=rParams.rev5;
				DDX_Radio( pDX, ID_STANDARD, i );
				rParams.rev5=i>0;
				GetDlgItem(ID_STANDARD)->EnableWindow(allowTypeBeChanged);
				GetDlgItem(ID_DRIVE)->EnableWindow(allowTypeBeChanged);
				GetDlgItem(ID_PROTECTED)->EnableWindow(allowTypeBeChanged);
				// . Creator
				const BYTE nCyls=rDiskInfo.nCylinders;
				rDiskInfo.nCylinders=0; // converting the Creator field to a null-terminated string
				DDX_Text( pDX, ID_CREATOR, rDiskInfo.creator, sizeof(rDiskInfo.creator)+1 ); // "+1" = terminal Null character
					DDV_MaxChars( pDX, rDiskInfo.creator, sizeof(rDiskInfo.creator) );
				rDiskInfo.nCylinders=nCyls;
			}
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				switch (msg){
					case WM_NOTIFY:
						if (((LPNMHDR)lParam)->code==NM_CLICK){
							// . defining the Dialog
							class CHelpDialog sealed:public TUtils::CCommandDialog{
								void PreInitDialog() override{
									// dialog initialization
									// : base
									TUtils::CCommandDialog::PreInitDialog();
									// : supplying available actions
									__addCommandButton__( ID_SIZE, _T("Which version of DSK image should I prefer?") );
									__addCommandButton__( ID_FORMAT, _T("How do I merge two images? What is a \"patch\"?") );
									__addCommandButton__( IDCANCEL, MSG_HELP_CANCEL );
								}
							public:
								CHelpDialog()
									// ctor
									: TUtils::CCommandDialog(_T("Suitable DSK version may spare you space on disk and burden during emulation.")) {
								}
							} d;
							// . showing the Dialog and processing its result
							TCHAR url[200];
							switch (d.DoModal()){
								case ID_SIZE:
									TUtils::NavigateToUrlInDefaultBrowser( TUtils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_dsk.html"),url) );
									break;
								case ID_FORMAT:
									TUtils::NavigateToUrlInDefaultBrowser( TUtils::GetApplicationOnlineHtmlDocumentUrl(_T("faq_patch.html"),url) );
									break;
							}
						}
						break;
				}
				return CDialog::WindowProc(msg,wParam,lParam);
			}
		public:
			const bool allowTypeBeChanged;
			TParams &rParams;
			TDiskInfo &rDiskInfo;

			CTypeSelectionDialog(bool allowTypeBeChanged,TParams &rParams,TDiskInfo &rDiskInfo)
				: CDialog(IDR_DSK_TYPE)
				, allowTypeBeChanged(allowTypeBeChanged)
				, rParams(rParams) , rDiskInfo(rDiskInfo) {
			}
		} d(allowTypeBeChanged,params,diskInfo);
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			SetModifiedFlag(TRUE);
			return true;
		}else
			return false;
	}

	TStdWinError CDsk5::Reset(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		if (__showOptions__(true)){
			// . saving settings
			app.WriteProfileInt( INI_DSK, INI_VERSION, params.rev5 );
			const BYTE nCyls=diskInfo.nCylinders;
			diskInfo.nCylinders=0; // converting the Creator field to a null-terminated string
				app.WriteProfileString( INI_DSK, INI_CREATOR, diskInfo.creator );
			diskInfo.nCylinders=nCyls;
			// . resetting this Image using the above parameters
			return __reset__();
		}else
			return ERROR_CANCELLED;
	}

	TStdWinError CDsk5::FormatTrack(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte){
		// formats given Track {Cylinder,Head} to the requested NumberOfSectors, each with corresponding Length and FillerByte as initial content; returns Windows standard i/o error
		if (nSectors>DSK_TRACKINFO_SECTORS_MAX) return ERROR_BAD_COMMAND;
		WORD w=cyl*diskInfo.nHeads+head;
		if (w>=DSK_REV5_TRACKS_MAX)
			return ERROR_BAD_COMMAND;
		if (!params.rev5 && cyl>diskInfo.nCylinders)
			return ERROR_BAD_COMMAND; // only at most one consecutive Track may be formatted at "standard" DSK
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
				diskInfo.nCylinders=max( diskInfo.nCylinders, 1+cyl ); // updating the NumberOfCylinders
				diskInfo.std_trackLength=max( diskInfo.std_trackLength, trackLength );
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
		if (head<diskInfo.nHeads && (params.rev5||cyl==diskInfo.nCylinders-1)){
			// A&(B|C); A = existing Head, B|C = existing Cylinder
			// . redimensioning the Image
			if (const PTrackInfo ti=__findTrack__(cyl,head)){
				diskInfo.rev5_trackOffsets256[cyl*diskInfo.nHeads+head]=0;
				::free(ti), tracks[cyl*diskInfo.nHeads+head]=NULL;
			}
			// . updating the NumberOfCylinders
			for( cyl=diskInfo.nCylinders; cyl--; )
				if (__findTrack__(cyl,0)!=__findTrack__(cyl,1)){ // equal only if both Sides are Null
					diskInfo.nCylinders=1+cyl;
					break;
				}
			m_bModified=TRUE;
			return ERROR_SUCCESS;
		}else
			return ERROR_BAD_COMMAND;
	}

	BOOL CDsk5::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:
				// update
				switch (nID){
					case ID_IMAGE_SETTINGS:
						((CCmdUI *)pExtra)->Enable(TRUE);
						return TRUE;
				}
				break;
			case CN_COMMAND:
				// command
				switch (nID){
					case ID_IMAGE_SETTINGS:
						__showOptions__(false); // allowing changes in everything but Type of DSK Image (Std vs Rev5)
						return TRUE;
				}
				break;
		}
		return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	}
