#include "stdafx.h"

	const TFormat TFormat::Unknown={ Medium::UNKNOWN, Codec::ANY, -1,-1,-1, TFormat::LENGTHCODE_128,-1, 1 };

	bool TFormat::operator==(const TFormat &fmt2) const{
		// True <=> Formats{1,2} are equal, otherwise False
		return	supportedMedia&fmt2.supportedMedia
				&&
				supportedCodecs&fmt2.supportedCodecs
				&&
				nCylinders==fmt2.nCylinders
				&&
				nHeads==fmt2.nHeads
				&&
				nSectors==fmt2.nSectors
				&&
				sectorLength==fmt2.sectorLength
				&&
				clusterSize==fmt2.clusterSize;
	}
	DWORD TFormat::GetCountOfAllSectors() const{
		// determines and returns the count of all Sectors
		return (DWORD)nCylinders*nHeads*nSectors;
	}
	WORD TFormat::GetCountOfSectorsPerCylinder() const{
		// determines and returns the count of all Sectors on a single Cylinder
		return (WORD)nHeads*nSectors;
	}
	TTrack TFormat::GetCountOfAllTracks() const{
		// determines and returns the count of all Tracks
		return (TTrack)nCylinders*nHeads;
	}





	const TSectorId TSectorId::Invalid={ -1, -1, -1, -1 };
	
	TSector TSectorId::CountAppearances(const TSectorId *ids,TSector nIds,const TSectorId &id){
		// returns the # of appearances of specified ID
		TSector nAppearances=0;
		while (nIds--)
			nAppearances+=*ids++==id;
		return nAppearances;
	}

	CString TSectorId::List(PCSectorId ids,TSector nIds,TSector iHighlight,char highlightBullet){
		// creates and returns a List of Sector IDs in order as provided
		ASSERT( iHighlight>=nIds || highlightBullet );
		if (!nIds)
			return _T("- [none]\r\n");
		CString list;
		list.Format( _T("- [%d sectors, chronologically]\r\n"), nIds );
		for( TSector i=0; i<nIds; i++ ){
			TCHAR duplicateId[8];
			if (const TSector nDuplicates=CountAppearances( ids, i, ids[i] ))
				::wsprintf( duplicateId, _T(" (%d)"), nDuplicates+1 );
			else
				*duplicateId='\0';
			CString tmp;
			tmp.Format( _T("%c %s%s\r\n"), i!=iHighlight?'-':highlightBullet, ids[i].ToString(), duplicateId );
			list+=tmp;
		}
		return list;
	}

	bool TSectorId::operator==(const TSectorId &id2) const{
		// True <=> Sector IDs are equal, otherwise False
		return	cylinder==id2.cylinder
				&&
				side==id2.side
				&&
				sector==id2.sector
				&&
				lengthCode==id2.lengthCode;
	}
	bool TSectorId::operator!=(const TSectorId &id2) const{
		// True <=> Identifiers are not equal, otherwise False
		return !operator==(id2);
	}

	TSectorId &TSectorId::operator=(const FD_ID_HEADER &rih){
		// assigns Simon Owen's definition of ID to this ID and returns it
		cylinder=rih.cyl, side=rih.head, sector=rih.sector, lengthCode=rih.size;
		return *this;
	}

	TSectorId &TSectorId::operator=(const FD_TIMED_ID_HEADER &rtih){
		// assigns Simon Owen's definition of ID to this ID and returns it
		cylinder=rtih.cyl, side=rtih.head, sector=rtih.sector, lengthCode=rtih.size;
		return *this;
	}

	CString TSectorId::ToString() const{
		// returns a string describing the Sector's ID
		CString result;
		result.Format(_T("ID={%d,%d,%d,%d}"),cylinder,side,sector,lengthCode);
		return result;
	}

	const TPhysicalAddress TPhysicalAddress::Invalid={ -1, -1, TSectorId::Invalid };

	bool TPhysicalAddress::operator==(const TPhysicalAddress &chs2) const{
		// True <=> PhysicalAddresses are equal, otherwise False
		return	cylinder==chs2.cylinder
				&&
				head==chs2.head
				&&
				sectorId==chs2.sectorId;
	}
	bool TPhysicalAddress::operator!=(const TPhysicalAddress &chs2) const{
		// True <=> PhysicalAddresses are NOT equal, otherwise False
		return !operator==(chs2);
	}
	TTrack TPhysicalAddress::GetTrackNumber() const{
		// determines and returns the Track number based on DOS's current Format
		return GetTrackNumber( CImage::GetActive()->GetHeadCount() );
	}
	CString TPhysicalAddress::GetTrackIdDesc(THead nHeads) const{
		// returns a string identifying current Track
		if (!nHeads)
			nHeads=CImage::GetActive()->GetHeadCount();
		CString desc;
		desc.Format( _T("Track %d (Cyl=%d, Head=%d)"), GetTrackNumber(nHeads), cylinder, head );
		return desc;
	}
	TTrack TPhysicalAddress::GetTrackNumber(THead nHeads) const{
		// determines and returns the Track number based on the specified NumberOfHeads
		return GetTrackNumber( cylinder, head, nHeads );
	}





	const TFdcStatus TFdcStatus::Unknown(-1,-1);
	const TFdcStatus TFdcStatus::WithoutError;
	const TFdcStatus TFdcStatus::SectorNotFound( FDC_ST1_NO_ADDRESS_MARK, 0 );
	const TFdcStatus TFdcStatus::IdFieldCrcError( FDC_ST1_DATA_ERROR|FDC_ST1_NO_DATA, 0 );
	const TFdcStatus TFdcStatus::DataFieldCrcError( FDC_ST1_DATA_ERROR, FDC_ST2_CRC_ERROR_IN_DATA );
	const TFdcStatus TFdcStatus::NoDataField( FDC_ST1_NO_ADDRESS_MARK, FDC_ST2_NOT_DAM );
	const TFdcStatus TFdcStatus::DeletedDam( FDC_ST1_NO_DATA, FDC_ST2_DELETED_DAM );

	TFdcStatus::TFdcStatus(BYTE _reg1,BYTE _reg2)
		// ctor
		: reg1(_reg1 & (FDC_ST1_END_OF_CYLINDER|FDC_ST1_DATA_ERROR|FDC_ST1_NO_DATA|FDC_ST1_NO_ADDRESS_MARK))
		, reg2(_reg2 & (FDC_ST2_DELETED_DAM|FDC_ST2_CRC_ERROR_IN_DATA|FDC_ST2_NOT_DAM)) {
	}

	WORD TFdcStatus::GetSeverity(WORD mask) const{
		// returns a number representing the severity of the errors (higher number is more severe)
		enum TSeverity:WORD{
			MAXIMUM	=0xffff,
			HIGH	=0x8000,
			MEDIUM	=0x4000
		};
		if (DescribesMissingId()) // ID not found?
			return TSeverity::MAXIMUM;
		union{
			struct{ BYTE low,high; };
			WORD value;
		} result;
		const WORD wm=w&mask;
		result.low=Utils::CountSetBits(w);
		result.high=Utils::CountSetBits(wm);
		if (DescribesIdFieldCrcError()) // ID with CRC error?
			result.value|=TSeverity::HIGH;
		if (DescribesMissingDam()) // Data not found?
			result.value|=TSeverity::MEDIUM;
		return result.value;
	}

	void TFdcStatus::GetDescriptionsOfSetBits(LPCTSTR *pDescriptions) const{
		// generates Descriptions of currently set bits (caller guarantees that the buffer is large enough)
		if (reg1 & FDC_ST1_END_OF_CYLINDER)		*pDescriptions++=_T("end of cylinder");
		if (reg1 & FDC_ST1_DATA_ERROR)			*pDescriptions++=_T("error in ID or Data field");
		if (reg1 & FDC_ST1_NO_DATA)				*pDescriptions++=_T("ID field with error");
		if (reg1 & FDC_ST1_NO_ADDRESS_MARK)		*pDescriptions++=_T("missing address mark");
		*pDescriptions++=nullptr; // end of Descriptions of set bits in Register1
		if (reg2 & FDC_ST2_DELETED_DAM)			*pDescriptions++=_T("Data field deletion inconsistence");
		if (reg2 & FDC_ST2_CRC_ERROR_IN_DATA)	*pDescriptions++=_T("Data field CRC error");
		if (reg2 & FDC_ST2_NOT_DAM)				*pDescriptions++=_T("no Data field found");
		*pDescriptions++=nullptr; // end of Descriptions of set bits in Register2
	}

	bool TFdcStatus::IsWithoutError() const{
		// True <=> Registers don't describe any error, otherwise False
		return (reg1|reg2)==0;
	}

	bool TFdcStatus::DescribesIdFieldCrcError() const{
		// True <=> Registers describe that ID Field cannot be read without error, otherwise False
		return (w&IdFieldCrcError.w)==IdFieldCrcError.w;
	}

	void TFdcStatus::CancelIdFieldCrcError(){
		// resets ID Field CRC error bits
		reg1&=~FDC_ST1_NO_DATA;
		if (!DescribesDataFieldCrcError()) // no indication that Data Field contains a CRC error
			reg1&=~FDC_ST1_DATA_ERROR;
	}

	bool TFdcStatus::DescribesDataFieldCrcError() const{
		// True <=> Registers describe that ID Field or Data contain CRC error, otherwise False
		return (reg2&FDC_ST2_CRC_ERROR_IN_DATA)!=0;
	}

	void TFdcStatus::CancelDataFieldCrcError(){
		// resets Data Field CRC error bits
		reg2&=~FDC_ST2_CRC_ERROR_IN_DATA;
		if (!DescribesIdFieldCrcError()) // no indication that ID Field contains a CRC error
			reg1&=~FDC_ST1_DATA_ERROR;
	}

	bool TFdcStatus::DescribesDeletedDam() const{
		// True <=> Registers describe that using normal data reading command, deleted data have been read instead, otherwise False
		return (reg2&FDC_ST2_DELETED_DAM)!=0;
	}

	bool TFdcStatus::DescribesMissingId() const{
		// True <=> Registers describe that the Sector ID has not been found, otherwise False
		return (reg1&FDC_ST1_NO_ADDRESS_MARK)!=0 && reg2==0;
	}

	bool TFdcStatus::DescribesMissingDam() const{
		// True <=> Registers describe that the data portion of a Sector has not been found, otherwise False
		return (reg1&FDC_ST1_NO_ADDRESS_MARK)!=0 || (reg2&FDC_ST2_NOT_DAM)!=0;
	}










