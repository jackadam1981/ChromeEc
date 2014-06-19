/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comm-host.h"
#include "misc_util.h"

int ec_sb_firmware_write(const uint8_t *buf, int size)
{
	int rv, i;
	struct ec_params_sb_wr_word *sb_wr =
		(struct ec_params_sb_wr_word *)ec_outbuf;
	int bsize, step = 2;

	/* Write data in chunks */
	printf("Write size %d...\n", step);

	for (i = 0; i < size; i += step) {
		bsize = MIN(size - i, step);
		memcpy(&sb_wr->value, buf + i, bsize);
		rv = ec_command(EC_CMD_SB_WRITE_WORD, 0,
			sb_wr, sizeof(*sb_wr), NULL, 0);
		if (rv < 0) {
			fprintf(stderr,
			"Smart Battery Firmware Update error off@%d\n", i);
			return rv;
		}
	}

	return 0;
}
