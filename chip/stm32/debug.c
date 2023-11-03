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

/* Debugger internal-only panic routine for debugger_disable. */
void debugger_internal_panic_unimplemented(void);

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
	if (IS_ENABLED(CHIP_FAMILY_STM32F4) ||
	    IS_ENABLED(CHIP_FAMILY_STM32H7)) {
		/*
		 * There are two important gpio settings that must be set
		 * correctly, for the JTAG/SWD pins, in order for an attached
		 * debugger to function correctly.
		 * - The first is that the gpio mode must be set to use the
		 * alternative function.
		 * - The second is that the alternative function selector must
		 * be set to system/default/0.
		 *
		 * Changing either of these will effectively block communication
		 * to the debug module. We alter both as an extra precaution.
		 * See debugger_enable for information about each debug pin.
		 *
		 * Note that we configure the pins as analog input, instead of
		 * digital input, to save power. See ST's AN4365 section 1.2.6
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
		debugger_internal_panic_unimplemented();
		__builtin_unreachable();
	}
}

__override void debugger_enable(void)
{
	if (IS_ENABLED(CHIP_FAMILY_STM32F4) ||
	    IS_ENABLED(CHIP_FAMILY_STM32H7)) {
		/*
		 * STM32F412 and STM32H743 Debug Port Default Configuration.
		 *
		 * These pins and pulls are listed in the STM32F412 Reference
		 * Manual (RM0402 Rev 5) section 7.3.1 AND in the STM32H743
		 * Reference Manual (RM0433 Rev 6) section 10.3.1 as the
		 * following:
		 *
		 * - PA15: JTDI in pull-up
		 * - PA14: JTCK/SWCLK in pull-down
		 * - PA13: JTMS/SWDAT in pull-up
		 * - PB4:  NJTRST in pull-up
		 * - PB3:  JTDO in floating state
		 */
		gpio_set_flags_by_mask(STM32_GPIOA_BASE, BIT(15) | BIT(13),
				       GPIO_PULL_UP);
		gpio_set_flags_by_mask(STM32_GPIOA_BASE, BIT(14),
				       GPIO_PULL_DOWN);
		gpio_set_flags_by_mask(STM32_GPIOB_BASE, BIT(4), GPIO_PULL_UP);
		gpio_set_flags_by_mask(STM32_GPIOB_BASE, BIT(3),
				       GPIO_FLAG_NONE);
		gpio_set_alternate_function(STM32_GPIOA_BASE, GENMASK(15, 13),
					    GPIO_ALT_FUNC_DEFAULT);
		gpio_set_alternate_function(STM32_GPIOB_BASE, GENMASK(4, 3),
					    GPIO_ALT_FUNC_DEFAULT);
	}
}