namespace Medium{
	bool TProperties::IsAcceptableRevolutionTime(TLogTime tRevolutionQueried) const{
		return revolutionTime/10*9<tRevolutionQueried && tRevolutionQueried<revolutionTime/10*11; // 10% tolerance (don't set more for indices on 300 RPM drive appear only 16% slower than on 360 RPM drive!)
	}

	bool TProperties::IsAcceptableCountOfCells(DWORD nCellsQueried) const{
		return nCells/100*85<nCellsQueried && nCellsQueried<nCells/100*115; // 20% (or more) is too much - a 360rpm drive is 20% faster than 300rpm drive which would introduce confusion to the rest of app
	}

	TIwProfile::TIwProfile(TLogTime iwTimeDefault,BYTE iwTimeTolerancePercent)
		// ctor
		: iwTimeDefault(iwTimeDefault)
		, iwTime(iwTimeDefault)
		, iwTimeMin( iwTimeDefault*(100-iwTimeTolerancePercent)/100 )
		, iwTimeMax( iwTimeDefault*(100+iwTimeTolerancePercent)/100 ) {
	}

	void TIwProfile::ClampIwTime(){
		// keep the inspection window size within limits
		if (iwTime<iwTimeMin)
			iwTime=iwTimeMin;
		else if (iwTime>iwTimeMax)
			iwTime=iwTimeMax;
	}



	LPCTSTR GetDescription(TType mediumType){
		// returns the string description of a given MediumType
		if (const PCProperties p=GetProperties(mediumType))
			return p->description;
		else{
			ASSERT(FALSE); // ending up here isn't a problem but always requires attention!
			return _T("Unknown medium");
		}
	}

	const TProperties TProperties::FLOPPY_HD_350={
		_T("3.5\" HD floppy"), // description
		{ 1, FDD_CYLINDERS_MAX }, // supported range of Cylinders (min and max)
		{ 1, 2 },	// supported range of Heads (min and max)
		{ 1, FDD_SECTORS_MAX }, // supported range of Sectors (min and max)
		5, // Revolutions per second
		TIME_SECOND(1)/5, // single revolution time [nanoseconds]
		TIME_MICRO(1), // single recorded data cell time [nanoseconds] = 1 second / 500kb = 2 �s -> 1 �s for MFM encoding
		200000 // RevolutionTime/CellTime
	};

	const TProperties TProperties::FLOPPY_HD_525={
		_T("5.25\" HD floppy, 360 RPM drive"), // description
		{ 1, FDD_CYLINDERS_MAX }, // supported range of Cylinders (min and max)
		{ 1, 2 },	// supported range of Heads (min and max)
		{ 1, FDD_SECTORS_MAX }, // supported range of Sectors (min and max)
		6, // Revolutions per second
		TIME_SECOND(1)/6, // single revolution time [nanoseconds]
		TIME_MICRO(1), // single recorded data cell time [nanoseconds] = same as 3.5" HD floppies
		166666 // RevolutionTime/CellTime
	};

