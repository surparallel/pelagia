/* manage.c - Global manager
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
#include <pthread.h>
#include "pequeue.h"
#include "psds.h"
#include "pdict.h"
#include "padlist.h"
#include "pdictset.h"
#include "pelog.h"
#include "pjob.h"
#include "pfile.h"
#include "pdisk.h"
#include "pinterface.h"
#include "pmanage.h"
#include "plocks.h"
#include "pfilesys.h"
#include "ptimesys.h"
#include "pelagia.h"
#include "pjson.h"
#include "pjob.h"
#include "pbase64.h"
#include "pstart.h"

#define NORET
#define CheckUsingThread(r) if (plg_MngCheckUsingThread()) {elog(log_error, "Cannot run management interface in non user environment");return r;}

/*
The structure of global filer is protected by readlock lock,
Only the thread obtaining the lock can read and write data in this structure
Global management is equivalent to a router
All objects with locks are registered here
Direct transfer of pointers between threads is prohibited
Because the lock itself is a special memory
Once assigned, it cannot be destroyed
It can only be destroyed when the system exits
The locks to be recorded include
1. Cache data of each thread
2. Data lock of each file
3. Handle of each message queue
The manned process of tablename
1. Read the JSON to get the corresponding relationship between the job and tablename and generate the tablename "job by classification
2. Classify jobs according to table name
----------------------------------
Readlock: multithreaded read lock
Pjobhandle: one way unequal, message processing sent to the management queue does not pass the order queue, and management needs a management thread to process management instructions,
Status: run 1, no run 0
Dictdisk: handle to all disks, one disk for each file
Listjob: handle to all jobs
Listorder: internal event
Listuserevent: external event
Order_Process: main table, all events correspond to the handle of processing method plg_Mngaddorder creation
Order_queue: all message correspondence tables, externally sent to internal functions for use
Event ABCD dicttablename: main table, all events and the list of their corresponding tablenames PLG ABCD mngaddtable creation
Tablename "diskhandle: handle of hard disk corresponding to all tablenames
*/
typedef struct _Manage
{
	void* mutexHandle;
	void* pJobHandle;
	int runStatus;

	//Data to be destroyed by management
	list* listDisk;
	list* listJob;
	list* listOrder;
	list* listProcess;
	dict* dictTableName;

	dict* order_process;
	dict* order_equeue;
	PDictSet order_tableName;
	dict* tableName_diskHandle;
	sds	dbPath;
	sds objName;
	
	unsigned short fileCount;
	unsigned int jobDestroyCount;
	unsigned int fileDestroyCount;
	unsigned int maxTableWeight;

	//lvm
	sds luaDllPath;
	sds luaPath;
	sds dllPath;

	//event for exit;
	void* pEvent;
} *PManage, Manage;

static void listSdsFree(void *ptr) {
	plg_sdsFree(ptr);
}

static void listFileFree(void *ptr) {
	plg_DiskFileCloseHandle(ptr);
}

static void listJobFree(void *ptr) {
	plg_JobDestoryHandle(ptr);
}

static void listProcessFree(void *ptr) {
	plg_JobProcessDestory(ptr);
}

static int sdsCompareCallback(void *privdata, const void *key1, const void *key2) {
	int l1, l2;
	DICT_NOTUSED(privdata);

	l1 = plg_sdsLen((sds)key1);
	l2 = plg_sdsLen((sds)key2);
	if (l1 != l2) return 0;
	return memcmp(key1, key2, l1) == 0;
}

static void sdsFreeCallback(void *privdata, void *val) {
	DICT_NOTUSED(privdata);
	plg_sdsFree(val);
}

static void dictTableNameFree(void *privdata, void *val) {
	NOTUSED(privdata);
	PTableName pTableName = val;
	plg_sdsFree(pTableName->sdsParent);
	free(val);
}

static unsigned long long sdsHashCallback(const void *key) {
	return plg_dictGenHashFunction((unsigned char*)key, plg_sdsLen((char*)key));
}

static dictType SdsDictType = {
	sdsHashCallback,
	NULL,
	NULL,
	sdsCompareCallback,
	sdsFreeCallback,
	dictTableNameFree
};


typedef struct _ManageDestroy
{
	PManage pManage;
	AfterDestroyFun fun;
	void* ptr;
	char type;
}*PManageDestroy, ManageDestroy;

char* plg_MngGetDBPath(void* pvManage) {
	PManage pManage = pvManage;
	return pManage->dbPath;
}

/*
load file from p1,p2,p3,p4... to init listDisk
*/
static void manage_InitLoadFile(void* pvManage) {

	PManage pManage = pvManage;
	pManage->fileCount = 0;
	unsigned short loop = 0;
	do {
		sds fullPath = plg_sdsCatFmt(plg_sdsEmpty(), "%sp%i", pManage->dbPath, loop++);
		void* pDiskHandle;

		if (access_t(fullPath, 0) != 0){
			plg_sdsFree(fullPath);
			break;
		}

		if (1 == plg_DiskFileOpen(plg_JobEqueueHandle(pManage->pJobHandle), fullPath, &pDiskHandle, 0, 0)) {
			plg_listAddNodeHead(pManage->listDisk, pDiskHandle);
		} else {
			plg_sdsFree(fullPath);
			break;
		}	
	} while (1);
}

static void manage_DestroyDisk(void* pvManage) {

	PManage pManage = pvManage;
	plg_dictEmpty(pManage->tableName_diskHandle, NULL);
	plg_listEmpty(pManage->listDisk);
}

