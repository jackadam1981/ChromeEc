/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Touch data encoding/decoding */

#include "common.h"
#include "debug.h"
#include "touch_scan.h"
#include "util.h"

#define BUF_SIZE 5700
static uint8_t encoded[BUF_SIZE];
static int encoded_size;

void encode_reset(void)
{
	encoded_size = 0;
}

void encode_add_column(const uint8_t *dptr)
{
	int seg_count = 0;
	int data_count = 1; /* Need 1 byte to store segment count */
	int p, p_start;
	uint8_t *eptr = encoded + encoded_size, *e_seg_size;

	p = 0;
	while (p < ROW_COUNT * 2) {
		if (dptr[p++] < THRESHOLD)
			continue;
		seg_count++;
		data_count += 3;
		while (p < ROW_COUNT * 2 && dptr[p++] >= THRESHOLD)
			data_count++;
	}

	/* Just give up if the buffer is full */
	if (encoded_size + data_count > BUF_SIZE)
		return;
	encoded_size += data_count;
	*(eptr++) = seg_count;

	p = 0;
	while (p < ROW_COUNT * 2) {
		if (dptr[p] < THRESHOLD) {
			++p;
			continue;
		}

		/* Save current position */
		*(eptr++) = p;

		/* Leave a byte for storing segment size */
		e_seg_size = eptr;
		eptr++;

		/* Record segment starting point */
		p_start = p;

		/* Save the segment */
		while (p < ROW_COUNT * 2 && dptr[p] >= THRESHOLD)
			*(eptr++) = dptr[p++];

		/* Fill in the segment size now that we know it */
		*e_seg_size = p - p_start;
	}
}

void encode_dump_matrix(void)
{
	uint8_t *dptr;
	int row, col;
	int seg_count;
	int seg;
	int seg_end;

	debug_printf("encoded size = %d\n", encoded_size);

	dptr = encoded;
	for (col = 0; col < COL_COUNT * 2; ++col) {
		if (dptr >= encoded + encoded_size) {
			for (row = 0; row < ROW_COUNT * 2; ++row)
				debug_printf("  - ");
			debug_printf("\n");
			continue;
		}
		seg_count = *(dptr++);
		row = 0;
		for (seg = 0; seg < seg_count; ++seg) {
			while (row < *dptr) {
				debug_printf("  - ");
				row++;
			}
			dptr++;
			seg_end = *dptr + row;
			dptr++;
			for (; row < seg_end; ++row, ++dptr)
				debug_printf("%3d ", *dptr);
		}
		while (row < ROW_COUNT * 2) {
			debug_printf("  - ");
			row++;
		}
		debug_printf("\n");
	}
}
