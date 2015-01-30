/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Flash memory module for Flash internal to Chrome EC - common functions */

#include "common.h"
#include "console.h"
#include "flash.h"
#include "gpio.h"
#include "host_command.h"
#include "shared_mem.h"
#include "system.h"
#include "util.h"
#include "vboot_hash.h"

/* Persistent protection state - emulates a SPI status register for flashrom */
struct persist_state {
	uint8_t version;            /* Version of this struct */
	uint8_t flags;              /* Lock flags (PERSIST_FLAG_*) */
	uint8_t reserved[2];        /* Reserved; set 0 */
};

#define PERSIST_STATE_VERSION 2  /* Expected persist_state.version */

/* Flags for persist_state.flags */
/* Protect persist state and RO firmware at boot */
#define PERSIST_FLAG_PROTECT_RO 0x02



/**
 * Get the physical memory address of a flash offset
 *
 * This is used for direct flash access. We assume that the flash is
 * contiguous from this start address through to the end of the usable
 * flash.
 *
 * @param offset	Flash offset to get address of
 * @param dataptrp	Returns pointer to memory address of flash offset
 * @return pointer to flash memory offset, if ok, else NULL
 */
const char *flash_physical_dataptr(int offset)
{
	return (char *)((uintptr_t)CONFIG_FLASH_BASE + offset);
}

/**
 * Read persistent state into pstate.
 *
 * @param pstate	Destination for persistent state
 */
static void flash_read_pstate(struct persist_state *pstate)
{
	memcpy(pstate, flash_physical_dataptr(PSTATE_OFFSET), sizeof(*pstate));

	/* Sanity-check data and initialize if necessary */
	if (pstate->version != PERSIST_STATE_VERSION) {
		memset(pstate, 0, sizeof(*pstate));
		pstate->version = PERSIST_STATE_VERSION;
#ifdef CONFIG_WP_ALWAYS
		pstate->flags |= PERSIST_FLAG_PROTECT_RO;
#endif
	}
}

/**
 * Write persistent state from pstate, erasing if necessary.
 *
 * @param pstate	Source persistent state
 * @return EC_SUCCESS, or nonzero if error.
 */
static int flash_write_pstate(const struct persist_state *pstate)
{
	struct persist_state current_pstate;
	int rv;

	/* Check if pstate has actually changed */
	flash_read_pstate(&current_pstate);
	if (!memcmp(&current_pstate, pstate, sizeof(*pstate)))
		return EC_SUCCESS;

	/* Erase pstate */
	rv = flash_physical_erase(PSTATE_OFFSET, PSTATE_SIZE);
	if (rv)
		return rv;

	/*
	 * Note that if we lose power in here, we'll lose the pstate contents.
	 * That's ok, because it's only possible to write the pstate before
	 * it's protected.
	 */

	/* Rewrite the data */
	return flash_physical_write(PSTATE_OFFSET, sizeof(*pstate),
				    (const char *)pstate);
}





int flash_protect_at_boot(enum flash_wp_range range)
{
	struct persist_state pstate;
	int new_flags = (range != FLASH_WP_NONE) ? PERSIST_FLAG_PROTECT_RO : 0;

	/* Read the current persist state from flash */
	flash_read_pstate(&pstate);

	if (pstate.flags != new_flags) {
		/* Need to update pstate */
		int rv;

		/* Fail if write protect block is already locked */
		if (flash_physical_get_protect(PSTATE_BANK))
			return EC_ERROR_ACCESS_DENIED;

		/* Set the new flag */
		pstate.flags = new_flags;

		/* Write the state back to flash */
		rv = flash_write_pstate(&pstate);
		if (rv)
			return rv;
	}

#ifdef CONFIG_FLASH_PROTECT_NEXT_BOOT
	/*
	 * Try updating at-boot protection state, if on a platform where write
	 * protection only changes after a reboot.  Otherwise we wouldn't
	 * update it until after the next reboot, and we'd need to reboot
	 * again.  Ignore errors, because the protection registers might
	 * already be locked this boot, and we'll still apply the correct state
	 * again on the next boot.
	 *
	 * This assumes PSTATE immediately follows RO, which it does on
	 * all STM32 platforms (which are the only ones with this config).
	 */
	flash_physical_protect_at_boot(range);
#endif

	return EC_SUCCESS;
}

