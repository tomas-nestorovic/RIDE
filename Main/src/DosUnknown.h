#ifndef DOS_UNKNOWN_H
#define DOS_UNKNOWN_H

	class CUnknownDos sealed:public CDos{
	public:
		static const TProperties Properties;

		CUnknownDos(PImage image,PCFormat pFormatBoot);

		// boot
		void FlushToBootSector() const override; // projects information stored in internal FormatBoot back to the Boot Sector (e.g. called automatically by BootView)
		// FAT
		bool GetSectorStatuses(TCylinder,THead,TSector nSectors,PCSectorId,PSectorStatus buffer) const override;
		bool ModifyStdSectorStatus(RCPhysicalAddress,TSectorStatus) const override;
		bool GetFileFatPath(PCFile,CFatPath &) const override;
		bool ModifyFileFatPath(PFile,const CFatPath &) const override;
		DWORD GetFreeSpaceInBytes(TStdWinError &rError) const override;
		// file system
		bool GetFileNameOrExt(PCFile file,PPathString,PPathString) const override;
		TStdWinError ChangeFileNameAndExt(PFile,RCPathString,RCPathString,PFile &) override;
		DWORD GetFileSize(PCFile,PBYTE,PBYTE,TGetFileSizeOptions) const override;
		DWORD GetAttributes(PCFile file) const override;
		TStdWinError DeleteFile(PFile) override;
		std::unique_ptr<TDirectoryTraversal> BeginDirectoryTraversal(PCFile directory) const override;
		CPathString GetFileExportNameAndExt(PCFile,bool) const override;
		TStdWinError ImportFile(CFile *fIn,DWORD fileSize,RCPathString nameAndExtension,DWORD winAttr,PFile &rFile) override;
		// other
		TStdWinError CreateUserInterface(HWND hTdi) override;
		TCmdResult ProcessCommand(WORD cmd) override;
		void InitializeEmptyMedium(CFormatDialog::PCParameters,CActionProgress &) override;
	};

#endif // DOS_UNKNOWN_H