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
