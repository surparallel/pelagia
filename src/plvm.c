/* lvm.c - load lua
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
#include "plvm.h"
#include "pelog.h"
#include "plibsys.h"
#include "plapi.h"
#include "pelagia.h"
#include "pdict.h"
#include "psds.h"
#include "pfilesys.h"

/*
#define LUA_VERSION_NUM 501
#define STATIC_LUA
#define STATIC_LUA_AS_DLL
*/
#include "papidefine.h"
#include "plauxlib.h"
#include "plualib.h"
#include "plua.h"

static void sdsFreeCallback(void *privdata, void *val) {
	DICT_NOTUSED(privdata);
	plg_sdsFree(val);
}

static int sdsCompareCallback(void *privdata, const void *key1, const void *key2) {
	int l1, l2;
	NOTUSED(privdata);

	l1 = plg_sdsLen((sds)key1);
	l2 = plg_sdsLen((sds)key2);
	if (l1 != l2) return 0;
	return memcmp(key1, key2, l1) == 0;
}

static unsigned long long sdsHashCallback(const void *key) {
	return plg_dictGenHashFunction((unsigned char*)key, plg_sdsLen((char*)key));
}

static dictType SdsDictType = {
	sdsHashCallback,
	NULL,
	NULL,
	sdsCompareCallback,
	sdsFreeCallback,
	NULL
};

enum LuaVersion {
	lua5_1 = 1,
	lua5_2,
	lua5_3
};
typedef struct _lVMHandle
{
	void* hInstance;//dll handle
	lua_State *luaVM;//lua handle
	short luaVersion;
	short luaHot;
	dict* lua_file;
}*PlVMHandle, lVMHandle;

short plg_LvmSetLuaVersion(void* hInstance) {

	short version = 0;
	void* fun = (void*)plg_SysLibSym(hInstance, "luaL_newstate");
	if (fun) {
		version = lua5_1;
	}

	fun = (void*)plg_SysLibSym(hInstance, "luaL_checkunsigned");
	if (fun) {
		version = lua5_2;
	}

	fun = (void*)plg_SysLibSym(hInstance, "lua_rotate");
	if (fun) {
		version = lua5_3;
	}

	return version;
}

void* plg_LvmGetInstance(void* pvlVMHandle) {
	PlVMHandle plVMHandle = pvlVMHandle;
	return plVMHandle->hInstance;
}

void* plg_LvmGetL(void* pvlVMHandle) {
	PlVMHandle plVMHandle = pvlVMHandle;
	return plVMHandle->luaVM;
}

short plg_LvmGetV(void* pvlVMHandle) {
	PlVMHandle plVMHandle = pvlVMHandle;
	return plVMHandle->luaVersion;
}

void plg_LvmSetL(void* pvlVMHandle, void* L) {
	PlVMHandle plVMHandle = pvlVMHandle;
	plVMHandle->luaVM = L;
}

void* plg_LvmCheckSym(void *lib, const char *sym) {

	if (!lib) {
		elog(log_error, "plg_LvmCheckSym.lib");
		return 0;
	}

	void* fun = plg_SysLibSym(lib, sym);
	if (!fun) {
		elog(log_error, "plg_LvmCheckSym.plg_SysLibSym:%s", sym);
		return 0;
	} else {
		return fun;
	}
}

#define NORET
#define FillFun(h, n, r)n p##n = plg_LvmCheckSym(h, #n);if (!p##n) {return r;}

void plg_Lvmgetfield(void* pvlVMHandle, void* L, int idx, const char *k) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_getfield, NORET);
	plua_getfield(L, idx, k);
#else
	lua_getfield(L, idx, k);
#endif
}

void plg_Lvmgetglobal(void* pvlVMHandle, void* L, const char * name) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_getglobal, NORET);
	plua_getglobal(L, name);
#else
	lua_getglobal(L, name);
#endif
}


