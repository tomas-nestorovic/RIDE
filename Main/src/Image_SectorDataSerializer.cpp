#include "stdafx.h"

	CImage::CSectorDataSerializer::CSectorDataSerializer(CHexaEditor *pParentHexaEditor,PImage image,LONG dataTotalLength)
		// ctor
		: pParentHexaEditor(pParentHexaEditor) , image(image) , dataTotalLength(dataTotalLength) , position(0) , currTrack(0)
		, nAvailableRevolutions( image->GetAvailableRevolutionCount() )
		, revolution(Revolution::ANY_GOOD) {
		sector.indexOnTrack=0, sector.offset=0;
	}









	#if _MFC_VER>=0x0A00
	ULONGLONG CImage::CSectorDataSerializer::GetLength() const{
	#else
	DWORD CImage::CSectorDataSerializer::GetLength() const{
	#endif
		// returns the File size
		return dataTotalLength;
	}

	#if _MFC_VER>=0x0A00
	void CImage::CSectorDataSerializer::SetLength(ULONGLONG dwNewLen){
	#else
	void CImage::CSectorDataSerializer::SetLength(DWORD dwNewLen){
	#endif
		// overrides the reported DataTotalLength
		ASSERT( dwNewLen<=dataTotalLength ); // can only "shrink" the reported DataTotalLength
		dataTotalLength=dwNewLen;
		if (position>dataTotalLength)
			position=dataTotalLength;
	}

	#if _MFC_VER>=0x0A00
	ULONGLONG CImage::CSectorDataSerializer::GetPosition() const{
	#else
	DWORD CImage::CSectorDataSerializer::GetPosition() const{
	#endif
		// returns the actual Position in the Serializer
		return position;
	}

	#if _MFC_VER>=0x0A00
	ULONGLONG CImage::CSectorDataSerializer::Seek(LONGLONG lOff,UINT nFrom){
	#else
	LONG CImage::CSectorDataSerializer::Seek(LONG lOff,UINT nFrom){
	#endif
		// sets the actual Position in the Serializer
		switch ((SeekPosition)nFrom){
			case SeekPosition::current:
				lOff+=position;
				//fallthrough
			case SeekPosition::begin:
				position=std::min<>(lOff,dataTotalLength);
				break;
			case SeekPosition::end:
				position=std::max<>( dataTotalLength-lOff, (decltype(position))0 );
				break;
			default:
				ASSERT(FALSE);
		}
		return position;
	}

	UINT CImage::CSectorDataSerializer::Read(LPVOID lpBuf,UINT nCount){
		// tries to read given NumberOfBytes into the Buffer, starting with current Position; returns the number of Bytes actually read (increments the Position by this actually read number of Bytes)
		nCount=std::min<UINT>( nCount, dataTotalLength-position );
		const UINT nBytesToRead=nCount;
		bool readWithoutCrcError=true; // assumption
		WORD w; TFdcStatus sr;
		while (true)
			if (revolution==Revolution::ALL_INTERSECTED){
				const TPhysicalAddress chs=GetCurrentPhysicalAddress();
				PCSectorData data[Revolution::MAX];
				for( BYTE rev=0; rev<nAvailableRevolutions; rev++ ){
					data[rev]=image->GetSectorData( chs, (Revolution::TType)rev, sector.indexOnTrack, false, &w, &sr );
					readWithoutCrcError&=data[rev]!=nullptr;
				}
				if (!w) // e.g. reading Sector with LengthCode 231 - such Sector has by default no data (a pointer to zero-length data has been returned by GetSectorData)
					break;
				if (!readWithoutCrcError) // at least one Revolution didn't return any data for the Sector
					if (nCount<nBytesToRead) // some Bytes already read?
						return nBytesToRead-nCount; // keeping the error of already read data
					else
						break;
				w-=sector.offset;
				for( UINT n=0,N=std::min<UINT>(w,nCount); n<N; n++ ){
					const BYTE reference=data[0][sector.offset];
					for( BYTE rev=1; rev<nAvailableRevolutions; readWithoutCrcError&=data[rev++][sector.offset]==reference );
					if (readWithoutCrcError)
						*(PBYTE)lpBuf=reference, lpBuf=(PBYTE)lpBuf+sizeof(reference), sector.offset++;
					else{
						Seek(n,SeekPosition::current);
						::SetLastError(ERROR_SUCCESS); // first n Bytes are ok
						return n+nBytesToRead-nCount;
					}
				}
				if (w<nCount){
					nCount-=w;
					Seek(w,SeekPosition::current);
				}else{
					Seek(nCount,SeekPosition::current);
					::SetLastError( ERROR_SUCCESS );
					return nBytesToRead;
				}
			}else if (const PCSectorData sectorData=image->GetSectorData(GetCurrentPhysicalAddress(),revolution,sector.indexOnTrack,true,&w,&sr)){ // True = attempting to recover from errors despite many small read requests are made, potentially leading to a floppy calibration overhead
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
		nCount=std::min<UINT>( nCount, dataTotalLength-position );
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

	BYTE CImage::CSectorDataSerializer::GetCurrentSectorIndexOnTrack() const{
		// returns the zero-based index of current Sector on the Track
		return sector.indexOnTrack;
	}
