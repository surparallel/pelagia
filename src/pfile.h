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
#ifndef __FILE_H
#define __FILE_H

typedef unsigned int(*FlushCallBack)(void* pFileHandle, void* pPFileParamPageInfo, void** pageArrary, unsigned int pageArrarySize);

unsigned int plg_FileInsideFlushPage(void* pFileHandle, void* pPFileParamPageInfo, void** pageArrary, unsigned int pageArrarySize);
unsigned int plg_FileFlushPage(void* pFileHandle, void* pPFileParamPageInfo, void** pageArrary, unsigned int pageArrarySize);
unsigned int plg_FileLoadPage(void* pFileHandle, unsigned int pageSize, unsigned int pageAddr, void* page);
void* plg_FileCreateHandle(char* fullPath, void* pManageEqueue, unsigned int pageSize);
void plg_FileDestoryHandle(void* pFileHandle);
void* plg_FileJobHandle(void* pFileHandle);
void plg_FileMallocPageArrary(void* pFileHandle, void*** memArrary, unsigned int size);
void* plg_MaskMalloc(unsigned int pageId, char* src, char* des, int len);
void plg_MaskCmp(void* ptrVMask, char* src, char* des, int len);
void plg_MaskBit(void* ptrVMask, int num);
#endif