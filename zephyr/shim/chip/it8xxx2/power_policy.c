/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <pm/pm.h>
#include <soc.h>

#include "console.h"
#include "cros_version.h"
#include "gpio.h"
#include "system.h"

static const struct pm_state_info pm_min_residency[] =
	PM_STATE_INFO_DT_ITEMS_LIST(DT_NODELABEL(cpu0));

/* CROS PM policy handler */
struct pm_state_info pm_policy_next_state(int32_t ticks)
{

	/* Disable all interrupts. */
	interrupt_disable_all();

	/* Deep sleep is allowed and console is not in use. */
	if (DEEP_SLEEP_ALLOWED) {

#if 0
		for (int i = ARRAY_SIZE(pm_min_residency) - 1; i >= 0; i--) {
			/* Find suitable power state by residency time */
			if (ticks == K_TICKS_FOREVER ||
			    ticks >= k_us_to_ticks_ceil32(
					     pm_min_residency[i]
						     .min_residency_us)) {


				return pm_min_residency[i];
			}
		}
#endif
		/* Save and disable interrupts */
		if (IS_ENABLED(CONFIG_ITE_IT8XXX2_INTC))
			ite_intc_save_and_disable_interrupts();

		/* enable uart wui */
		gpio_enable_interrupt(GPIO_UART1_RX);
		/* deep doze mode */
		chip_pll_ctrl(CHIP_PLL_DEEP_DOZE);
		/* Wait for interrupt */
		__asm__ volatile("wfi");

		if (IS_ENABLED(CONFIG_ITE_IT8XXX2_INTC))
			ite_intc_restore_interrupts();

		return pm_min_residency[1];
	}

	return (struct pm_state_info){ PM_STATE_ACTIVE, 0, 0 };
}
