#ifndef UTILS_H
#define UTILS_H

#define MSG_HELP_CANCEL	_T("Don't need any help now.")

typedef long TStdWinError; // Windows standard i/o error

namespace Utils{

	template<typename T,typename TIndex=int>
	class CCallocPtr:public std::unique_ptr<T,void (__cdecl *)(PVOID)>{
	public:
		TIndex length;

		CCallocPtr()
			: std::unique_ptr<T,void (__cdecl *)(PVOID)>( pointer(), ::free )
			, length(0) {
		}
		CCallocPtr(TIndex length)
			: std::unique_ptr<T,void (__cdecl *)(PVOID)>( (T *)::calloc(length,sizeof(T)), ::free )
			, length(length) {
		}
		CCallocPtr(TIndex length,int initByte)
			: std::unique_ptr<T,void (__cdecl *)(PVOID)>(  (T *)::memset( ::calloc(length,sizeof(T)), initByte, length*sizeof(T) ),  ::free  )
			, length(length) {
		}
		CCallocPtr(TIndex length,const T *pCopyInitData)
			: std::unique_ptr<T,void (__cdecl *)(PVOID)>(  (T *)::memcpy( ::calloc(length,sizeof(T)), pCopyInitData, length*sizeof(T) ),  ::free  )
			, length(length) {
		}
		CCallocPtr(CCallocPtr &&r)
			: std::unique_ptr<T,void (__cdecl *)(PVOID)>( std::move(r) )
			, length(r.length) {
		}

		inline operator bool() const{ return get()!=pointer(); }
		inline operator T *() const{ return get(); }
		inline T *operator+(TIndex i) const{ return get()+i; }
		inline T &operator[](TIndex i) const{ return get()[i]; }

		T *Realloc(TIndex newLength){
			if (const PVOID tmp=::realloc( get(), sizeof(T)*newLength )){ // enough memory for reallocation?
				if (tmp!=get()){ // had to move the memory block?
					release(); // already ::Freed, so don't call ::Free again
					reset( (T *)tmp );
					length=newLength;
				}
				return get();
			}else
				return nullptr; // currently allocated memory has not been affected
		}

		// 'for each' support
		inline T *begin() const{ return get(); }
		inline T *end() const{ return get()+length; }
	};

