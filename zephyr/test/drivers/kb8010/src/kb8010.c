/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "driver/retimer/kb8010.h"
#include "driver/retimer/kb8010_public.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_kb8010.h"
#include "i2c.h"
#include "test/drivers/stubs.h"
#include "test/drivers/test_mocks.h"
#include "test/drivers/test_state.h"
#include "timer.h"
#include "usb_mux.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define KB8010_NODE DT_NODELABEL(usb_c1_kb8010_emul)
#define EMUL EMUL_DT_GET(KB8010_NODE)
#define COMMON_DATA emul_kb8010_get_i2c_common_data(EMUL)

ZTEST(kb8010, test_kb8010_init)
{
	kb8010_emul_set_reg(EMUL, KB8010_REG_RESET, 0);

	zassert_equal(
		EC_SUCCESS,
		kb8010_usb_retimer_driver.init(usb_muxes[USBC_PORT_C1].mux),
		NULL);
	zassert_equal(KB8010_RESET_MASK & ~KB8010_RESET_MM,
		      kb8010_emul_get_reg(EMUL, KB8010_REG_RESET));
}

ZTEST(kb8010, test_kb8010_set)
{
	mux_state_t mux_state = 1;
	bool ack_required;

	kb8010_emul_set_reg(EMUL,
			    KB8010_REG_SBBR_COMRX_AZC_CTRL_CTLE_OC_BW_STG3, 0);

	zassert_equal(EC_SUCCESS,
		      kb8010_usb_retimer_driver.set(usb_muxes[USBC_PORT_C1].mux,
						    mux_state, &ack_required));
	zassert_equal(ack_required, false);
	zassert_equal(0x0A,
		      kb8010_emul_get_reg(
			      EMUL,
			      KB8010_REG_SBBR_COMRX_AZC_CTRL_CTLE_OC_BW_STG3));
}

static inline void reset_kb8010_state(void)
{
	test_set_chipset_to_s0();
	i2c_common_emul_set_write_func(COMMON_DATA, NULL, NULL);
	i2c_common_emul_set_read_func(COMMON_DATA, NULL, NULL);
	i2c_common_emul_set_write_fail_reg(COMMON_DATA,
					   I2C_COMMON_EMUL_NO_FAIL_REG);
	i2c_common_emul_set_read_fail_reg(COMMON_DATA,
					  I2C_COMMON_EMUL_NO_FAIL_REG);

	kb8010_emul_reset(EMUL);
}

static void kb8010_before(void *state)
{
	ARG_UNUSED(state);
	reset_kb8010_state();
}

static void kb8010_after(void *state)
{
	ARG_UNUSED(state);
	reset_kb8010_state();
}

ZTEST_SUITE(kb8010, drivers_predicate_post_main, NULL, kb8010_before,
	    kb8010_after, NULL);
