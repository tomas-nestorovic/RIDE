#ifndef DIFF_H
#define DIFF_H

	class CDiffBase abstract{
	public:
		typedef const struct TScriptItem sealed{
			enum:char{
				INSERTION	='+',
				DELETION	='-'
			} operation;
			int iPosA;
			union{
				struct{
					int iPosB;
					int nItemsB;
				} op;
				struct{
					int iPosB;
					int nItemsB;
				} ins;
				struct{
					int iPosB;
					int nItemsA;
				} del;
			};

			void ConvertToDual();
		} *PCScriptItem;
	protected:
		mutable TScriptItem *pEmptyScriptItem;
		mutable DWORD nEmptyScriptItems;

		int MergeScriptItems(TScriptItem *buffer) const;
	};





	template<typename T>
	class CDiff:public CDiffBase{
		// implementation of diff algorithm from
		// Myers, E., "An O(ND) Difference Algorithm and its Variations," Algorithmica 1(2), 1986, p.251.
		// Adopted from
		// https://github.com/psionic12/Myers-Diff-in-c-
		// and
		// http://www.mathertel.de/Diff

		const T *const A;
		const int N;
		const T *B;
		int max;
		PINT fv,rv; // pointers to [0], allowing for negative indices!
		PActionProgress pap;

		struct TMidSnake sealed{
			int x,y,u,v,D;
		} GetShortestMiddleSnake(const T *a,int n,const T *b,int m) const{
			// find and return the Shortest Middle Snake (SMS)
			// - initialization
			const int delta=n-m;
			fv[1]=0;
			rv[delta+1]=n+1;
			// - SMS
			const bool oddDelta=(delta&1)!=0;
			for( int D=0,DEnd=(m+n+1)/2; D<=DEnd; D++ ){
				for( int k=-D; k<=D; k+=2 ){ // extend the forward path
					// . find the only or better starting point
					int x =	k==-D  ||  k!=D && fv[k-1]<fv[k+1]
							? fv[k+1]
							: fv[k-1]+1;
					int y=x-k;
					// . find the furthest reaching forward D-path ending in diagonal K
					while (x<n && y<m && a[x]==b[y])
						x++, y++; // diagonal
					fv[k]=x;
					// . overlap ?
					if (oddDelta && delta-(D-1)<=k && k<=delta+D-1)
						if (fv[k]>=rv[k]){
							const TMidSnake result={ rv[k], rv[k]-k, x, y, 2*D-1 };
							return result;
						}
				}
				for( int k=-D+delta; k<=D+delta; k+=2 ){ // extend the reverse path
					// . find the only or better starting point
					int x =	k==-D+delta  ||  k!=D+delta && rv[k-1]>=rv[k+1]
							? rv[k+1]-1
							: rv[k-1];
					int y=x-k;
					// . find the furthest reaching reverse D-path ending in diagonal K
					while (x>0 && y>0 && a[x-1]==b[y-1])
						x--, y--; // diagonal
					rv[k]=x;
					// . overlap ?
					if (!oddDelta && -D<=k && k<=D)
						if (fv[k]>=rv[k]){
							const TMidSnake result={ x, y, fv[k], fv[k]-k, 2*D };
							return result;
						}
				}
			}
			// - we shouldn't end up here
			ASSERT(FALSE);
			static constexpr TMidSnake TooManyDiffs={ 0, 0, 0, 0, -1 };
			return TooManyDiffs;
		}

		bool GetShortestEditScript(const T *a,int n,const T *b,int m) const{
			// the divide-and-conquer implementation of the longes common-subsequence
			// - skip equal starts
			while (n>0 && m>0 && *a==*b)
				a++, n--,  b++, m--;
			// - skip equal ends
			while (n>0 && m>0 && a[n-1]==b[m-1])
				n--, m--;
			// - detect complete equality
			if ((m|n)==0)
				return true;
			// - detect Insertion ("theirs" contains some extra bits that "this" misses; must "insert" into "this")
			if (n==0)
				if (nEmptyScriptItems>0){
					TScriptItem &r=*pEmptyScriptItem++;
						r.operation=TScriptItem::INSERTION;
						r.iPosA=a-A;
						r.ins.iPosB=b-B;
						r.ins.nItemsB=m;
					nEmptyScriptItems--;
					return true;
				}else
					return false; // insufficient buffer
			// - detect Deletion ("theirs" misses some bits that "this" contains; must "delete" from "this")
			if (m==0)
				if (nEmptyScriptItems>0){
					TScriptItem &r=*pEmptyScriptItem++;
						r.operation=TScriptItem::DELETION;
						r.iPosA=a-A;
						r.del.iPosB=b-B;
						r.del.nItemsA=n;
					nEmptyScriptItems--;
					return true;
				}else
					return false; // insufficient buffer
			// - find the shortest middle snake (SMS) and length of optimal path for A and B
			pap->UpdateProgress( a-A, TBPFLAG::TBPF_NORMAL );
			const TMidSnake sms=GetShortestMiddleSnake( a, n, b, m );
			// - the path is from StartX to (x,y) and (x,y) to EndX
			return	sms.D>=0
					&&
					GetShortestEditScript( a, sms.x, b, sms.y )
					&&
					GetShortestEditScript( a+sms.u, n-sms.u, b+sms.v, m-sms.v );
		}
	public:
		CDiff(const T *A,int N)
			// ctor
			: A(A) , N(N) {
		}

		int GetShortestEditScript(const T *B,int M,TScriptItem *pOutScriptItemBuffer,DWORD nScriptItemsBufferCapacity,CActionProgress &rap){
			// composes the shortest edit Script and returns the number of its Items (or -1 if Script couldn't have been composed, e.g. insufficient output Buffer)
			this->B=B;
			this->max=N+M+1;
			pEmptyScriptItem=pOutScriptItemBuffer;
			nEmptyScriptItems=nScriptItemsBufferCapacity;
			const auto fv=Utils::MakeCallocPtr<int>(2*max+2), rv=Utils::MakeCallocPtr<int>(2*max+2);
			this->fv=fv+max, this->rv=rv+max;
			( pap=&rap )->SetProgressTarget(N);
			return	GetShortestEditScript( A, N, B, M ) // Script composed?
					? MergeScriptItems(pOutScriptItemBuffer)
					: -1;
		}
	};

#endif // DIFF_H
