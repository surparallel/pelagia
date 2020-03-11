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
char* plg_MngGetDBPath(void* pManage);
void plg_MngAddUserEvent(void* pManage, char* nevent, short neventLen, void* equeue);
void* plg_MngJobHandle(void* pManage);
char plg_MngCheckUsingThread();
int plg_MngSetTableParent(void* pManage, char* nameTable, short nameTableLen, char* parent, short parentLen);
void plg_MngPrintAllDetails(void* pManage);
int plg_MngInterAllocJob(void* pManage, unsigned int core, char* fileName);

void plg_MngOutJson(char* fileName, char* outJson);
void plg_MngFromJson(char* fromJson);
void plg_MngSendExit(void* pvManage);
#endif