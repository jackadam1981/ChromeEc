/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This test must be called with fw_wp_state:force_on, debugger disconnected,
 * and after a fresh power-cycle/pin-reset.
 */

#include "assert.h"
#include "common.h"
#include "debug.h"
#include "flash-rdp.h"
#include "flash.h"
#include "gpio.h"
#include "registers.h"
#include "string.h"
#include "system.h"
#include "task.h"
#include "test_util.h"
#include "util.h"

#if !(defined(CHIP_FAMILY_STM32F4) || defined(CHIP_FAMILY_STM32H7))
#error Unsupported chip
#endif

/******************* Helper Types and Routines ********************************/

/* The entire gpio port configuration */
struct gpio_port_config {
	struct stm32_gpio_moder_reg mode;
	struct stm32_gpio_otyper_reg out_type;
	struct stm32_gpio_ospeedr_reg out_speed;
	struct stm32_gpio_pupdr_reg pulls;
	struct stm32_gpio_afrl_reg alt_func_lower;
	struct stm32_gpio_afrh_reg alt_func_upper;
};

/* All gpio and clock configuration for JTAG/SWD pins */
struct gpio_config {
	struct gpio_port_config port_a;
	struct gpio_port_config port_b;
#ifdef CHIP_FAMILY_STM32F4
	struct stm32_rcc_ahb1enr clocks;
#endif
#ifdef CHIP_FAMILY_STM32H7
	struct stm32_rcc_ahb4enr clocks;
#endif
};

/* Fetch the gpio_port_config for the given port */
static struct gpio_port_config get_gpio_port_config(uint32_t port)
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

/* Fetch all gpio port and clock configurations for JTAG/SWD pins */
static struct gpio_config get_gpio_config(void)
{
	struct gpio_config cfg;
	cfg.port_a = get_gpio_port_config(STM32_GPIOA_BASE);
	cfg.port_b = get_gpio_port_config(STM32_GPIOB_BASE);
#ifdef CHIP_FAMILY_STM32F4
	cfg.clocks = STM32_RCC_AHB1ENR_STRUCT;
#endif
#ifdef CHIP_FAMILY_STM32H7
	cfg.clocks = STM32_RCC_AHB4ENR_STRUCT;
#endif
	return cfg;
}

/******************* Test Functionality ***************************************/

/*
 * PRECONDITION:
 * Check that that RO_AT_BOOT isn't enabled, since this would mean that
 * bloonchipper and dartmonkey already disabled the debug interface.
 */
test_static int test_rdp_is_disabled(void)
{
	uint32_t flash_protect = crec_flash_get_protect();
	TEST_EQ(flash_protect & EC_FLASH_PROTECT_RO_AT_BOOT, 0, "0x%X");
	return EC_SUCCESS;
}

/*
 * PRECONDITION:
 * Check that that RO_AT_BOOT is enabled.
 */
test_static int test_rdp_is_enabled(void)
{
	uint32_t flash_protect = crec_flash_get_protect();
	TEST_EQ(flash_protect & EC_FLASH_PROTECT_RO_AT_BOOT, 1, "0x%X");
	return EC_SUCCESS;
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

	TEST_ASSERT_ARRAY_EQ((char *)&on_boot_cfg, (char *)&after_enable_cfg,
			     sizeof(struct gpio_config));

	return EC_SUCCESS;
}

/*
 * Check that the debugger interface is properly configured.
 */
