/* pconio.c - console
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
#include "pconio.h"
#include "plateform.h"
#include "pelog.h"

void plg_GotoXY(int x, int y) {
#ifdef _WIN32
	COORD pos;
	pos.X = x;
	pos.Y = y;
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
#else
	printf("\033[%d;%df", y, x);
#endif
}

void plg_ClrScr() {
#ifdef _WIN32
	system("cls");
#else
	if (system("clear") == -1) {
		elog(log_error, "faile cmd clear");
	}
#endif
}


void plg_Color(int c) {
#ifdef _WIN32
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), c);
#else
	printf("%c[%dm", 0x1B, c);
#endif
}

void plg_ClearColor() {
#ifdef _WIN32
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);
#else
	printf("\033[%dm", 0);
#endif
}
