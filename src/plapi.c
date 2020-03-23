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

//As long as there is no problem of not loading multiple Lua modules, loading multiple modules may result in one of them not being used
static void* _plVMHandle = 0;

static int L_NVersion(lua_State* L) {

	NOTUSED(L);
	plg_Lvmpushnumber(_plVMHandle, plg_NVersion());
	return 1;
}

static int L_MVersion(lua_State* L) {

	NOTUSED(L);
	plg_Lvmpushnumber(_plVMHandle, plg_MVersion());
	return 1;
}

static int LRemoteCall(lua_State* L) {

	NOTUSED(L);
	size_t oLen, vLen;
	const char* o = plg_Lvmchecklstring(_plVMHandle, 1, &oLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, 2, &vLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobRemoteCall((void*)o, oLen, (void*)v, vLen));
	return 1;
}

static int LTimer(lua_State* L) {

	NOTUSED(L);
	size_t oLen, vLen;
	double timer = plg_Lvmchecknumber(_plVMHandle, 1);
	const char* o = plg_Lvmchecklstring(_plVMHandle, 2, &oLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, 3, &vLen);

	plg_JobAddTimer(timer, (void*)o, oLen, (void*)v, vLen);
	return 0;
}


static int LSet(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, 3, &vLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobSet((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));
	return 1;
}

static int LMultiSet(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	lua_Number r = 0;
	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		plg_Lvmpushnumber(_plVMHandle, r);
		return 1;
	}

	void* pDictExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictExten, item->string, strlen(item->string), item->valuestring, strlen(item->valuestring));
		}
	}

	r = plg_JobMultiSet((void*)t, tLen, pDictExten);
	plg_DictExtenDestroy(pDictExten);

	pJson_Delete(root);
	plg_Lvmpushnumber(_plVMHandle, r);
	return 1;
}

static int LDel(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobDel((void*)t, tLen, (void*)k, kLen));
	return 1;
}

static int LSetIfNoExit(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, 3, &vLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobSetIfNoExit((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));
	return 1;
}

static int LTableClear(lua_State* L) {

	NOTUSED(L);
	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);

	plg_JobTableClear((void*)t, tLen);
	return 0;
}

static int LRename(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, nkLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* nk = plg_Lvmchecklstring(_plVMHandle, 3, &nkLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobRename((void*)t, tLen, (void*)k, kLen, (void*)nk, nkLen));
	return 1;
}

static int LGet(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	const char* p = plg_JobGet((void*)t, tLen, (void*)k, kLen, &pLen);

	if (p != 0) {
		plg_Lvmpushlstring(_plVMHandle, p, pLen);
	} else {
		plg_Lvmpushnil(_plVMHandle);
	}	
	return 1;
}

static int LLength(lua_State* L) {

	NOTUSED(L);
	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobLength((void*)t, tLen));
	return 1;
}

static int LIsKeyExist(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobIsKeyExist((void*)t, tLen, (void*)k, kLen));
	return 1;
}

static int LLimite(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	lua_Integer l = plg_Lvmcheckinteger(_plVMHandle, 3);
	lua_Integer r = plg_Lvmcheckinteger(_plVMHandle, 4);

	void* pDictExten = plg_DictExtenCreate();
	pJSON* root = pJson_CreateObject();

	plg_JobLimite((void*)t, tLen, (void*)k, kLen, l, r, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen = 0, valueLen = 0;
		char* pk = plg_DictExtenKey(dictNode, &keyLen);
		char* pv = plg_DictExtenValue(dictNode, &valueLen);

		pJson_AddStringToObjectWithLen(root, pk, keyLen, pv, valueLen);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);
	
	return 1;
}

