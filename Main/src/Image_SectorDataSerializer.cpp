#include "stdafx.h"

	CImage::CSectorDataSerializer::CSectorDataSerializer(CHexaEditor *pParentHexaEditor,PImage image,LONG dataTotalLength,const BYTE &nDiscoveredRevolutions)
		// ctor
		: pParentHexaEditor(pParentHexaEditor) , image(image) , currTrack(0)
		, nDiscoveredRevolutions(nDiscoveredRevolutions)
		, revolution(Revolution::ANY_GOOD) {
		this->dataTotalLength=dataTotalLength;
		sector.indexOnTrack=0, sector.offset=0;
	}









	UINT CImage::CSectorDataSerializer::Read(LPVOID lpBuf,UINT nCount){
		// tries to read given NumberOfBytes into the Buffer, starting with current Position; returns the number of Bytes actually read (increments the Position by this actually read number of Bytes)
		nCount=std::min<UINT>( nCount, dataTotalLength-position );
		const UINT nBytesToRead=nCount;
		for( WORD w; true; )
			if (revolution==Revolution::ALL_INTERSECTED){
				const TPhysicalAddress chs=GetCurrentPhysicalAddress();
				const BYTE nAvailableRevolutions=GetAvailableRevolutionCount(chs.cylinder,chs.head);
				PCSectorData data[Revolution::MAX];
				bool allRevolutionsIdentical=true; // assumption
				for( BYTE rev=0; rev<nAvailableRevolutions; rev++ ){
					data[rev]=image->GetSectorData( chs, sector.indexOnTrack, (Revolution::TType)rev, &w );
					allRevolutionsIdentical&=data[rev]!=nullptr;
				}
				if (!w) // e.g. reading Sector with LengthCode 231 - such Sector has by default no data (a pointer to zero-length data has been returned by GetSectorData)
					break;
				if (!allRevolutionsIdentical) // at least one Revolution didn't return any data for the Sector
					if (nCount<nBytesToRead) // already read something?
						return nBytesToRead-nCount; // returning only the ok part to not mix readable and unreadable data
					else
						break;
				w-=sector.offset;
				for( WORD i=sector.offset,const iEnd=i+std::min<UINT>(w,nCount); i<iEnd; i++ ){
					const BYTE reference=data[0][i];
					for( BYTE rev=1; rev<nAvailableRevolutions; allRevolutionsIdentical&=data[rev++][i]==reference );
					if (allRevolutionsIdentical)
						*(PBYTE)lpBuf=reference, lpBuf=(PBYTE)lpBuf+sizeof(reference);
					else{
						const WORD nIdentical=i-sector.offset;
						Seek( nIdentical, SeekPosition::current );
						::SetLastError(ERROR_SUCCESS);
						return nIdentical+nBytesToRead-nCount; // returning only the ok part to not mix readable and unreadable data
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
			}else{
				const TPhysicalAddress chs=GetCurrentPhysicalAddress();
				if (revolution>=Revolution::MAX)
					revolution=Revolution::ANY_GOOD;
				TFdcStatus sr; // in/out
				const PCSectorData sectorData=image->GetSectorData( chs, sector.indexOnTrack, revolution, &w, &sr );
				if (!sectorData)
					break;
				if (!w) // e.g. reading Sector with LengthCode 231 - such Sector has by default no data (a pointer to zero-length data has been returned by GetSectorData)
					break;
				w-=sector.offset;
				if (sr.IsWithoutError()){
					if (w<nCount){
						lpBuf=(PBYTE)::memcpy(lpBuf,sectorData+sector.offset,w)+w;
						nCount-=w;
						Seek(w,SeekPosition::current);
					}else{
						::memcpy(lpBuf,sectorData+sector.offset,nCount);
						Seek(nCount,SeekPosition::current);
						::SetLastError( ERROR_SUCCESS );
						return nBytesToRead;
					}
				}else
					if (nCount<nBytesToRead){ // already read something?
						::SetLastError( ERROR_SUCCESS );
						return nBytesToRead-nCount; // returning only the ok part to not mix readable and unreadable data
					}else{
						const UINT len=std::min<UINT>( nCount, w );
						::memcpy( lpBuf, sectorData+sector.offset, len );
						Seek( len, SeekPosition::current );
						::SetLastError( ERROR_CRC );
						return len; // bad data are to be retrieved in individual chunks
					}
			}
		::SetLastError(ERROR_READ_FAULT);
		return nBytesToRead-nCount;
	}

	void CImage::CSectorDataSerializer::Write(LPCVOID lpBuf,UINT nCount){
		// tries to write given NumberOfBytes from the Buffer to the current Position (increments the Position by the number of Bytes actually written)
		nCount=std::min<UINT>( nCount, dataTotalLength-position );
		bool writtenWithoutCrcError=true; // assumption
		for( WORD w; true; ){
			const TPhysicalAddress chs=GetCurrentPhysicalAddress();
			TFdcStatus sr; // in/out
			if (const PSectorData sectorData=image->GetSectorData(chs,sector.indexOnTrack,Revolution::CURRENT,&w,&sr)){ // Revolution.Current = freezing the state of data (eventually erroneous)
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

	HRESULT CImage::CSectorDataSerializer::Clone(IStream **ppstm){
		if (ppstm){
			*ppstm=image->CreateSectorDataSerializer(pParentHexaEditor);
			return S_OK;
		}else
			return E_INVALIDARG;
	}

	BYTE CImage::CSectorDataSerializer::GetCurrentSectorIndexOnTrack() const{
		// returns the zero-based index of current Sector on the Track
		return sector.indexOnTrack;
	}

	BYTE CImage::CSectorDataSerializer::GetAvailableRevolutionCount(TCylinder cyl,THead head) const{
		// wrapper around CImage::GetAvailableRevolutionCount
		return	std::min( (BYTE)Revolution::MAX, image->GetAvailableRevolutionCount(cyl,head) );
	}
