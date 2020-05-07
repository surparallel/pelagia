/* equeue.h
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

#ifndef __EQUEUE_H
#define __EQUEUE_H

typedef void(*QueuerDestroyFun)(void* value);
void* plg_eqCreate();
void plg_eqPush(void* pEventQueue, void* value);
int plg_eqIfNoPush(void* pvEventQueue, void* value, unsigned int maxQueue);
int plg_eqTimeWait(void* pEventQueue, long long sec, long long nsec);
int plg_eqWait(void* pEventQueue);
void* plg_eqPop(void* pEventQueue);
void* plg_eqPopWithLen(void* pvEventQueue, unsigned int *len);
void plg_eqDestory(void* pEventQueue, QueuerDestroyFun fun);

#endif