#pragma once

namespace Track
{
	namespace Event
	{
		typedef size_t N; // index or count

		enum:Time::T{
			TimelyFromPrevious=Time::Invalid
		};

		enum TType:BYTE{
			NONE,			// invalid ParseEvent
			SYNC_3BYTES,	// dw
			MARK_1BYTE,		// b
			PREAMBLE,		// dw (length)
			DATA_OK,		// dw (length)
			DATA_BAD,		// dw (length)
			DATA_IN_GAP,	// dw (length)
			CRC_OK,			// dw
			CRC_BAD,		// dw
			NONFORMATTED,	// - (no params)
			FUZZY_OK,		// - (no params); at least one Revolution yields OK data
			FUZZY_BAD,		// - (no params); all Revolutions yield only bad data
			META_STRING,	// lpsz; textual description of a ParseEvent not covered above
			LAST
		};

		extern const COLORREF TypeColors[LAST];

		struct T:public Time::TInterval{
			TType type;
			DWORD size; // length of this ParseEvent in Bytes
			union{
				BYTE b;
				WORD w;
				DWORD dw;
				int i;
				char lpszMetaString[sizeof(DWORD)]; // textual description of a ParseEvent event
			};

			inline T(){}
			T(TType type,TLogTime tStart,TLogTime tEnd,DWORD data);

			inline bool IsType(TType typeFrom,TType typeTo) const{ return typeFrom<=type && type<=typeTo; }
			inline bool IsDataStd() const{ return IsType( DATA_OK, DATA_BAD ); }
			inline bool IsDataAny() const{ return IsType( DATA_OK, DATA_IN_GAP ); }
			inline bool IsCrc() const{ return IsType( CRC_OK, CRC_BAD ); }
			inline bool IsFuzzy() const{ return IsType( FUZZY_OK, FUZZY_BAD ); }
			CString GetDescription(bool preferDecimalValues=false) const;
		};

		struct TMetaString:public T{
			static void Create(T &buffer,TLogTime tStart,TLogTime tEnd,LPCSTR lpszMetaString);
		};

		struct TData:public T{
			typedef struct TByteInfo:public Bit::TFlags{
				struct{
					BYTE value;
					union{
						WORD w;
						Bit::TPattern bits;
						inline operator Bit::TPattern() const{ return bits; }
						inline Bit::TPattern operator=(Bit::TPattern p){ return bits=p; }
					} encoded;
				} org;
				TLogTime dtStart; // offset against ParseEvent's start
			} *PByteInfo;

			Time::Decoder::TProfile profileEnd; // Profile at the end of this ParseEvent (aka. at the end of the last Byte)
			TSectorId sectorId; // or TSectorId::Invalid
			union{
				struct:TByteInfo{ BYTE v; } dummy[SHRT_MAX]; // to give the structure initial maximum size
				BYTE bytes[1];
			};

			TData(const TSectorId &sectorId,TLogTime tStart);

			void Finalize(TLogTime tEnd,const Time::Decoder::TProfile &profileEnd,Sector::L nBytes,TType type=DATA_BAD);
			inline TLogTime GetByteTime(Sector::L iByte) const{ return tStart+GetByteInfos()[iByte].dtStart; }
			inline Sector::L GetByteCount() const{ return w; }
			inline PByteInfo GetByteInfos() const{ return (PByteInfo)(bytes+GetByteCount()); }
		};




		typedef Utils::CSharedPodPtr<T> CSharedPtr;

		class CList:private Utils::CPodList<T>{
			N peTypeCounts[LAST];

			CList(const CList &r); //delete
		public:
			typedef std::multimap<TLogTime,const T *> CLogTiming; // multimap to allow ParseEvents starting concurrently at the same time

			typedef class CIterator:public CLogTiming::const_iterator{
				friend class CList;
				const CLogTiming &logTimes;
			public:
				CIterator(const CLogTiming &logTimes,const CLogTiming::const_iterator &it);

				inline operator const T &() const{ return *(*this)->second; }
				inline operator bool() const{ return *this!=logTimes.cend(); }
				inline CIterator &operator=(const CLogTiming::const_iterator &r){ return __super::operator=(r),*this; }
			} CIteratorByStart, CIteratorByEnd;
		private:
			struct TBinarySearch sealed:public CLogTiming{
				inline TBinarySearch(){}
				inline TBinarySearch(TBinarySearch &&r)
					: CLogTiming( std::move(static_cast<CLogTiming &>(r)) ) {
				}
				CIterator Find(TLogTime tMin,TType typeFrom,TType typeTo) const;
			} logStarts, logEnds; // values may not correspond to real ParseEvent timing (hence the prefix "logical") - but the value is always AT LEAST the real Start/End
		public:
			CList();
			CList(CList &&r); // move-ctor

			void Add(const CSharedPtr &ptr);
			void Add(const T &pe);
			void Add(const CList &list);
			CIterator GetIterator() const;
			inline N GetCount() const{ return logStarts.size(); }
			inline CIteratorByStart GetFirstByStart() const{ return GetIterator(); }
			CIteratorByStart GetLastByStart() const;
			inline CIteratorByStart FindByStart(TLogTime tStartMin,TType typeFrom=NONE,TType typeTo=LAST) const{ return logStarts.Find( tStartMin, typeFrom, typeTo ); }
			CIteratorByStart FindByStart(TLogTime tStartMin,TType type) const;
			inline CIteratorByEnd FindByEnd(TLogTime tEndMin,TType typeFrom=NONE,TType typeTo=LAST) const{ return logEnds.Find( tEndMin, typeFrom, typeTo ); }
			CIteratorByEnd FindByEnd(TLogTime tEndMin,TType type) const;
			inline bool Contains(TType type) const{ return peTypeCounts[type]>0; }
			bool IntersectsWith(const TLogTimeInterval &ti) const;
			void RemoveConsecutiveBeforeEnd(TLogTime tEndMax);
			TType GetTypeOfFuzziness(CIterator &itContinue,const Time::TInterval &tiFuzzy,TLogTime tTrackEnd) const;
			// 'for each' support
			inline CLogTiming::const_iterator begin() const{ return logStarts.cbegin(); }
			inline CLogTiming::const_iterator end() const{ return logStarts.cend(); }
		};

	}

}

typedef Track::Event::T TParseEvent;
typedef Track::Event::TData TDataParseEvent;
typedef Track::Event::TData::PByteInfo PByteInfo;
typedef Track::Event::CList CParseEventList;
typedef Track::Event::CSharedPtr CSharedParseEventPtr;
