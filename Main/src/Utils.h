#ifndef UTILS_H
#define UTILS_H

#define MSG_HELP_CANCEL	_T("Don't need any help now.")

typedef long TStdWinError; // Windows standard i/o error

namespace Utils{

	template<typename T,typename TIndex=int>
	class CSharedPodPtr:protected CString{ // 'std::shared_ptr'-like pointer to Plain Old Data
	public:
		TIndex length;

		CSharedPodPtr()
			: length(0) {
		}
		CSharedPodPtr(TIndex length)
			: length(length) {
			GetBufferSetLength( (sizeof(T)*length+sizeof(TCHAR)-1)/sizeof(TCHAR) );
		}
		CSharedPodPtr(TIndex length,int initByte)
			: length(length) {
			::memset(
				GetBufferSetLength( (sizeof(T)*length+sizeof(TCHAR)-1)/sizeof(TCHAR) ),
				initByte, sizeof(T)*length
			);
		}
		CSharedPodPtr(TIndex length,const T *pCopyInitData)
			: length(length) {
			::memcpy(
				GetBufferSetLength( (sizeof(T)*length+sizeof(TCHAR)-1)/sizeof(TCHAR) ),
				pCopyInitData, sizeof(T)*length
			);
		}

		inline operator bool() const{ return length>0; }
		inline operator T *() const{ return (T *)operator LPCTSTR(); }
		inline operator LPCVOID() const{ return begin(); }
		inline T *operator+(TIndex i) const{ return begin()+i; }
		inline T &operator[](TIndex i) const{ return begin()[i]; }

		inline void reset(){ Empty(), length=0; }

		T *Realloc(TIndex newLength){
			if (newLength){
				const CSharedPodPtr tmp(newLength);
				::memcpy( tmp.begin(), begin(), sizeof(T)*std::min(length,newLength) );
				return (*this=tmp);
			}else{ // the special case for which the above would fail
				*this=CSharedPodPtr();
				return nullptr;
			}
		}

		template<typename V,class Predicate>
		T *LowerBound(const V &v,Predicate p) const{
			return std::lower_bound( begin(), end(), v, p );
		}

		// 'for each' support
		inline T *begin() const{ return operator T *(); }
		inline T *end() const{ return begin()+length; }
	};

	// a workaround to template argument deduction on pre-2017 compilers
	template<typename T,typename TIndex>
	inline static CSharedPodPtr<T,typename std::tr1::decay<TIndex>::type> MakeSharedPodPtr(TIndex length){
		return CSharedPodPtr<T,typename std::tr1::decay<TIndex>::type>( length );
	}
	template<typename T,typename TIndex>
	inline static CSharedPodPtr<T,typename std::tr1::decay<TIndex>::type> MakeSharedPodPtr(TIndex length,int initByte){
		return CSharedPodPtr<T,typename std::tr1::decay<TIndex>::type>( length, initByte );
	}
	template<typename T,typename TIndex>
	inline static CSharedPodPtr<T,typename std::tr1::decay<TIndex>::type> MakeSharedPodPtr(TIndex length,const T *pCopyInitData){
		return CSharedPodPtr<T,typename std::tr1::decay<TIndex>::type>( length, pCopyInitData );
	}

