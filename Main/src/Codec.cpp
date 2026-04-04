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





	TLimits::TLimits(TLogTime iwTimeDefault,BYTE iwTimeTolerancePercent)
		// ctor
		: iwTimeDefault(iwTimeDefault)
		, iwTime(iwTimeDefault)
		, iwTimeMin( iwTimeDefault*(100-iwTimeTolerancePercent)/100 )
		, iwTimeMax( iwTimeDefault*(100+iwTimeTolerancePercent)/100 ) {
	}

	void TLimits::ClampIwTime(){
		// keep the inspection window size within limits
		if (iwTime<iwTimeMin)
			iwTime=iwTimeMin;
		else if (iwTime>iwTimeMax)
			iwTime=iwTimeMax;
	}

}
