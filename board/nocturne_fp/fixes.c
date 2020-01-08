
/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* A place to organize legacy fixes and overrides */

#include "bkpdata.h"
#include "common.h"
#include "cpu.h" /* system_reset */
#include "ec_commands.h" /* Reset cause */
#include "gpio.h"
#include "hooks.h"
#include "panic.h" /* system_reset */
#include "system.h"
#include "task.h"
#include "watchdog.h"

static void system_forge_reset_flag_por(void) {
	/* Add in preserved flag for user */
	const uint32_t flags = EC_RESET_FLAG_POWER_ON | EC_RESET_FLAG_PRESERVED;

	/* Modify current flags, in case a reset with preserve flags occurs */
	system_set_reset_flags(flags);

	/* Preserve flags in case a reset pulse occurs */
	bkpdata_write(BKPDATA_INDEX_SAVED_RESET_FLAGS, flags & 0xffff);
	bkpdata_write(BKPDATA_INDEX_SAVED_RESET_FLAGS_2, flags >> 16);
}

static void board_init_fixes(void)
{
	if (!gpio_get_level(GPIO_WP)) {
		system_forge_reset_flag_por();
	}
}
DECLARE_HOOK(HOOK_INIT, board_init_fixes, HOOK_PRIO_DEFAULT - 1);

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