/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Flash memory module for stm32f4 */

#include "clock.h"
#include "compile_time_macros.h"
#include "console.h"
#include "common.h"
#include "flash.h"
#include "hooks.h"
#include "registers.h"
#include "system.h"
#include "panic.h"
#include "watchdog.h"


#define CPRINTS(format, args...) cprints(CC_CLOCK, format, ## args)

/*
 * Approximate number of CPU cycles per iteration of the loop when polling
 * the flash status
 */
#define CYCLE_PER_FLASH_LOOP 10

/* Flash page programming timeout.  This is 2x the datasheet max. */
#define FLASH_TIMEOUT_US 16000

static inline int calculate_flash_timeout(void)
{
	return (FLASH_TIMEOUT_US *
		(clock_get_freq() / SECOND) / CYCLE_PER_FLASH_LOOP);
}


/* Flag indicating whether we have locked down entire flash */
static int entire_flash_locked;

#define FLASH_SYSJUMP_TAG 0x5750 /* "WP" - Write Protect */
#define FLASH_HOOK_VERSION 1
/* The previous write protect state before sys jump */
struct flash_wp_state {
	int entire_flash_locked;
};

struct flash_sector {
	int base;
	int size;
};

/* STM32F4 has non-uniform sector size with the following layout */
static const struct flash_sector sectors[] = {
	{(0 * 1024), (16 * 1024)},
	{(16 * 1024), (16 * 1024)},
	{(32 * 1024), (16 * 1024)},
	{(48 * 1024), (16 * 1024)},
	{(64 * 1024), (64 * 1024)},
	{(128 * 1024), (128 * 1024)},
	{(256 * 1024), (128 * 1024)},
	{(384 * 1024), (128 * 1024)},
	{(512 * 1024), (128 * 1024)},
	{(640 * 1024), (128 * 1024)},
	{(784 * 1024), (128 * 1024)},
	{(912 * 1024), (128 * 1024)}
};
static const int num_sectors = ARRAY_SIZE(sectors);


/*****************************************************************************/
/* Physical layer APIs */

static int wait_busy(void)
{
	int timeout = calculate_flash_timeout();

	while (STM32_FLASH_SR & (1 << 0) && timeout-- > 0)
		udelay(CYCLE_PER_FLASH_LOOP);
	return (timeout > 0) ? EC_SUCCESS : EC_ERROR_TIMEOUT;
}

static int unlock(int locks)
{
	/*
	 * We may have already locked the flash module and get a bus fault
	 * in the attempt to unlock. Need to disable bus fault handler now.
	 */
	ignore_bus_fault(1);

	/* unlock CR if needed */
	if (STM32_FLASH_CR & FLASH_CR_LOCK) {
		STM32_FLASH_KEYR = FLASH_KEYR_KEY1;
		STM32_FLASH_KEYR = FLASH_KEYR_KEY2;
	}

	/* unlock option memory if required */
	if ((locks & FLASH_OPTCR_LOCK) &&
	    (STM32_FLASH_OPTCR & FLASH_OPTCR_LOCK)) {
		STM32_FLASH_OPTKEYR = FLASH_OPTKEYR_KEY1;
		STM32_FLASH_OPTKEYR = FLASH_OPTKEYR_KEY2;
	}

	/* Re-enable bus fault handler */
	ignore_bus_fault(0);

	return ((STM32_FLASH_CR & FLASH_CR_LOCK) |
		(STM32_FLASH_OPTCR & locks)) ?  EC_ERROR_UNKNOWN : EC_SUCCESS;
}

static void lock(void)
{
	STM32_FLASH_CR = FLASH_CR_LOCK;
	STM32_FLASH_OPTCR &= FLASH_OPTCR_LOCK;
}

static uint16_t read_optb_wp(void)
{
	return (*(uint32_t *)(STM32_OPTB_BASE + STM32_OPTB_WRP_OFF) & 0xFFF);
}

static int write_optb_wp(uint16_t value)
{
	volatile uint32_t *word = (uint32_t *)(STM32_OPTB_BASE +
					       STM32_OPTB_WRP_OFF);
	int rv;
	uint32_t wp_mask = 0x0FFF;

	rv = wait_busy();
	if (rv)
		return rv;

	/* The target byte is the value we want to write. */
	if ((value & wp_mask) == (*word & wp_mask))
		return EC_SUCCESS;

	rv = unlock(FLASH_OPTCR_LOCK);
	if (rv)
		return rv;

	STM32_FLASH_OPTCR = (STM32_FLASH_OPTCR & ~0x0FFF0000) |
			    (value & wp_mask) << 16;

	/* set OPTSTRT bit */
	STM32_FLASH_OPTCR |= FLASH_OPTCR_STRT;

	rv = wait_busy();
	if (rv)
		return rv;
	lock();

	return EC_SUCCESS;
}

