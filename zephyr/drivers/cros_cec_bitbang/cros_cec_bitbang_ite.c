/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_ec_cec_bitbang

#include "cec.h"
#include "driver/cec/bitbang.h"
#include "ec_tasks.h"
#include "gpio/gpio_int.h"
#include "task.h"
#include "timer.h"

#include <zephyr/drivers/counter.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/cros_cec_bitbang.h>

LOG_MODULE_REGISTER(cros_cec_bitbang, LOG_LEVEL_ERR);

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) >= 1,
	     "At least one instance of cros-ec,cec-bitbang should be defined.");

#define CEC_CLOCK_FREQ_HZ 32768
#define CEC_US_TO_TICKS(t) ((t) * CEC_CLOCK_FREQ_HZ / 1000000)

/* Timestamp when the most recent interrupt occurred */
static timestamp_t interrupt_time;

/* Timestamp when the second most recent interrupt occurred */
static timestamp_t prev_interrupt_time;

/* Flag set when a transfer is initiated from the AP */
static bool transfer_initiated;

/* The capture edge we're waiting for */
static enum cec_cap_edge expected_cap_edge;

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

// void cec_ext_timer_interrupt(enum ext_timer_sel ext_timer)
//{
//	int port = port_from_timer(ext_timer);

//	if (transfer_initiated) {
//		transfer_initiated = false;
//		cec_event_tx(port);
//	} else {
//		cec_update_interrupt_time(port);
//		cec_event_timeout(port);
//	}
//}

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

static int cec_bitbang_ite_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	/* Clock default is on */
	return 0;
}

static int cros_cec_bitbang_it8xxx2_tmr_cap_start(const struct device *dev,
						  int port,
						  enum cec_cap_edge edge,
						  int timeout)
{
	ARG_UNUSED(dev);

	//	const struct bitbang_cec_config *drv_config =
	//		cec_config[port].drv_config;

	expected_cap_edge = edge;

	if (timeout > 0) {
		/*
		 * Take into account the delay from when the interrupt occurs to
		 * when we actually get here. Since the timing is done in
		 * software, there is an additional unknown delay from when the
		 * interrupt occurs to when the ISR starts. Empirically, this
		 * seems to be about 100 us, so account for this too.
		 */
		int delay = CEC_US_TO_TICKS(get_time().val -
					    interrupt_time.val + 100);
		int timer_count = timeout - delay;

		/*
		 * Handle the case where the delay is greater than the timeout.
		 * This should never actually happen for typical delay and
		 * timeout values.
		 */
		if (timer_count < 0) {
			timer_count = 0;
			LOG_WRN("CEC%d warning: timer_count < 0", port);
		}

		/* Start the timer and enable the timer interrupt */
		//		ext_timer_ms(drv_config->timer,
		// CEC_CLOCK_SOURCE, 1, 1, timer_count, 0, 1);
	} else {
		//		ext_timer_stop(drv_config->timer, 1);
	}

	return 0;
}

static int cros_cec_bitbang_it8xxx2_tmr_cap_stop(const struct device *dev,
						 int port)
{
	ARG_UNUSED(dev);
	const struct bitbang_cec_config *drv_config =
		cec_config[port].drv_config;

	gpio_pin_interrupt_configure_dt(gpio_get_dt_spec(drv_config->gpio_in),
					GPIO_INT_DISABLE);
	//	ext_timer_stop(drv_config->timer, 1);
	return 0;
}

static int cros_cec_bitbang_it8xxx2_tmr_cap_get(const struct device *dev,
						int port)
{
	ARG_UNUSED(dev);
	return CEC_US_TO_TICKS(interrupt_time.val - prev_interrupt_time.val);
}

static int cros_cec_bitbang_it8xxx2_debounce_enable(const struct device *dev,
						    int port)
{
	ARG_UNUSED(dev);
	const struct bitbang_cec_config *drv_config =
		cec_config[port].drv_config;

	gpio_pin_interrupt_configure_dt(gpio_get_dt_spec(drv_config->gpio_in),
					GPIO_INT_DISABLE);
	return 0;
}

static int cros_cec_bitbang_it8xxx2_debounce_disable(const struct device *dev,
						     int port)
{
	ARG_UNUSED(dev);
	const struct bitbang_cec_config *drv_config =
		cec_config[port].drv_config;

	gpio_pin_interrupt_configure_dt(gpio_get_dt_spec(drv_config->gpio_in),
					GPIO_INT_ENABLE | GPIO_INT_EDGE_BOTH);
	return 0;
}

static int cros_cec_bitbang_it8xxx2_trigger_send(const struct device *dev,
						 int port)
{
	ARG_UNUSED(dev);
	//	const struct bitbang_cec_config *drv_config =
	//		cec_config[port].drv_config;

	/* Elevate to interrupt context */
	transfer_initiated = true;
	//	task_trigger_irq(et_ctrl_regs[drv_config->timer].irq);
	return 0;
}

static int cros_cec_bitbang_it8xxx2_enable_timer(const struct device *dev,
						 int port)
{
	ARG_UNUSED(dev);
	const struct bitbang_cec_config *drv_config =
		cec_config[port].drv_config;

	/*
	 * Enable gpio interrupts. Timer interrupts will be enabled as needed by
	 * cec_tmr_cap_start().
	 */
	gpio_pin_interrupt_configure_dt(gpio_get_dt_spec(drv_config->gpio_in),
					GPIO_INT_ENABLE | GPIO_INT_EDGE_BOTH);
	return 0;
}

static int cros_cec_bitbang_it8xxx2_disable_timer(const struct device *dev,
						  int port)
{
	ARG_UNUSED(dev);
	cros_cec_bitbang_it8xxx2_tmr_cap_stop(dev, port);

	interrupt_time.val = 0;
	prev_interrupt_time.val = 0;
	return 0;
}

static int cros_cec_bitbang_it8xxx2_init_timer(const struct device *dev,
					       int port)
{
	ARG_UNUSED(dev);
	//	const struct bitbang_cec_config *drv_config =
	//		cec_config[port].drv_config;

	//	ext_timer_ms(drv_config->timer, CEC_CLOCK_SOURCE, 0, 0, 0, 1,
	// 0);
	return 0;
}

static DEVICE_API(cros_cec_bitbang, cros_cec_bitbang_ite_driver_api) = {
	.tmr_cap_start = cros_cec_bitbang_it8xxx2_tmr_cap_start,
	.tmr_cap_stop = cros_cec_bitbang_it8xxx2_tmr_cap_stop,
	.tmr_cap_get = cros_cec_bitbang_it8xxx2_tmr_cap_get,
	.debounce_enable = cros_cec_bitbang_it8xxx2_debounce_enable,
	.debounce_disable = cros_cec_bitbang_it8xxx2_debounce_disable,
	.trigger_send = cros_cec_bitbang_it8xxx2_trigger_send,
	.enable_timer = cros_cec_bitbang_it8xxx2_enable_timer,
	.disable_timer = cros_cec_bitbang_it8xxx2_disable_timer,
	.init_timer = cros_cec_bitbang_it8xxx2_init_timer,
};

DEVICE_DT_INST_DEFINE(0, cec_bitbang_ite_init, NULL, NULL, NULL, PRE_KERNEL_1,
		      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
		      &cros_cec_bitbang_ite_driver_api);