uint32_t flash_get_protect(void)
{
	struct persist_state pstate;
	uint32_t flags = 0;
	int not_protected[2] = {0};
	int i;

	/* Read write protect GPIO */
#ifdef CONFIG_WP_ALWAYS
	flags |= EC_FLASH_PROTECT_GPIO_ASSERTED;
#elif defined(CONFIG_WP_ACTIVE_HIGH)
	if (gpio_get_level(GPIO_WP))
		flags |= EC_FLASH_PROTECT_GPIO_ASSERTED;
#else
	if (!gpio_get_level(GPIO_WP_L))
		flags |= EC_FLASH_PROTECT_GPIO_ASSERTED;
#endif

	/* Read persistent state of RO-at-boot flag */
	flash_read_pstate(&pstate);
	if (pstate.flags & PERSIST_FLAG_PROTECT_RO)
		flags |= EC_FLASH_PROTECT_RO_AT_BOOT;

	/* Scan flash protection */
	for (i = 0; i < PHYSICAL_BANKS; i++) {
		/*
		 * Is this bank part of RO?  Needs to handle PSTATE not
		 * immediately following RO code, since it doesn't on link.
		 */
		int is_ro = ((i >= RO_BANK_OFFSET &&
			      i < RO_BANK_OFFSET + RO_BANK_COUNT) ||
			     (i >= PSTATE_BANK &&
			      i < PSTATE_BANK + PSTATE_BANK_COUNT)) ? 1 : 0;
		int bank_flag = (is_ro ? EC_FLASH_PROTECT_RO_NOW :
				 EC_FLASH_PROTECT_ALL_NOW);

		if (flash_physical_get_protect(i)) {
			/* At least one bank in the region is protected */
			flags |= bank_flag;
			if (not_protected[is_ro])
				flags |= EC_FLASH_PROTECT_ERROR_INCONSISTENT;
		} else {
			/* At least one bank in the region is NOT protected */
			not_protected[is_ro] = 1;
			if (flags & bank_flag)
				flags |= EC_FLASH_PROTECT_ERROR_INCONSISTENT;
		}
	}

	/*
	 * If the RW banks are protected but the RO banks aren't, that's
	 * inconsistent.
	 *
	 * Note that we check this before adding in the physical flags below,
	 * since some chips can also protect ALL_NOW for the current boot by
	 * locking up the flash program-erase registers.
	 */
	if ((flags & EC_FLASH_PROTECT_ALL_NOW) &&
	    !(flags & EC_FLASH_PROTECT_RO_NOW))
		flags |= EC_FLASH_PROTECT_ERROR_INCONSISTENT;

	/* Add in flags from physical layer */
	return flags | flash_physical_get_protect_flags();
}

