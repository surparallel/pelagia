/* timesys.h - Time system related
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
#ifndef __TIMESYS_H
#define __TIMESYS_H

#ifdef _WIN32
#include <windows.h>
#define sleep(sec)   Sleep(sec * 1000)
#define msleep(msec) Sleep(msec)
void usleep(unsigned long usec);

#else
#include <unistd.h>
#define msleep(msec) usleep(msec * 1000)
#endif

unsigned long long plg_GetCurrentMilli();
unsigned long long plg_GetCurrentSec();
void plg_GetTime(long long *sec, int *usec);
char* plg_GetTimForm();
char* plg_GetDayForm();

#endif