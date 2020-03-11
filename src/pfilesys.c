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

short plg_SysFileExits(char* filePath) {
	FILE *outputFile;
	outputFile = fopen_t(filePath, "rb");
	if (!outputFile) {
		return 0;
	} else {
		fclose(outputFile);
		return 1;
	}
}