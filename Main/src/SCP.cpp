#include "stdafx.h"
#include "CapsBase.h"
#include "SuperCardProBase.h"
#include "SCP.h"

	static LPCTSTR Recognize(PTCHAR){
		static constexpr TCHAR SingleDeviceName[]=_T("SuperCard Pro image\0");
		return SingleDeviceName;
	}
	static PImage Instantiate(LPCTSTR){
		return new CSCP;
	}

	const CImage::TProperties CSCP::Properties={
		MAKE_IMAGE_ID('S','C','P','i','m','a','g','e'), // a unique identifier
		Recognize,	// list of recognized device names
		Instantiate, // instantiation function
		_T("*.scp"), // filter
		(Medium::TType)Medium::FLOPPY_ANY, // supported Media
		Codec::ANY, // supported Codecs
		1,16384	// Sector supported min and max length
	};

	CSCP::CSCP()
		// ctor
		: CSuperCardProBase( &CSCP::Properties, '\0', INI_SUPERCARDPRO, Recognize(nullptr) ) { // '\0' = not a real drive
		Reset(); // initialize properly
	}







	BOOL CSCP::OnOpenDocument(LPCTSTR lpszPathName){
		// True <=> Image opened successfully, otherwise False
		// - base
		if (!__super::OnOpenDocument(nullptr) // don't involve CAPS in Image opening
			&&
			::GetLastError()!=ERROR_NOT_SUPPORTED // the CAPS library currently doesn't support reading Stream files
		)
			return FALSE;
		// - opening
		canBeModified=true; // assumption
		if (TStdWinError err=OpenImageForReadingAndWriting(lpszPathName,f)) // if cannot open for both reading and writing ...
			if ( err=OpenImageForReading(lpszPathName,f) ){ // ... trying to open at least for reading, and if neither this works ...
				::SetLastError(err);
				return FALSE; // ... the Image cannot be open in any way
			}else
				canBeModified=false;
		// - if data shorter than an empty Image, resetting to empty Image
		const WORD nHeaderBytesRead=f.Read(&header,sizeof(header));
		if (!app.IsInGodMode()) // must follow the rules?
			canBeModified&=header.flags.modifiable;
		if (!nHeaderBytesRead){
			Reset(); // to correctly initialize using current Parameters
			return TRUE;
		}else if (nHeaderBytesRead<sizeof(header)){
formatError: ::SetLastError(ERROR_BAD_FORMAT);
			return FALSE;
		}
		// - reading content of the Image file and continuously validating its structure
		if (!header.IsValid()){
			::SetLastError(ERROR_INVALID_DATA);
			return FALSE;
		}
		if (!header.IsSupported()){
			::SetLastError(ERROR_NOT_SUPPORTED);
			return FALSE;
		}
		preservationQuality=!header.flags.normalized;
		if (header.flags.extended)
			f.Seek( 0x70, CFile::current );
		if (f.Read( tdhOffsets, sizeof(tdhOffsets) )!=sizeof(tdhOffsets))
			goto formatError;
		// - setting a classical 3.5" floppy geometry
		if (!capsImageInfo.maxcylinder) // # of Cylinders not yet set (e.g. via confirmed EditSettings dialog)
			capsImageInfo.maxcylinder=FDD_CYLINDERS_MAX-1; // inclusive!
		capsImageInfo.maxhead=2-1; // inclusive!
		// - confirming initial settings
		if (!EditSettings(true)){ // dialog cancelled?
			::SetLastError( ERROR_CANCELLED );
			return FALSE;
		}
		header.flags.normalized|=params.corrections.valid&&params.corrections.use;
		// - warning on unsupported Cylinders
		WarnOnAndCorrectExceedingCylinders();
		return TRUE;
	}

	CCapsBase::PInternalTrack CCapsBase::GetInternalTrackSafe(TCylinder cyl,THead head) const{
		return	cyl<FDD_CYLINDERS_MAX && head<2
				? internalTracks[cyl][head]
				: nullptr; // Cylinders beyond FDD_CYLINDERS_MAX are inaccessible
	}

	CImage::CTrackReader CSCP::ReadTrack(TCylinder cyl,THead head) const{
		// creates and returns a general description of the specified Track, represented using neutral LogicalTimes
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - checking that specified Track actually CAN exist
		if (cyl>capsImageInfo.maxcylinder || head>capsImageInfo.maxhead)
			return CTrackReaderWriter::Invalid;
		// - if Track already read before, returning the result from before
		PInternalTrack &rit=internalTracks[cyl][head];
		if (GetInternalTrackSafe(cyl,head)!=nullptr)
			return *rit;
		// - construction of InternalTrack
		const BYTE cylFile=cyl<<(BYTE)params.doubleTrackStep;
		if (!tdhOffsets[cylFile][head]) // maybe a hardware error during Image creation?
			return CTrackReaderWriter::Invalid;
		f.Seek( tdhOffsets[cylFile][head], CFile::begin );
		if (CTrackReaderWriter trw=StreamToTrack( f, cylFile, head )){
			// it's a SuperCardPro Track
			if (head && params.flippyDisk)
				trw.Reverse();
			rit=CInternalTrack::CreateFrom( *this, trw );
			return *rit;
		}
		return CTrackReaderWriter::Invalid;
	}

	TStdWinError CSCP::Reset(){
		// resets internal representation of the disk (e.g. by disposing all content without warning)
		EXCLUSIVELY_LOCK_THIS_IMAGE();
		// - base
		if (const TStdWinError err=__super::Reset())
			return err;
		// - reinitializing to an empty Image
		header=THeader();
		::ZeroMemory( tdhOffsets, sizeof(tdhOffsets) );
		return ERROR_SUCCESS;
	}

	TStdWinError CSCP::SaveTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const{
		// saves the specified Track to the inserted Medium; returns Windows standard i/o error
		return ERROR_NOT_SUPPORTED; // individual Track saving is not supported for this kind of Image (OnSaveDocument must be called instead)
	}

	TStdWinError CSCP::SaveAllModifiedTracks(LPCTSTR lpszPathName,CActionProgress &ap){
		// saves all Modified Tracks; returns Windows standard i/o error
		CFile fTmp;
		const bool savingToCurrentFile= lpszPathName==f.GetFilePath() && f.m_hFile!=CFile::hFileNull && ::GetFileAttributes(lpszPathName)!=INVALID_FILE_ATTRIBUTES; // saving to the same file and that file exists (handle doesn't exist when creating new Image)
		if (!savingToCurrentFile)
			if (const TStdWinError err=CreateImageForWriting(lpszPathName,fTmp))
				return err;
		ap.SetProgressTarget(4000);
		const Medium::PCProperties mp=Medium::GetProperties( floppyType );
		// - creating ContentLayout map of the file in which UNMODIFIED occupied space is represented by positive numbers, whereas gaps with negative numbers
		typedef std::map<DWORD,LONG> CContentLayout;
		CContentLayout contentLayout; // key = position in file, value>0 = Track length, value<0 = unused gap size
		if (savingToCurrentFile){
			// . adding unmodified Tracks to ContentLayout as "occupied" (value>0)
			for( TCylinder cylFile=ARRAYSIZE(tdhOffsets); cylFile-->0; )
				for( THead head=2; head>0; )
					if (const DWORD tdhOffset=tdhOffsets[cylFile][--head]){ // Track actually exists in the file?
						const TCylinder cyl=cylFile>>(BYTE)params.doubleTrackStep;
						const PCInternalTrack pit=GetInternalTrackSafe(cyl,head);
						if (!pit || !pit->modified){ // not Modified or not even read Track?
							TTrackDataHeader tdh(0);
							f.Seek( tdhOffset, CFile::begin );
							if (!tdh.Read( f, cylFile, head, header.nAvailableRevolutions ))
								return ERROR_INVALID_BLOCK; // read invalid TrackDataHeader structure
							if (const DWORD capacity=tdh.GetFullTrackCapacityInBytes( mp, header ))
								contentLayout.insert(
									std::make_pair( tdhOffset, capacity )
								);
							else
								return ERROR_NOT_SUPPORTED; // likely bad information in Header! (e.g. nFluxCellBits)
						}
					}
			// . adding gaps (value<0)
			CContentLayout gaps;
			DWORD prevTrackEnd=sizeof(header)+header.flags.extended*0x70+sizeof(tdhOffsets); // first Track potential position in the file
			for each( const auto &kvp in contentLayout ){
				if (prevTrackEnd<kvp.first) // gap in the file?
					gaps.insert(  std::make_pair( prevTrackEnd, prevTrackEnd-kvp.first )  );
				prevTrackEnd=kvp.first+kvp.second;
			}
			contentLayout.insert( gaps.cbegin(), gaps.cend() );
		}
		// - saving
		CFile &fTarget= savingToCurrentFile ? f : fTmp;
		if (!savingToCurrentFile)
			fTarget.SetLength( sizeof(header)+header.flags.extended*0x70+sizeof(tdhOffsets) );
		if (const auto buffer=Utils::MakeCallocPtr<BYTE>(SCP_BUFFER_CAPACITY)){
			auto sub=ap.CreateSubactionProgress( 2000, sizeof(tdhOffsets)/sizeof(DWORD) );
			for( TCylinder cylFile=0; cylFile<ARRAYSIZE(tdhOffsets); cylFile++ ){
				const TCylinder cyl=cylFile>>(BYTE)params.doubleTrackStep;
				bool bStreamAdjusted;
				for( THead head=0; head<2; ap.UpdateProgress(++head+cylFile*2) ){
					const auto fTargetLength=fTarget.GetLength();
					if (savingToCurrentFile){
						// modifying existing file
						if (const PInternalTrack pit=GetInternalTrackSafe(cyl,head)) // Track read?
							if (pit->modified){ // and Modified?
								pit->FlushSectorBuffers(); // convert all modifications into flux transitions
								if (const DWORD trackLength=TrackToStream( *pit, CMemFile(buffer,SCP_BUFFER_CAPACITY), cylFile, head, bStreamAdjusted )){
									// conversion of the Track to SuperCardPro stream succeeded?
									const LONG capacity=((TTrackDataHeader *)buffer.get())->GetFullTrackCapacityInBytes( mp, header );
									if (GetCurrentDiskFreeSpace()<capacity)
										return ERROR_DISK_FULL;
									// . success thanks to adjustments to overcome SuperCard Pro limitations?
									header.flags.normalized|=bStreamAdjusted;
									// . search for sufficiently big gap
									tdhOffsets[cylFile][head]=fTargetLength; // assumption (no gap big enough to contain this Track, so must extend the file)
									for( auto it=contentLayout.begin(); it!=contentLayout.end(); it++ )
										if (it->second<0 && it->second<=-capacity){ // a gap that can contain the Track
											if (it->second<-capacity) // gap not yet entirely filled?
												contentLayout.insert( // shrunk new gap
													std::make_pair( it->first+capacity, it->second+capacity )
												);
											tdhOffsets[cylFile][head]=it->first; // write Track here in the file
											it->second=capacity; // this gap is now the Track
											break;
										}
									// . saving the Track to the file
									fTarget.SetLength( // make reserve so that the Track can expand upon different data
										std::max<DWORD>( fTargetLength, tdhOffsets[cylFile][head]+capacity )
									);
									fTarget.Seek( tdhOffsets[cylFile][head], CFile::begin );
									fTarget.Write( buffer, trackLength );
								}else
									return ERROR_BAD_COMMAND;
							}
					}else{
						// creating a brand new file
						const PInternalTrack pit=GetInternalTrackSafe(cyl,head);
						if (pit && pit->modified){ // Track Modified?
							pit->FlushSectorBuffers(); // convert all modifications into flux transitions
							if (const DWORD trackLength=TrackToStream( *pit, CMemFile(buffer,SCP_BUFFER_CAPACITY), cylFile, head, bStreamAdjusted )){
								// conversion of the Track to SuperCardPro stream succeeded?
								const DWORD capacity=((TTrackDataHeader *)buffer.get())->GetFullTrackCapacityInBytes( mp, header );
								if (GetCurrentDiskFreeSpace()<capacity)
									return ERROR_DISK_FULL;
								// . success thanks to adjustments to overcome SuperCard Pro limitations?
								header.flags.normalized|=bStreamAdjusted;
								// . append Track to file
								fTarget.SetLength( // make reserve so that the Track can expand upon different data
									fTargetLength + capacity
								);
								fTarget.Seek(
									tdhOffsets[cylFile][head] = fTargetLength,
									CFile::begin
								);
								fTarget.Write( buffer, trackLength );
							}else
								return ERROR_BAD_COMMAND;
						}else if (const DWORD tdhOffset=tdhOffsets[cylFile][head]){ // Track actually exists in the original file?
							TTrackDataHeader tdh(0);
							f.Seek( tdhOffset, CFile::begin );
							if (!tdh.Read( f, cylFile, head, header.nAvailableRevolutions ))
								return ERROR_INVALID_BLOCK; // read invalid TrackDataHeader structure
							f.Seek( tdhOffset, CFile::begin ); // seek back to the TrackDataHeader
							const DWORD capacity=tdh.GetFullTrackCapacityInBytes( mp, header );
							if (GetCurrentDiskFreeSpace()<capacity)
								return ERROR_DISK_FULL;
							fTarget.SetLength( // make reserve so that the Track can expand upon different data
								fTargetLength + capacity
							);
							fTarget.Seek( // append Track to file
								tdhOffsets[cylFile][head] = fTargetLength,
								CFile::begin
							);
							fTarget.Write(
								buffer,
								f.Read( buffer, tdh.GetFullTrackLengthInBytes(header) )
							);
						}
					}
				}
			}
		}else
			return ERROR_NOT_ENOUGH_MEMORY;
		// - consolidating/defragmenting the file
		/*if (savingToCurrentFile){ //TODO
			auto sub=ap.CreateSubactionProgress( 2000, f.GetLength() );
			//TODO
			f.SetLength(f.GetPosition()); // "trimming" eventual unnecessary data (e.g. when unformatting Cylinders)
		}*/
		// - save Offsets
		fTarget.Seek( sizeof(header)+header.flags.extended*0x70, CFile::begin );
		if (GetCurrentDiskFreeSpace()<sizeof(tdhOffsets))
			return ERROR_DISK_FULL;
		fTarget.Write( tdhOffsets, sizeof(tdhOffsets) );
		// - update Header
		header.signatureAndRevision=THeader().signatureAndRevision;
		header.firstTrack=0;
		header.lastTrack=FDD_CYLINDERS_MAX*2-1; // inclusive!
		header.flags.tpi96=!params.doubleTrackStep;
		switch (floppyType){
			case Medium::FLOPPY_DD: // 3.5" or 5.25" 2DD floppy in 300 RPM drive
				header.deviceDiskType=THeader::OTHER_FLOPPY_DD;
				header.flags.rpm360=false;
				break;
			case Medium::FLOPPY_HD_525:
				header.deviceDiskType=THeader::OTHER_FLOPPY_HD_525;
				header.flags.rpm360=true;
				break;
			case Medium::FLOPPY_HD_350:
				header.deviceDiskType=THeader::OTHER_FLOPPY_HD_350;
				header.flags.rpm360=false;
				break;
			case Medium::FLOPPY_DD_525: // 5.25" HD floppy in 360 RPM drive
				header.deviceDiskType=THeader::OTHER_FLOPPY_DD;
				header.flags.rpm360=true;
				break;
			default:
				ASSERT(FALSE);
				break;
		}
		header.flags.footerPresent=false; // discard any footer
		header.flags.createdUsingScp=false;
		BYTE nTracks[2]; // for Head 0 and 1, respectively
		for( TCylinder cylFile=ARRAYSIZE(tdhOffsets); cylFile-->0; )
			for( THead head=2; head-->0; nTracks[head]+=tdhOffsets[cylFile][head]!=0 );
		if (nTracks[0]>0 && nTracks[1]>0)
			header.heads=THeader::BOTH_HEADS;
		else
			header.heads= nTracks[1]>0 ? THeader::HEAD_1_ONLY : THeader::HEAD_0_ONLY;
		header.checksum=0;
		if (header.flags.modifiable){
			fTarget.Seek( 0x10, CFile::begin );
			do{
				BYTE checksumBuffer[65536];
				for( auto nBytesRead=fTarget.Read(checksumBuffer,sizeof(checksumBuffer)); nBytesRead>0; header.checksum+=checksumBuffer[--nBytesRead] );
			}while (fTarget.GetPosition()<fTarget.GetLength());
		}
		fTarget.SeekToBegin();
		fTarget.Write( &header, sizeof(header) ); // no need to test free space on disk - we would already fail somewhere above
		m_bModified=FALSE;
		// - reopening Image's underlying file
		if (f.m_hFile!=CFile::hFileNull)
			f.Close();
		if (fTmp.m_hFile!=CFile::hFileNull)
			fTmp.Close();
		return OpenImageForReadingAndWriting(lpszPathName,f);
	}
