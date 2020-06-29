/* filesys.h - File system related
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
#ifndef __FILESYS_H
#define __FILESYS_H

#ifdef _WIN32
#ifdef _WIN64
#define fopen_t fopen
#define fseek_t _fseeki64
#define ftell_t _ftelli64
#else
#define fopen_t fopen
#define fseek_t fseek
#define ftell_t ftell
#endif // _WIN64
#else
#ifdef __APPLE__
#ifdef __LP64__
#define fopen_t fopen
#define fseek_t fseeko
#define ftell_t ftello
#else
#define fopen_t fopen
#define fseek_t fseek
#define ftell_t ftell
#endif // __LP64__
#else
#ifdef __LP64__
#define fopen_t fopen64
#define fseek_t fseeko64
#define ftell_t ftello64
#else
#define fopen_t fopen
#define fseek_t fseek
#define ftell_t ftell
#endif // __LP64__
#endif
#endif // _WIN32

#ifdef _WIN32
#define access_t  _access
#define getcwd_t _getcwd
#define chdir_t _chdir

#define PATH_DIV "\\"
#else
#define access_t access
#define getcwd_t getcwd
#define chdir_t chdir

#define PATH_DIV "/"
#endif

void plg_MkDirs(char *muldir);
short plg_SysSetFileLength(void* file, unsigned long long len);
#endif