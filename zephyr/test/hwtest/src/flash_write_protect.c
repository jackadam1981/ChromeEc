/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "flash.h"
#include "system.h"
#include "write_protect.h"

#include <zephyr/ztest.h>

#ifdef CONFIG_EEPROM_CBI_WP
#warning "EEPROM CBI WP tests not implemented."
#endif

enum {
	/* Random number to signal the next stage of the test */
	TEST_STATE_WP_DISABLE = 0x8C0A,
};

static void *flash_write_protect_setup(void)
{
	return NULL;
}

void flash_write_protect_teardown(void *fixture)
{
	system_set_scratchpad(0);
}

ZTEST_SUITE(flash_write_protect, NULL, flash_write_protect_setup, NULL, NULL, flash_write_protect_teardown);

static int check_image_and_hardware_write_protect(void)
{
	bool wp;

	if (system_get_image_copy() != EC_IMAGE_RO) {
		ccprintf("This test is only works when running RO\n");
		return -ENOTSUP;
	}

	wp = write_protect_is_asserted();

	if (!wp) {
		ccprintf("Hardware write protect (GPIO_WP) must be enabled\n");
		return -ENOTSUP;
	}

	return 0;
}

static void test_wp_enable(void)
{
	int rv;

	zassert_equal(check_image_and_hardware_write_protect(), 0);

	/* Equivalent of ectool --name=cros_fp flashprotect enable */
	rv = crec_flash_set_protect(EC_FLASH_PROTECT_RO_AT_BOOT,
				    EC_FLASH_PROTECT_RO_AT_BOOT);

	zassert_equal(rv, EC_SUCCESS);
}

static void test_wp_disable(void)
{
	int rv;

	zassert_equal(check_image_and_hardware_write_protect(), 0);

	/* Equivalent of ectool --name=cros_fp flashprotect disable */
	rv = crec_flash_set_protect(EC_FLASH_PROTECT_RO_AT_BOOT, 0);

	zassert_not_equal(rv, EC_SUCCESS);
}

ZTEST(flash_write_protect, test_flash_write_protect)
{
	uint32_t state = 0;

	system_get_scratchpad(&state);

	switch (state) {
	case TEST_STATE_WP_DISABLE:
		test_wp_disable();
		break;
	default:
		test_wp_enable();
		cflush();
		system_set_scratchpad(TEST_STATE_WP_DISABLE);
		system_reset(SYSTEM_RESET_HARD);
	}
}

static void test_thread(void *arg1, void *arg2, void *arg3)
{
	uint32_t state = 0;

	system_get_scratchpad(&state);

	/* The first state is run via console */
	switch (state) {
	case TEST_STATE_WP_DISABLE:
		ztest_run_test_suites(NULL, false, 1, 1);
		break;
	default:
		break;
	}
}
K_THREAD_DEFINE(test_thread_tid, 1024, test_thread, NULL, NULL, NULL, 1, 0, 0);
