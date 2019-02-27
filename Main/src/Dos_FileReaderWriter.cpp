#include "stdafx.h"

	CDos::CFileReaderWriter::CFileReaderWriter(const CDos *dos,PCFile file)
		// ctor to read/edit an existing File in Image
		: dos(dos) , fileSize(dos->__getFileSize__(file)) , fatPath(dos,file)
		, position(0) {
	}

	CDos::CFileReaderWriter::CFileReaderWriter(const CDos *dos,RCPhysicalAddress chs)
		// ctor to read/edit particular Sector in Image (e.g. Boot Sector)
		: dos(dos) , fileSize(dos->formatBoot.sectorLength) , fatPath(dos,chs)
		, position(0) {
	}

	CDos::CFileReaderWriter::~CFileReaderWriter(){
		// dtor
	}








	DWORD CDos::CFileReaderWriter::GetLength() const{
		// returns the File size
		return fileSize;
	}

	DWORD CDos::CFileReaderWriter::GetPosition() const{
		// returns the actual Position in open File
		return position;
	}

	LONG CDos::CFileReaderWriter::Seek(LONG lOff,UINT nFrom){
		// sets the actual Position in open File
		switch ((SeekPosition)nFrom){
			case SeekPosition::current:
				lOff+=position;
				//fallthrough
			case SeekPosition::begin:
				position=min(lOff,fileSize);
				break;
			case SeekPosition::end:
				position=max(fileSize-lOff,0);
				break;
			default:
				ASSERT(FALSE);
		}
		return position;
	}

	UINT CDos::CFileReaderWriter::Read(LPVOID lpBuf,UINT nCount){
		// tries to read given NumberOfBytes into the Buffer, starting with current Position; returns the number of Bytes actually read (increments the Position by this actually read number of Bytes)
		nCount=min(nCount,fileSize-position);
		const UINT nBytesToRead=nCount;
		CFatPath::PCItem item; DWORD n;
		if (!fatPath.GetItems(item,n)){
			div_t d=div((int)position,(int)dos->formatBoot.sectorLength-dos->properties->dataBeginOffsetInSector-dos->properties->dataEndOffsetInSector);
			item+=d.quot, n-=d.quot; // skipping Sectors from which not read
			bool readWithoutCrcError=true;
			TFdcStatus sr;
			for( WORD w; n--; item++ )
				if (const PCSectorData sectorData=dos->image->GetSectorData(item->chs,0,true,&w,&sr)){
					readWithoutCrcError&=sr.IsWithoutError();
					w-=d.rem+dos->properties->dataBeginOffsetInSector+dos->properties->dataEndOffsetInSector;
					if (w<nCount){
						lpBuf=(PBYTE)::memcpy(lpBuf,sectorData+dos->properties->dataBeginOffsetInSector+d.rem,w)+w;
						nCount-=w, position+=w, d.rem=0;
					}else{
						::memcpy(lpBuf,sectorData+dos->properties->dataBeginOffsetInSector+d.rem,nCount);
						position+=nCount;
						::SetLastError( readWithoutCrcError ? ERROR_SUCCESS : ERROR_CRC );
						return nBytesToRead;
					}
				}else
					break;
		}
		::SetLastError(ERROR_READ_FAULT);
		return nBytesToRead-nCount;
	}

	void CDos::CFileReaderWriter::Write(LPCVOID lpBuf,UINT nCount){
		// tries to write given NumberOfBytes from the Buffer to the current Position (increments the Position by the number of Bytes actually written)
		nCount=min(nCount,fileSize-position);
		CFatPath::PCItem item; DWORD n;
		if (!fatPath.GetItems(item,n)){
			div_t d=div((int)position,(int)dos->formatBoot.sectorLength-dos->properties->dataBeginOffsetInSector-dos->properties->dataEndOffsetInSector);
			item+=d.quot, n-=d.quot; // skipping Sectors into which not written
			TFdcStatus sr;
			for( WORD w; n--; item++ )
				if (const PSectorData sectorData=dos->image->GetSectorData(item->chs,0,false,&w,&sr)){ // False = freezing the state of data (eventually erroneous)
					w-=d.rem+dos->properties->dataBeginOffsetInSector+dos->properties->dataEndOffsetInSector;
					dos->image->MarkSectorAsDirty(item->chs,0,&sr);
					if (w<nCount){
						::memcpy(sectorData+dos->properties->dataBeginOffsetInSector+d.rem,lpBuf,w);
						lpBuf=(PCBYTE)lpBuf+w, nCount-=w, position+=w, d.rem=0;
					}else{
						::memcpy(sectorData+dos->properties->dataBeginOffsetInSector+d.rem,lpBuf,nCount);
						position+=nCount;
						return;
					}
				}else
					break;
		}
	}

	/*UINT CDos::CFileReaderWriter::GetBufferPtr(UINT nCommand,UINT nCount,PVOID *ppBufStart,PVOID *ppBufMax){
		// direct buffering support; for a given read/write request returns the number of Bytes actually available in the Buffer at current Position (increments the Position by this number of Bytes)
		switch (nCommand){
			case CFile::bufferCheck:
				return TRUE; // yes, direct buffering supported
			case CFile::bufferCommit:
				return 0; // not supported
			default:{
				// reading/writing is possible only within a single Sector; if inteded to read from/write to multiple Sectors, multiple calls to GetBufferPtr must be made
				ASSERT( ppBufStart!=NULL && ppBufMax!=NULL );
				CFatPath::PCItem item; DWORD n;
				if (!fatPath.GetItems(item,n)){
					const WORD usableSectorLength=dos->formatBoot.sectorLength-dos->properties->dataBeginOffsetInSector-dos->properties->dataEndOffsetInSector;
					const div_t d=div( (int)position, usableSectorLength );
					WORD w; TFdcStatus sr;
					if (const PSectorData sectorData=dos->image->GetSectorData( item[d.quot].chs, 0, false, &w, &sr )){
						*ppBufStart = sectorData+dos->properties->dataBeginOffsetInSector+d.rem;
						*ppBufMax = (PBYTE)*ppBufStart + min(nCount,usableSectorLength-d.rem);
						position+=( nCount=(PCBYTE)*ppBufMax-(PCBYTE)*ppBufStart );
					}else{
						*ppBufStart = *ppBufMax = NULL;
						nCount=0;
					}
					::SetLastError( !sr.IsWithoutError()*ERROR_SECTOR_NOT_FOUND );
					return nCount;
				}else
					return 0;
			}
		}
	}*/
