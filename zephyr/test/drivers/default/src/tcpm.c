/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ec_tasks.h"
#include "emul/emul_common_i2c.h"
#include "emul/tcpc/emul_tcpci.h"
#include "hooks.h"
#include "i2c.h"
#include "tcpm/tcpci.h"
#include "tcpm/tcpm.h"
#include "test/drivers/stubs.h"
#include "test/drivers/tcpci_test_common.h"
#include "test/drivers/test_state.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define TCPCI_EMUL_NODE DT_NODELABEL(tcpci_emul)

#define TCPM_TEST_PORT USBC_PORT_C0

FAKE_VALUE_FUNC(int, set_vconn, int, int);

/* Convenience pointer to a single tcpc config used for testing */
//static struct tcpc_config_t *tcpm_test_config;

struct tcpm_fixture {
	/* The original driver pointer that gets restored after the tests */
	const struct tcpm_drv *saved_driver_ptr;
	/* Mock driver that gets substituted */
	struct tcpm_drv mock_driver;
};

ZTEST_F(tcpm, test_tcpm_drv_set_vconn_failure)
{
	int res;
	/*
	const struct emul *emul = EMUL_DT_GET(TCPCI_EMUL_NODE);
	struct i2c_common_emul_data *common_data =
		emul_tcpci_generic_get_i2c_common_data(emul);
	*/

	tcpc_config[TCPM_TEST_PORT].flags = TCPC_FLAGS_CONTROL_VCONN;

	fixture->mock_driver.set_vconn = set_vconn;
	set_vconn_fake.return_val = -1;

	res = tcpm_set_vconn(TCPM_TEST_PORT, true);

	zassert_true(set_vconn_fake.call_count > 0);
	zassert_equal(-1, res);
}

static void *tcpm_setup(void)
{
	static struct tcpm_fixture fixture;

	return &fixture;
}

static void tcpm_before(void *state)
{
	struct tcpm_fixture *fixture = state;

	//tcpm_test_config = &tcpc_config[TCPM_TEST_PORT];

	fixture->mock_driver = (struct tcpm_drv) { 0 };
	fixture->saved_driver_ptr = tcpc_config[TCPM_TEST_PORT].drv;
	tcpc_config[TCPM_TEST_PORT].drv = &fixture->mock_driver;
}

static void tcpm_after(void *state)
{
	struct tcpm_fixture *fixture = state;

	tcpc_config[TCPM_TEST_PORT].drv = fixture->saved_driver_ptr;

	RESET_FAKE(set_vconn);
}

ZTEST_SUITE(tcpm, drivers_predicate_pre_main, tcpm_setup, tcpm_before,
	tcpm_after, NULL);
