/* disk.c - Functions related to page structure
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
#include "pfilesys.h"
#include "pelog.h"
#include "psds.h"
#include "padlist.h"
#include "pbitarray.h"
#include "pcrc16.h"
#include "pdict.h"
#include "plocks.h"
#include "pmanage.h"
#include "pquicksort.h"
#include "prandomlevel.h"
#include "pinterface.h"
#include "pfile.h"
#include "pdisk.h"
#include "ptable.h"
#include "ptimesys.h"

//Default parameters
#define _KEYWORD_ 0x74736f72
#define _VERSION_ 1
#define _PAGEBITADDR_ 1

//Data format stored on file
#pragma pack(push,1)
/*
Because the page header occupies page 0, the actual page starts from page 1,
So the default address of pageaddr is 0, which means there is no page.
_Diskhead is the first page of the database file.
Keyword: the keyword identifying the database file.
Version: the version number of the database file is used for the conversion tool between different versions.
PageSize: page size is 64 by default. In theory, it can be 4, 16, 64 without modification.
CRC: CRC check bit of the current page.
*/
typedef struct _DiskHead
{
	unsigned int keyWord;
	unsigned int version;
	unsigned short pageSize;
	unsigned short crc;
}*PDiskHead, DiskHead;

/*
arrary
Because a file is difficult to be larger than 32G, there is usually only one bitpage for a file.
Topcur: the largest cur to quickly organize the end of the file
Length: the current amount of bitpage allocated
*/
typedef struct _DiskBitPage
{
	unsigned int topCur;
	unsigned int bitLength;
	unsigned char element[];
} *PDiskBitPage, DiskBitPage;


/*
Pageaddr: page address of zero page
Pageusingamount: number of pages in use.
Pagebitheadaddr: indicates the address of the bit array page used by the page.
Pagebittailaddr: the end page address is used to calculate the end of the file quickly
Bitpagelistsize: length of page list
Bitpagesize: array length in bit page, equivalent to constant
Tableinfile: record the first address of the global table name
*/
typedef struct _DiskHeadBody
{
	unsigned int pageAddr;
	unsigned int pageUsingAmount;
	unsigned int pageBitHeadAddr;
	unsigned int pageBitTailAddr;
	unsigned int bitPageSize;
	TableInFile tableInFile;
} *PDiskHeadBody, DiskHeadBody;

#pragma pack(pop)

/*
The protection of file data structure needs to obtain disklock lock,
Only PMT lock can read and write this structure.
You cannot allow multiple threads to open files at the same time in multithreading.
The existence of the lock ensures the uniqueness of the file.
Although the file handle has some blocking function,
But it does not apply to the split between threads
Tablecount: the number of files currently put in, used to count the allocation
Filehandle: file handle
Tablehandle: of global tables
Objname: object name
Mutexhandle: file lock, protecting file data integrity in multiple threads
Diskhead: pointer to file header information, pointing to page 0 of cache page
Diskheadbody: the data part of the file header information
Pagedisk: page cache, file header, bitpage and tablepage are all resident caches
Pagedirty: dirty page. The modified and newly created pages in each operation are written back to the file after the operation is completed
MemPool: memory pool
*/
typedef struct _DiskHandle
{
	int noSave;
	unsigned int allWeight;
	void* fileHandle;
	void* tableHandle;
	sds objName;
	void* mutexHandle;
	PDiskHead diskHead;
	PDiskHeadBody diskHeadBody;
	dict* pageDisk;
	dict* pageDirty;
} *PDiskHandle, DiskHandle;

/*
Format the new file
*/
static void* plg_DiskFileFormat(){

	elog(log_fun, "plg_DiskFileFormat");
	//calloc memory
	unsigned char* pagebuffer = calloc(1, FULLSIZE(_PAGESIZE_) * 2);

	//init PDiskHead
	PDiskHead pDiskHead = (PDiskHead)pagebuffer;
	pDiskHead->keyWord = _KEYWORD_;
	pDiskHead->version = _VERSION_;
	pDiskHead->pageSize = _PAGESIZE_;

	//init PDiskHeadBody
	PDiskHeadBody pDiskHeadBody = (PDiskHeadBody)(pagebuffer + sizeof(DiskHead));
	pDiskHeadBody->pageUsingAmount = _PAGEAMOUNT_;
	pDiskHeadBody->pageBitTailAddr = pDiskHeadBody->pageBitHeadAddr = _PAGEBITADDR_;

	//init table skip list head
	plg_TableInitTableInFile(&pDiskHeadBody->tableInFile);

	//init bitpage
	PDiskPageHead pDiskPageHead = (PDiskPageHead)(pagebuffer + FULLSIZE(_PAGESIZE_));

	//begin 0
	pDiskPageHead->addr = _PAGEBITADDR_;
	pDiskPageHead->type = BITPAGE;
	PDiskBitPage pDiskBitPage = (PDiskBitPage)(pagebuffer + FULLSIZE(_PAGESIZE_) + sizeof(DiskPageHead));
	pDiskHeadBody->bitPageSize = FULLSIZE(_PAGESIZE_) - sizeof(DiskPageHead) - sizeof(DiskBitPage) * 8;

	//begin 0
	pDiskBitPage->bitLength = _PAGEAMOUNT_;
	plg_BitArrayAdd(pDiskBitPage->element, 0);
	plg_BitArrayAdd(pDiskBitPage->element, 1);

	//Calculate CRC
	pDiskHead->crc = plg_crc16((char*)pDiskHeadBody, FULLSIZE(_PAGESIZE_) - sizeof(DiskHead));
	pDiskPageHead->crc = plg_crc16((char*)pDiskBitPage, FULLSIZE(_PAGESIZE_) - sizeof(DiskPageHead));

	return pagebuffer;
}

