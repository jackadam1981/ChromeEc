/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_power/ap_power.h"
#include "atomic.h"
#include "cros_board_info.h"
#include "hooks.h"
#include "power/mt8186.h"
#include "timer.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/dt-bindings/gpio/ite-it8xxx2-gpio.h>
#include <zephyr/dt-bindings/interrupt-controller/ite-intc.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/policy.h>
#include <zephyr/sys/atomic.h>

#include <chip_chipregs.h>

LOG_MODULE_DECLARE(suspend_resume_hack, LOG_LEVEL_INF);

#define IRQ_CONNECTED 0
#define PREINIT_VERSION 0xFFFFFFFF

atomic_t connected;
static uint32_t version = PREINIT_VERSION;
static struct gpio_callback cs_cb;
const static struct gpio_dt_spec cs =
	GPIO_DT_SPEC_GET(DT_NODELABEL(shi0), cs_gpios);

static void enable_irq_deferred(void)
{
	int ret;
	ret = gpio_add_callback_dt(&cs, &cs_cb);
	if (ret < 0) {
		LOG_ERR("Failed to add interrupt %d", ret);
	}
	ret = gpio_pin_interrupt_configure_dt(&cs, GPIO_INT_EDGE_BOTH);
	if (ret < 0) {
		LOG_ERR("Failed to configure CS(0x%x) interrupt %d",
			GPIO_INT_EDGE_BOTH, ret);
	}
}
DECLARE_DEFERRED(enable_irq_deferred);

void ap_wakeup_isr(const struct device *port, struct gpio_callback *cb,
		   gpio_port_pins_t pins)

{
	const struct gpio_dt_spec *s3_indicator_l =
		GPIO_DT_FROM_NODELABEL(gpio_ap_in_sleep_l);
	int val = gpio_pin_get_dt(&cs);
	int ret;

	LOG_INF("%sRecv AP wake-up signal CS %d\033[m",
		val == 0 ? "\033[32m" : "\033[31m", val);

	if (val) {
		gpio_pin_configure_dt(s3_indicator_l, GPIO_INPUT);
		ret = gpio_remove_callback_dt(&cs, &cs_cb);
		if (ret < 0) {
			LOG_ERR("Failed to remove interrupt %d", ret);
		}
		ret = gpio_pin_interrupt_configure_dt(&cs,
						      GPIO_INT_EDGE_FALLING);
		if (ret < 0) {
			LOG_ERR("Failed to configure CS(0x%x) interrupt %d",
				GPIO_INT_EDGE_FALLING, ret);
		}
	} else {
		gpio_pin_configure_dt(s3_indicator_l, GPIO_OUTPUT_LOW);
	}
}

static void navi_power_event_handler(struct ap_power_ev_callback *callback,
				     struct ap_power_ev_data data)
{
	int ret;
	const struct gpio_dt_spec *s3_indicator_l =
		GPIO_DT_FROM_NODELABEL(gpio_ap_in_sleep_l);

	switch (data.event) {
	case AP_POWER_SHUTDOWN:
		/* fall-through */
	case AP_POWER_RESUME_INIT:
		ret = gpio_remove_callback_dt(&cs, &cs_cb);
		if (ret < 0) {
			LOG_ERR("Failed to remove interrupt(%d)", ret);
		}
		ret = gpio_pin_interrupt_configure_dt(&cs,
						      GPIO_INT_EDGE_FALLING);
		if (ret < 0) {
			LOG_ERR("Failed to configure CS(0x%x) interrupt %d",
				GPIO_INT_EDGE_FALLING, ret);
		}
		gpio_pin_configure_dt(s3_indicator_l, GPIO_INPUT);
		break;
	case AP_POWER_SUSPEND:
	default:
		break;
	}
}

static void init_suspend_resume(void)
{
	static struct ap_power_ev_callback cb;
	int ret;

	if (version == PREINIT_VERSION) {
		if (cbi_get_board_version(&version)) {
			LOG_ERR("Getting board version failed.");
			return;
		}
	}

	if (version != 1) {
		return;
	}

	LOG_INF("Board version = 1. Applied suspend_resume workaround.");
	ap_power_ev_init_callback(&cb, navi_power_event_handler,
				  AP_POWER_RESUME_INIT | AP_POWER_SUSPEND |
					  AP_POWER_SHUTDOWN);
	ap_power_ev_add_callback(&cb);

	/* initialize the callback */
	gpio_init_callback(&cs_cb, ap_wakeup_isr, BIT(cs.pin));
	if (ret < 0) {
		LOG_ERR("Failed to add CS callback");
	}
}
/* Ensure this hook is called after CBI init */
DECLARE_HOOK(HOOK_INIT, init_suspend_resume, HOOK_PRIO_LAST);

__override void board_process_host_sleep_event(enum host_sleep_event state)
{
	if (version != 1) {
		return;
	}

	if (state == HOST_SLEEP_EVENT_S3_SUSPEND) {
		/*
		 * delay 50 ms to enable the IRQ so it won't be triggered by the
		 * on-going SPI transaction
		 */
		hook_call_deferred(&enable_irq_deferred_data, 50 * MSEC);
	}
}
