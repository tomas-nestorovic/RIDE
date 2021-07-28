#include "stdafx.h"

	int CDiffBase::MergeScriptItems(TScriptItem *buffer) const{
		// merges script Operations which the descendant has split into multiple consecutive ScriptItems; returns the number of merged ScriptItems
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
					case TScriptItem::DELETION:
						if (psiNext->iPosA==psiLast->iPosA+psiLast->del.nItemsA){
							psiLast->del.nItemsA+=psiNext->del.nItemsA; // merge
							continue;
						}else
							break;
				}
			*++psiLast=*psiNext; // can't be merged
		}
		return psiLast-buffer;
	}
