/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "rex_fakes.h"
#include "test_state.h"
#include "usbc_config.h"

#include <zephyr/ztest.h>

FAKE_VALUE_FUNC(int, ppc_vbus_sink_enable, int, int);

static void usbc_config_before(void *fixture)
{
	ARG_UNUSED(fixture);
}

ZTEST_USER(usbc_config, test_active_charge_port)
{
	ppc_vbus_sink_enable_fake.return_val = EC_SUCCESS;

	zassert_ok(board_set_active_charge_port(CHARGE_PORT_NONE));
	zassert_ok(board_set_active_charge_port(USBC_PORT_C0));
	zassert_ok(board_set_active_charge_port(USBC_PORT_C1));
	zassert_not_ok(board_set_active_charge_port(99));
}

ZTEST_SUITE(usbc_config, rex_predicate_post_main, NULL, usbc_config_before,
	    NULL, NULL);
