#ifndef UTILS_H
#define UTILS_H

#define MSG_HELP_CANCEL	_T("Don't need any help now.")

typedef DWORD TStdWinError; // Windows standard i/o error

namespace Utils{

	class CRidePen sealed:public ::CPen{
	public:
		static const CRidePen BlackHairline, WhiteHairline, RedHairline;

		CRidePen(BYTE thickness,COLORREF color);
	};

	class CRideBrush sealed:public ::CBrush{
	public:
		static const CRideBrush None, Black, White, BtnFace, Selection;

		CRideBrush(int stockObjectId);
		CRideBrush(bool sysColor,int sysColorId);

		operator COLORREF() const;
	};

	class CRideFont sealed:public ::CFont{
	public:
		static const CRideFont Small, Std, StdBold;

		int charAvgWidth,charHeight;

		CRideFont(LPCTSTR face,int pointHeight,bool bold=false,bool dpiScaled=false,int pointWidth=0);
		CRideFont(HWND hWnd,bool bold=false);

		SIZE GetTextSize(LPCTSTR text,int textLength) const;
		SIZE GetTextSize(LPCTSTR text) const;
	};

	class CCommandDialog:public CDialog{
		const LPCTSTR information;
	protected:
		void __addCommandButton__(WORD id,LPCTSTR caption);

		CCommandDialog(LPCTSTR _information);
		CCommandDialog(WORD dialogId,LPCTSTR _information);

		void PreInitDialog() override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	};

	typedef const struct TSplitButtonAction sealed{
		WORD commandId;
		LPCTSTR commandCaption;
	} *PCSplitButtonAction;

	class CByteIdentity sealed{
		BYTE values[(BYTE)-1+1];
	public:
		CByteIdentity();

		operator PCBYTE() const;
	};

	class CLocalTime sealed:public CTimeSpan{
		short nMilliseconds;
		CLocalTime(const CTimeSpan &ts,short nMilliseconds);
	public:
		CLocalTime();
		CLocalTime operator+(const CLocalTime &rTime2) const;
		CLocalTime operator-(const CLocalTime &rTime2) const;
		WORD GetMilliseconds() const;
		DWORD ToMilliseconds() const;
	};

	extern const float LogicalUnitScaleFactor;

