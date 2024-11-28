/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "drivers/cros_system.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(cros_system, LOG_LEVEL_ERR);

static const char *cros_system_rtk_get_chip_vendor(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "rtk";
}

static uint32_t system_get_chip_id(void)
{
	return 0x5915;
}

static uint8_t system_get_chip_version(void)
{
	return 0xB;
}

static const char *cros_system_rtk_get_chip_name(const struct device *dev)
{
	ARG_UNUSED(dev);

	static char buf[8] = { 'r', 't', 's' };
	uint32_t chip_id = system_get_chip_id();
	int num = 4;

	for (int n = 3; num >= 0; n++, num--)
		snprintf(buf + n, (sizeof(buf) - n), "%x",
			 chip_id >> (num * 4) & 0xF);

	return buf;
}

static const char *
cros_system_rtk_get_chip_revision(const struct device *dev)
{
	ARG_UNUSED(dev);

	static char buf[3];
	uint8_t rev = system_get_chip_version();

	snprintf(buf, sizeof(buf), "%cx", rev + 'a');

	return buf;
}

static int cros_system_rtk_get_reset_cause(const struct device *dev)
{
	ARG_UNUSED(dev);

	return UNKNOWN_RST;
}

static int cros_system_rtk_init(const struct device *dev)
{
	return 0;
}

static int cros_system_rtk_soc_reset(const struct device *dev)
{
	/* Spin and wait for reboot */
	while (1)
		;

	/* Should never return */
	return 0;
}

/*
 * Fake wake ISR handler, needed for pins that do not have a handler.
 */
void wake_isr(enum gpio_signal signal)
{
}

static int cros_system_rtk_hibernate(const struct device *dev,
					 uint32_t seconds,
					 uint32_t microseconds)
{
	return 0;
}

static const struct cros_system_driver_api cros_system_driver_rtk_api = {
	.get_reset_cause = cros_system_rtk_get_reset_cause,
	.soc_reset = cros_system_rtk_soc_reset,
	.hibernate = cros_system_rtk_hibernate,
	.chip_vendor = cros_system_rtk_get_chip_vendor,
	.chip_name = cros_system_rtk_get_chip_name,
	.chip_revision = cros_system_rtk_get_chip_revision,
};
#if CONFIG_CROS_SYSTEM_REALTEK_INIT_PRIORITY >= \
	CONFIG_PLATFORM_EC_SYSTEM_PRE_INIT_PRIORITY
#error "CROS_SYSTEM must initialize before the SYSTEM_PRE initialization"
#endif

DEVICE_DEFINE(cros_system_rtk_0, "CROS_SYSTEM", cros_system_rtk_init,
	      NULL, NULL, NULL, PRE_KERNEL_1,
	      CONFIG_CROS_SYSTEM_REALTEK_INIT_PRIORITY,
	      &cros_system_driver_rtk_api);