static int LOrder(lua_State* L) {

	NOTUSED(L);
	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	lua_Integer o = plg_Lvmcheckinteger(_plVMHandle, 2);
	lua_Integer l = plg_Lvmcheckinteger(_plVMHandle, 3);

	void* pDictExten = plg_DictExtenCreate();
	pJSON* root = pJson_CreateObject();

	plg_JobOrder((void*)t, tLen, o, l, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		pJson_AddStringToObjectWithLen(root, pk, keyLen, pv, valueLen);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LRang(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, keLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, 3, &keLen);

	void* pDictExten = plg_DictExtenCreate();
	pJSON* root = pJson_CreateObject();

	plg_JobRang((void*)t, tLen, (void*)k, kLen, (void*)ke, keLen, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		pJson_AddStringToObjectWithLen(root, pk, keyLen, pv, valueLen);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LPattern(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, keLen, pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, 3, &keLen);
	const char* p = plg_Lvmchecklstring(_plVMHandle, 4, &pLen);

	void* pDictExten = plg_DictExtenCreate();
	pJSON* root = pJson_CreateObject();

	plg_JobPattern((void*)t, tLen, (void*)k, kLen, (void*)ke, keLen, (void*)p, pLen, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		pJson_AddStringToObjectWithLen(root, pk, keyLen, pv, valueLen);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LMultiGet(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictKeyExten, item->valuestring, strlen(item->valuestring), NULL, 0);
		}
	}
	pJson_Delete(root);

	root = pJson_CreateObject();
	void* pDictExten = plg_DictExtenCreate();
	root = pJson_CreateObject();

	plg_JobMultiGet((void*)t, tLen, pDictKeyExten, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		pJson_AddStringToObjectWithLen(root, pk, keyLen, pv, valueLen);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LRand(lua_State* L) {

	NOTUSED(L);
	size_t tLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);

	const char* p = plg_JobRand((void*)t, tLen, &pLen);

	if (p != 0) {
		plg_Lvmpushlstring(_plVMHandle, p, pLen);
	} else {
		plg_Lvmpushnil(_plVMHandle);
	}
	return 1;
}

static int LSetAdd(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, 3, &vLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobSAdd((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));
	return 1;
}

static int LSetMove(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, dkLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* dk = plg_Lvmchecklstring(_plVMHandle, 3, &dkLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, 4, &vLen);

	plg_JobSMove((void*)t, tLen, (void*)k, kLen, (void*)dk, dkLen, (void*)v, vLen);
	return 0;
}

static int LSetPop(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	const char* p = plg_JobSPop((void*)t, tLen, (void*)k, kLen, &pLen);

	if (p != 0) {
		plg_Lvmpushlstring(_plVMHandle, p, pLen);
	} else {
		plg_Lvmpushnil(_plVMHandle);
	}
	return 1;
}

static int LSetDel(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 3, &vLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictKeyExten, item->valuestring, strlen(item->valuestring), NULL, 0);
		}
	}
	pJson_Delete(root);

	plg_JobSDel((void*)t, tLen, (void*)k, kLen, pDictKeyExten);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetUionStore(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &vLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 3, &kLen);
	
	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictKeyExten, item->valuestring, strlen(item->valuestring), NULL, 0);
		}
	}
	pJson_Delete(root);

	plg_JobSUionStore((void*)t, tLen, pDictKeyExten, (void*)k, kLen);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetInterStore(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &vLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 3, &kLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictKeyExten, item->valuestring, strlen(item->valuestring), NULL, 0);
		}
	}
	pJson_Delete(root);

	plg_JobSInterStore((void*)t, tLen, pDictKeyExten, (void*)k, kLen);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetDiffStore(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &vLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 3, &kLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictKeyExten, item->valuestring, strlen(item->valuestring), NULL, 0);
		}
	}
	pJson_Delete(root);

	plg_JobSDiffStore((void*)t, tLen, pDictKeyExten, (void*)k, kLen);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetRang(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, kbLen, keLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* kb = plg_Lvmchecklstring(_plVMHandle, 3, &kbLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, 4, &keLen);

	void* pDictExten = plg_DictExtenCreate();
	pJSON* root = pJson_CreateObject();

	plg_JobSRang((void*)t, tLen, (void*)k, kLen, (void*)kb, kbLen, (void*)ke, keLen, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		pJson_AddStringToObjectWithLen(root, pk, keyLen, pv, valueLen);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LSetLimite(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, 3, &vLen);
	lua_Integer l = plg_Lvmcheckinteger(_plVMHandle, 4);
	lua_Integer r = plg_Lvmcheckinteger(_plVMHandle, 5);

	void* pDictExten = plg_DictExtenCreate();
	pJSON* root = pJson_CreateObject();

	plg_JobSLimite((void*)t, tLen, (void*)k, kLen, (void*)v, vLen, l, r, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		pJson_AddStringToObjectWithLen(root, pk, keyLen, pv, valueLen);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LSetLength(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobSLength((void*)t, tLen, (void*)k, kLen));
	return 1;
}

static int LSetIsKeyExist(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, 3, &vLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobSIsKeyExist((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));
	return 1;
}

static int LSetMembers(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	void* pDictExten = plg_DictExtenCreate();
	pJSON* root = pJson_CreateObject();

	plg_JobSMembers((void*)t, tLen, (void*)k, kLen, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		pJson_AddStringToObjectWithLen(root, pk, keyLen, pv, valueLen);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LSetRand(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	const char* p = plg_JobSRand((void*)t, tLen, (void*)k, kLen, &pLen);

	if (p) {
		plg_Lvmpushlstring(_plVMHandle, p, pLen);
	} else {
		plg_Lvmpushnil(_plVMHandle);
	}
	return 1;
}

static int LSetRangCount(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, keLen, kbLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* kb = plg_Lvmchecklstring(_plVMHandle, 3, &kbLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, 4, &keLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobSRangCount((void*)t, tLen, (void*)k, kLen, (void*)kb, kbLen, (void*)ke, keLen));
	return 1;
}

static int LSetUion(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictExten, item->valuestring, strlen(item->valuestring), NULL, 0);
		}
	}
	pJson_Delete(root);

	void* pDictKeyExten = plg_DictExtenCreate();
	root = pJson_CreateObject();

	plg_JobSUion((void*)t, tLen, pDictExten, pDictKeyExten);

	void* dictIter = plg_DictExtenGetIterator(pDictKeyExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		pJson_AddStringToObjectWithLen(root, pk, keyLen, pv, valueLen);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LSetInter(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictExten, item->valuestring, strlen(item->valuestring), NULL, 0);
		}
	}
	pJson_Delete(root);

	void* pDictKeyExten = plg_DictExtenCreate();
	root = pJson_CreateObject();

	plg_JobSInter((void*)t, tLen, pDictExten, pDictKeyExten);

	void* dictIter = plg_DictExtenGetIterator(pDictKeyExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		pJson_AddStringToObjectWithLen(root, pk, keyLen, pv, valueLen);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LSetDiff(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictExten, item->valuestring, strlen(item->valuestring), NULL, 0);
		}
	}
	pJson_Delete(root);

	void* pDictKeyExten = plg_DictExtenCreate();
	root = pJson_CreateObject();

	plg_JobSDiff((void*)t, tLen, pDictExten, pDictKeyExten);

	void* dictIter = plg_DictExtenGetIterator(pDictKeyExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		void* pk = plg_DictExtenKey(dictNode, &keyLen);
		void* pv = plg_DictExtenValue(dictNode, &valueLen);

		pJson_AddStringToObjectWithLen(root, pk, keyLen, pv, valueLen);
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LSet2(lua_State* L) {
	
	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	char* v = plg_LvmMallocWithType(_plVMHandle, 3, &vLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobSet((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));

	free(v);
	return 1;
}

static int LMultiSet2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	lua_Number r = 0;
	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		plg_Lvmpushnumber(_plVMHandle, r);
		return 1;
	}

	void* pDictExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			int len = strlen(item->valuestring);
			void* p = plg_LvmMallocForBuf(item->valuestring, len, LUA_TSTRING);
			len += 1;
			plg_DictExtenAdd(pDictExten, item->string, strlen(item->string), p, len);
		} else if (pJson_Number == item->type) {
			int len = sizeof(item->valuedouble);
			void* p = plg_LvmMallocForBuf(item->valuestring, len, LUA_TNUMBER);
			len += 1;
			plg_DictExtenAdd(pDictExten, item->string, strlen(item->string), p, len);
		}
	}

	r = plg_JobMultiSet((void*)t, tLen, pDictExten);
	plg_DictExtenDestroy(pDictExten);

	pJson_Delete(root);
	plg_Lvmpushnumber(_plVMHandle, r);
	return 1;
}

