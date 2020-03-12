/* job.c - Thread related functions
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
#include "psds.h"
#include "pdict.h"
#include "pjob.h"
#include "pequeue.h"
#include "padlist.h"
#include "pcache.h"
#include "pinterface.h"
#include "pmanage.h"
#include "plocks.h"
#include "pelog.h"
#include "pdictexten.h"
#include "ptimesys.h"
#include "plibsys.h"
#include "plvm.h"
#include "pquicksort.h"
#include "pelagia.h"

/*
Thread model can be divided into two ways: asynchronous and synchronous
This is also true between multiple threads
For read operations, synchronization is used because the user virtual single thread environment needs to be maintained
For write operations, asynchronous queue mode is used for efficiency between different hardware environments
Disk writes are not crowded, so thread queues and threads are not implemented
Manage has separate threads because it manages multiple modules
File has a separate thread to write to a slow hard disk
*/
#define NORET
#define CheckUsingThread(r) if (plg_JobCheckUsingThread()) {elog(log_error, "Cannot run job interface in non job environment"); return r;}

enum ScriptType {
	ST_LUA = 1,
	ST_DLL = 2,
	ST_PTR = 3
};

typedef struct _EventPorcess
{
	unsigned char scriptType;
	sds fileClass;
	sds function;
	RoutingFun functionPoint;
	unsigned int weight;
}*PEventPorcess, EventPorcess;

static void PtrFreeCallback(void *privdata, void *val) {
	DICT_NOTUSED(privdata);
	plg_CacheDestroyHandle(val);
}

static int sdsCompareCallback(void *privdata, const void *key1, const void *key2) {
	int l1, l2;
	DICT_NOTUSED(privdata);

	l1 = plg_sdsLen((sds)key1);
	l2 = plg_sdsLen((sds)key2);
	if (l1 != l2) return 0;
	return memcmp(key1, key2, l1) == 0;
}

static unsigned long long sdsHashCallback(const void *key) {
	return plg_dictGenHashFunction((unsigned char*)key, plg_sdsLen((char*)key));
}

static dictType PtrDictType = {
	sdsHashCallback,
	NULL,
	NULL,
	sdsCompareCallback,
	NULL,
	PtrFreeCallback
};

/*
Threadtype: the type of the current thread
Pmanagequeue: event handle for management thread
Privatedata: the private data of the current thread, which is filehandle
EQueue: the message slot of the current thread
Order Ou queue: message slot corresponding to all events
Dictcache: all the caches of a job are used to release, create and check whether the tablename is writable
Order_Process: the processing process of the current thread event
Tablename? Cachehandle: Currently, all cachehandles corresponding to tablename are used to find and write data
Allweight: all processes have the same weight
Userevent: event not called remotely
Userprocess: handling of non remote call events
ExitThread: exit flag
Trancache: cache used by the current transaction
Tranflush: multiple caches preparing for flush
Transaction commit related flags
Flush_laststamp: time of last submission
Flush_interval: commit interval
Flush_Lastcount: number of submissions
Flush Ou count: total number of times
*/
typedef struct _JobHandle
{
	enum ThreadType threadType;
	void* pManageEqueue;
	void* privateData;
	void* eQueue;
	dict* order_equeue;
	dict* dictCache;
	dict* order_process;
	dict* tableName_cacheHandle;
	unsigned int allWeight;
	list* userEvent;
	list* userProcess;
	short exitThread;
	short donotFlush;
	short donotCommit;

	list* tranCache;
	list* tranFlush;

	//config
	unsigned long long flush_lastStamp;
	unsigned int flush_interval;
	unsigned int flush_lastCount;
	unsigned int flush_count;

	//vm
	char* luaPath;
	void* luaHandle;
	void* dllHandle;

	//order;
	char* pOrderName;

	//intervalometer
	list* pListIntervalometer;

} *PJobHandle, JobHandle;

SDS_TYPE
void* plg_JobCreateFunPtr(RoutingFun funPtr) {

	long long ss = ll;
	ss = ss * 0;
	PEventPorcess pEventPorcess = malloc(sizeof(EventPorcess));
	pEventPorcess->scriptType = ST_PTR;
	pEventPorcess->functionPoint = funPtr;
	pEventPorcess->weight = 1;
	return pEventPorcess;
}

void* plg_JobCreateLua(char* fileClass, short fileClassLen, char* fun, short funLen) {

	PEventPorcess pEventPorcess = malloc(sizeof(EventPorcess));
	pEventPorcess->scriptType = ST_LUA;
	pEventPorcess->fileClass = plg_sdsNewLen(fileClass, fileClassLen);
	pEventPorcess->function = plg_sdsNewLen(fun, funLen);
	pEventPorcess->weight = 1;
	return pEventPorcess;
}

void* plg_JobCreateDll(char* fileClass, short fileClassLen, char* fun, short funLen) {

	PEventPorcess pEventPorcess = malloc(sizeof(EventPorcess));
	pEventPorcess->scriptType = ST_DLL;
	pEventPorcess->fileClass = plg_sdsNewLen(fileClass, fileClassLen);
	pEventPorcess->function = plg_sdsNewLen(fun, funLen);
	pEventPorcess->weight = 1;
	return pEventPorcess;
}

void plg_JobSetWeight(void* pvEventPorcess, unsigned int weight) {
	PEventPorcess pEventPorcess = pvEventPorcess;
	pEventPorcess->weight = weight;
}

void plg_JobProcessDestory(void* pvEventPorcess) {

	PEventPorcess pEventPorcess = pvEventPorcess;
	if (pEventPorcess->scriptType != ST_PTR) {
		plg_sdsFree(pEventPorcess->fileClass);
		plg_sdsFree(pEventPorcess->function);
	}
	free(pEventPorcess);
}

static void listSdsFree(void *ptr) {
	plg_sdsFree(ptr);
}

typedef struct __Intervalometer {
	unsigned long long tim;
	sds Order;
	sds Value;
}*PIntervalometer, Intervalometer;

static void listIntervalometerFree(void *ptr) {
	PIntervalometer pPIntervalometer = (PIntervalometer)ptr;
	plg_sdsFree(pPIntervalometer->Order);
	plg_sdsFree(pPIntervalometer->Value);
	free(ptr);
}

