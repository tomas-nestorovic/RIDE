#include "stdafx.h"

	const Yahel::TInterval<char> CImage::CSectorReaderWriter::NoPadding(0,0);

	CImage::CSectorReaderWriter::CSectorReaderWriter(PImage image,Yahel::TPosition dataTotalLength,const Yahel::TInterval<char> &padding,const BYTE &nDiscoveredRevolutions,FOnWritten onWritten)
		// ctor
		: image(image)
		, revolution(Revolution::ANY_GOOD)
		, onWritten(onWritten)
		, nDiscoveredRevolutions(nDiscoveredRevolutions) {
		this->dataTotalLength=dataTotalLength;
		static_cast<TPhysicalAddress &>(sector)=TPhysicalAddress::Invalid;
		sector.indexOnTrack=0, sector.offset=0;
		sector.padding=padding;
		badByteMask.flags=-1;
	}




#if _MFC_VER>=0x0A00
	ULONGLONG CImage::CSectorReaderWriter::Seek(LONGLONG lOff,UINT nFrom){
#else
	LONG CImage::CSectorReaderWriter::Seek(LONG lOff,UINT nFrom){
#endif
		// sets the actual Position
		const auto result=__super::Seek(lOff,nFrom);
		GetPhysicalAddress( result, sector, sector.indexOnTrack, &sector.offset );
		return result;
	}

	UINT CImage::CSectorReaderWriter::Read(LPVOID lpBuf,UINT nCount){
		// tries to read given NumberOfBytes into the Buffer, starting with current Position; returns the number of Bytes actually read (increments the Position by this actually read number of Bytes)
		nCount=std::min( nCount, UINT(dataTotalLength-position) );
		UINT nBytesToRead=nCount;
		bool readWithoutError=true; // assumption
		for( WORD w; true; ){
			const TPhysicalAddress &chs=GetCurrentPhysicalAddress();
			if (revolution==Revolution::ALL_INTERSECTED){
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
				w-=sector.offset+sector.padding.z;
				for( WORD i=sector.offset,const iEnd=i+std::min((UINT)w,nCount); i<iEnd; i++ ){
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
				if (revolution>=Revolution::MAX)
					revolution=Revolution::ANY_GOOD;
				TFdcStatus sr; PByteInfo pbi; // in/out
				const PCSectorData sectorData=image->GetSectorData( chs, sector.indexOnTrack, revolution, &w, &sr, nullptr, &pbi );
				if (!sectorData || !w){ // A|B, B = DAM not found, B = e.g. reading Sector with LengthCode 231 (has by default no data, a pointer to zero-length data has been returned by GetSectorData)
					// no data for this Sector
					nBytesToRead-=nCount;
					if (!nBytesToRead) // nothing read yet ?
						readWithoutError=false; // output nothing with a read error
					nCount=0; // output just what's been read thus far
				}else{
					// some (good or bad) data exist for this Sector
					w-=sector.offset+sector.padding.z;
					const WORD n=std::min( nCount, (UINT)w );
					if (pbi && revolution<Revolution::MAX){ // info on Bad Bytes applicable only to particular Revolution
						pbi+=sector.offset;
						for( w=0; w<n; w++ )
							if (pbi++->flags&badByteMask.flags){ // a relevantly Bad Byte encountered
								nBytesToRead+=w-nCount; // include just good part of this Sector and drop the rest of the query
								if (!nBytesToRead){ // nothing read yet ?
									nBytesToRead=++w; // output just this single Bad Byte
									readWithoutError=false;
								}
								nCount=w; // output just what's been read thus far
								break;
							}
					}else{
						readWithoutError=sr.IsWithoutError();
						w=n;
					}
					lpBuf=(PBYTE)::memcpy( lpBuf, sectorData+sector.offset, w )+w;
					Seek( w, SeekPosition::current ); // advance pointer
					nCount-=w;
				}
				if (!nCount){ // all Bytes read ?
					::SetLastError( readWithoutError ? ERROR_SUCCESS : ERROR_CRC );
					return nBytesToRead;
				}
			}
		}
		::SetLastError(ERROR_READ_FAULT);
		return nBytesToRead-nCount;
	}

	void CImage::CSectorReaderWriter::Write(LPCVOID lpBuf,UINT nCount){
		// tries to write given NumberOfBytes from the Buffer to the current Position (increments the Position by the number of Bytes actually written)
		nCount=std::min( nCount, UINT(dataTotalLength-position) );
		bool writtenWithoutCrcError=true; // assumption
		for( WORD w; true; ){
			const TPhysicalAddress &chs=GetCurrentPhysicalAddress();
			if (!chs) // invalid PhysicalAddress? (may be for descendants, e.g. error in FAT for FileReaderWriter)
				break;
			TFdcStatus sr; // in/out
			if (const PSectorData sectorData=image->GetSectorData(chs,sector.indexOnTrack,Revolution::CURRENT,&w,&sr)){ // Revolution.Current = freezing the state of data (eventually erroneous)
				if (!w) // e.g. writing Sector with LengthCode 231 - such Sector has by default no data (a pointer to zero-length data has been returned by GetSectorData)
					break;
				writtenWithoutCrcError&=sr.IsWithoutError();
				w-=sector.offset+sector.padding.z;
				const WORD n=std::min( nCount, (UINT)w );
				::memcpy( sectorData+sector.offset, lpBuf, n );
				image->MarkSectorAsDirty( chs, sector.indexOnTrack, &sr, true );
				if (onWritten)
					onWritten(  Yahel::TPosInterval( position, position+n )  );
				Seek( n, SeekPosition::current );
				lpBuf=(PCBYTE)lpBuf+n, nCount-=n;
				if (!nCount){
					::SetLastError( writtenWithoutCrcError ? ERROR_SUCCESS : ERROR_CRC );
					return;
				}
			}else
				break;
		}
		::SetLastError(ERROR_WRITE_FAULT);
	}

	BYTE CImage::CSectorReaderWriter::GetAvailableRevolutionCount(TCylinder cyl,THead head) const{
		// wrapper around CImage::GetAvailableRevolutionCount
		return	std::min( (BYTE)Revolution::MAX, image->GetAvailableRevolutionCount(cyl,head) );
	}

	CImage::CSectorReaderWriter::TScannerStatus CImage::CSectorReaderWriter::GetTrackScannerStatus(PCylinder pnOutScannedCyls) const{
		// returns Track scanner Status, if any
		return TScannerStatus::UNAVAILABLE; // no scanner needed - assumed all data are available (e.g. raw-sectored Image)
	}
	void CImage::CSectorReaderWriter::SetTrackScannerStatus(TScannerStatus status){
		// suspends/resumes Track scanner, if any (if none, simply ignores the request)
		//nop
	}

	TPhysicalAddress CImage::CSectorReaderWriter::GetPhysicalAddress(Yahel::TPosition pos) const{
		TPhysicalAddress chs; BYTE iSector;
		GetPhysicalAddress( pos, chs, iSector, nullptr );
		return chs;
	}

	LPCWSTR CImage::CSectorReaderWriter::GetRecordLabelW(Yahel::TPosition pos,PWCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const{
		// populates the Buffer with label for the Record that STARTS at specified LogicalPosition, and returns the Buffer; returns Null if no Record starts at specified LogicalPosition
		TPhysicalAddress chs; BYTE iSector; WORD offset;
		GetPhysicalAddress( pos, chs, iSector, &offset );
		if (!chs || offset!=-sector.padding.a) // start Padding provided as NEGATIVE!
			return nullptr;
		switch (const Revolution::TType dirtyRev=image->GetDirtyRevolution(chs,iSector)){
			case Revolution::NONE:
				#ifdef UNICODE
					return ::lstrcpyn( labelBuffer, chs.sectorId.ToString(), labelBufferCharsMax );
				#else
					::MultiByteToWideChar( CP_ACP, 0, chs.sectorId.ToString(),-1, labelBuffer,labelBufferCharsMax );
					return labelBuffer;
				#endif
			default:
				#ifdef UNICODE
					::wnsprintf( labelBuffer, labelBufferCharsMax, L"\x25d9Rev%d %s", dirtyRev+1, chs.sectorId.ToString() );
				#else
					::wnsprintfW( labelBuffer, labelBufferCharsMax, L"\x25d9Rev%d %S", dirtyRev+1, chs.sectorId.ToString() );
				#endif
				return labelBuffer;
		}
	}











	CImage::CSameLengthSectorReaderWriter::CSameLengthSectorReaderWriter(PImage image,Yahel::TPosition dataTotalLength,const Yahel::TInterval<char> &padding,const BYTE &nDiscoveredRevolutions,FOnWritten onWritten,const TSameLengthSectorParams &slsp)
		// ctor
		: CSectorReaderWriter( image, dataTotalLength, padding, nDiscoveredRevolutions, onWritten )
		, TSameLengthSectorParams(slsp)
		, usableSectorLength(slsp.sectorLength-padding.GetLength()) {
	}




	Yahel::TPosition CImage::CSameLengthSectorReaderWriter::GetSectorStartPosition(RCPhysicalAddress chs,BYTE nSectorsToSkip) const{
		// computes and returns the position of the first Byte of the Sector at the PhysicalAddress
		return Yahel::TPosition( chs.GetTrackNumber(image->GetHeadCount())*nSectors + chs.sectorId.sector-firstSectorNumber )*sectorLength;
	}

	void CImage::CSameLengthSectorReaderWriter::GetPhysicalAddress(Yahel::TPosition pos,TPhysicalAddress &outChs,BYTE &outSectorIndex,PWORD pOutOffset) const{
		// determines the PhysicalAddress that contains the specified LogicalPosition
		const auto &&s=div( pos, usableSectorLength ); // Quot = # of Sectors to skip, Rem = the first Byte to read in the Sector yet to be computed
		if (pOutOffset)
			*pOutOffset=s.rem-sector.padding.a; // start Padding provided as NEGATIVE!
		const auto &&t=div( s.quot, nSectors ); // Quot = # of Tracks to skip, Rem = the zero-based Sector index on a Track yet to be computed
		outSectorIndex=t.rem;
		const auto &&h=div( t.quot, image->GetHeadCount() ); // Quot = # of Cylinders to skip, Rem = Head in a Cylinder
		outChs.cylinder=h.quot;
		outChs.head=h.rem;
		outChs.sectorId.cylinder=h.quot;
		outChs.sectorId.side=image->sideMap[h.rem];
		outChs.sectorId.sector=firstSectorNumber+sector.indexOnTrack;
		outChs.sectorId.lengthCode=sectorLengthCode;
	}

	Yahel::TRow CImage::CSameLengthSectorReaderWriter::LogicalPositionToRow(Yahel::TPosition logPos,WORD nBytesInRow){
		// computes and returns the row containing the specified LogicalPosition
		const auto &&d=div( logPos, usableSectorLength );
		const auto nRowsPerRecord=Utils::RoundDivUp( usableSectorLength, nBytesInRow );
		return d.quot*nRowsPerRecord + d.rem/nBytesInRow;
	}

	Yahel::TPosition CImage::CSameLengthSectorReaderWriter::RowToLogicalPosition(Yahel::TRow row,WORD nBytesInRow){
		// converts Row begin (i.e. its first Byte) to corresponding logical position in underlying File and returns the result
		const auto nRowsPerRecord=Utils::RoundDivUp( usableSectorLength, nBytesInRow );
		const auto &&d=div( row, nRowsPerRecord );
		return d.quot*usableSectorLength + d.rem*nBytesInRow;
	}

	void CImage::CSameLengthSectorReaderWriter::GetRecordInfo(Yahel::TPosition logPos,Yahel::PPosition pOutRecordStartLogPos,Yahel::PPosition pOutRecordLength,bool *pOutDataReady){
		// retrieves the start logical position and length of the Record pointed to by the input LogicalPosition
		if (pOutRecordStartLogPos)
			*pOutRecordStartLogPos = logPos/usableSectorLength*usableSectorLength;
		if (pOutRecordLength)
			*pOutRecordLength = usableSectorLength;
		if (pOutDataReady)
			*pOutDataReady=true;
	}
