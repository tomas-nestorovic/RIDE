#ifndef FILLEMPTYSPACEDIALOG_H
#define FILLEMPTYSPACEDIALOG_H

	class CFillEmptySpaceDialog sealed:public Utils::CRideDialog{
		DECLARE_MESSAGE_MAP()
	private:
		const CDos *const dos;
		const CDos::PCProperties dosProps;
		BYTE nOptionsChecked;

		void DoDataExchange(CDataExchange *pDX) override;
		afx_msg void __enableOkButton__(UINT id);
		afx_msg void __setDefaultFillerByteForGeneralSectors__();
		afx_msg void __setDefaultFillerByteForDirectorySectors__();
	public:
		BYTE sectorFillerByte,directoryFillerByte;
		int fillEmptySectors;
		int fillFileEndings, fillSubdirectoryFileEndings;
		int fillEmptyDirectoryEntries, fillEmptySubdirectoryEntries;

		CFillEmptySpaceDialog(const CDos *dos);
	};

#endif // FILLEMPTYSPACEDIALOG_H
