/*
 * Copyright 2020 Google LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Chrome OS-specific API for flash memory access
 * This exists only support the interface expected by the Chrome OS EC. It seems
 * better to implement this so we can make use of most of the existing code in
 * its keyboard_scan.c file and thus make sure we operate the same way.
 *
 * It provides raw access to flash memory module.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CROS_CBI_H_
#define ZEPHYR_INCLUDE_DRIVERS_CROS_CBI_H_

#include <kernel.h>
#include <device.h>
#include <devicetree.h>
#include "motionsense_sensors.h"
#include "cbi_ssfc.h"


#define CBI_SSFC_VALUE_COMPAT			named_cbi_ssfc_value
#define CBI_SSFC_VALUE_ID(id)			DT_CAT(CBI_SSFC_VALUE_, id)
#define CBI_SSFC_VALUE_ID_WITH_COMMA(id)	CBI_SSFC_VALUE_ID(id),
#define CBI_SSFC_VALUE_INST_ENUM(inst, _)	CBI_SSFC_VALUE_ID_WITH_COMMA(DT_INST(inst, CBI_SSFC_VALUE_COMPAT))

enum cbi_ssfc_value_id {
	UTIL_LISTIFY(DT_NUM_INST_STATUS_OKAY(CBI_SSFC_VALUE_COMPAT), CBI_SSFC_VALUE_INST_ENUM)
};


typedef int (*cros_cbi_api_check_ssfc_match)(const struct device *dev, enum cbi_ssfc_value_id value_id);

__subsystem struct cros_cbi_driver_api {
	cros_cbi_api_check_ssfc_match check_ssfc_match;
};

/**
 * @endcond
 */

/**
 * @brief Check if the CBI SSFC value matches the one in the EEPROM
 *
 * @param dev Pointer to the device.
 *
 * @return 1 If matches, 0 if not.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_cbi_check_ssfc_match(const struct device *dev, enum cbi_ssfc_value_id value_id);

static inline int z_impl_cros_cbi_check_ssfc_match(const struct device *dev, enum cbi_ssfc_value_id value_id)
{
	const struct cros_cbi_driver_api *api =
		(const struct cros_cbi_driver_api *)dev->api;

	if (!api->check_ssfc_match) {
		return -ENOTSUP;
	}

	return api->check_ssfc_match(dev, value_id);
}

/**
 * @}
 */
#include <syscalls/cros_cbi.h>
#endif /* ZEPHYR_INCLUDE_DRIVERS_CROS_CBI_H_ */
