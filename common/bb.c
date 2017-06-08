/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "board_binary.h"
#include "common.h"
#include "console.h"
#include "link_defs.h"
#include "util.h"

#define SIZE_OFFSET	8
#define CHKS_OFFSET	12
#define BOARD_OFFSET	16

#ifdef CONFIG_COMMON_RUNTIME

#define CPRINTF(format, args...) cprintf(CC_BB, format, ## args)
#define CPRINTS(format, args...) cprints(CC_BB, format, ## args)

#else /* CONFIG_COMMON_RUNTIME */

#define CPRINTF(format, args...)
#define CPRINTS(format, args...)

#endif


static int bb_okay = -1;
static uint32_t blob_offset;

static uint32_t checksum(int len)
{
	int i;
	uint32_t *bb = (uint32_t *)__bb;
	uint32_t csum = 0;

	for (i = 0; i < len; i++)
		csum += *(bb + i);

	return -csum;
}

static int bb_verify(void)
{
	int bb_size = __bb_end - __bb;
	int i;
	char *magic = "-chrome-";
	int size;

	if (bb_size <= 0)
		return 0;

	/* test magic */
	for (i = 0; i < 8; i++) {
		if (magic[i] != *(__bb + i))
			return 0;
	}

	/* test size */
	size = *(int *)(__bb + SIZE_OFFSET);
	if (size != bb_size)
		return 0;

	/* test checksum */
	if (checksum(size / 4))
		return 0;

	/* get offset to first blob */
	blob_offset = BOARD_OFFSET;
	while ((*(__bb + blob_offset)) != 0)
		blob_offset++;

	/* move to next 4-byte boundary */
	blob_offset = (blob_offset + 3) & ~4;

	return bb_size;
}

void *bb_lookup(enum blob_struct_type type)
{
	if (bb_okay < 0) {
		bb_okay = bb_verify();

		if (bb_okay)
			CPRINTS("Board Binary verified");
		else
			CPRINTS("Board Binary failed");
	}

	if (bb_okay) {
		uint32_t next_blob;
		uint32_t blob_info;
		uint32_t blob_size;
		uint8_t blob_type;
		uint32_t inc = blob_offset;

		do {
			next_blob = *(uint32_t *)(__bb + inc);
			inc += 4;

			blob_info = *(uint32_t *)(__bb + inc);
			blob_type = type = BLOB_TYPE(blob_info);
			inc += 4;

			blob_size = *(uint32_t *)(__bb + inc);
			inc += 4;

			if (blob_type == type)
				return (void *)(__bb + inc);
			inc += blob_size;

		} while ((next_blob != 0) && (inc < bb_okay));
	}

	return NULL;
}
