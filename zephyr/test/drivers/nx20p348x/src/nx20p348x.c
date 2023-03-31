/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery_smart.h"
#include "charger.h"
#include "console.h"
#include "driver/ppc/nx20p348x.h"
#include "driver/ppc/nx20p348x_public.h"
#include "emul/emul_nx20p348x.h"
#include "test/drivers/stubs.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "usb_pd_tcpm.h"

#include <zephyr/shell/shell.h>
#include <zephyr/ztest.h>

#define NX20P383X_NODE DT_NODELABEL(nx20p348x_emul)

#define TEST_PORT USBC_PORT_C0

struct nx20p348x_driver_fixture {
	const struct emul *nx20p348x_emul;
};

static void *nx20p348x_driver_setup(void)
{
	static struct nx20p348x_driver_fixture fix;

	fix.nx20p348x_emul = EMUL_DT_GET(NX20P383X_NODE);

	return &fix;
}

ZTEST_SUITE(nx20p348x_driver, drivers_predicate_post_main,
	    nx20p348x_driver_setup, NULL, NULL, NULL);

struct curr_limit_pair {
	enum tcpc_rp_value rp;
	uint8_t reg;
};

/* Note: Register values are slightly higher to account for overshoot */
static struct curr_limit_pair currents[] = {
	{ .rp = TYPEC_RP_3A0, .reg = NX20P348X_ILIM_3_200 },
	{ .rp = TYPEC_RP_1A5, .reg = NX20P348X_ILIM_1_600 },
	{ .rp = TYPEC_RP_USB, .reg = NX20P348X_ILIM_0_600 },
};

ZTEST_F(nx20p348x_driver, test_source_curr_limits)
{
	for (int i = 0; i < ARRAY_SIZE(currents); i++) {
		uint8_t read;

		ppc_set_vbus_source_current_limit(TEST_PORT, currents[i].rp);
		read = nx20p348x_emul_peek(fixture->nx20p348x_emul,
					   NX20P348X_5V_SRC_OCP_THRESHOLD_REG);
		zassert_equal((read & NX20P348X_ILIM_MASK), currents[i].reg);
	}
}

ZTEST_F(nx20p348x_driver, test_discharge_vbus)
{
	uint8_t reg;

	zassert_ok(ppc_discharge_vbus(TEST_PORT, true));
	reg = nx20p348x_emul_peek(fixture->nx20p348x_emul,
				  NX20P348X_DEVICE_CONTROL_REG);
	zassert_equal((reg & NX20P348X_CTRL_VBUSDIS_EN),
		      NX20P348X_CTRL_VBUSDIS_EN);

	zassert_ok(ppc_discharge_vbus(TEST_PORT, false));
	reg = nx20p348x_emul_peek(fixture->nx20p348x_emul,
				  NX20P348X_DEVICE_CONTROL_REG);
	zassert_not_equal((reg & NX20P348X_CTRL_VBUSDIS_EN),
			  NX20P348X_CTRL_VBUSDIS_EN);
}

ZTEST(nx20p348x_driver, test_sink_enable)
{
	/* Note: PPC requires a TCPC GPIO to enable its sinking */
	zassert_equal(ppc_vbus_sink_enable(TEST_PORT, true), EC_ERROR_TIMEOUT);
}

ZTEST(nx20p348x_driver, test_source_enable)
{
	/* Note: PPC requires a TCPC GPIO to enable its sourcing */
	zassert_equal(ppc_vbus_source_enable(TEST_PORT, true),
		      EC_ERROR_TIMEOUT);
}

ZTEST(nx20p348x_driver, test_ppc_dump)
{
	const struct shell *shell_zephyr = get_ec_shell();

	/* This chip supports PPC dump, so should return success */
	zassert_ok(shell_execute_cmd(shell_zephyr, "ppc_dump 0"));
}
