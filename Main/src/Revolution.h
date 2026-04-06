#pragma once

namespace Revolution
{
	typedef BYTE N; // count, e.g. Revolutions per second

	enum TType:N{
		// constants to address one particular Revolution
		R0			=0,
		R1, R2, R3, R4, R5, R6, R7,
		MAX,
		// constants to select Revolution
		CURRENT		=MAX,	// don't calibrate the head, settle with any (already buffered) Data, even erroneous
		NEXT,				// don't calibrate the head, settle with any (newly buffered) Data, even erroneous
		ANY_GOOD,			// do anything that doesn't involve the user to get flawless data
		NONE,
		// constants to describe quantities
		QUANTITY_FIRST,
		INFINITY,
		// the following constants should be ignored by all containers
		ALL_INTERSECTED
	};

}

typedef Revolution::N TRev,*PRev;
