/* disk.h
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
#ifndef __CMD_H
#define __CMD_H

#ifndef _TEST_
#define TMAIN __main
#define PMIAN main
#else
#define TMAIN main
#define PMIAN __main
#endif

typedef int(*FUNIssueCommand)(int argc, char **argv, int noFind);

void plg_CliOutputGenericHelp(void);
int plg_Interactive(FUNIssueCommand pIssueCommand);
int plg_ReadArgFromParam(int argc, char **argv);
int plg_IssueCommand(int argc, char **argv, int noFind);

#endif