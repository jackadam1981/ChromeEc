/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "temp_sensor.h"
#include "temp_sensor/temp_sensor.h"
#include "adc.h"
#include "temp_sensor/thermistor.h"

#if DT_NODE_EXISTS(DT_PATH(named_temp_sensors))
static int thermistor_get_temp(const struct temp_sensor_t *sensor,
			       int *temp_ptr) {
	return  thermistor_get_temperature(
		  sensor->idx, temp_ptr, sensor->thermistor
		);
}

#define GET_THERMISTOR_DATA_SAMPLE(node_sample_id)	\
  [DT_PROP(node_sample_id, sample_number)] = (struct thermistor_data_pair) { \
    .mv = DT_PROP(node_sample_id, milivolt),				\
    .temp = DT_PROP(node_sample_id, temp)				\
  },

// Note we need this due to self referential macros not expanding.
#define DT_FOREACH_CHILD_2(node_id, fn) \
	DT_CAT(node_id, _FOREACH_CHILD)(fn)

#define THERMISTOR_DATA(node_id) \
  static const struct thermistor_data_pair DT_CAT(node_id,_thermistor_data)[] = \
    {DT_FOREACH_CHILD_2(node_id, GET_THERMISTOR_DATA_SAMPLE)};

#define GET_THERMISTOR_INFO(node_id)					\
        & (struct thermistor_info) {					\
	    .scaling_factor = DT_PROP(node_id, scaling_factor),		\
	    .num_pairs = DT_PROP(node_id, num_pairs),			\
	      .data = DT_CAT(node_id,_thermistor_data),			\
	}								

#define TEMP_THERMISTOR(node_id)                               \
	[ZSHIM_TEMP_SENSOR_ID(node_id)] = {                    \
		.name = DT_LABEL(node_id),                     \
		.read = &thermistor_get_temp,                  \
		.idx = ZSHIM_ADC_ID(DT_PHANDLE(node_id, adc)), \
		.type = TEMP_SENSOR_TYPE_BOARD,                \
		.thermistor = GET_THERMISTOR_INFO(DT_PHANDLE(node_id, thermistor)), \
	},


DT_FOREACH_CHILD(DT_PATH(thermistors), THERMISTOR_DATA)

const struct temp_sensor_t temp_sensors[] = {
	DT_FOREACH_CHILD(DT_PATH(named_temp_sensors), TEMP_THERMISTOR)
};
#endif /* named_temp_sensors */
