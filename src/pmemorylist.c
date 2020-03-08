/* memorylist.c - Specific memory manager
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
#include "pmemorylist.h"
#include "plateform.h"
#include "plocks.h"
#include "pelog.h"
#include "psds.h"
#include "pdict.h"
#include "ptimesys.h"

typedef struct _MemoryListNode
{
	unsigned long long stamp;
	void* next;
}*PMemoryListNode, MemoryListNode;

typedef struct _MemoryListHandle
{
	PMemoryListNode head;
	dict* dictPop;
	unsigned int sec;
	unsigned int size;
	unsigned int length;
	unsigned int lastFree;
	unsigned char isLock;
	unsigned int percent;
	void* mutexLock;
	sds objName;
}*PMemoryListHandle, MemoryListHandle;

void* plg_MemListCreate(unsigned int sec, unsigned int size, unsigned char isLock) {
	PMemoryListHandle pMemoryListHandle = malloc(sizeof(MemoryListHandle));
	pMemoryListHandle->sec = sec;
	pMemoryListHandle->percent = 5;
	pMemoryListHandle->size = size;
	pMemoryListHandle->lastFree = 0;
	pMemoryListHandle->isLock = isLock;
	pMemoryListHandle->head = 0;
	pMemoryListHandle->dictPop = plg_dictCreate(plg_DefaultPtrDictPtr(), NULL, DICT_MIDDLE);
	pMemoryListHandle->length = 0;
	if (pMemoryListHandle->isLock) {
		pMemoryListHandle->mutexLock = plg_MutexCreateHandle(LockLevel_4);
	}
	pMemoryListHandle->objName = plg_sdsNew("MemoryList");
	return pMemoryListHandle;
}

/*
Messages that are not processed in the queue will be discarded, including pages that are not saved.
If the program exits abnormally, the data will be lost.
*/
void plg_MemListDestory(void* pvMemoryListHandle) {

	PMemoryListHandle pMemoryListHandle = pvMemoryListHandle;
	plg_sdsFree(pMemoryListHandle->objName);
	if (pMemoryListHandle->isLock) {
		plg_MutexDestroyHandle(pMemoryListHandle->mutexLock);
	}

	PMemoryListNode node = pMemoryListHandle->head;
	do {
		if (node == 0) {
			break;
		}
		PMemoryListNode next = node->next;
		free(node);
		node = next;
	} while (1);

	plg_dictRelease(pMemoryListHandle->dictPop);
	free(pMemoryListHandle);
}

void plg_MemListPush(void* pvMemoryListHandle, void* ptr) {

	PMemoryListHandle pMemoryListHandle = pvMemoryListHandle;
	if (pMemoryListHandle->isLock) {
		MutexLock(pMemoryListHandle->mutexLock, pMemoryListHandle->objName);
	}
	unsigned long long stamp = plg_GetCurrentSec();

	//add to hesdlist
	PMemoryListNode pMemoryListNode = (PMemoryListNode)((char*)ptr - sizeof(MemoryListNode));
	dictEntry* entry = plg_dictFind(pMemoryListHandle->dictPop, pMemoryListNode);
	assert(entry);

	pMemoryListNode->stamp = stamp;
	pMemoryListNode->next = pMemoryListHandle->head;
	pMemoryListHandle->head = pMemoryListNode;
	pMemoryListHandle->length += 1;
	plg_dictDelete(pMemoryListHandle->dictPop, pMemoryListNode);

	//check time for del
	if ((stamp - pMemoryListHandle->lastFree) > pMemoryListHandle->sec) {
		pMemoryListHandle->lastFree = stamp;
		unsigned int freeMemCount = pMemoryListHandle->length * pMemoryListHandle->percent / 100;
		unsigned int limite = pMemoryListHandle->sec * 3;
		unsigned int count = 0;
		PMemoryListNode node = pMemoryListHandle->head;
		PMemoryListNode prev = 0;
		do {
			if (node == 0) {
				break;
			}
			if (stamp - node->stamp < limite) {
				prev = node;
				node = node->next;
				continue;
			}

			if (prev == 0) {
				pMemoryListHandle->head = node->next;
				free(node);
				node = pMemoryListHandle->head;
			} else {
				prev->next = node->next;
				free(node);
				node = prev->next;
			}

			pMemoryListHandle->length -= 1;
			if (++count > freeMemCount) {
				break;
			}
		} while (1);
	}

	if (pMemoryListHandle->isLock) {
		MutexUnlock(pMemoryListHandle->mutexLock, pMemoryListHandle->objName);
	}
}

void* plg_MemListPop(void* pvMemoryListHandle) {

	PMemoryListHandle pMemoryListHandle = pvMemoryListHandle;
	if (pMemoryListHandle->isLock) {
		MutexLock(pMemoryListHandle->mutexLock, pMemoryListHandle->objName);
	}

	PMemoryListNode pMemoryListNode;
	if (pMemoryListHandle->length > 0) {
		pMemoryListNode = pMemoryListHandle->head;
		pMemoryListHandle->head = pMemoryListHandle->head->next;
		pMemoryListHandle->length -= 1;
	} else {
		pMemoryListNode = malloc(pMemoryListHandle->size + sizeof(MemoryListNode));	
		pMemoryListNode->next = 0;
	}
	plg_dictAdd(pMemoryListHandle->dictPop, pMemoryListNode, NULL);

	if (pMemoryListHandle->isLock) {
		MutexUnlock(pMemoryListHandle->mutexLock, pMemoryListHandle->objName);
	}
	
	return ((char*)pMemoryListNode + sizeof(MemoryListNode));
}

