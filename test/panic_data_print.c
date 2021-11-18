/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "panic.h"
#include "test_util.h"

#if !(defined(CORE_CORTEX_M) || defined(CORE_CORTEX_M0))
#error "Architecture not supported"
#endif


test_static int test_panic_data_print(void)
{
	const struct panic_data pdata = {
		.arch = PANIC_ARCH_CORTEX_M,
		.struct_version = 2,
		.flags = PANIC_DATA_FLAG_FRAME_VALID,
		.reserved = 0,
		.cm = {
			.regs = {
				[CORTEX_PANIC_REGISTER_PSP] = 42,
				[CORTEX_PANIC_REGISTER_IPSR] = 43,
				[CORTEX_PANIC_REGISTER_MSP] = 44,
				[CORTEX_PANIC_REGISTER_R4] = 4,
				[CORTEX_PANIC_REGISTER_R5] = 5,
				[CORTEX_PANIC_REGISTER_R6] = 6,
				[CORTEX_PANIC_REGISTER_R7] = 7,
				[CORTEX_PANIC_REGISTER_R8] = 8,
				[CORTEX_PANIC_REGISTER_R9] = 9,
				[CORTEX_PANIC_REGISTER_R10] = 10,
				[CORTEX_PANIC_REGISTER_R11] = 11,
				[CORTEX_PANIC_REGISTER_LR] = 14,
			},
			.frame = {
				[CORTEX_PANIC_FRAME_REGISTER_R0] = 0,
				[CORTEX_PANIC_FRAME_REGISTER_R1] = 1,
				[CORTEX_PANIC_FRAME_REGISTER_R2] = 2,
				[CORTEX_PANIC_FRAME_REGISTER_R3] = 3,
				[CORTEX_PANIC_FRAME_REGISTER_R12] = 12,
				[CORTEX_PANIC_FRAME_REGISTER_LR] = 14,
				[CORTEX_PANIC_FRAME_REGISTER_PC] = 15,
				[CORTEX_PANIC_FRAME_REGISTER_PSR] = 16,
			},
#if 0
			.cfsr = 17,
			.bfar = 18,
			.mfar = 19,
			.shcsr = 20,
			.hfsr = 21,
			.dfsr = 22
#endif
		},
		.struct_size = sizeof(pdata),
		.magic = PANIC_DATA_MAGIC,
	};
	panic_data_print(&pdata);
	return EC_SUCCESS;
}

void run_test(int argc, char **argv)
{
	RUN_TEST(test_panic_data_print);
	test_print_result();
}
