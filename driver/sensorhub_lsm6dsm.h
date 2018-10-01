/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * LSM6DSM Sensor Hub driver to enable interfacing with external sensors
 * like magnetometer for Chrome EC
 */

#ifndef __CROS_EC_SENSORHUB_LSM6DSM_H
#define __CROS_EC_SENSORHUB_LSM6DSM_H

#include "common.h"
#include "motion_sense.h"

#ifdef CONFIG_SENSORHUB_LSM6DSM

int sensorhub_set_ext_data_rate(const struct motion_sensor_t *s,
						int rate, int rnd);

int sensorhub_config_ext_reg(const struct motion_sensor_t *s,
				uint8_t slv_addr, uint8_t reg, uint8_t val);

int sensorhub_config_slv0_read(const struct motion_sensor_t *s,
				uint8_t slv_addr, uint8_t reg, int len);

int sensorhub_slv0_data_read(const struct motion_sensor_t *s, intv3_t v);

int sensorhub_check_and_rst(const struct motion_sensor_t *s, uint8_t slv_addr,
					uint8_t whoami_reg, uint8_t whoami_val,
					uint8_t rst_reg, uint8_t rst_val);
#else

static inline int sensorhub_set_ext_data_rate(const struct motion_sensor_t *s,
							int rate, int rnd)
{
	return EC_ERROR_UNIMPLEMENTED;
}

static inline int sensorhub_config_ext_reg(const struct motion_sensor_t *s,
				uint8_t slv_addr, uint8_t reg, uint8_t val)
{
	return EC_ERROR_UNIMPLEMENTED;
}

static inline int sensorhub_config_slv0_read(const struct motion_sensor_t *s,
					uint8_t slv_addr, uint8_t reg, int len)
{
	return EC_ERROR_UNIMPLEMENTED;
}

static inline int sensorhub_slv0_data_read(const struct motion_sensor_t *s,
								intv3_t v)
{
	return EC_ERROR_UNIMPLEMENTED;
}

static inline int sensorhub_check_and_rst(const struct motion_sensor_t *s,
		uint8_t slv_addr, uint8_t whoami_reg, uint8_t whoami_val,
					uint8_t rst_reg, uint8_t rst_val)
{
	return EC_ERROR_UNIMPLEMENTED;
}

#endif /* CONFIG_SENSORHUB_LSM6DSM */
#endif /* __CROS_EC_SENSORHUB_LSM6DSM_H */
