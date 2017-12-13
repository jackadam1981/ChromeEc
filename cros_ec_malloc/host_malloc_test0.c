/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stdio.h>

#include "malloc_pal.h"

#define CHUNK_SIZE (512*1024)
static uint8_t __heap[CHUNK_SIZE];

int main(int argc, char **argv)
{
	void *ptr1 = NULL, *ptr2 = NULL;
	int32_t rv;

	FpcMallocInit(__heap, sizeof(__heap));

	rv = FpcMalloc(&ptr1, 1026);
	printf("ptr1 %p => %d\n", ptr1, rv);
	rv = FpcMalloc(&ptr2, 53800);
	printf("ptr2 %p => %d\n", ptr2, rv);
	FpcFree(ptr1);
	FpcFree(ptr2);
	return 0;
}