static void listProcessFree(void *ptr) {
	plg_JobProcessDestory(ptr);
}

void* job_Handle() {

	CheckUsingThread(0);
	return plg_LocksGetSpecific();
}

void* job_ManageEqueue() {

	CheckUsingThread(0);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	return pJobHandle->pManageEqueue;
}

void plg_JobSetExitThread(char value) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	pJobHandle->exitThread = value;
}

void plg_JobSetDonotFlush(short value) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	pJobHandle->donotFlush = value;
}

void plg_JobSetDonotCommit(short value) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	pJobHandle->donotCommit = value;
}

void job_Flush(void* pvJobHandle) {

	PJobHandle pJobHandle = pvJobHandle;
	listIter* iter = plg_listGetIterator(pJobHandle->tranFlush, AL_START_HEAD);
	listNode* node;
	while ((node = plg_listNext(iter)) != NULL) {
		plg_CacheFlush(listNodeValue(node));
	}
	plg_listReleaseIterator(iter);
	plg_listEmpty(pJobHandle->tranFlush);
}

void job_Commit(void* pvJobHandle) {

	PJobHandle pJobHandle = pvJobHandle;
	listIter* iter = plg_listGetIterator(pJobHandle->tranCache, AL_START_HEAD);
	listNode* node;
	while ((node = plg_listNext(iter)) != NULL) {
		plg_CacheCommit(listNodeValue(node));

		plg_listAddNodeHead(pJobHandle->tranFlush, listNodeValue(node));
	}
	plg_listReleaseIterator(iter);
	plg_listEmpty(pJobHandle->tranCache);
}

void job_Rollback(void* pvJobHandle) {

	PJobHandle pJobHandle = pvJobHandle;
	listIter* iter = plg_listGetIterator(pJobHandle->tranCache, AL_START_HEAD);
	listNode* node;
	while ((node = plg_listNext(iter)) != NULL) {
		plg_CacheRollBack(listNodeValue(node));
	}
	plg_listReleaseIterator(iter);
	plg_listEmpty(pJobHandle->tranCache);
}

static int OrderDestroy(char* value, short valueLen) {
	elog(log_fun, "job.OrderDestroy");
	plg_JobSendOrder(job_ManageEqueue(), "destroycount", value, valueLen);
	plg_JobSetExitThread(1);
	return 1;
}

static int OrderDestroyJob(char* value, short valueLen) {
	NOTUSED(value);
	NOTUSED(valueLen);
	elog(log_fun, "job.OrderDestroyJob");
	plg_JobSetExitThread(1);
	return 1;
}

static int OrderJobFinish(char* value, short valueLen) {
	NOTUSED(value);
	NOTUSED(valueLen);
	PJobHandle pJobHandle = job_Handle();

	if (!pJobHandle->donotCommit) {
		job_Commit(pJobHandle);		
	} else {
		pJobHandle->donotCommit = 0;
	}
	
	if (!pJobHandle->donotFlush) {
		unsigned long long stamp = plg_GetCurrentSec();
		if (pJobHandle->flush_count > pJobHandle->flush_lastCount++) {
			pJobHandle->flush_lastCount = 0;
			job_Flush(pJobHandle);
		} else if (pJobHandle->flush_lastStamp - stamp > pJobHandle->flush_interval) {
			pJobHandle->flush_lastStamp = stamp;
			job_Flush(pJobHandle);
		}	
	} else {
		pJobHandle->donotFlush = 0;
	}
	return 1;
}

static void InitProcessCommend(void* pvJobHandle) {

	//event process
	PJobHandle pJobHandle = pvJobHandle;
	plg_JobAddAdmOrderProcess(pJobHandle, "destroy", plg_JobCreateFunPtr(OrderDestroy));
	plg_JobAddAdmOrderProcess(pJobHandle, "destroyjob", plg_JobCreateFunPtr(OrderDestroyJob));
	plg_JobAddAdmOrderProcess(pJobHandle, "finish", plg_JobCreateFunPtr(OrderJobFinish));
}

void plg_JobSetPrivate(void* pvJobHandle, void* privateData) {
	PJobHandle pJobHandle = pvJobHandle;
	pJobHandle->privateData = privateData;
}

void* plg_JobGetPrivate() {

	CheckUsingThread(0);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	return pJobHandle->privateData;
}

void* plg_JobCreateHandle(void* pManageEqueue, enum ThreadType threadType, char* luaPath, char* luaDllPath, char* dllPath) {

	PJobHandle pJobHandle = malloc(sizeof(JobHandle));
	pJobHandle->eQueue = plg_eqCreate();
	pJobHandle->order_equeue = plg_dictCreate(plg_DefaultSdsDictPtr(), NULL, DICT_MIDDLE);
	pJobHandle->order_process = plg_dictCreate(plg_DefaultSdsDictPtr(), NULL, DICT_MIDDLE);
	pJobHandle->tableName_cacheHandle = plg_dictCreate(plg_DefaultSdsDictPtr(), NULL, DICT_MIDDLE);
	pJobHandle->dictCache = plg_dictCreate(&PtrDictType, NULL, DICT_MIDDLE);

	pJobHandle->tranCache = plg_listCreate(LIST_MIDDLE);
	pJobHandle->tranFlush = plg_listCreate(LIST_MIDDLE);

	pJobHandle->userEvent = plg_listCreate(LIST_MIDDLE);
	listSetFreeMethod(pJobHandle->userEvent, listSdsFree);

	pJobHandle->userProcess = plg_listCreate(LIST_MIDDLE);
	listSetFreeMethod(pJobHandle->userProcess, listProcessFree);

	pJobHandle->threadType = threadType;
	pJobHandle->pManageEqueue = pManageEqueue;
	pJobHandle->allWeight = 0;
	pJobHandle->exitThread = 0;
	pJobHandle->donotFlush = 0;
	pJobHandle->donotCommit = 0;

	pJobHandle->flush_lastStamp = plg_GetCurrentSec();
	pJobHandle->flush_interval = 5*60;
	pJobHandle->flush_count = 1;
	pJobHandle->flush_lastCount = 0;

	pJobHandle->luaPath = luaPath;

	if (dllPath && plg_sdsLen(dllPath)) {
		pJobHandle->dllHandle = plg_SysLibLoad(dllPath, 0);
	} else {
		pJobHandle->dllHandle = 0;
	}

	if (luaDllPath && plg_sdsLen(luaDllPath)) {
		pJobHandle->luaHandle = plg_LvmLoad(luaDllPath);
	} else {
		pJobHandle->luaHandle = 0;
	}

	SDS_CHECK(pJobHandle->allWeight, pJobHandle->luaHandle);
	pJobHandle->pListIntervalometer = plg_listCreate(LIST_MIDDLE);
	listSetFreeMethod(pJobHandle->pListIntervalometer, listIntervalometerFree);

	if (pJobHandle->threadType == TT_PROCESS) {
		InitProcessCommend(pJobHandle);
	}

	elog(log_fun, "plg_JobCreateHandle:%U", pJobHandle);
	return pJobHandle;
}

