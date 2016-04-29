/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Flash memory module for Chrome EC */

#include "common.h"
#include "flash.h"
#include "hooks.h"
#include "registers.h"
#include "system.h"
#include "panic.h"

/* Flag indicating whether we have locked down entire flash */
static int entire_flash_locked;

#define FLASH_SYSJUMP_TAG 0x5750 /* "WP" - Write Protect */
#define FLASH_HOOK_VERSION 1
/* The previous write protect state before sys jump */
/*
 * TODO(crosbug.com/p/23798): check if STM32L code works here too - that is,
 * check if entire flash is locked by attempting to lock it rather than keeping
 * a global variable.
 */
struct flash_wp_state {
	int entire_flash_locked;
};


/*****************************************************************************/
/* High-level APIs */

int flash_pre_init(void)
{
#if 0
	uint32_t reset_flags = system_get_reset_flags();
	uint32_t prot_flags = flash_get_protect();
	int need_reset = 0;

	if (flash_physical_restore_state())
		return EC_SUCCESS;

	/*
	 * If we have already jumped between images, an earlier image could
	 * have applied write protection. Nothing additional needs to be done.
	 */
	if (reset_flags & RESET_FLAG_SYSJUMP)
		return EC_SUCCESS;

	if (prot_flags & EC_FLASH_PROTECT_GPIO_ASSERTED) {
		if ((prot_flags & EC_FLASH_PROTECT_RO_AT_BOOT) &&
		    !(prot_flags & EC_FLASH_PROTECT_RO_NOW)) {
			/*
			 * Pstate wants RO protected at boot, but the write
			 * protect register wasn't set to protect it.  Force an
			 * update to the write protect register and reboot so
			 * it takes effect.
			 */
			flash_physical_protect_at_boot(FLASH_WP_RO);
			need_reset = 1;
		}

		if (registers_need_reset()) {
			/*
			 * Write protect register was in an inconsistent state.
			 * Set it back to a good state and reboot.
			 *
			 * TODO(crosbug.com/p/23798): this seems really similar
			 * to the check above.  One of them should be able to
			 * go away.
			 */
			flash_protect_at_boot(
				(prot_flags & EC_FLASH_PROTECT_RO_AT_BOOT) ?
				FLASH_WP_RO : FLASH_WP_NONE);
			need_reset = 1;
		}
	} else {
		if (prot_flags & EC_FLASH_PROTECT_RO_NOW) {
			/*
			 * Write protect pin unasserted but some section is
			 * protected. Drop it and reboot.
			 */
			unprotect_all_blocks();
			need_reset = 1;
		}
	}

	if ((flash_physical_get_valid_flags() & EC_FLASH_PROTECT_ALL_AT_BOOT) &&
	    (!!(prot_flags & EC_FLASH_PROTECT_ALL_AT_BOOT) !=
	     !!(prot_flags & EC_FLASH_PROTECT_ALL_NOW))) {
		/*
		 * ALL_AT_BOOT and ALL_NOW should be both set or both unset
		 * at boot. If they are not, it must be that the chip requires
		 * OBL_LAUNCH to be set to reload option bytes. Let's reset
		 * the system with OBL_LAUNCH set.
		 * This assumes OBL_LAUNCH is used for hard reset in
		 * chip/stm32/system.c.
		 */
		need_reset = 1;
	}

	if (need_reset)
		system_reset(SYSTEM_RESET_HARD | SYSTEM_RESET_PRESERVE_FLAGS);
#endif
	return EC_SUCCESS;
}


/*****************************************************************************/
/* Physical layer APIs */

int flash_physical_get_protect(int block)
{
	return entire_flash_locked;
}

uint32_t flash_physical_get_protect_flags(void)
{
	uint32_t flags = 0;

	/* Read all-protected state from our shadow copy */
	if (entire_flash_locked)
		flags |= EC_FLASH_PROTECT_ALL_NOW;

	return flags;
}

int flash_physical_protect_now(int all)
{
	if (all) {
		/*
		 * Lock by writing a wrong key to FLASH_KEYR. This triggers a
		 * bus fault, so we need to disable bus fault handler while
		 * doing this.
		 */
		ignore_bus_fault(1);
		STM32_FLASH_KEYR = 0xffffffff;
		ignore_bus_fault(0);

		entire_flash_locked = 1;

		return EC_SUCCESS;
	} else {
		/* No way to protect just the RO flash until next boot */
		return EC_ERROR_INVAL;
	}
}

uint32_t flash_physical_get_valid_flags(void)
{
	return EC_FLASH_PROTECT_RO_AT_BOOT |
	       EC_FLASH_PROTECT_RO_NOW |
	       EC_FLASH_PROTECT_ALL_NOW;
}

