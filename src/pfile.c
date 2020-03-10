/* file.c - File write queue
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
#include <pthread.h>

#include "psds.h"
#include "pelog.h"
#include "pfile.h"
#include "plocks.h"
#include "pjob.h"
#include "pmemorylist.h"
#include "pfilesys.h"
#include "pelagia.h"

#define FileName(filePath) (strrchr(filePath, '\\') ? (strrchr(filePath, '\\') + 1):filePath)

typedef struct _FileHandle
{
	void* memoryList;
	sds filePath;
	FILE* fileHandle;
	sds fileName;
	void* pJobHandle;
	sds objName;
	void* mutexHandle;
	unsigned int fullPageSize;
} *PFileHandle, FileHandle;

void* plg_FileJobHandle(void* pvFileHandle) {
	PFileHandle pFileHandle = pvFileHandle;
	return pFileHandle->pJobHandle;
}

static int OrderDestroy(char* value, short valueLen) {
	elog(log_fun, "file.OrderDestroy");
	plg_JobSendOrder(job_ManageEqueue(), "destroycount", value, valueLen);
	plg_JobSetExitThread(1);
	return 1;
}

void file_FreePageArrary(void* pvFileHandle, void** memArrary, unsigned int size) {
	elog(log_fun, "file_FreePageArrary");
	PFileHandle pFileHandle = pvFileHandle;
	for (unsigned int l = 0; l < size; l++) {
		plg_MemListPush(pFileHandle->memoryList, memArrary[l]);
	}
	free(memArrary);
}

unsigned int plg_FileInsideFlushPage(void* pvFileHandle, unsigned int* pageAddr, void** pageArrary, unsigned int pageArrarySize) {

	elog(log_fun, "plg_FileInsideFlushPage");
	PFileHandle pFileHandle = pvFileHandle;

	for (unsigned int l = 0; l < pageArrarySize; l++) {

		//check length
		fseek_t(pFileHandle->fileHandle, 0, SEEK_END);
		unsigned long long fileLength = ftell_t(pFileHandle->fileHandle);
		unsigned long long newFileLength = pageAddr[l] * pFileHandle->fullPageSize + pFileHandle->fullPageSize;
		if (fileLength < newFileLength) {
			plg_SysSetFileLength(pFileHandle->fileHandle, newFileLength);
		}

		//write to file ftruncate
		fseek_t(pFileHandle->fileHandle, pageAddr[l] * pFileHandle->fullPageSize, SEEK_SET);
		fwrite(pageArrary[l], 1, pFileHandle->fullPageSize, pFileHandle->fileHandle);
	}

	//close file
	fflush(pFileHandle->fileHandle);
	file_FreePageArrary(pFileHandle, pageArrary, pageArrarySize);
	free(pageAddr);
	return 1;
}

typedef struct OrderFlushPageValue
{
	PFileHandle pFileHandle;
	unsigned int* pageAddr;
	void** pageArrary;
	unsigned int pageArrarySize;
}*POrderFlushPageValue, OrderFlushPageValue;

static int OrderFlushPage(char* value, short valueLen) {
	NOTUSED(valueLen);
	POrderFlushPageValue pOrderFlushPageValue = (POrderFlushPageValue)value;
	plg_FileInsideFlushPage(pOrderFlushPageValue->pFileHandle, pOrderFlushPageValue->pageAddr, pOrderFlushPageValue->pageArrary, pOrderFlushPageValue->pageArrarySize);
	return 1;
}

void* plg_FileCreateHandle(char* fullPath, void* pManageEqueue, unsigned int fullPageSize) {
	PFileHandle pFileHandle = malloc(sizeof(FileHandle));
	pFileHandle->filePath = fullPath;

	FILE *outputFile;
	outputFile = fopen_t(pFileHandle->filePath, "rb+");
	if (!outputFile) {
		elog(log_warn, "plg_FileCreateHandle.fopen_t.rb+!");
		return 0;
	}
	pFileHandle->fileHandle = outputFile;

	pFileHandle->fileName = plg_sdsNew(FileName(fullPath));
	pFileHandle->mutexHandle = plg_MutexCreateHandle(LockLevel_3);
	pFileHandle->pJobHandle = plg_JobCreateHandle(pManageEqueue, TT_FILE, NULL, NULL, NULL);
	pFileHandle->objName = plg_sdsNew("file");
	pFileHandle->fullPageSize = fullPageSize;
	pFileHandle->memoryList = plg_MemListCreate(60, fullPageSize, 1);
	plg_JobSPrivate(pFileHandle->pJobHandle, pFileHandle);
	//order process
	plg_JobAddAdmOrderProcess(pFileHandle->pJobHandle, "destroy", plg_JobCreateFunPtr(OrderDestroy));
	plg_JobAddAdmOrderProcess(pFileHandle->pJobHandle, "flush", plg_JobCreateFunPtr(OrderFlushPage));
	return pFileHandle;
}

void plg_FileDestoryHandle(void* pvFileHandle) {
	PFileHandle pFileHandle = pvFileHandle;
	plg_MemListDestory(pFileHandle->memoryList);
	plg_JobDestoryHandle(pFileHandle->pJobHandle);
	plg_sdsFree(pFileHandle->filePath);
	fclose(pFileHandle->fileHandle);
	plg_sdsFree(pFileHandle->fileName);
	plg_sdsFree(pFileHandle->objName);
	plg_MutexDestroyHandle(pFileHandle->mutexHandle);
	free(pFileHandle);
}

/*
loading page from file;
*/
unsigned int file_InsideLoadPageFromFile(void* pvFileHandle, unsigned int pageSize, unsigned int pageAddr, void* page) {

	PFileHandle pFileHandle = pvFileHandle;
	elog(log_fun, "file_InsideLoadPageFromFile:%s pageAddr:%i", pFileHandle->filePath, pageAddr);
	//The file header cannot be loaded through this function
	if (pageAddr == 0) {
		elog(log_error, "pageAddr is zero!");
		return 0;
	}

	//check file length
	fseek_t(pFileHandle->fileHandle, 0, SEEK_END);
	long long inputFileLength = ftell_t(pFileHandle->fileHandle);

	//file size error
	if (inputFileLength < pageAddr * pageSize) {
		elog(log_error, "file_InsideLoadPageFromFile.inputFileLength!");
		return 0;
	}

	//file read
	fseek_t(pFileHandle->fileHandle, pageAddr * pageSize, SEEK_SET);
	unsigned long long retRead = fread(page, 1, pageSize, pFileHandle->fileHandle);
	if (retRead != pageSize) {
		elog(log_error, "file_InsideLoadPageFromFile.fread!");
		return 0;
	}
	fflush(pFileHandle->fileHandle);
	return 1;
}

