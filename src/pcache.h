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
#ifndef __CACHE_H
#define __CACHE_H

//API
void* plg_CacheCreateHandle(void* pDiskHandle);
void plg_CacheDestroyHandle(void* pvCacheHandle);

//safe api for run
unsigned short plg_CacheGetTableType(void* pvCacheHandle, sds sdsTable, short recent);
unsigned short plg_CacheSetTableType(void* pvCacheHandle, sds sdsTable, unsigned short tableType);
unsigned short plg_CacheSetTableTypeIfByte(void* pvCacheHandle, sds sdsTable, unsigned short tableType);
unsigned int plg_CacheTableAdd(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, void* value, unsigned int length);
unsigned int plg_CacheTableDel(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen);
int plg_CacheTableFind(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, void* pDictExten, short recent);
unsigned int plg_CacheTableLength(void* pvCacheHandle, char* sdsTable, short recent);
unsigned int plg_CacheTableAddIfNoExist(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, void* value, unsigned int length);
unsigned int plg_CacheTableIsKeyExist(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, short recent);
unsigned int plg_CacheTableRename(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, void* vNewKey, short newKeyLen);
void plg_CacheTableLimite(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, unsigned int left, unsigned int right, void* pDictExten, short recent);
void plg_CacheTableOrder(void* pvCacheHandle, char* sdsTable, short order, unsigned int limite, void* pDictExten, short recent);
void plg_CacheTableRang(void* pvCacheHandle, char* sdsTable, void* beginKey, short beginKeyLen, void* endKey, short endKeyLen, void* pDictExten, short recent);
void plg_CacheTablePattern(void* pvCacheHandle, char* sdsTable, void* beginKey, short beginKeyLen, void* endKey, short endKeyLen, void* pattern, short patternLen, void* pDictExten, short recent);
unsigned int plg_CacheTableMultiAdd(void* pvCacheHandle, char* sdsTable, void* pDictExten);
void plg_CacheTableMultiFind(void* pvCacheHandle, char* sdsTable, void* pKeyDictExten, void* pValueDictExten, short recent);
unsigned int plg_CacheTableRand(void* pvCacheHandle, char* sdsTable, void* pDictExten, short recent);
void plg_CacheTableClear(void* pvCacheHandle, char* sdsTable);
void plg_CacheTablePoint(void* pvCacheHandle, sds sdsTable, void* beginKey, short beginKeyLen, unsigned int direction, unsigned int offset, void* pDictExten, short recent);

//set
unsigned int plg_CacheTableSetAdd(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, void* vValue, short valueLen);
void plg_CacheTableSetRang(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, void* beginValue, short beginValueLen, void* endValue, short endValueLen, void* pDictExten, short recent);
void plg_CacheTableSetPoint(void* pvCacheHandle, sds sdsTable, void* vKey, short keyLen, void* beginValue, short beginValueLen, unsigned int direction, unsigned int offset, void* pDictExten, short recent);
void plg_CacheTableSetLimite(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, void* vValue, short valueLen, unsigned int left, unsigned int right, void* pDictExten, short recent);
unsigned int plg_CacheTableSetLength(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, short recent);
unsigned int plg_CacheTableSetIsKeyExist(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, void* vValue, short valueLen, short recent);
void plg_CacheTableSetMembers(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, void* pDictExten, short recent);
unsigned int plg_CacheTableSetRand(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, void* pDictExten, short recent);
void plg_CacheTableSetDel(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, void* pValueDictExten);
unsigned int plg_CacheTableSetPop(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, void* pDictExten, short recent);
unsigned int plg_CacheTableSetRangCount(void* pvCacheHandle, char* sdsTable, void* vKey, short keyLen, void* beginValue, short beginValueLen, void* endValue, short endValueLen, short recent);
unsigned int plg_CacheTableSetUion(void* pvCacheHandle, char* sdsTable, void* pSetDictExten, void* pKeyDictExten, short recent);
unsigned int plg_CacheTableSetUionStore(void* pvCacheHandle, char* sdsTable, void* pSetDictExten, void* vKey, short keyLen);
unsigned int plg_CacheTableSetInter(void* pvCacheHandle, char* sdsTable, void* pSetDictExten, void* pKeyDictExten, short recent);
unsigned int plg_CacheTableSetInterStore(void* pvCacheHandle, char* sdsTable, void* pSetDictExten, void* vKey, short keyLen);
unsigned int plg_CacheTableSetDiff(void* pvCacheHandle, char* sdsTable, void* pSetDictExten, void* pKeyDictExten, short recent);
unsigned int plg_CacheTableSetDiffStore(void* pvCacheHandle, char* sdsTable, void* pSetDictExten, void* vKey, short keyLen);
unsigned int plg_CacheTableSetMove(void* pvCacheHandle, sds sdsTable, void* vSrcKey, short  srcKeyLen, void* vDesKey, short desKeyLen, void* vValue, short valueLen);

int plg_CacheCommit(void* pvCacheHandle);
int plg_CacheRollBack(void* pvCacheHandle);
void plg_CacheFlush(void* pvCacheHandle);

//config
void plg_CacheSetInterval(void* pvCacheHandle, unsigned int interval);
void plg_CacheSetPercent(void* pvCacheHandle, unsigned int percent);

unsigned int plg_CacheTableMembersWithJson(void* pvCacheHandle, char* sdsTable, void* jsonRoot, short recent);
#endif