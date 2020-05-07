/*cache.c - cache related functions
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

#include <pthread.h>
#include "plateform.h"
#include "pinterface.h"

#include "pelog.h"
#include "psds.h"
#include "padlist.h"
#include "pbitarray.h"
#include "pcrc16.h"
#include "pdict.h"
#include "plocks.h"
#include "pmanage.h"
#include "pcache.h"
#include "pquicksort.h"
#include "prandomlevel.h"
#include "pinterface.h"
#include "pequeue.h"
#include "pfile.h"
#include "pdisk.h"
#include "plistdict.h"
#include "ptable.h"
#include "pmemorylist.h"
#include "pdictexten.h"
#include "ptimesys.h"
#include "pjson.h"
#include "pelagia.h"

/*
When it comes to transaction, the transaction to delete a page must be submitted immediately, otherwise the address of the page in the file will be wrong
Transactions that do not delete pages can defer or user-defined commits
pDiskHandle:for disk API
pageSize:page size
mutexHandle:mutex protection
recent:whether to retrieve the write cache or not. Write cache is not retrieved for non current write data
listDictPageCache:cache pages
pageDirty:dirty pages
listDictTableHandle:table header data cache
Dicttablehandledirty: dirty record of table header data cache
//transaction
transaction_listDictPageCache: page cache in transaction
transaction_listDictTableInFile: header data cache in transaction
transaction_delPage: a deleted page in a transaction. It can only be deleted after the transaction is submitted successfully
*/
typedef struct _CacheHandle
{
	void* pDiskHandle;
	unsigned int pageSize;
	void* mutexHandle;
	short recent;
	sds objectName;

	//cache
	unsigned int cachePercent;
	unsigned int cacheInterval;
	unsigned long long cacheStamp;
	ListDict* listPageCache;
	dict* pageMask;
	dict* pageDirty;
	ListDict* listTableHandle;
	dict* dictTableHandleDirty;
	dict* delPage;

	//transaction
	ListDict* transaction_listDictPageCache;
	ListDict* transaction_listDictTableInFile;
	dict* transaction_delPage;

	//alloc memory
	void* memoryListPage;
	void* memoryListTable;

	//Statistics
	short isOpenStat;
	dict* tableName_pageCount;
	unsigned long long freeCacheCount;

} *PCacheHandle, CacheHandle;

static int PageCacheCmpFun(void* left, void* right) {

	PDiskPageHead leftPage = (PDiskPageHead)left;
	PDiskPageHead rightPage = (PDiskPageHead)right;

	if (leftPage->hitStamp > rightPage->hitStamp) {
		return 1;
	} else if (leftPage->hitStamp == rightPage->hitStamp) {
		return 0;
	} else {
		return -1;
	}
}

static void PageFreeCallback(void *privdata, void *val) {
	PCacheHandle pCacheHandle = privdata;
	plg_MemListPush(pCacheHandle->memoryListPage, listNodeValue((listNode*)val));
}

static unsigned long long hashCallback(const void *key) {
	return plg_dictGenHashFunction((unsigned char*)key, sizeof(unsigned int));
}

static int uintCompareCallback(void *privdata, const void *key1, const void *key2) {
	NOTUSED(privdata);
	if (*(unsigned int*)key1 != *(unsigned int*)key2)
		return 0;
	else
		return 1;
}

static dictType pageDictType = {
	hashCallback,
	NULL,
	NULL,
	uintCompareCallback,
	NULL,
	PageFreeCallback
};

static int sdsCompareCallback(void *privdata, const void *key1, const void *key2) {
	int l1, l2;
	NOTUSED(privdata);

	l1 = plg_sdsLen((sds)key1);
	l2 = plg_sdsLen((sds)key2);
	if (l1 != l2) return 0;
	return memcmp(key1, key2, l1) == 0;
}

static unsigned long long sdsHashCallback(const void *key) {
	return plg_dictGenHashFunction((unsigned char*)key, plg_sdsLen((char*)key));
}

static void TableFreeCallback(void* privdata, void *val) {

	PCacheHandle pCacheHandle = privdata;
	plg_MemListPush(pCacheHandle->memoryListTable, plg_TablePTableInFile(listNodeValue((listNode*)val)));
	plg_TableDestroyHandle(listNodeValue((listNode*)val));
}

static void sdsFreeCallback(void *privdata, void *val) {
	DICT_NOTUSED(privdata);
	plg_sdsFree(val);
}

static dictType tableDictType = {
	sdsHashCallback,
	NULL,
	NULL,
	sdsCompareCallback,
	sdsFreeCallback,
	TableFreeCallback
};

static void TableHeadFreeCallback(void* privdata, void *val) {

	PCacheHandle pCacheHandle = privdata;
	plg_MemListPush(pCacheHandle->memoryListTable, listNodeValue((listNode*)val));
}

static dictType tableHeadDictType = {
	sdsHashCallback,
	NULL,
	NULL,
	sdsCompareCallback,
	NULL,
	TableHeadFreeCallback
};

static dictType maskDictType = {
	hashCallback,
	NULL,
	NULL,
	uintCompareCallback,
	NULL,
	NULL
};
/*
loading page from file;
*/
SDS_TYPE
unsigned int cache_LoadPageFromFile(void* pvCacheHandle, unsigned int pageAddr, void* page) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	if (plg_DiskIsNoSave(pCacheHandle->pDiskHandle)) {
		return 0;
	}

	if (0 == plg_FileLoadPage(plg_DiskFileHandle(pCacheHandle->pDiskHandle), FULLSIZE(pCacheHandle->pageSize), pageAddr, page)){
		elog(log_error, "cache_LoadPageFromFile.plg_FileLoadPage");
		return 0;
	}

	//check crc
	PDiskPageHead pdiskPageHead = (PDiskPageHead)page;
	SDS_CHECK(pvCacheHandle, pageAddr);
	char* pdiskBitPage = (char*)page + sizeof(DiskPageHead);
	unsigned short crc = plg_crc16(pdiskBitPage, FULLSIZE(pCacheHandle->pageSize) - sizeof(DiskPageHead));
	if (pdiskPageHead->crc == 0 || pdiskPageHead->crc != crc) {
		elog(log_error, "page crc error!");
		return 0;
	}
	return 1;
}

