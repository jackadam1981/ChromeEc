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
		 * interger number of 2 kB pages. This field is currently not
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

static int write_optb(uint32_t mask, uint32_t value)
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

static int write_optb(uint32_t mask, uint32_t value)
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
/* Hooks */

static void flash_preserve_state(void)
{
	struct flash_wp_state state;

	state.entire_flash_locked = entire_flash_locked;

	system_add_jump_tag(FLASH_SYSJUMP_TAG, FLASH_HOOK_VERSION,
			    sizeof(state), &state);
}
DECLARE_HOOK(HOOK_SYSJUMP, flash_preserve_state, HOOK_PRIO_DEFAULT);
