/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drivers/cros_system.h"
#include "system.h"

#include <zephyr/device.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/reboot.h>

#define DRV_DATA(dev) ((struct cros_system_elan_data *)(dev)->data)

/* Driver data */
struct cros_system_elan_data {
	int reset; /* reset cause */
};

static const char *cros_system_elan_get_chip_vendor(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "elan";
}

static const char *cros_system_elan_get_chip_name(const struct device *dev)
{
	ARG_UNUSED(dev);

	return CONFIG_SOC;
}

static const char *cros_system_elan_get_chip_revision(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "";
}

static int cros_system_elan_get_reset_cause(const struct device *dev)
{
	struct cros_system_elan_data *data = DRV_DATA(dev);

	return data->reset;
}

static int cros_system_elan_soc_reset(const struct device *dev)
{
	ARG_UNUSED(dev);

	while(1);
	/* Should never return */
	return 0;
}

static int cros_system_elan_init(const struct device *dev)
{
	struct cros_system_elan_data *data = DRV_DATA(dev);

	data->reset = UNKNOWN_RST;
	
	return 0;
}

static struct cros_system_elan_data cros_system_elan_dev_data;

static DEVICE_API(cros_system, cros_system_driver_elan_api) = {
	.get_reset_cause = cros_system_elan_get_reset_cause,
	.soc_reset = cros_system_elan_soc_reset,
	.chip_vendor = cros_system_elan_get_chip_vendor,
	.chip_name = cros_system_elan_get_chip_name,
	.chip_revision = cros_system_elan_get_chip_revision,
};

DEVICE_DEFINE(cros_system_elan_0, "CROS_SYSTEM", cros_system_elan_init, NULL,
	      &cros_system_elan_dev_data, NULL, PRE_KERNEL_1,
	      CONFIG_CROS_SYSTEM_ELAN_INIT_PRIORITY,
	      &cros_system_driver_elan_api);

#if CONFIG_CROS_SYSTEM_ELAN_INIT_PRIORITY >= \
	CONFIG_PLATFORM_EC_SYSTEM_PRE_INIT_PRIORITY
#error "CROS_SYSTEM must initialize before the SYSTEM_PRE initialization"
#endif
