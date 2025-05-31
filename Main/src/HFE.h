#ifndef HFE_H
#define HFE_H

	class CHFE sealed:public CCapsBase{
		enum TFloppyInterface:BYTE{
			IBM_PC_DD,
			IBM_PC_HD,
			ATARI_ST_DD,
			ATARI_ST_HD,
			AMIGA_DD,
			AMIGA_HD,
			CPC_DD,
			GENERIC_SHUGART,
			IBM_PC_ED,
			MSX2_DD,
			C64_DD,
			EMU_SHUGART,
			S950_DD,
			S950_HD,
			LAST_KNOWN,
			DISABLED	=0xfe
		};

		enum TTrackEncoding:BYTE{
			IBM_MFM,
			AMIGA_MFM,
			IBM_FM,
			EMU_FM,
			UNKNOWN	=0xff
		};

		enum TStep:BYTE{
			DOUBLE	=0,
			SINGLE	=255
		};

		struct TBlock{
			BYTE bytes[512];
		};

		#pragma pack(1)
		union UHeader{
			struct{
				char signature[8];
				BYTE formatRevision; // Revision 0
				BYTE nCylinders;
				BYTE nHeads;
				TTrackEncoding trackEncoding;
				WORD dataBitRate; // data kBit/s; e.g. 250 = 250,000 bit/s; multiply by 2 to get total bitrate (data and clock bits)
				WORD driveRpm;
				TFloppyInterface floppyInterface;
				BYTE reserved; // don't use
				WORD cylInfosBegin; // offset (in multiples of 512 Bytes) of the TTrackInfo structures in the image; e.g. 1 = 0x200
				BYTE writeable; // NOT write-protected ?
				TStep step;
				struct{
					BYTE disabled;
					TTrackEncoding track0Encoding; // Track 0 encoding
				} alternative[2]; // Alternative encoding for Track 0 under either Head
			};
			TBlock block;

			UHeader(LPCSTR signature);

			bool IsValid() const;
			bool IsVersion3() const;
			CString ListUnsupportedFeatures() const;
			TLogTime GetCellTime() const;
		} header;

		#pragma pack(1)
		struct TTrackData sealed{
			BYTE bytes[256]; // 256*8 bits
		};

		#pragma pack(1)
		struct TCylinderBlock sealed{
			TTrackData head[2]; // 2 Heads
		};

		#pragma pack(1)
		struct TCylinderInfo sealed{
			WORD nBlocksOffset;
			WORD nBytesLength;

			inline bool IsValid() const{ return nBlocksOffset>=2 && nBytesLength!=0; }
		} cylInfos[84];

		class CTrackBytes sealed:public Utils::CCallocPtr<BYTE>{
			WORD count;
		public:
			CTrackBytes(WORD count);
			CTrackBytes(CTrackBytes &&r);

			inline operator PBYTE() const{ return get(); }
			inline WORD GetCount() const{ return count; }
			inline PBYTE end() const{ return get()+count; }
			inline void TrimTo(WORD newCount){ count=newCount; }
			void Invalidate();
			void ReverseBitsInEachByte() const;
		};

		enum TOpCode:BYTE{
			NOP			=0xf0,
			SETINDEX,
			SETBITRATE,
			SKIPBITS,
			RANDOM,
			LAST
		};

		mutable CFile f;

		CTrackBytes ReadTrackBytes(TCylinder cyl,THead head) const;
		CTrackBytes TrackToBytes(CInternalTrack &rit) const;
		PInternalTrack BytesToTrack(const CTrackBytes &bytes) const;
		TStdWinError SaveAllModifiedTracks(LPCTSTR lpszPathName,CActionProgress &ap) override;
	public:
		static const TProperties Properties;

		CHFE();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		TStdWinError SaveTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const;
		CTrackReader ReadTrack(TCylinder cyl,THead head) const override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		void EnumSettings(CSettings &rOut) const override;
		TStdWinError Reset() override;
		TStdWinError FormatTrack(TCylinder cyl,THead head,Codec::TType codec,TSector nSectors,PCSectorId bufferId,PCWORD bufferLength,PCFdcStatus bufferFdcStatus,BYTE gap3,BYTE fillerByte,const volatile bool &cancelled) override;
		CString ListUnsupportedFeatures() const override;
	};

#endif // HFE_H
