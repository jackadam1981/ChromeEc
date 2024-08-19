/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_ec_keyboard_factory_test

#include <zephyr/device.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/init.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <host_command.h>
#include <keyboard_scan.h>
#include <system.h>

LOG_MODULE_REGISTER(keyboard_factory_test, LOG_LEVEL_INF);

PINCTRL_DT_DEFINE(DT_INST_PARENT(0));

static const struct pinctrl_dev_config *pcfg =
	PINCTRL_DT_DEV_CONFIG_GET(DT_INST_PARENT(0));

const struct gpio_dt_spec scan_gpios[32] = {
	GPIO_DT_SPEC_INST_GET_OR(0, pin1_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin2_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin3_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin4_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin5_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin6_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin7_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin8_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin9_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin10_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin11_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin12_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin13_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin14_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin15_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin16_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin17_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin18_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin19_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin21_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin22_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin23_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin24_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin25_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin26_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin27_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin28_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin29_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin30_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin31_gpios, { 0 }),
	GPIO_DT_SPEC_INST_GET_OR(0, pin32_gpios, { 0 }),
};

static int keyboard_factory_test_scan(void)
{
	uint16_t shorted = 0;
	int ret;

	/* Disable keyboard scan while testing */
	keyboard_scan_enable(0, KB_SCAN_DISABLE_LID_CLOSED);

	/* Give the keyboard driver some time to shut down. */
	k_sleep(K_MSEC(200));

	ret = pinctrl_apply_state(pcfg, PINCTRL_STATE_SLEEP);
	if (ret < 0) {
		LOG_ERR("pinctrl_apply_state failed: %d", ret);
		goto done;
	}

	/* Set all of KSO/KSI pins to internal pull-up and input */
	for (uint8_t i = 0; i < ARRAY_SIZE(scan_gpios); i++) {
		const struct gpio_dt_spec *gpio = &scan_gpios[i];

		if (gpio->port == NULL) {
			continue;
		}

		gpio_pin_configure_dt(gpio, GPIO_INPUT | GPIO_PULL_UP);
	}

	/*
	 * Set start pin to output low, then check other pins going to low
	 * level, it indicate the two pins are shorted.
	 */
	for (uint8_t i = 0; i < ARRAY_SIZE(scan_gpios); i++) {
		const struct gpio_dt_spec *gpio = &scan_gpios[i];

		if (gpio->port == NULL) {
			continue;
		}

		gpio_pin_configure_dt(gpio, GPIO_OUTPUT_INACTIVE);

		for (int j = 0; j < ARRAY_SIZE(scan_gpios); j++) {
			const struct gpio_dt_spec *gpio_in = &scan_gpios[j];

			if (gpio_in->port == NULL || i == j)
				continue;

			if (gpio_pin_get_dt(gpio_in) == 0) {
				shorted = i << 8 | j;
				goto done;
			}
		}

		gpio_pin_configure_dt(gpio, GPIO_INPUT);
	}

done:
	ret = pinctrl_apply_state(pcfg, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		LOG_ERR("pinctrl_apply_state failed: %d", ret);
		return -1;
	}

	keyboard_scan_enable(1, KB_SCAN_DISABLE_LID_CLOSED);

	return shorted;
}

static enum ec_status keyboard_factory_test(struct host_cmd_handler_args *args)
{
	struct ec_response_keyboard_factory_test *r = args->response;

	/* Only available on unlocked systems */
	if (system_is_locked())
		return EC_RES_ACCESS_DENIED;

	r->shorted = keyboard_factory_test_scan();

	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_KEYBOARD_FACTORY_TEST, keyboard_factory_test,
		     EC_VER_MASK(0));

static int command_kbfactorytest(int argc, const char **argv)
{
	uint16_t shorted;

	shorted = keyboard_factory_test_scan();

	ccprintf("Keyboard factory test: shorted=%04x (%d, %d)\n", shorted,
		 shorted & 0xff, shorted >> 8);

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(kbfactorytest, command_kbfactorytest, "kbfactorytest",
			"Run the keyboard factory test");