void plg_CacheSetStat(void* pvCacheHandle, short stat) {
	PCacheHandle pCacheHandle = pvCacheHandle;
	pCacheHandle->isOpenStat = stat;
}

static unsigned int cache_ArrangementCheckBigValue(void* pvCacheHandle, void* page) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	PDiskPageHead pDiskPageHead = (PDiskPageHead)page;
	PDiskValuePage pDiskValuePage = (PDiskValuePage)((unsigned char*)page + sizeof(DiskPageHead));

	if (pDiskPageHead->type != VALUEPAGE) {
		return 0;
	}

	if (pDiskValuePage->valueLength == 0) {
		return 0;
	}

	unsigned long long sec = plg_GetCurrentSec();
	if (pDiskValuePage->valueArrangmentStamp + _ARRANGMENTTIME_ < sec) {
		return 0;
	}
	pDiskValuePage->valueArrangmentStamp = sec;

	unsigned int pageSize = FULLSIZE(pCacheHandle->pageSize) - sizeof(DiskPageHead) - sizeof(DiskTablePage);
	if (((float)pDiskValuePage->valueDelSize / pageSize) * 100 > _ARRANGMENTPERCENTAGE_) {
		plg_TableArrangmentBigValue(pCacheHandle->pageSize, page);
	}

	return 1;
}

static unsigned int cache_ArrangementCheck(void* pTableHandle, void* page) {

	PCacheHandle pCacheHandle = plg_TableOperateHandle(pTableHandle);
	PDiskPageHead pDiskPageHead = (PDiskPageHead)page;
	PDiskTablePage pDiskTablePage = (PDiskTablePage)((unsigned char*)page + sizeof(DiskPageHead));

	if (pDiskPageHead->type != TABLEPAGE) {
		cache_ArrangementCheckBigValue(pCacheHandle, page);
		return 0;
	}

	if (pDiskTablePage->tableLength == 0) {
		return 0;
	}

	unsigned long long sec = plg_GetCurrentSec();
	if (pDiskTablePage->arrangmentStamp == 0) {
		pDiskTablePage->arrangmentStamp = sec;
	}

	if (pDiskTablePage->arrangmentStamp + _ARRANGMENTTIME_ > sec) {
		return 0;
	}
	pDiskTablePage->arrangmentStamp = sec;

	unsigned int pageSize = FULLSIZE(pCacheHandle->pageSize) - sizeof(DiskPageHead) - sizeof(DiskTablePage);
	if (((float)pDiskTablePage->delSize / pageSize * 100) > _ARRANGMENTPERCENTAGE_) {
		plg_TableArrangementPage(pCacheHandle->pageSize, page);
	}

	return 1;
}



static unsigned int cache_FindPage(void* pTableHandle, unsigned int pageAddr, void** page) {

	PCacheHandle pCacheHandle = plg_TableOperateHandle(pTableHandle);
	elog(log_fun, "cache_FindPage.pageAddr:%i recent:%i", pageAddr, pCacheHandle->recent);
	if (pCacheHandle->recent) {
		dictEntry* findTranPageEntry = plg_dictFind(plg_ListDictDict(pCacheHandle->transaction_listDictPageCache), &pageAddr);

		if (findTranPageEntry !=0 ) {
			*page = plg_ListDictGetVal(findTranPageEntry);

			PDiskPageHead leftPage = *page;
			if (leftPage->type == TABLEPAGE) {
				plg_assert(plg_TableCheckLength(leftPage, pCacheHandle->pageSize));
				plg_assert(plg_TableCheckSpace(leftPage));
			}
			leftPage->hitStamp = plg_GetCurrentSec();
			return 1;
		} else {
			elog(log_details, "cache_FindPage.transaction_listDictPageCache.miss:%i", pageAddr);
		}
	}

	dictEntry* findPageEntry = plg_dictFind(plg_ListDictDict(pCacheHandle->listPageCache), &pageAddr);
	if (findPageEntry == 0) {
		*page = plg_MemListPop(pCacheHandle->memoryListPage);
		if (0 == cache_LoadPageFromFile(pCacheHandle, pageAddr, *page)) {
			elog(log_error, "cache_FindPage.disk load page %i!", pageAddr);
			plg_MemListPush(pCacheHandle->memoryListPage, *page);
			return 0;
		} else {
			elog(log_details, "cache_FindPage.cache_LoadPageFromFile:%i", pageAddr);
		}
		PDiskPageHead leftPage = *page;
		if (pCacheHandle->isOpenStat) {
			dictAddValueWithUint(pCacheHandle->tableName_pageCount, plg_TableName(pTableHandle), 1);
		}		
		plg_ListDictAdd(pCacheHandle->listPageCache, &leftPage->addr, *page);
	} else {
		*page = plg_ListDictGetVal(findPageEntry);
	}

	PDiskPageHead leftPage = *page;
	if (leftPage->type == TABLEPAGE) {
		plg_assert(plg_TableCheckLength(leftPage, pCacheHandle->pageSize));
		plg_assert(plg_TableCheckSpace(leftPage));
	}
	leftPage->hitStamp = plg_GetCurrentSec();
	return 1;
}

/*
If it does not exist at the end of the file at the time of writing,
Will be created when the file is written
Note that this function does not write to the file immediately, so it is inconsistent with the file
This function cannot create bitpage
1, find in bitpage
2, if no find to create bitpage
3, sign bitpage
4, create page
Retpage: the address of the returned new page
Type: page type
Prvid: Previous page of page chain
NextID: address of the next page of the page chain of the previous page
*/
static unsigned int cache_CreatePage(void* pTableHandle,
	void** retPage,
	char type) {

	PCacheHandle pCacheHandle = plg_TableOperateHandle(pTableHandle);
	unsigned int pageAddr = 0;
	if (dictSize(pCacheHandle->transaction_delPage)) {

		dictEntry* entry = plg_dictGetRandomKey(pCacheHandle->transaction_delPage);
		if (entry) {
			pageAddr = *(unsigned int*)dictGetKey(entry);
			plg_dictDelete(pCacheHandle->transaction_delPage, dictGetKey(entry));
		}
	} else {
		plg_DiskAllocPage(pCacheHandle->pDiskHandle, &pageAddr);
		if (pageAddr == 0) {
			return 0;
		}

		elog(log_fun, "cache_CreatePage.plg_DiskAllocPage:%i", pageAddr);
	}
	//calloc memory
	*retPage = plg_MemListPop(pCacheHandle->memoryListPage);
	memset(*retPage, 0, FULLSIZE(pCacheHandle->pageSize));

	//init
	PDiskPageHead pDiskPageHead = (PDiskPageHead)*retPage;
	pDiskPageHead->addr = pageAddr;
	pDiskPageHead->type = type;
	pDiskPageHead->hitStamp = plg_GetCurrentSec();

	//add to chache
	if (pCacheHandle->isOpenStat) {
		dictAddValueWithUint(pCacheHandle->tableName_pageCount, plg_TableName(pTableHandle), 1);
	}
	plg_ListDictAdd(pCacheHandle->transaction_listDictPageCache, &pDiskPageHead->addr, *retPage);
	return 1;
}

