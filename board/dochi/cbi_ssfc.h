/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _DOCHI_CBI_SSFC__H_
#define _DOCHI_CBI_SSFC__H_

#include "stdint.h"

/****************************************************************************
 * Dochi CBI Second Source Factory Cache
 */

/*
 * Base Sensor (Bits 2-3)
 */
enum ec_ssfc_base_sensor {
	SSFC_SENSOR_BASE_LIS2DE = 0,
	SSFC_SENSOR_BASE_BMI422 = 1
};

/*
 * Lid Sensor (Bits 4-5)
 */
enum ec_ssfc_lid_sensor {
	SSFC_SENSOR_LID_LIS2DE = 0,
	SSFC_SENSOR_LID_BMA422 = 1,
};

union dochi_cbi_ssfc {
	struct {
		uint32_t audio_codec : 2;
		uint32_t base_sensor : 2;
		uint32_t lid_sensor : 2;
		uint32_t TS_Source : 4;
		uint32_t reserved_2 : 22;
	};
	uint32_t raw_value;
};

/**
 * Get the Base sensor type from SSFC_CONFIG.
 *
 * @return the Base sensor board type.
 */
enum ec_ssfc_base_sensor get_cbi_ssfc_base_sensor(void);

/**
 * Get the Lid sensor type from SSFC_CONFIG.
 *
 * @return the Lid sensor board type.
 */
enum ec_ssfc_lid_sensor get_cbi_ssfc_lid_sensor(void);

#endif /* _DOCHI_CBI_SSFC__H_ */
