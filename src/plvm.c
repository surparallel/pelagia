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

typedef struct _lVMHandle
{
	void* hInstance;//dll handle
	lua_State *luaVM;//lua handle
}*PlVMHandle, lVMHandle;

void* plg_LvmGetInstance(void* pvlVMHandle) {
	PlVMHandle plVMHandle = pvlVMHandle;
	return plVMHandle->hInstance;
}

void* plg_LvmGetL(void* pvlVMHandle) {
	PlVMHandle plVMHandle = pvlVMHandle;
	return plVMHandle->luaVM;
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

int plg_Lvmloadfilex(void* pvlVMHandle, void* L, const char *filename) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, luaL_loadfile, 0);
	return pluaL_loadfile(L, filename);
}

int plg_Lvmpcall(void* pvlVMHandle, void* L, int nargs, int nresults, int errfunc) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, lua_pcall, 0);
	return plua_pcall(L, nargs, nresults, errfunc);
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
	FillFun(plVMHandle->hInstance, lua_tonumber, 0);
	return plua_tonumber(L, idx);
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

void plg_Lvmregister(void* pvlVMHandle, const char *libname, const luaL_Reg *l) {

	PlVMHandle plVMHandle = pvlVMHandle;
	FillFun(plVMHandle->hInstance, luaL_register, NORET);
	pluaL_register(plVMHandle->luaVM, libname, l);
}

void* plg_LvmLoad(const char *path) {

	void* hInstance = plg_SysLibLoad(path, 1);
	if (hInstance == NULL) {
		elog(log_error, "plg_LvmLoad.plg_SysLibLoad:%s", path);
		return 0;
	}

	luaL_newstate funNewstate = (luaL_newstate)plg_SysLibSym(hInstance, "luaL_newstate");
	if (!funNewstate) {
		elog(log_error, "plg_LvmLoad.plg_SysLibSym.luaL_newstate");
		return 0;
	}

	lua_State *luaVM = (*funNewstate)();

	luaL_openlibs funOpenlibs = (luaL_openlibs)plg_SysLibSym(hInstance, "luaL_openlibs");
	if (!funOpenlibs) {
		elog(log_error, "plg_LvmLoad.plg_SysLibSym.luaL_openlibs");
		return 0;
	}

	funOpenlibs(luaVM);
	

	PlVMHandle plVMHandle = malloc(sizeof(lVMHandle));
	plVMHandle->hInstance = hInstance;
	plVMHandle->luaVM = luaVM;
	
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
	if (plg_Lvmloadfilex(plVMHandle, plVMHandle->luaVM, file)){
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
	plg_Lvmgetfield(pvlVMHandle, plVMHandle->luaVM, LUA_GLOBALSINDEX, fun);
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

void* plg_LvmMallocForBuf(void* p, int len, char type) {
	char* r = 0;
	r = malloc(len + 1);
	r[0] = type;
	memcpy((r + 1), p, len);
	return r;
}

void* plg_LvmMallocWithType(void* plVMHandle, void* L, int nArg, size_t* len) {

	int t = plg_Lvmtype(plVMHandle, L, nArg);
	char* p = 0;

	if (t == LUA_TNUMBER) {
		*len = sizeof(lua_Number) + 1;
		p = malloc(*len);
		lua_Number r = plg_Lvmchecknumber(plVMHandle, L, nArg);
		memcpy((p + 1), (char*)&r, sizeof(lua_Number));
		p[0] = LUA_TNUMBER;

		return p;
	} else if (t == LUA_TSTRING) {
		size_t sLen;
		const char* s = plg_Lvmchecklstring(plVMHandle, L, nArg, &sLen);
		*len = sLen + 1;
		p = malloc(*len);

		memcpy((p + 1), s, sLen);
		p[0] = LUA_TSTRING;

		return p;
	}

	return 0;
}


#undef FillFun
#undef NORET