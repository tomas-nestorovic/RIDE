#include "stdafx.h"

namespace Time
{
	const TLogTimeInterval TLogTimeInterval::Invalid( Infinity, INT_MIN );



	const TCHAR Prefixes[]=_T("nnnµµµmmm   "); // nano, micro, milli, no-prefix

	CTimeline::CTimeline(T logTimeLength,T logTimePerUnit,BYTE initZoomFactor)
		// ctor
		: Utils::CAxis( logTimeLength, logTimePerUnit, 's', Prefixes, initZoomFactor ) {
	}

}