int plg_Lvmloadfile(void* pvlVMHandle, void* L, const char *filename) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	if (plVMHandle->luaVersion == lua5_1) {
		FillFun(plVMHandle->hInstance, luaL_loadfile, 0);
		return pluaL_loadfile(L, filename);
	} else {
		FillFun(plVMHandle->hInstance, luaL_loadfilex, 0);
		return pluaL_loadfilex(L, filename, NULL);
	}
#else
#if LUA_VERSION_NUM == 501
	return luaL_loadfile(L, filename);
#else
	return luaL_loadfilex(L, filename, NULL);
#endif
#endif
}

int plg_Lvmpcall(void* pvlVMHandle, void* L, int nargs, int nresults, int errfunc) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	if (plVMHandle->luaVersion == lua5_1) {

		FillFun(plVMHandle->hInstance, lua_pcall, 0);
		return plua_pcall(L, nargs, nresults, errfunc);
	} else {
		FillFun(plVMHandle->hInstance, lua_pcallk, 0);
		return plua_pcallk(L, nargs, nresults, errfunc, 0 , NULL);
	}
#else
#if LUA_VERSION_NUM == 501
	return lua_pcall(L, nargs, nresults, errfunc);
#else
	return lua_pcallk(L, nargs, nresults, errfunc, 0, NULL);
#endif
#endif
}

void plg_Lvmpushlstring(void* pvlVMHandle, void* L, const char *s, size_t l) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_pushlstring, NORET);
	plua_pushlstring(L, s, l);
#else
	lua_pushlstring(L, s, l);
#endif
}

int plg_Lvmisnumber(void* pvlVMHandle, void* L, int idx) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_isnumber, 0);
	return plua_isnumber(L, idx);
#else
	return lua_isnumber(L, idx);
#endif
}

lua_Number plg_Lvmtonumber(void* pvlVMHandle, void* L, int idx) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	if (plVMHandle->luaVersion == lua5_1) {
		FillFun(plVMHandle->hInstance, lua_tonumber, 0);
		return plua_tonumber(L, idx);
	} else {
		FillFun(plVMHandle->hInstance, lua_tonumberx, 0);
		return plua_tonumberx(L, idx, NULL);
	}
#else
#if LUA_VERSION_NUM == 501
	return lua_tonumber(L, idx);
#else
	return lua_tonumberx(L, idx, NULL);
#endif
#endif
}

void plg_Lvmsettop(void* pvlVMHandle, void* L, int idx) {

#ifndef STATIC_LUA	
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_settop, NORET);
	plua_settop(L, idx);
#else
	lua_settop(L, idx);
#endif
}

const char* plg_Lvmtolstring(void* pvlVMHandle, void* L, int idx, size_t *len) {

#ifndef STATIC_LUA	
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_tolstring, 0);
	return plua_tolstring(L, idx, len);
#else
	return lua_tolstring(L, idx, len);
#endif
}

int plg_Lvmtype(void* pvlVMHandle, void* L, int idx) {

#ifndef STATIC_LUA	
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_type, 0);
	return plua_type(L, idx);
#else
	return lua_type(L, idx);
#endif
}

lua_Number plg_Lvmchecknumber(void* pvlVMHandle, void* L, int numArg) {

#ifndef STATIC_LUA	
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, luaL_checknumber, 0);
	return pluaL_checknumber(L, numArg);
#else
	return luaL_checknumber(L, numArg);
#endif
}

const char * plg_Lvmchecklstring(void* pvlVMHandle, void* L, int numArg, size_t *l) {

#ifndef STATIC_LUA	
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, luaL_checklstring, 0);
	return pluaL_checklstring(L, numArg, l);
#else
	return luaL_checklstring(L, numArg, l);
#endif
}

void plg_Lvmpushlightuserdata(void* pvlVMHandle, void* L, void *p) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_pushlightuserdata, NORET);
	plua_pushlightuserdata(L, p);
