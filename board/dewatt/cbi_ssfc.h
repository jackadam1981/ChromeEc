/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _DEWATT_CBI_SSFC__H_
#define _DEWATT_CBI_SSFC__H_

#include "stdint.h"

/****************************************************************************
 * Dewatt CBI SSFC
 */

/*
 * Temp Sensor (Bits 0-1)
 */
enum ec_ssfc_temp_sensor {
	SSFC_TEMP_SENSOR_PCT2075 = 0,
	SSFC_TEMP_SENSOR_NCT7715 = 1,
};
#define SSFC_TEMP_SENSOR_OFFSET 0
#define SSFC_TEMP_SENSOR_MASK GENMASK(2, 0)

/**
 * Get the Temp sensor type from SSFC.
 *
 * @return the Temp sensor board type.
 */
enum ec_ssfc_temp_sensor get_cbi_ssfc_temp_sensor(void);

#endif /* _DEWATT_CBI_SSFC__H_ */
