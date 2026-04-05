#pragma once

namespace Sector
{
	typedef BYTE N; // index or count
	typedef BYTE *PData;
	typedef const BYTE *PCData;
	typedef WORD L; // length
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
		TId &operator=(const FD_ID_HEADER &rih);
		TId &operator=(const FD_TIMED_ID_HEADER &rtih);

		CString ToString() const;
	} *PId;

	#pragma pack(1)
	struct TPhysicalAddress{
		static const TPhysicalAddress Invalid;

		inline static TTrack GetTrackNumber(TCylinder cyl,THead head,THead nHeads){ return cyl*nHeads+head; }

		TCylinder cylinder;
		THead head;
		TId sectorId;

		inline operator bool() const{ return *this!=Invalid; }
		bool operator==(const TPhysicalAddress &chs2) const;
		inline bool operator!=(const TPhysicalAddress &chs2) const{ return !operator==(chs2); }
		TTrack GetTrackNumber() const;
		TTrack GetTrackNumber(THead nHeads) const;
		CString GetTrackIdDesc(THead nHeads=0) const;
		inline bool IsValid() const{ return *this; }
		inline void Invalidate(){ *this=Invalid; }
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
