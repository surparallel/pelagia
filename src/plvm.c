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
#include "plauxlib.h"
#include "pelog.h"
#include "plibsys.h"
#include "plualib.h"
#include "plua.h"
#include "plapi.h"
#include "pelagia.h"

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
}*PlVMHandle, lVMHandle;

short plg_LvmSetLuaVersion(void* hInstance) {

	short version = 0;
	void* fun = (luaL_newstate)plg_SysLibSym(hInstance, "luaL_newstate");
	if (fun) {
		version = lua5_1;
	}

	fun = (luaL_newstate)plg_SysLibSym(hInstance, "luaL_checkunsigned");
	if (fun) {
		version = lua5_2;
	}

	fun = (luaL_newstate)plg_SysLibSym(hInstance, "lua_rotate");
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

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_getfield, NORET);
	plua_getfield(L, idx, k);
}

void plg_Lvmgetglobal(void* pvlVMHandle, void* L, const char * name) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_getglobal, NORET);
	plua_getglobal(L, name);
}


int plg_Lvmloadfile(void* pvlVMHandle, void* L, const char *filename) {

	PlVMHandle plVMHandle = pvlVMHandle;
	if (plVMHandle->luaVersion == lua5_1) {
		FillFun(plVMHandle->hInstance, luaL_loadfile, 0);
		return pluaL_loadfile(L, filename);
	} else {
		FillFun(plVMHandle->hInstance, luaL_loadfilex, 0);
		return pluaL_loadfilex(L, filename, NULL);
	}
}

int plg_Lvmpcall(void* pvlVMHandle, void* L, int nargs, int nresults, int errfunc) {

	PlVMHandle plVMHandle = pvlVMHandle;
	if (plVMHandle->luaVersion == lua5_1) {

		FillFun(plVMHandle->hInstance, lua_pcall, 0);
		return plua_pcall(L, nargs, nresults, errfunc);
	} else {
		FillFun(plVMHandle->hInstance, lua_pcallk, 0);
		return plua_pcallk(L, nargs, nresults, errfunc, 0 , NULL);
	}
}

void plg_Lvmpushlstring(void* pvlVMHandle, void* L, const char *s, size_t l) {
	
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_pushlstring, NORET);
	plua_pushlstring(L, s, l);
}

int plg_Lvmisnumber(void* pvlVMHandle, void* L, int idx) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_isnumber, 0);
	return plua_isnumber(L, idx);
}

lua_Number plg_Lvmtonumber(void* pvlVMHandle, void* L, int idx) {

	PlVMHandle plVMHandle = pvlVMHandle;
	if (plVMHandle->luaVersion == lua5_1) {
		FillFun(plVMHandle->hInstance, lua_tonumber, 0);
		return plua_tonumber(L, idx);
	} else {
		FillFun(plVMHandle->hInstance, lua_tonumberx, 0);
		return plua_tonumberx(L, idx, NULL);
	}
}

void plg_Lvmsettop(void* pvlVMHandle, void* L, int idx) {
	
	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_settop, NORET);
	plua_settop(L, idx);
}

const char* plg_Lvmtolstring(void* pvlVMHandle, void* L, int idx, size_t *len) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_tolstring, 0);
	return plua_tolstring(L, idx, len);
}

int plg_Lvmtype(void* pvlVMHandle, void* L, int idx) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_type, 0);
	return plua_type(L, idx);
}

lua_Number plg_Lvmchecknumber(void* pvlVMHandle, void* L, int numArg) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, luaL_checknumber, 0);
	return pluaL_checknumber(L, numArg);
}

const char * plg_Lvmchecklstring(void* pvlVMHandle, void* L, int numArg, size_t *l) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, luaL_checklstring, 0);
	return pluaL_checklstring(L, numArg, l);
}

void plg_Lvmpushlightuserdata(void* pvlVMHandle, void* L, void *p) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_pushlightuserdata, NORET);
	plua_pushlightuserdata(L, p);
}

void plg_Lvmpushstring(void* pvlVMHandle, void* L, const char *s) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_pushstring, NORET);
	plua_pushstring(L, s);
}

void plg_Lvmpushnil(void* pvlVMHandle, void* L) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_pushnil, NORET);
	plua_pushnil(L);
}

void plg_Lvmsettable(void* pvlVMHandle, void* L, int idx) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_settable, NORET);
	plua_settable(L, idx);
}

void plg_Lvmpushnumber(void* pvlVMHandle, void* L, double n) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_pushnumber, NORET);
	plua_pushnumber(L, n);
}

