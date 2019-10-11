/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test system_common.
 */

#include "common.h"
#include "console.h"
#include "host_command.h"
#include "sysjump_impl.h"
#include "system.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

#include <stdio.h>

#define CHECK(fn) TEST_ASSERT((fn) == EC_SUCCESS)

/*
 * panic_data section
 */

/*
 * size3 as of:
 * 59d060ebfe (Dino Li           2019-06-10 16:26:36 +0800  86)
 */

struct panic_data_size3 {
	uint8_t arch;             /* Architecture (PANIC_ARCH_*) */
	uint8_t struct_version;   /* Structure version (currently 2) */
	uint8_t flags;            /* Flags (PANIC_DATA_FLAG_*) */
	uint8_t reserved;         /* Reserved; set 0 */

	/* core specific panic data */
	union {
		struct cortex_panic_data cm;       /* Cortex-Mx registers */
		struct nds32_n8_panic_data nds_n8; /* NDS32 N8 registers */
		struct x86_panic_data x86;         /* Intel x86 */
		struct rv32i_panic_data riscv;     /* RISC-V RV32I */
	};

	/*
	 * These fields go at the END of the struct so we can find it at the
	 * end of memory.
	 */
	uint32_t struct_size;     /* Size of this struct */
	uint32_t magic;           /* PANIC_SAVE_MAGIC if valid */
};

BUILD_ASSERT(sizeof(struct panic_data_size3) == sizeof(struct panic_data));

static int set_panic_size3(void)
{
	static const struct panic_data_size3 p3data = {
		.magic = PANIC_DATA_MAGIC,
		.struct_size = sizeof(struct panic_data_size3),
	};
	uintptr_t p3addr;

	p3addr = CONFIG_RAM_BASE + CONFIG_RAM_SIZE - sizeof(struct panic_data_size3);
	memcpy((void *)p3addr, &p3data, sizeof(p3data));
	TEST_ASSERT(panic_get_data());
	return EC_SUCCESS;
}

static int set_panic_none(void)
{
	PANIC_DATA_PTR->magic = 0;
	TEST_ASSERT(!panic_get_data());
	return EC_SUCCESS;
}

/*
 * jump_data section
 */

static uintptr_t top_of_jd_ram(void)
{
	struct panic_data *pd;
	uintptr_t top =  CONFIG_RAM_BASE + CONFIG_RAM_SIZE;

	pd = panic_get_data();
	if (pd)
		top -= pd->struct_size;
	return top;
}

struct jump_data_v2 {
	/* Fields from version 2 */
	int jump_tag_total;   /* Total size of all jump tags */

	/* Fields from version 1 */
	uint32_t reset_flags; /* Reset flags from the previous boot */
	int version;          /* Version (JUMP_DATA_VERSION) */
	int magic;            /* Magic number (JUMP_DATA_MAGIC).  If this
			       * doesn't match at pre-init time, assume no valid
			       * data from the previous image. */
};

BUILD_ASSERT(sizeof(struct jump_data_v2) == JUMP_DATA_SIZE_V2);

/*
 * jump_data saved by the sysjump caller is always uprev'd to the
 * current version by the callee in system_common_pre_init().
 */

static int set_jump_v2(void)
{
	static const struct jump_data_v2 jdata = {
		.magic = JUMP_DATA_MAGIC,
		.version = 2,
	};
	uintptr_t jd;

	jd = top_of_jd_ram() - sizeof(jdata);
	memcpy((void *)jd, &jdata, sizeof(jdata));
	return EC_SUCCESS;
}

/*
 * if this assert fails, we must have bumped the jump_data version.
 * add tests for the missing version(s) and uprev as appropriate.
 */

BUILD_ASSERT(JUMP_DATA_VERSION == 3);

static int set_jump_v3(void)
{
	static const struct jump_data jdata = {
		.magic = JUMP_DATA_MAGIC,
		.version = JUMP_DATA_VERSION,
	};
	uintptr_t jd;

	jd = top_of_jd_ram() - sizeof(jdata);
	memcpy((void *)jd, &jdata, sizeof(jdata));
	return EC_SUCCESS;
}

static void setup(void)
{
	system_common_reset_state();
	system_pre_init();
}

static int fake_sysjump(void)
{
	TEST_ASSERT(!system_jumped_to_this_image());
	TEST_ASSERT(!(system_get_reset_flags() & EC_RESET_FLAG_SYSJUMP));
	system_clear_reset_flags(EC_RESET_FLAG_POWER_ON);
	system_common_pre_init();
	return EC_SUCCESS;
}

/*
 * verify that the jump_data was recognized
 */

static int verify_sysjump(void)
{
	TEST_ASSERT(system_jumped_to_this_image());
	TEST_ASSERT(system_get_reset_flags() & EC_RESET_FLAG_SYSJUMP);
	return EC_SUCCESS;
}

static int run_sysjump(void)
{
	CHECK(fake_sysjump());
	CHECK(verify_sysjump());
	return EC_SUCCESS;
}

/*
 * jump_data V2 (legacy) scenarios
 */

static int test_Pn_J2(void)
{
	setup();
	CHECK(set_panic_none());
	CHECK(set_jump_v2());
	CHECK(run_sysjump());
	return EC_SUCCESS;
}

static int test_P3_J2(void)
{
	setup();
	CHECK(set_panic_size3());
	CHECK(set_jump_v2());
	CHECK(run_sysjump());
	return EC_SUCCESS;
}

/*
 * jump_data V3 scenarios
 */

static int test_Pn_J3(void)
{
	setup();
	CHECK(set_panic_none());
	CHECK(set_jump_v3());
	CHECK(run_sysjump());
	return EC_SUCCESS;
}

static int test_P3_J3(void)
{
	setup();
	CHECK(set_panic_size3());
	CHECK(set_jump_v3());
	CHECK(run_sysjump());
	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_Pn_J3);
	RUN_TEST(test_P3_J3);

	RUN_TEST(test_Pn_J2);
	RUN_TEST(test_P3_J2);

	test_print_result();
}