test_static int test_debugger_is_enabled(void)
{
	struct gpio_config cfg;

	cfg = get_gpio_config();

	/* If the clocks aren't enable for the gpio module, the values are
	 * stale. */
	TEST_EQ((int)cfg.clocks.gpio_a_en, true, "%d");
	TEST_EQ((int)cfg.clocks.gpio_b_en, true, "%d");

	/*
	 * Anything other than STM32_GPIO_MODER_ALT_FUNC will disconnect pin
	 * from the JTAG/SWD alternative function, but we want to ensure it is
	 * set to analog to ensure power savings.
	 */
	TEST_EQ((int)cfg.port_a.mode.moder13, STM32_GPIO_MODER_ALT_FUNC, "%d");
	TEST_EQ((int)cfg.port_a.mode.moder14, STM32_GPIO_MODER_ALT_FUNC, "%d");
	TEST_EQ((int)cfg.port_a.mode.moder15, STM32_GPIO_MODER_ALT_FUNC, "%d");
	TEST_EQ((int)cfg.port_b.mode.moder3, STM32_GPIO_MODER_ALT_FUNC, "%d");
	TEST_EQ((int)cfg.port_b.mode.moder4, STM32_GPIO_MODER_ALT_FUNC, "%d");

	/* AF0 is the system/default, which is connected to the JTAG/SWD module.
	 */
	TEST_EQ((int)cfg.port_a.alt_func_upper.afrh13, STM32_GPIO_ALT_FUNC_AF0,
		"%d");
	TEST_EQ((int)cfg.port_a.alt_func_upper.afrh14, STM32_GPIO_ALT_FUNC_AF0,
		"%d");
	TEST_EQ((int)cfg.port_a.alt_func_upper.afrh15, STM32_GPIO_ALT_FUNC_AF0,
		"%d");
	TEST_EQ((int)cfg.port_b.alt_func_lower.afrl3, STM32_GPIO_ALT_FUNC_AF0,
		"%d");
	TEST_EQ((int)cfg.port_b.alt_func_lower.afrl4, STM32_GPIO_ALT_FUNC_AF0,
		"%d");

	TEST_EQ((int)cfg.port_a.pulls.pupdr13, STM32_GPIO_PUPDR_PULL_UP, "%d");
	TEST_EQ((int)cfg.port_a.pulls.pupdr14, STM32_GPIO_PUPDR_PULL_DOWN,
		"%d");
	TEST_EQ((int)cfg.port_a.pulls.pupdr15, STM32_GPIO_PUPDR_PULL_UP, "%d");
	TEST_EQ((int)cfg.port_b.pulls.pupdr3, STM32_GPIO_PUPDR_NONE, "%d");
	TEST_EQ((int)cfg.port_b.pulls.pupdr4, STM32_GPIO_PUPDR_PULL_UP, "%d");

	return EC_SUCCESS;
}

/*
 * Ensure that calling debugger_disable() sets the correct gpio mode and
 * alt-func. We want to ensure both mechanisms are used to disable JTAG/SWD.
 */
test_static int test_debugger_is_disabled(void)
{
	struct gpio_config cfg;

	cfg = get_gpio_config();

	/* If the clocks aren't enable for the gpio module, the values are
	 * stale. */
	TEST_EQ((int)cfg.clocks.gpio_a_en, true, "%d");
	TEST_EQ((int)cfg.clocks.gpio_b_en, true, "%d");

	/*
	 * Anything other than STM32_GPIO_MODER_ALT_FUNC will disconnect pin
	 * from the JTAG/SWD alternative function, but we want to ensure it is
	 * set to analog to ensure power savings.
	 */
	TEST_EQ((int)cfg.port_a.mode.moder13, STM32_GPIO_MODER_ANALOG, "%d");
	TEST_EQ((int)cfg.port_a.mode.moder14, STM32_GPIO_MODER_ANALOG, "%d");
	TEST_EQ((int)cfg.port_a.mode.moder15, STM32_GPIO_MODER_ANALOG, "%d");
	TEST_EQ((int)cfg.port_b.mode.moder3, STM32_GPIO_MODER_ANALOG, "%d");
	TEST_EQ((int)cfg.port_b.mode.moder4, STM32_GPIO_MODER_ANALOG, "%d");

	/* AF0 is the system/default, which is connected to the JTAG/SWD module.
	 */
	TEST_NE((int)cfg.port_a.alt_func_upper.afrh13, STM32_GPIO_ALT_FUNC_AF0,
		"%d");
	TEST_NE((int)cfg.port_a.alt_func_upper.afrh14, STM32_GPIO_ALT_FUNC_AF0,
		"%d");
	TEST_NE((int)cfg.port_a.alt_func_upper.afrh15, STM32_GPIO_ALT_FUNC_AF0,
		"%d");
	TEST_NE((int)cfg.port_b.alt_func_lower.afrl3, STM32_GPIO_ALT_FUNC_AF0,
		"%d");
	TEST_NE((int)cfg.port_b.alt_func_lower.afrl4, STM32_GPIO_ALT_FUNC_AF0,
		"%d");

	return EC_SUCCESS;
}

