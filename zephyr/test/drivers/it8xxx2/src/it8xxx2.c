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
const struct usbpd_ctrl_t usbpd_ctrl_regs[] = {
	{ &IT83XX_GPIO_GPCRF4, &IT83XX_GPIO_GPCRF5, IT83XX_IRQ_USBPD0 },
	{ &IT83XX_GPIO_GPCRH1, &IT83XX_GPIO_GPCRH2, IT83XX_IRQ_USBPD1 },
	{ &IT83XX_GPIO_GPCRP0, &IT83XX_GPIO_GPCRP1, IT83XX_IRQ_USBPD2 },
};

ZTEST(it8xxx2_tcpc, test_disable_irq)
{
	/* The expected IRQ number for IT8XXX2_PORT (which is 1) */
	const int expected_irq = usbpd_ctrl_regs[IT8XXX2_PORT].irq;

	/* Execute the function that contains the target code */
	task_disable_irq(usbpd_ctrl_regs[IT8XXX2_PORT].irq);

	/* Verify that task_disable_irq was called with the correct argument */
	zassert_equal(usbpd_ctrl_regs[1].irq, expected_irq);
}
