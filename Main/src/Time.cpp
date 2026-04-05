#include "stdafx.h"

namespace Time
{
	const TCHAR Prefixes[]=_T("nnnµµµmmm   "); // nano, micro, milli, no-prefix

	CTimeline::CTimeline(T logTimeLength,T logTimePerUnit,BYTE initZoomFactor)
		// ctor
		: Utils::CAxis( logTimeLength, logTimePerUnit, 's', Prefixes, initZoomFactor ) {
	}

}
