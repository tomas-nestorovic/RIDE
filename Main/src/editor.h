#ifndef RIDEAPP_H
#define RIDEAPP_H

	#define INI_GENERAL			_T("General")

	class CRideApp sealed:public CWinApp{
		DECLARE_MESSAGE_MAP()
	private:
		bool godMode;
	public:
		class CRecentFileListEx sealed:public CRecentFileList{
			CDos::PCProperties openWith[ID_FILE_MRU_LAST+1-ID_FILE_MRU_FIRST];
		public:
			CRecentFileListEx(const CRecentFileList &rStdMru);

			CDos::PCProperties GetDosMruFileOpenWith(int nIndex) const;
			void Add(LPCTSTR lpszPathName,CDos::PCProperties dosProps);
			void Remove(int nIndex) override;
			void ReadList() override;
			void WriteList() override;
		};

		static CImage::PCProperties DoPromptFileName(PTCHAR fileName,bool fddAccessAllowed,UINT stdStringId,DWORD flags,CImage::PCProperties singleAllowedImage);

		static CLIPFORMAT cfDescriptor,cfRideFileList,cfContent,cfPreferredDropEffect,cfPerformedDropEffect,cfPasteSucceeded;

		BOOL InitInstance() override;
		int ExitInstance() override;
		CDocument *OpenDocumentFile(LPCTSTR lpszFileName) override;
		void OnFileOpen(); // public wrapper
		CRecentFileListEx *GetRecentFileList() const;
		bool IsInGodMode() const;
		#if _MFC_VER>=0x0A00
		afx_msg void OnOpenRecentFile(UINT nID);
		#else
		afx_msg BOOL OnOpenRecentFile(UINT nID);
		#endif
		afx_msg void __createNewImage__();
		afx_msg void __openImage__();
		afx_msg void __openImageAs__();
		afx_msg void __showAbout__();
	};

#endif // RIDEAPP_H