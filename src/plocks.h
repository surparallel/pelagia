/* locks.c - Lock record of critical point
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

#ifndef __LOCK_H
#define __LOCK_H

#include "psds.h"

#define Log_Switch 1

enum _LockLevel
{
	LockLevel_1 = 1,
	LockLevel_2,
	LockLevel_3,
	LockLevel_4
};
void plg_LocksCreate();
void plg_LocksDestroy();
char plg_LocksEntry(void* pSafeMutex);
char plg_LocksLeave(void* pSafeMutex);

void* plg_MutexCreateHandle(unsigned int rank);
void plg_MutexDestroyHandle(void* pSafeMutex);
int plg_MutexLock(void* pSafeMutex);
int plg_MutexUnlock(void* pSafeMutex);
void plg_MutexThreadDestroy();


#define MutexLock(lockObj, lockName) do {\
if(0==plg_LocksEntry(lockObj)){\
		sds x = plg_sdsCatPrintf(plg_sdsEmpty(),"entry mutex %p %s!", lockObj, lockName);\
		elog(log_error, x);\
		plg_sdsFree(x);}else {\
plg_MutexLock(lockObj);}\
} while (0)

#define MutexUnlock(lockObj, lockName) do {\
if(0==plg_LocksLeave(lockObj)){\
		sds x = plg_sdsCatPrintf(plg_sdsEmpty(),"leave mutex %p %s!", lockObj, lockName);\
		elog(log_error, x);\
		plg_sdsFree(x);}else{\
plg_MutexUnlock(lockObj);}\
} while (0)

void plg_LocksSetSpecific(void* ptr);
void* plg_LocksGetSpecific();
void plg_LocksSetLogFile(void* ptr);
void* plg_LocksGetLogFile();
#endif