/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Flash memory module for stm32g4 */

#include <stdbool.h>
#include "battery.h"
#include "console.h"
#include "clock.h"
#include "flash.h"
#include "hooks.h"
#include "registers.h"
#include "panic.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/*
 * Approximate number of CPU cycles per iteration of the loop when polling
 * the flash status
 */
#define CYCLE_PER_FLASH_LOOP 10

/*
 * While flash write / erase is in progress, the stm32 CPU core is mostly
 * non-functional, due to the inability to fetch instructions from flash.
 * This may greatly increase interrupt latency.
 */

/* Flash page programming timeout.  This is 2x the datasheet max. */
#define FLASH_WRITE_TIMEOUT_US 16000
/* 20ms < tERASE < 40ms on F0/F3, for 1K / 2K sector size. */
#define FLASH_ERASE_TIMEOUT_US 40000

#if defined(CONFIG_FLASH_READOUT_PROTECTION_AS_PSTATE)
#if !defined(CHIP_FAMILY_STM32G4)
#error "CONFIG_FLASH_READOUT_PROTECTION_AS_PSTATE should work with all STM32F "
"series chips, but has not been tested"
#endif /* !CHIP_FAMILY_STM32G4 */
#endif /* CONFIG_FLASH_READOUT_PROTECTION_AS_PSTATE */

/* Forward declarations */
#if defined(CONFIG_FLASH_READOUT_PROTECTION_AS_PSTATE)
static enum flash_rdp_level flash_physical_get_rdp_level(void);
static int flash_physical_set_rdp_level(enum flash_rdp_level level);
#endif /* CONFIG_FLASH_READOUT_PROTECTION_AS_PSTATE */

/*
 * The STM32G4 family contains both category 2 and category 3 devices. The key
 * difference between category 3 and category 3 devices is the size of the
 * internal flash memory and that category 3 devices have 2 banks, where
 * category 2 devices have only a single bank.
 *
 * STM32G431xb is a category 2 device.
 * There are 2 main flash memory areas:
 *     1. Main memory 128kB consisting of 64 2kB pages, supporting both mass and
 *        page erase.
 *     2. Information block containing:
 *        - System memory (boot loader) 28 kB
 *        - 1 kbyte OTP
 *        - Option bytes block (48 bytes)
 */
struct ec_flash_bank const flash_bank_array[] = {
	{
		.count = 64,
		.size_exp = __fls(SIZE_2KB),
		.write_size_exp = __fls(CONFIG_FLASH_WRITE_SIZE),
		.erase_size_exp = __fls(SIZE_2KB),
		/*
		 * STM32G4 supports 2 or 4 WP regions which each must be an
		 * integer number of 2 kB pages. This field is currently not
		 * being used, but setting the value here to encompass the RO
		 * region (assuming RO/RW)
		 */
		.protect_size_exp = __fls(SIZE_64KB),
	},
};

/* Flag indicating whether we have locked down entire flash */
static int entire_flash_locked;

#define FLASH_SYSJUMP_TAG 0x5750 /* "WP" - Write Protect */
#define FLASH_HOOK_VERSION 1

/* The previous write protect state before sys jump */
struct flash_wp_state {
	int entire_flash_locked;
};


static int flash_physical_get_protect_at_boot(int block)
{
	/* 0: Write protection active on sector i. */
	return !(STM32_OPTB_WP & STM32_OPTB_nWRP(block));
}

static int flash_physical_protect_at_boot_update_rdp_pstate(uint32_t new_flags)
{
#if defined(CONFIG_FLASH_READOUT_PROTECTION_AS_PSTATE)
	int rv = EC_SUCCESS;

	bool rdp_enable = (new_flags & EC_FLASH_PROTECT_RO_AT_BOOT) != 0;

	/*
	 * This is intentionally a one-way latch. Once we have enabled RDP
	 * Level 1, we will only allow going back to Level 0 using the
	 * bootloader (e.g., "stm32mon -U") since transitioning from Level 1 to
	 * Level 0 triggers a mass erase.
	 */
	if (rdp_enable)
		rv = flash_physical_set_rdp_level(FLASH_RDP_LEVEL_1);

	return rv;
#else
	return EC_SUCCESS;
#endif
}


static inline int calculate_flash_timeout(void)
{
	return (FLASH_WRITE_TIMEOUT_US *
		(clock_get_freq() / SECOND) / CYCLE_PER_FLASH_LOOP);
}

static int wait_busy(void)
{
	int timeout = calculate_flash_timeout();
	while ((STM32_FLASH_SR & FLASH_SR_BUSY) && timeout-- > 0)
		udelay(CYCLE_PER_FLASH_LOOP);
	return (timeout > 0) ? EC_SUCCESS : EC_ERROR_TIMEOUT;
}


