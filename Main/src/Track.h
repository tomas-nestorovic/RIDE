#pragma once

namespace Track
{
	typedef DWORD TTypeId;

	enum:TTypeId{
		InvalidTypeId=0
	};

	enum TOrder:BYTE{
		BY_CYLINDERS	=1,
		BY_HEADS		=2
	};

	class CBits:public Bit::CSequence{ // 'base' factors in Decoder reset upon Index pulse
	public:
		struct{
			TRev nFull;
			Bit::CSequence list[Revolution::MAX];

			inline const Bit::CSequence &operator[](TRev i) const{ return list[i]; }

			// 'for each' support for --FULL-- Revolutions
			inline const Bit::CSequence *begin() const{ return list; }
			inline const Bit::CSequence *end() const{ return list+nFull; }
		} revs;

		CBits();

		void ConvertToInspectionWindows(const Time::Decoder::TLimits &limits) const;
	};




	class CReaderBuffers:public Time::Decoder::CBase{
	protected:
		typedef Time::Decoder::TMethod TDecoderMethod;
		typedef Time::Decoder::TProfile TProfile;
		
		bool resetDecoderOnIndex;
		bool corrected; // True <=> corrections (e.g. jitter) applied, otherwise False
		Codec::TType codec;
		Time::CSharedArrayEx indexPulses; // buffer to contain 'Max' full Revolutions

		CReaderBuffers(Time::N nLogTimesInitCapacity,TDecoderMethod method);
	};




	struct TCorrections{
		union{
			struct{
				WORD reserved:1;
				WORD use:1;
				WORD indexTiming:1;
				WORD cellCountPerRevolution:1;
				WORD fitTimesIntoIwMiddles:1;
				WORD offsetIndices:1;
			};
			WORD w;
		};
		Time::T16 indexOffsetMicroseconds;

		TCorrections(); // no Corrections
		TCorrections(LPCTSTR iniSection,LPCTSTR iniName=_T("crt")); // load

		void Save(LPCTSTR iniSection,LPCTSTR iniName=_T("crt")) const;
		bool ShowModal(CWnd *pParentWnd);
	};

	class CReaderWriter:public CReaderBuffers{
		struct:public Memory::CSharedBytes{
			TTypeId id;
		} rawDeviceData; // valid until Track modified, then disposed

