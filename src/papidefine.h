/* papidefine.h - apiºêµÄ¶¨Òå
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

/*
@@ PELAGIA_API is a mark for all core API functions.
@@ PELAGIALIB_API is a mark for all auxiliary library functions.
@@ PELAGIAMOD_API is a mark for all standard library opening functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** PELAGIA_BUILD_AS_DLL to get it).
*/

#ifndef __APIDEFINE_H
#define __APIDEFINE_H

#if defined(PLG_BUILD_AS_DLL)

#if defined(PELAGIA_CORE) || defined(PELAGIA_LIB)
#define PELAGIA_API __declspec(dllexport)
#else
#define PELAGIA_API __declspec(dllimport)
#endif

#else
#define PELAGIA_API		extern
#endif


#if defined(STATIC_LUA_AS_DLL)
#define LUA_API __declspec(dllimport)
#else
#define LUA_API		extern
#endif

#endif