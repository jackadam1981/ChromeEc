/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef __REQUIRE_ZEPHYR_GPIOS__
#undef __REQUIRE_ZEPHYR_GPIOS__
#endif
#include "cros_version.h"
#include "gpio.h"
#include "gpio/gpio.h"
#include "ioexpander.h"
#include "system.h"

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(gpio_shim, LOG_LEVEL_ERR);

/*
 * Static information about each GPIO that is configured in the named_gpios
 * device tree node.
 */
struct gpio_config {
	/* Access structure for lookup */
	struct gpio_dt_spec spec;
	/* GPIO net name */
	const char *name;
	/* From DTS, excludes interrupts flags */
	gpio_flags_t init_flags;
	/* From DTS, skips initialisation */
	bool no_auto_init;

#ifdef CONFIG_GPIO_HOGS
	/* Legacy GPIO signal enumeration value */
	enum gpio_signal gpio_signal;
#endif
};

/*
 * Initialise a gpio_dt_spec structure.
 * Normally the standard macro (GPIO_DT_SPEC_GET) could be used, but
 * the flags stored in our device tree config are the full 32 bit flags,
 * whereas the standard macros assume that only 8 bits of initial flags
 * will be needed.
 */
#define OUR_DT_SPEC(id)                                          \
	{                                                        \
		.port = DEVICE_DT_GET(DT_GPIO_CTLR(id, gpios)),  \
		.pin = DT_GPIO_PIN(id, gpios),                   \
		.dt_flags = 0xFFFF & (DT_GPIO_FLAGS(id, gpios)), \
	}

#define GPIO_CONFIG(id)                                    \
	{                                                  \
		.spec = OUR_DT_SPEC(id),                   \
		.name = DT_NODE_FULL_NAME(id),             \
		.init_flags = DT_GPIO_FLAGS(id, gpios),    \
		.no_auto_init = DT_PROP(id, no_auto_init), \
	},
#define GPIO_IMPL_CONFIG(id) \
	COND_CODE_1(DT_NODE_HAS_PROP(id, gpios), (GPIO_CONFIG(id)), ())

static const struct gpio_config configs[] = {
#if DT_NODE_EXISTS(NAMED_GPIOS_NODE)
	DT_FOREACH_CHILD(NAMED_GPIOS_NODE, GPIO_IMPL_CONFIG)
#endif
};

#undef GPIO_IMPL_CONFIG
#undef GPIO_CONFIG
#undef OUR_DT_SPEC

#ifdef CONFIG_GPIO_HOGS

/* Expands to 1 if node_id is a GPIO controller, 0 otherwise */
#define GPIO_HOGS_NODE_IS_GPIO_CTLR(node_id) \
	DT_PROP_OR(node_id, gpio_controller, 0)

/* Expands to to 1 if node_id is a GPIO hog, empty otherwise */
#define GPIO_HOGS_NODE_IS_GPIO_HOG(node_id) \
	IF_ENABLED(DT_PROP_OR(node_id, gpio_hog, 0), 1)

/* Expands to 1 if GPIO controller node_id has GPIO hog children, 0 otherwise */
#define GPIO_HOGS_GPIO_CTLR_HAS_HOGS(node_id)                      \
	COND_CODE_0(IS_EMPTY(DT_FOREACH_CHILD_STATUS_OKAY(         \
			    node_id, GPIO_HOGS_NODE_IS_GPIO_HOG)), \
		    (1), (0))

#define GPIO_HOG_LINE_NAME_TOKEN_BY_IDX(hog_node_id, idx)                    \
	COND_CODE_1(                                                         \
		DT_PROP_HAS_IDX(hog_node_id, line_names, idx),                \
		(DT_STRING_UPPER_TOKEN_BY_IDX(hog_node_id, line_names, idx)), \
		(GPIO_LIMIT))

