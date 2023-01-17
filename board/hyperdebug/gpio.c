/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* HyperDebug board configuration */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "timer.h"
#include "util.h"

struct monitoring_buffer_t {
	/* Even indices are rising edges, odd indices are falling edges. */
	timestamp_t edges[16];
	volatile uint8_t head, tail;
	uint8_t overflow;
	int gpio_signal;
};

struct monitoring_buffer_t monitoring_slots[16];

#define EDGE_INDEX_MASK (ARRAY_SIZE(monitoring_slots[0].edges) - 1)

/*
 * Buffer size must be power of two in order for indexing using bitwise and
 * with EDGE_INDEX_MASK to work.
 */
BUILD_ASSERT((ARRAY_SIZE(monitoring_slots[0].edges) & EDGE_INDEX_MASK) == 0);

static void overflow(struct monitoring_buffer_t *slot) {
	slot->overflow = 1;
	gpio_disable_interrupt(slot->gpio_signal);
}

void gpio_edge(enum gpio_signal signal) {
	struct monitoring_buffer_t *slot =
		monitoring_slots + GPIO_MASK_TO_NUM(gpio_list[signal].mask);
	int current_level = gpio_get_level(signal);
	timestamp_t now = get_time();
	uint8_t tail = slot->tail, head = slot->head;
	if ((head & EDGE_INDEX_MASK) == (tail & EDGE_INDEX_MASK) && head != tail)
		return overflow(slot);
	slot->edges[head++ & EDGE_INDEX_MASK] = now;
	slot->head = head;
	if (!!current_level != !!(head & 1)) {
		if ((head & EDGE_INDEX_MASK) == (tail & EDGE_INDEX_MASK) && head != tail)
			return overflow(slot);
		slot->edges[head++ & EDGE_INDEX_MASK] = now;
		slot->head = head;
	}
		
	//cprints(CC_GPIO, "GPIO edge detected: %s:%d", gpio_list[signal].name, current_level);
}



static void board_gpio_init(void) {
	/* Mark every slot as unused. */
	for (int i = 0; i < ARRAY_SIZE(monitoring_slots); i++)
		monitoring_slots[i].gpio_signal = GPIO_COUNT;
	cprints(CC_GPIO, "GPIO edge detection ready %x %x", STM32_EXTI_RTSR, STM32_EXTI_FTSR);
	//gpio_enable_interrupt(GPIO_NUCLEO_LED3);
	//gpio_enable_interrupt(GPIO_CN10_16);
	//gpio_enable_interrupt(GPIO_CN10_31);
	//gpio_enable_interrupt(GPIO_NUCLEO_LED2);
	//gpio_enable_interrupt(GPIO_NUCLEO_LED1);
	//STM32_EXTI_FTSR = 0xFFFF;
	//STM32_EXTI_RTSR = 0xFFFF;

	//STM32_EXTI_SWIER = 0x00002000;
	//for (int i = 0; ARRAY_SIZE(monitoring_slot); i++)
	//	monitoring_slot[i] = -1;
}
DECLARE_HOOK(HOOK_INIT, board_gpio_init, HOOK_PRIO_DEFAULT);

/**
 * Find a GPIO signal by name.
 *
 * This is copied from gpio.c unfortunately, as it is static over there.
 *
 * @param name		Signal name to find
 *
 * @return the signal index, or GPIO_COUNT if no match.
 */
static enum gpio_signal find_signal_by_name(const char *name)
{
	int i;

	if (!name || !*name)
		return GPIO_COUNT;

	for (i = 0; i < GPIO_COUNT; i++)
		if (gpio_is_implemented(i) &&
		    !strcasecmp(name, gpio_get_name(i)))
			return i;

	return GPIO_COUNT;
}

/*
 * Set the mode of a GPIO pin: input/opendrain/pushpull/alternate.
 */