/*
 * We at least unlock the control register lock.
 * We may also unlock other locks.
 */
enum extra_lock_type  {
	NO_EXTRA_LOCK = 0,
	OPT_LOCK = 1,
};

/**
 * Check if write protect register state is inconsistent with RO_AT_BOOT and
 * ALL_AT_BOOT state.
 *
 * @return zero if consistent, non-zero if inconsistent.
 */
static int registers_need_reset(void)
{
	uint32_t flags = flash_get_protect();
	int i;
	int ro_at_boot = (flags & EC_FLASH_PROTECT_RO_AT_BOOT) ? 1 : 0;
	int ro_wp_region_start = WP_BANK_OFFSET;
	int ro_wp_region_end = WP_BANK_OFFSET + WP_BANK_COUNT;

	for (i = ro_wp_region_start; i < ro_wp_region_end; i++)
		if (flash_physical_get_protect_at_boot(i) != ro_at_boot)
			return 1;
	return 0;
}

static int unlock(int locks)
{
	/*
	 * We may have already locked the flash module and get a bus fault
	 * in the attempt to unlock. Need to disable bus fault handler now.
	 */
	ignore_bus_fault(1);

	/* Always unlock CR if needed */
	if (STM32_FLASH_CR & FLASH_CR_LOCK) {
		STM32_FLASH_KEYR = FLASH_KEYR_KEY1;
		STM32_FLASH_KEYR = FLASH_KEYR_KEY2;
	}
	/* unlock option memory if required */
	if ((locks & OPT_LOCK) && STM32_FLASH_OPT_LOCKED) {
		STM32_FLASH_OPTKEYR = FLASH_OPTKEYR_KEY1;
		STM32_FLASH_OPTKEYR = FLASH_OPTKEYR_KEY2;
	}

	/* Re-enable bus fault handler */
	ignore_bus_fault(0);

	if ((locks & OPT_LOCK) && STM32_FLASH_OPT_LOCKED)
		return EC_ERROR_UNKNOWN;
	if (STM32_FLASH_CR & FLASH_CR_LOCK)
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

static void lock(void)
{
	STM32_FLASH_CR |= FLASH_CR_LOCK;
}

int write_optb(uint32_t mask, uint32_t value)
{
	int rv;

	rv = wait_busy();
	if (rv)
		return rv;

	/* The target byte is the value we want to write. */
	if ((STM32_FLASH_OPTCR & mask) == value)
		return EC_SUCCESS;

	rv = unlock(OPT_LOCK);
	if (rv)
		return rv;

	STM32_FLASH_OPTCR = (STM32_FLASH_OPTCR & ~mask) | value;
	STM32_FLASH_OPTCR |= FLASH_OPTSTRT;

	rv = wait_busy();
	if (rv)
		return rv;
	lock();

	return EC_SUCCESS;
}

#if defined(CONFIG_FLASH_READOUT_PROTECTION_AS_PSTATE)
/**
 * @return true if RDP (read protection) Level 1 or 2 enabled, false otherwise
 */
bool is_flash_rdp_enabled(void)
{
	enum flash_rdp_level level = flash_physical_get_rdp_level();

	if (level == FLASH_RDP_LEVEL_INVALID) {
		CPRINTS("ERROR: unable to read RDP level");
		return false;
	}

	return level != FLASH_RDP_LEVEL_0;
}
#endif /* CONFIG_FLASH_READOUT_PROTECTION_AS_PSTATE */

/*****************************************************************************/
/* Physical layer APIs */

int flash_physical_write(int offset, int size, const char *data)
{
#if CONFIG_FLASH_WRITE_SIZE == 1
	uint8_t *address = (uint8_t *)(CONFIG_PROGRAM_MEMORY_BASE + offset);
	uint8_t quantum = 0;
#elif CONFIG_FLASH_WRITE_SIZE == 2
	uint16_t *address = (uint16_t *)(CONFIG_PROGRAM_MEMORY_BASE + offset);
	uint16_t quantum = 0;
#elif CONFIG_FLASH_WRITE_SIZE == 4
	uint32_t *address = (uint32_t *)(CONFIG_PROGRAM_MEMORY_BASE + offset);
	uint32_t quantum = 0;
#else
#error "CONFIG_FLASH_WRITE_SIZE not supported."
#endif
	int res = EC_SUCCESS;
	int timeout = calculate_flash_timeout();

	if (unlock(NO_EXTRA_LOCK) != EC_SUCCESS) {
		res = EC_ERROR_UNKNOWN;
		goto exit_wr;
	}

	/* Clear previous error status */
	STM32_FLASH_SR = FLASH_SR_ALL_ERR | FLASH_SR_EOP;

	/* set PG bit */
	STM32_FLASH_CR |= FLASH_CR_PG;

	for (; size > 0; size -= CONFIG_FLASH_WRITE_SIZE) {
		int i;

		for (i = CONFIG_FLASH_WRITE_SIZE - 1, quantum = 0; i >= 0; i--)
			quantum = (quantum << 8) + data[i];
		data += CONFIG_FLASH_WRITE_SIZE;
		/*
		 * Reload the watchdog timer to avoid watchdog reset when doing
		 * long writing with interrupt disabled.
		 */
		watchdog_reload();

		/* wait to be ready  */
		for (i = 0;
		     (STM32_FLASH_SR & FLASH_SR_BUSY) &&
		     (i < timeout);
		     i++)
			;

		/* write the data */
		*address++ = quantum;

		/* Wait for writes to complete */
		for (i = 0;
		     (STM32_FLASH_SR & FLASH_SR_BUSY) &&
		     (i < timeout);
		     i++)
			;

		if (STM32_FLASH_SR & FLASH_SR_BUSY) {
			res = EC_ERROR_TIMEOUT;
			goto exit_wr;
		}

		/* Check for error conditions - erase failed, voltage error,
		 * protection error */
		if (STM32_FLASH_SR & FLASH_SR_ALL_ERR) {
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
	int sector_size;
	int timeout_us;
#ifdef CHIP_FAMILY_STM32F4
	int sector = flash_bank_index(offset);
	/* we take advantage of sector_size == erase_size */
	if ((sector < 0) || (flash_bank_index(offset + size) < 0))
		return EC_ERROR_INVAL;  /* Invalid range */
#endif

	if (unlock(NO_EXTRA_LOCK) != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	/* Clear previous error status */
	STM32_FLASH_SR = FLASH_SR_ALL_ERR | FLASH_SR_EOP;

	/* set SER/PER bit */
	STM32_FLASH_CR |= FLASH_CR_PER;

	while (size > 0) {
		timestamp_t deadline;
#ifdef CHIP_FAMILY_STM32F4
		sector_size = flash_bank_size(sector);
		/* Timeout: from spec, proportional to the size
		 * inversely proportional to the write size.
		 */
		timeout_us = sector_size * 4 / CONFIG_FLASH_WRITE_SIZE;
#else
		sector_size = CONFIG_FLASH_ERASE_SIZE;
		timeout_us = FLASH_ERASE_TIMEOUT_US;
#endif
		/* Do nothing if already erased */
		if (flash_is_erased(offset, sector_size))
			goto next_sector;
#ifdef CHIP_FAMILY_STM32F4
		/* select page to erase */
		STM32_FLASH_CR = (STM32_FLASH_CR & ~STM32_FLASH_CR_SNB_MASK) |
			(sector << STM32_FLASH_CR_SNB_OFFSET);
#else
		/* select page to erase */
		STM32_FLASH_CR = offset / CONFIG_FLASH_ERASE_SIZE;
#endif
		/* set STRT bit : start erase */
		STM32_FLASH_CR |= FLASH_CR_STRT;

		deadline.val = get_time().val + timeout_us;
		/* Wait for erase to complete */
		watchdog_reload();
		while ((STM32_FLASH_SR & FLASH_SR_BUSY) &&
		       (get_time().val < deadline.val)) {
			usleep(timeout_us/100);
		}
		if (STM32_FLASH_SR & FLASH_SR_BUSY) {
			res = EC_ERROR_TIMEOUT;
			goto exit_er;
		}

		/*
		 * Check for error conditions - erase failed, voltage error,
		 * protection error
		 */
		if (STM32_FLASH_SR & FLASH_SR_ALL_ERR) {
			res = EC_ERROR_UNKNOWN;
			goto exit_er;
		}
next_sector:
		size -= sector_size;
		offset += sector_size;
#ifdef CHIP_FAMILY_STM32F4
		sector++;
#endif
	}

exit_er:
	/* reset SER/PER bit */
	STM32_FLASH_CR &= ~FLASH_CR_PER;

	lock();

	return res;
}

int flash_physical_get_protect(int block)
{
	return (entire_flash_locked ||
		!(STM32_OPTB_WP & STM32_OPTB_nWRP(block)));
}

uint32_t flash_physical_get_protect_flags(void)
{
	uint32_t flags = 0;

	/* Read all-protected state from our shadow copy */
	if (entire_flash_locked)
		flags |= EC_FLASH_PROTECT_ALL_NOW;

#if defined(CONFIG_FLASH_READOUT_PROTECTION_AS_PSTATE)
	if (is_flash_rdp_enabled())
		flags |= EC_FLASH_PROTECT_RO_AT_BOOT;
#endif

	return flags;
}

int flash_physical_protect_at_boot(uint32_t new_flags)
{
	int block;
	int original_val, val;

	original_val = val = STM32_OPTB_WP & STM32_OPTB_nWRP_ALL;

	for (block = WP_BANK_OFFSET;
	     block < WP_BANK_OFFSET + PHYSICAL_BANKS;
	     block++) {
		int protect = new_flags & EC_FLASH_PROTECT_ALL_AT_BOOT;

		if (block >= WP_BANK_OFFSET &&
		    block < WP_BANK_OFFSET + WP_BANK_COUNT)
			protect |= new_flags & EC_FLASH_PROTECT_RO_AT_BOOT;
#ifdef CONFIG_FLASH_PROTECT_RW
		else
			protect |= new_flags & EC_FLASH_PROTECT_RW_AT_BOOT;
#endif

		if (protect)
			val &= ~BIT(block);
		else
			val |= 1 << block;
	}
	if (original_val != val) {
		write_optb(STM32_FLASH_nWRP_ALL,
			   val << STM32_FLASH_nWRP_OFFSET);
	}


	return flash_physical_protect_at_boot_update_rdp_pstate(new_flags);
}

void unprotect_all_blocks(void)
{
	write_optb(STM32_FLASH_nWRP_ALL, STM32_FLASH_nWRP_ALL);
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
		 * permanently locked until reset, a correct keyring write
		 * will not unlock it. In this way we can implement system
		 * write protect.
		 */
		ignore_bus_fault(1);
		STM32_FLASH_KEYR = 0xffffffff;
		ignore_bus_fault(0);

		entire_flash_locked = 1;

		return EC_SUCCESS;
	}

	/* No way to protect just the RO flash until next boot */
	return EC_ERROR_INVAL;
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
	if (reset_flags & EC_RESET_FLAG_SYSJUMP) {
		prev = (const struct flash_wp_state *)system_get_jump_tag(
				FLASH_SYSJUMP_TAG, &version, &size);
		if (prev && version == FLASH_HOOK_VERSION &&
		    size == sizeof(*prev))
			entire_flash_locked = prev->entire_flash_locked;
		return 1;
	}

	return 0;
}

#if defined(CONFIG_FLASH_READOUT_PROTECTION_AS_PSTATE)
/**
 * @return true if RDP (read protection) Level 1 or 2 enabled, false otherwise
 */
bool is_flash_rdp_enabled(void)
{
	enum flash_rdp_level level = flash_physical_get_rdp_level();

	if (level == FLASH_RDP_LEVEL_INVALID) {
		CPRINTS("ERROR: unable to read RDP level");
		return false;
	}

	return level != FLASH_RDP_LEVEL_0;
}
#endif /* CONFIG_FLASH_READOUT_PROTECTION_AS_PSTATE */


/*****************************************************************************/
/* High-level APIs */

int flash_pre_init(void)
{
	uint32_t reset_flags = system_get_reset_flags();
	uint32_t prot_flags = flash_get_protect();
	int need_reset = 0;


#ifdef CHIP_FAMILY_STM32G4
	unlock(NO_EXTRA_LOCK);
	/* Set the proper write size */
	STM32_FLASH_CR = (STM32_FLASH_CR & ~STM32_FLASH_CR_PSIZE_MASK) |
		 (31 - __builtin_clz(CONFIG_FLASH_WRITE_SIZE)) <<
		 STM32_FLASH_CR_PSIZE_OFFSET;
	lock();
#endif
	if (flash_physical_restore_state())
		return EC_SUCCESS;

	/*
	 * If we have already jumped between images, an earlier image could
	 * have applied write protection. Nothing additional needs to be done.
	 */
	if (reset_flags & EC_RESET_FLAG_SYSJUMP)
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

#ifdef CONFIG_FLASH_PROTECT_RW
	if ((flash_physical_get_valid_flags() & EC_FLASH_PROTECT_RW_AT_BOOT) &&
	    (!!(prot_flags & EC_FLASH_PROTECT_RW_AT_BOOT) !=
	     !!(prot_flags & EC_FLASH_PROTECT_RW_NOW))) {
		/* RW_AT_BOOT and RW_NOW do not match. */
		need_reset = 1;
	}
#endif

#ifdef CONFIG_ROLLBACK
	if ((flash_physical_get_valid_flags() & EC_FLASH_PROTECT_ROLLBACK_AT_BOOT) &&
	    (!!(prot_flags & EC_FLASH_PROTECT_ROLLBACK_AT_BOOT) !=
	     !!(prot_flags & EC_FLASH_PROTECT_ROLLBACK_NOW))) {
		/* ROLLBACK_AT_BOOT and ROLLBACK_NOW do not match. */
		need_reset = 1;
	}
#endif

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