static void manage_AddTableToDisk(void* pvManage, PTableName pTableName, sds tableName) {

	PManage pManage = pvManage;
	unsigned int count = UINT_MAX;
	void* countLost = 0;

	unsigned int noSaveCount = UINT_MAX;
	void* noSaveCountLost = 0;

	listIter* iter = plg_listGetIterator(pManage->listDisk, AL_START_HEAD);
	listNode* node;
	while ((node = plg_listNext(iter)) != NULL) {

		//Already exists in the file
		if (plg_DiskTableFind(listNodeValue(node), tableName, NULL)) {
			plg_dictAdd(pManage->tableName_diskHandle, tableName, listNodeValue(node));
			plg_listReleaseIterator(iter);
			return;
		}

		//have parent
		if (pTableName->sdsParent && plg_DiskTableFind(listNodeValue(node), pTableName->sdsParent, 0)) {
			plg_DiskAddTableWeight(listNodeValue(node), 1);
			plg_dictAdd(pManage->tableName_diskHandle, tableName, listNodeValue(node));
			plg_listReleaseIterator(iter);
			return;
		}

		//no save file
		if (plg_DiskIsNoSave(listNodeValue(node)) && plg_DiskGetTableAllWeight(listNodeValue(node)) < noSaveCount) {
			noSaveCountLost = listNodeValue(node);
			noSaveCount = pTableName->weight;
		}

		//Count the number of files that have been added find the least added files
		if (plg_DiskGetTableAllWeight(listNodeValue(node)) < count) {
			countLost = listNodeValue(node);
			count = pTableName->weight;
		}
	}
	plg_listReleaseIterator(iter);

	if (pTableName->noSave) {
		if (noSaveCount > pManage->maxTableWeight) {
			sds fullPath = plg_sdsCatFmt(plg_sdsEmpty(), "%spnosave", pManage->dbPath);
			void* pDiskHandle;
			if (1 == plg_DiskFileOpen(plg_JobEqueueHandle(pManage->pJobHandle), fullPath, &pDiskHandle, 1, 1)) {
				plg_listAddNodeHead(pManage->listDisk, pDiskHandle);
				plg_DiskAddTableWeight(pDiskHandle, pTableName->weight);
				plg_dictAdd(pManage->tableName_diskHandle, tableName, pDiskHandle);
			}
		} else {
			plg_DiskAddTableWeight(noSaveCountLost, pTableName->weight);
			plg_dictAdd(pManage->tableName_diskHandle, tableName, noSaveCountLost);
		}
	} else {
		if (count > pManage->maxTableWeight) {
			sds fullPath = plg_sdsCatFmt(plg_sdsEmpty(), "%sp%i", pManage->dbPath, listLength(pManage->listDisk));
			void* pDiskHandle;
			if (1 == plg_DiskFileOpen(plg_JobEqueueHandle(pManage->pJobHandle), fullPath, &pDiskHandle, 1, 0)) {
				plg_listAddNodeHead(pManage->listDisk, pDiskHandle);
				plg_DiskAddTableWeight(pDiskHandle, pTableName->weight);
				plg_dictAdd(pManage->tableName_diskHandle, tableName, pDiskHandle);
			}
		} else {
			plg_DiskAddTableWeight(countLost, pTableName->weight);
			plg_dictAdd(pManage->tableName_diskHandle, tableName, countLost);
		}
	}

}

static void pFillTableNameCB(void* pDiskHandle, void* ptr, char* tableName) {

	PManage pManage = (PManage)ptr;
	plg_dictAdd(pManage->tableName_diskHandle, tableName, pDiskHandle);

	plg_MngAddTable(pManage, "o1", 2, tableName, plg_sdsLen(tableName));
}

static int NullRouting(char* value, short valueLen) {
	NOTUSED(value);
	NOTUSED(valueLen);
	return 1;
}

static void manage_CreateDiskWithFileName(void* pvManage,char* FileName) {

	PManage pManage = pvManage;
	pManage->fileCount = 0;

	sds fullPath = plg_sdsCatFmt(plg_sdsEmpty(), "%s%s", pManage->dbPath, FileName);
	void* pDiskHandle;

	if (access_t(fullPath, 0) != 0){
		elog(log_error, "manage_CreateDiskWithFileName.plg_SysFileExits:%s no exit!", fullPath);
		plg_sdsFree(fullPath);
		return;
	}

	if (1 == plg_DiskFileOpen(plg_JobEqueueHandle(pManage->pJobHandle), fullPath, &pDiskHandle, 0, 0)) {
		plg_listAddNodeHead(pManage->listDisk, pDiskHandle);
	} else {
		elog(log_error, "manage_CreateDiskWithFileName.plg_DiskFileOpen:%s", fullPath);
		plg_sdsFree(fullPath);
		return;
	}

	plg_MngAddOrder(pManage, "o1", 2, plg_JobCreateFunPtr(NullRouting));
	plg_DiskFillTableName(pDiskHandle, pManage, pFillTableNameCB);
}

static void manage_CreateDisk(void* pvManage) {

	PManage pManage = pvManage;
	manage_InitLoadFile(pManage);
	//dictTableName
	dictIterator* tableNameIter = plg_dictGetIterator(pManage->dictTableName);
	dictEntry* tableNameNode;
	while ((tableNameNode = plg_dictNext(tableNameIter)) != NULL) {

		PTableName pTableName = dictGetVal(tableNameNode);
		manage_AddTableToDisk(pManage, pTableName, dictGetKey(tableNameNode));
	}

	plg_dictReleaseIterator(tableNameIter);
}

int plg_MngFreeJob(void* pvManage) {

	PManage pManage = pvManage;
	if (pManage->runStatus) {
		elog(log_error, "Releasing resources is not allowed while the system is running");
		return 0;
	}

	CheckUsingThread(0);

	//listjob
	plg_listEmpty(pManage->listJob);
	plg_dictEmpty(pManage->order_equeue, NULL);
	plg_DictSetEmpty(pManage->order_tableName);

	return 1;
}

static void manage_AddEqueueToJob(void* pvManage, sds order, void* equeue) {

	//Manage external function usage
	PManage pManage = pvManage;
	plg_dictAdd(pManage->order_equeue, order, equeue);

	//listjob
	listIter* jobIter = plg_listGetIterator(pManage->listJob, AL_START_HEAD);
	listNode* jobNode;
	while ((jobNode = plg_listNext(jobIter)) != NULL) {
		plg_JobAddEventEqueue(listNodeValue(jobNode), order, equeue);
	}
	plg_listReleaseIterator(jobIter);
}

