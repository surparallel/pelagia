/* table.h - Table structure related
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

#ifndef __TABLE_H
#define __TABLE_H

#include "pinterface.h"

typedef struct _TableHandleCallBack {
	unsigned int(*findPage)(void* pageOperateHandle, unsigned int pageAddr, void** page);
	unsigned int(*createPage)(void* pageOperateHandle, void** retPage, char type);
	unsigned int(*delPage)(void* pageOperateHandle, unsigned int pageAddr);
	unsigned int(*arrangementCheck)(void* pageOperateHandle, void* page);
	void*(*pageCopyOnWrite)(void* pageOperateHandle, unsigned int pageAddr, void* page);
	void(*addDirtyPage)(void* pageOperateHandle, unsigned int pageAddr);
	void*(*tableCopyOnWrite)(void* pageOperateHandle, sds table, void* tableInFile);
	void(*addDirtyTable)(void* pageOperateHandle, sds table);
	void*(*findTableInFile)(void* pageOperateHandle, sds table, void* tableInFile);
}*PTableHandleCallBack, TableHandleCallBack;

void* plg_TableCreateHandle(void* pTableInFile, void* pageOperateHandle, unsigned int pageSize,
	sds	nameaTable, PTableHandleCallBack pTableHandleCallBack);

void* plg_TablePTableInFile(void* pTableHandle);
void plg_TableDestroyHandle(void* pTableHandle);
int plg_TableHandleCmpFun(void* left, void* right);
void* plg_TableName(void* pTableHandle);
unsigned int plg_TableHitStamp(void* pTableHandle);
void plg_TableResetHandle(void* pTableHandle, void* pTableInFile, sds tableName);

void plg_TableArrangementPage(unsigned int pageSize, void* page);
typedef int(*FindCmpFun)(void* key1, unsigned int key1Len, void* key2, unsigned int Key2Len);
int plg_TablePrevFindCmpFun(void* key1, unsigned int key1Len, void* key2, unsigned int Key2Len);
int plg_TableTailFindCmpFun(void* key1, unsigned int key1Len, void* key2, unsigned int Key2Len);
unsigned int plg_TableFindWithName(void* pTableHandle, char* key, unsigned short keyLen, void* skipListPoint, FindCmpFun pFindCmpFun);

//normal
unsigned int plg_TableAdd(void* pTableHandle, char* sdsKey, void* value, unsigned short length);
unsigned int plg_TableDel(void* pTableHandle, char* sdsKey);
unsigned int plg_TableAlter(void* pTableHandle, char* sdsKey, void* value, unsigned short length);
int plg_TableFind(void* pTableHandle, char* sdsKey, void* pDictExten, short isSet);
unsigned int plg_TableAddWithAlter(void* pTableHandle, char* sdsKey, char valueType, void* value, unsigned short length);
unsigned int plg_TableLength(void* pTableHandle);
unsigned int plg_TableAddIfNoExist(void* pTableHandle, char* sdsKey, char valueType, void* value, unsigned short length);
unsigned int plg_TableIsKeyExist(void* pTableHandle, char* sdsKey);
unsigned int plg_TableRename(void* pTableHandle, char* sdsKey, char* sdsNewKey);
void plg_TableLimite(void* pTableHandle, char* sdsKey, unsigned int left, unsigned int right, void* pDictExten);
void plg_TableOrder(void* pTableHandle, short order, unsigned int limite, void* pDictExten);
void plg_TableRang(void* pTableHandle, char* sdsBeginKey, char* sdsEndKey, void* pDictExten);
void plg_TablePattern(void* pTableHandle, char* sdsBeginKey, char* sdsEndKey, char* pattern, void* pDictExten);
unsigned int plg_TableMultiAdd(void* pTableHandle, void* pDictExten);
void plg_TableMultiFind(void* pTableHandle, void* pKeyDictExten, void* pValueDictExten);
unsigned int plg_TableRand(void* pTableHandle, void* pDictExten);
void plg_TableClear(void* pTableHandle, short recursive);
unsigned short plg_TableBigValueSize();

//set
unsigned int plg_TableSetAdd(void* pTableHandle, char* sdsKey, char* sdsValue);
void plg_TableSetRang(void* pTableHandle, char* sdsKey, char* sdsBeginValue, char* sdsEndValue, void* pInDictExten);
void plg_TableSetLimite(void* pTableHandle, char* sdsKey, char* sdsValue, unsigned int left, unsigned int right, void* pInDictExten);
unsigned int plg_TableSetLength(void* pTableHandle, char* sdsKey);
unsigned int plg_TableSetIsKeyExist(void* pTableHandle, char* sdsKey, char* sdsValue);
void plg_TableSetMembers(void* pTableHandle, char* sdsKey, void* pInDictExten);
unsigned int plg_TableSetRand(void* pTableHandle, char* sdsKey, void* pInDictExten);
void plg_TableSetDel(void* pTableHandle, char* sdsKey, void* pValueDictExten);
unsigned int plg_TableSetPop(void* pTableHandle, char* sdsKey, void* pInDictExten);
unsigned int plg_TableSetRangCount(void* pTableHandle, char* sdsKey, char* sdsBeginValue, char* sdsEndValue);
void plg_TableSetUion(void* pTableHandle, void* pSetDictExten, void* pKeyDictExten);
void plg_TableSetUionStore(void* pTableHandle, void* pSetDictExten, char* sdsKey);
void plg_TableSetInter(void* pTableHandle, void* pSetDictExten, void* pKeyDictExten);
void plg_TableSetInterStore(void* pTableHandle, void* pSetDictExten, char* sdsKey);
void plg_TableSetDiff(void* pTableHandle, void* pSetDictExten, void* pKeyDictExten);
void plg_TableSetDiffStore(void* pTableHandle, void* pSetDictExten, char* sdsKey);
void plg_TableSetMove(void* pTableHandle, char* sdsSrcKey, char* sdsDesKey, char* sdsValue);

//iter return pTableIterator
void* plg_TableGetIteratorToTail(void* pTableHandle);
void* plg_TableGetIteratorWithKey(void* pTableHandle, sds key);
void* plg_TablePrevIterator(void* pTableIterator);
void* plg_TableNextIterator(void* pTableIterator);
void plg_TableReleaseIterator(void* pTableIterator);
void plg_TableCheckTable(void* pTableHandle);
unsigned int plg_TableIteratorAddr(void* pTableIterator);
unsigned short plg_TableIteratorOffset(void* pTableIterator);

//big value
unsigned int plg_TableNewBigValue(void* pTableHandle, char* value, unsigned int valueLen, void* pDiskKeyBigValue);
void plg_TableArrangmentBigValue(unsigned int pageSize, void* page);

void plg_TableMembersWithJson(void* pTableHandle, void* jsonRoot);

void plg_TableInitTableInFile(void* pTableInFile);
#endif