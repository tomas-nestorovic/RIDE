#include "stdafx.h"
#include "TRDOS.h"

	static PImage __instantiate__(){
		return new CSCL;
	}

	const CImage::TProperties CSCL::Properties={_T("SCL image"),// name
												__instantiate__,// instantiation function
												_T("*.scl"),	// filter
												TMedium::FLOPPY_DD, // supported Media
												TRDOS503_SECTOR_LENGTH_STD,TRDOS503_SECTOR_LENGTH_STD	// Sector supported min and max length
											};






	

	CSCL::CSCL()
		// ctor
		: CImageRaw(&Properties) {
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

	BOOL CSCL::OnSaveDocument(LPCTSTR lpszPathName){
		// True <=> this Image has been successfully saved, otherwise False
		if (const CTRDOS503::PCBootSector boot=CTRDOS503::__getBootSector__(this)){
			if (f.m_hFile!=(UINT_PTR)INVALID_HANDLE_VALUE) // Image's underlying file doesn't exist if saving a fresh formatted Image
				f.Close();
			if (!__openImageForWriting__(lpszPathName,&f))
				return FALSE;
			// - saving Header
			TSclHeader header;
			::lstrcpyA( header.id, HEADER_SINCLAIR );
			header.nFiles=boot->nFiles;
			f.Write(&header,sizeof(header));
			// - instantiating temporary TRDOS
			const CTRDOS503 tmpTrdos(this);
			// - saving Directory
			CTRDOS503::PDirectoryEntry directory[TRDOS503_FILE_COUNT_MAX],*pde=directory;
			for( BYTE n=tmpTrdos.__getDirectory__(directory); n--; f.Write(*pde++,sizeof(TSclDirectoryItem)) );
			// - saving each File's data
			BYTE buf[65536];
			for( pde=directory; header.nFiles--; )
				f.Write(buf,
						tmpTrdos.ExportFile( *pde++, &CMemFile(buf,sizeof(buf)), -1, NULL )
					);
			m_bModified=FALSE;
			// - reopening the Image's underlying file
			f.Close();
			return __openImageForReadingAndWriting__(lpszPathName);
		}else
			return FALSE;
	}

	TStdWinError CSCL::SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber){
		// sets the given MediumType and its geometry; returns Windows standard i/o error
		// - base (allowed are only TRDOS-compliant formats)
		if (pFormat->nSectors==TRDOS503_TRACK_SECTORS_COUNT && pFormat->sectorLength==TRDOS503_SECTOR_LENGTH_STD && pFormat->sectorLengthCode==TRDOS503_SECTOR_LENGTH_STD_CODE){
			const TStdWinError err=CImageRaw::SetMediumTypeAndGeometry(pFormat,sideMap,firstSectorNumber);
			if (err!=ERROR_SUCCESS)
				return err;
		}else
			return ERROR_BAD_COMMAND; // not a TRDOS format
		// - attempting to read as TRDOS 5.03 Image
		if (f.m_hFile!=(UINT_PTR)INVALID_HANDLE_VALUE){ // handle doesn't exist if creating a new Image
			// . rewinding to the beginning to reload the content of the Image's underlying file
			f.SeekToBegin();
			// . reading TRDOS 5.03 Image
			TSclHeader sclHeader;
			if (f.Read(&sclHeader,sizeof(sclHeader))==sizeof(sclHeader) && !::memcmp(sclHeader.id,HEADER_SINCLAIR,sizeof(sclHeader.id)) && sclHeader.nFiles<=TRDOS503_FILE_COUNT_MAX){
				// recognized as TRDOS 5.03 Image
				// : instantiating TRDOS 5.03
				CTRDOS503 tmpTrdos(this);
				::memcpy( tmpTrdos.sideMap, sideMap, sizeof(tmpTrdos.sideMap) ); // overwriting the SideMap to make sure it complies with the one provided as input
				// : destroying any previous attempt to set Medium's geometry (e.g. during automatic recognition of suitable DOS to open this Image with)
				while (TCylinder n=GetCylinderCount())
					UnformatTrack(--n,0);
				// : formatting Image to the maximum possible capacity
				TStdWinError err=__extendToNumberOfCylinders__( tmpTrdos.formatBoot.nCylinders, tmpTrdos.properties->sectorFillerByte );
				if (err!=ERROR_SUCCESS){
error:				f.Close();
					::SetLastError(err);
					return err;
				}
				// : initializing empty TRDOS Image
				tmpTrdos.InitializeEmptyMedium(NULL);
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
					err=tmpTrdos.ImportFile( &f, pdi->nSectors*TRDOS503_SECTOR_LENGTH_STD, tmpName, 0, file );
					if (err!=ERROR_SUCCESS)
						goto error;
					*(TSclDirectoryItem *)file=*pdi;
				}
			}else
				// not recognized as TRDOS 5.03 Image
				return ERROR_BAD_FORMAT;
		}
		// - Format and MediumType set successfully
		return ERROR_SUCCESS;
	}
