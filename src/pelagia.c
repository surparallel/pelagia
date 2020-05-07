/*peagia.c - test console for tcl
 *
 * Copyright(C) 2019 - 2020, sun shuo <sun.shuo@surparallel.org>
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http ://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <pthread.h>
#include "plateform.h"

#include "pelagia.h"
#include "pelog.h"
#include "psds.h"
#include "pfile.h"
#include "pdisk.h"
#include "pmanage.h"
#include "pstart.h"
#include "pcmd.h"
#include "pbaseall.h"
#include "psimple.h"
#include "prfesa.h"
#include "pbase64.h"

static void* _pManage = 0;

/* Print generic help. */
void plg_CliOutputGenericHelp(void) {
	printf(
		"\n"
		"      \"quit\" to exit\n"
		"      \"init [path]\" Init system. path: The path of the file.\n"
		"      \"iwj [json path]\" Init system wiht json. path: The path of the file.\n"
		"      \"destory\" Destory system.\n"
		"      \"star\" Star job.\n"
		"      \"stop\" Stop job.\n"
		"      \"order [order] [class] [fun]\" Add order.\n"
		"      \"max [weight]\" Set max table weight.\n"
		"      \"table [order] [table]\" Add table.\n"
		"      \"weight [table] [weight]\" Set table weight.\n"
		"      \"share [table] [share]\" Set table share.\n"
		"      \"save [table] [save]\" Set table no save.\n"
		"      \"aj [core]\" Alloc job.\n"
		"      \"fj\" Free job.\n"
		"      \"rc [order] [arg]\" Remote call.\n"
		"      \"rcj [order] [arg]\" Remote call with json.\n"
		"      \"pas\"Print all status.\n"
		"      \"pajs\" Print all job status.\n"
		"      \"pajd\" Print all job details.\n"
		"      \"ppa\" Print possible alloc.\n"
		"      \"pajo\" Print all order of job.\n"
		"      \"base\" base example.\n"
		"      \"simple\" simple example.\n"
		"      \"fe\" spseudo random finite element simulation analysis.\n"
		"      \"logfile\" Log set to file\n"
		"      \"loglevel [0~5]\" Level of log output\n"
		"      \"logprint\" Log set to print stdio\n"
		);
}

