/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/it83xx_pd.h"
#include "driver/tcpm/it8xxx2_pd_public.h"
#include "driver/tcpm/tcpci.h"
#include "emul/tcpc/emul_it8xxx2.h"
#include "emul/tcpc/emul_tcpci.h"
#include "test/drivers/test_state.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#define IT8XXX2_PORT 1
// #define IT8XXX2_NODE DT_NODELABEL(it8xxx2_emul)

// const struct emul *it8xxx2_emul = EMUL_DT_GET(IT8XXX2_NODE);

ZTEST(tcpc_it8xxx2, test_init_disables_irq_with_correct_param)
{
	const int expected_irq = usbpd_ctrl_regs[IT8XXX2_PORT].irq;

	/* Clear previous history of the fake function */
	RESET_FAKE(task_disable_irq);

	/*
	 * Action: Trigger the code path.
	 * tcpm_init() calls the driver's .init() handler, which is where the
	 * #ifdef CONFIG_ZEPHYR block (containing task_disable_irq) resides.
	 */
	zassert_ok(tcpm_init(IT8XXX2_PORT));

	/* * Assertion 1: Verify task_disable_irq was called exactly once.
	 * The original test already had this assertion.
	 */
	zassert_equal(task_disable_irq_fake.call_count, 1,
		      "task_disable_irq should be called once during init");

	/* * Assertion 2: Verify the argument passed was the correct IRQ number.
	 * This explicitly covers the parameter: usbpd_ctrl_regs[port].irq.
	 */
	zassert_equal(task_disable_irq_fake.arg0_val, expected_irq,
		      "task_disable_irq called with incorrect IRQ number");
}

/* Original test, renamed to avoid redundancy if the new test replaces it */
ZTEST(tcpc_it8xxx2, test_init_disables_irq_legacy)
{
	/* Clear previous history of the fake function */
	RESET_FAKE(task_disable_irq);

	zassert_ok(tcpm_init(IT8XXX2_PORT));

	/* Verify task_disable_irq was called exactly once */
	zassert_equal(task_disable_irq_fake.call_count, 1,
		      "task_disable_irq should be called once during init");
}
