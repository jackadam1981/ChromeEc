/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_cbi.h"
#include "hooks.h"
#include "usb_config.h"
#include "usb_mux.h"
#include "usbc/usb_muxes.h"

#include <zephyr/devicetree.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

FAKE_VALUE_FUNC(int, cros_cbi_get_fw_config, enum cbi_fw_config_field_id,
		uint32_t *);

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

int mock_cros_cbi_get_fw_config_error(enum cbi_fw_config_field_id field_id,
				      uint32_t *value)
{
	return -1;
}

static void usb_config_before(void *fixture)
{
	ARG_UNUSED(fixture);

	RESET_FAKE(cros_cbi_get_fw_config);
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
}

ZTEST_USER(usb_config, test_setup_usb_db_error_reading_cbi)
{
	cros_cbi_get_fw_config_fake.custom_fake =
		mock_cros_cbi_get_fw_config_error;

	hook_notify(HOOK_INIT);

	zassert_equal(2, cros_cbi_get_fw_config_fake.call_count);
	zassert_equal(-1, usb_db_type);
}

ZTEST_SUITE(usb_config, NULL, NULL, usb_config_before, NULL, NULL);