	const TProperties TProperties::FLOPPY_DD={
		_T("3.x\"/5.25\" 2DD floppy, 300 RPM drive"), // description
		{ 1, FDD_CYLINDERS_MAX }, // supported range of Cylinders (min and max)
		{ 1, 2 },	// supported range of Heads (min and max)
		{ 1, FDD_SECTORS_MAX }, // supported range of Sectors (min and max)
		5, // Revolutions per second
		TIME_SECOND(1)/5, // single revolution time [nanoseconds]
		TIME_MICRO(2), // single recorded data cell time [nanoseconds] = 1 second / 250kb = 4 �s -> 2 �s for MFM encoding
		100000 // RevolutionTime/CellTime
	};

	const TProperties TProperties::FLOPPY_DD_525={
		_T("5.25\" 2DD floppy, 360 RPM drive"), // description
		{ 1, FDD_CYLINDERS_MAX }, // supported range of Cylinders (min and max)
		{ 1, 2 },	// supported range of Heads (min and max)
		{ 1, FDD_SECTORS_MAX }, // supported range of Sectors (min and max)
		6, // Revolutions per second
		TIME_MICRO(166600), // single revolution time [nanoseconds], rounded TIME_SECOND(1)/6
		TIME_MICRO(2)*5/6, // single recorded data cell time [nanoseconds] = 1 second / 300kb = 3.333 �s -> 1.666 �s for MFM encoding
		100000 // RevolutionTime/CellTime
	};

