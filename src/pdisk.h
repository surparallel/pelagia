/* disk.h
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
#ifndef __DISK_H
#define __DISK_H

//API
unsigned int plg_DiskFileOpen(void* pManage, char* filePath, void** pDiskHandle, char isNew, char noSave);
void plg_DiskFileCloseHandle(void* pDiskHandle);
unsigned long long plg_DiskGetPageSize(void* pDiskHandle);
void* plg_DiskFileHandle(void* pDiskHandle);
void* plg_DiskTableHandle(void* pDiskHandle);
char plg_DiskIsNoSave(void* pDiskHandle);

//at run status
unsigned int plg_DiskTableAdd(void* pDiskHandle, void* tableName, void* value, unsigned int length);
unsigned int plg_DiskTableDel(void* pDiskHandle, void* tableName);
int plg_DiskTableFind(void* pDiskHandle, void* tableName, void* pDictExten);

unsigned int plg_DiskAllocPage(void* pDiskHandle, unsigned int* pageAddr);
unsigned int plg_DiskFreePage(void* pDiskHandle, unsigned int pageAddr);

void plg_DiskSetIsRun(void* pDiskHandle, int isRun);

//for test
unsigned int plg_DiskInsideTableAdd(void* pDiskHandle, void* tableName, void* value, unsigned int length);
unsigned int plg_DiskInsideTableDel(void* pDiskHandle, void* tableName);
int plg_DiskInsideTableFind(void* pDiskHandle, void* tableName, void* pDictExten);

extern unsigned int plg_DiskFlushDirtyToFile(void* pvDiskHandle, FlushCallBack pFlushCallBack);
void plg_DiskPrintTableName(void* pDiskHandle);
void plg_DiskAddTableWeight(void* pDiskHandle, unsigned int weight);
unsigned int plg_DiskGetTableAllWeight(void* pDiskHandle);

typedef void(*FillTableNameCB)(void* pDiskHandle, void* ptr, char* TableName);
void plg_DiskFillTableName(void* pDiskHandle, void* ptr, FillTableNameCB funCB);
#endif