static void OrderFree(void* ptr) {

	POrderPacket pOrderPacket = ptr;
	plg_sdsFree(pOrderPacket->order);
	plg_sdsFree(pOrderPacket->value);
	free(pOrderPacket);
}

void plg_JobDestoryHandle(void* pvJobHandle) {

	PJobHandle pJobHandle = pvJobHandle;
	elog(log_fun, "plg_JobDestoryHandle:%U", pJobHandle);
	plg_eqDestory(pJobHandle->eQueue, OrderFree);
	plg_dictRelease(pJobHandle->order_equeue);
	plg_dictRelease(pJobHandle->dictCache);
	plg_listRelease(pJobHandle->tranCache);
	plg_listRelease(pJobHandle->tranFlush);
	plg_dictRelease(pJobHandle->order_process);
	plg_dictRelease(pJobHandle->tableName_cacheHandle);
	plg_listRelease(pJobHandle->userEvent);
	plg_listRelease(pJobHandle->userProcess);
	plg_listRelease(pJobHandle->pListIntervalometer);

	if (pJobHandle->luaHandle) {
		plg_LvmDestory(pJobHandle->luaHandle);
	}
	
	if (pJobHandle->dllHandle) {
		plg_SysLibUnload(pJobHandle->dllHandle);
	}
	free(pJobHandle);
}

unsigned char plg_JobFindTableName(void* pvJobHandle, sds tableName) {

	PJobHandle pJobHandle = pvJobHandle;
	dictEntry * entry = plg_dictFind(pJobHandle->dictCache, tableName);
	if (entry != 0) {
		return 1;
	} else {
		return 0;
	}
}

void plg_JobAddEventEqueue(void* pvJobHandle, sds nevent, void* equeue) {
	PJobHandle pJobHandle = pvJobHandle;
	plg_dictAdd(pJobHandle->order_equeue, nevent, equeue);
}

void plg_JobAddEventProcess(void* pvJobHandle, sds nevent, void* pvProcess) {

	PEventPorcess process = pvProcess;
	PJobHandle pJobHandle = pvJobHandle;
	plg_dictAdd(pJobHandle->order_process, nevent, process);
	pJobHandle->allWeight += process->weight;
}

/*
In this case, the cache should be triggered to obtain the disk related handle or perform table initialization when the re query is empty
*/
void* plg_JobNewTableCache(void* pvJobHandle, char* table, void* pDiskHandle) {

	PJobHandle pJobHandle = pvJobHandle;
	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, table);
	if (valueEntry == 0) {
		void* pCacheHandle = plg_CacheCreateHandle(pDiskHandle);
		plg_dictAdd(pJobHandle->dictCache, table, pCacheHandle);
		return pCacheHandle;
	} else {
		return dictGetVal(valueEntry);
	}
}

void plg_JobAddTableCache(void* pvJobHandle, char* table, void* pCacheHandle) {

	PJobHandle pJobHandle = pvJobHandle;
	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, table);
	if (valueEntry == 0) {
		plg_dictAdd(pJobHandle->tableName_cacheHandle, table, pCacheHandle);
	}
}

void* plg_JobEqueueHandle(void* pvJobHandle) {
	PJobHandle pJobHandle = pvJobHandle;
	return pJobHandle->eQueue;
}

unsigned int  plg_JobAllWeight(void* pvJobHandle) {
	PJobHandle pJobHandle = pvJobHandle;
	return pJobHandle->allWeight;
}

unsigned int  plg_JobIsEmpty(void* pvJobHandle) {
	PJobHandle pJobHandle = pvJobHandle;
	return dictSize(pJobHandle->order_process);
}

/*
User VM use
*/
int plg_JobRemoteCall(void* order, unsigned short orderLen, void* value, unsigned short valueLen) {

	CheckUsingThread(0);

	PJobHandle pJobHandle = plg_LocksGetSpecific();
	POrderPacket pOrderPacket = malloc(sizeof(OrderPacket));
	pOrderPacket->order = plg_sdsNewLen(order, orderLen);
	pOrderPacket->value = plg_sdsNewLen(value, valueLen);
	
	dictEntry* entry = plg_dictFind(pJobHandle->order_equeue, pOrderPacket->order);
	if (entry) {
		plg_eqPush(dictGetVal(entry), pOrderPacket);
		return 1;
	} else {
		elog(log_error, "plg_JobRemoteCall.Order:%s not found", order);
		return 0;
	}
}

static char job_IsCacheAllowWrite(void* pvJobHandle, char* PtrCache) {

	PJobHandle pJobHandle = pvJobHandle;
	dictEntry* entry = plg_dictFind(pJobHandle->dictCache, PtrCache);
	if (entry != 0)
		return 1;
	else
		return 0;
}

static int IntervalometerCmpFun(void* value1, void* value2) {

	PIntervalometer pi1 = (PIntervalometer)value1;
	PIntervalometer pi2 = (PIntervalometer)value2;

	if (pi1->tim > pi2->tim) {
		return 1;
	} else if (pi1->tim == pi2->tim) {
		return 0;
	} else {
		return -1;
	}
}

