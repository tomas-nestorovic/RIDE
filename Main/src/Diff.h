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
		// https://github.com/google/diff-match-patch
		// and structurally inspired by
		// https://github.com/psionic12/Myers-Diff-in-c-
		// and
		// http://www.mathertel.de/Diff

		const T *const A;
		const int N;
		const T *B;
		int max;
		PINT fv,rv; // pointers set to [0], allowing for negative indices!
		PActionProgress pap;

		POINT GetBisectSplit(const T *a,int n,const T *b,int m) const{
			// find and return the point at which the A and B input strings can be split for optimal recurrent processing
			// - initialization
			const int DMax=(m+n+1)/2;
			for( int k=-DMax; k<=DMax; k++ )
				fv[k] = rv[k] = -1;
			fv[1] = rv[1] = 0;
			const int delta=n-m;
			// - SMS
			static constexpr POINT Cancelled;
			const bool oddDelta=(delta&1)!=0; // if the total number of characters is odd, then the front path will collide with the reverse path
			int fkStart=0, fkEnd=0, rkStart=0, rkEnd=0; // offsets for start and end of k loop; prevent mapping of space beyond the grid
			for( int D=0; D<DMax; D++ ){
				// . extend the Front snake one step ahead
				for( int k=-D+fkStart; k<=D-fkEnd; k+=2 ){
					if (pap->Cancelled)
						return Cancelled;
					// : find the only or better starting point
					int x =	k==-D  ||  k!=D && fv[k-1]<fv[k+1]
							? fv[k+1]
							: fv[k-1]+1;
					int y=x-k;
					// : find the furthest reaching Front D-path ending in diagonal K
					while (x<n && y<m && a[x]==b[y])
						x++, y++; // diagonal
					fv[k]=x;
					// : evaluating where we ended up
					if (x>n) // ran off the right of the graph?
						fkEnd+=2;
					else if (y>m) // ran off the bottom of the graph?
						fkStart+=2;
					else if (oddDelta){
						const int rk=delta-k;
						if (-DMax<=rk && rk<DMax && rv[rk]!=-1){
							// mirror rx onto top-left coordinate system
							const int rx=n-rv[rk];
							if (x>=rx){ // overlap of the Front and Back snakes?
								const POINT result={ x, y };
								return result;
							}
						}
					}
				}
				// . extend the Back snake one step ahead
				for( int k=-D+rkStart; k<=D-rkEnd; k+=2 ){
					if (pap->Cancelled)
						return Cancelled;
					// : find the only or better starting point
					int x =	k==-D  ||  k!=D && rv[k-1]<rv[k+1]
							? rv[k+1]
							: rv[k-1]+1;
					int y=x-k;
					// : find the furthest reaching reverse D-path ending in diagonal K
					while (x<n && y<m && a[n-x-1]==b[m-y-1])
						x++, y++; // diagonal
					rv[k]=x;
					// : evaluating where we ended up
					if (x>n) // ran off the left of the graph?
						rkEnd+=2;
					else if (y>m) // ran off the top of the graph?
						rkStart+=2;
					else if (!oddDelta){
						const int fk=delta-k;
						if (-DMax<=fk && fk<DMax && fv[fk]!=-1){
							// mirror fx onto top-left coordinate system
							const POINT result={ fv[fk], fv[fk]-fk };
							if (result.x>=n-x) // overlap of the Front and Back snakes?
								return result;
						}
					}
				}
			}
			// - no commonality at all
			static constexpr POINT NoCommonality={ -1, 0 };
			return NoCommonality;
		}

		bool GetShortestEditScript(const T *a,int n,const T *b,int m) const{
			// the divide-and-conquer implementation of the longest common-subsequence
			// - skip equal starts
			while (n>0 && m>0 && *a==*b)
				a++, n--,  b++, m--;
			// - skip equal ends
			while (n>0 && m>0 && a[n-1]==b[m-1])
				n--, m--;
			// - detect complete equality
			if ((m|n)<=0)
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
			pap->UpdateProgress( a-A );
			const POINT &&split=GetBisectSplit( a, n, b, m );
			if (pap->Cancelled)
				return false;
			if (split.x<0) // no commonality at all?
				return	GetShortestEditScript( a,n, b,0 ) // delete A
						&&
						GetShortestEditScript( a,0, b,m ); // insert B
			ASSERT( split.y>=0 );
			// - the path is from StartX to (x,y) and (x,y) to EndX
			return	GetShortestEditScript( a, split.x, b, split.y )
					&&
					GetShortestEditScript( a+split.x, n-split.x, b+split.y, m-split.y );
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
