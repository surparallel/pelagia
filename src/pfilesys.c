/* filesys.c - File system related
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
#include "pfilesys.h"
#include "pelog.h"

static void mkdir_t(const char *_Path) {

#ifdef _WIN32
	mkdir(_Path);
#else
	mkdir(_Path, 0777);
#endif

}

short plg_SysSetFileLength(void* vfile, unsigned long long len)
{
	elog(log_fun, "plg_SysSetFileLength");
	FILE* file = vfile;
#ifdef _WIN32
	fseek_t(file, len, SEEK_SET);
	int fd = _fileno(file);
	HANDLE hfile = (HANDLE)_get_osfhandle(fd);
	return SetEndOfFile(hfile);
#else
	int fd = fileno(file);
	return ftruncate(fd, len) == 0;
#endif
}

void plg_MkDirs(char *muldir)
{
	int i, len;
	char str[512];
	strncpy(str, muldir, 512);
	len = strlen(str);
	for (i = 0; i < len; i++) {

		if (str[i] == '/') {
			str[i] = '\0';
			if (access_t(str, 0) != 0) {
				mkdir_t(str);
			}
			str[i] = '/';
		}
	}
	if (len > 0 && access_t(str, 0) != 0) {
		mkdir_t(str);
	}
	return;
}
