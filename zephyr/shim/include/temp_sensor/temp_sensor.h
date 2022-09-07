/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_SHIM_INCLUDE_TEMP_SENSOR_TEMP_SENSOR_H_
#define ZEPHYR_SHIM_INCLUDE_TEMP_SENSOR_TEMP_SENSOR_H_

#include <zephyr/devicetree.h>
#include "include/temp_sensor.h"
#include "charger/chg_rt9490.h"

#ifdef CONFIG_PLATFORM_EC_TEMP_SENSOR

#define PCT2075_COMPAT nxp_pct2075
#define TMP112_COMPAT cros_ec_temp_sensor_tmp112
#define SB_TSI_COMPAT amd_sb_tsi
#define THERMISTOR_COMPAT cros_ec_temp_sensor_thermistor

#define TEMP_RT9490_FN(node_id, fn) \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, thermistor), (fn(node_id)), ())

#define FOREACH_TEMP_SENSOR(fn)                                             \
	DT_FOREACH_STATUS_OKAY(PCT2075_COMPAT, fn)                          \
	DT_FOREACH_STATUS_OKAY(TMP112_COMPAT, fn)                           \
	DT_FOREACH_STATUS_OKAY_VARGS(RT9490_CHG_COMPAT, TEMP_RT9490_FN, fn) \
	DT_FOREACH_STATUS_OKAY(SB_TSI_COMPAT, fn)                           \
	DT_FOREACH_STATUS_OKAY(THERMISTOR_COMPAT, fn)

#define HAS_POWER_GOOD_PIN(node_id) DT_NODE_HAS_PROP(node_id, power_good_pin) ||

#define ANY_INST_HAS_POWER_GOOD_PIN \
	(DT_FOREACH_CHILD(DT_PATH(named_temp_sensors), HAS_POWER_GOOD_PIN) 0)

#define TEMP_SENSOR_ID(node_id) DT_CAT(TEMP_SENSOR_, node_id)
#define TEMP_SENSOR_ID_WITH_COMMA(node_id) TEMP_SENSOR_ID(node_id),

enum temp_sensor_id {
	FOREACH_TEMP_SENSOR(TEMP_SENSOR_ID_WITH_COMMA) TEMP_SENSOR_COUNT
};

#undef TEMP_SENSOR_ID_WITH_COMMA

#define TEMP_SENSOR_ID_BY_NAMED(node_id) \
	TEMP_SENSOR_ID(DT_PHANDLE(node_id, sensor))

/* PCT2075 access array */
#define PCT2075_SENSOR_ID(node_id) DT_CAT(PCT2075_, node_id)
#define PCT2075_SENSOR_ID_WITH_COMMA(node_id) PCT2075_SENSOR_ID(node_id),

enum pct2075_sensor {
	DT_FOREACH_STATUS_OKAY(PCT2075_COMPAT, PCT2075_SENSOR_ID_WITH_COMMA)
		PCT2075_COUNT,
};

#undef PCT2075_SENSOR_ID_WITH_COMMA

/* TMP112 access array */
#define TMP112_SENSOR_ID(node_id) DT_CAT(TMP112_, node_id)
#define TMP112_SENSOR_ID_WITH_COMMA(node_id) TMP112_SENSOR_ID(node_id),

enum tmp112_sensor {
	DT_FOREACH_STATUS_OKAY(TMP112_COMPAT, TMP112_SENSOR_ID_WITH_COMMA)
		TMP112_COUNT,
};

#undef TMP112_SENSOR_ID_WITH_COMMA

struct zephyr_temp_sensor {
	/* Read sensor value in K into temp_ptr; return non-zero if error. */
	int (*read)(const struct temp_sensor_t *sensor, int *temp_ptr);
	struct thermistor_info *thermistor;
};

#if ANY_INST_HAS_POWER_GOOD_PIN
struct zephyr_temp_power_good {
	const struct device *power_good_dev;
	gpio_pin_t power_good_pin;
};
#endif /* ANY_INST_HAS_POWER_GOOD_PIN */

#endif /* CONFIG_PLATFORM_EC_TEMP_SENSOR */

#endif /* ZEPHYR_SHIM_INCLUDE_TEMP_SENSOR_TEMP_SENSOR_H_ */
