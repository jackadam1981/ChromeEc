/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "driver/tcpm/tcpci.h"
#include "test/drivers/test_state.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#define IT8XXX2_PORT 1

/* Define the Fakes */
FAKE_VOID_FUNC(chip_pd_irq, const void *);

/* These task functions are likely needed unless provided by
 * a common test library in your environment.
 */
FAKE_VOID_FUNC(task_disable_irq, int);
FAKE_VOID_FUNC(task_enable_irq, int);
FAKE_VOID_FUNC(task_clear_pending_irq, int);

/* Register the Fakes */
static void drivers_predicate_post_main(void *fixture)
{
	RESET_FAKE(chip_pd_irq);
	RESET_FAKE(task_disable_irq);
	RESET_FAKE(task_enable_irq);
	RESET_FAKE(task_clear_pending_irq);
}

ZTEST(tcpc_it8xxx2, test_enter_l_p_m)
{
	zassert_ok(tcpm_enter_low_power_mode(IT8XXX2_PORT));
}

ZTEST(tcpc_it8xxx2, test_set_vconn)
{
	zassert_ok(tcpm_set_vconn(IT8XXX2_PORT, 0));
	zassert_ok(tcpm_set_vconn(IT8XXX2_PORT, 1));
	zassert_ok(tcpm_set_vconn(IT8XXX2_PORT, 0));
}

ZTEST(tcpc_it8xxx2, test_init)
{
	/* This will now successfully link and run because
	 * task_disable_irq and chip_pd_irq are defined.
	 */
	zassert_ok(tcpm_init(IT8XXX2_PORT));

	/* Optional: Verify the irq disable was actually called */
	zassert_equal(task_disable_irq_fake.call_count, 1,
		      "Expected task_disable_irq to be called once");
}

ZTEST_SUITE(tcpc_it8xxx2, drivers_predicate_post_main, NULL, NULL, NULL, NULL);
