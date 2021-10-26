#include "stdafx.h"

	void CDiffBase::TScriptItem::ConvertToDual(){
		// transforms in-place the Item to its dual counterpart
		std::swap( iPosA, op.iPosB );
		switch (operation){
			case INSERTION:
				operation=DELETION;
				break;
			default:
				ASSERT(FALSE);
				//fallthrough
			case DELETION:
				operation=INSERTION;
				break;
		}
	}




	int CDiffBase::MergeScriptItems(TScriptItem *buffer) const{
		// merges script Operations which the descendant has split into multiple consecutive ScriptItems; returns the number of merged ScriptItems
		if (buffer==pEmptyScriptItem)
			return 0;
		TScriptItem *psiLast=buffer;
		for( PCScriptItem psiNext=buffer+1; psiNext<pEmptyScriptItem; psiNext++ ){
			if (psiNext->operation==psiLast->operation)
				switch (psiNext->operation){
					case TScriptItem::INSERTION:
						if (psiNext->iPosA==psiLast->iPosA && psiNext->ins.iPosB==psiLast->ins.iPosB+psiLast->ins.nItemsB){
							psiLast->ins.nItemsB+=psiNext->ins.nItemsB; // merge
							continue;
						}else
							break;
					default:
						ASSERT(FALSE);
						//fallthrough
					case TScriptItem::DELETION:
						if (psiNext->iPosA==psiLast->iPosA+psiLast->del.nItemsA){
							psiLast->del.nItemsA+=psiNext->del.nItemsA; // merge
							continue;
						}else
							break;
				}
			*++psiLast=*psiNext; // can't be merged
		}
		return psiLast+1-buffer;
	}
