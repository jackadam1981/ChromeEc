/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _DEDEDE_CBI_SSFC__H_
#define _DEDEDE_CBI_SSFC__H_

#include "stdint.h"

/****************************************************************************
 * Dedede CBI Second Source Factory Cache
 */

/*
 * DB type (Bit 0-4)
 */
enum ec_ssfc_db_type {
	SSFC_DB_NONE = 0,
	SSFC_DB_2C = 1,
	SSFC_DB_1C_LTE = 2,
	SSFC_DB_1A_HDMI = 3,
	SSFC_DB_1C_1A = 4,
	SSFC_DB_LTE_HDMI = 5,
	SSFC_DB_1C_1A_LTE = 6,
	SSFC_DB_1C = 7
};

/*
 * Base Sensor (Bits 0-2)
 */
enum ec_ssfc_base_sensor {
	SSFC_SENSOR_BASE_DEFAULT = 0,
	SSFC_SENSOR_BMI160 = 1,
	SSFC_SENSOR_ICM426XX = 2,
	SSFC_SENSOR_LSM6DSM = 3,
	SSFC_SENSOR_ICM42607 = 4
};

/*
 * Lid Sensor (Bits 3-5)
 */
enum ec_ssfc_lid_sensor {
	SSFC_SENSOR_LID_DEFAULT = 0,
	SSFC_SENSOR_BMA255 = 1,
	SSFC_SENSOR_KX022 = 2,
	SSFC_SENSOR_LIS2DWL = 3
};

union pirika_cbi_ssfc {
	struct {
		uint32_t db_type : 4;
		uint32_t reserved_2 : 28;
	};
	uint32_t raw_value;
};

/**
 * Get the DB type from SSFC_CONFIG.
 *
 * @return the DB type.
 */
enum ec_ssfc_db_type get_cbi_ssfc_db_type(void);

#endif /* _DEDEDE_CBI_SSFC__H_ */
