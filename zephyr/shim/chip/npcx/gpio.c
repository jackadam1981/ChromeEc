/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>

#include "console.h"
#include "gpio/gpio.h"
#include "soc_gpio.h"
#include "util.h"

LOG_MODULE_REGISTER(shim_cros_gpio, LOG_LEVEL_ERR);

static const struct unused_pin_config unused_pin_configs[] = {
	UNUSED_GPIO_CONFIG_LIST
};

int gpio_config_unused_pins(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(unused_pin_configs); ++i) {
		int rv;
		int flags;
		const struct device *dev =
			device_get_binding(unused_pin_configs[i].dev_name);

		if (dev == NULL) {
			LOG_ERR("Not found (%s)",
				unused_pin_configs[i].dev_name);
			return -ENOTSUP;
		}

		/*
		 * Set the default setting for the floating IOs. The floating
		 * IOs cause the leakage current. Set unused pins as input with
		 * internal PU to prevent extra power consumption.
		 */
		if (unused_pin_configs[i].flags == 0)
			flags = GPIO_INPUT | GPIO_PULL_UP;
		else
			flags = unused_pin_configs[i].flags;

		rv = gpio_pin_configure(dev, unused_pin_configs[i].pin, flags);

		if (rv < 0) {
			LOG_ERR("Config failed %s-%d (%d)",
				unused_pin_configs[i].dev_name,
				unused_pin_configs[i].pin, rv);
			return rv;
		}
	}

	return 0;
}

#ifdef CONFIG_PLATFORM_EC_CONSOLE_CMD_GPIODBG
/*
 * IO information about each GPIO that is configured in the `named_gpios` and
 *` unused_pins` device tree nodes.
 */
struct npcx_io_info {
	/* A npcx gpio port device */
	const struct device *dev;
	/* A npcx gpio port number */
	int port;
	/* Bit number of pin within a npcx gpio port */
	gpio_pin_t pin;
	/* Config flags of npcx gpio pin */
	gpio_flags_t flags;
	/* GPIO net name */
	const char *name;
	/* Enable flag of npcx gpio input buffer */
	bool enable;
};

#define NAMED_GPIO_INFO(node)						\
	{								\
		.dev = DEVICE_DT_GET(DT_GPIO_CTLR(node, gpios)),	\
		.port = DT_PROP(DT_GPIO_CTLR(node, gpios), index),	\
		.pin = DT_GPIO_PIN(node, gpios),			\
		.flags = DT_GPIO_FLAGS(node, gpios),			\
		.name = DT_NODE_FULL_NAME(node),			\
		.enable = true,						\
	},

#define UNUSED_GPIO_INFO(node, prop, idx)					\
	{									\
		.dev = DEVICE_DT_GET(DT_GPIO_CTLR_BY_IDX(node, prop, idx)),	\
		.port = DT_PROP(DT_GPIO_CTLR_BY_IDX(node, prop, idx), index),	\
		.pin = DT_GPIO_PIN_BY_IDX(node, prop, idx),			\
		.flags =  DT_GPIO_FLAGS_BY_IDX(node, prop, idx),		\
		.name = "unused pin",						\
		.enable = true,							\
	},

static struct npcx_io_info gpio_info[] = {
#if DT_NODE_EXISTS(DT_PATH(named_gpios))
	DT_FOREACH_CHILD(DT_PATH(named_gpios), NAMED_GPIO_INFO)
#endif
#if DT_NODE_EXISTS(DT_PATH(unused_pins))
	DT_FOREACH_PROP_ELEM(DT_PATH(unused_pins), unused_gpios, UNUSED_GPIO_INFO)
#endif
};

/*
 * Command used to turn on/off input buffer of gpios to investigate power
 * consumption
 */
static int command_gpio_debug(int argc, char **argv)
{
	if (argc == 2) {
		/* List all GPIOs in 'named-gpios' node */
		if (!strcasecmp(argv[1], "list")) {
			ccprintf("NO |ON| GPIO |  Name\n");
			ccprintf("---+--+------+----------\n");

			for (int i = 0; i < ARRAY_SIZE(gpio_info); i++) {
				ccprintf("%02d |%s |", i,
					gpio_info[i].enable ? "*" : " ");
				ccprintf(" io%x%x | ",
					gpio_info[i].port, gpio_info[i].pin);
				ccprintf("%s\n",  gpio_info[i].name);
			}
			return EC_SUCCESS;
		}
	}
	else if (argc == 3) {
		int enable;
		char *e;
		int num = strtoi(argv[1], &e, 0);

		if (*e || num < 0 || num >=  ARRAY_SIZE(gpio_info))
			return EC_ERROR_PARAM1;

		if (parse_bool(argv[2], &enable)) {
			if (enable) {
				gpio_info[num].enable = true;
				npcx_gpio_enable_io_pads(gpio_info[num].dev,
							gpio_info[num].pin);
			} else {
				gpio_info[num].enable = false;
				npcx_gpio_disable_io_pads(gpio_info[num].dev,
							gpio_info[num].pin);
			}
		}
		else
			return EC_ERROR_PARAM2;

		return EC_SUCCESS;
	}
	return EC_ERROR_INVAL;
}
DECLARE_CONSOLE_COMMAND(gpiodbg, command_gpio_debug,
		"list/<num> on|off",
		"Turn on/off GPIO buffer to investigate power consumption");
#endif /* CONFIG_PLATFORM_EC_CONSOLE_CMD_GPIODBG */
