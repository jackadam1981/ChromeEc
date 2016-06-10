/*
 *
 * Sensor driver implemnetation for BMP280 for Chrome EC 
 *
 */

#ifndef __CROS_EC_BAROMETER_SENSOR_H
#define __CROS_EC_BAROMETER_SENSOR_H

#include "common.h"

enum barosensor_func {
  BAROSENSOR_FUNC_PRESSURE = 0,
  BAROSENSOR_FUNC_TEMPERATURE = 1,
  BAROSENSOR_FUNC_MAX,
};

enum barosensor_running_mode {
	NORMAL_MODE = 0,
	LOW_POWER_MODE = 1,
	/* Todo: This can be enabled if needed */
#if 0
 	HIGH_RESOLUTION_MODE,
#endif
};

struct baro_sensor_t {
	char *name;
	uint32_t port;
	uint32_t addr;
	/* Brometer may have multiple sensor options in-built into 
	 * Pressure or temperature - can be removed */
	enum barosensor_func func;
	enum barosensor_running_mode mode;
	int (*init)(const struct baro_sensor_t *s);
	int (*read)(int *val);

};


extern const struct baro_sensor_t baro[];
/**
 * Read pressure
 *
 * @param id		Which one?
 * @param pressure      Value read in units
 *
 * @return EC_SUCCESS, or non-zero if error.
 */
int get_pressure(int id, int *pressure);

#endif
