/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_VOLTAGE_LIMIT_H
#define __CROS_EC_VOLTAGE_LIMIT_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_POWER_65W 20000
#define DEFAULT_POWER 15000
#define IS_SKU_ID_IN_RANGE(sku_id) \
	((sku_id >= 0x2A0000) && (sku_id <= 0x2A0010))

/**
 * Calculate the maximum voltage based on the input value and SKU ID.
 *
 * @param val The input value to calculate the voltage from.
 * @param sku_id The SKU ID to determine specific voltage limits.
 * @return The calculated maximum voltage.
 */
uint32_t calculate_max_voltage(uint32_t val, uint32_t sku_id);

#ifdef __cplusplus
}
#endif

#endif /* __CROS_EC_VOLTAGE_LIMIT_H */