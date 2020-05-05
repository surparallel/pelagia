/* plateform_type.h - 
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

Complier		windows vc12		linux gcc-5.3.1
Target			win32   x64			i686		x86_64
char				1				1			1
unsigned char		1				1			1
short				2				2			2
unsgned short		2				2			2
int					4				4			4
unsinged int		4				4			4
*long				4				4			8*
*unsigned long		4				4			8*
float				4				4			4
double				8				8			8
*long int			4				8			8*
long long			8				8			8
*long double		8				12			16*

http://www.ccl8.com/archives/21/
http://nadeausoftware.com/articles/2012/01/c_c_tip_how_use_compiler_predefined_macros_detect_operating_system
Developer:     Microsoft
Distributions:     Windows XP, Vista, 7, 8
Processors:     x86, x86-64

#if defined(_WIN64)
 Microsoft Windows (64-bit). ------------------------------
#elif defined(_WIN32)
Microsoft Windows (32-bit). ------------------------------
#endif

http://www.ccl8.com/archives/19/
http://nadeausoftware.com/articles/2012/10/c_c_tip_how_detect_compiler_name_and_version_using_compiler_predefined_macros
#if defined(__clang__)
 Clang/LLVM. ---------------------------------------------- 
#elif defined(__ICC) || defined(__INTEL_COMPILER)
 Intel ICC/ICPC. ------------------------------------------ 
#elif defined(__GNUC__) || defined(__GNUG__)
 GNU GCC/G++. --------------------------------------------- 
#elif defined(__HP_cc) || defined(__HP_aCC)
 Hewlett-Packard C/aC++. ---------------------------------- 
#elif defined(__IBMC__) || defined(__IBMCPP__)
 IBM XL C/C++. -------------------------------------------- 
#elif defined(_MSC_VER)
 Microsoft Visual Studio. --------------------------------- 
#elif defined(__PGI)
 Portland Group PGCC/PGCPP. -------------------------------
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
 Oracle Solaris Studio. -----------------------------------
#endif

*/


#pragma once

#ifndef _WIN32

// Linux needs this to support file operation on files larger then 4+GB
// But might need better if/def to select just the platforms that needs them.

#ifdef __LP64__

#ifndef __USE_FILE_OFFSET64
#define __USE_FILE_OFFSET64
#endif

#ifndef __USE_LARGEFILE64
#define __USE_LARGEFILE64
#endif

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#undef _FILE_OFFSET_BIT
#define _FILE_OFFSET_BIT 64

#endif
#endif

#include <assert.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#ifdef _DEBUG
#ifdef _TEST_
#define  _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#endif
#endif

#ifdef _WIN32
#include <fcntl.h>
#include <sys/timeb.h>
#include  <io.h>
#ifndef STDIN_FILENO
#define STDIN_FILENO (_fileno(stdin))
#define fileno _fileno
#endif

#include <windows.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strtoull _strtoui64
#else
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>
#include <assert.h>
#endif

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

#ifndef _WIN32
#include <readline/readline.h>
#include <readline/history.h>
#define plg_readline(b,p)	(((b)=readline(p)) != NULL)
#define plg_saveline(b) \
	if (strlen(b) > 0) \
	  add_history(b);
#define plg_freeline(b)	(free(b))
#else
#define plg_readline(b,p)	\
	(fputs(p, stdout), fflush(stdout),\
	fgets(b, 1024, stdin) != NULL)
#define plg_saveline(b)	{ (void)b; }
#define plg_freeline(b)	{ (void)b; }
#endif

#ifdef _WIN32
#if defined(_MSC_VER) && _MSC_VER < 1900

#define snprintf c99_snprintf
#define vsnprintf c99_vsnprintf

__inline int c99_vsnprintf(char *outBuf, unsigned int size, const char *format, va_list ap)
{
	int count = -1;

	if (size != 0)
		count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
	if (count == -1)
		count = _vscprintf(format, ap);

	return count;
}

__inline int c99_snprintf(char *outBuf, unsigned int size, const char *format, ...)
{
	int count;
	va_list ap;

	va_start(ap, format);
	count = c99_vsnprintf(outBuf, size, format, ap);
	va_end(ap);

	return count;
}

#endif

#endif//win32

#define KB (unsigned int)(1 << 10)
#define MB (unsigned int)(1 << 20)
#define GB (unsigned int)(1 << 30)

#define NOTUSED(V) ((void) V)

#ifdef _WIN32
#ifdef _PLG_ASSERT_
#define plg_assert assert
#else
#define plg_assert(_Expression) ((void)0)
#endif
#else
#ifdef _PLG_ASSERT_
#define plg_assert assert
#else
#define plg_assert(_Expression) ((void)0)
#endif
#endif