void plg_DiskFileCloseHandle(void* pvDiskHandle) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	//free page dict
	dictIterator* dictIter = plg_dictGetSafeIterator(pDiskHandle->pageDisk);
	dictEntry* dictNode;
	while ((dictNode = plg_dictNext(dictIter)) != NULL) {
		void* page = dictGetVal(dictNode);
		free(page);
	}
	plg_dictReleaseIterator(dictIter);
	plg_dictRelease(pDiskHandle->pageDisk);

	//free page dirty
	plg_dictRelease(pDiskHandle->pageDirty);

	//free other
	plg_sdsFree(pDiskHandle->objName);
	if (!pDiskHandle->noSave) {
		plg_FileDestoryHandle(pDiskHandle->fileHandle);
	}
	plg_MutexDestroyHandle(pDiskHandle->mutexHandle);
	plg_TableDestroyHandle(pDiskHandle->tableHandle);
	free(pDiskHandle);
}

/*
loading page from file;
*/
static unsigned int plg_DiskLoadPageFromFile(void* pvDiskHandle, unsigned int pageAddr, void* page) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	if (pDiskHandle->noSave) {
		return 0;
	}

	if (0 == plg_FileLoadPage(pDiskHandle->fileHandle, FULLSIZE(pDiskHandle->diskHead->pageSize), pageAddr, page)){
		elog(log_error, "plg_DiskLoadPageFromFile.plg_FileLoadPage");
		return 0;
	}

	//check crc
	PDiskPageHead pdiskPageHead = (PDiskPageHead)page;
	char* pdiskBitPage = (char*)page + sizeof(DiskPageHead);
	unsigned short crc = plg_crc16(pdiskBitPage, FULLSIZE(pDiskHandle->diskHead->pageSize) - sizeof(DiskPageHead));
	if (pdiskPageHead->crc == 0 || pdiskPageHead->crc != crc) {
		elog(log_error, "page crc error!");
		return 0;
	}
	return 1;

}

static unsigned int _plg_DiskFindPage(void* pvDiskHandle, unsigned int pageAddr, void** page) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	dictEntry* findPageEntry;
	findPageEntry = plg_dictFind(pDiskHandle->pageDisk, &pageAddr);

	if (findPageEntry == 0) {

		*page = malloc(FULLSIZE(pDiskHandle->diskHead->pageSize));
		if (0 == plg_DiskLoadPageFromFile(pDiskHandle, pageAddr, *page)) {
			elog(log_error, "plg_DiskFindPage.disk load page %i!", pageAddr);
			free(*page);
			return 0;
		} else {
			elog(log_details, "plg_DiskFindPage.cache_LoadPageFromFile:%i", pageAddr);
		}
		PDiskPageHead leftPage = *page;
		plg_dictAdd(pDiskHandle->pageDisk, &leftPage->addr, *page);
	} else {
		*page = dictGetVal(findPageEntry);
	}

	return 1;
}

static unsigned int plg_DiskFindPage(void* pTableHandle, unsigned int pageAddr, void** page) {

	PDiskHandle pDiskHandle = plg_TableOperateHandle(pTableHandle);
	_plg_DiskFindPage(pDiskHandle, pageAddr, page);
	return 1;
}

static unsigned int plg_DiskArrangementCheck(void* pTableHandle, void* page) {

	PDiskHandle pDiskHandle = plg_TableOperateHandle(pTableHandle);
	PDiskPageHead pDiskPageHead = (PDiskPageHead)page;
	PDiskTablePage pDiskTablePage = (PDiskTablePage)((unsigned char*)page + sizeof(DiskPageHead));

	if (pDiskPageHead->type != TABLEPAGE) {
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

	unsigned int pageSize = FULLSIZE(pDiskHandle->diskHead->pageSize) - sizeof(DiskPageHead) - sizeof(DiskTablePage);
	if (((float)pDiskTablePage->delSize / pageSize * 100) > _ARRANGMENTPERCENTAGE_) {
		plg_TableArrangementPage(pDiskHandle->diskHead->pageSize, page);
	}

	return 0;
}

/*
flush dirty page to file
*/
unsigned int plg_DiskFlushDirtyToFile(void* pvDiskHandle, FlushCallBack pFlushCallBack) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	if (pDiskHandle->noSave) {
		return 0;
	}

	if (dictSize(pDiskHandle->pageDirty) == 0) {
		elog(log_error, "plg_DiskFlushDirtyToFile.noPageDirty");
		return 0;
	}

	//flush dict page
	int dirtySize = dictSize(pDiskHandle->pageDirty);

	PFileParamPageInfo pPFileParamPageInfo = malloc(dirtySize*sizeof(FileParamPageInfo));
	unsigned count = 0;
	void** memArrary;
	plg_FileMallocPageArrary(pDiskHandle->fileHandle, &memArrary, dirtySize);

	dictIterator* dictIter = plg_dictGetSafeIterator(pDiskHandle->pageDirty);
	dictEntry* dictNode;
	while ((dictNode = plg_dictNext(dictIter)) != NULL) {
		pPFileParamPageInfo[count].pageId = *(unsigned int*)dictGetKey(dictNode);
		pPFileParamPageInfo[count].pPMaskPage = 0;
		count++;
	}
	plg_dictReleaseIterator(dictIter);

	for (int l = 0; l < dirtySize; l++) {
		dictEntry* diskNode = plg_dictFind(pDiskHandle->pageDisk, &pPFileParamPageInfo[l].pageId);
		if (diskNode != 0) {
			unsigned char* page = dictGetVal(diskNode);
			if (pPFileParamPageInfo[l].pageId == 0) {

				//Write files one way during use
				PDiskHead pdiskHead = (PDiskHead)page;
				PDiskHeadBody pdiskHeadBody = (PDiskHeadBody)(page + sizeof(DiskHead));

				//Calculate CRC
				pdiskHead->crc = plg_crc16((char*)pdiskHeadBody, FULLSIZE(pdiskHead->pageSize) - sizeof(DiskHead));
			} else {
				PDiskPageHead pDiskPageHead = (PDiskPageHead)page;
				unsigned char* pDiskPage = page + sizeof(DiskPageHead);

				//Calculate CRC
				pDiskPageHead->crc = plg_crc16((char*)pDiskPage, FULLSIZE(pDiskHandle->diskHead->pageSize) - sizeof(DiskPageHead));
			}
			memcpy(memArrary[l], page, FULLSIZE(pDiskHandle->diskHead->pageSize));
		}
	}

	//Clear before switching to file critical area for transaction integrity
	plg_dictEmpty(pDiskHandle->pageDirty, NULL);

	pFlushCallBack(pDiskHandle->fileHandle, pPFileParamPageInfo, memArrary, dirtySize);
	return 1;
}