/* TODO: handle multiple GPIOs per hog... */
#define GPIO_HOGS_SHIM_INIT_HOG_BY_IDX(idx, hog_node_id, gpio_ctrl_id) \
	{                                                              \
		.spec = { \
			.port = DEVICE_DT_GET(gpio_ctrl_id), \
			.pin = DT_GPIO_HOG_PIN_BY_IDX(hog_node_id, idx), \
			.dt_flags = DT_GPIO_HOG_FLAGS_BY_IDX(hog_node_id, idx),\
			}, \
		.init_flags = DT_GPIO_HOG_FLAGS_BY_IDX(hog_node_id, idx) | 	\
			      COND_CODE_1(DT_PROP(hog_node_id, input), 		\
					  (GPIO_INPUT),				\
					  (COND_CODE_1(DT_PROP(hog_node_id, output_low),		\
						 (GPIO_OUTPUT_INACTIVE),			\
						 (COND_CODE_1(DT_PROP(hog_node_id, output_high),\
							     (GPIO_OUTPUT_ACTIVE), (0)))))),	\
		.gpio_signal = GPIO_HOG_LINE_NAME_TOKEN_BY_IDX(hog_node_id, idx),               \
	}

/* Called for each enabled GPIO hogs child */
#define GPIO_HOGS_SHIM_INIT_HOGS(hog_node_id, gpio_ctrl_id)                    \
	LISTIFY(DT_NUM_GPIO_HOGS(hog_node_id), GPIO_HOGS_SHIM_INIT_HOG_BY_IDX, \
		(, ), hog_node_id, gpio_ctrl_id),

/* Called on all children of GPIO controller  */
#define GPIO_HOGS_SHIM_COND_INIT_HOGS(hog_node_id, gpio_ctrl_id)       \
	COND_CODE_0(IS_EMPTY(GPIO_HOGS_NODE_IS_GPIO_HOG(hog_node_id)), \
		    (GPIO_HOGS_SHIM_INIT_HOGS(hog_node_id, gpio_ctrl_id)), ())

/* Called on each GPIO controller that has a hogs child node */
#define GPIO_HOGS_SHIM_INIT_GPIO_CTLR(node_id) \
	DT_FOREACH_CHILD_STATUS_OKAY_VARGS(    \
		node_id, GPIO_HOGS_SHIM_COND_INIT_HOGS, node_id)

/* Called on each GPIO controller */
#define GPIO_HOGS_SHIM_COND_INIT_GPIO_CTRL(node_id)       \
	IF_ENABLED(GPIO_HOGS_GPIO_CTLR_HAS_HOGS(node_id), \
		   (GPIO_HOGS_SHIM_INIT_GPIO_CTLR(node_id)))

/* Called on every node in the tree */
#define GPIO_HOGS_SHIM_COND_INIT(node_id)                \
	IF_ENABLED(GPIO_HOGS_NODE_IS_GPIO_CTLR(node_id), \
		   (GPIO_HOGS_SHIM_COND_INIT_GPIO_CTRL(node_id)))

static const struct gpio_config hogs_config[] = { DT_FOREACH_STATUS_OKAY_NODE(
	GPIO_HOGS_SHIM_COND_INIT) };

#endif /* CONFIG_GPIO_HOGS */

/*
 * Generate a pointer for each GPIO, pointing to the gpio_dt_spec entry
 * in the table. These are named after the GPIO generated signal name,
 * so they can be used directly in Zephyr GPIO API calls.
 *
 * Potentially, instead of generating a pointer, the macro could
 * point directly into the table by exposing the gpio_config struct.
 */

#define GPIO_PTRS(id)                                                    \
	const struct gpio_dt_spec *const GPIO_DT_NAME(GPIO_SIGNAL(id)) = \
		&configs[GPIO_SIGNAL(id)].spec;

#if DT_NODE_EXISTS(NAMED_GPIOS_NODE)
DT_FOREACH_CHILD(NAMED_GPIOS_NODE, GPIO_PTRS)
#endif

int gpio_is_implemented(enum gpio_signal signal)
{
	return signal >= 0 && signal < ARRAY_SIZE(configs);
}

