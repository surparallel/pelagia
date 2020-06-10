/*
** Standard library header.
** Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LUALIB_H
#define _LUALIB_H

#include "plua.h"

#define LUA_FILEHANDLE	"FILE*"

#define LUA_COLIBNAME	"coroutine"
#define LUA_MATHLIBNAME	"math"
#define LUA_STRLIBNAME	"string"
#define LUA_TABLIBNAME	"table"
#define LUA_IOLIBNAME	"io"
#define LUA_OSLIBNAME	"os"
#define LUA_LOADLIBNAME	"package"
#define LUA_DBLIBNAME	"debug"
#define LUA_BITLIBNAME	"bit"
#define LUA_JITLIBNAME	"jit"
#define LUA_FFILIBNAME	"ffi"

typedef int (*luaopen_base)(lua_State *L);
typedef int (*luaopen_math)(lua_State *L);
typedef int (*luaopen_string)(lua_State *L);
typedef int (*luaopen_table)(lua_State *L);
typedef int (*luaopen_io)(lua_State *L);
typedef int (*luaopen_os)(lua_State *L);
typedef int (*luaopen_package)(lua_State *L);
typedef int (*luaopen_debug)(lua_State *L);
typedef int (*luaopen_bit)(lua_State *L);
typedef int (*luaopen_jit)(lua_State *L);
typedef int (*luaopen_ffi)(lua_State *L);

#ifdef STATIC_LUA
void luaL_openlibs (lua_State *L);
#else
typedef void (*luaL_openlibs)(lua_State *L);
#endif

#ifndef lua_assert
#define lua_assert(x)	((void)0)
#endif

#endif
