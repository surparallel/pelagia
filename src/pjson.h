/*
  Copyright (c) 2009 Dave Gamble
 
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
 
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
 
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef PJSON__H
#define PJSON__H

#ifdef __cplusplus
extern "C"
{
#endif

/* pJSON Types: */
#define pJson_False 0
#define pJson_True 1
#define pJson_NULL 2
#define pJson_Number 3
#define pJson_String 4
#define pJson_Array 5
#define pJson_Object 6
	
#define pJson_IsReference 256
#define pJson_StringIsConst 512

/* The pJSON structure: */
typedef struct pJSON {
	struct pJSON *next,*prev;	/* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
	struct pJSON *child;		/* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */

	int type;					/* The type of the item, as above. */

	char *valuestring;			/* The item's string, if type==pJson_String */
	int valueint;				/* The item's number, if type==pJson_Number */
	double valuedouble;			/* The item's number, if type==pJson_Number */

	char *string;				/* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
} pJSON;

typedef struct pJson_Hooks {
	void *(*malloc_fn)(size_t sz);
      void (*free_fn)(void *ptr);
} pJson_Hooks;

/* Supply malloc, realloc and free functions to pJSON */
extern void pJson_InitHooks(pJson_Hooks* hooks);


/* Supply a block of JSON, and this returns a pJSON object you can interrogate. Call pJson_Delete when finished. */
extern pJSON *pJson_Parse(const char *value);
/* Render a pJSON entity to text for transfer/storage. Free the char* when finished. */
extern char  *pJson_Print(pJSON *item);
/* Render a pJSON entity to text for transfer/storage without any formatting. Free the char* when finished. */
extern char  *pJson_PrintUnformatted(pJSON *item);
/* Render a pJSON entity to text using a buffered strategy. prebuffer is a guess at the final size. guessing well reduces reallocation. fmt=0 gives unformatted, =1 gives formatted */
extern char *pJson_PrintBuffered(pJSON *item,int prebuffer,int fmt);
/* Delete a pJSON entity and all subentities. */
extern void   pJson_Delete(pJSON *c);

/* Returns the number of items in an array (or object). */
extern int	  pJson_GetArraySize(pJSON *array);
/* Retrieve item number "item" from array "array". Returns NULL if unsuccessful. */
extern pJSON *pJson_GetArrayItem(pJSON *array,int item);
/* Get item "string" from object. Case insensitive. */
extern pJSON *pJson_GetObjectItem(pJSON *object,const char *string);

/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when pJson_Parse() returns 0. 0 when pJson_Parse() succeeds. */
extern const char *pJson_GetErrorPtr(void);
	
/* These calls create a pJSON item of the appropriate type. */
extern pJSON *pJson_CreateNull(void);
extern pJSON *pJson_CreateTrue(void);
extern pJSON *pJson_CreateFalse(void);
extern pJSON *pJson_CreateBool(int b);
extern pJSON *pJson_CreateNumber(double num);
extern pJSON *pJson_CreateString(const char *string);
extern pJSON *pJson_CreateStringWihtLen(const char *string, size_t len);
extern pJSON *pJson_CreateArray(void);
extern pJSON *pJson_CreateObject(void);

/* These utilities create an Array of count items. */
extern pJSON *pJson_CreateIntArray(const int *numbers,int count);
extern pJSON *pJson_CreateFloatArray(const float *numbers,int count);
extern pJSON *pJson_CreateDoubleArray(const double *numbers,int count);
extern pJSON *pJson_CreateStringArray(const char **strings,int count);

/* Append item to the specified array/object. */
extern void pJson_AddItemToArray(pJSON *array, pJSON *item);
extern void	pJson_AddItemToObject(pJSON *object,const char *string,pJSON *item);
extern void pJson_AddItemToObjectWithLen(pJSON *object, const char *string, size_t len, pJSON *item);
extern void	pJson_AddItemToObjectCS(pJSON *object,const char *string,pJSON *item);	/* Use this when string is definitely const (i.e. a literal, or as good as), and will definitely survive the pJSON object */
/* Append reference to item to the specified array/object. Use this when you want to add an existing pJSON to a new pJSON, but don't want to corrupt your existing pJSON. */
extern void pJson_AddItemReferenceToArray(pJSON *array, pJSON *item);
extern void	pJson_AddItemReferenceToObject(pJSON *object,const char *string,pJSON *item);

/* Remove/Detatch items from Arrays/Objects. */
extern pJSON *pJson_DetachItemFromArray(pJSON *array,int which);
extern void   pJson_DeleteItemFromArray(pJSON *array,int which);
extern pJSON *pJson_DetachItemFromObject(pJSON *object,const char *string);
extern void   pJson_DeleteItemFromObject(pJSON *object,const char *string);
	
/* Update array items. */
extern void pJson_InsertItemInArray(pJSON *array,int which,pJSON *newitem);	/* Shifts pre-existing items to the right. */
extern void pJson_ReplaceItemInArray(pJSON *array,int which,pJSON *newitem);
extern void pJson_ReplaceItemInObject(pJSON *object,const char *string,pJSON *newitem);

/* Duplicate a pJSON item */
extern pJSON *pJson_Duplicate(pJSON *item,int recurse);
/* Duplicate will create a new, identical pJSON item to the one you pass, in new memory that will
need to be released. With recurse!=0, it will duplicate any children connected to the item.
The item->next and ->prev pointers are always zero on return from Duplicate. */

/* ParseWithOpts allows you to require (and check) that the JSON is null terminated, and to retrieve the pointer to the final byte parsed. */
extern pJSON *pJson_ParseWithOpts(const char *value,const char **return_parse_end,int require_null_terminated);

extern void pJson_Minify(char *json);

/* Macros for creating things quickly. */
#define pJson_AddNullToObject(object,name)		pJson_AddItemToObject(object, name, pJson_CreateNull())
#define pJson_AddTrueToObject(object,name)		pJson_AddItemToObject(object, name, pJson_CreateTrue())
#define pJson_AddFalseToObject(object,name)		pJson_AddItemToObject(object, name, pJson_CreateFalse())
#define pJson_AddBoolToObject(object,name,b)	pJson_AddItemToObject(object, name, pJson_CreateBool(b))
#define pJson_AddNumberToObject(object,name,n)	pJson_AddItemToObject(object, name, pJson_CreateNumber(n))
#define pJson_AddStringToObject(object,name,s)	pJson_AddItemToObject(object, name, pJson_CreateString(s))
#define pJson_AddStringToObjectWithLen(object,name,ln,s,ls)	pJson_AddItemToObjectWithLen(object, name, ln, pJson_CreateStringWihtLen(s,ls))

/* When assigning an integer value, it needs to be propagated to valuedouble too. */
#define pJson_SetIntValue(object,val)			((object)?(object)->valueint=(object)->valuedouble=(val):(val))
#define pJson_SetNumberValue(object,val)		((object)?(object)->valueint=(object)->valuedouble=(val):(val))

#ifdef __cplusplus
}
#endif

#endif
