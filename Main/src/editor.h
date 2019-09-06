#ifndef RIDEAPP_H
#define RIDEAPP_H

	#define INI_GENERAL			_T("General")

	class CRidePen sealed:public CPen{
	public:
		static const CRidePen BlackHairline, WhiteHairline, RedHairline;

		CRidePen(BYTE thickness,COLORREF color);
	};

	class CRideBrush sealed:public CBrush{
	public:
		static const CRideBrush None, Black, White, BtnFace, Selection;

		CRideBrush(int stockObjectId);
		CRideBrush(bool sysColor,int sysColorId);
	};

	class CRideFont sealed:public CFont{
	public:
		static const CRideFont Small, Std, StdBold;

		int charAvgWidth,charHeight;

		CRideFont(LPCTSTR face,int pointHeight,bool bold=false,bool dpiScaled=false,int pointWidth=0);

		SIZE GetTextSize(LPCTSTR text,int textLength) const;
		SIZE GetTextSize(LPCTSTR text) const;
	};




	class CRideApp sealed:public CWinApp{
		DECLARE_MESSAGE_MAP()
	public:
		static bool __doPromptFileName__(PTCHAR fileName,bool fddAccessAllowed,UINT stdStringId,DWORD flags,LPCVOID singleAllowedImageProperties);

		static CLIPFORMAT cfDescriptor,cfRideFileList,cfContent,cfPreferredDropEffect,cfPerformedDropEffect,cfPasteSucceeded;

		BOOL InitInstance() override;
		int ExitInstance() override;
		CDocument *OpenDocumentFile(LPCTSTR lpszFileName) override;
		void OnFileOpen(); // public wrapper
		CRecentFileList *GetRecentFileList() const;
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