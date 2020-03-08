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

static void* pManage = 0;

#define VERSION_MAJOR	"0"
#define VERSION_MINOR	"6"

#define VERSION_NUMMAJOR	0
#define VERSION_NUMMINOR	6

unsigned int plg_NVersion() {
	return VERSION_NUMMINOR;
}

unsigned int plg_MVersion() {
	return VERSION_NUMMAJOR;
}

void plg_Version() {
	printf("pelgia version \"" VERSION_MAJOR "." VERSION_MINOR "\"\n");
	printf("Copyright(C) 2019 - 2020, sun shuo <sun.shuo@surparallel.org>\n");
	printf("* All rights reserved. *\n");
}

/* Print generic help. */
void plg_CliOutputGenericHelp(void) {
	printf(
		"\n"
		"      \"quit\" to exit\n"
		"      \"init [path]\" Init system. path: The path of the file.\n"
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
		"      \"pas\"Print all status.\n"
		"      \"pajs\" Print all job status.\n"
		"      \"pajd\" Print all job details.\n"
		"      \"ppa\" Print possible alloc.\n"
		"      \"base\" base example.\n"
		"      \"simple\" simple example.\n"
		"      \"fe\" spseudo random finite element simulation analysis.\n"
		);
}

static int IssueCommand(int argc, char **argv) {
	char *command = argv[0];
	
	if (!strcasecmp(command, "help") || !strcasecmp(command, "?")) {
		plg_CliOutputGenericHelp();
	}
	else if (!strcasecmp(command, "version")) {
		plg_Version();
	}
	else if (!strcasecmp(command, "quit")) {
		printf("bye!\n");
		return 0;
	}
	else if (!strcasecmp(command, "init")) {
		if (pManage == 0) {
			pManage = plg_MngCreateHandle(argv[1], strlen(argv[1]));
		} else {
			printf("It has been initialized. Please destroy it first\n");
		}
	}
	else if (!strcasecmp(command, "destory")) {
		if (pManage != 0) {
			plg_MngDestoryHandle(pManage, 0, 0);
		}
	}
	else if (!strcasecmp(command, "star")) {
		if (pManage != 0) {
			plg_MngStarJob(pManage);
		}
	}
	else if (!strcasecmp(command, "stop")) {
		if (pManage != 0) {
			plg_MngStopJob(pManage);
		}
	}
	else if (!strcasecmp(command, "order")) {
		if (pManage != 0) {
			if (argc == 4) {
				plg_MngAddOrder(pManage, argv[1], strlen(argv[1]), plg_JobCreateLua(argv[2], strlen(argv[2]), argv[3], strlen(argv[3])));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		}
	}
	else if (!strcasecmp(command, "max")) {
		if (pManage != 0) {
			if (argc == 2) {
				plg_MngSetMaxTableWeight(pManage, atoi(argv[1]));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		}
	}
	else if (!strcasecmp(command, "table")) {
		if (pManage != 0) {
			if (argc == 3) {
				plg_MngAddTable(pManage, argv[1], strlen(argv[1]), argv[2], strlen(argv[2]));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		}
	}
	else if (!strcasecmp(command, "weight")) {
		if (pManage != 0) {
			if (argc == 3) {
				plg_MngSetWeight(pManage, argv[1], strlen(argv[1]), atoi(argv[2]));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		}
	}
	else if (!strcasecmp(command, "share")) {
		if (pManage != 0) {
			if (argc == 3) {
				plg_MngSetNoShare(pManage, argv[1], strlen(argv[1]), atoi(argv[2]));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		}
	}
	else if (!strcasecmp(command, "save")) {
		if (pManage != 0) {
			if (argc == 3) {
				plg_MngSetNoSave(pManage, argv[1], strlen(argv[1]), atoi(argv[2]));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		}
	}
	else if (!strcasecmp(command, "aj")) {
		if (pManage != 0) {
			if (argc == 2) {
				plg_MngAllocJob(pManage, atoi(argv[1]));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		}
	}
	else if (!strcasecmp(command, "fj")) {
		if (pManage != 0) {
			if (argc == 1) {
				plg_MngFreeJob(pManage);
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		}
	}
	else if (!strcasecmp(command, "rc")) {
		if (pManage != 0) {
			if (argc == 3) {
				plg_MngRemoteCall(pManage, argv[1], strlen(argv[1]), argv[2], strlen(argv[2]));
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		}
	}
	else if (!strcasecmp(command, "pas")) {
		if (pManage != 0) {
			if (argc == 1) {
				plg_MngPrintAllStatus(pManage);
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		}
	}
	else if (!strcasecmp(command, "pajs")) {
		if (pManage != 0) {
			if (argc == 1) {
				plg_MngPrintAllJobStatus(pManage);
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		}
	}
	else if (!strcasecmp(command, "pajd")) {
		if (pManage != 0) {
			if (argc == 1) {
				plg_MngPrintAllJobDetails(pManage);
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		}
	}
	else if (!strcasecmp(command, "ppa")) {
		if (pManage != 0) {
			if (argc == 1) {
				plg_MngPrintPossibleAlloc(pManage);
			} else {
				printf("Parameter does not meet the requirement\n");
			}
		}
	}
	else if (!strcasecmp(command, "base")) {
		plg_BaseAll();
	} 
	else if (!strcasecmp(command, "simple")) {
		plg_simple();
	}
	else if (!strcasecmp(command, "fe")) {
		PRFESA();
	}

	NOTUSED(plg_DiskFlushDirtyToFile);
	return 1;
}

static sds ReadArgFromStdin(void) {
	char buf[1024];
	sds arg = plg_sdsEmpty();

	while (1) {
		int nread = (int)read(fileno(stdin), buf, 1024);
		if (nread == 0) break;
		else if (nread == -1) {
			perror("Reading from standard input");
			exit(1);
		}
		arg = plg_sdsCatLen(arg, buf, nread);
		if (arg[nread-1] == '\n') break;
	}
	return arg;
}

int plg_Interactive(FUNIssueCommand pIssueCommand) {
	while (1) {
		sds ptr = ReadArgFromStdin();
		int vlen;
		sds *v = plg_sdsSplitLen(ptr, (int)plg_sdsLen(ptr) - 1, " ", 1, &vlen);
		plg_sdsFree(ptr);
		int ret = pIssueCommand(vlen, v);
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
				plg_StartFromJsonFile(argv[i + 1]);
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
			if (checkArg(argv[i + 1]) && checkArg(argv[i + 2])) {
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
	plg_LogInit();
	printf("Welcome to pelgia!\n");

	plg_LogSetErrFile();
	plg_LogSetMaxLevel(log_all);

	elog(log_warn, "--------Welcome to pelgia!--------");
	
	if (plg_ReadArgFromParam(argc, argv))
		return plg_Interactive(IssueCommand);
	else
		return 1;
}
