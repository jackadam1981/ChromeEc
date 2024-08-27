/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_power/ap_pwrseq_sm.h"
#include "gpio.h"
#include "gpio_signal.h"
#include "system_boot_time.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <power_signals.h>

LOG_MODULE_DECLARE(ap_pwrseq, LOG_LEVEL_INF);

#define X86_NON_DSX_FORCE_SHUTDOWN_TO_MS 50

#define BOARD_BOOT_HALT_DELAY	 K_MSEC(1000)

static struct k_work_delayable boot_halt_work;

static void board_boot_halt_handler(struct k_work *ccd_work)
{
	if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(boot_halt))) {
		/* Keep polling pin state */
		k_work_schedule(&boot_halt_work, BOARD_BOOT_HALT_DELAY);
	} else {
		/* Resume normal power sequence */
		const struct device *ap_dev = ap_pwrseq_get_instance();

		ap_pwrseq_post_event(ap_dev, AP_PWRSEQ_EVENT_POWER_TIMEOUT);
	}
}

static int board_boot_halt_handler_init(void)
{
	k_work_init_delayable(&boot_halt_work, board_boot_halt_handler);

	return 0;
}
SYS_INIT(board_boot_halt_handler_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

void board_ap_power_force_shutdown(void)
{
	int timeout_ms = X86_NON_DSX_FORCE_SHUTDOWN_TO_MS;

	/* Turn off PCH_RMSRST to meet tPCH12 */
	power_signal_set(PWR_EC_PCH_RSMRST, 1);

	/* Turn off PRIM load switch. */
	power_signal_set(PWR_EN_PP3300_A, 0);

	power_signal_set(PWR_EN_PP5000_A, 0);
	/* Wait RSMRST to be off. */
	while (power_signal_get(PWR_RSMRST_PWRGD) && (timeout_ms > 0)) {
		k_msleep(1);
		timeout_ms--;
	};

	if (power_signal_get(PWR_RSMRST_PWRGD))
		LOG_WRN("RSMRST_PWRGD didn't go low!  Assuming G3.");
}

int board_ap_power_action_g3_entry(void *data)
{
	board_ap_power_force_shutdown();

	return 0;
}

static int board_ap_power_action_g3_run(void *data)
{
	if (ap_pwrseq_sm_is_event_set(data, AP_PWRSEQ_EVENT_POWER_STARTUP)) {
		power_signal_set(PWR_EN_PP5000_A, 1);
		/* Turn on the PP3300_PRIM rail. */
		power_signal_set(PWR_EN_PP3300_A, 1);
		update_ap_boot_time(ARAIL);
	}

	/* Return 0 only if power rails have been enabled  */
	return !power_signal_get(PWR_EN_PP3300_A);
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_G3, board_ap_power_action_g3_entry,
			  board_ap_power_action_g3_run, NULL);


static int board_ap_power_action_s5_run(void *data)
{
	if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(boot_halt)) &&
	    power_signal_get(PWR_EC_PCH_RSMRST) == 0 &&
	    power_signal_get(PWR_SLP_S5) != 0) {
		LOG_INF("Halting SOC Boot");
		k_work_schedule(&boot_halt_work, BOARD_BOOT_HALT_DELAY);
		return 1;
	}

	return 0;
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_S5, NULL,
			  board_ap_power_action_s5_run, NULL);

int board_power_signal_get(enum power_signal signal)
{
	switch (signal) {
	case PWR_EC_PCH_SYS_PWROK:
		return power_signal_get(PWR_PCH_PWROK);
		break;
	default:
		return -EINVAL;
		break;
	}
	return 0;
}

int board_power_signal_set(enum power_signal signal, int value)
{
	switch (signal) {
	case PWR_SYS_RST:
		gpio_set_level(GPIO_SYS_RST_ODL, value);
		break;
	default:
		return -EINVAL;
		break;
	}
	return 0;
}
