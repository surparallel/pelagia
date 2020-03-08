/* elog.c - Log related, the annoying output format to the callback function
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
#include "pelog.h"
#include "psds.h"
#include "plocks.h"
#include "ptimesys.h"

static void plg_LogErrFunPrintf(int level, const char* describe, const char* time, const char* fileName, int line);
static ErrFun _errFun = plg_LogErrFunPrintf;
static short _setMaxlevel = log_warn;
static short _setMinlevel = log_error;

static char* _outDir = "./";
static char* _outFile = " log";
static unsigned long long fileSec;

static void* mutexHandle = NULL;
static FILE *outputFile;

const char* GetLevelName(int level) {
	if (level == log_error) {
		return "ERROR";
	} else if (level == log_warn) {
		return "WARN";
	} else if (level == log_fun) {
		return "FUNCTION";
	} else if (level == log_details) {
		return "DETAILS";
	}
	return "NULL";
}

char* plg_LogGetTimForm() {

	time_t tt;
	time(&tt);
	tt = tt + 8 * 3600;//transform the time zone
	struct tm* t = gmtime(&tt);

	sds x = plg_sdsCatPrintf(plg_sdsEmpty(), "%04d-%02d-%02d %02d:%02d:%02d",
		t->tm_year + 1900, t->tm_mon + 1,
		t->tm_mday, t->tm_hour,
		t->tm_min, t->tm_sec);
	return x;
}

char* plg_LogFormatDescribe(char const *fmt, ...) {
	sds s = plg_sdsEmpty();
	unsigned int initlen = plg_sdsLen(s);
	const char *f = fmt;
	int i;
	va_list ap;

	va_start(ap, fmt);
	f = fmt;    /* Next format specifier byte to process. */
	i = initlen; /* Position of the next byte to write to dest str. */
	while (*f) {
		char next, *str;
		unsigned int l;
		long long num;
		unsigned long long unum;

		/* Make sure there is always space for at least 1 char. */
		if (plg_sdsAvail(s) == 0) {
			s = plg_sdsMakeRoomFor(s, 1);
		}

		switch (*f) {
		case '%':
			next = *(f + 1);
			f++;
			switch (next) {
			case 's':
			case 'S':
				str = va_arg(ap, char*);
				l = (next == 's') ? strlen(str) : plg_sdsLen(str);
				if (plg_sdsAvail(s) < l) {
					s = plg_sdsMakeRoomFor(s, l);
				}
				memcpy(s + i, str, l);
				plg_sdsIncLen(s, l);
				i += l;
				break;
			case 'i':
			case 'I':
				if (next == 'i')
					num = va_arg(ap, int);
				else
					num = va_arg(ap, long long);
				{
					char buf[SDS_LLSTR_SIZE];
					l = plg_sdsll2str(buf, num);
					if (plg_sdsAvail(s) < l) {
						s = plg_sdsMakeRoomFor(s, l);
					}
					memcpy(s + i, buf, l);
					plg_sdsIncLen(s, l);
					i += l;
				}
				break;
			case 'u':
			case 'U':
				if (next == 'u')
					unum = va_arg(ap, unsigned int);
				else
					unum = va_arg(ap, unsigned long long);
				{
					char buf[SDS_LLSTR_SIZE];
					l = plg_sdsull2str(buf, unum);
					if (plg_sdsAvail(s) < l) {
						s = plg_sdsMakeRoomFor(s, l);
					}
					memcpy(s + i, buf, l);
					plg_sdsIncLen(s, l);
					i += l;
				}
				break;
			default: /* Handle %% and generally %<unknown>. */
				s[i++] = next;
				plg_sdsIncLen(s, 1);
				break;
			}
			break;
		default:
			s[i++] = *f;
			plg_sdsIncLen(s, 1);
			break;
		}
		f++;
	}
	va_end(ap);

	/* Add null-term */
	s[i] = '\0';
	return s;
}

void plg_LogFreeForm(void* s) {
	plg_sdsFree(s);
}

void plg_LogSetMaxLevel(short level) {
	_setMaxlevel = level;
}

short plg_LogGetMaxLevel() {
	return _setMaxlevel;
}

void plg_LogSetMinLevel(short level) {
	_setMinlevel = level;
}

short plg_LogGetMinLevel() {
	return _setMinlevel;
}

void plg_LogSetOutDir(char* outDir) {
	_outDir = outDir;
}

void plg_LogSetOutFile(char* outFile) {
	_outFile = outFile;
}

static void plg_LogErrFunFile(int level, const char* describe, const char* time, const char* fileName, int line) {

	unsigned long long csec = plg_GetCurrentSec();
	if (csec - fileSec > 60 * 60 * 24) {
		plg_LogSetErrFile();
	}
	fprintf(outputFile, "%s %s (%s-%d) %s\n", time, GetLevelName(level), fileName, line, describe);
	fflush(outputFile);
}

static void plg_LogErrFunPrintf(int level, const char* describe, const char* time, const char* fileName, int line) {
	
	printf("%s %s (%s-%d) %s\n", time, GetLevelName(level), fileName, line, describe);
}

char* plg_LogGetTimFormDay() {

	time_t tt;
	time(&tt);
	tt = tt + 8 * 3600;//transform the time zone

	struct tm* t = gmtime(&tt);

	sds x = plg_sdsCatPrintf(plg_sdsEmpty(), "%04d-%02d-%02d",
		t->tm_year + 1900, t->tm_mon + 1,
		t->tm_mday);
	return x;
}

void plg_LogSetErrCallBack(ErrFun errFun) {
	if (errFun == NULL) {
		_errFun = plg_LogErrFunPrintf;
	} else {
		_errFun = errFun;
	}
	
}

void plg_LogInit() {
	mutexHandle = plg_MutexCreateHandle(LockLevel_4);
}

void plg_LogDestroy() {
	plg_MutexDestroyHandle(mutexHandle);
}

void plg_LogSetError(int level, char* describe, const char* fileName, int line) {

	plg_MutexLock(mutexHandle);
	if (_errFun != NULL) {
		char* time = plg_LogGetTimForm();
		_errFun(level, describe, time, fileName, line);
		plg_sdsFree(time);
	}
	plg_MutexUnlock(mutexHandle);
}

void plg_LogSetErrFile() {

	if (outputFile) {
		fclose(outputFile);
	}

	sds day = plg_LogGetTimFormDay();
	sds fielPath = plg_sdsCatFmt(plg_sdsEmpty(), "%s/%s%s", _outDir, _outFile, day);
	outputFile = fopen_t(fielPath, "a");

	if (!outputFile) {
		outputFile = NULL;
		plg_LogSetErrCallBack(NULL);
		return;
	}

	plg_sdsFree(fielPath);
	plg_sdsFree(day);
	fileSec = plg_GetCurrentSec();

	plg_LogSetErrCallBack(plg_LogErrFunFile);
}

void plg_LogSetErrPrint() {

	plg_LogSetErrCallBack(plg_LogErrFunPrintf);
}