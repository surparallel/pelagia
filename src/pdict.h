/* Hash Tables Implementation.
 *
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
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

#include <stdint.h>

#ifndef __DICT_H
#define __DICT_H

#define DICT_OK 0
#define DICT_ERR 1

#define DICT_BIG 10000
#define DICT_MIDDLE 1000
#define DICT_SMALL 100

/* Unused arguments generate annoying warnings... */
#define DICT_NOTUSED(V) ((void) V)
typedef struct dictEntry {
    void *key;
    union {
        void *val;
        unsigned long long u64;
        int64_t s64;
        double d;
    } v;
    struct dictEntry *next;
} dictEntry;
typedef struct dictType {
    unsigned long long (*hashFunction)(const void *key);
    void *(*keyDup)(void *privdata, const void *key);
    void *(*valDup)(void *privdata, const void *obj);
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);
    void (*keyDestructor)(void *privdata, void *key);
    void (*valDestructor)(void *privdata, void *obj);
} dictType;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
typedef struct dictht {
    dictEntry **table;
    unsigned int size;
    unsigned int sizemask;
    unsigned int used;
} dictht;

typedef struct dict {
    dictType *type;
    void *privdata;
    dictht ht[2];
    int rehashidx; /* rehashing not in progress if rehashidx == -1 */
    unsigned int iterators; /* number of iterators currently running */
	void* memoryPool;
} dict;

/* If safe is set to 1 this is a safe iterator, that means, you can call
 * plg_dictAdd, plg_dictFind, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only plg_dictNext()
 * should be called while iterating. */
typedef struct dictIterator {
    dict *d;
    int index;
    int table, safe;
    dictEntry *entry, *nextEntry;
    /* unsafe iterator fingerprint for misuse detection. */
    long long fingerprint;
} dictIterator;

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);
typedef void (dictScanBucketFunction)(void *privdata, dictEntry **bucketref);

/* This is the initial size of every hash table */
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        (entry)->v.val = (d)->type->valDup((d)->privdata, _val_); \
																															    else \
        (entry)->v.val = (_val_); \
} while(0)

#define dictSetSignedIntegerVal(entry, _val_) \
    do { (entry)->v.s64 = _val_; } while(0)

#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { (entry)->v.u64 = _val_; } while(0)

#define dictSetDoubleVal(entry, _val_) \
    do { (entry)->v.d = _val_; } while(0)

#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        (entry)->key = (d)->type->keyDup((d)->privdata, _key_); \
																															    else \
        (entry)->key = (_key_); \
} while(0)

#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

#define dictHashKey(d, key) (d)->type->hashFunction(key)
#define dictGetKey(he) ((he)->key)
#define dictGetVal(he) ((he)->v.val)
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
#define dictGetDoubleVal(he) ((he)->v.d)
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
#define dictIsRehashing(d) ((d)->rehashidx != -1)

#define dictAddWithNum(d, key, val) do {\
	sds keyStr = plg_sdsFromLonglong(key);\
	if (DICT_ERR == plg_dictAdd(d, keyStr, val)) {\
		plg_sdsFree(keyStr);\
					}\
} while (0);

#define dictAddWithUint(d, key, val) do {\
	unsigned int* mkey = malloc(sizeof(unsigned int));\
	*mkey = key;\
	if (DICT_ERR == plg_dictAdd(d, mkey, val)) {\
		free(mkey);\
					}\
} while (0);

#define dictAddValueWithUint(d, key, val) do {\
	dictEntry * entry = plg_dictFind(d, key);\
	if (entry) {\
		unsigned int* mvalue = dictGetVal(entry);\
		*mvalue += val;\
	} else {\
		unsigned int* mvalue = malloc(sizeof(unsigned int));\
		*mvalue = val; \
		if (DICT_ERR == plg_dictAdd(d, key, mvalue)) {\
			free(mvalue); \
		}\
	}\
} while (0);

void* plg_DefaultUintPtr();
void* plg_DefaultBenchmarkPtr();
void* plg_DefaultNoneDictPtr();
void* plg_DefaultPtrDictPtr();
void* plg_DefaultSdsDictPtr();

/* API */
dict *plg_dictCreate(dictType *type, void *privDataPtr, short pool);
int plg_dictExpand(dict *d, unsigned int size);
int plg_dictAdd(dict *d, void *key, void *val);
dictEntry *plg_dictAddRaw(dict *d, void *key, dictEntry **existing);
dictEntry *plg_dictAddOrFind(dict *d, void *key);
int plg_dictReplace(dict *d, void *key, void *val);
int plg_dictDelete(dict *d, const void *key);
dictEntry *plg_dictUnlink(dict *ht, const void *key);
void plg_dictFreeUnlinkedEntry(dict *d, dictEntry *he);
void plg_dictRelease(dict *d);
dictEntry * plg_dictFind(dict *d, const void *key);
void *plg_dictFetchValue(dict *d, const void *key);
int plg_dictResize(dict *d);
dictIterator *plg_dictGetIterator(dict *d);
dictIterator *plg_dictGetSafeIterator(dict *d);
dictEntry *plg_dictNext(dictIterator *iter);
void plg_dictReleaseIterator(dictIterator *iter);
dictEntry *plg_dictGetRandomKey(dict *d);
unsigned int plg_dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count);
void plg_dictGetStats(char *buf, unsigned int bufsize, dict *d);
unsigned long long plg_dictGenHashFunction(const void *key, int len);
unsigned long long plg_dictGenCaseHashFunction(const unsigned char *buf, int len);
void plg_dictEmpty(dict *d, void(callback)(void*));
void plg_dictEnableResize(void);
void plg_dictDisableResize(void);
int plg_dictRehash(dict *d, int n);
void plg_dictSetHashFunctionSeed(uint8_t *seed);
uint8_t *plg_dictGetHashFunctionSeed(void);
unsigned int plg_dictScan(dict *d, unsigned int v, dictScanFunction *fn, dictScanBucketFunction *bucketfn, void *privdata);
unsigned long long plg_dictGetHash(dict *d, const void *key);
dictEntry **plg_dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, unsigned long long hash);

/* Hash table types */
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif /* __DICT_H */
