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
#include "fifo_mirror.h"

void fifo_mirror_init(struct fifo_mirror *que,
			void *base,
			uint16_t *p_read_idx,
			uint16_t *p_write_idx,
			uint8_t size_n)
{
	que->size_n      = size_n;
	que->base        = base;
	que->p_write_idx = p_write_idx;
	que->p_read_idx  = p_read_idx;

	/* init write pointer to zero */
	if (p_write_idx)
		*p_write_idx = 0;

	/* init read pointer to zero */
	if (p_read_idx)
		*p_read_idx = 0;
}

/*
 * The max capacity of a FIFO is always power of two.
 * @return the max capacity of the fifo
 */
static uint16_t get_size(struct fifo_mirror *que)
{
	return 1 << (que->size_n);
}

/*
 * get_index_mask
 * @return index bit-mask (1-bit more than size-bit-mask)
 */
static uint16_t get_index_mask(struct fifo_mirror *que)
{
	return (1 << (que->size_n + 1)) - 1;
}

/*
 * get read index value
 */
static uint16_t get_read_idx(struct fifo_mirror *que)
{
	return (*(que->p_read_idx)) & get_index_mask(que);
}

/*
 * set read index value
 */
static uint16_t set_read_idx(struct fifo_mirror *que, uint16_t idx)
{
	return *(que->p_read_idx) = idx & get_index_mask(que);
}

/*
 * get write index value
 */
static uint16_t get_write_idx(struct fifo_mirror *que)
{
	return *(que->p_write_idx) & get_index_mask(que);
}

/*
 * set write index value
 */
static uint16_t set_write_idx(struct fifo_mirror *que, uint16_t idx)
{
	return *(que->p_write_idx) = idx & get_index_mask(que);
}


int fifo_mirror_is_full(struct fifo_mirror *que)
{
	return (get_read_idx(que) ^ get_write_idx(que)) == get_size(que);
}

int fifo_mirror_is_empty(struct fifo_mirror *que)
{
	return get_read_idx(que) == get_write_idx(que);
}

int fifo_mirror_get_size(struct fifo_mirror *que)
{
	uint16_t write_idx = get_write_idx(que);
	uint16_t read_idx  = get_read_idx(que);

	return (read_idx > write_idx) ?
		(write_idx + (get_size(que)  << 1) - read_idx)
		: (write_idx - read_idx);
}

int fifo_mirror_enque8(struct fifo_mirror *que, uint8_t val)
{
	uint16_t n = 0;
	uint16_t widx;
	if (!fifo_mirror_is_full(que)) {
		widx = get_write_idx(que);
		que->base8[widx & (get_size(que) - 1)] = val;
		set_write_idx(que, ++widx);
		n++;
	}
	return n;
}

volatile uint8_t *fifo_mirror_at8(struct fifo_mirror *que, int ith)
{
	register int size = fifo_mirror_get_size(que);
	if (size > ith)
		return &que->base8[(get_read_idx(que) + ith) &
					(get_size(que) - 1)];
	return NULL;
}

volatile uint8_t *fifo_mirror_top8(struct fifo_mirror *que)
{
	return fifo_mirror_at8(que, 0);
}

int fifo_mirror_deque8(struct fifo_mirror *que, uint8_t *p_val)
{
	uint16_t ridx;
	if (!fifo_mirror_is_empty(que)) {
		ridx = get_read_idx(que);
		*p_val = *fifo_mirror_top8(que);
		set_read_idx(que, ++ridx);
		return sizeof(uint8_t);
	}
	return 0;
}

int fifo_mirror_enque32(struct fifo_mirror *que, uint32_t val)
{
	uint16_t n = 0;
	uint16_t widx;
	if (!fifo_mirror_is_full(que)) {
		widx = get_write_idx(que);
		/* check if write index is 4-byte aligned */
		if (widx  & 0x3)
			return n;
		que->base32[((widx  & (get_size(que) - 1)) >> 2)] = val;
		set_write_idx(que, widx + sizeof(uint32_t));
		n += sizeof(uint32_t);
	}
	return n;
}

volatile uint32_t *fifo_mirror_at32(struct fifo_mirror *que, int ith)
{
	register int size = fifo_mirror_get_size(que) >> 2;
	if (size > ith) {
		register int rd = get_read_idx(que) + (ith << 2);
		rd &= (get_size(que) - 1);
		/* check if read index is 4-byte aligned */
		if (rd & 0x3)
			return NULL;
		return &que->base32[rd >> 2];
	}
	return NULL;
}

volatile uint32_t *fifo_mirror_top32(struct fifo_mirror *que)
{
	return fifo_mirror_at32(que, 0);
}