/*
create bit page in disk
*/
static unsigned int plg_DiskCreatBitPage(void* pvDiskHandle, PDiskPageHead pPrevDiskPageHead) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	//calloc page
	unsigned char* pagebuffer = calloc(1, FULLSIZE(pDiskHandle->diskHead->pageSize));

	//new bitpage
	PDiskPageHead pDiskPageHead = (PDiskPageHead)pagebuffer;
	PDiskBitPage pDiskBitPage = (PDiskBitPage)(pagebuffer + sizeof(DiskBitPage));

	if (pPrevDiskPageHead->addr == 1) {
		pDiskPageHead->addr = pDiskHandle->diskHeadBody->bitPageSize;
	} else {
		pDiskPageHead->addr = pPrevDiskPageHead->addr + pDiskHandle->diskHeadBody->bitPageSize;
	}
	pDiskPageHead->type = BITPAGE;
	pDiskPageHead->prevPage = pPrevDiskPageHead->addr;
	pPrevDiskPageHead->nextPage = pDiskPageHead->addr;
	pDiskHandle->diskHeadBody->pageBitTailAddr = pDiskPageHead->addr;
	pDiskHandle->diskHeadBody->pageUsingAmount += 1;
	plg_BitArrayAdd(pDiskBitPage->element, 0);
	pDiskBitPage->bitLength += 1;

	//add to chache
	plg_dictAdd(pDiskHandle->pageDisk, &pDiskPageHead->addr, pagebuffer);

	//add to dirty
	dictAddWithUint(pDiskHandle->pageDirty, 0, NULL);
	dictAddWithUint(pDiskHandle->pageDirty, pDiskPageHead->addr, NULL);
	dictAddWithUint(pDiskHandle->pageDirty, pPrevDiskPageHead->addr, NULL);

	return pDiskPageHead->addr;
}

/*
file alloc
find space page from bitpage;
It also applies to the creation of other pages, and the allocation of page address is equivalent to
Allocation of storage space for files
After the space is allocated, make sure that the space will not be reallocated incorrectly.
It seems to be a question of consistency.
But as long as it is not reallocated, even if it is not uniformly written to the hard disk, it will be safe.
Assume that the write state of hard disk buffer and thread buffer is 0 and the write state is 1.
Then 00 and 11, that is, the hard disk has no data and the hard disk has data.
01 that is to say, the hard disk buffer data is not written, and the thread buffer is written, which will cause the thread buffer data to be lost.
10. If the hard disk buffer writes data, but the thread buffer is not written, the allocated page will become no home page, and the thread buffer will be lost.
No home page is a page that has been assigned but is not used. This page may read all zeros or deleted pages from the hard disk.
*/
unsigned int plg_DiskInsideAllocPage(void* pvDiskHandle, unsigned int* pageAddr) {

	//init
	PDiskHandle pDiskHandle = pvDiskHandle;
	void* pbitPage;
	unsigned int pageBitAddr = pDiskHandle->diskHeadBody->pageBitHeadAddr;

	//while all bitpage
	do {

		//find bitpage in disk
		unsigned int ret = _plg_DiskFindPage(pDiskHandle, pageBitAddr, &pbitPage);
		if (ret == 0) {
			break;
		}

		//find free space
		PDiskPageHead pDiskPageHead = (PDiskPageHead)pbitPage;
		PDiskBitPage pDiskBitPage = (PDiskBitPage)((unsigned char*)pbitPage + sizeof(DiskPageHead));


		//bit page is full
		if (pDiskBitPage->bitLength == pDiskHandle->diskHeadBody->bitPageSize) {
			continue;
		}

		//while page for uint
		unsigned int maxUint = 0 - 1;
		unsigned int bitSize = 8 * sizeof(unsigned int);
		unsigned int end = pDiskHandle->diskHeadBody->bitPageSize / bitSize;
		unsigned int* ptr = (unsigned int*)pDiskBitPage->element;
		for (unsigned int l = 0; l < end; l++) {
			if (ptr[l] != maxUint) {

				//while uint for bit
				unsigned int cur = l * bitSize;
				unsigned int curEnd = l * bitSize + bitSize;
				for (; cur < curEnd; cur++) {
					if (plg_BitArrayIsIn(pDiskBitPage->element, cur) == 0) {

						//find
						if (pDiskPageHead->addr == 1) {
							*pageAddr = cur;
						} else {
							*pageAddr = pDiskPageHead->addr + cur;
						}

						plg_BitArrayAdd(pDiskBitPage->element, cur);
						pDiskBitPage->bitLength += 1;

						//add to dirty
						dictAddWithUint(pDiskHandle->pageDirty, pDiskPageHead->addr, NULL);

						//Record amount
						pDiskHandle->diskHeadBody->pageUsingAmount += 1;
						dictAddWithUint(pDiskHandle->pageDirty, 0, NULL);

						elog(log_fun, "plg_DiskInsideAllocPage.pageAddr:%i", *pageAddr);
						return 1;
					}
					if (cur >= pDiskHandle->diskHeadBody->bitPageSize) {
						break;
					}
				}
				if (cur >= pDiskHandle->diskHeadBody->bitPageSize) {
					break;
				}
			}
		}

		//no find in current page find to next page
		if (*pageAddr == 0 && pDiskPageHead->nextPage != 0){
			pageBitAddr = pDiskPageHead->nextPage;
		} else {
			pageBitAddr = plg_DiskCreatBitPage(pDiskHandle, pDiskPageHead);
			break;
		}

	} while (1);

	elog(log_error, "plg_DiskInsideAllocPage.pageAddr");
	return 0;
}