/*
Inverse function of cache_CreatePage
*/
static unsigned int cache_DelPage(void* pTableHandle, unsigned int pageAddr) {

	PCacheHandle pCacheHandle = plg_TableOperateHandle(pTableHandle);
	elog(log_fun, "cache_DelPage %i", pageAddr);

	if (pCacheHandle->isOpenStat) {
		dictAddValueWithUint(pCacheHandle->tableName_pageCount, plg_TableName(pTableHandle), -1);
	}

	//add to transaction
	plg_ListDictDel(pCacheHandle->transaction_listDictPageCache, &pageAddr);

	//Temporary recycle pageAddr
	dictAddWithUint(pCacheHandle->transaction_delPage, pageAddr, NULL);
	return 1;
}


static void* cache_pageCopyOnWrite(void* pTableHandle, unsigned int pageAddr, void* page) {
	
	elog(log_fun, "cache_pageCopyOnWrite.pageAddr:%i", pageAddr);
	PCacheHandle pCacheHandle = plg_TableOperateHandle(pTableHandle);

	PDiskPageHead pDiskPageHead = (PDiskPageHead)page;
	if (pDiskPageHead->type == TABLEPAGE) {
		plg_assert(plg_TableCheckLength(page, pCacheHandle->pageSize));
		plg_assert(plg_TableCheckSpace(page));
	}

	dictEntry* entry = plg_dictFind(plg_ListDictDict(pCacheHandle->transaction_listDictPageCache), &pageAddr);
	if (entry) {
		PDiskPageHead pDiskPageHead = (PDiskPageHead)plg_ListDictGetVal(entry);
		if (pDiskPageHead->type == TABLEPAGE) {
			plg_assert(plg_TableCheckLength(page, pCacheHandle->pageSize));
			plg_assert(plg_TableCheckSpace(pDiskPageHead));
		}
		return pDiskPageHead;
	} else {
		void* copyPage = plg_MemListPop(pCacheHandle->memoryListPage);
		memcpy(copyPage, page, FULLSIZE(pCacheHandle->pageSize));
		PDiskPageHead pDiskPageHead = (PDiskPageHead)copyPage;
		if (pDiskPageHead->type == TABLEPAGE) {
			plg_assert(plg_TableCheckLength(page, pCacheHandle->pageSize));
			plg_assert(plg_TableCheckSpace(copyPage));
		}
		plg_ListDictAdd(pCacheHandle->transaction_listDictPageCache, &pDiskPageHead->addr, copyPage);
		return copyPage;
	}
}

static void* cache_tableCopyOnWrite(void* pTableHandle, sds table, void* tableHead) {

	elog(log_fun, "cache_tableCopyOnWrite.table:%s", table);
	PCacheHandle pCacheHandle = plg_TableOperateHandle(pTableHandle);
	//check not is set type
	dictEntry* entry = plg_dictFind(plg_ListDictDict(pCacheHandle->listTableHandle), table);
	if (!entry) {
		return tableHead;
	}

	entry = plg_dictFind(plg_ListDictDict(pCacheHandle->transaction_listDictTableInFile), table);
	if (entry) {
		return plg_ListDictGetVal(entry);
	} else {
		PTableInFile copyTableInFile = plg_MemListPop(pCacheHandle->memoryListTable);
		memcpy(copyTableInFile, tableHead, sizeof(TableInFile));
		plg_ListDictAdd(pCacheHandle->transaction_listDictTableInFile, table, copyTableInFile);
		return copyTableInFile;
	}
}

/*
cache 使用事务机制不需要提交脏页
*/
static void cache_addDirtyPage(void* pTableHandle, unsigned int pageAddr) {
	NOTUSED(pTableHandle);
	NOTUSED(pageAddr);
	return;
}

static void cache_addDirtyTable(void* pTableHandle, sds table){
	NOTUSED(pTableHandle);
	NOTUSED(table);
	return;
}

static void* cache_findTableInFile(void* pTableHandle, sds table, void* tableInFile) {
	
	elog(log_fun, "cache_findTableInFile.table:%s", table);
	PCacheHandle pCacheHandle = plg_TableOperateHandle(pTableHandle);
	//find in tran
	if (pCacheHandle->recent) {
		dictEntry* entry = plg_dictFind(plg_ListDictDict(pCacheHandle->transaction_listDictTableInFile), table);
		if (entry != 0) {
			return plg_ListDictGetVal(entry);
		}
	}

	return tableInFile;
}

static TableHandleCallBack tableHandleCallBack = {
	cache_FindPage,
	cache_CreatePage,
	cache_DelPage,
	cache_ArrangementCheck,
	cache_pageCopyOnWrite,
	cache_addDirtyPage,
	cache_tableCopyOnWrite,
	cache_addDirtyTable,
	cache_findTableInFile
};

static void FreeCallback(void *privdata, void *val) {
	DICT_NOTUSED(privdata);
	free(val);
}

static dictType SdsDictType = {
	sdsHashCallback,
	NULL,
	NULL,
	sdsCompareCallback,
	NULL,
	FreeCallback
};

