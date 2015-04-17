/*
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_SENSORS_H
#define __CROS_EC_SENSORS_H
#include "sensors.wrap"

/*
 * This is just defining a struct to count the total number of accelgyro
 * sensors.  Each member of the struct is 1 byte.
 */
struct motion_sensor_count {
	SENSOR_LIST(EXPAND_AS_STRUCT)
};
#define MOTION_SENSOR_COUNT sizeof(struct motion_sensor_count)

/* Create chip enum */
enum motion_sensor_chips {
	MOTION_SENSOR_CHIPS(EXPAND_AS_CHIP_ENUM)
	NUM_MOTION_SENSOR_CHIPS
};

/* Create sensor enums*/
MOTION_SENSOR_CHIPS(CREATE_SENSOR_ENUMS)

#endif /* __CROS_EC_SENSORS_H */
