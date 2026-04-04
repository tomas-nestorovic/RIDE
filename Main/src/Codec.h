#pragma once

namespace Codec
{
	typedef BYTE TTypeSet;

	typedef const struct TProperties sealed{
		LPCTSTR description;
		struct{
			BYTE d,k;
		} RLL; // Run-length Limited, https://en.wikipedia.org/wiki/Run-length_limited
	} *PCProperties;

	typedef enum TType:TTypeSet{
		UNKNOWN		=0,
		MFM			=1,
		FM			=2,
		//AMIGA		=4,
		//GCR		=8,
		FLOPPY_IBM	=/*FM|*/MFM,
		FLOPPY_ANY	=FLOPPY_IBM,//|AMIGA|GCR
		ANY			=FLOPPY_ANY
	} *PType;

	PCProperties GetProperties(TType codec);
	LPCTSTR GetDescription(TType codec);
	TType FirstFromMany(TTypeSet set);



	#pragma pack(1)
	struct TLimits{
		TLogTime iwTimeDefault; // inspection window default size
		TLogTime iwTime; // inspection window size; a "1" is expected in its centre
		TLogTime iwTimeMin,iwTimeMax; // inspection window possible time range

		TLimits(TLogTime iwTimeDefault,BYTE iwTimeTolerancePercent=0);

		void ClampIwTime();
		inline TLogTime PeekNextIwTime(TLogTime tIwCurr) const{ return tIwCurr+iwTime; }
	};

}