unsigned int plg_DiskAllocPage(void* pvDiskHandle, unsigned int* pageAddr) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	MutexLock(pDiskHandle->mutexHandle, pDiskHandle->objName);
	unsigned int r = plg_DiskInsideAllocPage(pDiskHandle, pageAddr);
	MutexUnlock(pDiskHandle->mutexHandle, pDiskHandle->objName);
	return r;
}

void plg_DiskAddDirtyPage(void* pTableHandle, unsigned int pageAddr) {
	PDiskHandle pDiskHandle = plg_TableOperateHandle(pTableHandle);
	dictAddWithUint(pDiskHandle->pageDirty, pageAddr, NULL);
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
static unsigned int plg_DiskCreatePage(void* pTableHandle, void** retPage, char type) {

	PDiskHandle pDiskHandle = plg_TableOperateHandle(pTableHandle);
	unsigned int pageAddr = 0;
	plg_DiskInsideAllocPage(pDiskHandle, &pageAddr);
	if (pageAddr == 0) {
		return 0;
	}

	//calloc memory
	*retPage = calloc(1, FULLSIZE(pDiskHandle->diskHead->pageSize));

	//init
	PDiskPageHead pDiskPageHead = (PDiskPageHead)*retPage;
	pDiskPageHead->addr = pageAddr;
	pDiskPageHead->type = type;

	//add to chache
	plg_dictAdd(pDiskHandle->pageDisk, &pDiskPageHead->addr, *retPage);

	//add to dirty
	dictAddWithUint(pDiskHandle->pageDirty, pDiskPageHead->addr, NULL);

	return 1;
}

/*
Inverse function of plg_DiskCreatBitPage
using ftruncate to reset file
*/
static unsigned int plg_DiskDelBitPage(void* pvDiskHandle, unsigned int pageAddr) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	//find page
	void* page;
	if (_plg_DiskFindPage(pDiskHandle, pageAddr, &page) == 0)
		return 0;
	PDiskPageHead pDiskPageHead = (PDiskPageHead)page;

	unsigned int prevPageAddr = 0;
	if (pDiskPageHead->prevPage != 0) {

		//prevPage
		void* prevPage;
		if (_plg_DiskFindPage(pDiskHandle, pDiskPageHead->prevPage, &prevPage) == 0)
			return 0;
		PDiskPageHead pPrevDiskPageHead = (PDiskPageHead)prevPage;
		pPrevDiskPageHead->nextPage = pDiskPageHead->nextPage;
		prevPageAddr = pPrevDiskPageHead->addr;
		if (pPrevDiskPageHead->nextPage == 0) {
			pDiskHandle->diskHeadBody->pageBitTailAddr = pPrevDiskPageHead->addr;
		}

		if (pDiskPageHead->nextPage != 0) {
			void* nextPage;
			if (_plg_DiskFindPage(pDiskHandle, pDiskPageHead->nextPage, &nextPage) == 0)
				return 0;
			PDiskPageHead pDiskPageHead = (PDiskPageHead)nextPage;
			pDiskPageHead->prevPage = pPrevDiskPageHead->addr;

			dictAddWithUint(pDiskHandle->pageDirty, pDiskPageHead->nextPage, NULL);
		}
		dictAddWithUint(pDiskHandle->pageDirty, prevPageAddr, NULL);
	} else {
		pDiskHandle->diskHeadBody->pageBitHeadAddr = pDiskPageHead->nextPage;
		if (pDiskPageHead->nextPage == 0) {
			pDiskHandle->diskHeadBody->pageBitTailAddr = 0;
		}

		if (pDiskPageHead->nextPage != 0) {
			void* nextPage;
			if (_plg_DiskFindPage(pDiskHandle, pDiskPageHead->nextPage, &nextPage) == 0)
				return 0;
			PDiskPageHead pDiskPageHead = (PDiskPageHead)nextPage;
			pDiskPageHead->prevPage = 0;
			dictAddWithUint(pDiskHandle->pageDirty, pDiskPageHead->nextPage, NULL);
		}
	}

	pDiskHandle->diskHeadBody->pageUsingAmount -= 1;
	plg_dictDelete(pDiskHandle->pageDisk, &pageAddr);
	plg_dictDelete(pDiskHandle->pageDirty, &pageAddr);
	free(page);
	return 1;
}

