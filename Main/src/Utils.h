#ifndef UTILS_H
#define UTILS_H

#define MSG_HELP_CANCEL	_T("Don't need any help now.")

typedef long TStdWinError; // Windows standard i/o error

namespace Utils{

	template<typename T,typename TCount=int>
	class CCallocPtr:public std::unique_ptr<T,void (__cdecl *)(PVOID)>{
	public:
		CCallocPtr()
			: std::unique_ptr<T,void (__cdecl *)(PVOID)>( pointer(), ::free ) {
		}
		CCallocPtr(TCount count)
			: std::unique_ptr<T,void (__cdecl *)(PVOID)>( (T *)::calloc(count,sizeof(T)), ::free ) {
		}
		CCallocPtr(TCount count,int initByte)
			: std::unique_ptr<T,void (__cdecl *)(PVOID)>(  (T *)::memset( ::calloc(count,sizeof(T)), initByte, count*sizeof(T) ),  ::free  ) {
		}
		CCallocPtr(TCount count,const T *pCopyInitData)
			: std::unique_ptr<T,void (__cdecl *)(PVOID)>(  (T *)::memcpy( ::calloc(count,sizeof(T)), pCopyInitData, count*sizeof(T) ),  ::free  ) {
		}

		inline operator bool() const{ return get()!=pointer(); }
		inline operator T *() const{ return get(); }
		inline T *operator+(TCount i) const{ return get()+i; }
		inline T &operator[](TCount i) const{ return get()[i]; }

		T *Realloc(TCount newCount){
			if (const PVOID tmp=::realloc( get(), sizeof(T)*newCount )){ // enough memory for reallocation?
				if (tmp!=get()){ // had to move the memory block?
					release(); // already ::Freed, so don't call ::Free again
					reset( (T *)tmp );
				}
				return get();
			}else
				return nullptr; // currently allocated memory has not been affected
		}
	};

	// a workaround to template argument deduction on pre-2017 compilers
	template<typename T,typename TCount>
	inline static CCallocPtr<T,typename std::tr1::decay<TCount>::type> MakeCallocPtr(TCount count){
		return CCallocPtr<T,typename std::tr1::decay<TCount>::type>( count );
	}
	template<typename T,typename TCount>
	inline static CCallocPtr<T,typename std::tr1::decay<TCount>::type> MakeCallocPtr(TCount count,int initByte){
		return CCallocPtr<T,typename std::tr1::decay<TCount>::type>( count, initByte );
	}
	template<typename T,typename TCount>
	inline static CCallocPtr<T,typename std::tr1::decay<TCount>::type> MakeCallocPtr(TCount count,const T *pCopyInitData){
		return CCallocPtr<T,typename std::tr1::decay<TCount>::type>( count, pCopyInitData );
	}

	template<typename Ptr>
	class CPtrList:public ::CPtrList{
	public:
		inline POSITION AddHead(Ptr newElement){ return __super::AddHead((PVOID)newElement); }
		inline POSITION AddTail(Ptr newElement){ return __super::AddTail((PVOID)newElement); }
		inline Ptr &GetNext(POSITION &rPosition){ return (Ptr &)__super::GetNext(rPosition); }
		inline Ptr GetNext(POSITION &rPosition) const{ return (Ptr)__super::GetNext(rPosition); }
		inline Ptr &GetPrev(POSITION &rPosition){ return (Ptr &)__super::GetPrev(rPosition); }
		inline Ptr GetPrev(POSITION &rPosition) const{ return (Ptr)__super::GetPrev(rPosition); }
		inline Ptr &GetHead(){ return (Ptr &)__super::GetHead(); }
		inline Ptr GetHead() const{ return (Ptr)__super::GetHead(); }
		inline Ptr &GetTail(){ return (Ptr &)__super::GetTail(); }
		inline Ptr GetTail() const{ return (Ptr)__super::GetTail(); }
		inline Ptr RemoveHead(){ return (Ptr)__super::RemoveHead(); }
		inline Ptr RemoveTail(){ return (Ptr)__super::RemoveTail(); }
		inline Ptr &GetAt(POSITION position){ return (Ptr &)__super::GetAt(position); }
		inline Ptr GetAt(POSITION position) const{ return (Ptr)__super::GetAt(position); }
		inline void SetAt(POSITION pos,Ptr newElement){ return __super::SetAt(pos,(PVOID)newElement); }
		inline POSITION InsertBefore(POSITION position,Ptr newElement){ return __super::InsertBefore(position,(PVOID)newElement); }
		inline POSITION InsertAfter(POSITION position,Ptr newElement){ return __super::InsertAfter(position,(PVOID)newElement); }
	};

