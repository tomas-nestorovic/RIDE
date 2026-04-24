#pragma once

namespace Sector
{
	typedef BYTE N; // index or count
	typedef BYTE *PData;
	typedef const BYTE *PCData;
	typedef WORD L,*PL; // length
	typedef BYTE LC; // length code

	#pragma pack(1)
	typedef struct TId sealed{
		static const TId Invalid;

		static N CountAppearances(const TId *ids,N nIds,const TId &id);
		static CString List(const TId *ids,N nIds,N iHighlight=-1,char highlightBullet='\0');

		TCylinder cylinder;
		TSide side;
		N sector;
		LC lengthCode;

		bool operator==(const TId &id2) const;
		inline bool operator!=(const TId &id2) const{ return !operator==(id2); }
		inline operator bool() const{ return !operator==(Invalid); }
		TId &operator=(const FD_ID_HEADER &rih);
		TId &operator=(const FD_TIMED_ID_HEADER &rtih);

		CString ToString() const;
	} *PId;

	#pragma pack(1)
	struct TPhysicalAddress{
		static const TPhysicalAddress Invalid;

		inline static Track::N GetTrackNumber(TCylinder cyl,THead head,THead nHeads){ return cyl*nHeads+head; }

		TCylinder cylinder;
		THead head;
		TId sectorId;

		inline operator bool() const{ return *this!=Invalid; }
		bool operator==(const TPhysicalAddress &chs2) const;
		inline bool operator!=(const TPhysicalAddress &chs2) const{ return !operator==(chs2); }
		Track::N GetTrackNumber() const;
		Track::N GetTrackNumber(THead nHeads) const;
		CString GetTrackIdDesc(THead nHeads=0) const;
		inline bool IsValid() const{ return *this; }
		inline void Invalidate(){ *this=Invalid; }
	};

	enum TDataStatus{
		NOT_READY	=0,		// querying data via CImage::GetTrackData may lead to delay (e.g. application freezes if called from main thread)
		READY		=1,		// erroneous data are buffered, there will be no delay in calling CImage::GetTrackData
		READY_HEALTHY=READY|2 // healthy data are buffered, there will be no delay in calling CImage::GetTrackData
	};




	#define FDC_ST1_END_OF_CYLINDER		128
			// ^ FDC tried to access a sector beyond the final sector of the track (255D*).  Will be set if TC is not issued after Read or Write Data command
	#define FDC_ST1_DATA_ERROR			32
			// ^ FDC detected a CRC error in either the ID field or the data field of a sector
	#define FDC_ST1_NO_DATA				4
			// ^ (1) "Read Data" or "Read Deleted Data" commands - FDC did not find the specified sector; may also occur in "Read ID" and "Read Track" commands, but that's not important in scope of this application
			//	 (2) "Read ID" command - FDC cannot read the ID field without an error
			//	 (3) "Read Track" command - FDC cannot find the proper sector sequence
	#define FDC_ST1_NO_ADDRESS_MARK		1
			// ^ (1) The FDC did not detect an ID address mark at the specified track after encountering the index pulse from the IDX pin twice
			//	 (2) The FDC cannot detect a data address mark or a deleted data address mark on the specified track


	#define FDC_ST2_DELETED_DAM			64
			// ^ (1) "Read Data" command - FDC encountered a deleted data address mark
			//	 (2) "Read Deleted Data" command - the FDC encountered a data address mark
	#define FDC_ST2_CRC_ERROR_IN_DATA	32
			// ^ FDC detected a CRC error in the data field
	#define FDC_ST2_NOT_DAM				1
			// ^ The FDC cannot detect a data address mark or a deleted data address mark

	#pragma pack(1)
	struct TFdcStatus sealed{
		static const TFdcStatus Unknown; // e.g. when Sector not yet attempted for reading
		static const TFdcStatus WithoutError;
		static const TFdcStatus SectorNotFound;
		static const TFdcStatus IdFieldCrcError;
		static const TFdcStatus DataFieldCrcError;
		static const TFdcStatus NoDataField;
		static const TFdcStatus DeletedDam;

		union{
			struct{
				BYTE reg1,reg2;
			};
			WORD w;
		};

		inline TFdcStatus() : w(0) {}
		inline TFdcStatus(WORD w) : w(w) {}
		TFdcStatus(BYTE _reg1,BYTE _reg2);

		inline operator WORD() const{ return w; }
		inline bool operator==(const WORD st) const{ return w==st; }

		WORD GetSeverity(WORD mask=-1) const;
		inline void ExtendWith(const WORD st){ w|=st; }
		void GetDescriptionsOfSetBits(LPCTSTR *pDescriptions) const;
		inline bool IsWithoutError() const{ static_assert(sizeof(*this)==sizeof(WORD),""); return *(PCWORD)this==0; } // True <=> Registers don't describe any error, otherwise False
		bool DescribesIdFieldCrcError() const;
		void CancelIdFieldCrcError();
		bool DescribesDataFieldCrcError() const;
		void CancelDataFieldCrcError();
		bool DescribesDeletedDam() const;
		bool DescribesMissingId() const; // aka. Sector not found
		bool DescribesMissingDam() const;
	};




	class CReaderWriter abstract:public CHexaEditor::CYahelStreamFile,public Yahel::Stream::IAdvisor{
	public:
		typedef void (* FOnWritten)(const Yahel::TPosInterval &);

