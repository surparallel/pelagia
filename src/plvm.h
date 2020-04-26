/* lvm.h
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
#ifndef __LVM_H
#define __LVM_H

#include "plauxlib.h"

#define FillFun(h, n, r)n p##n = plg_LvmCheckSym(h, #n);if (!p##n) {return r;}

void* plg_LvmLoad(const char *path, short luaHot);
void plg_LvmDestory(void* plVMHandle);
int plg_LvmCallFile(void* plVMHandle, char* sdsFile, char* fun, void* value, short len);
void* plg_LvmCheckSym(void *lib, const char *sym);
void* plg_LvmGetInstance(void* plVMHandle);
void* plg_LvmGetL(void* plVMHandle);
void plg_LvmSetL(void* pvlVMHandle, void* L);
void plg_Lvmregister(void* pvlVMHandle, void* L, const char *libname, const luaL_Reg *l);
short plg_LvmGetV(void* plVMHandle);

//lua api
void* plg_LvmMallocWithType(void* plVMHandle, void* L, int nArg, size_t* len, unsigned short *tt);
void plg_Lvmgetfield(void* pvlVMHandle, void* L, int idx, const char *k);
int plg_Lvmloadfile(void* pvlVMHandle, void* L, const char *filename);
int plg_Lvmpcall(void* pvlVMHandle, void* L, int nargs, int nresults, int errfunc);
void plg_Lvmpushlstring(void* pvlVMHandle, void* L, const char *s, size_t l);
int plg_Lvmisnumber(void* pvlVMHandle, void* L, int idx);
double plg_Lvmtonumber(void* pvlVMHandle, void* L, int idx);
void plg_Lvmsettop(void* pvlVMHandle, void* L, int idx);
const char* plg_Lvmtolstring(void* pvlVMHandle, void* L, int idx, size_t *len);
int plg_Lvmtype(void* pvlVMHandle, void* L, int idx);
double plg_Lvmchecknumber(void* pvlVMHandle, void* L, int numArg);
const char * plg_Lvmchecklstring(void* pvlVMHandle, void* L, int numArg, size_t *l);
void plg_Lvmpushlightuserdata(void* pvlVMHandle, void* L, void *p);
void plg_Lvmpushstring(void* pvlVMHandle, void* L, const char *s);
void plg_Lvmsettable(void* pvlVMHandle, void* L, int idx);
void plg_Lvmpushnumber(void* pvlVMHandle, void* L, double n);
long long plg_Lvmcheckinteger(void* pvlVMHandle, void* L, int numArg);
void plg_Lvmpushnil(void* pvlVMHandle, void* L);

void plg_LvmRequiref(void* pvlVMHandle, const char *modname, lua_CFunction openf, int glb);

#endif