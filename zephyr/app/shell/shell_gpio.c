/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <shell/shell.h>
#include <shell/gpio_name.h>
#include <devicetree.h>
#include <string.h>
#include <drivers/gpio.h>
#include <logging/log.h>

/*
 * Try to determine the base node containing the GPIO nodes.
 */

#if DT_NODE_EXISTS(DT_PATH(soc))
#define GPIO_BASE_NODE DT_PATH(soc)
#elif DT_HAS_COMPAT_STATUS_OKAY(zephyr_posix)
#define GPIO_BASE_NODE DT_ROOT
#else
#error "Unable to determine node containing GPIO controllers"
#endif

/*
 * Holds the gpio-line-names data captured from DTS.
 */
struct gpio_names {
	const struct device *port;
	const char * const *names;
	uint8_t name_count;
};

/*
 * Create a string array containing the gpio-line-names strings,
 * and name it as the node ID.
 */
#define NAME_DEFN(id) \
	static const char * const id[] = DT_PROP(id, gpio_line_names)

/*
 * For SOC nodes that have the gpio-line-names property,
 * create the string array for the names.
 */
#define NAME_ENTRY(id)				\
	COND_CODE_1(DT_NODE_HAS_PROP(id, gpio_line_names), \
		(NAME_DEFN(id);),			   \
		())

DT_FOREACH_CHILD_STATUS_OKAY(GPIO_BASE_NODE, NAME_ENTRY)

/*
 * Init one entry for a GPIO controller.
 */
#define CONTR_ENTRY(id)				\
	{					\
	.port = DEVICE_DT_GET(id),		\
	.names = id,				\
	.name_count = DT_PROP_LEN(id, gpio_line_names), \
	},

/*
 * Conditionally create a gpio_names entry if the
 * node has the gpio-line-names property.
 */
#define GPIO_CONTR(id) \
	COND_CODE_1(DT_NODE_HAS_PROP(id, gpio_line_names), \
		(CONTR_ENTRY(id)),			    \
		())

/*
 * Table of gpio_names, one entry for each GPIO controller.
 */
static const struct gpio_names gpio_names[] = {
	DT_FOREACH_CHILD_STATUS_OKAY(GPIO_BASE_NODE, GPIO_CONTR)
};

/*
 * Array of last displayed values for GPIOs.
 */
static gpio_port_pins_t gpio_last_value[ARRAY_SIZE(gpio_names)];

/*
 * Search for a matching name.
 * Return the index of the controller, or -1 if not
 * found. Also store the pin number in the pointer passed.
 */
static int gpio_find_by_name(const char *name, gpio_pin_t *ppin)
{
	for (int i = 0; i < ARRAY_SIZE(gpio_names); i++) {
		const char * const *n = gpio_names[i].names;

		for (int j = 0; j < gpio_names[i].name_count; j++) {
			if (n[j] != NULL && strcmp(n[j], name) == 0) {
				*ppin = j;
				return i;
			}
		}
	}
	*ppin = 0;
	return -1;
}

/*
 * Print the details of this GPIO.
 */
static void gpio_print(const struct shell *shell, int index, gpio_pin_t pin)
{
	int value;
	char changed, polarity;
	uint32_t last = gpio_last_value[index];
	const struct gpio_names *gp = &gpio_names[index];
	struct gpio_driver_data *data;

	if (pin >= gp->name_count) {
		return;
	}
	data = (struct gpio_driver_data *)gp->port->data;
	/* Get current state of GPIO */
	value = gpio_pin_get_raw(gp->port, pin);
	if (value != ((last >> pin) & 1)) {
		changed = '*';
		/* Update remembered value */
		gpio_last_value[index] =
			(last & ~(1 << pin)) | (value << pin);
	} else {
		changed = ' ';
	}
	/* Get the polarity (active low/high) of the pin */
	polarity = (data->invert & (1 << pin)) ? 'L' : ' ';
	shell_print(shell, " %d%c %c %s", value, changed,
		    polarity, gp->names[pin]);
}

const char *gpio_pin_get_name(const struct device *port, gpio_pin_t pin)
{
	const char *name;

	/* Search table for matching port */
	for (int i = 0; i < ARRAY_SIZE(gpio_names); i++) {
		if (gpio_names[i].port == port) {
			if (pin >= gpio_names[i].name_count) {
				return NULL;
			}
			name = gpio_names[i].names[pin];
			/* Make sure it is not NULL or empty */
			if (name != NULL && *name != 0) {
				return name;
			}
			return NULL;
		}
	}
	return NULL;
}

int gpio_pin_by_name(const char *name,
		     const struct device **pport,
		     gpio_pin_t *ppin)
{
	int index = gpio_find_by_name(name, ppin);

	if (index < 0) {
		*pport = NULL;
		return -ENOENT;
	}
	*pport = gpio_names[index].port;
	return 0;
}

static int cmd_gpioget(const struct shell *shell,
		       size_t argc,
		       const char **argv)
{
	if (argc == 2) {
		/* Display a single GPIO */
		gpio_pin_t pin;
		int index = gpio_find_by_name(argv[1], &pin);

		if (index < 0) {
			shell_print(shell, "%s: Unknown GPIO", argv[1]);
			return 0;
		}
		gpio_print(shell, index, pin);
		return 0;
	}
	if (argc != 1) {
		shell_print(shell, "Arg error. Usage %s [ name ]", argv[0]);
		return 0;
	}
	/* Display all GPIOs */
	for (int i = 0; i < ARRAY_SIZE(gpio_names); i++) {
		for (int j = 0; j < gpio_names[i].name_count; j++) {
			const char *np = gpio_names[i].names[j];
			/* Don't attempt to print missing or empty names */
			if (np != NULL && *np != 0) {
				gpio_print(shell, i, j);
			}
		}
	}
	return 0;
}

static int cmd_gpioset(const struct shell *shell,
		       size_t argc,
		       const char **argv)
{
	int value, rv;
	const struct device *port;
	gpio_pin_t pin;

	if (argc != 3) {
		shell_print(shell, "Arg error. Usage %s name {0,1}", argv[0]);
		return 0;
	}
	if (strcmp(argv[2], "0") == 0) {
		value = 0;
	} else if (strcmp(argv[2], "1") == 0) {
		value = 1;
	} else {
		shell_print(shell, "Value must be 0 or 1");
		return 0;
	}
	if (gpio_pin_by_name(argv[1], &port, &pin) != 0) {
		shell_print(shell, "%s: not found", argv[1]);
		return 0;
	}
	/*
	 * There isn't a way of discovering whether the pin is
	 * configured as an input or output, so force it to be
	 * an output.
	 */
	rv = gpio_pin_configure(port, pin, GPIO_OUTPUT);
	if (rv) {
		shell_print(shell, "%s: configuration failed (err %d)",
			    argv[1], -rv);
		return 0;
	}
	rv = gpio_pin_set_raw(port, pin, value);
	if (rv) {
		shell_print(shell, "%s: pin set failed (err %d)",
			    argv[1], -rv);
		return 0;
	}
	return 0;
}

SHELL_CMD_REGISTER(gpioget, NULL, "GPIO get", cmd_gpioget);
SHELL_CMD_REGISTER(gpioset, NULL, "GPIO get", cmd_gpioset);
