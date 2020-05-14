/* elog.h
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
#ifndef __ELOG_H
#define __ELOG_H

enum OutPutLevel
{
	log_null = 0,
	log_error,
	log_warn,
	log_stat,
	log_fun,
	log_details,
	log_all,
};

typedef void (*ErrFun)(int level, const char* describe, const char* time, const char* fileName, int line);

void plg_LogSetError(int level, char* describe, const char* fileName, int line);
char* plg_LogFormatDescribe(char const *fmt, ...);
void plg_LogFreeForm(void* s);
short plg_LogGetMaxLevel();
short plg_LogGetMinLevel();

void plg_LogSetErrCallBack(ErrFun errFun);

void plg_LogInit();
void plg_LogDestroy();

#define __FILENAME__ (strrchr(__FILE__, '\\') ? (strrchr(__FILE__, '\\') + 1):__FILE__)

#define elog(level, describe, ...) do {\
	if (level >= plg_LogGetMinLevel() && level <= plg_LogGetMaxLevel()) {\
	char* sdsDescribe = plg_LogFormatDescribe(describe, ##__VA_ARGS__);\
	plg_LogSetError(level, sdsDescribe, __FILENAME__, __LINE__);\
	plg_LogFreeForm(sdsDescribe);}\
} while (0);

#endif


