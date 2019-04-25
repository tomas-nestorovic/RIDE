#include "stdafx.h"

	CImage::CSectorDataSerializer::CSectorDataSerializer(PImage image,DWORD dataTotalLength)
		// ctor
		: image(image) , dataTotalLength(dataTotalLength) , position(0) {
	}









	DWORD CImage::CSectorDataSerializer::GetLength() const{
		// returns the File size
		return dataTotalLength;
	}

	void CImage::CSectorDataSerializer::SetLength(DWORD dwNewLen){
		// overrides the reported DataTotalLength
		ASSERT( dwNewLen<=dataTotalLength ); // can only "shrink" the reported DataTotalLength
		dataTotalLength=dwNewLen;
		if (position>dataTotalLength)
			position=dataTotalLength;
	}

	DWORD CImage::CSectorDataSerializer::GetPosition() const{
		// returns the actual Position in the Serializer
		return position;
	}

	LONG CImage::CSectorDataSerializer::Seek(LONG lOff,UINT nFrom){
		// sets the actual Position in the Serializer
		switch ((SeekPosition)nFrom){
			case SeekPosition::current:
				lOff+=position;
				//fallthrough
			case SeekPosition::begin:
				position=min(lOff,dataTotalLength);
				break;
			case SeekPosition::end:
				position=max(dataTotalLength-lOff,0);
				break;
			default:
				ASSERT(FALSE);
		}
		return position;
	}

	UINT CImage::CSectorDataSerializer::Read(LPVOID lpBuf,UINT nCount){
		// tries to read given NumberOfBytes into the Buffer, starting with current Position; returns the number of Bytes actually read (increments the Position by this actually read number of Bytes)
		nCount=min( nCount, dataTotalLength-position );
		const UINT nBytesToRead=nCount;
		bool readWithoutCrcError=true; // assumption
		WORD w; TFdcStatus sr;
		while (true)
			if (const PCSectorData sectorData=image->GetSectorData(sector.chs,0,true,&w,&sr)){
				readWithoutCrcError&=sr.IsWithoutError();
				w-=sector.offset;
				if (w<nCount){
					lpBuf=(PBYTE)::memcpy(lpBuf,sectorData+sector.offset,w)+w;
					nCount-=w;
					Seek(w,SeekPosition::current);
				}else{
					::memcpy(lpBuf,sectorData+sector.offset,nCount);
					Seek(nCount,SeekPosition::current);
					::SetLastError( readWithoutCrcError ? ERROR_SUCCESS : ERROR_CRC );
					return nBytesToRead;
				}
			}else
				break;
		::SetLastError(ERROR_READ_FAULT);
		return nBytesToRead-nCount;
	}

	void CImage::CSectorDataSerializer::Write(LPCVOID lpBuf,UINT nCount){
		// tries to write given NumberOfBytes from the Buffer to the current Position (increments the Position by the number of Bytes actually written)
		nCount=min( nCount, dataTotalLength-position );
		WORD w; TFdcStatus sr;
		while (true)
			if (const PSectorData sectorData=image->GetSectorData(sector.chs,0,false,&w,&sr)){ // False = freezing the state of data (eventually erroneous)
				image->MarkSectorAsDirty(sector.chs,0,&sr);
				w-=sector.offset;
				if (w<nCount){
					::memcpy(sectorData+sector.offset,lpBuf,w);
					lpBuf=(PCBYTE)lpBuf+w, nCount-=w;
					Seek(w,SeekPosition::current);
				}else{
					::memcpy(sectorData+sector.offset,lpBuf,nCount);
					Seek(nCount,SeekPosition::current);
					return;
				}
			}else
				break;
	}