/*
添加表到job
*/
static void manage_AddTableToJob(void* pvManage, void* pJobHandle, dict * table) {

	PManage pManage = pvManage;
	dictIterator* tableIter = plg_dictGetSafeIterator(table);
	dictEntry* tableNode;
	while ((tableNode = plg_dictNext(tableIter)) != NULL) {
		dictEntry* diskEntry = plg_dictFind(pManage->tableName_diskHandle, dictGetKey(tableNode));
		if (diskEntry == 0) {
			continue;
		}

		void* pCacheHandle = plg_JobNewTableCache(pJobHandle, dictGetKey(diskEntry), dictGetVal(diskEntry));
		dictEntry * tableEntry = plg_dictFind(pManage->dictTableName, dictGetKey(diskEntry));
		if (tableEntry == 0) {
			continue;
		}

		PTableName pTableName = dictGetVal(tableEntry);
		//only add to current job
		if (pTableName->noShare) {
			plg_JobAddTableCache(pJobHandle, dictGetKey(tableNode), pCacheHandle);
		} else {
			//listjob
			listIter* jobIter = plg_listGetIterator(pManage->listJob, AL_START_HEAD);
			listNode* jobNode;
			while ((jobNode = plg_listNext(jobIter)) != NULL) {
				plg_JobAddTableCache(listNodeValue(jobNode), dictGetKey(tableNode), pCacheHandle);
			}
			plg_listReleaseIterator(jobIter);
		}
	}
	plg_dictReleaseIterator(tableIter);
}

/*
loop event_dictTableName
core: number core
*/
int plg_MngInterAllocJob(void* pvManage, unsigned int core, char* fileName) {

	PManage pManage = pvManage;
	if (pManage->runStatus) {
		elog(log_error, "Reallocation of resources is not allowed while the system is running");
		return 0;
	}

	//plg_MngFreeJob(pManage);
	manage_DestroyDisk(pManage);

	if (fileName) {
		manage_CreateDiskWithFileName(pManage, fileName);
	} else {
		manage_CreateDisk(pManage);
	}
	

	CheckUsingThread(0);
	//Create n jobs
	for (unsigned int l = 0; l < core; l++) {
		plg_listAddNodeHead(pManage->listJob, plg_JobCreateHandle(plg_JobEqueueHandle(pManage->pJobHandle), TT_PROCESS, pManage->luaPath, pManage->luaDllPath, pManage->dllPath));
	}

	//listOrder
	listIter* eventIter = plg_listGetIterator(pManage->listOrder, AL_START_HEAD);
	listNode* eventNode;
	while ((eventNode = plg_listNext(eventIter)) != NULL) {

		//process
		dictEntry * EventProcessEntry = plg_dictFind(pManage->order_process, listNodeValue(eventNode));
		if (EventProcessEntry == 0) {
			continue;
		}

		//List job intersection classification
		char nextContinue = 0;
		listIter* jobIter = plg_listGetIterator(pManage->listJob, AL_START_HEAD);
		listNode* jobNode;
		while ((jobNode = plg_listNext(jobIter)) != NULL) {

			//Judge whether there is intersection
			dict * table = plg_DictSetValue(pManage->order_tableName, listNodeValue(eventNode));
			if (!table) {
				continue;
			}
			dictIterator* tableIter = plg_dictGetSafeIterator(table);
			dictEntry* tableNode;
			while ((tableNode = plg_dictNext(tableIter)) != NULL) {

				if (plg_JobFindTableName(listNodeValue(jobNode), dictGetKey(tableNode))) {

					//table
					manage_AddTableToJob(pManage, listNodeValue(jobNode), table);

					//process
					plg_JobAddEventProcess(listNodeValue(jobNode), dictGetKey(EventProcessEntry), dictGetVal(EventProcessEntry));

					//equeue
					manage_AddEqueueToJob(pManage, listNodeValue(eventNode), plg_JobEqueueHandle(listNodeValue(jobNode)));
					nextContinue = 1;
					break;
				}
			}
			plg_dictReleaseIterator(tableIter);
			if (nextContinue) {
				break;
			}
		}
		plg_listReleaseIterator(jobIter);
		if (nextContinue) {
			continue;
		}

		//other
		do {
			void* minJob = 0;
			unsigned int Weight = UINT_MAX;
			listIter* jobIter = plg_listGetIterator(pManage->listJob, AL_START_HEAD);
			listNode* jobNode;
			while ((jobNode = plg_listNext(jobIter)) != NULL) {
				
				//min
				if (plg_JobAllWeight(listNodeValue(jobNode)) < Weight) {
					Weight = plg_JobAllWeight(listNodeValue(jobNode));
					minJob = listNodeValue(jobNode);
				}
			}
			plg_listReleaseIterator(jobIter);

			//table
			dict * table = plg_DictSetValue(pManage->order_tableName, listNodeValue(eventNode));
			if (table) {
				manage_AddTableToJob(pManage, minJob, table);
			}

			//process
			plg_JobAddEventProcess(minJob, dictGetKey(EventProcessEntry), dictGetVal(EventProcessEntry));

			//equeue
			manage_AddEqueueToJob(pManage, listNodeValue(eventNode), plg_JobEqueueHandle(minJob));
		} while (0);
	}
	plg_listReleaseIterator(eventIter);
	return 1;
}

int plg_MngAllocJob(void* pvManage, unsigned int core) {
	PManage pManage = pvManage;
	return plg_MngInterAllocJob(pManage, core, 0);
}

/*
添加事件然后manage_CreateJob分配到多个线程
single:1独占线程
*/
int plg_MngAddOrder(void* pvManage, char* nameOrder, short nameOrderLen, void* ptrProcess) {

	CheckUsingThread(0);
	PManage pManage = pvManage;
	sds sdsnameOrder = plg_sdsNewLen(nameOrder, nameOrderLen);
	dictEntry * entry = plg_dictFind(pManage->order_process, sdsnameOrder);
	if (entry == 0) {
		plg_listAddNodeHead(pManage->listOrder, sdsnameOrder);
		plg_listAddNodeHead(pManage->listProcess, ptrProcess);
		plg_dictAdd(pManage->order_process, sdsnameOrder, ptrProcess);
		return 1;
	} else {
		plg_sdsFree(sdsnameOrder);
		free(ptrProcess);
		return 0;
	}
}