	template<typename Ptr>
	class CPtrList:public ::CPtrList{
	public:
		inline POSITION AddHead(Ptr newElement){ return __super::AddHead((PVOID)newElement); }
		inline POSITION AddTail(Ptr newElement){ return __super::AddTail((PVOID)newElement); }
		inline bool Contains(Ptr element) const{ return Find(element)!=nullptr; }
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
		bool valid;
	protected:
		T &var;
	public:
		inline CVarBackup(T &var)
			: value0(var) , var(var) , valid(true) {
		}
		inline ~CVarBackup(){
			if (valid)
				var=value0;
		}
		inline void Invalidate(){
			valid=false;
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

#ifdef RELEASE_MFC42
	//#pragma optimize("",off) // optimizations off for RoundDivUp ('_alldvrm' routine may not be found for 'long long' arguments, must use '_alldiv' and '_allrem' instead)
#endif
	template<typename T>
	T RoundDivUp(const T value,const T denominator){
		//return (value+denominator-1)/denominator; // this may cause range overrun, e.g. when rounding 1.5 second up to whole seconds (1,500,000,000 + 1,000,000,000 doesn't fit into 'int')
		return value/denominator + (value%denominator!=0);
	}
#ifdef RELEASE_MFC42
	template<>
	Yahel::TPosition RoundDivUp(const Yahel::TPosition value,const Yahel::TPosition denominator);
#endif

	template<typename T>
	T RoundUpToMuls(const T value,const T mul){
		return RoundDivUp(value,mul)*mul;
	}

	class CExclusivelyLocked{
		CSyncObject &syncObj;
	public:
		CExclusivelyLocked(CSyncObject &syncObj);
		~CExclusivelyLocked();
	};

	struct TInternetHandle{
		const HINTERNET handle;

		inline TInternetHandle(HINTERNET handle) : handle(handle) {}
		~TInternetHandle();

		inline operator bool() const{ return handle!=nullptr; }
		inline operator HINTERNET() const{ return handle; }
	};

	struct TInternetConnection:public TInternetHandle{
		inline TInternetConnection(HINTERNET handle) : TInternetHandle(handle) {}

		TStdWinError DownloadHttp(LPCTSTR object,LPVOID buffer,DWORD bufferSize,DWORD &outObjectSize) const;
	};

	struct TInternetSession:public TInternetHandle{
		TInternetSession();

		TInternetConnection ConnectTo(LPCTSTR server) const;
		TStdWinError DownloadOneHttp(LPCTSTR server,LPCTSTR object,LPVOID buffer,DWORD bufferSize,DWORD &outObjectSize) const;
		TStdWinError DownloadOneHttp(LPCTSTR url,LPVOID buffer,DWORD bufferSize,DWORD &outObjectSize) const;
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
		inline operator LRESULT() const{ return (LRESULT)m_hObject; }
	};

	class CRideFont sealed:public ::CFont{
		void InitBy(const LOGFONT &lf);
	public:
		static const CRideFont Small, Std, StdDpi, StdBold;
		static const CRideFont Webdings80, Webdings120, Webdings175;
		static const CRideFont Wingdings105;

		int charAvgWidth,charHeight,charDescent;

		CRideFont(LPCTSTR face,int pointHeight,bool bold=false,bool dpiScaled=false,int pointWidth=0);
		CRideFont(HWND hWnd,bool bold=false);
		CRideFont(HFONT hFont);

		inline operator WPARAM() const{ return (WPARAM)m_hObject; }

		BOOL Attach(HFONT hFont);
		SIZE GetTextSize(LPCTSTR text,int textLength) const;
		SIZE GetTextSize(LPCTSTR text) const;
		SIZE GetTextSize(const CString &text) const;
		HFONT CreateRotated(int nDegrees) const;
	};

	class CRideContextMenu sealed:public ::CMenu{
		CMenu parent;
	public:
		static void UpdateUI(CWnd *pUiUpdater,CMenu *pMenu); // pay attention at spelling of "-UI"

		CRideContextMenu();
		CRideContextMenu(UINT idMenuRes,CWnd *pUiUpdater=nullptr);
		CRideContextMenu(HMENU hMenuOwnedByCaller);
		~CRideContextMenu();

		CString GetMenuString(UINT uIDItem,UINT flags) const;
		inline void UpdateUi(CWnd *pUiUpdater){ UpdateUI(pUiUpdater,this); } // pay attention at spelling of "-Ui"
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

	struct TGdiMatrix:XFORM{
		static const XFORM Identity;

		inline TGdiMatrix(const XFORM &m=Identity) : XFORM(m) {}
		TGdiMatrix(HDC dc);
		TGdiMatrix(float dx,float dy);

		TGdiMatrix &Shift(float dx,float dy);
		TGdiMatrix &RotateCv90();
		TGdiMatrix &RotateCcv90();
		TGdiMatrix &Scale(float sx,float sy);
		TGdiMatrix &Combine(const XFORM &next);

		POINTF Transform(float x,float y) const;
		inline POINTF Transform(const POINTF &pt) const{ return Transform( pt.x, pt.y ); }
		POINTF TransformInversely(const POINTF &pt) const;
		POINTF TransformInversely(const POINT &pt) const;
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
		static void SetParentFont(HWND hWnd);
		template<typename T> static T GetWindowUserData(HWND hWnd){ return (T)::GetWindowLongW( hWnd, GWL_USERDATA ); }
		template<typename T> static void SetWindowUserData(HWND hWnd,const T &data){ ::SetWindowLongW( hWnd, GWL_USERDATA, (long)data ); }

		CRideDialog(LPCTSTR lpszTemplateName,const CWnd *pParentWnd=nullptr);
		CRideDialog(UINT nIDTemplate,const CWnd *pParentWnd=nullptr);

		INT_PTR DoModal() override;
		HWND GetDlgItemHwnd(WORD id) const;
		int GetDlgItemTextLength(WORD id) const;
		void SetDlgItemText(WORD id,LPCTSTR text) const;
		inline int GetDlgItemInt(WORD id) const{ return Yahel::Gui::GetDlgItemInt( m_hWnd, id ); }
		void SetDlgItemInt(WORD id,Yahel::TPosition value) const{ Yahel::Gui::SetDlgItemInt( m_hWnd, id, value ); }
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
		inline void SetDlgItemCompactPath(WORD id,LPCTSTR fullpath) const{ ::PathSetDlgItemPath( m_hWnd, id, fullpath ); }
		void SetDlgItemFormattedText(WORD id,LPCTSTR format,...) const;
		void SetDlgItemSingleCharUsingFont(WORD id,WCHAR singleChar,HFONT hFont) const;
		void SetDlgItemSingleCharUsingFont(WORD id,WCHAR singleChar,LPCTSTR fontFace,int fontPointSize) const;
		void PopulateDlgComboBoxWithSequenceOfNumbers(WORD cbId,BYTE iStartValue,LPCTSTR strStartValueDesc,BYTE iEndValue,LPCTSTR strEndValueDesc) const;
		void ConvertDlgButtonToSplitButtonEx(WORD id,PCSplitButtonAction pAction,BYTE nActions,LPACCEL *ppOutAccels=nullptr,CWnd *pUiUpdater=nullptr) const;
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
		void ConvertDlgButtonToSplitButton(WORD id,const TSplitButtonAction (&actions)[N],LPACCEL *ppOutAccels=nullptr) const{
			ConvertDlgButtonToSplitButtonEx( id, actions, N, ppOutAccels );
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

		inline operator PCBYTE() const{ return values; }
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
		inline bool operator==(const FILETIME &t2) const{ return operator-(t2)==0; }
		inline bool operator!=(const FILETIME &t2) const{ return operator-(t2)!=0; }
		CRideTime operator-(const FILETIME &t2) const;
		inline CRideTime operator-(const CRideTime &t2) const{ return operator-( (FILETIME)t2 ); }

		int ToMilliseconds() const;
		WORD GetDosDate() const;
		DWORD GetDosDateTime() const;
		CString DateToStdString() const;
		CString TimeToStdString() const;
		CRideTime ToTzSpecificLocalTime() const;
		bool Edit(bool dateEditingEnabled,bool timeEditingEnabled,const SYSTEMTIME *epoch);
	};

	struct TViewportOrg:public CPoint{
		inline TViewportOrg(HDC dc){ ::GetViewportOrgEx(dc,this); }
	};

	class CViewportOrgBackup:public TViewportOrg{
		const HDC dc;
	public:
		CViewportOrgBackup(HDC dc);
		~CViewportOrgBackup();
	};

	struct TClientRect:CRect{
		inline TClientRect(HWND hWnd){ ::GetClientRect(hWnd,this); }
	};

	class CAxis{
	protected:
		// any value that is 'long' is in device pixels (incl. 'POINT' and 'RECT' structs!)
		// any value that is 'int' is in device units (e.g. for drawing)
		// any value that is 'TLogValue' is in Axis units
		TLogValue logLength;
		BYTE zoomFactor;
		BYTE scrollFactor;
		struct TDcState sealed{
			int nUnitsAtOrigin;
			int graphicsMode;
			TViewportOrg ptViewportOrg; // [pixels] GM_COMPATIBLE
			TGdiMatrix mAdvanced; // [units] GM_ADVANCED

			TDcState(HDC dc,int nVisibleUnitsA,int nDrawnUnitsA);

			int ApplyTo(HDC dc) const;
			void RevertFrom(HDC dc,int iSavedDc) const;
		} dcLastDrawing; // for any subsequent drawing, e.g. cursor indication
		TLogValue logCursorPos;

		TLogValue GetValue(int nUnits,BYTE factor) const;
	public:
		typedef int TScrollPos;

		class CGraphics{
			const CAxis &axis;
			const HDC dc;
			int iSavedDc;
			CDC dcMem;

			int vPerpLineAndText(TLogValue v,int nUnitsFrom,int nUnitsTo,const SIZE &szUnitsLabelOffset,LPCTSTR format,va_list args) const; // perpendicular line with text description
		public:
			CGraphics(HDC dc,const CAxis &axis);
			CGraphics(CGraphics &&r);
			~CGraphics();

			int PerpLine(TLogValue v,int nUnitsFrom,int nUnitsTo) const; // perpendicular line
			int PerpLine(TLogValue v,int nUnitsLength) const; // perpendicular line
			int __cdecl PerpLineAndText(TLogValue v,int nUnitsFrom,int nUnitsTo,const SIZE &szUnitsLabelOffset,LPCTSTR format,...) const; // perpendicular line with text description
			int __cdecl PerpLineAndText(TLogValue v,int nUnitsFrom,int nUnitsTo,LPCTSTR format,...) const; // perpendicular line with text description
			int TextIndirect(int nUnitsX,int nUnitsY,const CRideFont &font,const CString &text,int rop=SRCINVERT) const;
			int PerpLineAndTextIndirect(TLogValue v,int nUnitsFrom,int nUnitsTo,int nUnitsLabel,const CRideFont &font,const CString &text,int rop=SRCINVERT) const; // perpendicular line with text description
			void DimensioningIndirect(TLogValue vStart,TLogValue vEnd,int nUnitsFrom,int nUnitsTo,const CString &text,int nUnitsExtra=5,int rop=SRCINVERT) const;
			void Rect(TLogValue vStart,TLogValue vEnd,int nUnitsTop,int nUnitsBottom,HBRUSH brush) const;
		};

		const enum TVerticalAlign{
			NONE,
			TOP,
			BOTTOM
		} ticksAndLabelsAlign;
		const TLogValue logValuePerUnit;
		const TCHAR unit;
		const LPCTSTR unitPrefixes;
		const CRideFont &font;

		static const TCHAR NoPrefixes[];
		static const TCHAR CountPrefixes[];
		static const CRideFont FontWingdings;

		static long GetPixelDistance(int nUnitsA,int nUnitsZ);

		CAxis(TLogValue logLength,TLogTime logValuePerUnit,TCHAR unit,LPCTSTR unitPrefixes,BYTE initZoomFactor,TVerticalAlign ticksAndLabelsAlign=TVerticalAlign::TOP,const CRideFont &font=CRideFont::Std);

		// any value that is 'long' is in device pixels (incl. 'POINT' and 'RECT' structs!)
		// any value that is 'int' is in device units (e.g. for drawing)
		// any value that is 'TLogValue' is in Axis units
		inline CGraphics CreateGraphics(HDC dc) const{ return CGraphics(dc,*this); }
		TLogInterval Draw(HDC dc,TLogInterval visible,int primaryGridLength=0,HPEN hPrimaryGridPen=nullptr);
		TLogInterval Draw(HDC dc,TLogValue from,long nVisiblePixels,int primaryGridLength=0,HPEN hPrimaryGridPen=nullptr);
		TLogInterval DrawWhole(HDC dc,int primaryGridLength=0,HPEN hPrimaryGridPen=nullptr);
		TLogInterval DrawScrolled(HDC dc,long scrollPos,long nVisiblePixels,int primaryGridLength=0,HPEN hPrimaryGridPen=nullptr);
		TLogInterval DrawScrolled(HDC dc,int primaryGridLength=0,HPEN hPrimaryGridPen=nullptr);
		inline TLogValue GetCursorPos() const{ return logCursorPos; }
		void DrawCursorPos(HDC dc,TLogValue newLogPos);
		void DrawCursorPos(HDC dc,const POINT &ptClient);
		int GetUnitCount(TLogValue logValue,BYTE zoomFactor) const;
		int GetUnitCount(TLogValue logValue) const;
		int GetUnitCount() const;
		TLogValue GetValue(int nUnits) const;
		TLogValue GetValue(const POINT &ptClient) const;
		TLogValue GetValueFromScroll(TScrollPos pos) const;
		TLogValue GetValueFromPixel(long nPixels) const;
		long GetPixelCount(TLogValue v,BYTE zoomFactor) const;
		long GetPixelCount(TLogValue v) const;
		int GetClientUnits(TLogValue logValue) const; // for drawing in client area
		inline TLogValue GetLength() const{ return logLength; }
		void SetLength(TLogValue newLogLength);
		TScrollPos GetScrollMax();
		TScrollPos GetScrollPos(TLogValue v) const;
		inline BYTE GetZoomFactor() const{ return zoomFactor; }
		BYTE GetZoomFactorToFitWidth(long width,BYTE zoomFactorMax) const;
		BYTE GetZoomFactorToFitWidth(TLogValue logValue,long width,BYTE zoomFactorMax) const;
		void SetZoomFactor(BYTE newZoomFactor);
		int ValueToReadableString(TLogValue logValue,PTCHAR buffer) const;
		CString ValueToReadableString(TLogValue logValue) const;
		inline CString CursorValueToReadableString() const{ return ValueToReadableString(logCursorPos); }
	};

	class CTimeline:public CAxis{
	public:
		static const TCHAR TimePrefixes[];

		CTimeline(TLogTime logTimeLength,TLogTime logTimePerUnit,BYTE initZoomFactor);

		inline TLogTime GetTime(int nUnits) const{ return GetValue(nUnits); }
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
		inline operator WORD() const{ return MAKEWORD(lowByte,highByte); }
		inline WORD GetBigEndian() const{ return *(PCWORD)this; }
	};

	class CBigEndianDWord sealed{
		CBigEndianWord highWord,lowWord;
	public:
		DWORD operator=(DWORD newValue);
		inline operator DWORD() const{ return MAKELONG(lowWord,highWord); }
	};

	template <typename T>
	bool QuerySingleInt(LPCTSTR caption,LPCTSTR label,const Yahel::TPosInterval &range,T &inOutValue,bool hexa){
		return Yahel::Gui::QuerySingleInt<T>( caption, label, range, inOutValue, (Yahel::Gui::TNotation)hexa, app.GetEnabledActiveWindow() );
	}

	bool IsVistaOrNewer();
	TStdWinError ErrorByOs(TStdWinError vistaOrNewer,TStdWinError xpOrOlder);
#ifdef UNICODE
	static_assert( false, "Unicode support not implemented" );
#else
	inline LPCSTR ToStringA(LPCTSTR s){ return s; }
	inline LPCTSTR ToStringT(LPCSTR s){ return s; }
	CString ToStringT(LPCWSTR lpsz); // converts to UTF-8
#endif
	LPCTSTR BoolToYesNo(bool value);
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
	bool QuerySinglePercent(LPCTSTR caption,LPCTSTR label,BYTE &inOutValue,const Yahel::TPosInterval &range=Yahel::Percent);
	CString BytesToHigherUnits(DWORD bytes);
	CString BytesToHexaText(PCBYTE bytes,BYTE nBytes,bool lastDelimitedWithAnd);
	CString BytesToHexaText(const char *chars,BYTE nChars,bool lastDelimitedWithAnd);
	void NavigateToUrlInDefaultBrowser(LPCTSTR url);
	void NavigateToFaqInDefaultBrowser(LPCTSTR faqPageId);
	void ScaleLogicalUnit(HDC dc);
	void ScaleLogicalUnit(PINT values,BYTE nValues);
	void UnscaleLogicalUnit(PINT values,BYTE nValues);
	POINT LPtoDP(const POINT &pt);
	SIZE LPtoDP(const SIZE &sz);
	POINT DPtoLP(const POINT &pt);
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
	void SetClipboardString(LPCTSTR str);
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
