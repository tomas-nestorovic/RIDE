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
			for( WORD w; n--; item++ )
				if (const PCSectorData sectorData=dos->image->GetSectorData(item->chs,&w)){
					w-=d.rem+dos->properties->dataBeginOffsetInSector+dos->properties->dataEndOffsetInSector;
					if (w<nCount){
						lpBuf=(PBYTE)::memcpy(lpBuf,sectorData+dos->properties->dataBeginOffsetInSector+d.rem,w)+w;
						nCount-=w, position+=w, d.rem=0;
					}else{
						::memcpy(lpBuf,sectorData+dos->properties->dataBeginOffsetInSector+d.rem,nCount);
						position+=nCount;
						return nBytesToRead;
					}
				}else
					break;
		}
		return nBytesToRead-nCount;
	}

	void CDos::CFileReaderWriter::Write(LPCVOID lpBuf,UINT nCount){
		// tries to write given NumberOfBytes from the Buffer to the current Position (increments the Position by the number of Bytes actually written)
		nCount=min(nCount,fileSize-position);
		CFatPath::PCItem item; DWORD n;
		if (!fatPath.GetItems(item,n)){
			div_t d=div((int)position,(int)dos->formatBoot.sectorLength-dos->properties->dataBeginOffsetInSector-dos->properties->dataEndOffsetInSector);
			item+=d.quot, n-=d.quot; // skipping Sectors into which not written
			for( WORD w; n--; item++ )
				if (const PSectorData sectorData=dos->image->GetSectorData(item->chs,&w)){
					w-=d.rem+dos->properties->dataBeginOffsetInSector+dos->properties->dataEndOffsetInSector;
					dos->image->MarkSectorAsDirty(item->chs);
					if (w<nCount){
						::memcpy(sectorData+dos->properties->dataBeginOffsetInSector+d.rem,lpBuf,w);
						lpBuf=(PCBYTE)lpBuf+w, nCount-=w, position+=w, d.rem=0;
					}else{
						::memcpy(sectorData+dos->properties->dataBeginOffsetInSector+d.rem,lpBuf,nCount);
						position+=nCount;
						break;
					}
				}else
					break;
		}
	}
