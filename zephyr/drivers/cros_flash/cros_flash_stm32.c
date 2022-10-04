/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT st_stm32_cros_flash

#include "assert.h"
#include "flash.h"
#include "host_command.h"
#include "system.h"
#include "watchdog.h"

#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/flash/st_flash_api_extensions.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/cros_flash.h>
#include <soc.h>

LOG_MODULE_REGISTER(cros_flash, LOG_LEVEL_DBG);

/* Device data */
struct cros_flash_stm32_data {
	FLASH_TypeDef *regs;
	bool all_protected;
};

/* Driver convenience defines */
#define DRV_DATA(dev) ((struct cros_flash_stm32_data *)(dev)->data)
#define FLASH_STM32_REGS(dev) (DRV_DATA(dev)->regs)

#define FLASH_SIZE DT_REG_SIZE(DT_CHOSEN(zephyr_flash))

static const struct device *const flash_controller =
	DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller));

/* cros ec flash api functions */
static int cros_flash_stm32_write(const struct device *dev, int offset,
				  int size, const char *src_data)
{
	struct cros_flash_stm32_data *const data = DRV_DATA(dev);

	if (data->all_protected) {
		return -EACCES;
	}

	/*
	 * If AP sends write flash command continuously, EC might not have
	 * chance to go back to hook task to touch watchdog. Reload watchdog
	 * on each flash write to prevent the reset.
	 */
	if (IS_ENABLED(CONFIG_PLATFORM_EC_WATCHDOG))
		watchdog_reload();

	return flash_write(flash_controller, offset, src_data, size);
}

static int cros_flash_stm32_erase(const struct device *dev, int offset,
				  int size)
{
	struct cros_flash_stm32_data *const data = DRV_DATA(dev);

	if (data->all_protected) {
		return -EACCES;
	}

	if (IS_ENABLED(CONFIG_PLATFORM_EC_WATCHDOG))
		watchdog_reload();

	return flash_erase(flash_controller, offset, size);
}

static int cros_flash_sector_mask_from_offset(const struct device *dev,
					      off_t offset, size_t size,
					      uint32_t *sector_mask)
{
	struct flash_pages_info start_page, end_page;

	if (flash_get_page_info_by_offs(dev, offset, &start_page) ||
	    flash_get_page_info_by_offs(dev, offset + size - 1, &end_page)) {
		LOG_ERR("Flash range invalid. offset: 0x%lx, len: 0x%zx",
			(long)offset, size);
		return -EINVAL;
	}

	/* Check if sectors don't cover wider range than requested. */
	if (start_page.start_offset != offset ||
	    end_page.start_offset + end_page.size != offset + size) {
		LOG_ERR("Range covered by sectors doesn't match requested "
			"range. Requested (0x%lx, 0x%lx), "
			"covered (0x%lx, 0x%lx).",
			offset, offset + size - 1, start_page.start_offset,
			end_page.start_offset + end_page.size - 1);
		return -EINVAL;
	}

	*sector_mask = ((1UL << (end_page.index + 1)) - 1) &
		       ~((1UL << start_page.index) - 1);
	LOG_DBG("Sector mask for offset 0x%lx, size 0x%zx is 0x%x", offset,
		size, *sector_mask);

	return 0;
}