#else
	lua_pushlightuserdata(L, p);
#endif
}

void plg_Lvmpushstring(void* pvlVMHandle, void* L, const char *s) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_pushstring, NORET);
	plua_pushstring(L, s);
#else
	lua_pushstring(L, s);
#endif
}

void plg_Lvmpushnil(void* pvlVMHandle, void* L) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_pushnil, NORET);
	plua_pushnil(L);
#else
	lua_pushnil(L);
#endif
}

void plg_Lvmsettable(void* pvlVMHandle, void* L, int idx) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_settable, NORET);
	plua_settable(L, idx);
#else
	lua_settable(L, idx);
#endif
}

void plg_Lvmpushnumber(void* pvlVMHandle, void* L, double n) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_pushnumber, NORET);
	plua_pushnumber(L, n);
#else
	lua_pushnumber(L, n);
#endif
}

long long plg_Lvmcheckinteger(void* pvlVMHandle, void* L, int numArg) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, luaL_checkinteger, 0);
	return pluaL_checkinteger(L, numArg);
#else
	return luaL_checkinteger(L, numArg);
#endif
}

void plg_Lvmregister(void* pvlVMHandle, void* L, const char *libname, void* l) {
	PlVMHandle plVMHandle = pvlVMHandle;
	luaL_Reg * lr = (luaL_Reg *)l;
#ifndef STATIC_LUA
	if (plVMHandle->luaVersion == lua5_1) {
		FillFun(plVMHandle->hInstance, luaL_register, NORET);
		pluaL_register(L, libname, lr);
	} else {
		FillFun(plVMHandle->hInstance, lua_createtable, NORET);
		plua_createtable(L, 0, -1);
		FillFun(plVMHandle->hInstance, luaL_setfuncs, NORET);
		pluaL_setfuncs(L, lr, 0);
	}
#else
#if LUA_VERSION_NUM == 501
	luaL_register(L, libname, l);
#else
	lua_createtable(L, 0, -1);
	luaL_setfuncs(L, l, 0);
#endif
#endif
}

void plg_Lvmcreatetable(void* pvlVMHandle, void *L, int narr, int nrec) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_createtable, NORET);
	plua_createtable(L, narr, nrec);
#else
	lua_createtable(L, narr, nrec);
#endif
}

int plg_Lvmnext(void* pvlVMHandle, void *L, int idx) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_next, 0);
	return plua_next(L, idx);
#else
	return lua_next(L, idx);
#endif
}

void plg_LvmRequiref(void* pvlVMHandle, const char *modname, void* openf, int glb) {
	PlVMHandle plVMHandle = pvlVMHandle;
	lua_CFunction lcfun = (lua_CFunction)openf;
#ifndef STATIC_LUA
	if (plVMHandle->luaVersion == lua5_2 || plVMHandle->luaVersion == lua5_3)  {
		FillFun(plVMHandle->hInstance, luaL_requiref, NORET);
		pluaL_requiref(plVMHandle->luaVM, modname, lcfun, glb);
	}
#else
#if LUA_VERSION_NUM != 501
	luaL_requiref(plVMHandle->luaVM, modname, openf, glb);
#endif
#endif
}

lua_State * plg_Lvmnewstate(void* pvlVMHandle) {

#ifndef STATIC_LUA
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, luaL_newstate, 0);
	return pluaL_newstate();
#else
	return luaL_newstate();
#endif
}

void plg_Lvmopenlibs(void* pvlVMHandle) {

	PlVMHandle plVMHandle = pvlVMHandle;
#ifndef STATIC_LUA
	
	FillFun(plVMHandle->hInstance, luaL_openlibs, NORET);
	pluaL_openlibs(plVMHandle->luaVM);
#else
	luaL_openlibs(plVMHandle->luaVM);
#endif
}

