/* lapi.c - lua api
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
#include "plapi.h"
#include "plua.h"
#include "plauxlib.h"
#include "plvm.h"
#include "pjson.h"
#include "pelagia.h"
#include "pelog.h"
#include "psds.h"
#include "pbase64.h"
#include "pjob.h"
#include "ptimesys.h"

//As long as there is no problem of not loading multiple Lua modules, loading multiple modules may result in one of them not being used
static void* _plVMHandle = 0;

static int L_NVersion(lua_State* L) {
	plg_Lvmpushnumber(_plVMHandle, L, plg_NVersion());
	return 1;
}

static int L_MVersion(lua_State* L) {

	plg_Lvmpushnumber(_plVMHandle, L, plg_MVersion());
	return 1;
}

static int LRemoteCall(lua_State* L) {

	size_t oLen, vLen;
	const char* o = plg_Lvmchecklstring(_plVMHandle, L, 1, &oLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, L, 2, &vLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobRemoteCall((void*)o, oLen, (void*)v, vLen));
	return 1;
}

static int LMS(lua_State* L) {

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_GetCurrentMilli());
	return 1;
}

static int LTableName(lua_State* L) {
	
	void* p = plg_JobTableNameWithJson();
	plg_Lvmpushstring(_plVMHandle, L, p);
	free(p);
	return 1;
}

static int LOrderName(lua_State* L) {

	short orderLen = 0;
	void* p = plg_JobCurrentOrder(&orderLen);
	plg_Lvmpushlstring(_plVMHandle, L, p, orderLen);
	free(p);
	return 1;
}

static int LTimer(lua_State* L) {
	
	size_t oLen, vLen;
	double timer = plg_Lvmchecknumber(_plVMHandle, L, 1);
	const char* o = plg_Lvmchecklstring(_plVMHandle, L, 2, &oLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, L, 3, &vLen);

	plg_JobAddTimer(timer, (void*)o, oLen, (void*)v, vLen);
	return 0;
}

static int LCommit(lua_State* L) {

	NOTUSED(L);
	plg_JobForceCommit();
	return 0;
}

static int LSet(lua_State* L) {

	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, L, 3, &vLen);

	unsigned rtype = 0;
	rtype = plg_JobSetTableTypeIfByte((void*)t, tLen, TT_String);
	if (rtype != TT_String) {
		elog(log_warn, "LSet Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobSet((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));
	return 1;
}

static int LMultiSet(lua_State* L) {
	
	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	unsigned rtype = 0;
	rtype = plg_JobSetTableTypeIfByte((void*)t, tLen, TT_String);
	if (rtype != TT_String) {
		elog(log_warn, "LMultiSet Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* k;char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &k, &v, & dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictExten, k, strlen(k), v, strlen(v));
		}
	}

	unsigned int r = plg_JobMultiSet((void*)t, tLen, pDictExten);
	plg_DictExtenDestroy(pDictExten);
	plg_Lvmpushnumber(_plVMHandle, L, r);
	return 1;
}

static int LDel(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobDel((void*)t, tLen, (void*)k, kLen));
	return 1;
}

static int LSetIfNoExit(lua_State* L) {

	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, L, 3, &vLen);

	unsigned rtype = 0;
	rtype = plg_JobSetTableTypeIfByte((void*)t, tLen, TT_String);
	if (rtype != TT_String) {
		elog(log_warn, "LSetIfNoExit Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobSetIfNoExit((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));
	return 1;
}

static int LTableClear(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	plg_JobTableClear((void*)t, tLen);
	return 0;
}

static int LRename(lua_State* L) {

	size_t tLen, kLen, nkLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* nk = plg_Lvmchecklstring(_plVMHandle, L, 3, &nkLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobRename((void*)t, tLen, (void*)k, kLen, (void*)nk, nkLen));
	return 1;
}

static int LGet(lua_State* L) {

	size_t tLen, kLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_String) {
		elog(log_warn, "LGet Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	char* p = plg_JobGet((void*)t, tLen, (void*)k, kLen, &pLen);
	if (p != 0) {
		plg_Lvmpushlstring(_plVMHandle, L, p, pLen);
		free(p);
	} else {
		plg_Lvmpushnil(_plVMHandle, L);
	}
	return 1;
}

static int LLength(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobLength((void*)t, tLen));
	return 1;
}

static int LIsKeyExist(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobIsKeyExist((void*)t, tLen, (void*)k, kLen));
	return 1;
}

static int LLimite(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	lua_Integer l = plg_Lvmcheckinteger(_plVMHandle, L, 3);
	lua_Integer r = plg_Lvmcheckinteger(_plVMHandle, L, 4);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_String) {
		elog(log_warn, "LLimite Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobLimite((void*)t, tLen, (void*)k, kLen, l, r, pDictExten);

	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		char* pk = plg_DictExtenKey(dictNode, &keyLen);
		char* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LOrder(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	lua_Integer o = plg_Lvmcheckinteger(_plVMHandle, L, 2);
	lua_Integer l = plg_Lvmcheckinteger(_plVMHandle, L, 3);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_String) {
		elog(log_warn, "LOrder Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobOrder((void*)t, tLen, o, l, pDictExten);

	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LRang(lua_State* L) {

	size_t tLen, kLen, keLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, L, 3, &keLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_String) {
		elog(log_warn, "LRang Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobRang((void*)t, tLen, (void*)k, kLen, (void*)ke, keLen, pDictExten);

	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LPoint(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	unsigned int dirtction = plg_Lvmcheckinteger(_plVMHandle, L, 3);
	unsigned int offset = plg_Lvmcheckinteger(_plVMHandle, L, 4);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_String) {
		elog(log_warn, "LRang Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobPoint((void*)t, tLen, (void*)k, kLen, dirtction, offset, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LPattern(lua_State* L) {

	size_t tLen, kLen, keLen, pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, L, 3, &keLen);
	const char* p = plg_Lvmchecklstring(_plVMHandle, L, 4, &pLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_String) {
		elog(log_warn, "LPattern Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobPattern((void*)t, tLen, (void*)k, kLen, (void*)ke, keLen, (void*)p, pLen, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LMembers(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_String) {
		elog(log_warn, "LPattern Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobMembers((void*)t, tLen, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LMultiGet(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_String) {
		elog(log_warn, "LMultiGet Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictKeyExten, v, strlen(v), NULL, 0);
		}
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobMultiGet((void*)t, tLen, pDictKeyExten, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);
	return 1;
}

static int LRand(lua_State* L) {

	size_t tLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_String) {
		elog(log_warn, "LRand Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	const char* p = plg_JobRand((void*)t, tLen, &pLen);
	if (p != 0) {
		plg_Lvmpushlstring(_plVMHandle, L, p, pLen);
	} else {
		plg_Lvmpushnil(_plVMHandle, L);
	}
	return 1;
}

static int LSetAdd(lua_State* L) {

	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, L, 3, &vLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_Byte) {
		elog(log_warn, "LSetAdd Current table %s type is %s to TT_Byte", t, plg_TT2String(rtype));
	}

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobSAdd((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));
	return 1;
}

static int LSetMove(lua_State* L) {

	size_t tLen, kLen, dkLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* dk = plg_Lvmchecklstring(_plVMHandle, L, 3, &dkLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, L, 4, &vLen);

	plg_JobSMove((void*)t, tLen, (void*)k, kLen, (void*)dk, dkLen, (void*)v, vLen);
	return 0;
}

static int LSetPop(lua_State* L) {

	size_t tLen, kLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	const char* p = plg_JobSPop((void*)t, tLen, (void*)k, kLen, &pLen);
	if (p != 0) {
		plg_Lvmpushlstring(_plVMHandle, L, p, pLen);
	} else {
		plg_Lvmpushnil(_plVMHandle, L);
	}
	return 1;
}

static int LSetDel(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 3, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictKeyExten, v, strlen(v), NULL,0);
		}
	}

	plg_JobSDel((void*)t, tLen, (void*)k, kLen, pDictKeyExten);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetUionStore(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictKeyExten, v, strlen(v), NULL, 0);
		}
	}
	
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 3, &kLen);

	plg_JobSUionStore((void*)t, tLen, pDictKeyExten, (void*)k, kLen);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetInterStore(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictKeyExten, v, strlen(v), NULL, 0);
		}
	}

	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 3, &kLen);

	plg_JobSInterStore((void*)t, tLen, pDictKeyExten, (void*)k, kLen);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetDiffStore(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictKeyExten, v, strlen(v), NULL, 0);
		}
	}

	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 3, &kLen);

	plg_JobSDiffStore((void*)t, tLen, pDictKeyExten, (void*)k, kLen);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetRang(lua_State* L) {

	size_t tLen, kLen, kbLen, keLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* kb = plg_Lvmchecklstring(_plVMHandle, L, 3, &kbLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, L, 4, &keLen);

	void* pDictExten = plg_DictExtenCreate();
	plg_JobSRang((void*)t, tLen, (void*)k, kLen, (void*)kb, kbLen, (void*)ke, keLen, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LSetPoint(lua_State* L) {

	size_t tLen, kLen, kbLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* kb = plg_Lvmchecklstring(_plVMHandle, L, 3, &kbLen);
	unsigned int dirtction = plg_Lvmcheckinteger(_plVMHandle, L, 4);
	unsigned int offset = plg_Lvmcheckinteger(_plVMHandle, L, 5);

	void* pDictExten = plg_DictExtenCreate();
	plg_JobSPoint((void*)t, tLen, (void*)k, kLen, (void*)kb, kbLen, dirtction, offset, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LSetLimite(lua_State* L) {

	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, L, 3, &vLen);
	lua_Integer l = plg_Lvmcheckinteger(_plVMHandle, L, 4);
	lua_Integer r = plg_Lvmcheckinteger(_plVMHandle, L, 5);

	void* pDictExten = plg_DictExtenCreate();
	plg_JobSLimite((void*)t, tLen, (void*)k, kLen, (void*)v, vLen, l, r, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LSetLength(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobSLength((void*)t, tLen, (void*)k, kLen));
	return 1;
}

static int LSetIsKeyExist(lua_State* L) {

	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, L, 3, &vLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobSIsKeyExist((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));
	return 1;
}

static int LSetMembers(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	void* pDictExten = plg_DictExtenCreate();
	plg_JobSMembers((void*)t, tLen, (void*)k, kLen, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LSetRand(lua_State* L) {

	size_t tLen, kLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	char* p = plg_JobSRand((void*)t, tLen, (void*)k, kLen, &pLen);

	if (p) {
		plg_Lvmpushlstring(_plVMHandle, L, p, pLen);
		free(p);
	} else {
		plg_Lvmpushnil(_plVMHandle, L);
	}
	return 1;
}

static int LSetRangCount(lua_State* L) {

	size_t tLen, kLen, keLen, kbLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* kb = plg_Lvmchecklstring(_plVMHandle, L, 3, &kbLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, L, 4, &keLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobSRangCount((void*)t, tLen, (void*)k, kLen, (void*)kb, kbLen, (void*)ke, keLen));
	return 1;
}

static int LSetUion(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	void* pDictExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictExten, v, strlen(v), NULL, 0);
		}
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_JobSUion((void*)t, tLen, pDictExten, pDictKeyExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictKeyExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);
	return 1;
}

static int LSetInter(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	void* pDictExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictExten, v, strlen(v), NULL, 0);
		}
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_JobSInter((void*)t, tLen, pDictExten, pDictKeyExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictKeyExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);
	return 1;
}

static int LSetDiff(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	void* pDictExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictExten, v, strlen(v), NULL, 0);
		}
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_JobSDiff((void*)t, tLen, pDictExten, pDictKeyExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictKeyExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);
	return 1;
}

static int LSet2(lua_State* L) {

	size_t tLen, kLen, vLen;
	unsigned short tt;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	char* v = plg_LvmMallocWithType(_plVMHandle, L, 3, &vLen, &tt);

	unsigned rtype = 0;
	rtype = plg_JobSetTableTypeIfByte((void*)t, tLen, tt);
	if (rtype != tt) {
		elog(log_warn, "LSet2 Current table %s type is %s to %s", t, plg_TT2String(rtype), plg_TT2String(tt));
	}

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobSet((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));

	free(v);
	return 1;
}

static int LMultiSet2(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	void* pDictExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* k; char* v = 0;
	double dv = 0;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &k, &v, &dv) != 0)
	{
		if (v) {

			unsigned rtype = 0;
			rtype = plg_JobSetTableTypeIfByte((void*)t, tLen, TT_String);
			if (rtype != TT_String) {
				elog(log_warn, "LMultiSet2 Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
			}

			plg_DictExtenAdd(pDictExten, k, strlen(k), v, strlen(v));
		} else {

			unsigned rtype = 0;
			rtype = plg_JobSetTableTypeIfByte((void*)t, tLen, TT_Double);
			if (rtype != TT_Double) {
				elog(log_warn, "LMultiSet2 Current table %s type is %s to TT_Double", t, plg_TT2String(rtype));
			}

			plg_DictExtenAdd(pDictExten, k, strlen(k), &dv, sizeof(dv));
		}
	}

	unsigned int r = plg_JobMultiSet((void*)t, tLen, pDictExten);
	plg_DictExtenDestroy(pDictExten);
	plg_Lvmpushnumber(_plVMHandle, L, r);
	return 1;
}

static int LDel2(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobDel((void*)t, tLen, (void*)k, kLen));
	return 1;
}

static int LSetIfNoExit2(lua_State* L) {

	size_t tLen, kLen, vLen;
	unsigned short tt;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	char* v = plg_LvmMallocWithType(_plVMHandle, L, 3, &vLen, &tt);

	unsigned rtype = 0;
	rtype = plg_JobSetTableTypeIfByte((void*)t, tLen, tt);
	if (rtype != tt) {
		elog(log_warn, "LSetIfNoExit2 Current table %s type is %s to %s", t, plg_TT2String(rtype), plg_TT2String(tt));
	}

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobSetIfNoExit((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));
	return 1;
}

static int LTableClear2(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	plg_JobTableClear((void*)t, tLen);
	return 0;
}

static int LRename2(lua_State* L) {
	
	size_t tLen, kLen, nkLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* nk = plg_Lvmchecklstring(_plVMHandle, L, 3, &nkLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobRename((void*)t, tLen, (void*)k, kLen, (void*)nk, nkLen));
	return 1;
}

static int LGet2(lua_State* L) {

	size_t tLen, kLen;
	unsigned int valueLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_Double && rtype != TT_String) {
		elog(log_warn, "LGet2 Current table %s type is %s to TT_String or TT_Double", t, plg_TT2String(rtype));
	}

	char* pValue = plg_JobGet((void*)t, tLen, (void*)k, kLen, &valueLen);
	if (pValue) {
		if (rtype == TT_Double) {
			lua_Number v;
			memcpy(&v, pValue, valueLen);
			plg_Lvmpushnumber(_plVMHandle, L, v);
		} else if (rtype == TT_String) {
			plg_Lvmpushlstring(_plVMHandle, L, pValue, valueLen);
		}
		free(pValue);
	} else {
		plg_Lvmpushnil(_plVMHandle, L);
	}
	return 1;
}

static int LLength2(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobLength((void*)t, tLen));
	return 1;
}

static int LIsKeyExist2(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobIsKeyExist((void*)t, tLen, (void*)k, kLen));
	return 1;
}

static int LLimite2(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	lua_Integer l = plg_Lvmcheckinteger(_plVMHandle, L, 3);
	lua_Integer r = plg_Lvmcheckinteger(_plVMHandle, L, 4);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_Double && rtype != TT_String) {
		elog(log_warn, "LLimite2 Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobLimite((void*)t, tLen, (void*)k, kLen, l, r, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pkey = plg_DictExtenKey(dictNode, &keyLen);
		char* pValue = plg_DictExtenValue(dictNode, &valueLen);
		if (rtype == TT_Double) {
			lua_Number v;
			memcpy((char*)&v, (char*) pValue, valueLen);
			plg_Lvmpushlstring(_plVMHandle, L, pkey, keyLen);
			plg_Lvmpushnumber(_plVMHandle, L, v);
			plg_Lvmsettable(_plVMHandle, L, -3);
		} else if (rtype == TT_String && valueLen != 0) {		
			plg_Lvmpushlstring(_plVMHandle, L, pkey, keyLen);
			plg_Lvmpushlstring(_plVMHandle, L, pValue, valueLen);
			plg_Lvmsettable(_plVMHandle, L, -3);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LOrder2(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	lua_Integer o = plg_Lvmcheckinteger(_plVMHandle, L, 2);
	lua_Integer l = plg_Lvmcheckinteger(_plVMHandle, L, 3);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_Double && rtype != TT_String) {
		elog(log_warn, "LOrder2 Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobOrder((void*)t, tLen, o, l, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pkey = plg_DictExtenKey(dictNode, &keyLen);
		char* pValue = plg_DictExtenValue(dictNode, &valueLen);
		if (rtype == TT_Double) {
			lua_Number v;
			memcpy((char*)&v, (char*)pValue, valueLen);
			plg_Lvmpushlstring(_plVMHandle, L, pkey, keyLen);
			plg_Lvmpushnumber(_plVMHandle, L, v);
			plg_Lvmsettable(_plVMHandle, L, -3);
		} else if (rtype == TT_String && valueLen != 0) {
			plg_Lvmpushlstring(_plVMHandle, L, pkey, keyLen);
			plg_Lvmpushlstring(_plVMHandle, L, pValue, valueLen);
			plg_Lvmsettable(_plVMHandle, L, -3);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LRang2(lua_State* L) {

	size_t tLen, kLen, keLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, L, 3, &keLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_Double && rtype != TT_String) {
		elog(log_warn, "LRang2 Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobRang((void*)t, tLen, (void*)k, kLen, (void*)ke, keLen, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pkey = plg_DictExtenKey(dictNode, &keyLen);
		char* pValue = plg_DictExtenValue(dictNode, &valueLen);
		if (rtype == TT_Double) {
			lua_Number v;
			memcpy((char*)&v, (char*)pValue, valueLen);
			plg_Lvmpushlstring(_plVMHandle, L, pkey, keyLen);
			plg_Lvmpushnumber(_plVMHandle, L, v);
			plg_Lvmsettable(_plVMHandle, L, -3);
		} else if (rtype == TT_String && valueLen != 0) {
			plg_Lvmpushlstring(_plVMHandle, L, pkey, keyLen);
			plg_Lvmpushlstring(_plVMHandle, L, pValue, valueLen);
			plg_Lvmsettable(_plVMHandle, L, -3);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LPoint2(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	unsigned int dirtction = plg_Lvmcheckinteger(_plVMHandle, L, 3);
	unsigned int offset = plg_Lvmcheckinteger(_plVMHandle, L, 4);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_Double && rtype != TT_String) {
		elog(log_warn, "LPoint2 Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobPoint((void*)t, tLen, (void*)k, kLen, dirtction, offset, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pkey = plg_DictExtenKey(dictNode, &keyLen);
		char* pValue = plg_DictExtenValue(dictNode, &valueLen);
		if (rtype == TT_Double) {
			lua_Number v;
			memcpy((char*)&v, (char*)pValue, valueLen);
			plg_Lvmpushlstring(_plVMHandle, L, pkey, keyLen);
			plg_Lvmpushnumber(_plVMHandle, L, v);
			plg_Lvmsettable(_plVMHandle, L, -3);
		} else if (rtype == TT_String && valueLen != 0) {
			plg_Lvmpushlstring(_plVMHandle, L, pkey, keyLen);
			plg_Lvmpushlstring(_plVMHandle, L, pValue, valueLen);
			plg_Lvmsettable(_plVMHandle, L, -3);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LPattern2(lua_State* L) {

	size_t tLen, kLen, keLen, pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, L, 3, &keLen);
	const char* p = plg_Lvmchecklstring(_plVMHandle, L, 4, &pLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_Double && rtype != TT_String) {
		elog(log_warn, "LPattern2 Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobPattern((void*)t, tLen, (void*)k, kLen, (void*)ke, keLen, (void*)p, pLen, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pkey = plg_DictExtenKey(dictNode, &keyLen);
		char* pValue = plg_DictExtenValue(dictNode, &valueLen);
		if (rtype == TT_Double) {
			lua_Number v;
			memcpy((char*)&v, (char*)pValue, valueLen);
			plg_Lvmpushlstring(_plVMHandle, L, pkey, keyLen);
			plg_Lvmpushnumber(_plVMHandle, L, v);
			plg_Lvmsettable(_plVMHandle, L, -3);
		} else if (rtype == TT_String && valueLen != 0) {
			plg_Lvmpushlstring(_plVMHandle, L, pkey, keyLen);
			plg_Lvmpushlstring(_plVMHandle, L, pValue, valueLen);
			plg_Lvmsettable(_plVMHandle, L, -3);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LMultiGet2(lua_State* L) {
	
	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_Double && rtype != TT_String) {
		elog(log_warn, "LPattern2 Current table %s type is %s to TT_String", t, plg_TT2String(rtype));
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictKeyExten, v, strlen(v), NULL, 0);
		}
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobMultiGet((void*)t, tLen, pDictKeyExten, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pkey = plg_DictExtenKey(dictNode, &keyLen);
		char* pValue = plg_DictExtenValue(dictNode, &valueLen);
		if (rtype == TT_Double) {
			lua_Number v;
			memcpy((char*)&v, (char*)pValue, valueLen);
			plg_Lvmpushlstring(_plVMHandle, L, pkey, keyLen);
			plg_Lvmpushnumber(_plVMHandle, L, v);
			plg_Lvmsettable(_plVMHandle, L, -3);
		} else if (rtype == TT_String && valueLen != 0) {
			plg_Lvmpushlstring(_plVMHandle, L, pkey, keyLen);
			plg_Lvmpushlstring(_plVMHandle, L, pValue, valueLen);
			plg_Lvmsettable(_plVMHandle, L, -3);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);
	return 1;
}

static int LRand2(lua_State* L) {

	size_t tLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_Double && rtype != TT_String) {
		elog(log_warn, "LRand2 Current table %s type is %s to TT_String or TT_Double", t, plg_TT2String(rtype));
	}

	const char* pValue = plg_JobRand((void*)t, tLen, &pLen);
	if (rtype == TT_Double) {
		lua_Number v;
		memcpy(&v, pValue, pLen);
		plg_Lvmpushnumber(_plVMHandle, L, v);
	} else if (rtype == TT_String) {
		plg_Lvmpushlstring(_plVMHandle, L, pValue, pLen);
	}

	return 1;
}

static int LSetAdd2(lua_State* L) {

	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, L, 3, &vLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_Byte) {
		elog(log_warn, "LSetAdd Current table %s type is %s to TT_Byte", t, plg_TT2String(rtype));
	}

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobSAdd((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));
	return 1;
}

static int LSetMove2(lua_State* L) {

	size_t tLen, kLen, dkLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* dk = plg_Lvmchecklstring(_plVMHandle, L, 3, &dkLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, L, 4, &vLen);

	plg_JobSMove((void*)t, tLen, (void*)k, kLen, (void*)dk, dkLen, (void*)v, vLen);
	return 0;
}

static int LSetPop2(lua_State* L) {
	
	size_t tLen, kLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	unsigned rtype = 0;
	if (rtype != TT_Byte) {
		elog(log_warn, "LSetPop2 Current table %s type is %s to TT_Byte", t, plg_TT2String(rtype));
	}

	const char* pValue = plg_JobSPop((void*)t, tLen, (void*)k, kLen, &pLen);

	if (pValue) {
		plg_Lvmpushlstring(_plVMHandle, L, pValue, pLen);
	}
	return 1;
}

static int LSetDel2(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 3, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictKeyExten, v, strlen(v), NULL, 0);
		}
	}

	plg_JobSDel((void*)t, tLen, (void*)k, kLen, pDictKeyExten);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetUionStore2(lua_State* L) {
	
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictKeyExten, v, strlen(v), NULL, 0);
		}
	}

	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 3, &kLen);

	plg_JobSUionStore((void*)t, tLen, pDictKeyExten, (void*)k, kLen);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetInterStore2(lua_State* L) {
	
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictKeyExten, v, strlen(v), NULL, 0);
		}
	}

	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 3, &kLen);

	plg_JobSInterStore((void*)t, tLen, pDictKeyExten, (void*)k, kLen);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetDiffStore2(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictKeyExten, v, strlen(v), NULL, 0);
		}
	}

	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 3, &kLen);

	plg_JobSDiffStore((void*)t, tLen, pDictKeyExten, (void*)k, kLen);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetRang2(lua_State* L) {

	size_t tLen, kLen, kbLen, keLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* kb = plg_Lvmchecklstring(_plVMHandle, L, 3, &kbLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, L, 4, &keLen);

	unsigned rtype = 0;
	if (rtype != TT_Byte) {
		elog(log_warn, "LSetRang2 Current table %s type is %s to TT_Byte", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobSRang((void*)t, tLen, (void*)k, kLen, (void*)kb, kbLen, (void*)ke, keLen, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}


static int LSetPoint2(lua_State* L) {

	size_t tLen, kLen, kbLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* kb = plg_Lvmchecklstring(_plVMHandle, L, 3, &kbLen);
	unsigned int dirtction = plg_Lvmcheckinteger(_plVMHandle, L, 4);
	unsigned int offset = plg_Lvmcheckinteger(_plVMHandle, L, 5);

	unsigned rtype = 0;
	if (rtype != TT_Byte) {
		elog(log_warn, "LSetPoint2 Current table %s type is %s to TT_Byte", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobSPoint((void*)t, tLen, (void*)k, kLen, (void*)kb, kbLen, dirtction, offset, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LSetLimite2(lua_State* L) {
	
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, L, 3, &vLen);
	lua_Integer l = plg_Lvmcheckinteger(_plVMHandle, L, 4);
	lua_Integer r = plg_Lvmcheckinteger(_plVMHandle, L, 5);

	unsigned rtype = 0;
	if (rtype != TT_Byte) {
		elog(log_warn, "LSetLimite2 Current table %s type is %s to TT_Byte", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobSLimite((void*)t, tLen, (void*)k, kLen, (void*)v, vLen, l, r, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LSetLength2(lua_State* L) {

	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobSLength((void*)t, tLen, (void*)k, kLen));
	return 1;
}

static int LSetIsKeyExist2(lua_State* L) {

	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, L, 3, &vLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobSIsKeyExist((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));
	return 1;
}

static int LSetMembers2(lua_State* L) {
	
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	unsigned rtype = 0;
	if (rtype != TT_Byte) {
		elog(log_warn, "LSetMembers2 Current table %s type is %s to TT_Byte", t, plg_TT2String(rtype));
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobSMembers((void*)t, tLen, (void*)k, kLen, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	return 1;
}

static int LSetRand2(lua_State* L) {

	size_t tLen, kLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);

	unsigned rtype = 0;
	if (rtype != TT_Byte) {
		elog(log_warn, "LSetRand2 Current table %s type is %s to TT_Byte", t, plg_TT2String(rtype));
	}

	char* pValue = plg_JobSRand((void*)t, tLen, (void*)k, kLen, &pLen);
	if (pValue) {
		plg_Lvmpushlstring(_plVMHandle, L, pValue, pLen);
		free(pValue);
	}
	return 1;
}

static int LSetRangCount2(lua_State* L) {
	
	size_t tLen, kLen, keLen, kbLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, L, 2, &kLen);
	const char* kb = plg_Lvmchecklstring(_plVMHandle, L, 3, &kbLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, L, 4, &keLen);

	plg_Lvmpushnumber(_plVMHandle, L, (lua_Number)plg_JobSRangCount((void*)t, tLen, (void*)k, kLen, (void*)kb, kbLen, (void*)ke, keLen));
	return 1;
}

static int LSetUion2(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_Byte) {
		elog(log_warn, "LSetUion2 Current table %s type is %s to TT_Byte", t, plg_TT2String(rtype));
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictKeyExten, v, strlen(v), NULL, 0);
		}
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	plg_JobSUion((void*)t, tLen, pDictKeyExten, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);
	return 1;
}

static int LSetInter2(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_Byte) {
		elog(log_warn, "LSetInter2 Current table %s type is %s to TT_Byte", t, plg_TT2String(rtype));
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictKeyExten, v, strlen(v), NULL, 0);
		}
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobSInter((void*)t, tLen, pDictKeyExten, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);
	return 1;
}

static int LSetDiff2(lua_State* L) {

	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, L, 1, &tLen);

	unsigned rtype = 0;
	rtype = plg_JobGetTableType((void*)t, tLen);
	if (rtype != TT_Byte) {
		elog(log_warn, "LSetDiff2 Current table %s type is %s to TT_Byte", t, plg_TT2String(rtype));
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	plg_Lvmpushnil(_plVMHandle, L);
	char* arrary_k; char* v = 0;
	double dv;
	while (plg_LvmTableNext(_plVMHandle, L, 2, &arrary_k, &v, &dv) != 0)
	{
		if (v) {
			plg_DictExtenAdd(pDictKeyExten, v, strlen(v), NULL, 0);
		}
	}

	void* pDictExten = plg_DictExtenCreate();
	plg_JobSDiff((void*)t, tLen, pDictKeyExten, pDictExten);
	plg_Lvmcreatetable(_plVMHandle, L, 0, 0);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		plg_Lvmpushlstring(_plVMHandle, L, pk, keyLen);
		plg_Lvmpushlstring(_plVMHandle, L, pv, valueLen);
		plg_Lvmsettable(_plVMHandle, L, -3);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);
	return 1;
}

static int LEventSend(lua_State* L) {

	size_t hLen, vLen;
	const char* h = plg_Lvmchecklstring(_plVMHandle, L, 1, &hLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, L, 2, &vLen);

	unsigned int decsize;
	unsigned char* buff = plg_B64DecodeEx(h, hLen, &decsize);
	void* p;

	memcpy(&p, buff, decsize);
	unsigned long long up = (unsigned long long)p;
	if (up == 101021) {
		printf("%s\n", v);
	} else {
		plg_EventSendWithMax(p, v, vLen, job_MaxQueue());
	}
	
	free(buff);
	return 0;
}

static luaL_Reg mylibs[] = {
	{ "NVersion", L_NVersion },
	{ "MVersion", L_MVersion },

	{ "RemoteCall", LRemoteCall },
	{ "MS", LMS },
	{ "TableName", LTableName },
	{ "OrderName", LOrderName },
	{ "Timer", LTimer },
	{ "Commit", LCommit },

	{ "Set", LSet },
	{ "MultiSet", LMultiSet },
	{ "Del", LDel },
	{ "SetIfNoExit", LSetIfNoExit },
	{ "TableClear", LTableClear },
	{ "Rename", LRename },

	{ "Get", LGet },
	{ "Length", LLength },
	{ "IsKeyExist", LIsKeyExist },
	{ "Limite", LLimite },
	{ "Order", LOrder },
	{ "Rang", LRang },
	{ "Point", LPoint },
	{ "Pattern", LPattern },
	{ "MultiGet", LMultiGet },
	{ "Rand", LRand },
	{ "Members", LMembers },

	{ "SetAdd", LSetAdd },
	{ "SetMove", LSetMove },
	{ "SetPop", LSetPop },
	{ "SetDel", LSetDel },
	{ "SetUionStore", LSetUionStore },
	{ "SetInterStore", LSetInterStore },
	{ "SetDiffStore", LSetDiffStore },

	{ "SetRang", LSetRang },
	{ "SetPoint", LSetPoint },
	{ "SetLimite", LSetLimite },
	{ "SetLength", LSetLength },
	{ "SetIsKeyExist", LSetIsKeyExist },
	{ "SetMembers", LSetMembers },
	{ "SetRand", LSetRand },
	{ "SetRangCount", LSetRangCount },
	{ "SetUion", LSetUion },
	{ "SetInter", LSetInter },
	{ "SetDiff", LSetDiff },

	//with type
	{ "Set2", LSet2 },
	{ "MultiSet2", LMultiSet2 },
	{ "Del2", LDel2 },
	{ "SetIfNoExit2", LSetIfNoExit2 },
	{ "TableClear2", LTableClear2 },
	{ "Rename2", LRename2 },

	{ "Get2", LGet2 },
	{ "Length2", LLength2 },
	{ "IsKeyExist2", LIsKeyExist2 },
	{ "Limite2", LLimite2 },
	{ "Order2", LOrder2 },
	{ "Rang2", LRang2 },
	{ "Point2", LPoint2 },
	{ "Pattern2", LPattern2 },
	{ "MultiGet2", LMultiGet2 },
	{ "Rand2", LRand2 },

	{ "SetAdd2", LSetAdd2 },
	{ "SetMove2", LSetMove2 },
	{ "SetPop2", LSetPop2 },
	{ "SetDel2", LSetDel2 },
	{ "SetUionStore2", LSetUionStore2 },
	{ "SetInterStore2", LSetInterStore2 },
	{ "SetDiffStore2", LSetDiffStore2 },

	{ "SetRang2", LSetRang2 },
	{ "SetPoint2", LSetPoint2 },
	{ "SetLimite2", LSetLimite2 },
	{ "SetLength2", LSetLength2 },
	{ "SetIsKeyExist2", LSetIsKeyExist2 },
	{ "SetMembers2", LSetMembers2 },
	{ "SetRand2", LSetRand2 },
	{ "SetRangCount2", LSetRangCount2 },
	{ "SetUion2", LSetUion2 },
	{ "SetInter2", LSetInter2 },
	{ "SetDiff2", LSetDiff2 },

	//event
	{ "EventSend", LEventSend },
	{ NULL, NULL }
};

static int plg_lualapilib2(lua_State *L)
{
	const char *libName = "pelagia";
	plg_Lvmregister(_plVMHandle, L, libName, mylibs);
	return 1;
}

int plg_lualapilib(void* plVMHandle)
{
	if (plVMHandle == NULL) {
		elog(log_error, "plg_lualapilib.plVMHandle");
		return 0;
	}

	_plVMHandle = plVMHandle;
	const char *libName = "pelagia";
	if (plg_LvmGetV(plVMHandle) == 1) {
		plg_Lvmregister(plVMHandle, plg_LvmGetL(plVMHandle), libName, mylibs);
	} else  {
		plg_LvmRequiref(plVMHandle, "pelagia", plg_lualapilib2, 1);
	}

	return 1;
}

#undef FillFun
#undef NORET