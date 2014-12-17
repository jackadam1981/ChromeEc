/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/*
 * A FIFO or Circular Buffer
 *  - Implementation: mirroring method
 *  - Max size: 128 elements
 */
#include <stdint.h>
#include <stdio.h>
#include "fifo128.h"

void fifo128_init(struct fifo128 *que)
{
	que->write_ptr = 0;
	que->read_ptr  = 0;
}

int fifo128_is_full(struct fifo128 *que)
{
	return ((que->write_ptr ^ que->read_ptr) ==  FIFO_SIZE_128);
}

int fifo128_is_empty(struct fifo128 *que)
{
	return (que->read_ptr == que->write_ptr);
}

int fifo128_get_size(struct fifo128 *que)
{
	return (que->read_ptr > que->write_ptr) ?
		(que->write_ptr + (FIFO_SIZE_128  << 1) - que->read_ptr)
		: (que->write_ptr - que->read_ptr);
}

int fifo128_enque8(struct fifo128 *que, uint8_t k)
{
	register int is_full = fifo128_is_full(que);
	if (!is_full) {
		que->d8[que->write_ptr & FIFO_SIZE_D8_MASK] = k;
		que->write_ptr++;
	}
	return is_full;
}

uint8_t *fifo128_at8(struct fifo128 *que, int ith)
{
	register int size = fifo128_get_size(que);
	if (size > ith)
		return &que->d8[(que->read_ptr + ith) & FIFO_SIZE_D8_MASK];
	return NULL;
}

uint8_t *fifo128_top8(struct fifo128 *que)
{
	return fifo128_at8(que, 0);
}

int fifo128_deque8(struct fifo128 *que, uint8_t *pK)
{
	register int is_empty =  fifo128_is_empty(que);
	if (!is_empty) {
		*pK = que->d8[que->read_ptr & FIFO_SIZE_D8_MASK];
		que->read_ptr++;
	}
	return is_empty;
}

int fifo128_enque32(struct fifo128 *que, uint32_t k)
{
	register int is_full = fifo128_is_full(que);
	if (!is_full) {
		que->d32[(que->write_ptr >> 2) & FIFO_SIZE_D32_MASK] = k;
		que->write_ptr += sizeof(uint32_t);
	}
	return is_full;
}

uint32_t *fifo128_at32(struct fifo128 *que, int ith)
{
	register int size = fifo128_get_size(que) >> 2;
	if (size > ith) {
		register int rd = (que->read_ptr >> 2) + ith;
		return &que->d32[rd & FIFO_SIZE_D32_MASK];
	}
	return NULL;
}

uint32_t *fifo128_top32(struct fifo128 *que)
{
	return fifo128_at32(que, 0);
}

int fifo128_deque32(struct fifo128 *que, uint32_t *pK)
{
	register int size = fifo128_get_size(que);
	if (size >= sizeof(uint32_t)) {
		*pK = que->d32[(que->read_ptr >> 2) & FIFO_SIZE_D32_MASK];
		que->read_ptr += sizeof(uint32_t);
		return sizeof(uint32_t);
	}
	return 0;
}

#ifdef SELF_TEST
void fifo128_print8(struct fifo128 *que)
{
	register int size = fifo128_get_size(que);
	int i;
	CPRINTS("---------sz: %d -------------------\n", size);
	for (i = 0; i < size; i++)
		if (fifo128_at8(que, i))
			CPRINTS("%02X\n", *fifo128_at8(que, i));
		else
			CPRINTS("i: %d/%d ERROR\n", i, size);
}

void fifo128_print32(struct fifo128 *que)
{
	register int size = fifo128_get_size(que);
	int i;
	CPRINTS("---------sz: %d -------------------\n", size);
	size >>= 2;
	for (i = 0; i < size; i++)
		if (fifo128_at32(que, i))
			CPRINTS("%08X\n", *fifo128_at32(que, i));
		else
			CPRINTS("i: %d/%d ERROR\n", i, size);
}

int main(int argc, char *argv[])
{
	fifo128 que;
	uint32_t d32;
	uint8_t d8;
	fifo128_init(&que);
	d32 = 0;
	while (!fifo128_enque8(&que, d32++))
		;
	if (fifo128_top8(&que))
		CPRINTS("top:%02X\n", *fifo128_top8(&que));
	fifo128_print8(&que);
	while (!fifo128_deque8(&que, &d8))
		;

	d32 = 0;
	while (!fifo128_enque8(&que, d32++))
		;
	if (fifo128_top8(&que))
		CPRINTS("top:%02X\n", *fifo128_top8(&que));
	fifo128_print32(&que);
	while (fifo128_deque32(&que, &d32) >= sizeof(uint32_t))
		;

	d32 = 0;
	while (!fifo128_enque32(&que, d32++))
		;
	if (fifo128_top32(&que))
		CPRINTS("top:%08X\n", *fifo128_top32(&que));
	fifo128_print8(&que);
	while (!fifo128_deque8(&que, &d8))
		;

	d32 = 0;
	while (!fifo128_enque32(&que, d32++))
		;
	if (fifo128_top32(&que))
		CPRINTS("top:%08X\n", *fifo128_top32(&que));
	fifo128_print32(&que);
	while (fifo128_deque32(&que, &d32) >= sizeof(uint32_t))
		;
	return 0;
}
#endif