		WORD ScanFm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,Event::CList *pOutParseEvents);
		WORD ScanMfm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,Event::CList *pOutParseEvents);
		TFdcStatus ReadDataFm(const TSectorId &sectorId,WORD nBytesToRead,Event::CSharedPtr *pOutDataPe,Event::CList *pOutParseEvents);
		TFdcStatus ReadDataMfm(const TSectorId &sectorId,WORD nBytesToRead,Event::CSharedPtr *pOutDataPe,Event::CList *pOutParseEvents);
		bool WriteDataFm(Event::TData &peData,TFdcStatus sr);
		bool WriteDataMfm(Event::TData &peData,TFdcStatus sr);
	public:
		CReaderWriter(Time::N nLogTimesInitCapacity,TDecoderMethod method,bool resetDecoderOnIndex);
		CReaderWriter(Time::N nLogTimes,const Medium::TProperties &mp); // 'nLogTimes' uniformly distributed across a single-Revolution Track

		inline PLogTime GetBuffer() const{ return logTimes; }
		inline TRev GetIndexCount() const{ return indexPulses.length; }
		inline Codec::TType GetCodec() const{ return codec; }
		inline void ForkTimes(){ logTimes.Fork(); }

		inline
		const TLogTimeInterval &GetFullRevolutionTimeInterval(TRev rev) const{
			static_assert( sizeof(TLogTimeInterval)==2*sizeof(*indexPulses), "" );
			ASSERT( rev<indexPulses.length-1 );
			return *(TLogTimeInterval *)(indexPulses.begin()+rev);
		}

		void SetCodec(Codec::TType codec);
		void SetMedium(const Medium::TProperties &mp);
		void SetCurrentTime(TLogTime logTime);
		void SetCurrentTimeAndProfile(TLogTime logTime,const TProfile &profile);
		TLogTime RewindToIndex(TRev index);
		TLogTime RewindToIndexAndResetProfile(TRev index);
		TLogTime GetIndexTime(TRev index) const;
		TLogTime GetLastIndexTime() const;
		TLogTime GetAvgIndexDistance() const;
		TLogTime GetTotalTime() const;
		bool ReadBit(TLogTime &rtOutOne=Time::Ignore);
		bool IsLastReadBitHealthy() const;
		Bit::CSequence CreateBitSequence(TLogTime tFrom,const TProfile &profileFrom,TLogTime tTo,BYTE oneOkPercent=0) const;
		Bit::CSequence CreateBitSequence(const TLogTimeInterval &ti,BYTE oneOkPercent=0) const;
		Bit::CSequence CreateBitSequence(Revolution::TType rev,BYTE oneOkPercent=0) const;
		Bit::CSequence CreateBitSequence(BYTE oneOkPercent=0) const;
		CBits CreateFullRevBitSequences(BYTE oneOkPercent=0) const;
		char ReadByte(Bit::TPattern &rOutBits,PBYTE pOutValue=nullptr);
		WORD Scan(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,Event::CList *pOutParseEvents=nullptr);
		WORD ScanAndAnalyze(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,PLogTime pOutDataEnds,Event::CList &rOutParseEvents,CActionProgress &ap,bool fullAnalysis=true,CBits *pOutBits=nullptr);
		WORD ScanAndAnalyze(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,PLogTime pOutDataEnds,Event::CList &rOutParseEvents,CActionProgress &ap,bool fullAnalysis=true,CBits *pOutBits=nullptr);
		Event::CList ScanAndAnalyze(CActionProgress &ap,bool fullAnalysis=true,CBits *pOutBits=nullptr);
		TFdcStatus ReadData(const TSectorId &id,TLogTime idEndTime,const TProfile &idEndProfile,WORD nBytesToRead,Event::CSharedPtr *pOutDataPe,Event::CList *pOutAllParseEvents);

		void AppendTime(TLogTime logTime);
		void AppendTimes(PCLogTime logTimes,Time::N nLogTimes);
		void AppendByte(TLogTimeInterval &inOutAt,BYTE b);
		void AppendWord(TLogTimeInterval &inOutAt,WORD w);
		void AppendDWord(TLogTimeInterval &inOutAt,DWORD dw);
		void AppendIndexTime(TLogTime logTime);
		void InsertMetaData(const Time::TMetaDataItem &mdi);
		const Memory::CSharedBytes &GetRawDeviceData(TTypeId dataId) const;
		void SetRawDeviceData(TTypeId dataId,const Memory::CSharedBytes &data);
		void TrimToTimesCount(Time::N nKeptLogTimes);
		bool ReplaceTimes(const TLogTimeInterval &clearTimes,const CReaderWriter &writeTimes);
		void ClearMetaData(const TLogTimeInterval &ti);
		void ClearAllMetaData();
		bool WriteData(TLogTime idEndTime,const TProfile &idEndProfile,Event::TData &peData,TFdcStatus sr);
		TStdWinError Normalize(const Medium::TProperties &mp);
		TStdWinError Apply(const Medium::TProperties &mp,const TCorrections &c);
		CReaderWriter &Reverse();
		BYTE __cdecl ShowModal(const Time::CSharedColorIntervalArray &regions,UINT messageBoxButtons,bool initAllFeaturesOn,TLogTime tScrollTo,LPCTSTR format,...) const;
		void __cdecl ShowModal(LPCTSTR format,...) const;
	};

	extern const CReaderWriter Invalid;
}

typedef Track::TOrder TTrackScheme;
typedef Track::CReaderWriter CTrackReader,CTrackReaderWriter;
