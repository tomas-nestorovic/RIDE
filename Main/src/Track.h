#pragma once

namespace Track
{
	typedef DWORD TTypeId;

	enum TOrder:BYTE{
		BY_CYLINDERS	=1,
		BY_HEADS		=2
	};


}

typedef Track::N TTrack,*PTrack;
typedef Track::TOrder TTrackScheme;
