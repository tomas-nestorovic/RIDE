#pragma once

namespace Track
{
	typedef DWORD TTypeId;

	enum TOrder:BYTE{
		BY_CYLINDERS	=1,
		BY_HEADS		=2
	};

	class CBits:public Bit::CSequence{ // 'base' factors in Decoder reset upon Index pulse
	public:
		Bit::CSequence revs[Revolution::MAX];
	};




	class CReaderBuffers:public Time::Decoder::CBase{
	protected:
		typedef Time::Decoder::TMethod TDecoderMethod;
		typedef Time::Decoder::TProfile TProfile;
		typedef Time::Decoder::CBase CDecoder;

		struct TLogTimesInfoData abstract{
			Medium::PCProperties mediumProps;
			bool resetDecoderOnIndex;
			Codec::TType codec;
			TLogTime indexPulses[Revolution::MAX+2]; // "+2" = "+1+1" = "+A+B", A = tail IndexPulse of last possible Revolution, B = terminator
			Time::CMetaData metaData;
			struct:public Utils::CSharedBytes{
				TTypeId id;
			} rawDeviceData; // valid until Track modified, then disposed

			TLogTimesInfoData(bool resetDecoderOnIndex);
		};

		typedef class CLogTimesInfo sealed:public TLogTimesInfoData{
			UINT nRefs;
		public:
			CLogTimesInfo(bool resetDecoderOnIndex);

			inline UINT GetRefCount() const{ return nRefs; }
			inline void AddRef(){ ::InterlockedIncrement(&nRefs); }
			bool Release();
		} *PLogTimesInfo;

		PLogTimesInfo pLogTimesInfo;
		PLogTime indexPulses; // buffer to contain 'Max' full Revolutions

		CReaderBuffers(const CDecoder &decoder,PLogTimesInfo pLti);
	public:
		inline const Time::CMetaData &GetMetaData() const{ return pLogTimesInfo->metaData; }
	};




	class CReader:public CReaderBuffers{
	protected:
		TRev iNextIndexPulse,nIndexPulses;

		CReader(Time::N nLogTimesMax,TDecoderMethod method,PLogTimesInfo pLti,Codec::TType codec);

		WORD ScanFm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,Event::CList *pOutParseEvents);
		WORD ScanMfm(PSectorId pOutFoundSectors,PLogTime pOutIdEnds,TProfile *pOutIdProfiles,TFdcStatus *pOutIdStatuses,Event::CList *pOutParseEvents);
		TFdcStatus ReadDataFm(const TSectorId &sectorId,WORD nBytesToRead,Event::CSharedPtr *pOutDataPe,Event::CList *pOutParseEvents);
		TFdcStatus ReadDataMfm(const TSectorId &sectorId,WORD nBytesToRead,Event::CSharedPtr *pOutDataPe,Event::CList *pOutParseEvents);
	public:
		CReader(const CReader &tr);
		CReader(CReader &&tr);
		~CReader();

		inline TRev GetIndexCount() const{ return nIndexPulses; }
		inline PCLogTime GetBuffer() const{ return logTimes; }
		inline Codec::TType GetCodec() const{ return pLogTimesInfo->codec; }

		inline
		const TLogTimeInterval &GetFullRevolutionTimeInterval(TRev rev) const{
			static_assert( sizeof(TLogTimeInterval)==2*sizeof(*indexPulses), "" );
			ASSERT( rev<GetIndexCount()-1 );
			return *(TLogTimeInterval *)(indexPulses+rev);
		}

		void SetCodec(Codec::TType codec);
		void SetMediumType(Medium::TType mediumType);
		void SetCurrentTime(TLogTime logTime);
		void SetCurrentTimeAndProfile(TLogTime logTime,const TProfile &profile);
		TLogTime RewindToIndex(TRev index);
		TLogTime RewindToIndexAndResetProfile(TRev index);
		TLogTime GetIndexTime(TRev index) const;
		TLogTime GetLastIndexTime() const;
		TLogTime GetAvgIndexDistance() const;
		TLogTime GetTotalTime() const;
		const Utils::CSharedBytes &GetRawDeviceData(TTypeId dataId) const;
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
		BYTE __cdecl ShowModal(const Time::CSharedColorIntervalArray &regions,UINT messageBoxButtons,bool initAllFeaturesOn,TLogTime tScrollTo,LPCTSTR format,...) const;
		void __cdecl ShowModal(LPCTSTR format,...) const;
	};




	class CReaderWriter:public CReader{
		enum{
			LogTimesCountExtra=1
		};

		void AddExternalTimes(PCLogTime logTimes,Time::N nLogTimes);
		bool ReplaceTimes(const TLogTimeInterval &clearTimes,const CReader &writeTimes);
		bool WriteDataFm(Event::TData &peData,TFdcStatus sr);
		bool WriteDataMfm(Event::TData &peData,TFdcStatus sr);
	public:
		static const CReaderWriter Invalid;

		CReaderWriter(Time::N nLogTimesMax,TDecoderMethod method,bool resetDecoderOnIndex);
		CReaderWriter(Time::N nLogTimes,Medium::TType mediumType); // 'nLogTimes' uniformly distributed across a single-Revolution Track
		CReaderWriter(const CReaderWriter &trw,bool shareTimes=true);
		CReaderWriter(CReaderWriter &&trw);
		CReaderWriter(const CReader &tr);

		inline PLogTime GetBuffer() const{ return logTimes; }
		inline Time::N GetBufferCapacity() const{ return logTimes.length-LogTimesCountExtra; }

		void AddTime(TLogTime logTime);
		void AddTimes(PCLogTime logTimes,Time::N nLogTimes);
		void AddByte(TLogTimeInterval &inOutAt,BYTE b);
		void AddWord(TLogTimeInterval &inOutAt,WORD w);
		void AddDWord(TLogTimeInterval &inOutAt,DWORD dw);
		void AddIndexTime(TLogTime logTime);
		void AddMetaData(const Time::TMetaDataItem &mdi);
		void SetRawDeviceData(TTypeId dataId,const Utils::CSharedBytes &data);
		void TrimToTimesCount(Time::N nKeptLogTimes);
		void ClearMetaData(TLogTime a,TLogTime z);
		void ClearAllMetaData();
		bool WriteData(TLogTime idEndTime,const TProfile &idEndProfile,Event::TData &peData,TFdcStatus sr);
		TStdWinError Normalize();
		TStdWinError NormalizeEx(TLogTime indicesOffset,bool fitTimesIntoIwMiddles,bool correctCellCountPerRevolution,bool correctRevolutionTime);
		CReaderWriter &Reverse();
		CReaderWriter &Offset(TLogTime dt);
	};

}

typedef Track::N TTrack,*PTrack;
typedef Track::TOrder TTrackScheme;
typedef Track::CReader CTrackReader;
typedef Track::CReaderWriter CTrackReaderWriter;