	typedef CPtrList<int> CIntList;

	template<typename T>
	class CCopyList:public CStringList{
	public:
		POSITION AddHead(const T &element,int elementSize=sizeof(T)){
			const POSITION pos=__super::AddHead(_T(""));
			::memcpy( __super::GetAt(pos).GetBuffer(elementSize), &element, elementSize );
			return pos;
		}
		POSITION AddTail(const T &element,int elementSize=sizeof(T)){
			const POSITION pos=__super::AddTail(_T(""));
			SetAt( pos, element, elementSize );
			return pos;
		}
		T &GetNext(POSITION &rPosition) const{
			return *(T *)__super::GetNext(rPosition).operator LPCTSTR();
		}
		T &GetPrev(POSITION &rPosition) const{
			return *(T *)__super::GetPrev(rPosition).operator LPCTSTR();
		}
		T &GetHead() const{
			return *(T *)__super::GetHead().operator LPCTSTR();
		}
		T &GetTail() const{
			return *(T *)__super::GetTail().operator LPCTSTR();
		}
		T &GetAt(POSITION position) const{
			return *(T *)__super::GetAt(position).operator LPCTSTR();
		}
		void SetAt(POSITION pos,const T &element,int elementSize=sizeof(T)){
			::memcpy( __super::GetAt(pos).GetBuffer(elementSize), &element, elementSize );
		}
		POSITION InsertBefore(POSITION pos,const T &element,int elementSize=sizeof(T)){
			pos=__super::InsertBefore(pos,_T(""));
			SetAt( pos, element, elementSize );
			return pos;
		}
		POSITION InsertAfter(POSITION pos,const T &element,int elementSize=sizeof(T)){
			pos=__super::InsertAfter(pos,_T(""));
			SetAt( pos, element, elementSize );
			return pos;
		}
	};

	class CExclusivelyLocked{
		CSyncObject &syncObj;
	public:
		CExclusivelyLocked(CSyncObject &syncObj);
		~CExclusivelyLocked();
	};

	class CRidePen sealed:public ::CPen{
	public:
		static const CRidePen BlackHairline, WhiteHairline, RedHairline;

		CRidePen(BYTE thickness,COLORREF color);
		CRidePen(BYTE thickness,COLORREF color,UINT style);
	};

	class CRideBrush sealed:public ::CBrush{
	public:
		static const CRideBrush None, Black, White, BtnFace, Selection;

		CRideBrush(int stockObjectId);
		CRideBrush(COLORREF solidColor);
		CRideBrush(bool sysColor,int sysColorId);
		CRideBrush(CRideBrush &&r);

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

	class CRideContextMenu sealed:public ::CMenu{
		CMenu parent;
	public:
		static void UpdateUI(CWnd *pUiUpdater,CMenu *pMenu);

		CRideContextMenu(UINT idMenuRes,CWnd *pUiUpdater=nullptr);
		~CRideContextMenu();

		CString GetMenuString(UINT uIDItem,UINT flags) const;
		inline void UpdateUi(CWnd *pUiUpdater){ UpdateUI(pUiUpdater,this); }
		inline CString GetMenuStringByCmd(WORD cmd) const{ return GetMenuString(cmd,MF_BYCOMMAND); }
		inline CString GetMenuStringByPos(WORD pos) const{ return GetMenuString(pos,MF_BYPOSITION); }
	};

	typedef const struct TSplitButtonAction sealed{
		static const TSplitButtonAction HorizontalLine;

		WORD commandId;
		LPCTSTR commandCaption;
		WORD menuItemFlags;
	} *PCSplitButtonAction;

	class CRideDialog:public CDialog{
	protected:
		CRideDialog();
	public:
		static LPCTSTR GetDialogTemplateCaptionText(UINT idDlgRes,PTCHAR chars,WORD nCharsMax);
		static LPCTSTR GetDialogTemplateItemText(UINT idDlgRes,WORD idItem,PTCHAR chars,WORD nCharsMax);
		static void DrawClosingCurlyBracket(HDC dc,int x,int yMin,int yMax);
		static void SetDlgItemSingleCharUsingFont(HWND hDlg,WORD id,WCHAR singleChar,HFONT hFont);
		static void ConvertToCommandLikeButton(HWND hStdBtn,WCHAR wingdingsGlyphBeforeText=0xf0e8,COLORREF textColor=0,int glyphPointSizeIncrement=0,COLORREF glyphColor=0); // 0xf0e8 = arrow right

