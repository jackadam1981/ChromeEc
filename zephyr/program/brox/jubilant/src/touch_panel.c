/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "cros_board_info.h"
#include "cros_cbi.h"
#include "gpio/gpio.h"
#include "gpio/gpio_int.h"
#include "hooks.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(brox_touch, LOG_LEVEL_INF);

/* touch panel power sequence control */

#define TOUCH_ENABLE_DELAY_MS (500 * USEC_PER_MSEC)
#define TOUCH_DISABLE_DELAY_MS (0 * USEC_PER_MSEC)

void touch_disable(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_touch_en), 0);
}
DECLARE_DEFERRED(touch_disable);

void touch_enable(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_touch_en), 1);
}
DECLARE_DEFERRED(touch_enable);

/* Called on AP S3 -> S5 transition */
static void pogo_chipset_shutdown(void)
{
	/* Cancel touch_enable touch_enable touch_disable_hook. */
	hook_call_deferred(&touch_enable_data, -1);
	hook_call_deferred(&touch_disable_data, -1);

	touch_disable();
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, pogo_chipset_shutdown, HOOK_PRIO_DEFAULT);

static void touch_panel_interrupt(const struct device *device,
				  struct gpio_callback *callback,
				  gpio_port_pins_t pins)
{
	int state;

	state = gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_soc_edp_bl_en));

	LOG_INF("%s: %d", __func__, state);

	if (state) {
		hook_call_deferred(&touch_enable_data, TOUCH_ENABLE_DELAY_MS);
	} else {
		hook_call_deferred(&touch_disable_data, TOUCH_DISABLE_DELAY_MS);
	}
}

static void touch_enable_init(void)
{
	const struct gpio_dt_spec *const tpgpio_gpio =
		GPIO_DT_FROM_NODELABEL(gpio_soc_edp_bl_en);

	static struct gpio_callback cb;
	int rv, ret, irq_key;
	uint32_t val;

	ret = cbi_get_board_version(&val);
	if (ret != EC_SUCCESS) {
		LOG_ERR("Error retrieving CBI BOARD_VER.");
		return;
	}

	LOG_INF("%s: %sable", __func__, (val >= 4) ? "en" : "dis");

	if (val <= 3)
		return;

	gpio_init_callback(&cb, touch_panel_interrupt, BIT(tpgpio_gpio->pin));
	gpio_add_callback(tpgpio_gpio->port, &cb);

	rv = gpio_pin_interrupt_configure_dt(tpgpio_gpio, GPIO_INT_EDGE_BOTH);
	__ASSERT(rv == 0,
		 "touch panel interrupt configuration returned error %d", rv);
	/*
	 * Run the touch_panel handler once to ensure output is in sync.
	 * Lock interrupts to ensure that we don't cause desync if an
	 * interrupt comes in between the internal read of the input
	 * and write to the output.
	 */
	irq_key = irq_lock();
	touch_panel_interrupt(tpgpio_gpio->port, &cb, BIT(tpgpio_gpio->pin));
	irq_unlock(irq_key);

	return;
}
DECLARE_HOOK(HOOK_INIT, touch_enable_init, HOOK_PRIO_POST_FIRST);
