/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "gpio.h"
#include "host_command.h"
#include "keyboard_config.h"
#include "keyboard_raw.h"
#include "keyboard_scan.h"
#include "system.h"

/* Run keyboard factory testing, scan out KSO/KSI if any shorted. */
static int keyboard_factory_test_scan(void)
{
	int i, j, flags;
	uint16_t shorted = 0;
	int port, id;

	/* Disable keyboard scan while testing */
	keyboard_scan_enable(0, KB_SCAN_DISABLE_LID_CLOSED);
	flags = gpio_get_default_flags(GPIO_KBD_KSO2);

	if (IS_ENABLED(CONFIG_ZEPHYR))
		/* set all KSI/KSO pins to GPIO_ALT_FUNC_NONE */
		keybaord_raw_config_alt(0);

	/* Set all of KSO/KSI pins to internal pull-up and input */
	for (i = 0; i < keyboard_factory_scan_pins_used; i++) {
		if (keyboard_factory_scan_pins[i][0] < 0)
			continue;

		port = keyboard_factory_scan_pins[i][0];
		id = keyboard_factory_scan_pins[i][1];

		if (!IS_ENABLED(CONFIG_ZEPHYR))
			gpio_set_alternate_function(port, 1 << id,
						    GPIO_ALT_FUNC_NONE);
		gpio_set_flags_by_mask(port, 1 << id,
				       GPIO_INPUT | GPIO_PULL_UP);
	}

	/*
	 * Set start pin to output low, then check other pins
	 * going to low level, it indicate the two pins are shorted.
	 */
	for (i = 0; i < keyboard_factory_scan_pins_used; i++) {
		if (keyboard_factory_scan_pins[i][0] < 0)
			continue;

		port = keyboard_factory_scan_pins[i][0];
		id = keyboard_factory_scan_pins[i][1];

		gpio_set_flags_by_mask(port, 1 << id, GPIO_OUT_LOW);

		for (j = 0; j < keyboard_factory_scan_pins_used; j++) {
			if (keyboard_factory_scan_pins[j][0] < 0 || i == j)
				continue;

			if (keyboard_raw_is_input_low(
				    keyboard_factory_scan_pins[j][0],
				    keyboard_factory_scan_pins[j][1])) {
				shorted = i << 8 | j;
				goto done;
			}
		}
		gpio_set_flags_by_mask(port, 1 << id,
				       GPIO_INPUT | GPIO_PULL_UP);
	}
done:
	if (IS_ENABLED(CONFIG_ZEPHYR))
		keybaord_raw_config_alt(1);
	else
		gpio_config_module(MODULE_KEYBOARD_SCAN, 1);
	gpio_set_flags(GPIO_KBD_KSO2, flags);
	keyboard_scan_enable(1, KB_SCAN_DISABLE_LID_CLOSED);

	return shorted;
}

static enum ec_status keyboard_factory_test(struct host_cmd_handler_args *args)
{
	struct ec_response_keyboard_factory_test *r = args->response;

	/* Only available on unlocked systems */
	if (system_is_locked())
		return EC_RES_ACCESS_DENIED;

	if (keyboard_factory_scan_pins_used == 0)
		return EC_RES_INVALID_COMMAND;

	r->shorted = keyboard_factory_test_scan();

	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_KEYBOARD_FACTORY_TEST, keyboard_factory_test,
		     EC_VER_MASK(0));

static int command_kb_factorytest(int argc, const char **argv)
{
	int shorted;

	if (keyboard_factory_scan_pins_used == 0) {
		return EC_RES_INVALID_COMMAND;
	}

	shorted = keyboard_factory_test_scan();

	ccprintf("Keyboard factory test: shorted=%d\n", shorted);

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(kb_factorytest, command_kb_factorytest,
			"kb_factorytest", "Run the keyboard factory test");