		CRideDialog(LPCTSTR lpszTemplateName,const CWnd *pParentWnd=nullptr);
		CRideDialog(UINT nIDTemplate,const CWnd *pParentWnd=nullptr);

		INT_PTR DoModal() override;
		HWND GetDlgItemHwnd(WORD id) const;
		bool EnableDlgItem(WORD id,bool enabled=true) const;
		bool EnableDlgItems(PCWORD pIds,bool enabled) const;
		bool ShowDlgItem(WORD id,bool show=true) const;
		bool ShowDlgItems(PCWORD pIds,bool show=true) const;
		void FocusDlgItem(WORD id) const;
		bool IsDlgItemEnabled(WORD id) const;
		RECT GetDlgItemClientRect(WORD id) const;
		RECT MapDlgItemClientRect(WORD id) const;
		POINT MapDlgItemClientOrigin(WORD id) const;
		void OffsetDlgItem(WORD id,int dx,int dy) const;
		void SetDlgItemPos(WORD id,int x,int y,int cx=0,int cy=0) const;
		void SetDlgItemPos(WORD id,const RECT &rc) const;
		void SetDlgItemSize(WORD id,int cx,int cy) const;
		LONG_PTR GetDlgComboBoxSelectedValue(WORD id) const;
		bool SelectDlgComboBoxValue(WORD id,LONG_PTR value) const;
		int GetDlgComboBoxSelectedIndex(WORD id) const;
		int GetDlgListBoxSelectedIndex(WORD id) const;
		void WrapDlgItemsByClosingCurlyBracketWithText(WORD idA,WORD idZ,LPCTSTR text,DWORD textColor) const;
		void SetDlgItemFormattedText(WORD id,LPCTSTR format,...) const;
		void SetDlgItemSingleCharUsingFont(WORD id,WCHAR singleChar,HFONT hFont) const;
		void SetDlgItemSingleCharUsingFont(WORD id,WCHAR singleChar,LPCTSTR fontFace,int fontPointSize) const;
		void PopulateDlgComboBoxWithSequenceOfNumbers(WORD cbId,BYTE iStartValue,LPCTSTR strStartValueDesc,BYTE iEndValue,LPCTSTR strEndValueDesc) const;
		void ConvertDlgButtonToSplitButton(WORD id,PCSplitButtonAction pAction,BYTE nActions) const;
		bool GetDlgItemIntList(WORD id,CIntList &rOutList,const PropGrid::Integer::TUpDownLimits &limits,UINT nIntsMin=0,UINT nIntsMax=INT_MAX) const;
		void SetDlgItemIntList(WORD id,const CIntList &list) const;
	};

	class CSingleNumberDialog sealed:public CRideDialog{
		const LPCTSTR caption,label;
		const PropGrid::Integer::TUpDownLimits &range;
		int hexa;
	protected:
		bool GetCurrentValue(int &outValue) const;
		void PreInitDialog() override;
		void DoDataExchange(CDataExchange *pDX) override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		int Value;

		CSingleNumberDialog(LPCTSTR caption,LPCTSTR label,const PropGrid::Integer::TUpDownLimits &range,int initValue,bool hexa,CWnd *pParent);

		operator bool() const;
	};

	class CCommandDialog:public CRideDialog{
		const LPCTSTR information;
		WORD defaultCommandId;
	protected:
		void AddButton(WORD id,LPCTSTR caption,WCHAR wingdingsGlyphBeforeText);
		void AddCommandButton(WORD id,LPCTSTR caption,bool defaultCommand=false);
		void AddHelpButton(WORD id,LPCTSTR caption);
		void AddCancelButton(LPCTSTR caption=_T("Cancel"));
		void AddCheckBox(LPCTSTR caption);

		CCommandDialog(LPCTSTR _information);
		CCommandDialog(WORD dialogId,LPCTSTR _information);

		BOOL OnInitDialog() override;
		void DoDataExchange(CDataExchange *pDX) override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		int checkBoxStatus;
	};

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

	class CAxis{
	protected:
		const TLogValue logValuePerUnit;

		TLogValue PixelToValue(int pixel) const;
	public:
		enum TVerticalAlign{
			NONE,
			TOP,
			BOTTOM
		};

		static const TCHAR CountPrefixes[];

		const TLogValue logLength;
		BYTE zoomFactor;

