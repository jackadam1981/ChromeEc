/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT andestech_qspi_nor_xip_cros_flash

#include "../drivers/flash/spi_nor.h"
#include "flash.h"
#include "spi_flash_reg.h"

#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/cros_flash.h>
#include <soc.h>

LOG_MODULE_REGISTER(cros_flash, LOG_LEVEL_ERR);

int all_protected; /* Has all-flash protection been requested? */
int addr_prot_start;
int addr_prot_length;
uint8_t saved_sr1;
uint8_t saved_sr2;

/* Device data */
struct cros_flash_andestech_xip_data {
	const struct device *flash_dev;
};

#define FLASH_DEV DT_CHOSEN(zephyr_flash_controller)

#define DRV_DATA(dev) ((struct cros_flash_andestech_xip_data *)(dev)->data)

__ramfunc int read_srs(const struct device *dev, uint8_t *reg1, uint8_t *reg2);

int flash_check_prot_range(unsigned int offset, unsigned int bytes)
{
	/* Invalid value */
	if (offset + bytes > CONFIG_FLASH_SIZE_BYTES)
		return EC_ERROR_INVAL;

	/* Check if ranges overlap */
	if (MAX(addr_prot_start, offset) <
	    MIN(addr_prot_start + addr_prot_length, offset + bytes))
		return EC_ERROR_ACCESS_DENIED;

	return EC_SUCCESS;
}

int get_status_reg(const struct device *dev, uint8_t cmd_code, uint8_t *reg)
{
	return 0;
}

/* cros ec flash api functions */
static int cros_flash_spi_nor_init(const struct device *dev)
{
	return 0;
}

static int cros_flash_spi_nor_write(const struct device *dev, int offset, int size,
				 const char *src_data)
{
	struct cros_flash_andestech_xip_data *data = DRV_DATA(dev);

	return flash_write(data->flash_dev, offset, src_data, size);
}

static int cros_flash_spi_nor_erase(const struct device *dev, int offset, int size)
{
	struct cros_flash_andestech_xip_data *data = DRV_DATA(dev);

	return flash_erase(data->flash_dev, offset, size);
}

/* Flags for flash protection */
/* RO flash code protected when the EC boots */
// #define EC_FLASH_PROTECT_RO_AT_BOOT BIT(0)
/*
 * RO flash code protected now.  If this bit is set, at-boot status cannot
 * be changed.
 */
// #define EC_FLASH_PROTECT_RO_NOW BIT(1)
/* Entire flash code protected now, until reboot. */
// #define EC_FLASH_PROTECT_ALL_NOW BIT(2)
/* Flash write protect GPIO is asserted now */
// #define EC_FLASH_PROTECT_GPIO_ASSERTED BIT(3)
/* Error - at least one bank of flash is stuck locked, and cannot be unlocked */
// #define EC_FLASH_PROTECT_ERROR_STUCK BIT(4)
/*
 * Error - flash protection is in inconsistent state.  At least one bank of
 * flash which should be protected is not protected.  Usually fixed by
 * re-requesting the desired flags, or by a hard reset if that fails.
 */
// #define EC_FLASH_PROTECT_ERROR_INCONSISTENT BIT(5)

static uint32_t cros_flash_spi_nor_get_protect_flags(const struct device *dev)
{
	return 0;
}


static int cros_flash_spi_nor_protect_now(const struct device *dev, int all)
{
	return 0;
}

static int cros_flash_spi_nor_get_jedec_id(const struct device *dev,
					uint8_t *manufacturer, uint16_t *device)
{
	return 0;
}

static int cros_flash_spi_nor_get_status(const struct device *dev, uint8_t *sr1,
				      uint8_t *sr2)
{
	return 0;
}

void flash_get_status(const struct device *dev, uint8_t *sr1,
			     uint8_t *sr2)
{
	struct cros_flash_andestech_xip_data *data = DRV_DATA(dev);

	// if (all_protected) {
	// 	*sr1 = saved_sr1;
	// 	*sr2 = saved_sr2;
	// 	return;
	// }

	/* Lock physical flash operations */
	// crec_flash_lock_mapped_storage(1);

	read_srs(data->flash_dev, sr1, sr2);

	/* Unlock physical flash operations */
	// crec_flash_lock_mapped_storage(0);
}