	// a workaround to template argument deduction on pre-2017 compilers
	template<typename T,typename TIndex>
	inline static CCallocPtr<T,typename std::tr1::decay<TIndex>::type> MakeCallocPtr(TIndex length){
		return CCallocPtr<T,typename std::tr1::decay<TIndex>::type>( length );
	}
	template<typename T,typename TIndex>
	inline static CCallocPtr<T,typename std::tr1::decay<TIndex>::type> MakeCallocPtr(TIndex length,int initByte){
		return CCallocPtr<T,typename std::tr1::decay<TIndex>::type>( length, initByte );
	}
	template<typename T,typename TIndex>
	inline static CCallocPtr<T,typename std::tr1::decay<TIndex>::type> MakeCallocPtr(TIndex length,const T *pCopyInitData){
		return CCallocPtr<T,typename std::tr1::decay<TIndex>::type>( length, pCopyInitData );
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
		CCopyList(){}
		CCopyList(const CCopyList &r){
			// shallow-copy ctor
			for( POSITION pos=r.GetHeadPosition(); pos; )
				__super::AddTail( static_cast<const CStringList &>(r).GetNext(pos) );
		}

		POSITION AddHead(const T &element,int elementSize=sizeof(T)){
			const POSITION pos=__super::AddHead(_T(""));
			SetAt( pos, element, elementSize );
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

	template<typename T>
	class CVarBackup{
		const T value0;
	protected:
		T &var;
	public:
		inline CVarBackup(T &var)
			: value0(var) , var(var) {
		}
		inline ~CVarBackup(){
			var=value0;
		}
	};

	template<typename T>
	class CVarTempReset:public CVarBackup<T>{
	public:
		inline CVarTempReset(T &var,const T &newValue)
			: CVarBackup(var) {
			var=newValue;
		}
		inline operator T() const{ return var; }
		inline T &operator->() const{ return var; }
	};

	template<typename T>
	T RoundDivUp(T value,T denominator){
		return (value+denominator-1)/denominator;
	}

	template<typename T>
	T RoundUpToMuls(T value,T mul){
		return RoundDivUp(value,mul)*mul;
	}

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
		void InitBy(const LOGFONT &lf);
	public:
		static const CRideFont Small, Std, StdBold;

		int charAvgWidth,charHeight;

		CRideFont(LPCTSTR face,int pointHeight,bool bold=false,bool dpiScaled=false,int pointWidth=0);
		CRideFont(HWND hWnd,bool bold=false);

		inline operator WPARAM() const{ return (WPARAM)m_hObject; }

		SIZE GetTextSize(LPCTSTR text,int textLength) const;
		SIZE GetTextSize(LPCTSTR text) const;
	};

	class CRideContextMenu sealed:public ::CMenu{
		CMenu parent;
	public:
		static void UpdateUI(CWnd *pUiUpdater,CMenu *pMenu);

		CRideContextMenu(UINT idMenuRes,CWnd *pUiUpdater=nullptr);
		CRideContextMenu(HMENU hMenuOwnedByCaller);
		~CRideContextMenu();

		CString GetMenuString(UINT uIDItem,UINT flags) const;
		inline void UpdateUi(CWnd *pUiUpdater){ UpdateUI(pUiUpdater,this); }
		inline CString GetMenuStringByCmd(WORD cmd) const{ return GetMenuString(cmd,MF_BYCOMMAND); }
		inline CString GetMenuStringByPos(WORD pos) const{ return GetMenuString(pos,MF_BYPOSITION); }
		inline void AppendSeparator(){ AppendMenu(MF_SEPARATOR); }
		void Insert(UINT uPosition,const CRideContextMenu &menu);
		bool InsertAfter(WORD existingId,UINT nFlags,UINT_PTR nIDNewItem,LPCTSTR lpszNewItem);
		inline void Prepend(const CRideContextMenu &menu){ Insert(0,menu); }
		void Append(const CRideContextMenu &menu);
		bool ModifySubmenu(UINT uPosition,HMENU hNewSubmenu);
		int GetPosByContainedSubcommand(WORD cmd) const;
	};

	typedef const struct TSplitButtonAction sealed{
		static const TSplitButtonAction HorizontalLine;

		WORD commandId;
		LPCTSTR commandCaption;
		WORD menuItemFlags;
	} *PCSplitButtonAction;

	class CRideDialog:public CDialog{
	protected:
		static LRESULT WINAPI SplitButton_WndProc(HWND hSplitBtn,UINT msg,WPARAM wParam,LPARAM lParam);
		static LRESULT WINAPI CommandLikeButton_WndProc(HWND hCmdBtn,UINT msg,WPARAM wParam,LPARAM lParam);

		class CRideDC:public CClientDC{
			int iDc0;
		public:
			CRect rect;

			CRideDC(const CRideDialog &d);
			CRideDC(const CRideDialog &d,WORD id);
			CRideDC(HWND hWnd);
		};

		CRideDialog();

		class CSplitterWnd:public ::CSplitterWnd{ // an overridden version to be used inside CRideDialog instances
		protected:
			BOOL OnCommand(WPARAM wParam,LPARAM lParam) override;
			BOOL OnNotify(WPARAM wParam,LPARAM lParam,LRESULT *pResult) override;
		public:
			CWnd *GetActivePane(int *pRow,int *pCol) override;
			void SetActivePane(int row,int col,CWnd *pWnd) override;
		};
	public:
		static bool BeepWhenShowed;

		static LPCTSTR GetDialogTemplateCaptionText(UINT idDlgRes,PTCHAR chars,WORD nCharsMax);
		static LPCTSTR GetDialogTemplateItemText(UINT idDlgRes,WORD idItem,PTCHAR chars,WORD nCharsMax);
		static WORD GetClickedHyperlinkId(LPARAM lNotify);
		static void DrawOpeningCurlyBracket(HDC dc,int x,int yMin,int yMax);
		static void DrawClosingCurlyBracket(HDC dc,int x,int yMin,int yMax);
		static void SetDlgItemSingleCharUsingFont(HWND hDlg,WORD id,WCHAR singleChar,HFONT hFont);
		static void ConvertToCommandLikeButton(HWND hStdBtn,WCHAR wingdingsGlyphBeforeText=0xf0e0,COLORREF textColor=0,int glyphPointSizeIncrement=0,COLORREF glyphColor=0,bool compactPath=true); // 0xf0e0 = thin arrow right
		static void ConvertToCancelLikeButton(HWND hStdBtn,COLORREF textColor=0,int glyphPointSizeIncrement=0,COLORREF glyphColor=0);
		template<typename T> static T GetWindowUserData(HWND hWnd){ return (T)::GetWindowLongW( hWnd, GWL_USERDATA ); }
		template<typename T> static void SetWindowUserData(HWND hWnd,const T &data){ ::SetWindowLongW( hWnd, GWL_USERDATA, (long)data ); }

		CRideDialog(LPCTSTR lpszTemplateName,const CWnd *pParentWnd=nullptr);
		CRideDialog(UINT nIDTemplate,const CWnd *pParentWnd=nullptr);

		INT_PTR DoModal() override;
		HWND GetDlgItemHwnd(WORD id) const;
		int GetDlgItemTextLength(WORD id) const;
		void SetDlgItemText(WORD id,LPCTSTR text) const;
		bool IsDlgItemShown(WORD id) const;
		bool CheckDlgItem(WORD id,bool checked=true) const;
		bool IsDlgItemChecked(WORD id) const;
		bool EnableDlgItem(WORD id,bool enabled=true) const;
		bool EnableDlgItems(PCWORD pIds,bool enabled) const;
		void CheckAndEnableDlgItem(WORD id,bool check,bool enable) const;
		void CheckAndEnableDlgItem(WORD id,bool checkAndEnable) const;
		bool ShowDlgItem(WORD id,bool show=true) const;
		bool ShowDlgItems(PCWORD pIds,bool show=true) const;
		void FocusDlgItem(WORD id) const;
		bool IsDlgItemEnabled(WORD id) const;
		void ModifyDlgItemStyle(WORD id,UINT addedStyle,UINT removedStyle=0) const;
		RECT GetDlgItemClientRect(WORD id) const;
		RECT MapDlgItemClientRect(WORD id) const;
		RECT MapDlgItemClientRect(HWND hItem) const;
		POINT MapDlgItemClientOrigin(WORD id) const;
		void OffsetDlgItem(WORD id,int dx,int dy) const;
		void SetDlgItemPos(HWND itemHwnd,int x,int y,int cx=0,int cy=0) const;
		void SetDlgItemPos(HWND itemHwnd,const RECT &rc) const;
		void SetDlgItemPos(WORD id,int x,int y,int cx=0,int cy=0) const;
		void SetDlgItemPos(WORD id,const RECT &rc) const;
		void SetDlgItemSize(WORD id,int cx,int cy) const;
		void SetDlgItemFont(WORD id,const CRideFont &font) const;
		template<typename T> T GetDlgItemUserData(WORD id) const{ return GetWindowUserData( GetDlgItemHwnd(id) ); }
		template<typename T> void SetDlgItemUserData(WORD id,const T &data) const{ SetWindowUserData( GetDlgItemHwnd(id), data ); }
		void InvalidateDlgItem(WORD id) const;
		void InvalidateDlgItem(HWND hItem) const;
		LONG_PTR GetDlgComboBoxSelectedValue(WORD id) const;
		bool SelectDlgComboBoxValue(WORD id,LONG_PTR value,bool cancelPrevSelection=true) const;
		int GetDlgComboBoxSelectedIndex(WORD id) const;
		void AppendDlgComboBoxValue(WORD id,LONG_PTR value,LPCTSTR text) const;
		int GetDlgListBoxSelectedIndex(WORD id) const;
		void WrapDlgItemsByOpeningCurlyBracket(WORD idA,WORD idZ) const;
		void WrapDlgItemsByClosingCurlyBracketWithText(WORD idA,WORD idZ,LPCTSTR text,DWORD textColor) const;
		CString CompactPathToFitInDlgItem(WORD id,LPCTSTR fullpath) const;
		void SetDlgItemCompactPath(WORD id,LPCTSTR fullpath) const;
		void SetDlgItemFormattedText(WORD id,LPCTSTR format,...) const;
		void SetDlgItemSingleCharUsingFont(WORD id,WCHAR singleChar,HFONT hFont) const;
		void SetDlgItemSingleCharUsingFont(WORD id,WCHAR singleChar,LPCTSTR fontFace,int fontPointSize) const;
		void PopulateDlgComboBoxWithSequenceOfNumbers(WORD cbId,BYTE iStartValue,LPCTSTR strStartValueDesc,BYTE iEndValue,LPCTSTR strEndValueDesc) const;
		void ConvertDlgButtonToSplitButtonEx(WORD id,PCSplitButtonAction pAction,BYTE nActions) const;
		void ConvertDlgCheckboxToHyperlink(WORD id) const;
		bool GetDlgItemIntList(WORD id,CIntList &rOutList,const PropGrid::Integer::TUpDownLimits &limits,int nIntsMin=0,int nIntsMax=INT_MAX) const;
		void SetDlgItemIntList(WORD id,const CIntList &list) const;
		void DDX_CheckEnable(CDataExchange *pDX,int nIDC,bool &value,bool enable) const;

		template <typename T>
		void DDX_CBValue(CDataExchange *pDX,WORD id,T &value) const{
			if (pDX->m_bSaveAndValidate)
				value=(T)GetDlgComboBoxSelectedValue(id);
			else
				SelectDlgComboBoxValue( id, value );
		}

		template <size_t N>
		void ConvertDlgButtonToSplitButton(WORD id,const TSplitButtonAction (&actions)[N]) const{
			ConvertDlgButtonToSplitButtonEx( id, actions, N );
		}

		template <size_t N>
		int GetDlgItemText(WORD id,TCHAR (&buffer)[N]) const{
			return ::GetDlgItemText( m_hWnd, id, buffer, N );
		}

		template <size_t N>
		int GetDlgItemTextW(WORD id,WCHAR (&buffer)[N]) const{
			return ::GetDlgItemTextW( m_hWnd, id, buffer, N );
		}
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
	protected:
		void AddButton(WORD id,LPCTSTR caption,WCHAR wingdingsGlyphBeforeText);
		void AddCommandButton(WORD id,LPCTSTR caption,bool defaultCommand=false);
		void AddHelpButton(WORD id,LPCTSTR caption);
		void AddCancelButton(LPCTSTR caption=STR_CANCEL);
		void AddCheckBox(LPCTSTR caption);

		CCommandDialog(LPCTSTR _information);
		CCommandDialog(WORD dialogId,LPCTSTR _information);

		BOOL OnInitDialog() override;
		void DoDataExchange(CDataExchange *pDX) override;
		LRESULT WindowProc(UINT msg,WPARAM wParam,LPARAM lParam) override;
	public:
		bool checkBoxTicked;
	};

	class CSimpleCommandDialog:public CCommandDialog{
	public:
		typedef const struct TCmdButtonInfo{
			WORD id;
			LPCTSTR caption;
		} *PCCmdButtonInfo;
	protected:
		const PCCmdButtonInfo buttons;
		const BYTE nButtons;
		const LPCTSTR cancelButtonCaption;

		BOOL OnInitDialog() override;
	public:
		CSimpleCommandDialog(LPCTSTR information,PCCmdButtonInfo buttons,BYTE nButtons,LPCTSTR cancelButtonCaption=STR_CANCEL);
	};

	class CByteIdentity sealed{
		BYTE values[(BYTE)-1+1];
	public:
		CByteIdentity();

		operator PCBYTE() const;
	};

	class CRideTime:public SYSTEMTIME{
		CRideTime(const time_t &t);

		operator time_t() const;
		CRideTime operator-(const time_t &t2) const;
	public:
		static const CRideTime None;

		CRideTime(); // current local time
		CRideTime(const FILETIME &t);

		operator FILETIME() const;
		bool operator==(const FILETIME &t2) const;
		bool operator!=(const FILETIME &t2) const;
		CRideTime operator-(const FILETIME &t2) const;
		CRideTime operator-(const CRideTime &t2) const;

		int ToMilliseconds() const;
		WORD GetDosDate() const;
		DWORD GetDosDateTime() const;
		CString DateToStdString() const;
		CString TimeToStdString() const;
		CRideTime ToTzSpecificLocalTime() const;
		bool Edit(bool dateEditingEnabled,bool timeEditingEnabled,const SYSTEMTIME *epoch);
	};

	class CAxis{
	protected:
		const TLogValue logValuePerUnit;
		TLogValue logLength;
		BYTE zoomFactor;
		class CDcState sealed{
			int mappingMode,graphicsMode;
			int nUnitsStart;
			POINT ptViewportOrg;
			XFORM advanced;
		public:
			inline CDcState(){}
			CDcState(HDC dc,int nUnitsStart);

			int ApplyTo(HDC dc) const;
			void RevertFrom(HDC dc,int iSavedDc) const;
		} dcState; // for any subsequent drawing, e.g. cursor indication
		TLogValue logCursorPos;

		TLogValue PixelToValue(int pixel) const;
	public:
		const enum TVerticalAlign{
			NONE,
			TOP,
			BOTTOM
		} ticksAndLabelsAlign;
		const TCHAR unit;
		const LPCTSTR unitPrefixes;

		static const TCHAR NoPrefixes[];
		static const TCHAR CountPrefixes[];
		static const CRideFont FontWingdings;

		CAxis(TLogValue logLength,TLogTime logValuePerUnit,TCHAR unit,LPCTSTR unitPrefixes,BYTE initZoomFactor,TVerticalAlign ticksAndLabelsAlign=TVerticalAlign::TOP);

		BYTE Draw(HDC dc,long nVisiblePixels,const CRideFont &font,int primaryGridLength=0,HPEN hPrimaryGridPen=nullptr,PLogTime pOutVisibleStart=nullptr,PLogTime pOutVisibleEnd=nullptr);
		inline TLogValue GetCursorPos() const{ return logCursorPos; }
		void SetCursorPos(HDC dc,TLogValue newLogPos);
		int GetUnitCount(TLogValue logValue,BYTE zoomFactor) const;
		int GetUnitCount(TLogValue logValue) const;
		int GetUnitCount() const;
		TLogValue GetValue(int nUnits) const;
		inline TLogValue GetLength() const{ return logLength; }
		void SetLength(TLogValue newLogLength);
		inline BYTE GetZoomFactor() const{ return zoomFactor; }
		BYTE GetZoomFactorToFitWidth(int nUnits,BYTE zoomFactorMax) const;
		BYTE GetZoomFactorToFitWidth(TLogValue logValue,int nUnits,BYTE zoomFactorMax) const;
		void SetZoomFactor(BYTE newZoomFactor);
		int ValueToReadableString(TLogValue logValue,PTCHAR buffer) const;
		CString ValueToReadableString(TLogValue logValue) const;
		inline CString CursorValueToReadableString() const{ return ValueToReadableString(logCursorPos); }
	};

	class CTimeline:public CAxis{
	public:
		static const TCHAR TimePrefixes[];

		CTimeline(TLogTime logTimeLength,TLogTime logTimePerUnit,BYTE initZoomFactor);

		void Draw(HDC dc,const CRideFont &font,PLogTime pOutVisibleStart=nullptr,PLogTime pOutVisibleEnd=nullptr);

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
		inline TRationalNumber Simplify() const{
			TRationalNumber result=*this;
			while (((result.quot|result.rem)&1)==0)
				result.quot>>=1, result.rem>>=1;
			return result;
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
	inline TRationalNumber operator/(long long lhs,const TRationalNumber &rhs){
		return	TRationalNumber( rhs.rem*lhs, rhs.quot );
	}
	inline POINT operator/(const POINT &pt,const TRationalNumber &rhs){
		return	CPoint( pt.x/rhs, pt.y/rhs );
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


	bool IsVistaOrNewer();
	TStdWinError ErrorByOs(TStdWinError vistaOrNewer,TStdWinError xpOrOlder);
#ifdef UNICODE
#else
	inline LPCSTR ToStringA(LPCTSTR s){ return s; }
	inline LPCTSTR ToStringT(LPCSTR s){ return s; }
	CString ToStringT(LPCWSTR lpsz); // converts to UTF-8
#endif
	CString SimpleFormat(LPCTSTR format,va_list v);
	CString SimpleFormat(LPCTSTR format,LPCTSTR param);
	CString SimpleFormat(LPCTSTR format,LPCTSTR param1,LPCTSTR param2);
	CString SimpleFormat(LPCTSTR format,LPCTSTR param1,int param2);
	CString SimpleFormat(LPCTSTR format,int param1,LPCTSTR param2);
	CString SimpleFormat(LPCTSTR format,int param1,int param2,LPCTSTR param3);
	CString SimpleFormat(LPCTSTR format,int param1,int param2,int param3);
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
	void ScaleLogicalUnit(PINT values,BYTE nValues);
	void UnscaleLogicalUnit(PINT values,BYTE nValues);
	COLORREF GetSaturatedColor(COLORREF color,float saturationFactor);
	COLORREF GetBlendedColor(COLORREF color1,COLORREF color2,float blendFactor=.5f);
	BYTE GetReversedByte(BYTE b);
	BYTE CountSetBits(WORD w);
	CString GenerateTemporaryFileName();
	CString GetCommonHtmlHeadStyleBody(COLORREF bodyBg=CLR_DEFAULT,LPCTSTR tableStyle=_T("table{border:1pt solid black;spacing:10pt}"));
	CFile &WriteToFile(CFile &f,LPCTSTR text);
	CFile &WriteToFileFormatted(CFile &f,LPCTSTR format,...);
	CFile &WriteToFile(CFile &f,TCHAR chr);
	CFile &WriteToFile(CFile &f,int number,LPCTSTR formatting);
	CFile &WriteToFile(CFile &f,int number);
	CFile &WriteToFile(CFile &f,double number,LPCTSTR formatting);
	CFile &WriteToFile(CFile &f,double number);
	PTCHAR GetApplicationOnlineFileUrl(LPCTSTR documentName,PTCHAR buffer);
	PTCHAR GetApplicationOnlineHtmlDocumentUrl(LPCTSTR documentName,PTCHAR buffer);
	TStdWinError DownloadSingleFile(LPCTSTR onlineFileUrl,PBYTE fileDataBuffer,DWORD fileDataBufferLength,PDWORD pDownloadedFileSize,LPCTSTR fatalErrorConsequence);
	void RandomizeData(PVOID buffer,WORD nBytes);
	WNDPROC SubclassWindow(HWND hWnd,WNDPROC newWndProc);
	WNDPROC SubclassWindowW(HWND hWnd,WNDPROC newWndProc);
	CString DoPromptSingleTypeFileName(LPCTSTR defaultSaveName,LPCTSTR singleFilter,DWORD flags=0);
	void StdBeep();
}

void DDX_Check(CDataExchange *pDX,int nIDC,bool &value);

template<typename T>
void DDX_CBIndex(CDataExchange *pDX,int nIDC,T &index){
	int tmp=index;
		DDX_CBIndex( pDX, nIDC, tmp );
	index=(T)tmp;
}

#endif // UTILS_H