	PCProperties GetProperties(TType mediumType){
		// returns properties of a given MediumType
		switch (mediumType){
			case FLOPPY_DD_525:
				return &TProperties::FLOPPY_DD_525;
			case FLOPPY_DD:
				return &TProperties::FLOPPY_DD;
			case FLOPPY_HD_525:
				return &TProperties::FLOPPY_HD_525;
			case FLOPPY_HD_350:
				return &TProperties::FLOPPY_HD_350;
			case HDD_RAW:{
				static constexpr TProperties P={
					_T("Hard disk (without MBR support)"), // description
					{ 1, HDD_CYLINDERS_MAX },// supported range of Cylinders (min and max)
					{ 1, HDD_HEADS_MAX },	// supported range of Heads (min and max)
					{ 1, (TSector)-1 },	// supported range of Sectors (min and max)
					0, // N/A - single revolution time [nanoseconds]
					0, // N/A - single recorded data cell time [nanoseconds]
					0 // N/A - RevolutionTime/CellTime
				};
				return &P;
			}
			default:
				ASSERT(FALSE);
				return nullptr;
		}
	}
}









	namespace Codec{

		PCProperties GetProperties(TType codec){
			// returns Properties of a given Codec
			switch (codec){
				case FM:{
					static const TProperties P={
						_T("FM (Digital Frequency Modulation)"),
						{ 0, 1 }
					};
					return &P;
				}
				case MFM:{
					static const TProperties P={
						_T("MFM (Modified FM)"),
						{ 1, 3 }
					};
					return &P;
				}
				default:
					ASSERT(FALSE);
					return nullptr;
			}
		}

		LPCTSTR GetDescription(TType codec){
			// returns the string description of a given Codec
			if (const PCProperties p=GetProperties(codec))
				return p->description;
			else
				return nullptr;
		}

		TType FirstFromMany(TTypeSet set){
			// returns a Codec with the lowest Id in the input Set (or Unknown if Set empty)
			for( TTypeSet mask=1; mask!=0; mask<<=1 )
				if (set&mask)
					return (TType)mask;
			return UNKNOWN;
		}
	}










	bool CImage::TProperties::IsRealDevice() const{
		return filter==nullptr;
	}












	void CImage::CSettings::Add(LPCTSTR name,bool value){
		SetAt( name, Utils::BoolToYesNo(value) );
	}

	void CImage::CSettings::Add(LPCTSTR name,bool value,bool userForcedValue){
		TCHAR buf[80];
		::wsprintf( buf, _T("%s%s"), Utils::BoolToYesNo(value), userForcedValue?_T(" (user forced)"):_T("") );
		SetAt( name, buf );
	}

	void CImage::CSettings::Add(LPCTSTR name,int value){
		TCHAR buf[16];
		SetAt( name, _itot(value,buf,10) );
	}

	void CImage::CSettings::Add(LPCTSTR name,LPCSTR value){
		#ifdef UNICODE
			static_assert( false, "Unicode support not implemented" );
		#else
			SetAt( name, Utils::SimpleFormat(_T("\"%s\""),value) );
		#endif
	}

	void CImage::CSettings::AddLibrary(LPCTSTR name,int major,int minor){
		TCHAR ver[80];
		::wsprintf( ver, _T("%d%c%d"), major, minor>=0?'.':'\0', minor );
		SetAt( name, ver );
	}

	void CImage::CSettings::AddRevision(int major,int minor){
		AddLibrary( _T("revision"), major, minor );
	}

	void CImage::CSettings::AddMediumIsForced(bool isForced){
		Add( _T("user-forced medium"), isForced );
	}

	void CImage::CSettings::AddMediumIsFlippy(bool isFlippy,bool userForced){
		Add( _T("medium flippy"), isFlippy, userForced );
	}

	void CImage::CSettings::AddDecaHexa(LPCTSTR name,int value){
		TCHAR buf[80];
		::wsprintf( buf, _T("%u (0x%X)"), value, value );
		SetAt( name, buf );
	}

	void CImage::CSettings::AddId(int value){
		AddDecaHexa( _T("ID"), value );
	}

	void CImage::CSettings::AddAuto(LPCTSTR name){
		SetAt( name, _T("Auto") );
	}

	void CImage::CSettings::AddCylinderCount(TCylinder n){
		Add( _T("cylinders"), n );
	}

	void CImage::CSettings::AddHeadCount(THead n){
		Add( _T("heads"), n );
	}

	void CImage::CSettings::AddRevolutionCount(BYTE n){
		Add( _T("revolutions"), n );
	}

	void CImage::CSettings::AddSectorCount(TSector n){
		Add( _T("sectors"), n );
	}

	void CImage::CSettings::AddSides(PCSide list,THead n){
		ASSERT( list && n );
		TCHAR buf[1024], *p=buf;
		while (n-->0)
			p+=::wsprintf( p, _T("%d, "), (int)*list++ );
		p[-2]='\0'; // drop last comma
		SetAt( _T("sides"), buf );
	}

	void CImage::CSettings::AddSectorSize(WORD nBytes){
		Add( _T("sector length"), nBytes );
	}

	void CImage::CSettings::Add40TrackDrive(bool value){
		Add( _T("40-track drive"), value );
	}

	void CImage::CSettings::AddDoubleTrackStep(bool isDouble,bool userForced){
		Add( _T("double track distance"), isDouble, userForced );
	}

	void CImage::CSettings::AddBaudRate(int baudRate){
		Add( _T("baud rate"), baudRate );
	}











	PImage CImage::GetActive(){
		// returns active document
		return (PImage)CMainWindow::CTdiTemplate::pSingleInstance->__getDocument__();
	}

	DWORD CImage::GetCurrentDiskFreeSpace(){
		// determines and returns number of free Bytes on current working disk (nullptr below); this function to be used during Image saving to avoid exceptions (for can't use try-catch blocks in "MFC4.2" release!)
		DWORD nSectorsPerCluster,nBytesPerSector,nFreeClusters,nAllClusters;
		if (!::GetDiskFreeSpace( nullptr, &nSectorsPerCluster, &nBytesPerSector, &nFreeClusters, &nAllClusters ))
			return 0;
		const auto nFreeBytes=(ULONGLONG)nSectorsPerCluster*nBytesPerSector*nFreeClusters;
		return	nFreeBytes<ULONG_MAX ? nFreeBytes : ULONG_MAX;
	}

	TStdWinError CImage::OpenImageForReading(LPCTSTR fileName,CFile &f){
		// True <=> File successfully opened for reading, otherwise False
		CFileException e;
			e.m_cause=ERROR_SUCCESS;
		f.Open( fileName, CFile::modeRead|CFile::typeBinary|CFile::shareDenyWrite, &e );
		return e.m_cause;
	}

	TStdWinError CImage::OpenImageForReadingAndWriting(LPCTSTR fileName,CFile &f){
		// True <=> File successfully opened for both reading and writing, otherwise False
		if (f.m_hFile!=CFile::hFileNull)
			f.Close();
		CFileException e;
		if (f.Open( fileName, CFile::modeReadWrite|CFile::shareExclusive|CFile::typeBinary, &e )!=FALSE)
			e.m_cause=ERROR_SUCCESS; // because the last error might have been 183 (File cannot be created because it already exists)
		::SetLastError(e.m_cause);
		return e.m_cause;
	}

	TStdWinError CImage::CreateImageForReadingAndWriting(LPCTSTR fileName,CFile &f){
		// True <=> File successfully created/truncated for reading+writing, otherwise False
		CFile( fileName, CFile::modeCreate|CFile::shareDenyRead|CFile::typeBinary ).Close(); // creating the underlying file on local disk
		return	OpenImageForReadingAndWriting( fileName, f );
	}

	TSector CImage::CountSectorsBelongingToCylinder(TCylinder cylRef,PCSectorId ids,TSector nIds){
		// returns # of input IDs whose Cylinder value is the same as Referential
		TSector result=0;
		while (nIds-->0)
			result+=ids++->cylinder==cylRef;
		return result;
	}

	#define LENGTH_CODE_BASE	0x80

	TFormat::TLengthCode CImage::GetSectorLengthCode(WORD sectorLength){
		// returns the code of the SectorLength
		BYTE lengthCode=0;
		while (sectorLength>LENGTH_CODE_BASE) sectorLength>>=1, lengthCode++;
		return (TFormat::TLengthCode)lengthCode;
	}

	WORD CImage::GetOfficialSectorLength(BYTE sectorLengthCode){
		// returns the official size in Bytes of a Sector with the given LengthCode
		return LENGTH_CODE_BASE<<sectorLengthCode;
	}

	Utils::CPtrList<CImage::PCProperties> CImage::Known;
	Utils::CPtrList<CImage::PCProperties> CImage::Devices;

	CImage::PCProperties CImage::DetermineType(LPCTSTR fileName){
		// determines and returns Properties for an Image with a given FileName; returns Null if the file represents an unknown Image
		CImage::PCProperties result=nullptr; // assumption (unknown Image type)
		int nLongestExtChars=-1;
		const LPCTSTR fileNameEnd=fileName+::lstrlen(fileName);
		for( POSITION pos=Known.GetHeadPosition(); pos; ){
			const CImage::PCProperties p=CImage::Known.GetNext(pos);
			TCHAR tmp[40], *pFilter=_tcstok( ::lstrcpy(tmp,p->filter), IMAGE_FORMAT_SEPARATOR );
			do{
				const int nExtChars=::lstrlen(pFilter);
				if (!::lstrcmpi(fileNameEnd-::lstrlen(pFilter)+1,pFilter+1)) // "+1" = asterisk as in "*.D40"
					if (nExtChars>nLongestExtChars)
						nLongestExtChars=nExtChars, result=p;
			}while ( pFilter=_tcstok(nullptr,IMAGE_FORMAT_SEPARATOR) );
		}
		return result;
	}

	BYTE CImage::PopulateComboBoxWithCompatibleMedia(HWND hComboBox,WORD dosSupportedMedia,PCProperties imageProperties){
		// populates ComboBox with Media supported both by DOS and Image, and returns their number (or zero if there is no intersection)
		CComboBox cb;
		cb.Attach(hComboBox);
			cb.ResetContent();
			const WORD mediaSupportedByImage= imageProperties ? imageProperties->supportedMedia : 0;
			BYTE result=0;
			for( WORD commonMedia=dosSupportedMedia&mediaSupportedByImage,type=1,n=8*sizeof(commonMedia); n--; type<<=1 )
				if (commonMedia&type){
					cb.SetItemDataPtr( cb.AddString(Medium::GetDescription((Medium::TType)type)), (PVOID)type );
					result++;
				}
			if (!result)
				cb.SetItemData( cb.AddString(_T("No compatible")), Medium::UNKNOWN );
			cb.EnableWindow(result);
			cb.SetCurSel(0);
		cb.Detach();
		return result;
	}

	BYTE CImage::PopulateComboBoxWithCompatibleCodecs(HWND hComboBox,WORD dosSupportedCodecs,PCProperties imageProperties){
		// populates ComboBox with Codecs supported both by DOS and Image, and returns their number (or zero if there is no intersection)
		CComboBox cb;
		cb.Attach(hComboBox);
			cb.ResetContent();
			const Codec::TTypeSet codecsSupportedByImage= imageProperties ? imageProperties->supportedCodecs : 0;
			BYTE result=0;
			for( WORD commonCodecs=dosSupportedCodecs&codecsSupportedByImage,type=1,n=8*sizeof(commonCodecs); n--; type<<=1 )
				if (commonCodecs&type){
					cb.SetItemData( cb.AddString(Codec::GetDescription((Codec::TType)type)), type );
					result++;
				}
			if (!result)
				cb.SetItemData( cb.AddString(_T("No compatible")), Codec::UNKNOWN );
			else if (result>1)
				cb.SetItemData( cb.InsertString(0,_T("Automatically")), dosSupportedCodecs&codecsSupportedByImage );
			cb.EnableWindow(result);
			cb.SetCurSel(0);
		cb.Detach();
		return result;
	}

	void CImage::PopulateComboBoxWithSectorLengths(HWND hComboBox){
		// populates ComboBox with all available SectorLengths
		CComboBox cb;
		cb.Attach(hComboBox);
			cb.ResetContent();
			TCHAR desc[8];
			for( BYTE lengthCode=0; lengthCode<TFormat::TLengthCode::LAST; lengthCode++ )
				cb.SetItemData( cb.AddString(_itot(GetOfficialSectorLength(lengthCode),desc,10)), lengthCode );
			cb.EnableWindow();
			cb.SetCurSel(0);
		cb.Detach();
	}









	CImage::CReadOnlyMessageBar::CReadOnlyMessageBar(LPCTSTR readOnlyReason)
		// ctor
		: CMainWindow::CMessageBar( _T(""), L'\xf0cf' ) {
		SetReadOnlyReason(readOnlyReason);
	}

	void CImage::CReadOnlyMessageBar::SetReadOnlyReason(LPCTSTR readOnlyReason){
		//
		msgHyperlink.Format( _T("%s, editing disabled."), readOnlyReason );
	}



	CImage::CUnsupportedFeaturesMessageBar::CUnsupportedFeaturesMessageBar()
		// ctor
		: CMainWindow::CMessageBar( _T("The image contains <a>features currently not supported</a> by ") APP_ABBREVIATION _T(".") ) {
	}

	CString CImage::CUnsupportedFeaturesMessageBar::CreateListItemIfUnsupported(TCylinder nCyls,TCylinder nCylsMax){
		CString item;
		if (nCyls>=nCylsMax) // # of Cylinders exceeds supported limit
			item.Format( _T("- disk contains %d cylinders, ") _T(APP_ABBREVIATION) _T(" shows just first %d of them\n"), nCyls, nCylsMax );
		return item;
	}

	void CImage::CUnsupportedFeaturesMessageBar::HyperlinkClicked(LPCWSTR id) const{
		Utils::Information(report);
	}

	void CImage::CUnsupportedFeaturesMessageBar::Show(const CString &report){
		//
		if (!( this->report=report ).IsEmpty())
			__super::Show();
	}









	CImage::CImage(PCProperties _properties,bool hasEditableSettings)
		// ctor
		// - initialization
		: properties(_properties) , dos(nullptr)
		, hasEditableSettings(hasEditableSettings) , writeProtected(true) , canBeModified(!_properties->isReadOnly)
		, sideMap(nullptr) // no explicit mapping of Heads to Side numbers
		// - creating the TrackMap
		, trackMap(this)
		// - creating MessageBars
		, readOnlyMessageBar( _T("Disk marked as Read-only") )
		, draftVersionMessageBar( _T("This image is a draft!"), L'\xf060' )
		// - creating Toolbar (its displaying in CTdiView::ShowContent)
		, toolbar(IDR_IMAGE,ID_IMAGE) { // ID_IMAGE = "some" unique ID
		// - when destroying all Views, the document must exist further (e.g. when switching Tabs in TDI)
		m_bAutoDelete=FALSE;
	}

	CImage::~CImage(){
		// dtor
	}







	void CImage::OnCloseDocument(){
		// document is being closed
		// - render data to clipboard
		if (::OleIsCurrentClipboard(dataInClipboard)==S_OK){
			::OleFlushClipboard();
			dataInClipboard.Release();
		}
		// - close DOS and all views
{		PREVENT_FROM_DESTRUCTION(*this);
		TDI_INSTANCE->CloseAllTabsOfImage(this);
		if (dos)
			delete dos, dos=nullptr;
		// - dispose all Cylinders
		class CBackgroundActionModal:public CBackgroundActionCancelable{
			BOOL OnInitDialog() override{
				const BOOL result=__super::OnInitDialog();
				EnableDlgItem( IDCANCEL, false );
				return result;
			}
			static UINT AFX_CDECL CylinderDisposalThread(PVOID pCancelableAction){
				// thread to dispose all Cylinders, regardless if they are modified or not
				const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
				const PImage image=(PImage)pAction->GetParams();
				const TCylinder nCyls=image->GetCylinderCount();
				pAction->SetProgressTarget(nCyls);
				EXCLUSIVELY_LOCK_IMAGE(*image);
				for( TCylinder cyl=nCyls; cyl-->0; pAction->UpdateProgress(nCyls-cyl) )
					for( THead head=image->GetHeadCount(); head-->0; )
						if (image->UnscanTrack(cyl,head)==ERROR_NOT_SUPPORTED)
							image->UnformatTrack(cyl,head);
				return pAction->TerminateWithSuccess();
			}
		public:
			CBackgroundActionModal(PImage image)
				: CBackgroundActionCancelable( CylinderDisposalThread, image, THREAD_PRIORITY_NORMAL ) {
			}
		} bam(this);
		bam.Perform(true);
		// - destroy us
}		delete this;
		// - base
		//nop (CDocument::OnCloseDocument destroys parent FrameWnd (MainWindow) - this must exist even after the document was closed)
	}

	TStdWinError CImage::CreateUserInterface(HWND hTdi){
		// creates disk-specific Tabs in TDI; returns Windows standard i/o error
		// - adding the Document (Image) to TdiTemplate
		CMainWindow::CTdiTemplate::pSingleInstance->AddDocument(this);
		// - adding the TrackMap to TDI
		CTdiCtrl::AddTabLast( hTdi, TRACK_MAP_TAB_LABEL, &trackMap.tab, false, TDI_TAB_CANCLOSE_NEVER, nullptr );
		return ERROR_SUCCESS; // always succeeds (but may fail in CDos-derivates)
	}

	CString CImage::ListUnsupportedFeatures() const{
		// returns a list of all features currently not properly implemented
		return CString();
	}

	void CImage::ToggleWriteProtection(){
		// toggles Image's WriteProtection flag
		// - cannot toggle if not allowed to write to the Image (e.g. because the Image has been opened from a CD-ROM)
		if (!canBeModified)
			return;
		// - if this Image is in existing file, verifying that the file can be modified
		const CString &path=GetPathName();
		if (!path.IsEmpty() && !properties->IsRealDevice()){
			const DWORD attr=::GetFileAttributes(path);
			if (attr!=(DWORD)INVALID_HANDLE_VALUE && attr&FILE_ATTRIBUTE_READONLY)
				return Utils::FatalError(_T("Cannot toggle the write protection"),_T("The file is read-only."));
		}
		// - toggling WriteProtection
		writeProtected=!writeProtected;
	}

	BYTE CImage::ShowModalTrackTimingAt(RCPhysicalAddress chs,BYTE nSectorsToSkip,WORD positionInSector,Revolution::TType rev){
		// displays modal dialog showing low-level timing for specified position on the Track
		const CString msg=Utils::SimpleFormat( _T("Can't determine timing for sector with %s"), chs.sectorId.ToString() );
		if (CImage::CTrackReader tr=ReadTrack( chs.cylinder, chs.head )){
			TLogTime tDataStart;
			if (GetSectorData( chs, nSectorsToSkip, rev, nullptr, nullptr, &tDataStart )!=nullptr){
				const auto peList=tr.ScanAndAnalyze( CActionProgress::None, false );
				const auto &peData=*static_cast<const CTrackReader::TDataParseEvent *>(peList.FindByStart(
					tDataStart, CTrackReader::TParseEvent::DATA_OK, CTrackReader::TParseEvent::DATA_BAD
				)->second);
				return tr.ShowModal(
					nullptr, 0, MB_OK, true, peData.GetByteTime(positionInSector),
					_T("%s, sector %s data timing"), (LPCTSTR)chs.GetTrackIdDesc(), (LPCTSTR)chs.sectorId.ToString()
				);
			}else
				Utils::Information( msg, _T("Data field not found.") );
		}else
			Utils::Information( msg, ERROR_NOT_SUPPORTED );
		return IDCLOSE;
	}

	void CImage::SetRedrawToAllViews(bool redraw) const{
		// notifies each of registered whether they should suspend or resume their drawing
		for( POSITION pos=GetFirstViewPosition(); pos; GetNextView(pos)->SetRedraw(redraw) );
	}

	bool CImage::ReportWriteProtection() const{
		// True <=> Image is WriteProtected and a warning window has been shown, otherwise False
		if (writeProtected){
			Utils::Information(_T("This operation requires the image to be accessible for writing.\n\nRemove the write protection and try again."));
			return true;
		}else
			return false;
	}

	#define INI_MSG_SAVE_AS		_T("msgsaveas")

	BOOL CImage::OnCmdMsg(UINT nID,int nCode,LPVOID pExtra,AFX_CMDHANDLERINFO *pHandlerInfo){
		// command processing
		switch (nCode){
			case CN_UPDATE_COMMAND_UI:
				// update
				if (dos)
					if (dos->UpdateCommandUi(nID,(CCmdUI *)pExtra))
						return TRUE;
				switch (nID){
					case ID_FILE_SAVE:
						((CCmdUI *)pExtra)->Enable(m_bModified);
						return TRUE;
					case ID_FILE_SAVE_AS:
						((CCmdUI *)pExtra)->Enable( !properties->IsRealDevice() ); // disabled for real Devices
						return TRUE;
					case ID_IMAGE_PROTECT:
						((CCmdUI *)pExtra)->SetCheck(writeProtected);
						((CCmdUI *)pExtra)->Enable(canBeModified && !PropGrid::IsValueBeingEdited());
						return TRUE;
					case ID_IMAGE_SETTINGS:
						((CCmdUI *)pExtra)->Enable(hasEditableSettings);
						return TRUE;
					case ID_IMAGE_DUMP:
						((CCmdUI *)pExtra)->Enable(TRUE);
						return TRUE;
				}
				break;
			case CN_COMMAND:
				// command
				if (dos)
					switch (dos->ProcessCommand(nID)){
						case CDos::TCmdResult::DONE_REDRAW:
							UpdateAllViews(nullptr,0,nullptr);
							// fallthrouh
						case CDos::TCmdResult::DONE:
							return TRUE;
					}
				switch (nID){
					case ID_FILE_SAVE_AS:
						Utils::InformationWithCheckableShowNoMore( _T("Conversion between image types (e.g. DSK to IMA) must be approached by dumping, not by \"Saving as\"."), INI_GENERAL, INI_MSG_SAVE_AS );
						OnFileSaveAs();
						return TRUE;
					case ID_IMAGE_PROTECT:
						// . toggling WriteProtection
						ToggleWriteProtection();
						// . refreshing known windows that depend on Image's WriteProtection flag
						if (CDos::CHexaPreview::pSingleInstance) 
							CDos::CHexaPreview::pSingleInstance->hexaEditor.SetEditable(!writeProtected);
						return TRUE;
					case ID_IMAGE_SETTINGS:
						EditSettings(false);
						return TRUE;
					case ID_IMAGE_DUMP:
						Dump();
						return TRUE;
					case ID_IMAGE_BROWSE:
						CDiskBrowserView::CreateAndSwitchToTab( CImage::GetActive(), TPhysicalAddress::Invalid, 0 );
						return TRUE;
					case ID_FILE_CLOSE:
						return FALSE; // for CDocument to be excluded from processing, and processing was forwarded right to MainWindow
				}
				break;
		}
		return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	}

	BOOL CImage::DoSave(LPCTSTR lpszPathName,BOOL bReplace){
		// True <=> Image successfully saved, otherwise False
		app.m_pMainWnd->SetFocus(); // to immediately carry out actions that depend on focus
		TCHAR bufFileName[MAX_PATH];
		if (properties->IsRealDevice())
			::lstrcpy(bufFileName,m_strTitle);
		else if (!lpszPathName){
			// FileName not determined yet or the file is read-only - determining FileName now
			if (/*bReplace &&*/ m_strPathName.IsEmpty()){ // A&B; A = the "Save as" command, B = fully qualified FileName not determined yet; commented out as "Save file copy" command not used (when it holds bReplace==False)
				// . validating that there are no Forbidden characters
				if (const PTCHAR forbidden=::StrPBrk( ::lstrcpy(bufFileName,m_strTitle), _T("#%;/\\<>:") ))
					*forbidden='\0';
				// . adding Extension
				::StrCatN( bufFileName, 1+properties->filter, 4+1 ); // 1 = asterisk, 4+1 = dot and three-character extension (e.g. "*.d40")
			}else
				::lstrcpy(bufFileName,m_strPathName);
			if (!CRideApp::DoPromptFileName( bufFileName, false, AFX_IDS_SAVEFILE, OFN_HIDEREADONLY|OFN_PATHMUSTEXIST, properties ))
				return FALSE;
		}else
			::lstrcpy(bufFileName,lpszPathName);
		return __super::DoSave( bufFileName, bReplace );
	}












	UINT AFX_CDECL CImage::SaveAllModifiedTracks_thread(PVOID _pCancelableAction){
		// thread to save all Modified Tracks
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		const TSaveThreadParams &stp=*(TSaveThreadParams *)pAction->GetParams();
		pAction->SetProgressTarget( stp.image->GetCylinderCount()+1 ); // "+1" = action dialog closed once the SaveAllModifiedTracks method has finished
		if (const TStdWinError err=stp.image->SaveAllModifiedTracks( stp.lpszPathName, *pAction ))
			return pAction->TerminateWithError(err);
		else{
			stp.image->m_bModified=FALSE;
			return pAction->TerminateWithSuccess();
		}
	}

	BOOL CImage::OnSaveDocument(LPCTSTR lpszPathName){
		// True <=> this Image has been successfully saved, otherwise False
		if (const TStdWinError err=CBackgroundActionCancelable(
				SaveAllModifiedTracks_thread,
				&TSaveThreadParams( this, lpszPathName ),
				THREAD_PRIORITY_ABOVE_NORMAL
			).Perform(true)
		){
			const CString msg=Utils::SimpleFormat( _T("Can't write to \"%s\""), lpszPathName );
			Utils::FatalError( msg, err, _T("Some tracks remain unsaved.") );
			::SetLastError(err);
			return FALSE;
		}else
			return TRUE;
	}

	bool CImage::IsWriteProtected() const{
		// True <=> Image is WriteProtected, otherwise False
		return writeProtected;
	}

	bool CImage::CanBeModified() const{
		// True <=> content of this Image CanBeModified (can't be if, for instance, opened from CD-ROM), otherwise False and the Disk remains WriteProtected
		return canBeModified;
	}

	THead CImage::GetNumberOfFormattedSides(TCylinder cyl) const{
		// determines and returns the number of Sides formatted on given Cylinder; returns 0 iff Cylinder not formatted
		for( THead head=GetHeadCount(); head-->0; )
			if (ScanTrack(cyl,head)>0)
				return head+1;
		return 0;
	}

	TTrack CImage::GetTrackCount() const{
		// returns the number of all Tracks in the Image
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		LOG_ACTION(_T("TTrack CImage::GetTrackCount"));
		return GetCylinderCount()*GetHeadCount();
	}

	BYTE CImage::GetAvailableRevolutionCount(TCylinder cyl,THead head) const{
		// returns the number of data variations of one Sector that are guaranteed to be distinct
		return 1;
	}

	TStdWinError CImage::SeekHeadsHome() const{
		// attempts to send Heads "home"; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}

	TStdWinError CImage::UnscanTrack(TCylinder cyl,THead head){
		// disposes internal representation of specified Track if possible; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}

	CString CImage::ListSectors(TCylinder cyl,THead head,TSector iHighlight,char highlightBullet) const{
		// creates and returns a List of current Sector IDs as they chronologically appear on the specified Track
		TSectorId ids[(TSector)-1];
		return TSectorId::List( ids, ScanTrack(cyl,head,nullptr,ids), iHighlight, highlightBullet );
	}

	bool CImage::IsTrackDirty(TCylinder cyl,THead head) const{
		// True <=> any of Track's Sectors is dirty, otherwise False
		TSectorId bufferId[(TSector)-1];
		for( TSector n=ScanTrack(cyl,head,nullptr,bufferId); n>0; ){
			const TPhysicalAddress chs={ cyl, head, bufferId[--n] };
			if (GetDirtyRevolution( chs, n )!=Revolution::NONE)
				return true;
		}
		return false;
	}

	TSector CImage::GetCountOfHealthySectors(TCylinder cyl,THead head) const{
		// returns the number of Sectors whose data are healthy
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - if Track is empty, assuming the Track surface is damaged, so the Track is NOT healthy
		TSectorId bufferId[(BYTE)-1]; WORD bufferLength[(BYTE)-1];
		const TSector nSectors=ScanTrack(cyl,head,nullptr,bufferId,bufferLength);
		if (!nSectors)
			return 0;
		// - counting the number of healthy Sectors
		TSector nHealthySectors=0;
		for( TSector s=0; s<nSectors; s++ ){
			const TPhysicalAddress chs={ cyl, head, bufferId[s] };
			TFdcStatus st; // in/out
			nHealthySectors+=	const_cast<PImage>(this)->GetSectorData(chs,s,Revolution::CURRENT,nullptr,&st)!=nullptr
								&&
								st.IsWithoutError();
		}
		return nHealthySectors;
	}

	bool CImage::IsTrackHealthy(TCylinder cyl,THead head) const{
		// True <=> specified Track is not empty and contains only well readable Sectors, otherwise False
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		LOG_ACTION(_T("bool CImage::IsTrackHealthy"));
		// - if Track is empty, assuming the Track surface is damaged, so the Track is NOT healthy
		const TSector nSectors=ScanTrack(cyl,head);
		if (!nSectors)
			return LOG_BOOL(false);
		// - if any of the Sectors cannot be read without error, the Track is NOT healthy
		return	LOG_BOOL( GetCountOfHealthySectors(cyl,head)==nSectors );
	}

	TLogTime CImage::EstimateNanosecondsPerOneByte() const{
		// estimates and returns the number of Nanoseconds that represent a single Byte on the Medium
		return 1;
	}

	void CImage::BufferTrackData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId bufferId,PCBYTE bufferNumbersOfSectorsToSkip,TSector nSectors){
		// buffers Sectors in the same Track by the underlying Image, making them ready for IMMEDIATE usage - later than immediate calls to GetSectorData may be slower
		LOG_TRACK_ACTION(cyl,head,_T("void CImage::BufferTrackData"));
		PVOID dummyBuffer[(TSector)-1];
		TFdcStatus statuses[(TSector)-1];
		GetTrackData( cyl, head, rev, bufferId, bufferNumbersOfSectorsToSkip, nSectors, (PSectorData *)dummyBuffer, (PWORD)dummyBuffer, statuses, (PLogTime)dummyBuffer ); // "DummyBuffer" = throw away any outputs
	}

	PSectorData CImage::GetSectorData(TCylinder cyl,THead head,Revolution::TType rev,PCSectorId pid,BYTE nSectorsToSkip,PWORD pSectorLength,TFdcStatus *pFdcStatus,TLogTime *outDataStarts){
		// returns Data of a Sector on a given PhysicalAddress; returns Null if Sector not found or Track not formatted
		PSectorData data; WORD w; TLogTime t;
		GetTrackData(
			cyl, head, rev, pid, &nSectorsToSkip, 1, &data,
			pSectorLength?pSectorLength:&w,
			pFdcStatus?pFdcStatus:&TFdcStatus(),
			outDataStarts?outDataStarts:&t
		);
		return data;
	}

	PSectorData CImage::GetSectorData(RCPhysicalAddress chs,BYTE nSectorsToSkip,Revolution::TType rev,PWORD pSectorLength,TFdcStatus *pFdcStatus,TLogTime *outDataStarts){
		// returns Data of a Sector on a given PhysicalAddress; returns Null if Sector not found or Track not formatted
		PSectorData data; WORD w; TLogTime t;
		GetTrackData(
			chs.cylinder, chs.head, rev, &chs.sectorId, &nSectorsToSkip, 1, &data,
			pSectorLength?pSectorLength:&w,
			pFdcStatus?pFdcStatus:&TFdcStatus(),
			outDataStarts?outDataStarts:&t
		);
		return data;
	}

	PSectorData CImage::GetHealthySectorData(TCylinder cyl,THead head,PCSectorId pid,PWORD sectorLength,BYTE nSectorsToSkip){
		// returns Data of a Sector on a given PhysicalAddress; returns Null if Sector not found or Track not formatted
		TFdcStatus st;
		::SetLastError(ERROR_SUCCESS); // assumption
		if (const PSectorData data=GetSectorData(cyl,head,Revolution::ANY_GOOD,pid,nSectorsToSkip,sectorLength,&st))
			if (st.IsWithoutError()) // Data must be either without error ...
				return data;
		if (!::GetLastError())
			::SetLastError(ERROR_SECTOR_NOT_FOUND);
		return nullptr; // ... or none
	}

	PSectorData CImage::GetHealthySectorData(RCPhysicalAddress chs,PWORD sectorLength,BYTE nSectorsToSkip){
		// returns Data of a Sector on a given PhysicalAddress; returns Null if Sector not found or Track not formatted
		return GetHealthySectorData(chs.cylinder,chs.head,&chs.sectorId,sectorLength,nSectorsToSkip);
	}

	PSectorData CImage::GetHealthySectorData(RCPhysicalAddress chs){
		// returns Data of a Sector on a given PhysicalAddress; returns Null if Sector not found or Track not formatted
		return GetHealthySectorData(chs,nullptr);
	}

	PSectorData CImage::GetHealthySectorDataOfUnknownLength(TPhysicalAddress &rChs,PWORD sectorLength){
		// returns Data of a Sector of unknown length (i.e. LengthCode is not used to find Sector with a given ID)
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - scanning given Track to find out Sectors a their Lengths
		TSectorId bufferId[(TSector)-1];	WORD bufferLength[(TSector)-1];
		TSector nSectorsOnTrack=ScanTrack(rChs.cylinder,rChs.head,nullptr,bufferId,bufferLength);
		// - searching for first matching ID among found Sectors (LengthCode ignored)
		for( PCSectorId pId=bufferId; nSectorsOnTrack; nSectorsOnTrack-- ){
			rChs.sectorId.lengthCode=pId->lengthCode; // accepting whatever LengthCode
			if (rChs.sectorId==*pId++)
				break;
		}
		if (!nSectorsOnTrack) // Sector with a given ID not found (LengthCode ignored)
			return nullptr;
		// - retrieving Data
		return GetHealthySectorData(rChs,sectorLength);
	}

	void CImage::MarkSectorAsDirty(RCPhysicalAddress chs){
		// marks Sector on a given PhysicalAddress as "dirty", plus sets it the given FdcStatus
		MarkSectorAsDirty(chs,0,&TFdcStatus::WithoutError);
	}

	Revolution::TType CImage::GetDirtyRevolution(RCPhysicalAddress chs,BYTE nSectorsToSkip) const{
		// returns the Revolution that has been marked as "dirty"
		return Revolution::NONE; // not applicable by default, hence no dirty Revolution by default
	}

	TStdWinError CImage::GetInsertedMediumType(TCylinder,Medium::TType &rOutMediumType) const{
		// recognizes a Medium inserted in the Drive; returns Windows standard i/o error
		if (dos!=nullptr){
			rOutMediumType=dos->formatBoot.mediumType; // returning officially recognized Medium
			return ERROR_SUCCESS;
		}else
			return ERROR_NO_MEDIA_IN_DRIVE;
	}

	TStdWinError CImage::SetMediumTypeAndGeometry(PCFormat,PCSide,TSector){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		return ERROR_SUCCESS;
	}

	void CImage::EnumSettings(CSettings &rOut) const{
		// returns a collection of relevant settings for this Image
		rOut.Add( _T("has unsupported features"), !ListUnsupportedFeatures().IsEmpty() );
	}

	bool CImage::RequiresFormattedTracksVerification() const{
		// True <=> the Image requires its newly formatted Tracks be verified, otherwise False (and caller doesn't have to carry out verification)
		return false; // verification NOT required by default (but Images abstracting physical drives can override this setting)
	}

	TStdWinError CImage::PresumeHealthyTrackStructure(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,BYTE gap3,BYTE fillerByte){
		// without formatting it, presumes that given Track contains Sectors with specified parameters; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED; // each Track by default must be explicitly formatted to be sure about its structure (but Images abstracting physical drives can override this setting)
	}

	TStdWinError CImage::MineTrack(TCylinder cyl,THead head,bool autoStartLastConfig){
		// begins mining of specified Track; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED; // this container doesn't support Track mining
	}

	TStdWinError CImage::SaveAllModifiedTracks(LPCTSTR lpszPathName,CActionProgress &ap){
		// saves all Modified Tracks by calling the SaveTrack method for each of them; returns Windows standard i/o error
		// - attempting to save the disk the "per-Track way" (must override this method in descendant if this way is not suitable for given disk, e.g. DSK images!)
		if (m_bModified){
			// the "Save" command is enabled only if disk Modified
			if (!m_strPathName.IsEmpty() && m_strPathName!=lpszPathName) // saving to a different location?
				return ERROR_NOT_SUPPORTED; // override in descendant the case for the "Save as" command!
			for( TCylinder cyl=0; cyl<GetCylinderCount(); ap.UpdateProgress(++cyl) )
				for( THead head=0; head<GetHeadCount(); head++ )
					if (ap.Cancelled)
						return ERROR_CANCELLED;
					else if (const TStdWinError err=SaveTrack( cyl, head, ap.Cancelled ))
						return err;
		}else{
			// we get here only with the "Save as" command
			if (m_strPathName.IsEmpty() || ::GetFileAttributes(m_strPathName)==INVALID_FILE_ATTRIBUTES) // current file doesn't exist
				return ERROR_FILE_NOT_FOUND;
			if (!::CopyFile( m_strPathName, lpszPathName, FALSE ))
				return ::GetLastError();
		}
		return ERROR_SUCCESS;
	}

	TStdWinError CImage::SaveTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const{
		// saves the specified Track to the inserted Medium; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED; // individual Track saving is not supported for this kind of Image (OnSaveDocument must be called instead)
	}

	CImage::CTrackReader CImage::ReadTrack(TCylinder cyl,THead head) const{
		// creates and returns a general description of the specified Track, represented using neutral LogicalTimes
		return CTrackReaderWriter::Invalid; // not supported (TrackReader invalid right from its creation)
	}

	TStdWinError CImage::WriteTrack(TCylinder cyl,THead head,CTrackReader tr){
		// converts general description of the specified Track into Image-specific representation; caller may provide Invalid TrackReader to check support of this feature; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}

	void CImage::SetPathName(LPCTSTR lpszPathName,BOOL bAddToMRU){
		//
		__super::SetPathName( lpszPathName, FALSE );
		if (bAddToMRU)
			if (CRideApp::CRecentFileListEx *const pMru=app.GetRecentFileList()){
				extern CDos::PCProperties manuallyForceDos;
				pMru->Add(
					lpszPathName,
					manuallyForceDos,
					properties->IsRealDevice() ? properties : nullptr
				);
			}
	}

	BOOL CImage::CanCloseFrame(CFrameWnd* pFrame){
		// True <=> the MainWindow can be closed (and thus the application), otherwise False
		PREVENT_FROM_DESTRUCTION(*this);
		//EXCLUSIVELY_LOCK_THIS_IMAGE(); // commented out as it's being not worked with the Image
		// - first asking the DOS that handles this Image
		if (!dos->CanBeShutDown(pFrame))
			return FALSE;
		// - then attempting to close this Image
		return	pFrame!=nullptr // Null if e.g. app closing
				? __super::CanCloseFrame(pFrame)
				: SaveModified();
	}