void* plg_CacheCreateHandle(void* pDiskHandle) {

	PCacheHandle pCacheHandle = malloc(sizeof(CacheHandle));
	pCacheHandle->pageSize = plg_DiskGetPageSize(pDiskHandle);
	pCacheHandle->pDiskHandle = pDiskHandle;
	pCacheHandle->recent = 1;

	pCacheHandle->cachePercent = 5;
	pCacheHandle->cacheInterval = 300;
	pCacheHandle->cacheStamp = plg_GetCurrentSec();
	pCacheHandle->listPageCache = plg_ListDictCreateHandle(&pageDictType, DICT_MIDDLE, LIST_MIDDLE, PageCacheCmpFun, pCacheHandle);
	pCacheHandle->pageDirty = plg_dictCreate(plg_DefaultUintPtr(), NULL, DICT_MIDDLE);
	pCacheHandle->pageMask = plg_dictCreate(&maskDictType, NULL, DICT_MIDDLE);
	pCacheHandle->listTableHandle = plg_ListDictCreateHandle(&tableDictType, DICT_MIDDLE, LIST_MIDDLE, plg_TableHandleCmpFun, pCacheHandle);
	pCacheHandle->dictTableHandleDirty = plg_dictCreate(plg_DefaultSdsDictPtr(), NULL, DICT_MIDDLE);
	pCacheHandle->delPage = plg_dictCreate(plg_DefaultUintPtr(), NULL, DICT_MIDDLE);

	pCacheHandle->mutexHandle = plg_MutexCreateHandle(LockLevel_1);
	pCacheHandle->objectName = plg_sdsNew("cache");

	pCacheHandle->transaction_listDictPageCache = plg_ListDictCreateHandle(&pageDictType, DICT_MIDDLE, LIST_MIDDLE, NULL, pCacheHandle);
	pCacheHandle->transaction_listDictTableInFile = plg_ListDictCreateHandle(&tableHeadDictType, DICT_MIDDLE, LIST_MIDDLE, NULL, pCacheHandle);
	pCacheHandle->transaction_delPage = plg_dictCreate(plg_DefaultUintPtr(), NULL, DICT_MIDDLE);
	pCacheHandle->memoryListPage = plg_MemListCreate(60, FULLSIZE(pCacheHandle->pageSize), 0);
	pCacheHandle->memoryListTable = plg_MemListCreate(60, sizeof(TableInFile), 0);

	pCacheHandle->tableName_pageCount = plg_dictCreate(&SdsDictType, NULL, DICT_MIDDLE);
	pCacheHandle->isOpenStat = 0;
	pCacheHandle->freeCacheCount = 0;
	return pCacheHandle;
}

void plg_CacheDestroyHandle(void* pvCacheHandle) {

	//free page dict
	PCacheHandle pCacheHandle = pvCacheHandle;
	plg_sdsFree(pCacheHandle->objectName);
	plg_ListDictDestroyHandle(pCacheHandle->listPageCache);
	plg_dictRelease(pCacheHandle->pageDirty);
	plg_dictRelease(pCacheHandle->pageMask);
	plg_ListDictDestroyHandle(pCacheHandle->listTableHandle);
	plg_dictRelease(pCacheHandle->dictTableHandleDirty);
	plg_dictRelease(pCacheHandle->delPage);

	plg_ListDictDestroyHandle(pCacheHandle->transaction_listDictPageCache);
	plg_ListDictDestroyHandle(pCacheHandle->transaction_listDictTableInFile);
	plg_dictRelease(pCacheHandle->transaction_delPage);

	plg_MemListDestory(pCacheHandle->memoryListPage);
	plg_MemListDestory(pCacheHandle->memoryListTable);
	plg_MutexDestroyHandle(pCacheHandle->mutexHandle);

	plg_dictRelease(pCacheHandle->tableName_pageCount);
	free(pCacheHandle);
}

/*
查询或创建?
首先在本地查找,
然后去disk查找,
*/
static void* cahce_GetTableHandle(void* pvCacheHandle, sds sdsTable) {

	//find in local
	PCacheHandle pCacheHandle = pvCacheHandle;
	dictEntry* entry = plg_dictFind(plg_ListDictDict(pCacheHandle->listTableHandle), sdsTable);
	if (entry != 0) {
		return plg_ListDictGetVal(entry);
	}

	sds newTable = plg_sdsNew(sdsTable);
	PTableInFile pTableInFile = plg_MemListPop(pCacheHandle->memoryListTable);
	plg_TableInitTableInFile(pTableInFile);

	void* pDictExten = plg_DictExtenCreate();
	//no find
	if (0 > plg_DiskTableFind(pCacheHandle->pDiskHandle, newTable, pDictExten)) {
		plg_sdsFree(newTable);
		elog(log_error, "Table <%s> does not exist!", sdsTable);
	} else {
		void* node = plg_DictExtenGetHead(pDictExten);
		if (node) {
			unsigned int valueLen;
			void* ptr = plg_DictExtenValue(node, &valueLen);
			if (valueLen) {
				memcpy(pTableInFile, ptr, sizeof(TableInFile));
			}
		}
		plg_DictExtenDestroy(pDictExten);
		void* ptableHandle = plg_TableCreateHandle(pTableInFile, pCacheHandle, pCacheHandle->pageSize, newTable, &tableHandleCallBack);
		plg_ListDictAdd(pCacheHandle->listTableHandle, newTable, ptableHandle);
		return ptableHandle;
	}
	return 0;
}

unsigned short plg_CacheGetTableType(void* pvCacheHandle, sds sdsTable, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	int r = 0;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		r = plg_TableGetTableType(pTableHandle);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return r;
}

unsigned short plg_CacheSetTableType(void* pvCacheHandle, sds sdsTable, unsigned short tableType) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	unsigned short r = 0;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {

		r = plg_TableSetTableType(pTableHandle, tableType);
	}
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return r;
}

unsigned short plg_CacheSetTableTypeIfByte(void* pvCacheHandle, sds sdsTable, unsigned short tableType) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	unsigned short r = 0;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {

		r = plg_TableSetTableTypeIfByte(pTableHandle, tableType);
	}
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return r;
}

unsigned int plg_CacheTableAdd(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* value, unsigned int length) {
	
	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	unsigned int r = 0;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		if (length > plg_TableBigValueSize()) {
			DiskKeyBigValue diskKeyBigValue;
			if (0 == plg_TableNewBigValue(pTableHandle, value, length, &diskKeyBigValue))
				return 0;
			r = plg_TableAddWithAlter(pTableHandle, vKey, keyLen, VALUE_BIGVALUE, &diskKeyBigValue, sizeof(DiskKeyBigValue));
		} else {
			r = plg_TableAddWithAlter(pTableHandle, vKey, keyLen, VALUE_NORMAL, value, length);
		}
		
	}
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return r;
}