/*
添加并分配table到文件
parent:所属key,子key与母key必须在同一个文件，优先于single
single:1独占文件
*/
int plg_MngAddTable(void* pvManage, char* nameOrder, short nameOrderLen, char* nameTable, short nameTableLen) {

	CheckUsingThread(0);
	PManage pManage = pvManage;
	sds sdsnameOrder = plg_sdsNewLen(nameOrder, nameOrderLen);
	dictEntry * entry = plg_dictFind(pManage->order_process, sdsnameOrder);
	if (entry != 0) {
		plg_sdsFree(sdsnameOrder);
		sdsnameOrder = dictGetKey(entry);
	} else {
		plg_sdsFree(sdsnameOrder);
		elog(log_error, "not find order!");
		return 0;
	}

	sds sdsTableName = plg_sdsNewLen(nameTable, nameTableLen);
	dictEntry * tableEntry = plg_dictFind(pManage->dictTableName, sdsTableName);
	if (tableEntry == 0) {
		PTableName pTableName = malloc(sizeof(TableName));
		pTableName->sdsParent = 0;
		pTableName->weight = 1;
		pTableName->noShare = 0;
		pTableName->noSave = 0;
		plg_dictAdd(pManage->dictTableName, sdsTableName, pTableName);

		if (!plg_DictSetIn(pManage->order_tableName, sdsnameOrder, sdsTableName)) {
			//add to list wait for manage_CreateJob
			plg_DictSetAdd(pManage->order_tableName, sdsnameOrder, sdsTableName);
		}
	} else {
		if (!plg_DictSetIn(pManage->order_tableName, sdsnameOrder, dictGetKey(tableEntry))) {
			//add to list wait for manage_CreateJob
			plg_DictSetAdd(pManage->order_tableName, sdsnameOrder, dictGetKey(tableEntry));
		}
		plg_sdsFree(sdsTableName);
	}
	return 1;
}

int plg_MngSetTableParent(void* pvManage, char* nameTable, short nameTableLen, char* parent, short parentLen) {

	PManage pManage = pvManage;
	int ret = 0;
	sds sdsNameTable = plg_sdsNewLen(nameTable, nameTableLen);
	dictEntry* tableNameNode = plg_dictFind(pManage->dictTableName, sdsNameTable);
	if (tableNameNode != NULL) {
		PTableName pTableName = dictGetVal(tableNameNode);
		if (pTableName != 0) {
			plg_sdsFree(pTableName->sdsParent);
		}
		pTableName->sdsParent = plg_sdsNewLen(parent, parentLen);
		ret = 1;
	}
	plg_sdsFree(sdsNameTable);
	return ret;
}

int plg_MngSetWeight(void* pvManage, char* nameTable, short nameTableLen, unsigned int weight) {

	PManage pManage = pvManage;
	int ret = 0;
	sds sdsNameTable = plg_sdsNewLen(nameTable, nameTableLen);
	dictEntry* tableNameNode = plg_dictFind(pManage->dictTableName, sdsNameTable);
	if (tableNameNode != NULL) {
		PTableName pTableName = dictGetVal(tableNameNode);
		pTableName->weight = weight;
		ret = 1;
	}
	plg_sdsFree(sdsNameTable);
	return ret;
}

int plg_MngSetNoSave(void* pvManage, char* nameTable, short nameTableLen, unsigned char noSave) {

	PManage pManage = pvManage;
	int ret = 0;
	sds sdsNameTable = plg_sdsNewLen(nameTable, nameTableLen);
	dictEntry* tableNameNode = plg_dictFind(pManage->dictTableName, sdsNameTable);
	if (tableNameNode != NULL) {
		PTableName pTableName = dictGetVal(tableNameNode);
		pTableName->noSave = noSave;
		ret = 1;
	}
	plg_sdsFree(sdsNameTable);
	return ret;
}

int plg_MngSetNoShare(void* pvManage, char* nameTable, short nameTableLen, unsigned char noShare) {

	PManage pManage = pvManage;
	int ret = 0;
	sds sdsNameTable = plg_sdsNewLen(nameTable, nameTableLen);
	dictEntry* tableNameNode = plg_dictFind(pManage->dictTableName, sdsNameTable);
	if (tableNameNode != NULL) {
		PTableName pTableName = dictGetVal(tableNameNode);
		pTableName->noShare = noShare;
		ret = 1;
	}
	plg_sdsFree(sdsNameTable);
	return ret;
}

/*
因为有检查模式所以create和star中间分开
用户可以根据结果调整核心的数量如果不满意可以
destroyjob后重新createjob
*/
int plg_MngStarJob(void* pvManage) {

	CheckUsingThread(0);
	PManage pManage = pvManage;
	if (pManage->runStatus == 1) {
		return 0;
	}

	//file job before workjob start
	listIter* diskIter = plg_listGetIterator(pManage->listDisk, AL_START_HEAD);
	listNode* diskNode;
	while ((diskNode = plg_listNext(diskIter)) != NULL) {
		void* fileHandle = plg_DiskFileHandle(listNodeValue(diskNode));
		if (fileHandle) {
			if (plg_jobStartRouting(plg_FileJobHandle(fileHandle)) != 0)
				elog(log_error, "can't create thread");
		}
	}
	plg_listReleaseIterator(diskIter);

	//listJob
	listIter* jobIter = plg_listGetIterator(pManage->listJob, AL_START_HEAD);
	listNode* jobNode;
	while ((jobNode = plg_listNext(jobIter)) != NULL) {
		if (plg_jobStartRouting(listNodeValue(jobNode)) != 0)
			elog(log_error, "can't create thread");
	}
	plg_listReleaseIterator(jobIter);

	//manage
	if (plg_jobStartRouting(pManage->pJobHandle) != 0)
		elog(log_error, "can't create thread");

	pManage->runStatus = 1;
	pManage->jobDestroyCount = 0;
	pManage->fileDestroyCount = 0;

	return 1;
}

/*
通过消息系统实现退出线程,不能强制退出
*/
static void manage_DestroyJob(void* pvManage, AfterDestroyFun fun, void* ptr) {

	elog(log_fun, "manage_DestroyJob");
	CheckUsingThread(NORET);
	//listjob
	PManage pManage = pvManage;
	listIter* jobIter = plg_listGetIterator(pManage->listJob, AL_START_HEAD);
	listNode* jobNode;
	while ((jobNode = plg_listNext(jobIter)) != NULL) {

		ManageDestroy pManageDestroy;
		pManageDestroy.fun = fun;
		pManageDestroy.ptr = ptr;
		pManageDestroy.type = 1;
		pManageDestroy.pManage = pManage;
		plg_JobSendOrder(plg_JobEqueueHandle(listNodeValue(jobNode)), "destroy", (char*)&pManageDestroy, sizeof(ManageDestroy));
	}
	plg_listReleaseIterator(jobIter);
}