void plg_Lvmclose(void* pvlVMHandle) {

	PlVMHandle plVMHandle = pvlVMHandle;
#ifndef STATIC_LUA

	FillFun(plVMHandle->hInstance, lua_close, NORET);
	plua_close(plVMHandle->luaVM);
#else
	lua_close(plVMHandle->luaVM);
#endif
}

void* plg_LvmLoad(const char *path, short luaHot) {

	void* hInstance = 0;
	short version = 0;

#ifndef STATIC_LUA
	sds sdsPath = plg_sdsNew(path);
	if (0 != access_t(sdsPath, 0)) {
		sdsPath = plg_sdsCat(sdsPath, LIB_EXT);
	}

	hInstance = plg_SysLibLoad(sdsPath, 1);
	plg_sdsFree(sdsPath);
	if (hInstance == NULL) {
		elog(log_error, "plg_LvmLoad.plg_SysLibLoad:%s", path);
		return 0;
	}

	version = plg_LvmSetLuaVersion(hInstance);
	if (version == 0) {
		elog(log_error, "plg_LvmLoad.plg_LvmSetLuaVersion: Unsupported Lua version");
		plg_SysLibUnload(hInstance);
		return 0;
	}
#endif

	PlVMHandle plVMHandle = malloc(sizeof(lVMHandle));
	plVMHandle->hInstance = hInstance;
	plVMHandle->luaVersion = version;
	plVMHandle->lua_file = plg_dictCreate(&SdsDictType, NULL, DICT_MIDDLE);
	plVMHandle->luaHot = luaHot;
	plVMHandle->luaVM = plg_Lvmnewstate(plVMHandle);

	plg_Lvmopenlibs(plVMHandle);
	plg_lualapilib(plVMHandle);
	return plVMHandle;
	
}

void plg_LvmDestory(void* pvlVMHandle) {

	PlVMHandle plVMHandle = pvlVMHandle;
	if (plVMHandle == 0) {
		return;
	}

	plg_Lvmclose(plVMHandle);

#ifndef STATIC_LUA
	plg_SysLibUnload(plVMHandle->hInstance);
#endif

	plg_dictRelease(plVMHandle->lua_file);
	free(plVMHandle);
}

int plg_LvmCallFile(void* pvlVMHandle, char* sdsFile, char* fun, void* value, short len) {

	PlVMHandle plVMHandle = pvlVMHandle;

	if (plVMHandle->luaHot) {
		if (plg_Lvmloadfile(plVMHandle, plVMHandle->luaVM, sdsFile)){
			elog(log_error, "plg_LvmCallFile.pluaL_loadfilex:%s", plg_Lvmtolstring(pvlVMHandle, plVMHandle->luaVM, -1, NULL));
			plg_Lvmsettop(plVMHandle, plVMHandle->luaVM, 0);
			return 0;
		}

		//load fun
		if (plg_Lvmpcall(pvlVMHandle, plVMHandle->luaVM, 0, LUA_MULTRET, 0)) {
			elog(log_error, "plg_LvmCallFile.plua_pcall:%s lua:%s", sdsFile, plg_Lvmtolstring(pvlVMHandle, plVMHandle->luaVM, -1, NULL));
			plg_Lvmsettop(plVMHandle, plVMHandle->luaVM, 0);
			return 0;
		}
	} else {
		dictEntry* entry = plg_dictFind(plVMHandle->lua_file, sdsFile);
		if (!entry) {
			if (plg_Lvmloadfile(plVMHandle, plVMHandle->luaVM, sdsFile)){
				elog(log_error, "plg_LvmCallFile.pluaL_loadfilex:%s", plg_Lvmtolstring(pvlVMHandle, plVMHandle->luaVM, -1, NULL));
				plg_Lvmsettop(plVMHandle, plVMHandle->luaVM, 0);
				return 0;
			}

			//load fun
			if (plg_Lvmpcall(pvlVMHandle, plVMHandle->luaVM, 0, LUA_MULTRET, 0)) {
				elog(log_error, "plg_LvmCallFile.plua_pcall:%s lua:%s", sdsFile, plg_Lvmtolstring(pvlVMHandle, plVMHandle->luaVM, -1, NULL));
				plg_Lvmsettop(plVMHandle, plVMHandle->luaVM, 0);
				return 0;
			}
			sds newFile = plg_sdsNew(sdsFile);
			plg_dictAdd(plVMHandle->lua_file, newFile, 0);
		}
	}

	//call fun
	if (plVMHandle->luaVersion == lua5_1) {
		plg_Lvmgetfield(pvlVMHandle, plVMHandle->luaVM, LUA_GLOBALSINDEX, fun);
	} else {
		plg_Lvmgetglobal(pvlVMHandle, plVMHandle->luaVM, fun);
	}
	plg_Lvmpushlstring(pvlVMHandle, plVMHandle->luaVM, value, len);

	if (plg_Lvmpcall(pvlVMHandle, plVMHandle->luaVM, 1, LUA_MULTRET, 0)) {
		elog(log_error, "plg_LvmCallFile.plua_pcall:%s lua:%s", fun, plg_Lvmtolstring(plVMHandle, plVMHandle->luaVM, -1, NULL));
		plg_Lvmsettop(pvlVMHandle, plVMHandle->luaVM, 0);
		return 0;
	}

	double ret = 1;
	if (plg_Lvmisnumber(pvlVMHandle, plVMHandle->luaVM, -1)) {
		ret = plg_Lvmtonumber(pvlVMHandle, plVMHandle->luaVM, -1);
	}

	//clear lua --lua_pop(L,1) lua_settop(L, -(n)-1)
	plg_Lvmsettop(pvlVMHandle, plVMHandle->luaVM, 0);
	return ret;
}

