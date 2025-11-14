#include "stdafx.h"
#include "ViewFatHexa.h"
using namespace Yahel;

	#define IMAGE	tab.image
	#define DOS		IMAGE->dos

	CFatHexaView::CFatHexaView(PDos dos,CDos::PFile file,LPCWSTR itemDefinition)
		// ctor
		// - base
		: CHexaEditor(this)
		// - initialization
		, file(file)
		, tab(0,0,0,dos->image,this) {
		// - setting up YAHEL
		RedefineItem( itemDefinition );
		ShowColumns( IInstance::TColumn::VIEW );
		SetEditable( !IMAGE->IsWriteProtected() );
	}

	LRESULT CFatHexaView::WindowProc(UINT msg,WPARAM wParam,LPARAM lParam){
		// window procedure
		switch (msg){
			case WM_CREATE:
				// window created
				// . reflecting write-protection into the look of controls
				SetEditable( !IMAGE->IsWriteProtected() );
				// . reinitializing content
				pFatData.Attach( new CFatPathReaderWriter(DOS,file) );
				Update( pFatData, pFatData, pFatData->GetLength() );
				// . base
				break;
		}
		return __super::WindowProc( msg, wParam, lParam );
	}

	bool CFatHexaView::ProcessCustomCommand(UINT cmd){
		// custom command processing
		switch (cmd){
			case ID_IMAGE_PROTECT:
				// toggles Image's WriteProtection flag
				IMAGE->ToggleWriteProtection(); // "base"
				SetEditable( !IMAGE->IsWriteProtected() );
				return true;
			case ID_FILE_CLOSE:
				CTdiCtrl::RemoveCurrentTab( TDI_HWND );
				return true;
		}
		return __super::ProcessCustomCommand(cmd);
	}









	typedef CDos::CFatPath::PItem	PFatItem;
	typedef CDos::CFatPath::PCItem	PCFatItem;

	CFatHexaView::CFatPathReaderWriter::CFatPathReaderWriter(PCDos dos,CDos::PFile file)
		// ctor
		: dos(dos) , file(file)
		, fatPath( CDos::CFileReaderWriter( dos, file ).fatPath ) {
		dataTotalLength= fatPath.GetNumberOfItems()*sizeof(CDos::CFatPath::TValue);
	}



	// IStream methods
	HRESULT CFatHexaView::CFatPathReaderWriter::Clone(IStream **ppstm){
		// creates an exact copy of this object
		if (ppstm){
			*ppstm=new CFatPathReaderWriter(*this);
			return S_OK;
		}else
			return E_INVALIDARG;
	}

	// CFile methods
