#pragma once

namespace Time
{
	typedef DWORD N; // index or count

	typedef TLogValue T,*P; // time in nanoseconds
	typedef const T *PC;
	typedef Utils::CSharedPodArray<T,N> CSharedArray;

	enum{
		Invalid=INT_MIN,
		Infinity=LogValueMax
	};

	extern const TCHAR Prefixes[];
	extern T Ignore;

	struct TInterval{
		union{
			struct{
				T tStart; // inclusive
				T tEnd; // exclusive
			};
			T tArray[2];
		};

		static const TInterval Invalid;

		inline TInterval(){}
		inline TInterval(T tStart,T tEnd)
			: tStart(tStart) , tEnd(tEnd) {
		}

		inline operator bool() const{
			return tStart<tEnd; // non-empty?
		}
		inline T GetLength() const{
			return tEnd-tStart;
		}
		inline bool Contains(T t) const{
			return tStart<=t && t<tEnd;
		}
		inline TInterval Add(T dt) const{
			return TInterval( tStart+dt, tEnd+dt );
		}
		inline TInterval Inflate(T dt) const{
			return TInterval( tStart-dt, tEnd+dt );
		}
		inline TInterval Intersect(const TInterval &ti) const{
			return TInterval( std::max(tStart,ti.tStart), std::min(tEnd,ti.tEnd) );
		}
		inline TInterval Unite(const TInterval &ti) const{
			return TInterval( std::min(tStart,ti.tStart), std::max(tEnd,ti.tEnd) );
		}
		inline void Offset(T dt){
			tStart+=dt, tEnd+=dt;
		}
	};

	typedef const struct TMetaDataItem sealed:public TInterval{
		Bit::N nBits; // 0 = no explicit # of bits, use DPLL algorithm to adjust next IW size
		bool isFuzzy;

		TMetaDataItem(const TInterval &ti); // to clear MetaData in specified Interval
		TMetaDataItem(const TInterval &ti,bool isFuzzy,Bit::N nBits);

		inline bool operator<(const TMetaDataItem &r) const{ return tStart<r.tStart; }
		inline bool IsDefault() const{ return nBits<=0; }
		inline T GetStartTimeSafe() const{ return this?tStart:0; }
		T GetBitTimeAvg() const;
		T GetBitTime(Bit::N iBit) const;
		Bit::N GetBitIndex(T t) const;
		TInterval GetIw(Bit::N iBit) const;
		TMetaDataItem Split(T tAt);
		bool Equals(const TMetaDataItem &r) const;
	} *PCMetaDataItem;

	struct CMetaData:public std::set<TMetaDataItem>{
		inline operator bool() const{ return size()>0; }
		const_iterator GetMetaDataIterator(T t) const;
		PCMetaDataItem GetMetaDataItem(T t) const;
		PCMetaDataItem GetFirst() const;
		PCMetaDataItem GetLast() const;
	};




	namespace Decoder
	{
		#pragma pack(1)
		struct TLimits{
			T iwTimeDefault; // inspection window default size
			T iwTime; // inspection window size; a "1" is expected in its centre
			T iwTimeMin,iwTimeMax; // inspection window possible time range

			TLimits(T iwTimeDefault,BYTE iwTimeTolerancePercent=0);

			void ClampIwTime();
			inline T PeekNextIwTime(T tIwCurr) const{ return tIwCurr+iwTime; }
		};

		enum TMethod:BYTE{
			NONE			=1,
			KEIR_FRASER		=2,
			MARK_OGDEN		=8,
			METADATA		=16, // a hidden decoder to help extract bits from Times tagged with MetaData
			FDD_METHODS		=NONE|KEIR_FRASER|MARK_OGDEN
		};

		LPCTSTR GetDescription(TMethod m);

		struct TProfile sealed:public TLimits{
			TMethod method;
			union{
				struct{
					Bit::N nConsecutiveZeros;
				} fraser;
				struct{
					bool up;
					BYTE fCnt, aifCnt, adfCnt, pcCnt;
				} ogden;
				struct{
					Bit::N iCurrBit;
				} metaData;
			} methodState;

			TProfile(TMethod method=TMethod::NONE);
			TProfile(const TMetaDataItem &mdi);

			void Reset();
		};

		class CBase{
			const CMetaData *pMetaData;
			TMethod defaultMethod; // when no MetaData available
			CMetaData::const_iterator itCurrMetaData;
		protected:
			TProfile profile;
			CSharedArray logTimes; // buffer and its capacity; absolute Times expected, no deltas!
			N nLogTimes; // used portion of the buffer
			N iNextTime;
			T currentTime;
			BYTE nConsecutiveZerosMax; // # of consecutive zeroes to lose synchronization; e.g. 3 for MFM code
			Bit::TPattern lastReadBits; // validity flag and bit, e.g. 10b = valid bit '0', 11b = valid bit '1', 0Xb = invalid bit 'X'

			CBase(TMethod defaultMethod,const CSharedArray &logTimes,N nLogTimes,const CMetaData &metaData);

			PCMetaDataItem GetCurrentTimeMetaData() const;
			PCMetaDataItem ApplyCurrentTimeMetaData();
			PCMetaDataItem IncrMetaDataIteratorAndApply();
			PCMetaDataItem DecrMetaDataIteratorAndApply();
			PCMetaDataItem FindMetaDataIteratorAndApply();
		public:
			template<char n,typename X>
			bool ReadBits(X &rOut){
				// True <=> all N bits successfully read, otherwise False
				static_assert( n>1, "" );
				for( char i=n; i>0; i-- ){
					if (!*this)
						return false;
					rOut=(rOut<<1)|(X)ReadBit();
				}
				return true;
			}

			inline N GetTimesCount() const{ return nLogTimes; }
			inline operator bool() const{ return iNextTime<nLogTimes; } // still some LogicalTimes to read ?
			inline T GetCurrentTime() const{ return currentTime; }
			inline const TProfile &GetCurrentProfile() const{ return profile; }
			inline const CMetaData::const_iterator &GetCurrentTimeMetaDataIterator() const{ return itCurrMetaData; }
			void SetCurrentTime(T logTime);
			TProfile CreateResetProfile() const;
			T TruncateCurrentTime();
			T GetLastTime() const;
			T ReadTime();
			bool ReadBit(T &rtOutOne=Ignore);
			bool IsLastReadBitHealthy() const;
			char ReadBits8(BYTE &rOut);
			bool ReadBits15(WORD &rOut);
			bool ReadBits16(WORD &rOut);
			bool ReadBits32(DWORD &rOut);
			void SaveCsv(LPCTSTR filename) const;
			void SaveDeltaCsv(LPCTSTR filename) const;
		#ifdef _DEBUG
			void VerifyChronology() const;
		#endif
		};
	}



	class CTimeline:public Utils::CAxis{
	public:
		CTimeline(T logTimeLength,T logTimePerUnit,BYTE initZoomFactor);

		inline T GetTime(int nUnits) const{ return GetValue(nUnits); }
	};
}


typedef Time::T TLogTime,*PLogTime; // time in nanoseconds
typedef const Time::T *PCLogTime;
typedef Time::TInterval TLogTimeInterval;

#define TIME_NANO(n)	(n)
#define TIME_MICRO(u)	((u)*1000)
#define TIME_MILLI(m)	((m)*1000000)
#define TIME_SECOND(s)	((s)*1000000000)