static long long plg_JogActIntervalometer(void* pvJobHandle) {

	PJobHandle pJobHandle = pvJobHandle;
	if (!listLength(pJobHandle->pListIntervalometer)) {
		return 0;
	}

	plg_SortList(pJobHandle->pListIntervalometer, IntervalometerCmpFun);
	
	unsigned long long sec = plg_GetCurrentSec();
	listIter* iter = plg_listGetIterator(pJobHandle->pListIntervalometer, AL_START_HEAD);
	listNode* node;
	while ((node = plg_listNext(iter)) != NULL) {

		PIntervalometer pPIntervalometer = (PIntervalometer)node->value;
		if (pPIntervalometer->tim <= sec) {
			plg_JobRemoteCall(pPIntervalometer->Order, plg_sdsLen(pPIntervalometer->Order), pPIntervalometer->Value, plg_sdsLen(pPIntervalometer->Value));
			plg_listDelNode(pJobHandle->pListIntervalometer, node);
		} else {
			break;
		}
	}
	plg_listReleaseIterator(iter);

	if (listLength(pJobHandle->pListIntervalometer)) {
		PIntervalometer pPIntervalometer = (PIntervalometer)listNodeValue(listFirst(pJobHandle->pListIntervalometer));
		return pPIntervalometer->tim;
	} else {
		return 0;
	}
}

static void* plg_JobThreadRouting(void* pvJobHandle) {

	elog(log_fun, "plg_JobThreadRouting");

	PJobHandle pJobHandle = pvJobHandle;
	plg_LocksSetSpecific(pJobHandle);

	//init
	sds sdsKey = plg_sdsNew("init");
	dictEntry* entry = plg_dictFind(pJobHandle->order_process, sdsKey);
	if (entry) {
		PEventPorcess pEventPorcess = (PEventPorcess)dictGetVal(entry);
		if (pEventPorcess->scriptType == ST_PTR) {
			pEventPorcess->functionPoint(NULL, 0);
		}
	}
	plg_sdsFree(sdsKey);

	//start
	sdsKey = plg_sdsNew("start");
	PEventPorcess pStartPorcess = 0;
	entry = plg_dictFind(pJobHandle->order_process, sdsKey);
	if (entry) {
		pStartPorcess = (PEventPorcess)dictGetVal(entry);
	}
	plg_sdsFree(sdsKey);

	//finish
	sdsKey = plg_sdsNew("finish");
	PEventPorcess pFinishPorcess = 0;
	entry = plg_dictFind(pJobHandle->order_process, sdsKey);
	if (entry) {
		pFinishPorcess = (PEventPorcess)dictGetVal(entry);
	}
	plg_sdsFree(sdsKey);
	long long timer = 0;

	do {
		if (timer == 0) {
			plg_eqWait(pJobHandle->eQueue);
		} else {
			if (-1 == plg_eqTimeWait(pJobHandle->eQueue, timer, 0)) {
				timer = plg_JogActIntervalometer(pJobHandle);
				continue;
			}
		}
		
		do {
			POrderPacket pOrderPacket = (POrderPacket)plg_eqPop(pJobHandle->eQueue);

			if (pOrderPacket != 0) {
				elog(log_details, "ThreadType:%i.plg_JobThreadRouting.order:%s", pJobHandle->threadType, (char*)pOrderPacket->order);
				pJobHandle->pOrderName = pOrderPacket->order;

				//start
				if (pStartPorcess && pStartPorcess->scriptType == ST_PTR) {
					pStartPorcess->functionPoint(NULL, 0);
				}

				entry = plg_dictFind(pJobHandle->order_process, pOrderPacket->order);
				if (entry) {
					PEventPorcess pEventPorcess = (PEventPorcess)dictGetVal(entry);
					if (pEventPorcess->scriptType == ST_PTR) {
						if (0 == pEventPorcess->functionPoint(pOrderPacket->value, plg_sdsLen(pOrderPacket->value))) {
							job_Rollback(pJobHandle);
						}
					} else if (pEventPorcess->scriptType == ST_DLL && pJobHandle->dllHandle) {

						RoutingFun fun = plg_SysLibSym(pJobHandle->dllHandle, pEventPorcess->function);
						if (fun) {
							if (0 == fun(pOrderPacket->value, plg_sdsLen(pOrderPacket->value))) {
								job_Rollback(pJobHandle);
							}
						}
					} else if (pEventPorcess->scriptType == ST_LUA && pJobHandle->luaHandle)  {

						sds file = plg_sdsCatFmt(plg_sdsEmpty(), "%s/%s", pJobHandle->luaPath, pEventPorcess->fileClass);
						if (0 == plg_LvmCallFile(pJobHandle->luaHandle, file, pEventPorcess->function, pOrderPacket->value, plg_sdsLen(pOrderPacket->value))) {
							job_Rollback(pJobHandle);
						}
					}
				}
				pJobHandle->pOrderName = 0;
				plg_sdsFree(pOrderPacket->order);
				plg_sdsFree(pOrderPacket->value);
				free(pOrderPacket);

				//finish
				if (pFinishPorcess && pFinishPorcess->scriptType == ST_PTR) {
					pFinishPorcess->functionPoint(NULL, 0);
				}

				elog(log_details, "plg_JobThreadRouting.finish!");
			} else {
				break; 
			}
		} while (1);

		timer = plg_JogActIntervalometer(pJobHandle);

		if (pJobHandle->exitThread == 1) {
			elog(log_details, "ThreadType:%i.plg_JobThreadRouting.exitThread:%i", pJobHandle->threadType, pJobHandle->exitThread);
			break;
		} else if (pJobHandle->exitThread == 2) {

			elog(log_details, "ThreadType:%i.plg_JobThreadRouting.exitThread:%i", pJobHandle->threadType, pJobHandle->exitThread);
			void* pManage = pJobHandle->privateData;
			plg_JobDestoryHandle(pJobHandle);
			pthread_detach(pthread_self());
			plg_MngSendExit(pManage);
			plg_MutexThreadDestroy();
			return 0;
		}
	} while (1);

	plg_MutexThreadDestroy();
	pthread_detach(pthread_self());

	return 0;
}

int plg_jobStartRouting(void* pvJobHandle) {

	pthread_t pid;
	return pthread_create(&pid, NULL, plg_JobThreadRouting, pvJobHandle);
}

/*
Internal message pipeline usage of manage and file
*/
void plg_JobSendOrder(void* eQueue, char* order, char* value, short valueLen) {

	POrderPacket POrderPacket = malloc(sizeof(OrderPacket));
	POrderPacket->order = plg_sdsNew(order);
	POrderPacket->value = plg_sdsNewLen(value, valueLen);

	plg_eqPush(eQueue, POrderPacket);
}

