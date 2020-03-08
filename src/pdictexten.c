/* dictExten.c - Return the data set composed of the dictExtens 
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
*
* 从互斥区复制出来，使用json效率较低。相对来说还是使用内存分配效率较高。
* 所以要额外使用dictExten将结果复制出互斥去然后再转为json。在效率和易用性上找到平衡。
* 而设置就没有那么多麻烦了，即使连续设置可能效率较低。
*
* 因为类型的不统一,C语言需要一个可以覆盖输入和输出的dict类型来与数据进行数据交换.
*/
#include <stdlib.h>
#include <string.h>
#include "plateform.h"
#include "padlist.h"
#include "pdict.h"
#include "pdictexten.h"
#include "pquicksort.h"

typedef struct _DictExten
{
	dict* dictExten;
	list* pList;
}*PDictExten, DictExten;

typedef struct DictExtenHead
{
	char type;
	unsigned int keyLen;
	unsigned int valueLen;
	char key[];
}*PDictExtenHead, DictExtenHead;

static unsigned long long HashCallback(const void *key) {
	PDictExtenHead pDictExtenHead = (PDictExtenHead)key;
	return plg_dictGenHashFunction(pDictExtenHead->key, pDictExtenHead->keyLen);
}

static int CompareCallback(void *privdata, const void *key1, const void *key2) {
	DICT_NOTUSED(privdata);
	PDictExtenHead pDictExtenHead1 = (PDictExtenHead)key1;
	PDictExtenHead pDictExtenHead2 = (PDictExtenHead)key2;

	if (pDictExtenHead1->keyLen != pDictExtenHead2->keyLen) {
		return 0;
	}
	return memcmp(pDictExtenHead1->key, pDictExtenHead2->key, pDictExtenHead1->keyLen) == 0;
}

static void FreeCallback(void* privdata, void *val) {

	PDictExten pDictExten = privdata;
	listNode* node = (listNode*)val;
	PDictExtenHead pDictExtenHead = (PDictExtenHead)listNodeValue(node);
	plg_listDelNode(pDictExten->pList, node);
	if (pDictExtenHead->type == 2) {
		void* pDictExten = 0;
		memcpy(&pDictExten, (pDictExtenHead->key + pDictExtenHead->keyLen), sizeof(void*));
		plg_DictExtenDestroy(pDictExten);
	}
	free(pDictExtenHead);
}

static dictType type = {
	HashCallback,
	NULL,
	NULL,
	CompareCallback,
	NULL,
	FreeCallback
};

static int KeyCmpFun(void* v1, void* v2) {

	listNode* node1 = (listNode*)v1;
	listNode* node2 = (listNode*)v2;
	PDictExtenHead pDictExtenHead1 = (PDictExtenHead)listNodeValue(node1);
	PDictExtenHead pDictExtenHead2 = (PDictExtenHead)listNodeValue(node2);

	if (pDictExtenHead1->keyLen > pDictExtenHead2->keyLen) {
		return -1;
	} else if (pDictExtenHead1->keyLen < pDictExtenHead2->keyLen) {
		return 1;
	}

	return memcmp(pDictExtenHead1->key, pDictExtenHead2->key, pDictExtenHead1->keyLen);
}

int ValueCmpFun(void* v1, void* v2) {

	listNode* node1 = (listNode*)v1;
	listNode* node2 = (listNode*)v2;
	PDictExtenHead pDictExtenHead1 = (PDictExtenHead)listNodeValue(node1);
	PDictExtenHead pDictExtenHead2 = (PDictExtenHead)listNodeValue(node2);

	if (pDictExtenHead1->valueLen > pDictExtenHead2->valueLen) {
		return -1;
	} else if (pDictExtenHead1->valueLen < pDictExtenHead2->valueLen) {
		return 1;
	}

	return memcmp(pDictExtenHead1->key + pDictExtenHead1->keyLen, pDictExtenHead2->key + pDictExtenHead1->keyLen, pDictExtenHead1->valueLen);
}

void plg_DictExtenSortWithKey(void* vpDictExten) {
	PDictExten pDictExten = vpDictExten;
	plg_SortList(pDictExten->pList, KeyCmpFun);
}