/*
Inverse function of plg_DiskAllocPage
*/
unsigned int plg_DiskInsideFreePage(void* pvDiskHandle, unsigned int pageAddr) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	unsigned int bitPageAddr = pageAddr / pDiskHandle->diskHeadBody->bitPageSize * pDiskHandle->diskHeadBody->bitPageSize;
	unsigned int bitPageCur = pageAddr % pDiskHandle->diskHeadBody->bitPageSize;
	if (bitPageAddr == 0) {
		bitPageAddr = 1;
	}

	//find page
	void* page;
	if (_plg_DiskFindPage(pDiskHandle, bitPageAddr, &page) == 0)
		return 0;

	PDiskBitPage pDiskBitPage = (PDiskBitPage)((unsigned char*)page + sizeof(DiskPageHead));
	plg_BitArrayClear(pDiskBitPage->element, bitPageCur);
	pDiskBitPage->bitLength -= 1;

	//Delete bitpage if bitlength is empty
	if (pDiskBitPage->bitLength == 1) {
		plg_DiskDelBitPage(pDiskHandle, bitPageAddr);
	} else {
		dictAddWithUint(pDiskHandle->pageDirty, bitPageAddr, NULL);
	}

	//Record amount
	pDiskHandle->diskHeadBody->pageUsingAmount -= 1;
	dictAddWithUint(pDiskHandle->pageDirty, 0, NULL);
	return 1;
}

unsigned int plg_DiskFreePage(void* pvDiskHandle, unsigned int pageAddr) {
	PDiskHandle pDiskHandle = pvDiskHandle;
	MutexLock(pDiskHandle->mutexHandle, pDiskHandle->objName);
	unsigned int r = plg_DiskInsideFreePage(pDiskHandle, pageAddr);
	MutexUnlock(pDiskHandle->mutexHandle, pDiskHandle->objName);
	return r;
}

/*
Inverse function of plg_DiskCreatePage
*/
static unsigned int plg_DiskDelPage(void* pTableHandle, unsigned int pageAddr) {

	PDiskHandle pDiskHandle = plg_TableOperateHandle(pTableHandle);
	elog(log_fun, "plg_DiskDelPage.pageAddr:%i", pageAddr);
	//find page
	void* page;
	if (_plg_DiskFindPage(pDiskHandle, pageAddr, &page) == 0) {
		return 0;
	}

	plg_dictDelete(pDiskHandle->pageDisk, &pageAddr);
	plg_dictDelete(pDiskHandle->pageDirty, &pageAddr);
	free(page);

	plg_DiskInsideFreePage(pDiskHandle, pageAddr);
	return 1;
}

unsigned int plg_DiskInsideTableAdd(void* pvDiskHandle, void* tableName, void* value, unsigned int length) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	if (length > plg_TableBigValueSize()) {
		DiskKeyBigValue diskKeyBigValue;
		if (0 == plg_TableNewBigValue(pDiskHandle->tableHandle, value, length, &diskKeyBigValue))
			return 0;
		return plg_TableAddWithAlter(pDiskHandle->tableHandle, tableName, plg_sdsLen(tableName), VALUE_BIGVALUE, &diskKeyBigValue, sizeof(DiskKeyBigValue));
	} else {
		return plg_TableAddWithAlter(pDiskHandle->tableHandle, tableName, plg_sdsLen(tableName), VALUE_NORMAL, value, length);
	}
}

unsigned int plg_DiskTableAdd(void* pvDiskHandle, void* tableName, void* value, unsigned int length) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	unsigned int r;
	MutexLock(pDiskHandle->mutexHandle, pDiskHandle->objName);
	r = plg_DiskInsideTableAdd(pDiskHandle, tableName, value, length);
	if (r) {
		plg_DiskFlushDirtyToFile(pDiskHandle, plg_FileFlushPage);
	}
	MutexUnlock(pDiskHandle->mutexHandle, pDiskHandle->objName);
	return r;
}

unsigned int plg_DiskInsideTableDel(void* pvDiskHandle, void* tableName) {
	PDiskHandle pDiskHandle = pvDiskHandle;
	return plg_TableDel(pDiskHandle->tableHandle, tableName, plg_sdsLen(tableName));
}

unsigned int plg_DiskTableDel(void* pvDiskHandle, void* tableName) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	MutexLock(pDiskHandle->mutexHandle, pDiskHandle->objName);
	unsigned int r = plg_DiskInsideTableDel(pDiskHandle, tableName);
	if (r) {
		plg_DiskFlushDirtyToFile(pDiskHandle, plg_FileFlushPage);
	}
	MutexUnlock(pDiskHandle->mutexHandle, pDiskHandle->objName);
	return r;
}

int plg_DiskInsideTableFind(void* pvDiskHandle, void* tableName, void* pDictExten) {
	PDiskHandle pDiskHandle = pvDiskHandle;
	return plg_TableFind(pDiskHandle->tableHandle, tableName, plg_sdsLen(tableName), pDictExten, 0);
}

int plg_DiskTableFind(void* pvDiskHandle, void* tableName, void* pDictExten) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	MutexLock(pDiskHandle->mutexHandle, pDiskHandle->objName);
	int r = plg_DiskInsideTableFind(pDiskHandle, tableName, pDictExten);
	MutexUnlock(pDiskHandle->mutexHandle, pDiskHandle->objName);
	return r;
}