static int LDel2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobDel((void*)t, tLen, (void*)k, kLen));
	return 1;
}

static int LSetIfNoExit2(lua_State* L) {
	
	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, 3, &vLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobSetIfNoExit((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));
	return 1;
}

static int LTableClear2(lua_State* L) {
	
	NOTUSED(L);
	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);

	plg_JobTableClear((void*)t, tLen);
	return 0;
}

static int LRename2(lua_State* L) {
	
	NOTUSED(L);
	size_t tLen, kLen, nkLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* nk = plg_Lvmchecklstring(_plVMHandle, 3, &nkLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobRename((void*)t, tLen, (void*)k, kLen, (void*)nk, nkLen));
	return 1;
}

static int LGet2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	const char* p = plg_JobGet((void*)t, tLen, (void*)k, kLen, &pLen);

	if (p[0] == LUA_TNUMBER) {
		lua_Number v;
		memcpy(&v, SHELLING(p), SHELLING(pLen));
		plg_Lvmpushnumber(_plVMHandle, v);
	} else if (p[0] == LUA_TSTRING) {
		plg_Lvmpushlstring(_plVMHandle, SHELLING(p), SHELLING(pLen));
	} 
	
	return 1;
}

static int LLength2(lua_State* L) {

	NOTUSED(L);
	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobLength((void*)t, tLen));
	return 1;
}