unsigned int plg_CacheTableMultiAdd(void* pvCacheHandle, sds sdsTable, void* pDictExten) {

	unsigned int r = 0;
	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		r = plg_TableMultiAdd(pTableHandle, pDictExten);
	}
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);

	return r;
};

unsigned int plg_CacheTableAddIfNoExist(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* value, unsigned int length) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	unsigned int r = 0;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		if (length > plg_TableBigValueSize()) {
			DiskKeyBigValue diskKeyBigValue;
			if (0 == plg_TableNewBigValue(pTableHandle, value, length, &diskKeyBigValue))
				return 0;
			r = plg_TableAddIfNoExist(pTableHandle, vKey, keyLen, VALUE_BIGVALUE, &diskKeyBigValue, sizeof(DiskKeyBigValue));
		} else {
			r = plg_TableAddIfNoExist(pTableHandle, vKey, keyLen, VALUE_NORMAL, value, length);
		}

	}
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return r;
}

unsigned int plg_CacheTableRename(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* vNewKey, short newKeyLen) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	unsigned int r = 0;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		r = plg_TableRename(pTableHandle, vKey, keyLen, vNewKey, newKeyLen);
	}
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return r;
}

unsigned int plg_CacheTableIsKeyExist(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	unsigned int r = 0;
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		r = plg_TableIsKeyExist(pTableHandle, vKey, keyLen);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return r;
}

unsigned int plg_CacheTableDel(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	unsigned int r = 0;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		r = plg_TableDel(pTableHandle, vKey, keyLen);
	}
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return r;
}

/*
Int recent: whether to search for the latest data. Because cachehandle will be distributed to all threads,
The current task can only get the latest data when retrieving its own data. When retrieving non owned data, it needs to return the submitted data
Because cache itself can't distinguish whether the current task has data, it needs to judge in the task interface
Recent: indicates that the page layer uses the latest cached data or does not. If no write is generated during data usage, the data does not need to be locked.
Based on the above, read-only data can be concurrent without lock. One is that it is difficult to write data, including statistical data, when reading complex data.
2、 The improvement of efficiency is limited in which part of data is read before data is written. 3、 Degradation of a lock that writes data results in increased complexity.
Because the use time of the mutex is reduced, the efficiency is improved to some extent, but the improvement is not obvious.
*/
int plg_CacheTableFind(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* pDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	int r = 0;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		r = plg_TableFind(pTableHandle, vKey, keyLen, pDictExten, 0);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return r;
}

void plg_CacheTableMultiFind(void* pvCacheHandle, sds sdsTable, void* pKeyDictExten, void* pValueDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableMultiFind(pTableHandle, pKeyDictExten, pValueDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
}

unsigned int plg_CacheTableRand(void* pvCacheHandle, sds sdsTable, void* pDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	unsigned int r = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		r = plg_TableRand(pTableHandle, pDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return r;
}

unsigned int plg_CacheTableLength(void* pvCacheHandle, sds sdsTable, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	unsigned int len = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		len = plg_TableLength(pTableHandle);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return len;
}

void plg_CacheTableLimite(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, unsigned int left , unsigned int right, void* pDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableLimite(pTableHandle, vKey, keyLen, left, right, pDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
}

void plg_CacheTableOrder(void* pvCacheHandle, sds sdsTable, short order, unsigned int limite, void* pDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableOrder(pTableHandle, order, limite, pDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
}

void plg_CacheTableRang(void* pvCacheHandle, sds sdsTable, void* beginKey, short beginKeyLen, void* endKey, short endKeyLen, void* pDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableRang(pTableHandle, beginKey, beginKeyLen, endKey, endKeyLen, pDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
}

void plg_CacheTablePoint(void* pvCacheHandle, sds sdsTable, void* beginKey, short beginKeyLen, unsigned int direction, unsigned int offset, void* pDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TablePoint(pTableHandle, beginKey, beginKeyLen, direction, offset, pDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
}

void plg_CacheTablePattern(void* pvCacheHandle, sds sdsTable, void* beginKey, short beginKeyLen, void* endKey, short endKeyLen, void* pattern, short patternLen, void* pDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TablePattern(pTableHandle, beginKey, beginKeyLen, endKey, endKeyLen, pattern, patternLen, pDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
}

void plg_CacheTableClear(void* pvCacheHandle, sds sdsTable) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableClear(pTableHandle, 1);
	}
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
}

unsigned int plg_CacheTableSetAdd(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* vValue, short valueLen) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	unsigned int r = 0;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		r = plg_TableSetAdd(pTableHandle, vKey, keyLen, vValue, valueLen);
	}
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return r;
}

void plg_CacheTableSetRang(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* beginValue, short beginValueLen, void* endValue, short endValueLen, void* pDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableSetRang(pTableHandle, vKey, keyLen, beginValue, beginValueLen, endValue, endValueLen, pDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
}

void plg_CacheTableSetPoint(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* beginValue, short beginValueLen, unsigned int direction, unsigned int offset, void* pDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableSetPoint(pTableHandle, vKey, keyLen, beginValue, beginValueLen, direction, offset, pDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
}

void plg_CacheTableSetLimite(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* vValue, short valueLen, unsigned int left, unsigned int right, void* pDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableSetLimite(pTableHandle, vKey, keyLen, vValue, valueLen, left, right, pDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
}

unsigned int plg_CacheTableSetLength(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	unsigned int len = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;

	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		len = plg_TableSetLength(pTableHandle, vKey, keyLen);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return len;
}

unsigned int plg_CacheTableSetIsKeyExist(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* vValue, short valueLen, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	unsigned int r = 0;
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		r = plg_TableSetIsKeyExist(pTableHandle, vKey, keyLen, vValue, valueLen);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return r;
}

void plg_CacheTableSetMembers(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* pDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableSetMembers(pTableHandle, vKey, keyLen, pDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
}

unsigned int plg_CacheTableSetRand(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* pDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	unsigned int r = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		r = plg_TableSetRand(pTableHandle, vKey, keyLen, pDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return r;
}

void plg_CacheTableSetDel(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* pValueDictExten) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableSetDel(pTableHandle, vKey, keyLen, pValueDictExten);
	}
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
}

unsigned int plg_CacheTableSetPop(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* pDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	unsigned int r = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		r = plg_TableSetPop(pTableHandle, vKey, keyLen, pDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return r;
}

unsigned int plg_CacheTableSetRangCount(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* beginValue, short beginValueLen, void* endValue, short endValueLen, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	unsigned int count = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		count = plg_TableSetRangCount(pTableHandle, vKey, keyLen, beginValue, beginValueLen, endValue, endValueLen);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return count;
}

/*
不支持跨表的集合操作
如果两个集合不是一个类型那么不需要联合
如果两个集合是一个类型必然可以在一个表
*/
unsigned int plg_CacheTableSetUion(void* pvCacheHandle, sds sdsTable, void* pSetDictExten, void* pKeyDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	unsigned int count = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableSetUion(pTableHandle, pSetDictExten, pKeyDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return count;
}

unsigned int plg_CacheTableSetUionStore(void* pvCacheHandle, sds sdsTable, void* pSetDictExten, void* vKey, short keyLen) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	unsigned int count = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableSetUionStore(pTableHandle, pSetDictExten, vKey, keyLen);
	}
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return count;
}

unsigned int plg_CacheTableSetInter(void* pvCacheHandle, sds sdsTable, void* pSetDictExten, void* pKeyDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	unsigned int count = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableSetInter(pTableHandle, pSetDictExten, pKeyDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return count;
}

unsigned int plg_CacheTableSetInterStore(void* pvCacheHandle, sds sdsTable, void* pSetDictExten, void* vKey, short keyLen) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	unsigned int count = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableSetInterStore(pTableHandle, pSetDictExten, vKey, keyLen);
	}
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return count;
}

unsigned int plg_CacheTableSetDiff(void* pvCacheHandle, sds sdsTable, void* pSetDictExten, void* pKeyDictExten, short recent) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	unsigned int count = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableSetDiff(pTableHandle, pSetDictExten, pKeyDictExten);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return count;
}

unsigned int plg_CacheTableSetDiffStore(void* pvCacheHandle, sds sdsTable, void* pSetDictExten, void* vKey, short keyLen) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	unsigned int count = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableSetDiffStore(pTableHandle, pSetDictExten, vKey, keyLen);
	}
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return count;
}

unsigned int plg_CacheTableSetMove(void* pvCacheHandle, sds sdsTable, void* vSrcKey, short  srcKeyLen, void* vDesKey, short desKeyLen, void* vValue, short valueLen) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	unsigned int count = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableSetMove(pTableHandle, vSrcKey, srcKeyLen, vDesKey, desKeyLen, vValue, valueLen);
	}
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return count;
}

