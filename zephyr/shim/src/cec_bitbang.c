/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cec.h"
#include "driver/cec/bitbang.h"
#include "driver/cec/it83xx_mock.h"
#include "ec_tasks.h"
#include "gpio/gpio_int.h"
#include "task.h"
#include "timer.h"

#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(shim_cros_cec_bitbang, LOG_LEVEL_ERR);

/* Do-nothing implementations which can be overridden for testing. */
#define counter_ite_dev DEVICE_DT_GET(DT_NODELABEL(counter0))

/* Timestamp when the most recent interrupt occurred */
static timestamp_t interrupt_time;

/* Timestamp when the second most recent interrupt occurred */
static timestamp_t prev_interrupt_time;

/* Flag set when a transfer is initiated from the AP */
static bool transfer_initiated;

/* The capture edge we're waiting for */
static enum cec_cap_edge expected_cap_edge;

static int port_from_drv(void)
{
	int port;

	for (port = 0; port < CEC_PORT_COUNT; port++) {
		if (cec_config[port].drv == &bitbang_cec_drv) {
			return port;
		}
	}

	/*
	 * If we don't find a match, return 0. The only way for this to happen
	 * is a configuration error, e.g. an incorrect timer is specified in
	 * board.c, and we assume static configuration is correct to improve
	 * performance.
	 */
	return 0;
}

static int port_from_gpio_in(enum gpio_signal signal)
{
	int port;
	const struct bitbang_cec_config *drv_config;

	for (port = 0; port < CEC_PORT_COUNT; port++) {
		if (cec_config[port].drv == &bitbang_cec_drv) {
			drv_config = cec_config[port].drv_config;
			if (drv_config->gpio_in == signal)
				return port;
		}
	}

	/*
	 * If we don't find a match, return 0. The only way for this to happen
	 * is a configuration error, e.g. an incorrect pin is mapped to
	 * cec_gpio_interrupt in gpio.inc, and we assume static configuration
	 * is correct to improve performance.
	 */
	return 0;
}

__override void cec_update_interrupt_time(int port)
{
	prev_interrupt_time = interrupt_time;
	interrupt_time = get_time();
}

/*
 * In chip/it83xx/cec_bitbang.c
 * void cec_ext_timer_interrupt(enum ext_timer_sel ext_timer)
 */
void cec_ext_timer_interrupt(void)
{
	int port = port_from_drv();

	if (transfer_initiated) {
		transfer_initiated = false;
		cec_event_tx(port);
	} else {
		cec_update_interrupt_time(port);
		cec_event_timeout(port);
	}
}

void cec_ext_top_timer_handler(const struct device *dev, void *user_data)
{
	cec_ext_timer_interrupt();
}

void cec_gpio_interrupt(enum gpio_signal signal)
{
	int port = port_from_gpio_in(signal);
	int level;

	cec_update_interrupt_time(port);

	level = gpio_pin_get_dt(gpio_get_dt_spec(signal));
	if (!((expected_cap_edge == CEC_CAP_EDGE_FALLING && level == 0) ||
	      (expected_cap_edge == CEC_CAP_EDGE_RISING && level == 1)))
		return;

	cec_event_cap(port);
}

test_mockable void cec_tmr_cap_start(int port, enum cec_cap_edge edge,
				     int timeout)
{
	expected_cap_edge = edge;

	if (timeout > 0) {
		/*
		 * Take into account the delay from when the interrupt occurs to
		 * when we actually get here. Since the timing is done in
		 * software, there is an additional unknown delay from when the
		 * interrupt occurs to when the ISR starts. Empirically, this
		 * seems to be about 100 us, so account for this too.
		 */
		int delay = counter_us_to_ticks(
			counter_ite_dev,
			(get_time().val - interrupt_time.val + 100));
		int timer_count = timeout - delay;
		struct counter_top_cfg top_cfg;

		/*
		 * Handle the case where the delay is greater than the timeout.
		 * This should never actually happen for typical delay and
		 * timeout values.
		 */
		if (timer_count < 0) {
			timer_count = 0;
			LOG_WRN("CEC%d warning: timer_count < 0", port);
		}

		/*
		 * Start the timer and enable the timer interrupt
		 * In chip/it83xx/cec_bitbang.c
		 * ext_timer_ms(drv_config->timer, CEC_CLOCK_SOURCE, 1, 1,
		 * timer_count, 0, 1);
		 */
		top_cfg.ticks = timer_count;
		top_cfg.callback = cec_ext_top_timer_handler;
		top_cfg.user_data = NULL;
		top_cfg.flags = 0;
		counter_set_top_value(counter_ite_dev, &top_cfg);
	} else {
		/*
		 * In chip/it83xx/cec_bitbang.c
		 * ext_timer_stop(drv_config->timer, 1);
		 */
		counter_stop(counter_ite_dev);
	}
}

void cec_tmr_cap_stop(int port)
{
	const struct bitbang_cec_config *drv_config =
		cec_config[port].drv_config;

	gpio_pin_interrupt_configure_dt(gpio_get_dt_spec(drv_config->gpio_in),
					GPIO_INT_DISABLE);

	/*
	 * In chip/it83xx/cec_bitbang.c
	 * ext_timer_stop(drv_config->timer, 1);
	 */
	counter_stop(counter_ite_dev);
}

test_mockable int cec_tmr_cap_get(int port)
{
	return counter_us_to_ticks(counter_ite_dev, (interrupt_time.val -
						     prev_interrupt_time.val));
}

test_mockable void cec_debounce_enable(int port)
{
	const struct bitbang_cec_config *drv_config =
		cec_config[port].drv_config;

	gpio_pin_interrupt_configure_dt(gpio_get_dt_spec(drv_config->gpio_in),
					GPIO_INT_DISABLE);
}

test_mockable void cec_debounce_disable(int port)
{
	const struct bitbang_cec_config *drv_config =
		cec_config[port].drv_config;

	gpio_pin_interrupt_configure_dt(gpio_get_dt_spec(drv_config->gpio_in),
					GPIO_INT_ENABLE | GPIO_INT_EDGE_BOTH);
}

test_mockable void cec_trigger_send(int port)
{
	unsigned int key;
	/* Elevate to interrupt context */
	transfer_initiated = true;

	/*
	 * In chip/it83xx/cec_bitbang.c
	 * task_trigger_irq(et_ctrl_regs[drv_config->timer].irq);
	 */
	key = irq_lock();
	cec_ext_timer_interrupt();
	irq_unlock(key);
}

void cec_enable_timer(int port)
{
	const struct bitbang_cec_config *drv_config =
		cec_config[port].drv_config;

	/*
	 * Enable gpio interrupts. Timer interrupts will be enabled as needed by
	 * cec_tmr_cap_start().
	 */
	gpio_pin_interrupt_configure_dt(gpio_get_dt_spec(drv_config->gpio_in),
					GPIO_INT_ENABLE | GPIO_INT_EDGE_BOTH);
}

void cec_disable_timer(int port)
{
	cec_tmr_cap_stop(port);

	interrupt_time.val = 0;
	prev_interrupt_time.val = 0;
}

void cec_init_timer(int port)
{
}
