/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_cbi.h"
#include "hooks.h"
#include "usb_config.h"
#include "usb_mux.h"
#include "usbc/usb_muxes.h"
#include "usbc_config.h"

#include <zephyr/devicetree.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

FAKE_VALUE_FUNC(int, cros_cbi_get_fw_config, enum cbi_fw_config_field_id,
		uint32_t *);
FAKE_VOID_FUNC(reset_nct38xx_port, int);
FAKE_VOID_FUNC(nx20p348x_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(syv682x_interrupt, enum gpio_signal);

int mock_cros_cbi_get_fw_config_db_usb3(enum cbi_fw_config_field_id field_id,
					uint32_t *value)
{
	*value = FW_USB_DB_USB3;
	return 0;
}

int mock_cros_cbi_get_fw_config_mb_usb3(enum cbi_fw_config_field_id field_id,
					uint32_t *value)
{
	*value = FW_USB_MB_USB3;
	return 0;
}

int mock_cros_cbi_get_fw_config_mb_usb4(enum cbi_fw_config_field_id field_id,
					uint32_t *value)
{
	*value = FW_USB_MB_USB4_HB;
	return 0;
}

int mock_cros_cbi_get_fw_config_error(enum cbi_fw_config_field_id field_id,
				      uint32_t *value)
{
	return -1;
}

static void usb_config_before(void *fixture)
{
	ARG_UNUSED(fixture);

	RESET_FAKE(cros_cbi_get_fw_config);
	RESET_FAKE(reset_nct38xx_port);
	RESET_FAKE(nx20p348x_interrupt);
	RESET_FAKE(syv682x_interrupt);
}

ZTEST_USER(usb_config, test_setup_db_usb3)
{
	cros_cbi_get_fw_config_fake.custom_fake =
		mock_cros_cbi_get_fw_config_db_usb3;

	hook_notify(HOOK_INIT);

	zassert_equal(2, cros_cbi_get_fw_config_fake.call_count);
	zassert_equal(1, usb_db_type);
}

ZTEST_USER(usb_config, test_setup_mb_usb3)
{
	cros_cbi_get_fw_config_fake.custom_fake =
		mock_cros_cbi_get_fw_config_mb_usb3;

	hook_notify(HOOK_INIT);

	zassert_equal(2, cros_cbi_get_fw_config_fake.call_count);
	zassert_equal(1, usb_mb_type);
	zassert_false(board_is_tbt_usb4_port(USBC_PORT_C0), NULL);
	zassert_false(board_is_tbt_usb4_port(USBC_PORT_C1), NULL);
	zassert_equal(2, board_get_usb_pd_port_count());
}

ZTEST_USER(usb_config, test_setup_mb_usb4)
{
	cros_cbi_get_fw_config_fake.custom_fake =
		mock_cros_cbi_get_fw_config_mb_usb4;

	hook_notify(HOOK_INIT);

	zassert_equal(2, cros_cbi_get_fw_config_fake.call_count);
	zassert_equal(2, usb_mb_type);
	zassert_true(board_is_tbt_usb4_port(USBC_PORT_C0), NULL);
	zassert_true(board_is_tbt_usb4_port(USBC_PORT_C1), NULL);
	zassert_equal(2, board_get_usb_pd_port_count());
}

ZTEST_USER(usb_config, test_setup_usb_db_error_reading_cbi)
{
	cros_cbi_get_fw_config_fake.custom_fake =
		mock_cros_cbi_get_fw_config_error;

	hook_notify(HOOK_INIT);

	zassert_equal(2, cros_cbi_get_fw_config_fake.call_count);
	zassert_equal(0, usb_db_type);
	zassert_equal(0, usb_mb_type);
	zassert_equal(1, board_get_usb_pd_port_count());
}

ZTEST_USER(usb_config, test_ppc_interrupt)
{
	/* Test TBT SKU ppc interrupt */
	screebo_ppc_interrupt(GPIO_USB_C0_PPC_INT_ODL);
	zassert_equal(1, syv682x_interrupt_fake.call_count);
	screebo_ppc_interrupt(GPIO_USB_C1_PPC_INT_ODL);
	zassert_equal(2, syv682x_interrupt_fake.call_count);

	/* Test USB3.2 SKU ppc interrupt */
	cros_cbi_get_fw_config_fake.custom_fake =
		mock_cros_cbi_get_fw_config_mb_usb3;
	hook_notify(HOOK_INIT);
	screebo_ppc_interrupt(GPIO_USB_C1_PPC_INT_ODL);
	zassert_equal(1, nx20p348x_interrupt_fake.call_count);

	/* Test switch default situation */
	RESET_FAKE(nx20p348x_interrupt);
	RESET_FAKE(syv682x_interrupt);
	screebo_ppc_interrupt(GPIO_POWER_BUTTON_L);
	zassert_equal(0, nx20p348x_interrupt_fake.call_count);
	zassert_equal(0, syv682x_interrupt_fake.call_count);
}

ZTEST_SUITE(usb_config, NULL, NULL, usb_config_before, NULL, NULL);
