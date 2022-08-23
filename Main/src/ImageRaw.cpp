#include "stdafx.h"

	static LPCTSTR Recognize(PTCHAR){
		static constexpr TCHAR SingleDeviceName[]=_T("Raw data image\0");
		return SingleDeviceName;
	}
	static PImage Instantiate(LPCTSTR){
		return new CImageRaw( &CImageRaw::Properties, true );
	}

	const CImage::TProperties CImageRaw::Properties={
		MAKE_IMAGE_ID('I','m','a','g','e','R','a','w'), // a unique identifier
		Recognize,	// list of recognized device names
		Instantiate, // instantiation function
		_T("*.ima") IMAGE_FORMAT_SEPARATOR _T("*.img") IMAGE_FORMAT_SEPARATOR _T("*.dat") IMAGE_FORMAT_SEPARATOR _T("*.bin") IMAGE_FORMAT_SEPARATOR _T("*.adf"),	// filter
		(Medium::TType)(Medium::FLOPPY_ANY | Medium::HDD_RAW), // supported Media
		Codec::ANY, // supported Codecs
		1,16384	// Sector supported min and max length
	};

	CImageRaw::CImageRaw(PCProperties properties,bool hasEditableSettings)
		// ctor
		: CImage(properties,hasEditableSettings)
		, trackAccessScheme(TTrackScheme::BY_CYLINDERS)
		, nCylinders(0) , nSectors(0) // = not initialized - see SetMediumTypeAndGeometry
		, sizeWithoutGeometry(0) {
		Reset(); // to be correctly initialized
	}

	CImageRaw::~CImageRaw(){
		// dtor
		__freeBufferOfCylinders__();
	}






	TStdWinError CImageRaw::ExtendToNumberOfCylinders(TCylinder nCyl,BYTE fillerByte,const volatile bool &cancelled){
		// formats new Cylinders to meet the minimum number requested; returns Windows standard i/o error
		// - redimensioning the Image
		if (!bufferOfCylinders.Realloc(nCyl))
			return ERROR_NOT_ENOUGH_MEMORY;
		// - initializing added Cylinders with the FillerByte
		for( const DWORD nBytesOfCylinder=nHeads*nSectors*sectorLength; nCylinders<nCyl; )
			if (cancelled)
				return ERROR_CANCELLED;
			else if (const PVOID tmp=bufferOfCylinders[nCylinders]=::malloc(nBytesOfCylinder)){
				::memset( tmp, fillerByte, nBytesOfCylinder );
				nCylinders++;
			}else
				return ERROR_NOT_ENOUGH_MEMORY;
		return ERROR_SUCCESS;
	}

	void CImageRaw::__freeCylinder__(TCylinder cyl){
		// disposes (unformats) the specified Cylinder (if previously formatted)
		if (bufferOfCylinders)
			if (const PVOID p=bufferOfCylinders[cyl]) // Cylinder formatted
				::free(p), bufferOfCylinders[cyl]=nullptr;
	}

	void CImageRaw::__freeBufferOfCylinders__(){
		// disposes (unformats) all Cylinders
		if (bufferOfCylinders){
			while (nCylinders)
				__freeCylinder__( --nCylinders );
			bufferOfCylinders.reset();
		}
	}

	bool CImageRaw::IsKnownSector(TCylinder cyl,THead head,RCSectorId id) const{
		//
		return	id.cylinder==cyl
				&&
				id.side==sideMap[head]
				&&
				id.sector>=firstSectorNumber && id.sector-firstSectorNumber<nSectors && id.lengthCode==sectorLengthCode;
	}

	PSectorData CImageRaw::__getBufferedSectorData__(TCylinder cyl,THead head,PCSectorId sectorId) const{
		// finds and returns buffered data of given Sector (or Null if not yet buffered; note that returning Null does NOT imply that the Sector doesn't exist in corresponding Track!)
		if (const PSectorData cylinderData=(PSectorData)bufferOfCylinders[cyl])
			return (PSectorData)cylinderData+(head*nSectors+sectorId->sector-firstSectorNumber)*sectorLength;
		return nullptr;
	}

	BOOL CImageRaw::OnOpenDocument(LPCTSTR lpszPathName){
		// True <=> Image opened successfully, otherwise False
		// - opening
		if (!OpenImageForReadingAndWriting(lpszPathName,f,true)) // if cannot open for both reading and writing ...
			if (!OpenImageForReading(lpszPathName,f)) // ... trying to open at least for reading, and if neither this works ...
				return FALSE; // ... the Image cannot be open in any way
			else
				canBeModified=false;
		// - currently without geometry (DOS must call SetMediumTypeAndGeometry)
		if ( sizeWithoutGeometry=f.GetLength() )
			nCylinders=1, nHeads=1, nSectors=1, sectorLengthCode=GetSectorLengthCode( sectorLength=std::min<DWORD>(sizeWithoutGeometry,(WORD)-1) );
		// - confirming initial settings
		if (!EditSettings(true)){ // dialog cancelled?
			::SetLastError( ERROR_CANCELLED );
			return FALSE;
		}
		return TRUE;
	}

	void CImageRaw::__saveTrackToCurrentPositionInFile__(CFile *pfOtherThanCurrentFile,TPhysicalAddress chs){
		// saves Track defined by the {Cylinder,Head} pair in PhysicalAddress to the current position in the open file; after saving, the position in the file will advance immediately "after" the just saved Track
		for( TSector s=0; s<nSectors; s++ ){
			chs.sectorId.sector=firstSectorNumber+s;
			const PCSectorData bufferedData=__getBufferedSectorData__(chs.cylinder,chs.head,&chs.sectorId);
			if (!pfOtherThanCurrentFile){
				// saving to Image's underlying file, currently open
				if (bufferedData)
					f.Write(bufferedData,sectorLength); // data modified - writing them to file
				else
					f.Seek(sectorLength,CFile::current); // data not modified - skipping them in file
			}else
				// saving to other than Image's underlying file
				if (bufferedData){
					pfOtherThanCurrentFile->Write(bufferedData,sectorLength);
					if (f.m_hFile!=CFile::hFileNull) // handle doesn't exist if creating a new Image
						f.Seek(sectorLength,CFile::current);
				}else{
					BYTE buffer[(WORD)-1+1];
					pfOtherThanCurrentFile->Write( buffer, f.Read(buffer,sectorLength) );
				}
		}
	}

	TStdWinError CImageRaw::SaveAllModifiedTracks(LPCTSTR lpszPathName,CActionProgress &ap){
		// saves all Modified Tracks; returns Windows standard i/o error
		// - saving
		CFile fTmp;
		const bool savingToCurrentFile= lpszPathName==f.GetFilePath() && ::GetFileAttributes(lpszPathName)!=INVALID_FILE_ATTRIBUTES; // saving to the same file and that file exists
		if (!savingToCurrentFile && !CreateImageForWriting(lpszPathName,fTmp))
			return ERROR_GEN_FAILURE;
		if (f.m_hFile!=CFile::hFileNull) // handle doesn't exist when creating new Image
			f.SeekToBegin();
		ap.SetProgressTarget( nCylinders*nHeads );
		TPhysicalAddress chs;
			chs.sectorId.lengthCode=sectorLengthCode;
		switch (trackAccessScheme){
			case TTrackScheme::BY_CYLINDERS:
				for( chs.cylinder=0; chs.cylinder<nCylinders; chs.cylinder++ )
					for( chs.sectorId.cylinder=chs.cylinder,chs.head=0; chs.head<nHeads; chs.head++,ap.IncrementProgress() ){
						chs.sectorId.side=sideMap[chs.head];
						__saveTrackToCurrentPositionInFile__( savingToCurrentFile?nullptr:&fTmp, chs );
					}
				break;
			case TTrackScheme::BY_SIDES:
				for( chs.head=0; chs.head<nHeads; chs.head++ )
					for( chs.sectorId.side=sideMap[chs.head],chs.cylinder=0; chs.cylinder<nCylinders; chs.cylinder++,ap.IncrementProgress() ){
						chs.sectorId.cylinder=chs.cylinder;
						__saveTrackToCurrentPositionInFile__( savingToCurrentFile?nullptr:&fTmp, chs );
					}
				break;
			default:
				ASSERT(FALSE);
		}
		m_bModified=FALSE;
		// - reopening Image's underlying file
		if (f.m_hFile!=CFile::hFileNull){
			if (savingToCurrentFile)
				f.SetLength(f.GetPosition()); // "trimming" eventual unnecessary data (e.g. when unformatting Cylinders)
			f.Close();
		}
		if (fTmp.m_hFile!=CFile::hFileNull)
			fTmp.Close();
		return OpenImageForReadingAndWriting(lpszPathName,f,true) ? ERROR_SUCCESS : ERROR_GEN_FAILURE;
	}

	TCylinder CImageRaw::GetCylinderCount() const{
		// determines and returns the actual number of Cylinders in the Image
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return nCylinders;
	}

	THead CImageRaw::GetHeadCount() const{
		// determines and returns the number of Sides formatted on given Cylinder; returns 0 iff Cylinder not formatted
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return nCylinders>0 || explicitSides ? nHeads : 0;
	}

	TSector CImageRaw::ScanTrack(TCylinder cyl,THead head,Codec::PType pCodec,PSectorId bufferId,PWORD bufferLength,PLogTime startTimesNanoseconds,PBYTE pAvgGap3) const{
		// returns the number of Sectors found in given Track, and eventually populates the Buffer with their IDs (if Buffer!=Null); returns 0 if Track not formatted or not found
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (cyl<nCylinders && head<nHeads){
			for( TSector n=0; n<nSectors; n++ ){
				if (bufferId){
					bufferId->cylinder=cyl, bufferId->side=sideMap[head], bufferId->sector=firstSectorNumber+n, bufferId->lengthCode=sectorLengthCode;
					bufferId++;
				}
				if (bufferLength)
					*bufferLength++=sectorLength;
				if (startTimesNanoseconds)
					*startTimesNanoseconds++=INT_MIN; // timing is not applicable for this kind of Image
			}
			if (pCodec)
				*pCodec=Codec::ANY;
			if (pAvgGap3)
				*pAvgGap3=FDD_350_SECTOR_GAP3;
			return nSectors;
		}else
			return 0;
	}

	bool CImageRaw::IsTrackScanned(TCylinder cyl,THead head) const{
		// True <=> Track exists and has already been scanned, otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return	cyl<nCylinders && head<nHeads;
	}

	void CImageRaw::GetTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses){
		// populates output buffers with specified Sectors' data, usable lengths, and FDC statuses; ALWAYS attempts to buffer all Sectors - caller is then to sort out eventual read errors (by observing the FDC statuses); caller can call ::GetLastError to discover the error for the last Sector in the input list
		ASSERT( outBufferData!=nullptr && outBufferLengths!=nullptr && outFdcStatuses!=nullptr );
		TStdWinError err=ERROR_SUCCESS; // assumption (all Sectors data retrieved successfully)
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (cyl<nCylinders && head<nHeads)
			while (nSectors>0){
				const TSectorId sectorId=*bufferId;
				if (IsKnownSector( cyl, head, sectorId )){
					// Sector with the given ID found in the Track
					if (const PSectorData bufferedData=__getBufferedSectorData__(cyl,head,&sectorId)){
						// Sector's Data successfully retrieved from the buffer
						*outBufferData++=bufferedData;
						*outBufferLengths++=sectorLength, *outFdcStatuses++=TFdcStatus::WithoutError; // any Data are of the same length and without error
					}else{
						// Sector not yet buffered - buffering the Cylinder that contains the requested Sector
						const DWORD nBytesOfTrack=this->nSectors*sectorLength, nBytesOfCylinder=nHeads*nBytesOfTrack;
						if (const PVOID p=bufferOfCylinders[cyl]=::malloc(nBytesOfCylinder)){
							DWORD nBytesRead;
							switch (trackAccessScheme){
								case TTrackScheme::BY_CYLINDERS:
									f.Seek( cyl*nBytesOfCylinder, CFile::begin );
									nBytesRead=f.Read(p,nBytesOfCylinder);
									break;
								case TTrackScheme::BY_SIDES:
									nBytesRead=0;
									for( THead head=0; head<nHeads; head++ ){
										f.Seek( (head*nCylinders+cyl)*nBytesOfTrack, CFile::begin );
										nBytesRead+=f.Read((PBYTE)p+head*nBytesOfTrack,nBytesOfTrack);
									}
									break;
								default:
									ASSERT(FALSE);
							}
							::memset( (PBYTE)p+nBytesRead, 0, nBytesOfCylinder-nBytesRead ); // what's left unread is zeroed
							continue; // reattempting to read the Sector from the just buffered Cylinder
						}else{
							err=ERROR_NOT_ENOUGH_MEMORY; // buffering of the Cylinder failed
							goto trackNotFound;
						}
					}
				}else{
					// Sector with the given ID not found in the Track
					*outBufferData++=nullptr;
					outBufferLengths++, *outFdcStatuses++=TFdcStatus::SectorNotFound;
				}
				nSectors--, bufferId++;
			}
		else
trackNotFound:
			while (nSectors-->0)
				*outBufferData++=nullptr, *outFdcStatuses++=TFdcStatus::SectorNotFound;
		::SetLastError( *--outBufferData ? err : ERROR_SECTOR_NOT_FOUND );
	}

	TDataStatus CImageRaw::IsSectorDataReady(TCylinder cyl,THead head,RCSectorId id,BYTE nSectorsToSkip,Revolution::TType rev) const{
		// True <=> specified Sector's data variation (Revolution) has been buffered, otherwise False
		ASSERT( rev<Revolution::MAX );
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (cyl<nCylinders && head<nHeads)
			if (IsKnownSector( cyl, head, id ))
				return TDataStatus::READY_HEALTHY;
		return TDataStatus::NOT_READY;
	}

	TStdWinError CImageRaw::MarkSectorAsDirty(RCPhysicalAddress chs,BYTE,PCFdcStatus pFdcStatus){
		// marks Sector with given PhysicalAddress as "dirty", plus sets it the given FdcStatus; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (pFdcStatus->IsWithoutError()){
			m_bModified=TRUE;
			return ERROR_SUCCESS;
		}else
			return ERROR_NOT_SUPPORTED;
	}

	TStdWinError CImageRaw::__setMediumTypeAndGeometry__(PCFormat pFormat,PCSide _sideMap,TSector _firstSectorNumber){
		// sets Medium's Type and geometry; returns Windows standard i/o error
		// - if geometry already set manually by the user, we are successfully done
		if (explicitSides)
			return ERROR_SUCCESS;
		// - determining the Image Size based on the size of Image's underlying file
		const DWORD fileSize=	f.m_hFile!=CFile::hFileNull // InvalidHandle if creating a new Image, for instance
								? sizeWithoutGeometry
								: 0;
		// - setting up geometry
		sideMap=_sideMap, firstSectorNumber=_firstSectorNumber;
		if (pFormat->mediumType!=Medium::UNKNOWN){
			// MediumType and its Format are already known
			nHeads=pFormat->nHeads, nSectors=pFormat->nSectors, sectorLengthCode=GetSectorLengthCode( sectorLength=pFormat->sectorLength );
			if (fileSize){ // some Cylinders exist only if Image contains some data (may not exist if Image not yet formatted)
				__freeBufferOfCylinders__();
				const int nSectorsInTotal=fileSize/sectorLength;
				switch (trackAccessScheme){
					case TTrackScheme::BY_CYLINDERS:{
						const int nSectorsOnCylinder=nHeads*nSectors; // NumberOfHeads constant ...
						const div_t d=div( nSectorsInTotal, nSectorsOnCylinder ); // ... and NumberOfCylinders computed
						if (!d.rem)
							// Image contains correct number of Sectors on the last Cylinder
							nCylinders=d.quot;
						else
							// Image contains low number of Sectors on the last Cylinder - extending to correct number
							nCylinders=1+d.quot; // "1" = the last incomplete Cylinder
						break;
					}
					case TTrackScheme::BY_SIDES:{
						const int nSectorsOnSide=( nCylinders=pFormat->nCylinders )*nSectors; // NumberOfCylinders constant ...
						nHeads=div( nSectorsInTotal+nSectorsOnSide-1, nSectorsOnSide ).quot; // ... and NumberOfHeads computed
						break;
					}
					default:
						ASSERT(FALSE);
				}
				if (auto tmp=Utils::MakeCallocPtr<PVOID,TCylinder>(nCylinders,0))
					bufferOfCylinders.reset( tmp.release() );
				else
					return ERROR_NOT_ENOUGH_MEMORY;
			}
		}else{
			// MediumType and/or its Format were not successfully determined (DosUnknown)
			__freeBufferOfCylinders__();
			if (fileSize){
				nCylinders=1, nHeads=1, nSectors=1, sectorLengthCode=GetSectorLengthCode( sectorLength=std::min<DWORD>(fileSize,(WORD)-1) );
				bufferOfCylinders.reset( Utils::MakeCallocPtr<PVOID,TCylinder>(nCylinders,0).release() );
			}//else
				//nop (see ctor, or specifically OnOpenDocument)
		}
		return ERROR_SUCCESS;		
	}

	TStdWinError CImageRaw::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - if geometry already set manually by the user, we are successfully done
		if (explicitSides)
			return ERROR_SUCCESS;
		// - choosing a proper TrackAccessScheme based on commonly known restrictions on emulation
		/*
		if (dos) // may not exist if creating a new Image
			if (dos->properties==&CGDOS::Properties)
				trackAccessScheme=TTrackScheme::BY_SIDES;
			else
				trackAccessScheme=TTrackScheme::BY_CYLINDERS;
		*/
		// - setting up Medium's Type and geometry
		return __setMediumTypeAndGeometry__(pFormat,sideMap,firstSectorNumber);
	}

	static constexpr TCHAR Custom[]=_T("Custom");
	static constexpr TCHAR Incorrect[]=_T("Incorrect!");

	bool CImageRaw::EditSettings(bool initialEditing){
		// True <=> new settings have been accepted (and adopted by this Image), otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - if no editable settings, we are done
		if (!hasEditableSettings)
			return true;
		// - defining the Dialog
		class CSettingsDialog sealed:public Utils::CRideDialog{
			const bool initialEditing;
			CImageRaw &rawImage;
			BYTE ignoreUiNotifications;

			void DoDataExchange(CDataExchange *pDX) override{
				// exchange of data from and to controls
				CComboBox cb;
				DDX_Check( pDX, ID_AUTO, autoCylinders );
				if (pDX->m_bSaveAndValidate){
					// . Geometry
					DDX_CBIndex( pDX, ID_FORMAT, manualRecognition );
					// . Cylinders
					if (IsDlgItemEnabled(ID_CYLINDER)){ // manual # of Cylinders
						DDX_Text( pDX, ID_CYLINDER, (RCylinder)nCylinders );
							DDV_MinMaxInt( pDX, nCylinders, 1, (TCylinder)-1 );
					}
					// . list of Side numbers
					if (IsDlgItemEnabled(ID_SIDE)) // manual list of Sides?
						if (nHeads<1 || (THead)-1<nHeads){
							SetDlgItemText( ID_HEAD, Incorrect );
							pDX->PrepareCtrl(ID_SIDE);
							pDX->Fail();
						}
					// . list of Sector numbers
					if (IsDlgItemEnabled(ID_SECTOR)){ // manual list of Sectors?
						DDX_Text( pDX, ID_NUMBER, firstSectorNumber );
							DDV_MinMaxInt( pDX, firstSectorNumber, 0, (TSector)-1-1 );
						DDX_Text( pDX, ID_SECTOR, nSectors );
							DDV_MinMaxInt( pDX, nSectors, 1, (TSector)-1+1-firstSectorNumber );
					}
					// . Sector length
					if (IsDlgItemEnabled(ID_SIZE)) // manual Sector length?
						sectorLengthCode=GetDlgComboBoxSelectedValue( ID_SIZE );
					// . trying to apply the Geometry to the Image
					if (manualRecognition){
						if (const TStdWinError err=TrySetMediumTypeAndGeometry()){
							Utils::Information( _T("Invalid geometry"), err );
							pDX->Fail();
						}
						if (rawImage.trackAccessScheme==TTrackScheme::BY_SIDES)
							if (nHeads!=rawImage.nHeads){ // Side numbers under- or over-specified?
								TCHAR buf[80];
								::wsprintf( buf, _T("< Given other settings, the number of heads must be %d."), rawImage.nHeads );
								SetDlgItemText( ID_HEAD, buf );
								pDX->PrepareCtrl(ID_SIDE);
								pDX->Fail();
							}
					}
				}else{
					// . can confirm the dialog only during InitialEditing
					EnableDlgItem( IDOK, initialEditing );
					// . ReadBufferSize
					TCHAR buf[512];
					int i=0;
					DDX_CBIndex( pDX, ID_BUFFER, i );
					/*cb.Attach( GetDlgItem(ID_BUFFER)->m_hWnd );
						BYTE selectedReadBufferSize=0; // by default, the smallest Size is selected in the ComboBox
						for( PCBYTE pSize=ReadBufferSizes; const TCylinder size=*pSize; pSize++ ){
							::wsprintf( buf, _T("%d cylinders"), size );
							cb.SetItemData( cb.AddString(buf), size );
							if (size==params.readBufferSize)
								selectedReadBufferSize=pSize-ReadBufferSizes;
						}
						cb.SetCurSel(selectedReadBufferSize);
					cb.Detach();*/
					// . information on how many Cylinders are currently buffered
					if (initialEditing)
						SetDlgItemText( ID_INFORMATION, _T("N/A") );
					else{
						TCylinder nCylindersBuffered=0;
						for( PVOID *pCyl=rawImage.bufferOfCylinders+nCylinders; pCyl>rawImage.bufferOfCylinders; nCylindersBuffered+=*--pCyl!=nullptr );
						float nUnits; LPCTSTR unitName;
						Utils::BytesToHigherUnits( nCylindersBuffered*nHeads*nSectors*rawImage.sectorLength, nUnits, unitName );
						_stprintf( buf, _T("%d cylinders (%.2f %s)"), nCylindersBuffered, nUnits, unitName );
						SetDlgItemText( ID_INFORMATION, buf );
					}
					// . populating dedicated combo-box with unique geometries
					cb.Attach( *GetDlgItem(ID_FORMAT) );
						cb.SetItemDataPtr( cb.AddString(_T("Automatically")), nullptr );
						for( POSITION pos=CDos::Known.GetHeadPosition(); pos; ){
							const CDos::TProperties &p=*CDos::Known.GetNext(pos);
							for( TCHAR i=0,desc[80]; i<p.nStdFormats; i++ ){
								const TFormat &f=p.stdFormats[i].params.format;
								if (f.nHeads>1)
									::wsprintf( desc, _T("Cyls %d, Sides 0-%d, Sectors 1-%d (%d Bytes)"), f.nCylinders+1, f.nHeads-1, f.nSectors, f.sectorLength );
								else
									::wsprintf( desc, _T("Cyls %d, Side 0, Sectors 1-%d (%d Bytes)"), f.nCylinders+1, f.nSectors, f.sectorLength );
								if (cb.FindString( -1, desc )<0) // avoid duplicities in the combo-box
									cb.SetItemDataPtr( cb.AddString(desc), (PVOID)&f );
							}
						}
						cb.SetItemDataPtr( cb.AddString(Custom), nullptr );
						if (initialEditing)
							cb.SetCurSel(0);
					cb.Detach();
					// . Geometry and associated inputs
					DDX_CBIndex( pDX, ID_FORMAT, manualRecognition );
					EnableDlgItem( ID_FORMAT, initialEditing );
					SendMessage( WM_COMMAND, MAKELONG(ID_FORMAT,CBN_SELCHANGE) );
				}
			}

			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override{
				// window procedure
				static constexpr TCHAR Auto[]=_T("Auto");
				if (msg==WM_COMMAND)
					switch (wParam){
						case MAKELONG(ID_FORMAT,CBN_SELCHANGE):
							// Geometry combo-box value changed
							if (!ignoreUiNotifications){
								static constexpr WORD Controls[]={ ID_CYLINDER, ID_AUTO, ID_SIDE, ID_CONTEXT_HELP, ID_NUMBER, ID_SECTOR, ID_SIZE, 0 };
								CComboBox cb;
								if (EnableDlgItems( Controls, GetDlgComboBoxSelectedIndex(ID_FORMAT)>0 )){
									// manually set geometry
									ignoreUiNotifications++;
										// . adopting selected geometry (if any)
										if (const PCFormat pf=(PCFormat)GetDlgComboBoxSelectedValue(ID_FORMAT)){
											nCylinders=pf->nCylinders+1;
											for( nHeads=0; nHeads<pf->nHeads; nHeads++ )
												sideNumbers[nHeads]=nHeads;
											firstSectorNumber=1;
											nSectors=pf->nSectors;
											sectorLengthCode=pf->sectorLengthCode;
										}
										// . Sides
										Utils::CIntList list;
										for( THead h=0; h<nHeads; list.AddTail(sideNumbers[h++]) );
										SetDlgItemIntList( ID_SIDE, list );
										ignoreUiNotifications--; // give way to UI updates
											SendMessage( WM_COMMAND, MAKELONG(ID_SIDE,EN_CHANGE) );
										ignoreUiNotifications++;
										// . Sectors
										SetDlgItemInt( ID_NUMBER, firstSectorNumber );
										SetDlgItemInt( ID_SECTOR, nSectors );
										// . SectorLength
										CImage::PopulateComboBoxWithSectorLengths( GetDlgItemHwnd(ID_SIZE) );
										SelectDlgComboBoxValue( ID_SIZE, sectorLengthCode );
										// . Controls enabled only if this is the InitialEditing
										if (!initialEditing) // disable all Controls only if this geometry already set
											EnableDlgItems( Controls, initialEditing );
										// . Cylinders
										//fallthrough
									ignoreUiNotifications--;
									//fallthrough
								}else{
									// automatically recognized geometry
									ignoreUiNotifications++;
										SetDlgItemText( ID_CYLINDER, Auto );
										SetDlgItemText( ID_SIDE, Auto );
											SetDlgItemText( ID_HEAD, _T("") );
										SetDlgItemText( ID_SECTOR, Auto );
											SetDlgItemText( ID_NUMBER, Auto );
										cb.Attach( GetDlgItemHwnd(ID_SIZE) );
											cb.SetCurSel( cb.AddString(Auto) );
										cb.Detach();
									ignoreUiNotifications--;
									break;
								}
								//fallthrough
							}else
								break;
						case MAKELONG(ID_AUTO,BN_CLICKED):
							// changed the access scheme (by Cylinders/Sides)
							if (!ignoreUiNotifications){
								ignoreUiNotifications++;
									if (EnableDlgItem( ID_CYLINDER, IsDlgButtonChecked(ID_AUTO)==BST_UNCHECKED ))
										SetDlgItemInt( ID_CYLINDER, nCylinders );
									else
										SetDlgItemText( ID_CYLINDER, Auto );
								ignoreUiNotifications--;
							}
							//fallthrough
						case MAKELONG(ID_SIDE,EN_CHANGE):
							// input controls manually changed
							if (!ignoreUiNotifications){
								ignoreUiNotifications++;
									static const PropGrid::Integer::TUpDownLimits SideLimits={ 0, (TSide)-1 };
									Utils::CIntList list;
									if (GetDlgItemIntList( ID_SIDE, list, SideLimits, 1, (THead)-1 )){
										TCHAR buf[16];
										nHeads=list.GetCount();
										::wsprintf( buf, _T("%d head%c"), nHeads, nHeads>1?'s':'\0' );
										SetDlgItemText( ID_HEAD, buf );
										for( THead h=0; h<nHeads; sideNumbers[h++]=list.RemoveHead() );
									}else{
										SetDlgItemText( ID_HEAD, Incorrect );
										nHeads=0;
									}
								ignoreUiNotifications--;
							}
							//fallthrough
						case MAKELONG(ID_CYLINDER,EN_CHANGE):
						case MAKELONG(ID_NUMBER,EN_CHANGE):
						case MAKELONG(ID_SECTOR,EN_CHANGE):
						case MAKELONG(ID_SIZE,CBN_SELCHANGE):
							// input controls manually changed - selecting one of pre-defined geometries (if any corresponding)
							if (!ignoreUiNotifications){
								ignoreUiNotifications++;
									// . collecting actual values
									const bool cylindersSignificant=IsDlgItemEnabled(ID_CYLINDER);
									const TCylinder nTmpCyls= cylindersSignificant ? GetDlgItemInt(ID_CYLINDER) : 0;
									const TSector tmpFirstSectorNumber=GetDlgItemInt(ID_NUMBER);
									const TSector nTmpSectors=GetDlgItemInt(ID_SECTOR);
									const BYTE tmpSectorLengthCode=GetDlgComboBoxSelectedIndex(ID_SIZE);
									// . selecting pre-defined geometry (or "Custom")
									CComboBox cb;
									cb.Attach( GetDlgItemHwnd(ID_FORMAT) );
										TSide sideMin=-1, sideMax=0;
										for( THead h=0; h<nHeads; ){
											const TSide s=sideNumbers[h++];
											if (s<sideMin)
												sideMin=s;
											if (sideMax<s)
												sideMax=s;
										}
										cb.SetCurSel( cb.FindString(-1,Custom) ); // assumption (no pre-defined geometry corresponds with current settings)
										for( int i=0; i<cb.GetCount(); i++ )
											if (const PCFormat pf=(PCFormat)cb.GetItemDataPtr(i))
												if (( nTmpCyls==pf->nCylinders+1 || !cylindersSignificant )
													&&
													nHeads==pf->nHeads && sideMin==0 && sideMax==nHeads-1
													&&
													tmpFirstSectorNumber==1 && nTmpSectors==pf->nSectors
													&&
													tmpSectorLengthCode==pf->sectorLengthCode
												){
													cb.SetCurSel(i);
													break;
												}
									cb.Detach();
								ignoreUiNotifications--;
							}
							break;
						case MAKELONG(ID_CONTEXT_HELP,BN_CLICKED):
							Utils::Information( _T("Type in sides as you would pages to print in a PDF reader.\n\nExample:\n- 0, 1 (2 heads)\n- 0-5, 6, 9-11 (10 heads)\n- 0-3, 7-4, 123 (9 heads)") );
							break;
					}
				return __super::WindowProc( msg, wParam, lParam );
			}
		public:
			int manualRecognition, autoCylinders;
			TCylinder nCylinders;
			THead nHeads;
			TSide sideNumbers[(THead)-1];
			TSector nSectors,firstSectorNumber;
			BYTE sectorLengthCode;

			CSettingsDialog(CImageRaw &rawImage,bool initialEditing)
				// ctor
				: Utils::CRideDialog(IDR_RAW_SETTINGS)
				, ignoreUiNotifications(0)
				, initialEditing(initialEditing)
				, rawImage(rawImage)
				, manualRecognition( rawImage.explicitSides )
				, autoCylinders( rawImage.trackAccessScheme==TTrackScheme::BY_CYLINDERS )
				, nCylinders( rawImage.nCylinders )
				, nHeads( rawImage.nHeads )
				, nSectors( rawImage.nSectors ) , firstSectorNumber( rawImage.firstSectorNumber )
				, sectorLengthCode( rawImage.sectorLengthCode ) {
				if (rawImage.explicitSides)
					::memcpy( sideNumbers, rawImage.explicitSides, nHeads*sizeof(TSide) );
				else if (initialEditing){ // automatic geometry - need to initialize the defaults
					nCylinders=1;
					nHeads=1, *sideNumbers=0;
					firstSectorNumber=1, nSectors=9, sectorLengthCode=TFormat::LENGTHCODE_512;
				}
			}

			TStdWinError TrySetMediumTypeAndGeometry() const{
				// tries to apply current geometry; returns Windows standard i/o error
				rawImage.trackAccessScheme = autoCylinders==BST_UNCHECKED ? TTrackScheme::BY_SIDES : TTrackScheme::BY_CYLINDERS;
				TFormat fmt=TFormat::Unknown; // the Image format ...
					fmt.mediumType=Medium::ANY; // ... is no longer Unknown, but the Medium can be Any of supported
					fmt.nCylinders=nCylinders;
					fmt.nHeads=nHeads;
					fmt.nSectors=nSectors;
					fmt.sectorLengthCode=(TFormat::TLengthCode)sectorLengthCode;
					fmt.sectorLength=GetOfficialSectorLength(sectorLengthCode);
				return rawImage.SetMediumTypeAndGeometry( &fmt, sideNumbers, firstSectorNumber );
			}
		} d( *this, initialEditing );
		// - showing the Dialog and processing its result
		if (d.DoModal()==IDOK){
			if (d.manualRecognition){
				if (d.TrySetMediumTypeAndGeometry())
					return false; // we should always succeed, but just to be sure
				explicitSides.reset( Utils::MakeCallocPtr<TSide,THead>(d.nHeads,d.sideNumbers).release() );
				nHeads=d.nHeads;
			}else
				explicitSides.reset();
			sideMap=explicitSides;
			return true;
		}else
			return false;
	}

	TStdWinError CImageRaw::Reset(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - closing Image's underlying file
		if (f.m_hFile!=CFile::hFileNull)
			f.Close();
		// - emptying the BufferOfCylinders
		__freeBufferOfCylinders__();
		// - resetting the geometry
		nCylinders=0, nHeads=0, nSectors=0;
		return ERROR_SUCCESS;
	}





	TStdWinError CImageRaw::FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector _nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte,const volatile bool &cancelled){
		// formats given Track {Cylinder,Head} to the requested NumberOfSectors, each with corresponding Length and FillerByte as initial content; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if ((codec&properties->supportedCodecs)==0)
			return ERROR_NOT_SUPPORTED;
		// - formatting to "no Sectors" is translated as unformatting the Track
		if (!_nSectors)
			return UnformatTrack(cyl,head);
		// - typically if no DOS has been recognized, the geometry may not be trusted
		if (!nCylinders){
			nSectors=_nSectors; // adopting current parameters as the geometry for the remainder of this Image
			sectorLength=*bufferLength, sectorLengthCode=bufferId->lengthCode;
		}
		// - validating the Number and Lengths of Sectors
		#ifdef _DEBUG
			if (!nSectors) // Image is empty - it's necessary to set its geometry first!
				ASSERT(FALSE);
		#endif
		BYTE involvedSectors[(TSector)-1+1];
		::ZeroMemory(involvedSectors,sizeof(involvedSectors));
		PCSectorId pId=bufferId;	 PCWORD pw=bufferLength;
		for( TSector n=_nSectors; n; n--,pId++ ){
			// . TEST: Number, Lengths and i/o errors of Sectors
			if (pId->cylinder!=cyl || pId->side!=sideMap[head] || pId->lengthCode!=sectorLengthCode || *pw++!=sectorLength || !bufferFdcStatus++->IsWithoutError())
				return Utils::ErrorByOs( ERROR_VHD_INVALID_STATE, ERROR_NOT_SUPPORTED );
			// . TEST: uniqueness of SectorIDs
			if (involvedSectors[pId->sector])
				return Utils::ErrorByOs( ERROR_VHD_INVALID_TYPE, ERROR_NOT_SUPPORTED );
			// . passed all tests - recording Sector's number
			involvedSectors[pId->sector]=TRUE;
		}
		const PCBYTE pFirstSectorNumber=(PCBYTE)::memchr(involvedSectors,TRUE,sizeof(involvedSectors));
		if (!nCylinders) firstSectorNumber=pFirstSectorNumber-involvedSectors;
		if (::memchr(pFirstSectorNumber,FALSE,nSectors)) // if missing some Sector -> error
			return Utils::ErrorByOs( ERROR_VHD_INVALID_SIZE, ERROR_NOT_SUPPORTED );
		if (::memchr(pFirstSectorNumber+nSectors,TRUE,sizeof(involvedSectors)-firstSectorNumber-nSectors)) // if some Sector redundand -> error
			return Utils::ErrorByOs( ERROR_VHD_INVALID_SIZE, ERROR_NOT_SUPPORTED );
		if (head>=nHeads)
			return Utils::ErrorByOs( ERROR_VHD_INVALID_SIZE, ERROR_NOT_SUPPORTED );
		// - formatting
		const DWORD nBytesOfTrack=nSectors*sectorLength;
		if (nCylinders<=cyl)
			// redimensioning the Image
			switch (trackAccessScheme){
				case TTrackScheme::BY_SIDES:
					if (nHeads>1) // if Image structured by Sides (and there are multiple Sides), all Cylinders must be buffered as the whole Image will have to be restructured when saving
						for( TCylinder c=0; c<nCylinders; ){
							if (cancelled)
								return ERROR_CANCELLED;
							const TPhysicalAddress chs={ c++, 0, {cyl,sideMap[0],firstSectorNumber,sectorLengthCode} };
							GetHealthySectorData(chs);
						}
					//fallthrough
				case TTrackScheme::BY_CYLINDERS:
					if (const TStdWinError err=ExtendToNumberOfCylinders( 1+cyl, fillerByte, cancelled ))
						return err;
					break;
				default:
					ASSERT(FALSE);
			}
		else{
			// reinitializing given Track
			// . buffering Cylinder by reading one of its Sector
			const TPhysicalAddress chs={ cyl, 0, {cyl,sideMap[0],firstSectorNumber,sectorLengthCode} };
			GetHealthySectorData(chs);
			// . reinitializing given Track to FillerByte
			::memset( (PBYTE)bufferOfCylinders[cyl]+head*nBytesOfTrack, fillerByte, nBytesOfTrack );
		}
		m_bModified=TRUE;
		return ERROR_SUCCESS;
	}

	TStdWinError CImageRaw::UnformatTrack(TCylinder cyl,THead head){
		// unformats given Track {Cylinder,Head}; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		switch (trackAccessScheme){
			case TTrackScheme::BY_SIDES:
				if (nHeads>1) // if Image structured by Sides (and there are multiple Sides), all Cylinders must be buffered as the whole Image will have to be restructured when saving
					for( TCylinder c=0; c<nCylinders; ){
						const TPhysicalAddress chs={ c++, 0, {cyl,sideMap[0],firstSectorNumber,sectorLengthCode} };
						GetHealthySectorData(chs);
					}
				//fallthrough
			case TTrackScheme::BY_CYLINDERS:
				if (cyl==nCylinders-1 // unformatting the last Cylinder in the Image
					&&
					head==0 // ignore this command for any but zeroth Head (some copy-protected disks have hidden Cylinder with data only under Head 0 - finding nothing under Head 1 would destroy the whole Cylinder that Head 0 created in the RawImage, e.g. during dump)
				){
					// . redimensioning the Image
					__freeCylinder__(cyl);
					// . adjusting the NumberOfCylinders
					nCylinders--;
					m_bModified=TRUE;
				}
				break;
			default:
				ASSERT(FALSE);
		}
		return ERROR_SUCCESS;
	}

	std::unique_ptr<CImage::CSectorDataSerializer> CImageRaw::CreateSectorDataSerializer(CHexaEditor *pParentHexaEditor){
		// abstracts all Sector data (good and bad) into a single file and returns the result
		// - defining the Serializer class
		static const BYTE nDiscoveredRawRevolutions=1;
		class CSerializer sealed:public CSectorDataSerializer{
			const CImageRaw *const image;

			void __getPhysicalAddress__(int logPos,TTrack &rOutTrack,BYTE &rOutSectorIndex,PWORD pOutOffset) const{
				// determines the PhysicalAddress that contains the specified LogicalPosition
				const div_t s=div( (int)position, image->sectorLength ); // Quot = # of Sectors to skip, Rem = the first Byte to read in the Sector yet to be computed
				if (pOutOffset)
					*pOutOffset=s.rem;
				const div_t t=div( s.quot, image->nSectors ); // Quot = # of Tracks to skip, Rem = the zero-based Sector index on a Track yet to be computed
				rOutTrack=t.quot;
				rOutSectorIndex=t.rem;
			}
		public:
			CSerializer(CHexaEditor *pParentHexaEditor,CImageRaw *image)
				// ctor
				: CSectorDataSerializer( pParentHexaEditor, image, image->nCylinders*image->nHeads*image->nSectors*image->sectorLength, nDiscoveredRawRevolutions )
				, image(image) {
			}

			// CSectorDataSerializer methods
			#if _MFC_VER>=0x0A00
			ULONGLONG Seek(LONGLONG lOff,UINT nFrom) override{
			#else
			LONG Seek(LONG lOff,UINT nFrom) override{
			#endif
				// sets the actual Position in the Serializer
				const auto result=__super::Seek(lOff,nFrom);
				__getPhysicalAddress__( result, currTrack, sector.indexOnTrack, &sector.offset );
				return result;
			}
			void SetCurrentRevolution(Revolution::TType rev) override{
				// selects Revolution from which to retrieve Sector data
				//nop
			}
			TPhysicalAddress GetCurrentPhysicalAddress() const override{
				// returns the current Sector's PhysicalAddress
				const div_t h=div( currTrack, image->nHeads ); // Quotient = # of Cylinders to skip, Remainder = Head in a Cylinder
				const TPhysicalAddress result={	h.quot, h.rem,
												{ h.quot, image->sideMap[h.rem], image->firstSectorNumber+sector.indexOnTrack, image->sectorLengthCode }
											};
				return result;
			}
			DWORD GetSectorStartPosition(RCPhysicalAddress chs,BYTE nSectorsToSkip) const override{
				// computes and returns the position of the first Byte of the Sector at the PhysicalAddress
				return (  (chs.cylinder*image->nHeads+chs.head)*image->nSectors + chs.sectorId.sector-image->firstSectorNumber  ) * image->sectorLength;
			}
			TScannerStatus GetTrackScannerStatus(PCylinder pnOutScannedCyls) const override{
				// returns Track scanner Status, if any
				return TScannerStatus::UNAVAILABLE; // no scanner needed, the Image has implicit structure
			}
			void SetTrackScannerStatus(TScannerStatus status) override{
				// suspends/resumes Track scanner, if any (if none, simply ignores the request)
				//nop
			}

			// CHexaEditor::IContentAdviser methods
			int LogicalPositionToRow(int logPos,BYTE nBytesInRow) override{
				// computes and returns the row containing the specified LogicalPosition
				const div_t d=div( logPos, image->sectorLength );
				const int nRowsPerRecord = (image->sectorLength+nBytesInRow-1)/nBytesInRow;
				return d.quot*nRowsPerRecord + d.rem/nBytesInRow;
			}
			int RowToLogicalPosition(int row,BYTE nBytesInRow) override{
				// converts Row begin (i.e. its first Byte) to corresponding logical position in underlying File and returns the result
				const int nRowsPerRecord = (image->sectorLength+nBytesInRow-1)/nBytesInRow;
				const div_t d=div( row, nRowsPerRecord );
				return d.quot*image->sectorLength + d.rem*nBytesInRow;
			}
			void GetRecordInfo(int logPos,PINT pOutRecordStartLogPos,PINT pOutRecordLength,bool *pOutDataReady) override{
				// retrieves the start logical position and length of the Record pointed to by the input LogicalPosition
				if (pOutRecordStartLogPos)
					*pOutRecordStartLogPos = logPos/image->sectorLength*image->sectorLength;
				if (pOutRecordLength)
					*pOutRecordLength = image->sectorLength;
				if (pOutDataReady)
					*pOutDataReady=true;
			}
			LPCWSTR GetRecordLabelW(int logPos,PWCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const override{
				// populates the Buffer with label for the Record that STARTS at specified LogicalPosition, and returns the Buffer; returns Null if no Record starts at specified LogicalPosition
				if (logPos%image->sectorLength==0){
					TTrack track; BYTE sectorIndex;
					__getPhysicalAddress__( logPos, track, sectorIndex, nullptr );
					const div_t h=div( track, image->nHeads ); // Quotient = # of Cylinders to skip, Remainder = Head in a Cylinder
					const TSectorId tmp={ h.quot, image->sideMap[h.rem], image->firstSectorNumber+sectorIndex, image->sectorLengthCode };
					#ifdef UNICODE
						return ::lstrcpyn( labelBuffer, tmp.ToString(), labelBufferCharsMax );
					#else
						::MultiByteToWideChar( CP_ACP, 0, tmp.ToString(),-1, labelBuffer,labelBufferCharsMax );
						return labelBuffer;
					#endif
				}else
					return nullptr;
			}
		};
		// - returning a Serializer class instance
		return std::unique_ptr<CSectorDataSerializer>( new CSerializer( pParentHexaEditor, this ) );
	}