void plg_JobAddAdmOrderProcess(void* pvJobHandle, char* nameOrder, void* pvProcess) {

	PEventPorcess process = pvProcess;
	PJobHandle pJobHandle = pvJobHandle;
	sds sdsOrder= plg_sdsNew(nameOrder);
	dictEntry * entry = plg_dictFind(pJobHandle->order_process, sdsOrder);
	if (entry == 0) {
		plg_listAddNodeHead(pJobHandle->userEvent, sdsOrder);
		plg_listAddNodeHead(pJobHandle->userProcess, process);
		plg_dictAdd(pJobHandle->order_process, sdsOrder, process);
	} else {
		assert(0);
		plg_sdsFree(sdsOrder);
		free(process);
	}
}

/*
Check the current running thread type to prevent users
Improper use of API in improper thread environment
*/
char plg_JobCheckIsType(enum ThreadType threadType) {

	PJobHandle pJobHandle = plg_LocksGetSpecific();
	if (pJobHandle == 0) {
		return 0;
	} else if (threadType == pJobHandle->threadType) {
		return 1;
	} else {
		return 0;
	}
}

char plg_JobCheckUsingThread() {

	if (1 == plg_JobCheckIsType(TT_OTHER)) {
		return 1;
	}
	return 0;
}

void plg_JobPrintStatus(void* pvJobHandle) {

	PJobHandle pJobHandle = pvJobHandle;
	printf("order_equeue:%d lcache:%d o_process:%d table_cache:%d aweight:%d uevent:%d uprocess:%d\n",
		dictSize(pJobHandle->order_equeue),
		dictSize(pJobHandle->dictCache),
		dictSize(pJobHandle->order_process),
		dictSize(pJobHandle->tableName_cacheHandle),
		pJobHandle->allWeight,
		listLength(pJobHandle->userEvent),
		listLength(pJobHandle->userProcess));
}

void plg_JobPrintDetails(void* pvJobHandle) {
	
	PJobHandle pJobHandle = pvJobHandle;
	printf("pJobHandle:%p %p\n", pJobHandle, pJobHandle->eQueue);
	printf("pJobHandle->tableName_cacheHandle>\n");
	dictIterator* dictIter = plg_dictGetSafeIterator(pJobHandle->tableName_cacheHandle);
	dictEntry* dictNode;
	while ((dictNode = plg_dictNext(dictIter)) != NULL) {
		printf("%s %p\n", (char*)dictGetKey(dictNode), dictGetVal(dictNode));
	}
	plg_dictReleaseIterator(dictIter);
	printf("<pJobHandle->tableName_cacheHandle\n");

	printf("pJobHandle->dictCache>\n");
	dictIter = plg_dictGetSafeIterator(pJobHandle->dictCache);
	while ((dictNode = plg_dictNext(dictIter)) != NULL) {
		printf("%p\n", dictGetKey(dictNode));
	}
	plg_dictReleaseIterator(dictIter);
	printf("<pJobHandle->dictCache\n");

	printf("pJobHandle->order_equeue>\n");
	dictIter = plg_dictGetSafeIterator(pJobHandle->order_equeue);
	while ((dictNode = plg_dictNext(dictIter)) != NULL) {
		printf("%s %p\n", (char*)dictGetKey(dictNode), dictGetVal(dictNode));
	}
	plg_dictReleaseIterator(dictIter);
	printf("<pJobHandle->order_equeue\n");
}

void plg_JobPrintOrder(void* pvJobHandle) {

	PJobHandle pJobHandle = pvJobHandle;
	printf("pJobHandle:%p %p\n", pJobHandle, pJobHandle->eQueue);

	dictIterator* dictIter = plg_dictGetSafeIterator(pJobHandle->order_equeue);
	dictEntry* dictNode;
	while ((dictNode = plg_dictNext(dictIter)) != NULL) {
		if (dictGetVal(dictNode) == pJobHandle->eQueue) {
			printf("%s %p\n", (char*)dictGetKey(dictNode), dictGetVal(dictNode));
		}
	}
	plg_dictReleaseIterator(dictIter);
}


