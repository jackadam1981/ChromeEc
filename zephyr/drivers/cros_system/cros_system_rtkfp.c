/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "cros_version.h"
#include "drivers/cros_system.h"
#include "stdint.h"
#include "system.h"
#include "stdio.h"

#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/barrier.h>
#include <zephyr/drivers/timer/system_timer.h>
#include <zephyr/cache.h>

LOG_MODULE_REGISTER(cros_system, LOG_LEVEL_ERR);

/* Driver data */
struct cros_system_rtkfp_data {
	int reset; /* reset cause */
};

static const struct device *const watchdog =
	DEVICE_DT_GET(DT_CHOSEN(cros_ec_watchdog));

#define R_CHIP_RESET 0x401e2100
#define CHIP_RESET_BIT BIT(0)
static void rtkfp_resetchip(void)
{
	uint32_t chip_rst;

	/* Disable interrupts to avoid task swaps during reboot */
	interrupt_disable_all();

	wdt_disable(watchdog);
	sys_clock_disable();

	/* Flush data cache */
	sys_cache_data_flush_all();

	barrier_dsync_fence_full();
	barrier_isync_fence_full();

	/* Chip reset */
	chip_rst = *(volatile uint32_t *)R_CHIP_RESET;
	chip_rst |= CHIP_RESET_BIT;
	*(volatile uint32_t *)R_CHIP_RESET = chip_rst;

	/* Spin and wait for reboot */
	while (1)
		;
}

static const char *cros_system_rtkfp_get_chip_vendor(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "rtk";
}

static const char *cros_system_rtkfp_get_chip_name(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "rts5817";
}

#define R_SYS_STS 0x40100000
#define VER_NUM_OFFSET 16
#define VER_NUM_MASK   (0xFF << VER_NUM_OFFSET)
static const char *cros_system_rtkfp_get_chip_revision(const struct device *dev)
{
	ARG_UNUSED(dev);
	static char buf[5];
	uint32_t sts_reg = *(volatile uint32_t *)(R_SYS_STS);
	uint8_t chip_ver = (sts_reg & VER_NUM_MASK) >> VER_NUM_OFFSET;

	snprintf(buf, sizeof(buf), "%c", chip_ver);

	return buf;
}

static int cros_system_rtkfp_get_reset_cause(const struct device *dev)
{
	struct cros_system_rtkfp_data *data = dev->data;

	return data->reset;
}

static int cros_system_rtkfp_init(const struct device *dev)
{
	struct cros_system_rtkfp_data *data = dev->data;

	/* Todo: implement get reset cause function */
	data->reset = POWERUP;

	return 0;
}

static int cros_system_rtkfp_soc_reset(const struct device *dev)
{
	rtkfp_resetchip();

	/* Should never return */
	return 0;
}

static int cros_system_rtkfp_hibernate(const struct device *dev, uint32_t seconds,
				       uint32_t microseconds)
{
	return 0;
}

static const struct cros_system_driver_api cros_system_driver_rtkfp_api = {
	.get_reset_cause = cros_system_rtkfp_get_reset_cause,
	.soc_reset = cros_system_rtkfp_soc_reset,
	.hibernate = cros_system_rtkfp_hibernate,
	.chip_vendor = cros_system_rtkfp_get_chip_vendor,
	.chip_name = cros_system_rtkfp_get_chip_name,
	.chip_revision = cros_system_rtkfp_get_chip_revision,
};
#if CONFIG_CROS_SYSTEM_REALTEK_INIT_PRIORITY >= \
	CONFIG_PLATFORM_EC_SYSTEM_PRE_INIT_PRIORITY
#error "CROS_SYSTEM must initialize before the SYSTEM_PRE initialization"
#endif

static struct cros_system_rtkfp_data cros_system_rtkfp_data_0;

DEVICE_DEFINE(cros_system_rtkfp_0, "CROS_SYSTEM", cros_system_rtkfp_init, NULL,
	      &cros_system_rtkfp_data_0, NULL, PRE_KERNEL_1,
	      CONFIG_CROS_SYSTEM_REALTEKFP_INIT_PRIORITY,
	      &cros_system_driver_rtkfp_api);
