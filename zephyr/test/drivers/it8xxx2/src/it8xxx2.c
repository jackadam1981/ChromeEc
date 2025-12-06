/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/it83xx_pd.h" /* Include the original driver header for struct/macros */
#include "driver/tcpm/it8xxx2_public.h"
#include "driver/tcpm/tcpci.h"
#include "emul/tcpc/emul_it8xxx2.h"
#include "test/drivers/test_state.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#define IT8XXX2_PORT 1
#define IT8XXX2_NODE DT_NODELABEL(it8xxx2_emul)

const struct emul *it8xxx2_emul = EMUL_DT_GET(IT8XXX2_NODE);

/*
 * 1. DEFINE DEPENDENCY: usbpd_ctrl_regs
 * This structure is defined in it83xx.h and must be provided by the board/EC
 * for the driver to use. We define a minimal version here for the test.
 */
#define TEST_IT8XXX2_IRQ_PORT0 42
#define TEST_IT8XXX2_IRQ_PORT1 43

const struct usbpd_ctrl_t usbpd_ctrl_regs[] = {
	[0] = { .irq = TEST_IT8XXX2_IRQ_PORT0, .cc1 = NULL, .cc2 = NULL },
	[1] = { .irq = TEST_IT8XXX2_IRQ_PORT1, .cc1 = NULL, .cc2 = NULL },
};

/* Define the fake (mock) function */
FAKE_VOID_FUNC(task_disable_irq, int);

ZTEST(tcpc_it8xxx2, test_check_vendor)
{
	int v;

	zassert_ok(tcpc_read16(IT8XXX2_PORT, TCPC_REG_VENDOR_ID, &v));
	zassert_equal(v, IT8XXX2_VENDOR_ID);

	tcpm_dump_registers(IT8XXX2_PORT);
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

ZTEST(tcpc_it8xxx2, test_set_polarity)
{
	zassert_ok(tcpm_set_polarity(IT8XXX2_PORT, POLARITY_CC1));

	zassert_ok(
		tcpci_emul_set_reg(it8xxx2_emul, TCPC_REG_CC_STATUS,
				   TCPC_REG_CC_STATUS_SET(0, TYPEC_CC_VOLT_RA,
							  TYPEC_CC_VOLT_OPEN)));

	zassert_ok(tcpm_set_polarity(IT8XXX2_PORT, POLARITY_CC1));
}

static void it8xxx2_test_before(void *data)
{
	zassert_ok(
		tcpci_emul_set_reg(it8xxx2_emul, TCPC_REG_CC_STATUS,
				   TCPC_REG_CC_STATUS_SET(0, TYPEC_CC_VOLT_OPEN,
							  TYPEC_CC_VOLT_OPEN)));
}

ZTEST_SUITE(tcpc_it8xxx2, drivers_predicate_post_main, NULL,
	    it8xxx2_test_before, NULL, NULL);

/*
 * 2. NEW TEST CASE: Verify task_disable_irq is called with the correct IRQ
 * number
 */
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