static int LIsKeyExist2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobIsKeyExist((void*)t, tLen, (void*)k, kLen));
	return 1;
}

static int LLimite2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	lua_Integer l = plg_Lvmcheckinteger(_plVMHandle, 3);
	lua_Integer r = plg_Lvmcheckinteger(_plVMHandle, 4);

	void* pDictExten = plg_DictExtenCreate();
	pJSON* root = pJson_CreateObject();

	plg_JobLimite((void*)t, tLen, (void*)k, kLen, l, r, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		char* p = plg_DictExtenValue(dictNode, &valueLen);
		if (p[0] == LUA_TNUMBER) {
			lua_Number v;
			memcpy((char*)&v, (char*) SHELLING(p), SHELLING(valueLen));
			pJson_AddNumberToObject(root, plg_DictExtenKey(dictNode, &keyLen), v);
		} else if (p[0] == LUA_TSTRING && valueLen != 0) {
			sds s = plg_sdsNewLen(SHELLING(p), SHELLING(valueLen));
			pJson_AddStringToObject(root, plg_DictExtenKey(dictNode, &keyLen), s);
			plg_sdsFree(s);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LOrder2(lua_State* L) {

	NOTUSED(L);
	size_t tLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	lua_Integer o = plg_Lvmcheckinteger(_plVMHandle, 2);
	lua_Integer l = plg_Lvmcheckinteger(_plVMHandle, 3);

	void* pDictExten = plg_DictExtenCreate();
	pJSON* root = pJson_CreateObject();

	plg_JobOrder((void*)t, tLen, o, l, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		char* p = plg_DictExtenValue(dictNode, &valueLen);
		if (p[0] == LUA_TNUMBER) {
			lua_Number v;
			memcpy((char*)&v, (char*)SHELLING(p), SHELLING(valueLen));
			pJson_AddNumberToObject(root, plg_DictExtenKey(dictNode, &keyLen), v);
		} else if (p[0] == LUA_TSTRING) {
			sds s = plg_sdsNewLen(SHELLING(p), SHELLING(valueLen));
			pJson_AddStringToObject(root, plg_DictExtenKey(dictNode, &keyLen), s);
			plg_sdsFree(s);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LRang2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, keLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, 3, &keLen);

	void* pDictExten = plg_DictExtenCreate();
	pJSON* root = pJson_CreateObject();

	plg_JobRang((void*)t, tLen, (void*)k, kLen, (void*)ke, keLen, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		char* p = plg_DictExtenValue(dictNode, &valueLen);
		if (p[0] == LUA_TNUMBER) {
			lua_Number v;
			memcpy((char*)&v, (char*)SHELLING(p), SHELLING(valueLen));
			pJson_AddNumberToObject(root, plg_DictExtenKey(dictNode, &keyLen), v);
		} else if (p[0] == LUA_TSTRING) {
			sds s = plg_sdsNewLen(SHELLING(p), SHELLING(valueLen));
			pJson_AddStringToObject(root, plg_DictExtenKey(dictNode, &keyLen), s);
			plg_sdsFree(s);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LPattern2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, keLen, pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, 3, &keLen);
	const char* p = plg_Lvmchecklstring(_plVMHandle, 4, &pLen);

	void* pDictExten = plg_DictExtenCreate();
	pJSON* root = pJson_CreateObject();

	plg_JobPattern((void*)t, tLen, (void*)k, kLen, (void*)ke, keLen, (void*)p, pLen, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		char* p = plg_DictExtenValue(dictNode, &valueLen);
		if (p[0] == LUA_TNUMBER) {
			lua_Number v;
			memcpy((char*)&v, (char*)SHELLING(p), SHELLING(valueLen));
			pJson_AddNumberToObject(root, plg_DictExtenKey(dictNode, &keyLen), v);
		} else if (p[0] == LUA_TSTRING) {
			sds s = plg_sdsNewLen(SHELLING(p), SHELLING(valueLen));
			pJson_AddStringToObject(root, plg_DictExtenKey(dictNode, &keyLen), s);
			plg_sdsFree(s);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LMultiGet2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictKeyExten, item->string, strlen(item->string), item->valuestring, strlen(item->valuestring));
		}
	}
	pJson_Delete(root);

	root = pJson_CreateObject();
	void* pDictExten = plg_DictExtenCreate();
	root = pJson_CreateObject();

	plg_JobMultiGet((void*)t, tLen, pDictKeyExten, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		char* p = plg_DictExtenValue(dictNode, &valueLen);
		if (p[0] == LUA_TNUMBER) {
			lua_Number v;
			memcpy((char*)&v, (char*)SHELLING(p), SHELLING(valueLen));
			pJson_AddNumberToObject(root, plg_DictExtenKey(dictNode, &keyLen), v);
		} else if (p[0] == LUA_TSTRING) {
			sds s = plg_sdsNewLen(SHELLING(p), SHELLING(valueLen));
			pJson_AddStringToObject(root, plg_DictExtenKey(dictNode, &keyLen), s);
			plg_sdsFree(s);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LRand2(lua_State* L) {

	NOTUSED(L);
	size_t tLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);

	const char* p = plg_JobRand((void*)t, tLen, &pLen);

	if (p[0] == LUA_TNUMBER) {
		lua_Number v;
		memcpy(&v, SHELLING(p), SHELLING(pLen));
		plg_Lvmpushnumber(_plVMHandle, v);
	} else if (p[0] == LUA_TSTRING) {
		plg_Lvmpushlstring(_plVMHandle, SHELLING(p), SHELLING(pLen));
	}

	return 1;
}

static int LSetAdd2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	char* v = plg_LvmMallocWithType(_plVMHandle, 3, &vLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobSAdd((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));

	free(v);
	return 1;
}

static int LSetMove2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, dkLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* dk = plg_Lvmchecklstring(_plVMHandle, 3, &dkLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, 4, &vLen);

	plg_JobSMove((void*)t, tLen, (void*)k, kLen, (void*)dk, dkLen, (void*)v, vLen);
	return 0;
}

static int LSetPop2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	const char* p = plg_JobSPop((void*)t, tLen, (void*)k, kLen, &pLen);

	if (p[0] == LUA_TNUMBER) {
		lua_Number v;
		memcpy(&v, SHELLING(p), SHELLING(pLen));
		plg_Lvmpushnumber(_plVMHandle, v);
	} else if (p[0] == LUA_TSTRING) {
		plg_Lvmpushlstring(_plVMHandle, SHELLING(p), SHELLING(pLen));
	}
	return 1;
}

static int LSetDel2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 3, &vLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictKeyExten, item->string, strlen(item->string), item->valuestring, strlen(item->valuestring));
		}
	}
	pJson_Delete(root);

	plg_JobSDel((void*)t, tLen, (void*)k, kLen, pDictKeyExten);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetUionStore2(lua_State* L) {
	
	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &vLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 3, &kLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictKeyExten, item->string, strlen(item->string), item->valuestring, strlen(item->valuestring));
		}
	}
	pJson_Delete(root);

	plg_JobSUionStore((void*)t, tLen, pDictKeyExten, (void*)k, kLen);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetInterStore2(lua_State* L) {
	
	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &vLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 3, &kLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictKeyExten, item->string, strlen(item->string), item->valuestring, strlen(item->valuestring));
		}
	}
	pJson_Delete(root);

	plg_JobSInterStore((void*)t, tLen, pDictKeyExten, (void*)k, kLen);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetDiffStore2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &vLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 3, &kLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictKeyExten, item->string, strlen(item->string), item->valuestring, strlen(item->valuestring));
		}
	}
	pJson_Delete(root);

	plg_JobSDiffStore((void*)t, tLen, pDictKeyExten, (void*)k, kLen);
	plg_DictExtenDestroy(pDictKeyExten);
	return 0;
}

