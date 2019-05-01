#include "stdafx.h"
#include "GDOS.h"

	static PImage __instantiate__(){
		return new CImageRaw( &CImageRaw::Properties, true );
	}

	const CImage::TProperties CImageRaw::Properties={
		_T("Raw data image"),	// name
		__instantiate__,		// instantiation function
		_T("*.ima") IMAGE_FORMAT_SEPARATOR _T("*.img") IMAGE_FORMAT_SEPARATOR _T("*.dat") IMAGE_FORMAT_SEPARATOR _T("*.bin"),	// filter
		(TMedium::TType)(TMedium::FLOPPY_ANY | TMedium::HDD_RAW), // supported Media
		1,16384	// Sector supported min and max length
	};

	CImageRaw::CImageRaw(PCProperties properties,bool hasEditableSettings)
		// ctor
		: CImage(properties,hasEditableSettings)
		, trackAccessScheme(TTrackScheme::BY_CYLINDERS)
		, nCylinders(0) , nSectors(0) // = not initialized - see SetMediumTypeAndGeometry
		, bufferOfCylinders(nullptr) , sizeWithoutGeometry(0) {
		Reset(); // to be correctly initialized
	}

	CImageRaw::~CImageRaw(){
		// dtor
		__freeBufferOfCylinders__();
	}






	bool CImageRaw::__openImageForReadingAndWriting__(LPCTSTR fileName){
		// True <=> given underlying file successfully opened for reading and writing, otherwise False
		return f.Open( fileName, CFile::modeReadWrite|CFile::typeBinary|CFile::shareExclusive )!=FALSE;
	}

	TStdWinError CImageRaw::__extendToNumberOfCylinders__(TCylinder nCyl,BYTE fillerByte){
		// formats new Cylinders to meet the minimum number requested; returns Windows standard i/o error
		// - redimensioning the Image
		if (const PVOID tmp=::realloc(bufferOfCylinders,sizeof(PVOID)*nCyl))
			bufferOfCylinders=(PVOID *)tmp;
		else
			return ERROR_NOT_ENOUGH_MEMORY;
		// - initializing added Cylinders with the FillerByte
		for( const DWORD nBytesOfCylinder=nHeads*nSectors*sectorLength; nCylinders<nCyl; )
			if (const PVOID tmp=bufferOfCylinders[nCylinders]=::malloc(nBytesOfCylinder)){
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
			::free(bufferOfCylinders), bufferOfCylinders=nullptr;
		}
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
		if (!__openImageForReadingAndWriting__(lpszPathName)) // if cannot open for both reading and writing ...
			if (!__openImageForReading__(lpszPathName,&f)) // ... trying to open at least for reading, and if neither this works ...
				return FALSE; // ... the Image cannot be open in any way
			else
				canBeModified=false;
		// - currently without geometry (DOS must call SetMediumTypeAndGeometry)
		if ( sizeWithoutGeometry=f.GetLength() )
			nCylinders=1, nHeads=1, nSectors=1, sectorLengthCode=__getSectorLengthCode__( sectorLength=min(sizeWithoutGeometry,(WORD)-1) );
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
					if (f.m_hFile!=(UINT_PTR)INVALID_HANDLE_VALUE) // handle doesn't exist if creating a new Image
						f.Seek(sectorLength,CFile::current);
				}else{
					BYTE buffer[(WORD)-1+1];
					pfOtherThanCurrentFile->Write( buffer, f.Read(buffer,sectorLength) );
				}
		}
	}

	BOOL CImageRaw::OnSaveDocument(LPCTSTR lpszPathName){
		// True <=> this Image has been successfully saved, otherwise False
		// - saving
		CFile fTmp;
		const bool savingToCurrentFile=lpszPathName==m_strPathName;
		if (!savingToCurrentFile && !__openImageForWriting__(lpszPathName,&fTmp))
			return FALSE;
		if (f.m_hFile!=(UINT_PTR)INVALID_HANDLE_VALUE) // handle doesn't exist when creating new Image
			f.Seek(0,CFile::begin);
		TPhysicalAddress chs;
			chs.sectorId.lengthCode=sectorLengthCode;
		switch (trackAccessScheme){
			case TTrackScheme::BY_CYLINDERS:
				for( chs.cylinder=0; chs.cylinder<nCylinders; chs.cylinder++ )
					for( chs.sectorId.cylinder=chs.cylinder,chs.head=0; chs.head<nHeads; chs.head++ ){
						chs.sectorId.side=sideMap[chs.head];
						__saveTrackToCurrentPositionInFile__( savingToCurrentFile?nullptr:&fTmp, chs );
					}
				break;
			case TTrackScheme::BY_SIDES:
				for( chs.head=0; chs.head<nHeads; chs.head++ )
					for( chs.sectorId.side=sideMap[chs.head],chs.cylinder=0; chs.cylinder<nCylinders; chs.cylinder++ ){
						chs.sectorId.cylinder=chs.cylinder;
						__saveTrackToCurrentPositionInFile__( savingToCurrentFile?nullptr:&fTmp, chs );
					}
				break;
			default:
				ASSERT(FALSE);
		}
		m_bModified=FALSE;
		// - reopening Image's underlying file
		if (f.m_hFile!=(UINT_PTR)INVALID_HANDLE_VALUE){
			if (savingToCurrentFile)
				f.SetLength(f.GetPosition()); // "trimming" eventual unnecessary data (e.g. when unformatting Cylinders)
			f.Close();
		}
		if (fTmp.m_hFile!=(UINT_PTR)INVALID_HANDLE_VALUE)
			fTmp.Close();
		return __openImageForReadingAndWriting__(lpszPathName);
	}

	TCylinder CImageRaw::GetCylinderCount() const{
		// determines and returns the actual number of Cylinders in the Image
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return nCylinders;
	}

	THead CImageRaw::GetNumberOfFormattedSides(TCylinder cyl) const{
		// determines and returns the number of Sides formatted on given Cylinder; returns 0 iff Cylinder not formatted
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		return cyl<nCylinders ? nHeads : 0;
	}

	TSector CImageRaw::ScanTrack(TCylinder cyl,THead head,PSectorId bufferId,PWORD bufferLength,PINT startTimesMicroseconds,PBYTE pAvgGap3) const{
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
				//if (startTimesMicroseconds)
					//TODO
			}
			if (pAvgGap3)
				*pAvgGap3=FDD_SECTOR_GAP3_STD;
			return nSectors;
		}else
			return 0;
	}

	void CImageRaw::GetTrackData(TCylinder cyl,THead head,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors,bool silentlyRecoverFromErrors,PSectorData *outBufferData,PWORD outBufferLengths,TFdcStatus *outFdcStatuses){
		// populates output buffers with specified Sectors' data, usable lengths, and FDC statuses; ALWAYS attempts to buffer all Sectors - caller is then to sort out eventual read errors (by observing the FDC statuses); caller can call ::GetLastError to discover the error for the last Sector in the input list
		ASSERT( outBufferData!=nullptr && outBufferLengths!=nullptr && outFdcStatuses!=nullptr );
		TStdWinError err=ERROR_SUCCESS; // assumption (all Sectors data retrieved successfully)
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (cyl<nCylinders && head<nHeads)
			while (nSectors>0){
				const TSectorId sectorId=*bufferId;
				if (sectorId.cylinder==cyl && sectorId.side==sideMap[head] && sectorId.sector>=firstSectorNumber && sectorId.sector-firstSectorNumber<this->nSectors && sectorId.lengthCode==sectorLengthCode){
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

	TStdWinError CImageRaw::MarkSectorAsDirty(RCPhysicalAddress chs,BYTE,PCFdcStatus pFdcStatus){
		// marks Sector with given PhysicalAddress as "dirty", plus sets it the given FdcStatus; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		if (pFdcStatus->IsWithoutError()){
			m_bModified=TRUE;
			return ERROR_SUCCESS;
		}else
			return ERROR_BAD_COMMAND;
	}

	TStdWinError CImageRaw::__setMediumTypeAndGeometry__(PCFormat pFormat,PCSide _sideMap,TSector _firstSectorNumber){
		// sets Medium's Type and geometry; returns Windows standard i/o error
		// - determining the Image Size based on the size of Image's underlying file
		const DWORD fileSize=	f.m_hFile!=(UINT_PTR)INVALID_HANDLE_VALUE // InvalidHandle if creating a new Image, for instance
								? sizeWithoutGeometry
								: 0;
		// - setting up geometry
		sideMap=_sideMap, firstSectorNumber=_firstSectorNumber;
		if (pFormat->mediumType!=TMedium::UNKNOWN){
			// MediumType and its Format are already known
			nHeads=pFormat->nHeads, nSectors=pFormat->nSectors, sectorLengthCode=__getSectorLengthCode__( sectorLength=pFormat->sectorLength );
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
				const DWORD dw=sizeof(PVOID)*nCylinders;
				if (bufferOfCylinders=(PVOID *)::malloc(dw))
					::ZeroMemory(bufferOfCylinders,dw);
				else
					return ERROR_NOT_ENOUGH_MEMORY;
			}
		}else{
			// MediumType and/or its Format were not successfully determined (DosUnknown)
			__freeBufferOfCylinders__();
			if (fileSize){
				nCylinders=1, nHeads=1, nSectors=1, sectorLengthCode=__getSectorLengthCode__( sectorLength=min(fileSize,(WORD)-1) );
				::ZeroMemory( bufferOfCylinders=(PVOID *)::malloc(sizeof(PVOID)), sizeof(PVOID) );
			}//else
				//nop (see ctor, or specifically OnOpenDocument)
		}
		return ERROR_SUCCESS;		
	}

	TStdWinError CImageRaw::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - choosing a proper TrackAccessScheme based on commonly known restrictions on emulation
		if (dos) // may not exist if creating a new Image
			if (dos->properties==&CGDOS::Properties)
				trackAccessScheme=TTrackScheme::BY_SIDES;
			else
				trackAccessScheme=TTrackScheme::BY_CYLINDERS;
		// - setting up Medium's Type and geometry
		return __setMediumTypeAndGeometry__(pFormat,sideMap,firstSectorNumber);
	}

	void CImageRaw::EditSettings(){
		// displays dialog with editable settings and reflects changes made by the user into the Image's inner state
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		//TODO
	}

	TStdWinError CImageRaw::Reset(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - closing Image's underlying file
		if (f.m_hFile!=(UINT_PTR)INVALID_HANDLE_VALUE)
			f.Close();
		// - emptying the BufferOfCylinders
		__freeBufferOfCylinders__();
		// - resetting the geometry
		nCylinders=0, nHeads=0, nSectors=0;
		return ERROR_SUCCESS;
	}





	TStdWinError CImageRaw::FormatTrack(TCylinder cyl,THead head,TSector _nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte){
		// formats given Track {Cylinder,Head} to the requested NumberOfSectors, each with corresponding Length and FillerByte as initial content; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - formatting to "no Sectors" is translated as unformatting the Track
		if (!_nSectors)
			return UnformatTrack(cyl,head);
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
				return ERROR_BAD_COMMAND;
			// . TEST: uniqueness of SectorIDs
			if (involvedSectors[pId->sector])
				return ERROR_BAD_COMMAND;
			// . passed all tests - recording Sector's number
			involvedSectors[pId->sector]=TRUE;
		}
		const PCBYTE pFirstSectorNumber=(PCBYTE)::memchr(involvedSectors,TRUE,sizeof(involvedSectors));
		if (!nCylinders) firstSectorNumber=pFirstSectorNumber-involvedSectors;
		if (::memchr(pFirstSectorNumber,FALSE,nSectors)) // if missing some Sector -> error
			return ERROR_BAD_COMMAND;
		if (::memchr(pFirstSectorNumber+nSectors,TRUE,sizeof(involvedSectors)-firstSectorNumber-nSectors)) // if some Sector redundand -> error
			return ERROR_BAD_COMMAND;
		if (head>=nHeads)
			return ERROR_BAD_COMMAND;
		// - formatting
		const DWORD nBytesOfTrack=nSectors*sectorLength;
		if (nCylinders<=cyl)
			// redimensioning the Image
			switch (trackAccessScheme){
				case TTrackScheme::BY_SIDES:
					if (nHeads>1) // if Image structured by Sides (and there are multiple Sides), all Cylinders must be buffered as the whole Image will have to be restructured when saving
						for( TCylinder c=0; c<nCylinders; ){
							const TPhysicalAddress chs={ c++, 0, {cyl,sideMap[0],firstSectorNumber,sectorLengthCode} };
							CImage::GetSectorData(chs);
						}
					//fallthrough
				case TTrackScheme::BY_CYLINDERS:{
					const TStdWinError err=__extendToNumberOfCylinders__(1+cyl,fillerByte);
					if (err!=ERROR_SUCCESS) return err;
					break;
				}
				default:
					ASSERT(FALSE);
			}
		else{
			// reinitializing given Track
			// . buffering Cylinder by reading one of its Sector
			const TPhysicalAddress chs={ cyl, 0, {cyl,sideMap[0],firstSectorNumber,sectorLengthCode} };
			CImage::GetSectorData(chs);
			// . reinitializing given Track to FillerByte
			::memset( (PBYTE)bufferOfCylinders[cyl]+head*nBytesOfTrack, fillerByte, nBytesOfTrack );
		}
		m_bModified=TRUE;
		return ERROR_SUCCESS;
	}

	TStdWinError CImageRaw::UnformatTrack(TCylinder cyl,THead){
		// unformats given Track {Cylinder,Head}; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		switch (trackAccessScheme){
			case TTrackScheme::BY_SIDES:
				if (nHeads>1) // if Image structured by Sides (and there are multiple Sides), all Cylinders must be buffered as the whole Image will have to be restructured when saving
					for( TCylinder c=0; c<nCylinders; ){
						const TPhysicalAddress chs={ c++, 0, {cyl,sideMap[0],firstSectorNumber,sectorLengthCode} };
						CImage::GetSectorData(chs);
					}
				//fallthrough
			case TTrackScheme::BY_CYLINDERS:
				if (cyl==nCylinders-1){ // unformatting the last Cylinder in the Image
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

	CImage::CSectorDataSerializer *CImageRaw::CreateSectorDataSerializer(){
		// abstracts all Sector data (good and bad) into a single file and returns the result
		// - defining the Serializer class
		class CSerializer sealed:public CSectorDataSerializer{
			const CImageRaw *const image;

			void __getPhysicalAddress__(int logPos,TPhysicalAddress &rOutChs,PWORD pOutOffset) const{
				// determines the PhysicalAddress that contains the specified LogicalPosition
				const div_t s=div( (int)position, image->sectorLength ); // Quot = # of Sectors to skip, Rem = the first Byte to read in the Sector yet to be computed
				if (pOutOffset)
					*pOutOffset=s.rem;
				const div_t t=div( s.quot, image->nSectors ); // Quot = # of Tracks to skip, Rem = the zero-based Sector index on a Track yet to be computed
				const div_t h=div( t.quot, image->nHeads ); // Quotient = # of Cylinders to skip, Remainder = Head in a Cylinder
				rOutChs.cylinder = rOutChs.sectorId.cylinder = h.quot;
				rOutChs.sectorId.side=image->sideMap[ rOutChs.head=h.rem ];
				rOutChs.sectorId.sector=image->firstSectorNumber+t.rem;
				rOutChs.sectorId.lengthCode=image->sectorLengthCode;
			}
		public:
			CSerializer(CImageRaw *image)
				// ctor
				: CSectorDataSerializer( image, image->nCylinders*image->nHeads*image->nSectors*image->sectorLength )
				, image(image) {
				Seek(0,SeekPosition::begin); // initializing state of current Sector to read from or write to
				sector.chs.sectorId.lengthCode=image->sectorLengthCode;
			}

			// CSectorDataSerializer methods
			LONG Seek(LONG lOff,UINT nFrom) override{
				// sets the actual Position in the Serializer
				const LONG result=__super::Seek(lOff,nFrom);
				__getPhysicalAddress__( result, sector.chs, &sector.offset );
				return result;
			}

			// CHexaEditor::IContentAdviser methods
			int LogicalPositionToRow(int logPos,BYTE nBytesInRow) const override{
				// computes and returns the row containing the specified LogicalPosition
				const div_t d=div( logPos, image->sectorLength );
				const int nRowsPerRecord = (image->sectorLength+nBytesInRow-1)/nBytesInRow;
				return d.quot*nRowsPerRecord + d.rem/nBytesInRow;
			}
			int RowToLogicalPosition(int row,BYTE nBytesInRow) const override{
				// converts Row begin (i.e. its first Byte) to corresponding logical position in underlying File and returns the result
				const int nRowsPerRecord = (image->sectorLength+nBytesInRow-1)/nBytesInRow;
				const div_t d=div( row, nRowsPerRecord );
				return d.quot*image->sectorLength + d.rem*nBytesInRow;
			}
			void GetRecordInfo(int logPos,PINT pOutRecordStartLogPos,PINT pOutRecordLength,bool *pOutDataReady) const override{
				// retrieves the start logical position and length of the Record pointed to by the input LogicalPosition
				if (pOutRecordStartLogPos)
					*pOutRecordStartLogPos = logPos/image->sectorLength*image->sectorLength;
				if (pOutRecordLength)
					*pOutRecordLength = image->sectorLength;
				if (pOutDataReady)
					*pOutDataReady=true;
			}
			LPCTSTR GetRecordLabel(int logPos,PTCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const override{
				// populates the Buffer with label for the Record that STARTS at specified LogicalPosition, and returns the Buffer; returns Null if no Record starts at specified LogicalPosition
				if (logPos%image->sectorLength==0){
					TPhysicalAddress chs;
					__getPhysicalAddress__( logPos, chs, nullptr );
					return chs.sectorId.ToString(labelBuffer);
				}else
					return nullptr;
			}
		};
		// - returning a Serializer class instance
		return new CSerializer(this);
	}
