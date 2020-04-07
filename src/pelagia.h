/*pelagia.h- API head file
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
#ifndef __PELAGIA_H
#define __PELAGIA_H

#define VERSION_MAJOR	"0"
#define VERSION_MINOR	"19"

#define VERSION_NUMMAJOR	0
#define VERSION_NUMMINOR	19

#include "papidefine.h"

//user manage API
PELAGIA_API void* plg_MngCreateHandle(char* dbPath, short dbPahtLen);
PELAGIA_API void* plg_MngCreateHandleWithJson(const char* jsonFile);
PELAGIA_API void plg_MngDestoryHandle(void* pManage);
PELAGIA_API int plg_MngStarJob(void* pManage);
PELAGIA_API void plg_MngStopJob(void* pManage);
PELAGIA_API int plg_MngAddOrder(void* pManage, char* nameOrder, short nameOrderLen, void* ptrProcess);

PELAGIA_API void plg_MngSetMaxTableWeight(void* pManage, unsigned int maxTableWeight);
PELAGIA_API int plg_MngAddTable(void* pManage, char* nameOrder, short nameOrderLen, char* nameTable, short nameTableLen);
PELAGIA_API int plg_MngSetWeight(void* pManage, char* nameTable, short nameTableLen, unsigned int weight);
PELAGIA_API int plg_MngSetNoShare(void* pManage, char* nameTable, short nameTableLen, unsigned char noShare);
PELAGIA_API int plg_MngSetNoSave(void* pManage, char* nameTable, short nameTableLen, unsigned char noSave);
PELAGIA_API void plg_MngSetLuaPath(void* pManage, char* newLuaPath);
PELAGIA_API void plg_MngSetLuaDllPath(void* pManage, char* newLuaDllPath);
PELAGIA_API void plg_MngSetDllPath(void* pManage, char* newDllPath);


PELAGIA_API int plg_MngAllocJob(void* pManage, unsigned int core);
PELAGIA_API int plg_MngFreeJob(void* pManage);
PELAGIA_API int plg_MngRemoteCall(void* pManage, char* order, short orderLen, char* value, short valueLen);
PELAGIA_API int plg_MngRemoteCallWithArg(void* pvManage, char* order, short orderLen, void* eventHandle, int argc, const char** argv);

//manage check API
PELAGIA_API void plg_MngPrintAllStatus(void* pManage);
PELAGIA_API void plg_MngPrintAllJobStatus(void* pManage);
PELAGIA_API void plg_MngPrintAllJobDetails(void* pManage);
PELAGIA_API void plg_MngPrintPossibleAlloc(void* pManage);
PELAGIA_API void plg_MngPrintAllJobOrder(void* pvManage);

//for ptrProcess of plg_MngAddOrder;
typedef int(*RoutingFun)(char* value, short valueLen);
PELAGIA_API void* plg_JobCreateFunPtr(RoutingFun funPtr);
PELAGIA_API void* plg_JobCreateLua(char* fileClass, short fileClassLen, char* fun, short funLen);
PELAGIA_API void* plg_JobCreateDll(char* fileClass, short fileClassLen, char* fun, short funLen);
PELAGIA_API void plg_JobSetWeight(void* pEventPorcess, unsigned int weight);
PELAGIA_API void plg_JobSetDonotFlush();
PELAGIA_API void plg_JobSetDonotCommit();

//system
PELAGIA_API int plg_JobRemoteCall(void* order, short orderLen, void* value, short valueLen);
PELAGIA_API char* plg_JobCurrentOrder(short* orderLen);//dont free
PELAGIA_API void plg_JobAddTimer(double timer, void* order, short orderLen, void* value, short valueLen);
PELAGIA_API char** plg_JobTableName(short* tableLen);//need free

//namorl db
PELAGIA_API unsigned int plg_JobSet(void* table, short tableLen, void* key, short keyLen, void* value, unsigned int valueLen);
PELAGIA_API unsigned int plg_JobMultiSet(void* table, short tableLen, void* pDictExten);
PELAGIA_API unsigned int plg_JobDel(void* table, short tableLen, void* key, short keyLen);
PELAGIA_API unsigned int plg_JobSetIfNoExit(void* table, short tableLen, void* key, short keyLen, void* value, unsigned int valueLen);
PELAGIA_API void plg_JobTableClear(void* table, short tableLen);
PELAGIA_API unsigned int plg_JobRename(void* table, short tableLen, void* key, short keyLen, void* newKey, short newKeyLen);

PELAGIA_API void* plg_JobGet(void* table, short tableLen, void* key, short keyLen, unsigned int* valueLen);
PELAGIA_API unsigned int plg_JobLength(void* table, short tableLen);
PELAGIA_API unsigned int plg_JobIsKeyExist(void* table, short tableLen, void* key, short keyLen);
PELAGIA_API void plg_JobLimite(void* table, short tableLen, void* key, short keyLen, unsigned int left, unsigned int right, void* pDictExten);
PELAGIA_API void plg_JobOrder(void* table, short tableLen, short order, unsigned int limite, void* pDictExten);
PELAGIA_API void plg_JobRang(void* table, short tableLen, void* beginKey, short beginKeyLen, void* endKey, short endKeyLen, void* pDictExten);
PELAGIA_API void plg_JobPattern(void* table, short tableLen, void* beginKey, short beginKeyLen, void* endKey, short endKeyLen, void* pattern, short patternLen, void* pDictExten);
PELAGIA_API void plg_JobMultiGet(void* table, short tableLen, void* pKeyDictExten, void* pValueDictExten);
PELAGIA_API void* plg_JobRand(void* table, short tableLen, unsigned int* valueLen);

//set db
PELAGIA_API unsigned int plg_JobSAdd(void* table, short tableLen, void* key, short keyLen, void* value, short valueLen);
PELAGIA_API void plg_JobSMove(void* table, short tableLen, void* srcKey, short srcKeyLen, void* desKey, short desKeyLen, void* value, short valueLen);
PELAGIA_API void* plg_JobSPop(void* table, short tableLen, void* key, short keyLen, unsigned int* valueLen);
PELAGIA_API void plg_JobSDel(void* table, short tableLen, void* key, short keyLen, void* pValueDictExten);
PELAGIA_API void plg_JobSUionStore(void* table, short tableLen, void* pSetDictExten, void* key, short keyLen);
PELAGIA_API void plg_JobSInterStore(void* table, short tableLen, void* pSetDictExten, void* key, short keyLen);
PELAGIA_API void plg_JobSDiffStore(void* table, short tableLen, void* pSetDictExten, void* key, short keyLen);

PELAGIA_API void plg_JobSRang(void* table, short tableLen, void* key, short keyLen, void* beginValue, short beginValueLen, void* endValue, short endValueLen, void* pDictExten);
PELAGIA_API void plg_JobSLimite(void* table, short tableLen, void* key, short keyLen, void* value, short valueLen, unsigned int left, unsigned int right, void* pDictExten);
PELAGIA_API unsigned int plg_JobSLength(void* table, short tableLen, void* key, short keyLen);
PELAGIA_API unsigned int plg_JobSIsKeyExist(void* table, short tableLen, void* key, short keyLen, void* value, short valueLen);
PELAGIA_API void plg_JobSMembers(void* table, short tableLen, void* key, short keyLen, void* pDictExten);
PELAGIA_API void* plg_JobSRand(void* table, short tableLen, void* key, short keyLen, unsigned int* valueLen);
PELAGIA_API unsigned int plg_JobSRangCount(void* table, short tableLen, void* key, short keyLen, void* beginValue, short beginValueLen, void* endValue, short endValueLen);
PELAGIA_API void plg_JobSUion(void* table, short tableLen, void* pSetDictExten, void* pKeyDictExten);
PELAGIA_API void plg_JobSInter(void* table, short tableLen, void* pSetDictExten, void* pKeyDictExten);
PELAGIA_API void plg_JobSDiff(void* table, short tableLen, void* pSetDictExten, void* pKeyDictExten);

//event for user
PELAGIA_API void* plg_EventCreateHandle();
PELAGIA_API void plg_EventDestroyHandle(void* pEventHandle);
PELAGIA_API int plg_EventTimeWait(void* pEventHandle, long long sec, int nsec);
PELAGIA_API int plg_EventWait(void* pEventHandle);
PELAGIA_API void plg_EventSend(void* pEventHandle, const char* value, unsigned int valueLen);
PELAGIA_API void* plg_EventRecvAlloc(void* pEventHandle, unsigned int* valueLen);
PELAGIA_API void plg_EventFreePtr(void* ptr);

//DictExten
PELAGIA_API void* plg_DictExtenCreate();
PELAGIA_API void* plg_DictExtenSubCreate(void* pDictExten, void* key, unsigned int keyLen);

PELAGIA_API void plg_DictExtenDestroy(void* pDictExten);
PELAGIA_API int plg_DictExtenAdd(void* pDictExten, void* key, unsigned int keyLen, void* value, unsigned int valueLen);
PELAGIA_API void plg_DictExtenDel(void* pDictExten, void* key, unsigned int keyLen);
PELAGIA_API void* plg_DictExtenSub(void* entry);
PELAGIA_API int plg_DictExtenSize(void* pDictExten);
PELAGIA_API void plg_DictExtenSortWithKey(void* vpDictExten);
PELAGIA_API void plg_DictExtenSortWithValue(void* vpDictExten);

//create iterator
PELAGIA_API void* plg_DictExtenGetIterator(void* pDictExten);
PELAGIA_API void plg_DictExtenReleaseIterator(void* iter);

//return entry
PELAGIA_API void* plg_DictExtenFind(void* pDictExten, void* key, unsigned int keyLen);
PELAGIA_API void* plg_DictExtenNext(void* iter);
PELAGIA_API void* plg_DictExtenGetHead(void* pDictExten);
PELAGIA_API int plg_DictExtenIsSub(void* entry);

//reurn buffer
PELAGIA_API void* plg_DictExtenValue(void* entry, unsigned int *valueLen);
PELAGIA_API void* plg_DictExtenKey(void* entry, unsigned int *keyLen);

//help for 'C' ansi
PELAGIA_API void* plg_DictExtenSubCreateForChar(void* pDictExten, char* key);
PELAGIA_API int plg_DictExtenAddForChar(void* pDictExten, char* key, void* value, unsigned int valueLen);
PELAGIA_API void plg_DictExtenDelForChar(void* pDictExten, char* key);
PELAGIA_API void* plg_DictExtenFindForChar(void* pDictExten, char* key);
PELAGIA_API int plg_DictExtenAddForCharWithInt(void* pDictExten, char* key, int value);
PELAGIA_API int plg_DictExtenAddForCharWithUInt(void* pDictExten, char* key, unsigned int value);
PELAGIA_API int plg_DictExtenAddForCharWithShort(void* pDictExten, char* key, short value);
PELAGIA_API int plg_DictExtenAddForCharWithLL(void* pDictExten, char* key, long long value);
PELAGIA_API int plg_DictExtenAddForCharWithDouble(void* pDictExten, char* key, double value);

//log
PELAGIA_API void plg_LogSetErrFile();
PELAGIA_API void plg_LogSetErrPrint();
PELAGIA_API void plg_LogSetMaxLevel(short level);
PELAGIA_API void plg_LogSetMinLevel(short level);
PELAGIA_API void plg_LogSetOutDir(char* outDir);
PELAGIA_API void plg_LogSetOutFile(char* outFile);

//version
PELAGIA_API unsigned int plg_NVersion();
PELAGIA_API unsigned int plg_MVersion();
PELAGIA_API void plg_Version();

#endif