int gpio_get_level(enum gpio_signal signal)
{
	if (!gpio_is_implemented(signal))
		return 0;

	const int l = gpio_pin_get_raw(configs[signal].spec.port,
				       configs[signal].spec.pin);

	if (l < 0) {
		LOG_ERR("Cannot read %s (%d)", configs[signal].name, l);
		return 0;
	}
	return l;
}

int gpio_get_ternary(enum gpio_signal signal)
{
	int pd, pu;
	int flags = gpio_get_default_flags(signal);

	/* Read GPIO with internal pull-down */
	gpio_set_flags(signal, GPIO_INPUT | GPIO_PULL_DOWN);
	pd = gpio_get_level(signal);
	udelay(100);

	/* Read GPIO with internal pull-up */
	gpio_set_flags(signal, GPIO_INPUT | GPIO_PULL_UP);
	pu = gpio_get_level(signal);
	udelay(100);

	/* Reset GPIO flags */
	gpio_set_flags(signal, flags);

	/* Check PU and PD readings to determine tristate */
	return pu && !pd ? 2 : pd;
}

const char *gpio_get_name(enum gpio_signal signal)
{
	if (!gpio_is_implemented(signal))
		return "UNIMPLEMENTED";

	return configs[signal].name;
}

void gpio_set_level(enum gpio_signal signal, int value)
{
	if (!gpio_is_implemented(signal))
		return;

	int rv = gpio_pin_set_raw(configs[signal].spec.port,
				  configs[signal].spec.pin, value);

	if (rv < 0) {
		LOG_ERR("Cannot write %s (%d)", configs[signal].name, rv);
	}
}

void gpio_set_level_verbose(enum console_channel channel,
			    enum gpio_signal signal, int value)
{
	cprints(channel, "Set %s: %d", gpio_get_name(signal), value);
	gpio_set_level(signal, value);
}

void gpio_or_ioex_set_level(int signal, int value)
{
	gpio_set_level(signal, value);
}

int gpio_or_ioex_get_level(int signal, int *value)
{
	*value = gpio_get_level(signal);
	return EC_SUCCESS;
}

/* Don't define any 1.8V bit if not supported. */
#ifndef GPIO_VOLTAGE_1P8
#define GPIO_VOLTAGE_1P8 0
#endif

/* GPIO flags which are the same in Zephyr and this codebase */
#define GPIO_CONVERSION_SAME_BITS                                             \
	(GPIO_OPEN_DRAIN | GPIO_PULL_UP | GPIO_PULL_DOWN | GPIO_VOLTAGE_1P8 | \
	 GPIO_INPUT | GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW |                    \
	 GPIO_OUTPUT_INIT_HIGH)

#define FLAGS_HANDLED_FROM_ZEPHYR                                      \
	(GPIO_CONVERSION_SAME_BITS | GPIO_INT_ENABLE | GPIO_INT_EDGE | \
	 GPIO_INT_HIGH_1 | GPIO_INT_LOW_0)

#define FLAGS_HANDLED_TO_ZEPHYR                                               \
	(GPIO_CONVERSION_SAME_BITS | GPIO_INT_F_RISING | GPIO_INT_F_FALLING | \
	 GPIO_INT_F_LOW | GPIO_INT_F_HIGH)

int convert_from_zephyr_flags(const gpio_flags_t zephyr)
{
	/* Start out with the bits that are the same. */
	int ec_flags = zephyr & GPIO_CONVERSION_SAME_BITS;
	gpio_flags_t unhandled_flags = zephyr & (~FLAGS_HANDLED_FROM_ZEPHYR);

	/* TODO(b/173789980): handle conversion of more bits? */
	if (unhandled_flags) {
		LOG_WRN("Unhandled GPIO bits in zephyr->ec conversion: 0x%08X",
			unhandled_flags);
	}

	if (zephyr & GPIO_INT_ENABLE) {
		if (zephyr & GPIO_INT_EDGE) {
			if (zephyr & GPIO_INT_HIGH_1)
				ec_flags |= GPIO_INT_F_RISING;
			if (zephyr & GPIO_INT_LOW_0)
				ec_flags |= GPIO_INT_F_FALLING;
		} else {
			if (zephyr & GPIO_INT_LOW_0)
				ec_flags |= GPIO_INT_F_LOW;
			if (zephyr & GPIO_INT_HIGH_1)
				ec_flags |= GPIO_INT_F_HIGH;
		}
	}

	return ec_flags;
}

