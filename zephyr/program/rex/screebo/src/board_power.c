/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "gpio/gpio.h"
#include "gpio_signal.h"
#include "system_boot_time.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include <ap_power/ap_power.h>
#include <ap_power/ap_power_events.h>
#include <ap_power/ap_power_interface.h>
#include <ap_power_override_functions.h>
#include <power_signals.h>
#include <x86_power_signals.h>

#ifdef CONFIG_AP_PWRSEQ_DRIVER
#include "ap_power/ap_pwrseq_sm.h"
#endif

#ifdef CONFIG_AP_PWRSEQ_DRIVER

static int board_ap_power_action_s0_run(void *data)
{
	if (power_signal_get(PWR_PCH_PWROK) &&
	    !gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_usb_a1_oc_pu_en))) {
		k_usleep(30);
		printk("\n--S0 pull up gpio_usb_a1_oc_pu_en--\n");
		cflush();
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_usb_a1_oc_pu_en),
				1);
	}
	return 0;
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_S0, NULL, board_ap_power_action_s0_run,
			  NULL);

static int board_ap_power_action_s5_run(void *data)
{
	if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_usb_a1_oc_pu_en))) {
		printk("\n--S5 pull down gpio_usb_a1_oc_pu_en--\n");
		cflush();
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_usb_a1_oc_pu_en),
				0);
	}
	return 0;
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_S5, NULL, board_ap_power_action_s5_run,
			  NULL);
#endif /* CONFIG_AP_PWRSEQ_DRIVER */