/*
First check the running cache
*/
unsigned int plg_JobSet(void* table, unsigned short tableLen, void* key, unsigned short keyLen, void* value, unsigned int valueLen) {

	CheckUsingThread(0);
	unsigned int r = 0;
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKye = plg_sdsNewLen(key, keyLen);
	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		if (job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry))) {
			r = plg_CacheTableAdd(dictGetVal(valueEntry), sdsTable, sdsKye, value, valueLen);
			if (r) {
				plg_listAddNodeHead(pJobHandle->tranCache, dictGetVal(valueEntry));
			}
		} else {
			elog(log_error, "plg_JobSet.No permission to table <%s>!", sdsTable);
		}

	} else {
		elog(log_error, "plg_JobSet.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKye);

	return r;
}

/*
Get set type will fail
*/
void* plg_JobGet(void* table, unsigned short tableLen, void* key, unsigned short keyLen, unsigned int* valueLen) {

	CheckUsingThread(0);
	void* ptr = 0;
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKye = plg_sdsNewLen(key, keyLen);
	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		void* pDictExten = plg_DictExtenCreate();
		if (0 <= plg_CacheTableFind(dictGetVal(valueEntry), sdsTable, sdsKye, pDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)))) {
			if (plg_DictExtenSize(pDictExten)) {
				void* entry = plg_DictExtenGetHead(pDictExten);
				void* valuePtr = plg_DictExtenValue(entry, valueLen);
				if (*valueLen) {
					ptr = malloc(*valueLen);
					memcpy(ptr, valuePtr, *valueLen);
				}
			}
			
		} else {
			elog(log_error, "plg_JobGet.Serious error in search operation!");
		}
		plg_DictExtenDestroy(pDictExten);
	} else {
		elog(log_error, "plg_JobGet.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKye);

	return ptr;
}

unsigned int plg_JobDel(void* table, unsigned short tableLen, void* key, unsigned short keyLen) {

	CheckUsingThread(0);
	unsigned int r = 0;
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKye = plg_sdsNewLen(key, keyLen);
	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		if (job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry))) {
			r = plg_CacheTableDel(dictGetVal(valueEntry), sdsTable, sdsKye);
			if (r) {
				plg_listAddNodeHead(pJobHandle->tranCache, dictGetVal(valueEntry));
			}
		} else {
			elog(log_error, "plg_JobDel.No permission to table <%s>!", sdsTable);
		}
	} else {
		elog(log_error, "plg_JobDel.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKye);

	return r;
}

unsigned int plg_JobLength(void* table, unsigned short tableLen) {

	unsigned int len = 0;
	CheckUsingThread(0);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		len = plg_CacheTableLength(dictGetVal(valueEntry), sdsTable, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobLength.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);

	return len;
}

unsigned int plg_JobSIfNoExit(void* table, unsigned short tableLen, void* key, unsigned short keyLen, void* value, unsigned int valueLen) {

	CheckUsingThread(0);
	unsigned int r = 0;
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKey = plg_sdsNewLen(key, keyLen);
	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		if (job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry))) {
			r = plg_CacheTableAddIfNoExist(dictGetVal(valueEntry), sdsTable, sdsKey, value, valueLen);
			if (r) {
				plg_listAddNodeHead(pJobHandle->tranCache, dictGetVal(valueEntry));
			}
		} else {
			elog(log_error, "plg_JobSIfNoExit.No permission to table <%s>!", sdsTable);
		}
	} else {
		elog(log_error, "plg_JobSIfNoExit.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKey);

	return r;
}

unsigned int plg_JobIsKeyExist(void* table, unsigned short tableLen, void* key, unsigned short keyLen) {

	CheckUsingThread(0);
	unsigned int r = 0;
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKye = plg_sdsNewLen(key, keyLen);
	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		r = plg_CacheTableIsKeyExist(dictGetVal(valueEntry), sdsTable, sdsKye, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobIsKeyExist.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKye);

	return r;
}

unsigned int plg_JobRename(void* table, unsigned short tableLen, void* key, unsigned short keyLen, void* newKey, unsigned short newKeyLen) {

	CheckUsingThread(0);
	unsigned int r = 0;
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKye = plg_sdsNewLen(key, keyLen);
	sds sdsNewKye = plg_sdsNewLen(newKey, newKeyLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		if (job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry))) {
			r = plg_CacheTableRename(dictGetVal(valueEntry), sdsTable, sdsKye, sdsNewKye);
			if (r) {
				plg_listAddNodeHead(pJobHandle->tranCache, dictGetVal(valueEntry));
			}
		} else {
			elog(log_error, "plg_JobRename.No permission to table <%s>!", sdsTable);
		}
	} else {
		elog(log_error, "plg_JobRename.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKye);
	plg_sdsFree(sdsNewKye);

	return r;
}

void plg_JobLimite(void* table, unsigned short tableLen, void* key, unsigned short keyLen, unsigned int left, unsigned int right, void* pDictExten) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKye = plg_sdsNewLen(key, keyLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		plg_CacheTableLimite(dictGetVal(valueEntry), sdsTable, sdsKye, left, right, pDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobLimite.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKye);
}

void plg_JobOrder(void* table, unsigned short tableLen, short order, unsigned int limite, void* pDictExten) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		plg_CacheTableOrder(dictGetVal(valueEntry), sdsTable, order, limite, pDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobOrder.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
}

void plg_JobRang(void* table, unsigned short tableLen, void* beginKey, unsigned short beginKeyLen, void* endKey, unsigned short endKeyLen, void* pDictExten) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsBeginKey = plg_sdsNewLen(beginKey, beginKeyLen);
	sds sdsEndKey = plg_sdsNewLen(endKey, endKeyLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		plg_CacheTableRang(dictGetVal(valueEntry), sdsTable, sdsBeginKey, sdsEndKey, pDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobRang.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsBeginKey);
	plg_sdsFree(sdsEndKey);
}

void plg_JobPattern(void* table, unsigned short tableLen, void* beginKey, unsigned short beginKeyLen, void* endKey, unsigned short endKeyLen, char* pattern, unsigned short patternLen, void* pDictExten) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsBeginKey = plg_sdsNewLen(beginKey, beginKeyLen);
	sds sdsEndKey = plg_sdsNewLen(endKey, endKeyLen);
	sds sdsPattern = plg_sdsNewLen(pattern, patternLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		plg_CacheTablePattern(dictGetVal(valueEntry), sdsTable, sdsBeginKey, sdsEndKey, sdsPattern, pDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobPattern.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsBeginKey);
	plg_sdsFree(sdsEndKey);
	plg_sdsFree(sdsPattern);
}

/*
First check the running cache
*/
unsigned int plg_JobMultiSet(void* table, unsigned short tableLen, void* pDictExten) {

	unsigned int r = 0;
	CheckUsingThread(0);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		if (job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry))) {
			r = plg_CacheTableMultiAdd(dictGetVal(valueEntry), sdsTable, pDictExten);
			if (r) {
				plg_listAddNodeHead(pJobHandle->tranCache, dictGetVal(valueEntry));
			}
		} else {
			elog(log_error, "plg_JobMultiSet.No permission to table <%s>!", sdsTable);
		}
	} else {
		elog(log_error, "plg_JobMultiSet.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	return r;
}

void plg_JobMultiGet(void* table, unsigned short tableLen, void* pKeyDictExten, void* pValueDictExten) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		plg_CacheTableMultiFind(dictGetVal(valueEntry), sdsTable, pKeyDictExten, pValueDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobMultiGet.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
}

void* plg_JobRand(void* table, unsigned short tableLen, unsigned int* valueLen) {

	CheckUsingThread(0);
	void* ptr = 0;
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {

		void* pDictExten = plg_DictExtenCreate();
		if (1 <= plg_CacheTableRand(dictGetVal(valueEntry), sdsTable, pDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)))) {
			if (plg_DictExtenSize(pDictExten)) {
				void* entry = plg_DictExtenGetHead(pDictExten);
				void* valuePtr = plg_DictExtenValue(entry, valueLen);
				if (*valueLen) {
					ptr = malloc(*valueLen);
					memcpy(ptr, valuePtr, *valueLen);
				}
			}

		} else {
			elog(log_error, "plg_JobRand.Serious error in search operation!");
		}
		plg_DictExtenDestroy(pDictExten);
	} else {
		elog(log_error, "plg_JobRand.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);

	return ptr;
}

void plg_JobTableClear(void* table, unsigned short tableLen) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		if (job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry))) {
			plg_CacheTableClear(dictGetVal(valueEntry), sdsTable);
			plg_listAddNodeHead(pJobHandle->tranCache, dictGetVal(valueEntry));
		} else {
			elog(log_error, "plg_JobTableClear.No permission to table <%s>!", sdsTable);
		}
	} else {
		elog(log_error, "plg_JobTableClear.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
}

unsigned int plg_JobSAdd(void* table, unsigned short tableLen, void* key, unsigned short keyLen, void* value, unsigned int valueLen) {

	CheckUsingThread(0);
	unsigned int r = 0;
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKye = plg_sdsNewLen(key, keyLen);
	sds sdsValue = plg_sdsNewLen(value, valueLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		if (job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry))) {
			r = plg_CacheTableSetAdd(dictGetVal(valueEntry), sdsTable, sdsKye, sdsValue);
			if (r) {
				plg_listAddNodeHead(pJobHandle->tranCache, dictGetVal(valueEntry));
			}
		} else {
			elog(log_error, "plg_JobSAdd.No permission to table <%s>!", sdsTable);
		}
	} else {
		elog(log_error, "plg_JobSAdd.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKye);
	plg_sdsFree(sdsValue);

	return r;
}

void plg_JobSRang(void* table, unsigned short tableLen, void* key, unsigned short keyLen, void* beginValue, unsigned short beginValueLen, void* endValue, unsigned short endValueLen, void* pDictExten) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKey = plg_sdsNewLen(key, keyLen);
	sds sdsBeginKey = plg_sdsNewLen(beginValue, beginValueLen);
	sds sdsEndKey = plg_sdsNewLen(endValue, endValueLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		plg_CacheTableSetRang(dictGetVal(valueEntry), sdsTable, sdsKey, sdsBeginKey, sdsEndKey, pDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobSRang.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKey);
	plg_sdsFree(sdsBeginKey);
	plg_sdsFree(sdsEndKey);
}

void plg_JobSLimite(void* table, unsigned short tableLen, void* key, unsigned short keyLen, void* value, unsigned short valueLen, unsigned int left, unsigned int right, void* pDictExten) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKey = plg_sdsNewLen(key, keyLen);
	sds sdsValue = plg_sdsNewLen(value, valueLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		plg_CacheTableSetLimite(dictGetVal(valueEntry), sdsTable, sdsKey, sdsValue, left, right, pDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobSLimite.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKey);
	plg_sdsFree(sdsValue);
}

unsigned int plg_JobSLength(void* table, unsigned short tableLen, void* key, unsigned short keyLen) {

	unsigned int len = 0;
	CheckUsingThread(0);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKey = plg_sdsNewLen(key, keyLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		len = plg_CacheTableSetLength(dictGetVal(valueEntry), sdsTable, sdsKey, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobSLength.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKey);

	return len;
}

unsigned int plg_JobSIsKeyExist(void* table, unsigned short tableLen, void* key, unsigned short keyLen, void* value, unsigned short valueLen) {

	CheckUsingThread(0);
	unsigned int r = 0;
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKey = plg_sdsNewLen(key, keyLen);
	sds sdsValue = plg_sdsNewLen(value, valueLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		r = plg_CacheTableSetIsKeyExist(dictGetVal(valueEntry), sdsTable, sdsKey, sdsValue, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobSIsKeyExist.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKey);
	plg_sdsFree(sdsValue);

	return r;
}

void plg_JobSMembers(void* table, unsigned short tableLen, void* key, unsigned short keyLen, void* pDictExten) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKey = plg_sdsNewLen(key, keyLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		plg_CacheTableSetMembers(dictGetVal(valueEntry), sdsTable, sdsKey, pDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobSMembers.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKey);
}

void* plg_JobSRand(void* table, unsigned short tableLen, void* key, unsigned short keyLen, unsigned int* valueLen) {

	CheckUsingThread(0);
	void* ptr = 0;
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKey = plg_sdsNewLen(key, keyLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {

		void* pDictExten = plg_DictExtenCreate();
		if (1 <= plg_CacheTableSetRand(dictGetVal(valueEntry), sdsTable, sdsKey, pDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)))) {
			if (plg_DictExtenSize(pDictExten)) {
				void* entry = plg_DictExtenGetHead(pDictExten);
				void* keyPtr = plg_DictExtenKey(entry, valueLen);
				if (*valueLen) {
					ptr = malloc(*valueLen);
					memcpy(ptr, keyPtr, *valueLen);
				}
			}

		} else {
			elog(log_error, "plg_JobSRand.Serious error in search operation!");
		}
		plg_DictExtenDestroy(pDictExten);
	} else {
		elog(log_error, "plg_JobSRand.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKey);

	return ptr;
}

void plg_JobSDel(void* table, unsigned short tableLen, void* key, unsigned short keyLen, void* pValueDictExten) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKey = plg_sdsNewLen(key, keyLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		if (job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry))) {
			plg_CacheTableSetDel(dictGetVal(valueEntry), sdsTable, sdsKey, pValueDictExten);
			plg_listAddNodeHead(pJobHandle->tranCache, dictGetVal(valueEntry));
		} else {
			elog(log_error, "plg_JobSDel.No permission to table <%s>!", sdsTable);
		}
	} else {
		elog(log_error, "plg_JobSDel.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKey);
}

void* plg_JobSPop(void* table, unsigned short tableLen, void* key, unsigned short keyLen, unsigned int* valueLen) {

	CheckUsingThread(0);
	void* ptr = 0;
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKey = plg_sdsNewLen(key, keyLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {

		void* pDictExten = plg_DictExtenCreate();
		if (1 <= plg_CacheTableSetPop(dictGetVal(valueEntry), sdsTable, sdsKey, pDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)))) {
			if (plg_DictExtenSize(pDictExten)) {
				void* entry = plg_DictExtenGetHead(pDictExten);
				void* valuePtr = plg_DictExtenKey(entry, valueLen);
				if (*valueLen) {
					ptr = malloc(*valueLen);
					memcpy(ptr, valuePtr, *valueLen);
				}
			}

		} else {
			elog(log_error, "plg_JobSPop.Serious error in search operation!");
		}
		plg_DictExtenDestroy(pDictExten);
	} else {
		elog(log_error, "plg_JobSPop.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKey);

	return ptr;

}

unsigned int plg_JobSRangCount(void* table, unsigned short tableLen, void* key, unsigned short keyLen, void* beginValue, unsigned short beginValueLen, void* endValue, unsigned short endValueLen) {

	CheckUsingThread(0);
	unsigned int r = 0;
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKey = plg_sdsNewLen(key, keyLen);
	sds sdsBeginKey = plg_sdsNewLen(beginValue, beginValueLen);
	sds sdsEndKey = plg_sdsNewLen(endValue, endValueLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		r = plg_CacheTableSetRangCount(dictGetVal(valueEntry), sdsTable, sdsKey, sdsBeginKey, sdsEndKey, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobSRangCount.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKey);
	plg_sdsFree(sdsBeginKey);
	plg_sdsFree(sdsEndKey);

	return r;
}

void plg_JobSUion(void* table, unsigned short tableLen, void* pSetDictExten, void* pKeyDictExten) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		plg_CacheTableSetUion(dictGetVal(valueEntry), sdsTable, pSetDictExten, pKeyDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobSUion.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
}

void plg_JobSUionStore(void* table, unsigned short tableLen, void* pSetDictExten, void* key, unsigned short keyLen) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKye = plg_sdsNewLen(key, keyLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		if (job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry))) {
			plg_CacheTableSetUionStore(dictGetVal(valueEntry), sdsTable, pSetDictExten, sdsKye);
			plg_listAddNodeHead(pJobHandle->tranCache, dictGetVal(valueEntry));
		} else {
			elog(log_error, "plg_JobSUionStore.No permission to table <%s>!", sdsTable);
		}
	} else {
		elog(log_error, "plg_JobSUionStore.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKye);
}

void plg_JobSInter(void* table, unsigned short tableLen, void* pSetDictExten, void* pKeyDictExten) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		plg_CacheTableSetInter(dictGetVal(valueEntry), sdsTable, pSetDictExten, pKeyDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobSInter.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
}

void plg_JobSInterStore(void* table, unsigned short tableLen, void* pSetDictExten, void* key, unsigned short keyLen) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKye = plg_sdsNewLen(key, keyLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		if (job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry))) {
			plg_CacheTableSetInterStore(dictGetVal(valueEntry), sdsTable, pSetDictExten, sdsKye);
			plg_listAddNodeHead(pJobHandle->tranCache, dictGetVal(valueEntry));
		} else {
			elog(log_error, "plg_JobSInterStore.No permission to table <%s>!", sdsTable);
		}	
	} else {
		elog(log_error, "plg_JobSInterStore.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKye);
}

void plg_JobSDiff(void* table, unsigned short tableLen, void* pSetDictExten, void* pKeyDictExten) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		plg_CacheTableSetDiff(dictGetVal(valueEntry), sdsTable, pSetDictExten, pKeyDictExten, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobSDiff.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
}

void plg_JobSDiffStore(void* table, unsigned short tableLen, void* pSetDictExten, void* key, unsigned short keyLen) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsKye = plg_sdsNewLen(key, keyLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		if (job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry))) {
			plg_CacheTableSetDiffStore(dictGetVal(valueEntry), sdsTable, pSetDictExten, sdsKye);
			plg_listAddNodeHead(pJobHandle->tranCache, dictGetVal(valueEntry));
		} else {
			elog(log_error, "plg_JobSDiffStore.No permission to table <%s>!", sdsTable);
		}
	} else {
		elog(log_error, "plg_JobSDiffStore.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsKye);
}

void plg_JobSMove(void* table, unsigned short tableLen, void* srcKey, unsigned short srcKeyLen, void* desKey, unsigned short desKeyLen, void* value, unsigned short valueLen) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);
	sds sdsSrcKye = plg_sdsNewLen(srcKey, srcKeyLen);
	sds sdsDesKye = plg_sdsNewLen(desKey, desKeyLen);
	sds sdsValue = plg_sdsNewLen(value, valueLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		if (job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry))) {
			plg_CacheTableSetMove(dictGetVal(valueEntry), sdsTable, sdsSrcKye, sdsDesKye, sdsValue);
			plg_listAddNodeHead(pJobHandle->tranCache, dictGetVal(valueEntry));
		} else {
			elog(log_error, "plg_JobSMove.No permission to table <%s>!", sdsTable);
		}
	} else {
		elog(log_error, "plg_JobSMove.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
	plg_sdsFree(sdsSrcKye);
	plg_sdsFree(sdsDesKye);
	plg_sdsFree(sdsValue);
}


void plg_JobTableMembersWithJson(void* table, unsigned short tableLen, void* jsonRoot) {

	CheckUsingThread(NORET);
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	sds sdsTable = plg_sdsNewLen(table, tableLen);

	dictEntry* valueEntry = plg_dictFind(pJobHandle->tableName_cacheHandle, sdsTable);
	if (valueEntry != 0) {
		plg_CacheTableMembersWithJson(dictGetVal(valueEntry), sdsTable, jsonRoot, job_IsCacheAllowWrite(pJobHandle, dictGetKey(valueEntry)));
	} else {
		elog(log_error, "plg_JobSDiff.Cannot access table <%s>!", sdsTable);
	}
	plg_sdsFree(sdsTable);
}

char* plg_JobCurrentOrder(short* orderLen) {
	CheckUsingThread(0);
	PJobHandle pJobHandle = plg_LocksGetSpecific();

	*orderLen = plg_sdsLen(pJobHandle->pOrderName);
	return pJobHandle->pOrderName;
}

void plg_JobAddTimer(unsigned int timer, void* order, unsigned short orderLen, void* value, unsigned short valueLen) {
	CheckUsingThread(NORET);

	unsigned long long sec = plg_GetCurrentSec();
	PJobHandle pJobHandle = plg_LocksGetSpecific();
	PIntervalometer pPIntervalometer = malloc(sizeof(Intervalometer));
	pPIntervalometer->Order = plg_sdsNewLen(order, orderLen);
	pPIntervalometer->Value = plg_sdsNewLen(value, valueLen);
	pPIntervalometer->tim = sec + timer;

	plg_listAddNodeHead(pJobHandle->pListIntervalometer, pPIntervalometer);
}

#undef NORET
#undef CheckUsingThread