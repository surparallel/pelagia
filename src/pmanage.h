/* manage.h - Global manager
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
#ifndef __MANAGE_H
#define __MANAGE_H

//user manage API for test
//Internal API
typedef void(*AfterDestroyFun)(void* value);
char* plg_MngGetDBPath(void* pvManage);
void plg_MngAddUserEvent(void* pvManage, char* nevent, short neventLen, void* equeue);
void* plg_MngJobHandle(void* pvManage);
char plg_MngCheckUsingThread();
int plg_MngSetTableParent(void* pvManage, char* nameTable, short nameTableLen, char* parent, short parentLen);
int plg_MngInterAllocJob(void* pvManage, unsigned int core, char* fileName);

void plg_MngOutJson(char* fileName, char* outJson);
void plg_MngFromJson(char* fromJson);
void plg_MngSendExit(void* pvManage);
int plg_MngTableIsInOrder(void* pvManage, void* order, short orderLen, void* table, short tableLen);
char** plg_MngOrderAllTable(void* pvManage, void* order, short orderLen, short* tableLen);
char* plg_MngOrderAllTableWithJson(void* pvManage, void* order, short orderLen);
int plg_MngRemoteCallPacket(void* pvManage, void* pvOrderPacket, char** order);
void* plg_MngGetProcess(void* pvManage, char* sdsOrder, char** retSdsOrder);
#endif