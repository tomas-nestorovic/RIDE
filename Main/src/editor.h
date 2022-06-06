#ifndef RIDEAPP_H
#define RIDEAPP_H

	#define INI_GENERAL			_T("General")

	#define INI_IS_UP_TO_DATE	_T("iu2e")
	#define INI_LATEST_KNOWN_VERSION _T("lu2d")

	class CRideApp sealed:public CWinApp{
		DECLARE_MESSAGE_MAP()
	private:
		bool godMode;
	public:
		class CRecentFileListEx sealed:public CRecentFileList{
			CDos::PCProperties openWith[ID_FILE_MRU_LAST+1-ID_FILE_MRU_FIRST];
			CImage::PCProperties m_deviceProps[ID_FILE_MRU_LAST+1-ID_FILE_MRU_FIRST];
		public:
			CRecentFileListEx(const CRecentFileList &rStdMru);

			CDos::PCProperties GetDosMruFileOpenWith(int nIndex) const;
			CImage::PCProperties GetMruDevice(int nIndex) const;
			void Add(LPCTSTR lpszPathName,CDos::PCProperties dosProps,CImage::PCProperties deviceProps);
			void Remove(int nIndex) override;
			void ReadList() override;
			void WriteList() override;
		};

		static CImage::PCProperties DoPromptFileName(PTCHAR fileName,bool deviceAccessAllowed,UINT stdStringId,DWORD flags,CImage::PCProperties singleAllowedImage);

		static CLIPFORMAT cfDescriptor,cfRideFileList,cfContent,cfPreferredDropEffect,cfPerformedDropEffect,cfPasteSucceeded;

		DWORD dateRecencyLastChecked; // 0 = recency not yet automatically checked online

		BOOL InitInstance() override;
		int ExitInstance() override;
		CDocument *OpenDocumentFile(LPCTSTR lpszFileName) override;
		void OnFileOpen(); // public wrapper
		CRecentFileListEx *GetRecentFileList() const;
		HWND GetEnabledActiveWindow() const;
		bool IsInGodMode() const;
		inline CMainWindow *GetMainWindow() const{ return (CMainWindow *)m_pMainWnd; }
		#if _MFC_VER>=0x0A00
		afx_msg void OnOpenRecentFile(UINT nID);
		#else
		afx_msg BOOL OnOpenRecentFile(UINT nID);
		#endif
		afx_msg void __createNewImage__();
		afx_msg void __openImage__();
		afx_msg void __openImageAs__();
		afx_msg void OpenImageWithoutDos();
		afx_msg void __openDevice__();
		afx_msg void __showAbout__();
	};

#endif // RIDEAPP_H