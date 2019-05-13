/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "interrupts.h"
#include "ish_fwst.h"
#include "power_mgt.h"
#include "registers.h"
#include "shared_mem.h"
#include "spi.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Indices for hibernate data registers (RAM backed by VBAT) */
enum hibdata_index {
	HIBDATA_INDEX_SCRATCHPAD = 0,    /* General-purpose scratchpad */
	HIBDATA_INDEX_SAVED_RESET_FLAGS  /* Saved reset flags */
};

int system_is_reboot_warm(void)
{
	return !(system_get_reset_flags() &
		 (RESET_FLAG_POWER_ON | RESET_FLAG_HARD));
}

void system_pre_init(void)
{
	ish_fwst_set_fw_status(FWSTS_FW_IS_RUNNING);

	task_enable_irq(ISH_FABRIC_IRQ);

	if (IS_ENABLED(CONFIG_LOW_POWER_IDLE))
		ish_pm_init();

	system_set_reset_flags(chip_read_reset_flags());
}

void chip_save_reset_flags(int flags)
{
	ISH_RESET_FLAGS = flags;
}

uint32_t chip_read_reset_flags(void)
{
	uint32_t flags = ISH_RESET_FLAGS;

	/*
	 * Try to detect invalid reset flags, and if detected, assume
	 * we came up from a cold reset
	 */
	if (!flags /* zero is not valid */
	    || (flags >> 18) /* only bits 0-18 are valid */
	    /* cannot be both soft and hard reset at the same time */
	    || ((flags & RESET_FLAG_SOFT) && (flags & RESET_FLAG_HARD))
	    /* other is mutually-exclusive */
	    || ((flags & RESET_FLAG_OTHER) && (flags & ~RESET_FLAG_OTHER)))
		return RESET_FLAG_POWER_ON;

	return flags;
}

/*
 * Kill the Minute-IA core and don't come back alive.
 *
 * Used when the watchdog timer exceeds max retries and we want to
 * disable ISH completely.
 */
__attribute__((noreturn))
static void system_halt(void)
{
	while (1) {
		disable_all_interrupts();
		WDT_CONTROL = 0;
		CCU_TCG_EN = 1;
		__asm__ volatile (
			"cli\n"
			"hlt\n");
	}
}

void system_reset(int flags)
{
	uint32_t save_flags;

	system_encode_save_flags(flags, &save_flags);

	if (flags & SYSTEM_RESET_AP_WATCHDOG) {
		save_flags |= RESET_FLAG_WATCHDOG;

		ISH_WDT_RESET_COUNTER += 1;
		if (ISH_WDT_RESET_COUNTER >= CONFIG_WATCHDOG_MAX_RETRIES) {
			ccprints("Halting ISH due to max watchdog resets");
			system_halt();
		}
	}

	chip_save_reset_flags(save_flags);

	/*
	 * ish_pm_reset() does more (poweroff main SRAM, etc) than
	 * ish_mia_reset() which just resets the ISH minute-ia cpu core
	 */
	if (!IS_ENABLED(CONFIG_LOW_POWER_IDLE) || flags & SYSTEM_RESET_HARD)
		ish_mia_reset();
	else
		ish_pm_reset();

	__builtin_unreachable();
}

const char *system_get_chip_vendor(void)
{
	return "intel";
}

const char *system_get_chip_name(void)
{
	return "intel";
}

static char to_hex(int x)
{
	if (x >= 0 && x <= 9)
		return '0' + x;
	return 'a' + x - 10;
}

const char *system_get_chip_revision(void)
{
	static char buf[3];
	uint8_t rev = 0x86;

	buf[0] = to_hex(rev / 16);
	buf[1] = to_hex(rev & 0xf);
	buf[2] = '\0';
	return buf;
}

int system_get_bbram(enum system_bbram_idx idx, uint8_t *value)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int system_set_bbram(enum system_bbram_idx idx, uint8_t value)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int system_set_scratchpad(uint32_t value)
{
	return EC_SUCCESS;
}

uint32_t system_get_scratchpad(void)
{
	return 0;
}

void system_hibernate(uint32_t seconds, uint32_t microseconds)
{
}

void htimer_interrupt(void)
{
	/* Time to wake up */
}

enum system_image_copy_t system_get_shrspi_image_copy(void)
{
	return 0;
}

uint32_t system_get_lfw_address(void)
{
	return 0;
}

void system_set_image_copy(enum system_image_copy_t copy)
{
}

static void fabric_isr(void)
{
	/**
	 * clear fabric error status, otherwise it will wakeup ISH immediately
	 * when entered low power mode.
	 * TODO(b:130740646): figure out why this issue happens.
	 */
	if (FABRIC_AGENT_STATUS & FABRIC_MIA_STATUS_BIT_ERR)
		FABRIC_AGENT_STATUS = FABRIC_AGENT_STATUS;
}

DECLARE_IRQ(ISH_FABRIC_IRQ, fabric_isr);
