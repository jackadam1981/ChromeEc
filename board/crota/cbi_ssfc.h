/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _CROTA_CBI_SSFC_H_
#define _CROTA_CBI_SSFC_H_

#include "stdint.h"

/****************************************************************************
 * Crota CBI Second Source Factory Cache
 */

/*
 * Base Sensor (Bits 0-1)
 */
enum ec_ssfc_base_sensor {
	SSFC_SENSOR_BASE_DEFAULT = 0,
	SSFC_SENSOR_BASE_BMI260 = 1
};

union crota_cbi_ssfc {
	struct {
		enum ec_ssfc_base_sensor base_sensor : 1;
		uint32_t reserved_1 : 31;
	};
	uint32_t raw_value;
};

/**
 * Get the Base sensor type from SSFC_CONFIG.
 *
 * @return the Base sensor board type.
 */
enum ec_ssfc_base_sensor get_cbi_ssfc_base_sensor(void);

#endif /* _CROTA_CBI_SSFC_H_ */