unsigned int plg_CacheTableMembersWithJson(void* pvCacheHandle, sds sdsTable, void* vjsonRoot, short recent) {

	pJSON* jsonRoot = vjsonRoot;
	PCacheHandle pCacheHandle = pvCacheHandle;
	unsigned int count = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	pCacheHandle->recent = recent;
	void* pTableHandle = cahce_GetTableHandle(pCacheHandle, sdsTable);
	if (pTableHandle != 0) {
		plg_TableMembersWithJson(pTableHandle, jsonRoot);
	}
	pCacheHandle->recent = 1;
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	return count;
}

/*
flush dirty page to file
*/
unsigned int plg_CacheFlushDirtyToFile(void* pvCacheHandle) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	if (plg_DiskIsNoSave(pCacheHandle->pDiskHandle)) {
		return 0;
	}

	if (dictSize(pCacheHandle->pageDirty) == 0) {
		return 0;
	}

	//flush dict page
	int dirtySize = dictSize(pCacheHandle->pageDirty);
	PFileParamPageInfo pPFileParamPageInfo = malloc(dirtySize*sizeof(FileParamPageInfo));
	unsigned count = 0;
	void** memArrary;
	void* fileHandle = plg_DiskFileHandle(pCacheHandle->pDiskHandle);
	plg_FileMallocPageArrary(fileHandle, &memArrary, dirtySize);

	dictIterator* dictIter = plg_dictGetSafeIterator(pCacheHandle->pageDirty);
	dictEntry* dictNode;
	while ((dictNode = plg_dictNext(dictIter)) != NULL) {
		pPFileParamPageInfo[count].pageId = *(unsigned int*)dictGetKey(dictNode);

		dictEntry* maskNode = plg_dictFind(pCacheHandle->pageMask, &pPFileParamPageInfo[count].pageId);
		if (maskNode) {
			pPFileParamPageInfo[count].pPMaskPage = dictGetVal(maskNode);
			plg_MaskBit(pPFileParamPageInfo[count].pPMaskPage, 1);
		} else {
			pPFileParamPageInfo[count].pPMaskPage = 0;
		}

		count++;
	}
	plg_dictReleaseIterator(dictIter);

	//Find in the original cache, update if it exists
	for (int l = 0; l < dirtySize; l++) {
		dictEntry* diskNode = plg_dictFind(plg_ListDictDict(pCacheHandle->listPageCache), &pPFileParamPageInfo[l].pageId);
		if (diskNode != 0) {
			char* page = plg_ListDictGetVal(diskNode);
			if (pPFileParamPageInfo[l].pageId != 0) {

				PDiskPageHead pDiskPageHead = (PDiskPageHead)page;
				char* pDiskPage = page + sizeof(DiskPageHead);

				//Calculate CRC
				pDiskPageHead->crc = plg_crc16((char*)pDiskPage, FULLSIZE(pCacheHandle->pageSize) - sizeof(DiskPageHead));
			}
			memcpy(memArrary[l], page, FULLSIZE(pCacheHandle->pageSize));
		}
	}

	//Clear before switching to file critical area for transaction integrity
	plg_dictEmpty(pCacheHandle->pageDirty, NULL);
	plg_dictEmpty(pCacheHandle->pageMask, NULL);

	plg_FileFlushPage(fileHandle, pPFileParamPageInfo, memArrary, dirtySize);
	return 1;
}