/*
通过消息系统实现退出线程,不能强制退出
*/
void plg_MngStopJob(void* pvManage) {

	CheckUsingThread(NORET);
	//listjob
	PManage pManage = pvManage;
	listIter* jobIter = plg_listGetIterator(pManage->listJob, AL_START_HEAD);
	listNode* jobNode;
	while ((jobNode = plg_listNext(jobIter)) != NULL) {
		plg_JobSendOrder(plg_JobEqueueHandle(listNodeValue(jobNode)), "destroyjob", 0, 0);
	}
	plg_listReleaseIterator(jobIter);
	pManage->runStatus = 0;
}


/*
不对等通讯指用户使用不同的方式发送和接收数据.
不对等通讯user使用发送数据,
user使用event接收数据
*/
int plg_MngRemoteCall(void* pvManage, char* order, short orderLen, char* value, short valueLen) {

	int r = 0;
	CheckUsingThread(0);
	PManage pManage = pvManage;
	POrderPacket POrderPacket = malloc(sizeof(OrderPacket));
	POrderPacket->order = plg_sdsNewLen(order, orderLen);
	POrderPacket->value = plg_sdsNewLen(value, valueLen);

	MutexLock(pManage->mutexHandle, pManage->objName);
	dictEntry* entry = plg_dictFind(pManage->order_equeue, POrderPacket->order);
	if (entry) {
		plg_eqPush(dictGetVal(entry), POrderPacket);
		r = 1;
	} else {
		elog(log_error, "plg_MngRemoteCall.Order:%s not found", order);
	}
	MutexUnlock(pManage->mutexHandle, pManage->objName);

	return r;
}

int plg_MngRemoteCallWithArg(void* pvManage, char* order, short orderLen, void* eventHandle, int argc, const char** argv) {

	int ret = 0;
	pJSON* root = pJson_CreateObject();
	
	pJson_AddNumberToObject(root, "argc", argc);
	char* bEventHandle = plg_B64Encode((unsigned char*)&eventHandle, sizeof(void*));
	pJson_AddStringToObject(root, "event", bEventHandle);

	if (argc) {
		pJSON* argvJson = pJson_CreateStringArray(argv, argc);
		pJson_AddItemToObject(root, "argv", argvJson);
	}

	char* cValue = pJson_Print(root);
	ret = plg_MngRemoteCall(pvManage, order, orderLen, cValue, strlen(cValue));

	free(bEventHandle);
	pJson_Delete(root);

	return ret;
}

void* plg_MngJobHandle(void* pvManage) {
	PManage pManage = pvManage;
	return pManage->pJobHandle;
}

/*
当锁在循环嵌套中时要谨慎考虑,这里加锁可能导致释放失败或死锁
*/
static int OrderDestroyCount(char* value, short valueLen) {
	unsigned int run = 0;
	PManageDestroy pManageDestroy = (PManageDestroy)value;
	MutexLock(pManageDestroy->pManage->mutexHandle, pManageDestroy->pManage->objName);
	if (valueLen == sizeof(ManageDestroy)) {
		if (pManageDestroy->type == 1) {
			pManageDestroy->pManage->jobDestroyCount += 1;
			if (pManageDestroy->pManage->jobDestroyCount == listLength(pManageDestroy->pManage->listJob)) {
				run = 1;
			}			
		}else if (pManageDestroy->type == 2) {
			pManageDestroy->pManage->fileDestroyCount += 1;
			if (pManageDestroy->pManage->fileDestroyCount == listLength(pManageDestroy->pManage->listDisk)) {
				run = 2;
			}
		}
	}
	MutexUnlock(pManageDestroy->pManage->mutexHandle, pManageDestroy->pManage->objName);

	if (run == 1) {
		sleep(0);
		pManageDestroy->fun(pManageDestroy->ptr);
	} else if (run == 2) {
		sleep(0);
		pManageDestroy->fun(pManageDestroy->ptr);
	}

	return 1;
}

static void manage_InternalDestoryHandle(void* pvManage) {

	PManage pManage = pvManage;
	plg_MutexDestroyHandle(pManage->mutexHandle);
	plg_listRelease(pManage->listDisk);
	plg_listRelease(pManage->listJob);
	plg_listRelease(pManage->listOrder);
	plg_listRelease(pManage->listProcess);
	plg_dictRelease(pManage->dictTableName);

	plg_dictRelease(pManage->order_process);
	plg_dictRelease(pManage->order_equeue);
	plg_DictSetDestroy(pManage->order_tableName);
	plg_dictRelease(pManage->tableName_diskHandle);
	
	plg_sdsFree(pManage->dbPath);
	plg_sdsFree(pManage->objName);
	plg_sdsFree(pManage->luaDllPath);
	plg_sdsFree(pManage->luaPath);
	plg_sdsFree(pManage->dllPath);
	
	free(pManage);
}

/*
所有线程已经停止,不用再使用锁了
*/
static void CompleteDestroyFile(void* value) {

	PManageDestroy pManageDestroy = (PManageDestroy)value;
	pManageDestroy->pManage->runStatus = 0; 
	free(value);
	plg_JobSetExitThread(2);
}

//to Destroy file
static void CallBackDestroyFile(void* value) {

	elog(log_fun, "CallBackDestroyFile");
	unsigned int run = 0;
	PManageDestroy pManageDestroy = (PManageDestroy)value;
	MutexLock(pManageDestroy->pManage->mutexHandle, pManageDestroy->pManage->objName);
	if (listLength(pManageDestroy->pManage->listDisk) == 0) {
		run = 1;
	} else {
		
		//listjob
		listIter* diskIter = plg_listGetIterator(pManageDestroy->pManage->listDisk, AL_START_HEAD);
		listNode* diskNode;
		while ((diskNode = plg_listNext(diskIter)) != NULL) {

			ManageDestroy pManageDestroyForFile;
			pManageDestroyForFile.fun = CompleteDestroyFile;
			pManageDestroyForFile.ptr = pManageDestroy;
			pManageDestroyForFile.pManage = pManageDestroy->pManage;
			pManageDestroyForFile.type = 2;		
			if (plg_DiskFileHandle(listNodeValue(diskNode))) {
				void* eQueueHandle = plg_JobEqueueHandle(plg_FileJobHandle(plg_DiskFileHandle(listNodeValue(diskNode))));
				plg_JobSendOrder(eQueueHandle, "destroy", (char*)&pManageDestroyForFile, sizeof(ManageDestroy));
			} else {
				plg_JobSendOrder(plg_JobEqueueHandle(pManageDestroy->pManage->pJobHandle), "destroycount", (char*)&pManageDestroyForFile, sizeof(ManageDestroy));
			}
		}
		plg_listReleaseIterator(diskIter);
	}
	MutexUnlock(pManageDestroy->pManage->mutexHandle, pManageDestroy->pManage->objName);

	if (run == 1) {
		CompleteDestroyFile(pManageDestroy);
	}
}

