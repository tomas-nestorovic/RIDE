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




	namespace Impl
	{
		namespace MFM
		{
			enum{
				CodedByteWidth=16
			};
			enum:Checksum::W{
				CRC_A1A1A1=0xcdb4 // CRC of 0xa1, 0xa1, 0xa1
			};

			extern bool g_prevDataBit;

			WORD EncodeByte(BYTE byte);
			DWORD EncodeWord(WORD w); // big-endian Word assumed

			BYTE DecodeByte(WORD w);
			WORD DecodeWord(DWORD dw);
		}
	}

}