void plg_DictExtenSortWithValue(void* vpDictExten) {
	PDictExten pDictExten = vpDictExten;
	plg_SortList(pDictExten->pList, ValueCmpFun);
}

void* plg_DictExtenCreate() {
	PDictExten pDictExten = malloc(sizeof(DictExten));
	pDictExten->dictExten = plg_dictCreate(&type, pDictExten, DICT_MIDDLE);
	pDictExten->pList = plg_listCreate(LIST_MIDDLE);

	return pDictExten;
}

void plg_DictExtenDestroy(void* vpDictExten) {
	PDictExten pDictExten = vpDictExten;
	plg_dictRelease(pDictExten->dictExten);
	plg_listRelease(pDictExten->pList);
	free(pDictExten);
}

int plg_DictExtenAdd(void* vpDictExten, void* key, unsigned int keyLen, void* value, unsigned int valueLen) {

	PDictExten pDictExten = vpDictExten;
	unsigned int size = sizeof(DictExtenHead) + keyLen + valueLen;
	PDictExtenHead pDictExtenHead = malloc(size);
		
	pDictExtenHead->type = 1;
	pDictExtenHead->keyLen = keyLen;
	pDictExtenHead->valueLen = valueLen;
	memcpy(pDictExtenHead->key, key, keyLen);
	memcpy(pDictExtenHead->key + keyLen, value, valueLen);
	
	dictEntry* entry = plg_dictFind(pDictExten->dictExten, pDictExtenHead);
	if (entry) {
		PDictExtenHead pEntryDictExtenHead = dictGetKey(entry);
		if (pEntryDictExtenHead->type == 1) {
			plg_dictDelete(pDictExten->dictExten, pDictExtenHead);
		} else {
			return 0;
		}
	}
	
	listNode* pListNode = plg_listAddNodeHead(pDictExten->pList, pDictExtenHead);
	plg_dictAdd(pDictExten->dictExten, pDictExtenHead, pListNode);

	return 1;
}

void* plg_DictExtenSubCreate(void* vpDictExten, void* key, unsigned int keyLen) {

	PDictExten pDictExten = vpDictExten;
	PDictExtenHead pDictExtenHead = malloc(sizeof(DictExtenHead) + keyLen + sizeof(void*));
	pDictExtenHead->type = 2;
	pDictExtenHead->keyLen = keyLen;
	pDictExtenHead->valueLen = sizeof(void*);
	memcpy(pDictExtenHead->key, key, keyLen);
	
	dictEntry * entry = plg_dictFind(pDictExten->dictExten, pDictExtenHead);
	void* subDictExten = 0;
	if (entry == 0) {
		subDictExten = plg_DictExtenCreate();
		memcpy(pDictExtenHead->key + pDictExtenHead->keyLen, &subDictExten, sizeof(void*));
		listNode* pListNode = plg_listAddNodeHead(pDictExten->pList, pDictExtenHead);
		plg_dictAdd(pDictExten->dictExten, pDictExtenHead, pListNode);
		return subDictExten;
	} else {
		free(pDictExtenHead);
		pDictExtenHead = dictGetKey(entry);
		if (pDictExtenHead->type == 2) {
			memcpy(&subDictExten, pDictExtenHead->key + pDictExtenHead->keyLen, sizeof(void*));
			return subDictExten;
		} else {
			return 0;
		}
	}
}

void plg_DictExtenDel(void* vpDictExten, void* key, unsigned int keyLen) {

	PDictExten pDictExten = vpDictExten;
	unsigned int size = sizeof(DictExtenHead) + keyLen;
	PDictExtenHead pDictExtenHead = malloc(size);

	pDictExtenHead->type = 1;
	pDictExtenHead->keyLen = keyLen;
	memcpy(pDictExtenHead->key, key, keyLen);

	dictEntry* entry = plg_dictFind(pDictExten->dictExten, pDictExtenHead);
	if (entry) {
		PDictExtenHead pEntryDictExtenHead = dictGetKey(entry);
		if (pEntryDictExtenHead->type == 1) {
			plg_dictDelete(pDictExten->dictExten, pDictExtenHead);
		}
	}

	free(pDictExtenHead);
}