static int LSetRang2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, kbLen, keLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* kb = plg_Lvmchecklstring(_plVMHandle, 3, &kbLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, 4, &keLen);

	void* pDictExten = plg_DictExtenCreate();
	pJSON* root = pJson_CreateObject();

	plg_JobSRang((void*)t, tLen, (void*)k, kLen, (void*)kb, kbLen, (void*)ke, keLen, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		char* p = plg_DictExtenValue(dictNode, &valueLen);
		if (p[0] == LUA_TNUMBER) {
			lua_Number v;
			memcpy((char*)&v, (char*)SHELLING(p), SHELLING(valueLen));
			pJson_AddNumberToObject(root, plg_DictExtenKey(dictNode, &keyLen), v);
		} else if (p[0] == LUA_TSTRING) {
			sds s = plg_sdsNewLen(SHELLING(p), SHELLING(valueLen));
			pJson_AddStringToObject(root, plg_DictExtenKey(dictNode, &keyLen), s);
			plg_sdsFree(s);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LSetLimite2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, 3, &vLen);
	lua_Integer l = plg_Lvmcheckinteger(_plVMHandle, 4);
	lua_Integer r = plg_Lvmcheckinteger(_plVMHandle, 5);

	void* pDictExten = plg_DictExtenCreate();
	pJSON* root = pJson_CreateObject();

	plg_JobSLimite((void*)t, tLen, (void*)k, kLen, (void*)v, vLen, l, r, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		char* p = plg_DictExtenValue(dictNode, &valueLen);
		if (p[0] == LUA_TNUMBER) {
			lua_Number v;
			memcpy((char*)&v, (char*)SHELLING(p), SHELLING(valueLen));
			pJson_AddNumberToObject(root, plg_DictExtenKey(dictNode, &keyLen), v);
		} else if (p[0] == LUA_TSTRING) {
			sds s = plg_sdsNewLen(SHELLING(p), SHELLING(valueLen));
			pJson_AddStringToObject(root, plg_DictExtenKey(dictNode, &keyLen), s);
			plg_sdsFree(s);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LSetLength2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobSLength((void*)t, tLen, (void*)k, kLen));
	return 1;
}

static int LSetIsKeyExist2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, vLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, 3, &vLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobSIsKeyExist((void*)t, tLen, (void*)k, kLen, (void*)v, vLen));
	return 1;
}

