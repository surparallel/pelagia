/* job.h - Thread related functions
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

#ifndef __JOB_H
#define __JOB_H

enum ThreadType {
	TT_OTHER = 0,
	TT_MANAGE = 1,
	TT_PROCESS = 2,
	TT_NET = 3,
	TT_FILE = 4
};

void plg_JobProcessDestory(void* pEventPorcess);
void* plg_JobCreateHandle(void* pManage, enum ThreadType threadType, char* luaLIBPath, short luaHot);
void plg_JobDestoryHandle(void* pJobHandle);
unsigned char plg_JobFindTableName(void* pJobHandle, char* tableName);
void plg_JobAddEventEqueue(void* pJobHandle, char* nevent, void* equeue);
void plg_JobAddEventProcess(void* pJobHandle, char* nevent, void* process);
void* plg_JobNewTableCache(void* pJobHandle, char* table, void* pDiskHandle);
void plg_JobAddTableCache(void* pJobHandle, char* table, void* pCacheHandle);
void* plg_JobEqueueHandle(void* pJobHandle);
unsigned int plg_JobAllWeight(void* pJobHandle);
unsigned int  plg_JobIsEmpty(void* pJobHandle);
void plg_JobSendOrder(void* eQueue, char* order, char* value, short valueLen);
void plg_JobAddAdmOrderProcess(void* pJobHandle, char* nevent, void* process);
char plg_JobCheckIsType(enum ThreadType threadType);
char plg_JobCheckUsingThread();
char* plg_TT2String(unsigned short tt);

void plg_JobPrintStatus(void* pJobHandle, void* vJson);
void plg_JobPrintDetails(void* pJobHandle, void* vJson);
void plg_JobPrintOrder(void* pvJobHandle, void* vJson);

//Operating system interface
void* job_Handle();
unsigned int job_MaxQueue();
void plg_JobSetExitThread(char value);
void* job_ManageEqueue();
void plg_JobSetPrivate(void* pJobHandle, void* privateData);
void* plg_JobGetPrivate();
char* plg_JobTableNameWithJson();
void plg_JobTableMembersWithJson(void* table, short tableLen, void* jsonRoot);
int plg_JobStartRouting(void* pvJobHandle);
void plg_JobSetStat(void* pvJobHandle, short stat, unsigned long long checkTime);
void plg_JobSetMaxQueue(void* pvJobHandle, unsigned int maxQueue);

#endif