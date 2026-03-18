#pragma once

namespace Bit
{
	struct TFlags{
		union{
			struct{
				BYTE value:1; // recognized 0 or 1 from the underlying low-level timing
				BYTE fuzzy:1; // the Value is likely different in each Revolution
				BYTE cosmeticFuzzy:1; // the Value is not wrong but should be displayed so for cosmetic reasons
				BYTE bad:1; // the Value is potentially wrong due to underlying low-level timing
			};
			BYTE flags;
		};

		inline operator BYTE() const{ return flags; }
		inline bool IsBad() const{ return bad; }
		inline void MarkHealthy(){ bad=false; }
	};

	struct TTimed abstract:public TFlags{
		TLogTime time;
	};

}