static int LSetMembers2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	void* pDictExten = plg_DictExtenCreate();
	pJSON* root = pJson_CreateObject();

	plg_JobSMembers((void*)t, tLen, (void*)k, kLen, pDictExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		char* p = plg_DictExtenValue(dictNode, &valueLen);
		if (p[0] == LUA_TNUMBER) {
			lua_Number v;
			memcpy((char*)&v, (char*)SHELLING(p), SHELLING(valueLen));
			pJson_AddNumberToObject(root, plg_DictExtenKey(dictNode, &keyLen), v);
		} else if (p[0] == LUA_TSTRING) {
			sds s = plg_sdsNewLen(SHELLING(p), SHELLING(valueLen));
			pJson_AddStringToObject(root, plg_DictExtenKey(dictNode, &keyLen), s);
			plg_sdsFree(s);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LSetRand2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	unsigned int pLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	const char* p = plg_JobSRand((void*)t, tLen, (void*)k, kLen, &pLen);

	if (p[0] == LUA_TNUMBER) {
		lua_Number v;
		memcpy(&v, SHELLING(p), SHELLING(pLen));
		plg_Lvmpushnumber(_plVMHandle, v);
	} else if (p[0] == LUA_TSTRING) {
		plg_Lvmpushlstring(_plVMHandle, SHELLING(p), SHELLING(pLen));
	}
	return 1;
}

static int LSetRangCount2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen, keLen, kbLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* k = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);
	const char* kb = plg_Lvmchecklstring(_plVMHandle, 3, &kbLen);
	const char* ke = plg_Lvmchecklstring(_plVMHandle, 4, &keLen);

	plg_Lvmpushnumber(_plVMHandle, (lua_Number)plg_JobSRangCount((void*)t, tLen, (void*)k, kLen, (void*)kb, kbLen, (void*)ke, keLen));
	return 1;
}

