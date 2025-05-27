/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* 
 * Desing description.
 *
 * Possible option for flash protection higly depends on hardware.
 * This driver is implemented for Andes based chips (e.g. Egis ET171),
 * using XIP mode. Such chips uses SPI for fetching instruction from flash.
 * There are no flash controller that introduces additional features.
 * From hardware perspective, only flash protection provided by connected
 * flash chip is avaiable.
 *
 * Additianaly, such chips requires QSPI mode, to speed up execution (faster
 * instructions fetching). That means, the hardware WP# pin can not be used,
 * because it is used for data transfer. Software locking of the status and
 * configuration registers has been added in flash driver to somehow emulate
 * behaviour of the WP# pin.
 *
 * The chip flash protection is used to identify _AT_BOOT flasgs.
 * The flash protection, in combination with locked register indentifies _NOW
 * flags.
 * 
 * It means, _NOW flags are cleared during sysjump, because the register lock
 * is cleared. To "reduce" that effect, the register lock is always enabled,
 * at the beggining of the boot if GPIO_WP is set.
 *
 * The driver allows extending flash protection range, even if the registers lock
 * is enabled.
 */

#define DT_DRV_COMPAT andestech_qspi_nor_xip_cros_flash

#include "../drivers/flash/spi_nor.h"
#include "flash.h"
#include "spi_flash_reg.h"
#include "write_protect.h"

#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/flash/andes_flash_xip_api_ex.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/cros_flash.h>
#include <soc.h>

LOG_MODULE_REGISTER(cros_flash, LOG_LEVEL_ERR);

struct cros_flash_andestech_xip_data {
	const struct device *flash_dev;
};

#define SPI_FLASH_CR_WPS BIT(2)

#define FLASH_DEV DT_CHOSEN(zephyr_flash_controller)

#define DRV_DATA(dev) ((struct cros_flash_andestech_xip_data *)(dev)->data)

static int cros_flash_andes_xip_get_status_regs(const struct device *dev, struct andes_xip_ex_ops_get_out *op_out)
{
	struct cros_flash_andestech_xip_data *data = DRV_DATA(dev);

	return flash_ex_op(data->flash_dev, FLASH_ANDES_XIP_EX_OP_GET_STATUS_REGS, (uintptr_t)NULL, op_out);
}

static int set_status_regs(const struct device *dev, struct andes_xip_ex_ops_set_in *op_in)
{
	struct cros_flash_andestech_xip_data *data = DRV_DATA(dev);

	return flash_ex_op(data->flash_dev, FLASH_ANDES_XIP_EX_OP_SET_STATUS_REGS, (uintptr_t)op_in, 0);
}

static int lock_status(const struct device *dev, bool enable)
{
	struct cros_flash_andestech_xip_data *data = DRV_DATA(dev);
	struct andes_xip_ex_ops_lock_in op_in = {
		.enable = enable,
	};

	return flash_ex_op(data->flash_dev, FLASH_ANDES_XIP_EX_OP_LOCK, (uintptr_t)&op_in, 0);
}

static int get_lock_status(const struct device *dev, bool *state)
{
	int ret;
	struct andes_xip_ex_ops_lock_state_out op_out;
	struct cros_flash_andestech_xip_data *data = DRV_DATA(dev);

	ret = flash_ex_op(data->flash_dev, FLASH_ANDES_XIP_EX_OP_LOCK_STATE, (uintptr_t)NULL, &op_out);
	if (!ret) {
		*state = op_out.state;
	}

	return ret;
}

static int cros_flash_andes_xip_get_status(const struct device *dev, uint8_t *sr1, uint8_t *sr2)
{
	struct andes_xip_ex_ops_get_out op_out;
	int ret;

	ret = cros_flash_andes_xip_get_status_regs(dev, &op_out);
	if (!ret) {
		*sr1 = op_out.regs[0];
		*sr2 = op_out.regs[1];
	}

	return ret;
}