void plg_DiskPrintTableName(void* pvDiskHandle) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	void* iter = plg_TableGetIteratorWithKey(pDiskHandle->tableHandle, NULL, 0);

	unsigned int pageAddr = 0;
	unsigned int count = 0;
	while (plg_TableNextIterator(iter)) {
		count += 1;
		if (plg_TableIteratorAddr(iter) && pageAddr != plg_TableIteratorAddr(iter)) {
			pageAddr = plg_TableIteratorAddr(iter);

			void *page = 0;
			if (_plg_DiskFindPage(pDiskHandle, pageAddr, &page) == 0)
				return;
			printf("address:%d;count:%d\n", pageAddr, count);

			PDiskPageHead pDiskPageHead = (PDiskPageHead)page;

			printf("type:%d;prev:%d,next:%d\n", pDiskPageHead->type, pDiskPageHead->prevPage, pDiskPageHead->nextPage);

			PDiskTablePage pDiskTablePage = (PDiskTablePage)((unsigned char*)page + sizeof(DiskPageHead));

			printf("arrangmentStamp:%llu;delSize:%d,spaceAddr:%d;spaceLength:%d;\ntableLength:%d;tableSize:%d;usingLength:%d;\nusingPageAddr:%d;usingPageOffset:%d\n",
				pDiskTablePage->arrangmentStamp, 
				pDiskTablePage->delSize,
				pDiskTablePage->spaceAddr,
				pDiskTablePage->spaceLength,
				pDiskTablePage->tableLength,
				pDiskTablePage->tableSize,
				pDiskTablePage->usingLength,
				pDiskTablePage->usingPageAddr,
				pDiskTablePage->usingPageOffset);
		}
	};
	printf("\n");
	plg_TableReleaseIterator(iter);
}

void plg_DiskFillTableName(void* pvDiskHandle, void* ptr, FillTableNameCB funCB) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	void* iter = plg_TableGetIteratorWithKey(pDiskHandle->tableHandle, NULL, 0);

	PDiskTableKey keyStr;
	while ((keyStr = plg_TableNextIterator(iter)) != NULL) {

		sds key = plg_sdsNewLen(keyStr->keyStr, keyStr->keyStrSize);
		funCB(pDiskHandle, ptr, key);
	};
	plg_TableReleaseIterator(iter);
}

unsigned long long plg_DiskGetPageSize(void* pvDiskHandle) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	return pDiskHandle->diskHead->pageSize;
}

void* plg_DiskFileHandle(void* pvDiskHandle) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	return pDiskHandle->fileHandle;
}

void* plg_DiskTableHandle(void* pvDiskHandle) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	return pDiskHandle->tableHandle;
}

void plg_DiskAddTableWeight(void* pvDiskHandle, unsigned int allWeight) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	pDiskHandle->allWeight += allWeight;
}

unsigned int plg_DiskGetTableAllWeight(void* pvDiskHandle) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	return pDiskHandle->allWeight;
}

static void* plg_DiskpageCopyOnWrite(void* pTableHandle, unsigned int pageAddr, void* page) {

	PDiskHandle pDiskHandle = plg_TableOperateHandle(pTableHandle);
	NOTUSED(pDiskHandle);
	NOTUSED(pageAddr);
	return page;
}

static void* plg_DisktableCopyOnWrite(void* pTableHandle, sds table, void* tableHead) {

	PDiskHandle pDiskHandle = plg_TableOperateHandle(pTableHandle);
	NOTUSED(pDiskHandle);
	NOTUSED(table);
	return tableHead;
}

static void plg_DiskaddDirtyTable(void* pTableHandle, sds table) {
	NOTUSED(table);
	PDiskHandle pDiskHandle = plg_TableOperateHandle(pTableHandle);
	dictAddWithUint(pDiskHandle->pageDirty, 0, NULL);
}

void* plg_DiskfindTableInFile(void* pvDiskHandle, sds table, void* tableInFile) {
	NOTUSED(table);
	NOTUSED(pvDiskHandle);
	return tableInFile;
}


static TableHandleCallBack tableHandleCallBack = {
	plg_DiskFindPage,
	plg_DiskCreatePage,
	plg_DiskDelPage,
	plg_DiskArrangementCheck,
	plg_DiskpageCopyOnWrite,
	plg_DiskAddDirtyPage,
	plg_DisktableCopyOnWrite,
	plg_DiskaddDirtyTable,
	plg_DiskfindTableInFile
};

/*
DiskHandle
*/
static void plg_DiskHandleInit(void* pvDiskHandle, char* filePath, void* pManageEqueue, char noSave) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	pDiskHandle->pageDisk = plg_dictCreate(plg_DefaultNoneDictPtr(), NULL, DICT_MIDDLE);
	pDiskHandle->pageDirty = plg_dictCreate(plg_DefaultUintPtr(), NULL, DICT_MIDDLE);
	pDiskHandle->mutexHandle = plg_MutexCreateHandle(LockLevel_2);
	pDiskHandle->objName = plg_sdsNew("disk");
	pDiskHandle->allWeight = 0;
	pDiskHandle->tableHandle = plg_TableCreateHandle(&pDiskHandle->diskHeadBody->tableInFile, pDiskHandle, pDiskHandle->diskHead->pageSize, NULL, &tableHandleCallBack);
	pDiskHandle->noSave = noSave;
	if (pDiskHandle->noSave) {
		pDiskHandle->fileHandle = 0;
	} else {
		pDiskHandle->fileHandle = plg_FileCreateHandle(filePath, pManageEqueue, FULLSIZE(pDiskHandle->diskHead->pageSize));
	}
}

