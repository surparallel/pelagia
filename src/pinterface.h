/* interface.h
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
#ifndef __INTERFACE_H
#define __INTERFACE_H

#define _PAGESIZE_ 64
#define _PAGEAMOUNT_ 2
#define FULLSIZE(PS) PS * 1024

#define _ARRANGMENTTIME_ 100
#define _ARRANGMENTPERCENTAGE_1 20
#define _ARRANGMENTCOUNT_1 150
#define _ARRANGMENTPERCENTAGE_2 40
#define _ARRANGMENTCOUNT_2 600
#define _ARRANGMENTPERCENTAGE_3 60
#define _ARRANGMENTCOUNT_3 1000
#define _ARRANGMENTPERCENTAGE_4 80
#define _ARRANGMENTCOUNT_4 1500

//page type
enum PageType {
	BITPAGE = 1,
	TABLEPAGE,
	TABLEUSING,
	VALUEPAGE,
	VALUEUSING
};

//page type
enum ValueType {
	VALUE_NORMAL = 1,
	VALUE_BIGVALUE,
	VALUE_SETHEAD
};

#define SKIPLIST_MAXLEVEL 8

#define OFFSET(page, point) ((unsigned char *)point - (unsigned char *)page)
#define POINTER(page, offset) (offset + (unsigned char *)page)

//Data format stored on file
#pragma pack(push,1)
/*
Page header structure
Id: page ID
Type: page type, including disk page, empty page record, key page, value page, usage record page of key page, etc
Different types of pages have different structures
Hitstamp: timestamp of page hit used for cache scheduling
Writestamp: time of writing to database, updated version, used to compare old and new data in AP process
Allocstamp: used to record the time of page creation, compare with the time of bitpage writing to the hard disk, and confirm that the relevant bitpage has been updated to the hard disk
Thread: the thread that last used the page
Event: the last event using the page is used to determine whether there is unsafe usage data.
Prevpage: the front page of a two-way linked list of the same type under the same table
NextPage: the next page of a two-way linked list of the same type under the same table
CRC: check bit of remaining data
*/
typedef struct _DiskPageHead
{
	unsigned int addr;
	unsigned char type;
	unsigned long long hitStamp;
	unsigned long long writeStamp;
	unsigned long long allocStamp;
	unsigned int prevPage;
	unsigned int nextPage;
	unsigned short crc;
} *PDiskPageHead, DiskPageHead;

/*
Skip list
Compress the data as much as possible to avoid data wasting at 8 times the scale
Different levels of the same element must share the key in one page
Providing two-way linked list will provide more functions. Here, using one-way linked list can meet the needs
Jump table query does not need to go home, there is no need to reverse jump
Because the complexity of forward query and reverse query is the same
So there is no need to reverse query
Nexterelementpage: the page number of the next element
Nexterelementoffset: next element in page offset, in char
Highelementoffset: recall from bottom to top when deleting
Lowelementoffset: the page offset of the next level, in char
Currentlevel: the level of the current hop table is 0-7. If it is 0, the link of the previous hop table is required
Keyoffset: if the current level is 0, it is the intra page offset of key value, in char
*/
typedef struct _DiskTableElement
{
	unsigned int nextElementPage;
	unsigned short nextElementOffset;
	unsigned short highElementOffset;
	unsigned short lowElementOffset;
	unsigned char currentLevel;
	unsigned short keyOffset;
} *PDiskTableElement, DiskTableElement;

/*
Separating table handle for recursive use
Tablehead: the address of the page where the first item of a table is located.
Tablepagehead: leave a placeholder when deleting a table
Tableusingpage: is an array page of page usage under the table.
Value page: the first page address of the value page.
Value using page: the first page of value using page is used to quickly find the usage of value page.
Issethead: 1 flag is set
*/
typedef struct _TableInFile
{
	DiskTableElement tableHead[SKIPLIST_MAXLEVEL];
	unsigned int tablePageHead;
	unsigned int tableUsingPage;
	unsigned int valuePage;
	unsigned int valueUsingPage;
	unsigned short isSetHead;
}*PTableInFile, TableInFile;

