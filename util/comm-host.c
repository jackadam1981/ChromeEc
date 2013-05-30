/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stdio.h>

#include "comm-host.h"

int (*ec_command)(int command, int version,
		  void *indata, int insize,
		  void *outdata, int outsize);
uint8_t (*read_mapped_mem8)(uint8_t offset);
uint16_t (*read_mapped_mem16)(uint8_t offset);
uint32_t (*read_mapped_mem32)(uint8_t offset);
int (*read_mapped_string)(uint8_t offset, char *buf);

int comm_init_dev(void) __attribute__((weak));
int comm_init_lpc(void) __attribute__((weak));
int comm_init_i2c(void) __attribute__((weak));

int comm_init(void)
{
	/* Prefer new /dev method */
	if (comm_init_dev && !comm_init_dev())
		return 0;

	/* Fallback to direct LPC on x86 */
	if (comm_init_lpc && !comm_init_lpc())
		return 0;

	/* Fallback to direct i2c on ARM */
	if (comm_init_i2c && !comm_init_i2c())
		return 0;

	/* Give up */
	fprintf(stderr, "Unable to establish host communication\n");
	return 1;
}
