/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT st_stm32_cros_flash

#include "assert.h"
#include "flash.h"
#include "hooks.h"
#include "host_command.h"
#include "system.h"
#include "watchdog.h"
#include "write_protect.h"

#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/flash/st_flash_api_extensions.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/cros_flash.h>
#include <soc.h>

LOG_MODULE_REGISTER(cros_flash, LOG_LEVEL_INF);

#define CROS_FLASH_STM32_PROT_VERSION 2
struct cros_flash_stm32_protection {
	bool access_disabled;
	bool option_disabled;
};

/* Device data */
struct cros_flash_stm32_data {
	FLASH_TypeDef *regs;
	struct cros_flash_stm32_protection protection;
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

	if (data->protection.access_disabled) {
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

	if (data->protection.access_disabled) {
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
	struct cros_flash_stm32_data *const data = DRV_DATA(dev);
	struct flash_stm32_ex_op_sector_wp_in wp_request;
	int err, first_err = 0;
	uint32_t range_sectors;

	if (data->protection.option_disabled) {
		return -EACCES;
	}

	wp_request.enable_mask = 0UL;
	wp_request.disable_mask = 0UL;

	if (write_protect_is_asserted()) {
		if (new_flags & EC_FLASH_PROTECT_ALL_AT_BOOT) {
			first_err = cros_flash_sector_mask_from_offset(
				flash_controller, 0, FLASH_SIZE,
				&wp_request.enable_mask);
		} else {
			err = cros_flash_sector_mask_from_offset(
				flash_controller, CONFIG_WP_STORAGE_OFF,
				CONFIG_WP_STORAGE_SIZE, &range_sectors);
			if (err == 0) {
				if (new_flags & EC_FLASH_PROTECT_RO_AT_BOOT)
					wp_request.enable_mask |= range_sectors;
				else
					wp_request.disable_mask |=
						range_sectors;
			} else if (first_err == 0) {
				first_err = err;
			}
#ifdef CONFIG_ROLLBACK
			err = cros_flash_sector_mask_from_offset(
				flash_controller, CONFIG_ROLLBACK_OFF,
				CONFIG_ROLLBACK_SIZE, &range_sectors);
			if (err == 0) {
				if (new_flags &
				    EC_FLASH_PROTECT_ROLLBACK_AT_BOOT)
					wp_request.enable_mask |= range_sectors;
				else
					wp_request.disable_mask |=
						range_sectors;
			} else if (first_err == 0) {
				first_err = err;
			}
#endif
#ifdef CONFIG_FLASH_PROTECT_RW
			err = cros_flash_sector_mask_from_offset(
				flash_controller,
				CONFIG_EC_WRITABLE_STORAGE_OFF,
				CONFIG_EC_WRITABLE_STORAGE_SIZE,
				&range_sectors);
			if (err == 0) {
				if (new_flags & EC_FLASH_PROTECT_RW_AT_BOOT)
					wp_request.enable_mask |= range_sectors;
				else
					wp_request.disable_mask |=
						range_sectors;
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
			LOG_ERR("Can't modify flash write protection, "
				"error: %d",
				err);
			if (first_err == 0)
				first_err = err;
		}
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

/**
 * Check if write protect register state is consistent with *_AT_BOOT in
 * prot_flags.
 *
 * @retval zero if consistent.
 * @retval positive value if inconsistent.
 * @retval negative value on error.
 */
static int wp_settings_are_incorrect(const struct device *dev,
				     uint32_t prot_flags)
{
	struct flash_stm32_ex_op_sector_wp_out wp_status;
	uint32_t range_sectors, expect_enabled = 0, expect_disabled = 0;
	int err, first_err = 0;
	bool consistent;

	if (prot_flags & EC_FLASH_PROTECT_ALL_AT_BOOT) {
		first_err = cros_flash_sector_mask_from_offset(
			flash_controller, 0, FLASH_SIZE, &range_sectors);
		expect_enabled = range_sectors;
	} else {
		err = cros_flash_sector_mask_from_offset(flash_controller,
							 CONFIG_WP_STORAGE_OFF,
							 CONFIG_WP_STORAGE_SIZE,
							 &range_sectors);
		if (err == 0) {
			if (prot_flags & EC_FLASH_PROTECT_RO_AT_BOOT)
				expect_enabled |= range_sectors;
			else
				expect_disabled |= range_sectors;
		} else if (first_err == 0) {
			first_err = err;
		}
#ifdef CONFIG_ROLLBACK
		err = cros_flash_sector_mask_from_offset(flash_controller,
							 CONFIG_ROLLBACK_OFF,
							 CONFIG_ROLLBACK_SIZE,
							 &range_sectors);
		if (err == 0) {
			if (prot_flags & EC_FLASH_PROTECT_ROLLBACK_AT_BOOT)
				expect_enabled |= range_sectors;
			else
				expect_disabled |= range_sectors;
		} else if (first_err == 0) {
			first_err = err;
		}
#endif
#ifdef CONFIG_FLASH_PROTECT_RW
		err = cros_flash_sector_mask_from_offset(
			flash_controller, CONFIG_EC_WRITABLE_STORAGE_OFF,
			CONFIG_EC_WRITABLE_STORAGE_SIZE, &range_sectors);
		if (err == 0) {
			if (prot_flags & EC_FLASH_PROTECT_RW_AT_BOOT)
				expect_enabled |= range_sectors;
			else
				expect_disabled |= range_sectors;
		} else if (first_err == 0) {
			first_err = err;
		}
#endif
	}

	err = flash_ex_op(flash_controller, FLASH_STM32_EX_OP_SECTOR_WP,
			  (uintptr_t)NULL, &wp_status);
	if (err) {
		LOG_ERR("Can't get flash write protect status, error: %d", err);
		return err;
	}

	/*
	 * Write protect settings are consistent when supported regions are
	 * entirely enabled or disabled. We don't care about sectors which are
	 * not covered by any region.
	 */
	consistent = (wp_status.protected_mask & expect_enabled) ==
			     expect_enabled &&
		     (wp_status.protected_mask & expect_disabled) == 0;

	return consistent ? 0 : 1;
}

static bool wp_all_disabled(const struct device *dev)
{
	struct flash_stm32_ex_op_sector_wp_out wp_status;

	if (flash_ex_op(flash_controller, FLASH_STM32_EX_OP_SECTOR_WP,
			(uintptr_t)NULL, &wp_status)) {
		return false;
	}

	return !wp_status.protected_mask;
}

static int cros_flash_stm32_disable_wp_all(const struct device *dev)
{
	struct cros_flash_stm32_data *const data = DRV_DATA(dev);
	struct flash_stm32_ex_op_sector_wp_in wp_request;
	int err;

	if (data->protection.option_disabled) {
		return -EACCES;
	}

	wp_request.enable_mask = 0UL;
	wp_request.disable_mask = 0UL;

	err = cros_flash_sector_mask_from_offset(
		flash_controller, 0, FLASH_SIZE, &wp_request.disable_mask);
	if (err == 0) {
		err = flash_ex_op(flash_controller, FLASH_STM32_EX_OP_SECTOR_WP,
				  (uintptr_t)&wp_request, NULL);
	}

	return err;
}

static int cros_flash_stm32_get_protect(const struct device *dev, int bank)
{
	struct cros_flash_stm32_data *const data = DRV_DATA(dev);
	struct flash_stm32_ex_op_sector_wp_out wp_status;

	if (data->protection.access_disabled) {
		return 1;
	}

	if (flash_ex_op(flash_controller, FLASH_STM32_EX_OP_SECTOR_WP,
			(uintptr_t)NULL, &wp_status)) {
		return 0;
	}

	return (wp_status.protected_mask & (1UL << bank)) ? 1 : 0;
}

#if defined(CONFIG_CROS_FLASH_STM32_READOUT_PROTECTION)
/**
 * Check if readout protection is enabled.
 *
 * @retval positive value if RDP is enabled.
 * @retval zero if RDP is disabled.
 * @retval negative value on error.
 */
static int rdp_is_enabled(const struct device *dev)
{
	struct flash_stm32_ex_op_rdp rdp_status;
	int err = flash_ex_op(flash_controller, FLASH_STM32_EX_OP_RDP,
			      (uintptr_t)NULL, &rdp_status);

	if (err)
		return err;

	return rdp_status.enable ? 1 : 0;
}
#endif

static uint32_t cros_flash_stm32_get_protect_flags(const struct device *dev)
{
	struct cros_flash_stm32_data *const data = DRV_DATA(dev);
	uint32_t flags = 0;

	/* Read all-protected state from our shadow copy */
	if (data->protection.access_disabled)
		flags |= EC_FLASH_PROTECT_ALL_NOW;

#if defined(CONFIG_CROS_FLASH_STM32_READOUT_PROTECTION)
	/* Readout protection as a PSTATE */
	int rdp = rdp_is_enabled(dev);

	if (rdp < 0)
		flags |= EC_FLASH_PROTECT_ERROR_UNKNOWN;
	else if (rdp == 1)
		flags |= EC_FLASH_PROTECT_RO_AT_BOOT;
#endif

	return flags;
}

static void disable_option_bytes(const struct device *dev)
{
	struct cros_flash_stm32_data *const data = DRV_DATA(dev);

	LOG_INF("Disabling option bytes");

	__set_FAULTMASK(1);
	data->regs->OPTKEYR = 0xffffffff;

	/* Clear Bus Fault pending bit */
	SCB->SHCSR &= ~SCB_SHCSR_BUSFAULTPENDED_Msk;
	__set_FAULTMASK(0);

	data->protection.option_disabled = true;
}

static void disable_control_register(const struct device *dev)
{
	struct cros_flash_stm32_data *const data = DRV_DATA(dev);

	LOG_INF("Disabling control register");

	__set_FAULTMASK(1);
	data->regs->KEYR = 0xffffffff;

	/* Clear Bus Fault pending bit */
	SCB->SHCSR &= ~SCB_SHCSR_BUSFAULTPENDED_Msk;
	__set_FAULTMASK(0);

	data->protection.access_disabled = true;
}

static int cros_flash_stm32_protect_now(const struct device *dev, int all)
{
	disable_option_bytes(dev);

	if (all)
		disable_control_register(dev);

	return EC_SUCCESS;
}

#define FLASH_SYSJUMP_TAG 0x5750 /* "WP" - Write Protect */

#ifdef CONFIG_CROS_FLASH_STM32_PROTECTION_COMPAT
#ifdef CONFIG_SOC_SERIES_STM32F4X
struct jump_wp_state {
	int entire_flash_locked;
};

static int
decode_wp_from_sysjump(struct cros_flash_stm32_protection *protection,
		       uint32_t prot_flags, const struct jump_wp_state *wp,
		       size_t size, int version)
{
	if (size != sizeof(*wp) || version != 1)
		return -EINVAL;

	if (wp->entire_flash_locked) {
		protection->access_disabled = true;
		protection->option_disabled = true;
	} else if (prot_flags & EC_FLASH_PROTECT_RO_NOW) {
		/*
		 * If RO_NOW flag is set we know that RO image disabled option
		 * bytes.
		 */
		protection->option_disabled = true;
	}

	return EC_SUCCESS;
}

static void prepare_wp_jump(struct cros_flash_stm32_protection *protection)
{
	struct jump_wp_state wp_state;

	wp_state.entire_flash_locked = 0;

	if (protection->access_disabled)
		wp_state.entire_flash_locked = 1;

	system_add_jump_tag(FLASH_SYSJUMP_TAG, 1, sizeof(wp_state), &wp_state);
}
#endif /* CONFIG_SOC_SERIES_STM32F4X */
#endif /* CONFIG_CROS_FLASH_STM32_PROTECTION_COMPAT */

static void cros_flash_stm32_restore_state(const struct device *dev,
					   uint32_t prot_flags)
{
	struct cros_flash_stm32_data *const data = DRV_DATA(dev);
	uint32_t reset_flags = system_get_reset_flags();
	int version, size;
	const void *prev;

	if (!(reset_flags & EC_RESET_FLAG_SYSJUMP))
		return;

	prev = system_get_jump_tag(FLASH_SYSJUMP_TAG, &version, &size);
	if (!prev)
		return;

	if (version == CROS_FLASH_STM32_PROT_VERSION &&
	    size == sizeof(struct cros_flash_stm32_protection)) {
		const struct cros_flash_stm32_protection *prev_prot =
			(const struct cros_flash_stm32_protection *)prev;
		data->protection.access_disabled = prev_prot->access_disabled;
		data->protection.option_disabled = prev_prot->option_disabled;

		return;
	}

#ifdef CONFIG_CROS_FLASH_STM32_PROTECTION_COMPAT
	if (version == 1) {
		if (decode_wp_from_sysjump(&data->protection, prot_flags, prev,
					   size, version) == EC_SUCCESS)
			return;
	}
#endif /* CONFIG_CROS_FLASH_STM32_PROTECTION_COMPAT */
}

static void cros_flash_stm32_preserve_state(void)
{
	struct cros_flash_stm32_data *const data =
		DRV_DATA(DEVICE_DT_GET(DT_CHOSEN(cros_ec_flash_controller)));

#ifndef CONFIG_CROS_FLASH_STM32_PROTECTION_COMPAT
	system_add_jump_tag(FLASH_SYSJUMP_TAG, CROS_FLASH_STM32_PROT_VERSION,
			    sizeof(struct cros_flash_stm32_protection),
			    &data->protection);
#else
	prepare_wp_jump(&data->protection);
#endif /* CONFIG_CROS_FLASH_STM32_PROTECTION_COMPAT */
}
DECLARE_HOOK(HOOK_SYSJUMP, cros_flash_stm32_preserve_state, HOOK_PRIO_DEFAULT);

static int cros_flash_stm32_init(const struct device *dev)
{
	uint32_t reset_flags = system_get_reset_flags();
	uint32_t prot_flags = crec_flash_get_protect();
	int need_reset = 0;

	/*
	 * If we have already jumped between images, an earlier image could
	 * have applied write protection. Restore protection information and
	 * exit.
	 */
	if (reset_flags & EC_RESET_FLAG_SYSJUMP) {
		cros_flash_stm32_restore_state(dev, prot_flags);
		return EC_SUCCESS;
	}

	if (prot_flags & EC_FLASH_PROTECT_GPIO_ASSERTED) {
		if (
#if defined(CONFIG_CROS_FLASH_STM32_READOUT_PROTECTION)
			((prot_flags & EC_FLASH_PROTECT_RO_AT_BOOT) &&
			 rdp_is_enabled(dev) <= 0) ||
#endif
			wp_settings_are_incorrect(dev, prot_flags) != 0) {
			/*
			 * Fix incorrect WP settings and RDP settings if
			 * RO_AT_BOOT is set.
			 *
			 * RO_AT_BOOT is set but the RO_NOW is not.
			 * It means that we have RO protection enabled somewhere
			 * (PSTATE, hardcoded, RDP) but it's not actually
			 * enabled now.
			 *
			 * wp_settings_are_incorrect() checks whether write
			 * protection on all supported regions matches AT_BOOT
			 * flag. It doesn't care about sectors not covered by
			 * any region. We can't check INCONSISTENT flag here
			 * because it's also reported if:
			 * - Write protection is enabled on a sector that is not
			 *   covered by any region.
			 * - Other regions are protected but RO is not,
			 *   regardless of RO_AT_BOOT flag.
			 * In above cases we will just leave the write
			 * protection inconsistent.
			 *
			 * All of these problems can be fixed by requesting
			 * *_AT_BOOT flags again. We can safely call function
			 * from this driver, because we are not setting any new
			 * flags. After enabling, we request a reset, so we can
			 * check it once again.
			 */
			cros_flash_stm32_protect_at_boot(dev, prot_flags);
			need_reset = 1;
		} else if ((prot_flags & EC_FLASH_PROTECT_RO_AT_BOOT) &&
			   (prot_flags & EC_FLASH_PROTECT_RO_NOW)) {
			/*
			 * Here RO is fully protected and write protect settings
			 * are consistent.
			 *
			 * Disable option bytes until next boot. The intention
			 * here is to prevent from disabling sector write
			 * protection. Please note that this has also following
			 * side effects:
			 * - It's not possible to enable write protection.
			 * - It's not possible to enable RDP protection.
			 */
			disable_option_bytes(dev);
		}
	} else {
		if (!wp_all_disabled(dev)) {
			/*
			 * Write protect pin unasserted but some section is
			 * protected. Drop it and reboot.
			 *
			 * Please note that some additional protection
			 * (e.g. RDP) will be still enabled.
			 */
			if (cros_flash_stm32_disable_wp_all(dev) == 0)
				need_reset = 1;
		}
	}

	if (need_reset)
		system_reset(SYSTEM_RESET_HARD | SYSTEM_RESET_PRESERVE_FLAGS);

	return EC_SUCCESS;
}

/* CrOS EC flash driver registration. */
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