		CAxis(TLogValue logLength,TLogTime logValuePerUnit,BYTE initZoomFactor);
		CAxis(TLogValue logLength,TLogTime logValuePerUnit,int nUnitsToFitIn,BYTE zoomFactorMax);

		BYTE Draw(HDC dc,long nVisiblePixels,TCHAR unit,LPCTSTR unitPrefixes,const CRideFont &font,TVerticalAlign ticksAndLabelsAlign=TVerticalAlign::TOP,int primaryGridLength=0,HPEN hPrimaryGridPen=nullptr,PLogTime pOutVisibleStart=nullptr,PLogTime pOutVisibleEnd=nullptr) const;
		int GetUnitCount(TLogValue logValue,BYTE zoomFactor) const;
		int GetUnitCount(TLogValue logValue) const;
		int GetUnitCount() const;
		TLogValue GetValue(int nUnits) const;
		BYTE GetZoomFactorToFitWidth(int nUnits,BYTE zoomFactorMax) const;
	};

	class CTimeline:public CAxis{
	public:
		static const TCHAR TimePrefixes[];

		CTimeline(TLogTime logTimeLength,TLogTime logTimePerUnit,BYTE initZoomFactor);

		int TimeToReadableString(TLogTime logTime,PTCHAR buffer) const;
		void Draw(HDC dc,const CRideFont &font,PLogTime pOutVisibleStart=nullptr,PLogTime pOutVisibleEnd=nullptr) const;

		inline TLogTime GetTime(int nUnits) const{
			return GetValue(nUnits);
		}
	};

	struct TRationalNumber{
		long long quot,rem;

		inline TRationalNumber(long long quot,long long rem)
			: quot(quot) , rem(rem) {
		}

		inline operator int() const{
			return quot/rem;
		}
		inline TRationalNumber operator*(BYTE i) const{
			return TRationalNumber( quot*i, rem );
		}
		inline TRationalNumber operator*(short i) const{
			return TRationalNumber( quot*i, rem );
		}
		inline TRationalNumber operator*(int i) const{
			return TRationalNumber( quot*i, rem );
		}
		inline TRationalNumber operator*(long i) const{
			return TRationalNumber( quot*i, rem );
		}
		inline TRationalNumber operator/(int i) const{
			return TRationalNumber( quot, rem*i );
		}
		inline bool operator!=(int i) const{
			return i*rem!=quot;
		}
	};

	inline int &operator*=(int &lhs,const TRationalNumber &rhs){
		return lhs=rhs*lhs;
	}
	inline long &operator*=(long &lhs,const TRationalNumber &rhs){
		return lhs=rhs*lhs;
	}
	inline TRationalNumber operator/(int lhs,const TRationalNumber &rhs){
		return	TRationalNumber( rhs.rem*lhs, rhs.quot );
	}
	inline TRationalNumber operator/(long lhs,const TRationalNumber &rhs){
		return	TRationalNumber( rhs.rem*lhs, rhs.quot );
	}
	inline int &operator/=(int &lhs,const TRationalNumber &rhs){
		return lhs=operator/(lhs,rhs);
	}
	inline long &operator/=(long &lhs,const TRationalNumber &rhs){
		return lhs=operator/(lhs,rhs);
	}

	extern const struct TLogicalUnitScaleFactor sealed:public TRationalNumber{
		TLogicalUnitScaleFactor();
	} LogicalUnitScaleFactor;

	class CBigEndianWord sealed{
		BYTE highByte,lowByte;
	public:
		inline CBigEndianWord(){}
		CBigEndianWord(WORD initLittleEndianValue);

		WORD operator=(WORD newValue);
		operator WORD() const;

		inline WORD GetBigEndian() const{
			return *(PCWORD)this;
		}
	};

	class CBigEndianDWord sealed{
		CBigEndianWord highWord,lowWord;
	public:
		DWORD operator=(DWORD newValue);
		operator DWORD() const;
	};


	TStdWinError ErrorByOs(TStdWinError vistaOrNewer,TStdWinError xpOrOlder);
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
	void BytesToHigherUnits(DWORD bytes,float &rHigherUnit,LPCTSTR &rHigherUnitName);
	CString BytesToHexaText(PCBYTE bytes,BYTE nBytes,bool lastDelimitedWithAnd);
	CString BytesToHexaText(const char *chars,BYTE nChars,bool lastDelimitedWithAnd);
	void NavigateToUrlInDefaultBrowser(LPCTSTR url);
	void ScaleLogicalUnit(HDC dc);
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
