/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include "gpio_debounce_common.h"
#include "drivers/gpio_debounce.h"
#include "hooks.h"
#include "host_command.h"
#include "lid_switch.h"
#include "task.h"
#include "util.h"

#define DT_DRV_COMPAT cros_ec_power_button

LOG_MODULE_REGISTER(power_button, LOG_LEVEL_INF);

const struct device *power_button_dev =
	DEVICE_DT_GET(DT_NODELABEL(cros_power_button));

static int debounced_power_pressed; /* Debounced power button state */
static int simulate_power_pressed;

static int power_button_signal_asserted(const struct device *dev)
{
	struct gpio_debounce_config cfg;

	gpio_debounce_get_config(dev, &cfg);

	const int is_active_high =
		((cfg.spec.dt_flags & GPIO_ACTIVE_LOW) ? 0 : 1);

	return gpio_debounce_get_pin_raw(dev) == is_active_high;
}

static int raw_power_button_pressed(const struct device *dev)
{
	if (simulate_power_pressed)
		return 1;

	/*
	 * Always indicate power button released if the lid is closed.
	 * This prevents waking the system if the device is squashed enough to
	 * press the power button through the closed lid.
	 */
	if (!IS_ENABLED(CONFIG_POWER_BUTTON_IGNORE_LID) && !lid_is_open())
		return 0;

	return power_button_signal_asserted(dev);
}

int power_button_is_pressed(void)
{
	return debounced_power_pressed;
}

int power_button_wait_for_release(int timeout_us)
{
	int debounce_us;
	bool released;

	gpio_debounce_get_debounce_us(power_button_dev, &debounce_us);

	LOG_INF("%s - wait for release", power_button_dev->name);
	released = WAIT_FOR(!(power_button_is_pressed()), timeout_us,
			    task_wait_event(MIN(timeout_us, debounce_us)));

	if (released) {
		LOG_INF("%s released in time", power_button_dev->name);
		return 0;
	}

	LOG_INF("%s not released in time", power_button_dev->name);
	return -ETIMEDOUT;
}

/**
 * Handle debounced power button changing state.
 */
static void power_button_changed(void)
{
	const int new_pressed = raw_power_button_pressed(power_button_dev);

	debounced_power_pressed = new_pressed;

	LOG_INF("%s %s", power_button_dev->name,
		new_pressed ? "pressed" : "released");

	/* Call hooks */
	hook_notify(HOOK_POWER_BUTTON_CHANGE);

	/* Notify host if power button has been pressed */
	if (new_pressed)
		host_set_single_event(EC_HOST_EVENT_POWER_BUTTON);
}

void power_button_interrupt(const struct device *dev,
			    struct gpio_callback *cbdata, uint32_t pins)
{
	power_button_changed();
}

void power_button_simulate_press(unsigned int duration)
{
	LOG_INF("Simulating %d ms %s press.\n", duration,
		power_button_dev->name);
	simulate_power_pressed = 1;
	power_button_changed();

	if (duration > 0)
		k_sleep(K_MSEC(duration));

	LOG_INF("Simulating %s release.\n", power_button_dev->name);
	simulate_power_pressed = 0;
	power_button_changed();
}

/**
 * Handle power button initialization.
 */
static int power_button_init(const struct device *dev)
{
	struct gpio_debounce_data *data =
		(struct gpio_debounce_data *)dev->data;

	data->dev = dev;
	data->pin_state = -1;
	data->is_stable = 0;

	if (raw_power_button_pressed(dev))
		debounced_power_pressed = 1;

	/* Enable interrupts, now that we've initialized */
	gpio_debounce_enable_interrupt(dev, power_button_interrupt);

	return 0;
}

static const struct gpio_debounce_api power_button_api = {
	.get_config = gpio_debounce_common_get_cfg,
	.get_debounce_us = gpio_debounce_common_get_debounce_us,
	.enable_interrupt = gpio_debounce_common_enable_interrupt,
	.disable_interrupt = gpio_debounce_common_disable_interrupt,
	.get_pin = gpio_debounce_common_get_pin,
	.get_pin_raw = gpio_debounce_common_get_pin_raw,
};

#define POWER_BUTTON_INIT(i)                                                  \
	static const struct gpio_debounce_config power_button_cfg_##i =       \
		GPIO_DEBOUNCE_CFG_DEF(i);                                     \
	static struct gpio_debounce_data power_button_data_##i;               \
	DEVICE_DT_INST_DEFINE(i, &power_button_init, NULL,                    \
			      &power_button_data_##i, &power_button_cfg_##i,  \
			      POST_KERNEL,                                    \
			      CONFIG_PLATFORM_EC_GPIO_DEBOUNCE_INIT_PRIORITY, \
			      &power_button_api);

DT_INST_FOREACH_STATUS_OKAY(POWER_BUTTON_INIT)

/*****************************************************************************/
/* Console commands */

static int command_powerbtn(const struct shell *shell, size_t argc, char **argv)
{
	int ms = 200; /* Press duration in ms */
	char *e;

	if (argc > 1) {
		ms = strtoi(argv[1], &e, 0);
		if (*e || ms < 0)
			return EC_ERROR_PARAM1;
	}

	power_button_simulate_press(ms);
	return EC_SUCCESS;
}

SHELL_CMD_ARG_REGISTER(powerbtn, NULL,
		       "Simulate power button press for \'n\' msec",
		       command_powerbtn, 1, 1);
