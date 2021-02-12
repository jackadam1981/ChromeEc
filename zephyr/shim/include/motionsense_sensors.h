/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_MOTIONSENSE_SENSORS_H
#define __CROS_EC_MOTIONSENSE_SENSORS_H

#include <devicetree.h>

#define SENSOR_NODE			DT_PATH(motionsense_sensor)
#define SENSOE_SENSOR_INFO_NODE		DT_PATH(motionsense_sensor_info)
#define LID_ACCEL_NODE			DT_PATH(motionsense_sensor, lid_accel)
#define BASE_ACCEL_NODE			DT_PATH(motionsense_sensor, base_accel)

#define SENSOR_ID(id)			DT_CAT(SENSOR_,id)
#define SENSOR_ID_WITH_COMMA(id)	SENSOR_ID(id),

enum sensor_id {
#if DT_NODE_EXISTS(SENSOR_NODE)
	DT_FOREACH_CHILD(SENSOR_NODE, SENSOR_ID_WITH_COMMA)
#endif
	SENSOR_COUNT,
};

#ifdef CONFIG_LID_ANGLE
#define CONFIG_LID_ANGLE_SENSOR_LID	SENSOR_ID(DT_NODELABEL(lid_accel))
#define CONFIG_LID_ANGLE_SENSOR_BASE	SENSOR_ID(DT_NODELABEL(base_accel))
#endif

#if DT_NODE_HAS_PROP(SENSOE_SENSOR_INFO_NODE, accel_force_mode_sensors)
#define FORCE_MODE_SENSOR(i, id)					\
	| BIT(SENSOR_ID(DT_PHANDLE_BY_IDX(id, accel_force_mode_sensors, i)))
#define CONFIG_ACCEL_FORCE_MODE_MASK					\
	(0 UTIL_LISTIFY(DT_PROP_LEN(SENSOE_SENSOR_INFO_NODE,		\
		accel_force_mode_sensors), FORCE_MODE_SENSOR,		\
		SENSOE_SENSOR_INFO_NODE))
#endif

#endif /* __CROS_EC_MOTIONSENSE_SENSORS_H */
