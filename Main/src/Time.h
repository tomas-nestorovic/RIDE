#pragma once

namespace Time
{
	typedef TLogValue T,*P; // time in nanoseconds
	typedef const T *PC;

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

}


typedef Time::T TLogTime,*PLogTime; // time in nanoseconds
typedef const Time::T *PCLogTime;
typedef Time::TInterval TLogTimeInterval;

#define TIME_NANO(n)	(n)
#define TIME_MICRO(u)	((u)*1000)
#define TIME_MILLI(m)	((m)*1000000)
#define TIME_SECOND(s)	((s)*1000000000)