/*
创建一个句柄用于管理多个文件
多线程不安全,在多线程启动期间只读。
*/
void* plg_MngCreateHandle(char* dbPath, short dbPahtLen) {

	CheckUsingThread(0);

	plg_LocksCreate();
	plg_LogInit();
	elog(log_warn, "--------Welcome to pelgia!--------");

	PManage pManage = malloc(sizeof(Manage));
	pManage->mutexHandle = plg_MutexCreateHandle(LockLevel_1);

	pManage->listDisk = plg_listCreate(LIST_MIDDLE);
	listSetFreeMethod(pManage->listDisk, listFileFree);
	
	pManage->listJob = plg_listCreate(LIST_MIDDLE);
	listSetFreeMethod(pManage->listJob, listJobFree);

	pManage->listOrder = plg_listCreate(LIST_MIDDLE);
	listSetFreeMethod(pManage->listOrder, listSdsFree);

	pManage->listProcess = plg_listCreate(LIST_MIDDLE);
	listSetFreeMethod(pManage->listProcess, listProcessFree);

	pManage->dictTableName = plg_dictCreate(&SdsDictType, NULL, DICT_MIDDLE);

	pManage->order_tableName = plg_DictSetCreate(plg_DefaultSdsDictPtr(), DICT_MIDDLE, plg_DefaultSdsDictPtr(), DICT_MIDDLE);
	pManage->tableName_diskHandle = plg_dictCreate(plg_DefaultSdsDictPtr(), NULL, DICT_MIDDLE);
	pManage->order_process = plg_dictCreate(plg_DefaultSdsDictPtr(), NULL, DICT_MIDDLE);
	pManage->order_equeue = plg_dictCreate(plg_DefaultSdsDictPtr(), NULL, DICT_MIDDLE);
	pManage->dbPath = plg_sdsNewLen(dbPath, dbPahtLen);
	pManage->objName = plg_sdsNew("manage");
	pManage->pJobHandle = plg_JobCreateHandle(0, TT_MANAGE, 0, 0, 0);
	plg_JobSetPrivate(pManage->pJobHandle, pManage);

	pManage->fileCount = 0;
	pManage->jobDestroyCount = 0;
	pManage->fileDestroyCount = 0;
	pManage->runStatus = 0;
	pManage->maxTableWeight = 1000;
	pManage->luaDllPath = plg_sdsEmpty();
	pManage->luaPath = plg_sdsEmpty();
	pManage->dllPath = plg_sdsEmpty();

	//event process
	plg_JobAddAdmOrderProcess(pManage->pJobHandle, "destroycount", plg_JobCreateFunPtr(OrderDestroyCount));
	return pManage;
}

void plg_MngSetDllPath(void* pvManage, char* newDllPath) {
	PManage pManage = pvManage;
	plg_sdsFree(pManage->dllPath);
	pManage->dllPath = plg_sdsNew(newDllPath);
}

void plg_MngSetLuaDllPath(void* pvManage, char* newLuaDllPath) {
	PManage pManage = pvManage;
	plg_sdsFree(pManage->luaDllPath);
	pManage->luaDllPath = plg_sdsNew(newLuaDllPath);
}

void plg_MngSetLuaPath(void* pvManage, char* newLuaPath) {
	PManage pManage = pvManage;
	plg_sdsFree(pManage->luaPath);
	pManage->luaPath = plg_sdsNew(newLuaPath);
}

void plg_MngSendExit(void* pvManage){
	PManage pManage = pvManage;
	plg_EventSend(pManage->pEvent, NULL, 0);
}

/*
实际要停止所有线程在多线程安全下执行
*/
void plg_MngDestoryHandle(void* pvManage) {
	
	CheckUsingThread(NORET);
	PManage pManage = pvManage;
	if (pManage->runStatus == 0) {
		plg_JobDestoryHandle(pManage->pJobHandle);
	} else {
		void* pEvent = plg_EventCreateHandle();
		pManage->pEvent = pEvent;
		MutexLock(pManage->mutexHandle, pManage->objName);
		PManageDestroy pManageDestroy = malloc(sizeof(ManageDestroy));
		pManageDestroy->pManage = pManage;
		pManageDestroy->type = 3;
		manage_DestroyJob(pManage, CallBackDestroyFile, pManageDestroy);
		MutexUnlock(pManage->mutexHandle, pManage->objName);

		plg_EventWait(pEvent);
		plg_EventDestroyHandle(pEvent);
	}
	manage_InternalDestoryHandle(pManage);
	plg_LogDestroy();
	plg_LocksDestroy();
}

char plg_MngCheckUsingThread() {
	if (1 != plg_JobCheckIsType(TT_OTHER)) {
		return 0;
	}
	return 1;
}

void plg_MngSetMaxTableWeight(void* pvManage, unsigned int maxTableWeight) {
	PManage pManage = pvManage;
	pManage->maxTableWeight = maxTableWeight;
}

void plg_MngPrintAllJobStatus(void* pvManage) {

	PManage pManage = pvManage;
	printf("job size: %d\n", listLength(pManage->listJob));
	//listjob
	listIter* jobIter = plg_listGetIterator(pManage->listJob, AL_START_HEAD);
	listNode* jobNode;
	while ((jobNode = plg_listNext(jobIter)) != NULL) {
		plg_JobPrintStatus(listNodeValue(jobNode));
	}
	plg_listReleaseIterator(jobIter);
}

void plg_MngPrintAllJobDetails(void* pvManage) {

	PManage pManage = pvManage;
	printf("job size: %d\n", listLength(pManage->listJob));
	//listjob
	listIter* jobIter = plg_listGetIterator(pManage->listJob, AL_START_HEAD);
	listNode* jobNode;
	while ((jobNode = plg_listNext(jobIter)) != NULL) {
		plg_JobPrintDetails(listNodeValue(jobNode));
	}
	plg_listReleaseIterator(jobIter);
}

