#pragma once

namespace Memory
{
	template<typename T>
	class CSharedPodPtr:public CString{ // 'std::shared_ptr'-like pointer to Plain Old Data
	public:
		inline CSharedPodPtr(TCHAR initByte,int initDataLength=sizeof(T))
			: CString( initByte, (initDataLength+sizeof(TCHAR)-1)/sizeof(TCHAR) ) {
		}
		CSharedPodPtr(const T &copyInitData,int initDataLength=sizeof(T)){
			::memcpy(
				GetBufferSetLength( (initDataLength+sizeof(TCHAR)-1)/sizeof(TCHAR) ),
				&copyInitData, initDataLength
			);
		}

		inline T &operator*() const{ return *(T *)operator LPCTSTR(); }
		inline T *operator->() const{ return (T *)operator LPCTSTR(); }
		inline bool operator==(const T *other) const{ return operator->()==other; }
	};

	static_assert( sizeof(CSharedPodPtr<BYTE>)==sizeof(CString), "can't easily retype these two, e.g. '(CSharedPodPtr)myAfxStr'" );




	template<typename T,typename TIndex=int>
	class CSharedPodArray:public CSharedPodPtr<T>{ // 'std::shared_ptr'-like pointer to array of Plain Old Data
	public:
		typedef TIndex N;

		inline static const CSharedPodArray &GetEmpty(){
			return static_cast<const CSharedPodArray &>( // this is utmost nasty but efficient, provided 'TIndex' of 'Empty' covers all possible ranges
				static_cast<const CString &>(Memory::UniversalEmptySharedPodArray)
			);
		}

		TIndex length;

		CSharedPodArray(TIndex length=0,TCHAR initByte=0)
			: CSharedPodPtr( initByte, sizeof(T)*length )
			, length(length) {
			static_assert( std::is_integral<TIndex>().value, "'TIndex' must be integral" );
		}
		CSharedPodArray(TIndex length,const T *pCopyInitData)
			: CSharedPodPtr( *pCopyInitData, sizeof(T)*length )
			, length(length) {
			static_assert( std::is_integral<TIndex>().value, "'TIndex' must be integral" );
		}

		inline operator bool() const{ return length>0; }
		inline operator T *() const{ return (T *)operator LPCTSTR(); }
		inline operator LPCVOID() const{ return operator LPCTSTR(); }
		inline T *operator+(TIndex i) const{ return begin()+i; }
		inline T &operator[](TIndex i) const{ return begin()[i]; }

		inline void reset(){ Empty(), length=0; }
		inline const T &Last() const{ ASSERT(length>0); return operator[](length-1); }

		T *Realloc(TIndex newLength){
			if (newLength){
				const CSharedPodArray tmp(newLength);
				::memcpy( tmp.begin(), begin(), sizeof(T)*std::min(length,newLength) );
				return (*this=tmp);
			}else{ // the special case for which the above would fail
				reset();
				return nullptr;
			}
		}

		TStdWinError Read(LPCTSTR filename){
			if (!filename || !*filename) // an empty string may succeed as filename on Win10!
				return ERROR_FILE_NOT_FOUND;
			CFileException e;
			CFile f;
			if (!f.Open( filename, CFile::modeRead|CFile::shareDenyWrite|CFile::typeBinary, &e ))
				return e.m_cause;
			N nItems=f.GetLength()/sizeof(T);
			const auto fLength=nItems*sizeof(T);
			if (f.Read( Realloc(nItems), fLength )!=fLength)
				return ::GetLastError();
			return ERROR_SUCCESS;
		}

		template<typename V,class Predicate>
		T *LowerBound(const V &v,Predicate p) const{
			return std::lower_bound( begin(), end(), v, p );
		}

		// 'for each' support
		inline T *begin() const{ return operator T *(); }
		inline T *end() const{ return begin()+length; }
	};

	typedef CSharedPodArray<BYTE> CSharedBytes;

	extern const CSharedPodArray<SYSTEMTIME,ULONGLONG> UniversalEmptySharedPodArray;




	// a workaround to template argument deduction on pre-2017 compilers
	template<typename T,typename TIndex>
	inline static CSharedPodArray<T,typename std::tr1::decay<TIndex>::type> MakeSharedPodArray(TIndex length){
		return CSharedPodArray<T,typename std::tr1::decay<TIndex>::type>( length );
	}
	template<typename T,typename TIndex>
	inline static CSharedPodArray<T,typename std::tr1::decay<TIndex>::type> MakeSharedPodArray(TIndex length,int initByte){
		return CSharedPodArray<T,typename std::tr1::decay<TIndex>::type>( length, initByte );
	}
	template<typename T,typename TIndex>
	inline static CSharedPodArray<T,typename std::tr1::decay<TIndex>::type> MakeSharedPodArray(TIndex length,const T *pCopyInitData){
		return CSharedPodArray<T,typename std::tr1::decay<TIndex>::type>( length, pCopyInitData );
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
	class CPodList:protected CStringList{ // list of Plain Old Data structures (of variable lengths but the same prefix <T>)
		// don't make CStringList 'public' - client mustn't call 'Find' (the contents are PODs instead of null-terminated strings!)
		// lots of casts to invoke 'CStringList' methods that return references to (instead of copies of) 'CString'
	public:
		inline CPodList(){}
		CPodList(const CPodList &r){
			// shallow-copy ctor (to avoid "Error C2248: 'CObject::CObject' : cannot access private member declared in class 'CObject'")
			__super::AddTail(
				&const_cast<CStringList &>( static_cast<const CStringList &>(r) )
			);
		}

		inline operator bool() const{ return GetCount()>0; }
		inline void RemoveAll(){ __super::RemoveAll(); }
		inline POSITION GetHeadPosition() const{ return __super::GetHeadPosition(); }
		inline POSITION GetTailPosition() const{ return __super::GetTailPosition(); }
		//inline ... Find(LPCTSTR) const; // commented out as the contents are PODs instead of null-terminated strings!

		POSITION AddHead(const T &element,int elementSize=sizeof(T)){
			return __super::AddHead( static_cast<const CString &>(CSharedPodPtr<T>(element,elementSize)) );
		}
		POSITION AddTail(const T &element,int elementSize=sizeof(T)){
			return __super::AddTail( static_cast<const CString &>(CSharedPodPtr<T>(element,elementSize)) );
		}
		T &GetNext(POSITION &rPosition) const{
			return *static_cast<CSharedPodPtr<T> &>(
				const_cast<CStringList *>( static_cast<const CStringList *>(this) )->GetNext(rPosition)
			);
		}
		T &GetPrev(POSITION &rPosition) const{
			return *static_cast<CSharedPodPtr<T> &>(
				const_cast<CStringList *>( static_cast<const CStringList *>(this) )->GetPrev(rPosition)
			);
		}
		T &GetHead() const{
			return *static_cast<CSharedPodPtr<T> &>(
				const_cast<CStringList *>( static_cast<const CStringList *>(this) )->GetHead()
			);
		}
		T &GetTail() const{
			return *static_cast<CSharedPodPtr<T> &>(
				const_cast<CStringList *>( static_cast<const CStringList *>(this) )->GetTail()
			);
		}
		const CSharedPodPtr<T> &GetPtrAt(POSITION position) const{
			return static_cast<const CSharedPodPtr<T> &>(
				const_cast<CStringList *>( static_cast<const CStringList *>(this) )->GetAt(position)
			);
		}
		T &GetAt(POSITION position) const{
			return *GetPtrAt(position);
		}
	};

}