gpio_flags_t convert_to_zephyr_flags(int ec_flags)
{
	/* Start out with the bits that are the same. */
	gpio_flags_t zephyr_flags = ec_flags & GPIO_CONVERSION_SAME_BITS;
	int unhandled_flags = ec_flags & (~FLAGS_HANDLED_TO_ZEPHYR);

	/* TODO(b/173789980): handle conversion of more bits? */
	if (unhandled_flags) {
		LOG_WRN("Unhandled GPIO bits in ec->zephyr conversion: 0x%08X",
			unhandled_flags);
	}

	if (ec_flags & GPIO_INT_F_RISING)
		zephyr_flags |= GPIO_INT_ENABLE | GPIO_INT_EDGE |
				GPIO_INT_HIGH_1;
	if (ec_flags & GPIO_INT_F_FALLING)
		zephyr_flags |= GPIO_INT_ENABLE | GPIO_INT_EDGE |
				GPIO_INT_LOW_0;
	if (ec_flags & GPIO_INT_F_LOW)
		zephyr_flags |= GPIO_INT_ENABLE | GPIO_INT_LOW_0;
	if (ec_flags & GPIO_INT_F_HIGH)
		zephyr_flags |= GPIO_INT_ENABLE | GPIO_INT_HIGH_1;

	return zephyr_flags;
}

int gpio_get_flags(enum gpio_signal signal)
{
	return gpio_get_default_flags(signal);
}

int gpio_get_default_flags(enum gpio_signal signal)
{
	if (!gpio_is_implemented(signal))
		return 0;

	return convert_from_zephyr_flags(configs[signal].init_flags);
}

const struct gpio_dt_spec *gpio_get_dt_spec(enum gpio_signal signal)
{
	if (!gpio_is_implemented(signal))
		return 0;
	return &configs[signal].spec;
}

/* Allow access to this function in tests so we can run it multiple times
 * without having to create a new binary for each run.
 */
test_export_static int init_gpios(const struct device *unused)
{
	gpio_flags_t flags;
	bool is_sys_jumped = system_jumped_to_this_image();

	ARG_UNUSED(unused);

#if 0
	if (is_sys_jumped) {
		gpio_hogs_configure(GPIO_OUTPUT_INIT_LOW | GPIO_OUTPUT_INIT_HIGH);
	} else {
		gpio_hogs_configure(0);
	}
#endif

	for (size_t i = 0; i < ARRAY_SIZE(configs); ++i) {
		int rv;

		/* Skip GPIOs that have set no-auto-init. */
		if (configs[i].no_auto_init)
			continue;

		if (!device_is_ready(configs[i].spec.port))
			LOG_ERR("Not found (%s)", configs[i].name);

		/*
		 * The configs[i].init_flags variable is read-only, so the
		 * following assignment is needed because the flags need
		 * adjusting on a warm reboot.
		 */
		flags = configs[i].init_flags;

		/*
		 * For warm boot, do not set the output state.
		 */
		if (is_sys_jumped && (flags & GPIO_OUTPUT)) {
			flags &=
				~(GPIO_OUTPUT_INIT_LOW | GPIO_OUTPUT_INIT_HIGH);
		}

		rv = gpio_pin_configure_dt(&configs[i].spec, flags);
		if (rv < 0) {
			LOG_ERR("Config failed %s (%d)", configs[i].name, rv);
		}
	}

	/* Configure unused pins in chip driver for better power consumption */
	if (gpio_config_unused_pins) {
		int rv;

		rv = gpio_config_unused_pins();
		if (rv < 0) {
			return rv;
		}
	}

	return 0;
}
#if CONFIG_PLATFORM_EC_GPIO_INIT_PRIORITY <= CONFIG_KERNEL_INIT_PRIORITY_DEFAULT
#error "GPIOs must initialize after the kernel default initialization"
#endif
SYS_INIT(init_gpios, POST_KERNEL, CONFIG_PLATFORM_EC_GPIO_INIT_PRIORITY);