/*
Open file,
The file does not exist. Create a new file and format it;
If the file already exists and its length is less than the first page, it fails to exit;
The file already exists. To verify the header, if the header does not conform to the requirements, fail to exit;
Read the first page of the file, verify the CRC, and exit if the verification is wrong
The file already exists, meets the requirements, creates a diskhandle, and adds it to the list.
To write to the manage list, you need to check whether the hand already exists
If there is one, get the handle first and then open the file. If there is no one, create it,
Open the manage handle first and then the file handle to manage the file
The file handle is opened directly during use
Pay attention to the concept of data lifetime here, which is pmanage during thread running
It will not be modified or deleted.
pManage:PManage
Filepath: open filename
Pdiskhandle: handle returned
*/

SDS_TYPE
unsigned int plg_DiskFileOpen(void* pManageEqueue, char* filePath, void** pDiskHandle, char isNew, char noSave) {

	PDiskHandle pdiskHandle = 0;

	if (noSave) {

		//no file
		char* ptr = plg_DiskFileFormat();
		PDiskHead pdiskHead = (PDiskHead)ptr;
		unsigned char* diskpagebuffer = malloc(FULLSIZE(pdiskHead->pageSize));
		memcpy(diskpagebuffer, ptr, FULLSIZE(pdiskHead->pageSize));

		//create DiskHandle and join to listDiskHandle
		pdiskHandle = malloc(sizeof(DiskHandle));

		//first page
		pdiskHandle->diskHead = (PDiskHead)diskpagebuffer;
		pdiskHandle->diskHeadBody = (PDiskHeadBody)(diskpagebuffer + sizeof(DiskHead));
		
		//init handle
		plg_DiskHandleInit(pdiskHandle, filePath, pManageEqueue, noSave);
		plg_dictAdd(pdiskHandle->pageDisk, &pdiskHandle->diskHeadBody->pageAddr, diskpagebuffer);

		//bit page
		unsigned char* bitpagebuffer = malloc(FULLSIZE(pdiskHandle->diskHead->pageSize));
		memcpy(bitpagebuffer, ptr + FULLSIZE(pdiskHead->pageSize), FULLSIZE(pdiskHead->pageSize));
		PDiskPageHead pdiskPageHead = (PDiskPageHead)bitpagebuffer;
		plg_dictAdd(pdiskHandle->pageDisk, &pdiskPageHead->addr, bitpagebuffer);

		plg_sdsFree(filePath);
		free(ptr);
		*pDiskHandle = pdiskHandle;
		return 1;
	}
	
	//if no find create
	FILE *inputFile = 0;
	
	if (isNew) {
		inputFile = fopen_t(filePath, "wb+");
		if (!inputFile) {
			elog(log_error, "plg_DiskFileOpen.fopen_t.wb+:%s!", filePath);
			return 0;
		}
	} else {
		inputFile = fopen_t(filePath, "rb+");
		if (!inputFile) {
			elog(log_error, "plg_DiskFileOpen.fopen_t.rb+:%s!", filePath);
			return 0;
		}
	}

	//check file length
	fseek_t(inputFile, 0, SEEK_END);
	unsigned int inputFileLength = ftell_t(inputFile);

	//error file size
	if (inputFileLength > 0 && inputFileLength < sizeof(DiskHead)) {
		elog(log_error, "plg_DiskFileOpen.inputFileLength:%i!", inputFileLength);
		return 0;
	}


	//base infomation write to file
	if (inputFileLength == 0) {
		fseek_t(inputFile, 0, SEEK_SET);
		void* ptr = plg_DiskFileFormat();
		fwrite(ptr, 1, FULLSIZE(_PAGESIZE_) * 2, inputFile);
		free(ptr);
	}

	//read file head
	fseek_t(inputFile, 0, SEEK_SET);
	PDiskHead pdiskHead = malloc(sizeof(DiskHead));
	unsigned long long retRead = fread(pdiskHead, 1, sizeof(DiskHead), inputFile);
	if (retRead != sizeof(DiskHead)) {
		elog(log_error, "plg_DiskFileOpen.fread.pdiskHead!");
		return 0;
	}

	//check keyword and version
	if (pdiskHead->keyWord != _KEYWORD_) {
		elog(log_error, "plg_DiskFileOpen.keyWord!");
		return 0;
	}
	if (pdiskHead->version != _VERSION_) {
		elog(log_error, "plg_DiskFileOpen.version!");
		return 0;
	}

	//create DiskHandle and join to listDiskHandle
	pdiskHandle = malloc(sizeof(DiskHandle));

	//read disk head page
	unsigned char* diskpagebuffer = malloc(FULLSIZE(pdiskHead->pageSize));
	fseek_t(inputFile, 0, SEEK_SET);
	retRead = fread(diskpagebuffer, 1, FULLSIZE(pdiskHead->pageSize), inputFile);
	if (retRead != FULLSIZE(pdiskHead->pageSize)) {
		elog(log_error, "plg_DiskFileOpen.fread.diskpagebuffer!");
		return 0;
	}

	free(pdiskHead);

	pdiskHandle->diskHead = (PDiskHead)diskpagebuffer;
	pdiskHandle->diskHeadBody = (PDiskHeadBody)(diskpagebuffer + sizeof(DiskHead));

	//check crc
	unsigned short crc = plg_crc16((char*)pdiskHandle->diskHeadBody, FULLSIZE(pdiskHandle->diskHead->pageSize) - sizeof(DiskHead));
	if (pdiskHandle->diskHead->crc == 0 || pdiskHandle->diskHead->crc != crc) {
		elog(log_error, "disk head crc error!");
		return 0;
	}

	
	//init handle
	plg_DiskHandleInit(pdiskHandle, filePath, pManageEqueue, noSave);
	plg_dictAdd(pdiskHandle->pageDisk, &pdiskHandle->diskHeadBody->pageAddr, diskpagebuffer);

	SDS_CHECK(diskpagebuffer, crc);
	int ret = 1;
	//read disk bit page
	unsigned nextpage = pdiskHandle->diskHeadBody->pageBitHeadAddr;
	while (1) {
		if (nextpage == 0) {
			break;
		}

		//read from file
		unsigned char* bitpagebuffer = malloc(FULLSIZE(pdiskHandle->diskHead->pageSize));
		fseek_t(inputFile, nextpage * FULLSIZE(pdiskHandle->diskHead->pageSize), SEEK_SET);
		retRead = fread(bitpagebuffer, 1, FULLSIZE(pdiskHandle->diskHead->pageSize), inputFile);
		if (retRead != FULLSIZE(pdiskHandle->diskHead->pageSize)) {
			elog(log_error, "plg_DiskFileOpen.fread.bitpagebuffer!");
			return 0;
		}

		//check type
		PDiskPageHead pdiskPageHead = (PDiskPageHead)bitpagebuffer;
		if (pdiskPageHead->type != BITPAGE) {
			elog(log_error, "bit page type error!");
			return 0;
		}

		//check crc
		char* pdiskBitPage = (char*)bitpagebuffer + sizeof(DiskPageHead);
		unsigned short crc = plg_crc16((char*)pdiskBitPage, FULLSIZE(pdiskHandle->diskHead->pageSize) - sizeof(DiskPageHead));
		if (pdiskPageHead->crc == 0 || pdiskPageHead->crc != crc) {
			elog(log_error, "bit page crc error!");
			return 0;
		}

		plg_dictAdd(pdiskHandle->pageDisk, &pdiskPageHead->addr, bitpagebuffer);
		nextpage = pdiskPageHead->nextPage;
	}

	//table page
	nextpage = pdiskHandle->diskHeadBody->tableInFile.tablePageHead;
	do {
		if (nextpage == 0) {
			break;
		}

		//read from file
		char* pagebuffer = malloc(FULLSIZE(pdiskHandle->diskHead->pageSize));
		fseek_t(inputFile, nextpage * FULLSIZE(pdiskHandle->diskHead->pageSize), SEEK_SET);
		retRead = fread(pagebuffer, 1, FULLSIZE(pdiskHandle->diskHead->pageSize), inputFile);
		if (retRead != FULLSIZE(pdiskHandle->diskHead->pageSize)) {
			elog(log_error, "plg_DiskFileOpen.fread.pagebuffer!");
			return 0;
		}

		//check type
		PDiskPageHead pdiskPageHead = (PDiskPageHead)pagebuffer;
		if (pdiskPageHead->type != TABLEPAGE) {
			elog(log_error, "bit page type error!");
			return 0;
		}

		//check crc
		char* pdiskPage = pagebuffer + sizeof(DiskPageHead);
		unsigned short crc = plg_crc16(pdiskPage, FULLSIZE(pdiskHandle->diskHead->pageSize) - sizeof(DiskPageHead));
		if (pdiskPageHead->crc == 0 || pdiskPageHead->crc != crc) {
			elog(log_error, "bit page crc error!");
			return 0;
		}

		plg_dictAdd(pdiskHandle->pageDisk, &pdiskPageHead->addr, pagebuffer);
		nextpage = pdiskPageHead->nextPage;
	} while (1);

	//table using page
	nextpage = pdiskHandle->diskHeadBody->tableInFile.tableUsingPage;
	do {
		if (nextpage == 0) {
			break;
		}

		//read from file
		unsigned char* pagebuffer = malloc(FULLSIZE(pdiskHandle->diskHead->pageSize));
		fseek_t(inputFile, nextpage * FULLSIZE(pdiskHandle->diskHead->pageSize), SEEK_SET);
		retRead = fread(pagebuffer, 1, FULLSIZE(pdiskHandle->diskHead->pageSize), inputFile);
		if (retRead != FULLSIZE(pdiskHandle->diskHead->pageSize)) {
			elog(log_error, "plg_DiskFileOpen.fread.pagebuffer!");
			return 0;
		}

		//check type
		PDiskPageHead pdiskPageHead = (PDiskPageHead)pagebuffer;
		if (pdiskPageHead->type != TABLEUSING) {
			elog(log_error, "bit page type error!");
			return 0;
		}

		//check crc
		char* pdiskPage = (char*)pagebuffer + sizeof(DiskPageHead);
		unsigned short crc = plg_crc16(pdiskPage, FULLSIZE(pdiskHandle->diskHead->pageSize) - sizeof(DiskPageHead));
		if (pdiskPageHead->crc == 0 || pdiskPageHead->crc != crc) {
			elog(log_error, "bit page crc error!");
			return 0;
		}

		plg_dictAdd(pdiskHandle->pageDisk, &pdiskPageHead->addr, pagebuffer);
		nextpage = pdiskPageHead->nextPage;
	} while (1);

	fclose(inputFile);

	*pDiskHandle = pdiskHandle;
	return ret;
}

char plg_DiskIsNoSave(void* pvDiskHandle) {

	PDiskHandle pDiskHandle = pvDiskHandle;
	return pDiskHandle->noSave;
}