int flash_set_protect(uint32_t mask, uint32_t flags)
{
	int retval = EC_SUCCESS;
	int rv;
	enum flash_wp_range range = FLASH_WP_NONE;
	int need_set_protect = 0;

	/*
	 * Process flags we can set.  Track the most recent error, but process
	 * all flags before returning.
	 */

	/*
	 * AT_BOOT flags are trickier than NOW flags, as they can be set
	 * when HW write protection is disabled and can be unset without
	 * a reboot.
	 *
	 * If we are only setting/clearing RO_AT_BOOT, things are simple.
	 * Setting ALL_AT_BOOT is processed only if HW write protection is
	 * enabled and RO_AT_BOOT is set, so it's also simple.
	 *
	 * The most tricky one is when we want to clear ALL_AT_BOOT. We need
	 * to determine whether to clear protection for the entire flash or
	 * leave RO protected. There are two cases that we want to keep RO
	 * protected:
	 *   1. RO_AT_BOOT was already set before flash_set_protect() is
	 *      called.
	 *   2. RO_AT_BOOT was not set, but it's requested to be set by
	 *      the caller of flash_set_protect().
	 */
	if (mask & EC_FLASH_PROTECT_RO_AT_BOOT) {
		range = (flags & EC_FLASH_PROTECT_RO_AT_BOOT) ?
			FLASH_WP_RO : FLASH_WP_NONE;
		need_set_protect = 1;
	}
	if ((mask & EC_FLASH_PROTECT_ALL_AT_BOOT) &&
	    !(flags & EC_FLASH_PROTECT_ALL_AT_BOOT)) {
		if (flash_get_protect() & EC_FLASH_PROTECT_RO_AT_BOOT)
			range = FLASH_WP_RO;
		need_set_protect = 1;
	}
	if (need_set_protect) {
		rv = flash_protect_at_boot(range);
		if (rv)
			retval = rv;
	}

	/*
	 * All subsequent flags only work if write protect is enabled (that is,
	 * hardware WP flag) *and* RO is protected at boot (software WP flag).
	 */
	if ((~flash_get_protect()) & (EC_FLASH_PROTECT_GPIO_ASSERTED |
				      EC_FLASH_PROTECT_RO_AT_BOOT))
		return retval;

	if ((mask & EC_FLASH_PROTECT_ALL_AT_BOOT) &&
	    (flags & EC_FLASH_PROTECT_ALL_AT_BOOT)) {
		rv = flash_protect_at_boot(FLASH_WP_ALL);
		if (rv)
			retval = rv;
	}

	if ((mask & EC_FLASH_PROTECT_RO_NOW) &&
	    (flags & EC_FLASH_PROTECT_RO_NOW)) {
		rv = flash_physical_protect_now(0);
		if (rv)
			retval = rv;
	}

	if ((mask & EC_FLASH_PROTECT_ALL_NOW) &&
	    (flags & EC_FLASH_PROTECT_ALL_NOW)) {
		rv = flash_physical_protect_now(1);
		if (rv)
			retval = rv;
	}

	return retval;
}


/*****************************************************************************/
/* Host commands */


static int flash_command_protect(struct host_cmd_handler_args *args)
{
	const struct ec_params_flash_protect *p = args->params;
	struct ec_response_flash_protect *r = args->response;

	/*
	 * Handle requesting new flags.  Note that we ignore the return code
	 * from flash_set_protect(), since errors will be visible to the caller
	 * via the flags in the response.  (If we returned error, the caller
	 * wouldn't get the response.)
	 */
	if (p->mask)
		flash_set_protect(p->mask, p->flags);

	/*
	 * Retrieve the current flags.  The caller can use this to determine
	 * which of the requested flags could be set.  This is cleaner than
	 * simply returning error, because it provides information to the
	 * caller about the actual result.
	 */
	r->flags = flash_get_protect();

	/* Indicate which flags are valid on this platform */
	r->valid_flags =
		EC_FLASH_PROTECT_GPIO_ASSERTED |
		EC_FLASH_PROTECT_ERROR_STUCK |
		EC_FLASH_PROTECT_ERROR_INCONSISTENT |
		flash_physical_get_valid_flags();
	r->writable_flags = flash_physical_get_writable_flags(r->flags);

	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}

/*
 * TODO(crbug.com/239197) : Adding both versions to the version mask is a
 * temporary workaround for a problem in the cros_ec driver. Drop
 * EC_VER_MASK(0) once cros_ec driver can send the correct version.
 */
DECLARE_HOST_COMMAND(EC_CMD_FLASH_PROTECT,
		     flash_command_protect,
		     EC_VER_MASK(0) | EC_VER_MASK(1));

static int flash_command_region_info(struct host_cmd_handler_args *args)
{
	const struct ec_params_flash_region_info *p = args->params;
	struct ec_response_flash_region_info *r = args->response;

	switch (p->region) {
	case EC_FLASH_REGION_RO:
		r->offset = CONFIG_FW_RO_OFF;
		r->size = CONFIG_FW_RO_SIZE;
		break;
	case EC_FLASH_REGION_RW:
		r->offset = CONFIG_FW_RW_OFF;
		r->size = CONFIG_FW_RW_SIZE;
		break;
	case EC_FLASH_REGION_WP_RO:
		r->offset = CONFIG_FW_WP_RO_OFF;
		r->size = CONFIG_FW_WP_RO_SIZE;
		break;
	default:
		return EC_RES_INVALID_PARAM;
	}

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FLASH_REGION_INFO,
		     flash_command_region_info,
		     EC_VER_MASK(EC_VER_FLASH_REGION_INFO));
