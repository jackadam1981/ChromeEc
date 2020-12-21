/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _VOLTEER_CBI_SSFC__H_
#define _VOLTEER_CBI_SSFC__H_

/****************************************************************************
 * Volteer CBI Second Source Factory Cache
 */

/*
 * Base Sensor (Bits 0-2)
 */
enum ssfc_base_sensor {
	SSFC_SENSOR_BASE_DEFAULT,
	SSFC_SENSOR_BMI160,
	SSFC_SENSOR_ICM426XX,
};
#define SSFC_SENSOR_BASE_OFFSET		0
#define SSFC_SENSOR_BASE_MASK		GENMASK(2, 0)

/*
 * Lid Sensor (Bits 3-5)
 */
enum ssfc_lid_sensor {
	SSFC_SENSOR_LID_DEFAULT,
	SSFC_SENSOR_BMA255,
	SSFC_SENSOR_KX022,
};
#define SSFC_SENSOR_LID_OFFSET		3
#define SSFC_SENSOR_LID_MASK		GENMASK(5, 3)

enum ssfc_base_sensor get_cbi_ssfc_base_sensor(void);
enum ssfc_lid_sensor get_cbi_ssfc_lid_sensor(void);

#endif /* _Volteer_CBI_SSFC__H_ */