static int cros_flash_stm32_protect_at_boot(const struct device *dev,
					    uint32_t new_flags)
{
	struct flash_stm32_ex_op_sector_wp_in wp_request;
	int err, first_err = 0;
	uint32_t range_sectors;

	wp_request.enable_mask = 0UL;
	wp_request.disable_mask = 0UL;

	if (new_flags & EC_FLASH_PROTECT_ALL_AT_BOOT) {
		err = cros_flash_sector_mask_from_offset(
			flash_controller, 0, FLASH_SIZE,
			&wp_request.enable_mask);
		if (err != 0 && first_err == 0)
			first_err = err;
	} else {
		err = cros_flash_sector_mask_from_offset(flash_controller,
							 CONFIG_WP_STORAGE_OFF,
							 CONFIG_WP_STORAGE_SIZE,
							 &range_sectors);
		if (err == 0) {
			if (new_flags & EC_FLASH_PROTECT_RO_AT_BOOT)
				wp_request.enable_mask |= range_sectors;
			else
				wp_request.disable_mask |= range_sectors;
		} else if (first_err == 0) {
			first_err = err;
		}
#ifdef CONFIG_ROLLBACK
		err = cros_flash_sector_mask_from_offset(flash_controller,
							 CONFIG_ROLLBACK_OFF,
							 CONFIG_ROLLBACK_SIZE,
							 &range_sectors);
		if (err == 0) {
			if (new_flags & EC_FLASH_PROTECT_ROLLBACK_AT_BOOT)
				wp_request.enable_mask |= range_sectors;
			else
				wp_request.disable_mask |= range_sectors;
		} else if (first_err == 0) {
			first_err = err;
		}
#endif
#ifdef CONFIG_FLASH_PROTECT_RW
		err = cros_flash_sector_mask_from_offset(
			flash_controller, CONFIG_EC_WRITABLE_STORAGE_OFF,
			CONFIG_EC_WRITABLE_STORAGE_SIZE, &range_sectors);
		if (err == 0) {
			if (new_flags & EC_FLASH_PROTECT_RW_AT_BOOT)
				wp_request.enable_mask |= range_sectors;
			else
				wp_request.disable_mask |= range_sectors;
		} else if (first_err == 0) {
			first_err = err;
		}
#endif
	}

	/* Commit write protect changes */
	LOG_INF("Commit WP changes: disabling: 0x%x, enabling: 0x%x",
		wp_request.disable_mask, wp_request.enable_mask);
	err = flash_ex_op(flash_controller, FLASH_STM32_EX_OP_SECTOR_WP,
			  (uintptr_t)&wp_request, NULL);
	if (err != 0) {
		LOG_ERR("Can't modify flash write protection, error: %d", err);
		if (first_err == 0)
			first_err = err;
	}

#if defined(CONFIG_CROS_FLASH_STM32_READOUT_PROTECTION)
	/*
	 * Enable readout protection if RO_AT_BOOT is set.
	 *
	 * This is intentionally a one-way latch. Once we have enabled RDP
	 * Level 1, we will only allow going back to Level 0 using the
	 * bootloader (e.g., "stm32mon -U") since transitioning from Level 1 to
	 * Level 0 triggers a mass erase.
	 */
	if (new_flags & EC_FLASH_PROTECT_RO_AT_BOOT) {
		struct flash_stm32_ex_op_rdp enable_rdp = {
			.enable = true,
			.permanent = false,
		};

		err = flash_ex_op(flash_controller, FLASH_STM32_EX_OP_RDP,
				  (uintptr_t)&enable_rdp, NULL);
		if (err != 0) {
			LOG_ERR("Can't enable RDP, error: %d", err);
			if (first_err == 0) {
				first_err = err;
			}
		}
	}
#endif

	return first_err;
}

static int cros_flash_stm32_disable_wp_all(const struct device *dev)
{
	struct flash_stm32_ex_op_sector_wp_in wp_request;
	int err;

	wp_request.enable_mask = 0UL;
	wp_request.disable_mask = 0UL;

	err = cros_flash_sector_mask_from_offset(dev, 0, FLASH_SIZE,
						 &wp_request.disable_mask);
	if (err == 0) {
		err = flash_ex_op(dev, FLASH_STM32_EX_OP_SECTOR_WP,
				  (uintptr_t)&wp_request, NULL);
	}

	return err;
}

static int cros_flash_stm32_get_protect(const struct device *dev, int bank)
{
	struct cros_flash_stm32_data *const data = DRV_DATA(dev);
	struct flash_stm32_ex_op_sector_wp_out wp_status;

	if (data->all_protected) {
		return 1;
	}

	if (flash_ex_op(flash_controller, FLASH_STM32_EX_OP_SECTOR_WP,
			(uintptr_t)NULL, &wp_status)) {
		return 0;
	}

	return (wp_status.protected_mask & (1UL << bank)) ? 1 : 0;
}

