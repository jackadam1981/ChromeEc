/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "debug.h"
#include "gpio.h"
#include "string.h"
#include "test_util.h"

#include "registers.h"

static bool debugger_connected;
static bool debugger_connected_previously /* = false */;

#include "util.h"
static uint32_t expand_to_2bit_mask(uint32_t mask)
{
	uint32_t mask_out = 0;
	while (mask) {
		int bit = get_next_bit(&mask);
		mask_out |= 3 << (bit * 2);
	}
	return mask_out;
}

static void print_usage(void)
{
	ccprintf(
		"usage: runtest [debugger|no_debugger] [was_debugger|was_no_debugger]\n");
	ccprintf("\n");
	ccprintf("debugger        - "
		 "There is currently a debugger connected.\n");
	ccprintf("no_debugger     - "
		 "There is not currently a debugger connected.\n");
	ccprintf("was_debugger    - "
		 "There was previously a debugger connected "
		 "(only power cycle can reset this).\n");
	ccprintf("was_no_debugger - "
		 "There was not previously a debugger connected "
		 "(only power cycle can reset this).\n");
}

test_static int test_debugger_is_connected(void)
{
	ccprintf("debugger_is_connected: %d\n", debugger_connected);
	TEST_EQ(debugger_is_connected(), debugger_connected, "%d");
	return EC_SUCCESS;
}

/*
 * Note that a reset will not suffice to reset debugger_was_connected() state.
 * It must be a power cycle.
 */
test_static int test_debugger_was_connected(void)
{
	ccprintf("debugger_was_connected: %d\n", debugger_connected_previously);
	TEST_EQ(debugger_was_connected(), debugger_connected_previously, "%d");
	return EC_SUCCESS;
}

struct port_config {
	uint32_t mode;
	uint32_t pulls;
	uint32_t out_type;
	uint32_t out_speed;
	uint32_t alt_fun_lower;
	uint32_t alt_fun_upper;
};

static struct port_config get_port_config(uint32_t port, uint32_t mask) {
	struct port_config cfg;
	cfg.mode = STM32_GPIO_MODER(port) & expand_to_2bit_mask(mask);
	cfg.pulls = STM32_GPIO_PUPDR(port) & expand_to_2bit_mask(mask);
	cfg.out_type = STM32_GPIO_OTYPER(port) & mask;
	cfg.out_speed = STM32_GPIO_OSPEEDR(port) & expand_to_2bit_mask(mask);
	cfg.alt_fun_lower = STM32_GPIO_AFRL(port); // too complicated to mask
	cfg.alt_fun_upper = STM32_GPIO_AFRH(port); // too complicated to mask
	return cfg;
}

test_static int test_debugger_enable_disable(void) {
	struct port_config a_prev, b_prev, a, b;
	a_prev = get_port_config(STM32_GPIOA_BASE, BIT(15)|BIT(14)|BIT(13));
	b_prev = get_port_config(STM32_GPIOB_BASE, BIT(4)|BIT(3));

	debugger_enable_disable(true);

	a = get_port_config(STM32_GPIOA_BASE, BIT(15)|BIT(14)|BIT(13));
	b = get_port_config(STM32_GPIOB_BASE, BIT(4)|BIT(3));

	// TEST_EQ(a_prev, a, "%x");
	// TEST_EQ(b_prev, b, "%x");

	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();

	if (argc < 2) {
		print_usage();
		test_fail();
		return;
	}

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "debugger") == 0) {
			debugger_connected = true;
		} else if (strcmp(argv[i], "no_debugger") == 0) {
			debugger_connected = false;
		} else if (strcmp(argv[i], "was_debugger") == 0) {
			debugger_connected_previously = true;
		} else if (strcmp(argv[i], "was_no_debugger") == 0) {
			debugger_connected_previously = false;
		} else {
			print_usage();
			test_fail();
			return;
		}
	}

	RUN_TEST(test_debugger_is_connected);
	RUN_TEST(test_debugger_was_connected);
	RUN_TEST(test_debugger_enable_disable);
	test_print_result();
}

static void print_debugger_usage(void)
{
	ccprintf("usage: debugger [enable|disable|help]\n");
	ccprintf("\n");
	ccprintf("This utility can check the status of the debugger, "
		 "or enable/disable the debugger port.\n");
}



__keep
void show_port_settings(uint32_t port, uint32_t mask) {
	ccprintf("port = 0x%X | mask = 0x%X\n", port, mask);

	ccprintf("STM32_GPIO_MODER(0x%X) = 0x%X\n", port, STM32_GPIO_MODER(port) & expand_to_2bit_mask(mask));
	ccprintf("STM32_GPIO_BSRR(0x%X) = 0x%X\n", port, STM32_GPIO_BSRR(port));
	ccprintf("STM32_GPIO_OTYPER(0x%X) = 0x%X\n", port, STM32_GPIO_OTYPER(port));
	ccprintf("STM32_GPIO_PUPDR(0x%X) = 0x%X\n", port, STM32_GPIO_PUPDR(port) & expand_to_2bit_mask(mask));

	ccprintf("STM32_GPIO_AFRL(0x%X) = 0x%X\n", port, STM32_GPIO_AFRL(port));
	ccprintf("STM32_GPIO_AFRH(0x%X) = 0x%X\n", port, STM32_GPIO_AFRH(port));

	ccprintf("\n");
}

// extern const struct gpio_alt_func gpio_alt_funcs[5];

static int command_debugger(int argc, const char **argv)
{
	ccprintf("debugger_is_connected() = %d\n", debugger_is_connected());
	ccprintf("debugger_was_connected() = %d\n", debugger_was_connected());

	show_port_settings(STM32_GPIOA_BASE, 0xE000);
	show_port_settings(STM32_GPIOB_BASE, 0x18);

	if (IS_ENABLED(BOARD_DARTMONKEY) || IS_ENABLED(BOARD_BLOONCHIPPER)) {
		if (argc < 2)
			return EC_SUCCESS;

		if (strcmp(argv[1], "enable") == 0) {
			ccprintf("Enabling JTAG port.\n");
			// gpio_config_module(MODULE_DEBUG, 1);
			debugger_enable_disable(true);
		} else if (strcmp(argv[1], "disable") == 0) {
			ccprintf("Disabling JTAG port.\n");
			// gpio_config_module(MODULE_DEBUG, 0);
			debugger_enable_disable(false);
		} else {
			print_debugger_usage();
		}
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(
	debugger, command_debugger, "",
	"Check detected debugger status or enable/disable debugger port.");
