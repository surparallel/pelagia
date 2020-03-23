/* start.c - System startup related
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
#include "pjson.h"
#include "pelagia.h"
#include "psds.h"

static void EnumTableJson(pJSON * root, void* pManage, char* order)
{
	plg_MngAddTable(pManage, order, strlen(order), root->string, strlen(root->string));

	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_Object != item->type)	{

			if (strcmp(item->string, "weight") == 0) {
				plg_MngSetWeight(pManage, root->string, strlen(root->string), item->valueint);
			} else if (strcmp(item->string, "nosave") == 0) {
				plg_MngSetNoSave(pManage, root->string, strlen(root->string), item->valueint);
			} else if (strcmp(item->string, "noshare") == 0) {
				plg_MngSetNoShare(pManage, root->string, strlen(root->string), item->valueint);
			}
		}
	}
}

static void EnumOrderJson(pJSON * root, void* pManage)
{
	char* orderType = 0;
	char* file = 0;
	char* fun = 0;
	int weight = -1;
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_Object != item->type) {
			if (strcmp(item->string, "orderType") == 0) {
				orderType = item->valuestring;
			} else if (strcmp(item->string, "file") == 0) {
				file = item->valuestring;
			} else if (strcmp(item->string, "fun") == 0) {
				fun = item->valuestring;
			} else if (strcmp(item->string, "weight") == 0) {
				weight = item->valueint;
			}
		}
	}

	void* process = 0;
	if (strcmp(orderType, "lua") == 0) {

		if (!file || !fun) {
			elog(log_error, "EnumOrderJson.lua:%s file or fun empty!", root->string);
		}
		process = plg_JobCreateLua(file, strlen(file), fun, strlen(fun));
		plg_MngAddOrder(pManage, root->string, strlen(root->string), process);
	} else if (strcmp(orderType, "dll") == 0) {

		if (!file || !fun) {
			elog(log_error, "EnumOrderJson.dll:%s file or fun empty!", root->string);
		}
		process = plg_JobCreateDll(file, strlen(file), fun, strlen(fun));
		plg_MngAddOrder(pManage, root->string, strlen(root->string), process);
	}

	if (weight != -1) {
		plg_JobSetWeight(process, weight);
	}

	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_Object == item->type)
			EnumTableJson(item, pManage, root->string);
	}
}

static void EnumJson(pJSON * root, void* pManage)
{
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_Object == item->type)      
			EnumOrderJson(item, pManage);
		else
		{
			if (strcmp(item->string, "MaxTableWeight")==0) {
				plg_MngSetMaxTableWeight(pManage, item->valueint);
			} else 	if (strcmp(item->string, "LuaPath") == 0) {
				plg_MngSetLuaPath(pManage, item->valuestring);
			} else 	if (strcmp(item->string, "LuaDllPath") == 0) {
				plg_MngSetLuaDllPath(pManage, item->valuestring);
			} else 	if (strcmp(item->string, "DllPath") == 0) {
				plg_MngSetDllPath(pManage, item->valuestring);
			}
		}
	}
}

static void* plg_StartFromJson(const char* jsonStr) {

	pJSON * root = pJson_Parse(jsonStr);
	if (!root) {
		printf("json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}

	void* pManage = 0;
	pJSON * dbPath = pJson_GetObjectItem(root, "dbPath");
	if (dbPath) {
		pManage = plg_MngCreateHandle(dbPath->valuestring, strlen(dbPath->valuestring));
	} else {
		pManage = plg_MngCreateHandle(0, 0);
	}

	EnumJson(root, pManage);

	int iCore = 1;
	pJSON * core = pJson_GetObjectItem(root, "core");
	if (core) {
		iCore = core->valueint;
	}

	plg_MngAllocJob(pManage, iCore);
	plg_MngStarJob(pManage);
	pJson_Delete(root);

	return pManage;
}

void* plg_MngCreateHandleWithJson(const char* jsonFile) {

	FILE *cFile;
	cFile = fopen_t(jsonFile, "rb");
	if (!cFile) {
		sds filePath = plg_sdsCatFmt(plg_sdsEmpty(), "./%s", jsonFile);
		cFile = fopen_t(filePath, "rb");
		plg_sdsFree(filePath);
		if (!cFile) {
			elog(log_warn, "plg_MngCreateHandleWithJson.fopen_t.rb!");
			return 0;
		}
	}

	fseek_t(cFile, 0, SEEK_END);
	long long fileLength = ftell_t(cFile);
	void* dstBuf = malloc(fileLength);
	fseek_t(cFile, 0, SEEK_SET);
	long long retRead = fread(dstBuf, 1, fileLength, cFile);
	if (retRead != fileLength) {
		elog(log_warn, "plg_MngCreateHandleWithJson.fread.rb!");
		return 0;
	}

	fclose(cFile);
	return plg_StartFromJson(dstBuf);
}

unsigned int plg_NVersion() {
	return VERSION_NUMMINOR;
}

unsigned int plg_MVersion() {
	return VERSION_NUMMAJOR;
}