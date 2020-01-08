
/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* A place to organize legacy fixes and overrides */

#include <stdbool.h>

#include "bkpdata.h"
#include "common.h"
#include "console.h"
#include "cpu.h" /* system_reset */
#include "ec_commands.h" /* Reset cause */
#include "gpio.h"
#include "hooks.h"
#include "panic.h" /* system_reset */
#include "system.h"
#include "task.h"
#include "watchdog.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SYSTEM, outstr)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/*
 * We only patch RW to ensure that future RO's have correct behavior.
 */
#ifdef APPLY_RESET_LOOP_FIX

/*
 * Add in ap-off flag to be able to detect on next boot.
 * No other code in this build uses this ap-off reset flag.
 */
#define FORGE_PORFLAG_FLAGS (EC_RESET_FLAG_POWER_ON|EC_RESET_FLAG_AP_OFF)

/* We should not need the extended reset flags (more than 16bits) */
BUILD_ASSERT(FORGE_PORFLAG_FLAGS < BIT(16));

/*
 * TODO(hesling): Fix race condition between board_init_fixes and interrupt wp_event
 */
static void forge_porflag(bool forge) {
	if (forge) {
		/* Preserve flags in case a reset pulse occurs */
		bkpdata_write(BKPDATA_INDEX_SAVED_RESET_FLAGS,
			      FORGE_PORFLAG_FLAGS & 0xffff);
	} else {
		/* Normal state of reset backup reg is 0 */
		bkpdata_write(BKPDATA_INDEX_SAVED_RESET_FLAGS, 0);
	}
}

void wp_event(enum gpio_signal signal)
{
	forge_porflag(!gpio_get_level(GPIO_WP));
}

static void board_init_fixes(void)
{
	gpio_disable_interrupt(GPIO_WP);
	gpio_clear_pending_interrupt(GPIO_WP);

	if ((system_get_reset_flags() & FORGE_PORFLAG_FLAGS) ==
	    FORGE_PORFLAG_FLAGS) {
		CPRINTF("WARNING: We forged the last power on reset flag.\n");
		system_clear_reset_flags(FORGE_PORFLAG_FLAGS);
	}

	if (!gpio_get_level(GPIO_WP))
		forge_porflag(true);

	gpio_enable_interrupt(GPIO_WP);
}
/* Run one priority level higher than the main board_init in board.c */
DECLARE_HOOK(HOOK_INIT, board_init_fixes, HOOK_PRIO_DEFAULT - 1);

/**
 * @brief Custom system_reset handler to patch an RO bug
 */
void system_reset(int flags)
{
	uint32_t save_flags = 0;

	/* Disable interrupts to avoid task swaps during reboot */
	interrupt_disable();

	/* Save current reset reasons if necessary */
	if (flags & SYSTEM_RESET_PRESERVE_FLAGS)
		save_flags = system_get_reset_flags() | EC_RESET_FLAG_PRESERVED;

	if (flags & SYSTEM_RESET_LEAVE_AP_OFF)
		save_flags |= EC_RESET_FLAG_AP_OFF;

	/* Remember that the software asked us to hard reboot */
	if (flags & SYSTEM_RESET_HARD)
		save_flags |= EC_RESET_FLAG_HARD;

	if (!gpio_get_level(GPIO_WP))
		save_flags |= FORGE_PORFLAG_FLAGS;

#ifdef CONFIG_STM32_RESET_FLAGS_EXTENDED
	if (flags & SYSTEM_RESET_AP_WATCHDOG)
		save_flags |= EC_RESET_FLAG_AP_WATCHDOG;

	bkpdata_write(BKPDATA_INDEX_SAVED_RESET_FLAGS, save_flags & 0xffff);
	bkpdata_write(BKPDATA_INDEX_SAVED_RESET_FLAGS_2, save_flags >> 16);
#else
	/* Reset flags are 32-bits, but BBRAM entry is only 16 bits. */
	ASSERT(!(save_flags >> 16));
	bkpdata_write(BKPDATA_INDEX_SAVED_RESET_FLAGS, save_flags);
#endif

	if (flags & SYSTEM_RESET_HARD) {
#ifdef CONFIG_SOFTWARE_PANIC
		uint32_t reason, info;
		uint8_t exception;

		/* Panic data will be wiped by hard reset, so save it */
		panic_get_reason(&reason, &info, &exception);
		/* 16 bits stored - upper 16 bits of reason / info are lost */
		bkpdata_write(BKPDATA_INDEX_SAVED_PANIC_REASON, reason);
		bkpdata_write(BKPDATA_INDEX_SAVED_PANIC_INFO, info);
		bkpdata_write(BKPDATA_INDEX_SAVED_PANIC_EXCEPTION, exception);
#endif

		/*
		 * RM0433 Rev 6
		 * Section 44.3.3
		 * https://www.st.com/resource/en/reference_manual/dm00314099.pdf#page=1898
		 *
		 * When the window option is not used, the IWDG can be
		 * configured as follows:
		 *
		 * 1. Enable the IWDG by writing 0x0000 CCCC in the Key
		 *    register (IWDG_KR).
		 * 2. Enable register access by writing 0x0000 5555 in the Key
		 *    register (IWDG_KR).
		 * 3. Write the prescaler by programming the Prescaler register
		 *    (IWDG_PR) from 0 to 7.
		 * 4. Write the Reload register (IWDG_RLR).
		 * 5. Wait for the registers to be updated
		 *    (IWDG_SR = 0x0000 0000).
		 * 6. Refresh the counter value with IWDG_RLR
		 *    (IWDG_KR = 0x0000 AAAA)
		 */

		/*
		 * Enable IWDG, which shouldn't be necessary since the IWDG
		 * only needs to be started once, but STM32F412 hangs unless
		 * this is added.
		 *
		 * See http://b/137045370.
		 */
		STM32_IWDG_KR = STM32_IWDG_KR_START;

		/* Ask the watchdog to trigger a hard reboot */
		STM32_IWDG_KR = STM32_IWDG_KR_UNLOCK;
		STM32_IWDG_RLR = 0x1;
		/* Wait for value to be reloaded. */
		while (STM32_IWDG_SR & STM32_IWDG_SR_RVU)
			;
		STM32_IWDG_KR = STM32_IWDG_KR_RELOAD;

		/* wait for the chip to reboot */
		while (1)
			;
	} else {
		if (flags & SYSTEM_RESET_WAIT_EXT) {
			int i;

			/* Wait 10 seconds for external reset */
			for (i = 0; i < 1000; i++) {
				watchdog_reload();
				udelay(10000);
			}
		}
		CPU_NVIC_APINT = 0x05fa0004;
	}

	/* Spin and wait for reboot; should never return */
	while (1)
		;
}

#endif /* APPLY_RESET_LOOP_FIX */