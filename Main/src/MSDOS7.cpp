#include "stdafx.h"
#include "MSDOS7.h"

	#define INI_MSDOS7	_T("MSDOS7")

	#define INI_DONT_SHOW_LONG_NAMES	_T("vfatoff")
	#define INI_DONT_SHOW_DOT			_T("dotoff")
	#define INI_DONT_SHOW_DOTDOT		_T("dotdotoff")

	CMSDOS7::CMSDOS7(PImage image,PCFormat pFormatBoot)
		// ctor
		// - base
		: CDos( image, pFormatBoot, TTrackScheme::BY_CYLINDERS, &Properties, ::lstrcmpi, CDos::StdSidesMap, IDR_MSDOS, &fileManager, TGetFileSizeOptions::OfficialDataLength )
		// - initialization
		, fat(*this) , fsInfo(this)
		, trackMap(this) , boot(this) , fileManager(this)
		, dontShowLongFileNames( __getProfileBool__(INI_DONT_SHOW_LONG_NAMES,false) )
		, dontShowDotEntries( __getProfileBool__(INI_DONT_SHOW_DOT,false) )
		, dontShowDotdotEntries( __getProfileBool__(INI_DONT_SHOW_DOTDOT,false) ) {
		SwitchToDirectory(MSDOS7_DIR_ROOT);
		if (const PCBootSector bootSector=boot.GetSectorData()) // may not exist when creating new Image
			if (bootSector->__isUsable__()){ // may not be usable if Image is being "Open As"
				// . determining the type of FAT
				BYTE b;
				const TLogSector32 nDataSectors=bootSector->__getCountOfAllSectors__()-__cluster2logSector__(MSDOS7_DATA_CLUSTER_FIRST,b);
				const TCluster32 nClusters=nDataSectors/bootSector->nSectorsInCluster;
				fat.type=CFat::GetFatType(nClusters);
				// . propagating the Medium type stored in Boot Sector to the inner FormatBoot structure
				__adoptMediumFromBootSector__();
			}
	}











	void CMSDOS7::__informationWithCheckableShowNoMore__(LPCTSTR text,LPCTSTR messageId){
		// shows a MessageBox with added "Don't show anymore" check-box
		Utils::InformationWithCheckableShowNoMore( text, INI_MSDOS7, messageId );
	}

	CMSDOS7::TLogSector32 CMSDOS7::__fyzlog__(RCPhysicalAddress chs) const{
		// converts PhysicalAddress to LogicalSector number and returns it
		return (chs.cylinder*formatBoot.nHeads+chs.head)*formatBoot.nSectors+chs.sectorId.sector-1; // "-1" = Sectors numbered from 1
	}
	TPhysicalAddress CMSDOS7::__logfyz__(TLogSector32 ls) const{
		// converts LogicalSector number to PhysicalAddress and returns it
		const div_t A=div( ls, formatBoot.nSectors ), B=div( A.quot, formatBoot.nHeads );
		const TPhysicalAddress chs={ B.quot, B.rem, { B.quot, sideMap[B.rem], A.rem+1, formatBoot.sectorLengthCode } }; // "+1" = Sectors numbered from 1
		return chs;
	}

	CMSDOS7::TLogSector32 CMSDOS7::__cluster2logSector__(TCluster32 c,PCBootSector boot){
		// determines and returns the first LogicalSector in specified Cluster
		return	boot->__getCountOfNondataSectors__() // number of all NON-data Sectors in this Volume (Boot, FAT, etc.)
				+
				(c-MSDOS7_DATA_CLUSTER_FIRST)*boot->nSectorsInCluster;
	}
	CMSDOS7::TLogSector32 CMSDOS7::__cluster2logSector__(TCluster32 cluster,BYTE &rnSectorsInCluster) const{
		// determines and returns the first LogicalSector in specified Cluster
		if (const PCBootSector bootSector=boot.GetSectorData()){
			rnSectorsInCluster=bootSector->nSectorsInCluster;
			return __cluster2logSector__( cluster, bootSector );
		}else
			return rnSectorsInCluster=0;
	}
	CMSDOS7::TCluster32 CMSDOS7::__logSector2cluster__(TLogSector32 ls) const{
		// determines and returns the Cluster of which the specified LogicalSector is part of
		if (const PCBootSector bootSector=boot.GetSectorData()){
			ls-=bootSector->__getCountOfNondataSectors__(); // number of all NON-data Sectors in this Volume (Boot, FAT, etc.)
			if (const BYTE nSectorsInCluster=bootSector->nSectorsInCluster)
				return ls/nSectorsInCluster+MSDOS7_DATA_CLUSTER_FIRST;
		}
		return 0;
	}

	CMSDOS7::TCluster32 CMSDOS7::__getCountOfClusters__() const{
		// determines and returns the count of Clusters that can be used to store data (i.e. without the first two Clusters, i.e. minus MSDOS7_DATA_CLUSTER_FIRST)
		if (const PCBootSector bootSector=boot.GetSectorData())
			if (bootSector->nSectorsInCluster){
				BYTE b;
				const TLogSector32 nDataSectors=bootSector->__getCountOfAllSectors__()-__cluster2logSector__(MSDOS7_DATA_CLUSTER_FIRST,b);
				const TCluster32 nClusters=nDataSectors/bootSector->nSectorsInCluster;
				switch (fat.type){
					case CFat::FAT12:
						return nClusters;
					case CFat::FAT16:
						return std::min<TCluster32>( nClusters, (MSDOS7_FAT_CLUSTER_BAD-MSDOS7_DATA_CLUSTER_FIRST)&0xffff );
					case CFat::FAT32:
						return std::min<TCluster32>( nClusters, MSDOS7_FAT_CLUSTER_BAD-MSDOS7_DATA_CLUSTER_FIRST );
					default:
						ASSERT(FALSE);
				}
			}
		return 0;
	}

	void CMSDOS7::__adoptMediumFromBootSector__(){
		// propagates the Medium stored in the Boot Sector to the inner FormatBoot structure by translating it to one of RIDE Medium Types
		if (const PCBootSector bootSector=boot.GetSectorData()) // should always exist, but just to be sure
			switch (bootSector->medium){ // translating only Medium information, geometry not taken into account (it's assumed the disk is MS-DOS 2.x or higher where geometry is given explicitly by information from Boot Sector)
				case TBootSector::DISK_35_1440_DS_18:formatBoot.mediumType=TMedium::FLOPPY_HD; break;
				case TBootSector::DISK_35_720_DS_9	:
				case TBootSector::DISK_525_180_SS_9	:
				case TBootSector::DISK_525_360_DS_9	:
				case TBootSector::DISK_525_160_SS_8	:
				case TBootSector::DISK_525_320_DS_8	:formatBoot.mediumType=TMedium::FLOPPY_DD; break;
				case TBootSector::DISK_HARD			:formatBoot.mediumType=TMedium::HDD_RAW; break;
				default: ASSERT(FALSE);
			}
		else
			ASSERT(FALSE); // caller guarantees that this function is called only if Boot Sector can be found
	}

	PSectorData CMSDOS7::__getHealthyLogicalSectorData__(TLogSector32 logSector) const{
		// returns data of LogicalSector, or Null of such Sector is unreadable or doesn't exist
		return image->GetHealthySectorData( __logfyz__(logSector) );
	}

	void CMSDOS7::__markLogicalSectorAsDirty__(TLogSector32 logSector) const{
		// marks given LogicalSector as dirty
		image->MarkSectorAsDirty( __logfyz__(logSector) );
	}








	#define MARKS_CLUSTER_EOF(c)\
				(c>=MSDOS7_FAT_CLUSTER_EOF)

	bool CMSDOS7::GetSectorStatuses(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PSectorStatus buffer) const{
		// True <=> Statuses of all Sectors in the Track successfully retrieved and populated the Buffer, otherwise False
		bool result=true; // assumption (Statuses of all Sectors successfully retrieved)
		const TPhysicalAddress chsBase={ cyl, head, { cyl, sideMap[head], 0, formatBoot.sectorLengthCode } };
		BYTE b;
		for( const TLogSector32 logSectorBase=__fyzlog__(chsBase),logSectorDataA=__cluster2logSector__(MSDOS7_DATA_CLUSTER_FIRST,b),logSectorDataZ=__cluster2logSector__(MSDOS7_DATA_CLUSTER_FIRST+__getCountOfClusters__(),b); nSectors--; bufferId++ ){
			const TSector sector=bufferId->sector;
			if (cyl>=formatBoot.nCylinders || head>=formatBoot.nHeads || bufferId->cylinder!=cyl || bufferId->side!=sideMap[head] || sector>formatBoot.nSectors || !sector || bufferId->lengthCode!=formatBoot.sectorLengthCode) // condition for Sector must be ">", not ">=" (Sectors numbered from 1 - see also "|!id")
				// Sector number out of official Format
				*buffer++=TSectorStatus::UNKNOWN;
			else{
				// getting Sector Status from FAT
				const TLogSector32 ls=sector+logSectorBase;
				if (ls>=logSectorDataA)
					if (ls<logSectorDataZ)
						switch (const DWORD value=fat.GetClusterValue(__logSector2cluster__(ls))){
							case MSDOS7_FAT_ERROR:
								*buffer++=TSectorStatus::UNKNOWN;
								result=false;
								break;
							case MSDOS7_FAT_CLUSTER_BAD:
								*buffer++=TSectorStatus::BAD;
								break;
							case MSDOS7_FAT_CLUSTER_EMPTY:
								*buffer++=TSectorStatus::EMPTY;
								break;
							default:
								*buffer++=TSectorStatus::OCCUPIED;
								break;
						}
					else
						*buffer++=TSectorStatus::EMPTY; // Sectors "beyond" FAT considered as "potentially Free"
				else
					*buffer++=TSectorStatus::SYSTEM;
			}
		}
		return result;
	}

	bool CMSDOS7::ModifyStdSectorStatus(RCPhysicalAddress chs,TSectorStatus status) const{
		// True <=> the Status of the specified DOS-standard Sector successfully changed, otherwise False
		if (const PCBootSector bootSector=boot.GetSectorData()){
			// Boot Sector exists
			DWORD value;
			switch (status){
				case TSectorStatus::EMPTY:
					value=MSDOS7_FAT_CLUSTER_EMPTY;
					break;
				default:
					ASSERT(FALSE);
				case TSectorStatus::UNAVAILABLE:
				case TSectorStatus::BAD:
					value=MSDOS7_FAT_CLUSTER_BAD;
					break;
			}
			const TLogSector32 ls=__fyzlog__(chs);
			BYTE b;
			return	ls>=__cluster2logSector__(MSDOS7_DATA_CLUSTER_FIRST,b)
					? fat.SetClusterValue( __logSector2cluster__(ls), value )
					: true;
		}else{
			// Boot Sector doesn't exist (may happen after unsuccessfull formatting)
			::SetLastError(ERROR_SECTOR_NOT_FOUND);
			return false;
		} 
	}








	bool CMSDOS7::GetFileFatPath(PCFile file,CFatPath &rFatPath) const{
		// True <=> FatPath of given File (even an erroneous FatPath) successfully retrieved, otherwise False
		// - if queried about a Directory, populating the FatPath with its Sectors
		CFatPath::TItem item;
		item.value=	file!=MSDOS7_DIR_ROOT
					? ((PDirectoryEntry)file)->shortNameEntry.__getFirstCluster__()
					: 0;
		if (IsDirectory(file)){
			item.chs=boot.GetPhysicalAddress();
			for( TMsdos7DirectoryTraversal dt(this,file); dt.__existsNextEntry__(); item.value++ )
				if (item.chs!=dt.chs){
					item.chs=dt.chs;
					if (!rFatPath.AddItem(&item)) break; // also sets an error in FatPath
				}
			return true;
		}
		// - no FatPath can be retrieved if DirectoryEntry is Empty
		if (*(PCBYTE)file==UDirectoryEntry::EMPTY_ENTRY)
			return false;
		// - zero-length File has no Clusters allocated
		if (!item.value)
			return true; // success despite the FatPath is empty (as no Clusters allocated)
		// - extracting the FatPath from FAT
		const TCluster32 clusterMax=MSDOS7_DATA_CLUSTER_FIRST+__getCountOfClusters__();
		do{
			// . adding all Sectors in Cluster to FatPath
			BYTE n;
			for( TLogSector32 ls=__cluster2logSector__(item.value,n); n--; ls++ ){
				// : determining Sector's PhysicalAddress
				item.chs=__logfyz__(ls);
				// : adding the Item to the FatPath
				if (!rFatPath.AddItem(&item)) // also sets error in FatPath
					return true; // True = FatPath retrieved (although it's with error)
			}
			// . VALIDATION: Value must "make sense"
			if (item.value==MSDOS7_FAT_CLUSTER_BAD){
				rFatPath.error=CFatPath::TError::VALUE_BAD_SECTOR;
				break;
			}
			if (!(	(item.value>=MSDOS7_DATA_CLUSTER_FIRST && item.value<clusterMax) // unknown Cluster addressed
					||
					MARKS_CLUSTER_EOF(item.value) // natural end of File in FAT
			)){
				rFatPath.error=CFatPath::TError::VALUE_INVALID;
				break;
			}
			// . VALIDATION: next Item can be retrieved
			if (( item.value=fat.GetClusterValue(item.value) )==MSDOS7_FAT_ERROR){ // if FAT Sector with next Item cannot be read ...
				rFatPath.error=CFatPath::TError::SECTOR; // ... setting corresponding error ...
				break; // ... and quitting
			}
		}while (!MARKS_CLUSTER_EOF(item.value)); // repeating until natural end of File is found
		return true; // FatPath (with or without error) successfully extracted from FAT
	}

	bool CMSDOS7::ModifyFileFatPath(PFile file,const CFatPath &rFatPath) const{
		// True <=> a error-free FatPath of given File successfully written, otherwise False
		CFatPath::PCItem pItem; DWORD nItems;
		if (rFatPath.GetItems(pItem,nItems)) // if FatPath erroneous ...
			return false; // ... we are done
		const PDirectoryEntry de=(PDirectoryEntry)file;
		if (!de->shortNameEntry.size) // zero-length Files are in NOT in the FAT
			return true;
		TCluster32 cluster0=__logSector2cluster__(__fyzlog__( pItem++->chs ));
		de->shortNameEntry.__setFirstCluster__(cluster0);
		for( TCluster32 c; --nItems; cluster0=c ) // all Sectors but the last one are Occupied in FatPath
			fat.SetClusterValue(cluster0, // no need to test readability of FAT Sector - tested by the caller
								c=__logSector2cluster__(__fyzlog__(pItem++->chs))
							);
		fat.SetClusterValue( cluster0, MSDOS7_FAT_CLUSTER_EOF ); // terminating the File's FatPath in FAT
		return true;
	}

	DWORD CMSDOS7::GetFreeSpaceInBytes(TStdWinError &rError) const{
		// computes and returns the empty space on disk
		rError=ERROR_SUCCESS;
		const PCBootSector bootSector=boot.GetSectorData(); // guaranteed to be found at this point
		const DWORD nBytesInCluster=bootSector->__getClusterSizeInBytes__();
		if (const PFsInfoSector fsInfoSector=fsInfo.GetSectorData()){
			// for FAT32, computing the free space quickly from available FS Info Sector
			if (fsInfoSector->nFreeClusters>__getCountOfClusters__()){
				// value in FsInfoSector out of range - fixing it
				fsInfoSector->nFreeClusters=__super::GetFreeSpaceInBytes(rError) // base, doing the hard work to compute the free space
											/
											nBytesInCluster;
				fsInfo.MarkSectorAsDirty();
			}
			return fsInfoSector->nFreeClusters*nBytesInCluster;
		}else{
			// for FAT32 without FS Info Sector or for FAT16/FAT12, using the temporary information on free space
			if (fat.nFreeClustersTemp>__getCountOfClusters__())
				// value out of range - fixing it
				fat.nFreeClustersTemp =	__super::GetFreeSpaceInBytes(rError) // base, doing the hard work to compute the free space
										/
										nBytesInCluster;
			return fat.nFreeClustersTemp*nBytesInCluster;
		}
	}

	CMSDOS7::TCluster32 CMSDOS7::__getFirstFreeHealthyCluster__() const{
		// searches for and returns the first Cluster that's reported Empty and is fully intact (i.e. is readable, and thus assumed also writeable); returns MSDOS7_FAT_CLUSTER_EOF if no free healthy Cluster can be found
		for( TPhysicalAddress chs; GetFirstEmptyHealthySector(true,chs)==ERROR_SUCCESS; ){
			// found an empty healthy Sector
			// . checking whether the whole Cluster is healthy
			const TCluster32 cluster=__logSector2cluster__( __fyzlog__(chs) );
			bool healthy=true; // assumption
			BYTE n;
			for( TLogSector32 ls=__cluster2logSector__(cluster,n); n--; healthy&=__getHealthyLogicalSectorData__(ls++)!=nullptr );
			// . if unhealthy, marking the Cluster as Bad in FAT and proceeding with the next Cluster
			if (!healthy){
				fat.SetClusterValue(cluster,MSDOS7_FAT_CLUSTER_BAD); // marking the Cluster as Bad
				continue;
			}
			// . updating the information on first free Cluster
			if (const PFsInfoSector fsInfoSector=fsInfo.GetSectorData()){
				// for FAT32, using the FS-Info Sector
				fsInfoSector->firstFreeCluster=cluster; // just in case the value was invalid
				fsInfo.MarkSectorAsDirty();
			}else
				// for FAT32 without FS-Info Sector or for FAT16/FAT12, using the temporary information on first free Cluster
				fat.firstFreeClusterTemp=cluster; // just in case the value was invalid
			// . an empty healthy Cluster found - we are done
			return cluster;
		}
		return MSDOS7_FAT_CLUSTER_EOF; // no free healhy Cluster found
	}

	TCylinder CMSDOS7::GetFirstCylinderWithEmptySector() const{
		// determines and returns the first Cylinder which contains at least one Empty Sector
		const TCluster32 clusterMax=MSDOS7_DATA_CLUSTER_FIRST+__getCountOfClusters__();
		const PFsInfoSector fsInfoSector=fsInfo.GetSectorData();
		TCluster32 result=MSDOS7_DATA_CLUSTER_FIRST;
		if (fsInfoSector){
			// for FAT32, using the FS-Info Sector
			if (fsInfoSector->firstFreeCluster<clusterMax) // FS-Info Sector exists and contains initialized value
				result=fsInfoSector->firstFreeCluster;
		}else
			// for FAT32 without FS-Info Sector or for FAT16/FAT12, using the temporary information on first free Cluster
			if (fat.firstFreeClusterTemp<clusterMax)
				result=fat.firstFreeClusterTemp;
		BYTE b;
		return __logfyz__( __cluster2logSector__(result,b) ).cylinder;
	}

	#define FILE_ATTRIBUTE_LONGNAME			( FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_VOLUME )
	#define FILE_ATTRIBUTE_LONGNAME_MASK	( FILE_ATTRIBUTE_LONGNAME | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_ARCHIVE )

	#define LONG_FILE_NAME_ENTRIES_COUNT_MAX	32

	BYTE CMSDOS7::__getLongFileNameEntries__(PCDirectoryEntry de,PDirectoryEntry *bufLongNameEntries) const{
		// populates Buffer with long name entries of the specified File and returns their count (returns 0 if no long name found); assumed that the Buffer capacity is at least LONG_FILE_NAME_ENTRIES_COUNT_MAX entries
		// - root Directory has no long name
		if (de==MSDOS7_DIR_ROOT)
			return 0;
		// - for Files marked with Volume Attribute the long name doesn't exist
		if (de->shortNameEntry.attributes&FILE_ATTRIBUTE_VOLUME)
			return 0;
		// - getting DirectoryEntries that potentially can comprise the File's long name
		PDirectoryEntry tmpBuf[LONG_FILE_NAME_ENTRIES_COUNT_MAX]; // cyclic buffer whose items are addressed using modulo implemented as "&(N-1)"
		::ZeroMemory(tmpBuf,sizeof(tmpBuf));
		BYTE i=0;
		TMsdos7DirectoryTraversal dt(this,currentDir);
		while (dt.__existsNextEntry__())
			if (( tmpBuf[i=++i&(LONG_FILE_NAME_ENTRIES_COUNT_MAX-1)]=(PDirectoryEntry)dt.entry )==de)
				break;
		if (dt.entry!=de) return 0; // given File doesn't feature a long name
		// - testing that found DirectoryEntries really comprise a long name
		BYTE sequenceNumber=1;
		for( const BYTE checksum=de->shortNameEntry.__getChecksum__(); sequenceNumber<LONG_FILE_NAME_ENTRIES_COUNT_MAX; sequenceNumber++ )
			if (const PDirectoryEntry de=tmpBuf[i=--i&(LONG_FILE_NAME_ENTRIES_COUNT_MAX-1)]){
				// . testing Entry's SequenceNumber
				const bool isLast=(de->longNameEntry.sequenceNumber&UDirectoryEntry::LONG_NAME_END)!=0;
				if ((de->longNameEntry.sequenceNumber&~UDirectoryEntry::LONG_NAME_END)!=sequenceNumber)
					break;
				// . testing Entry's Attributes
				if ((de->longNameEntry.attributes&FILE_ATTRIBUTE_LONGNAME_MASK)!=FILE_ATTRIBUTE_LONGNAME)
					break;
				// . testing that Entry's Zero fields are really zero
				if (de->longNameEntry.zero1 || de->longNameEntry.zero2)
					break;
				// . testing Entry's Checksum
				if (de->longNameEntry.checksum!=checksum)
					break;
				// . yes, the found Entry is part of File's long name
				*bufLongNameEntries++=de;
				if (isLast)
					return sequenceNumber;
			}else
				break;
		return 0; // long File name DirectoryEntries not found
	}

	void CMSDOS7::__deleteLongFileNameEntries__(PCDirectoryEntry de) const{
		// deletes Entries that are part of File's long name (if any)
		PDirectoryEntry longNameEntries[LONG_FILE_NAME_ENTRIES_COUNT_MAX],*p=longNameEntries;
		for( BYTE n=__getLongFileNameEntries__(de,longNameEntries); n--; ){
			de=*p++;
			*(PBYTE)de=UDirectoryEntry::EMPTY_ENTRY;
			MarkDirectorySectorAsDirty(de);
		}
	}

	#define KANJI				(char)0xe5
	#define KANJI_PLACEHOLDER	5

	void CMSDOS7::__getShortFileNameAndExt__(PCDirectoryEntry de,PTCHAR bufName,PTCHAR bufExt) const{
		// populates the Buffer with short File name and returns the Buffer; caller guarantess that Buffer's capacity is at least MAX_PATH chars
		if (de==MSDOS7_DIR_ROOT){
			if (bufName) ::lstrcpy(bufName,_T("\\"));
			if (bufExt)	 *bufExt='\0';
		}else if (*(PDWORD)de==MSDOS7_DIR_DOT){
			if (bufName) ::lstrcpy(bufName,_T("."));
			if (bufExt)	 *bufExt='\0';
		}else if (*(PDWORD)de==MSDOS7_DIR_DOTDOT){
			if (bufName) ::lstrcpy(bufName,_T(".."));
			if (bufExt)	 *bufExt='\0';
		}else{
			if (bufName){
				#ifdef UNICODE
					ASSERT(FALSE)
				#else
					::lstrcpynA( bufName, de->shortNameEntry.name, MSDOS7_FILE_NAME_LENGTH_MAX+1 );
				#endif
				for( PTCHAR p=bufName+MSDOS7_FILE_NAME_LENGTH_MAX; p--!=bufName; ) // trimming trailing spaces in Name
					if (*p==' ') *p='\0'; else break;
				if (*bufName==KANJI_PLACEHOLDER) *bufName=KANJI; // Kanji character at the beginning of the Name
			}
			if (bufExt){
				#ifdef UNICODE
					ASSERT(FALSE)
				#else
					::lstrcpynA( bufExt, de->shortNameEntry.extension, MSDOS7_FILE_EXT_LENGTH_MAX+1 );
				#endif
				for( PTCHAR p=bufExt+MSDOS7_FILE_EXT_LENGTH_MAX; p--!=bufExt; ) // trimming trailing spaces in Extension
					if (*p==' ') *p='\0'; else break;
			}
		}
	}

	bool CMSDOS7::__getLongFileNameAndExt__(PCDirectoryEntry de,PTCHAR bufName,PTCHAR bufExt) const{
		// populates the Buffer with long File name and returns the Buffer; returns Null if the File has no long name); caller guarantess that Buffer's capacity is at least MAX_PATH chars
		PDirectoryEntry longNameEntries[LONG_FILE_NAME_ENTRIES_COUNT_MAX];
		if (BYTE nEntries=__getLongFileNameEntries__(de,longNameEntries)){
			#ifdef UNICODE
				ASSERT(FALSE)
			#else
				WCHAR bufW[MAX_PATH],tmpW[14];
				bufW[0]='\0', tmpW[13]='\0';
				for( PDirectoryEntry *p=longNameEntries; nEntries--; ::lstrcatW(bufW,tmpW) ){
					de=*p++;
					::memcpy( tmpW, de->longNameEntry.name1, 10 );
					::memcpy( tmpW+5, de->longNameEntry.name2, 12 );
					::memcpy( tmpW+11, de->longNameEntry.name3, 4 );
				}
				if (const PWCHAR pDot=::wcsrchr(bufW,'.')){
					*pDot='\0';
					if (bufName)
						::WideCharToMultiByte( CP_ACP, 0, bufW,-1, bufName,MAX_PATH, nullptr,nullptr );
					if (bufExt)
						::WideCharToMultiByte( CP_ACP, 0, pDot+1,-1, bufExt,MAX_PATH, nullptr,nullptr );
				}else{
					if (bufName)
						::WideCharToMultiByte( CP_ACP, 0, bufW,-1, bufName,MAX_PATH, nullptr,nullptr );
					if (bufExt)
						*bufExt='\0';
				}
			#endif
			return true;
		}else
			return false;
	}

	bool CMSDOS7::GetFileNameOrExt(PCFile file,PPathString pOutName,PPathString pOutExt) const{
		// populates the Buffers with File's name and extension; caller guarantees that the Buffer sizes are at least MAX_PATH characters each
		// - attempting to get File's long name
		TCHAR bufName[MAX_PATH],bufExt[MAX_PATH];
		if (!dontShowLongFileNames)
			if (__getLongFileNameAndExt__((PCDirectoryEntry)file,bufName,bufExt)){
				if (pOutName) *pOutName=bufName;
				if (pOutExt) *pOutExt=bufExt;
				return true; // name relevant
			}
		// - only short name can be get for given File
		__getShortFileNameAndExt__((PCDirectoryEntry)file,bufName,bufExt);
		if (pOutName) *pOutName=bufName;
		if (pOutExt) *pOutExt=bufExt;
		return true; // name relevant
	}


	TStdWinError CMSDOS7::__changeShortFileNameAndExt__(PDirectoryEntry de,LPCTSTR newName,LPCTSTR newExt,PDirectoryEntry &rRenamedFile) const{
		// tries to change given File's short name and extension; returns Windows standard i/o error
		ASSERT(newName!=nullptr && newExt!=nullptr);
		// - can't change root Directory's name
		if (de==MSDOS7_DIR_ROOT)
			return ERROR_DIRECTORY;
		// - checking that the NewName+NewExt combination follows the "8.3" convention
		if (::lstrlen(newName)>MSDOS7_FILE_NAME_LENGTH_MAX || ::lstrlen(newExt)>MSDOS7_FILE_EXT_LENGTH_MAX)
			return ERROR_FILENAME_EXCED_RANGE;
		// - making sure that the NewNameAndExtension is without forbidden characters
		//TODO: carrying out this check only upon demand (a new user setting)
		//if don't check
			// still checking for presence of lowercase letters - those cannot be in a short name! (unless manually tweaked)
		//else{
			for( LPCTSTR p=newName; const TCHAR c=*p++; )
				if (!UDirectoryEntry::TShortNameEntry::__isCharacterValid__(c))
					return ERROR_ILLEGAL_CHARACTER;
			for( LPCTSTR p=newExt; const TCHAR c=*p++; )
				if (!UDirectoryEntry::TShortNameEntry::__isCharacterValid__(c))
					return ERROR_ILLEGAL_CHARACTER;
		//}
		// - making sure that the NewName+NewExt combination is not empty
		//TODO
		// - making sure that the NewName+NewExt combination isn't used by another File in CurrentDirectory
		for( TMsdos7DirectoryTraversal dt(this,currentDir); dt.__existsNextEntry__(); )
			if (dt.entry!=de)
				if (dt.entryType==TDirectoryTraversal::FILE || dt.entryType==TDirectoryTraversal::SUBDIR){
					TCHAR tmpName[MAX_PATH],tmpExt[MAX_PATH];
					__getShortFileNameAndExt__( (PCDirectoryEntry)dt.entry, tmpName, tmpExt );
					if (!::lstrcmpi(newName,tmpName)&&!lstrcmpi(newExt,tmpExt)){
						rRenamedFile=(PDirectoryEntry)dt.entry;
						return ERROR_FILE_EXISTS;
					}
				}
		// - renaming
		PDirectoryEntry longNameEntries[LONG_FILE_NAME_ENTRIES_COUNT_MAX];
		BYTE n=__getLongFileNameEntries__(de,longNameEntries);
		::memset( de->shortNameEntry.name, ' ', MSDOS7_FILE_NAME_LENGTH_MAX+MSDOS7_FILE_EXT_LENGTH_MAX );
		TCHAR tmp[MSDOS7_FILE_NAME_LENGTH_MAX+1];
		#ifdef UNICODE
			ASSERT(FALSE);
		#else
			::memcpy( de->shortNameEntry.name, ::CharUpper(::lstrcpy(tmp,newName)), ::lstrlen(newName) );
			::memcpy( de->shortNameEntry.extension, ::CharUpper(::lstrcpy(tmp,newExt)), ::lstrlen(newExt) );
		#endif
		MarkDirectorySectorAsDirty( rRenamedFile=de );
		// - recalculating the Checksum for File's long name Entries
		PDirectoryEntry *p=longNameEntries;
		for( const BYTE checksum=de->shortNameEntry.__getChecksum__(); n--; ){
			de=*p++;
			de->longNameEntry.checksum=checksum;
			MarkDirectorySectorAsDirty(de);
		}
		return ERROR_SUCCESS;
	}

	static PTCHAR __convertLongToShortTerm__(PTCHAR bufShort,BYTE bufShortChars,LPCTSTR bufLong){
		// returns BufferShort with long term (File name or extension) converted to short equivalent
		PTCHAR j=bufShort;
			while (const TCHAR c=*bufLong++)
				if (c!=' ' && c!='.'){ // preserving any other character but space or dot
					*j++=c;
					if (j-bufShort==bufShortChars) break;
				}
		*j='\0';
		return ::CharUpper(bufShort);
	}

	void CMSDOS7::__generateShortFileNameAndExt__(PDirectoryEntry de,LPCTSTR longName,LPCTSTR longExt) const{
		// generates and sets File's short name and extension based on specified LongNameAndExtension
		ASSERT(longName!=nullptr && longExt!=nullptr);
		// - converting to upper-case, and removing all spaces and intermediate dots
		TCHAR bufShortName[MSDOS7_FILE_NAME_LENGTH_MAX+1], bufShortExt[MSDOS7_FILE_EXT_LENGTH_MAX+1]; // "+1" = to accommodate terminating null character
		__convertLongToShortTerm__( bufShortName, MSDOS7_FILE_NAME_LENGTH_MAX, longName );
		__convertLongToShortTerm__( bufShortExt, MSDOS7_FILE_EXT_LENGTH_MAX, longExt );
		// - attempting to use the shortened "8.3" format
		PDirectoryEntry renamedFile;
		if (!::lstrcmp(bufShortName,longName)&&!::lstrcmp(bufShortExt,longExt)) // if the input LongName+LongExt combination is already in usable "8.3" format ...
			if (__changeShortFileNameAndExt__(de,longName,longExt,renamedFile)==ERROR_SUCCESS) // ... trying to use it first before generating an artificial "8.3" short name
				return;
		// - generating an artificial "8.3" short name
		const BYTE nCharsInShortName=::lstrlen(bufShortName);
		DWORD numericTail=1; // numeric tail (e.g. the "1" in "PROGRA~1", which is the short name for "Program Files")
		TCHAR tmp[MSDOS7_FILE_NAME_LENGTH_MAX+1];
		do{
			TCHAR bufNumericTail[MSDOS7_FILE_NAME_LENGTH_MAX+1];
			const BYTE nCharsInTail=::wsprintf(bufNumericTail,_T("~%d"),numericTail++);
			if (nCharsInShortName+nCharsInTail<MSDOS7_FILE_NAME_LENGTH_MAX)
				::lstrcat( ::lstrcpy(tmp,bufShortName), bufNumericTail );
			else
				_stprintf( tmp, _T("%.*s%s"), MSDOS7_FILE_NAME_LENGTH_MAX-nCharsInTail, bufShortName, bufNumericTail );
		}while (__changeShortFileNameAndExt__(de,tmp,bufShortExt,renamedFile)!=ERROR_SUCCESS);
	}

	TStdWinError CMSDOS7::__changeLongFileNameAndExt__(PDirectoryEntry de,LPCTSTR newName,LPCTSTR newExt,PDirectoryEntry &rRenamedFile) const{
		// tries to change given File's long name and extension; returns Windows standard i/o error
		ASSERT(newName!=nullptr && newExt!=nullptr);
		// - can't change root Directory's name
		if (de==MSDOS7_DIR_ROOT)
			return ERROR_DIRECTORY;
		// - doing nothing if the NewName and NewExt don't differ from the old name and old extension
		TCHAR tmpName[MAX_PATH],tmpExt[MAX_PATH];
		if (__getLongFileNameAndExt__(de,tmpName,tmpExt))
			if (!::lstrcmp(newName,tmpName) && !::lstrcmp(newExt,tmpExt)){
				rRenamedFile=de;
				return ERROR_SUCCESS;
			}
		// - making sure that the NewNameAndExtension is without forbidden characters
		//TODO
		// - making sure that the NewNameAndExtension is not empty
		//TODO
		// - making sure that the NewNameAndExtension isn't used by another File in CurrentDirectory
		for( TMsdos7DirectoryTraversal dt(this,currentDir); dt.__existsNextEntry__(); )
			if (dt.entry!=de)
				if (dt.entryType==TDirectoryTraversal::FILE || dt.entryType==TDirectoryTraversal::SUBDIR){
					__getShortFileNameAndExt__( (PCDirectoryEntry)dt.entry, tmpName, tmpExt );
					if (!::lstrcmpi(newName,tmpName)&&!::lstrcmpi(newExt,tmpExt)){
						rRenamedFile=(PDirectoryEntry)dt.entry;
						return ERROR_FILE_EXISTS;
					}
					if (__getLongFileNameAndExt__( (PCDirectoryEntry)dt.entry, tmpName, tmpExt )) // long name and extension exist
						if (!::lstrcmpi(newName,tmpName)&&!::lstrcmpi(newExt,tmpExt)){
							rRenamedFile=(PDirectoryEntry)dt.entry;
							return ERROR_FILE_EXISTS;
						}
				}
		// - renaming
		UDirectoryEntry tmpEntry=*de;
		if (__changeShortFileNameAndExt__(&tmpEntry,newName,newExt,rRenamedFile)==ERROR_SUCCESS)
			// NewNameAndExtension is in the "8.3" format
			*( rRenamedFile=de )=tmpEntry;
		else{
			// NewNameAndExtension doesn't follow the "8.3" format
			TCHAR longNameAndExt[MAX_PATH];
			::lstrcpy(longNameAndExt,newName);
			if (*newExt)
				::lstrcat( ::lstrcat( longNameAndExt, _T(".") ), newExt );
			// . allocating necessary number of DirectoryEntries to accommodate the long NameAndExtension
			PDirectoryEntry longNameEntries[LONG_FILE_NAME_ENTRIES_COUNT_MAX], *plnde=longNameEntries;
			TMsdos7DirectoryTraversal dt(this,currentDir);
			for( BYTE n=(::lstrlen(longNameAndExt)+12)/13; n--; ) // 13 = number of characters in one LongNameEntry
				if (!( *plnde++=dt.__allocateNewEntry__() ))
					return ERROR_CANNOT_MAKE;
			if (!( rRenamedFile=dt.__allocateNewEntry__() ))
				return ERROR_CANNOT_MAKE;
			// - initializing allocated LongNameEntries
			WCHAR bufW[256], *pw=(PWCHAR)::memset(bufW,-1,sizeof(bufW));
			#ifdef UNICODE
				ASSERT(FALSE)
			#else
				::MultiByteToWideChar( CP_ACP,0, longNameAndExt,-1, bufW,sizeof(bufW)/sizeof(WCHAR) );
			#endif
			for( BYTE n=1,const checksum=(*rRenamedFile=*de).shortNameEntry.__getChecksum__(); plnde!=longNameEntries; ){
				const PDirectoryEntry p=*--plnde;
				p->longNameEntry.sequenceNumber=n++;
				::memcpy( p->longNameEntry.name1, pw, 10 ),	pw+=5;
				p->longNameEntry.attributes=FILE_ATTRIBUTE_LONGNAME;
				p->longNameEntry.zero1=0, p->longNameEntry.zero2=0;
				p->longNameEntry.checksum=checksum;
				::memcpy( p->longNameEntry.name2, pw, 12 ),	pw+=6;
				::memcpy( p->longNameEntry.name3, pw, 4 ),	pw+=2;
				MarkDirectorySectorAsDirty(p);
			}
			(*plnde)->longNameEntry.sequenceNumber|=UDirectoryEntry::LONG_NAME_END;
			MarkDirectorySectorAsDirty(rRenamedFile);
			// - removing original short and long NameAndExtension
			for( BYTE n=__getLongFileNameEntries__(de,longNameEntries); n--; ){
				const PDirectoryEntry p=*plnde++;
				*(PBYTE)p=UDirectoryEntry::EMPTY_ENTRY;
				MarkDirectorySectorAsDirty(p);
			}
			*(PBYTE)de=UDirectoryEntry::EMPTY_ENTRY;
		}
		MarkDirectorySectorAsDirty(de);
		return ERROR_SUCCESS;
	}

	TStdWinError CMSDOS7::ChangeFileNameAndExt(PFile file,RCPathString newName,RCPathString newExt,PFile &rRenamedFile){
		// tries to change given File's name and extension; returns Windows standard i/o error
		return	dontShowLongFileNames
				? __changeShortFileNameAndExt__( (PDirectoryEntry)file, newName, newExt, (PDirectoryEntry &)rRenamedFile )
				: __changeLongFileNameAndExt__( (PDirectoryEntry)file, newName, newExt, (PDirectoryEntry &)rRenamedFile );
	}

	CMSDOS7::TCluster32 CMSDOS7::__allocateAndResetDirectoryCluster__() const{
		// allocates, initializes and returns a new Directory Cluster; if new Cluster cannot be allocated (e.g. because disk full), returns MSDOS7_FAT_CLUSTER_EOF
		const TCluster32 result=__getFirstFreeHealthyCluster__();
		if (result!=MSDOS7_FAT_CLUSTER_EOF){
			fat.SetClusterValue( result, MSDOS7_FAT_CLUSTER_EOF ); // guaranteed that FAT Sector exists
			BYTE n;
			for( TLogSector32 ls=__cluster2logSector__(result,n); n--; ls++ ){
				::ZeroMemory( __getHealthyLogicalSectorData__(ls), formatBoot.sectorLength ); // guaranteed that LogicalSector's data exist
				__markLogicalSectorAsDirty__(ls);
			}
		}
		return result;
	}

	TStdWinError CMSDOS7::CreateSubdirectory(LPCTSTR name,DWORD winAttr,PDirectoryEntry &rCreatedSubdir){
		// creates a new Subdirectory in CurrentDirectory; returns Windows standard i/o error
		// - allocating new Directory Cluster
		const TCluster32 cluster=__allocateAndResetDirectoryCluster__();
		if (cluster==MSDOS7_FAT_CLUSTER_EOF)
			return ERROR_DISK_FULL;
		// - allocating necessary DirectoryEntries in CurrentDirectory (to accommodate short and eventual long name)
		UDirectoryEntry tmp;
			::ZeroMemory(&tmp,sizeof(tmp));
			// . generating short name
			TCHAR tmpName[MAX_PATH];
			PTCHAR pExt=_tcsrchr( ::lstrcpy(tmpName,name), '.' );
			if (pExt)
				*pExt++='\0';
			else
				pExt=_T("");
			__generateShortFileNameAndExt__(&tmp,tmpName,pExt);
			// . size
			//nop (always 0)
			// . dates and times
			//nop (set separately by the caller via SetFileTimeStampts)
			// . first Cluster
			tmp.shortNameEntry.__setFirstCluster__(cluster);
			// . Attributes
			tmp.shortNameEntry.attributes=winAttr;
			// . setting long name (if any)
			if (const TStdWinError err=ChangeFileNameAndExt( &tmp, tmpName, pExt, (PFile &)rCreatedSubdir )){
				fat.FreeChainOfClusters(cluster);
				return err;
			}
		if (rCreatedSubdir==&tmp) // new Subdirectory's name follows the "8.3" convention
			if ( rCreatedSubdir=TMsdos7DirectoryTraversal(this,currentDir).__allocateNewEntry__() ){
				*rCreatedSubdir=tmp;
				MarkDirectorySectorAsDirty(rCreatedSubdir);
			}else{
				fat.FreeChainOfClusters(cluster);
				return ERROR_CANNOT_MAKE;
			}
		// - creating the "dot" and "dotdot" entries in newly created Subdirectory
		TMsdos7DirectoryTraversal dt(this,rCreatedSubdir);
		const PDirectoryEntry dot=dt.__allocateNewEntry__();
			*dot=*rCreatedSubdir;
			*(PCHAR)::memset( dot, ' ', MSDOS7_FILE_NAME_LENGTH_MAX+MSDOS7_FILE_EXT_LENGTH_MAX )='.';
		const PDirectoryEntry dotdot=dt.__allocateNewEntry__();
			( *dotdot=*dot ).shortNameEntry.name[1]='.';
			dotdot->shortNameEntry.__setFirstCluster__( currentDir!=MSDOS7_DIR_ROOT ? ((PCDirectoryEntry)currentDir)->shortNameEntry.__getFirstCluster__() : 0 );
		return ERROR_SUCCESS;
	}

	TStdWinError CMSDOS7::SwitchToDirectory(PDirectoryEntry directory){
		// changes CurrentDirectory; returns Windows standard i/o error
		if (directory)
			currentDir=directory->shortNameEntry.__getFirstCluster__() ? directory : MSDOS7_DIR_ROOT;
		else
			currentDir=MSDOS7_DIR_ROOT;
		return ERROR_SUCCESS;
	}

	DWORD CMSDOS7::GetDirectoryUid(PCFile dir) const{
		// determines and returns the unique identifier of the Directory specified
		return	dir!=MSDOS7_DIR_ROOT
				? ((PCDirectoryEntry)dir)->shortNameEntry.__getFirstCluster__()
				: 0;
	}
	
	TStdWinError CMSDOS7::MoveFileToCurrDir(PDirectoryEntry de,LPCTSTR exportFileNameAndExt,PFile &rMovedFile){
		// moves given File to CurrentDirectory; returns Windows standard i/o error
		// - "registering" the File's Name+Extension in CurrentDirectory
		TCHAR bufName[MAX_PATH], *pExt;
		if (const PTCHAR pDot=_tcsrchr(::lstrcpy(bufName,exportFileNameAndExt),'.'))
			*pDot='\0', pExt=1+pDot;
		else
			pExt=_T("");
		if (const TStdWinError err=ChangeFileNameAndExt( de, bufName, pExt, rMovedFile )) // also destroys original short name DirectoryEntry (leaving long name Entries orphaned but that's still a legal approach)
			return err;
		// - if moving a File with "8.3" name (e.g. "MYFILE.TXT"), moving the single DirectoryEntry "manually"
		if (rMovedFile==de) // a "8.3" named File - the above "registration" didn't function for it
			if (const PDirectoryEntry newDe=TMsdos7DirectoryTraversal(this,currentDir).__allocateNewEntry__()){
				*newDe=*de;
				*(PBYTE)de=UDirectoryEntry::EMPTY_ENTRY;
			}else
				return ERROR_CANNOT_MAKE;
		// - if a Directory is being moved, changing the reference to the parent in the "dotdot" DirectoryEntry
		if (IsDirectory(rMovedFile))
			if (const PDirectoryEntry dotdot=(PDirectoryEntry)__findFile__(rMovedFile,_T(".."),_T(""),nullptr))
				dotdot->shortNameEntry.__setFirstCluster__(	currentDir!=MSDOS7_DIR_ROOT
															? ((PCDirectoryEntry)currentDir)->shortNameEntry.__getFirstCluster__()
															: 0
														);
		return ERROR_SUCCESS;
	}

	DWORD CMSDOS7::GetFileSize(PCFile file,PBYTE pnBytesReservedBeforeData,PBYTE pnBytesReservedAfterData,TGetFileSizeOptions option) const{
		// determines and returns the size of specified File
		if (pnBytesReservedBeforeData) *pnBytesReservedBeforeData=0;
		if (pnBytesReservedAfterData) *pnBytesReservedAfterData=0;
		const PDirectoryEntry de=(PDirectoryEntry)file;
		switch (option){
			case TGetFileSizeOptions::OfficialDataLength:
				return IsDirectory(de) ? 0 : de->shortNameEntry.size;
			case TGetFileSizeOptions::SizeOnDisk:
				if (IsDirectory(de)){
					// Directory - finding out how much space it occupies on the disk (NOT including its content!)
					DWORD sizeOnDisk=0;
					for( TMsdos7DirectoryTraversal dt(this,de); dt.__existsNextEntry__(); sizeOnDisk+=sizeof(UDirectoryEntry) );
					return sizeOnDisk;
				}else{
					// File
					const DWORD clusterSizeInBytes=boot.GetSectorData()->__getClusterSizeInBytes__();
					return (de->shortNameEntry.size+clusterSizeInBytes-1)/clusterSizeInBytes * clusterSizeInBytes;
				}
			default:
				ASSERT(FALSE);
				return 0;
		}
	}

	void CMSDOS7::GetFileTimeStamps(PCFile file,LPFILETIME pCreated,LPFILETIME pLastRead,LPFILETIME pLastWritten) const{
		// given specific File, populates the Created, LastRead, and LastWritten outputs
		const PCDirectoryEntry de=(PCDirectoryEntry)file;
		if (pCreated)
			*pCreated= de!=MSDOS7_DIR_ROOT ? TDateTime(de->shortNameEntry.timeAndDateCreated) : TFileDateTime::None ;
		if (pLastRead)
			*pLastRead= de!=MSDOS7_DIR_ROOT ? TDateTime(de->shortNameEntry.dateLastAccessed) : TFileDateTime::None ;
		if (pLastWritten)
			*pLastWritten= de!=MSDOS7_DIR_ROOT ? TDateTime(de->shortNameEntry.timeAndDateLastModified) : TFileDateTime::None ;
	}

	void CMSDOS7::SetFileTimeStamps(PFile file,const FILETIME *pCreated,const FILETIME *pLastRead,const FILETIME *pLastWritten){
		// translates the Created, LastRead, and LastWritten intputs into this DOS File time stamps
		if (file==MSDOS7_DIR_ROOT)
			return;
		const PDirectoryEntry de=(PDirectoryEntry)file;
		if (pCreated)
			TDateTime(*pCreated).ToDWord( &de->shortNameEntry.timeAndDateCreated );
		if (pLastRead){
			DWORD tmp;
			TDateTime(*pLastRead).ToDWord( &tmp );
			de->shortNameEntry.dateLastAccessed=HIWORD(tmp);
		}
		if (pLastWritten)
			TDateTime(*pLastWritten).ToDWord( &de->shortNameEntry.timeAndDateLastModified );
		if (pCreated || pLastRead || pLastWritten)
			MarkDirectorySectorAsDirty(de);
	}

	DWORD CMSDOS7::GetAttributes(PCFile file) const{
		// maps File's attributes to Windows attributes and returns the result
		return	file!=MSDOS7_DIR_ROOT
				? ((PCDirectoryEntry)file)->shortNameEntry.attributes
				: FILE_ATTRIBUTE_DIRECTORY;
	}


	TStdWinError CMSDOS7::DeleteFile(PFile file){
		// deletes specified File; returns Windows standard i/o error
		if (file==MSDOS7_DIR_ROOT)
			return ERROR_ACCESS_DENIED; // can't delete the root Directory
		if (*(PCHAR)file!=UDirectoryEntry::EMPTY_ENTRY){ // File mustn't be already deleted (may happen during moving it in FileManager)
			const CFatPath fatPath(this,file);
			CFatPath::PCItem item; DWORD n;
			if (const LPCTSTR err=fatPath.GetItems(item,n)){
				ShowFileProcessingError(file,err);
				return ERROR_GEN_FAILURE;
			}else{
				const PDirectoryEntry de=(PDirectoryEntry)file; // retyping for easier use below
				// . deleting the content
				if (!de->shortNameEntry.__isDotOrDotdot__()){ // "dot" and "dotdot" Entries skipped
					// . recurrently deleting the content of a Directory
					if (IsDirectory(de))
						for( TMsdos7DirectoryTraversal dt(this,de); dt.__existsNextEntry__(); )
							switch (dt.entryType){
								case TDirectoryTraversal::SUBDIR:
									// Directory
									if (((PCDirectoryEntry)dt.entry)->shortNameEntry.__isDotOrDotdot__())
										continue; // "dot" and "dotdot" Entries skipped
									//fallthrough
								case TDirectoryTraversal::FILE:
									// File
									if (const TStdWinError err=DeleteFile(dt.entry)){
										ShowFileProcessingError(dt.entry,err);
										return err; // will remain switched to Subdirectory that contains the undeletable File
									}
									break;
							}
					// : modifying the information on free space
					const TSector nSectorsInCluster=boot.GetSectorData()->nSectorsInCluster; // Boot Sector guarateed to exist in this context
					if (const PFsInfoSector fsInfoSector=fsInfo.GetSectorData()){
						// for FAT32, using the FS-Info Sector
						fsInfoSector->nFreeClusters // guaranteed that NumberOfFreeClusters initialized (as caller had to call GetFreeSpaceInBytes, where it eventually has been initialized)
							+=(n+nSectorsInCluster-1)/nSectorsInCluster; // count of Directory Sectors rounded up to whole Clusters
						fsInfo.MarkSectorAsDirty();
					}else
						// for FAT32 without FS Info Sector or for FAT16/FAT12, using the temporary information on free space
						fat.nFreeClustersTemp
							+=(n+nSectorsInCluster-1)/nSectorsInCluster; // count of Directory Sectors rounded up to whole Clusters
					// : marking occupied Clusters as Empty in FAT
					if (n)
						fat.FreeChainOfClusters(item->value);
				}
				// . deleting long name from CurrentDirectory
				__deleteLongFileNameEntries__(de);
				// . deleting short name from CurrentDirectory
				*(PBYTE)de=UDirectoryEntry::EMPTY_ENTRY;
				de->shortNameEntry.__setFirstCluster__(MSDOS7_FAT_CLUSTER_EOF);
				MarkDirectorySectorAsDirty(de);
			}
		}
		return ERROR_SUCCESS;
	}

	PTCHAR CMSDOS7::__getFileExportNameAndExt__(LPCTSTR bufName,LPCTSTR bufExt,bool shellCompliant,PTCHAR pOutBuffer){
		// populates Buffer with specified export Name and Extension and returns the Buffer; returns Null if File cannot be exported (e.g. a "dotdot" entry in MS-DOS); caller guarantees that the Buffer is at least MAX_PATH characters big
		if (*bufExt)
			return ::lstrcat( ::lstrcat( ::lstrcpy(pOutBuffer,bufName), _T(".") ), bufExt );
		else
			return ::lstrcpy(pOutBuffer,bufName);
	}

	CString CMSDOS7::GetFileExportNameAndExt(PCFile file,bool shellCompliant) const{
		// returns File name concatenated with File extension for export of the File to another Windows application (e.g. Explorer)
		if (!((PCDirectoryEntry)file)->shortNameEntry.__isDotOrDotdot__()){
			CPathString bufName, bufExt;
			GetFileNameOrExt( file, &bufName, &bufExt );
			TCHAR buf[16384];
			return __getFileExportNameAndExt__( bufName, bufExt, shellCompliant, buf );
		}else
			return _T("");
	}

	TStdWinError CMSDOS7::ImportFile(CFile *f,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile){
		// imports specified File (physical or virtual) into the Image; returns Windows standard i/o error
		// - marking the Volume as "dirty"
		//TODO (below just draft)
/*		switch (msdos->fat){
			case FAT16:
				msdos->SetClusterValue( 1, msdos->GetClusterValue(1)&~FAT16_CLEAN_MASK );
				break;
			case FAT32:
				msdos->SetClusterValue( 1, msdos->GetClusterValue(1)&~FAT32_CLEAN_MASK );
				break;
		}*/
		// - initializing the description of File to import
		UDirectoryEntry tmp;
			::ZeroMemory(&tmp,sizeof(tmp));
			// . generating File's short name
			TCHAR tmpName[MAX_PATH];
			PTCHAR pExt=_tcsrchr( ::lstrcpy(tmpName,nameAndExtension), '.' );
			if (pExt)
				*pExt++='\0';
			else
				pExt=_T("");
			__generateShortFileNameAndExt__(&tmp,tmpName,pExt);
			// . size
			tmp.shortNameEntry.size=fileSize;
			// . dates and times
			//nop (set separately by the caller via SetFileTimeStampts
			// . first Cluster
			//nop (set up below, now 0 = zero-length File)
			// . Attributes
			tmp.shortNameEntry.attributes=winAttr;
			// . registering File's long name in CurrentDirectory
			//nop (ChangeFileNameAndExt called below by ImportFile)
		// - importing to Image
		if (dontShowLongFileNames){ // importing with long file names turned off - using the generated short File name
			pExt=tmpName+20;
			__getShortFileNameAndExt__(&tmp,tmpName,pExt);
		}
		CFatPath fatPath(this,fileSize);
		if (const TStdWinError err=__importFileData__( f, &tmp, tmpName, pExt, fileSize, true, rFile, fatPath ))
			return err;
		CFatPath::PCItem item; DWORD n;
		fatPath.GetItems(item,n);
		TCluster32 cluster0=fileSize
							? __logSector2cluster__( __fyzlog__(item->chs) ) // NON-zero-length File
							: 0; // zero-length File
		// - finishing initialization of DirectoryEntry of successfully imported File
		const PDirectoryEntry de=(PDirectoryEntry)rFile;
			// . first Cluster
			de->shortNameEntry.__setFirstCluster__(cluster0);
		// - modifying the information on free space
		const TSector nSectorsInCluster=boot.GetSectorData()->nSectorsInCluster; // Boot Sector guarateed to exist in this context
		if (const PFsInfoSector fsInfoSector=fsInfo.GetSectorData()){
			// for FAT32, using the FS-Info Sector
			fsInfoSector->nFreeClusters // guaranteed that NumberOfFreeClusters initialized (as caller had to call GetFreeSpaceInBytes, where it eventually has been initialized)
				-=(n+nSectorsInCluster-1)/nSectorsInCluster; // count of Directory Sectors rounded up to whole Clusters
			fsInfo.MarkSectorAsDirty();
		}else
			// for FAT32 without FS-Info Sector or for FAT16/FAT12, using the temporary information on free space
			fat.nFreeClustersTemp
				-=(n+nSectorsInCluster-1)/nSectorsInCluster; // count of Directory Sectors rounded up to whole Clusters
		// - marking newly occupied Clusters in the FAT
		ModifyFileFatPath( de, fatPath );
		// - File successfully imported to Image
		return ERROR_SUCCESS;
	}









	TStdWinError CMSDOS7::CreateUserInterface(HWND hTdi){
		// creates DOS-specific Tabs in TDI; returns Windows standard i/o error
		CDos::CreateUserInterface(hTdi); // guaranteed to always return ERROR_SUCCESS
		if (const PCBootSector bootSector=boot.GetSectorData())
			if (bootSector->__isUsable__()){ // Boot Sector contains values that make sense
				if (fat.type==CFat::FAT32)
					if (!fsInfo.GetSectorData()){ // FS Info Sector not found
						Utils::Information(_T("FAT32 volume detected but its FS-Info sector not found.\n\nCannot continue at the moment - please retry in future versions of this application."));
						return ERROR_REQUEST_REFUSED;
						/*
						if (!Utils::QuestionYesNo(_T("FAT32 volume detected but its FS-Info sector not found. Working with such volume can be VERY slow.\n\nContinue anyway?!"),MB_DEFBUTTON2))
							return ERROR_CANCELLED;
						//else
							//TODO: running ScanDisk to restore FS-Info Sector
						*/
					}
				CTdiCtrl::AddTabLast( hTdi, TRACK_MAP_TAB_LABEL, &trackMap.tab, false, TDI_TAB_CANCLOSE_NEVER, nullptr );
				CTdiCtrl::AddTabLast( hTdi, BOOT_SECTOR_TAB_LABEL, &boot.tab, false, TDI_TAB_CANCLOSE_NEVER, nullptr );
				if (fat.type==CFat::FAT32)
					CTdiCtrl::AddTabLast( hTdi, _T("FS-Info sector"), &fsInfo.tab, false, TDI_TAB_CANCLOSE_NEVER, nullptr );
				CTdiCtrl::AddTabLast( hTdi, FILE_MANAGER_TAB_LABEL, &fileManager.tab, true, TDI_TAB_CANCLOSE_NEVER, nullptr );
				return ERROR_SUCCESS;
			}
		return ERROR_REQUEST_REFUSED;
	}





	#pragma pack(1)
	struct TRemoveLongNameParams sealed{
		CMSDOS7 *const msdos;
		const bool recurrently;

		TRemoveLongNameParams(CMSDOS7 *msdos,bool recurrently)
			: msdos(msdos) , recurrently(recurrently) {
		}
	};
	UINT AFX_CDECL CMSDOS7::__removeLongNames_thread__(PVOID _pCancelableAction){
		// thread to remove long File names
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)_pCancelableAction;
		const TRemoveLongNameParams rlnp=*(TRemoveLongNameParams *)pAction->GetParams();
		pAction->SetProgressTarget( rlnp.msdos->formatBoot.nCylinders );
			CFileManagerView::TFileList bfsDirectories; // breadth first search, searching through Directories in breadth
			TCylinder state=0;
			for( bfsDirectories.AddTail(rlnp.msdos->currentDir); bfsDirectories.GetCount(); ){
				if (pAction->IsCancelled()) return ERROR_CANCELLED;
				TMsdos7DirectoryTraversal dt( rlnp.msdos, bfsDirectories.RemoveHead() );
				while (dt.__existsNextEntry__()){
					const PDirectoryEntry de=(PDirectoryEntry)dt.entry;
					switch (dt.entryType){
						case TDirectoryTraversal::SUBDIR:
							if (rlnp.recurrently && !de->shortNameEntry.__isDotOrDotdot__())
								bfsDirectories.AddTail(de);
							break;
						case TDirectoryTraversal::WARNING:
							return pAction->TerminateWithError(dt.warning);
					}
					if ((de->longNameEntry.attributes&FILE_ATTRIBUTE_LONGNAME_MASK)==FILE_ATTRIBUTE_LONGNAME){
						*(PBYTE)de=UDirectoryEntry::EMPTY_ENTRY;
						rlnp.msdos->MarkDirectorySectorAsDirty(de);
					}
				}
				pAction->UpdateProgress( state=std::max<>(state,dt.chs.cylinder) );
			}
		return pAction->TerminateWithError(ERROR_SUCCESS);
	}

	CDos::TCmdResult CMSDOS7::ProcessCommand(WORD cmd){
		// returns the Result of processing a DOS-specific command
		switch (cmd){
			case ID_DOS_FILL_EMPTY_SPACE:{
				// filling out empty space on disk
				// . temporarily disabling processing of dot and dotdot Entries
				const bool dot=dontShowDotEntries, dotdot=dontShowDotdotEntries;
				dontShowDotEntries = dontShowDotdotEntries = true;
				// . filling out empty space on disk
				__fillEmptySpace__( CFillEmptySpaceDialog(this) ); // WARNING: It's assumed that "dot" and "dotdot"-like DirectoryEntries are disabled to prevent from unfinite looping when selected to fill Empty DirectoryEntries!
				// . restoring processing of dot and dotdot Entries
				dontShowDotEntries=dot, dontShowDotdotEntries=dotdot;
				return TCmdResult::DONE_REDRAW;
			}
			case ID_MSDOS_REMOVE_LONG_NAMES:{
				// removing long File names
				// . cannot proceed if Image is WriteProtected
				if (image->__reportWriteProtection__())
					return TCmdResult::REFUSED;
				// . defining the Dialog
				class CLongFileNamesRemovalDialog sealed:public CDialog{
					void DoDataExchange(CDataExchange *pDX) override{
						if (pDX->m_bSaveAndValidate)
							DDX_Radio( pDX, ID_DIRECTORY, dirDepth );
						else
							CheckDlgButton( ID_DIRECTORY, BST_CHECKED );
					}
				public:
					int dirDepth;
					CLongFileNamesRemovalDialog()
						: CDialog(IDR_MSDOS_REMOVE_LONG_NAMES)
						, dirDepth(0) {
					}
				} d;
				// . showing the Dialog and processing its result
				if (d.DoModal()==IDOK)
					CBackgroundActionCancelable(
						__removeLongNames_thread__,
						&TRemoveLongNameParams( this, d.dirDepth!=0 ),
						THREAD_PRIORITY_BELOW_NORMAL
					).Perform();
				return TCmdResult::DONE_REDRAW;
			}
			case ID_MSDOS_IGNORE_LONG_NAMES:
				// showing/hiding long File names
				__writeProfileBool__( INI_DONT_SHOW_LONG_NAMES, dontShowLongFileNames=!dontShowLongFileNames );
				return TCmdResult::DONE_REDRAW;
			case ID_MSDOS_IGNOREDOT:
				// showing/hiding "dot" DirectoryEntries
				__writeProfileBool__( INI_DONT_SHOW_DOT, dontShowDotEntries=!dontShowDotEntries );
				return TCmdResult::DONE_REDRAW;
			case ID_MSDOS_IGNOREDOTDOT:
				// showing/hiding "dotdot" DirectoryEntries
				__writeProfileBool__( INI_DONT_SHOW_DOTDOT, dontShowDotdotEntries=!dontShowDotdotEntries );
				return TCmdResult::DONE_REDRAW;			
			case ID_DOS_FORMAT:
				// formatting Cylinders
				if (const TStdWinError err=__showDialogAndFormatStdCylinders__( CFormatDialog(this,nullptr,0) )){
					Utils::Information( DOS_ERR_CANNOT_FORMAT, err );
					return TCmdResult::REFUSED;
				}else
					return TCmdResult::DONE_REDRAW;
			case ID_DOS_UNFORMAT:{
				// unformatting Cylinders
				const TCylinder cylMin=1+__getLastOccupiedStdCylinder__(), cylMax=image->GetCylinderCount()-1;
				const CUnformatDialog::TStdUnformat stdUnformats[]={
					{ STR_TRIM_TO_MIN_NUMBER_OF_CYLINDERS,	cylMin, cylMax }
				};
				if (const TStdWinError err=__showDialogAndUnformatStdCylinders__( CUnformatDialog(this,stdUnformats,1) )){
					Utils::Information( DOS_ERR_CANNOT_UNFORMAT, err );
					return TCmdResult::REFUSED;
				}else
					return TCmdResult::DONE_REDRAW;
			}
			case ID_DOS_TAKEATOUR:
				// navigating to the online tour on this DOS
				((CMainWindow *)app.m_pMainWnd)->OpenApplicationPresentationWebPage(_T("Tour"),_T("MSDOS71/tour.html"));
				return TCmdResult::DONE;
		}
		return CDos::ProcessCommand(cmd);
	}

	bool CMSDOS7::UpdateCommandUi(WORD cmd,CCmdUI *pCmdUI) const{
		// True <=> given Command-specific user interface successfully updated, otherwise False
		switch (cmd){
			case ID_MSDOS_IGNORE_LONG_NAMES:
				pCmdUI->SetCheck( dontShowLongFileNames );
				return TRUE;
			case ID_MSDOS_IGNOREDOT:
				pCmdUI->SetCheck( dontShowDotEntries );
				return TRUE;
			case ID_MSDOS_IGNOREDOTDOT:
				pCmdUI->SetCheck( dontShowDotdotEntries );
				return TRUE;
		}
		return CDos::UpdateCommandUi(cmd,pCmdUI);
	}

	void CMSDOS7::InitializeEmptyMedium(CFormatDialog::PCParameters params){
		// initializes a fresh formatted Medium (Boot, FAT, root dir, etc.)
		// - initializing Boot Sector
		TPhysicalAddress chs={ 0, 0, {0,0,1,params->format.sectorLengthCode} };
		boot.ChangeToSector(chs);
		const PBootSector bootSector=boot.GetSectorData();
		if (!bootSector) // Boot Sector may not be found after unsuccessfull formatting
error:		return Utils::FatalError( _T("Cannot initialize the medium"), ::GetLastError() );
		bootSector->__init__( &formatBoot, params, fat ); // also initializes the Type of FAT
		boot.MarkSectorAsDirty();
		if (fat.type==CFat::FAT32){
			chs.sectorId.sector=MSDOS7_SECTOR_BKBOOT;
			if (const PBootSector bkBoot=(PBootSector)image->GetHealthySectorData(chs)){ // backup Boot Sector may not be found after unsuccessfull formatting
				*bkBoot=*bootSector;
				image->MarkSectorAsDirty(chs);
			}
		}
		// - initializing the FS Info Sector
		if (fat.type==CFat::FAT32)
			if (const PFsInfoSector fsInfoSector=(PFsInfoSector)__getHealthyLogicalSectorData__(bootSector->fat32.fsInfo)){ // FS Info Sector may not be found after unsuccessfull formatting
				fsInfoSector->__init__();
				fsInfo.MarkSectorAsDirty();
			}else
				goto error;
		// - initializing FAT
		fat.SetClusterValue( 0, 0x0fffff00|bootSector->medium ); // for backward compatibility with MS-DOS 1.0
		fat.SetClusterValue( 1, MSDOS7_FAT_CLUSTER_EOF );
		for( TCluster32 n=0,const nClusters=__getCountOfClusters__(); n<nClusters; ){
			const TCluster32 c=MSDOS7_DATA_CLUSTER_FIRST + n++;
			DWORD clusterState=MSDOS7_FAT_CLUSTER_EMPTY; // assumption (Cluster ok)
			BYTE b;
			for( TLogSector32 ls=__cluster2logSector__(c,b); b--; )
				if (!__getHealthyLogicalSectorData__(ls++)){
					clusterState=MSDOS7_FAT_CLUSTER_BAD;
					break;
				}
			fat.SetClusterValue( c, clusterState );
		}
		// - initializing root Directory
		SwitchToDirectory(MSDOS7_DIR_ROOT);
		switch (fat.type){
			case CFat::FAT12:
			case CFat::FAT16:
				for( TLogSector32 lsDir=bootSector->__getRootDirectoryFirstSector__(),lsData=lsDir+bootSector->__getCountOfPermanentRootDirectorySectors__(); lsDir<lsData; lsDir++ )
					if (const PSectorData tmp=__getHealthyLogicalSectorData__(lsDir)){
						::ZeroMemory( tmp, bootSector->sectorSize );
						__markLogicalSectorAsDirty__(lsDir);
					}
				break;
			case CFat::FAT32:
				if (( bootSector->fat32.rootDirectoryFirstCluster=__allocateAndResetDirectoryCluster__() )==MSDOS7_FAT_CLUSTER_EOF){
					::SetLastError(ERROR_DISK_FULL);
					goto error;
				}
				break;
			default:
				ASSERT(FALSE);
		}
	}












	static const WCHAR ForbiddenChars[]={ 0x22, 0x2a, 0x2b, 0x2c, 0x2e, 0x2f, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x5b, 0x5c, 0x5d, 0x7c, '\0' };

	bool CMSDOS7::UDirectoryEntry::TShortNameEntry::__isCharacterValid__(char c){
		// True <=> specified Character is valid in short name, otherwise False
		if (c<=' ' && c!=KANJI) return false;
		if ('a'<=c && c<='z') return false;
		return !::wcschr(ForbiddenChars,c);
	}

	CMSDOS7::TCluster32 CMSDOS7::UDirectoryEntry::TShortNameEntry::__getFirstCluster__() const{
		// returns the first Cluster of File
		return MAKELONG( firstClusterLow, firstClusterHigh );
	}
	void CMSDOS7::UDirectoryEntry::TShortNameEntry::__setFirstCluster__(TCluster32 c){
		// sets the first Cluster of File
		firstClusterLow=LOWORD(c), firstClusterHigh=HIWORD(c);
	}

	BYTE CMSDOS7::UDirectoryEntry::TShortNameEntry::__getChecksum__() const{
		// computes and returns the Checksum
		BYTE checksum=0;
		for( BYTE n=MSDOS7_FILE_NAME_LENGTH_MAX+MSDOS7_FILE_EXT_LENGTH_MAX,*p=(PBYTE)name; n--; checksum=((checksum&1)?0x80:0)+(checksum>>1)+*p++ );
		return checksum;
	}

	bool CMSDOS7::UDirectoryEntry::TShortNameEntry::__isDotOrDotdot__() const{
		// True <=> this Entry is "dot" or "dotdot", otherwise False
		return *(PDWORD)this==MSDOS7_DIR_DOT || *(PDWORD)this==MSDOS7_DIR_DOTDOT;
	}











	std::unique_ptr<CDos::TDirectoryTraversal> CMSDOS7::BeginDirectoryTraversal(PCFile directory) const{
		// initiates exploration of specified Directory through a DOS-specific DirectoryTraversal
		return std::unique_ptr<TDirectoryTraversal>( new TMsdos7DirectoryTraversal(this,directory) );
	}

	CMSDOS7::TMsdos7DirectoryTraversal::TMsdos7DirectoryTraversal(const CMSDOS7 *_msdos7,PCFile directory)
		// ctor
		// - base
		: TDirectoryTraversal( directory, sizeof(UDirectoryEntry) )
		// - initialization
		, msdos7(_msdos7)
		, foundEndOfDirectory(false) , fatError(false)
		, next( directory!=MSDOS7_DIR_ROOT ? ((PCDirectoryEntry)directory)->shortNameEntry.__getFirstCluster__() : 0 )
		, nRemainingSectorsInCluster(0) , nRemainingEntriesInSector(0) {
		// - "pointer" set to the first DirectoryEntry
		if (!next)
			// root Directory
			if (const PCBootSector bootSector=msdos7->boot.GetSectorData())
				switch (msdos7->fat.type){
					case CFat::FAT12:
					case CFat::FAT16:{
						cluster=0, next=MSDOS7_FAT_CLUSTER_EOF; // convenient setting to traverse root Directory of FAT12/FAT16
						dirSector=bootSector->__getRootDirectoryFirstSector__();
						nRemainingSectorsInCluster=bootSector->__getCountOfPermanentRootDirectorySectors__();
						break;
					}
					case CFat::FAT32:
						next=bootSector->fat32.rootDirectoryFirstCluster;
						break;
				}
	}

	bool CMSDOS7::TMsdos7DirectoryTraversal::__existsNextEntry__(){
		// True <=> another Entry in current Directory exists (Empty or not), otherwise False
		if (const PCBootSector bootSector=msdos7->boot.GetSectorData()){
			// . getting next LogicalSector of CurrentDirectory
			if (!nRemainingEntriesInSector){
				const TDirEntryType entryType0=entryType;
					entryType=TDirectoryTraversal::WARNING, warning=ERROR_SECTOR_NOT_FOUND; // assumption (LogicalSector not found)
					if (!nRemainingSectorsInCluster) // next LogicalSector must be retrieved from next Cluster
						if (MARKS_CLUSTER_EOF(next)){
							foundEndOfDirectory=true;
							return false; // EntryType must be anything but Empty - above set to Warning
						}else if (next==MSDOS7_FAT_ERROR)
							return fatError=true; // EntryType already set to Warning above
						else if (next==MSDOS7_FAT_CLUSTER_EMPTY){
							fatError=true; // if the Cluster is reported as Empty in the FAT, this is probably an error
							foundEndOfDirectory=true; // with Empty Cluster, the traversal through the Directory cannot continue
							return false;
						}else if (!fatError){
							dirSector=msdos7->__cluster2logSector__( cluster=next, (BYTE &)nRemainingSectorsInCluster ); // also sets the NumberOfRemainingSectorsInCluster
							next=msdos7->fat.GetClusterValue(cluster);
						}else
							return false; // EntryType already set to Warning above
					nRemainingSectorsInCluster--;
					chs=msdos7->__logfyz__(dirSector);
					entry=msdos7->__getHealthyLogicalSectorData__(dirSector++);
					if (!entry){ // LogicalSector not found
						warning=ERROR_SECTOR_NOT_FOUND;
						return true; // EntryType already set to Warning above
					}else
						entry=(PDirectoryEntry)entry-1; // pointer "before" the first DirectoryEntry
				entryType=entryType0;
				nRemainingEntriesInSector=bootSector->sectorSize/sizeof(UDirectoryEntry);
			}
			// . getting the next DirectoryEntry
			entry=(PDirectoryEntry)entry+1, nRemainingEntriesInSector--;
			const PCDirectoryEntry de=(PCDirectoryEntry)entry;
			if (!foundEndOfDirectory)
				switch (de->shortNameEntry.name[0]){
					case UDirectoryEntry::DIRECTORY_END:
						foundEndOfDirectory=true;
						//fallthrough
					case UDirectoryEntry::EMPTY_ENTRY:
						entryType=TDirectoryTraversal::EMPTY;
						break;
					default:{
						if (de->shortNameEntry.attributes&FILE_ATTRIBUTE_VOLUME)
							entryType=TDirectoryTraversal::CUSTOM;
						else if (de->shortNameEntry.attributes&FILE_ATTRIBUTE_DIRECTORY)
							switch (*(PDWORD)de){
								case MSDOS7_DIR_DOT:
									entryType=	msdos7->dontShowDotEntries
												? TDirectoryTraversal::CUSTOM
												: TDirectoryTraversal::SUBDIR;
									break;
								case MSDOS7_DIR_DOTDOT:
									entryType=	msdos7->dontShowDotdotEntries
												? TDirectoryTraversal::CUSTOM
												: TDirectoryTraversal::SUBDIR;
									break;
								default:
									entryType=TDirectoryTraversal::SUBDIR;
									break;
							}
						else
							entryType=TDirectoryTraversal::FILE;
						break;
					}
				}
			return true;
		}else
			return false;
	}

	bool CMSDOS7::TMsdos7DirectoryTraversal::AdvanceToNextEntry(){
		// True <=> found another entry in current Directory (Empty or not), otherwise False
		return __existsNextEntry__();
	}

	CMSDOS7::PDirectoryEntry CMSDOS7::TMsdos7DirectoryTraversal::__allocateNewEntry__(){
		// allocates new DirectoryEntry at the end of CurrentDirectory and returns the DirectoryEntry; returns Null if new DirectoryEntry cannot be allocated (e.g. because disk is full)
		do{
			// . moving pointer to the end of CurrentDirectory
			while (__existsNextEntry__() && !foundEndOfDirectory); // cycle as long as there exists another Entry and the Entry isn't the end of CurrentDirectory
			if (!foundEndOfDirectory) // if end of CurrentDirectory not found (e.g. because Sector unreadable or FAT error) ...
				return nullptr; // ... we are unsuccessfully done
			// . if an Empty DirectoryEntry found, we are successfully done
			if (entryType==TDirectoryTraversal::EMPTY)
				break;
			// . at this point, we have found end of CurrentDirectory but without an Empty DirectoryEntry - allocating new Cluster and returning the first DirectoryEntry in it
			switch (msdos7->fat.type){
				case CFat::FAT12:
				case CFat::FAT16:
					if (directory==MSDOS7_DIR_ROOT) // for root Directory of FAT12/FAT16 ...
						return nullptr; // ... cannot be allocated a new Cluster
					//fallthrough
				case CFat::FAT32:
					if (( next=msdos7->__allocateAndResetDirectoryCluster__() )==MSDOS7_FAT_CLUSTER_EOF)
						return nullptr; // failed to allocate a new Directory Cluster
					msdos7->fat.SetClusterValue(cluster,next);
					foundEndOfDirectory=false; // toggling the flag back - thanks to the newly allocated Cluster, the CurrentDirectory doesn't yet end
					break;
			}
		}while (true);
		return (PDirectoryEntry)entry;
	}

	CDos::PFile CMSDOS7::TMsdos7DirectoryTraversal::AllocateNewEntry(){
		// allocates and returns new entry at the end of current Directory and returns; returns Null if new entry cannot be allocated (e.g. because disk is full)
		return __allocateNewEntry__();
	}

	void CMSDOS7::TMsdos7DirectoryTraversal::ResetCurrentEntry(BYTE directoryFillerByte) const{
		// gets current entry to the state in which it would be just after formatting
		::memset( entry, directoryFillerByte, entrySize );
		*(PBYTE)entry=	foundEndOfDirectory
						? UDirectoryEntry::DIRECTORY_END
						: UDirectoryEntry::EMPTY_ENTRY;
	}