int fifo_mirror_deque32(struct fifo_mirror *que, uint32_t *p_val)
{
	register int size = fifo_mirror_get_size(que);
	volatile uint32_t *p32;
	if (size >= sizeof(uint32_t)) {
		p32 = fifo_mirror_top32(que);
		if (p32) {
			*p_val = *p32;
			set_read_idx(que, get_read_idx(que) + sizeof(uint32_t));
			return sizeof(uint32_t);
		}
	}
	return 0;
}

#ifdef SELF_TEST

#define CPRINTS printf

void fifo_mirror_print8(struct fifo_mirror *que)
{
	register int size = fifo_mirror_get_size(que);
	int i;
	CPRINTS("---------sz: %d -------------------\n", size);
	for (i = 0; i < size; i++)
		if (fifo_mirror_at8(que, i))
			CPRINTS("%02X\n", *fifo_mirror_at8(que, i));
		else
			CPRINTS("i: %d/%d ERROR\n", i, size);
}

void fifo_mirror_print32(struct fifo_mirror *que)
{
	register int size = fifo_mirror_get_size(que);
	int i;
	CPRINTS("---------sz: %d -------------------\n", size);
	size >>= 2;
	for (i = 0; i < size; i++)
		if (fifo_mirror_at32(que, i))
			CPRINTS("%08X\n", *fifo_mirror_at32(que, i));
		else
			CPRINTS("i: %d/%d ERROR\n", i, size);
}

int main(int argc, char *argv[])
{
	struct fifo_mirror que;
	uint32_t buffer[(128 >> 2)];
	uint32_t base32;
	uint8_t base8;
	uint16_t rd_idx = 0;
	uint16_t wd_idx = 0;
	int debug = 0;

	fifo_mirror_init(&que, buffer, &rd_idx, &wd_idx, 7);

	base32 = 1;
	while (!fifo_mirror_deque32(&que, &base32))
		fifo_mirror_enque8(&que, base32++);
	if (!fifo_mirror_is_empty(&que))
		CPRINTS("ERROR: SelfTest 1 Failed.\n");
	else
		CPRINTS("Passed: SelfTest 1.\n");

	while (fifo_mirror_enque8(&que, base32++))
		;
	if (!fifo_mirror_is_full(&que))
		CPRINTS("ERROR: SelfTest 2 Failed.\n");
	else
		CPRINTS("Passed: SelfTest 2.\n");

	if (debug)
		fifo_mirror_print8(&que);
	while (fifo_mirror_deque8(&que, &base8))
		;
	if (!fifo_mirror_is_empty(&que))
		CPRINTS("ERROR: SelfTest 3 Failed.\n");
	else
		CPRINTS("Passed: SelfTest 3.\n");

	base32 = 1;
	while (fifo_mirror_enque8(&que, base32++))
		;
	if (!fifo_mirror_is_full(&que))
		CPRINTS("ERROR: SelfTest 4 Failed.\n");
	else
		CPRINTS("Passed: SelfTest 4.\n");
	if (debug)
		fifo_mirror_print32(&que);
	while (fifo_mirror_deque32(&que, &base32) == sizeof(uint32_t))
		;
	if (!fifo_mirror_is_empty(&que))
		CPRINTS("ERROR: SelfTest 5 Failed.\n");
	else
		CPRINTS("Passed: SelfTest 5.\n");

	base32 = 1;
	while (fifo_mirror_enque32(&que, base32++))
		;
	if (!fifo_mirror_is_full(&que))
		CPRINTS("ERROR: SelfTest 6 Failed.\n");
	else
		CPRINTS("Passed: SelfTest 6.\n");
	if (debug)
		fifo_mirror_print8(&que);
	while (fifo_mirror_deque8(&que, &base8))
		;
	if (!fifo_mirror_is_empty(&que))
		CPRINTS("ERROR: SelfTest 7 Failed.\n");
	else
		CPRINTS("Passed: SelfTest 7.\n");

	base32 = 1;
	while (fifo_mirror_enque32(&que, base32++))
		;
	if (!fifo_mirror_is_full(&que))
		CPRINTS("ERROR: SelfTest 8 Failed.\n");
	else
		CPRINTS("Passed: SelfTest 8.\n");
	if (debug)
		fifo_mirror_print32(&que);
	while (fifo_mirror_deque32(&que, &base32) == sizeof(uint32_t))
		;
	if (!fifo_mirror_is_empty(&que))
		CPRINTS("ERROR: SelfTest 9 Failed.\n");
	else
		CPRINTS("Passed: SelfTest 9.\n");
	return 0;
}
#endif
