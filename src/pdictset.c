/* dictset.c - index
*
* Copyright(C) 2019 - 2020, sun shuo <sun.shuo@surparallel.org>
* All rights reserved.
*
* This program is free software : you can redistribute it and / or modify
* it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU Affero General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with this program.If not, see < https://www.gnu.org/licenses/>.
*/

#include "plateform.h"
#include "pdict.h"
#include "pdictset.h"

void* plg_DictSetCreate(void *vtype, short pool, void *vsType, short sPool) {
	dictType *type = vtype;
	dictType *sType = vsType;
	PDictSet pDictSet = malloc(sizeof(DictSet));
	pDictSet->dictSet = plg_dictCreate(type, NULL, pool);
	pDictSet->sType = sType;
	pDictSet->sPool = sPool;
	pDictSet->size = 0;
	return pDictSet;
}

void plg_DictSetAdd(PDictSet pDictSet, void* key, void* value) {
	dictEntry * entry = plg_dictFind(pDictSet->dictSet, key);
	dict* table;
	if (entry == 0) {
		table = plg_dictCreate(pDictSet->sType, NULL, pDictSet->sPool);
		plg_dictAdd(pDictSet->dictSet, key, table);
	} else {
		table = dictGetVal(entry);
	}

	dictEntry * enrtyTable = plg_dictFind(table, value);
	if (enrtyTable == 0) {
		plg_dictAdd(table, value, NULL);
		pDictSet->size += 1;
	}
}

void plg_DictSetDelValue(PDictSet pDictSet, void* key, void* value) {
	
	dictEntry * entry = plg_dictFind(pDictSet->dictSet, key);
	dict* table;
	if (entry == 0) {
		return;
	}
	
	table = dictGetVal(entry);
	dictEntry * enrtyTable = plg_dictFind(table, value);
	if (enrtyTable == 0) {
		return;
	}
	plg_dictDelete(table, dictGetKey(enrtyTable));

	if (dictSize(table) == 0) {
		plg_dictDelete(pDictSet->dictSet, dictGetKey(entry));
		plg_dictRelease(table);
		pDictSet->size -= 1;
	}
}

void plg_DictSetDel(PDictSet pDictSet, void* key) {

	dictEntry * entry = plg_dictFind(pDictSet->dictSet, key);
	dict* table;
	if (entry == 0) {
		return;
	}

	table = dictGetVal(entry);
	plg_dictDelete(pDictSet->dictSet, dictGetKey(entry));
	pDictSet->size -= dictSize(table);
	plg_dictRelease(table);
}

unsigned char plg_DictSetIn(PDictSet pDictSet, void* key, void* value) {
	dictEntry * entry = plg_dictFind(pDictSet->dictSet, key);
	dict* table;
	if (entry != 0) {
		table = dictGetVal(entry);
		dictEntry * enrtyTable = plg_dictFind(table, value);
		if (enrtyTable != 0) {
			return 1;
		}
	}
	return 0;
}

void* plg_DictSetValue(PDictSet pDictSet, void* key) {
	dictEntry * entry = plg_dictFind(pDictSet->dictSet, key);
	if (entry != 0) {
		return dictGetVal(entry);
	} else {
		return 0;
	}
}

void plg_DictSetDestroy(PDictSet pDictSet) {
	dictIterator* dictIter = plg_dictGetSafeIterator(pDictSet->dictSet);
	dictEntry* dictNode;
	while ((dictNode = plg_dictNext(dictIter)) != NULL) {
		dict* table = dictGetVal(dictNode);
		plg_dictRelease(table);
	}
	plg_dictReleaseIterator(dictIter);
	plg_dictRelease(pDictSet->dictSet);
	free(pDictSet);
}

void plg_DictSetEmpty(PDictSet pDictSet) {
	dictIterator* dictIter = plg_dictGetSafeIterator(pDictSet->dictSet);
	dictEntry* dictNode;
	while ((dictNode = plg_dictNext(dictIter)) != NULL) {
		dict* table = dictGetVal(dictNode);
		plg_dictEmpty(table, NULL);
	}
	plg_dictReleaseIterator(dictIter);
	plg_dictEmpty(pDictSet->dictSet, NULL);
}


void plg_DictSetDup(PDictSet pDictSetDest, PDictSet pDictSetSrc) {

	dictIterator* dictIter = plg_dictGetSafeIterator(pDictSetSrc->dictSet);
	dictEntry* dictNode;
	while ((dictNode = plg_dictNext(dictIter)) != 0) {

		dict* value = dictGetVal(dictNode);
		dictIterator* valueIter = plg_dictGetSafeIterator(value);
		dictEntry* valueNode;
		while ((valueNode = plg_dictNext(valueIter)) != NULL) {
			plg_DictSetAdd(pDictSetDest, dictGetKey(dictNode), dictGetVal(valueNode));
		}
		plg_dictReleaseIterator(valueIter);
	}
	plg_dictReleaseIterator(dictIter);
	pDictSetDest->size = pDictSetSrc->size;
}

unsigned int plg_DictSetSize(PDictSet pDictSet) {
	return pDictSet->size;
}

void* plg_DictSetDict(PDictSet pDictSet) {
	return pDictSet->dictSet;
}