void gpio_reset(enum gpio_signal signal)
{
	if (!gpio_is_implemented(signal))
		return;

	gpio_pin_configure_dt(&configs[signal].spec,
			      configs[signal].init_flags);
}

int gpio_save_port_config(const struct device *port, gpio_flags_t *flags,
			  int buff_size)
{
	int state_offset = 0;

	for (size_t i = 0; i < ARRAY_SIZE(configs); ++i) {
		if (state_offset >= buff_size) {
			LOG_ERR("%s buffer is too small", __func__);
			return EC_ERROR_UNKNOWN;
		}

		if (port == configs[i].spec.port) {
			gpio_pin_get_config_dt(&configs[i].spec,
					       &flags[state_offset++]);
		}
	}

	return EC_SUCCESS;
}

int gpio_restore_port_config(const struct device *port, gpio_flags_t *flags,
			     int buff_size)
{
	int state_offset = 0;

	for (size_t i = 0; i < ARRAY_SIZE(configs); ++i) {
		if (state_offset >= buff_size) {
			LOG_ERR("%s buffer is too small", __func__);
			return EC_ERROR_UNKNOWN;
		}

		if (port == configs[i].spec.port) {
			gpio_pin_configure_dt(&configs[i].spec,
					      flags[state_offset++]);
		}
	}

	return EC_SUCCESS;
}

void gpio_reset_port(const struct device *port)
{
	for (size_t i = 0; i < ARRAY_SIZE(configs); ++i) {
		if (port == configs[i].spec.port) {
			gpio_pin_configure_dt(&configs[i].spec,
					      configs[i].init_flags);
		}
	}
}

void gpio_set_flags(enum gpio_signal signal, int flags)
{
	if (!gpio_is_implemented(signal))
		return;

	gpio_pin_configure_dt(&configs[signal].spec,
			      convert_to_zephyr_flags(flags));
}

void gpio_set_flags_by_mask(uint32_t port, uint32_t mask, uint32_t flags)
{
	const gpio_flags_t zephyr_flags = convert_to_zephyr_flags(flags);

	/* Using __builtin_ctz here will guarantee that this loop is as
	 * performant as the underlying architecture allows it to be.
	 */
	while (mask != 0) {
		int pin = __builtin_ctz(mask);

		gpio_configure_port_pin(port, pin, zephyr_flags);
		mask &= ~BIT(pin);
	}
}

int signal_is_gpio(int signal)
{
	return true;
}

#ifdef CONFIG_PLATFORM_EC_GPIO_HOGS_CHECK

#define GPIO_OUTPUT_INIT_MASK \
	(GPIO_OUTPUT_INIT_LOW | GPIO_OUTPUT_INIT_HIGH | GPIO_OUTPUT_INIT_LOGICAL)

