#ifndef SUPERCARDPROBASE_H
#define SUPERCARDPROBASE_H

	#define INI_SUPERCARDPRO	_T("Scpflux")

	#define SCP_BUFFER_CAPACITY		2000000

	class CSuperCardProBase abstract:public CCapsBase{
	protected:
		struct TParamsEtc{
			const LPCTSTR iniSection;
			// persistent (saved and loaded)
			CString firmwareFileName;
			// volatile (current session only)
			//none

			TParamsEtc(LPCTSTR iniSection);
			~TParamsEtc();
		} paramsEtc;

		#pragma pack(1)
		struct THeader{
			union{
				struct{
					char signature[3];	// "SCP"
					BYTE revision;		// (version << 4) | revision, e.g. 0x24 = v2.4
				};
				DWORD signatureAndRevision;
			};
			enum:BYTE{
				CBM_C64_FLOPPY		=0x00,
				CBM_AMIGA_FLOPPY_DD	=0x04,
				CBM_AMIGA_FLOPPY_HD	=0x08,

				ATARI_FM_FLOPPY_SS	=0x10,
				ATARI_FM_FLOPPY_DS	=0x11,
				ATARI_FM_FLOPPY_EX	=0x12,
				ATARI_ST_FLOPPY_SS	=0x14,
				ATARI_ST_FLOPPY_DS	=0x15,

				APPLE_II			=0x20,
				APPLE_II_PRO		=0x21,
				APPLE_FLOPPY_400K	=0x24,
				APPLE_FLOPPY_800K	=0x25,
				APPLE_FLOPPY_HD_350	=0x26,

				PC_FLOPPY_SD		=0x30,
				PC_FLOPPY_DD		=0x31,
				PC_FLOPPY_HD_525	=0x32,
				PC_FLOPPY_HD_350	=0x33,

				TANDY_TRS80_SS_SD	=0x40,
				TANDY_TRS80_SS_DD	=0x41,
				TANDY_TRS80_DS_SD	=0x42,
				TANDY_TRS80_DS_DD	=0x43,

				TI_994A				=0x50,

				ROLAND_D20			=0x60,

				AMSTRAD_CPC			=0x70,

				OTHER_FLOPPY_SD		=0x80,
				OTHER_FLOPPY_HD_525	=0x81,
				OTHER_FLOPPY_DD		=0x84,
				OTHER_FLOPPY_HD_350	=0x85,

				TAPE_GCR1			=0xE0,
				TAPE_GCR2			=0xE1,
				TAPE_MFM			=0xE2,

				HDD_MFM				=0xF0,
				HDD_RLL				=0xF1
			} deviceDiskType; // device/disk
			BYTE nAvailableRevolutions; // number of stored (always FULL) Revolutions
			BYTE firstTrack;
			BYTE lastTrack;
			struct{
				BYTE indexAligned:1;	// True <=> index-synchronised, otherwise data start randomly in a Revolution and roll over indices in 200/166ms intervals (300/360rpm drives, respectively)
				BYTE tpi96:1;			// True <=> 96 TPI drive, otherwise 48 TPI
				BYTE rpm360:1;			// True <=> 360 RPM drive, otherwise 300 RPM
				BYTE normalized:1;		// True <=> normalized fluxes, otherwise preservation quality
				BYTE modifiable:1;		// True <=> read/write Image, otherwise read-only
				BYTE footerPresent:1;	// True <=> Footer blocks present, otherwise no Footer
				BYTE extended:1;		// True <=> this Image is NOT solely for floppy drives, otherwise this Image is intended ONLY for floppy drives
				BYTE createdUsingScp:1;	// True <=> this Image has been created by official SuperCard Pro device/software, otherwise 3rd party device/software used
			} flags;
			BYTE nFluxCellBits;	// 0 = 16 bits, non-zero = number of bits
			enum:BYTE{
				BOTH_HEADS,
				HEAD_0_ONLY,
				HEAD_1_ONLY
			} heads;
			BYTE resolutionMul;	// multiplier of the basic 25ns resolution
			DWORD checksum;		// 32-bit checksum from after header to EOF (unless FLAG_MODE is set)

			THeader();

			bool IsValid() const;
			bool IsSupported() const;
			inline TLogTime GetSampleClockTime() const{ return TIME_NANO(25)*std::max((BYTE)1,resolutionMul); }
		} header;

		struct TRevolution sealed{
			DWORD durationCounter; // length of the Revolution (=N); total time in nanoseconds then obtained as N*25ns
			DWORD nFluxes;
			DWORD iFluxDataBegin;
		};

		#pragma pack(1)
		struct TTrackDataHeader sealed{
			char signature[3]; // "TRK"
			BYTE trackNumber;
			TRevolution revolutions[Revolution::MAX]; // only first N are valid, see the Header

			TTrackDataHeader(BYTE trackNumber);

			bool IsTrack(BYTE trackNumber) const;
			bool Read(CFile &fSeeked,TCylinder cyl,THead head,BYTE nAvailableRevolutions);
			void Write(CFile &fSeeked,BYTE nAvailableRevolutions) const;
			const TRevolution &GetLastRevolution(BYTE nAvailableRevolutions) const;
			BYTE GetDistinctRevolutionCount(BYTE nAvailableRevolutions,PBYTE pOutUniqueRevs=nullptr) const;
			DWORD GetFullTrackLengthInBytes(const THeader &header) const;
			DWORD GetFullTrackCapacityInBytes(Medium::PCProperties mp,const THeader &header) const;
		};


		const LPCTSTR firmware;

		CSuperCardProBase(PCProperties properties,char realDriveLetter,LPCTSTR iniSection,LPCTSTR firmware);

		CTrackReaderWriter StreamToTrack(CFile &fTdhAndFluxes,TCylinder cyl,THead head) const;
		DWORD TrackToStream(CTrackReader tr,CFile &fTdhAndFluxes,TCylinder cyl,THead head,bool &rOutAdjusted) const;
	public:
		//static DWORD TimeToStdSampleCounter(TLogTime t);

		//TStdWinError SetMediumTypeAndGeometry(PCFormat pFormat,PCSide sideMap,TSector firstSectorNumber) override;
		bool EditSettings(bool initialEditing) override;
		//TStdWinError Reset() override;
	};

#endif // SUPERCARDPROBASE_H
