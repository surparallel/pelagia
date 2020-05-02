/* padlist.h - A generic doubly linked list implementation
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ADLIST_H__
#define __ADLIST_H__

#define LIST_BIG 10000
#define LIST_MIDDLE 1000
#define LIST_SMALL 100

/* Node, List, and Iterator are the only data structures used currently. */

typedef struct listNode {
    struct listNode *prev;
    struct listNode *next;
    void *value;
} listNode;

typedef struct listIter {
    listNode *next;
    int direction;
} listIter;

typedef struct list {
    listNode *head;
    listNode *tail;
    void *(*dup)(void *ptr);
    void (*lfree)(void *ptr);
    int (*match)(void *ptr, void *key);
    unsigned int len;
	void* memoryPool;
	short pool;
} list;

/* Functions implemented as macros */
#define listLength(l) ((l)->len)
#define listFirst(l) ((l)->head)
#define listLast(l) ((l)->tail)
#define listPrevNode(n) ((n)->prev)
#define listNextNode(n) ((n)->next)
#define listNodeValue(n) ((n)->value)

#define listSetDupMethod(l,m) ((l)->dup = (m))
#define listSetFreeMethod(l,m) ((l)->lfree = (m))
#define listSetMatchMethod(l,m) ((l)->match = (m))

#define listGetDupMethod(l) ((l)->dup)
#define listGetFree(l) ((l)->lfree)
#define listGetMatchMethod(l) ((l)->match)

/* Prototypes */
list *plg_listCreate(short pool);
void plg_listRelease(list *list);
void plg_listEmpty(list *list);
listNode *plg_listAddNodeHead(list *list, void *value);
listNode *plg_listAddNodeTail(list *list, void *value);
listNode *plg_listInsertNode(list *list, listNode *old_node, void *value, int after);
void plg_listDelNode(list *list, listNode *node);
listIter *plg_listGetIterator(list *list, int direction);
listNode *plg_listNext(listIter *iter);
listIter *plg_listIteratorDup(listIter *li);
void plg_listReleaseIterator(listIter *iter);
list *plg_listDup(list *orig);
listNode *plg_listSearchKey(list *list, void *key);
listNode *plg_listIndex(list *list, int index);
void plg_listRewind(list *list, listIter *li);
void plg_listRewindTail(list *list, listIter *li);
void plg_listRotate(list *list);
void plg_listJoin(list *l, list *o);

void plg_listDelNodeKeepMem(list *list, listNode *node);
list *plg_listAddNodeTailKeepMem(list *list, listNode *node);
list *plg_listAddNodeHeadKeepMem(list *list, listNode *node);
/* Directions for iterators */
#define AL_START_HEAD 0
#define AL_START_TAIL 1

#endif /* __ADLIST_H__ */
