/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/ztest.h>

#include "i2c/i2c.h"
#include "charge_manager.h"
#include "usbc_ppc.h"

bool source_enabled[2];
bool sink_enabled[2];

int fake_vbus_sink_enable(int port, int enable)
{
	zassert_true(0 <= port && port < 2, NULL);

	if (enable) {
		zassert_false(source_enabled[port], NULL);
	}
	sink_enabled[port] = enable;
	zassert_false(sink_enabled[0] && sink_enabled[1], NULL);

	return 0;
}

int fake_vbus_source_enable(int port, int enable)
{
	zassert_true(0 <= port && port < 2, NULL);

	if (enable) {
		zassert_false(sink_enabled[port], NULL);
	}
	source_enabled[port] = enable;

	return 0;
}

int fake_is_sourcing_vbus(int port)
{
	zassert_true(0 <= port && port < 2, NULL);

	return source_enabled[port];
}

struct ppc_drv fake_ppc_drv = {
	.is_sourcing_vbus = fake_is_sourcing_vbus,
	.vbus_sink_enable = fake_vbus_sink_enable,
	.vbus_source_enable = fake_vbus_source_enable,
};

struct ppc_config_t ppc_chips[] = {
	{ .drv = &fake_ppc_drv, },
	{ .drv = &fake_ppc_drv, },
};
unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

ZTEST(usbc_config, test_set_active_charge_port)
{
	zassert_ok(board_set_active_charge_port(CHARGE_PORT_NONE), "");
	zassert_ok(board_set_active_charge_port(0), "");
	zassert_ok(board_set_active_charge_port(1), "");
	zassert_ok(board_set_active_charge_port(0), "");
	zassert_ok(board_set_active_charge_port(CHARGE_PORT_NONE), "");

	ppc_vbus_source_enable(0, true);
	zassert_not_equal(board_set_active_charge_port(0), 0, "");
}

ZTEST_SUITE(usbc_config, NULL, NULL, NULL, NULL,
	    NULL);
