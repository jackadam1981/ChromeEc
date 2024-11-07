/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_power/ap_power.h"
#include "cros_board_info.h"
#include "hooks.h"
#include "power/mt8186.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/dt-bindings/gpio/ite-it8xxx2-gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/policy.h>

#include <chip_chipregs.h>

LOG_MODULE_DECLARE(suspend_resume_hack, LOG_LEVEL_INF);

void ap_wakeup_irq(const void *data)
{
	const struct gpio_dt_spec *s3_indicator_l =
		GPIO_DT_FROM_NODELABEL(gpio_ap_in_sleep_l);

	LOG_INF("Recv AP wake-up signal");
	gpio_pin_configure_dt(s3_indicator_l, GPIO_INPUT);
}

static void navi_power_event_handler(struct ap_power_ev_callback *callback,
				     struct ap_power_ev_data data)
{
	const struct gpio_dt_spec *s3_indicator_l =
		GPIO_DT_FROM_NODELABEL(gpio_ap_in_sleep_l);

	switch (data.event) {
	case AP_POWER_SHUTDOWN:
		/* fall-through */
	case AP_POWER_RESUME_INIT:
		gpio_pin_configure_dt(s3_indicator_l, GPIO_INPUT);
		irq_disconnect_dynamic(DT_IRQN(DT_NODELABEL(shi0)), 0,
				       ap_wakeup_irq, NULL, 0);
		break;
	case AP_POWER_SUSPEND:
		irq_connect_dynamic(DT_IRQN(DT_NODELABEL(shi0)), 0,
				    ap_wakeup_irq, NULL, 0);
		break;
	default:
		/* Other events ignored */
		break;
	}
}

static void init_suspend_resume(void)
{
	static struct ap_power_ev_callback cb;
	uint32_t version;

	if (cbi_get_board_version(&version)) {
		LOG_ERR("Getting board version failed.");
		return;
	}

	if (version != 1) {
		return;
	}

	LOG_INF("Board version = 1. Appled suspend_resume workaround.");
	ap_power_ev_init_callback(&cb, navi_power_event_handler,
				  AP_POWER_RESUME_INIT | AP_POWER_SUSPEND |
					  AP_POWER_SHUTDOWN);
	ap_power_ev_add_callback(&cb);
}
/* Ensure this hook is called after CBI init */
DECLARE_HOOK(HOOK_INIT, init_suspend_resume, HOOK_PRIO_LAST);

__override void board_process_host_sleep_event(enum host_sleep_event state)
{
	const struct gpio_dt_spec *s3_indicator_l =
		GPIO_DT_FROM_NODELABEL(gpio_ap_in_sleep_l);

	if (state == HOST_SLEEP_EVENT_S3_SUSPEND) {
		gpio_pin_configure_dt(s3_indicator_l, GPIO_OUTPUT_LOW);
	}
}
