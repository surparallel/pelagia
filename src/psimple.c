/*
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
#include "pelagia.h"
#include "psimple.h"
#include "ptimesys.h"

static int TaskRouting(char* value, short valueLen) {
	void* pEvent;
	memcpy(&pEvent, value, valueLen);

	char table[10] = { 0 };
	strcpy(table, "table");

	char key[10] = { 0 };
	strcpy(key, "key");

	char v[100] = { 0 };
	strcpy(v, "hello world!");

	//Set data to table
	plg_JobSet(table, strlen(table), key, strlen(key), v, strlen(v) + 1);

	unsigned int len = 0;
	//get data
	void* ptr = plg_JobGet(table, strlen(table), key, strlen(key), &len);
	if (ptr) {
		printf("%s\n", (char*)ptr);
		free(ptr);
	}

	plg_EventSend(pEvent, NULL, 0);
	printf("-----------------job all pass!-----------------\n");
	return 1;
}

void plg_simple() {

	printf("\n-----------------manage create!-----------------\n");
	//Create system handle
	void* pManage = plg_MngCreateHandle(0, 0);
	//Create a message handle to receive messages
	void* pEvent = plg_EventCreateHandle();
	
	//Add a task to the system, including the name of the task and the callback function.
	char order[10] = { 0 };
	strcpy(order, "order");
	plg_MngAddOrder(pManage, order, strlen(order), plg_JobCreateFunPtr(TaskRouting));

	//Add the table to be written by the task. The read table can be read without adding
	//Each table is an independent key-value database
	char table[10] = { 0 };
	strcpy(table, "table");
	plg_MngAddTable(pManage, order, strlen(order), table, strlen(table));

	//Create resources used by threads and assign tasks to threads
	plg_MngAllocJob(pManage, 1);
	//Create thread environment
	plg_MngStarJob(pManage);

	//Trigger the task and use the event handle as a parameter
	plg_MngRemoteCall(pManage, order, strlen(order), (char*)&pEvent, sizeof(void*));

	//Wait for task return event
	plg_EventWait(pEvent);

	//Extract events from queue
	unsigned int eventLen;
	void * ptr = plg_EventRecvAlloc(pEvent, &eventLen);
	plg_EventFreePtr(ptr);

	//Finish
	plg_EventDestroyHandle(pEvent);
	plg_MngDestoryHandle(pManage, 0, 0);
	printf("\n-----------------manage destroy!-----------------\n");

}

/*
static int TaskRouting2(char* value, short valueLen) {
	void* pEvent;
	memcpy(&pEvent, value, valueLen);

	char table[10] = { 0 };
	strcpy(table, "table");

	char key[10] = { 0 };
	strcpy(key, "key");

	unsigned int len = 0;
	//get data
	void* ptr = plg_JobGet(table, strlen(table), key, strlen(key), &len);
	if (ptr) {
		printf("%s\n", (char*)ptr);
		free(ptr);
	}

	plg_EventSend(pEvent, NULL, 0);
	printf("-----------------job all pass!-----------------\n");
	return 1;
}

static int TaskRouting1(char* value, short valueLen) {

	char table[10] = { 0 };
	strcpy(table, "table");

	char key[10] = { 0 };
	strcpy(key, "key");

	char v[100] = { 0 };
	strcpy(v, "hello world!");

	//Set data to table
	plg_JobSet(table, strlen(table), key, strlen(key), v, strlen(v) + 1);

	unsigned int len = 0;
	//get data
	void* ptr = plg_JobGet(table, strlen(table), key, strlen(key), &len);
	if (ptr) {
		printf("%s\n", (char*)ptr);
		free(ptr);
	}

	char order[10] = { 0 };
	strcpy(order, "order2");
	plg_JobRemoteCall(order, strlen(order), value, valueLen);
	return 1;
}

void plg_simple2() {

	printf("\n-----------------manage create!-----------------\n");
	//Create system handle
	void* pManage = plg_MngCreateHandle(0, 0);
	//Create a message handle to receive messages
	void* pEvent = plg_EventCreateHandle();

	//Add a task to the system, including the name of the task and the callback function.
	char order[10] = { 0 };
	strcpy(order, "order1");
	plg_MngAddOrder(pManage, order, strlen(order), plg_JobCreateFunPtr(TaskRouting1));

	strcpy(order, "order2");
	plg_MngAddOrder(pManage, order, strlen(order), plg_JobCreateFunPtr(TaskRouting2));

	//Add the table to be written by the task. The read table can be read without adding
	//Each table is an independent key-value database
	char table[10] = { 0 };
	strcpy(table, "table");
	plg_MngAddTable(pManage, order, strlen(order), table, strlen(table));

	//Create resources used by threads and assign tasks to threads
	plg_MngAllocJob(pManage, 2);

	plg_MngPrintAllJobOrder(pManage);

	//Create thread environment
	plg_MngStarJob(pManage);

	//Trigger the task and use the event handle as a parameter
	strcpy(order, "order1");
	plg_MngRemoteCall(pManage, order, strlen(order), (char*)&pEvent, sizeof(void*));

	//Wait for task return event
	plg_EventWait(pEvent);

	//Extract events from queue
	unsigned int eventLen;
	void * ptr = plg_EventRecvAlloc(pEvent, &eventLen);
	plg_EventFreePtr(ptr);

	//Finish
	plg_EventDestroyHandle(pEvent);
	plg_MngDestoryHandle(pManage, 0, 0);
	printf("\n-----------------manage destroy!-----------------\n");

}
*/