long long plg_Lvmcheckinteger(void* pvlVMHandle, void* L, int numArg) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, luaL_checkinteger, 0);
	return pluaL_checkinteger(L, numArg);
}

void plg_Lvmregister(void* pvlVMHandle, void* L, const char *libname, const luaL_Reg *l) {
	PlVMHandle plVMHandle = pvlVMHandle;

	if (plVMHandle->luaVersion == lua5_1) {
		FillFun(plVMHandle->hInstance, luaL_register, NORET);
		pluaL_register(L, libname, l);
	} else {
		FillFun(plVMHandle->hInstance, lua_createtable, NORET);
		plua_createtable(L, 0, -1);
		FillFun(plVMHandle->hInstance, luaL_setfuncs, NORET);
		pluaL_setfuncs(L, l, 0);
	}
}

void plg_LvmRequiref(void* pvlVMHandle, const char *modname, lua_CFunction openf, int glb) {
	PlVMHandle plVMHandle = pvlVMHandle;

	if (plVMHandle->luaVersion == lua5_2 || plVMHandle->luaVersion == lua5_3)  {
		FillFun(plVMHandle->hInstance, luaL_requiref, NORET);
		pluaL_requiref(plVMHandle->luaVM, modname, openf, glb);
	}
}

void* plg_LvmLoad(const char *path) {

	void* hInstance = plg_SysLibLoad(path, 1);
	if (hInstance == NULL) {
		elog(log_error, "plg_LvmLoad.plg_SysLibLoad:%s", path);
		plg_SysLibUnload(hInstance);
		return 0;
	}

	short version = plg_LvmSetLuaVersion(hInstance);
	if (version == 0) {
		elog(log_error, "plg_LvmLoad.plg_LvmSetLuaVersion: Unsupported Lua version");
		plg_SysLibUnload(hInstance);
		return 0;
	}

	luaL_newstate funNewstate = (luaL_newstate)plg_SysLibSym(hInstance, "luaL_newstate");
	if (!funNewstate) {
		elog(log_error, "plg_LvmLoad.plg_SysLibSym.luaL_newstate");
		return 0;
	}

	lua_State * luaVM = (*funNewstate)();

	luaL_openlibs funOpenlibs = (luaL_openlibs)plg_SysLibSym(hInstance, "luaL_openlibs");
	if (!funOpenlibs) {
		elog(log_error, "plg_LvmLoad.plg_SysLibSym.luaL_openlibs");
		return 0;
	}

	funOpenlibs(luaVM);	

	PlVMHandle plVMHandle = malloc(sizeof(lVMHandle));
	plVMHandle->hInstance = hInstance;
	plVMHandle->luaVM = luaVM;
	plVMHandle->luaVersion = version;
	
	plg_lualapilib(plVMHandle);
	return plVMHandle;
}

void plg_LvmDestory(void* pvlVMHandle) {

	PlVMHandle plVMHandle = pvlVMHandle;
	if (plVMHandle == 0) {
		return;
	}

	lua_close funClose = (lua_close)plg_SysLibSym(plVMHandle->hInstance, "lua_close");
	if (!funClose) {
		elog(log_error, "plg_LvmLoad.plg_SysLibSym.lua_close");
	}

	funClose(plVMHandle->luaVM);

	plg_SysLibUnload(plVMHandle->hInstance);
	free(plVMHandle);
}

int plg_LvmCallFile(void* pvlVMHandle, char* file, char* fun, void* value, short len) {

	PlVMHandle plVMHandle = pvlVMHandle;
	if (plg_Lvmloadfile(plVMHandle, plVMHandle->luaVM, file)){
		elog(log_error, "plg_LvmCallFile.pluaL_loadfilex:%s", plg_Lvmtolstring(pvlVMHandle, plVMHandle->luaVM, -1, NULL));
		plg_Lvmsettop(plVMHandle, plVMHandle->luaVM, 0);
		return 0;
	}

	//load fun
	if (plg_Lvmpcall(pvlVMHandle, plVMHandle->luaVM, 0, LUA_MULTRET, 0)) {
		elog(log_error, "plg_LvmCallFile.plua_pcall:%s lua:%s", file, plg_Lvmtolstring(pvlVMHandle, plVMHandle->luaVM, -1, NULL));
		plg_Lvmsettop(plVMHandle, plVMHandle->luaVM, 0);
		return 0;
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
	}

	return 0;
}

#undef FillFun
#undef NORET