void plg_MngPrintAllJobOrder(void* pvManage) {

	PManage pManage = pvManage;
	printf("job size: %d\n", listLength(pManage->listJob));
	//listjob
	listIter* jobIter = plg_listGetIterator(pManage->listJob, AL_START_HEAD);
	listNode* jobNode;
	while ((jobNode = plg_listNext(jobIter)) != NULL) {
		plg_JobPrintOrder(listNodeValue(jobNode));
	}
	plg_listReleaseIterator(jobIter);
}

void plg_MngPrintAllStatus(void* pvManage) {

	PManage pManage = pvManage;
	printf("ldisk:%d ljob:%d lorder:%d lprocess:%d dtable:%d order_process:%d o_equeue:%d o_table:%d table_disk:%d file_count:%d job_destroy_c:%d f_d_c:%d\n",
		listLength(pManage->listDisk),
		listLength(pManage->listJob),
		listLength(pManage->listOrder),
		listLength(pManage->listProcess),
		dictSize(pManage->dictTableName),
		dictSize(pManage->order_process),
		dictSize(pManage->order_equeue),
		plg_DictSetSize(pManage->order_tableName),
		dictSize(pManage->tableName_diskHandle),
		pManage->fileCount,
		pManage->jobDestroyCount,
		pManage->fileDestroyCount
		);
}

void plg_MngPrintAllDetails(void* pvManage) {

	PManage pManage = pvManage;
	dict * table = plg_DictSetDict(pManage->order_tableName);

	dictIterator* tableIter = plg_dictGetSafeIterator(table);
	dictEntry* tableNode;
	while ((tableNode = plg_dictNext(tableIter)) != NULL) {

		printf("\norder:%s ", (char*)dictGetKey(tableNode));

		dict* value = dictGetVal(tableNode);

		dictIterator* valueIter = plg_dictGetSafeIterator(value);
		dictEntry* valueNode;
		while ((valueNode = plg_dictNext(valueIter)) != NULL) {
			printf("table:%s", (char*)dictGetKey(valueNode));
		}
		plg_dictReleaseIterator(valueIter);
	}
	plg_dictReleaseIterator(tableIter);
	printf("\n");
}

/*
loop event_dictTableName
core: number core
*/
void plg_MngPrintPossibleAlloc(void* pvManage) {

	PManage pManage = pvManage;
	void* pDictTableName = plg_DictSetCreate(plg_DefaultUintPtr(), DICT_MIDDLE, plg_DefaultSdsDictPtr(), DICT_MIDDLE);
	void* pDictOrder = plg_DictSetCreate(plg_DefaultUintPtr(), DICT_MIDDLE, plg_DefaultSdsDictPtr(), DICT_MIDDLE);

	//listOrder
	listIter* eventIter = plg_listGetIterator(pManage->listOrder, AL_START_HEAD);
	listNode* eventNode;
	while ((eventNode = plg_listNext(eventIter)) != NULL) {

		//process
		dictEntry * EventProcessEntry = plg_dictFind(pManage->order_process, listNodeValue(eventNode));
		if (EventProcessEntry == 0) {
			continue;
		}

		//loop dictTableName
		char nextContinue = 0;
		dictIterator* jobIter = plg_dictGetIterator((dict*)plg_DictSetDict(pDictTableName));
		dictEntry* jobNode;
		while ((jobNode = plg_dictNext(jobIter)) != NULL) {

			//Judge whether there is intersection
			dict * table = plg_DictSetValue(pManage->order_tableName, listNodeValue(eventNode));
			if (!table) {
				continue;
			}
			dictIterator* tableIter = plg_dictGetSafeIterator(table);
			dictEntry* tableNode;
			while ((tableNode = plg_dictNext(tableIter)) != NULL) {
				
				//Find the intersection in dicttablename, and merge together if there is any
				if (plg_DictSetIn(pDictTableName, dictGetKey(jobNode), dictGetKey(tableNode))) {
					dictIterator* tableLoopIter = plg_dictGetSafeIterator(table);
					dictEntry* tableLoopNode;
					while ((tableLoopNode = plg_dictNext(tableLoopIter)) != NULL) {
						plg_DictSetAdd(pDictTableName, dictGetKey(jobNode), dictGetKey(tableLoopNode));
					}
					plg_dictReleaseIterator(tableLoopIter);

					plg_DictSetAdd(pDictOrder, dictGetKey(jobNode), dictGetKey(EventProcessEntry));
					nextContinue = 1;
					break;
				}
			}
			plg_dictReleaseIterator(tableIter);
			if (nextContinue) {
				break;
			}
		}
		plg_dictReleaseIterator(jobIter);
		if (nextContinue) {
			continue;
		}

		//Create new and merge not found
		do {
			unsigned int* key = malloc(sizeof(unsigned int));
			*key = dictSize((dict*)plg_DictSetDict(pDictTableName));
			unsigned int* key2 = malloc(sizeof(unsigned int));
			*key2 = *key;

			dict * table = plg_DictSetValue(pManage->order_tableName, listNodeValue(eventNode));
			if (table) {
				dictIterator* tableLoopIter = plg_dictGetSafeIterator(table);
				dictEntry* tableLoopNode;
				while ((tableLoopNode = plg_dictNext(tableLoopIter)) != NULL) {
					plg_DictSetAdd(pDictTableName, key, dictGetKey(tableLoopNode));
				}
				plg_dictReleaseIterator(tableLoopIter);

				plg_DictSetAdd(pDictOrder, key2, dictGetKey(EventProcessEntry));
			}

		} while (0);
	}
	plg_listReleaseIterator(eventIter);

	//Print all results
	printf("all order group: %d\n", dictSize((dict*)plg_DictSetDict(pDictTableName)));
	dictIterator* jobIter = plg_dictGetIterator((dict*)plg_DictSetDict(pDictTableName));
	dictEntry* jobNode;
	while ((jobNode = plg_dictNext(jobIter)) != NULL) {

		printf("group %d:\n", *(unsigned int*)dictGetKey(jobNode));

		printf("order:\n");
		dictIterator* orderIter = plg_dictGetIterator((dict*)plg_DictSetValue(pDictOrder, dictGetKey(jobNode)));
		dictEntry* orderNode;
		while ((orderNode = plg_dictNext(orderIter)) != NULL) {

			printf("%s ", (char*)dictGetKey(orderNode));
		}
		plg_dictReleaseIterator(orderIter);

		printf("\n");

		printf("table:\n");
		dictIterator* tableIter = plg_dictGetIterator((dict*)plg_DictSetValue(pDictTableName, dictGetKey(jobNode)));
		dictEntry* tableNode;
		while ((tableNode = plg_dictNext(tableIter)) != NULL) {

			printf("%s ", (char*)dictGetKey(tableNode));
		}
		plg_dictReleaseIterator(tableIter);

		printf("\n");
	}
	plg_dictReleaseIterator(jobIter);
	//free
	plg_DictSetDestroy(pDictTableName);
	plg_DictSetDestroy(pDictOrder);
	return;

}

