#include "stdafx.h"
using namespace Yahel;

	CDos::CFileReaderWriter::CFileReaderWriter(const CDos *dos,PCFile file,bool wholeSectors)
		// ctor to read/edit an existing File in Image
		: dos(dos) , sectorLength(dos->formatBoot.sectorLength) , fatPath(new CFatPath(dos,file))
		, fileSize( wholeSectors ? dos->GetFileSizeOnDisk(file) : dos->GetFileOccupiedSize(file) )
		, dataBeginOffsetInSector( wholeSectors ? 0 : dos->properties->dataBeginOffsetInSector)
		, dataEndOffsetInSector( wholeSectors ? 0 : dos->properties->dataEndOffsetInSector )
		, position(0)
		, recordLength(sectorLength) {
	}

	CDos::CFileReaderWriter::CFileReaderWriter(const CDos *dos,RCPhysicalAddress chs)
		// ctor to read/edit particular Sector in Image (e.g. Boot Sector)
		: dos(dos) , sectorLength(dos->formatBoot.sectorLength) , fatPath(new CFatPath(dos,chs))
		, fileSize(sectorLength)
		, dataBeginOffsetInSector(0) , dataEndOffsetInSector(0)
		, position(0)
		, recordLength(sectorLength) {
	}

	CDos::CFileReaderWriter::~CFileReaderWriter(){
		// dtor
	}








	#if _MFC_VER>=0x0A00
	ULONGLONG CDos::CFileReaderWriter::GetLength() const{
	#else
	DWORD CDos::CFileReaderWriter::GetLength() const{
	#endif
		// returns the File size
		return fileSize;
	}

	#if _MFC_VER>=0x0A00
	void CDos::CFileReaderWriter::SetLength(ULONGLONG dwNewLen){
	#else
	void CDos::CFileReaderWriter::SetLength(DWORD dwNewLen){
	#endif
		// overrides the reported FileSize
		ASSERT( dwNewLen<=fileSize ); // can only "shrink" the reported FileSize
		fileSize=dwNewLen;
		if (position>fileSize)
			position=fileSize;
	}

	#if _MFC_VER>=0x0A00
	ULONGLONG CDos::CFileReaderWriter::GetPosition() const{
	#else
	DWORD CDos::CFileReaderWriter::GetPosition() const{
	#endif
		// returns the actual Position in open File
		return position;
	}

	#if _MFC_VER>=0x0A00
	ULONGLONG CDos::CFileReaderWriter::Seek(LONGLONG lOff,UINT nFrom){
	#else
	LONG CDos::CFileReaderWriter::Seek(LONG lOff,UINT nFrom){
	#endif
		// sets the actual Position in open File
		switch ((SeekPosition)nFrom){
			case SeekPosition::current:
				lOff+=position;
				//fallthrough
			case SeekPosition::begin:
				position=std::min<>(lOff,fileSize);
				break;
			case SeekPosition::end:
				position=std::max<>( fileSize-lOff, (decltype(position))0 );
				break;
			default:
				ASSERT(FALSE);
		}
		return position;
	}

	UINT CDos::CFileReaderWriter::Read(LPVOID lpBuf,UINT nCount){
		// tries to read given NumberOfBytes into the Buffer, starting with current Position; returns the number of Bytes actually read (increments the Position by this actually read number of Bytes)
		nCount=std::min<UINT>(nCount,fileSize-position);
		const UINT nBytesToRead=nCount;
		CFatPath::PCItem item; DWORD n;
		if (!fatPath->GetItems(item,n)){
			div_t d=div((int)position,(int)sectorLength-dataBeginOffsetInSector-dataEndOffsetInSector);
			item+=d.quot, n-=d.quot; // skipping Sectors from which not read
			bool readWithoutCrcError=true;
			TFdcStatus sr;
			for( WORD w; n--; item++ )
				if (const PCSectorData sectorData=dos->image->GetSectorData(item->chs,0,Revolution::ANY_GOOD,&w,&sr)){
					readWithoutCrcError&=sr.IsWithoutError();
					w-=d.rem+dataBeginOffsetInSector+dataEndOffsetInSector;
					if (w<nCount){
						lpBuf=(PBYTE)::memcpy(lpBuf,sectorData+dataBeginOffsetInSector+d.rem,w)+w;
						nCount-=w, position+=w, d.rem=0;
					}else{
						::memcpy(lpBuf,sectorData+dataBeginOffsetInSector+d.rem,nCount);
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
		nCount=std::min<UINT>(nCount,fileSize-position);
		CFatPath::PCItem item; DWORD n;
		if (!fatPath->GetItems(item,n)){
			div_t d=div((int)position,(int)sectorLength-dataBeginOffsetInSector-dataEndOffsetInSector);
			item+=d.quot, n-=d.quot; // skipping Sectors into which not written
			bool writtenWithoutCrcError=true;
			TFdcStatus sr;
			for( WORD w; n--; item++ )
				if (const PSectorData sectorData=dos->image->GetSectorData(item->chs,0,Revolution::CURRENT,&w,&sr)){ // Revolution.Current = freezing the state of data (eventually erroneous)
					writtenWithoutCrcError&=sr.IsWithoutError();
					w-=d.rem+dataBeginOffsetInSector+dataEndOffsetInSector;
					dos->image->MarkSectorAsDirty(item->chs,0,&sr);
					if (w<nCount){
						::memcpy(sectorData+dataBeginOffsetInSector+d.rem,lpBuf,w);
						lpBuf=(PCBYTE)lpBuf+w, nCount-=w, position+=w, d.rem=0;
					}else{
						::memcpy(sectorData+dataBeginOffsetInSector+d.rem,lpBuf,nCount);
						position+=nCount;
						::SetLastError( writtenWithoutCrcError ? ERROR_SUCCESS : ERROR_CRC );
						return;
					}
				}else
					break;
		}
		::SetLastError(ERROR_WRITE_FAULT);
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
				ASSERT( ppBufStart!=nullptr && ppBufMax!=nullptr );
				CFatPath::PCItem item; DWORD n;
				if (!fatPath.GetItems(item,n)){
					const WORD usableSectorLength=dos->formatBoot.sectorLength-dataBeginOffsetInSector-dataEndOffsetInSector;
					const div_t d=div( (int)position, usableSectorLength );
					WORD w; TFdcStatus sr;
					if (const PSectorData sectorData=dos->image->GetSectorData( item[d.quot].chs, 0, false, &w, &sr )){
						*ppBufStart = sectorData+dataBeginOffsetInSector+d.rem;
						*ppBufMax = (PBYTE)*ppBufStart + min(nCount,usableSectorLength-d.rem);
						position+=( nCount=(PCBYTE)*ppBufMax-(PCBYTE)*ppBufStart );
					}else{
						*ppBufStart = *ppBufMax = nullptr;
						nCount=0;
					}
					::SetLastError( !sr.IsWithoutError()*ERROR_SECTOR_NOT_FOUND );
					return nCount;
				}else
					return 0;
			}
		}
	}*/

	const TPhysicalAddress &CDos::CFileReaderWriter::GetCurrentPhysicalAddress() const{
		// returns the PhysicalAddress currently seeked to
		const div_t d=div( (int)position, (int)sectorLength-dataBeginOffsetInSector-dataEndOffsetInSector );
		if (const CDos::CFatPath::PCItem p=fatPath->GetItem(d.quot)) // Sector exists
			return p->chs;
		else
			return TPhysicalAddress::Invalid;
	}

	WORD CDos::CFileReaderWriter::GetPositionInCurrentSector() const{
		//
		const div_t d=div( (int)position, (int)sectorLength-dataBeginOffsetInSector-dataEndOffsetInSector );
		return dataBeginOffsetInSector+d.rem;
	}








	HRESULT CDos::CFileReaderWriter::Clone(IStream **ppstm){
		if (ppstm){
			*ppstm=new CFileReaderWriter(*this);
			return S_OK;
		}else
			return E_INVALIDARG;
	}








	void CDos::CFileReaderWriter::GetRecordInfo(TPosition logPos,PPosition pOutRecordStartLogPos,PPosition pOutRecordLength,bool *pOutDataReady){
		// retrieves the start logical position and length of the Record pointed to by the input LogicalPosition
		if (pOutRecordStartLogPos)
			*pOutRecordStartLogPos = logPos/recordLength * recordLength;
		if (pOutRecordLength)
			*pOutRecordLength = recordLength;
		if (pOutDataReady)
			*pOutDataReady=true;
	}

	TRow CDos::CFileReaderWriter::LogicalPositionToRow(TPosition logPos,WORD nBytesInRow){
		// computes and returns the row containing the specified LogicalPosition
		const auto d=div( logPos, (TPosition)recordLength );
		const TRow nRowsPerRecord = (recordLength+nBytesInRow-1)/nBytesInRow;
		return d.quot*nRowsPerRecord + d.rem/nBytesInRow;// + (d.rem+nBytesInRow-1)/nBytesInRow;
	}

	TPosition CDos::CFileReaderWriter::RowToLogicalPosition(TRow row,WORD nBytesInRow){
		// converts Row begin (i.e. its first Byte) to corresponding logical position in underlying File and returns the result
		const TRow nRowsPerRecord = (recordLength+nBytesInRow-1)/nBytesInRow;
		const auto d=div( row, nRowsPerRecord );
		return d.quot*recordLength + d.rem*nBytesInRow;
	}

	LPCWSTR CDos::CFileReaderWriter::GetRecordLabelW(TPosition logPos,PWCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const{
		// populates the Buffer with label for the Record that STARTS at specified LogicalPosition, and returns the Buffer; returns Null if no Record starts at specified LogicalPosition
		const auto d=div( logPos, (TPosition)recordLength );
		if (!d.rem){
			CDos::CFatPath::PCItem pItem; DWORD nItems;
			#ifdef UNICODE
				if (const LPCTSTR err=fatPath.GetItems(pItem,nItems))
					return err;
				else
					return ::lstrcpyn( labelBuffer, (pItem+d.quot)->chs.sectorId.ToString(), labelBufferCharsMax );
			#else
				if (const LPCTSTR err=fatPath->GetItems(pItem,nItems))
					::MultiByteToWideChar( CP_ACP, 0, err,-1, labelBuffer,labelBufferCharsMax );
				else
					::MultiByteToWideChar( CP_ACP, 0, (pItem+d.quot)->chs.sectorId.ToString(),-1, labelBuffer,labelBufferCharsMax );
				return labelBuffer;
			#endif
		}else
			return nullptr;
	}









	CDos::CFileReaderWriter::CHexaEditor::CHexaEditor(PVOID param)
		// ctor
		// - base
		: ::CHexaEditor(param) {
		// - modification of ContextMenu
		contextMenu.AppendSeparator();
		contextMenu.AppendMenu( MF_BYCOMMAND|MF_STRING, ID_TIME, _T("Timing under cursor...") );
	}

	int CDos::CFileReaderWriter::CHexaEditor::GetCustomCommandMenuFlags(WORD cmd) const{
		// custom command GUI update
		const CFileReaderWriter &frw=*(const CFileReaderWriter *)GetCurrentStream().p;
		switch (cmd){
			case ID_TIME:
				return MF_GRAYED*!( frw.dos->image->ReadTrack(0,0) );
		}
		return __super::GetCustomCommandMenuFlags(cmd);
	}

	bool CDos::CFileReaderWriter::CHexaEditor::ProcessCustomCommand(UINT cmd){
		// custom command processing
		CFileReaderWriter &frw=*(CFileReaderWriter *)GetCurrentStream().p;
		switch (cmd){
			case ID_TIME:
				frw.Seek( instance->GetCaretPosition(), CFile::begin );
				frw.dos->image->ShowModalTrackTimingAt(
					frw.GetCurrentPhysicalAddress(),
					0, // no Sectors with duplicated ID fields are expected for any File!
					frw.GetPositionInCurrentSector(),
					Revolution::ANY_GOOD
				);
				return true;
		}
		return __super::ProcessCustomCommand(cmd);
	}
