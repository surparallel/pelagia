/* psemaphore.h - Time system related
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
#ifndef __PSEMAPHORE_H
#define __PSEMAPHORE_H

#ifdef __APPLE__
#include <pthread.h>
typedef struct
{
	pthread_mutex_t count_lock;
	pthread_cond_t  count_bump;
	unsigned count;
} sem_t;

int sem_init(sem_t *psem, int flags, unsigned count);
int sem_destroy(sem_t *psem);
int sem_trywait(sem_t *psem);
int sem_wait(sem_t *psem);
int sem_timedwait(sem_t *psem, const struct timespec *abstim);
int sem_post(sem_t *psem);
#endif

#endif