static uint32_t cros_flash_stm32_get_protect_flags(const struct device *dev)
{
	struct cros_flash_stm32_data *const data = DRV_DATA(dev);
	uint32_t flags = 0;
	struct flash_stm32_ex_op_rdp rdp_status;

	/* Read all-protected state from our shadow copy */
	if (data->all_protected)
		flags |= EC_FLASH_PROTECT_ALL_NOW;

#if defined(CONFIG_CROS_FLASH_STM32_READOUT_PROTECTION)
	if (flash_ex_op(flash_controller, FLASH_STM32_EX_OP_RDP,
			(uintptr_t)NULL, &rdp_status))
		flags |= EC_FLASH_PROTECT_ERROR_UNKNOWN;
	else if (rdp_status.enable)
		flags |= EC_FLASH_PROTECT_RO_AT_BOOT;
#endif

	return flags;
}

static int cros_flash_stm32_protect_now(const struct device *dev, int all)
{
	struct cros_flash_stm32_data *const data = DRV_DATA(dev);

	/*
	 * TODO: Protect now was disabling flash control and option registers
	 * need to figure out good implementation for this.
	 */

	if (all) {
		data->all_protected = true;

		return EC_SUCCESS;
	}

	return EC_SUCCESS;
}

/*
 * The previous write protect state before sys jump
 * WARNING! The structure for STM32H7 is different, so this driver needs to be
 * reorganized.
 */
struct flash_wp_state {
	int entire_flash_locked;
};
#define FLASH_SYSJUMP_TAG 0x5750 /* "WP" - Write Protect */
#define FLASH_HOOK_VERSION 1
static int crec_flash_restore_state(const struct device *dev)
{
	struct cros_flash_stm32_data *const data = DRV_DATA(dev);
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
			data->all_protected = prev->entire_flash_locked;
		return 1;
	}

	return 0;
}

/* ADD SAVING DATA ALSO */

/**
 * Check if write protect register state is inconsistent with RO_AT_BOOT and
 * ALL_AT_BOOT state.
 *
 * @return zero if consistent, non-zero if inconsistent.
 */
static int registers_need_reset(const struct device *dev)
{
	struct flash_stm32_ex_op_sector_wp_out wp_status;
	bool ro_at_boot =
		(crec_flash_get_protect() & EC_FLASH_PROTECT_RO_AT_BOOT) ?
			true :
			false;
	uint32_t range_sectors;
	bool enabled;
	int err;

	err = cros_flash_sector_mask_from_offset(flash_controller,
						 CONFIG_WP_STORAGE_OFF,
						 CONFIG_WP_STORAGE_SIZE,
						 &range_sectors);
	if (err)
		return err;

	err = flash_ex_op(flash_controller, FLASH_STM32_EX_OP_SECTOR_WP,
			  (uintptr_t)NULL, &wp_status);
	if (err) {
		LOG_ERR("Can't get flash write protect status, error: %d", err);
		return err;
	}

	enabled = (wp_status.protected_mask & range_sectors) == range_sectors;

	return ro_at_boot == enabled ? 0 : 1;
}

static uint32_t crec_flash_stm32_get_valid_flags(void)
{
	return EC_FLASH_PROTECT_RO_AT_BOOT | EC_FLASH_PROTECT_RO_NOW |
	       EC_FLASH_PROTECT_ALL_NOW;
}