#if _MFC_VER>=0x0A00
	void CFatHexaView::CFatPathReaderWriter::SetLength(ULONGLONG dwNewLen){
#else
	void CFatHexaView::CFatPathReaderWriter::SetLength(DWORD dwNewLen){
#endif
		// overrides the reported FileSize
		ASSERT(FALSE); // not supported, use other ways to modify the FatPath length!
	}

	UINT CFatHexaView::CFatPathReaderWriter::Read(LPVOID lpBuf,UINT nCount){
		// tries to read given NumberOfBytes into the Buffer, starting with current Position; returns the number of Bytes actually read (increments the Position by this actually read number of Bytes)
		PCFatItem item; DWORD n;
		fatPath.GetItems( item, n );
		const auto d=div( position, sizeof(item->value) );
		if (d.quot>=n) // reading beyond the content?
			return 0;
		item+=d.quot, n-=d.quot; // skip Items from which not read
		nCount=std::min<UINT>( nCount, dataTotalLength-position );
		PBYTE p=(PBYTE)lpBuf;
		if (d.rem){ // initial reading of an incomplete Item?
			::memcpy( p, ((PCBYTE)&item++->value)+d.rem, n=std::min<UINT>(sizeof(item->value)-d.rem,nCount) );
			p+=n, nCount-=n; // note resetting N above
		}
		while (nCount>=sizeof(item->value)){ // reading complete Items
			::memcpy( p, &item++->value, sizeof(item->value) );
			p+=sizeof(item->value), nCount-=sizeof(item->value);
		}
		if (nCount){ // final reading of an incomplete Item?
			::memcpy( p, &item->value, nCount );
			p+=nCount;
		}
		const auto nBytesRead=p-(PBYTE)lpBuf;
		position+=nBytesRead;
		return nBytesRead;
	}

	void CFatHexaView::CFatPathReaderWriter::Write(LPCVOID lpBuf,UINT nCount){
		// tries to write given NumberOfBytes from the Buffer to the current Position (increments the Position by the number of Bytes actually written)
		// - modifying the FatPath locally
		PFatItem item; DWORD n;
		fatPath.GetItems( item, n );
		const auto d=div( position, sizeof(item->value) );
		if (d.quot>=n) // writing beyond the content?
			return;
		item+=d.quot, n-=d.quot; // skip Items to which not written
		nCount=std::min<UINT>( nCount, dataTotalLength-position );
		PCBYTE p=(PCBYTE)lpBuf;
		if (d.rem){ // initial writing of an incomplete Item?
			::memcpy( ((PBYTE)&item->value)+d.rem, p, n=std::min<UINT>(sizeof(item->value)-d.rem,nCount) );
			p+=n, nCount-=n; // note resetting N above
			item++->chs.Invalidate(); // let the DOS determine the Sector from the new Value
		}
		while (nCount>=sizeof(item->value)){ // writing complete Items
			::memcpy( &item->value, p, sizeof(item->value) );
			p+=sizeof(item->value), nCount-=sizeof(item->value);
			item++->chs.Invalidate(); // let the DOS determine the Sector from the new Value
		}
		if (nCount){ // final writing of an incomplete Item?
			::memcpy( &item->value, p, nCount );
			p+=nCount;
			item->chs.Invalidate(); // let the DOS determine the Sector from the new Value
		}
		position+=p-(PBYTE)lpBuf;
		// - assigning the modified FatPath to the File
		dos->ModifyFileFatPath( file, fatPath );
	}



	// Yahel::Stream::IAdvisor methods
	void CFatHexaView::CFatPathReaderWriter::GetRecordInfo(TPosition logPos,PPosition pOutRecordStartLogPos,PPosition pOutRecordLength,bool *pOutDataReady){
		// retrieves the start logical position and length of the Record pointed to by the input LogicalPosition
		if (pOutRecordStartLogPos)
			*pOutRecordStartLogPos = 0;
		if (pOutRecordLength)
			*pOutRecordLength = dataTotalLength;
		if (pOutDataReady)
			*pOutDataReady=true;
	}

	TRow CFatHexaView::CFatPathReaderWriter::LogicalPositionToRow(TPosition logPos,WORD nBytesInRow){
		// computes and returns the row containing the specified LogicalPosition
		if (dataTotalLength){
			const auto d=div( logPos, dataTotalLength );
			const TRow nRowsPerRecord=Utils::RoundDivUp( dataTotalLength, (TPosition)nBytesInRow );
			return d.quot*nRowsPerRecord + d.rem/nBytesInRow;
		}else
			return 0;
	}

	TPosition CFatHexaView::CFatPathReaderWriter::RowToLogicalPosition(TRow row,WORD nBytesInRow){
		// converts Row begin (i.e. its first Byte) to corresponding logical position in underlying File and returns the result
		if (const auto nRowsPerRecord=Utils::RoundDivUp( dataTotalLength, (TPosition)nBytesInRow )){
			const auto d=div( row, nRowsPerRecord );
			return d.quot*dataTotalLength + d.rem*nBytesInRow;
		}else
			return 0;
	}

	LPCWSTR CFatHexaView::CFatPathReaderWriter::GetRecordLabelW(TPosition pos,PWCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const{
		// populates the Buffer with label for the Record that STARTS at specified LogicalPosition, and returns the Buffer; returns Null if no Record starts at specified LogicalPosition
		return nullptr;
	}