void flash_set_status(const struct device *dev, uint16_t srs, uint16_t mask)
{
	/* Lock physical flash operations */
	crec_flash_lock_mapped_storage(1);


	/* Unlock physical flash operations */
	crec_flash_lock_mapped_storage(0);
}

int flash_set_status_for_prot(const struct device *dev, uint16_t srs, uint16_t mask)
{
	// int rv;

	// TODO fail if SRP1 == 1, because Status Register and configure register can not be changed
	/*
	 * Writing SR regs will fail if our UMA lock is enabled. If WP
	 * is deasserted then remove the lock and allow the write.
	 */
	// if (all_protected) {
	// 	if (is_int_flash_protected(dev))
	// 		return EC_ERROR_ACCESS_DENIED;

	// 	if (crec_flash_get_protect() & EC_FLASH_PROTECT_GPIO_ASSERTED)
	// 		return EC_ERROR_ACCESS_DENIED;
	// 	flash_uma_lock(dev, 0);
	// }

	/*
	 * If WP# is active and ec doesn't protect the status registers of
	 * internal spi-flash, protect it now before setting them.
	 */
	// flash_protect_int_flash(dev, write_protect_is_asserted());

	// rv = flash_set_status(dev, reg1, reg2);
	// if (rv != EC_SUCCESS) {
	// 	return rv;
	// }

	// spi_flash_reg_to_protect(reg1, reg2, &addr_prot_start,
	// 			 &addr_prot_length);

	return EC_SUCCESS;
}

int flash_check_prot_reg(const struct device *dev, unsigned int offset,
				unsigned int bytes)
{
	unsigned int start;
	unsigned int len;
	uint8_t sr1, sr2;
	int rv = EC_SUCCESS;

	// TODO check that
	/*
	 * If WP# is active and ec doesn't protect the status registers of
	 * internal spi-flash, protect it now.
	 */
	//flash_protect_int_flash(dev, write_protect_is_asserted());

	/* Invalid value */
	if (offset + bytes > CONFIG_FLASH_SIZE_BYTES)
		return EC_ERROR_INVAL;

	/* Compute current protect range */
	flash_get_status(dev, &sr1, &sr2);
	sr1 = 0;
	sr2 = 0;
	rv = spi_flash_reg_to_protect(sr1, sr2, &start, &len);
	if (rv)
		return rv;

	/* Check if ranges overlap */
	if (MAX(start, offset) < MIN(start + len, offset + bytes))
		return EC_ERROR_ACCESS_DENIED;

	return EC_SUCCESS;
}

static int cros_flash_spi_nor_get_protect(const struct device *dev, int bank)
{
	uint32_t addr = bank * CONFIG_FLASH_BANK_SIZE;

	// return 0;
	return flash_check_prot_reg(dev, addr, CONFIG_FLASH_BANK_SIZE);
}

static int cros_flash_spi_nor_protect_at_boot(const struct device *dev,
					   uint32_t new_flags)
{
	// if ((new_flags & (EC_FLASH_PROTECT_RO_AT_BOOT |
	// 		  EC_FLASH_PROTECT_ALL_AT_BOOT)) == 0) {
	// 	/* Clear protection bits in status register */
	// 	return flash_set_status_for_prot(dev, 0, 0);
	// }

	return 0;
}


/* cros ec flash driver registration */
static DEVICE_API(cros_flash, cros_flash_spi_nor_driver_api) = {
	.init = cros_flash_spi_nor_init,
	.physical_write = cros_flash_spi_nor_write,
	.physical_erase = cros_flash_spi_nor_erase,
	.physical_get_protect = cros_flash_spi_nor_get_protect,
	.physical_get_protect_flags = cros_flash_spi_nor_get_protect_flags,
	.physical_protect_at_boot = cros_flash_spi_nor_protect_at_boot,
	.physical_protect_now = cros_flash_spi_nor_protect_now,
	.physical_get_jedec_id = cros_flash_spi_nor_get_jedec_id,
	.physical_get_status = cros_flash_spi_nor_get_status,
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

static struct cros_flash_andestech_xip_data cros_flash_data;
DEVICE_DT_INST_DEFINE(0, flash_npcx_init, NULL, &cros_flash_data, NULL,
		      POST_KERNEL, CONFIG_CROS_FLASH_ANDES_XIP_INIT_PRIORITY,
		      &cros_flash_spi_nor_driver_api);