/*
head array
usingPageAddr:using page of cureent table page
usingPageOffset:offset of using page
Tablelength: actual array length,
Tablesize: the number of hops that have been used. Subtracting tablelength is the number of vacancies that have been deleted
Spaceaddr: first address of free space
Spacelength: free space length
Usinglength: the length of used space. The total space length minus the length of used space minus the length of available space is the length of debris space
Arrangmentstamp: last collated timestamp
Delcount: number of deletions
Element: index of array
*/
typedef struct _DiskTablePage
{
	unsigned int usingPageAddr;
	unsigned short usingPageOffset;
	short tableLength;
	unsigned short tableSize;
	unsigned short spaceAddr;
	unsigned short spaceLength;
	unsigned short usingLength;
	unsigned long long arrangmentStamp;
	unsigned int delCount;
	DiskTableElement element[];
} *PDiskTablePage, DiskTablePage;

/*
prevElementPage:Previous page of double linked list
prevElementOffset:Offset in the previous page of a double linked list
ValueType: the type 1 direct storage type is normal, the type 2 is set, the type 2 is set's tabaleinfile structure, and the type 3 is big data, the type 3 is page address
Valuesize: value length
Keystrsize: key length
Keystr: first pointer
*/
typedef struct _DiskTableKey
{
	unsigned int prevElementPage;
	unsigned short prevElementOffset;
	char valueType;
	unsigned short valueSize;
	unsigned short keyStrSize;
	char keyStr[];
} *PDiskTableKey, DiskTableKey;

/*
arrary
Elements of tableusing
Pageaddr: page address
Spacelength: equal to spacelength of disktablepage
*/
typedef struct _DiskTableUsing
{
	unsigned int pageAddr;
	unsigned short spaceLength;
} *PDiskTableUsing, DiskTableUsing;

/*
Allspace: sum of all the free spaces in this page
Usinglength: number of arrays used
Usingsize: number of elements already assigned
*/
typedef struct _DiskTableUsingPage
{
	unsigned int allSpace;
	unsigned short usingPageLength;
	unsigned short usingPageSize;
	DiskTableUsing element[];
} *PDiskTableUsingPage, DiskTableUsingPage;

typedef struct _OrderPacket {
	void* order;
	void* value;
} *POrderPacket, OrderPacket;

typedef struct _DiskBigValue
{
	unsigned short valueSize;
	char valueBuff[];
} *PDiskBigValue, DiskBigValue;


/*
value element
*/
typedef struct _DiskValueElement
{
	unsigned int nextElementPage;
	unsigned short nextElementOffset;
	unsigned short valueOffset;
} *PDiskValueElement, DiskValueElement;

/*
head array
valueUsingPageAddr:using page of cureent table page
valueUsingPageOffset:offset of using page
Valuelength: actual array length, diskvalueelement as unit
Valuesize: the number of hops that have been used. Subtracting tablelength is the number of vacancies that have been deleted
Valuespaceaddr: first address of free space
Valuespacelength: free space length in char
Valueusinglength: the length of used space, the total space length minus the length of used space minus the length of available space is the length of the debris space
Valuearrangmentstamp: last collated timestamp
Valuedelcount: number of deletions
Valueelement: index of array

Value page can be divided into three cases: the enabled value page with Keystr greater than valueaddr, the enabled value page with Keystr smaller than size page and others
Create a new page without proper page composition, and multiple value pages larger than size page, with the total number less than 4G
Be careful! To segment value data as little as possible

Value using uses the same structure as table using disktableusingpage, disktableusing
*/
typedef struct _DiskValuePage
{
	unsigned int valueUsingPageAddr;
	unsigned short valueUsingPageOffset;
	short valueLength;
	unsigned short valueSize;
	unsigned short valueSpaceAddr;
	unsigned short valueSpaceLength;
	unsigned short valueUsingLength;
	unsigned long long valueArrangmentStamp;
	unsigned int valueDelCount;
	DiskValueElement valueElement[];
} *PDiskValuePage, DiskValuePage;

/*
Pointer to big value in key buffer
Valuepageaddr: element page address
Valueoffset: page offset of element
CRC: CRC verification
Allsize: full length
*/
typedef struct _DiskKeyBigValue
{
	unsigned int valuePageAddr;
	unsigned short valueOffset;
	unsigned short crc;
	unsigned int allSize;
} *PDiskKeyBigValue, DiskKeyBigValue;

/*
Sdsparent: mark the parent table, and the dependent table will be put into a file to keep the transaction
Weight: weight
Issave: save or not
Isshare: share or not
*/
typedef struct _TableName
{
	char* sdsParent;
	unsigned int weight;
	unsigned char noSave;
	unsigned char noShare;
}*PTableName, TableName;
#pragma pack(pop)
#endif