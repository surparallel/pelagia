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
#include "pfilesys.h"
#include "pelog.h"
#include "pjson.h"
#include "pelagia.h"
#include "psds.h"
#include "pconio.h"

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
			} else {
				elog(log_error, "Unable to process Tags %s.", item->string);
			}
		}
	}
}

static void EnumOrderJson(pJSON * root, void* pManage, char* pLuaPath, char* pLibPath)
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
			} else if (strcmp(item->string, "luaPath") == 0) {
				pLuaPath = item->valuestring;
			} else if (strcmp(item->string, "libPath") == 0) {
				pLibPath = item->valuestring;
			} else {
				elog(log_error, "Unable to process Tags %s.", item->string);
			}
		}
	}

	char path[512] = { 0 };
	void* process = 0;
	if (strcmp(orderType, "lua") == 0) {

		if (pLuaPath == 0 || (pLuaPath != 0 && strlen(pLuaPath) >= 512)) {
			if (pLuaPath != 0 && strlen(pLuaPath) >= 512) {
				elog(log_error, "luaPath Is greater than the string length limit of 512.");
			}
			if (0 == getcwd_t(path, 512))
				return;
		} else {
			strcpy(path, pLuaPath);
		}

		int fl = strlen(file);
		int pl = strlen(path);
		if (fl + pl + 1 >= 512)
			return;

		strcat(path, PATH_DIV);
		strcat(path, file);

		if (!file || !fun) {
			elog(log_error, "EnumOrderJson.lua:%s file or fun empty!", root->string);
		}

		process = plg_JobCreateLua(path, strlen(path), fun, strlen(fun));
		if (0 == plg_MngAddOrder(pManage, root->string, strlen(root->string), process))
			return;
	} else if (strcmp(orderType, "lib") == 0) {

		if (pLibPath == 0 || (pLibPath != 0 && strlen(pLibPath) >= 512)) {
			if (pLibPath != 0 && strlen(pLibPath) >= 512) {
				elog(log_error, "libPath Is greater than the string length limit of 512.");
			}
			if (0 == getcwd_t(path, 512))
				return;
		} else {
			strcpy(path, pLibPath);
		}

		int fl = strlen(file);
		int pl = strlen(path);
		if (fl + pl + 1 >= 512)
			return;

		strcat(path, PATH_DIV);
		strcat(path, file);

		if (!file || !fun) {
			elog(log_error, "EnumOrderJson.lib:%s file or fun empty!", root->string);
		}

		plg_MngAddLibFun(pManage, path, fun);
		process = plg_JobCreateLib(path, strlen(path), fun, strlen(fun));
		if (0 == plg_MngAddOrder(pManage, root->string, strlen(root->string), process))
			return;
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

static void* Intrnal_MngCreateHandleWithJson(const char* jsonFile, void* pManage, int isInclude);

static void EnumJson(pJSON * root, void* pManage, int isInclude)
{
	char* pLuaPath = 0;
	char* pLibPath = 0;
	for (int i = 0; i < pJson_GetArraySize(root); i++)
	{
		pJSON * item = pJson_GetArrayItem(root, i);
		if (pJson_Object == item->type)      
			EnumOrderJson(item, pManage, pLuaPath, pLibPath);
		else
		{
			int isHit = 1;
			if (!isInclude) {
				if (strcmp(item->string, "maxTableWeight") == 0) {
					plg_MngSetMaxTableWeight(pManage, item->valueint);
				} else 	if (strcmp(item->string, "luaPath") == 0) {
					pLuaPath = item->valuestring;
				} else 	if (strcmp(item->string, "luaLibPath") == 0) {
					plg_MngSetLuaLibPath(pManage, item->valuestring);
				} else 	if (strcmp(item->string, "libPath") == 0) {
					pLibPath = item->valuestring;
				} else 	if (strcmp(item->string, "luaHot") == 0) {
					plg_MngSetLuaHot(pManage, item->valueint);
				} else 	if (strcmp(item->string, "nosave") == 0) {
					plg_MngSetAllNoSave(pManage, item->valueint);
				} else 	if (strcmp(item->string, "stat") == 0) {
					plg_MngSetStat(pManage, item->valueint);
				} else 	if (strcmp(item->string, "checkTime") == 0) {
					plg_MngSetStatCheckTime(pManage, item->valueint);
				} else 	if (strcmp(item->string, "maxQueue") == 0) {
					plg_MngSetMaxQueue(pManage, item->valueint);
				} else 	if (strcmp(item->string, "logOutput") == 0) {

				} else 	if (strcmp(item->string, "logLevel") == 0) {

				} else 	if (strcmp(item->string, "dbPath") == 0) {

				} else 	if (strcmp(item->string, "core") == 0) {

				} else {
					isHit = 0;
				}
			}
			if (isHit == 0) {
				if (strcmp(item->string, "include") == 0) {
					if (pJson_Array != item->type)
						continue;

					for (int j = 0; j < pJson_GetArraySize(item); j++)
					{
						pJSON * subItem = pJson_GetArrayItem(item, j);
						if (pJson_Object != subItem->type)
							continue;

						pJSON * dirItem = pJson_GetObjectItem(subItem, "dir");
						if (dirItem) {

							char path[512] = { 0 };
							if (0 == getcwd_t(path, 512))
								continue;

							if (0 == chdir_t(dirItem->valuestring)) {

								pJSON * fileItem = pJson_GetObjectItem(subItem, "file");
								if (dirItem) {
									Intrnal_MngCreateHandleWithJson(fileItem->valuestring, pManage, 1);
								}
								if (0 == chdir_t(path)) {
									elog(log_error, "chdir_t faile %s", path);
								}
							}
						}
					}
				} else {
					elog(log_error, "Unable to process Tags %s.", item->string);
				}
			}//if (isHit == 0)
		}//if (pJson_Object == item->type) 
	}
}

static void* plg_StartFromJson(const char* jsonStr, void* pManage, int isInclude) {

	pJSON * root = pJson_Parse(jsonStr);
	if (!root) {
		printf("json Error before: [%s]\n", pJson_GetErrorPtr());
		return 0;
	}
		
	if (!isInclude) {

		pJSON * logoutput = pJson_GetObjectItem(root, "logOutput");
		if (logoutput) {
			if (strcmp(logoutput->valuestring, "file") == 0) {
				plg_LogSetErrFile();
			} else if (strcmp(logoutput->valuestring, "print") == 0) {
				plg_LogSetErrPrint();
			}
		}

		pJSON * loglevel = pJson_GetObjectItem(root, "logLevel");
		if (loglevel && (log_null <= loglevel->valueint && loglevel->valueint <= log_all)) {
			plg_LogSetMaxLevel(loglevel->valueint);
		}

		pJSON * dbPath = pJson_GetObjectItem(root, "dbPath");
		if (dbPath) {
			pManage = plg_MngCreateHandle(dbPath->valuestring, strlen(dbPath->valuestring));
		} else {
			pManage = plg_MngCreateHandle(0, 0);
		}
	}

	EnumJson(root, pManage, isInclude);

	if (!isInclude) {
		int iCore = 1;
		pJSON * core = pJson_GetObjectItem(root, "core");
		if (core) {
			iCore = core->valueint;
		}

		plg_MngAllocJob(pManage, iCore);
		plg_MngStarJob(pManage);
		pJson_Delete(root);
	}
	return pManage;
}

static void* Intrnal_MngCreateHandleWithJson(const char* jsonFile, void* pManage, int isInclude) {

	FILE *cFile;
	cFile = fopen_t(jsonFile, "rb");
	if (!cFile) {
		sds filePath = plg_sdsCatFmt(plg_sdsEmpty(), "./%s", jsonFile);
		cFile = fopen_t(filePath, "rb");
		plg_sdsFree(filePath);
		if (!cFile) {
			elog(log_error, "plg_MngCreateHandleWithJson.fopen_t.rb!");
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
	void* p = plg_StartFromJson(dstBuf, pManage, isInclude);
	free(dstBuf);
	return p;
}

void* plg_MngCreateHandleWithJson(const char* jsonFile) {
	return Intrnal_MngCreateHandleWithJson(jsonFile, 0, 0);
}

unsigned int plg_NVersion() {
	return VERSION_NUMMINOR;
}

unsigned int plg_MVersion() {
	return VERSION_NUMMAJOR;
}

void plg_Version() {
	
	plg_Color(11);
	printf("pelagia version \"" VERSION_MAJOR "." VERSION_MINOR "\"\n");
	printf("Copyright(C) 2019 - 2020, sun shuo <sun.shuo@surparallel.org>\n");
	printf("* All rights reserved. *\n");
	plg_ClearColor();
}