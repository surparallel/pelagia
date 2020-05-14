/* timesys.c - Time system related
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
#include "ptimesys.h"
#include "psds.h"

#ifdef _WIN32
void usleep(unsigned long usec)
{
	HANDLE timer;
	LARGE_INTEGER interval;
	interval.QuadPart = -(10 * usec);

	timer = CreateWaitableTimer(NULL, TRUE, NULL);
	SetWaitableTimer(timer, &interval, 0, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}
#endif

unsigned long long plg_GetCurrentMilli()
{
#ifdef _WIN32
	struct timeb rawtime;
	ftime(&rawtime);
	return rawtime.time * 1000 + rawtime.millitm;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

unsigned long long plg_GetCurrentSec()
{

#ifdef _WIN32
	struct timeb rawtime;
	ftime(&rawtime);
	return rawtime.time;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL); //Under Linux, this function is in the sys / time. H header file
	return tv.tv_sec;
#endif
}

void plg_GetTime(long long *sec, int *usec)
{
#ifdef _WIN32
	struct _timeb tb;

	memset(&tb, 0, sizeof(struct _timeb));
	_ftime_s(&tb);
	(*sec) = tb.time;
	(*usec) = tb.millitm * 1000;
#else
	struct timeval tv;

	gettimeofday(&tv, NULL);
	*sec = tv.tv_sec;
	*usec = tv.tv_usec;
#endif
}

char* plg_GetTimForm() {

	time_t tt;
	time(&tt);
	tt = tt + 8 * 3600;//transform the time zone
	struct tm rt;
#ifdef _WIN32
	gmtime_s(&rt, &tt);
#else
	gmtime_r(&tt, &rt);
#endif
	sds x = plg_sdsCatPrintf(plg_sdsEmpty(), "%04d-%02d-%02d_%02d:%02d:%02d",
		rt.tm_year + 1900, rt.tm_mon + 1,
		rt.tm_mday, rt.tm_hour,
		rt.tm_min, rt.tm_sec);
	return x;
}

char* plg_GetDayForm() {

	time_t tt;
	time(&tt);
	tt = tt + 8 * 3600;//transform the time zone
	struct tm rt;
#ifdef _WIN32
	gmtime_s(&rt, &tt);
#else
	gmtime_r(&tt, &rt);
#endif

	sds x = plg_sdsCatPrintf(plg_sdsEmpty(), "%04d-%02d-%02d",
		rt.tm_year + 1900, rt.tm_mon + 1,
		rt.tm_mday);
	return x;
}