static int get_prot_reg(const struct device *dev, uint32_t *start, uint32_t *end)
{
	unsigned int len;
	uint8_t sr1, sr2;
	int ret;

	/* Compute current protect range */
	ret = cros_flash_andes_xip_get_status(dev, &sr1, &sr2);
	if (ret) {
		return ret;
	}

	ret = spi_flash_reg_to_protect(sr1, sr2, start, &len);
	if (ret) {
		return ret;
	}
	*end = *start + len;

	return EC_SUCCESS;
}

static int check_prot_reg(const struct device *dev, unsigned int offset, unsigned int bytes)
{
	uint32_t prot_start, prot_end;
	int ret;

	/* Validate input params */
	if ((bytes > CONFIG_FLASH_SIZE_BYTES) || ((CONFIG_FLASH_SIZE_BYTES - bytes) < offset)) {
		return EC_ERROR_INVAL;
	}

	ret = get_prot_reg(dev, &prot_start, &prot_end);
	if (ret) {
		return ret;
	}

	/* Check if ranges overlap. Protection always starts from 0. */
	if (offset < prot_end)
		return EC_ERROR_ACCESS_DENIED;

	return EC_SUCCESS;
}

static int set_status_for_prot(const struct device *dev, uint8_t reg1, uint8_t reg2)
{
	int ret;
	struct andes_xip_ex_ops_set_in op_in;

	/* BP0-4 bits. */
	op_in.masks[0] = 0x7c;
	/* CMP bit. */
	op_in.masks[1] = 0x40;
	op_in.masks[2] = 0;

	op_in.regs[0] = reg1;
	op_in.regs[1] = reg2;

	/* Update only protecion related bits */
	ret = set_status_regs(dev, &op_in);

	return ret;
}

static int set_flash_prot(const struct device *dev, uint32_t offset, uint32_t bytes)
{
	int rv;
	uint8_t sr1, sr2;

	/* Validate input params */
	if ((bytes > CONFIG_FLASH_SIZE_BYTES) || ((CONFIG_FLASH_SIZE_BYTES - bytes) < offset)) {
		return EC_ERROR_INVAL;
	}

	/* Compute desired protect range */
	rv = spi_flash_protect_to_reg(offset, bytes, &sr1, &sr2);
	if (rv) {
		return rv;
	}

	return set_status_for_prot(dev, sr1, sr2);
}

static int cros_flash_andes_xip_init(const struct device *dev)
{
	struct andes_xip_ex_ops_set_in op_in = {
		.regs = {0}
	};
	int ret;

	/* Make sure to clear SRP bits - only software protection. */
	op_in.masks[0] = SPI_FLASH_SR1_SRP0;
	op_in.masks[1] = SPI_FLASH_SR2_SRP1;
	/* Make sure to clear WPS bit - per block protection. */
	op_in.masks[2] = SPI_FLASH_CR_WPS;

	ret = set_status_regs(dev, &op_in);
	if (ret) {
		return ret;
	}

	if (write_protect_is_asserted()) {
		/* Emulate behaviour of #WP pin and lock status registers. */
		ret = lock_status(dev, true);
	}

	return ret;
}	

