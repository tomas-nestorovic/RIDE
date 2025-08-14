#ifndef RIDEAPP_H
#define RIDEAPP_H

	#define INI_GENERAL			_T("General")

	#define INI_IS_UP_TO_DATE	_T("iu2e")
	#define INI_LATEST_KNOWN_VERSION _T("lu2d")

	class CRideApp sealed:public CWinApp{
		DECLARE_MESSAGE_MAP()
	private:
		bool godMode; // launched with "--godmode" param ?
		ATOM propGridWndClass;
	public:
		class CRecentFileListEx sealed:public CRecentFileList{
			HACCEL hAccelTable;
			CDos::PCProperties openWith[ID_FILE_MRU_LAST+1-ID_FILE_MRU_FIRST];
			CImage::PCProperties m_deviceProps[ID_FILE_MRU_LAST+1-ID_FILE_MRU_FIRST];
		public:
			CRecentFileListEx(const CRecentFileList &rStdMru);
			~CRecentFileListEx();

			inline const CString &operator[](int nIndex) const{ return const_cast<CRecentFileListEx *>(this)->__super::operator[](nIndex); }
			inline bool PreTranslateMessage(HWND hWnd,PMSG pMsg) const{ return ::TranslateAccelerator( hWnd, hAccelTable, pMsg )!=0; }
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
		bool GetProfileBool(LPCTSTR sectionName,LPCTSTR keyName,bool bDefault=false);
		CDocument *OpenDocumentFile(LPCTSTR lpszFileName) override;
		void OnFileOpen(); // public wrapper
		inline CRecentFileListEx *GetRecentFileList() const{ return (CRecentFileListEx *)m_pRecentFileList; }
		HWND GetEnabledActiveWindow() const;
		inline bool IsInGodMode() const{ return godMode; }
		inline LPCTSTR GetPropGridWndClass() const{ return (LPCTSTR)propGridWndClass; }
		inline CMainWindow *GetMainWindow() const{ return (CMainWindow *)m_pMainWnd; }
		#if _MFC_VER>=0x0A00
		afx_msg void OnOpenRecentFile(UINT nID);
		#else
		afx_msg BOOL OnOpenRecentFile(UINT nID);
		#endif
		afx_msg void CreateNewImage();
		afx_msg void OpenImage();
		afx_msg void __openImageAs__();
		afx_msg void OpenImageWithoutDos();
		afx_msg void __openDevice__();
		afx_msg void __showAbout__();

		template<typename T>
		T GetProfileEnum(LPCTSTR sectionName,LPCTSTR keyName,T nDefault){
			return (T)GetProfileInt( sectionName, keyName, nDefault );
		}
	};

#endif // RIDEAPP_H