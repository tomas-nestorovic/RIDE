#include "stdafx.h"

	CImage::CSectorDataSerializer::CSectorDataSerializer(CHexaEditor *pParentHexaEditor,PImage image,LONG dataTotalLength)
		// ctor
		: pParentHexaEditor(pParentHexaEditor) , image(image) , dataTotalLength(dataTotalLength) , position(0) , currTrack(0) {
		sector.indexOnTrack=0, sector.offset=0;
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
			if (const PCSectorData sectorData=image->GetSectorData(GetCurrentPhysicalAddress(),sector.indexOnTrack,true,&w,&sr)){
				if (!w) // e.g. reading Sector with LengthCode 231 - such Sector has by default no data (a pointer to zero-length data has been returned by GetSectorData)
					break;
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
		bool writtenWithoutCrcError=true; // assumption
		WORD w; TFdcStatus sr;
		while (true){
			const TPhysicalAddress chs=GetCurrentPhysicalAddress();
			if (const PSectorData sectorData=image->GetSectorData(chs,sector.indexOnTrack,false,&w,&sr)){ // False = freezing the state of data (eventually erroneous)
				if (!w) // e.g. reading Sector with LengthCode 231 - such Sector has by default no data (a pointer to zero-length data has been returned by GetSectorData)
					break;
				writtenWithoutCrcError&=sr.IsWithoutError();
				image->MarkSectorAsDirty(chs,sector.indexOnTrack,&sr);
				w-=sector.offset;
				if (w<nCount){
					::memcpy(sectorData+sector.offset,lpBuf,w);
					lpBuf=(PCBYTE)lpBuf+w, nCount-=w;
					Seek(w,SeekPosition::current);
				}else{
					::memcpy(sectorData+sector.offset,lpBuf,nCount);
					Seek(nCount,SeekPosition::current);
					::SetLastError( writtenWithoutCrcError ? ERROR_SUCCESS : ERROR_CRC );
					return;
				}
			}else
				break;
		}
		::SetLastError(ERROR_WRITE_FAULT);
	}

	TTrack CImage::CSectorDataSerializer::GetCurrentSectorIndexOnTrack() const{
		// returns the zero-based index of current Sector on the Track
		return sector.indexOnTrack;
	}
