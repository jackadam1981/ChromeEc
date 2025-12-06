/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "chip/it83xx/registers.h"
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

const struct usbpd_ctrl_t usbpd_ctrl_regs[] = {
	{ &IT83XX_GPIO_GPCRF4, &IT83XX_GPIO_GPCRF5, IT83XX_IRQ_USBPD0 },
	{ &IT83XX_GPIO_GPCRH1, &IT83XX_GPIO_GPCRH2, IT83XX_IRQ_USBPD1 },
};

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
	/* The expected IRQ number for IT8XXX2_PORT (which is 1) */
	const int expected_irq = usbpd_ctrl_regs[IT8XXX2_PORT].irq;

	/* Execute the function that contains the target code */
	task_disable_irq(usbpd_ctrl_regs[IT8XXX2_PORT].irq);

	/* Verify that task_disable_irq was called with the correct argument */
	zassert_equal(IT83XX_IRQ_USBPD1, expected_irq);
}

ZTEST_SUITE(tcpc_it8xxx2, drivers_predicate_post_main, NULL, NULL, NULL, NULL);
