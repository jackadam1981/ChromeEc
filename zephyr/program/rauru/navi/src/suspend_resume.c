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

/*
 * This file provides a workaround for an issue on early versions of the
 * Rauru/Navi platform where the S3 indicator (AP_IN_SLEEP_L) doesn't
 * function correctly.
 *
 * To address this, we utilize the AP/EC SPI CS_L pin as a substitute S3
 * indicator. When the AP enters S3 sleep (by sending the
 * EC_CMD_HOST_SLEEP_EVENT command to the EC), we monitor the CS_L line.
 * A low signal on CS_L indicates that the AP is in S3.  We then set the
 * AP_IN_SLEEP_L pin to OUTPUT_LOW.
 *
 * Upon resuming from S3, the CS_L line goes high. This signals the AP's
 * wake-up, allowing us to accurately detect the AP's power state.
 */
#define PREINIT_VERSION 0xDEADDEAD

static uint32_t version = PREINIT_VERSION;
static struct gpio_callback cs_cb;
const static struct gpio_dt_spec cs =
	GPIO_DT_SPEC_GET(DT_NODELABEL(shi0), cs_gpios);

static void add_callback(void)
{
	gpio_add_callback_dt(&cs, &cs_cb);
	gpio_pin_interrupt_configure_dt(&cs, GPIO_INT_EDGE_BOTH);
}

static void remove_callback(void)
{
	gpio_remove_callback_dt(&cs, &cs_cb);
	gpio_pin_interrupt_configure_dt(&cs, GPIO_INT_EDGE_FALLING);
}

static void enable_irq_deferred(void)
{
	add_callback();
}
DECLARE_DEFERRED(enable_irq_deferred);

void ap_wakeup_isr(const struct device *port, struct gpio_callback *cb,
		   gpio_port_pins_t pins)

{
	const struct gpio_dt_spec *s3_indicator_l =
		GPIO_DT_FROM_NODELABEL(gpio_ap_in_sleep_l);
	int val = gpio_pin_get_dt(&cs);

	if (val) {
		remove_callback();
		gpio_pin_configure_dt(s3_indicator_l, GPIO_INPUT);
	} else {
		gpio_pin_configure_dt(s3_indicator_l, GPIO_OUTPUT_LOW);
	}
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
		remove_callback();
		gpio_pin_configure_dt(s3_indicator_l, GPIO_INPUT);
		break;
	default:
		break;
	}
}

static void init_suspend_resume(void)
{
	static struct ap_power_ev_callback cb;

	if (version == PREINIT_VERSION) {
		if (cbi_get_board_version(&version)) {
			LOG_ERR("Getting board version failed.");
			return;
		}
	}

	if (version <= 1) {
		return;
	}

	LOG_INF("Board version <= 1. Applied suspend_resume workaround.");
	ap_power_ev_init_callback(&cb, navi_power_event_handler,
				  AP_POWER_RESUME_INIT | AP_POWER_SHUTDOWN);
	ap_power_ev_add_callback(&cb);

	/* initialize the callback */
	gpio_init_callback(&cs_cb, ap_wakeup_isr, BIT(cs.pin));
}
/* Ensure this hook is called after CBI init */
DECLARE_HOOK(HOOK_INIT, init_suspend_resume, HOOK_PRIO_LAST);

__override void board_process_host_sleep_event(enum host_sleep_event state)
{
	if (version <= 1) {
		return;
	}

	if (state == HOST_SLEEP_EVENT_S3_SUSPEND) {
		/*
		 * Delay 50 ms to enable the IRQ so it won't be triggered by the
		 * on-going SPI transaction
		 */
		hook_call_deferred(&enable_irq_deferred_data, 50 * MSEC);
	}
}
