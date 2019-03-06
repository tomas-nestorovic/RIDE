#include "stdafx.h"
#include "MDOS2.h" // included to refer to one of its StandardFormats

	static PDos __instantiate__(PImage image,PCFormat pFormatBoot){
		return new CUnknownDos(image,pFormatBoot);
	}
	static TStdWinError __recognize__(PImage image,PFormat pFormatBoot){
		// returns the result of attempting to recognize Image by this DOS as follows: ERROR_SUCCESS = recognized, ERROR_CANCELLED = user cancelled the recognition sequence, any other error = not recognized
		*pFormatBoot=TFormat::Unknown;
		return ERROR_SUCCESS;
	}
	const CDos::TProperties CUnknownDos::Properties={
		NULL, // name
		0, // unique identifier
		0, // recognition priority
		__recognize__, // recognition function
		__instantiate__, // instantiation function
		TMedium::UNKNOWN, // Unknown Medium
		1,	// number of std Formats
		CMDOS2::Properties.stdFormats, // std Formats ("some" Format in case of UnknownDos)
		0,0, // range of supported number of Sectors
		0, // minimal total number of Sectors required
		0, // maximum number of Sector in one Cluster (must be power of 2)
		0, // maximum size of a Cluster (in Bytes)
		0,0, // range of supported number of allocation tables (FATs)
		0,0, // range of supported number of root Directory entries
		1,	// lowest Sector number on each Track
		0xe5,0, // regular Sector and Directory Sector filler Byte
		0,0 // number of reserved Bytes at the beginning and end of each Sector
	};

	CUnknownDos::CUnknownDos(PImage image,PCFormat pFormatBoot)
		// ctor
		// - base
		: CDos( image, pFormatBoot, TTrackScheme::BY_CYLINDERS, &Properties, NULL, StdSidesMap, IDR_DOS_UNKNOWN, NULL, TGetFileSizeOptions::OfficialDataLength )
		// - initialization
		, trackMap(this) {
	}










	void CUnknownDos::FlushToBootSector() const{
		// flushes internal Format information to the actual Boot Sector's data
		//nop
	}





	bool CUnknownDos::GetSectorStatuses(TCylinder,THead,TSector nSectors,PCSectorId,PSectorStatus buffer) const{
		// True <=> Statuses of all Sectors in the Track successfully retrieved and populated the Buffer, otherwise False
		while (nSectors--) *buffer++=TSectorStatus::UNKNOWN; // all Sector are Unknown by design
		return true;
	}
	bool CUnknownDos::ModifyTrackInFat(TCylinder,THead,PSectorStatus){
		// True <=> Statuses of all Sectors in Track successfully changed, otherwise False; caller guarantees that the number of Statuses corresponds with the number of standard "official" Sectors in the Boot
		return false; //nop (doesn't have an allocation table)
	}
	bool CUnknownDos::GetFileFatPath(PCFile,CFatPath &) const{
		// True <=> FatPath of given File (even an erroneous FatPath) successfully retrieved, otherwise False
		return false; //nop (doesn't have an allocation table)
	}
	DWORD CUnknownDos::GetFreeSpaceInBytes(TStdWinError &rError) const{
		// computes and returns the empty space on disk
		rError=ERROR_SUCCESS;
		return 0;
	}





	void CUnknownDos::GetFileNameAndExt(PCFile,PTCHAR,PTCHAR) const{
		// populates the Buffers with File's name and extension; caller guarantees that the Buffer sizes are at least MAX_PATH characters each
	}
	TStdWinError CUnknownDos::ChangeFileNameAndExt(PFile,LPCTSTR,LPCTSTR,PFile &){
		// tries to change given File's name and extension; returns Windows standard i/o error
		return ERROR_FILE_INVALID;
	}
	DWORD CUnknownDos::GetFileSize(PCFile,PBYTE,PBYTE,TGetFileSizeOptions) const{
		// determines and returns the size of specified File
		return 0;
	}
	DWORD CUnknownDos::GetAttributes(PCFile) const{
		// maps File's attributes to Windows attributes and returns the result
		return 0; // none but standard attributes
	}
	TStdWinError CUnknownDos::DeleteFile(PFile){
		// deletes specified File; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}

	CDos::PDirectoryTraversal CUnknownDos::BeginDirectoryTraversal() const{
		// initiates exploration of current Directory through a DOS-specific DirectoryTraversal
		return NULL;
	}
	PTCHAR CUnknownDos::GetFileExportNameAndExt(PCFile,bool,PTCHAR) const{
		// populates Buffer with specified File's export name and extension and returns the Buffer; returns Null if File cannot be exported (e.g. a "dotdot" entry in MS-DOS); caller guarantees that the Buffer is at least MAX_PATH characters big
		return NULL;
	}
	TStdWinError CUnknownDos::ImportFile(CFile *fIn,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile){
		// imports specified File (physical or virtual) into the Image; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}






	TStdWinError CUnknownDos::CreateUserInterface(HWND hTdi){
		// creates DOS-specific Tabs in TDI; returns Windows standard i/o error
		CDos::CreateUserInterface(hTdi); // guaranteed to always return ERROR_SUCCESS
		CTdiCtrl::InsertTab( hTdi, 0, TRACK_MAP_TAB_LABEL, &trackMap.tab, true, NULL, NULL );
		return ERROR_SUCCESS;
	}
	CDos::TCmdResult CUnknownDos::ProcessCommand(WORD cmd){
		// returns the Result of processing a DOS-related command
		if (cmd==ID_DOS){
			Utils::Information(_T("You see this because:\n(a) the boot sector was not found on the disk, or\n(b) the boot sector was found but not recognized.\n\nIf you know for sure that the disk relates to one of implemented DOSes, you can try to open it using the \"Open as\" menu command.\n\nIf you ended up here after formatting a new disk, there have been problems formatting it - irregularities in the \"") TRACK_MAP_TAB_LABEL _T("\" tab help to reveal them."));
			return TCmdResult::DONE;
		}else
			return TCmdResult::REFUSED;
	}

	void CUnknownDos::InitializeEmptyMedium(CFormatDialog::PCParameters){
		// initializes a fresh formatted Medium (Boot, FAT, root dir, etc.)
	}
