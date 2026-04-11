#include "stdafx.h"

namespace Codec
{
	PCProperties GetProperties(TType codec){
		// returns Properties of a given Codec
		switch (codec){
			case FM:{
				static const TProperties P={
					_T("FM (Digital Frequency Modulation)"),
					{ 0, 1 }
				};
				return &P;
			}
			case MFM:{
				static const TProperties P={
					_T("MFM (Modified FM)"),
					{ 1, 3 }
				};
				return &P;
			}
			default:
				ASSERT(FALSE);
				return nullptr;
		}
	}

	LPCTSTR GetDescription(TType codec){
		// returns the string description of a given Codec
		if (const PCProperties p=GetProperties(codec))
			return p->description;
		else
			return nullptr;
	}

	TType FirstFromMany(TTypeSet set){
		// returns a Codec with the lowest Id in the input Set (or Unknown if Set empty)
		for( TTypeSet mask=1; mask!=0; mask<<=1 )
			if (set&mask)
				return (TType)mask;
		return UNKNOWN;
	}





	namespace Impl
	{
		namespace MFM
		{
			bool g_prevDataBit;

			static_assert( sizeof(Bit::TPattern)*CHAR_BIT>=CodedByteWidth, "" );

			WORD EncodeByte(BYTE byte){
				WORD result=0;
				for( WORD mask=0x8000; mask!=0; byte<<=1,mask>>=1 )
					if ((char)byte<0){ // current bit is a "1"
						mask>>=1; // clock is a "0"
						g_prevDataBit=true, result|=mask; // data is a "1"
					}else{ // current bit is a "0"
						result|=!g_prevDataBit*mask, mask>>=1; // insert "1" clock if previous data bit was a "0"
						g_prevDataBit=false; // data is a "0"
					}
				return result;
			}
			DWORD EncodeWord(WORD w){ // big-endian Word assumed
				const WORD high=EncodeByte( HIBYTE(w) );
				const WORD low =EncodeByte( LOBYTE(w) );
				return	MAKELONG( low, high );
			}

			BYTE DecodeByte(WORD w){
				BYTE result=0;
				for( BYTE n=8; n-->0; w<<=1,w<<=1 )
					result=(result<<1)|((w&0x4000)!=0);
				return result;
			}
			WORD DecodeWord(DWORD dw){
				WORD result=0;
				for( BYTE n=16; n-->0; dw<<=1,dw<<=1 )
					result=(result<<1)|((dw&0x40000000)!=0);
				return result;
			}
		}
	}

}
