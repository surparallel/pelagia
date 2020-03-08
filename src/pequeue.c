/* equeue.c 
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
#include "padlist.h"
#include "pelog.h"
#include "pequeue.h"
#include "psds.h"
#include "plocks.h"

#ifdef __APPLE__
#include "psemaphore.h"
#else
#include <semaphore.h>
#endif

typedef struct _EventQueue
{
	void* mutexHandle;
	sds objecName;
	sem_t semaphore;
	list* listQueue;
} *PEventQueue, EventQueue;


void* plg_eqCreate() {
	PEventQueue pEventQueue = malloc(sizeof(EventQueue));
	pEventQueue->mutexHandle = plg_MutexCreateHandle(LockLevel_4);

	if (sem_init(&pEventQueue->semaphore, PTHREAD_PROCESS_PRIVATE, 0) != 0) {
		free(pEventQueue);
		elog(log_error, "semaphore init failut!");
		return 0;
	}
	pEventQueue->listQueue = plg_listCreate(LIST_MIDDLE);
	pEventQueue->objecName = plg_sdsNew("equeue");
	return pEventQueue;
}

void plg_eqPush(void* pvEventQueue, void* value) {

	PEventQueue pEventQueue = pvEventQueue;
	MutexLock(pEventQueue->mutexHandle, pEventQueue->objecName);
	plg_listAddNodeHead(pEventQueue->listQueue, value);
	MutexUnlock(pEventQueue->mutexHandle, pEventQueue->objecName);

	if (sem_post(&pEventQueue->semaphore) != 0) {
		elog(log_error, "semaphore post failut!");
		return;
	}
}

int plg_eqTimeWait(void* pvEventQueue, long long sec, int nsec) {

	PEventQueue pEventQueue = pvEventQueue;
	struct timespec ts;
	ts.tv_sec = sec;
	ts.tv_nsec = nsec;
	return sem_timedwait(&pEventQueue->semaphore, &ts);
}

int plg_eqWait(void* pvEventQueue) {
	PEventQueue pEventQueue = pvEventQueue;
	return sem_wait(&pEventQueue->semaphore);
}

void* plg_eqPop(void* pvEventQueue) {
	PEventQueue pEventQueue = pvEventQueue;
	void* value = 0;
	MutexLock(pEventQueue->mutexHandle, pEventQueue->objecName);
	if (listLength(pEventQueue->listQueue) != 0) {
		listNode *node = listLast(pEventQueue->listQueue);
		value = listNodeValue(node);
		plg_listDelNode(pEventQueue->listQueue, node);
	}
	MutexUnlock(pEventQueue->mutexHandle, pEventQueue->objecName);
	return value;
}

void plg_eqDestory(void* pvEventQueue, QueuerDestroyFun fun) {
	PEventQueue pEventQueue = pvEventQueue;
	elog(log_fun, "plg_eqDestory:%U", pEventQueue);
	plg_sdsFree(pEventQueue->objecName);
	listSetFreeMethod(pEventQueue->listQueue, fun);
	plg_listRelease(pEventQueue->listQueue);

	sem_destroy(&pEventQueue->semaphore);
	plg_MutexDestroyHandle(pEventQueue->mutexHandle);
	free(pEventQueue);
}