static unsigned int cache_PageCount(void* pvCacheHandle) {
	PCacheHandle pCacheHandle = pvCacheHandle;

	unsigned int r = 1;
	if (pCacheHandle->isOpenStat) {
		unsigned int pageCount = 0;
		dictIterator* iter_pageCount = plg_dictGetSafeIterator(pCacheHandle->tableName_pageCount);
		dictEntry* node_pageCount;
		while ((node_pageCount = plg_dictNext(iter_pageCount)) != NULL) {
			char* key = dictGetKey(node_pageCount);
			pageCount += *(unsigned int*)dictGetVal(node_pageCount);
			NOTUSED(key);
		}
		plg_dictReleaseIterator(iter_pageCount);
		unsigned int size = dictSize(plg_ListDictDict(pCacheHandle->listPageCache));
		r = (pageCount == (size + pCacheHandle->freeCacheCount));	
	}
	return r;
}

void plg_CachePageAllCount(void* pvCacheHandle, unsigned long long* cacheCount, unsigned long long* freeCacheCount) {
	PCacheHandle pCacheHandle = pvCacheHandle;
	*cacheCount = dictSize(plg_ListDictDict(pCacheHandle->listPageCache));
	*freeCacheCount = pCacheHandle->freeCacheCount;
}

void plg_CachePageCountPrint(void* pvCacheHandle, char** catSdsStr) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	dictIterator* iter_pageCount = plg_dictGetSafeIterator(pCacheHandle->tableName_pageCount);
	dictEntry* node_pageCount;
	while ((node_pageCount = plg_dictNext(iter_pageCount)) != NULL) {
		char* key = dictGetKey(node_pageCount);
		unsigned int* count = dictGetVal(node_pageCount);
		*catSdsStr = plg_sdsCatPrintf(*catSdsStr, "%s:%d;", key, *count);
	}
	plg_dictReleaseIterator(iter_pageCount);
	return;
}

/*
事务提交
*/
int plg_CacheCommit(void* pvCacheHandle) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	elog(log_fun, "plg_CacheCommit %U", pCacheHandle);
	short tableHead = 0, delPage = 0;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	//copy from transaction_listDictPageCache to listPageCache
	dict* t_listDictPageCache = plg_ListDictDict(pCacheHandle->transaction_listDictPageCache);
	dictIterator* itert_listDictPageCache = plg_dictGetSafeIterator(t_listDictPageCache);
	dictEntry* nodet_listDictPageCache;
	while ((nodet_listDictPageCache = plg_dictNext(itert_listDictPageCache)) != NULL) {

		//merge page
		dictEntry* pcEntry = plg_dictFind(plg_ListDictDict(pCacheHandle->listPageCache), dictGetKey(nodet_listDictPageCache));
		void* page = 0;
		if (pcEntry == 0) {
			//calloc memory
			page = plg_MemListPop(pCacheHandle->memoryListPage);
			memcpy(page, plg_ListDictGetVal(nodet_listDictPageCache), FULLSIZE(pCacheHandle->pageSize));

			//add to chache
			PDiskPageHead pDiskPageHead = (PDiskPageHead)page;
			plg_ListDictAdd(pCacheHandle->listPageCache, &pDiskPageHead->addr, page);

			PMaskPage pPMaskPage = plg_MaskMalloc(pDiskPageHead->addr, 0, 0, FULLSIZE(pCacheHandle->pageSize));
			plg_dictAdd(pCacheHandle->pageMask, &pPMaskPage->pageId, pPMaskPage);
		} else {
			page = plg_ListDictGetVal(pcEntry);
			PDiskPageHead pDiskPageHead = (PDiskPageHead)page;

			dictEntry* pMaskEntry = plg_dictFind(pCacheHandle->pageMask, &pDiskPageHead->addr);
			if (pMaskEntry) {
				plg_MaskCmp(dictGetVal(pMaskEntry), page, plg_ListDictGetVal(nodet_listDictPageCache), FULLSIZE(pCacheHandle->pageSize));
			} else {
				PMaskPage pPMaskPage = plg_MaskMalloc(pDiskPageHead->addr, page, plg_ListDictGetVal(nodet_listDictPageCache), FULLSIZE(pCacheHandle->pageSize));
				plg_dictAdd(pCacheHandle->pageMask, &pPMaskPage->pageId, pPMaskPage);
			}

			memcpy(page, plg_ListDictGetVal(nodet_listDictPageCache), FULLSIZE(pCacheHandle->pageSize));
		}

		elog(log_details, "plg_CacheCommit.tranPageId: %i", *(unsigned int*)dictGetKey(nodet_listDictPageCache));
		dictAddWithUint(pCacheHandle->pageDirty, *(unsigned int*)dictGetKey(nodet_listDictPageCache), NULL);
	}
	plg_dictReleaseIterator(itert_listDictPageCache);
	plg_ListDictEmpty(pCacheHandle->transaction_listDictPageCache);

	//copy from transaction_listDictTableInFile to dictTableHandleDirty
	dict* t_listDictTableInFile = plg_ListDictDict(pCacheHandle->transaction_listDictTableInFile);
	dictIterator* itert_listDictTableInFile = plg_dictGetSafeIterator(t_listDictTableInFile);
	dictEntry* nodet_listDictTableInFile;
	while ((nodet_listDictTableInFile = plg_dictNext(itert_listDictTableInFile)) != NULL) {

		dictEntry* pcEntry = plg_dictFind(plg_ListDictDict(pCacheHandle->listTableHandle), dictGetKey(nodet_listDictTableInFile));
		if (pcEntry != 0) {

			tableHead++;
			memcpy(plg_TablePTableInFile(plg_ListDictGetVal(pcEntry)), plg_ListDictGetVal(nodet_listDictTableInFile), sizeof(TableInFile));
			plg_dictAdd(pCacheHandle->dictTableHandleDirty, dictGetKey(nodet_listDictTableInFile), NULL);
		}
	}
	plg_dictReleaseIterator(itert_listDictTableInFile);
	plg_ListDictEmpty(pCacheHandle->transaction_listDictTableInFile);

	//delpage
	dictIterator* itert_delpage = plg_dictGetSafeIterator(pCacheHandle->transaction_delPage);
	dictEntry* nodet_delpage;
	while ((nodet_delpage = plg_dictNext(itert_delpage)) != NULL) {

		dictEntry* pcEntry = plg_dictFind(pCacheHandle->delPage, dictGetKey(nodet_delpage));
		if (pcEntry == 0) {
			delPage++;
			dictAddWithUint(pCacheHandle->delPage, *(unsigned int*)dictGetKey(nodet_delpage), NULL);
		}
	}
	plg_dictReleaseIterator(itert_delpage);
	plg_dictEmpty(pCacheHandle->transaction_delPage, NULL);

	//pageCount
	cache_PageCount(pCacheHandle);
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);

	elog(log_details, "plg_CacheCommit.tableHead:%i delPage:%i", tableHead, delPage);
	return 1;
}

