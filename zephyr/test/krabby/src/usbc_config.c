/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/ztest.h>

#include "adc.h"
#include "driver/ppc/syv682x_public.h"
#include "driver/ppc/syv682x.h"
#include "emul/emul_syv682x.h"
#include "i2c/i2c.h"
#include "charge_manager.h"
#include "usbc_ppc.h"

struct ppc_config_t ppc_chips[] = {
	{
		.i2c_port = I2C_PORT_NODELABEL(i2c_ctrl0),
		.i2c_addr_flags = DT_REG_ADDR(DT_NODELABEL(port0_ppc_emul)),
		.drv = &syv682x_drv,
	},
	{
		.i2c_port = I2C_PORT_NODELABEL(i2c_ctrl0),
		.i2c_addr_flags = DT_REG_ADDR(DT_NODELABEL(port1_ppc_emul)),
		.drv = &syv682x_drv,
	},
};
unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

bool ppc_sink_enabled(int port)
{
	const struct emul *emul =
		(port == 0) ? EMUL_DT_GET(DT_NODELABEL(port0_ppc_emul)) :
			      EMUL_DT_GET(DT_NODELABEL(port1_ppc_emul));
	uint8_t val = 0;

	syv682x_emul_get_reg(emul, SYV682X_CONTROL_1_REG, &val);

	return !(val & (SYV682X_CONTROL_1_PWR_ENB | SYV682X_CONTROL_1_HV_DR));
}

ZTEST_SUITE(usbc_config, NULL, NULL, NULL, NULL, NULL);

ZTEST(usbc_config, test_set_active_charge_port)
{
	/* reset ppc state */
	zassert_ok(board_set_active_charge_port(CHARGE_PORT_NONE), NULL);
	zassert_false(ppc_is_sourcing_vbus(0), NULL);
	zassert_false(ppc_is_sourcing_vbus(1), NULL);
	zassert_false(ppc_sink_enabled(0), NULL);
	zassert_false(ppc_sink_enabled(1), NULL);

	/* sinking port 0 */
	zassert_ok(board_set_active_charge_port(0), NULL);
	zassert_false(ppc_is_sourcing_vbus(0), NULL);
	zassert_false(ppc_is_sourcing_vbus(1), NULL);
	zassert_true(ppc_sink_enabled(0), NULL);
	zassert_false(ppc_sink_enabled(1), NULL);

	/* sinking port 1 */
	zassert_ok(board_set_active_charge_port(1), NULL);
	zassert_false(ppc_is_sourcing_vbus(0), NULL);
	zassert_false(ppc_is_sourcing_vbus(1), NULL);
	zassert_false(ppc_sink_enabled(0), NULL);
	zassert_true(ppc_sink_enabled(1), NULL);

	/* back to port 0 */
	zassert_ok(board_set_active_charge_port(0), NULL);
	zassert_false(ppc_is_sourcing_vbus(0), NULL);
	zassert_false(ppc_is_sourcing_vbus(1), NULL);
	zassert_true(ppc_sink_enabled(0), NULL);
	zassert_false(ppc_sink_enabled(1), NULL);

	/* reset */
	zassert_ok(board_set_active_charge_port(CHARGE_PORT_NONE), NULL);
	zassert_false(ppc_is_sourcing_vbus(0), NULL);
	zassert_false(ppc_is_sourcing_vbus(1), NULL);
	zassert_false(ppc_sink_enabled(0), NULL);
	zassert_false(ppc_sink_enabled(1), NULL);

	/* sourcing port 0, expect port 0 not sinkable */
	ppc_vbus_source_enable(0, true);
	zassert_not_equal(board_set_active_charge_port(0), 0, NULL);
	zassert_true(ppc_is_sourcing_vbus(0), NULL);
	zassert_false(ppc_is_sourcing_vbus(1), NULL);
	zassert_false(ppc_sink_enabled(0), NULL);
	zassert_false(ppc_sink_enabled(1), NULL);

	/* invalid port should return error and doesn't change any state */
	zassert_not_equal(board_set_active_charge_port(2), 0, NULL);
	zassert_true(ppc_is_sourcing_vbus(0), NULL);
	zassert_false(ppc_is_sourcing_vbus(1), NULL);
	zassert_false(ppc_sink_enabled(0), NULL);
	zassert_false(ppc_sink_enabled(1), NULL);
}

ZTEST(usbc_config, test_adc_channel)
{
	zassert_equal(board_get_vbus_adc(0), ADC_VBUS_C0, NULL);
	zassert_equal(board_get_vbus_adc(1), ADC_VBUS_C1, NULL);
	zassert_equal(board_get_vbus_adc(99), ADC_VBUS_C0, NULL);
}
