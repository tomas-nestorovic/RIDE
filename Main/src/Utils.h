#ifndef UTILS_H
#define UTILS_H

#define MSG_HELP_CANCEL	_T("Don't need any help now.")

namespace TUtils{

	class CCommandDialog:public CDialog{
		const LPCTSTR information;
	protected:
		void __convertToCommandLikeButton__(HWND hButton,LPCTSTR text) const;
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

	extern const float LogicalUnitScaleFactor;

	PTCHAR __formatErrorCode__(PTCHAR buf,TStdWinError errCode);
	void FatalError(LPCTSTR text);
	void FatalError(LPCTSTR text,LPCTSTR causeOfError,LPCTSTR consequence=NULL);
	void FatalError(LPCTSTR text,TStdWinError causeOfError,LPCTSTR consequence=NULL);
	void Information(LPCTSTR text);
	void Information(LPCTSTR text,LPCTSTR causeOfError,LPCTSTR consequence=NULL);
	void Information(LPCTSTR text,TStdWinError causeOfError,LPCTSTR consequence=NULL);
	bool InformationWithCheckBox(LPCTSTR textInformation,LPCTSTR checkBoxCaption);
	void InformationWithCheckableShowNoMore(LPCTSTR text,LPCTSTR sectionId,LPCTSTR messageId);
	bool InformationOkCancel(LPCTSTR text);
	bool QuestionYesNo(LPCTSTR text,UINT defaultButton);
	BYTE QuestionYesNoCancel(LPCTSTR text,UINT defaultButton);
	BYTE QuestionYesNoCancel(LPCTSTR text,UINT defaultButton,LPCTSTR causeOfError,LPCTSTR consequence=NULL);
	BYTE QuestionYesNoCancel(LPCTSTR text,UINT defaultButton,TStdWinError causeOfError,LPCTSTR consequence=NULL);
	BYTE AbortRetryIgnore(LPCTSTR text,UINT defaultButton);
	BYTE AbortRetryIgnore(LPCTSTR text,TStdWinError causeOfError,UINT defaultButton,LPCTSTR consequence=NULL);
	BYTE AbortRetryIgnore(TStdWinError causeOfError,UINT defaultButton);
	bool RetryCancel(LPCTSTR text);
	bool RetryCancel(TStdWinError causeOfError);
	void Warning(LPCTSTR text);
	bool EnableDlgControls(HWND hDlg,PCWORD buttonIds,bool enabled);
	void NavigateToUrlInDefaultBrowser(LPCTSTR url);
	void DrawClosingCurlyBracket(HDC dc,int x,int yMin,int yMax);
	void WrapControlsByClosingCurlyBracketWithText(CWnd *wnd,const CWnd *pCtrlA,const CWnd *pCtrlZ,LPCTSTR text,DWORD textColor);
	void ConvertToSplitButton(HWND hStdBtn,PCSplitButtonAction pAction,BYTE nActions);
	void SetSingleCharTextUsingFont(HWND hWnd,WCHAR singleChar,LPCTSTR fontFace,int fontPointSize);
	void PopulateComboBoxWithSequenceOfNumbers(HWND hComboBox,BYTE iStartValue,LPCTSTR strStartValueDesc,BYTE iEndValue,LPCTSTR strEndValueDesc);
	float ScaleLogicalUnit(HDC dc);
	void UnscaleLogicalUnit(PINT values,BYTE nValues);
	CFile &WriteToFile(CFile &f,LPCTSTR text);
	CFile &WriteToFile(CFile &f,TCHAR chr);
	CFile &WriteToFile(CFile &f,int number,LPCTSTR formatting);
	CFile &WriteToFile(CFile &f,int number);
	CFile &WriteToFile(CFile &f,double number,LPCTSTR formatting);
	CFile &WriteToFile(CFile &f,double number);
	PTCHAR GetApplicationOnlineFileUrl(LPCTSTR documentName,PTCHAR buffer);
	PTCHAR GetApplicationOnlineHtmlDocumentUrl(LPCTSTR documentName,PTCHAR buffer);
	TStdWinError DownloadSingleFile(LPCTSTR onlineFileUrl,PBYTE fileDataBuffer,DWORD fileDataBufferLength,PDWORD pDownloadedFileSize,LPCTSTR fatalErrorConsequence);
}

#endif // UTILS_H