uint32_t flash_physical_get_writable_flags(uint32_t cur_flags)
{
	uint32_t ret = 0;

	/* If RO protection isn't enabled, its at-boot state can be changed. */
	if (!(cur_flags & EC_FLASH_PROTECT_RO_NOW))
		ret |= EC_FLASH_PROTECT_RO_AT_BOOT;

	/*
	 * If entire flash isn't protected at this boot, it can be enabled if
	 * the WP GPIO is asserted.
	 */
	if (!(cur_flags & EC_FLASH_PROTECT_ALL_NOW) &&
	    (cur_flags & EC_FLASH_PROTECT_GPIO_ASSERTED))
		ret |= EC_FLASH_PROTECT_ALL_NOW;

	return ret;
}

int flash_physical_restore_state(void)
{
	uint32_t reset_flags = system_get_reset_flags();
	int version, size;
	const struct flash_wp_state *prev;

	/*
	 * If we have already jumped between images, an earlier image could
	 * have applied write protection. Nothing additional needs to be done.
	 */
	if (reset_flags & RESET_FLAG_SYSJUMP) {
		prev = (const struct flash_wp_state *)system_get_jump_tag(
				FLASH_SYSJUMP_TAG, &version, &size);
		if (prev && version == FLASH_HOOK_VERSION &&
		    size == sizeof(*prev))
			entire_flash_locked = prev->entire_flash_locked;
		return 1;
	}

	return 0;
}



int flash_physical_write(int offset, int size, const char *data)
{
#if 0
	uint16_t *address = (uint16_t *)(CONFIG_PROGRAM_MEMORY_BASE + offset);
	int res = EC_SUCCESS;
	int i;

	if (unlock(PRG_LOCK) != EC_SUCCESS) {
		res = EC_ERROR_UNKNOWN;
		goto exit_wr;
	}

	/* Clear previous error status */
	STM32_FLASH_SR = 0x34;

	/* set PG bit */
	STM32_FLASH_CR |= PG;

	for (; size > 0; size -= sizeof(uint16_t)) {
		/*
		 * Reload the watchdog timer to avoid watchdog reset when doing
		 * long writing with interrupt disabled.
		 */
		watchdog_reload();

		/* wait to be ready  */
		for (i = 0; (STM32_FLASH_SR & 1) && (i < FLASH_TIMEOUT_LOOP);
		     i++)
			;

		/* write the half word */
		*address++ = data[0] + (data[1] << 8);
		data += 2;

		/* Wait for writes to complete */
		for (i = 0; (STM32_FLASH_SR & 1) && (i < FLASH_TIMEOUT_LOOP);
		     i++)
			;

		if (STM32_FLASH_SR & 1) {
			res = EC_ERROR_TIMEOUT;
			goto exit_wr;
		}

		/* Check for error conditions - erase failed, voltage error,
		 * protection error */
		if (STM32_FLASH_SR & 0x14) {
			res = EC_ERROR_UNKNOWN;
			goto exit_wr;
		}
	}

exit_wr:
	/* Disable PG bit */
	STM32_FLASH_CR &= ~PG;

	lock();

	return res;
#else
	return EC_ERROR_UNKNOWN;
#endif
}

int flash_physical_erase(int offset, int size)
{
#if 0
	int res = EC_SUCCESS;

	if (unlock(PRG_LOCK) != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	/* Clear previous error status */
	STM32_FLASH_SR = 0x34;

	/* set PER bit */
	STM32_FLASH_CR |= PER;

	for (; size > 0; size -= CONFIG_FLASH_ERASE_SIZE,
	     offset += CONFIG_FLASH_ERASE_SIZE) {
		timestamp_t deadline;

		/* Do nothing if already erased */
		if (flash_is_erased(offset, CONFIG_FLASH_ERASE_SIZE))
			continue;

		/* select page to erase */
		STM32_FLASH_AR = CONFIG_PROGRAM_MEMORY_BASE + offset;

		/* set STRT bit : start erase */
		STM32_FLASH_CR |= STRT;

		/*
		 * Reload the watchdog timer to avoid watchdog reset during a
		 * long erase operation.
		 */
		watchdog_reload();

		deadline.val = get_time().val + FLASH_TIMEOUT_US;
		/* Wait for erase to complete */
		while ((STM32_FLASH_SR & 1) &&
		       (get_time().val < deadline.val)) {
			usleep(300);
		}
		if (STM32_FLASH_SR & 1) {
			res = EC_ERROR_TIMEOUT;
			goto exit_er;
		}

		/*
		 * Check for error conditions - erase failed, voltage error,
		 * protection error
		 */
		if (STM32_FLASH_SR & 0x14) {
			res = EC_ERROR_UNKNOWN;
			goto exit_er;
		}
	}

exit_er:
	/* reset PER bit */
	STM32_FLASH_CR &= ~PER;

	lock();

	return res;
#else
	return EC_ERROR_UNKNOWN;
#endif
}


/*****************************************************************************/
/* Hooks */

static void flash_preserve_state(void)
{
	struct flash_wp_state state;

	state.entire_flash_locked = entire_flash_locked;

	system_add_jump_tag(FLASH_SYSJUMP_TAG, FLASH_HOOK_VERSION,
			    sizeof(state), &state);
}
DECLARE_HOOK(HOOK_SYSJUMP, flash_preserve_state, HOOK_PRIO_DEFAULT);