static uint32_t cros_flash_andes_xip_get_protect_flags(const struct device *dev)
{
	uint32_t flags = 0;
	struct andes_xip_ex_ops_get_out op_out;
	unsigned int prot_start;
	unsigned int prot_end;
	unsigned int prot_len;
	int ret;
	bool status_locked;

	ret = get_lock_status(dev, &status_locked);
	if (ret) {
		return ret;
	}

	ret = cros_flash_andes_xip_get_status_regs(dev, &op_out);
	if (ret) {
		return EC_FLASH_PROTECT_ERROR_UNKNOWN;
	}

	/*
	 * Make sure per block protection is disabled (WPS) and only software write protecion is enabled.
	 * QSPI mode is required to speed up the XIP mode. That means the WP pin is not used for protection.
	 */
	if ((op_out.regs[0] & SPI_FLASH_SR1_SRP0) || (op_out.regs[1] & SPI_FLASH_SR2_SRP1) || (op_out.regs[2] & SPI_FLASH_CR_WPS)) {
		flags |= EC_FLASH_PROTECT_ERROR_INCONSISTENT;
	}

	ret = spi_flash_reg_to_protect(op_out.regs[0], op_out.regs[1], &prot_start, &prot_len);
	if (ret) {
		return EC_FLASH_PROTECT_ERROR_UNKNOWN;
	}
	prot_end = prot_start + prot_len;

	/* Check if ranges overlap */
	if (prot_start <= CONFIG_WP_STORAGE_OFF) {
		/* This logic assumes a certain flash layout: RO -> ROLLBACKS -> RW. */
		if (prot_end >= (CONFIG_RW_MEM_OFF + CONFIG_RW_SIZE)) {
			flags |= EC_FLASH_PROTECT_ALL_AT_BOOT | EC_FLASH_PROTECT_RO_AT_BOOT;
#ifdef CONFIG_ROLLBACK
			flags |= EC_FLASH_PROTECT_ROLLBACK_AT_BOOT;
		} else if (prot_end >= (CONFIG_ROLLBACK_OFF + CONFIG_ROLLBACK_SIZE)) {
			flags |= EC_FLASH_PROTECT_RO_AT_BOOT | EC_FLASH_PROTECT_ROLLBACK_AT_BOOT;
#endif /* CONFIG_ROLLBACK */
		} else if (prot_end >= (CONFIG_WP_STORAGE_OFF + CONFIG_WP_STORAGE_SIZE)) {
			flags |= EC_FLASH_PROTECT_RO_AT_BOOT;
		}
	}

	/* Status registers are locked. Add proper _NOW flags. */
	if (status_locked) {
		if (flags & EC_FLASH_PROTECT_ALL_AT_BOOT) {
			flags |= EC_FLASH_PROTECT_ALL_NOW | EC_FLASH_PROTECT_RO_NOW;
#ifdef CONFIG_ROLLBACK
			flags |= EC_FLASH_PROTECT_ROLLBACK_NOW;
		} else if (flags & EC_FLASH_PROTECT_ROLLBACK_AT_BOOT) {
			flags |= EC_FLASH_PROTECT_ROLLBACK_NOW | EC_FLASH_PROTECT_RO_NOW;
#endif /* CONFIG_ROLLBACK */
		} else if (flags & EC_FLASH_PROTECT_RO_AT_BOOT) {
			flags |= EC_FLASH_PROTECT_RO_NOW;
		}
	}

	return flags;
}

static int cros_flash_andes_xip_get_protect(const struct device *dev, int bank)
{
	uint32_t addr_start = bank * CONFIG_FLASH_BANK_SIZE;
	int ret;
	bool status_locked;

	ret = get_lock_status(dev, &status_locked);
	if (ret) {
		return ret;
	}

	/* 
	 * The physical_get_protect returns protection state untill reboot,
	 * so it is used to determine *_NOW flags.
	 *
	 * Make sure flash protection is enabled and status registers are locked.
	 */
	if (status_locked || (addr_start < CONFIG_WP_STORAGE_OFF)) {
		ret = check_prot_reg(dev, addr_start, CONFIG_FLASH_BANK_SIZE);
	} else {
		return EC_SUCCESS;
	}

	return ret;
}

static int cros_flash_andes_xip_protect_at_boot(const struct device *dev,
					   uint32_t new_flags)
{
	int ret;
	bool status_locked;
	uint32_t new_prot_end;

	ret = get_lock_status(dev, &status_locked);
	if (ret) {
		return ret;
	}

	/* There is no independent protection of each section. Protection of WP is within pretection ranges of Rollbacks */
	if (new_flags & (EC_FLASH_PROTECT_ALL_AT_BOOT)) {
		new_prot_end = CONFIG_RW_MEM_OFF + CONFIG_RW_SIZE;
	} else if (new_flags & EC_FLASH_PROTECT_ROLLBACK_AT_BOOT) {
		new_prot_end = CONFIG_ROLLBACK_OFF + CONFIG_ROLLBACK_SIZE;
	} else if (new_flags & EC_FLASH_PROTECT_RO_AT_BOOT) {
		new_prot_end = CONFIG_WP_STORAGE_OFF + CONFIG_WP_STORAGE_SIZE;
	} else {
		/* Disable protection of WP section, but do not disable protection of the flash header. */
		new_prot_end = CONFIG_WP_STORAGE_OFF;
	}

	if (!status_locked) {
		return set_flash_prot(dev, 0, new_prot_end);
	} else {
		uint32_t curr_prot_start, curr_prot_end;

		ret = get_prot_reg(dev, &curr_prot_start, &curr_prot_end);
		if (ret) {
			return ret;
		}

		/* In case of locked status registers, allow changing protection range only if it is extending the range. */
		if (new_prot_end > curr_prot_end) {
			int ret2;

			// TODO add irq lock?
			ret = lock_status(dev, false);
			if (ret) {
				return ret;
			}

			ret = set_flash_prot(dev, 0, new_prot_end);
			/* Always lock the status again. */
			ret2 = lock_status(dev, true);

			if (!ret) {
				ret = ret2;
			}
		} else {
			return EC_ERROR_ACCESS_DENIED;
		}
	}
	
	return ret;
}

