#include "stdafx.h"

	static const TRev nDiscoveredRevolutions=1;

	CDos::CFileReaderWriter::CFileReaderWriter(const CDos *dos,PCFile file,bool wholeSectors)
		// ctor to read/edit an existing File in Image
		: Sector::CSameLengthReaderWriter(
				dos->image,
				wholeSectors ? dos->GetFileSizeOnDisk(file) : dos->GetFileOccupiedSize(file),
				Yahel::TInterval<char>(
					wholeSectors ? 0 : -dos->properties->dataBeginOffsetInSector, // start Padding provided as NEGATIVE!
					wholeSectors ? 0 : dos->properties->dataEndOffsetInSector
				),
				nDiscoveredRevolutions,
				nullptr,
				Sector::TSameLengthParams( dos->formatBoot.nSectors, dos->formatBoot.sectorLength )
			)
		, fatPath(dos,file) {
		badByteMask.flags=0, badByteMask.badEncoding=true; // ignore other "badness"
	}

	CDos::CFileReaderWriter::CFileReaderWriter(const CDos *dos,RCPhysicalAddress chs,FOnWritten onWritten)
		// ctor to read/edit particular Sector in Image (e.g. Boot Sector)
		: Sector::CSameLengthReaderWriter(
				dos->image,
				dos->formatBoot.sectorLength,
				NoPadding,
				nDiscoveredRevolutions,
				onWritten,
				Sector::TSameLengthParams( 1, dos->formatBoot.sectorLength )
			)
		, fatPath(dos,chs) {
		badByteMask.flags=0, badByteMask.badEncoding=true; // ignore other "badness"
	}








	void CDos::CFileReaderWriter::GetPhysicalAddress(Yahel::TPosition pos,TPhysicalAddress &outChs,Sector::N &outSectorIndex,Sector::PL pOutOffset) const{
		// returns the PhysicalAddress currently seeked to
		const auto &&d=div( pos, usableSectorLength );
		if (const CDos::CFatPath::PCItem p=fatPath.GetItem(d.quot)){ // Sector exists
			outChs=p->chs;
			outSectorIndex=0; // Files are not known to occupy Sectors with duplicate IDs
			if (pOutOffset)
				*pOutOffset=d.rem-sector.padding.a; // start Padding provided as NEGATIVE!
		}else
			outChs=TPhysicalAddress::Invalid;
	}








	HRESULT CDos::CFileReaderWriter::Clone(IStream **ppstm){
		// creates an exact copy of this object
		if (ppstm){
			*ppstm=new CFileReaderWriter(*this);
			return S_OK;
		}else
			return E_INVALIDARG;
	}








	LPCWSTR CDos::CFileReaderWriter::GetRecordLabelW(Yahel::TPosition logPos,PWCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const{
		// populates the Buffer with label for the Record that STARTS at specified LogicalPosition, and returns the Buffer; returns Null if no Record starts at specified LogicalPosition
		if (!div( logPos, sectorLength ).rem)
			if (const LPCTSTR err=fatPath.GetErrorDesc()){
				#ifdef UNICODE
					return err;
				#else
					::MultiByteToWideChar( CP_ACP, 0, err,-1, labelBuffer,labelBufferCharsMax );
					return labelBuffer;
				#endif
			}
		return __super::GetRecordLabelW( logPos, labelBuffer, labelBufferCharsMax, param );
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
				return MF_GRAYED*!( frw.image->ReadTrack(0,0) );
		}
		return __super::GetCustomCommandMenuFlags(cmd);
	}

	bool CDos::CFileReaderWriter::CHexaEditor::ProcessCustomCommand(UINT cmd){
		// custom command processing
		CFileReaderWriter &frw=*(CFileReaderWriter *)GetCurrentStream().p;
		switch (cmd){
			case ID_TIME:
				frw.Seek( instance->GetCaretPosition(), CFile::begin );
				frw.image->ShowModalTrackTimingAt(
					frw.GetCurrentPhysicalAddress(),
					0, // no Sectors with duplicated ID fields are expected for any File!
					frw.GetPositionInCurrentSector(),
					Revolution::ANY_GOOD
				);
				return true;
		}
		return __super::ProcessCustomCommand(cmd);
	}
