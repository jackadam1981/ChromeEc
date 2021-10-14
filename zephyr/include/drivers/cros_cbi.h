/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Chrome OS-specific API for access to Cros Board Info(CBI)
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CROS_CBI_H_
#define ZEPHYR_INCLUDE_DRIVERS_CROS_CBI_H_

#include <kernel.h>
#include <device.h>
#include <devicetree.h>

#define CBI_SSFC_VALUE_COMPAT			named_cbi_ssfc_value
#define CBI_SSFC_VALUE_ID(id)			DT_CAT(CBI_SSFC_VALUE_, id)
#define CBI_SSFC_VALUE_ID_WITH_COMMA(id)	CBI_SSFC_VALUE_ID(id),
#define CBI_SSFC_VALUE_INST_ENUM(inst, _) \
	CBI_SSFC_VALUE_ID_WITH_COMMA(DT_INST(inst, CBI_SSFC_VALUE_COMPAT))
#define CROS_CBI_LABEL				"cros_cbi"

enum cbi_ssfc_value_id {
	UTIL_LISTIFY(DT_NUM_INST_STATUS_OKAY(CBI_SSFC_VALUE_COMPAT),
		     CBI_SSFC_VALUE_INST_ENUM)
	CBI_SSFC_VALUE_COUNT
};

#define CBI_BOARD_VER_COMPAT			named_cbi_board_version
#define CBI_BOARD_VER_NODE			DT_INST(0, CBI_BOARD_VER_COMPAT)
#define CBI_BOARD_VER_ID(id)			DT_CAT(CBI_BOARD_VER_, id)
#define CBI_BOARD_VER_ID_WITH_COMMA(id)		CBI_BOARD_VER_ID(id),

enum cbi_board_ver_id {
#if DT_NODE_EXISTS(CBI_BOARD_VER_NODE)
	DT_FOREACH_CHILD(CBI_BOARD_VER_NODE, CBI_BOARD_VER_ID_WITH_COMMA)
#endif
	CBI_BOARD_VER_COUNT
};

/**
 * @cond INTERNAL_HIDDEN
 *
 * cros cbi raw driver API definition and system call entry points
 *
 * (Internal use only.)
 */
typedef int (*cros_cbi_api_init)(const struct device *dev);
typedef bool (*cros_cbi_api_ssfc_check_match)(const struct device *dev,
					      enum cbi_ssfc_value_id value_id);
typedef bool (*cros_cbi_api_board_ver_check)(const struct device *dev, enum cbi_board_ver_id board_ver_id);


__subsystem struct cros_cbi_driver_api {
	cros_cbi_api_init init;
	cros_cbi_api_ssfc_check_match ssfc_check_match;
	cros_cbi_api_board_ver_check board_ver_check;
};

/**
 * @endcond
 */

/**
 * @brief Initialize CBI.
 *
 * @param dev Pointer to the device structure for the CBI instance.
 *
 * @return 0 If successful.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_cbi_init(const struct device *dev);

static inline int z_impl_cros_cbi_init(const struct device *dev)
{
	const struct cros_cbi_driver_api *api =
		(const struct cros_cbi_driver_api *)dev->api;

	if (!api->init) {
		return -ENOTSUP;
	}

	return api->init(dev);
}

/**
 * @brief Check if the CBI SSFC value matches the one in the EEPROM
 *
 * @param dev Pointer to the device.
 *
 * @return 1 If matches, 0 if not.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_cbi_ssfc_check_match(const struct device *dev,
					enum cbi_ssfc_value_id value_id);

static inline int
z_impl_cros_cbi_ssfc_check_match(const struct device *dev,
				 enum cbi_ssfc_value_id value_id)
{
	const struct cros_cbi_driver_api *api =
		(const struct cros_cbi_driver_api *)dev->api;

	if (!api->ssfc_check_match) {
		return -ENOTSUP;
	}

	return api->ssfc_check_match(dev, value_id);
}


/**
 * @brief Check if the CBI SSFC value matches the one in the EEPROM
 *
 * @param dev Pointer to the device.
 *
 * @return 1 If matches, 0 if not.
 * @retval -ENOTSUP Not supported api function.
 */
__syscall int cros_cbi_board_ver_check(const struct device *dev,
					enum cbi_board_ver_id board_ver_id);


static inline int
z_impl_cros_cbi_board_ver_check(const struct device *dev, enum cbi_board_ver_id board_ver_id)
{
	const struct cros_cbi_driver_api *api =
		(const struct cros_cbi_driver_api *)dev->api;

	if (!api->board_ver_check) {
		return -ENOTSUP;
	}

	return api->board_ver_check(dev, board_ver_id);
}

/**
 * @}
 */
#include <syscalls/cros_cbi.h>
#endif /* ZEPHYR_INCLUDE_DRIVERS_CROS_CBI_H_ */