		class CHexaEditor:public ::CHexaEditor{
			int GetCustomCommandMenuFlags(WORD cmd) const override;
			bool ProcessCustomCommand(UINT cmd) override;
			LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
		public:
			CHexaEditor(PVOID param);
		};
	private:
		const FOnWritten onWritten;
	protected:
		static const Yahel::TInterval<char> NoPadding;

		Revolution::TType revolution;
		struct:public TPhysicalAddress{ // call 'Seek' to modify this structure
			Yahel::TInterval<char> padding; // respectively at the beginning (NEGATIVE!) and end of EACH Sector, regardless of Sector size; e.g. (-2,1) = two padding Bytes at the start and one padding Byte at the end of EACH Sector
			N indexOnTrack; // zero-based index of the Sector on the Track (to distinguish among duplicate-ID Sectors)
			L offset; // pointer to Sector data (always holds 'padding.a<=offset')
		} sector; // call 'Seek' to modify this structure
		Bit::TFlags badByteMask;

		CReaderWriter(PImage image,Yahel::TPosition dataTotalLength,const Yahel::TInterval<char> &padding,const TRev &nDiscoveredRevolutions,FOnWritten onWritten);
	public:
		typedef ATL::CComPtr<CReaderWriter> CComPtr;

		enum TScannerStatus:BYTE{
			RUNNING, // Track scanner exists and is running (e.g. parallel thread that scans Tracks on real FDD)
			PAUSED, // Track scanner exists but is suspended (same example as above)
			UNAVAILABLE // Track scanner doesn't exist (e.g. a CImageRaw descendant)
		};

		const PImage image;
		const TRev &nDiscoveredRevolutions;

		// CFile methods
	#if _MFC_VER>=0x0A00
		ULONGLONG Seek(LONGLONG lOff,UINT nFrom) override sealed;
	#else
		LONG Seek(LONG lOff,UINT nFrom) override sealed;
	#endif
		UINT Read(LPVOID lpBuf,UINT nCount) override sealed;
		void Write(LPCVOID lpBuf,UINT nCount) override sealed;

		// Yahel::Stream::IAdvisor methods
		LPCWSTR GetRecordLabelW(Yahel::TPosition pos,PWCHAR labelBuffer,BYTE labelBufferCharsMax,PVOID param) const override;

		// other
		inline N GetCurrentSectorIndexOnTrack() const{ return sector.indexOnTrack; } // returns the zero-based index of current Sector on the Track
		inline L GetPositionInCurrentSector() const{ return sector.offset; }
		inline const TPhysicalAddress &GetCurrentPhysicalAddress() const{ return sector; }
		TRev GetAvailableRevolutionCount(TCylinder cyl,THead head) const;
		inline Revolution::TType GetCurrentRevolution() const{ return revolution; }
		inline void SetCurrentRevolution(Revolution::TType rev){ revolution=rev; }
		virtual Yahel::TPosition GetSectorStartPosition(const TPhysicalAddress &chs,N nSectorsToSkip) const=0;
		virtual TScannerStatus GetTrackScannerStatus(PCylinder pnOutScannedCyls=nullptr) const;
		virtual void SetTrackScannerStatus(TScannerStatus status);
		virtual void GetPhysicalAddress(Yahel::TPosition pos,TPhysicalAddress &outChs,N &outSectorIndex,PL pOutOffset) const=0;
		TPhysicalAddress GetPhysicalAddress(Yahel::TPosition pos) const;
	};

	struct TSameLengthParams{
		N nSectors, firstSectorNumber;
		L sectorLength;
		LC sectorLengthCode;

		inline TSameLengthParams()
			: nSectors(0) {
		}
		inline TSameLengthParams(N nSectors,L sectorLength)
			: nSectors(nSectors) , sectorLength(sectorLength) {
		}
	};

	class CSameLengthReaderWriter abstract:public CReaderWriter,protected TSameLengthParams{
	protected:
		const L usableSectorLength;

		CSameLengthReaderWriter(PImage image,Yahel::TPosition dataTotalLength,const Yahel::TInterval<char> &padding,const TRev &nDiscoveredRevolutions,FOnWritten onWritten,const TSameLengthParams &slsp);
	public:
		// Yahel::Stream::IAdvisor methods
		Yahel::TRow LogicalPositionToRow(Yahel::TPosition logPos,WORD nBytesInRow) override;
		Yahel::TPosition RowToLogicalPosition(Yahel::TRow row,WORD nBytesInRow) override;
		void GetRecordInfo(Yahel::TPosition logPos,Yahel::PPosition pOutRecordStartLogPos,Yahel::PPosition pOutRecordLength,bool *pOutDataReady) override;

		// other
		Yahel::TPosition GetSectorStartPosition(const TPhysicalAddress &chs,N nSectorsToSkip) const override;
		void GetPhysicalAddress(Yahel::TPosition pos,TPhysicalAddress &outChs,N &outSectorIndex,PL pOutOffset) const override;
	};



	L GetLength(LC lengthCode);
	LC GetLengthCode(L length);

}

typedef Sector::N TSector,*PSector;
typedef const Sector::N *PCSector;
typedef Sector::PData PSectorData;
typedef Sector::PCData PCSectorData;
typedef Sector::TId TSectorId,*PSectorId;
typedef const Sector::TId *PCSectorId,&RCSectorId;
typedef Sector::TPhysicalAddress TPhysicalAddress;
typedef const Sector::TPhysicalAddress &RCPhysicalAddress;
typedef Sector::TDataStatus TDataStatus;
typedef Sector::TFdcStatus TFdcStatus;
typedef const Sector::TFdcStatus *PCFdcStatus;
