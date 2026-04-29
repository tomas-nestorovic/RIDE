#pragma once

namespace Bit
{
	struct TFlags{
		union{
			struct{
				BYTE value:1; // recognized 0 or 1 from the underlying low-level timing
				BYTE fuzzy:1; // the Value is likely different in each Revolution
				BYTE cosmeticFuzzy:1; // the Value is not different in each Revolution but should be displayed so for cosmetic reasons
				BYTE badTiming:1; // the Value is potentially wrong due to underlying low-level timing
				BYTE badEncoding:1; // the Value is potentially wrong due to underlying low-level encoding
				BYTE badCosmetic:1; // the Value is not wrong but should be displayed so for cosmetic reasons
			};
			BYTE flags;
		};

		inline operator BYTE() const{ return flags; }
		inline bool IsBad() const{ return badTiming||badEncoding||badCosmetic; }
		inline bool IsFuzzy() const{ return fuzzy||cosmeticFuzzy; }
		inline void MarkHealthy(){ badTiming=false, badEncoding=false, badCosmetic=false; }
	};

	struct TTimed abstract:public TFlags{
		TLogTime time;
	};

	typedef Utils::CSharedPodArray<CDiffBase::TScriptItem,N> CSharedDiffScript;

}