void* plg_LvmMallocWithType(void* plVMHandle, void* L, int nArg, size_t* len, unsigned short *tt) {

	int t = plg_Lvmtype(plVMHandle, L, nArg);
	char* p = 0;

	if (t == LUA_TNUMBER) {
		*len = sizeof(lua_Number);
		p = malloc(*len);
		lua_Number r = plg_Lvmchecknumber(plVMHandle, L, nArg);
		memcpy(p, (char*)&r, sizeof(lua_Number));
		*tt = TT_Double;

		return p;
	} else if (t == LUA_TSTRING) {
		size_t sLen;
		const char* s = plg_Lvmchecklstring(plVMHandle, L, nArg, &sLen);
		*len = sLen;
		p = malloc(*len);

		memcpy(p, s, sLen);
		*tt = TT_String;

		return p;
	} else {
		*len = 0;
		*tt = TT_Byte;
	}

	return 0;
}

int plg_LvmTableNext(void* plVMHandle, void* L, int i, char **k, char **sv, double *dv) {
	if (plg_Lvmnext(plVMHandle, L, i) != 0)
	{
		size_t len;
		if (plg_Lvmtype(plVMHandle, L, -2) == LUA_TSTRING) {
			*k = (char *)plg_Lvmtolstring(plVMHandle, L, -2, &len);
		} else if (plg_Lvmtype(plVMHandle, L, -2) == LUA_TNUMBER) {
			long long kd = plg_Lvmtonumber(plVMHandle, L, -2);
			*k = (void*) kd;
		}
		
		if (plg_Lvmtype(plVMHandle, L, -1) == LUA_TSTRING) {
			*sv = (char *)plg_Lvmtolstring(plVMHandle, L, -1, &len);
		} else if (plg_Lvmtype(plVMHandle, L, -1) == LUA_TNUMBER) {
			*dv = plg_Lvmtonumber(plVMHandle, L, -1);
		}
		
		//clear lua --lua_pop(L,1) lua_settop(L, -(n)-1)
		plg_Lvmsettop(plVMHandle, L, -2);
		return 1;
	} else
		return 0;
}

#undef FillFun
#undef NORET