static int LSetUion2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictKeyExten, item->string, strlen(item->string), item->valuestring, strlen(item->valuestring));
		}
	}
	pJson_Delete(root);

	root = pJson_CreateObject();
	void* pDictExten = plg_DictExtenCreate();
	root = pJson_CreateObject();

	plg_JobSUion((void*)t, tLen, pDictExten, pDictKeyExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		char* p = plg_DictExtenValue(dictNode, &valueLen);
		if (p[0] == LUA_TNUMBER) {
			lua_Number v;
			memcpy((char*)&v, (char*)SHELLING(p), SHELLING(valueLen));
			pJson_AddNumberToObject(root, plg_DictExtenKey(dictNode, &keyLen), v);
		} else if (p[0] == LUA_TSTRING) {
			sds s = plg_sdsNewLen(SHELLING(p), SHELLING(valueLen));
			pJson_AddStringToObject(root, plg_DictExtenKey(dictNode, &keyLen), s);
			plg_sdsFree(s);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LSetInter2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictKeyExten, item->string, strlen(item->string), item->valuestring, strlen(item->valuestring));
		}
	}
	pJson_Delete(root);

	root = pJson_CreateObject();
	void* pDictExten = plg_DictExtenCreate();
	root = pJson_CreateObject();

	plg_JobSInter((void*)t, tLen, pDictExten, pDictKeyExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		char* p = plg_DictExtenValue(dictNode, &valueLen);
		if (p[0] == LUA_TNUMBER) {
			lua_Number v;
			memcpy((char*)&v, (char*)SHELLING(p), SHELLING(valueLen));
			pJson_AddNumberToObject(root, plg_DictExtenKey(dictNode, &keyLen), v);
		} else if (p[0] == LUA_TSTRING) {
			sds s = plg_sdsNewLen(SHELLING(p), SHELLING(valueLen));
			pJson_AddStringToObject(root, plg_DictExtenKey(dictNode, &keyLen), s);
			plg_sdsFree(s);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LSetDiff2(lua_State* L) {

	NOTUSED(L);
	size_t tLen, kLen;
	const char* t = plg_Lvmchecklstring(_plVMHandle, 1, &tLen);
	const char* json = plg_Lvmchecklstring(_plVMHandle, 2, &kLen);

	pJSON * root = pJson_Parse(json);
	if (!root) {
		elog(log_error, "json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pDictKeyExten = plg_DictExtenCreate();
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_String == item->type) {
			plg_DictExtenAdd(pDictKeyExten, item->string, strlen(item->string), item->valuestring, strlen(item->valuestring));
		}
	}
	pJson_Delete(root);

	root = pJson_CreateObject();
	void* pDictExten = plg_DictExtenCreate();
	root = pJson_CreateObject();

	plg_JobSDiff((void*)t, tLen, pDictExten, pDictKeyExten);

	void* dictIter = plg_DictExtenGetIterator(pDictExten);
	void* dictNode;
	while ((dictNode = plg_DictExtenNext(dictIter)) != NULL) {
		unsigned int keyLen=0, valueLen=0;
		char* p = plg_DictExtenValue(dictNode, &valueLen);
		if (p[0] == LUA_TNUMBER) {
			lua_Number v;
			memcpy((char*)&v, (char*)SHELLING(p), SHELLING(valueLen));
			pJson_AddNumberToObject(root, plg_DictExtenKey(dictNode, &keyLen), v);
		} else if (p[0] == LUA_TSTRING) {
			sds s = plg_sdsNewLen(SHELLING(p), SHELLING(valueLen));
			pJson_AddStringToObject(root, plg_DictExtenKey(dictNode, &keyLen), s);
			plg_sdsFree(s);
		}
	}
	plg_DictExtenReleaseIterator(dictIter);
	plg_DictExtenDestroy(pDictExten);
	plg_DictExtenDestroy(pDictKeyExten);

	char* out = pJson_Print(root);
	pJson_Delete(root);

	plg_Lvmpushstring(_plVMHandle, out);
	free(out);

	return 1;
}

static int LEventSend(lua_State* L) {

	NOTUSED(L);
	size_t hLen, vLen;
	const char* h = plg_Lvmchecklstring(_plVMHandle, 1, &hLen);
	const char* v = plg_Lvmchecklstring(_plVMHandle, 2, &vLen);

	unsigned int decsize;
	unsigned char* buff = plg_B64DecodeEx(h, hLen, &decsize);
	void* p;

	memcpy(&p, buff, decsize);
	unsigned long long up = (unsigned long long)p;
	if (up == 101021) {
		printf("%s\n", v);
	} else {
		plg_EventSend(p , v, vLen);
	}
	
	free(buff);
	return 0;
}

static luaL_Reg mylibs[] = {
	{ "NVersion", L_NVersion },
	{ "MVersion", L_MVersion },

	{ "RemoteCall", LRemoteCall },
	{ "LTimer", LTimer },
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
	{ "Pattern", LPattern },
	{ "MultiGet", LMultiGet },
	{ "Rand", LRand },

	{ "SetAdd", LSetAdd },
	{ "SetMove", LSetMove },
	{ "SetPop", LSetPop },
	{ "SetDel", LSetDel },
	{ "SetUionStore", LSetUionStore },
	{ "SetInterStore", LSetInterStore },
	{ "SetDiffStore", LSetDiffStore },

	{ "SetRang", LSetRang },
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


int plg_lualapilib(void* plVMHandle)
{
	if (plVMHandle == NULL) {
		elog(log_error, "plg_lualapilib.plVMHandle");
		return 0;
	}

	_plVMHandle = plVMHandle;
	const char *libName = "pelagia";
	plg_Lvmregister(plVMHandle, libName, mylibs);
	return 1;
}

#undef FillFun
#undef NORET