static int command_gpio_mode(int argc, const char **argv)
{
	int gpio;
	int flags;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	gpio = find_signal_by_name(argv[1]);
	if (gpio == GPIO_COUNT)
		return EC_ERROR_PARAM1;
	flags = gpio_get_flags(gpio);

	flags = flags & ~(GPIO_INPUT | GPIO_OUTPUT | GPIO_OPEN_DRAIN);
	if (strcasecmp(argv[2], "input") == 0)
		flags |= GPIO_INPUT;
	else if (strcasecmp(argv[2], "opendrain") == 0)
		flags |= GPIO_OUTPUT | GPIO_OPEN_DRAIN;
	else if (strcasecmp(argv[2], "pushpull") == 0)
		flags |= GPIO_OUTPUT;
	else if (strcasecmp(argv[2], "alternate") == 0)
		flags |= GPIO_ALTERNATE;
	else
		return EC_ERROR_PARAM2;

	/* Update GPIO flags. */
	gpio_set_flags(gpio, flags);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND_FLAGS(gpiomode, command_gpio_mode,
			      "name <input | opendrain | pushpull | alternate>",
			      "Set a GPIO mode", CMD_FLAG_RESTRICTED);

/*
 * Set the weak pulling of a GPIO pin: up/down/none.
 */
static int command_gpio_pull_mode(int argc, const char **argv)
{
	int gpio;
	int flags;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	gpio = find_signal_by_name(argv[1]);
	if (gpio == GPIO_COUNT)
		return EC_ERROR_PARAM1;
	flags = gpio_get_flags(gpio);

	flags = flags & ~(GPIO_PULL_UP | GPIO_PULL_DOWN);
	if (strcasecmp(argv[2], "none") == 0)
		;
	else if (strcasecmp(argv[2], "up") == 0)
		flags |= GPIO_PULL_UP;
	else if (strcasecmp(argv[2], "down") == 0)
		flags |= GPIO_PULL_DOWN;
	else
		return EC_ERROR_PARAM2;

	/* Update GPIO flags. */
	gpio_set_flags(gpio, flags);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND_FLAGS(gpiopullmode, command_gpio_pull_mode,
			      "name <none | up | down>",
			      "Set a GPIO weak pull mode", CMD_FLAG_RESTRICTED);


static int command_gpio_monitoring_start(int argc, const char **argv)
{
	int gpio;
	struct monitoring_buffer_t *slot;

	if (argc != 4)
		return EC_ERROR_PARAM_COUNT;

	gpio = find_signal_by_name(argv[3]);
	if (gpio == GPIO_COUNT)
		return EC_ERROR_PARAM3;

	slot = monitoring_slots + GPIO_MASK_TO_NUM(gpio_list[gpio].mask);
	if (slot->gpio_signal != GPIO_COUNT) {
		ccprintf("Error: Already monitoring %s\n", gpio_list[slot->gpio_signal].name);
		return EC_ERROR_PARAM3;
	}
	slot->gpio_signal = gpio;
	slot->head = slot->tail = gpio_get_level(gpio) ? 1 : 0;
	slot->overflow = 0;
	gpio_enable_interrupt(gpio);

	return EC_SUCCESS;
}

static int command_gpio_monitoring_read(int argc, const char **argv)
{
	int gpio;
	struct monitoring_buffer_t *slot;

	if (argc != 4)
		return EC_ERROR_PARAM_COUNT;

	gpio = find_signal_by_name(argv[3]);
	if (gpio == GPIO_COUNT)
		return EC_ERROR_PARAM3;

	slot = monitoring_slots + GPIO_MASK_TO_NUM(gpio_list[gpio].mask);
	if (slot->gpio_signal != gpio) {
		ccprintf("Error: Not monitoring %s\n", gpio_list[gpio].name);
		return EC_ERROR_PARAM3;
	}
	while (slot->tail != slot->head) {
		ccprintf("  %lld %s\n", slot->edges[slot->tail & EDGE_INDEX_MASK].val, (slot->tail & 1) ? "F" : "R");
		slot->tail++;
	}
	if (slot->overflow) {
		ccprintf("Error: Buffer overflow\n");
	}
	return EC_SUCCESS;
}

static int command_gpio_monitoring_stop(int argc, const char **argv)
{
	int gpio;
	struct monitoring_buffer_t *slot;

	if (argc != 4)
		return EC_ERROR_PARAM_COUNT;

	gpio = find_signal_by_name(argv[3]);
	if (gpio == GPIO_COUNT)
		return EC_ERROR_PARAM3;

	slot = monitoring_slots + GPIO_MASK_TO_NUM(gpio_list[gpio].mask);
	if (slot->gpio_signal != gpio) {
		ccprintf("Error: Not monitoring %s\n", gpio_list[gpio].name);
		return EC_ERROR_PARAM3;
	}
	slot->gpio_signal = GPIO_COUNT;
	if (!slot->overflow)
		gpio_disable_interrupt(gpio);
	return EC_SUCCESS;
}

static int command_gpio_monitoring(int argc, const char **argv)
{
	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;
	if (!strcasecmp(argv[2], "start"))
		return command_gpio_monitoring_start(argc, argv);
	if (!strcasecmp(argv[2], "read"))
		return command_gpio_monitoring_read(argc, argv);
	if (!strcasecmp(argv[2], "stop"))
		return command_gpio_monitoring_stop(argc, argv);
	return EC_ERROR_PARAM2;
}

static int command_gpio(int argc, const char **argv)
{
	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;
	if (!strcasecmp(argv[1], "monitoring"))
		return command_gpio_monitoring(argc, argv);
	return EC_ERROR_PARAM1;
}
DECLARE_CONSOLE_COMMAND_FLAGS(gpio, command_gpio,
			      "monitoring start PIN"
			      "\nmonitoring read PIN"
			      "\nmonitoring stop PIN",
			      "GPIO manipulation", CMD_FLAG_RESTRICTED);