int flash_physical_get_protect(int block)
{
	int offset = block * CONFIG_FLASH_BANK_SIZE;
	uint16_t wp_val = read_optb_wp();
	int i;

	for (i = 0; i < num_sectors; i++) {
		if (offset >= sectors[i].base &&
		    offset < sectors[i].base + sectors[i].size) {
			return !(wp_val & (1 << i));
		}
	}

	return 0;
}

uint32_t flash_physical_get_protect_flags(void)
{
	return entire_flash_locked ? EC_FLASH_PROTECT_ALL_NOW : 0;
}

int flash_physical_protect_now(int all)
{
	if (all) {
		/*
		 * Lock by writing a wrong key to FLASH_KEYR. This triggers a
		 * bus fault, so we need to disable bus fault handler while
		 * doing this.
		 *
		 * This incorrect key fault causes the flash to become
		 * permanenlty locked until reset, a correct keyring write
		 * will not unlock it. In this way we can implement system
		 * write protect.
		 */
		ignore_bus_fault(1);
		STM32_FLASH_KEYR = 0xffffffff;
		ignore_bus_fault(0);

		entire_flash_locked = 1;

		/* Check if lock happened */
		if (STM32_FLASH_CR & FLASH_CR_LOCK)
			return EC_SUCCESS;
	}

	/* No way to protect just the RO flash until next boot */
	return EC_ERROR_INVAL;
}