static int cros_flash_stm32_init(const struct device *dev)
{
	uint32_t reset_flags = system_get_reset_flags();
	uint32_t prot_flags = crec_flash_get_protect();
	int need_reset = 0;

	if (crec_flash_restore_state(dev))
		return EC_SUCCESS;

	/*
	 * If we have already jumped between images, an earlier image could
	 * have applied write protection. Nothing additional needs to be done.
	 */
	if (reset_flags & EC_RESET_FLAG_SYSJUMP) {
		LOG_ERR("%s: jumped!", __func__);
		return EC_SUCCESS;
	}

	if (prot_flags & EC_FLASH_PROTECT_GPIO_ASSERTED) {
		if (prot_flags & EC_FLASH_PROTECT_RO_NOW) {
			/* Enable physical protection for RO (0 means RO). */
			crec_flash_physical_protect_now(0);
		}

		if ((prot_flags & EC_FLASH_PROTECT_RO_AT_BOOT) &&
		    !(prot_flags & EC_FLASH_PROTECT_RO_NOW)) {
			/*
			 * Pstate wants RO protected at boot, but the write
			 * protect register wasn't set to protect it.  Force an
			 * update to the write protect register and reboot so
			 * it takes effect.
			 */
			cros_flash_stm32_protect_at_boot(
				dev, EC_FLASH_PROTECT_RO_AT_BOOT);
			need_reset = 1;
		}

		if (registers_need_reset(dev)) {
			/*
			 * Write protect register was in an inconsistent state.
			 * Set it back to a good state and reboot.
			 *
			 * TODO(crosbug.com/p/23798): this seems really similar
			 * to the check above.  One of them should be able to
			 * go away.
			 */
			crec_flash_protect_at_boot(prot_flags &
						   EC_FLASH_PROTECT_RO_AT_BOOT);
			need_reset = 1;
		}
	} else {
		if (prot_flags & EC_FLASH_PROTECT_RO_NOW) {
			/*
			 * Write protect pin unasserted but some section is
			 * protected. Drop it and reboot.
			 */
			if (cros_flash_stm32_disable_wp_all(flash_controller) ==
			    0)
				need_reset = 1;
		}
	}

	if ((crec_flash_stm32_get_valid_flags() &
	     EC_FLASH_PROTECT_ALL_AT_BOOT) &&
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
	if ((crec_flash_stm32_get_valid_flags() &
	     EC_FLASH_PROTECT_RW_AT_BOOT) &&
	    (!!(prot_flags & EC_FLASH_PROTECT_RW_AT_BOOT) !=
	     !!(prot_flags & EC_FLASH_PROTECT_RW_NOW))) {
		/* RW_AT_BOOT and RW_NOW do not match. */
		need_reset = 1;
	}
#endif

#ifdef CONFIG_ROLLBACK
	if ((crec_flash_stm32_get_valid_flags() &
	     EC_FLASH_PROTECT_ROLLBACK_AT_BOOT) &&
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

/* cros ec flash driver registration */
static const struct cros_flash_driver_api cros_flash_stm32_driver_api = {
	.init = cros_flash_stm32_init,
	.physical_write = cros_flash_stm32_write,
	.physical_erase = cros_flash_stm32_erase,
	.physical_get_protect = cros_flash_stm32_get_protect,
	.physical_get_protect_flags = cros_flash_stm32_get_protect_flags,
	.physical_protect_at_boot = cros_flash_stm32_protect_at_boot,
	.physical_protect_now = cros_flash_stm32_protect_now,
};

static int flash_stm32_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	if (!device_is_ready(flash_controller)) {
		LOG_ERR("Selected flash device %s is not ready",
			flash_controller->name);
		return -ENODEV;
	}

	return 0;
}

static struct cros_flash_stm32_data cros_flash_data = {
	.regs = (FLASH_TypeDef *)DT_INST_REG_ADDR(0),
};

#if CONFIG_CROS_FLASH_STM32_INIT_PRIORITY <= CONFIG_FLASH_INIT_PRIORITY
#error "CONFIG_CROS_FLASH_STM32_INIT_PRIORITY must be greater than" \
	"CONFIG_FLASH_INIT_PRIORITY."
#endif

DEVICE_DT_INST_DEFINE(0, flash_stm32_init, NULL, &cros_flash_data, NULL,
		      POST_KERNEL, CONFIG_CROS_FLASH_STM32_INIT_PRIORITY,
		      &cros_flash_stm32_driver_api);
