#include "stdafx.h"
#include "BSDOS.h"

	TStdWinError CBSDOS308::__recognizeDisk__(PImage image,PFormat pFormatBoot){
		// returns the result of attempting to recognize Image by this DOS as follows: ERROR_SUCCESS = recognized, ERROR_CANCELLED = user cancelled the recognition sequence, any other error = not recognized
		// - determining the Type of Medium (type of floppy)
		TFormat fmt={ TMedium::FLOPPY_DD, 1,1,BSDOS_SECTOR_NUMBER_TEMP, BSDOS_SECTOR_LENGTH_STD_CODE,BSDOS_SECTOR_LENGTH_STD, 1 };
		if (image->SetMediumTypeAndGeometry(&fmt,StdSidesMap,BSDOS_SECTOR_NUMBER_FIRST)!=ERROR_SUCCESS || !image->GetNumberOfFormattedSides(0)){
			fmt.mediumType=TMedium::FLOPPY_HD;
			if (image->SetMediumTypeAndGeometry(&fmt,StdSidesMap,BSDOS_SECTOR_NUMBER_FIRST)!=ERROR_SUCCESS || !image->GetNumberOfFormattedSides(0))
				return ERROR_UNRECOGNIZED_VOLUME; // unknown Medium Type
		}
		// - recognition attempt
		if (const PCBootSector boot=TBootSector::GetData(image))
			if (boot->jmpInstruction.opCode==0x18 // "jr N" instruction
				&&
				boot->signature1==0x02
				&&
				((boot->signature2|boot->signature3)==0x00)
				&&
				BSDOS_SECTOR_NUMBER_FIRST<boot->nSectorsPerTrack && boot->nSectorsPerTrack<=BSDOS_SECTOR_NUMBER_LAST
				&&
				1<=boot->nHeads && boot->nHeads<=2
				&&
				boot->nSectorsPerCluster>=1
				&&
				boot->nBytesInFat==BSDOS_SECTOR_LENGTH_STD*boot->nSectorsPerFat
				&&
				(boot->fatStarts[0]<BSDOS_SECTOR_LENGTH_STD/sizeof(TFatValue) || boot->fatStarts[1]<BSDOS_SECTOR_LENGTH_STD/sizeof(TFatValue))
			){
				if (boot->nSectorsPerTrack>BSDOS_SECTOR_NUMBER_LAST/2)
					fmt.mediumType=TMedium::FLOPPY_HD;
				fmt.nCylinders=boot->nCylinders;
				fmt.nHeads=boot->nHeads;
				fmt.nSectors=boot->nSectorsPerTrack;
				fmt.clusterSize=boot->nSectorsPerCluster;
				*pFormatBoot=fmt;
				return ERROR_SUCCESS;
			}
		// - not recognized
		return ERROR_UNRECOGNIZED_VOLUME;
	}

	static PDos __instantiate__(PImage image,PCFormat pFormatBoot){
		return new CBSDOS308(image,pFormatBoot);
	}

	#define BSDOS_SECTOR_GAP3	32 /* smaller than regular IBM norm-compliant Gap to make sure all Sectors fit in a Track */

	static const CFormatDialog::TStdFormat StdFormats[]={
		{ _T("5.25\" DS DD"), 0, {TMedium::FLOPPY_DD,39,2,5,TFormat::TLengthCode::LENGTHCODE_1024,BSDOS_SECTOR_LENGTH_STD,1}, 1, 0, BSDOS_SECTOR_GAP3, 2, 32 },
		{ _T("3.5\"/5.25\" DS DD"), 0, {TMedium::FLOPPY_DD,79,2,5,TFormat::TLengthCode::LENGTHCODE_1024,BSDOS_SECTOR_LENGTH_STD,1}, 1, 0, BSDOS_SECTOR_GAP3, 2, 32 },
		{ _T("3.5\" DS HD"), 0, {TMedium::FLOPPY_HD,79,2,11,TFormat::TLengthCode::LENGTHCODE_1024,BSDOS_SECTOR_LENGTH_STD,1}, 1, 0, BSDOS_SECTOR_GAP3, 2, 32 }
	};

	const CDos::TProperties CBSDOS308::Properties={
		_T("BS-DOS 308 (MB-02)"), // name
		MAKE_DOS_ID('B','S','D','O','S','-','0','2'), // unique identifier
		60, // recognition priority
		__recognizeDisk__, // recognition function
		__instantiate__, // instantiation function
		TMedium::FLOPPY_ANY, // Unknown Medium
		&MBD::Properties, // the most common Image to contain data for this DOS (e.g. *.D80 Image for MDOS)
		3,	// number of std Formats
		StdFormats,//CMDOS2::Properties.stdFormats, // std Formats ("some" Format in case of UnknownDos)
		1,11, // range of supported number of Sectors
		5, // minimal total number of Sectors required
		1, // maximum number of Sector in one Cluster (must be power of 2)
		BSDOS_SECTOR_LENGTH_STD, // maximum size of a Cluster (in Bytes)
		1,2, // range of supported number of allocation tables (FATs)
		32,12640, // range of supported number of root Directory entries
		1,	// lowest Sector number on each Track
		0x00,0x00, // regular Sector and Directory Sector filler Byte
		0,0 // number of reserved Bytes at the beginning and end of each Sector
	};









	const TPhysicalAddress CBSDOS308::TBootSector::CHS={ 0, 0, {0,0,1,BSDOS_SECTOR_LENGTH_STD_CODE} };

	CBSDOS308::PBootSector CBSDOS308::TBootSector::GetData(PImage image){
		// attempts to get and eventually returns BS-DOS BootSector data, eventually returning Null in case of error
		return reinterpret_cast<TBootSector *>(image->GetHealthySectorData(CHS));
	}

	CBSDOS308::PBootSector CBSDOS308::CBsdosBootView::GetSectorData() const{
		// attempts to get and eventually returns BS-DOS BootSector data; returns Null if BootSector not found
		return TBootSector::GetData(tab.dos->image);
	}










	CBSDOS308::CBsdosBootView::CBsdosBootView(CBSDOS308 *bsdos)
		// ctor
		// - base
		: CBootView(bsdos,TBootSector::CHS) {
	}










	void CBSDOS308::CBsdosBootView::GetCommonBootParameters(RCommonBootParameters rParam,PSectorData boot){
		// gets basic parameters from the Boot Sector
		rParam.geometryCategory=true;
			rParam.chs=true;
		rParam.volumeCategory=true;
			rParam.clusterSize=true;
	}

	void WINAPI CBSDOS308::TBootSector::OnDiskIdChanged(PropGrid::PCustomParam bootSector){
		const PBootSector boot=(PBootSector)bootSector;
		boot->diskIdChecksum=__xorChecksum__( boot->diskId, sizeof(boot->diskId) );
		CBootView::__bootSectorModified__(boot);
	}

	void CBSDOS308::CBsdosBootView::AddCustomBootParameters(HWND hPropGrid,HANDLE hGeometry,HANDLE hVolume,const TCommonBootParameters &rParam,PSectorData _boot){
		// gets DOS-specific parameters from the Boot
		const PBootSector boot=reinterpret_cast<PBootSector>(_boot);
		PropGrid::AddProperty(	hPropGrid, hVolume, _T("Label"), boot->diskName,
								TZxRom::CLineComposerPropGridEditor::Define(sizeof(boot->diskName),' ',nullptr,__bootSectorModified__)
							);
		PropGrid::AddProperty(	hPropGrid, hVolume, _T("Comment"), boot->diskComment,
								TZxRom::CLineComposerPropGridEditor::Define(sizeof(boot->diskComment),' ',nullptr,__bootSectorModified__)
							);
		PropGrid::AddProperty(	hPropGrid, hVolume, _T("Date formatted"), &boot->formattedDateTime,
								CMSDOS7::TDateTime::DefinePropGridDateTimeEditor(__bootSectorModified__)
							);
		PropGrid::AddProperty(	hPropGrid, hVolume, _T("Disk ID"), &boot->diskId,
								CHexaValuePropGridEditor::Define( nullptr, sizeof(boot->diskId), TBootSector::OnDiskIdChanged ),
								boot
							);
		const HANDLE hAdvanced=PropGrid::AddCategory( hPropGrid, hVolume, BOOT_SECTOR_ADVANCED );
			const PropGrid::PCEditor pWordEditor=PropGrid::Integer::DefineWordEditor(__bootSectorModified__);
			PropGrid::AddProperty( hPropGrid, hAdvanced, _T("FAT sectors"), &boot->nSectorsPerFat, pWordEditor );
			PropGrid::AddProperty( hPropGrid, hAdvanced, _T("FAT Bytes"), &boot->nBytesInFat, pWordEditor );
			PropGrid::AddPropertyW( hPropGrid, hAdvanced, L"FAT\xB9 start", &boot->fatStarts[0], pWordEditor );
			PropGrid::AddPropertyW( hPropGrid, hAdvanced, L"FAT\xB2 start", &boot->fatStarts[1], pWordEditor );
			PropGrid::AddProperty( hPropGrid, hAdvanced, _T("DIRS sector"), &boot->dirsLogSector, pWordEditor );


		if (const auto pdt=tab.dos->BeginDirectoryTraversal())
			while (pdt->AdvanceToNextEntry()){
				const PCDirectoryEntry de=(PCDirectoryEntry)pdt->entry;
				::Sleep(0);
			}
	}

	void CBSDOS308::FlushToBootSector() const{
		// flushes internal Format information to the actual Boot Sector's data
		if (const PBootSector boot=TBootSector::GetData(image)){
			boot->signature1=0x02;
			boot->signature2 = boot->signature3 = 0x00;
			boot->nCylinders=formatBoot.nCylinders;
			boot->nSectorsPerTrack=formatBoot.nSectors;
			boot->nHeads=formatBoot.nHeads;
			boot->nSectorsPerCluster=formatBoot.clusterSize;
			boot->nBytesInFat=BSDOS_SECTOR_LENGTH_STD*boot->nSectorsPerFat;
			image->MarkSectorAsDirty(TBootSector::CHS);
		}
	}
