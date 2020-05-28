/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chip/stm32/flash-f.h"
#include "flash.h"
#include "panic.h"
#include "test_util.h"

struct flash_info {
	int num_flash_banks;
	int write_protect_bank_offset;
	int write_protect_bank_count;
};

#if defined(CHIP_VARIANT_STM32F412)
struct flash_info flash_info = {
	.num_flash_banks = 12,
	.write_protect_bank_offset = 0,
	.write_protect_bank_count = 5,
};
#elif defined(CHIP_VARIANT_STM32H7X3)
struct flash_info flash_info = {
	.num_flash_banks = 16,
	.write_protect_bank_offset = 0,
	.write_protect_bank_count = 6,
};
#else
#error "Flash info not defined for this chip. Please add it."
#endif

test_static int test_flash_physical_write(void)
{
	return EC_ERROR_UNIMPLEMENTED;
}

test_static int test_flash_physical_erase(void)
{
	return EC_ERROR_UNIMPLEMENTED;
}

test_static int test_flash_physical_get_protect(void)
{
	return EC_ERROR_UNIMPLEMENTED;
}

test_static int test_flash_physical_get_protect_flags(void)
{
	return EC_ERROR_UNIMPLEMENTED;
}

test_static int test_flash_physical_get_valid_flags(void)
{
	return EC_ERROR_UNIMPLEMENTED;
}

test_static int test_flash_physical_get_writable_flags(void)
{
	return EC_ERROR_UNIMPLEMENTED;
}

test_static int test_flash_physical_protect_at_boot_all(void)
{
	int rv;
	int i;

	/* Nothing should be protected */
	for (i = 0; i < flash_info.num_flash_banks; i++) {
		TEST_EQ(flash_physical_get_protect(i), 0, "%d");
	}

	/* Protecting everything. */
	rv = flash_physical_protect_at_boot(EC_FLASH_PROTECT_ALL_AT_BOOT);
	TEST_EQ(rv, EC_SUCCESS, "%d");

	for (i = 0; i < flash_info.num_flash_banks; i++) {
		rv = flash_physical_get_protect(i);
		TEST_EQ(rv, 1, "%d");
	}

	/* TODO: Check that RDP 1 is enabled */

	return EC_SUCCESS;
}

test_static int test_flash_physical_protect_at_boot_rollback(void)
{
	return EC_ERROR_UNIMPLEMENTED;
}

test_static int test_flash_physical_protect_at_boot_ro(void)
{
	int rv;
	int i;

	/* Nothing should be protected */
	for (i = 0; i < flash_info.num_flash_banks; i++) {
		TEST_EQ(flash_physical_get_protect(i), 0, "%d");
	}

	/* Protecting only RO */
	rv = flash_physical_protect_at_boot(EC_FLASH_PROTECT_RO_AT_BOOT);
	TEST_EQ(rv, EC_SUCCESS, "%d");

	for (i = 0; i < flash_info.num_flash_banks; i++) {
		rv = flash_physical_get_protect(i);
		if (i >= flash_info.write_protect_bank_offset &&
			i < flash_info.write_protect_bank_offset +
				flash_info.write_protect_bank_count) {
			TEST_EQ(rv, 1, "%d");
		} else {
			TEST_EQ(rv, 0, "%d");
		}
	}

	/* TODO: Check that RDP 1 is enabled */

	return EC_SUCCESS;
}

test_static int test_flash_physical_protect_now(void)
{
	int rv;
	int i;

	/* Nothing should be protected */
	for (i = 0; i < flash_info.num_flash_banks; i++) {
		TEST_EQ(flash_physical_get_protect(i), 0, "%d");
	}

	/* Protecting only RO */
	rv = flash_physical_protect_now(0);
	TEST_EQ(rv, EC_SUCCESS, "%d");

	/* In current implementation, option bytes are disabled.
	 * We should check for that here. Do we expect anything else to be
	 * done?
	 */

	return EC_SUCCESS;
}


test_static int test_lock_option_bytes(void)
{
	TEST_EQ(flash_option_bytes_locked(), true, "%d");

	unlock_flash_option_bytes();

	TEST_EQ(flash_option_bytes_locked(), false, "%d");

	lock_flash_option_bytes();

	TEST_EQ(flash_option_bytes_locked(), true, "%d");

	unlock_flash_option_bytes();

	TEST_EQ(flash_option_bytes_locked(), false, "%d");

	return EC_SUCCESS;
}

test_static int test_disable_option_bytes(void)
{
	TEST_EQ(flash_option_bytes_locked(), false, "%d");

	disable_flash_option_bytes();

	TEST_EQ(flash_option_bytes_locked(), true, "%d");

	/* Since we've disabled the option bytes we'll get a bus fault. */
	ignore_bus_fault(1);

	unlock_flash_option_bytes();

	ignore_bus_fault(0);

	/* Option bytes should still be locked. */
	TEST_EQ(flash_option_bytes_locked(), true, "%d");

	return EC_SUCCESS;
}

test_static int test_lock_flash_control_register(void)
{
	TEST_EQ(flash_control_register_locked(), true, "%d");

	unlock_flash_control_register();

	TEST_EQ(flash_control_register_locked(), false, "%d");

	lock_flash_control_register();

	TEST_EQ(flash_control_register_locked(), true, "%d");

	unlock_flash_control_register();

	TEST_EQ(flash_control_register_locked(), false, "%d");

	return EC_SUCCESS;
}

test_static int test_disable_flash_control_register(void)
{
	TEST_EQ(flash_control_register_locked(), false, "%d");

	disable_flash_control_register();

	TEST_EQ(flash_control_register_locked(), true, "%d");

	/* Since we've disabled the option bytes we'll get a bus fault. */
	ignore_bus_fault(1);

	unlock_flash_control_register();

	ignore_bus_fault(0);

	/* Control register should still be locked. */
	TEST_EQ(flash_control_register_locked(), true, "%d");

	return EC_SUCCESS;
}

test_static int test_flash_config(void)
{
	TEST_EQ(PHYSICAL_BANKS, flash_info.num_flash_banks, "%d");
	TEST_EQ(WP_BANK_OFFSET, flash_info.write_protect_bank_offset, "%d");
	TEST_EQ(WP_BANK_COUNT, flash_info.write_protect_bank_count, "%d");
	return EC_SUCCESS;
}

void run_test(int argc, char **argv)
{
	ccprintf("Running flash physical test\n");
	RUN_TEST(test_flash_config);

	// RUN_TEST(test_flash_physical_protect_at_boot_ro);
	// RUN_TEST(test_flash_physical_protect_at_boot_all);
	RUN_TEST(test_flash_physical_protect_now);

	/*
	 * TODO(b/157692395): These should be implemented for the STM32H743 as
	 * well.
	 */
// #if defined(CHIP_VARIANT_STM32F412)
#if 0
	RUN_TEST(test_lock_option_bytes);
	RUN_TEST(test_disable_option_bytes);
	RUN_TEST(test_lock_flash_control_register);
	RUN_TEST(test_disable_flash_control_register);
#endif
	test_print_result();
}
