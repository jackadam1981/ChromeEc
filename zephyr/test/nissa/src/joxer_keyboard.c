/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "cros_cbi.h"
#include "joxer.h"
#include "keyboard_protocol.h"

#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

void kb_layout_init(void);

FAKE_VALUE_FUNC(int, cros_cbi_get_fw_config, enum cbi_fw_config_field_id,
		uint32_t *);
FAKE_VOID_FUNC(set_scancode_set2, uint8_t, uint8_t, uint16_t);
FAKE_VOID_FUNC(get_scancode_set2, uint8_t, uint8_t);

static bool keyboard_layout;

static int cros_cbi_get_fw_config_mock(enum cbi_fw_config_field_id field_id,
				       uint32_t *value)
{
	if (field_id != FW_KB_LAYOUT)
		return -EINVAL;

	*value = keyboard_layout ? FW_KB_LAYOUT_US2 : FW_KB_LAYOUT_DEFAULT;
	return 0;
}

static void joxer_keyboard_before(void *fixture)
{
	ARG_UNUSED(fixture);
	RESET_FAKE(cros_cbi_get_fw_config);
	RESET_FAKE(set_scancode_set2);
	RESET_FAKE(get_scancode_set2);
}

ZTEST_SUITE(joxer_keyboard, NULL, NULL, joxer_keyboard_before, NULL, NULL);

ZTEST(joxer_keyboard, test_keyboard_config)
{
	zassert_equal_ptr(board_vivaldi_keybd_config(), &joxer_kb_legacy);
}

ZTEST(joxer_keyboard, test_kb_layout_init)
{
	cros_cbi_get_fw_config_fake.custom_fake = cros_cbi_get_fw_config_mock;

	keyboard_layout = false;
	kb_layout_init();
	zassert_equal(set_scancode_set2_fake.call_count, 0);
	zassert_equal(get_scancode_set2_fake.call_count, 0);

	keyboard_layout = true;
	kb_layout_init();
	zassert_equal(set_scancode_set2_fake.call_count, 1);
	zassert_equal(get_scancode_set2_fake.call_count, 1);
}

ZTEST(joxer_keyboard, test_kb_layout_init_cbi_error)
{
	cros_cbi_get_fw_config_fake.return_val = EINVAL;
	kb_layout_init();
	zassert_equal(set_scancode_set2_fake.call_count, 0);
	zassert_equal(get_scancode_set2_fake.call_count, 0);
}