typedef struct _Param
{
	sds outJson;
	void* pEvent;
	void* pManage;
	pJSON* fromJson;
	short endFlg;
}*PParam, Param;

static int OutJsonRouting(char* value, short valueLen) {

	//routing
	NOTUSED(valueLen);
	PParam pParam = (PParam)value;
	PManage pManage = pParam->pManage;
	pJSON* root = pJson_CreateObject();

	dictIterator* tableNameIter = plg_dictGetIterator(pManage->dictTableName);
	dictEntry* tableNameNode;
	while ((tableNameNode = plg_dictNext(tableNameIter)) != NULL) {
		pJSON* obj = pJson_CreateObject();
		pJson_AddItemToObject(root, dictGetKey(tableNameNode), obj);
		plg_JobTableMembersWithJson(dictGetKey(tableNameNode), plg_sdsLen(dictGetKey(tableNameNode)), obj);
	}
	plg_dictReleaseIterator(tableNameIter);

	//open file
	FILE *outputFile;
	outputFile = fopen_t(pParam->outJson, "wb");
	if (outputFile) {
		char* ptr = pJson_Print(root);
		fwrite(ptr, 1, strlen(ptr), outputFile);
	} else {
		elog(log_warn, "OutJsonRouting.fopen_t.wb!");
	}

	fclose(outputFile);

	plg_sdsFree(pParam->outJson);
	pJson_Delete(root);

	//all pass
	plg_EventSend(pParam->pEvent, NULL, 0);
	printf("OutJsonRouting all pass!\n");
	sleep(0);
	return 1;
}

void plg_MngOutJson(char* fileName, char* outJson) {

	void* pManage = plg_MngCreateHandle(0, 0);
	void* pEvent = plg_EventCreateHandle();

	plg_MngFreeJob(pManage);

	char order[10] = { 0 };
	sprintf(order, "order");
	plg_MngAddOrder(pManage, order, strlen(order), plg_JobCreateFunPtr(OutJsonRouting));

	plg_MngInterAllocJob(pManage, 1, fileName);
	plg_MngStarJob(pManage);

	Param param;
	param.outJson = plg_sdsNew(outJson);
	param.pEvent = pEvent;
	param.pManage = pManage;
	plg_MngRemoteCall(pManage, order, strlen(order), (char*)&param, sizeof(Param));

	//Because it is not a thread created by ptw32, ptw32 new cannot release memory leak
	plg_EventWait(pEvent);
	sleep(2);

	unsigned int eventLen;
	void * ptr = plg_EventRecvAlloc(pEvent, &eventLen);
	plg_EventFreePtr(ptr);

	plg_EventDestroyHandle(pEvent);
	plg_MngDestoryHandle(pManage);
	sleep(2);
}

static int FromJsonRouting(char* value, short valueLen) {

	//routing
	NOTUSED(valueLen);
	PParam pParam = (PParam)value;
	
	for (int i = 0; i < pJson_GetArraySize(pParam->fromJson); i++)
	{
		pJSON * item = pJson_GetArrayItem(pParam->fromJson, i);
		if (pJson_String == item->type) {
			unsigned int outLen;
			unsigned char* pValue = plg_B64DecodeEx(item->valuestring, strlen(item->valuestring), &outLen);
			plg_JobSet(pParam->fromJson->string, strlen(pParam->fromJson->string), item->valuestring, strlen(item->valuestring), pValue, outLen);
		}
	}

	if (pParam->endFlg) {
		//all pass
		plg_EventSend(pParam->pEvent, NULL, 0);
		printf("FromJsonRouting all pass!\n");
	}

	return 1;
}

void plg_MngFromJson(char* fromJson) {

	void* pManage = plg_MngCreateHandle(0, 0);
	void* pEvent = plg_EventCreateHandle();

	plg_MngFreeJob(pManage);

	char order[10] = { 0 };
	strcpy(order, "order");
	plg_MngAddOrder(pManage, order, strlen(order), plg_JobCreateFunPtr(FromJsonRouting));

	//open file
	FILE *inputFile;
	char* rootJson = 0;
	inputFile = fopen_t(fromJson, "rb");
	if (inputFile) {
		fseek_t(inputFile, 0, SEEK_END);
		unsigned int inputFileLength = ftell_t(inputFile);

		fseek_t(inputFile, 0, SEEK_SET);
		rootJson = malloc(inputFileLength);
		unsigned long long retRead = fread(rootJson, 1, inputFileLength, inputFile);
		if (retRead != inputFileLength) {
			elog(log_error, "plg_MngFromJson.fread.rootJson!");
			return;
		}
		fclose(inputFile);
	} else {
		elog(log_warn, "plg_MngFromJson.fopen_t.rb!");
	}

	pJSON * root = pJson_Parse(fromJson);
	free(rootJson);
	if (!root) {
		elog(log_error, "plg_MngFromJson:json Error before: [%s]\n", pJson_GetErrorPtr());
		return;
	}

	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_Object == item->type) {
			plg_MngAddTable(pManage, order, strlen(order), item->string, strlen(item->string));
		}
	}

	plg_MngAllocJob(pManage, 1);
	plg_MngStarJob(pManage);

	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_Object == item->type) {
			Param param;
			param.fromJson = item;
			param.pEvent = pEvent;
			param.pManage = pManage;
			param.endFlg = (i == pJson_GetArraySize(root)) ? 1 : 0;
			plg_MngRemoteCall(pManage, order, strlen(order), (char*)&param, sizeof(Param));
		}
	}

	
	//Because it is not a thread created by ptw32, ptw32 new cannot release memory leak
	plg_EventWait(pEvent);
	sleep(0);

	unsigned int eventLen;
	void * ptr = plg_EventRecvAlloc(pEvent, &eventLen);
	plg_EventFreePtr(ptr);

	pJson_Delete(root);
	plg_EventDestroyHandle(pEvent);
	plg_MngDestoryHandle(pManage);
}

#undef NORET
#undef CheckUsingThread