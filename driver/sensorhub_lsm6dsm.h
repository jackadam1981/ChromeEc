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

/**
 * sensorhub_set_ext_data_rate() - Set external sensor data rate
 * @s:		Pointer to external motion sensor's data structure.
 * @rate:	Preferred output data rate from the sensor.
 * @rnd:	Flag to indicate rounding to the nearest supported
 *		output data rate by the sensor.
 * @ret_rate:	Rate configured by the sensor hub depending on the
 *		accelerometer data rate.
 *
 * Return: EC_SUCCESS on success, EC error codes on failure.
 *
 * This function is used to set the output data rate of the external
 * sensor that is attached to the sensor hub to the requested rate.
 */
int sensorhub_set_ext_data_rate(const struct motion_sensor_t *s,
					int rate, int rnd, int *ret_rate);

/**
 * sensorhub_config_ext_reg() - Configure external sensor register
 * @s:		Pointer to external motion sensor's data structure.
 * @slv_addr:	I2C Slave Address of the external sensor.
 * @reg:	Register Address to write within the external sensor.
 * @val:	Value to be written into the external sensor register.
 *
 * Return: EC_SUCCESS on success, EC error codes on failure.
 *
 * This function is used to configure the register of an external
 * sensor that is attached to sensor hub with a specific value.
 */
int sensorhub_config_ext_reg(const struct motion_sensor_t *s,
				uint8_t slv_addr, uint8_t reg, uint8_t val);

/**
 * sensorhub_config_slv0_read() - Configure sensor hub to read slave0
 * @s:		Pointer to external motion sensor's data structure.
 * @slv_addr:	I2C Slave Address of the external sensor.
 * @reg:	Register Address to read from the external sensor.
 * @len:	Length of data to be read.
 *
 * Return: EC_SUCCESS on success, EC error codes on failure.
 *
 * This function is used to configure the sensor hub to read data from
 * a specific register of an external sensor that is attached to it.
 */
int sensorhub_config_slv0_read(const struct motion_sensor_t *s,
				uint8_t slv_addr, uint8_t reg, int len);

/**
 * sensorhub_slv0_data_read() - Read the data from slave0
 * @s:	Pointer to external motion sensor's data structure.
 * @v:	Vector to hold the data from the external sensor.
 *
 * Return: EC_SUCCESS on success, EC error codes on failure.
 *
 * This function reads the data from the register bank that is associated
 * to the slav0 of the sensor hub.
 */
int sensorhub_slv0_data_read(const struct motion_sensor_t *s, intv3_t v);

/**
 * sensorhub_check_and_rst() - Check and reset the external sensor.
 * @s:		Pointer to external motion sensor's data structure.
 * @slv_addr:	I2C Slave Address of the external sensor.
 * @whoami_reg:	Register address to identify the sensor.
 * @whoami_val:	Expected value to be read from the whoami_reg.
 * @rst_reg:	Register address to reset the external sensor.
 * @rst_val:	Value to be written to the reset register.
 *
 * Return: EC_SUCCESS on success, EC error codes on failure.
 *
 * This function is used to check the identity of the external sensor and
 * then reset the external sensor that is attached to the sensor hub.
 */
int sensorhub_check_and_rst(const struct motion_sensor_t *s, uint8_t slv_addr,
					uint8_t whoami_reg, uint8_t whoami_val,
					uint8_t rst_reg, uint8_t rst_val);
#endif /* __CROS_EC_SENSORHUB_LSM6DSM_H */