	CString ComposeErrorMessage(LPCTSTR text,LPCTSTR causeOfError,LPCTSTR consequence=nullptr);
	CString ComposeErrorMessage(LPCTSTR text,TStdWinError causeOfError,LPCTSTR consequence=nullptr);
	void FatalError(LPCTSTR text);
	void FatalError(LPCTSTR text,LPCTSTR causeOfError,LPCTSTR consequence=nullptr);
	void FatalError(LPCTSTR text,TStdWinError causeOfError,LPCTSTR consequence=nullptr);
	void Information(LPCTSTR text);
	void Information(LPCTSTR text,LPCTSTR causeOfError,LPCTSTR consequence=nullptr);
	void Information(LPCTSTR text,TStdWinError causeOfError,LPCTSTR consequence=nullptr);
	bool InformationWithCheckBox(LPCTSTR textInformation,LPCTSTR checkBoxCaption);
	void InformationWithCheckableShowNoMore(LPCTSTR text,LPCTSTR sectionId,LPCTSTR messageId);
	bool InformationOkCancel(LPCTSTR text);
	bool QuestionYesNo(LPCTSTR text,UINT defaultButton);
	BYTE QuestionYesNoCancel(LPCTSTR text,UINT defaultButton);
	BYTE QuestionYesNoCancel(LPCTSTR text,UINT defaultButton,LPCTSTR causeOfError,LPCTSTR consequence=nullptr);
	BYTE QuestionYesNoCancel(LPCTSTR text,UINT defaultButton,TStdWinError causeOfError,LPCTSTR consequence=nullptr);
	BYTE AbortRetryIgnore(LPCTSTR text,UINT defaultButton);
	BYTE AbortRetryIgnore(LPCTSTR text,TStdWinError causeOfError,UINT defaultButton,LPCTSTR consequence=nullptr);
	BYTE AbortRetryIgnore(TStdWinError causeOfError,UINT defaultButton);
	bool RetryCancel(LPCTSTR text);
	bool RetryCancel(TStdWinError causeOfError);
	BYTE CancelRetryContinue(LPCTSTR text,UINT defaultButton);
	BYTE CancelRetryContinue(LPCTSTR text,TStdWinError causeOfError,UINT defaultButton,LPCTSTR consequence=nullptr);
	void Warning(LPCTSTR text);
	bool EnableDlgControl(HWND hDlg,WORD controlId,bool enabled);
	bool EnableDlgControls(HWND hDlg,PCWORD buttonIds,bool enabled);
	bool IsDlgControlEnabled(HWND hDlg,WORD controlId);
	void OffsetDlgControl(HWND hDlg,WORD controlId,int dx,int dy);
	void BytesToHigherUnits(DWORD bytes,float &rHigherUnit,LPCTSTR &rHigherUnitName);
	CString BytesToHexaText(PCBYTE bytes,BYTE nBytes,bool lastDelimitedWithAnd);
	CString BytesToHexaText(const char *chars,BYTE nChars,bool lastDelimitedWithAnd);
	void NavigateToUrlInDefaultBrowser(LPCTSTR url);
	void DrawClosingCurlyBracket(HDC dc,int x,int yMin,int yMax);
	void WrapControlsByClosingCurlyBracketWithText(CWnd *wnd,const CWnd *pCtrlA,const CWnd *pCtrlZ,LPCTSTR text,DWORD textColor);
	void ConvertToSplitButton(HWND hStdBtn,PCSplitButtonAction pAction,BYTE nActions);
	void ConvertToCommandLikeButton(HWND hStdBtn,WCHAR wingdingsGlyphBeforeText=0xf0e8,COLORREF textColor=0,int glyphPointSizeIncrement=0,COLORREF glyphColor=0); // 0xf0e8 = arrow right
	void SetSingleCharTextUsingFont(HWND hWnd,WCHAR singleChar,LPCTSTR fontFace,int fontPointSize);
	void PopulateComboBoxWithSequenceOfNumbers(HWND hComboBox,BYTE iStartValue,LPCTSTR strStartValueDesc,BYTE iEndValue,LPCTSTR strEndValueDesc);
	float ScaleLogicalUnit(HDC dc);
	void UnscaleLogicalUnit(PINT values,BYTE nValues);
	COLORREF GetSaturatedColor(COLORREF color,float saturationFactor);
	COLORREF GetBlendedColor(COLORREF color1,COLORREF color2,float blendFactor=.5f);
	CFile &WriteToFile(CFile &f,LPCTSTR text);
	CFile &WriteToFileFormatted(CFile &f,LPCTSTR format,...);
	CFile &WriteToFile(CFile &f,TCHAR chr);
	CFile &WriteToFile(CFile &f,int number,LPCTSTR formatting);
	CFile &WriteToFile(CFile &f,int number);
	CFile &WriteToFile(CFile &f,double number,LPCTSTR formatting);
	CFile &WriteToFile(CFile &f,double number);
	PTCHAR GetApplicationOnlineFileUrl(LPCTSTR documentName,PTCHAR buffer);
	PTCHAR GetApplicationOnlineHtmlDocumentUrl(LPCTSTR documentName,PTCHAR buffer);
	HMENU GetSubmenuByContainedCommand(HMENU hMenu,WORD cmd,PBYTE pOutSubmenuPosition=nullptr);
	HMENU CreateSubmenuByContainedCommand(UINT menuResourceId,WORD cmd,PBYTE pOutSubmenuPosition=nullptr);
	TStdWinError DownloadSingleFile(LPCTSTR onlineFileUrl,PBYTE fileDataBuffer,DWORD fileDataBufferLength,PDWORD pDownloadedFileSize,LPCTSTR fatalErrorConsequence);
	void RandomizeData(PVOID buffer,WORD nBytes);
}

#endif // UTILS_H
