#include "stdafx.h"
#include "TRDOS.h"

	static LPCTSTR Recognize(PTCHAR){
		static constexpr TCHAR SingleDeviceName[]=_T("SCL image\0");
		return SingleDeviceName;
	}
	static PImage Instantiate(LPCTSTR){
		return new CSCL;
	}

	const CImage::TProperties CSCL::Properties={
		MAKE_IMAGE_ID('T','R','D','O','S','S','C','L'), // a unique identifier
		Recognize,	// list of recognized device names
		Instantiate,// instantiation function
		_T("*.scl"),	// filter
		Medium::FLOPPY_DD_ANY, // supported Media
		Codec::MFM, // supported Codecs
		TRDOS503_SECTOR_LENGTH_STD,TRDOS503_SECTOR_LENGTH_STD	// Sector supported min and max length
	};






	

	CSCL::CSCL()
		// ctor
		: CImageRaw(&Properties,true) {
	}








	#define HEADER_SINCLAIR		"SINCLAIR"

	#pragma pack(1)
	struct TSclDirectoryItem sealed{
		char name[TRDOS503_FILE_NAME_LENGTH_MAX];
		BYTE extension;
		WORD parameterA;
		WORD parameterB;
		BYTE nSectors;
	};

	#pragma pack(1)
	struct TSclHeader sealed{
		char id[8];
		BYTE nFiles;
	};

	TStdWinError CSCL::SaveAllModifiedTracks(LPCTSTR lpszPathName,CActionProgress &ap){
		// saves all Modified Tracks; returns Windows standard i/o error
		if (const CTRDOS503::PCBootSector boot=CTRDOS503::TBootSector::Get(this)){
			if (f.m_hFile!=CFile::hFileNull) // Image's underlying file doesn't exist if saving a fresh formatted Image
				f.Close();
			if (const TStdWinError err=CreateImageForReadingAndWriting(lpszPathName,f))
				return err;
			// - saving Header
			TSclHeader header;
			::lstrcpyA( header.id, HEADER_SINCLAIR );
			header.nFiles=boot->nFiles;
			if (GetCurrentDiskFreeSpace()<sizeof(header))
				return ERROR_DISK_FULL;
			f.Write(&header,sizeof(header));
			// - saving Directory
			CTRDOS503 *const trdos=(CTRDOS503 *)dos;
			CTRDOS503::PDirectoryEntry directory[TRDOS503_FILE_COUNT_MAX],*pde=directory;
			for( BYTE n=trdos->__getDirectory__(directory); n--; f.Write(*pde++,sizeof(TSclDirectoryItem)) )
				if (GetCurrentDiskFreeSpace()<sizeof(TSclDirectoryItem))
					return ERROR_DISK_FULL;
			// - saving each File's data
	{		const Utils::CVarTempReset<CDos::TGetFileSizeOptions> gfso0( trdos->getFileSizeDefaultOption, CDos::TGetFileSizeOptions::SizeOnDisk ); // exporting whole Sectors instead of just reported File length
				BYTE buf[65536];
				for( pde=directory; header.nFiles--; ){
					const DWORD fileLength=trdos->ExportFile( *pde++, &CMemFile(buf,sizeof(buf)), sizeof(buf), nullptr );
					if (GetCurrentDiskFreeSpace()<fileLength)
						return ERROR_DISK_FULL;
					f.Write( buf, fileLength );
				}
				m_bModified=FALSE;
	}		// - reopening the Image's underlying file
			return ERROR_SUCCESS;
		}else
			return ERROR_SECTOR_NOT_FOUND;
	}

	TStdWinError CSCL::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - base
		if (const TStdWinError err=__super::SetMediumTypeAndGeometry(pFormat,sideMap,firstSectorNumber))
			return err; // we should always succeeed, but just to be sure
		// - allowed are only TRDOS-compliant formats
		if (pFormat->nSectors!=TRDOS503_TRACK_SECTORS_COUNT || pFormat->sectorLength!=TRDOS503_SECTOR_LENGTH_STD || pFormat->sectorLengthCode!=TRDOS503_SECTOR_LENGTH_STD_CODE)
			return Utils::ErrorByOs( ERROR_VHD_FORMAT_UNKNOWN, ERROR_NOT_SUPPORTED ); // not a TRDOS format
		// - attempting to read as TRDOS 5.0x Image
		if (f.m_hFile!=CFile::hFileNull){ // handle doesn't exist if creating a new Image
			// . rewinding to the beginning to reload the content of the Image's underlying file
			f.SeekToBegin();
			// . reading TRDOS 5.0x Image
			TSclHeader sclHeader;
			if (f.Read(&sclHeader,sizeof(sclHeader))==sizeof(sclHeader) && !::memcmp(sclHeader.id,HEADER_SINCLAIR,sizeof(sclHeader.id)) && sclHeader.nFiles<=TRDOS503_FILE_COUNT_MAX){
				// recognized as TRDOS 5.0x Image
				// : destroying any previous attempt to set Medium's geometry (e.g. during automatic recognition of suitable DOS to open this Image with)
				while (TCylinder n=GetCylinderCount())
					UnformatTrack(--n,0);
				// : instantiating the TRDOS with the highest priority in the recognition sequence
				std::unique_ptr<CTRDOS503> pTrdos=nullptr; // assumption (no TR-DOS participates in the recognition sequence)
				const CDos::CRecognition recognition;
				for( POSITION pos=recognition.GetFirstRecognizedDosPosition(); pos; ){
					const CDos::PCProperties p=recognition.GetNextRecognizedDos(pos);
					if (!::memcmp(p->name,TRDOS_NAME_BASE,sizeof(TRDOS_NAME_BASE)-1)){
						const TFormat fmt={
							Medium::FLOPPY_DD, Codec::MFM, 80,
							explicitSides ? GetHeadCount() : pFormat->nHeads,
							TRDOS503_TRACK_SECTORS_COUNT, TRDOS503_SECTOR_LENGTH_STD_CODE,TRDOS503_SECTOR_LENGTH_STD, 1
						};
						pTrdos.reset( (CTRDOS503 *)p->fnInstantiate(this,&fmt) );
						break;
					}
				}
				if (!pTrdos) // no TR-DOS participates in the recognition sequence ...
					return ERROR_UNRECOGNIZED_VOLUME; // ... hence can never recognize this Image regardless it's clear it's a TR-DOS Image
				pTrdos->zeroLengthFilesEnabled=true;
				pTrdos->importToSysTrack=false;
				::memcpy( pTrdos->sideMap, sideMap, sizeof(pTrdos->sideMap) ); // overwriting the SideMap to make sure it complies with the one provided as input
				// : formatting Image to the maximum possible capacity
				TStdWinError err=ExtendToNumberOfCylinders( pTrdos->formatBoot.nCylinders, pTrdos->properties->sectorFillerByte, false );
				if (err!=ERROR_SUCCESS){
error:				f.Close();
					::SetLastError(err);
					return err;
				}
				// : initializing empty TRDOS Image
				const Utils::CVarTempReset<bool> wp0( writeProtected, false );
				pTrdos->InitializeEmptyMedium( nullptr, CActionProgress::None );
				// : reading SCL Directory
				TSclDirectoryItem directory[TRDOS503_FILE_COUNT_MAX],*pdi=directory;
				const WORD nBytesOfDirectory=sclHeader.nFiles*sizeof(TSclDirectoryItem);
				if (f.Read(directory,nBytesOfDirectory)!=nBytesOfDirectory)
					goto error;
				// : importing TRDOS Files under temporary names (their substitution below)
				for( BYTE n=sclHeader.nFiles; n--; pdi++ ){
					TCHAR tmpName[MAX_PATH];
					::wsprintf( tmpName, _T("%d.%c"), n, CTRDOS503::TDirectoryEntry::BLOCK );
					CDos::PFile file;
					err=pTrdos->ImportFile( &f, pdi->nSectors*TRDOS503_SECTOR_LENGTH_STD, tmpName, 0, file );
					if (err!=ERROR_SUCCESS)
						goto error;
					*(TSclDirectoryItem *)file=*pdi;
				}
				// : making sure the proper disposal of the DOS isn't blocked by this method
				//TODO: This can be removed once all Views that work only with the Image (e.g. TrackMap) are members of the Image, not of the DOS
				this->locker.Unlock();
					pTrdos.reset();
				this->locker.Lock();
			}else
				// not recognized as TRDOS 5.0x Image
				return Utils::ErrorByOs( ERROR_VHD_FORMAT_UNKNOWN, ERROR_UNRECOGNIZED_MEDIA );
		}
		// - Format and MediumType set successfully
		return ERROR_SUCCESS;
	}