int plg_IssueCommand(int argc, char **argv, int noFind) {

	if (!argc) {
		return 1;
	}
	char *command = argv[0];
	
	if (!strcasecmp(command, "help") || !strcasecmp(command, "?")) {
		plg_CliOutputGenericHelp();
		return 1;
	}
	else if (!strcasecmp(command, "version")) {
		plg_Version();
		return 1;
	}
	else if (!strcasecmp(command, "quit")) {
		printf("bye!\n");
		return 0;
	}
	else if (!strcasecmp(command, "init")) {
		if (_pManage != 0) {
			plg_MngDestoryHandle(_pManage);
		}

		if (argc == 2) {
			_pManage = plg_MngCreateHandle(argv[1], strlen(argv[1]));
			if (!_pManage){
				printf("Manage initialization failed\n");
			}
		} else {
			printf("Parameter does not meet the requirement\n");
		}
		return 1;
	} else if (!strcasecmp(command, "iwj")) {
		if (_pManage != 0) {
			plg_MngDestoryHandle(_pManage);
		}

		if (argc == 2) {
			_pManage = plg_MngCreateHandleWithJson(argv[1]);
			if (!_pManage){
				printf("Manage initialization failed\n");
			}
		} else {
			printf("Parameter does not meet the requirement\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "destory")) {
		if (_pManage != 0) {
			plg_MngDestoryHandle(_pManage);
			_pManage = 0;
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "star")) {
		if (_pManage != 0) {
			plg_MngStarJob(_pManage);
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
	}
	else if (!strcasecmp(command, "stop")) {
		if (_pManage != 0) {
			plg_MngStopJob(_pManage);
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "order")) {
		if (_pManage != 0) {
			if (argc == 4) {
				plg_MngAddOrder(_pManage, argv[1], strlen(argv[1]), plg_JobCreateLua(argv[2], strlen(argv[2]), argv[3], strlen(argv[3])));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "max")) {
		if (_pManage != 0) {
			if (argc == 2) {
				plg_MngSetMaxTableWeight(_pManage, atoi(argv[1]));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "table")) {
		if (_pManage != 0) {
			if (argc == 3) {
				plg_MngAddTable(_pManage, argv[1], strlen(argv[1]), argv[2], strlen(argv[2]));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "weight")) {
		if (_pManage != 0) {
			if (argc == 3) {
				plg_MngSetWeight(_pManage, argv[1], strlen(argv[1]), atoi(argv[2]));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "share")) {
		if (_pManage != 0) {
			if (argc == 3) {
				plg_MngSetNoShare(_pManage, argv[1], strlen(argv[1]), atoi(argv[2]));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "save")) {
		if (_pManage != 0) {
			if (argc == 3) {
				plg_MngSetNoSave(_pManage, argv[1], strlen(argv[1]), atoi(argv[2]));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "aj")) {
		if (_pManage != 0) {
			if (argc == 2) {
				plg_MngAllocJob(_pManage, atoi(argv[1]));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "fj")) {
		if (_pManage != 0) {
			if (argc == 1) {
				plg_MngFreeJob(_pManage);
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "rc")) {
		if (_pManage != 0) {
			if (argc == 3) {
				plg_MngRemoteCall(_pManage, argv[1], strlen(argv[1]), argv[2], strlen(argv[2]));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "rcj")) {
		if (_pManage != 0) {
			if (argc >= 2) {
				unsigned long long p = 101021;
				plg_MngRemoteCallWithArg(_pManage, argv[1], strlen(argv[1]), (void*)p, argc - 2, (const char**) &argv[2]);
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "pas")) {
		if (_pManage != 0) {
			plg_MngPrintAllStatus(_pManage);
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "pajs")) {
		if (_pManage != 0) {
			plg_MngPrintAllJobStatus(_pManage);
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "pajd")) {
		if (_pManage != 0) {
			plg_MngPrintAllJobDetails(_pManage);
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "ppa")) {
		if (_pManage != 0) {
			plg_MngPrintPossibleAlloc(_pManage);
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "pajo")) {
		if (_pManage != 0) {
			plg_MngPrintAllJobOrder(_pManage);
		} else{
			printf("Manage is not initialized. Please call iwj for initialization\n");
		}
		return 1;
	}
	else if (!strcasecmp(command, "base")) {
		plg_BaseAll();
		return 1;
	} 
	else if (!strcasecmp(command, "simple")) {
		plg_simple();
		return 1;
	}
	else if (!strcasecmp(command, "pfs")) {
		PRFESA();
		return 1;
	}
	else if (!strcasecmp(command, "logfile")) {
		plg_LogSetErrFile();
		return 1;
	}
	else if (!strcasecmp(command, "logprint")) {
		plg_LogSetErrPrint();
		return 1;
	}
	else if (!strcasecmp(command, "loglevel")) {
		if (argc == 1) {
			plg_LogSetMaxLevel(atoi(argv[1]));
		} else {
			printf("Parameter does not meet the requirement\n");
		}
		return 1;
	}
	else {
		if(noFind)printf("command \"%s\" no found!\n", command);
		return -1;
	}

	return 1;
}

static sds ReadArgFromStdin(void) {
	char buf[1024];
	char* p = buf;
	sds arg = plg_sdsEmpty();
	size_t len;

	if (plg_readline(p, ">") == 0)
		return 0;

	len = strlen(p);
	if (p[len - 1] == '\n') {
		p[len - 1] = '\0';
		len -= 1;
	}

	arg = plg_sdsCatLen(arg, p, len);
	plg_freeline(p);
	return arg;
}

int plg_Interactive(FUNIssueCommand pIssueCommand) {
	while (1) {
		sds ptr = ReadArgFromStdin();
		if (ptr == 0) {
			continue;
		}
		int vlen;
		sds *v = plg_sdsSplitLen(ptr, (int)plg_sdsLen(ptr), " ", 1, &vlen);
		plg_sdsFree(ptr);
		int ret = pIssueCommand(vlen, v, 1);
		plg_saveline(v[0]);
		plg_sdsFreeSplitres(v, vlen);
		if (0 == ret) break;
	}
	return 1;
}


static int checkArg(char* argv) {
	if (strcmp(argv, "--") == 0 || strcmp(argv, "-") == 0)
		return 0;
	else
		return 1;
}

int plg_ReadArgFromParam(int argc, char **argv) {

	for (int i = 0; i < argc; i++) {
		/* Handle special options --help and --version */
		if (strcmp(argv[i], "-v") == 0 ||
			strcmp(argv[i], "--version") == 0)
		{
			plg_Version();
			return 0;
		}
		else if (strcmp(argv[i], "--help") == 0 ||
			strcmp(argv[i], "-h") == 0)
		{
			printf(
				"\n"
				"      \"-v --version\" to version\n"
				"      \"-h --help\" to help\n"
				"      \"-s --start [dbPath]\" to start from [dbPath]\n"
				"      \"-o --output [dbFile] [jsonFile]\"outPut to json\n"
				"      \"-i --input [dbFile] [jsonFile]\"input to json\n"
				"      \"-d --decode [strbase64]\"decode base64\n"
				"      \"-e --encode [strbase64]\"encode base64\n"
				);
			return 0;
		} else if (strcmp(argv[i], "--start") == 0 ||
			strcmp(argv[i], "-s") == 0)
		{
			if (checkArg(argv[i + 1])) {
				plg_MngCreateHandleWithJson(argv[i + 1]);
			} else {
				printf("Not enough parameters found!\n");
			}
			return 0;
		} else if (strcmp(argv[i], "--output") == 0 ||
			strcmp(argv[i], "-o") == 0)
		{
			if (checkArg(argv[i + 1]) && checkArg(argv[i + 2])) {
				plg_MngOutJson(argv[i + 1], argv[i + 2]);
			} else {
				printf("Not enough parameters found!\n");
			}
			return 0;
		} else if (strcmp(argv[i], "--input") == 0 ||
			strcmp(argv[i], "-i") == 0)
		{
			if (checkArg(argv[i + 1])) {
				plg_MngFromJson(argv[i + 1]);
			} else {
				printf("Not enough parameters found!\n");
			}
			return 0;
		} else if (strcmp(argv[i], "--encode") == 0 ||
			strcmp(argv[i], "-e") == 0)
		{
			if (checkArg(argv[i + 1])) {
				plg_B64Encode((unsigned char*)(argv[i + 1]), strlen(argv[i + 1]));
			} else {
				printf("Not enough parameters found!\n");
			}
			return 0;
		} else if (strcmp(argv[i], "--decode") == 0 ||
			strcmp(argv[i], "-d") == 0)
		{
			if (checkArg(argv[i + 1])) {
				plg_B64Decode(argv[i + 1], strlen(argv[i + 1]));
			} else {
				printf("Not enough parameters found!\n");
			}
			return 0;
		}
	}

	return 1;
}

int PMIAN(int argc, char **argv) {

	plg_Version();
	int ret = 1;
	if (plg_ReadArgFromParam(argc, argv)){
		ret = plg_Interactive(plg_IssueCommand);
		
		if (_pManage) {
			plg_MngDestoryHandle(_pManage);
		}

		return ret;
	} else {
		return ret;
	}
}
