/* bitarray.c - bit operation
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

#include "plateform.h"
#include "pbitarray.h"
#define SHIFT 3  
#define MASK 0x7  

unsigned char* plg_BitArrayInit(unsigned int size){
	unsigned char *tmp;
	tmp = (unsigned char*)malloc(size / 8 + 1);
	//initial to 0  
	memset(tmp, 0, (size / 8 + 1)); 
	return tmp;
}

/* 
Num: represents the position to be inserted into the array, starting from zero
*/
void plg_BitArrayAdd(unsigned char *bitarr, unsigned int num){
	bitarr[num >> SHIFT] |= (1 << (num & MASK));
}

int plg_BitArrayIsIn(unsigned char *bitarr, unsigned int num){
	return bitarr[num >> SHIFT] & (1 << (num & MASK));
}

void plg_BitArrayClear(unsigned char *bitarr, unsigned int num){
	bitarr[num >> SHIFT] &= ~(1 << (num & MASK));
}

#ifdef  TEST
void test(char *bitarr){
	if (plg_BitArrayIsIn(bitarr, 25) != 0)
		printf("25 in\n");
	else
		printf("25 not in\n");
	if (plg_BitArrayIsIn(bitarr, 30) != 0)
		printf("30 in\n");
	else
		printf("30 not in\n");
}

int main(){
	char *arr;

	arr = plg_BitArrayInit(100);
	plg_BitArrayAdd(arr, 25);
	test(arr);
	plg_BitArrayClear(arr, 25);
	test(arr);
	getchar();
	return 0;
}
#endif //  TEST