unsigned int plg_FileLoadPage(void* pvFileHandle, unsigned int pageSize, unsigned int pageAddr, void* page) {

	PFileHandle pFileHandle = pvFileHandle;
	MutexLock(pFileHandle->mutexHandle, pFileHandle->objName);
	unsigned int r = file_InsideLoadPageFromFile(pFileHandle, pageSize, pageAddr, page);
	MutexUnlock(pFileHandle->mutexHandle, pFileHandle->objName);
	return r;
}

/*
Managing memory allocation by yourself and returning memory across threads are also involved.
The overall efficiency is poor
*/
void plg_FileMallocPageArrary(void* pvFileHandle, void*** memArrary, unsigned int size) {

	PFileHandle pFileHandle = pvFileHandle;
	*memArrary = malloc(size * sizeof(void*));
	for (unsigned int l = 0; l < size; l++) {
		(*memArrary)[l] = plg_MemListPop(pFileHandle->memoryList);
	}
}

unsigned int plg_FileFlushPage(void* pvFileHandle, unsigned int* pageAddr, void** pageArrary, unsigned int pageArrarySize) {

	PFileHandle pFileHandle = pvFileHandle;
	OrderFlushPageValue orderFlushPageValue;
	orderFlushPageValue.pFileHandle = pFileHandle;
	orderFlushPageValue.pageAddr = pageAddr;
	orderFlushPageValue.pageArrary = pageArrary;
	orderFlushPageValue.pageArrarySize = pageArrarySize;

	plg_JobSendOrder(plg_JobEqueueHandle(pFileHandle->pJobHandle), "flush", (char*)&orderFlushPageValue, sizeof(OrderFlushPageValue));
	return 1;
}