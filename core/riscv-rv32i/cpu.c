/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Set up the RISC-V core
 */

#include "common.h"
#include "cpu.h"

void cpu_init(void)
{
	/* bit3: Global interrupt enable (M-mode) */
	asm volatile ("csrsi mstatus, 0x8");
}

/* TODO: Move to separate file. */
struct entry {
	void *func;
	int count;
};

struct entry entries[1024];

void __attribute__((no_instrument_function)) _mcount(void)
{
	void __unused *func = __builtin_return_address(0);

	// record the address in entries
}