static int cros_flash_andes_xip_protect_now(const struct device *dev, bool all)
{
	int ret;
	
	/* Set flash protection and lock the status. */
	if (all) {
		ret = cros_flash_andes_xip_protect_at_boot(dev, EC_FLASH_PROTECT_ALL_AT_BOOT);
	} else {
		ret = cros_flash_andes_xip_protect_at_boot(dev, EC_FLASH_PROTECT_RO_AT_BOOT);
	}
	if (!ret) {
		ret = lock_status(dev, true);
	}

	return ret;
}

static int cros_flash_andes_xip_write(const struct device *dev, int offset, int size,
				 const char *src_data)
{
	struct cros_flash_andestech_xip_data *data = DRV_DATA(dev);
	int ret = 0;

	if ((offset < 0) || (size < 0)) {
		return -EINVAL;
	}

	/* Check protection */
	if (check_prot_reg(dev, offset, size)) {
		return EC_ERROR_ACCESS_DENIED;
	}

	ret = flash_write(data->flash_dev, offset, src_data, size);
	//TODO check status

	return ret;
}

static int cros_flash_andes_xip_erase(const struct device *dev, int offset, int size)
{
	struct cros_flash_andestech_xip_data *data = DRV_DATA(dev);
	int ret = 0;

	if ((offset < 0) || (size < 0)) {
		return -EINVAL;
	}

	/* Check protection */
	if (check_prot_reg(dev, offset, size)) {
		return EC_ERROR_ACCESS_DENIED;
	}

	ret = flash_erase(data->flash_dev, offset, size);

	//TODO check status
	return ret;
}

static DEVICE_API(cros_flash, cros_flash_andes_xip_driver_api) = {
	.init = cros_flash_andes_xip_init,
	.physical_write = cros_flash_andes_xip_write,
	.physical_erase = cros_flash_andes_xip_erase,
	.physical_get_protect = cros_flash_andes_xip_get_protect,
	.physical_get_protect_flags = cros_flash_andes_xip_get_protect_flags,
	.physical_protect_at_boot = cros_flash_andes_xip_protect_at_boot,
	.physical_protect_now = cros_flash_andes_xip_protect_now,
	.physical_get_status = cros_flash_andes_xip_get_status,
};

static int flash_npcx_init(const struct device *dev)
{
	struct cros_flash_andestech_xip_data *data = DRV_DATA(dev);

	data->flash_dev = DEVICE_DT_GET(FLASH_DEV);
	if (!device_is_ready(data->flash_dev)) {
		LOG_ERR("device %s not ready", data->flash_dev->name);
		return -ENODEV;
	}

	return EC_SUCCESS;
}

BUILD_ASSERT(CONFIG_FLASH_ANDES_QSPI_INIT_PRIORITY < CONFIG_CROS_FLASH_ANDES_XIP_INIT_PRIORITY);

static struct cros_flash_andestech_xip_data cros_flash_data;
DEVICE_DT_INST_DEFINE(0, flash_npcx_init, NULL, &cros_flash_data, NULL,
		      POST_KERNEL, CONFIG_CROS_FLASH_ANDES_XIP_INIT_PRIORITY,
		      &cros_flash_andes_xip_driver_api);
