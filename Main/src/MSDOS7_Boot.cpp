#include "stdafx.h"
#include "MSDOS7.h"

	void CMSDOS7::TVolumeInfo::__init__(const CFat &rFat){
		// initializes this Volume information
		infoValid=0x29; // this information now is valid
		Utils::RandomizeData( &id, sizeof(id) );
		::memcpy(	::memset(label,' ',MSDOS7_LABEL_LENGTH_MAX),
					VOLUME_LABEL_DEFAULT_ANSI_8CHARS,
					sizeof(VOLUME_LABEL_DEFAULT_ANSI_8CHARS)-1
				);
		::wsprintfA( fatId, "FAT%d   ", rFat.type*4 );
	}





	TPhysicalAddress CMSDOS7::TBootSector::__getRecognizedChs__(PImage image,bool recognizeBoot,bool *pSuccess){
		// True <=> Boot Sector has been recognized on the disk, otherwise False
		TPhysicalAddress chs={ 0, 0, {0,0,1,-1} };
		// - in case the Image is a physical floppy disk, determining the Type of Medium (type of floppy)
		TFormat fmt={ TMedium::FLOPPY_DD_525, 1,1,MSDOS7_SECTOR_BKBOOT, MSDOS7_SECTOR_LENGTH_STD_CODE,MSDOS7_SECTOR_LENGTH_STD, 1 };
		if (image->SetMediumTypeAndGeometry(&fmt,StdSidesMap,1)!=ERROR_SUCCESS || !image->GetNumberOfFormattedSides(0)){
			fmt.mediumType=TMedium::FLOPPY_DD_350;
			if (image->SetMediumTypeAndGeometry(&fmt,StdSidesMap,1)!=ERROR_SUCCESS || !image->GetNumberOfFormattedSides(0)){
				fmt.mediumType=TMedium::FLOPPY_HD_350;
				if (image->SetMediumTypeAndGeometry(&fmt,StdSidesMap,1)!=ERROR_SUCCESS || !image->GetNumberOfFormattedSides(0))
					return chs; // unknown Medium Type, any address will sooner or later cause a failure in access
			}
		}
		// - recognizing the Boot Sector
		if (pSuccess) *pSuccess=true; // assumption (Boot Sector found and successfully recognized)
		WORD w;
		PCBootSector boot=(PCBootSector)image->GetHealthySectorDataOfUnknownLength(chs,&w);
		if (!boot || recognizeBoot&&!boot->__recognize__(w)){
			chs.sectorId.sector=MSDOS7_SECTOR_BKBOOT;
			boot=(PCBootSector)image->GetHealthySectorDataOfUnknownLength(chs,&w);
			if (!boot || recognizeBoot&&!boot->__recognize__(w))
				if (pSuccess) *pSuccess=false; // neither normal nor backup Boot Sector could be recognized
		}
		return chs;
	}

	bool CMSDOS7::TBootSector::__recognize__(WORD sectorLength) const{
		// True <=> Boot Sector recognized, otherwise False
		return	sectorLength>=MSDOS7_SECTOR_LENGTH_STD
				&&
				( jmpInstruction.opCode==0xeb&&jmpInstruction.param>=0x9000 || jmpInstruction.opCode==0xe9 )
				&&
				__getCountOfAllSectors__()!=0 // Medium is known
				&&
				nSectorsInCluster
				&&
				nReservedSectors
				&&
				nFatCopies && nFatCopies<=15
				&&
				//AA55mark==0xaa55 // commented out as some MS-DOS floppies neglect this mark, causing this app to not recognize them (e.g. the Czech game "7 dni a 7 noci" or Slovak edition of "Ramonovo kouzlo")
				//&&
				__isUsable__(); // Boot Sector contains values that make sense
	}

	bool CMSDOS7::TBootSector::__isUsable__() const{
		// True <=> Boot Sector contains values that make sense, otherwise False
		return nHeads && nSectorsOnTrack && sectorSize && nSectorsInCluster; // geometry
	}

	CMSDOS7::TLogSector32 CMSDOS7::TBootSector::__getCountOfAllSectors__() const{
		// determines and returns the total number of (officially reported) Sectors on the Medium
		switch (medium){
			case TBootSector::DISK_35_1440_DS_18:
			case TBootSector::DISK_35_720_DS_9:
			case TBootSector::DISK_525_180_SS_9:
			case TBootSector::DISK_525_360_DS_9:
			case TBootSector::DISK_525_160_SS_8:
			case TBootSector::DISK_525_320_DS_8:
				// for floppies, only the 16-bit value is considered valid (ignoring whatever value is in the 32-bit counterpart)
				return nSectorsInTotal16;
			case TBootSector::DISK_HARD:
				// for hard disks, both 16-bit and 32-bit values are considered
				return nSectorsInTotal16|nSectorsInTotal32;
			default:
				// for unknown Media, neither of the values is considered
				return 0;
		}
	}

	void CMSDOS7::TBootSector::__getGeometry__(PFormat pFormat) const{
		// extracts information on geometry from this Boot Sector
		if (const WORD nSectorsOnCylinder=nHeads*nSectorsOnTrack)
			pFormat->nCylinders=__getCountOfAllSectors__()/nSectorsOnCylinder;
		else
			pFormat->nCylinders=0;
		pFormat->nHeads=nHeads;
		pFormat->nSectors=nSectorsOnTrack;
		pFormat->sectorLength=sectorSize;
		pFormat->clusterSize=nSectorsInCluster;
	}

	void CMSDOS7::TBootSector::__init__(PCFormat pFormatBoot,CFormatDialog::PCParameters params,CFat &rOutFat){
		// initializes this Boot Sector
		const DWORD nSectorsInTotal=pFormatBoot->GetCountOfAllSectors();
		::ZeroMemory( this, MSDOS7_SECTOR_LENGTH_STD );
		// - initializing information that all Types of FAT have in common
		jmpInstruction.opCode=0xeb;
		jmpInstruction.param=0x903c;
		::lstrcpyA(oemName,"MSWIN4.1");
		sectorSize=pFormatBoot->sectorLength;
		nSectorsInCluster=pFormatBoot->clusterSize;
		nFatCopies=params->nAllocationTables;
		switch (pFormatBoot->mediumType){
			case TMedium::FLOPPY_HD_350:
				medium=DISK_35_1440_DS_18;	break;
			case TMedium::FLOPPY_DD_350:
				medium=DISK_35_720_DS_9;	break;
			case TMedium::FLOPPY_DD_525:
				medium=DISK_525_360_DS_9;	break;
			case TMedium::HDD_RAW:
				medium=DISK_HARD;			break;
			default:
				ASSERT(FALSE);
		}
		nSectorsOnTrack=pFormatBoot->nSectors;
		nHeads=pFormatBoot->nHeads;
		//nSectorsHidden=0; // see ZeroMemory above
		AA55mark=0xaa55;
		// - determining the Type of FAT
		DWORD nSectorsFat;
		static const BYTE ReservedSectorCounts[]={1,1,32}; // for FAT12/16/32, respectively
		for( BYTE f=0; f<3; f++ ){
			// . computing the NumberOfSectors that will be occupied by all FatCopies
			rOutFat.type=CFat::Types[f];
			nRootDirectoryEntries= rOutFat.type!=CFat::FAT32 ? params->nRootDirectoryEntries : 0;
			const DWORD fatTmpVal1=nSectorsInTotal-( nReservedSectors=ReservedSectorCounts[f] )-__getCountOfPermanentRootDirectorySectors__();
			const DWORD fatTmpVal2= sectorSize*nSectorsInCluster*2/rOutFat.type + nFatCopies ;
			nSectorsFat=(fatTmpVal1+fatTmpVal2-1)/fatTmpVal2;
			// . making sure that FAT usable
			if (CFat::GetFatType( (fatTmpVal1-nFatCopies*nSectorsFat)/nSectorsInCluster )==rOutFat.type)
				break;
		}
		// - initializing information specific for above selected Type of FAT
		switch (rOutFat.type){
			case CFat::FAT12:
			case CFat::FAT16:
				if (nSectorsInTotal>=0x10000)
					nSectorsInTotal32=nSectorsInTotal;
				else
					nSectorsInTotal16=nSectorsInTotal32=nSectorsInTotal;
				nSectorsFat16=nSectorsFat;
				fat1216.mediumType= pFormatBoot->mediumType&TMedium::FLOPPY_ANY
									? TMsdosMediumType::FLOPPY
									: TMsdosMediumType::HDD;
				fat1216.volume.__init__(rOutFat);
				break;
			case CFat::FAT32:
				nSectorsInTotal32=nSectorsInTotal;
				fat32.nSectorsFat32=nSectorsFat;
				//fat32.rootDirectoryFirstCluster=... // initialized by caller when creating root Directory
				fat32.fsInfo=MSDOS7_SECTOR_FSINFO;
				fat32.bootCopy=MSDOS7_SECTOR_BKBOOT;
				fat32.mediumType=	pFormatBoot->mediumType&TMedium::FLOPPY_ANY
									? TMsdosMediumType::FLOPPY
									: TMsdosMediumType::HDD;
				fat32.volume.__init__(rOutFat);
				break;
			default:
				ASSERT(FALSE);
		}
	}

	DWORD CMSDOS7::TBootSector::__getCountOfSectorsInOneFatCopy__() const{
		// returns the reported number of Sectors occupied by ONE COPY of FAT
		return nSectorsFat16 ? nSectorsFat16 : fat32.nSectorsFat32;
	}

	CMSDOS7::TLogSector32 CMSDOS7::TBootSector::__getRootDirectoryFirstSector__() const{
		// computes and returns the first Sector permanently occupied by root Directory
		return	nReservedSectors	// Boot
				+
				nFatCopies*__getCountOfSectorsInOneFatCopy__(); // FAT
	}

	CMSDOS7::TLogSector16 CMSDOS7::TBootSector::__getCountOfPermanentRootDirectorySectors__() const{
		// computes and returns the number of Sectors that are permanently occupied by root Directory
		return	sectorSize
				? (nRootDirectoryEntries*sizeof(UDirectoryEntry)+sectorSize-1)/sectorSize
				: 0;
	}

	DWORD CMSDOS7::TBootSector::__getCountOfNondataSectors__() const{
		// returns the number of ALL NON-DATA Sectors in this Volume
		return	nReservedSectors	// Boot Sector, etc.
				+
				nFatCopies*__getCountOfSectorsInOneFatCopy__() // FAT
				+
				__getCountOfPermanentRootDirectorySectors__(); // root Directory
	}

	DWORD CMSDOS7::TBootSector::__getClusterSizeInBytes__() const{
		// returns the number of Bytes that a single Cluster can contain
		return nSectorsInCluster*sectorSize;
	}











	TStdWinError CMSDOS7::__recognizeDisk__(PImage image,PFormat pFormatBoot){
		// returns the result of attempting to recognize Image by this DOS as follows: ERROR_SUCCESS = recognized, ERROR_CANCELLED = user cancelled the recognition sequence, any other error = not recognized
		// - in case the Image is a physical floppy disk, determining the Type of Medium (type of floppy)
		TFormat fmt={ TMedium::FLOPPY_DD_525, 1,1,MSDOS7_SECTOR_BKBOOT, MSDOS7_SECTOR_LENGTH_STD_CODE,MSDOS7_SECTOR_LENGTH_STD, 1 };
		if (image->SetMediumTypeAndGeometry(&fmt,StdSidesMap,1)!=ERROR_SUCCESS || !image->GetNumberOfFormattedSides(0)){
			fmt.mediumType=TMedium::FLOPPY_DD_350;
			if (image->SetMediumTypeAndGeometry(&fmt,StdSidesMap,1)!=ERROR_SUCCESS || !image->GetNumberOfFormattedSides(0)){
				fmt.mediumType=TMedium::FLOPPY_HD_350;
				if (image->SetMediumTypeAndGeometry(&fmt,StdSidesMap,1)!=ERROR_SUCCESS || !image->GetNumberOfFormattedSides(0))
					return ERROR_UNRECOGNIZED_VOLUME; // unknown Medium Type
			}
		}
		// - finding Boot Sector
		bool bootSectorRecognized;
		const TPhysicalAddress bootChs=TBootSector::__getRecognizedChs__(image,true,&bootSectorRecognized);
		if (!bootSectorRecognized)
			return ERROR_UNRECOGNIZED_VOLUME; // neither normal nor backup Boot Sector recognized
		const PCBootSector bootSector=(PCBootSector)image->GetHealthySectorData(bootChs);
		// - MS-DOS recognized
		*pFormatBoot=fmt;
		bootSector->__getGeometry__(pFormatBoot); // receives only geometry; Medium Type received in MS-DOS ctor
		if (pFormatBoot->GetCountOfAllSectors()
			>= // testing minimal number of Sectors
			__cluster2logSector__( MSDOS7_DATA_CLUSTER_FIRST, bootSector )
		)
			return ERROR_SUCCESS;
		else
			return Utils::ErrorByOs( ERROR_VOLMGR_DISK_LAYOUT_PARTITIONS_TOO_SMALL, ERROR_UNRECOGNIZED_VOLUME );
	}

	PDos CMSDOS7::__instantiate__(PImage image,PCFormat pFormatBoot){
		return new CMSDOS7(image,pFormatBoot);
	}

	#define BOOSTED_CAPACITY	_T("Boosted capacity (beware under WinNT!)")
	#define ARCHIVE_CAPACITY	_T("Single archive (beware under WinNT!)")

	static const CFormatDialog::TStdFormat StdFormats[]={
		{ _T("Standard 1440 kB"), 0, {TMedium::FLOPPY_HD_350,79,2,18,MSDOS7_SECTOR_LENGTH_STD_CODE,MSDOS7_SECTOR_LENGTH_STD,1}, 1, 0, FDD_SECTOR_GAP3_STD, 2, 224 },
		{ BOOSTED_CAPACITY, 0, {TMedium::FLOPPY_HD_350,FDD_CYLINDERS_MAX-1,2,21,MSDOS7_SECTOR_LENGTH_STD_CODE,MSDOS7_SECTOR_LENGTH_STD,2}, 2, 20, 5, 2, 128 },
		{ ARCHIVE_CAPACITY, 0, {TMedium::FLOPPY_HD_350,FDD_CYLINDERS_MAX-1,2,21,MSDOS7_SECTOR_LENGTH_STD_CODE,MSDOS7_SECTOR_LENGTH_STD,16}, 2, 20, 5, 1, 16 },
		{ _T("Standard 720 kB"), 0, {TMedium::FLOPPY_DD_350,79,2,9,MSDOS7_SECTOR_LENGTH_STD_CODE,MSDOS7_SECTOR_LENGTH_STD,1}, 1, 0, FDD_SECTOR_GAP3_STD, 2, 224 },
		{ BOOSTED_CAPACITY, 0, {TMedium::FLOPPY_DD_350,FDD_CYLINDERS_MAX-1,2,10,MSDOS7_SECTOR_LENGTH_STD_CODE,MSDOS7_SECTOR_LENGTH_STD,2}, 2, 9, 5, 2, 128 },
		{ ARCHIVE_CAPACITY, 0, {TMedium::FLOPPY_DD_350,FDD_CYLINDERS_MAX-1,2,10,MSDOS7_SECTOR_LENGTH_STD_CODE,MSDOS7_SECTOR_LENGTH_STD,16}, 2, 9, 5, 1, 16 },
		{ _T("Hard disk 50 MB"), 0, {TMedium::HDD_RAW,99,16,63,MSDOS7_SECTOR_LENGTH_STD_CODE,MSDOS7_SECTOR_LENGTH_STD,4}, 1, 0, FDD_SECTOR_GAP3_STD, 2, 224 }
	};
	const CDos::TProperties CMSDOS7::Properties={
		_T("MS-DOS 7.1 (experimental)"), // name
		MAKE_DOS_ID('M','S','-','D','O','S','7','1'), // unique identifier
		90, // recognition priority (the bigger the number the earlier the DOS gets crack on the image)
		__recognizeDisk__, // recognition function
		__instantiate__, // instantiation function
		(TMedium::TType)(TMedium::FLOPPY_ANY|TMedium::HDD_RAW),
		&CImageRaw::Properties, // the most common Image to contain data for this DOS (e.g. *.D80 Image for MDOS)
		7,	// number of std Formats
		StdFormats, // std Formats
		1,127, // range of supported number of Sectors
		1+9+14, // minimal total number of Sectors required
		// ^ boot + 1xFAT + dir
		128, // maximum number of Sector in one Cluster (must be power of 2)
		32768, // maximum size of a Cluster (in Bytes)
		1,7, // range of supported number of allocation tables (FATs)
		1,16384, // range of supported number of root Directory entries
		1,	// lowest Sector number on each Track
		0xf6,UDirectoryEntry::EMPTY_ENTRY,	// regular Sector and Directory Sector filler Byte
		0,0 // number of reserved Bytes at the beginning and end of each Sector
	};










	CMSDOS7::CMsdos7BootView::CMsdos7BootView(PMSDOS7 msdos)
		// ctor
		// - base
		: CBootView(msdos,
					TBootSector::__getRecognizedChs__(
						msdos->image,
						false, // False = no attempt to recognize Boot Sector, just getting a readable Sector where a Boot is normally expected (this is for the case that the Image is being "Opened as")
						nullptr
					)
				) {
		// - extracting information from Boot Sector (for the case that the Image is being "Opened as")
		if (const PCBootSector sector=GetSectorData()) // if Null, it's an indirect proof that we are "Opening as" (the opening will be rejected in CreateUserInterface)
			sector->__getGeometry__(&msdos->formatBoot);
	}









	#define DOS		tab.dos
	#define IMAGE	DOS->image

	CMSDOS7::PBootSector CMSDOS7::CMsdos7BootView::GetSectorData() const{
		// returns data of Boot Sector (or Null if Boot Sector unreadable)
		return (PBootSector)IMAGE->GetHealthySectorData( GetPhysicalAddress() );
	}

	bool WINAPI CMSDOS7::CMsdos7BootView::__labelModified__(PropGrid::PCustomParam,LPCSTR newLabel,short newLabelChars){
		const PMSDOS7 msdos=(PMSDOS7)CDos::GetFocused();
		const PBootSector boot=msdos->boot.GetSectorData();
		TVolumeInfo *pvi;
		switch (msdos->fat.type){
			case CFat::FAT12:
			case CFat::FAT16:
				pvi=&boot->fat1216.volume;	break;
			case CFat::FAT32:
				pvi=&boot->fat32.volume;	break;
			default:
				ASSERT(FALSE);
				return false;
		}
		::memcpy(	::memset( pvi->label, ' ', MSDOS7_LABEL_LENGTH_MAX ),
					newLabel,
					newLabelChars
				);
		return __bootSectorModified__(nullptr,0);
	}

	void CMSDOS7::CMsdos7BootView::GetCommonBootParameters(RCommonBootParameters rParam,PSectorData _boot){
		// gets basic parameters from the Boot Sector
		const PBootSector boot=(PBootSector)_boot;
		const PMSDOS7 msdos=(PMSDOS7)DOS;
		rParam.geometryCategory=true;
			rParam.chs=true;
			rParam.sectorLength=true;
		rParam.volumeCategory=true;
			TVolumeInfo *pvi;
			switch (msdos->fat.type){
				case CFat::FAT12:
				case CFat::FAT16:
					pvi=&boot->fat1216.volume;	break;
				case CFat::FAT32:
					pvi=&boot->fat32.volume;	break;
				default:
					ASSERT(FALSE);
					return;
			}
			//rParam.label.length=0; // assumption (Label doesn't exist); commented out as already zeroed by the caller
			for( TMsdos7DirectoryTraversal dt(msdos,MSDOS7_DIR_ROOT); dt.__existsNextEntry__(); )
				if (dt.entryType==TDirectoryTraversal::CUSTOM
					&&
					((PCDirectoryEntry)dt.entry)->shortNameEntry.attributes==FILE_ATTRIBUTE_VOLUME
				){
					// Label found
					rParam.label.length=MSDOS7_LABEL_LENGTH_MAX;
						rParam.label.bufferA=((PDirectoryEntry)dt.entry)->shortNameEntry.name;
						rParam.label.fillerByte=' ';
						rParam.label.onLabelConfirmedA=__labelModified__;
					break;
				}
			rParam.id.buffer=&pvi->id;
				rParam.id.bufferCapacity=sizeof(DWORD);
			rParam.clusterSize=true;
	}

	bool WINAPI CMSDOS7::CMsdos7BootView::__onMediumChanged__(PVOID,PropGrid::Enum::UValue newValue){
		// Medium Type changed via PropertyGrid
		CMSDOS7 *const msdos=(CMSDOS7 *)CDos::GetFocused();
		// - changing the Medium Type in FAT
		msdos->fat.SetClusterValue( 0, msdos->fat.GetClusterValue(0)&0xffffff00|(BYTE)newValue.charValue );
		// - propagating the Medium type that has just been changed in Boot Sector to the inner FormatBoot structure
		msdos->__adoptMediumFromBootSector__();
		return __bootSectorModified__(nullptr,0);
	}
	PropGrid::Enum::PCValueList WINAPI CMSDOS7::CMsdos7BootView::__getListOfMedia__(PVOID,WORD &rnMedia){
		// returns the List of known Media
		static const TBootSector::TMsdosMedium List[]={
			TBootSector::DISK_35_1440_DS_18,
			TBootSector::DISK_35_720_DS_9,
			TBootSector::DISK_525_180_SS_9,
			TBootSector::DISK_525_360_DS_9,
			TBootSector::DISK_525_160_SS_8,
			TBootSector::DISK_525_320_DS_8,
			TBootSector::DISK_HARD
		};
		rnMedia=sizeof(List)/sizeof(TBootSector::TMsdosMedium);
		return (PropGrid::Enum::PCValueList)List;
	}
	LPCTSTR WINAPI CMSDOS7::CMsdos7BootView::__getMediumDescription__(PVOID,PropGrid::Enum::UValue medium,PTCHAR,short){
		// populates the Buffer with given Medium description and returns the description
		switch ((TBootSector::TMsdosMedium)medium.charValue){
			case TBootSector::DISK_35_1440_DS_18: return _T("3.5\" floppy, DS 1440kB");
			case TBootSector::DISK_35_720_DS_9	: return _T("3.5\" floppy, DS 720kB");
			case TBootSector::DISK_525_180_SS_9	: return _T("5.25\" floppy, SS 180kB");
			case TBootSector::DISK_525_360_DS_9	: return _T("5.25\" floppy, DS 360kB");
			case TBootSector::DISK_525_160_SS_8	: return _T("5.25\" floppy, SS 160kB");
			case TBootSector::DISK_525_320_DS_8	: return _T("5.25\" floppy, DS 320kB");
			case TBootSector::DISK_HARD			: return _T("Hard disk");
			default:
				return _T("<unknown>");
		}
	}

	PropGrid::Enum::PCValueList WINAPI CMSDOS7::CMsdos7BootView::__getListOfMediaTypes__(PVOID,WORD &rnMediumTypes){
		// returns the List of known Media
		static const TBootSector::TMsdosMediumType List[]={
			TBootSector::FLOPPY,
			TBootSector::HDD
		};
		rnMediumTypes=sizeof(List)/sizeof(TBootSector::TMsdosMediumType);
		return (PropGrid::Enum::PCValueList)List;
	}
	LPCTSTR WINAPI CMSDOS7::CMsdos7BootView::__getMediumTypeDescription__(PVOID,PropGrid::Enum::UValue mediumType,PTCHAR,short){
		// populates the Buffer with given Medium description and returns the description
		switch ((TBootSector::TMsdosMediumType)mediumType.charValue){
			case TBootSector::FLOPPY: return _T("Floppy");
			case TBootSector::HDD	: return _T("Hard disk");
			default:
				return _T("<unknown>");
		}
	}

	bool WINAPI CMSDOS7::CMsdos7BootView::__pg_createLabel__(PropGrid::PCustomParam,int hyperlinkId,LPCTSTR hyperlinkName){
		// True <=> PropertyGrid's Editor can be destroyed after this function has terminated, otherwise False
		const PMSDOS7 msdos=(PMSDOS7)CDos::GetFocused();
		if (const PDirectoryEntry de=TMsdos7DirectoryTraversal(msdos,MSDOS7_DIR_ROOT).__allocateNewEntry__()){
			de->shortNameEntry.attributes=FILE_ATTRIBUTE_VOLUME;
			::memcpy(	::memset(de->shortNameEntry.name,' ',MSDOS7_LABEL_LENGTH_MAX),
						VOLUME_LABEL_DEFAULT_ANSI_8CHARS,
						sizeof(VOLUME_LABEL_DEFAULT_ANSI_8CHARS)-1
					);
			__labelModified__( nullptr, de->shortNameEntry.name, MSDOS7_LABEL_LENGTH_MAX );
		}else
			Utils::Information(_T("Can't create label"),ERROR_CANNOT_MAKE);
		return true; // True = destroy PropertyGrid's Editor
	}

	void CMSDOS7::CMsdos7BootView::AddCustomBootParameters(HWND hPropGrid,HANDLE hGeometry,HANDLE hVolume,const TCommonBootParameters &rParam,PSectorData _boot){
		// gets DOS-specific parameters from the Boot
		const PBootSector boot=(PBootSector)_boot;
		const CFat::TType fatType=((PMSDOS7)DOS)->fat.type;
		TVolumeInfo *const pvi=	fatType==CFat::FAT32 ? &boot->fat32.volume : &boot->fat1216.volume;
		if (!rParam.label.length)
			PropGrid::AddProperty(	hPropGrid, hVolume, _T("Label not found"),
									"<a>Create</a>",
									PropGrid::Hyperlink::DefineEditorA( __pg_createLabel__, CBootView::__updateCriticalSectorView__ )
								);
		PropGrid::AddProperty(	hPropGrid, hVolume, _T("ID valid"),
								&pvi->infoValid,
								PropGrid::Boolean::DefineByteEditor( __bootSectorModified__, nullptr, 0x29, true )
							);
		const HANDLE hGeometryAdvanced=PropGrid::AddCategory(hPropGrid,hGeometry,BOOT_SECTOR_ADVANCED,true);
			// . Medium
			PropGrid::AddProperty(	hPropGrid, hGeometryAdvanced, _T("Medium"),
									&boot->medium,
									PropGrid::Enum::DefineConstStringListEditorA( sizeof(TBootSector::TMsdosMedium), __getListOfMedia__, __getMediumDescription__, nullptr, __onMediumChanged__ )
								);
			// . number of Sectors on the disk
			if (fatType==CFat::FAT32 || boot->nSectorsInTotal16==0)
				PropGrid::AddProperty(	hPropGrid, hGeometryAdvanced, _T("All sectors"),
										&boot->nSectorsInTotal32,
										PropGrid::Integer::DefineEditor( sizeof(TLogSector32), PropGrid::Integer::TUpDownLimits::PositiveInteger, __bootSectorModified__ )
									);
			else
				PropGrid::AddProperty(	hPropGrid, hGeometryAdvanced, _T("All sectors"),
										&boot->nSectorsInTotal16,
										PropGrid::Integer::DefinePositiveWordEditor( __bootSectorModified__ )
									);
			// . number of reserved Sectors
			PropGrid::AddProperty(	hPropGrid, hGeometryAdvanced, _T("Reserved sectors"),
									&boot->nReservedSectors,
									PropGrid::Integer::DefinePositiveWordEditor( __bootSectorModified__ )
								);
			// . number of hidden Sectors
			PropGrid::AddProperty(	hPropGrid, hGeometryAdvanced, _T("Hidden sectors"),
									&boot->nSectorsHidden,
									PropGrid::Integer::DefineEditor( sizeof(TLogSector32), PropGrid::Integer::TUpDownLimits::NonNegativeInteger, __bootSectorModified__ )
								);
		const HANDLE hVolumeAdvanced=PropGrid::AddCategory(hPropGrid,hVolume,BOOT_SECTOR_ADVANCED,true);
			// . OEM Name
			PropGrid::AddProperty(	hPropGrid, hVolumeAdvanced, _T("OEM name"),
									&boot->oemName,
									PropGrid::String::DefineFixedLengthEditorA( sizeof(boot->oemName), __bootSectorModifiedA__, ' ' )
								);
			// . MediumType
			PropGrid::AddProperty(	hPropGrid, hVolumeAdvanced, _T("Medium type"),
									fatType==CFat::FAT32?&boot->fat32.mediumType:&boot->fat1216.mediumType,
									PropGrid::Enum::DefineConstStringListEditorA( sizeof(TBootSector::TMsdosMediumType), __getListOfMediaTypes__, __getMediumTypeDescription__, nullptr, __bootSectorModified__ )
								);
			// . FAT Name
			PropGrid::AddProperty(	hPropGrid, hVolumeAdvanced, _T("FAT name"),
									&pvi->fatId,
									PropGrid::String::DefineFixedLengthEditorA( sizeof(pvi->fatId), __bootSectorModifiedA__, ' ' )
								);
			// . number of FAT copies
			static const PropGrid::Integer::TUpDownLimits ByteOneToSevenLimits={ 1, 7 };
			PropGrid::AddProperty(	hPropGrid, hVolumeAdvanced, _T("FAT copies"),
									&boot->nFatCopies,
									PropGrid::Integer::DefineEditor( sizeof(BYTE), ByteOneToSevenLimits, __bootSectorModified__ )
								);
			// . number of root Directory entries
			PropGrid::AddProperty(	hPropGrid, hVolumeAdvanced, _T("Root dir size"),
									&boot->nRootDirectoryEntries,
									PropGrid::Integer::DefinePositiveWordEditor( __bootSectorModified__ )
								);
			
	}










	void CMSDOS7::FlushToBootSector() const{
		// flushes internal Format information to the actual Boot Sector's data
		if (const PBootSector bootSector=boot.GetSectorData()){
			bootSector->nSectorsInTotal16=formatBoot.GetCountOfAllSectors();
			bootSector->nHeads=formatBoot.nHeads;
			bootSector->nSectorsOnTrack=formatBoot.nSectors;
			bootSector->nSectorsInCluster=formatBoot.clusterSize;
			bootSector->sectorSize=formatBoot.sectorLength;
			boot.MarkSectorAsDirty();
		}
	}

	#define ERR_MSG_FORMAT_CYLINDER_RANGE	_T("Given the current number of heads, sectors per track, and cluster size, the %simum number of cylinders is limited to %d for this instance of FAT%d. ")

	bool CMSDOS7::ValidateFormatChangeAndReportProblem(bool reformatting,PCFormat f) const{
		// True <=> specified Format is acceptable, otherwise False (and informing on error)
		// - base
		if (!__super::ValidateFormatChangeAndReportProblem(reformatting,f))
			return false;
		// - new CountOfClusters mustn't overflow nor underflow limits of current FAT Type
		if (reformatting){
			// NOT formatting from scratch, just reformatting some Cylinders
			const PCBootSector bootSector=boot.GetSectorData();
			if (!bootSector)
				return false;
			const TCylinder nCylindersMax=(	std::min<>(	fat.GetMaxCountOfClusters()
														,
														(bootSector->__getCountOfSectorsInOneFatCopy__()*bootSector->sectorSize-MSDOS7_DATA_CLUSTER_FIRST)*2/fat.type
													)
											*
											bootSector->nSectorsInCluster
											+
											bootSector->__getCountOfNondataSectors__()
										)/
										formatBoot.GetCountOfSectorsPerCylinder();
			if (f->nCylinders>nCylindersMax){
				TCHAR buf[200];
				::wsprintf(buf,ERR_MSG_FORMAT_CYLINDER_RANGE _T("The type of FAT can be changed only by formatting from Cylinder 0."),"max",nCylindersMax,4*fat.type);
				Utils::Information(buf);
				return false;
			}
			const TCylinder nCylindersMin=(	fat.GetMinCountOfClusters()
											*
											bootSector->nSectorsInCluster
											+
											bootSector->__getCountOfNondataSectors__()
										)/
										formatBoot.GetCountOfSectorsPerCylinder();
			if (f->nCylinders<nCylindersMin){
				TCHAR buf[200];
				::wsprintf(buf,ERR_MSG_FORMAT_CYLINDER_RANGE _T("The type of FAT cannot be changed."),"min",nCylindersMin,4*fat.type);
				Utils::Information(buf);
				return false;
			}
		}//else
			// formatting from scratch (e.g. an empty disk)
			//nop
		// - new Format is acceptable
		return true;
	}
