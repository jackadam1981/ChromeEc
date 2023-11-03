/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "debug.h"
#include "gpio.h"
#include "flash.h"
#include "registers.h"
#include "string.h"
#include "test_util.h"

#include "system.h"
#include "task.h"

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
}

/* The entire gpio port configuration */
struct gpio_port_config {
	struct stm32_gpio_moder_reg mode;
	struct stm32_gpio_otyper_reg out_type;
	struct stm32_gpio_ospeedr_reg out_speed;
	struct stm32_gpio_pupdr_reg pulls;
	struct stm32_gpio_afrl_reg alt_func_lower;
	struct stm32_gpio_afrh_reg alt_func_upper;
};

static struct gpio_port_config get_gpio_port_config(uint32_t port,
						    uint32_t mask)
{
	struct gpio_port_config cfg;
	REG32(&cfg.mode) = STM32_GPIO_MODER(port);
	REG32(&cfg.out_type) = STM32_GPIO_OTYPER(port);
	REG32(&cfg.out_speed) = STM32_GPIO_OSPEEDR(port);
	REG32(&cfg.pulls) = STM32_GPIO_PUPDR(port);
	REG32(&cfg.alt_func_lower) = STM32_GPIO_AFRL(port);
	REG32(&cfg.alt_func_upper) = STM32_GPIO_AFRH(port);
	return cfg;
}

struct gpio_config
{
	struct gpio_port_config port_a;
	struct gpio_port_config port_b;
};

static struct gpio_config get_gpio_config(void)
{
	struct gpio_config cfg;
	cfg.port_a = get_gpio_port_config(STM32_GPIOA_BASE,
				      BIT(15) | BIT(14) | BIT(13));
	cfg.port_b = get_gpio_port_config(STM32_GPIOB_BASE, BIT(4) | BIT(3));
	return cfg;
}


test_static int test_debugger_disable(void)
{
	struct gpio_port_config a_prev, b_prev, a, b;
	a_prev = get_gpio_port_config(STM32_GPIOA_BASE,
				      BIT(15) | BIT(14) | BIT(13));
	b_prev = get_gpio_port_config(STM32_GPIOB_BASE, BIT(4) | BIT(3));

	debugger_disable();

	a = get_gpio_port_config(STM32_GPIOA_BASE, BIT(15) | BIT(14) | BIT(13));
	b = get_gpio_port_config(STM32_GPIOB_BASE, BIT(4) | BIT(3));

	// TEST_EQ(a_prev, a, "%x");
	// TEST_EQ(b_prev, b, "%x");

	return EC_SUCCESS;
}


__keep void show_port_settings(uint32_t port, uint32_t mask)
{
	ccprintf("port = 0x%X | mask = 0x%X\n", port, mask);

	ccprintf("STM32_GPIO_MODER(0x%X) = 0x%X\n", port,
		 STM32_GPIO_MODER(port) & expand_to_2bit_mask(mask));
	ccprintf("STM32_GPIO_BSRR(0x%X) = 0x%X\n", port, STM32_GPIO_BSRR(port));
	ccprintf("STM32_GPIO_OTYPER(0x%X) = 0x%X\n", port,
		 STM32_GPIO_OTYPER(port));
	ccprintf("STM32_GPIO_PUPDR(0x%X) = 0x%X\n", port,
		 STM32_GPIO_PUPDR(port) & expand_to_2bit_mask(mask));

	ccprintf("STM32_GPIO_AFRL(0x%X) = 0x%X\n", port, STM32_GPIO_AFRL(port));
	ccprintf("STM32_GPIO_AFRH(0x%X) = 0x%X\n", port, STM32_GPIO_AFRH(port));

	ccprintf("\n");
}

///////////////////////////////////////////////////////////////////////////////

/*
 * The purpose of this step is primarily to reboot the MCU to ensure that
 * the debug GPIOs have not been fiddled with, yet.
 */
test_static void run_test_step1(void)
{
	ccprintf("STEP 1: Checking\n");
	cflush();

	uint32_t flags = crec_flash_get_protect();
	if (flags & EC_FLASH_PROTECT_RO_NOW) {
		ccprintf("STEP 1: Checking\n");
		test_fail();
	}

	test_reboot_to_next_step(TEST_STATE_STEP_2);
}

/*
 * Ensure that calling debugger_enable() after fresh boot doesn't change
 * any gpio flags, since that would mean that we aren't setting the correct
 * default gpio settings.
 */
test_static int test_debugger_enable_no_change(void)
{
	struct gpio_config on_boot_cfg;
	struct gpio_config after_enable_cfg;

	on_boot_cfg = get_gpio_config();

	debugger_enable();

	after_enable_cfg = get_gpio_config();

	if (memcmp(&on_boot_cfg, &after_enable_cfg, sizeof(struct gpio_config)) != 0) {
		ccprintf("Didn't match!!!\n");
	}

	return EC_SUCCESS;
}

test_static void run_test_step2(void)
{
	ccprintf("STEP 2: Checking that debugger_enable doesn't alter flags.\n");
	RUN_TEST(test_debugger_enable_no_change);

	if (test_get_error_count())
		test_reboot_to_next_step(TEST_STATE_FAILED);
	else
		test_reboot_to_next_step(TEST_STATE_PASSED);
}

void test_run_step(uint32_t state)
{
	if (state & TEST_STATE_MASK(TEST_STATE_STEP_1)) {
		run_test_step1();
	} else if (state & TEST_STATE_MASK(TEST_STATE_STEP_2)) {
		run_test_step2();
	}
}


int task_test(void *data)
{
	test_run_multistep();
	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();
	msleep(30); /* Wait for TASK_ID_TEST to initialize */
	task_wake(TASK_ID_TEST);
}


/////////////////////////////////////////////////////////////////////////////

static void print_debugger_usage(void)
{
	ccprintf("usage: debugger [enable|disable|help]\n");
	ccprintf("\n");
	ccprintf("This utility can check the status of the debugger, "
		 "or enable/disable the debugger port.\n");
}

static int command_debugger(int argc, const char **argv)
{
	ccprintf("debugger_is_connected() = %d\n", debugger_is_connected());
	ccprintf("debugger_was_connected() = %d\n", debugger_was_connected());

	show_port_settings(STM32_GPIOA_BASE, 0xE000);
	show_port_settings(STM32_GPIOB_BASE, 0x18);

	if (IS_ENABLED(CHIP_FAMILY_STM32F4) ||
	    IS_ENABLED(CHIP_FAMILY_STM32H7)) {
		if (argc < 2)
			return EC_SUCCESS;

		if (strcmp(argv[1], "enable") == 0) {
			ccprintf("Enabling JTAG port.\n");
			debugger_enable();
		} else if (strcmp(argv[1], "disable") == 0) {
			ccprintf("Disabling JTAG port.\n");
			debugger_disable();
		} else {
			print_debugger_usage();
		}
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(
	debugger, command_debugger, "",
	"Check detected debugger status or enable/disable debugger port.");
