/* event.c - Users create, receive and send messages
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
#include "pjob.h"
#include "pequeue.h"
#include "psds.h"
#include "pelagia.h"

void plg_eventFree(void* ptr) {
	plg_sdsFree(ptr);
}

void* plg_EventCreateHandle() {
	return plg_eqCreate();
}

void plg_EventDestroyHandle(void* pEventHandle) {
	plg_eqDestory(pEventHandle, plg_eventFree);
}

void plg_EventSend(void* pEventHandle, const char* value, unsigned int valueLen) {
	sds sdsvalue = plg_sdsNewLen(value, valueLen);
	plg_eqPush(pEventHandle, sdsvalue);
}

void plg_EventSendWithMax(void* pEventHandle, const char* value, unsigned int valueLen, unsigned int maxQueue) {
	sds sdsvalue = plg_sdsNewLen(value, valueLen);
	plg_eqIfNoPush(pEventHandle, sdsvalue, maxQueue);
}

int plg_EventTimeWait(void* pEventHandle, long long sec, int nsec) {
	return plg_eqTimeWait(pEventHandle, sec, nsec);
}

int plg_EventWait(void* pEventHandle) {
	return plg_eqWait(pEventHandle);
}

void* plg_EventRecvAlloc(void* pEventHandle, unsigned int* valueLen) {
	sds sdsvalue = plg_eqPop(pEventHandle);
	if (sdsvalue) {
		*valueLen = plg_sdsLen(sdsvalue);
		return sdsvalue;
	} else {
		*valueLen = 0;
	}
	return sdsvalue;
}

void plg_EventFreePtr(void* ptr) {
	plg_sdsFree(ptr);
}