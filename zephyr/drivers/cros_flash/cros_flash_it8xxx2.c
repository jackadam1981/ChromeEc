/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT ite_it8xxx2_cros_flash

#include <drivers/cros_flash.h>
#include <drivers/gpio.h>
#include <kernel.h>
#include <logging/log.h>
#include <soc.h>

#include <drivers/flash.h>

LOG_MODULE_REGISTER(cros_flash, LOG_LEVEL_ERR);

/* Device config */
struct cros_flash_it8xxx2_config {
	/* flash interface unit base address */
	uintptr_t base;
	/* Flash size (Unit:bytes) */
	//int size;
};

/* Device data */
struct cros_flash_it8xxx2_data {
	/* flag of flash write protection */
	bool write_protectied;
	/* mutex of flash interface controller */
	struct k_sem lock_sem;
};

/* Driver convenience defines */
#define DRV_CONFIG(dev) ((const struct cros_flash_it8xxx2_config *)(dev)->config)
#define DRV_DATA(dev) ((struct cros_flash_it8xxx2_data *)(dev)->data)


/* cros ec flash api functions */
static int cros_flash_it8xxx2_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static int cros_flash_it8xxx2_read(const struct device *dev, int offset, int size,
				char *dst_data)
{
	return flash_read(dev, offset, dst_data, size);
}

static int cros_flash_it8xxx2_write(const struct device *dev, int offset, int size,
				 const char *src_data)
{
	return flash_write(dev, offset, src_data, size);
}

static int cros_flash_it8xxx2_erase(const struct device *dev, int offset, int size)
{
	return flash_erase(dev, offset, size);
}

static int cros_flash_it8xxx2_get_status_reg(const struct device *dev,
					  char cmd_code, char *data)
{
	return 0;
}

static int cros_flash_it8xxx2_set_status_reg(const struct device *dev, char *data)
{
	return 0;
}

static int cros_flash_it8xxx2_write_protection_set(const struct device *dev,
						bool enable)
{
	return flash_write_protection_set(dev, enable);
}

static int cros_flash_it8xxx2_write_protection_is_set(const struct device *dev)
{
	return 0;
}

static int cros_flash_it8xxx2_uma_lock(const struct device *dev, bool enable)
{
	return 0;
}

/* cros ec flash driver registration */
static const struct cros_flash_driver_api cros_flash_it8xxx2_driver_api = {
	.init = cros_flash_it8xxx2_init,
	.physical_read = cros_flash_it8xxx2_read,
	.physical_write = cros_flash_it8xxx2_write,
	.physical_erase = cros_flash_it8xxx2_erase,
	.write_protection = cros_flash_it8xxx2_write_protection_set,
	.write_protection_is_set = cros_flash_it8xxx2_write_protection_is_set,
	.get_status_reg = cros_flash_it8xxx2_get_status_reg,
	.set_status_reg = cros_flash_it8xxx2_set_status_reg,
	.uma_lock = cros_flash_it8xxx2_uma_lock,
};

static int flash_it8xxx2_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static const struct cros_flash_it8xxx2_config cros_flash_cfg = {
	.base = DT_INST_REG_ADDR(0),

};

static struct cros_flash_it8xxx2_data cros_flash_data;

DEVICE_DEFINE(cros_flash_it8xxx2_0, DT_INST_LABEL(0), flash_it8xxx2_init, NULL,
	      &cros_flash_data, &cros_flash_cfg, PRE_KERNEL_1,
	      62,
	      &cros_flash_it8xxx2_driver_api);