uint32_t flash_physical_get_valid_flags(void)
{
	return EC_FLASH_PROTECT_RO_AT_BOOT |
	       EC_FLASH_PROTECT_RO_NOW |
	       EC_FLASH_PROTECT_ALL_AT_BOOT |
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

static int flash_idle(void)
{
	timestamp_t deadline;

	deadline.val = get_time().val + FLASH_TIMEOUT_US;
	/* Wait for flash op to complete.
	 * This function is used for both reads and writes, so
	 * we need a long timeout, but a relatively short poll interval.
	 */
	while ((STM32_FLASH_SR & FLASH_SR_BUSY) &&
		(get_time().val < deadline.val)) {
		usleep(1);
	}

	if (STM32_FLASH_SR & FLASH_SR_BUSY)
		return EC_ERROR_TIMEOUT;

	return EC_SUCCESS;
}

static void clear_flash_errors(void)
{
	/* Clear previous error status */
	STM32_FLASH_SR = FLASH_SR_ERR_MASK;
}

/*****************************************************************************/
/* Physical layer APIs */

int flash_physical_protect_at_boot(uint32_t new_flags)
{
	uint16_t wp_val = 0xFFF;
	int i;

	unlock(FLASH_OPTCR_LOCK);

	for (i = 0; i < num_sectors; i++) {
		int protect = new_flags & EC_FLASH_PROTECT_ALL_AT_BOOT;

		if (sectors[i].base >= CONFIG_FLASH_SIZE)
			break;

		if (sectors[i].base >= CONFIG_WP_STORAGE_OFF  &&
		    sectors[i].base + sectors[i].size <=
		    CONFIG_WP_STORAGE_OFF + CONFIG_WP_STORAGE_SIZE)
			protect |= new_flags & EC_FLASH_PROTECT_RO_AT_BOOT;

		if (protect)
			wp_val &= ~(1 << i);
	}

	return write_optb_wp(wp_val);
}

int flash_physical_write(int offset, int size, const char *data)
{
	uint32_t *address = (uint32_t *)(CONFIG_MAPPED_STORAGE_BASE + offset);
	int res = EC_SUCCESS;

	if (unlock(0) != EC_SUCCESS) {
		res = EC_ERROR_UNKNOWN;
		goto exit_wr;
	}

	/* Wait for busy to clear */
	res = flash_idle();
	if (res)
		goto exit_wr;
	clear_flash_errors();

	/* set PG bit */
	STM32_FLASH_CR &= ~FLASH_CR_PSIZE_MASK;
	STM32_FLASH_CR |= FLASH_CR_PSIZE(FLASH_CR_PSIZE_32);
	STM32_FLASH_CR |= FLASH_CR_PG;

	for (; size > 0; size -= sizeof(uint32_t)) {
		/*
		 * Reload the watchdog timer to avoid watchdog reset when doing
		 * long writing with interrupt disabled.
		 */
		watchdog_reload();

		res = flash_idle();
		if (res)
			goto exit_wr;

		/* write the word */
		*address = data[0] + (data[1] << 8) +
			   (data[2] << 16) + (data[3] << 24);

		address++;
		data += sizeof(uint32_t);

		res = flash_idle();
		if (res)
			goto exit_wr;

		if (STM32_FLASH_SR & FLASH_SR_BUSY) {
			res = EC_ERROR_TIMEOUT;
			goto exit_wr;
		}

		/* Check for error conditions - erase failed, voltage error,
		 * protection error.
		 */
		if (STM32_FLASH_SR & FLASH_SR_ERR_MASK) {
			res = EC_ERROR_UNKNOWN;
			goto exit_wr;
		}
	}

exit_wr:
	/* Disable PG bit */
	STM32_FLASH_CR &= ~FLASH_CR_PG;

	lock();

	return res;
}

int flash_physical_erase(int offset, int size)
{
	int res = EC_SUCCESS;
	int start_sector;
	int end_sector;

	/* Check that offset/size align with sectors. */
	for (start_sector = 0; start_sector < num_sectors; start_sector++)
		if (offset == sectors[start_sector].base)
			break;
	for (end_sector = start_sector; end_sector < num_sectors; end_sector++)
		if ((offset + size) ==
		    (sectors[end_sector].base + sectors[end_sector].size))
			break;

	/* We can only erase on sector boundaries. */
	if ((start_sector >= num_sectors) || (end_sector >= num_sectors))
		return EC_ERROR_PARAM1;

	if (unlock(0) != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	res = flash_idle();
	if (res)
		goto exit_er;

	clear_flash_errors();

	for (; start_sector <= end_sector; start_sector++) {
		/* Do nothing if already erased */
		if (flash_is_erased(sectors[start_sector].base,
				    sectors[start_sector].size))
			continue;

		res = flash_idle();
		if (res)
			goto exit_er;

		/* set Sector Erase bit and select sector */
		STM32_FLASH_CR = (STM32_FLASH_CR & ~FLASH_CR_SNB_MASK) |
				FLASH_CR_SER | FLASH_CR_SNB(start_sector);

		/* set STRT bit : start erase */
		STM32_FLASH_CR |= FLASH_CR_STRT;

		/*
		 * Reload the watchdog timer to avoid watchdog reset during a
		 * long erase operation.
		 */
		watchdog_reload();

		/* Wait for erase to complete, this will be awhile */
		res = flash_idle();
		if (res)
			goto exit_er;
		/*
		 * Check for error conditions - erase failed, voltage error,
		 * protection error
		 */
		if (STM32_FLASH_SR & FLASH_SR_ERR_MASK) {
			res = EC_ERROR_UNKNOWN;
			goto exit_er;
		}
	}

exit_er:
	/* reset PER bit */
	STM32_FLASH_CR &= ~FLASH_CR_SER;

	lock();

	return res;
}

/**
 * Check if write protect register state is inconsistent with RO_AT_BOOT and
 * ALL_AT_BOOT state.
 *
 * @return zero if consistent, non-zero if inconsistent.
 */
static int registers_need_reset(void)
{
	uint32_t flags = flash_get_protect();
	int wp_val = STM32_FLASH_WRPR;
	int i;
	int ro_at_boot = (flags & EC_FLASH_PROTECT_RO_AT_BOOT) ? 1 : 0;

	for (i = 0; i < num_sectors; i++) {
		if (sectors[i].base >= CONFIG_WP_STORAGE_OFF  &&
		    sectors[i].base + sectors[i].size <=
		    CONFIG_WP_STORAGE_OFF + CONFIG_WP_STORAGE_SIZE) {
			int protect = (wp_val >> i) & 0x1 ? 0 : 1;
			if (protect != ro_at_boot)
				return 1;
		}
	}
	return 0;
}

static void unprotect_all_blocks(void)
{
	write_optb_wp(0xFFF);
}

/*****************************************************************************/
/* High-level APIs */

int flash_pre_init(void)
{
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
			flash_physical_protect_at_boot(
				EC_FLASH_PROTECT_RO_AT_BOOT);
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
				prot_flags & EC_FLASH_PROTECT_RO_AT_BOOT);
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

	return EC_SUCCESS;
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