/*
 * This simply enables RDP for the next test.
 */
test_static int test_enable_rdp(void)
{
	/* Equivalent of ectool --name=cros_fp flashprotect enable */
	TEST_EQ(crec_flash_set_protect(EC_FLASH_PROTECT_RO_AT_BOOT,
				       EC_FLASH_PROTECT_RO_AT_BOOT),
		EC_SUCCESS, "%d");
	return EC_SUCCESS;
}

__override void test_multistep_run_step(enum test_multistep_step step)
{
	switch (step) {
	case TEST_MULTISTEP_STEP_1:
		/*
		 * The purpose of this step is primarily to reboot the MCU to
		 * ensure that the debug GPIOs have not been fiddled with, yet.
		 *
		 * Additionally, we can check that that RO_AT_BOOT isn't
		 * enabled, since this would mean that bloonchipper and
		 * dartmonkey already disabled the debug interface.
		 */
		RUN_TEST(test_rdp_is_disabled);
		break;
	case TEST_MULTISTEP_STEP_2:
		RUN_TEST(test_debugger_is_enabled);
		RUN_TEST(test_debugger_enable_no_change);
		RUN_TEST(test_debugger_is_enabled);
		break;
	case TEST_MULTISTEP_STEP_3:
		/*
		 * Call debugger_disable() and then check that the gpio
		 * configuration is correct. The reboot after this step will
		 * reenable the debug interface.
		 */
		debugger_disable();
		RUN_TEST(test_debugger_is_disabled);
		break;
	case TEST_MULTISTEP_STEP_4:
		/*
		 * This call to debugger_disable_on_boot() should do nothing if
		 * RDP is not enabled.
		 */
		RUN_TEST(test_rdp_is_disabled);
		debugger_disable_on_boot();
		RUN_TEST(test_debugger_is_enabled);
		break;
	case TEST_MULTISTEP_STEP_5:
		/*
		 * Enable RDP and then check whether the
		 * debugger_disable_on_boot() function disabled the debug
		 * interface.
		 *
		 * The effect of calling debugger_disable_on_boot is
		 * non-persistent across reboots.
		 */
		RUN_TEST(test_enable_rdp);
		RUN_TEST(test_rdp_is_enabled);
		debugger_disable_on_boot();
		RUN_TEST(test_debugger_is_disabled);
		break;
	case TEST_MULTISTEP_STEP_6:
		/*
		 * On reboot with RDP enabled, the debugger should be
		 * automatically disabled.
		 */
		RUN_TEST(test_rdp_is_enabled);
		RUN_TEST(test_debugger_is_disabled);
		break;
	default:
		assert(0);
	}

	if (test_get_error_count())
		test_multistep_finish(TEST_MULTISTEP_STATUS_FAILED);
	else if (step == TEST_MULTISTEP_STEP_6)
		test_multistep_finish(TEST_MULTISTEP_STATUS_PASSED);
	else
		test_multistep_reboot_to_next_step(step + 1);
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

/*********** Console Command: debugger [enable|disable] ***********************/

#define PRINTD(x) ccprintf("%s = %d\n", #x, x)

static void show_gpio_config(void)
{
	struct gpio_config cfg = get_gpio_config();

	ccprintf("Debug Pins GPIO Configuration:\n");

	PRINTD(cfg.clocks.gpio_a_en);
	PRINTD(cfg.clocks.gpio_b_en);

	PRINTD(cfg.port_a.mode.moder13);
	PRINTD(cfg.port_a.mode.moder14);
	PRINTD(cfg.port_a.mode.moder15);
	PRINTD(cfg.port_b.mode.moder3);
	PRINTD(cfg.port_b.mode.moder4);

	PRINTD(cfg.port_a.alt_func_upper.afrh13);
	PRINTD(cfg.port_a.alt_func_upper.afrh14);
	PRINTD(cfg.port_a.alt_func_upper.afrh15);
	PRINTD(cfg.port_b.alt_func_lower.afrl3);
	PRINTD(cfg.port_b.alt_func_lower.afrl4);

	PRINTD(cfg.port_a.pulls.pupdr13);
	PRINTD(cfg.port_a.pulls.pupdr14);
	PRINTD(cfg.port_a.pulls.pupdr15);
	PRINTD(cfg.port_b.pulls.pupdr3);
	PRINTD(cfg.port_b.pulls.pupdr4);

	/* The following don't really matter. */

	PRINTD(cfg.port_a.out_speed.ospeedr13);
	PRINTD(cfg.port_a.out_speed.ospeedr14);
	PRINTD(cfg.port_a.out_speed.ospeedr15);
	PRINTD(cfg.port_b.out_speed.ospeedr3);
	PRINTD(cfg.port_b.out_speed.ospeedr4);

	PRINTD(cfg.port_a.out_type.otyper13);
	PRINTD(cfg.port_a.out_type.otyper14);
	PRINTD(cfg.port_a.out_type.otyper15);
	PRINTD(cfg.port_b.out_type.otyper3);
	PRINTD(cfg.port_b.out_type.otyper4);
}

/* Check whether the gpio configuration indicates that JTAG/SWD is enabled */
static bool is_debug_enabled(void)
{
	const struct gpio_config cfg = get_gpio_config();
	const enum stm32_gpio_moder modes[] = {
		cfg.port_a.mode.moder13, cfg.port_a.mode.moder14,
		cfg.port_a.mode.moder15, cfg.port_b.mode.moder3,
		cfg.port_b.mode.moder4,
	};
	const enum stm32_gpio_alt_func alt_func_selects[] = {
		cfg.port_a.alt_func_upper.afrh13,
		cfg.port_a.alt_func_upper.afrh14,
		cfg.port_a.alt_func_upper.afrh15,
		cfg.port_b.alt_func_lower.afrl3,
		cfg.port_b.alt_func_lower.afrl4,
	};
	for (int i = 0; i < ARRAY_SIZE(modes); i++) {
		if (modes[i] != STM32_GPIO_MODER_ALT_FUNC) {
			return false;
		}
	}
	for (int i = 0; i < ARRAY_SIZE(alt_func_selects); i++) {
		if (alt_func_selects[i] != STM32_GPIO_ALT_FUNC_AF0) {
			return false;
		}
	}
	/* We don't check pulls. */
	return true;
}

static void print_debugger_usage(void)
{
	ccprintf("usage: debugger [enable|disable|help]\n");
	ccprintf("\n");
	ccprintf("This utility can check the status of the debugger, "
		 "or enable/disable the debugger port.\n");
}

static int command_debugger(int argc, const char **argv)
{
	if (argc > 1) {
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

	ccprintf("debugger_is_connected() = %d\n", debugger_is_connected());
	ccprintf("debugger_was_connected() = %d\n", debugger_was_connected());
	ccprintf("is_debug_enabled() = %d\n", is_debug_enabled());
	ccprintf("is_flash_rdp_enabled() = %d\n", is_flash_rdp_enabled());

	show_gpio_config();

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(
	debugger, command_debugger, "",
	"Check detected debugger status or enable/disable debugger port.");
