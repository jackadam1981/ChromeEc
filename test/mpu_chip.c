/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "mpu.h"
#include "mpu_chip.h"
#include "string.h"
#include "system.h"
#include "test_util.h"

#include <stdbool.h>
#include <stdlib.h>

static int write_verify(uint32_t addr)
{
	uint32_t wr_data;

	wr_data = (uint32_t)(rand());
	*(volatile uint32_t *)addr = wr_data;

	if (*(volatile uint32_t *)addr != wr_data)
		return EC_ERROR_UNKNOWN;

	return EC_SUCCESS;
}

#define VERIFY_NO_WRITE(addr)                                        \
	do {                                                         \
		TEST_ASSERT(write_verify(addr) == EC_ERROR_UNKNOWN); \
	} while (0)

#define VERIFY_WRITE(addr)                                     \
	do {                                                   \
		TEST_ASSERT(write_verify(addr) == EC_SUCCESS); \
	} while (0)

#if defined(CHIP_VARIANT_NPCX9MFP)
struct mpu_rw_regions alias_code_ram = { .num_regions = REGION_DATA_RAM,
					 .addr = { 0x20070000 },
					 .size = { 0x5000 } };

struct mpu_rw_regions alias_data_ram = { .num_regions = REGION_STORAGE,
					 .addr = { 0x100D0000 },
					 .size = { 0x5000 } };

struct mpu_rw_regions invalid_code_reg_addr = { .num_regions = REGION_STORAGE,
						.addr = { 0x10059AB1 },
						.size = { 0x3000 } };

struct mpu_rw_regions invalid_code_reg_size = { .num_regions = REGION_STORAGE,
						.addr = { 0x10058000 },
						.size = { 0x3A80 } };
#else
#error "MPU info not defined for this chip. Please add it."
#endif

test_static int test_mpu_chip_config_lock_region_invalid_addr(void)
{
	/*
	 * Test address that is not aligned.
	 */
	TEST_EQ(mpu_chip_config_lock_region(invalid_code_reg_addr.num_regions,
					    invalid_code_reg_addr.addr[0],
					    invalid_code_reg_addr.size[0]),
		-EC_ERROR_INVAL, "%d");

	return EC_SUCCESS;
}

test_static int test_mpu_chip_config_lock_region_invalid_size(void)
{
	/*
	 * Test size that is not aligned.
	 */
	TEST_EQ(mpu_chip_config_lock_region(invalid_code_reg_size.num_regions,
					    invalid_code_reg_size.addr[0],
					    invalid_code_reg_size.size[0]),
		-EC_ERROR_INVAL, "%d");

	return EC_SUCCESS;
}

test_static int test_mpu_chip_config_lock_region(void)
{
	TEST_EQ(mpu_chip_config_lock_region(REGION_DATA_RAM, CONFIG_RAM_BASE,
					    0x10000),
		EC_SUCCESS, "%d");
	TEST_EQ(mpu_chip_config_lock_region(REGION_STORAGE,
					    CONFIG_PROGRAM_MEMORY_BASE +
						    CONFIG_RO_MEM_OFF,
					    0x10000),
		EC_SUCCESS, "%d");

	return EC_SUCCESS;
}

static int test_ram_write_protect(void)
{
	VERIFY_NO_WRITE(CONFIG_PROGRAM_MEMORY_BASE + CONFIG_RO_MEM_OFF);
	VERIFY_WRITE(CONFIG_RAM_BASE);

	return EC_SUCCESS;
}

test_static int test_mpu_chip_config_lock_region_alias(void)
{
	TEST_EQ(mpu_chip_config_lock_region(alias_data_ram.num_regions,
					    alias_data_ram.addr[0],
					    alias_data_ram.size[0]),
		EC_SUCCESS, "%d");
	TEST_EQ(mpu_chip_config_lock_region(alias_code_ram.num_regions,
					    alias_code_ram.addr[0],
					    alias_code_ram.size[0]),
		EC_SUCCESS, "%d");

	return EC_SUCCESS;
}

static int test_ram_alias_write_protect(void)
{
	VERIFY_NO_WRITE(alias_data_ram.addr[0]);
	VERIFY_WRITE(alias_code_ram.addr[0]);

	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	ccprintf("Running MPU chip test\n");

	RUN_TEST(test_mpu_chip_config_lock_region_invalid_addr);
	RUN_TEST(test_mpu_chip_config_lock_region_invalid_size);
	RUN_TEST(test_mpu_chip_config_lock_region);
	RUN_TEST(test_ram_write_protect);
	RUN_TEST(test_mpu_chip_config_lock_region_alias);
	RUN_TEST(test_ram_alias_write_protect);
	test_print_result();
}