void* plg_DictExtenFind(void* vpDictExten, void* key, unsigned int keyLen) {

	PDictExten pDictExten = vpDictExten;
	unsigned int size = sizeof(DictExtenHead) + keyLen;
	PDictExtenHead pDictExtenHead = malloc(size);

	pDictExtenHead->type = 1;
	pDictExtenHead->keyLen = keyLen;
	memcpy(pDictExtenHead->key, key, keyLen);

	dictEntry* entry = plg_dictFind(pDictExten->dictExten, pDictExtenHead);
	free(pDictExtenHead);
	return dictGetVal(entry);
}

int plg_DictExtenIsSub(void* ventry) {
	listNode* entry = ventry;
	PDictExtenHead pDictExtenHead = listNodeValue(entry);
	return pDictExtenHead->type == 2;
}

void* plg_DictExtenSub(void* ventry) {
	listNode* entry = ventry;
	PDictExtenHead pDictExtenHead = listNodeValue(entry);
	void* subDictExten = 0;
	if (pDictExtenHead->type == 2) {
		memcpy(&subDictExten, pDictExtenHead->key + pDictExtenHead->keyLen, sizeof(void*));
		return subDictExten;
	}
	return 0;
}

void* plg_DictExtenValue(void* ventry, unsigned int *valueLen) {
	listNode* entry = ventry;
	PDictExtenHead pDictExtenHead = listNodeValue(entry);
	*valueLen = pDictExtenHead->valueLen;
	return pDictExtenHead->key + pDictExtenHead->keyLen;
}

void* plg_DictExtenKey(void* ventry, unsigned int *keyLen) {
	listNode* entry = ventry;
	PDictExtenHead pDictExtenHead = listNodeValue(entry);
	*keyLen = pDictExtenHead->keyLen;
	return pDictExtenHead->key;
}

void* plg_DictExtenGetIterator(void* vpDictExten) {
	PDictExten pDictExten = vpDictExten;
	return plg_listGetIterator(pDictExten->pList, AL_START_HEAD);
}

void plg_DictExtenReleaseIterator(void *viter) {
	listIter *iter = viter;
	plg_listReleaseIterator(iter);
}

void* plg_DictExtenNext(void *viter) {
	listIter *iter = viter;
	return plg_listNext(iter);
}

void* plg_DictExtenGetHead(void* vpDictExten) {
	PDictExten pDictExten = vpDictExten;
	return listFirst(pDictExten->pList);
}

//help for 'C'
void* plg_DictExtenSubCreateForChar(void* pDictExten, char* key) {
	return plg_DictExtenSubCreate(pDictExten, key, strlen(key));
}

int plg_DictExtenAddForChar(void* pDictExten, char* key, void* value, unsigned int valueLen){
	return plg_DictExtenAdd(pDictExten, key, strlen(key), value, valueLen);
}

void plg_DictExtenDelForChar(void* pDictExten, char* key) {
	plg_DictExtenDel(pDictExten, key, strlen(key));
}

void* plg_DictExtenFindForChar(void* pDictExten, char* key) {
	return plg_DictExtenFind(pDictExten, key, strlen(key));
}

int plg_DictExtenAddForCharWithInt(void* pDictExten, char* key, int value) {
	return plg_DictExtenAdd(pDictExten, key, strlen(key), &value, sizeof(int));
}

int plg_DictExtenAddForCharWithUInt(void* pDictExten, char* key, unsigned int value) {
	return plg_DictExtenAdd(pDictExten, key, strlen(key), &value, sizeof(unsigned int));
}

int plg_DictExtenAddForCharWithShort(void* pDictExten, char* key, short value) {
	return plg_DictExtenAdd(pDictExten, key, strlen(key), &value, sizeof(short));
}

int plg_DictExtenAddForCharWithLL(void* pDictExten, char* key, long long value) {
	return plg_DictExtenAdd(pDictExten, key, strlen(key), &value, sizeof(long long));
}

int plg_DictExtenAddForCharWithDouble(void* pDictExten, char* key, double value) {
	return plg_DictExtenAdd(pDictExten, key, strlen(key), &value, sizeof(double));
}

int plg_DictExtenSize(void* vpDictExten) {
	PDictExten pDictExten = vpDictExten;
	return dictSize(pDictExten->dictExten);
}