static bool compare_named_gpio_to_gpio_hog(const struct shell *shell,
					   const struct gpio_config *named_gpio,
					   const struct gpio_config *gpio_hog)
{
	gpio_flags_t hog_flags;

	if (memcmp(&gpio_hog->spec, &named_gpio->spec,
		   sizeof(struct gpio_dt_spec)) != 0) {
		goto match_failed;
	}

	/*
	 * GPIO hogs uses GPIO_OUTPUT_INIT_LOGICAL but this is not used by
	 * named-gpios. Convert the GPIO hogs output level flags to account
	 * for active high/active low setting.
	 */
	hog_flags = gpio_hog->init_flags;
	if (((hog_flags & GPIO_OUTPUT_INIT_LOGICAL) != 0)
	    && ((hog_flags & (GPIO_OUTPUT_INIT_LOW | GPIO_OUTPUT_INIT_HIGH)) != 0)
	    && ((hog_flags & GPIO_ACTIVE_LOW) != 0)) {
		hog_flags ^= GPIO_OUTPUT_INIT_LOW | GPIO_OUTPUT_INIT_HIGH;
	}
	hog_flags &= ~GPIO_OUTPUT_INIT_LOGICAL;

	if (named_gpio->init_flags != hog_flags) {
		goto match_failed;
	}


	return true;

match_failed:
	shell_fprintf(
		shell, SHELL_INFO,
		"Mismatch: GPIO %s, port %p, pin %d, dt_flags 0x%04x, init_flags 0x%08x\n",
		named_gpio->name, named_gpio->spec.port, named_gpio->spec.pin,
		named_gpio->spec.dt_flags, named_gpio->init_flags);
	shell_fprintf(
		shell, SHELL_INFO,
		"          Hog      port %p, pin %d, dt_flags 0x%04x, init_flags 0x%08x\n",
		gpio_hog->spec.port, gpio_hog->spec.pin,
		gpio_hog->spec.dt_flags, gpio_hog->init_flags);

	return false;
}

static bool check_gpio_hog(const struct shell *shell, const struct gpio_config *gpio_hog)
{
	for (int i = 0; i < ARRAY_SIZE(configs); i++) {
		if (configs[i].spec.port == gpio_hog->spec.port &&
		    configs[i].spec.pin == gpio_hog->spec.pin) {
			return compare_named_gpio_to_gpio_hog(shell, &configs[i], gpio_hog);
		}
	}

	shell_fprintf(shell, SHELL_INFO, "GPIO hog port %p, pin %d, not found in named-gpios\n",
		gpio_hog->spec.port, gpio_hog->spec.pin);

	return false;
}

static int gpio_hogs_check(const struct shell *shell, size_t argc, char **argv)
{
	int named_hogs_checked = 0;
	int unnamed_hogs_checked = 0;
	enum gpio_signal signal;
	bool failed = false;

	shell_fprintf(shell, SHELL_INFO,
		      "GPIO Hogs cross check, GPIOs %d, Hogs %d\n",
		      ARRAY_SIZE(configs), ARRAY_SIZE(hogs_config));

	for (int i = 0; i < ARRAY_SIZE(hogs_config); i++) {
		if (hogs_config[i].gpio_signal != GPIO_LIMIT) {
			named_hogs_checked++;

			signal = hogs_config[i].gpio_signal;

			if (!compare_named_gpio_to_gpio_hog(
				    shell, &configs[signal], &hogs_config[i])) {
				failed = true;
			}
		} else {
			/*
			 * Search all the configs[i] for a matching GPIO port and pin
			 */
			unnamed_hogs_checked++;
			if (!check_gpio_hog(shell, &hogs_config[i])) {
				failed = true;
			}
		}
	}

	shell_fprintf(shell, SHELL_INFO, "Hogs checked: named %d, unnamed %d\n",
			named_hogs_checked, unnamed_hogs_checked);

	if (failed) {
		shell_fprintf(shell, SHELL_INFO,
			     "FAILED: one more more GPIO hogs mismatched named-gpios\n");
	} else if (named_hogs_checked + unnamed_hogs_checked != ARRAY_SIZE(configs)) {
		shell_fprintf(shell, SHELL_INFO,
			     "FAILED: number of hogs (named %d, unnamed %d) doesn't match named-gpios %d\n",
			     named_hogs_checked, unnamed_hogs_checked, ARRAY_SIZE(configs));
	} else {
		shell_fprintf(shell, SHELL_INFO, "PASSED: all GPIO hogs verified\n");
	}

	return 0;
}

SHELL_CMD_REGISTER(gpiohogs, NULL, NULL, gpio_hogs_check);

#endif /* CONFIG_PLATFORM_EC_GPIO_HOGS_CHECK */
