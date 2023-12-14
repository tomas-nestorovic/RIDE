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
			GENERIC_SHUGART_DD,
			IBM_PC_ED,
			MSX2_DD,
			C64_DD,
			EMU_SHUGART,
			S950_DD,
			S950_HD,
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
				WORD bitrate; // kBit/s; e.g. 250 = 250,000 bit/s
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

			UHeader();

			bool IsValid() const;
			inline bool WantDoubleTrackStep() const{ return step==TStep::DOUBLE; }
		} header;

		#pragma pack(1)
		struct TTrackBlock sealed{
			BYTE data[256]; // 256*8 bits
		};

		#pragma pack(1)
		struct TCylinderBlock sealed{
			TTrackBlock data[2]; // 2 Heads
		};

		#pragma pack(1)
		struct TCylinderInfo sealed{
			WORD nBlocksOffset;
			WORD nBytesLength;
		} cylInfos[FDD_CYLINDERS_MAX];

		mutable CFile f;

		//TStdWinError SaveAllModifiedTracks(LPCTSTR lpszPathName,CActionProgress &ap) override;
	public:
		static const TProperties Properties;

		CHFE();

		BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
		//TStdWinError SaveTrack(TCylinder cyl,THead head,const volatile bool &cancelled) const;
		CTrackReader ReadTrack(TCylinder cyl,THead head) const override;
		TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		TStdWinError Reset() override;
	};

#endif // HFE_H
