/*
 *
 * Sensor driver implementation for BMP280 for Chrome EC
 *
 */

#ifndef __CROS_EC_BAROMETER_SENSOR_H
#define __CROS_EC_BAROMETER_SENSOR_H

#include "common.h"

enum barosensor_func {
  BAROSENSOR_FUNC_PRESSURE = 1,
  BAROSENSOR_FUNC_TEMPERATURE = 2,
  BAROSENSOR_FUNC_MAX,
};

struct baro_sensor_t {
	char *name;
	uint32_t port;
	uint32_t addr;
	uint32_t mode;
	/* Brometer may have multiple sensor options in-built into
	 * Pressure or temperature - can be removed */
	enum barosensor_func func;
	int (*init)(const struct baro_sensor_t *s);
	int (*read)(int *val);
	/* Calculate and return compensated pressure value */
	int (*comp)(int pressure);
	/* Set work mode */
	int (*set_work_mode)(uint8_t mode);

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
