/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "host_command.h"
#include "task.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio_keys.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/buttons.h>

LOG_MODULE_REGISTER(button, CONFIG_GPIO_LOG_LEVEL);

#define DT_DRV_COMPAT zephyr_gpio_keys

static struct buttons_data_s {
	int8_t power_button_state;
} button_data;

int power_button_is_pressed(void)
{
	return button_data.power_button_state;
}

int power_button_wait_for_release(int timeout_us)
{
	int check_interval_us = 30000;
	bool released;

	released =
		WAIT_FOR(!(button_data.power_button_state), timeout_us,
			 task_wait_event(MIN(timeout_us, check_interval_us)));

	return released ? 0 : -ETIMEDOUT;
}

static void handle_power_button(int8_t new_pin_state)
{
	LOG_DBG("Handling power button state=%d", new_pin_state);

	button_data.power_button_state = new_pin_state;

	hook_notify(HOOK_POWER_BUTTON_CHANGE);
	host_set_single_event(EC_HOST_EVENT_POWER_BUTTON);
}

static void buttons_cb_handler(const struct device *dev,
			       struct gpio_keys_callback *cbdata, uint32_t pins)
{
	LOG_DBG("Button %s, pins=0x%x, zephyr_code=%u, pin_state=%d", dev->name,
		pins, cbdata->zephyr_code, cbdata->pin_state);

	switch (cbdata->zephyr_code) {
	case BUTTON_POWER:
		handle_power_button(cbdata->pin_state);
		break;
	default:
		LOG_ERR("Unknown button code=%u", cbdata->zephyr_code);
		break;
	}
}

#define BUTTONS_INIT(node_id)                                           \
	gpio_keys_enable_interrupt(DEVICE_DT_GET(DT_DRV_INST(node_id)), \
				   buttons_cb_handler)

static int buttons_init(const struct device *device)
{
	DT_INST_FOREACH_STATUS_OKAY(BUTTONS_INIT);

	return 0;
}

SYS_INIT(buttons_init, POST_KERNEL, 51);