/*
事务回滚
*/
int plg_CacheRollBack(void* pvCacheHandle) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	elog(log_fun, "plg_CacheRollBack %U", pCacheHandle);
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
	plg_ListDictEmpty(pCacheHandle->transaction_listDictPageCache);
	plg_ListDictEmpty(pCacheHandle->transaction_listDictTableInFile);
	plg_dictEmpty(pCacheHandle->transaction_delPage, NULL);
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);

	return 1;
}

void plg_CacheSetInterval(void* pvCacheHandle, unsigned int interval){
	PCacheHandle pCacheHandle = pvCacheHandle;
	pCacheHandle->cacheInterval = interval;
}

void plg_CacheSetPercent(void* pvCacheHandle, unsigned int percent){
	PCacheHandle pCacheHandle = pvCacheHandle;
	pCacheHandle->cachePercent = percent;
}

/*
Defragment the page cache. You must clear all transactions and dirty pages before it can be executed according to certain conditions
*/
static void cache_Arrange(void* pvCacheHandle) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	if (plg_DiskIsNoSave(pCacheHandle->pDiskHandle)) {
		elog(log_details, "cache_Arrange but DiskIsNoSave");
		return;
	}
	elog(log_fun, "cache_Arrange %U", pCacheHandle);
	//check interval
	unsigned long long stamp = plg_GetCurrentSec();
	if (stamp - pCacheHandle->cacheStamp < pCacheHandle->cacheInterval) {
		return;
	}

	//page
	list* listPage = plg_ListDictList(pCacheHandle->listPageCache);
	listNode* nodePage = listLast(listPage);
	unsigned int limite = listLength(listPage) / 100 * pCacheHandle->cachePercent;
	unsigned int interval = pCacheHandle->cacheInterval * 3;
	unsigned int count = 0;
	do {
		if (nodePage == 0) {
			break;
		}
		PDiskPageHead pageHead = listNodeValue(nodePage);
		nodePage = listPrevNode(nodePage);
		if (stamp - pageHead->hitStamp > interval) {
			plg_ListDictDel(pCacheHandle->listPageCache, &pageHead->addr);
			pCacheHandle->freeCacheCount += 1;
		}
		if (++count > limite) {
			break;
		}
	} while (1);

	//tableHandle
	list* listTable = plg_ListDictList(pCacheHandle->listTableHandle);
	listNode* nodeTable = listLast(listTable);
	limite = listLength(listPage) / 100 * pCacheHandle->cachePercent;
	count = 0;
	do {
		if (nodeTable == 0) {
			break;
		}
		void* tableHandle = listNodeValue(nodeTable);
		nodeTable = listPrevNode(nodeTable);
		if (stamp - plg_TableHitStamp(tableHandle) > interval) {
			plg_ListDictDel(pCacheHandle->listTableHandle, plg_TableName(tableHandle));
		}
		if (++count > limite) {
			break;
		}
	} while (1);

	pCacheHandle->cacheStamp = stamp;
}

/*
Triggered page update to file
Trigger a transaction commit and update cache to file
Note that it is different from cache scheduling, but scheduling strategies need to be formulated
Caching also requires scheduling strategies.
*/
void plg_CacheFlush(void* pvCacheHandle) {

	PCacheHandle pCacheHandle = pvCacheHandle;
	MutexLock(pCacheHandle->mutexHandle, pCacheHandle->objectName);	
	//process pCacheHandle->dictTableHandleDirty
	dictIterator* iter_dictTableHandleDirty = plg_dictGetSafeIterator(pCacheHandle->dictTableHandleDirty);
	dictEntry* node_dictTableHandleDirty;
	while ((node_dictTableHandleDirty = plg_dictNext(iter_dictTableHandleDirty)) != NULL) {

		dictEntry* tableHandleEntry = plg_dictFind(plg_ListDictDict(pCacheHandle->listTableHandle), dictGetKey(node_dictTableHandleDirty));
		if (tableHandleEntry != 0) {
			PTableInFile pTableInFile = plg_TablePTableInFile(plg_ListDictGetVal(tableHandleEntry));
			if (pTableInFile->tablePageHead == 0) {
				plg_DiskTableDel(pCacheHandle->pDiskHandle, dictGetKey(node_dictTableHandleDirty));
			} else {
				plg_DiskTableAdd(pCacheHandle->pDiskHandle, dictGetKey(node_dictTableHandleDirty), pTableInFile, sizeof(TableInFile));
			}
		}
	}
	plg_dictReleaseIterator(iter_dictTableHandleDirty);
	plg_dictEmpty(pCacheHandle->dictTableHandleDirty, NULL);

	//process pCacheHandle->delPage
	dictIterator* iter_delPage = plg_dictGetSafeIterator(pCacheHandle->delPage);
	dictEntry* node_delPage;
	while ((node_delPage = plg_dictNext(iter_delPage)) != NULL) {
		plg_ListDictDel(pCacheHandle->listPageCache, dictGetKey(node_delPage));
		plg_dictDelete(pCacheHandle->pageDirty, dictGetKey(node_delPage));

		plg_DiskFreePage(pCacheHandle->pDiskHandle, *(unsigned int*)dictGetKey(node_delPage));
	}
	plg_dictReleaseIterator(iter_delPage);
	plg_dictEmpty(pCacheHandle->delPage, NULL);

	//process pCacheHandle->pageDirty;
	plg_CacheFlushDirtyToFile(pCacheHandle);

	cache_Arrange(pCacheHandle);
	MutexUnlock(pCacheHandle->mutexHandle, pCacheHandle->objectName);
}