/*test_prfesa.c pseudo random finite element simulation analysis
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "prfesa.h"
#include "pelagia.h"
#include "ptimesys.h"
#include "pcrc16.h"

typedef struct _PfsParam
{
	void* pEvent;
	short i;
	short dmage;
	short o;
}*PPfsParam, PfsParam;

#define TEST_POINT 10
#define TEST_COUNT 100

#define TEST_CORE  4
#define TEST_BLOCK 100

static int InitRouting(char* value, short valueLen) {

	valueLen += 0;
	PPfsParam pParam = (PPfsParam)value;
	printf("---InitRouting--%d-\n", pParam->i);

	char table[256] = { 0 };
	sprintf(table, "t%d", pParam->i);

	int count = TEST_COUNT;
	plg_JobSet(table, strlen(table), "count", strlen("count"), &count, sizeof(count));

	return 1;
}

static int TestRouting(char* value, short valueLen) {

	//routing valueLen unused parameter 
	valueLen += 0;
	PPfsParam pParam = (PPfsParam)value;

	for (int i = 0; i < TEST_POINT; i++)
	{
		int count;
		char table[10] = { 0 };
		sprintf(table, "t%d", i);

		unsigned int len = 0;
		void* ptr = plg_JobGet(table, strlen(table), "count", strlen("count"), &len);

		if (ptr) {
			count = *(int*)ptr;
			free(ptr);

			if (count < 0) {
				plg_EventSend(pParam->pEvent, NULL, 0);
				printf("%d job all pass!\n", pParam->o);
				return 1;
			}
		}
	}

	//block
	void* p = malloc(1024 * 1024 * 64);
	short* s = (short*)p;
	for (long long i = 0; i < TEST_BLOCK; i++) {
		*s = plg_crc16(p, 1024 * 64);
	}
	free(p);
	//block

	int count = 0;
	char table[10] = { 0 };
	sprintf(table, "t%d", pParam->i);

	unsigned int len = 0;
	void* ptr = plg_JobGet(table, strlen(table), "count", strlen("count"), &len);

	if (ptr) {
		count = *(int*)ptr;
		free(ptr);

		count -= pParam->dmage;
		plg_JobSet(table, strlen(table), "count", strlen("count"), &count, sizeof(count));
		if (count < 0) {
			//all pass
			plg_EventSend(pParam->pEvent, NULL, 0);
			printf("%d job all pass!\n", pParam->o);
		} else {


			int l = rand();
			int c = 0;
			for (int i = 0; i < TEST_POINT; i++) {
				c = l % TEST_POINT;
				if (pParam->i == c) {
					continue;
				} else {
					break;
				}
			}

			char order[10] = { 0 };
			sprintf(order, "o%d", c);
			pParam->i = c;
			pParam->dmage = rand() % 1 ? 2 : 5;
			plg_JobRemoteCall(order, strlen(order), (char*)pParam, sizeof(PfsParam));
			plg_JobSetDonotFlush(1);
		}
	}
	//printf("---TestRouting--%d--%d--\n", pParam->i, count);
	return 1;
}

void PRFESA(void) {

	unsigned long long time = plg_GetCurrentMilli();

	void* pManage = plg_MngCreateHandle(0, 0);
	void* pEvent = plg_EventCreateHandle();

	plg_MngFreeJob(pManage);

	for (int i = 0; i < TEST_POINT; i++) {

		char order[10] = { 0 };
		sprintf(order, "i%d", i);
		plg_MngAddOrder(pManage, order, strlen(order), plg_JobCreateFunPtr(InitRouting));

		char table[10] = { 0 };
		sprintf(table, "t%d", i);
		plg_MngAddTable(pManage, order, strlen(order), table, strlen(table));
	}

	for (int i = 0; i < TEST_POINT; i++) {

		char order[10] = { 0 };
		sprintf(order, "o%d", i);
		plg_MngAddOrder(pManage, order, strlen(order), plg_JobCreateFunPtr(TestRouting));

		char table[10] = { 0 };
		sprintf(table, "t%d", i);
		plg_MngAddTable(pManage, order, strlen(order), table, strlen(table));
	}

	plg_MngPrintAllStatus(pManage);
	plg_MngAllocJob(pManage, TEST_CORE);
	plg_MngStarJob(pManage);
	printf("\n-----------------manage create-----------------\n");

	for (int i = 0; i < TEST_POINT; i++) {

		char order[10] = { 0 };
		sprintf(order, "i%d", i);
		PfsParam param;
		param.i = i;
		param.pEvent = pEvent;
		plg_MngRemoteCall(pManage, order, strlen(order), (char*)&param, sizeof(PfsParam));
	}

	for (int i = 0; i < TEST_POINT; i++) {

		char order[10] = { 0 };
		sprintf(order, "o%d", i);
		PfsParam param;
		param.i = i;
		param.o = i;
		param.pEvent = pEvent;
		param.dmage = 1;
		plg_MngRemoteCall(pManage, order, strlen(order), (char*)&param, sizeof(PfsParam));
	}

	printf("\n-----------------manage send o0-----------------\n");

	//Because it is not a thread created by ptw32, ptw32 new cannot release memory leak
	for (int i = 0; i < TEST_POINT; i++) {
		plg_EventWait(pEvent);

		unsigned int eventLen;
		void * ptr = plg_EventRecvAlloc(pEvent, &eventLen);
		plg_EventFreePtr(ptr);
	}

	plg_EventDestroyHandle(pEvent);
	plg_MngDestoryHandle(pManage);

	printf("\n-----------------manage destroy!-%f----------------\n", (double)(plg_GetCurrentMilli() - time) / 1000);
}