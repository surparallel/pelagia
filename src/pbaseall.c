/*base.c --base test of all api
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
#include "pbaseall.h"
#include "pelagia.h"
#include "ptimesys.h"
#include "pelog.h"

static int TestRouting(char* value, short valueLen) {
	void* pEvent; 
	memcpy(&pEvent, value, valueLen);

	//TableClear and get
	plg_JobTableClear("t0", 2);
	unsigned int  len = 0, error = 1;;
	void* ptr = plg_JobGet("t0", 2, "c1", 1, &len);
	if (!ptr) {
		error = 0;
	}

	if (error) {
		printf("fail TableClear and get!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//set and get
	plg_JobSet("t0", 2, "a", 1, "b", 1);
	len = 0, error = 1;
	ptr = plg_JobGet("t0", 2, "a", 1, &len);
	if (ptr) {
		if (memcmp(ptr, "b", 1)== 0) {
			error = 0;
		}
		free(ptr);
	}

	if (error) {
		printf("fail set and get!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//set and rand
	len = 0;
	error = 1;
	ptr = plg_JobRand("t0", 2, &len);
	if (ptr) {
		if (memcmp(ptr, "b", 1) == 0) {
			error = 0;
		}
		free(ptr);
	} 

	if(error) {
		printf("fail set and rand!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//set and length
	unsigned int length = plg_JobLength("t0", 2);
	if (length != 1) {
		printf("fail set and length!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}
	
	//set and IsKeyExist
	if (!plg_JobIsKeyExist("t0", 2, "a", 1)) {
		printf("fail set and IsKeyExist!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//del and get
	error = 1;
	plg_JobDel("t0", 2, "a", 1);
	len = 0;
	ptr = plg_JobGet("t0", 2, "a", 1, &len);
	if (ptr) {
		free(ptr);
	} else {
		error = 0;	
	}
	
	if (error) {
		printf("fail del and get!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//setIfNoExit and get
	error = 1;
	plg_JobSetIfNoExit("t0", 2, "a", 1, "b", 1);
	len = 0;
	ptr = plg_JobGet("t0", 2, "a", 1, &len);
	if (ptr) {
		if (memcmp(ptr, "b", 1) == 0) {
			error = 0;
		}
		free(ptr);
	} 
	
	if (error) {
		printf("fail SetIfNoExit and get!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	// rename and get
	error = 1;
	plg_JobRename("t0", 2, "a", 1, "c", 1);
	len = 0;
	ptr = plg_JobGet("t0", 2, "c", 1, &len);
	if (ptr) {
		if (memcmp(ptr, "b", 1) == 0) {
			error = 0;
		}
		free(ptr);
	}
	
	if (error) {
		printf("fail Rename and get!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//multiset and get
	error = 1;
	void* pDictExten = plg_DictExtenCreate();
	plg_DictExtenAdd(pDictExten, "c1", 2, "c", 1);
	plg_DictExtenAdd(pDictExten, "c2", 2, "b", 1);
	plg_DictExtenAdd(pDictExten, "c3", 2, "c", 1);
	plg_DictExtenAdd(pDictExten, "c4", 2, "b", 1);
	plg_DictExtenAdd(pDictExten, "c5", 2, "b", 1);
	plg_DictExtenAdd(pDictExten, "c6", 2, "c", 1);
	plg_JobMultiSet("t0", 2, pDictExten);
	len = 0;
	ptr = plg_JobGet("t0", 2, "c2", 2, &len);
	if (ptr) {
		if (memcmp(ptr, "b", 1) == 0) {
			error = 0;
		}
		free(ptr);
	}
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail MultiSet and get!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//multiset and limite
	error = 1;
	pDictExten = plg_DictExtenCreate();
	plg_JobLimite("t0", 2, "c3", 2, 2, 2, pDictExten);

	if (plg_DictExtenSize(pDictExten)) {
		unsigned int valueLen;
		void* entry = plg_DictExtenGetHead(pDictExten);
		void* valuePtr = plg_DictExtenValue(entry, &valueLen);
		if (valueLen) {
			if (memcmp(valuePtr, "c", 1) == 0) {
				error = 0;
			}
		}
	}
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail MultiSet and Limite!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//mulitiset and order
	error = 1;
	pDictExten = plg_DictExtenCreate();
	plg_JobOrder("t0", 2, 0, 1, pDictExten);

	if (plg_DictExtenSize(pDictExten)) {
		unsigned int valueLen;
		void* entry = plg_DictExtenGetHead(pDictExten);
		void* valuePtr = plg_DictExtenValue(entry, &valueLen);
		if (valueLen) {
			if (memcmp(valuePtr, "b", 1) == 0) {
				error = 0;
			}
		}
	}
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail MultiSet and order at first!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	error = 1;
	pDictExten = plg_DictExtenCreate();
	plg_JobOrder("t0", 2, 1, 1, pDictExten);

	if (plg_DictExtenSize(pDictExten)) {
		unsigned int valueLen;
		void* entry = plg_DictExtenGetHead(pDictExten);;
		void* valuePtr = plg_DictExtenValue(entry, &valueLen);
		if (valueLen) {
			if (memcmp(valuePtr, "c", 1) == 0) {
				error = 0;
			}
		}
	}
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail MultiSet and order at tail!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//mulitiset and rang
	error = 1;
	pDictExten = plg_DictExtenCreate();
	plg_JobRang("t0", 2, "c1", 2, "c1", 2, pDictExten);

	if (plg_DictExtenSize(pDictExten)) {
		unsigned int valueLen;
		void* entry = plg_DictExtenGetHead(pDictExten);
		void* valuePtr = plg_DictExtenValue(entry, &valueLen);
		if (valueLen) {
			if (memcmp(valuePtr, "c", 1) == 0) {
				error = 0;
			}
		}
	}
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail MultiSet and rang!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//mulitiset and MultiGet
	error = 1;
	void* pDictKeyExten = plg_DictExtenCreate();
	plg_DictExtenAdd(pDictKeyExten, "c1", 2, NULL, 0);
	pDictExten = plg_DictExtenCreate();
	plg_JobMultiGet("t0", 2, pDictKeyExten, pDictExten);

	if (plg_DictExtenSize(pDictExten)) {
		unsigned int valueLen;
		void* entry = plg_DictExtenGetHead(pDictExten);
		void* valuePtr = plg_DictExtenValue(entry, &valueLen);
		if (valueLen) {
			if (memcmp(valuePtr, "c", 1) == 0) {
				error = 0;
			}
		}
	}
	plg_DictExtenDestroy(pDictKeyExten);
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail MultiSet and MultiGet!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//mulitiset and Pattern
	error = 1;
	pDictExten = plg_DictExtenCreate();
	plg_JobPattern("t0", 2, "c1", 2, "c1", 2, "?1", 2, pDictExten);

	if (plg_DictExtenSize(pDictExten)) {
		unsigned int valueLen;
		void* entry = plg_DictExtenGetHead(pDictExten);
		void* valuePtr = plg_DictExtenValue(entry, &valueLen);
		if (valueLen) {
			if (memcmp(valuePtr, "c", 1) == 0) {
				error = 0;
			}
		}
	}
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail MultiSet and Pattern!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//mulitiset and point
	error = 1;
	pDictExten = plg_DictExtenCreate();
	plg_JobPoint("t0", 2, "c3", 2, 1, 1, pDictExten);

	if (plg_DictExtenSize(pDictExten)) {
		unsigned int valueLen;
		void* entry = plg_DictExtenGetHead(pDictExten);
		void* valuePtr = plg_DictExtenValue(entry, &valueLen);
		if (valueLen) {
			if (memcmp(valuePtr, "b", 1) == 0) {
				error = 0;
			}
		}
	}
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail MultiSet and Point!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//set////////////////////////////////////////////////////////////////////////

	//multiset and table clear
	plg_JobTableClear("t1", 2);
	if (plg_JobSIsKeyExist("t1", 2, "a", 1, "b", 1)) {
		printf("fail plg_JobTableClear!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetIsKeyExist
	plg_JobSAdd("t1", 2, "a", 1, "b", 1);
	if (!plg_JobSIsKeyExist("t1", 2, "a", 1, "b", 1)) {
		printf("fail setadd and SetIsKeyExist!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetRang
	error = 1;
	pDictExten = plg_DictExtenCreate();
	plg_JobSRang("t1", 2, "a", 1, "b", 1, "b", 1, pDictExten);

	if (plg_DictExtenSize(pDictExten)) {
		unsigned int keyLen;
		void* entry = plg_DictExtenGetHead(pDictExten);
		void* keyPtr = plg_DictExtenKey(entry, &keyLen);
		if (keyLen) {
			if (memcmp(keyPtr, "b", 1) == 0) {
				error = 0;
			}
		}
	}
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail setadd and rang!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetRangCount
	if (1 != plg_JobSRangCount("t1", 2, "a", 1, "b", 1, "b", 1)) {
		printf("fail SetAdd and SetRangCount!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetLimit
	error = 1;
	pDictExten = plg_DictExtenCreate();
	plg_JobSLimite("t1", 2, "a", 1, "b", 1, 1, 1, pDictExten);

	if (plg_DictExtenSize(pDictExten)) {
		unsigned int keyLen;
		void* entry = plg_DictExtenGetHead(pDictExten);
		void* keyPtr = plg_DictExtenKey(entry, &keyLen);
		if (valueLen) {
			if (memcmp(keyPtr, "b", 1) == 0) {
				error = 0;
			}
		}
	}
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail SetAdd and SetLimite!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetLength
	if (1 != plg_JobSLength("t1", 2, "a", 1)) {
		printf("fail setadd and setlength!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetMembers
	error = 1;
	pDictExten = plg_DictExtenCreate();
	plg_JobSMembers("t1", 2, "a", 1, pDictExten);

	if (plg_DictExtenSize(pDictExten)) {
		unsigned int keyLen;
		void* entry = plg_DictExtenGetHead(pDictExten);
		void* keyPtr = plg_DictExtenKey(entry, &keyLen);
		if (valueLen) {
			if (memcmp(keyPtr, "b", 1) == 0) {
				error = 0;
			}
		}
	}
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail SetAdd and SetMembers!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetRand
	error = 1;
	len = 0;
	ptr = plg_JobSRand("t1", 2, "a", 1, &len);
	if (ptr) {
		if (memcmp(ptr, "b", 1) == 0) {
			error = 0;
		}
		free(ptr);
	}
	if (error) {
		printf("fail setadd and setrand!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetMove
	plg_JobSMove("t1", 2, "a", 1, "c", 1, "b", 1);

	if (plg_JobSIsKeyExist("t1", 2, "a", 1, "b", 1)) {
		printf("fail SetAdd and SetMove!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	if (!plg_JobSIsKeyExist("t1", 2, "c", 1, "b", 1)) {
		printf("fail SetAdd and SetMove!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetPop
	error = 1;
	len = 0;
	ptr = plg_JobSPop("t1", 2, "c", 1, &len);
	if (ptr) {
		if (memcmp(ptr, "b", 1) == 0) {
			error = 0;
		}
		free(ptr);
	}
	if (error) {
		printf("fail SetAdd and SetPop!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetDel
	error = 1;
	plg_JobSAdd("t1", 2, "a", 1, "b", 1);
	pDictKeyExten = plg_DictExtenCreate();
	plg_DictExtenAdd(pDictKeyExten, "b", 1, NULL, 0);
	plg_JobSDel("t1", 2, "a", 1, pDictKeyExten);

	if (!plg_JobSIsKeyExist("t1", 2, "a", 1, "b", 1)) {
		error = 0;
	}
	plg_DictExtenDestroy(pDictKeyExten);

	if (error) {
		printf("fail SetaDel and SetIsKeyExist!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetUion
	error = 1;
	plg_JobSAdd("t1", 2, "a", 1, "b", 1);
	plg_JobSAdd("t1", 2, "c", 1, "d", 1);

	pDictKeyExten = plg_DictExtenCreate();
	pDictExten = plg_DictExtenCreate();
	plg_DictExtenAdd(pDictKeyExten, "a", 1, NULL, 0);
	plg_DictExtenAdd(pDictKeyExten, "c", 1, NULL, 0);
	plg_JobSUion("t1", 2, pDictKeyExten, pDictExten);

	if (plg_DictExtenSize(pDictExten)) {
		unsigned int keyLen;
		void* entry = plg_DictExtenGetHead(pDictExten);
		void* keyPtr = plg_DictExtenKey(entry, &keyLen);
		if (valueLen) {
			if (memcmp(keyPtr, "b", 1) ==0) {
				error = 0;
			}
		}
	}
	plg_DictExtenDestroy(pDictKeyExten);
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail SetAdd and Uion!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetInter
	error = 1;
	plg_JobSAdd("t1", 2, "a", 1, "e", 1);
	plg_JobSAdd("t1", 2, "c", 1, "e", 1);

	pDictKeyExten = plg_DictExtenCreate();
	pDictExten = plg_DictExtenCreate();
	plg_DictExtenAdd(pDictKeyExten, "a", 1, NULL, 0);
	plg_DictExtenAdd(pDictKeyExten, "c", 1, NULL, 0);
	plg_JobSInter("t1", 2, pDictKeyExten, pDictExten);

	if (plg_DictExtenSize(pDictExten)) {
		unsigned int keyLen;
		void* entry = plg_DictExtenGetHead(pDictExten);
		void* keyPtr = plg_DictExtenKey(entry, &keyLen);
		if (keyLen) {
			if (memcmp(keyPtr, "e", 1) == 0) {
				error = 0;
			}
		}
	}
	plg_DictExtenDestroy(pDictKeyExten);
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail SetAdd and Inter!\n");
		plg_DictExtenDestroy(pDictExten);
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetDiff
	error = 1;
	pDictKeyExten = plg_DictExtenCreate();
	pDictExten = plg_DictExtenCreate();
	plg_DictExtenAdd(pDictKeyExten, "a", 1, NULL, 0);
	plg_DictExtenAdd(pDictKeyExten, "c", 1, NULL, 0);
	plg_JobSDiff("t1", 2, pDictKeyExten, pDictExten);

	if (plg_DictExtenSize(pDictExten)) {
		unsigned int keyLen;
		void* entry = plg_DictExtenGetHead(pDictExten);
		void* keyPtr = plg_DictExtenKey(entry, &keyLen);
		if (keyLen) {
			if (memcmp(keyPtr, "d", 1) == 0) {
				error = 0;
			}
		}
	}
	plg_DictExtenDestroy(pDictKeyExten);
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail SetAdd and Diff!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetUionStore
	error = 1;
	pDictKeyExten = plg_DictExtenCreate();
	plg_DictExtenAdd(pDictKeyExten, "a", 1, NULL, 0);
	plg_DictExtenAdd(pDictKeyExten, "c", 1, NULL, 0);
	plg_JobSUionStore("t1", 2, pDictKeyExten, "u", 1);
	plg_DictExtenDestroy(pDictKeyExten);

	if (plg_JobSIsKeyExist("t1", 2, "u", 1, "d", 1)) {
		error = 0;
	}
	
	if (error) {
		printf("fail SetAdd and SetUionStore!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetInterStore
	error = 1;
	pDictKeyExten = plg_DictExtenCreate();
	plg_DictExtenAdd(pDictKeyExten, "a", 1, NULL, 0);
	plg_DictExtenAdd(pDictKeyExten, "c", 1, NULL, 0);
	plg_JobSInterStore("t1", 2, pDictKeyExten, "i", 1);

	if (plg_JobSIsKeyExist("t1", 2, "i", 1, "e", 1)) {
		error = 0;
	}
	plg_DictExtenDestroy(pDictKeyExten);

	if (error) {
		printf("fail SetAdd and SetInterStore!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetDiffStore
	pDictKeyExten = plg_DictExtenCreate();
	plg_DictExtenAdd(pDictKeyExten, "a", 1, NULL, 0);
	plg_DictExtenAdd(pDictKeyExten, "c", 1, NULL, 0);
	plg_JobSDiffStore("t1", 2, pDictKeyExten, "f", 1);

	if (plg_JobSIsKeyExist("t1", 2, "f", 1, "d", 1)) {
		error = 0;
	}
	plg_DictExtenDestroy(pDictKeyExten);
	if (error) {
		printf("fail SetAdd and SetDiffStore!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//SetAdd and SetPoint
	error = 1;
	pDictExten = plg_DictExtenCreate();
	plg_JobSPoint("t1", 2, "a", 1, "b", 1, 1, 1, pDictExten);

	if (plg_DictExtenSize(pDictExten)) {
		unsigned int keyLen;
		void* entry = plg_DictExtenGetHead(pDictExten);
		void* keyPtr = plg_DictExtenKey(entry, &keyLen);
		if (keyLen) {
			if (memcmp(keyPtr, "e", 1) == 0) {
				error = 0;
			}
		}
	}
	plg_DictExtenDestroy(pDictExten);

	if (error) {
		printf("fail setadd and rang!\n");
		plg_EventSend(pEvent, NULL, 0);
		return 1;
	}

	//all pass
	plg_EventSend(pEvent, NULL, 0);
	printf("job all pass!\n");
	return 1;
}

void plg_BaseAll() {

	void* pManage = plg_MngCreateHandle(0, 0);
	void* pEvent = plg_EventCreateHandle();

	plg_MngFreeJob(pManage);

	for (int i = 0; i < 1; i++) {
		char order[256] = {0};
		sprintf(order, "o%d", i);
		plg_MngAddOrder(pManage, order, strlen(order), plg_JobCreateFunPtr(TestRouting));

		for (int j = 0; j < 10; j++) {
			char table[256] = { 0 };
			sprintf(table, "t%d", j);
			plg_MngAddTable(pManage, order, strlen(order), table, strlen(table));
		}
	}

	plg_MngPrintAllStatus(pManage);
	plg_MngAllocJob(pManage, 1);
	plg_MngPrintAllJobStatus(pManage);
	plg_MngStarJob(pManage);
	printf("\n-----------------manage create-----------------\n");

	plg_MngRemoteCall(pManage, "o0", 2, (char*)&pEvent, sizeof(void*));
	printf("\n-----------------manage send o0-----------------\n");

	//Because it is not a thread created by ptw32, ptw32 new cannot release memory leak
	plg_EventWait(pEvent);

	unsigned int eventLen;
	void * ptr = plg_EventRecvAlloc(pEvent, &eventLen);
	plg_EventFreePtr(ptr);

	plg_EventDestroyHandle(pEvent);
	plg_MngDestoryHandle(pManage);
	printf("\n-----------------manage destroy!-----------------\n");
}

