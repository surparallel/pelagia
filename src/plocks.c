/* locks.c - Lock record of critical point
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
#include "padlist.h"
#include "pelog.h"
#include "plocks.h"

typedef struct _SafeMutex
{
	unsigned int rank;
	pthread_mutex_t lock;
}*PSafeMutex, SafeMutex;

static pthread_key_t exclusionZone = 0;
static pthread_key_t environmental = 0;
static pthread_key_t logfile = 0;

void plg_LocksCreate() {
	if (0 != pthread_key_create(&exclusionZone, NULL)) {
		plg_assert(0);
	}
	if (0 != pthread_key_create(&environmental, NULL)) {
		plg_assert(0);
	}
	if (0 != pthread_key_create(&logfile, NULL)) {
		plg_assert(0);
	}
}

void plg_LocksDestroy() {
	pthread_key_delete(exclusionZone);
	pthread_key_delete(environmental);
	pthread_key_delete(logfile);
}

char plg_LocksEntry(void* pvSafeMutex) {

	PSafeMutex pSafeMutex = pvSafeMutex;
	//Currently internal thread needs to check lock status
	if (plg_LocksGetSpecific() != 0) {
		plg_assert(exclusionZone);
		list* ptr = pthread_getspecific(exclusionZone);
		//Current non lock state enters lock state creation
		if (ptr == 0) {
			ptr = plg_listCreate(LIST_MIDDLE);
			plg_listAddNodeHead(ptr, pSafeMutex);
			pthread_setspecific(exclusionZone, ptr);
			return 1;
		} else {
			listNode *node = listFirst(ptr);
			if (node != 0) {
				PSafeMutex pHeadMutex = listNodeValue(node);

				//Access is allowed only when the permission is high
				if (pSafeMutex->rank <= pHeadMutex->rank) {
					return 0;
				}
			}
			plg_listAddNodeHead(ptr, pSafeMutex);
			return 1;
		}
	} else {
		return 1;
	}
}

char plg_LocksLeave(void* pvSafeMutex) {

	PSafeMutex pSafeMutex = pvSafeMutex;
	//Currently internal thread needs to check lock status
	if (plg_LocksGetSpecific() != 0) {
		plg_assert(exclusionZone);
		list* ptr = pthread_getspecific(exclusionZone);
		
		//Repeated release will result in a critical error
		if (ptr != 0) {
			listNode *node = listFirst(ptr);
			if (node != 0) {

				PSafeMutex pHeadMutex = listNodeValue(node);
				//Lock not released in pairs, serious error reported
				if (pHeadMutex != pSafeMutex) {
					elog(log_error, "plg_LocksLeave.lock lose");
					return 0;
				}
				plg_listDelNode(ptr, node);
				return 1;
			}
			return 0;
		} else {
			return 0;
		}
	} else {
		return 1;
	}
}

void plg_LocksSetSpecific(void* ptr) {
	plg_assert(environmental);
	pthread_setspecific(environmental, ptr);
}

void* plg_LocksGetSpecific() {
	if (!environmental) {
		return 0;
	}
	void* ptr = pthread_getspecific(environmental);
	return ptr;
}

void plg_LocksSetLogFile(void* ptr) {
	plg_assert(logfile);
	pthread_setspecific(logfile, ptr);
}

void* plg_LocksGetLogFile() {
	plg_assert(logfile);
	return pthread_getspecific(logfile);
}

void* plg_MutexCreateHandle(unsigned int rank) {
	PSafeMutex pSafeMutex = malloc(sizeof(SafeMutex));
	pSafeMutex->rank = rank;
	pthread_mutex_init(&pSafeMutex->lock, 0);
	return pSafeMutex;
}

void plg_MutexThreadDestroy() {
	plg_assert(exclusionZone);
	list* ptr = pthread_getspecific(exclusionZone);
	if (ptr != 0) {
		plg_listRelease(ptr);
		pthread_setspecific(exclusionZone, 0);
	}
}
void plg_MutexDestroyHandle(void* pvSafeMutex) {
	PSafeMutex pSafeMutex = pvSafeMutex;
	pthread_mutex_destroy(&pSafeMutex->lock);
	free(pSafeMutex);
}

int plg_MutexLock(void* pvSafeMutex) {
	PSafeMutex pSafeMutex = pvSafeMutex;
	return pthread_mutex_lock(&pSafeMutex->lock);
}

int plg_MutexUnlock(void* pvSafeMutex) {
	PSafeMutex pSafeMutex = pvSafeMutex;
	return pthread_mutex_unlock(&pSafeMutex->lock);
}