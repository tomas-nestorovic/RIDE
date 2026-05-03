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




	class CSequence{
	public:
		typedef const struct TBit sealed:public TTimed{
			int uid; // unique identifier (unused by default, set by caller)

			inline bool operator==(const TBit &r) const{ return value==r.value; }
			inline TLogTime GetLength() const{ return this[1].time-time; }
		} *PCBit;
				
		template <typename T>
		struct TData sealed:public TTimed{
			T value; // '__super::value' now used to indicate validity of this Data

			inline TData(){ flags=0; } // initialized as invalid
			inline operator bool() const{ return __super::value!=0; }
			inline operator T() const{ return value; }
			inline void Validate(){ __super::value=1; }
		};
	private:
		Utils::CSharedPodArray<TBit,N> bitBuffer;
		TBit *pBits;
		N nBits;
	public:
		CSequence();
		CSequence(Track::CReader &tr,N nBitsFromCurrTime,BYTE oneOkPercent=0);
		CSequence(const CSequence &base,const TLogTimeInterval &ti);

		inline operator bool() const{ return nBits>0; }
		inline TBit &operator[](N i) const{ ASSERT(0<=i&&i<nBits); return pBits[i]; }
		inline N GetBitCount() const{ return nBits; }
		TData<WORD> GetWord(int i) const;
		PCBit Find(TLogTime t) const;
		PCBit FindOrNull(TLogTime t) const;
		CSharedDiffScript GetShortestEditScript(const CSequence &theirs,CActionProgress &ap) const;
		Time::CSharedColorIntervalArray ScriptToLocalDiffs(const CSharedDiffScript &script) const;
		Time::CSharedColorIntervalArray ScriptToLocalRegions(const CSharedDiffScript &script,COLORREF regionColor) const;
		void InheritFlagsFrom(const CSequence &theirs,const CSharedDiffScript &script) const;
		void OffsetAll(TLogTime dt) const;
	#ifdef _DEBUG
		void SaveCsv(LPCTSTR filename) const;
	#endif
		// 'for each' support
		inline TBit *begin() const{ return pBits; }
		inline TBit *end() const{ return pBits+nBits; }
	};
	
}
