/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT andestech_qspi_nor_xip_cros_flash

#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/cros_flash.h>
#include <soc.h>

#define flash_ctrl_dev DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller))

/* cros ec flash api functions */
static int cros_flash_spi_nor_init(const struct device *dev)
{
	return 0;
}

static int cros_flash_spi_nor_write(const struct device *dev, int offset, int size,
				 const char *src_data)
{
	return flash_write(flash_ctrl_dev, offset, src_data, size);
}

static int cros_flash_spi_nor_erase(const struct device *dev, int offset, int size)
{
	return flash_erase(flash_ctrl_dev, offset, size);
}

static int cros_flash_spi_nor_get_protect(const struct device *dev, int bank)
{
	return 0;
}

static uint32_t cros_flash_spi_nor_get_protect_flags(const struct device *dev)
{
	return 0;
}

static int cros_flash_spi_nor_protect_at_boot(const struct device *dev,
					   uint32_t new_flags)
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

DEVICE_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL,
		      POST_KERNEL, CONFIG_CROS_FLASH_ANDES_XIP_INIT_PRIORITY,
		      &cros_flash_spi_nor_driver_api);
