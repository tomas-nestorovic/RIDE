#include "stdafx.h"
#include "BSDOS.h"

	CBSDOS308::TFatValue::TFatValue(){
		// ctor
	}

	CBSDOS308::TFatValue::TFatValue(bool occupied,bool continuous,WORD info)
		// ctor
		: info(info) , continuous(continuous) , occupied(occupied) {
	}

	CBSDOS308::TFatValue::TFatValue(WORD w){
		// ctor
		*(PWORD)this=w;
		//info=w, continuous=(w&0x4000)!=0, occupied=(short)w<0;
	}

	CBSDOS308::TFatValue::operator WORD() const{
		return *(PCWORD)this;
	}











	CBSDOS308::TFatCopyRetrievalEntry::TFatCopyRetrievalEntry(const CBSDOS308 *bsdos,PBootSector boot,BYTE onlyFatCopy)
		// ctor
		: TDirectoryEntry(	bsdos,
							(	disabledFat=(onlyFatCopy+1)%BSDOS_FAT_COPIES_MAX,
								disabledFatStart=boot->fatStarts[disabledFat],
								boot->fatStarts[disabledFat]=0,
								bsdos->__getHealthyLogicalSectorData__(boot->fatStarts[onlyFatCopy]) // if selected FAT copy's first Sector well readable ...
									? 0 // ... doing nothing
									: (boot->fatStarts[disabledFat]=disabledFatStart), // ... otherwise turning the other FAT copy on to retrieve the selected FAT copy Sectors
								boot->fatStarts[onlyFatCopy]
							)
						)
		, bootSector(boot) {
	}

	CBSDOS308::TFatCopyRetrievalEntry::~TFatCopyRetrievalEntry(){
		// dtor
		bootSector->fatStarts[disabledFat]=disabledFatStart;
	}










	bool CBSDOS308::__isValidFatSectorNumber__(TLogSector lsFat) const{
		return	BSDOS_FAT_LOGSECTOR_MIN<=lsFat && lsFat<std::min<DWORD>(BSDOS_FAT_LOGSECTOR_MAX,formatBoot.GetCountOfAllSectors());
	}

	CBSDOS308::TFatValue CBSDOS308::__getLogicalSectorFatItem__(TLogSector logSector) const{
		// returns the value in FAT of the specified LogicalSector; returns BSDOS_FAT_ERROR if FAT Sector read error
		if (const PCBootSector bootSector=boot.GetSectorData())
			for( BYTE fatCopy=0; fatCopy<BSDOS_FAT_COPIES_MAX; fatCopy++ ){
				TLogSector lsFat=bootSector->fatStarts[fatCopy];
				if (PCFatValue fat=reinterpret_cast<PCFatValue>( __getHealthyLogicalSectorData__(lsFat) ))
					for( TLogSector index=logSector; __isValidFatSectorNumber__(lsFat); index-=BSDOS_FAT_ITEMS_PER_SECTOR ){
						const TFatValue value=fat[lsFat];
						if (!value.occupied)
							break; // next FAT copy
						if (index<BSDOS_FAT_ITEMS_PER_SECTOR)
							// navigated to the correct FAT Sector - reading Index-th value from it
							if ((value.continuous || index*sizeof(TFatValue)<value.info)
								&&
								( fat=reinterpret_cast<PCFatValue>(__getHealthyLogicalSectorData__(lsFat)) )
							)
								return fat[index];
							else
								break; // next FAT copy
						if (!value.continuous)
							break; // next FAT copy
						lsFat=value.info;
					}
			}
		return TFatValue(TFatValue::FatError); // FAT i/o error
	}

	bool CBSDOS308::GetSectorStatuses(TCylinder cyl,THead head,TSector nSectors,PCSectorId bufferId,PSectorStatus buffer) const{
		// True <=> Statuses of all Sectors in the Track successfully retrieved and populated the Buffer, otherwise False
		bool result=true; // assumption (statuses of all Sectors successfully retrieved)
		if (const PBootSector bootSector=boot.GetSectorData())
			for( const TLogSector logSectorBase=(formatBoot.nHeads*cyl+head)*formatBoot.nSectors-BSDOS_SECTOR_NUMBER_FIRST; nSectors--; bufferId++ ){
				const TSector secNum=bufferId->sector;
				if (cyl>=formatBoot.nCylinders || head>=formatBoot.nHeads || bufferId->cylinder!=cyl || bufferId->side!=sideMap[head] || secNum>formatBoot.nSectors || !secNum || bufferId->lengthCode!=BSDOS_SECTOR_LENGTH_STD_CODE) // condition for Sector must be ">", not ">=" (Sectors numbered from 1 - see also "|!id")
					// Sector number out of official Format
					*buffer++=TSectorStatus::UNKNOWN;
				else{
					// getting Sector Status from FAT
					const TLogSector ls=secNum+logSectorBase;
					if (ls<2) // first two Sectors ...
systemSector:			*buffer++=TSectorStatus::SYSTEM; // ... are always reserved for system usage, regardless of the value stored in FAT
					else
						switch (const TFatValue fatValue=__getLogicalSectorFatItem__(ls)){
							case TFatValue::FatError:
								result=false;
								//fallthrough
							case TFatValue::SectorUnavailable:
								*buffer++=TSectorStatus::UNAVAILABLE;
								break;
							case TFatValue::SystemSector:
								goto systemSector;
							case TFatValue::SectorErrorInDataField:
							case TFatValue::SectorNotFound:
								*buffer++=TSectorStatus::BAD;
								break;
							case TFatValue::SectorUnknown:
								*buffer++=TSectorStatus::UNKNOWN;
								break;
							default:
								// . if Sector is reported Empty, we are done
								if (!fatValue.occupied){
									*buffer++=TSectorStatus::EMPTY;
									break;
								}
								// . DIRS Sector is a System one
								if (ls==bootSector->dirsLogSector)
									goto systemSector;
								// . FAT Sectors are System ones
								const TPhysicalAddress chs={ cyl, head, *bufferId };
								for( BYTE fatCopy=0; fatCopy<BSDOS_FAT_COPIES_MAX; fatCopy++ )
									if (CFatPath( this, &TFatCopyRetrievalEntry(this,bootSector,fatCopy) ).ContainsSector(chs))
										goto systemSector;
								// . the Sector is Occupied
								*buffer++=TSectorStatus::OCCUPIED;
								break;
						}
				}
			}
		else
			while (nSectors--)
				*buffer++=TSectorStatus::UNKNOWN; // all Sector are Unknown by default
		return result;
	}

	bool CBSDOS308::__setLogicalSectorFatItem__(TLogSector logSector,TFatValue newValue) const{
		// returns the value in FAT of the specified LogicalSector; returns BSDOS_FAT_ERROR if FAT Sector read error
		bool valueWritten=false; // assumption (the Value couldn't be written into any FatCopy)
		if (const PCBootSector bootSector=boot.GetSectorData())
			for( BYTE fatCopy=0; fatCopy<BSDOS_FAT_COPIES_MAX; fatCopy++ ){
				const TLogSector lsFat0=bootSector->fatStarts[fatCopy];
				if (PFatValue fat=reinterpret_cast<PFatValue>(__getHealthyLogicalSectorData__(lsFat0)))
					for( TLogSector index=logSector,lsFat=lsFat0; __isValidFatSectorNumber__(lsFat); index-=BSDOS_FAT_ITEMS_PER_SECTOR ){
						const TFatValue value=fat[lsFat];
						if (!value.occupied)
							break; // next FAT copy
						if (index<BSDOS_FAT_ITEMS_PER_SECTOR){
							// navigated to the correct FAT Sector - writing NewValue at Index-th position
							BYTE &rFatChecksum=fat[0].upperByte;
							if ((value.continuous || index*sizeof(TFatValue)<value.info)
								&&
								( fat=reinterpret_cast<PFatValue>(__getHealthyLogicalSectorData__(lsFat)) )
							){
								if (logSector>0){
									const TFatValue oldValue=fat[index];
									rFatChecksum-=oldValue.lowerByte+oldValue.upperByte;
									rFatChecksum+=newValue.lowerByte+newValue.upperByte;
									__markLogicalSectorAsDirty__(lsFat0);
								}
								fat[index]=newValue;
								valueWritten=true;
								__markLogicalSectorAsDirty__(lsFat);
							}
							break; // next FAT copy
						}
						if (!value.continuous)
							break; // next FAT copy
						lsFat=value.info;
					}
			}
		return valueWritten;
	}

	bool CBSDOS308::ModifyStdSectorStatus(RCPhysicalAddress chs,TSectorStatus status) const{
		// True <=> the Status of the specified DOS-standard Sector successfully changed, otherwise False
		TFatValue value=TFatValue::SectorUnknown;
		switch (status){
			case TSectorStatus::UNAVAILABLE	: value=TFatValue::SectorUnavailable; break;
			case TSectorStatus::BAD			: value=TFatValue::SectorErrorInDataField; break;
			case TSectorStatus::EMPTY		: value=TFatValue::SectorEmpty; break;
			case TSectorStatus::UNKNOWN		: value=TFatValue::SectorUnknown; break;
			default:
				ASSERT(FALSE);
				break;
		}
		return __setLogicalSectorFatItem__( __fyzlog__(chs), value );
	}

	bool CBSDOS308::ModifyFileFatPath(PFile file,const CFatPath &rFatPath) const{
		// True <=> a error-free FatPath of given File successfully written, otherwise False
		CFatPath::PCItem pItem; DWORD nItems;
		if (rFatPath.GetItems(pItem,nItems)){ // if new FatPath erroneous ...
			ASSERT(false); // this shouldn't happen
			return false; // ... we are done
		}
		if (IsDirectory(file)){
			// root Subdirectory
			TLogSector ls = ((CDirsSector::PSlot)file)->firstSector =  pItem->chs ? __fyzlog__(pItem->chs) : pItem->value;
			for( WORD h; --nItems; ls=h )
				__setLogicalSectorFatItem__( ls, // no need to test FAT Sector existence (already tested above)
					TFatValue( true, true,
						h = (++pItem)->chs ? __fyzlog__(pItem->chs) : pItem->value
					)
				);
			__setLogicalSectorFatItem__(ls, // terminating the FatPath in FAT
										TFatValue( true, false, BSDOS_SECTOR_LENGTH_STD )
									);
			dirsSector.MarkAsDirty();
		}else{
			const PDirectoryEntry de=(PDirectoryEntry)file;
			if (DWORD fileSize=de->file.dataLength){
				TLogSector ls = de->file.firstSector =  pItem->chs ? __fyzlog__(pItem->chs) : pItem->value;
				for( WORD h; --nItems; ls=h,fileSize-=BSDOS_SECTOR_LENGTH_STD ) // all Sectors but the last one are Occupied in FatPath
					__setLogicalSectorFatItem__( ls, // no need to test FAT Sector existence (already tested above)
						TFatValue( true, true,
							h = (++pItem)->chs ? __fyzlog__(pItem->chs) : pItem->value
						)
					);
				__setLogicalSectorFatItem__(ls, // terminating the FatPath in FAT
											TFatValue( true, false, fileSize )
										);
			}else // zero-length Files are in NOT in the FAT
				de->file.firstSector=boot.GetSectorData()->dirsLogSector; // some sensible value
			MarkDirectorySectorAsDirty(de);
		}
		return true;
	}

	BYTE CBSDOS308::__getFatChecksum__(PCSectorData *pSectorData,WORD nSectors){
		// computes and returns the checksum of specified FAT Sectors
		BYTE result=(4096-nSectors*BSDOS_SECTOR_LENGTH_STD)*0xff;
		while (nSectors>0)
			if (const PCSectorData fatData=pSectorData[--nSectors])
				for( WORD i=sizeof(TFatValue)*(nSectors==0); i<BSDOS_SECTOR_LENGTH_STD; result+=fatData[i++] ); // first Value in FAT's first Sectors isn't included in the checksum
			//else
				//result+=BSDOS_SECTOR_LENGTH_STD*0xff; // commented out as adding zero
		return result;

	}

	BYTE CBSDOS308::__getFatChecksum__(BYTE fatCopy) const{
		// computes and returns the checksum of specified FAT copy
		if (const PBootSector bootSector=boot.GetSectorData())
			if (const CFatPath &&tmp=CFatPath( this, &TFatCopyRetrievalEntry(this,bootSector,fatCopy) )){
				PCSectorData sectorData[BSDOS_FAT_LOGSECTOR_MAX];
				for( BYTE s=0; s<bootSector->nSectorsPerFat; s++ )
					if (const auto pItem=tmp.GetHealthyItem(s))
						sectorData[s]=image->GetHealthySectorData(pItem->chs);
					else
						sectorData[s]=nullptr;
				return __getFatChecksum__( sectorData, bootSector->nSectorsPerFat );
			}
		return 0;
	}

	CBSDOS308::TLogSector CBSDOS308::__getEmptyHealthyFatSector__(bool allowFileFragmentation) const{
		// 
		// - first, searching for a healthy Sector that is reported Empty in existing FAT
		TPhysicalAddress chs;
		if (const TStdWinError err=GetFirstEmptyHealthySector(true,chs)) // if error finding (any) Empty healthy Sector ...
			return 0; // ... we are unsuccessfully done
		const TLogSector ls=__fyzlog__(chs);
		if (ls<BSDOS_FAT_LOGSECTOR_MAX)
			return ls;
		// - if fragmentation of existing Files not Allowed, we are unsuccessfully done
		if (!allowFileFragmentation)
			return 0;
		// - next, fragmenting an existing File, re-allocating one of its Sectors that begins before BSDOS_FAT_LOGSECTOR_MAX Sector
		for( CDirsSector::CTraversal rt(this); const PCFile subdir=rt.GetNextFileOrSubdir(); ) // root traversal
			for( TDirectoryEntry::CTraversal dt(this,subdir); const PFile file=dt.GetNextFileOrSubdir(); ){ // Subdirectory traversal
				const CFatPath fatPath(this,file);
				CFatPath::PItem pItem; DWORD nItems;
				if (fatPath.GetItems(pItem,nItems))
					continue; // can't re-allocate a Sector of a File whose FatPath is erroneous
				for( ; nItems--; pItem++ ){
					const TLogSector ls=__fyzlog__(pItem->chs);
					if (ls<BSDOS_FAT_LOGSECTOR_MAX) // found a Sector that might be moved to make space for a FAT Sector
						if (const PCSectorData data=__getHealthyLogicalSectorData__(ls)){ // the Sector is healthy
							// . finding a replacement Empty healthy Sector
							//nop (already found above)
							// . copying existing Data to the above Empty Sector
							const TLogSector lsEmpty=__fyzlog__(chs);
							::memcpy(	__getHealthyLogicalSectorData__(lsEmpty), // readability guaranteed
										data,
										formatBoot.sectorLength
									);
							//__markLogicalSectorAsDirty__(lsEmpty); // postponed until below
							// . changing File's FatPath
							pItem->chs=chs;
							if (!ModifyFileFatPath( file, fatPath )) // if FAT non-modifiable ...
								break; // ... better attempting to re-allocate Sector of another File
							__markLogicalSectorAsDirty__(lsEmpty);
							// . successfully re-allocated a healthy Sector, making it now Empty
							__setLogicalSectorFatItem__( ls, TFatValue::SectorEmpty );
							return ls;
						}
				}
			}
		// - no healthy Sector could be re-allocated
		return 0;
	}

	bool CBSDOS308::GetFileFatPath(PCFile file,CFatPath &rFatPath) const{
		// True <=> FatPath of given File (even an erroneous FatPath) successfully retrieved, otherwise False
		// - root Directory consists only of the DIRS sector
		CFatPath::TItem item;
		if (file==ZX_DIR_ROOT){
			if (const PCBootSector bootSector=boot.GetSectorData()){
				item.value=bootSector->dirsLogSector;
				item.chs=__logfyz__(bootSector->dirsLogSector);
				rFatPath.AddItem(&item);
			}else
				rFatPath.error=CFatPath::TError::VALUE_BAD_SECTOR;
			return true;
		}
		// - query on root Subdirectory is translated to a query on File
		if (IsDirectory(file))
			return GetFileFatPath( &TDirectoryEntry(this,((CDirsSector::PCSlot)file)->firstSector), rFatPath );
		// - no FatPath can be retrieved if DirectoryEntry is Empty
		const PCDirectoryEntry de=(PCDirectoryEntry)file;
		if (!de->occupied){
			rFatPath.error=CFatPath::TError::FILE;
			return false;
		}
		// - FatPath has no Items if File has no data or its length is zero
		if (!de->fileHasData || !de->file.dataLength)
			return true;
		// - extracting the FatPath from FAT
		const DWORD logSectorMax=formatBoot.GetCountOfAllSectors();
		TFatValue fatValue = item.value = de->file.firstSector;
		do{
			// . determining Sector's PhysicalAddress
			item.chs=__logfyz__(item.value);
			// . adding the Item to the FatPath
			if (!rFatPath.AddItem(&item)) break; // also sets an error in FatPath
			// . VALIDATION: Value must "make sense"
			fatValue=__getLogicalSectorFatItem__(item.value);
			if (fatValue==TFatValue::FatError){ // if FAT Sector cannot be read ...
				rFatPath.error=CFatPath::TError::SECTOR; // ... setting corresponding error ...
				break; // ... and quitting
			}
			if (fatValue>=TFatValue::SectorErrorInDataField){
				rFatPath.error=CFatPath::TError::VALUE_BAD_SECTOR;
				break;
			}
			if (!fatValue.occupied
				||
				fatValue.continuous && !(2<fatValue.info && fatValue.info<logSectorMax) // value must be within range
				||
				!fatValue.continuous && fatValue.info>BSDOS_SECTOR_LENGTH_STD
			){
				rFatPath.error=CFatPath::TError::VALUE_INVALID;
				break;
			}
			item.value=fatValue.info;
		}while (fatValue.continuous); // until the natural correct end of a File is found
		return true; // FatPath (with or without error) successfully extracted from FAT
	}











	#define BSDOS	static_cast<CBSDOS308 *>(vp.dos)
	#define IMAGE	BSDOS->image

	UINT AFX_CDECL CBSDOS308::FatReadabilityVerification_thread(PVOID pCancelableAction){
		// thread to verify the FAT integrity
		const PBackgroundActionCancelable pAction=(PBackgroundActionCancelable)pCancelableAction;
		const TSpectrumVerificationParams &vp=*(TSpectrumVerificationParams *)pAction->GetParams();
		vp.fReport.OpenSection(FAT_VERIFICATION_READABILITY);
		// - verifying basic FAT information
		const PBootSector boot=TBootSector::GetData(IMAGE);
		if (!boot)
			return vp.TerminateAll(ERROR_UNRECOGNIZED_VOLUME);
		if (const TStdWinError err=vp.VerifyUnsignedValue( TBootSector::CHS, BOOT_SECTOR_LOCATION_STRING, _T("FAT sectors"), boot->nSectorsPerFat, (WORD)1, (WORD)4 ))
			return vp.TerminateAll(err);
		if (const TStdWinError err=vp.VerifyUnsignedValue( TBootSector::CHS, BOOT_SECTOR_LOCATION_STRING, _T("FAT Bytes"), boot->nBytesInFat, (WORD)(boot->nSectorsPerFat*BSDOS_SECTOR_LENGTH_STD) ))
			return vp.TerminateAll(err);
		pAction->SetProgressTarget(	1 // retrieving both FAT copies
									+
									boot->nSectorsPerFat*BSDOS_FAT_COPIES_MAX
									+
									1 // decision-making (e.g. initial detection if FAT copies are cross-linked)
									+
									1 // allocation of missing FAT Sectors
									+
									1 // checking that all Sectors beyond the official format are marked as Unknown
									+
									1 // checking FAT checksums
									//+
									//1 // splitting FATs, thus increasing disk safety
									+
									1 // checking the DIRS Sector isn't actually mapped to one of FAT Sectors
								);
		int step=0;
		// Step 1: retrieving both FAT copies
		const class CFatPathEx sealed:public CFatPath{
		public:
			CFatPathEx(PCDos dos,PCDirectoryEntry de,WORD nSectorsExpected)
				: CFatPath( dos, de ) {
				if (GetNumberOfItems()!=nSectorsExpected)
					error=TError::LENGTH;
			}
		} fat1( BSDOS, &TFatCopyRetrievalEntry(BSDOS,boot,0), boot->nSectorsPerFat )
		, fat2( BSDOS, &TFatCopyRetrievalEntry(BSDOS,boot,1), boot->nSectorsPerFat );
		const CFatPath *pFats[]={ &fat1, &fat2 };
		for( BYTE i=0; i<BSDOS_FAT_COPIES_MAX; i++ )
			if (const LPCTSTR errMsg=pFats[i]->GetErrorDesc()){
				CString msg, sol;
				msg.Format( _T("FAT-%d is corrupted: %s"), 1+i, errMsg );
				sol.Format( _T("A merge with FAT-%d is suggested."), 1+(i+1)%BSDOS_FAT_COPIES_MAX );
				switch (vp.ConfirmFix( msg, sol )){
					case IDCANCEL:
						return vp.CancelAll();
					case IDNO:
						continue;
				}
				boot->fatStarts[i] = boot->fatStarts[(i+1)%BSDOS_FAT_COPIES_MAX];
				pFats[i] = pFats[(i+1)%BSDOS_FAT_COPIES_MAX];
				IMAGE->MarkSectorAsDirty(TBootSector::CHS);
				vp.fReport.CloseProblem(true);
			}
		if (pFats[0]->error || pFats[1]->error) // for the remainder of the verification, it's needed that both FAT copies are intact ... 
			return vp.TerminateAll(ERROR_VALIDATE_CONTINUE); // ... otherwise the results may be impredicatble
		pAction->UpdateProgress( ++step );
		// - Steps 2-(N-2): buffering FAT Sectors
		PSectorData fatData[BSDOS_FAT_COPIES_MAX][BSDOS_FAT_LOGSECTOR_MAX];
		for( BYTE s=0; s<boot->nSectorsPerFat; s++ )
			for( BYTE i=0; i<BSDOS_FAT_COPIES_MAX; i++,pAction->UpdateProgress(++step) )
				if (const auto pItem=pFats[i]->GetHealthyItem(s))
					fatData[i][s]=IMAGE->GetHealthySectorData(pItem->chs);
				else
					fatData[i][s]=nullptr;
		// - Step N-1: comparing the two FAT copies, eventually searching for a FAT instance that is a combination of both copies and has the correct checksum
		bool fatCopiesAreIdentical=true; // assumption
		for( BYTE s=0; s<boot->nSectorsPerFat; s++ )
			if ( fatCopiesAreIdentical&=!( fatData[0][s]!=nullptr ^ fatData[1][s]!=nullptr ) ) // either both copies have or don't have readable Sectors
				if (fatData[0][s]!=fatData[1][s]) // both FAT copies have readable Sectors
					fatCopiesAreIdentical=!::memcmp( fatData[0][s], fatData[1][s], BSDOS_SECTOR_LENGTH_STD );
		if (!fatCopiesAreIdentical)
			switch (vp.ConfirmFix( _T("The FAT copies are not identical"), _T("A brute-force search for a valid FAT instance is suggested.") )){
				case IDCANCEL:
					return vp.CancelAll();
				case IDNO:
					return vp.TerminateAndGoToNextAction(ERROR_VALIDATE_CONTINUE);
				case IDYES:{
					// . searching for a FAT instance that is a combination of both copies and has the correct checksum
					struct{
						TPhysicalAddress chs[BSDOS_FAT_LOGSECTOR_MAX];
						BYTE nSectorsBad;
					} current, optimal; // FAT instance
					optimal.nSectorsBad=-1; // -1 = absolutely unoptimal
					DWORD fatCombination=0; // bitwise identification of a FAT instance; N-th bit reset = use N-th Sector from first FAT copy, otherwise use N-th Sector from second FAT copy
					for( const DWORD fatLastCombination=1<<boot->nSectorsPerFat; fatCombination<fatLastCombination; fatCombination++ ){
						current.nSectorsBad=0;
						PCSectorData currentData[BSDOS_FAT_LOGSECTOR_MAX];
						for( BYTE s=0; s<boot->nSectorsPerFat; s++ ){
							const BYTE i=(fatCombination&1<<s)!=0;
							current.chs[s]=pFats[i]->GetHealthyItem(s)->chs;
							current.nSectorsBad+=( currentData[s]=fatData[i][s] )==nullptr;
						}
						if (current.nSectorsBad<optimal.nSectorsBad) // potentially a better FAT instance - need to check its checksum
							if (*currentData){ // first Sector exists, so existing checksum can be obtained
								const TFatValue v=*(TFatValue *)currentData[0];
								if (__getFatChecksum__(currentData,boot->nSectorsPerFat)==v.upperByte)
									// success - found an instance of FAT that has the expected checksum and has fewer bad Sectors
									optimal=current;
							}
					}
					if (optimal.nSectorsBad<boot->nSectorsPerFat){ // at least a sub-optimal FAT instance found
						TDirectoryEntry de1(BSDOS,boot->fatStarts[0]), de2(BSDOS,boot->fatStarts[1]);
						for( BYTE s=0; s<boot->nSectorsPerFat; s++ )
							if (pFats[0]->GetHealthyItem(s)->chs!=optimal.chs[s]){ // let the first FAT copy be the valid one
								std::swap( pFats[0]->GetHealthyItem(s)->chs, pFats[1]->GetHealthyItem(s)->chs );
								std::swap( fatData[0][s], fatData[1][s] );
							}
						BSDOS->ModifyFileFatPath( &de1, *pFats[0] );
							boot->fatStarts[0]=de1.file.firstSector;
						BSDOS->ModifyFileFatPath( &de2, *pFats[1] );
							boot->fatStarts[1]=de2.file.firstSector;
						BSDOS->boot.MarkSectorAsDirty();
						for( BYTE s=0; s<boot->nSectorsPerFat; s++ )
							if (fatData[1][s]){ // if possible, spreading the optimal FAT instance to the second FAT copy
								::memcpy( fatData[1][s], fatData[0][s], BSDOS_SECTOR_LENGTH_STD );
								IMAGE->MarkSectorAsDirty( pFats[1]->GetHealthyItem(s)->chs );
							}
						vp.fReport.CloseProblem(true);
						break; // solved - both FAT copies are identical now
					}
					// . declaring the correct FAT instance the more up-to-date copy of FAT
					if (fatData[0][0] && fatData[1][0]){ // both FAT copies have readable first Sectors
						const BYTE iSrc=*(PCBYTE)fatData[0][0]<=*(PCBYTE)fatData[1][0], iDst=(iSrc+1)%BSDOS_FAT_COPIES_MAX;
						for( BYTE s=0; s<boot->nSectorsPerFat; s++ )
							if (fatData[0][s] && fatData[1][s]){
								::memcpy( fatData[iDst][s], fatData[iSrc][s], BSDOS_SECTOR_LENGTH_STD );
								IMAGE->MarkSectorAsDirty( pFats[iDst]->GetHealthyItem(s)->chs );
							}
						vp.fReport.CloseProblem(true);
						break; // solved - both FAT copies are identical now
					}
					// . declaring the correct FAT instance the copy which is has readable first Sector
					const BYTE iSrc=(ULONG_PTR)fatData[0][0]<(ULONG_PTR)fatData[1][0], iDst=(iSrc+1)%BSDOS_FAT_COPIES_MAX;
					for( BYTE s=0; s<boot->nSectorsPerFat; s++ )
						if (fatData[0][s] && fatData[1][s]){
							::memcpy( fatData[iDst][s], fatData[iSrc][s], BSDOS_SECTOR_LENGTH_STD );
							IMAGE->MarkSectorAsDirty( pFats[iDst]->GetHealthyItem(s)->chs );
						}
					vp.fReport.CloseProblem(true);
					break;
				}
			}

		pAction->UpdateProgress(++step);
		// - recovering readability of both FAT copies
		TDirectoryEntry deFat( BSDOS, boot->fatStarts[0] ); // both FAT copies are valid multi-sector structures, hence any of them can be refered to create this DirectoryEntry
		for( BYTE i=0,replacementDecision=0; i<BSDOS_FAT_COPIES_MAX; i++ ){
			// . replacing erroneous Sectors
			bool fatCopyModified=false; // assumption
			for( BYTE s=0; s<boot->nSectorsPerFat; s++ )
				if (!fatData[i][s]){
					if (!replacementDecision) // not yet asked about what to do
						replacementDecision=vp.ConfirmFix( _T("Some FAT sectors are bad"), _T("All should be replaced with healthy ones.") );
					switch (replacementDecision){
						case IDCANCEL:
							return vp.CancelAll();
						case IDNO:
							continue;
					}
					TPhysicalAddress &rChs=pFats[i]->GetHealthyItem(s)->chs;
					if (const TLogSector ls=BSDOS->__getEmptyHealthyFatSector__(true)){
						// found a healthy Sector to replace the erroneous
						// : marking the original Sector as Bad (first Sector of first FAT copy is guaranteed to be always readable, hence the information on the Bad Sector will be registered)
						BSDOS->__setLogicalSectorFatItem__( BSDOS->__fyzlog__(rChs), TFatValue::SectorErrorInDataField );
						// : replacing the erroneous Sector
						rChs=BSDOS->__logfyz__(ls);
						if (!BSDOS->ModifyFileFatPath( &deFat, *pFats[i] ))
							return vp.TerminateAll(ERROR_FUNCTION_FAILED); // we shouldn't end up here but just to be sure
						if (!s){
							boot->fatStarts[i]=ls;
							IMAGE->MarkSectorAsDirty(TBootSector::CHS);
						}
						// : initializing the replacement Sector
						const PSectorData newData=BSDOS->__getHealthyLogicalSectorData__(ls);
						if (!i)
							// first FAT copy erroneous Sector declares all addressed Sectors are Empty
							::ZeroMemory( newData, BSDOS_SECTOR_LENGTH_STD );
						else
							// second FAT copy mirrors the referential first FAT copy
							::memcpy( newData, fatData[0][s], BSDOS_SECTOR_LENGTH_STD );
						fatData[i][s]=newData;
						BSDOS->__markLogicalSectorAsDirty__(ls);
						fatCopyModified=true;
						vp.fReport.CloseProblem(true);
					}else
						vp.fReport.LogWarning( _T("FAT-%d sector with %s is unreadable and can't be replaced"), i, (LPCTSTR)rChs.sectorId.ToString() );
				}
			// . writing the FAT copy Sectors to the Boot Sector
			bool fatCopySectorsMatch=true;
			for( BYTE s=1; s<boot->nSectorsPerFat; s++ )
				fatCopySectorsMatch&=boot->fatSectorsListing[i+2*(s-1)]==BSDOS->__fyzlog__( pFats[i]->GetHealthyItem(s)->chs );
			if (!fatCopyModified && !fatCopySectorsMatch){ // although no modifications were made to the FAT copy, its Sectors don't match with those reported in the Boot Sector
				CString msg;
				msg.Format( _T("FAT-%d sectors don't match with those reported in disk boot"), i );
				switch (vp.ConfirmFix( msg, _T("Replacement directly from FAT is suggested.") )){
					case IDCANCEL:
						return vp.CancelAll();
					case IDNO:
						continue;
				}
				for( BYTE s=1; s<boot->nSectorsPerFat; s++ )
					boot->fatSectorsListing[i+2*(s-1)]=BSDOS->__fyzlog__( pFats[i]->GetHealthyItem(s)->chs );
				IMAGE->MarkSectorAsDirty(TBootSector::CHS);
				vp.fReport.CloseProblem(true);
			}
		}
		pAction->UpdateProgress(++step);
		// - checking that all Sectors beyond the official format are marked as Unknown
		for( TLogSector ls=BSDOS->formatBoot.GetCountOfAllSectors(),fixConfirmation=0; ls<boot->nSectorsPerFat*BSDOS_FAT_ITEMS_PER_SECTOR; ls++ )
			if (BSDOS->__getLogicalSectorFatItem__(ls)!=TFatValue::SectorUnknown){
				if (!fixConfirmation) // not yet asked about what to do
					fixConfirmation=vp.ConfirmFix( _T("Sectors beyond the official format aren't marked as \"unknown\""), _T("") );
				switch (fixConfirmation){
					case IDCANCEL:
						return vp.CancelAll();
					case IDNO:
						continue;
				}
				if (!BSDOS->__setLogicalSectorFatItem__( ls, TFatValue::SectorUnknown ))
					return vp.TerminateAll(ERROR_FUNCTION_FAILED);
				vp.fReport.CloseProblem(true);
			}
		pAction->UpdateProgress(++step);
		// - checking FAT copies checksums
		for( BYTE i=0; i<BSDOS_FAT_COPIES_MAX; i++ )
			if (const PFatValue v=(PFatValue)fatData[i][0]){ // first Sector exists
				const BYTE correctChecksum=BSDOS->__getFatChecksum__(i);
				if (v->upperByte!=correctChecksum){
					TCHAR buf[80];
					::wsprintf( buf, _T("FAT-%d checksum is incorrect"), i );
					switch (vp.ConfirmFix( buf, _T("Checksum should be recalculated.") )){
						case IDCANCEL:
							return vp.CancelAll();
						case IDNO:
							continue;
					}
					v->upperByte=correctChecksum;
					IMAGE->MarkSectorAsDirty( pFats[i]->GetHealthyItem(0)->chs );
					vp.fReport.CloseProblem(true);
				}
			}
		pAction->UpdateProgress(++step);
		// - splitting FATs, thus increasing disk safety
		/* // commented out, TODO eventually in the future
		if (boot->fatStarts[0]==boot->fatStarts[1])
			switch (vp.ConfirmFix( _T("The disk contains just one FAT copy"), _T("There should be also a second back-up copy.") )){
				case IDCANCEL:
					return vp.CancelAll();
				case IDYES:{
					//TODO
					break;
				}
			}
		pAction->UpdateProgress(++step);
		*/
		// - checking the DIRS Sector isn't actually cross-linked with one of FAT Sectors
		TLogSector lsDirs=boot->dirsLogSector;
		TPhysicalAddress chsDirs=BSDOS->__logfyz__(lsDirs);
		boot->dirsLogSector=0;
			const TSectorStatus lsDirsStatus=BSDOS->GetSectorStatus(chsDirs);
		boot->dirsLogSector=lsDirs;
		if (lsDirsStatus==TSectorStatus::SYSTEM) // yes, cross-linked with one of FAT Sectors
			switch (vp.ConfirmFix( _T("DIRS sector cross-linked with FAT"), _T("Their split is suggested.") )){
				case IDCANCEL:
					return vp.CancelAll();
				case IDNO:
					return vp.TerminateAll(ERROR_VALIDATE_CONTINUE); // can't continue if fix suggestion rejected
				case IDYES:
					if (const auto dirsDataOrg=BSDOS->dirsSector.GetSlots()){
						if (const TStdWinError err=BSDOS->GetFirstEmptyHealthySector(true,chsDirs))
							return vp.TerminateAndGoToNextAction(err);
						boot->dirsLogSector = lsDirs = BSDOS->__fyzlog__(chsDirs);
						IMAGE->MarkSectorAsDirty(TBootSector::CHS);
						::memcpy( BSDOS->__getHealthyLogicalSectorData__(lsDirs), dirsDataOrg, BSDOS_SECTOR_LENGTH_STD );
						BSDOS->__markLogicalSectorAsDirty__(lsDirs);
						BSDOS->__setLogicalSectorFatItem__( lsDirs, TFatValue(true,false,BSDOS_SECTOR_LENGTH_STD) );
						vp.fReport.CloseProblem(true);
					}else
						vp.fReport.LogWarning( _T("DIRS sector unreadable") );
					break;
			}
		pAction->UpdateProgress(++step);
		return ERROR_SUCCESS;
	}
