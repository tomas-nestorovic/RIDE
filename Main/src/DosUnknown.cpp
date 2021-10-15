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
		nullptr, // name
		0, // unique identifier
		0, // recognition priority
		__recognize__, // recognition function
		__instantiate__, // instantiation function
		Medium::UNKNOWN, // Unknown Medium
		nullptr, // the most common Image to contain data for this DOS (e.g. *.D80 Image for MDOS)
		1,	// number of std Formats
		CMDOS2::Properties.stdFormats, // std Formats ("some" Format in case of UnknownDos)
		Codec::ANY, // a set of Codecs this DOS supports
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
		: CDos( image, pFormatBoot, TTrackScheme::BY_CYLINDERS, &Properties, nullptr, image->GetSideMap()?image->GetSideMap():StdSidesMap, IDR_DOS_UNKNOWN, nullptr, TGetFileSizeOptions::OfficialDataLength, TSectorStatus::UNKNOWN )
		// - initialization
		, trackMap(image) {
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
	bool CUnknownDos::ModifyStdSectorStatus(RCPhysicalAddress,TSectorStatus) const{
		// True <=> the Status of the specified DOS-standard Sector successfully changed, otherwise False
		return false; //nop (doesn't have an allocation table)
	}
	bool CUnknownDos::GetFileFatPath(PCFile,CFatPath &) const{
		// True <=> FatPath of given File (even an erroneous FatPath) successfully retrieved, otherwise False
		return false; //nop (doesn't have an allocation table)
	}
	bool CUnknownDos::ModifyFileFatPath(PFile,const CFatPath &) const{
		// True <=> a error-free FatPath of given File successfully written, otherwise False
		return false; //nop (doesn't have an allocation table)
	}
	DWORD CUnknownDos::GetFreeSpaceInBytes(TStdWinError &rError) const{
		// computes and returns the empty space on disk
		rError=ERROR_SUCCESS;
		return 0;
	}





	bool CUnknownDos::GetFileNameOrExt(PCFile,PPathString,PPathString) const{
		// populates the Buffers with File's name and extension; caller guarantees that the Buffer sizes are at least MAX_PATH characters each
		return false; // name irrelevant
	}
	TStdWinError CUnknownDos::ChangeFileNameAndExt(PFile,RCPathString,RCPathString,PFile &){
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

	std::unique_ptr<CDos::TDirectoryTraversal> CUnknownDos::BeginDirectoryTraversal(PCFile) const{
		// initiates exploration of specified Directory through a DOS-specific DirectoryTraversal
		return nullptr;
	}
	CString CUnknownDos::GetFileExportNameAndExt(PCFile,bool) const{
		// returns File name concatenated with File extension for export of the File to another Windows application (e.g. Explorer)
		return _T("");
	}
	TStdWinError CUnknownDos::ImportFile(CFile *fIn,DWORD fileSize,LPCTSTR nameAndExtension,DWORD winAttr,PFile &rFile){
		// imports specified File (physical or virtual) into the Image; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED;
	}






	TStdWinError CUnknownDos::CreateUserInterface(HWND hTdi){
		// creates DOS-specific Tabs in TDI; returns Windows standard i/o error
		__super::CreateUserInterface(hTdi); // guaranteed to always return ERROR_SUCCESS
		CTdiCtrl::InsertTab( hTdi, 0, TRACK_MAP_TAB_LABEL, &trackMap.tab, true, nullptr, nullptr );
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
