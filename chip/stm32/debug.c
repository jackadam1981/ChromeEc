/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "assert.h"
#include "common.h"
#include "compile_time_macros.h"
#include "debug.h"
#include "gpio.h"
#include "panic.h"
#include "registers.h"

/*
 * This function looks for signs that a debugger was attached. If we
 * see that a debugger was attached, we know that the chip's security features
 * may function as if the debugger is still attached.
 *
 * This is important because STM32 chips will emit a bus error and hang
 * upon enabling read protection level 1 (RDP1/software-write-protect), if it
 * detects a debugger. More specifically, if any flash access is performed,
 * say an instruction read, while RDP1 is enabled and in the presence of a
 * debugger, the MCU will trigger a bus error.
 *
 * From RM0402 Rev 5 Section 3.6.3 about read protection level 1:
 * "No access (read, erase, program) to Flash memory can be performed while the
 * debug feature is connected or while booting from RAM or system memory
 * bootloader. A bus error is generated in case of read request."
 */
__override bool debugger_was_connected(void)
{
	/*
	 * The bits we are looking for are the MCU debug control register bits
	 * responsible for permitting the clocks to continue running when the
	 * MCU goes into Sleep, Stop, or Standby. This allows the debugger to
	 * still communicate and control the MCU while in low-power modes.
	 * These bits seem to always be set by debugging software
	 * (JLink and OpenOCD) and not cleaned up upon disconnected.
	 *
	 * These bits and the chip debugger status are not cleared on reset.
	 * Only power-on-reset / power-cycle.
	 */
	return STM32_DBGMCU_CR & STM32_DBGMCU_CR_LOW_PWR_FRIENDLY;
}

__override void debugger_disable(void)
{
	/* Only board which */
	if (IS_ENABLED(BOARD_BLOONCHIPPER) || IS_ENABLED(BOARD_DARTMONKEY)) {
		/*
		 * Ensure that we are always called after the gpio peripherals
		 * have been enabled. Otherwise, our gpio configuration will be
		 * ignored by the peripheral. This is just an simple indicator
		 * and the an exhaustive check of the peripheral config.
		 */
#ifdef CHIP_FAMILY_STM32F4
		assert(STM32_RCC_AHB1ENR_STRUCT.gpio_a_en == true);
		assert(STM32_RCC_AHB1ENR_STRUCT.gpio_b_en == true);
#endif
#ifdef CHIP_FAMILY_STM32H7
		assert(STM32_RCC_AHB4ENR_STRUCT.gpio_a_en == true);
		assert(STM32_RCC_AHB4ENR_STRUCT.gpio_b_en == true);
#endif

		/*
		 * There are two crucial GPIO settings that determine whether a
		 * connected JTAG/SWD debugger will operate correctly.
		 *
		 * 1. The GPIO mode must be configured to utilize the
		 *    alternate function.
		 * 2. The alternate function selector must be set to
		 *    system/default/0.
		 *
		 * Modifying either of these two settings will effectively
		 * obstruct communication between the physical debugger and the
		 * debug module. As an added precaution, we modify both
		 * settings. Please see the board's gpio.inc file for detailed
		 * information about each debug pin.
		 *
		 * To conserve power, we configure the pins as analog input
		 * instead of digital input. See ST's AN4365 section 1.2.6
		 * for more detail.
		 */
		gpio_set_alternate_function(STM32_GPIOA_BASE, GENMASK(15, 13),
					    GPIO_ALT_FUNC_1);
		gpio_set_alternate_function(STM32_GPIOB_BASE, GENMASK(4, 3),
					    GPIO_ALT_FUNC_1);
		gpio_set_flags_by_mask(STM32_GPIOA_BASE, GENMASK(15, 13),
				       GPIO_ANALOG);
		gpio_set_flags_by_mask(STM32_GPIOB_BASE, GENMASK(4, 3),
				       GPIO_ANALOG);
	} else {
		panic("Called unimplemented security function debugger_disable");
		__builtin_unreachable();
	}
}

__override void debugger_enable(void)
{
	/*
	 * Configure the debug pins as specified in the board's gpio.inc.
	 *
	 * A failure here would simply mean that the module wasn't specified
	 * in the gpio.inc, which isn't critical.
	 */
	gpio_config_module(MODULE_DEBUG, 1);
}
